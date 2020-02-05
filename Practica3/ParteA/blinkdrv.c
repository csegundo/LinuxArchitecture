/*
 * Simple driver for the Blinkstick Strip USB device (v1.0)
 *
 * Copyright (C) 2018 Juan Carlos Saez (jcsaezal@ucm.es)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 * This driver is based on the sample driver found in the
 * Linux kernel sources  (drivers/usb/usb-skeleton.c) 
 * 
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Blinkstick Driver - Practica 2 - Parte A - LIN 2019/2020");

/* Get a minor range for your devices from the usb maintainer */
#define USB_BLINK_MINOR_BASE	0 

#define NR_LEDS				8
#define NR_BYTES_BLINK_MSG	6

#define APAGAR_COLOR 0x000000


/* Structure to hold all of our device specific stuff */
struct usb_blink {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct kref		kref;
};

unsigned int ledsParaEncender[8] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int coloresDeLosLeds[8] = {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000};
int index = 0;


#define to_blink_dev(d) container_of(d, struct usb_blink, kref)

static struct usb_driver blink_driver;
static int encenderOApagarLeds(struct usb_blink *dev);

/* 
 * Free up the usb_blink structure and
 * decrement the usage count associated with the usb device 
 */
static void blink_delete(struct kref *kref)
{
	struct usb_blink *dev = to_blink_dev(kref);
	usb_put_dev(dev->udev);
	vfree(dev);
}

/* Called when a user program invokes the open() system call on the device */
static int blink_open(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	
	/* Obtain reference to USB interface from minor number */
	interface = usb_find_interface(&blink_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
			__func__, subminor);
		return -ENODEV;
	}

	/* Obtain driver data associated with the USB interface */
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;

	return retval;
}

/* Called when a user program invokes the close() system call on the device */
static int blink_release(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* decrement the count on our device */
	kref_put(&dev->kref, blink_delete);
	return 0;
}


/* Called when a user program invokes the write() system call on the device */
static ssize_t blink_write(struct file *file, const char *user_buffer, size_t len, loff_t *off)
{
	struct usb_blink *dev=file->private_data;
	int retval = 0;
	int i;

	int ledAEncender;
	unsigned int colorDelLed;
	char* split; // Aqui es donde vamos a ir guardando cada una de las particiones del comando
	char* kbuff = (char*)vmalloc(len);

	if(copy_from_user(kbuff, user_buffer, len) != 0){
		retval = -EACCES;
		goto out_error;
	}

	kbuff[len] = '\0';
	printk(KERN_ALERT "%s\n",kbuff);

	if(strlen(kbuff) != 1 ){
		while((split = strsep(&kbuff, ",")) != NULL){
			if(sscanf(split, "%d:%x", &ledAEncender, &colorDelLed) == 2){
				if(ledAEncender > 7 || ledAEncender < 0){
					retval = -EINVAL;
					printk(KERN_ALERT "Ha habido un error de parametro\n");
					goto  out_error;
				}
				ledsParaEncender[index] = ledAEncender;
				coloresDeLosLeds[index] = colorDelLed;
				++index;
			}else{
				retval = -EINVAL;
				printk(KERN_ALERT "Ha habido un error de parametro\n");
				goto  out_error;
			}
		}

	}

 	if((retval = encenderOApagarLeds(dev)) < 0)
		goto out_error;

	for(i = 0; i< NR_LEDS; ++i){
		ledsParaEncender[i] = 0;
		coloresDeLosLeds[i] = 0x000000;
	}
	index = 0;
	vfree(kbuff);
	(*off)+=len;
	return len;

out_error:
	
	for(i = 0; i< NR_LEDS; ++i){
		ledsParaEncender[i] = 0;
		coloresDeLosLeds[i] = 0x000000;
	}
	index = 0;
	vfree(kbuff);
	return retval;	
}

