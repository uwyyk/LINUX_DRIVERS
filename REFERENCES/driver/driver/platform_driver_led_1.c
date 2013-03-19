#include<linux/device.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/string.h>
#include<linux/platform_device.h>
#include <asm/uaccess.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include<linux/fs.h>
#include<linux/mm.h>
#include<linux/io.h>
MODULE_AUTHOR("Harry Dong");
MODULE_LICENSE("Dual BSD/GPL");

int major = 200;
int minor = 0;

struct cdev dev;
struct resource *platform_resource = NULL;
volatile unsigned int *gpbcon_phy = NULL;
volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;

void led_all_off(void)
{
	*gpbdat |= (0x1<<5) | (0x1<<6) | (0x1<<7) | (0x1<<8);
}

void led_all_on(void)
{
	*gpbdat &= ~((1<<5) | (1<<6) | (1<<7) | (1<<8));
}

void dyf_led_init(void)
{
	*gpbcon |= (1<<10) | (1<<12) | (1<<14) | (1<<16); 
}

void led_on_or_off(unsigned long lednum,unsigned int on_or_off)
{
	led_all_off();
	switch(lednum)
	{
		case 1:if(on_or_off == 0)
					*gpbdat &= ~(1<<5);
				else
					*gpbdat |= (1<<5);
				break;
		case 2:if(on_or_off == 0)
					*gpbdat &= ~(1<<6);
				else
					*gpbdat |= (1<<6);
				break;
		case 3:if(on_or_off == 0)
					*gpbdat &= ~(1<<7);
				else
					*gpbdat |= (1<<7);
				break;
		case 4:if(on_or_off == 0)
					*gpbdat &= ~(1<<8);
				else
					*gpbdat |= (1<<8);
				break;
		default:break;
	}
}
int led_open(struct inode *node,struct file *myfile)
{
	led_all_on();
	return 0;
}

int led_release(struct inode *node,struct file *myfile)
{
	led_all_off();
	return 0;
}

int led_ioctl(struct inode *node,struct file *myfile,unsigned int cmd,unsigned long args)
{
	led_on_or_off(args,cmd);
	return 0;
}

struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.ioctl = led_ioctl,
}; 

void cdev_setup(void)
{
		int err;
		dev_t devno;
		
		devno = MKDEV(major,minor);
		cdev_init(&dev,&dev_fops);
		dev.owner = THIS_MODULE;
		err = cdev_add(&dev,devno,1);
		if(err)
			printk("Error %d adding dev!\n",err);
}

static int platform_led_probe(struct platform_device *pdev)
{
	int result;
	dev_t devno;
	
	devno = MKDEV(major,minor);
	if(major)
		result = register_chrdev_region(devno,1,"led-dyf");
	else{
		result = alloc_chrdev_region(&devno,0,1,"led-dyf");
		major = MAJOR(devno);
	}
	
	if(result < 0){
		printk("driver init unsuccess!\n");
		return result;
	}
	
	platform_resource = platform_get_resource(pdev,IORESOURCE_MEM,0);
	gpbcon_phy = (volatile unsigned int *)platform_resource->start;
	gpbcon = (volatile unsigned long *)ioremap((unsigned int)gpbcon_phy,8);
	gpbdat = gpbcon + 1;
	
	cdev_setup();
	dyf_led_init();
	led_all_off();
	
	printk("platform_led_driver is init successfull!\n");
	
	return 0;
}

static int platform_led_remove(struct platform_device *pdev)
{
	iounmap(gpbcon);
	cdev_del(&dev);
	unregister_chrdev_region(MKDEV(major,minor),1);
	printk("platform_led_driver is unregister successfull!\n");
	
	return 0;
}

static struct platform_driver platform_led_driver = {
	.probe = platform_led_probe,
	.remove = platform_led_remove,
	.driver ={
		.owner = THIS_MODULE,
		.name = "platform-led"
	},
};

static int __init my_driver_init(void)
{
	//×¢²áÆ½Ì¨Çý¶¯
	return platform_driver_register(&platform_led_driver);
}

static void my_driver_exit(void)
{
	platform_driver_unregister(&platform_led_driver);
}

module_init(my_driver_init);
module_exit(my_driver_exit);