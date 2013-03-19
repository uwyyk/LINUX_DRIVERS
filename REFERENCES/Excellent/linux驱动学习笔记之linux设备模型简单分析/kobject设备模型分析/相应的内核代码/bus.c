/*
 * bus.c - bus driver management
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2007 Greg Kroah-Hartman <gregkh@suse.de>
 * Copyright (c) 2007 Novell Inc.
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/string.h>
#include "base.h"
#include "power/power.h"

#define to_bus_attr(_attr) container_of(_attr, struct bus_attribute, attr)
#define to_bus(obj) container_of(obj, struct bus_type_private, subsys.kobj)

/*
 * sysfs bindings for drivers
 */

#define to_drv_attr(_attr) container_of(_attr, struct driver_attribute, attr)


static int __must_check bus_rescan_devices_helper(struct device *dev,
						void *data);

static struct bus_type *bus_get(struct bus_type *bus)
{
	if (bus) {
		kset_get(&bus->p->subsys);
		return bus;
	}
	return NULL;
}

static void bus_put(struct bus_type *bus)   /* 减小bus的kset计数 */
{
	if (bus)
		kset_put(&bus->p->subsys);
}

static ssize_t drv_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct driver_attribute *drv_attr = to_drv_attr(attr);
	struct driver_private *drv_priv = to_driver(kobj);
	ssize_t ret = -EIO;

	if (drv_attr->show)
		ret = drv_attr->show(drv_priv->driver, buf);
	return ret;
}

static ssize_t drv_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct driver_attribute *drv_attr = to_drv_attr(attr);
	struct driver_private *drv_priv = to_driver(kobj);
	ssize_t ret = -EIO;

	if (drv_attr->store)
		ret = drv_attr->store(drv_priv->driver, buf, count);
	return ret;
}

static struct sysfs_ops driver_sysfs_ops = {
	.show	= drv_attr_show,
	.store	= drv_attr_store,
};

static void driver_release(struct kobject *kobj)
{
	struct driver_private *drv_priv = to_driver(kobj);

	pr_debug("driver: '%s': %s\n", kobject_name(kobj), __func__);
	kfree(drv_priv);
}

static struct kobj_type driver_ktype = {   /* driver的kobject的类型属性 */
	.sysfs_ops	= &driver_sysfs_ops,
	.release	= driver_release,
};

/*
 * sysfs bindings for buses
 */
static ssize_t bus_attr_show(struct kobject *kobj, struct attribute *attr,   /* 用户空间读属性文件时会最终调用到该函数 */
			     char *buf)
{
	struct bus_attribute *bus_attr = to_bus_attr(attr);
	struct bus_type_private *bus_priv = to_bus(kobj);
	ssize_t ret = 0;

	if (bus_attr->show)
		ret = bus_attr->show(bus_priv->bus, buf);
	return ret;
}

static ssize_t bus_attr_store(struct kobject *kobj, struct attribute *attr,   /* 用户空间写属性文件时会最终调用到该函数 */
			      const char *buf, size_t count)
{
	struct bus_attribute *bus_attr = to_bus_attr(attr);
	struct bus_type_private *bus_priv = to_bus(kobj);
	ssize_t ret = 0;

	if (bus_attr->store)
		ret = bus_attr->store(bus_priv->bus, buf, count);
	return ret;
}

static struct sysfs_ops bus_sysfs_ops = {   /* 总线属性文件操作函数 */
	.show	= bus_attr_show,
	.store	= bus_attr_store,
};

  /* 创建新总线属性文件 ，该属性文件实际上向用户空间提供了一种接口 使得用户程序可以通过文
     件的方式来显示某一内核对象的属性或者重新配置这一属性*/	                                                                                  
int bus_create_file(struct bus_type *bus, struct bus_attribute *attr)   
{
	int error;
	if (bus_get(bus)) {   /* 增加 bus的kset计数 同时返回bus*/
		error = sysfs_create_file(&bus->p->subsys.kobj, &attr->attr); /*创建一个属性文件， 如果该函数调用成功将使用attribute结
                                                                                                                   构中的名字创建文件并返回0，否则返回一个负的错误码 */
		bus_put(bus);  /* 减小bus的kset计数 */
	} else
		error = -EINVAL;
	return error;
}
EXPORT_SYMBOL_GPL(bus_create_file);

void bus_remove_file(struct bus_type *bus, struct bus_attribute *attr)  /* 删除总线属性文件 */
{
	if (bus_get(bus)) {
		sysfs_remove_file(&bus->p->subsys.kobj, &attr->attr);
		bus_put(bus);
	}
}
EXPORT_SYMBOL_GPL(bus_remove_file);

static struct kobj_type bus_ktype = {  /* 设置总线属性文件操作函数 */
	.sysfs_ops	= &bus_sysfs_ops,  /* 该函数主要用来显示(show)或者设置(store)  当前注册的bus在sysfs文件系统中属性 */
};

