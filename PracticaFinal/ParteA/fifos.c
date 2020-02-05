#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo Nieto");
MODULE_DESCRIPTION("Practica Final - Variante 2 - Parte A");

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int elementosEnLista = 1;

static int max_entries = 4;
static int max_size = 64;

module_param(max_entries, int, 0);
MODULE_PARM_DESC(max_entries, "Numero maximo de entradas simultaneas que puede haber sin contar la entrada control");
module_param(max_size, int, 0);
MODULE_PARM_DESC(max_size, "Tam maximo de los buffers circulares especificado en BYTES");

// STRUCT PARA EL CAMPO private_data DE LOS FIFOS QUE CREEMOS
struct my_private_data {
	char* nombre;
	struct kfifo cbufferr;
};
// STRUCT QUE TIENE LA ENTRADA /proc Y SUS DATOS PRIVADOS
struct entrada_proc {
	struct proc_dir_entry* entry;
	struct my_private_data* datos;
};
// CADA NODO DE LA LISTA CONTIENE TANTO LA ENTRADA /proc COMO SUS DATOS PRIVADOS ==> struct entrada_proc
struct list_item {
	struct entrada_proc* procEntry;
	struct list_head links;
};
struct list_head mylist;		/* Lista donde vamos a ir guardando todas las entradas /proc */

struct proc_dir_entry* proc_padre 	= NULL;	/* Entrada /proc/fifo */
struct proc_dir_entry* proc_control = NULL; /* Entrada /proc/fifo/control */
struct proc_dir_entry* proc_default = NULL; /* Entrada /proc/fifo/default */

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int prod_count = 0;				/* Número de procesos que abrieron la entrada /proc para escritura (productores) */
int cons_count = 0;				/* Número de procesos que abrieron la entrada /proc para lectura (consumidores) */

struct semaphore mtx;			/* para garantizar Exclusión Mutua*/
struct semaphore sem_prod;		/* cola de espera para productor(es) */
struct semaphore sem_cons;		/* cola de espera para consumidor(es) */
 
int nr_prod_waiting = 0;		/* Número de procesos productores esperando*/
int nr_cons_waiting = 0;		/* Número de procesos consumidores esperando*/

static int fifo_release(struct inode *inode, struct file *file);
static int fifo_open(struct inode *inode, struct file *file);
static ssize_t fifo_read(struct file *file, char *buff, size_t len, loff_t *offset);
static ssize_t fifo_write(struct file *file, const char *buff, size_t len, loff_t *offset);

static const struct file_operations fifo_fops = {
	.read 		= fifo_read,
	.write 		= fifo_write,
	.open 		= fifo_open,
	.release 	= fifo_release,
};

static ssize_t control_write(struct file *file, const char *buff, size_t len, loff_t *offset);

static const struct file_operations control_fops = {
	.write 		= control_write,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int init_module(void){
	struct entrada_proc* entradaDefault = (struct entrada_proc*)vmalloc(sizeof(struct entrada_proc));
	struct list_item* item = NULL;

	// Comprobar que los parametros que se especifican cuando compilamos son correctos
	if(max_entries <= 0 || max_size <= 0)
		goto failed;

	entradaDefault->datos = (struct my_private_data*)vmalloc(sizeof(struct my_private_data));
	entradaDefault->datos->nombre = "default";

	if(kfifo_alloc(&entradaDefault->datos->cbufferr, max_size, GFP_KERNEL))
		return -EINTR;

    sema_init(&mtx, 1);
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0);

	INIT_LIST_HEAD(&mylist);

	proc_padre = proc_mkdir("fifo", NULL);
	proc_control = proc_create("control", 0666, proc_padre, &control_fops);

	proc_default = proc_create_data("default", 0666, proc_padre, &fifo_fops, entradaDefault->datos);
	entradaDefault->entry = proc_default;

	if (!proc_padre || !proc_default || !proc_control){
		vfree(proc_default);
		vfree(entradaDefault->datos);
        return -EAGAIN;
	}

    // Metemos a la lista la entrada default
    item = (struct list_item*)vmalloc(sizeof(struct list_item));
    item->procEntry = entradaDefault;
    list_add_tail(&item->links, &mylist);
	
	return 0;

	failed:
		vfree(entradaDefault);
		return -EINVAL;
}

