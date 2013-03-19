/*
 * class.c - basic device class management
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2003-2004 Greg Kroah-Hartman
 * Copyright (c) 2003-2004 IBM Corp.
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/mutex.h>
#include "base.h"

#define to_class_attr(_attr) container_of(_attr, struct class_attribute, attr)

static ssize_t class_attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf)
{
	struct class_attribute *class_attr = to_class_attr(attr);
	struct class_private *cp = to_class(kobj);
	ssize_t ret = -EIO;

	if (class_attr->show)
		ret = class_attr->show(cp->class, buf);
	return ret;
}

static ssize_t class_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct class_attribute *class_attr = to_class_attr(attr);
	struct class_private *cp = to_class(kobj);
	ssize_t ret = -EIO;

	if (class_attr->store)
		ret = class_attr->store(cp->class, buf, count);
	return ret;
}

static void class_release(struct kobject *kobj)
{
	struct class_private *cp = to_class(kobj);
	struct class *class = cp->class;

	pr_debug("class '%s': release.\n", class->name);

	if (class->class_release)
		class->class_release(class);
	else
		pr_debug("class '%s' does not have a release() function, "
			 "be careful\n", class->name);
}

static struct sysfs_ops class_sysfs_ops = {  /* 设置设置类的kobject的类型属性文件的操作函数 */
	.show	= class_attr_show,
	.store	= class_attr_store,
};

static struct kobj_type class_ktype = {  /* 设置类的kobject的类型属性 */
	.sysfs_ops	= &class_sysfs_ops,
	.release	= class_release,
};

/* Hotplug events for classes go to the class class_subsys */
static struct kset *class_kset;


int class_create_file(struct class *cls, const struct class_attribute *attr)  /* 创建属于类的任何属性 */
{
	int error;
	if (cls)
		error = sysfs_create_file(&cls->p->class_subsys.kobj,
					  &attr->attr);
	else
		error = -EINVAL;
	return error;
}

void class_remove_file(struct class *cls, const struct class_attribute *attr) /* 删除属于类的任何属性	 */
{
	if (cls)
		sysfs_remove_file(&cls->p->class_subsys.kobj, &attr->attr);
}

static struct class *class_get(struct class *cls)
{
	if (cls)
		kset_get(&cls->p->class_subsys);
	return cls;
}

static void class_put(struct class *cls)
{
	if (cls)
		kset_put(&cls->p->class_subsys);
}

static int add_class_attrs(struct class *cls)
{
	int i;
	int error = 0;

	if (cls->class_attrs) {
		for (i = 0; attr_name(cls->class_attrs[i]); i++) {
			error = class_create_file(cls, &cls->class_attrs[i]);
			if (error)
				goto error;
		}
	}
done:
	return error;
error:
	while (--i >= 0)
		class_remove_file(cls, &cls->class_attrs[i]);
	goto done;
}

static void remove_class_attrs(struct class *cls)
{
	int i;

	if (cls->class_attrs) {
		for (i = 0; attr_name(cls->class_attrs[i]); i++)
			class_remove_file(cls, &cls->class_attrs[i]);
	}
}

static void klist_class_dev_get(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_class);

	get_device(dev);
}

static void klist_class_dev_put(struct klist_node *n)
{
	struct device *dev = container_of(n, struct device, knode_class);

	put_device(dev);
}

/* 该函数对传入的参数所代表的设备类进行部分字段的设置，包括设备类的属性、引用计数器等，然后将此
    设备类添加(注册)到linux内核系统中，设备类对应设备的设备文件，但该函数不会在目录/dev下生成设备文件，
    还需要将设备类传给逻辑设备，将逻辑设备注册进内核才会形成设备文件
    如果函数返回0则表示注册成功，否则返回-ENOMEM*/
