#include <linux/module.h>		/* Requerido por todos los módulos */
#include <linux/kernel.h>		/* Definición de KERN_INFO */
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm-generic/errno.h>
#include <linux/tty.h>      	/* For fg_console */
#include <linux/kd.h>       	/* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/version.h>		/* For LINUX_VERSION_CODE */
#include <linux/slab.h>


MODULE_LICENSE("GPL");										/* Licencia del modulo */
MODULE_AUTHOR("Carlos Segundo Nieto - Joaquin Asensio");	/* Autores del modulo */
MODULE_DESCRIPTION("P2 - LIN - 2019/2020.");				/* Descripcion del modulo */


#define ALL_ON			7
#define ALL_OFF			0
#define SUCCESS			0



// DEFINIMOS LAS VARIABLES
static struct proc_dir_entry* proc_ledctl; // Entrada /proc/ledctl
static struct tty_driver* kbd_driver = NULL;

/* Get driver handler */
static struct tty_driver* get_kbd_driver_handler(void)
{
    printk(KERN_INFO "modleds: loading\n");
    printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) )
    return vc_cons[fg_console].d->port.tty->driver;
#else
    return vc_cons[fg_console].d->vc_tty->driver;
#endif
}


// FUNCIONES A IMPLEMENTAR

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask)
{
#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) )
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
#else
    return (handler->ops->ioctl) (vc_cons[fg_console].d->vc_tty, NULL, KDSETLED, mask);
#endif
}

static ssize_t ledctl_auxiliar_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	char *local_buff = kmalloc(8*sizeof(char),GFP_KERNEL);
    char ch = ' ';
    int i = 0 ;

	if(copy_from_user(local_buff,buff,10))
		return -EFAULT;

	//LEER Y PROCESAR CADA NUMERO/LED A ENCENDER DEL BUFFER
	set_leds(kbd_driver,ALL_OFF);

	while( i < strlen(local_buff) && local_buff[i] != '\n' && local_buff[i] != '\0')
	{
		ch = local_buff[i];
		i++;

	}
    kfree(local_buff);
          

       switch(ch){
        case '0':
            set_leds(kbd_driver,0x0);
            break;
        case '1':
            set_leds(kbd_driver,0x1);
            break;
        case '2':
            set_leds(kbd_driver,0x4);
            break;
        case '3':
            set_leds(kbd_driver,0x5);
            break;
        case '4':
            set_leds(kbd_driver,0x2);
            break;
        case '5':
            set_leds(kbd_driver,0x3);
            break;
        case '6':
            set_leds(kbd_driver,0x6);
            break;
        case '7':
            set_leds(kbd_driver,0x7);
            break;
    }

	return 4; //Siempre nos quedamos con el ultimo int
}

static ssize_t ledctl_auxiliar_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
    printk(KERN_ALERT "ERROR. Operacion de lectura no permitida.\n");
    return -EPERM;
}

// Operacion a implementar unicamente de escritura para depurar la llamada al sistema
struct file_operations fops = {
	.read = ledctl_auxiliar_read,
	.write = ledctl_auxiliar_write
};


static int __init ledctl_auxiliar_init(void)
{
	proc_ledctl = proc_create("ledctl", 0222, NULL, &fops); // SOLO escritura
	kbd_driver= get_kbd_driver_handler();

	if(proc_ledctl == NULL || kbd_driver == NULL){
		printk(KERN_INFO "Fallo al crear la entrada en /proc\n");
		return -EAGAIN; // Error: try again
	}
	else {
		printk(KERN_INFO "Modulo ledctl cargado correctamente.\n");
	}

	/* Encender todos los leds para indicar que se ha cargado correctamente el modulo */
    set_leds(kbd_driver, ALL_ON);

	return SUCCESS;
}

static void __exit ledctl_auxiliar_exit(void)
{
	/* Apagar todos los leds */
	set_leds(kbd_driver, ALL_OFF);

	remove_proc_entry("ledctl", NULL);

	printk(KERN_INFO "Modulo ledctl descargado correctamente.\n");
}

/* Declaración de funciones init y exit */
module_init(ledctl_auxiliar_init);
module_exit(ledctl_auxiliar_exit);