void cleanup_module(void){
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	struct list_head* aux = NULL;

	// Recorremos la lista borrando memoria asociada a cada entrada proc asi como la misma
	if(!list_empty(&mylist)){

		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, struct list_item, links); 				// Obtenemos un elemento
			remove_proc_entry(item->procEntry->datos->nombre, proc_padre);		// Eliminamos la entrada /proc
			kfifo_free(&item->procEntry->datos->cbufferr);						// Eliminamos el buffer circular referente a cada /proc
			vfree(item->procEntry->datos);										// Liberamos memoria asociada a los datos de ese /proc
			list_del(cur_node);													// Eliminamos la asociacion de ese nodo de la lista
			vfree(item);														// Eliminamos memoria de ese elemento
		}
	}

	// Por ultimo borramos la entrada /proc/fifo/control y el directorio /proc/fifo
	remove_proc_entry("control", proc_padre);
	remove_proc_entry("fifo", NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static ssize_t control_write(struct file *file, const char *buff, size_t len, loff_t *offset){
	char* kbuff = (char*)vmalloc(len);
	struct list_item* item;
	struct list_head* cur_node = NULL;
	struct list_head* aux = NULL;
	struct kfifo nuevoFifo;
	struct my_private_data* datosAsociados = (struct my_private_data*)vmalloc(sizeof(struct my_private_data));
	struct entrada_proc* entradaDatos = (struct entrada_proc*)vmalloc(sizeof(struct entrada_proc));
	struct proc_dir_entry* nuevaEntrada;
	int existeEntrada = 0; // Para saber si existe la entrada o no

	if(copy_from_user(kbuff, buff, len) != 0){
		vfree(kbuff);
		return -EACCES;
	}

	kbuff[len] = '\0';

	if(sscanf(kbuff, "create %s", kbuff) == 1){
		if(elementosEnLista <= max_entries){
			item = (struct list_item*)vmalloc(sizeof(struct list_item));

			// Creamos el struct de los datos asociados a la nueva entrada /proc
			datosAsociados->nombre = kbuff;

			if(kfifo_alloc(&nuevoFifo, max_size, GFP_KERNEL))
				goto failed;
			datosAsociados->cbufferr = nuevoFifo;

			// Creamos el struct con esos datos asociados al nuevo /proc y su propia entrada
			nuevaEntrada = proc_create_data(datosAsociados->nombre, 0666, proc_padre, &fifo_fops, datosAsociados);
			entradaDatos->datos = datosAsociados;
			entradaDatos->entry = nuevaEntrada;

			// Ahora lo metemos todo a la lista
			item->procEntry = entradaDatos;
			list_add_tail(&item->links, &mylist);

			++elementosEnLista;
		}
		else{
			goto failed;
		}
	}
	else if(sscanf(kbuff, "delete %s", kbuff) == 1){
		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, struct list_item, links);

			if(strcmp(kbuff, item->procEntry->datos->nombre)){
				kfifo_free(&item->procEntry->datos->cbufferr);
				vfree(item->procEntry->datos->nombre);
				vfree(item->procEntry->datos);
				vfree(item->procEntry);
				remove_proc_entry(kbuff, proc_padre);

				list_del(cur_node);

				++existeEntrada;
			}
		}
		vfree(datosAsociados);
		vfree(entradaDatos);

		// Si no existe la entrada, no mostrar mensaje de error
		if(existeEntrada != 1)
			goto entradaNoExiste;
	}
	else{
		vfree(datosAsociados);
		vfree(entradaDatos);
		vfree(kbuff);
		return -EINVAL;
	}

	return len;

	entradaNoExiste:
		vfree(kbuff);
		return -ENOENT;		// "No such file or directory"

	failed:
		vfree(datosAsociados);
		vfree(entradaDatos);
		return -EINTR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Se invoca al hacer close() de entrada /proc */
