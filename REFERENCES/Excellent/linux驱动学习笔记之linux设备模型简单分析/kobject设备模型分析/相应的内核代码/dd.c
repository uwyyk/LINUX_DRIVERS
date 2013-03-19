/*
 * drivers/base/dd.c - The core device/driver interactions.
 *
 * This file contains the (sometimes tricky) code that controls the
 * interactions between devices and drivers, which primarily includes
 * driver binding and unbinding.
 *
 * All of this code used to exist in drivers/base/bus.c, but was
 * relocated to here in the name of compartmentalization (since it wasn't
 * strictly code just for the 'struct bus_type'.
 *
 * Copyright (c) 2002-5 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2007 Greg Kroah-Hartman <gregkh@suse.de>
 * Copyright (c) 2007 Novell Inc.
 *
 * This file is released under the GPLv2
 */

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/async.h>

#include "base.h"
#include "power/power.h"

/* 该函数来将驱动程序的一些数据信息加入到dev对象中 */
static void driver_bound(struct device *dev)
{
	if (klist_node_attached(&dev->p->knode_driver)) {
		printk(KERN_WARNING "%s: device %s already bound\n",
			__func__, kobject_name(&dev->kobj));
		return;
	}

	pr_debug("driver: '%s': %s: bound to device '%s'\n", dev_name(dev),
		 __func__, dev->driver->name);

	if (dev->bus)
		blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
					     BUS_NOTIFY_BOUND_DRIVER, dev);

	klist_add_tail(&dev->p->knode_driver, &dev->driver->p->klist_devices);
}

/* 该函数用来在sysfs文件系统中建立绑定的设备与驱动程序之间的链接符号文件 */
static int driver_sysfs_add(struct device *dev)
{
	int ret;

	ret = sysfs_create_link(&dev->driver->p->kobj, &dev->kobj,
			  kobject_name(&dev->kobj));
	if (ret == 0) {
		ret = sysfs_create_link(&dev->kobj, &dev->driver->p->kobj,
					"driver");
		if (ret)
			sysfs_remove_link(&dev->driver->p->kobj,
					kobject_name(&dev->kobj));
	}
	return ret;
}
/* 删除 在sysfs文件系统中建立绑定的设备与驱动程序之间的链接符号文件 */
static void driver_sysfs_remove(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv) {
		sysfs_remove_link(&drv->p->kobj, kobject_name(&dev->kobj));
		sysfs_remove_link(&dev->kobj, "driver");
	}
}

/**
 * device_bind_driver - bind a driver to one device.
 * @dev: device.
 *
 * Allow manual attachment of a driver to a device.
 * Caller must have already set @dev->driver.
 *
 * Note that this does not modify the bus reference count
 * nor take the bus's rwsem. Please verify those are accounted
 * for before calling this. (It is ok to call with no other effort
 * from a driver's probe() method.)
 *
 * This function must be called with @dev->sem held.
 */
 /* 将设备和驱动绑定 */