static int encenderOApagarLeds(struct usb_blink *dev)
{
	int retval = 0, i;
	unsigned char* message = kmalloc(NR_BYTES_BLINK_MSG,GFP_DMA);
	message[0] = '\x05';
	message[1] = 0;

	for(i = 0; i < NR_LEDS; ++i){
		message[2] = i;
		message[3]=((APAGAR_COLOR>>16) & 0xff);
		message[4]=((APAGAR_COLOR>>8) & 0xff);
		message[5]=(APAGAR_COLOR & 0xff);
		retval=usb_control_msg(dev->udev,	
				usb_sndctrlpipe(dev->udev,00), /* Specify endpoint #0 */
				USB_REQ_SET_CONFIGURATION, 
				USB_DIR_OUT| USB_TYPE_CLASS | USB_RECIP_DEVICE,
				0x5,	/* wValue */
				0, 	/* wIndex=Endpoint # */
				message,	/* Pointer to the message */ 
				NR_BYTES_BLINK_MSG, /* message's size in bytes */
				0);
	}
	

	for(i = 0; i < index; ++i){
		message[2] = ledsParaEncender[i];
		message[3]=((coloresDeLosLeds[i]>>16) & 0xff);
		message[4]=((coloresDeLosLeds[i]>>8) & 0xff);
		message[5]=(coloresDeLosLeds[i] & 0xff);
		retval=usb_control_msg(dev->udev,	
				usb_sndctrlpipe(dev->udev,00), /* Specify endpoint #0 */
				USB_REQ_SET_CONFIGURATION, 
				USB_DIR_OUT| USB_TYPE_CLASS | USB_RECIP_DEVICE,
				0x5,	/* wValue */
				0, 	/* wIndex=Endpoint # */
				message,	/* Pointer to the message */ 
				NR_BYTES_BLINK_MSG, /* message's size in bytes */
				0);
	}

	if (retval<0){
		printk(KERN_ALERT "Executed with retval=%d\n",retval);
	}
	kfree(message);
	
	return retval;
}


/*
 * Operations associated with the character device 
 * exposed by driver
 * 
 */
static const struct file_operations blink_fops = {
	.owner =	THIS_MODULE,
	.write =	blink_write,	 	/* write() operation on the file */
	.open =		blink_open,			/* open() operation on the file */
	.release =	blink_release, 		/* close() operation on the file */
};

/* 
 * Return permissions and pattern enabling udev 
 * to create device file names under /dev
 * 
 * For each blinkstick connected device a character device file
 * named /dev/usb/blinkstick<N> will be created automatically  
 */
char* set_device_permissions(struct device *dev, umode_t *mode) 
{
	if (mode)
		(*mode)=0666; /* RW permissions */
 	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev)); /* Return formatted string */
}


/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver blink_class = {
	.name =		"blinkstick%d",  /* Pattern used to create device files */	
	.devnode=	set_device_permissions,	
	.fops =		&blink_fops,
	.minor_base =	USB_BLINK_MINOR_BASE,
};

/*
 * Invoked when the USB core detects a new
 * blinkstick device connected to the system.
 */
static int blink_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_blink *dev;
	int retval = -ENOMEM;

	/*
 	 * Allocate memory for a usb_blink structure.
	 * This structure represents the device state.
	 * The driver assigns a separate structure to each blinkstick device
 	 *
	 */
	dev = vmalloc(sizeof(struct usb_blink));

	if (!dev) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	/* Initialize the various fields in the usb_blink structure */
	kref_init(&dev->kref);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &blink_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */	
	dev_info(&interface->dev,
		 "Blinkstick device now attached to blinkstick-%d",
		 interface->minor);
	return 0;

error:
	if (dev)
		/* this frees up allocated memory */
		kref_put(&dev->kref, blink_delete);
	return retval;
}

/*
 * Invoked when a blinkstick device is 
 * disconnected from the system.
 */
static void blink_disconnect(struct usb_interface *interface)
{
	struct usb_blink *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &blink_class);

	/* prevent more I/O from starting */
	dev->interface = NULL;

	/* decrement our usage count */
	kref_put(&dev->kref, blink_delete);

	dev_info(&interface->dev, "Blinkstick device #%d has been disconnected", minor);
}

/* Define these values to match your devices */
#define BLINKSTICK_VENDOR_ID	0X20A0
#define BLINKSTICK_PRODUCT_ID	0X41E5

/* table of devices that work with this driver */
static const struct usb_device_id blink_table[] = {
	{ USB_DEVICE(BLINKSTICK_VENDOR_ID,  BLINKSTICK_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, blink_table);

static struct usb_driver blink_driver = {
	.name =		"blinkstick",
	.probe =	blink_probe,
	.disconnect =	blink_disconnect,
	.id_table =	blink_table,
};

/* Module initialization */
int blinkdrv_module_init(void)
{
   return usb_register(&blink_driver);
}

/* Module cleanup function */
void blinkdrv_module_cleanup(void)
{
  usb_deregister(&blink_driver);
}

module_init(blinkdrv_module_init);
module_exit(blinkdrv_module_cleanup);
