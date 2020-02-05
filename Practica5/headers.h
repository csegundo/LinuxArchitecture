#ifndef _HEADERS_H_
#define _HEADERS_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Segundo - Joaquin Asensio");
MODULE_DESCRIPTION("Practica 5");

extern int timer_period_ms;
extern int max_random;
extern int emergency_threshold;

extern int conf_release (struct inode *inode, struct file *file);
extern int conf_open(struct inode *inode, struct file *file);
extern ssize_t conf_read(struct file *file, char *buff, size_t len, loff_t *offset);
extern ssize_t conf_write(struct file *file, const char *buff, size_t len, loff_t *offset);

extern void my_work_func(struct work_struct *work);

extern int init_modul(void);
extern void cleanup_modul(void);

extern struct file_operations config_fops;

#endif