#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/string.h>

//
// USB
//
#include <linux/usb.h>

static struct usb_device_id usb_table [] = {
    { USB_DEVICE(0x0951, 0x1653) }, // USB stick
    { }
};

static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    printk(KERN_INFO "SECRET STACK UNLOCKED\n");
    return 0;
}

static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "SECRET STACK HIDDEN\n");
    return;
}

MODULE_DEVICE_TABLE(usb, usb_table);

static struct usb_driver usb_driver = {
    .name =        "usb_key",
    .id_table =    usb_table,
    .probe =       usb_probe,
    .disconnect =  usb_disconnect,
};

//
// STACK
//
struct stack {
    int size;
    int items;
    int32_t *ptr;
};

typedef struct stack Stack;
typedef Stack *stackPtr;

int32_t push(Stack *nodePtr, int32_t *value);
int32_t pop(Stack *nodePtr, int32_t *value);
int32_t resize(Stack *nodePtr, int size);

// STACK IMPL
struct mutex stack_mutex; 

int32_t push(Stack *nodePtr, int32_t *value) {
    if (nodePtr->ptr == NULL) {
        int32_t *newptr = kmalloc(nodePtr->size * sizeof(int32_t), GFP_KERNEL);
        if (newptr == NULL){
            return 2;
        }
        nodePtr->ptr = newptr;
        nodePtr->items = 1;
        *nodePtr->ptr = *value;
    } else {
        if (nodePtr->items == nodePtr->size) {
            return 1;
        }
        if (nodePtr->items != 0) {
            nodePtr->ptr++;
        }
        *nodePtr->ptr = *value;
        nodePtr->items++;
    }
    return 0;
}

// Returns status, popped value is put in *value
int32_t pop(Stack *nodePtr, int32_t *value) {
    if (nodePtr->ptr == NULL || nodePtr->items == 0) {
        return 1;
    }
    *value = *nodePtr->ptr;
    if (nodePtr->items != 1) {
        nodePtr->ptr--;
    }
    nodePtr->items--;
    return 0;
}

// Resize, returns status
int32_t resize(Stack *nodePtr, int size) {
    if (size <= 0) {
        return 1;
    }
    if (nodePtr->ptr == NULL) { // If stack is empty just set new size
        nodePtr->size = size;
    } else { // copy old stack
        if (nodePtr->size == size) { // Same size
            return 0;
        }
        mutex_lock(&stack_mutex);
        int32_t *root_ptr = nodePtr->ptr;
        if (nodePtr->items > 0) {
            root_ptr = nodePtr->ptr - nodePtr->items + 1;
        }
        int32_t *new_ptr = kmalloc(size * sizeof(int32_t), GFP_KERNEL);
        if (new_ptr == NULL) {
            mutex_unlock(&stack_mutex);
            return 2;
        }
        int to_copy = nodePtr->items;
        if (to_copy > size) {
            to_copy = size;
            nodePtr->items = size;
        }
        memcpy(new_ptr, root_ptr, to_copy * sizeof(int32_t));
        kfree(root_ptr);

        nodePtr->size = size;
        nodePtr->ptr = new_ptr + to_copy - 1;
        mutex_unlock(&stack_mutex);
    }
    return 0;
}


// CHARDEV
// Some code borrowed from
// https://olegkutkov.me/2018/03/14/simple-linux-character-device-driver/
#define MAX_DEV 1
static int mychardev_open(struct inode *inode, struct file *file);
static int mychardev_release(struct inode *inode, struct file *file);
static long mychardev_ioctl(struct file *file, unsigned int cmd,
                            unsigned long arg);
static ssize_t mychardev_read(struct file *file, char __user *buf, size_t count,
                              loff_t *offset);
static ssize_t mychardev_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *offset);

// CHARDEV IMPL
static const struct file_operations mychardev_fops = {
    .owner = THIS_MODULE,
    .open = mychardev_open,
    .release = mychardev_release,
    .unlocked_ioctl = mychardev_ioctl,
    .read = mychardev_read,
    .write = mychardev_write};

struct mychar_device_data {
    struct cdev cdev;
};

static int dev_major = 0;
static struct class *mychardev_class = NULL;
static struct mychar_device_data mychardev_data[MAX_DEV];