int __class_register(struct class *cls, struct lock_class_key *key)
{
	struct class_private *cp;
	int error;

	pr_debug("device class '%s': registering\n", cls->name);

	cp = kzalloc(sizeof(*cp), GFP_KERNEL);  /* 分配一个struct class_private  ，代表class内核对象的kobject内嵌在struct class_private  数据
	                                                                 结构的class_subsys成员中*/
	if (!cp)
		return -ENOMEM;
	klist_init(&cp->class_devices, klist_class_dev_get, klist_class_dev_put);  /* 初始化cp->class_devices链表 */
	INIT_LIST_HEAD(&cp->class_interfaces);
	kset_init(&cp->class_dirs);
	__mutex_init(&cp->class_mutex, "struct class mutex", key);
	error = kobject_set_name(&cp->class_subsys.kobj, "%s", cls->name);   /* cp->class_subsys.kobj.name= cls->name*/
	if (error) {
		kfree(cp);
		return error;
	}

	/* set the default /sys/dev directory for devices of this class */
	/* 为这个类的设备设置默认的目录 */
	if (!cls->dev_kobj)  /* 如果cls->dev_kobj没有指定 */
		cls->dev_kobj = sysfs_dev_char_kobj;  /* 则cls->dev_kobj = sysfs_dev_char_kobj，可以看出会在/sys/dev/char  */

#if defined(CONFIG_SYSFS_DEPRECATED) && defined(CONFIG_BLOCK)
	/* let the block class directory show up in the root of sysfs */
	if (cls != &block_class)
		cp->class_subsys.kobj.kset = class_kset;
#else
	cp->class_subsys.kobj.kset = class_kset;  /* class_kset为系统中所有class对象的顶层kset，此处当前的class对象的kobj.kset指向
	                                                                     class_kset，意味着通过class_create生成的class在sysfs文件中的入口点(目录)将在
	                                                                     /sys/class目录下产生*/
#endif
	cp->class_subsys.kobj.ktype = &class_ktype;   /* 设置类的kobject类型属性*/
	cp->class = cls;
	cls->p = cp;

	error = kset_register(&cp->class_subsys);  /* 注册类的kset，将之前产生的class加入到系统中，这样将会在/sys/class目录下
	                                                                       生成新的目录 */
	if (error) {
		kfree(cp);
		return error;
	}
	error = add_class_attrs(class_get(cls));  /*添加类的属性 */
	class_put(cls);
	return error;
}
EXPORT_SYMBOL_GPL(__class_register);

/* 该函数用于删除设备的逻辑类(即注销设备的逻辑类) */
void class_unregister(struct class *cls)  
{
	pr_debug("device class '%s': unregistering\n", cls->name);
	remove_class_attrs(cls);
	kset_unregister(&cls->p->class_subsys);
}

static void class_create_release(struct class *cls)  /*  释放类所占用的资源*/
{
	pr_debug("%s called for %s\n", __func__, cls->name);
	kfree(cls);
}

/**
 * class_create - create a struct class structure
 * @owner: pointer to the module that is to "own" this struct class
 * @name: pointer to a string for the name of this class.
 * @key: the lock_class_key for this class; used by mutex lock debugging
 *
 * This is used to create a struct class pointer that can then be used
 * in calls to device_create().
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to class_destroy().
 */
 /* 该函数用于动态创建的逻辑类，并完成相应字段的初始化，然后将其添加进linux内核系统中，此函数的执行
     效果就是在目录/sys/class下创建一个新的文件夹，此文件夹的名字为此函数的第二个参数，但此文件夹为空。
     owner指向函数__class_create即将创建的struct class 类型对象的拥有者，一般赋值为THIS_MODULE
     name是即将创建的struct class变量的名字，用于给struct class的name字段赋值
     key代表访问struct class类型变量的互斥锁*/
struct class *__class_create(struct module *owner, const char *name,
			     struct lock_class_key *key)
{
	struct class *cls;
	int retval;

	cls = kzalloc(sizeof(*cls), GFP_KERNEL);  /* 分配一个 struct class 结构*/
	if (!cls) {
		retval = -ENOMEM;
		goto error;
	}
      /* 类的成员初始化 */
	cls->name = name;
	cls->owner = owner;
	cls->class_release = class_create_release;

	retval = __class_register(cls, key);   /* 向系统注册该新生成的类对象 */
	if (retval)
		goto error;

