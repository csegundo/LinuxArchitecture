#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/rwlock.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Practica 4A");

struct list_head mylist;
struct proc_dir_entry* my_proc;

rwlock_t rwl;
DEFINE_RWLOCK(rwl);

struct list_item {
	#ifdef P_01
		char* data = (char*)vmalloc(1000);
	#else
		int data;
	#endif
	struct list_head links;
};

static ssize_t proc_read(struct file *filp, char *buffer, size_t length, loff_t * offset);
static ssize_t proc_write(struct file *filp, const char *buff, size_t len, loff_t * off);

static struct file_operations fops = {
	.read = proc_read,
	.write = proc_write,
};



/************************************************************************************************************************/


/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_lin_init(void)
{
	rwlock_init(&rwl);

	my_proc = proc_create("modlist", 0666, NULL, &fops); // Creamos la entrada /proc/modlist
	if(my_proc == NULL){
		return -EAGAIN; // Error: try again
	}

	INIT_LIST_HEAD(&mylist); // Inicializamos la lista enlazada

	/* Devolver 0 para indicar una carga correcta del módulo */
	printk(KERN_INFO "Modulo cargado correctamente.");
	return 0;
}

/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	remove_proc_entry("modlist", NULL);

	if(!list_empty(&mylist)){
		struct list_item* item = NULL;
		struct list_head* cur_node = NULL;
		struct list_head* aux = NULL;

		write_lock(&rwl);

		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			list_del(cur_node);
			vfree(item);
		};
		write_unlock(&rwl);
	}
	printk(KERN_INFO "Modulo descargado correctamente.");
}

/* Funcion que se invoca cuando se desea leer la entrada /proc/modlist */
static ssize_t proc_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
	char* kbuff = (char*)vmalloc(1024);
	char* dest = kbuff;

	struct list_item* item = NULL;
	struct list_head* nodo = NULL;

	char* checkBuff = (char*)vmalloc(1024);
	char* checkAux = checkBuff;
	int checkSpace;


	if(*offset>0)
	   return 0;
	
	read_lock(&rwl);
	list_for_each(nodo, &mylist){
		item = list_entry(nodo, struct list_item, links);
		checkBuff = checkAux;
		checkSpace = sprintf(checkBuff, "%d\n", item->data);
		if(checkSpace > 0 && ((dest - kbuff) + checkSpace) <= 1024){
			dest += sprintf(dest, "%d\n", item->data);
		}
		else
			goto retfail;
	};
	read_unlock(&rwl);
		
	if(copy_to_user(buffer, kbuff, dest - kbuff) != 0){
		vfree(kbuff);
		return -EINVAL;
	}
	vfree(kbuff);
	*offset += dest-kbuff;

	return dest-kbuff;

	retfail:
	vfree(kbuff);
	vfree(checkBuff);
	read_unlock(&rwl);
	return -ENOSPC;
}

/* Funcion que se invoca cuando se desea escribir en la entrada /proc/modlist */
static ssize_t proc_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	char* kbuff = (char*)vmalloc(1024);
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	struct list_head* aux = NULL;
	int n;

	if(copy_from_user(kbuff, buff, len) != 0){
		return -EACCES;
	}

	kbuff[len] = '\0';

	// ADD
	#ifdef P_01
	if(sscanf(kbuff, "add %s",kbuff )==1){
		item = (struct list_item*)vmalloc(sizeof(struct list_item));
		strcpy(item->data,kbuff);

		write_lock(&rwl);
		list_add_tail(&item->links,&mylist);
		write_unlock(&rwl);
	}
	#else
	if(sscanf(kbuff, "add %d",&n )==1){
		item = (struct list_item*)vmalloc(sizeof(struct list_item));
		write_unlock(&rwl);
		item->data = n;
		list_add_tail(&item->links,&mylist);
		write_unlock(&rwl);
	}
	#endif
	
	// REMOVE
	#ifdef P_01
	else if(sscanf(kbuff,"remove %s",kbuff)==1){
		write_lock(&rwl);
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			if(strcmp(kbuff,item->data)==0){
				list_del(cur_node);
				vfree(item);
			}
		}
		write_unlock(&rwl);
	}
	#else
	else if(sscanf(kbuff,"remove %d",&n)==1){
		write_lock(&rwl);
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			if(item->data == n){
				list_del(cur_node);
				vfree(item);
			}
		}
		write_unlock(&rwl);
	}
	#endif

	// CLEAUNP
	else if(strcmp(kbuff,"cleanup\n")==0){
		write_lock(&rwl);
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			list_del(cur_node);
			vfree(item);
		}
		write_unlock(&rwl);
	}else{
		vfree(kbuff);
		return -EINVAL;
	}

	vfree(kbuff);


	return len;
}

/* Declaración de funciones init y exit */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