static int mychardev_uevent(struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

Stack *nodePtr;

static int __init mychardev_init(void) {
    int ret = usb_register(&usb_driver);
    if (ret) {
		printk(KERN_ERR "Error registering USB Device\n");
        return 2;
    }
    int err, i;
    dev_t dev;
    mutex_init(&stack_mutex);

    nodePtr = kmalloc(sizeof(Stack), GFP_KERNEL);
    if (!nodePtr) {
		printk(KERN_ERR "MYCHARDEV: no memory for stack\n");
		return 1;
	}
    nodePtr->size = 4;
    nodePtr->items = 0;

    err = alloc_chrdev_region(&dev, 0, MAX_DEV, "myusbchardev");
    dev_major = MAJOR(dev);
    mychardev_class = class_create(THIS_MODULE, "myusbchardev");
    mychardev_class->dev_uevent = mychardev_uevent;

    cdev_init(&mychardev_data[0].cdev, &mychardev_fops);
    mychardev_data[0].cdev.owner = THIS_MODULE;
    cdev_add(&mychardev_data[0].cdev, MKDEV(dev_major, 0), 1);
    device_create(mychardev_class, NULL, MKDEV(dev_major, 0), NULL, "myusbchardev-0");
    printk(KERN_ERR "MYCHARDEV: Successfully loaded\n");
    return 0;
}

static void __exit mychardev_exit(void) {
    device_destroy(mychardev_class, MKDEV(dev_major, 0));
    class_unregister(mychardev_class);
    class_destroy(mychardev_class);
    unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);
    printk(KERN_ERR "MYCHARDEV: Successfully unloaded\n");
    usb_deregister(&usb_driver);
}

static int mychardev_open(struct inode *inode, struct file *file) {
    printk("MYCHARDEV: Device open\n");
    return 0;
}

static int mychardev_release(struct inode *inode, struct file *file) {
    printk("MYCHARDEV: Device close\n");
    return 0;
}

static long mychardev_ioctl(struct file *file, unsigned int cmd,
                            unsigned long arg) {
    printk("MYCHARDEV: Device ioctl\n");
    if (cmd == 1) { // resize
        int res = resize(nodePtr, arg);
        if (res) {
            return 1;
        }
    }
    return 0;
}

union Item {
   int32_t int32;
   uint8_t int8[4];
};

static ssize_t mychardev_read(struct file *file, char __user *buf, size_t count,
                              loff_t *offset) {
    printk("Reading device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));
    if (count > 4) {
        count = 4;
    }
    union Item *val = kmalloc(sizeof(union Item), GFP_KERNEL);
    if (val == NULL) {
        return -ENOSPC;
    }

    mutex_lock(&stack_mutex);
    int res = pop(nodePtr, &val->int32);
    mutex_unlock(&stack_mutex);
    int ret = count;
    if (res || copy_to_user(buf, val->int8, count)) {
        ret = -ENODATA;
    }
    kfree(val);
    return ret;
}

static ssize_t mychardev_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *offset) {
    size_t datalen = 1024, ncopied; // Arbitrary limit for kmalloc not to kill my system.
    printk("Writing device: %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

    if (count < datalen) {
        datalen = count;
    }
    int n_to_push = (datalen - 1) / 4 + 1; // Amount of 32 bit numbers to push
    uint8_t *databuf = kmalloc(n_to_push * 4 * sizeof(uint8_t), GFP_KERNEL);;
    uint8_t *databuf_root = databuf;
    if (databuf == NULL) {
        return -ENOSPC;
    }
    memset(databuf, 0, n_to_push * 4 * sizeof(uint8_t));

    ncopied = copy_from_user(databuf, buf, datalen);
    if (!ncopied == 0) {
        printk("Could't copy %zd bytes from the user\n", ncopied);
        kfree(databuf_root);
        return -ENOSPC;
    }
    printk("Data from the user: %s\n", databuf);

    int i;
    int partial_success = 0;
    for (i = 0; i < n_to_push; i++) {
        mutex_lock(&stack_mutex);
        int res = push(nodePtr, (int32_t *)databuf);
        mutex_unlock(&stack_mutex);
        if (!res) {
            partial_success = 1;
        }
        else { // Stack is full
            break;
        }
        databuf+=4;
    }

    size_t ret = count;
    if (!partial_success) {
        printk("Failed to push data onto stack");
        ret = -ENOSPC;
    }

    kfree(databuf_root);
    return datalen;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniil Latypov <d.latypov@innopolis.university>");

module_init(mychardev_init);
module_exit(mychardev_exit);
