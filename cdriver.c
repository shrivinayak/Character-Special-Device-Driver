#include <linux/module.h>	/* for modules */
#include <linux/fs.h>		/* file_operations */
#include <linux/uaccess.h>	/* copy_(to,from)_user */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/slab.h>		/* kmalloc */
#include <linux/cdev.h>	
#include <linux/moduleparam.h>
#include <linux/string.h> 
#include <linux/kernel.h> 
#include <linux/device.h>

#define default_NUM_DEVICES 5
#define DRIVER_NAME "mycdrv"


#define MYCDEV_IOC_MAGIC  'Z'
#define ASP_CLEAR_BUF    _IOW(MYCDEV_IOC_MAGIC, 1,int)


struct mycdev {
	struct cdev dev;
	char *ramdisk;
	loff_t ramdisk_size;
	struct semaphore sem;
}*mycdev_gbl;

dev_t dev_no=0;

static loff_t ramdisk_size = PAGE_SIZE*16;


static struct class *mycdev_class = NULL;
static unsigned int NUM_DEVICES = default_NUM_DEVICES;

module_param(NUM_DEVICES, int, S_IRUGO);

static ssize_t mycdev_read(struct file *flip, char *buffer, size_t len, loff_t *offset);
static ssize_t mycdev_write(struct file *flip, const char *buffer, size_t len, loff_t *offset);
static int mycdev_open(struct inode *inode, struct file *file);
static int mycdev_release(struct inode *inode, struct file *file);
static loff_t mycdev_lseek(struct file *file, loff_t  offset,int orig);
static long mycdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);



struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = mycdev_open,
	.release = mycdev_release,
	.write = mycdev_write,
	.read = mycdev_read,
	.llseek = mycdev_lseek,
	.unlocked_ioctl = mycdev_ioctl,
};




static int mycdev_init(void){

	char temp[10];

	int i,d =0;
	int len;
	if (NUM_DEVICES <= 0){
		printk(KERN_WARNING "Invalid value for NUM_DEVICESices: %d\n", NUM_DEVICES);
		d = -EINVAL;
		return d;
	}


	/*create major number and range of minor numbers starting from 0*/
	d = alloc_chrdev_region(&dev_no, 0, NUM_DEVICES, DRIVER_NAME);
	
	/*if creation of device nodes failed*/
	if (d < 0) {
		printk(KERN_WARNING "alloc_chrdev_region() failed\n");
		return d;
	}
	printk(KERN_WARNING "alloc_chrdev_region succedded major_number=%d",MAJOR(dev_no));
	
	mycdev_gbl = (struct mycdev *) kmalloc(NUM_DEVICES * sizeof(struct mycdev),GFP_KERNEL); 
	for(i=0;i<NUM_DEVICES;i++){

		cdev_init(&mycdev_gbl[i].dev,&fops);
		cdev_add(&mycdev_gbl[i].dev,MKDEV(MAJOR(dev_no),MINOR(dev_no)+i),1);

		sema_init(&mycdev_gbl[i].sem, 1);

		mycdev_gbl[i].ramdisk_size = ramdisk_size;
		mycdev_gbl[i].ramdisk = (char *) kmalloc(mycdev_gbl[i].ramdisk_size, GFP_KERNEL);
		memset(mycdev_gbl[i].ramdisk,0,sizeof(char)*mycdev_gbl[i].ramdisk_size);

	}

	

	/*create device class for the range of devices*/
	mycdev_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(mycdev_class)) {
		d = PTR_ERR(mycdev_class);
		goto failure;
	}

	for(i=0;i<NUM_DEVICES;i++){

		strcpy(temp,DRIVER_NAME);
		len=strlen(temp);
		temp[len]='0'+i;
		temp[len+1]='\0'; 
		device_create(mycdev_class,NULL,MKDEV(MAJOR(dev_no),MINOR(dev_no)+i),NULL,temp);
	}

	printk(KERN_INFO "mycdev driver loaded!\n");

	return 0;

	failure:
		return d;
}

static void mycdev_exit(void)
{	int i;


	for(i=0;i<NUM_DEVICES;i++){
		kfree(mycdev_gbl[i].ramdisk);
		cdev_del(&mycdev_gbl[i].dev);

		device_destroy(mycdev_class,MKDEV(MAJOR(dev_no),MINOR(dev_no)+i));
	}
	if(mycdev_class){
		class_destroy(mycdev_class);
	}

	unregister_chrdev_region(dev_no,NUM_DEVICES);

	kfree(mycdev_gbl);
	printk(KERN_INFO "mycdev driver unloaded!\n");

}