	return cls;

error:
	kfree(cls);
	return ERR_PTR(retval);
}
EXPORT_SYMBOL_GPL(__class_create);

/**
 * class_destroy - destroys a struct class structure
 * @cls: pointer to the struct class that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to class_create().
 */
 /* 该函数用于删除设备的逻辑类(从系统中注销一个class对象)，即从linux内核系统删除设备的逻辑类，此函
     数执行的效果是删除函数__class_create或宏class_create在目录/sys/class下创建的逻辑类对应的文件夹*/    
void class_destroy(struct class *cls) 
{
	if ((cls == NULL) || (IS_ERR(cls)))
		return;

	class_unregister(cls);
}

#ifdef CONFIG_SYSFS_DEPRECATED
char *make_class_name(const char *name, struct kobject *kobj)
{
	char *class_name;
	int size;

	size = strlen(name) + strlen(kobject_name(kobj)) + 2;

	class_name = kmalloc(size, GFP_KERNEL);
	if (!class_name)
		return NULL;

	strcpy(class_name, name);
	strcat(class_name, ":");
	strcat(class_name, kobject_name(kobj));
	return class_name;
}
#endif

/**
 * class_dev_iter_init - initialize class device iterator
 * @iter: class iterator to initialize
 * @class: the class we wanna iterate over
 * @start: the device to start iterating from, if any
 * @type: device_type of the devices to iterate over, NULL for all
 *
 * Initialize class iterator @iter such that it iterates over devices
 * of @class.  If @start is set, the list iteration will start there,
 * otherwise if it is NULL, the iteration starts at the beginning of
 * the list.
 */
void class_dev_iter_init(struct class_dev_iter *iter, struct class *class,
			 struct device *start, const struct device_type *type)
{
	struct klist_node *start_knode = NULL;

	if (start)
		start_knode = &start->knode_class;
	klist_iter_init_node(&class->p->class_devices, &iter->ki, start_knode);
	iter->type = type;
}
EXPORT_SYMBOL_GPL(class_dev_iter_init);

/**
 * class_dev_iter_next - iterate to the next device
 * @iter: class iterator to proceed
 *
 * Proceed @iter to the next device and return it.  Returns NULL if
 * iteration is complete.
 *
 * The returned device is referenced and won't be released till
 * iterator is proceed to the next device or exited.  The caller is
 * free to do whatever it wants to do with the device including
 * calling back into class code.
 */
struct device *class_dev_iter_next(struct class_dev_iter *iter)
{
	struct klist_node *knode;
	struct device *dev;

	while (1) {
		knode = klist_next(&iter->ki);
		if (!knode)
			return NULL;
		dev = container_of(knode, struct device, knode_class);
		if (!iter->type || iter->type == dev->type)
			return dev;
	}
}
EXPORT_SYMBOL_GPL(class_dev_iter_next);

/**
 * class_dev_iter_exit - finish iteration
 * @iter: class iterator to finish
 *
 * Finish an iteration.  Always call this function after iteration is
 * complete whether the iteration ran till the end or not.
 */
void class_dev_iter_exit(struct class_dev_iter *iter)
{
	klist_iter_exit(&iter->ki);
}
EXPORT_SYMBOL_GPL(class_dev_iter_exit);

/**
 * class_for_each_device - device iterator
 * @class: the class we're iterating
 * @start: the device to start with in the list, if any.
 * @data: data for the callback
 * @fn: function to be called for each device
 *
 * Iterate over @class's list of devices, and call @fn for each,
 * passing it @data.  If @start is set, the list iteration will start
 * there, otherwise if it is NULL, the iteration starts at the
 * beginning of the list.
 *
 * We check the return of @fn each time. If it returns anything
 * other than 0, we break out and return that value.
 *
 * @fn is allowed to do anything including calling back into class
 * code.  There's no locking restriction.
 */
