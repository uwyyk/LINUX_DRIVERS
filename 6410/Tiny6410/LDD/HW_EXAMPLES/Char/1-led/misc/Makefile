KERNEL_DIR = /home/kyyang/yan/kernel/linux-2.6.38_tiny6410

all:
	make -C $(KERNEL_DIR) M=`pwd` modules

clean:
	make -C $(KERNEL_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= tiny6410_misc_leds.o

