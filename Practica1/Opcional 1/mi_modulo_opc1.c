#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Practica 1");

struct list_head mylist;
struct proc_dir_entry* my_proc;

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
	my_proc = proc_create("modlist", 0666, NULL, &fops); // Creamos la entrada /proc/modlist
	if(my_proc == NULL){
		return -EAGAIN;
	}
	else {
		INIT_LIST_HEAD(&mylist);
	}
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

		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			list_del(cur_node);
			vfree(item);
		};
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


	if(*offset>0)
	   return 0;
	
	list_for_each(nodo, &mylist){
		item = list_entry(nodo, struct list_item, links);
		dest += sprintf(dest, "%d\n", item->data);
	}
		
	if(copy_to_user(buffer, kbuff, dest - kbuff) != 0){
		vfree(kbuff);
		return -EINVAL;
	}
	vfree(kbuff);
	*offset += dest-kbuff;

	return dest-kbuff;
}

/* Funcion que se invoca cuando se desea escribir en la entrada /proc/modlist */
static ssize_t proc_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	char* kbuff = (char*)vmalloc(len);
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
		list_add_tail(&item->links,&mylist);
	}
	#else
	if(sscanf(kbuff, "add %d",&n )==1){
		item = (struct list_item*)vmalloc(sizeof(struct list_item));
		item->data = n;
		list_add_tail(&item->links,&mylist);
	}
	#endif
	
	// REMOVE
	#ifdef P_01
	else if(sscanf(kbuff,"remove %s",kbuff)==1){
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			if(strcmp(kbuff,item->data)==0){
				list_del(cur_node);
				vfree(item);
			}
		}
	}
	#else
	else if(sscanf(kbuff,"remove %d",&n)==1){
		list_for_each_safe(cur_node, aux, &mylist) {
			item = list_entry(cur_node, struct list_item, links);
			if(item->data == n){
				list_del(cur_node);
				vfree(item);
			}
		}
	}
	#endif

	// CLEAUNP
	else if(strcmp(kbuff,"cleanup\n")==0){
			list_for_each_safe(cur_node, aux, &mylist) {
				item = list_entry(cur_node, struct list_item, links);
				list_del(cur_node);
				vfree(item);
			}
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