static int bus_uevent_filter(struct kset *kset, struct kobject *kobj)   /* 总线事件过滤函数 */
{
	struct kobj_type *ktype = get_ktype(kobj);

	if (ktype == &bus_ktype)   /* 如果要求发送uevent消息的kobj对象类型是bus_ktype则返回1表示产生事件，否则返回0表示
	                                              uevent消息将不会发送到用户空间*/
		return 1;
	return 0;
}

static struct kset_uevent_ops bus_uevent_ops = {   /* 设置总线热插拔事件处理函数 */
	.filter = bus_uevent_filter,
};

static struct kset *bus_kset;


#ifdef CONFIG_HOTPLUG
/* Manually detach a device from its associated driver. */
static ssize_t driver_unbind(struct device_driver *drv,  /* 将设备和驱动分离 */
			     const char *buf, size_t count)
{
	struct bus_type *bus = bus_get(drv->bus);
	struct device *dev;
	int err = -ENODEV;

	dev = bus_find_device_by_name(bus, NULL, buf);
	if (dev && dev->driver == drv) {
		if (dev->parent)	/* Needed for USB */
			down(&dev->parent->sem);
		device_release_driver(dev);
		if (dev->parent)
			up(&dev->parent->sem);
		err = count;
	}
	put_device(dev);
	bus_put(bus);
	return err;
}
static DRIVER_ATTR(unbind, S_IWUSR, NULL, driver_unbind);/* 该宏可用于创建和初始化驱动属性对象driver_attr_unbind*/

/*
 * Manually attach a device to a driver.
 * Note: the driver must want to bind to the device,
 * it is not possible to override the driver's id table.
 */
static ssize_t driver_bind(struct device_driver *drv,   /* 将设备 依附于驱动*/
			   const char *buf, size_t count)
{
	struct bus_type *bus = bus_get(drv->bus);
	struct device *dev;
	int err = -ENODEV;

	dev = bus_find_device_by_name(bus, NULL, buf);
	if (dev && dev->driver == NULL && driver_match_device(drv, dev)) {
		if (dev->parent)	/* Needed for USB */
			down(&dev->parent->sem);
		down(&dev->sem);
		err = driver_probe_device(drv, dev);
		up(&dev->sem);
		if (dev->parent)
			up(&dev->parent->sem);

		if (err > 0) {
			/* success */
			err = count;
		} else if (err == 0) {
			/* driver didn't accept device */
			err = -ENODEV;
		}
	}
	put_device(dev);
	bus_put(bus);
	return err;
}
static DRIVER_ATTR(bind, S_IWUSR, NULL, driver_bind);    /* 该宏可用于创建和初始化驱动属性对象driver_attr_bind*/

static ssize_t show_drivers_autoprobe(struct bus_type *bus, char *buf)  /* 显示自动探测设备 */
{
	return sprintf(buf, "%d\n", bus->p->drivers_autoprobe);
}

static ssize_t store_drivers_autoprobe(struct bus_type *bus,  /* 设置自动探测设备 */
				       const char *buf, size_t count)
{
	if (buf[0] == '0')
		bus->p->drivers_autoprobe = 0;
	else
		bus->p->drivers_autoprobe = 1;
	return count;
}

static ssize_t store_drivers_probe(struct bus_type *bus,    /* 匹配驱动 */
				   const char *buf, size_t count)
{
	struct device *dev;

	dev = bus_find_device_by_name(bus, NULL, buf);   /* 首先将用户空间输入和设备名称对应的设备与驱动匹配一次，
	                                                                                       若能匹配则返回设备*/
	if (!dev)
		return -ENODEV;
	if (bus_rescan_devices_helper(dev, NULL) != 0)
		return -EINVAL;
	return count;
}
#endif

static struct device *next_device(struct klist_iter *i)  /* 寻找下一个设备 */
{
	struct klist_node *n = klist_next(i);
	struct device *dev = NULL;
	struct device_private *dev_prv;

	if (n) {
		dev_prv = to_device_private_bus(n);
		dev = dev_prv->device;
	}
	return dev;
}

/**
 * bus_for_each_dev - device iterator.
 * @bus: bus type.
 * @start: device to start iterating from.
 * @data: data for the callback.
 * @fn: function to be called for each device.
 *
 * Iterate over @bus's list of devices, and call @fn for each,
 * passing it @data. If @start is not NULL, we use that device to
 * begin iterating from.
 *
 * We check the return of @fn each time. If it returns anything
 * other than 0, we break out and return that value.
 *
 * NOTE: The device that returns a non-zero value is not retained
 * in any way, nor is its refcount incremented. If the caller needs
 * to retain this data, it should do, and increment the reference
 * count in the supplied callback.
 */
/*
 *对设备和驱动程序的迭代：
 *在编写总线层代码会发现不得不对注册到总线的所有设备和驱动程序执行某些操作，为了操作注册
 *到总线的每个设备，可调用下面函数
 */
