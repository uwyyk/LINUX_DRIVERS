/*
 * Sample kobject implementation
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 *
 * Released under the GPL version 2 only.
 *
 */
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h> 
#include <linux/fs.h> 

/*
 * This module shows how to create a simple subdirectory in sysfs called
 * /sys/kernel/kobject-example  In that directory, 3 files are created:
 * "foo", "baz", and "bar".  If an integer is written to these files, it can be
 * later read out of it.
 */

static int foo;
static int baz;
static int bar;

/*
 * The "foo" file where a static variable is read from and written to.
 */
static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", foo);
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	sscanf(buf, "%du", &foo);
	return count;
}

static struct kobj_attribute foo_attribute =   /* 设置kobj的属性文件 */
	__ATTR(foo, 0666, foo_show, foo_store);

/*
 * More complex function where we determine which varible is being accessed by
 * looking at the attribute for the "baz" and "bar" files.
 */
static ssize_t b_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "baz") == 0)
		var = baz;
	else
		var = bar;
	return sprintf(buf, "%d\n", var);
}

static ssize_t b_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int var;

	sscanf(buf, "%du", &var);
	if (strcmp(attr->attr.name, "baz") == 0)
		baz = var;
	else
		bar = var;
	return count;
}

static struct kobj_attribute baz_attribute =
	__ATTR(baz, 0666, b_show, b_store);
static struct kobj_attribute bar_attribute =
	__ATTR(bar, 0666, b_show, b_store);


/*
 * Create a group of attributes so that we can create and destory them all
 * at once.
 */
static struct attribute *attrs[] = {  /* 设置属性 */
	&foo_attribute.attr,
	&baz_attribute.attr,
	&bar_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *child;
static struct kobject * parent;

static int __init kobj_init(void)
{
	int retval;

	/*
	 * Create a simple kobject with the name of "kobject_example",
	 * located under /sys/kernel/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	parent = kobject_create_and_add("hello", NULL);  /* 此函数的调用将在/sys目录下创建hello目录 */
	if (!parent)
		return -ENOMEM;
	child = kobject_create_and_add("word", parent);/* 此函数的调用将在/sys/hello目录下创建word目录 */
	if (!parent)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(child, &attr_group);  /* 在/sys/hello/word目录下创建属性文件 */
	if (retval)
		{
		kobject_put(child);
              kobject_put(parent );
	return retval;
}

static void __exit kobj_exit(void)
{
	kobject_put(child);   /*  */
	kobject_put(parent );
}

module_init(kobj_init);
module_exit(kobj_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("scp");