static int fifo_release (struct inode *inode, struct file *file){
	struct my_private_data* datosEntrada = (struct my_private_data*)PDE_DATA(file->f_inode);

	if(down_interruptible(&mtx))
		return -EINTR;

	if(file->f_mode & FMODE_READ){
        cons_count--;
	    if(nr_prod_waiting > 0){
	        /* Despierta 1 de los hilos bloqueados */
	        nr_prod_waiting--;
			up(&sem_prod);
	    }
	}
	else{
        prod_count--;

	    if(nr_cons_waiting > 0){
	        /* Despierta 1 de los hilos bloqueados */
	        nr_cons_waiting--;
			up(&sem_cons);
	    }
	}

    if(cons_count == 0 && prod_count == 0)
        kfifo_reset(&datosEntrada->cbufferr);

	up(&mtx);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Se invocaal hacer open() de entrada /proc */
static int fifo_open(struct inode *inode, struct file *file){
    if (file->f_mode & FMODE_READ){
        /* Un consumidor abrió el FIFO*/
        if(down_interruptible(&mtx))
	    	return -EINTR;
		cons_count++;

	    while(prod_count == 0){
	        nr_cons_waiting++;
	        up(&mtx); /*Libera el 'mutex' */
	        /*se bloquea en la cola */
	        if(down_interruptible(&sem_cons)){
	           down(&mtx);
	           nr_cons_waiting--;
	           cons_count--;
	           up(&mtx);
	           return -EINTR;
	        }
	        /* Adquiere el mutx */
	        if(down_interruptible(&mtx))
	            return -EINTR;
	    }
    	/* Libera el mute */
  	  	up(&mtx);

  	  	if(down_interruptible(&mtx))
     		 return -EINTR;

	    if(nr_prod_waiting > 0){
	        /* Despierta 1 de los hilos bloqueados */
	        nr_prod_waiting--;
			up(&sem_prod);
	    }
	    up(&mtx);

    }
    else{
	    /* Un productorabrió el FIFO*/
		if(down_interruptible(&mtx))
	    	return -EINTR;
	    prod_count++;

	    while(cons_count == 0 ){
	       nr_prod_waiting++;
	        up(&mtx); /*Libera el 'mutex' */
	        /*se bloquea en la cola */
	        if(down_interruptible(&sem_prod)){
	            down(&mtx);
	            nr_prod_waiting--;
	            prod_count--;
	            up(&mtx);
	            return -EINTR;
	        }
	        /* Adquiere el mutex */
	        if(down_interruptible(&mtx))
	            return -EINTR;
	    }
	    /* Libera el mutex */
	    up(&mtx);

	    if(down_interruptible(&mtx))
     		 return -EINTR;

	    if(nr_cons_waiting > 0){
	        /* Despierta 1 de los hilos bloqueados */
	        nr_cons_waiting--;
			up(&sem_cons);
	    }
	    up(&mtx);
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Se invocaal hacer read() de entrada /proc */
static ssize_t fifo_read(struct file *file, char *buff, size_t len, loff_t *offset){
	char kbuffer[max_size];
	struct my_private_data* datosDeLaEntrada = (struct my_private_data*)PDE_DATA(file->f_inode);

    if (len > max_size) 
        return -ENOBUFS;

    if(down_interruptible(&mtx))
    	return -EINTR;

    while(kfifo_len(&datosDeLaEntrada->cbufferr) < len && prod_count > 0){
        nr_cons_waiting++;
        up(&mtx); /*Libera el 'mutex' */
        /*se bloquea en la cola */
        if(down_interruptible(&sem_cons)){
            down(&mtx);
            nr_cons_waiting--;
            up(&mtx);
            return -EINTR;
        }
        /* Adquiere el mutx */
        if(down_interruptible(&mtx))
            return -EINTR;
    }

    if (prod_count == 0 && kfifo_is_empty(&datosDeLaEntrada->cbufferr)) {
        up(&mtx);
        return 0;
    }

    kfifo_out(&datosDeLaEntrada->cbufferr, kbuffer, len);/* Despertar a posible productor bloqueado*/
    
    /* Libera el mute */
    if(nr_prod_waiting > 0){
        /* Despierta 1 de los hilos bloqueados */
        nr_prod_waiting--;
		up(&sem_prod);
    }
    up(&mtx);
	copy_to_user(buff, &kbuffer, len);

    return len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Se invoca al hacer write() de entrada /proc */
static ssize_t fifo_write(struct file *file, const char *buff, size_t len, loff_t *offset){
	char kbuffer[max_size];
	struct my_private_data* datosDeLaEntrada = (struct my_private_data*)PDE_DATA(file->f_inode);

	if (len> max_size) {
		return -ENOBUFS;
	}
	if (copy_from_user(kbuffer, buff, len)) {
		return -ENOSPC;
	}

	if(down_interruptible(&mtx))
      return -EINTR;

    while(kfifo_avail(&datosDeLaEntrada->cbufferr) < len && cons_count > 0){
        nr_prod_waiting++;
        up(&mtx); /*Libera el 'mutex' */
        /*se bloquea en la cola */
        if(down_interruptible(&sem_prod)){
            down(&mtx);
            nr_prod_waiting--;
            up(&mtx);
            return -EINTR;
        }
        /* Adquiere el mutx */
        if(down_interruptible(&mtx))
            return -EINTR;
    }
	/* Detectar fin de comunicación por error(consumidor cierra FIFO antes) */
	if (cons_count == 0) {
		up(&mtx);
		return -EPIPE;
	}
	kfifo_in(&datosDeLaEntrada->cbufferr, kbuffer, len);
	/* Despertar a posible consumidor bloqueado*/
	
	if(nr_cons_waiting > 0){
        /* Despierta 1 de los hilos bloqueados */
        nr_cons_waiting--;
		up(&sem_cons);
    }

    /* Libera el mutex */
    up(&mtx);

	return len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//module_init(init_module);
//module_exit(cleanup_module);