int bus_for_each_dev(struct bus_type *bus, struct device *start,/* 该函数迭代了在总线上的每个设备，将相关的device结构传递给
                                                                                                                fn，同时传递data值，如果start是NULL。 将从总线的第一个设备开始
                                                                                                                迭代，否则将从start后的第一个设备开始迭代。如果fn返回一个非
                                                                                                                0值，将停止迭代，而这个值也会从该函数返回*/
		     void *data, int (*fn)(struct device *, void *))
{
	struct klist_iter i;
	struct device *dev;
	int error = 0;

	if (!bus)
		return -EINVAL;

	klist_iter_init_node(&bus->p->klist_devices, &i,
			     (start ? &start->p->knode_bus : NULL));
	while ((dev = next_device(&i)) && !error)
		error = fn(dev, data);
	klist_iter_exit(&i);
	return error;
}
EXPORT_SYMBOL_GPL(bus_for_each_dev);

/**
 * bus_find_device - device iterator for locating a particular device.
 * @bus: bus type
 * @start: Device to begin with
 * @data: Data to pass to match function
 * @match: Callback function to check device
 *
 * This is similar to the bus_for_each_dev() function above, but it
 * returns a reference to a device that is 'found' for later use, as
 * determined by the @match callback.
 *
 * The callback should return 0 if the device doesn't match and non-zero
 * if it does.  If the callback returns non-zero, this function will
 * return to the caller and not iterate over any more devices.
 */
struct device *bus_find_device(struct bus_type *bus,   /* 在总线上寻找一个设备 */
			       struct device *start, void *data,
			       int (*match)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *dev;

	if (!bus)
		return NULL;

	klist_iter_init_node(&bus->p->klist_devices, &i,
			     (start ? &start->p->knode_bus : NULL));
	while ((dev = next_device(&i)))
		if (match(dev, data) && get_device(dev))
			break;
	klist_iter_exit(&i);
	return dev;
}
EXPORT_SYMBOL_GPL(bus_find_device);

static int match_name(struct device *dev, void *data)   /* 通过名字来匹配 */
{
	const char *name = data;

	return sysfs_streq(name, dev_name(dev));
}

/**
 * bus_find_device_by_name - device iterator for locating a particular device of a specific name
 * @bus: bus type
 * @start: Device to begin with
 * @name: name of the device to match
 *
 * This is similar to the bus_find_device() function above, but it handles
 * searching by a name automatically, no need to write another strcmp matching
 * function.
 */
struct device *bus_find_device_by_name(struct bus_type *bus,  /* 通过设备的名字在总线上寻找相应的设备 */
				       struct device *start, const char *name)
{
	return bus_find_device(bus, start, (void *)name, match_name);
}
EXPORT_SYMBOL_GPL(bus_find_device_by_name);

static struct device_driver *next_driver(struct klist_iter *i)  /* 在链表里寻找下一个驱动 */
{
	struct klist_node *n = klist_next(i);
	struct driver_private *drv_priv;

	if (n) {
		drv_priv = container_of(n, struct driver_private, knode_bus);
		return drv_priv->driver;
	}
	return NULL;
}

/**
 * bus_for_each_drv - driver iterator
 * @bus: bus we're dealing with.
 * @start: driver to start iterating on.
 * @data: data to pass to the callback.
 * @fn: function to call for each driver.
 *
 * This is nearly identical to the device iterator above.
 * We iterate over each driver that belongs to @bus, and call
 * @fn for each. If @fn returns anything but 0, we break out
 * and return it. If @start is not NULL, we use it as the head
 * of the list.
 *
 * NOTE: we don't return the driver that returns a non-zero
 * value, nor do we leave the reference count incremented for that
 * driver. If the caller needs to know that info, it must set it
 * in the callback. It must also be sure to increment the refcount
 * so it doesn't disappear before returning to the caller.
 */
int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,/* 该函数用于驱动程序的迭代，该函数的工作方式与上面的
                                                                                                                           函数相似， 只是它的工作对象是驱动程序*/
		     void *data, int (*fn)(struct device_driver *, void *))
{
	struct klist_iter i;
	struct device_driver *drv;
	int error = 0;

	if (!bus)
		return -EINVAL;

	klist_iter_init_node(&bus->p->klist_drivers, &i,
			     start ? &start->p->knode_bus : NULL);
	while ((drv = next_driver(&i)) && !error)
		error = fn(drv, data);
	klist_iter_exit(&i);
	return error;
}
EXPORT_SYMBOL_GPL(bus_for_each_drv);

static int device_add_attrs(struct bus_type *bus, struct device *dev)  /* 添加设备默认属性 */
{
	int error = 0;
	int i;

	if (!bus->dev_attrs)
		return 0;

	for (i = 0; attr_name(bus->dev_attrs[i]); i++) {
		error = device_create_file(dev, &bus->dev_attrs[i]);
		if (error) {
			while (--i >= 0)
				device_remove_file(dev, &bus->dev_attrs[i]);
			break;
		}
	}
	return error;
}

static void device_remove_attrs(struct bus_type *bus, struct device *dev)  /* 删除设备默认属性 */
{
	int i;

	if (bus->dev_attrs) {
		for (i = 0; attr_name(bus->dev_attrs[i]); i++)
			device_remove_file(dev, &bus->dev_attrs[i]);
	}
}

#ifdef CONFIG_SYSFS_DEPRECATED
static int make_deprecated_bus_links(struct device *dev)
{
	return sysfs_create_link(&dev->kobj,
				 &dev->bus->p->subsys.kobj, "bus");
}

