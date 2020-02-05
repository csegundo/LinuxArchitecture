#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/vt_kern.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Practica 4 - Parte B - Parte opcional");

#define MAX_KBUF            64
#define MAX_CBUFFER_LEN     64
#define DEVICE_NAME         "chrProdConsDos"
#define SUCCESS             0

struct kfifo cbuffer;/* Buffer circular*/
int prod_count = 0;/* Númerode procesos que abrieron la entrada/proc para escritura(productores) */
int cons_count = 0;/* Númerode procesos que abrieron la entrada/proc para lectura(consumidores) */

struct semaphore mtx;/* para garantizar Exclusión Mutua*/
struct semaphore sem_prod;/* cola de espera para productor(es) */
struct semaphore sem_cons;/* cola de espera para consumidor(es) */
 
int nr_prod_waiting = 0;/* Número de procesos productores esperando*/
int nr_cons_waiting = 0;/* Número de procesos consumidores esperando*/

static int major;       /* Major number assigned to our device driver */
static int minor;       /* Minor number assigned to the associated character device */

static int fifoproc_release(struct inode *inode, struct file *file);
static int fifoproc_open(struct inode *inode, struct file *file);
static ssize_t fifoproc_read(struct file *file, char *buff, size_t len, loff_t *offset);
static ssize_t fifoproc_write(struct file *file, const char *buff, size_t len, loff_t *offset);

static struct file_operations fops = {
    .owner      = THIS_MODULE,
    .read       = fifoproc_read,
    .write      = fifoproc_write,
    .open       = fifoproc_open,
    .release    = fifoproc_release,
};

dev_t start;
struct cdev* chardev=NULL;


int init_module(void){
    int ret;

    kfifo_alloc(&cbuffer,MAX_CBUFFER_LEN,GFP_KERNEL);
    sema_init(&mtx,1);
    sema_init(&sem_prod,0);
    sema_init(&sem_cons,0);

    /* Get available (major,minor) range */
    if ((ret=alloc_chrdev_region(&start, 0, 1,DEVICE_NAME))) {
        printk(KERN_INFO "Can't allocate chrdev_region()");
        return ret;
    }

    /* Create associated cdev */
    if ((chardev=cdev_alloc())==NULL) {
        printk(KERN_INFO "cdev_alloc() failed ");
        unregister_chrdev_region(start, 1);
        return -ENOMEM;
    }

    cdev_init(chardev,&fops);

    if ((ret=cdev_add(chardev,start,1))) {
        printk(KERN_INFO "cdev_add() failed ");
        kobject_put(&chardev->kobj);
        unregister_chrdev_region(start, 1);
        return ret;
    }

    major=MAJOR(start);
    minor=MINOR(start);

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'sudo mknod -m 666 /dev/%s c %d %d'.\n", DEVICE_NAME, major,minor);
    printk(KERN_INFO "Remove the device file and module when done.\n");

    return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void){

    /* Destroy chardev */
    if (chardev)
        cdev_del(chardev);

    /*
     * Unregister the device
     */
    unregister_chrdev_region(major, DEVICE_NAME);
    //unregister_chrdev_region(start, 1); // https://www.fsl.cs.sunysb.edu/kernel-api/re942.html

    // Libera el buffer circular
    kfifo_free(&cbuffer);
}


/* Se invocaal hacerclose() de entrada/proc*/
static int fifoproc_release (struct inode *inode, struct file *file){
     if(down_interruptible(&mtx))
          return -EINTR;

    if(file->f_mode & FMODE_READ){
        printk("Open --READ\n");
        cons_count--;
        if(nr_prod_waiting > 0){
            /* Despierta 1 de los hilos bloqueados */
            nr_prod_waiting--;
            up(&sem_prod);
        }
    }else{
        prod_count--;
        printk("Open --write\n");

        if(nr_cons_waiting > 0){
            /* Despierta 1 de los hilos bloqueados */
            nr_cons_waiting--;
            up(&sem_cons);
        }
    }

    if(cons_count == 0 && prod_count == 0)
        kfifo_reset(&cbuffer);

    up(&mtx);
}

/* Se invocaal haceropen() de entrada/proc*/
static int fifoproc_open(struct inode *inode, struct file *file){
    if (file->f_mode & FMODE_READ){
        printk("Open READ\n");
        /* Un consumidorabrió el FIFO*/
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
        //cond_signal(sem_cons, &nr_cons_waiting);

    } else{
        printk("Open WRITE\n");
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
            /* Adquiere el mutx */
            if(down_interruptible(&mtx))
                return -EINTR;
        }
        /* Libera el mute */
        up(&mtx);

        if(down_interruptible(&mtx))
             return -EINTR;

        if(nr_cons_waiting > 0){
            /* Despierta 1 de los hilos bloqueados */
            nr_cons_waiting--;
            up(&sem_cons);
        }
        up(&mtx);
        //cond_signal(sem_prod, &nr_prod_waiting);
    }
    return 0; //ESTE RETURN??
}

/* Se invocaal hacer read() de entrada/proc*/
static ssize_t fifoproc_read(struct file *file, char *buff, size_t len, loff_t *offset){
    char kbuffer[MAX_KBUF];

    if (len > MAX_CBUFFER_LEN || len > MAX_KBUF) 
        return -ENOBUFS;

    if(down_interruptible(&mtx))
        return -EINTR;

    while(kfifo_len(&cbuffer)<len && prod_count>0){
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

    if (prod_count == 0 && kfifo_is_empty(&cbuffer)) {
        up(&mtx);
        return 0;
    }

    kfifo_out(&cbuffer,kbuffer,len);/* Despertar a posible productor bloqueado*/
    
    //cond_signal(prod);
    /* Libera el mute */
    if(nr_prod_waiting > 0){
        /* Despierta 1 de los hilos bloqueados */
        nr_prod_waiting--;
        up(&sem_prod);
    }
    up(&mtx);
    copy_to_user(buff,&kbuffer,len);

    return len;
}

/* Se invocaal hacer write() de entrada/proc*/
static ssize_t fifoproc_write(struct file *file, const char *buff, size_t len, loff_t *offset){
    char kbuffer[MAX_KBUF];
    if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) {
        return -ENOBUFS;
    }if (copy_from_user(kbuffer,buff,len)) {
        return -ENOSPC;
    }   

    if(down_interruptible(&mtx))
      return -EINTR;

    while(kfifo_avail(&cbuffer)<len && cons_count>0){
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
    if (cons_count==0) {
        up(&mtx);
        return -EPIPE;
    }
    kfifo_in(&cbuffer,kbuffer,len);
    /* Despertar a posible consumidor bloqueado*/
    
    //cond_signal(cons,&nr_cons_waiting);
    if(nr_cons_waiting > 0){
        /* Despierta 1 de los hilos bloqueados */
        nr_cons_waiting--;
        up(&sem_cons);
    }

    /* Libera el mute */
    up(&mtx);

    return len;
}

