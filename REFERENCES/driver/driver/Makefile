KERNELDIR = /home/dongyf/linux/linux-2.6.35
PWD := $(shell pwd)
INSTALLDIR = /home/mylib/modules

CROSS_COMPILE    =/usr/4.4.3/bin/arm-linux-
CC    = $(CROSS_COMPILE)gcc

obj-m := platform_driver_led_1.o platform_device_led.o

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	cp platform_driver_led.ko platform_device_led.ko $(INSTALLDIR)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions module* Module.symvers

.PHONY:
	modules modules_install clean