static void remove_deprecated_bus_links(struct device *dev)
{
	sysfs_remove_link(&dev->kobj, "bus");
}
#else
static inline int make_deprecated_bus_links(struct device *dev) { return 0; }
static inline void remove_deprecated_bus_links(struct device *dev) { }
#endif

/**
 * bus_add_device - add device to bus
 * @dev: device being added
 *
 * - Add the device to its bus's list of devices.
 * - Create link to device's bus.
 */
int bus_add_device(struct device *dev)  /* 添加一个设备到总线 */
{
	struct bus_type *bus = bus_get(dev->bus);
	int error = 0;

	if (bus) {
		pr_debug("bus: '%s': add device %s\n", bus->name, dev_name(dev));
		error = device_add_attrs(bus, dev);
		if (error)
			goto out_put;
		error = sysfs_create_link(&bus->p->devices_kset->kobj,
						&dev->kobj, dev_name(dev));
		if (error)
			goto out_id;
		error = sysfs_create_link(&dev->kobj,
				&dev->bus->p->subsys.kobj, "subsystem");
		if (error)
			goto out_subsys;
		error = make_deprecated_bus_links(dev);
		if (error)
			goto out_deprecated;
	}
	return 0;

out_deprecated:
	sysfs_remove_link(&dev->kobj, "subsystem");
out_subsys:
	sysfs_remove_link(&bus->p->devices_kset->kobj, dev_name(dev));
out_id:
	device_remove_attrs(bus, dev);
out_put:
	bus_put(dev->bus);
	return error;
}

/**
 * bus_attach_device - add device to bus
 * @dev: device tried to attach to a driver
 *
 * - Add device to bus's list of devices.
 * - Try to attach to driver.
 */
void bus_attach_device(struct device *dev)  /* 将设备依附到总线上 */
{
	struct bus_type *bus = dev->bus;
	int ret = 0;

	if (bus) {
		if (bus->p->drivers_autoprobe)   /* 如果 设定了自动探测driver的标志位，则调用device_attach来试图将当前的设备绑定到
			                                               它的驱动程序上*/
			ret = device_attach(dev);
		WARN_ON(ret < 0);
		if (ret >= 0)
			klist_add_tail(&dev->p->knode_bus,
				       &bus->p->klist_devices);
	}
}

/**
 * bus_remove_device - remove device from bus
 * @dev: device to be removed
 *
 * - Remove symlink from bus's directory.
 * - Delete device from bus's list.
 * - Detach from its driver.
 * - Drop reference taken in bus_add_device().
 */
void bus_remove_device(struct device *dev)  /* 从总线上移除设备 */
{
	if (dev->bus) {
		sysfs_remove_link(&dev->kobj, "subsystem");
		remove_deprecated_bus_links(dev);
		sysfs_remove_link(&dev->bus->p->devices_kset->kobj,
				  dev_name(dev));
		device_remove_attrs(dev->bus, dev);
		if (klist_node_attached(&dev->p->knode_bus))
			klist_del(&dev->p->knode_bus);

		pr_debug("bus: '%s': remove device %s\n",
			 dev->bus->name, dev_name(dev));
		device_release_driver(dev);
		bus_put(dev->bus);
	}
}

static int driver_add_attrs(struct bus_type *bus, struct device_driver *drv)  /* 添加驱动的默认属性 */
{
	int error = 0;
	int i;

	if (bus->drv_attrs) {
		for (i = 0; attr_name(bus->drv_attrs[i]); i++) {
			error = driver_create_file(drv, &bus->drv_attrs[i]);
			if (error)
				goto err;
		}
	}
done:
	return error;
err:
	while (--i >= 0)
		driver_remove_file(drv, &bus->drv_attrs[i]);
	goto done;
}

static void driver_remove_attrs(struct bus_type *bus,   /* 删除驱动的默认属性 */
				struct device_driver *drv)
{
	int i;

	if (bus->drv_attrs) {
		for (i = 0; attr_name(bus->drv_attrs[i]); i++)
			driver_remove_file(drv, &bus->drv_attrs[i]);
	}
}

#ifdef CONFIG_HOTPLUG
/*
 * Thanks to drivers making their tables __devinit, we can't allow manual
 * bind and unbind from userspace unless CONFIG_HOTPLUG is enabled.
 */
static int __must_check add_bind_files(struct device_driver *drv)  /* 添加链接属性文件 */
{
	int ret;

	ret = driver_create_file(drv, &driver_attr_unbind);
	if (ret == 0) {
		ret = driver_create_file(drv, &driver_attr_bind);
		if (ret)
			driver_remove_file(drv, &driver_attr_unbind);
	}
	return ret;
}

static void remove_bind_files(struct device_driver *drv)  /* 删除与驱动相关的属性*/
{
	driver_remove_file(drv, &driver_attr_bind);
	driver_remove_file(drv, &driver_attr_unbind);
}

