

static struct miscdevice tiny210_leds_misc = {
    
};

static int __init tiny210_leds_init(void)
{
    
    return 0;
}

static void __exit tiny210_leds_exit(void)
{
}

module_init(tiny210_leds_init);
module_exit(tiny210_leds_exit);

MODULE_LICENSE("GPL");

