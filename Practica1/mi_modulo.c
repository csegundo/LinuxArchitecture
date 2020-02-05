#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Practica 1");

struct list_head mylist;
struct proc_dir_entry* my_proc;

int sizeList = 0;

struct list_item {
	int data;
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
	my_proc = proc_create("modlist", 0666, NULL, &fops); // Creamos la entrada /proc/modlist
	if(my_proc == NULL){
		printk(KERN_INFO "Fallo al crear la entrada en /proc\n");
		return -EAGAIN; // Error: try again
	}
	else {
		INIT_LIST_HEAD(&mylist); // Inicializamos la lista enlazada
		printk(KERN_INFO "Modulo modlist cargado. Hola kernel.\n");
	}
	
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

		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			list_del(cur_node);
			vfree(item);
		};
		printk(KERN_INFO "Lista enlazada eliminada.\n");
	}
	printk(KERN_INFO "Modulo modlist descargado. Adios kernel.\n");
}

/* Funcion que se invoca cuando se desea leer la entrada /proc/modlist */
static ssize_t proc_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
	char* kbuff = (char*)vmalloc((sizeList*sizeof(int))*2);
	char* dest = kbuff;

	struct list_item* item = NULL;
	struct list_head* nodo = NULL;


	if(*offset>=sizeList)
		return 0;

	if(!list_empty(&mylist)){

		list_for_each(nodo, &mylist){
			item = list_entry(nodo, struct list_item, links);
			dest += sprintf(dest, "%d\n", item->data);
		};
		
		if(copy_to_user(buffer, kbuff, dest - kbuff +1) != 0){
			vfree(kbuff);
			return -EINVAL;
		}
	}
	printk(KERN_INFO "%d",*offset);
	vfree(kbuff);
	*offset += dest-kbuff;
	printk(KERN_INFO "%d",*offset);


	return *offset;
}

/* Funcion que se invoca cuando se desea escribir en la entrada /proc/modlist */
static ssize_t proc_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	char* kbuff = (char*)vmalloc(len);
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	struct list_head* aux = NULL;
	char* clean = (char*)vmalloc(10);
	char* msg = "ERROR:Comando erroneo\n";
	char* cleanup = "cleanup";
	int n;

	if(copy_from_user(kbuff, buff, len) != 0){
		return -EACCES;
	}

	kbuff[len] = '\0';

	// ADD
	if(sscanf(kbuff, "add %d",&n )==1){
		item = (struct list_item*)vmalloc(sizeof(struct list_item));
		item->data = n;
		INIT_LIST_HEAD(&item->links);
		list_add_tail(&item->links,&mylist);
		sizeList += 1;
	}
	// REMOVE
	else if(sscanf(kbuff,"remove %d",&n)==1){
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			if(item->data == n){
				list_del(cur_node);
				vfree(item);
				sizeList -= 1;
			}
		};
	}
	// CLEAUNP
	else if(sscanf(kbuff,"%s",clean)==1){
		if(strcmp(clean,cleanup)==0){
			list_for_each_safe(cur_node, aux, &mylist) {
				item = list_entry(cur_node, struct list_item, links);
				list_del(cur_node);
				vfree(item);
				sizeList -= 1;

			};
		}else{
			vfree(kbuff);
			vfree(clean);
		 	return -EINVAL	;
		}
	}
	vfree(kbuff);
	vfree(clean);

	return len;
}

/* Declaración de funciones init y exit */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