static BUS_ATTR(drivers_probe, S_IWUSR, NULL, store_drivers_probe);   /* 该宏可用于创建和初始化总线属性对象 bus_attr_drivers_probe*/
static BUS_ATTR(drivers_autoprobe, S_IWUSR | S_IRUGO,        
		show_drivers_autoprobe, store_drivers_autoprobe);    /* 该宏可用于创建和初始化总线属性对象 bus_attr_drivers_autoprobe*/

static int add_probe_files(struct bus_type *bus)   /* 创建bus_create_file和 drivers_autoprobe两个属性文件*/
{
	int retval;

	retval = bus_create_file(bus, &bus_attr_drivers_probe);  /* 调用BUS_ATTR(drivers_probe, S_IWUSR, NULL, store_drivers_probe)来创建和初始化总线属性，
	                                                                                            然后bus_create_file会为之生成属性文件drivers_probe*/
	if (retval)
		goto out;

	retval = bus_create_file(bus, &bus_attr_drivers_autoprobe); /* 调用BUS_ATTR(drivers_autoprobe, S_IWUSR | S_IRUGO, show_drivers_autoprobe, store_drivers_autoprobe);
	                                                                                                来创建和初始化总线属性，然后bus_create_file会为之生成属性文件drivers_autoprobe*/
	                                                                                            
	if (retval)
		bus_remove_file(bus, &bus_attr_drivers_probe);
out:
	return retval;
}

static void remove_probe_files(struct bus_type *bus)  /* 移除探测属性文件 */
{
	bus_remove_file(bus, &bus_attr_drivers_autoprobe);
	bus_remove_file(bus, &bus_attr_drivers_probe);
}
#else
static inline int add_bind_files(struct device_driver *drv) { return 0; }
static inline void remove_bind_files(struct device_driver *drv) {}
static inline int add_probe_files(struct bus_type *bus) { return 0; }
static inline void remove_probe_files(struct bus_type *bus) {}
#endif

static ssize_t driver_uevent_store(struct device_driver *drv,   /* 驱动事件处理函数 */
				   const char *buf, size_t count)
{
	enum kobject_action action;

	if (kobject_action_type(buf, count, &action) == 0)
		kobject_uevent(&drv->p->kobj, action);
	return count;
}
static DRIVER_ATTR(uevent, S_IWUSR, NULL, driver_uevent_store);  /* 该宏可用于创建和初始化驱动属性对象driver_attr_ uevent*/

/**
 * bus_add_driver - Add a driver to the bus.
 * @drv: driver.
 */
int bus_add_driver(struct device_driver *drv)  /* 添加一个驱动到总线 */
{
	struct bus_type *bus;
	struct driver_private *priv;
	int error = 0;

	bus = bus_get(drv->bus);  /* 获取指向总线的指针 */
	if (!bus)
		return -EINVAL;

	pr_debug("bus: '%s': add driver %s\n", bus->name, drv->name);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);  /* 为驱动的私有结构分配内存空间 */
	if (!priv) {
		error = -ENOMEM;
		goto out_put_bus;
	}
	klist_init(&priv->klist_devices, NULL, NULL);  /* 初始化priv->klist_devices */
	priv->driver = drv;
	drv->p = priv;
	priv->kobj.kset = bus->p->drivers_kset;  /* 由此可知将会在/sys/bus/drivers */
	error = kobject_init_and_add(&priv->kobj, &driver_ktype, NULL,   /* 将drv所对应的内核对象加入到sysfs文件树中，如此将在
		                                                                                                      /sys/bus/drivers目录下新建一个目录，其名称为drv->name*/
				     "%s", drv->name);
	if (error)
		goto out_unregister;

	if (drv->bus->p->drivers_autoprobe) {  /* 如果自动探测设备驱动标志位被设置则 */
		error = driver_attach(drv);           /* 将当前注册的驱动drv与该bus上所属的设备进行绑定 */
		if (error)
			goto out_unregister;
	}
	klist_add_tail(&priv->knode_bus, &bus->p->klist_drivers);   /* 初始化priv->knode_bus，并将它添加到bus->p->klist_drivers链表尾部 */
	module_add_driver(drv->owner, drv);  /* 为模块添加驱动 */

	error = driver_create_file(drv, &driver_attr_uevent);  /* 为驱动创建属性文件 uevent，在  /sys/bus/drivers/drv->name/目录下*/
	if (error) {
		printk(KERN_ERR "%s: uevent attr (%s) failed\n",
			__func__, drv->name);
	}
	error = driver_add_attrs(bus, drv);   /*为驱动添加属性文件  */
	if (error) {
		/* How the hell do we get out of this pickle? Give up */
		printk(KERN_ERR "%s: driver_add_attrs(%s) failed\n",
			__func__, drv->name);
	}
	error = add_bind_files(drv);   /* 添加链接属性文件 */
	if (error) {
		/* Ditto */
		printk(KERN_ERR "%s: add_bind_files(%s) failed\n",
			__func__, drv->name);
	}

	kobject_uevent(&priv->kobj, KOBJ_ADD);    /* 向用户空间发送KOBJ_ADD消息*/
	return 0;
out_unregister:
	kfree(drv->p);
	drv->p = NULL;
	kobject_put(&priv->kobj);
out_put_bus:
	bus_put(bus);
	return error;
}