int device_bind_driver(struct device *dev)
{
	int ret;

	ret = driver_sysfs_add(dev);  /* 该函数用来在sysfs文件系统中建立绑定的设备与驱动程序直接的链接符号文件 */
	if (!ret)
		driver_bound(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(device_bind_driver);

static atomic_t probe_count = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(probe_waitqueue);

/* 该函数用于真正的探测实际的设备 */
static int really_probe(struct device *dev, struct device_driver *drv)
{
	int ret = 0;

	atomic_inc(&probe_count);
	pr_debug("bus: '%s': %s: probing driver %s with device %s\n",
		 drv->bus->name, __func__, drv->name, dev_name(dev));
	WARN_ON(!list_empty(&dev->devres_head));

	dev->driver = drv;  /* 将当前驱动程序对象drv赋值给dev->driver*/
	if (driver_sysfs_add(dev)) {
		printk(KERN_ERR "%s: driver_sysfs_add(%s) failed\n",
			__func__, dev_name(dev));
		goto probe_failed;
	}

	if (dev->bus->probe) {  /* 如果定义了探测设备的方法dev->bus->probe，则调用该方法来探测设备 */
		ret = dev->bus->probe(dev);  /* 用来探测当前的设备是否适合当前的驱动 */
		if (ret)
			goto probe_failed;
	} else if (drv->probe) {  /* 否则如果定义了 探测设备的方法drv->probe()，则调用该方法来探测设备*/
		ret = drv->probe(dev);  /* 用来探测当前的设备是否适合当前的驱动以及当前设备是否处于工作状态等 */
		if (ret)
			goto probe_failed;
	}

	driver_bound(dev);  /* 如果探测成功则最后调用该函数来将驱动程序的一些数据信息加入到dev对象中 */
	ret = 1;
	pr_debug("bus: '%s': %s: bound device %s to driver %s\n",
		 drv->bus->name, __func__, dev_name(dev), drv->name);
	goto done;

probe_failed:
	devres_release_all(dev);
	driver_sysfs_remove(dev);
	dev->driver = NULL;

	if (ret != -ENODEV && ret != -ENXIO) {
		/* driver matched but the probe failed */
		printk(KERN_WARNING
		       "%s: probe of %s failed with error %d\n",
		       drv->name, dev_name(dev), ret);
	}
	/*
	 * Ignore errors returned by ->probe so that the next driver can try
	 * its luck.
	 */
	ret = 0;
done:
	atomic_dec(&probe_count);
	wake_up(&probe_waitqueue);
	return ret;
}

/**
 * driver_probe_done
 * Determine if the probe sequence is finished or not.
 *
 * Should somehow figure out how to use a semaphore, not an atomic variable...
 */
 /* 确定探测函数队列是否探测完成 */
int driver_probe_done(void)
{
	pr_debug("%s: probe_count = %d\n", __func__,
		 atomic_read(&probe_count));
	if (atomic_read(&probe_count))
		return -EBUSY;
	return 0;
}

/**
 * wait_for_device_probe
 * Wait for device probing to be completed.
 */
 /* 等待设备探测完成，用于同步 */
void wait_for_device_probe(void)
{
	/* wait for the known devices to complete their probing */
	wait_event(probe_waitqueue, atomic_read(&probe_count) == 0);
	async_synchronize_full();
}
EXPORT_SYMBOL_GPL(wait_for_device_probe);

/**
 * driver_probe_device - attempt to bind device & driver together
 * @drv: driver to bind a device to
 * @dev: device to try to bind to the driver
 *
 * This function returns -ENODEV if the device is not registered,
 * 1 if the device is bound sucessfully and 0 otherwise.
 *
 * This function must be called with @dev->sem held.  When called for a
 * USB interface, @dev->parent->sem must be held as well.
 */
 /* 该函数试图尝试将设备dev和驱动drv绑定在一起 */
int driver_probe_device(struct device_driver *drv, struct device *dev)
{
	int ret = 0;

	if (!device_is_registered(dev))  /* 如果设备没有没有注册则返回错误码-ENODEV*/
		return -ENODEV;

	pr_debug("bus: '%s': %s: matched device %s with driver %s\n",
		 drv->bus->name, __func__, dev_name(dev), drv->name);

	ret = really_probe(dev, drv);   /*  */

	return ret;
}

/* 将驱动绑定到设备 */
static int __device_attach(struct device_driver *drv, void *data)
{
	struct device *dev = data;

	if (!driver_match_device(drv, dev))  /*  driver_match_device用来判断drv和dev是否匹配，该函数返回1则表明匹配
	                                                                 成功，不成功返回0*/
		return 0;

	return driver_probe_device(drv, dev);  /* 如果driver_match_device匹配成功，那么调用该函数将drv和dev进行绑定 */
}

/**
 * device_attach - try to attach device to a driver.
 * @dev: device.
 *
 * Walk the list of drivers that the bus has and call
 * driver_probe_device() for each pair. If a compatible
 * pair is found, break out and return.
 *
 * Returns 1 if the device was bound to a driver;
 * 0 if no matching device was found;
 * -ENODEV if the device is not registered.
 *
 * When called for a USB interface, @dev->parent->sem must be held.
 */
int device_attach(struct device *dev)  /* 将设备绑定到它的驱动程序上*/
{
	int ret = 0;

	down(&dev->sem);
	if (dev->driver) {   /* 如果dev->driver不为空，表明当前设备dev已经和它的驱动程序进行了绑定*/ 
		ret = device_bind_driver(dev);/* 则调用该函数在sysfs文件树中建立与其驱动程序之间的互联关系 */
		if (ret == 0)
			ret = 1;
		else {
			dev->driver = NULL;  /* 否则设置设备的驱动为空 */
			ret = 0;
		}
	} else { /* 否则dev->driver = NULL，则表明当前设备对象dev还没有和它的驱动程序绑定，此时需要遍
	                 历dev所在总线dev->bus上挂载的所有驱动程序对象 */
		ret = bus_for_each_drv(dev->bus, NULL, dev, __device_attach);  /* 遍历挂在总线上的所有驱动对象drv，调用__device_attach进行绑定*/
	}
	up(&dev->sem);
	return ret;
}
EXPORT_SYMBOL_GPL(device_attach);

/* 将参数dev表示的设备与data指向的驱动进行 匹配*/
static int __driver_attach(struct device *dev, void *data)
{
	struct device_driver *drv = data;

	/*
	 * Lock device and try to bind to it. We drop the error
	 * here and always return 0, because we need to keep trying
	 * to bind to devices and some drivers will return an error
	 * simply if it didn't support the device.
	 *
	 * driver_probe_device() will spit a warning if there
	 * is an error.
	 */

	if (!driver_match_device(drv, dev))  /* 匹配驱动drv和设备dev */
		return 0;

	if (dev->parent)	/* Needed for USB */
		down(&dev->parent->sem);
	down(&dev->sem);
	if (!dev->driver)
		driver_probe_device(drv, dev);  /* 探测设备并尝试将设备dev和驱动drv绑定 */
	up(&dev->sem);
	if (dev->parent)
		up(&dev->parent->sem);

	return 0;
}

/**
 * driver_attach - try to bind driver to devices.
 * @drv: driver.
 *
 * Walk the list of devices that the bus has on it and try to
 * match the driver with each one.  If driver_probe_device()
 * returns 0 and the @dev->driver is set, we've found a
 * compatible pair.
 */
int driver_attach(struct device_driver *drv)  /* 将某个总线上的设备和驱动进行绑定 */
{
	return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}
EXPORT_SYMBOL_GPL(driver_attach);

/*
 * __device_release_driver() must be called with @dev->sem held.
 * When called for a USB interface, @dev->parent->sem must be held as well.
 */
 /* 移除设备的驱动 */
static void __device_release_driver(struct device *dev)
{
	struct device_driver *drv;

	drv = dev->driver;
	if (drv) {
		driver_sysfs_remove(dev);

		if (dev->bus)
			blocking_notifier_call_chain(&dev->bus->p->bus_notifier,
						     BUS_NOTIFY_UNBIND_DRIVER,
						     dev);

		if (dev->bus && dev->bus->remove)
			dev->bus->remove(dev);
		else if (drv->remove)
			drv->remove(dev);
		devres_release_all(dev);
		dev->driver = NULL;
		klist_remove(&dev->p->knode_driver);
	}
}

/**
 * device_release_driver - manually detach device from driver.
 * @dev: device.
 *
 * Manually detach device from driver.
 * When called for a USB interface, @dev->parent->sem must be held.
 */
 /* 将驱动和设备分离 */
void device_release_driver(struct device *dev)
{
	/*
	 * If anyone calls device_release_driver() recursively from
	 * within their ->remove callback for the same device, they
	 * will deadlock right here.
	 */
	down(&dev->sem);
	__device_release_driver(dev);
	up(&dev->sem);
}
EXPORT_SYMBOL_GPL(device_release_driver);

/**
 * driver_detach - detach driver from all devices it controls.
 * @drv: driver.
 */
 /* 将驱动同它所控制的所有的设备分离 */
void driver_detach(struct device_driver *drv)
{
	struct device_private *dev_prv;
	struct device *dev;

	for (;;) {
		spin_lock(&drv->p->klist_devices.k_lock);
		if (list_empty(&drv->p->klist_devices.k_list)) {
			spin_unlock(&drv->p->klist_devices.k_lock);
			break;
		}
		dev_prv = list_entry(drv->p->klist_devices.k_list.prev,
				     struct device_private,
				     knode_driver.n_node);
		dev = dev_prv->device;
		get_device(dev);
		spin_unlock(&drv->p->klist_devices.k_lock);

		if (dev->parent)	/* Needed for USB */
			down(&dev->parent->sem);
		down(&dev->sem);
		if (dev->driver == drv)
			__device_release_driver(dev);
		up(&dev->sem);
		if (dev->parent)
			up(&dev->parent->sem);
		put_device(dev);
	}
}
