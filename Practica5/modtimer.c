#include "headers.h"

#define MAX_CBUFFER_LEN 32

struct list_head mylist; // LISTA ENLAZADA
int waitin = 0;

// NODOS DE LA LISTA
struct list_item {
	int data;
	struct list_head links;
};

struct kfifo cbuffer;		/* Buffer circular*/
struct timer_list my_timer; // Estructura para implementar el temporizador
struct work_struct work;
struct workqueue_struct* wq;

spinlock_t sp_timer;
struct semaphore sem_list;
struct semaphore queue;

struct proc_dir_entry* timer_proc; // Entrada /proc/modtimer
struct proc_dir_entry* config_proc; // Entrada /proc/modconfig

static int timer_release(struct inode *inode, struct file *file);
static int timer_open(struct inode *inode, struct file *file);
static ssize_t timer_read(struct file *file, char *buff, size_t len, loff_t *offset);
void my_timer_function(unsigned long data);
void my_work_func(struct work_struct *work);

void cleanup_modul(void);
int init_modul(void);

static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
	.read 	= timer_read,
	.open 	= timer_open,
	.release = timer_release,
};

void my_work_func(struct work_struct *work)
{
	//copiar del buffer a array, y de array a list
	struct list_item* item = NULL;
	int array[MAX_CBUFFER_LEN/4];	
	int i, nElems;
	unsigned long flags;

	// Procedemos a pasarlo del buffer al array
	spin_lock_irqsave(&sp_timer,flags);

	nElems = kfifo_len(&cbuffer);
	i = kfifo_out(&cbuffer,&array,nElems);

	spin_unlock_irqrestore(&sp_timer,flags);

	for (i = 0; i < nElems/4; i++)
		printk(KERN_INFO "%d,",array[i]);

	if(down_interruptible(&sem_list))
		return (void)-EINTR;
	// Procedemos a pasarlo del array a la lista enlazada
	for(i = 0; i < 	nElems/4; i++){ // Dividimos entre 4 porque cuando alcanza el % de ocupacion salen 24 pero son bytes
		item = (struct list_item*) vmalloc(sizeof(struct list_item));
		item->data = array[i];
		list_add_tail(&item->links,&mylist);
	}
	if(waitin && nElems/4 > 0)
		up(&queue); //cola de espera de lista enlazada vacia
	up(&sem_list);
}

// Generar numero aleatorio y meterlo al buffer circular 
void my_timer_function(unsigned long data){
	unsigned long flags = 0;
	int n = get_random_int() % max_random;

	spin_lock_irqsave(&sp_timer,flags);

	kfifo_in(&cbuffer, &n, sizeof(n));

	//posible down_interruptible(&sem_list)
	if((kfifo_len(&cbuffer)*100)/kfifo_size(&cbuffer) >= emergency_threshold) //% de ocupacion alcanzado
		queue_work(wq, &work);

	spin_unlock_irqrestore(&sp_timer,flags);
	mod_timer(&my_timer, jiffies + HZ);
}

/* Funciones de inicialización y descarga del módulo*/
int init_modul(void){
	// CREAMOS Y DEFINIMOS EL BUFFER CIRCULAR
	if(kfifo_alloc(&cbuffer,MAX_CBUFFER_LEN,GFP_KERNEL))
		return -EINTR;

	// INICIALIZAMOS LA LISTA ENLAZADA
	INIT_LIST_HEAD(&mylist);

    sema_init(&sem_list,1);
	sema_init(&queue,0);
	spin_lock_init(&sp_timer);

	// CREAMOS LA WORKQUEUE
	INIT_WORK(&work,my_work_func);
	wq = create_workqueue("bottom_half");

	// INICIALIZAMOS LAS VARIABLES
    timer_period_ms = 500;
    max_random = 300;
    emergency_threshold = 75;

	timer_proc = proc_create("modtimer", 0666, NULL, &timer_fops); // Creamos la entrada /proc/modtimer
	config_proc = proc_create("modconfig", 0666, NULL, &config_fops); // Creamos la entrada /proc/modconfig
	if(config_proc == NULL || timer_proc == NULL)
		goto failed;

	// CREAMOS E INICIALIZAMOS EL TIMER
	init_timer(&my_timer);
	my_timer.expires = jiffies + timer_period_ms; // EXPIRES ==> Tiempo de activacion del timer ======== timer_period_ms ????
	my_timer.data = 0;
	my_timer.function = my_timer_function;

	// AQUI ACTIVAMOS EL TIMER POR PRIMERA VEZ, EMPEZAMOS LA CUENTA
	add_timer(&my_timer);

	return 0;

	failed:
		kfifo_free(&cbuffer);
		destroy_workqueue(wq);
		return -EAGAIN; // Error: ty again
}

void cleanup_modul(void){
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	struct list_head* aux = NULL;
	unsigned long flags = 0;
	
	del_timer_sync(&my_timer);
	// Esperar a que acaben los trabajos de la workqueue creada
	flush_workqueue(wq);

	// ELIMINAMOS LAS ENTRADAS /proc
	remove_proc_entry("modtimer",NULL);
	remove_proc_entry("modconfig",NULL);

	if(down_interruptible(&sem_list))
		return (void)-EINTR;

	if(!list_empty(&mylist)){ // Ponemos if porque la funcion list_for_each_safe() recorre la lista asique no hace falta hacer mas iteraciones
		//En aux almacenamos el siguiente nodo de la lista y cur_node es el nodo a borrar (???)
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);//&mylist);
			list_del(cur_node);
			vfree(item);
		};
	}
	up(&sem_list);

	spin_lock_irqsave(&sp_timer,flags);
	kfifo_free(&cbuffer);
	spin_unlock_irqrestore(&sp_timer,flags);
	destroy_workqueue(wq);
}

/* Se invoca al hacer close() de entrada /proc */
static int timer_release (struct inode *inode, struct file *file){
	module_put(THIS_MODULE);

	return 0;
}

/* Se invocaal hacer open() de entrada /proc */
static int timer_open(struct inode *inode, struct file *file){
	// AUMENTAMOS EN UNO EL CONTADOR INTERNO DE REFERENCIAS
	try_module_get(THIS_MODULE);

	return 0;
}

/* Se invocaal hacer read() de entrada /proc */
static ssize_t timer_read(struct file *file, char *buff, size_t len, loff_t *offset){
	struct list_item* item = NULL;
	struct list_head* aux = NULL;
	struct list_head* cur_node = NULL;
	char* kbuff = NULL;
	char* kbuff_aux = NULL;
	int ret = 0;
	int nums = (((MAX_CBUFFER_LEN/4)*75)/100);

	if(list_empty(&mylist)){
		waitin = 1;
		if(down_interruptible(&queue))
			return -EINTR;
	}
	
	if(down_interruptible(&sem_list))
		return -EINTR;

	kbuff = (char*) vmalloc( (nums*sizeof(int))+nums );
	list_for_each_safe(cur_node, aux, &mylist) {
		kbuff_aux = (char*) vmalloc(sizeof(int)+1);
		item = list_entry(cur_node, struct list_item, links);
		ret += sprintf(kbuff_aux,"%d\n",item->data);
		strcat(kbuff,kbuff_aux);
		list_del(cur_node);
		vfree(item);
		vfree(kbuff_aux);
	};
	if(copy_to_user(buff,kbuff,ret +(ret/4)))
		goto fail;
	vfree(kbuff);
	up(&sem_list);	
	return ret;

	fail:
		vfree(item);
		vfree(kbuff);
		up(&sem_list);
		return -EINTR;
}

module_init(init_modul);
module_exit(cleanup_modul);