/**
 * bus_remove_driver - delete driver from bus's knowledge.
 * @drv: driver.
 *
 * Detach the driver from the devices it controls, and remove
 * it from its bus's list of drivers. Finally, we drop the reference
 * to the bus we took in bus_add_driver().
 */
void bus_remove_driver(struct device_driver *drv)  /* 从已知的总线删去驱动 */
{
	if (!drv->bus)
		return;
       /* 删除相应的文件结构 */
	remove_bind_files(drv);  
	driver_remove_attrs(drv->bus, drv);
	driver_remove_file(drv, &driver_attr_uevent);
	klist_remove(&drv->p->knode_bus);
	pr_debug("bus: '%s': remove driver %s\n", drv->bus->name, drv->name);
	
	driver_detach(drv);   /* 将设备和驱动分离 */
	module_remove_driver(drv);
	kobject_put(&drv->p->kobj);  /* 减少引用计数 */
	bus_put(drv->bus);
}

/* Helper for bus_rescan_devices's iter */
static int __must_check bus_rescan_devices_helper(struct device *dev,   /* 帮助重新在总线上扫描，为设备匹配驱动 */
						  void *data)
{
	int ret = 0;

	if (!dev->driver) {
		if (dev->parent)	/* Needed for USB */
			down(&dev->parent->sem);
		ret = device_attach(dev);  /* 将设备和总线绑定 */
		if (dev->parent)
			up(&dev->parent->sem);
	}
	return ret < 0 ? ret : 0;
}

/**
 * bus_rescan_devices - rescan devices on the bus for possible drivers
 * @bus: the bus to scan.
 *
 * This function will look for devices on the bus with no driver
 * attached and rescan it against existing drivers to see if it matches
 * any by calling device_attach() for the unbound devices.
 */
int bus_rescan_devices(struct bus_type *bus)  /*在总线上扫描；重新为设备 匹配驱动*/
{
	return bus_for_each_dev(bus, NULL, NULL, bus_rescan_devices_helper);
}
EXPORT_SYMBOL_GPL(bus_rescan_devices);

/**
 * device_reprobe - remove driver for a device and probe for a new driver
 * @dev: the device to reprobe
 *
 * This function detaches the attached driver (if any) for the given
 * device and restarts the driver probing process.  It is intended
 * to use if probing criteria changed during a devices lifetime and
 * driver attachment should change accordingly.
 */
int device_reprobe(struct device *dev)  /* 移除一个设备的驱动并且探测新驱动*/
{
	if (dev->driver) {  /* 如果设备有驱动则释放该驱动 */
		if (dev->parent)        /* Needed for USB */
			down(&dev->parent->sem);
		device_release_driver(dev);  /* 将该驱动和设备分离 */
		if (dev->parent)
			up(&dev->parent->sem);
	}
	return bus_rescan_devices_helper(dev, NULL);  /* 重新扫描总线驱动，为设备匹配驱动 */
}
EXPORT_SYMBOL_GPL(device_reprobe);

/**
 * find_bus - locate bus by name.
 * @name: name of bus.
 *
 * Call kset_find_obj() to iterate over list of buses to
 * find a bus by name. Return bus if found.
 *
 * Note that kset_find_obj increments bus' reference count.
 */
#if 0
struct bus_type *find_bus(char *name)
{
	struct kobject *k = kset_find_obj(bus_kset, name);
	return k ? to_bus(k) : NULL;
}
#endif  /*  0  */


/**
 * bus_add_attrs - Add default attributes for this bus.
 * @bus: Bus that has just been registered.
 */

static int bus_add_attrs(struct bus_type *bus)   /* 为总线添加默认的属性 */
{
	int error = 0;
	int i;

	if (bus->bus_attrs) {
		for (i = 0; attr_name(bus->bus_attrs[i]); i++) {
			error = bus_create_file(bus, &bus->bus_attrs[i]);
			if (error)
				goto err;
		}
	}
done:
	return error;
err:
	while (--i >= 0)
		bus_remove_file(bus, &bus->bus_attrs[i]);
	goto done;
}

static void bus_remove_attrs(struct bus_type *bus)  /* 移除bus默认属性 */
{
	int i;

	if (bus->bus_attrs) {
		for (i = 0; attr_name(bus->bus_attrs[i]); i++)
			bus_remove_file(bus, &bus->bus_attrs[i]);
	}
}

static void klist_devices_get(struct klist_node *n)   /* 从 设备节点中获取指向设备的指针，增加设备引用计数*/
{
	struct device_private *dev_prv = to_device_private_bus(n);   /* 获取设备私有结构 */
	struct device *dev = dev_prv->device;

	get_device(dev);   /* 增加设备引用计数同时返回指向设备的指针 */
}

static void klist_devices_put(struct klist_node *n)   /* 从 设备节点中获取指向设备的指针，减小引用设备计数*/
{
	struct device_private *dev_prv = to_device_private_bus(n);
	struct device *dev = dev_prv->device;

	put_device(dev);  /* 减小设备引用计数同时返回指向设备的指针  */
}

