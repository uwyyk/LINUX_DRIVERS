#include<linux/device.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/string.h>
#include<linux/platform_device.h>

MODULE_AUTHOR("Harry Dong");
MODULE_LICENSE("Dual BSD/GPL");

static struct platform_device *platform_device_led;
struct resource platform_source_led[1] = {
	[0] = {
		.start = 0x56000010,
		.end = 0x56000014,
		.flags = IORESOURCE_MEM,
	},
};

static int __init platform_device_led_init(void)
{
	int ret = 0;
	
	//分配结构
	platform_device_led = platform_device_alloc("platform-led",-1);
	
	platform_device_led->num_resources = ARRAY_SIZE(platform_source_led);
	platform_device_led->resource = platform_source_led;
	//注册设备
	ret = platform_device_add(platform_device_led);
	if(ret)
		platform_device_put(platform_device_led);
	
	return ret;
}

static void platform_device_led_exit(void)
{
	platform_device_unregister(platform_device_led);
}

module_init(platform_device_led_init);
module_exit(platform_device_led_exit);