/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>      // file_operations
#include <linux/uaccess.h> /* copy_*_user */
#include <linux/slab.h>    /* kmalloc() */
#include <linux/kernel.h>  /* printk() */
#include <linux/errno.h>   /* error codes */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "aesdchar.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Mostafa Gamal"); /** DONE: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev; /* device information */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */
    PDEBUG("finished open");

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    /**
     * DONE: handle read
     */
    struct aesd_buffer_entry *return_buffer_entry = NULL;
    struct aesd_dev *dev = (struct aesd_dev *)(filp->private_data);
    size_t offset_rtn = 0;
    size_t bytes_to_read;
    size_t bytes_not_read = 0;

    PDEBUG("Aquire lock");
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    if (*f_pos >= dev->size)
    {
        goto out;
    }
    if (*f_pos + count > dev->size)
    {
        count = dev->size - *f_pos;
    }

    do
    {
        PDEBUG("Get entry from c_buffer");
        return_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->c_buffer), *(f_pos), &offset_rtn);
        if (!return_buffer_entry)
        {
            PDEBUG("Getting entry returned NULL");
            goto out;
        }
        else
        {
            bytes_to_read = return_buffer_entry->size - offset_rtn;
            if (bytes_to_read > count)
            {
                bytes_to_read = count;
            }
            PDEBUG("Copy entry to user space buffer");
            bytes_not_read = copy_to_user(&(buf[retval]), &(return_buffer_entry->buffptr[offset_rtn]), bytes_to_read);
            if (bytes_not_read)
            {
                PDEBUG("bytes_not_read=%zu out of bytes_to_read=%zu", bytes_not_read, bytes_to_read);
            }
            *f_pos += (loff_t)(bytes_to_read - bytes_not_read);
            retval += bytes_to_read - bytes_not_read;
        }
    } while (bytes_not_read);

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */
    /*
        writes are accepted as string without a newline '\n' or string terminated with '\n'
        i.e: "abcdef" or "abc\n"
        writes with a newline in between are not handled
        i.e: "abc\ndef"
    */

    struct aesd_dev *dev = (struct aesd_dev *)(filp->private_data);
    char *newline_exist;
    size_t bytes_not_written = 0;

    PDEBUG("Aquire lock");
    if (mutex_lock_interruptible(&dev->lock))
    {
        return -ERESTARTSYS;
    }

    PDEBUG("Memory Alloc for entry");
    if (dev->c_buffer_entry.size == 0)
    {
        dev->c_buffer_entry.buffptr = kmalloc(count, GFP_KERNEL);
    }
    else
    {
        dev->c_buffer_entry.buffptr = krealloc(dev->c_buffer_entry.buffptr, dev->c_buffer_entry.size + count, GFP_KERNEL);
    }
    if (dev->c_buffer_entry.buffptr == NULL)
    {
        goto out;
    }

    PDEBUG("Copy from user");
    bytes_not_written = copy_from_user((void *)(&(dev->c_buffer_entry.buffptr[dev->c_buffer_entry.size])), buf, count);

    retval = count - bytes_not_written;
    dev->c_buffer_entry.size += retval;
    dev->size += retval;

    newline_exist = (char *)memchr(dev->c_buffer_entry.buffptr, '\n', dev->c_buffer_entry.size);
    if (newline_exist != NULL)
    {
        PDEBUG("Add new entry to circular buffer");
        aesd_circular_buffer_add_entry(&(dev->c_buffer), &(dev->c_buffer_entry));
        dev->c_buffer_entry.buffptr = NULL;
        dev->c_buffer_entry.size = 0;
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    PDEBUG("Initializing Mutex");
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    PDEBUG("Free allocated memory for aesd_device c_buffer entries");
    int i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        if (aesd_device.c_buffer.entry[i].buffptr)
            kfree(aesd_device.c_buffer.entry[i].buffptr);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