static ssize_t bus_uevent_store(struct bus_type *bus,   /* 总线事件处理函数 */
				const char *buf, size_t count)
{
	enum kobject_action action;

	if (kobject_action_type(buf, count, &action) == 0)
		kobject_uevent(&bus->p->subsys.kobj, action);     /* 热插拔事件处理函数--------通知用户空间*/
	return count;
}
static BUS_ATTR(uevent, S_IWUSR, NULL, bus_uevent_store);    /* 该宏可用于创建和初始化总线属性对象bus_attr_uevent*/

/**
 * bus_register - register a bus with the system.
 * @bus: bus.
 *
 * Once we have that, we registered the bus with the kobject
 * infrastructure, then register the children subsystems it has:
 * the devices and drivers that belong to the bus.
 */
 /* 该函数注册了一个总线类型，创建对应的文件系统(包括目录和属性)，初始化总线上的驱动和设备；
      这样，我们就可以通过内核提供的函数往总线上注册设备和驱动了*/
int bus_register(struct bus_type *bus)/* 总线的注册(为设备和驱动注册一个总线类型):设置好struct bus_type后，使用该函数注册
                                                                   （注意只有非常少的 bus_type成员需要初始化，它们中的大多数都由设备 模型核心所
                                                                   控制，但是必须为总线指定名字、match、uevent等一些必要的成员））*/
                                                                   
{
	int retval;
	struct bus_type_private *priv; 

	priv = kzalloc(sizeof(struct bus_type_private), GFP_KERNEL); /* 分配一个总线私有结构 */
	if (!priv)
		return -ENOMEM;
       /* 初始化私有结构成员 */
	priv->bus = bus;  
	bus->p = priv;  /* 将bus的成员指针p指向该私有结构 */

	BLOCKING_INIT_NOTIFIER_HEAD(&priv->bus_notifier);

	retval = kobject_set_name(&priv->subsys.kobj, "%s", bus->name);  /* priv->subsys.kobj.name= bus->name*/
	if (retval)
		goto out;
        /* 设置bus的kobject */
	priv->subsys.kobj.kset = bus_kset;    /*  表明了当前注册的bus对象所属的上层kset对象就是buses_init()中创建的名为"bus"的kset
	                                                               (bus_kset是系统启动时创建的，系统init进程kernel_init()中调用do_basic_setup()，其调用
		                                                        driver_init()，其又调用buses_init())*/
	priv->subsys.kobj.ktype = &bus_ktype;  /* 表明当前注册的bus的属性类型bus_ktype */
	priv->drivers_autoprobe = 1;  /* 表示当系统的某一总线中注册某一设备或者驱动的时候，进行设备与
	                                                 与驱动的绑定操作*/

	retval = kset_register(&priv->subsys);  /* 注册kset ，会在/sys/bus目录下为当前注册的bus生成新的目录，目录名为bus->name
						                              (因为priv->subsys.kobj.parent=NULL并且priv->subsys.kobj.kset = bus_kset)*/
	if (retval)
		goto out;

	retval = bus_create_file(bus, &bus_attr_uevent);   /*  会在/sys/bus/xxx/下创建uevent属性文件 ，该属性文件实际上向用户空间提供了一种接口
	                                                                                  使得用户程序可以通过文件的方式来显示某一内核对象的属性或者重
	                                                                                  新配置这一属性*/
	if (retval)
		goto bus_uevent_fail;
       /* 创建两个kset，其内嵌kobject的parent都指向priv->subsys.kobj ，这意味着将在当前正在向系统注册的新bus目录下
	    产生两个kset目录，分别对应新bus的devices和drivers*/
	priv->devices_kset = kset_create_and_add("devices", NULL,   /* 动态的创建一个 struct kset 结构并把它添加到sysfs中即/sys/bus/xxx/devices,xxx为我们注册的新总线*/  
						 &priv->subsys.kobj);
	if (!priv->devices_kset) {
		retval = -ENOMEM;
		goto bus_devices_fail;
	}

	priv->drivers_kset = kset_create_and_add("drivers", NULL,   /*  动态的创建一个 struct kset 结构并把它添加到sysfs中即/sys/bus/xxx/drivers*/
						 &priv->subsys.kobj);
	if (!priv->drivers_kset) {
		retval = -ENOMEM;
		goto bus_drivers_fail;
	}

	klist_init(&priv->klist_devices, klist_devices_get, klist_devices_put);   /* 初始化总线上的设备的链表 */
	klist_init(&priv->klist_drivers, NULL, NULL);   /* 初始化总线上的总线的链表 */                      

	retval = add_probe_files(bus);   /* 会在/sys/bus/xxx/下创建drivers_probe和drivers_autoprobe两个属性文件(比如ls  /sys/bus/platform/ -l可看到
	                                                        确实存在这些文件，我们可cat和echo命令来读取和修改这些属性文件)*/
	if (retval)
		goto bus_probe_files_fail;

	retval = bus_add_attrs(bus);   /* 为总线添加默认的属性 文件*/
	if (retval)
		goto bus_attrs_fail;

	pr_debug("bus: '%s': registered\n", bus->name);
	return 0;

bus_attrs_fail:
	remove_probe_files(bus);
bus_probe_files_fail:
	kset_unregister(bus->p->drivers_kset);
bus_drivers_fail:
	kset_unregister(bus->p->devices_kset);
bus_devices_fail:
	bus_remove_file(bus, &bus_attr_uevent);
bus_uevent_fail:
	kset_unregister(&bus->p->subsys);
	kfree(bus->p);
out:
	bus->p = NULL;
	return retval;
}
EXPORT_SYMBOL_GPL(bus_register);