static int mycdev_open(struct inode *inode, struct file *file) {


	struct mycdev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct mycdev, dev);

	file->private_data = dev;

 	printk(KERN_INFO ">mycdev_open\n");
	
 return 0;
}

static int mycdev_release(struct inode *inode, struct file *file) {
	 printk(KERN_INFO ">mycdev_release\n");

	
 return 0;
}


static ssize_t mycdev_write(struct file *file, const char __user * buf, size_t lbuf,loff_t * ppos){ 

	struct mycdev * mycdev_ptr = file->private_data;

 	loff_t nbytes;

	if(down_interruptible(&mycdev_ptr->sem))
		return 0;


 	printk(KERN_INFO ">mycdev_write\n");
 
	if ((lbuf + *ppos) > mycdev_ptr->ramdisk_size) {
		pr_info("trying to write past end of device,"
			"aborting because this is just a stub!\n");
		up(&mycdev_ptr->sem);
	
		return 0;
	}

	nbytes = lbuf - copy_from_user(&mycdev_ptr-> ramdisk[*ppos], buf, lbuf);
	*ppos += nbytes;
	
	// printk(KERN_INFO "%s pointer=%lld",mycdev_ptr->ramdisk,*ppos);
	// pr_info("\n WRITING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);

	up(&mycdev_ptr->sem);



	return nbytes;
}


static ssize_t mycdev_read(struct file *file, char __user * buf, size_t lbuf, loff_t * ppos){ 

	struct mycdev * mycdev_ptr = file->private_data;
	loff_t nbytes;

	down_interruptible(&mycdev_ptr->sem);
	if(*ppos > mycdev_ptr->ramdisk_size)
		goto out; //EOF
 	
 	// printk(KERN_INFO ">mycdev_read\n");
 	
 	// printk(KERN_INFO "file->f_pos %d begin=%d offset=%lld ramdisk_size=%d\n",file->f_pos,lbuf,*ppos,mycdev_ptr->ramdisk_size);
	

	nbytes = lbuf - copy_to_user(buf, &mycdev_ptr->ramdisk[*ppos], lbuf);
	*ppos += nbytes;
	// pr_info("\n READING function, nbytes=%d, pos=%d\n", nbytes, (int)*ppos);
	up(&mycdev_ptr->sem);

	return nbytes;

	out:
	up(&mycdev_ptr->sem);
	return 0;

}


static loff_t mycdev_lseek(struct file *filp, loff_t offset, int orig){

	loff_t testpos;

	struct mycdev * mycdev_ptr = filp->private_data;

	down_interruptible(&mycdev_ptr->sem);

	printk(KERN_INFO ">mycdev_lseek offset=%lld orig=%d\n",offset,orig);

	switch (orig) {
		case SEEK_SET:
			testpos = offset;
			break;
		case SEEK_CUR:
			testpos = filp->f_pos + offset;
			break;
		case SEEK_END:
			testpos = mycdev_ptr->ramdisk_size + offset;
			break;
		default:
		up(&mycdev_ptr->sem);
		return -EINVAL;
	}


	if(testpos<0){
		// filp->f_pos = 0;
		up(&mycdev_ptr->sem);

		return -1;

	}
	if(testpos>mycdev_ptr->ramdisk_size){
		mycdev_ptr->ramdisk = krealloc((void *)mycdev_ptr->ramdisk,testpos,GFP_KERNEL);
		memset(mycdev_ptr->ramdisk+mycdev_ptr->ramdisk_size,0,sizeof(char)*(2*testpos-mycdev_ptr->ramdisk_size));
		mycdev_ptr->ramdisk_size=testpos*2;

	}
	filp->f_pos = testpos;
	pr_info("Seeking to pos=%ld\n", (long)testpos);
	up(&mycdev_ptr->sem);

	return testpos;
}


static long mycdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int err = 0;
  	
	struct mycdev * mycdev_ptr = file->private_data;
	// loff_t nbytes;

	down_interruptible(&mycdev_ptr->sem);


  	printk(KERN_WARNING "printing from ioctl");
	
	
    
	if (_IOC_TYPE(cmd) != MYCDEV_IOC_MAGIC) 
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {

	  case ASP_CLEAR_BUF:
	  	file->f_pos = 0;

	  	memset(mycdev_ptr->ramdisk,0,sizeof(char)*mycdev_ptr->ramdisk_size);
	  	printk(KERN_INFO "Ramdisk cleared");
		break;
        
	 
	  default:  
		up(&mycdev_ptr->sem);
		return -ENOTTY;
	}
	up(&mycdev_ptr->sem);
	return retval;

}

module_init(mycdev_init);
module_exit(mycdev_exit);
MODULE_AUTHOR("Shrivinayak Bhat");
MODULE_LICENSE("GPL");