int class_for_each_device(struct class *class, struct device *start,
			  void *data, int (*fn)(struct device *, void *))
{
	struct class_dev_iter iter;
	struct device *dev;
	int error = 0;

	if (!class)
		return -EINVAL;
	if (!class->p) {
		WARN(1, "%s called for class '%s' before it was initialized",
		     __func__, class->name);
		return -EINVAL;
	}

	class_dev_iter_init(&iter, class, start, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		error = fn(dev, data);
		if (error)
			break;
	}
	class_dev_iter_exit(&iter);

	return error;
}
EXPORT_SYMBOL_GPL(class_for_each_device);

/**
 * class_find_device - device iterator for locating a particular device
 * @class: the class we're iterating
 * @start: Device to begin with
 * @data: data for the match function
 * @match: function to check device
 *
 * This is similar to the class_for_each_dev() function above, but it
 * returns a reference to a device that is 'found' for later use, as
 * determined by the @match callback.
 *
 * The callback should return 0 if the device doesn't match and non-zero
 * if it does.  If the callback returns non-zero, this function will
 * return to the caller and not iterate over any more devices.
 *
 * Note, you will need to drop the reference with put_device() after use.
 *
 * @fn is allowed to do anything including calling back into class
 * code.  There's no locking restriction.
 */
struct device *class_find_device(struct class *class, struct device *start,
				 void *data,
				 int (*match)(struct device *, void *))
{
	struct class_dev_iter iter;
	struct device *dev;

	if (!class)
		return NULL;
	if (!class->p) {
		WARN(1, "%s called for class '%s' before it was initialized",
		     __func__, class->name);
		return NULL;
	}

	class_dev_iter_init(&iter, class, start, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		if (match(dev, data)) {
			get_device(dev);
			break;
		}
	}
	class_dev_iter_exit(&iter);

	return dev;
}
EXPORT_SYMBOL_GPL(class_find_device);

int class_interface_register(struct class_interface *class_intf) /* 注册接口类 */
{
	struct class *parent;
	struct class_dev_iter iter;
	struct device *dev;

	if (!class_intf || !class_intf->class)
		return -ENODEV;

	parent = class_get(class_intf->class);
	if (!parent)
		return -EINVAL;

	mutex_lock(&parent->p->class_mutex);
	list_add_tail(&class_intf->node, &parent->p->class_interfaces);
	if (class_intf->add_dev) {
		class_dev_iter_init(&iter, parent, NULL, NULL);
		while ((dev = class_dev_iter_next(&iter)))
			class_intf->add_dev(dev, class_intf);
		class_dev_iter_exit(&iter);
	}
	mutex_unlock(&parent->p->class_mutex);

	return 0;
}

void class_interface_unregister(struct class_interface *class_intf) /* 注册接口类 */
{
	struct class *parent = class_intf->class;
	struct class_dev_iter iter;
	struct device *dev;

	if (!parent)
		return;

	mutex_lock(&parent->p->class_mutex);
	list_del_init(&class_intf->node);
	if (class_intf->remove_dev) {
		class_dev_iter_init(&iter, parent, NULL, NULL);
		while ((dev = class_dev_iter_next(&iter)))
			class_intf->remove_dev(dev, class_intf);
		class_dev_iter_exit(&iter);
	}
	mutex_unlock(&parent->p->class_mutex);

	class_put(parent);
}

/* class的初始化(这是类的起源函数，在系统初始化期间调用，主要是产生类对象的顶层kset-----class_kset ) */
int __init classes_init(void)
{
	class_kset = kset_create_and_add("class", NULL, NULL);  /* 该函数的调用将导致在/sys目录下新生成一个"class"目录(/sys/class)，
	                                                                                              在以后的class相关的操作中，class_kset将作为所有class内核对象的
	                                                                                              顶层kset*/
	if (!class_kset)
		return -ENOMEM;
	return 0;
}

EXPORT_SYMBOL_GPL(class_create_file);
EXPORT_SYMBOL_GPL(class_remove_file);
EXPORT_SYMBOL_GPL(class_unregister);
EXPORT_SYMBOL_GPL(class_destroy);

EXPORT_SYMBOL_GPL(class_interface_register);
EXPORT_SYMBOL_GPL(class_interface_unregister);