/**
 * bus_unregister - remove a bus from the system
 * @bus: bus.
 *
 * Unregister the child subsystems and the bus itself.
 * Finally, we call bus_put() to release the refcount
 */
void bus_unregister(struct bus_type *bus)  /* 总线的注销 */
{
	pr_debug("bus: '%s': unregistering\n", bus->name);
	bus_remove_attrs(bus);   /* 删除总线默认属性 */
	remove_probe_files(bus);  /* 删除bus_attr_drivers_probe和 bus_attr_drivers_autoprobe两个属性文件*/
	kset_unregister(bus->p->drivers_kset);  /* 注销 驱动的kset*/
	kset_unregister(bus->p->devices_kset); /* 注销设备的kset */
	bus_remove_file(bus, &bus_attr_uevent); /* 删除总线属性文件 */
	kset_unregister(&bus->p->subsys);    /* 注销子系统 */
	kfree(bus->p);  /* 释放空间 */
	bus->p = NULL;
}
EXPORT_SYMBOL_GPL(bus_unregister);

int bus_register_notifier(struct bus_type *bus, struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&bus->p->bus_notifier, nb);
}
EXPORT_SYMBOL_GPL(bus_register_notifier);

int bus_unregister_notifier(struct bus_type *bus, struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&bus->p->bus_notifier, nb);
}
EXPORT_SYMBOL_GPL(bus_unregister_notifier);

struct kset *bus_get_kset(struct bus_type *bus)
{
	return &bus->p->subsys;
}
EXPORT_SYMBOL_GPL(bus_get_kset);

struct klist *bus_get_device_klist(struct bus_type *bus)
{
	return &bus->p->klist_devices;
}
EXPORT_SYMBOL_GPL(bus_get_device_klist);

/*
 * Yes, this forcably breaks the klist abstraction temporarily.  It
 * just wants to sort the klist, not change reference counts and
 * take/drop locks rapidly in the process.  It does all this while
 * holding the lock for the list, so objects can't otherwise be
 * added/removed while we're swizzling.
 */
static void device_insertion_sort_klist(struct device *a, struct list_head *list,
					int (*compare)(const struct device *a,
							const struct device *b))
{
	struct list_head *pos;
	struct klist_node *n;
	struct device_private *dev_prv;
	struct device *b;

	list_for_each(pos, list) {
		n = container_of(pos, struct klist_node, n_node);
		dev_prv = to_device_private_bus(n);
		b = dev_prv->device;
		if (compare(a, b) <= 0) {
			list_move_tail(&a->p->knode_bus.n_node,
				       &b->p->knode_bus.n_node);
			return;
		}
	}
	list_move_tail(&a->p->knode_bus.n_node, list);
}

void bus_sort_breadthfirst(struct bus_type *bus,
			   int (*compare)(const struct device *a,
					  const struct device *b))
{
	LIST_HEAD(sorted_devices);
	struct list_head *pos, *tmp;
	struct klist_node *n;
	struct device_private *dev_prv;
	struct device *dev;
	struct klist *device_klist;

	device_klist = bus_get_device_klist(bus);

	spin_lock(&device_klist->k_lock);
	list_for_each_safe(pos, tmp, &device_klist->k_list) {
		n = container_of(pos, struct klist_node, n_node);
		dev_prv = to_device_private_bus(n);
		dev = dev_prv->device;
		device_insertion_sort_klist(dev, &sorted_devices, compare);
	}
	list_splice(&sorted_devices, &device_klist->k_list);
	spin_unlock(&device_klist->k_lock);
}
EXPORT_SYMBOL_GPL(bus_sort_breadthfirst);

/* 初始化总线，它是总线在系统中的起源，在系统初始化阶段，就通过调用该函数为系统中后续的bus操作
    奠定了基础(系统init进程kernel_init()中调用do_basic_setup()，其调用driver_init()，其又调用buses_init()) */
int __init buses_init(void)   
		                                                        
{
	bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL);  /* 动态的创建一个 struct kset 结构并把它添加到sysfs中， 从而知道
	                                                                                                               创建的文件系统在/sys/bus(注意这里的bus_uevent_ops定义了当"bus"
	                                                                                                               这个kset中有变化时，用来通知用户空间uevent消息的操作集(
	                                                                                                               我们前面已经讨论过，当某个kset中的状态变化时，如果需要
	                                                                                                               向用户空间发送uevent消息，将由kset的最顶层kset来执行，因为
	                                                                                                               bus_kset是系统中所有bus subsystem最顶层的kset，所以bus中uevent调用
	                                                                                                               会最终汇集到这里的bus_uevent_ops中))*/
	if (!bus_kset)
		return -ENOMEM;
	return 0;
}
