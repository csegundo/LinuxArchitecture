#include <linux/syscalls.h>		/* For SYSCALL_DEFINEi() */
#include <linux/tty.h>      	/* For fg_console */
#include <linux/kernel.h>		/* Definición de KERN_INFO */
#include <linux/version.h>
#include <linux/vt_kern.h>
#include <linux/kd.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>


SYSCALL_DEFINE1(ledctl, unsigned int, leds)
{
	unsigned int mask = 0;

       switch(leds){
        case 0:
            mask =0x0;
            break;
        case 1:
            mask = 0x1;
            break;
        case 2:
            mask = 0x4;
            break;
        case 3:
            mask = 0x5;
            break;
        case 4:
            mask = 0x2;
            break;
        case 5:
           mask = 0x3;
            break;
        case 6:
            mask = 0x6;
            break;
        case 7:
            mask = 0x7;
            break;
    }

	#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) )
    	return ((vc_cons[fg_console].d->port.tty->driver)->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);	
	#else
    	return ((vc_cons[fg_console].d->vc_tty->driver)->ops->ioctl) (vc_cons[fg_console].d->vc_tty, NULL, KDSETLED, mask);
	#endif
}
