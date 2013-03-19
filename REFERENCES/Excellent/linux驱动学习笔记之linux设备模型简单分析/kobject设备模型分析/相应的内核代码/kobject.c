/*
 * kobject.c - library routines for handling generic kernel objects
 *
 * Copyright (c) 2002-2003 Patrick Mochel <mochel@osdl.org>
 * Copyright (c) 2006-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (c) 2006-2007 Novell Inc.
 *
 * This file is released under the GPLv2.
 *
 *
 * Please see the file Documentation/kobject.txt for critical information
 * about using the kobject interface.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/slab.h>

/*
 * populate_dir - populate directory with attributes.
 * @kobj: object we're working on.
 *
 * Most subsystems have a set of default attributes that are associated
 * with an object that registers with them.  This is a helper called during
 * object registration that loops through the default attributes of the
 * subsystem and creates attributes files for them in sysfs.
 */
static int populate_dir(struct kobject *kobj)   /* 设置目录属性 */
{
	struct kobj_type *t = get_ktype(kobj);  /* 获取 kobject 成员ktype的指针*/
	struct attribute *attr;
	int error = 0;
	int i;

	if (t && t->default_attrs) {
		for (i = 0; (attr = t->default_attrs[i]) != NULL; i++) {  /* 遍历所有属性 */
			error = sysfs_create_file(kobj, attr);   /*创建属性文件， 如果该函数调用成功将使用attribute结
                                                                                      构中的名字创建文件并返回0，否则返回一个负的错误码 */
			if (error)
				break;
		}
	}
	return error;
}

static int create_dir(struct kobject *kobj)     /* 创建目录----sysfs入口 */
{
	int error = 0;
	if (kobject_name(kobj)) {
		error = sysfs_create_dir(kobj);  /* 在sysfs中创建目录 */
		if (!error) {
			error = populate_dir(kobj);  /* 设置目录属性 */
			if (error)
				sysfs_remove_dir(kobj);  /* 如果出现错误则移除创建的目录 */
		}
	}
	return error;
}

static int get_kobj_path_length(struct kobject *kobj)   /* 获取路径长度 */
{
	int length = 1;
	struct kobject *parent = kobj;

	/* walk up the ancestors until we hit the one pointing to the
	 * root.
	 * Add 1 to strlen for leading '/' of each level.
	 */
	do {
		if (kobject_name(parent) == NULL)
			return 0;
		length += strlen(kobject_name(parent)) + 1;
		parent = parent->parent;
	} while (parent);
	return length;
}

static void fill_kobj_path(struct kobject *kobj, char *path, int length)   /* 将路径字符串填写到path */
{
	struct kobject *parent;

	--length;
	for (parent = kobj; parent; parent = parent->parent) {
		int cur = strlen(kobject_name(parent));
		/* back up enough to print this name with '/' */
		length -= cur;
		strncpy(path + length, kobject_name(parent), cur);
		*(path + --length) = '/';
	}

	pr_debug("kobject: '%s' (%p): %s: path = '%s'\n", kobject_name(kobj),
		 kobj, __func__, path);
}

/**
 * kobject_get_path - generate and return the path associated with a given kobj and kset pair.
 *
 * @kobj:	kobject in question, with which to build the path
 * @gfp_mask:	the allocation type used to allocate the path
 *
 * The result must be freed by the caller with kfree().
 */
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask)  /* 获取引发事件的kobject在sysfs中路径 */
{
	char *path;
	int len;

	len = get_kobj_path_length(kobj);   /* 获取路径的长度 */
	if (len == 0)
		return NULL;
	path = kzalloc(len, gfp_mask);  /* 分配存放路径的字符串 */
	if (!path)
		return NULL;
	fill_kobj_path(kobj, path, len);  /* 将路径字符串存放到 path ，然后返回*/

	return path;
}
EXPORT_SYMBOL_GPL(kobject_get_path);

/* add the kobject to its kset's list */
static void kobj_kset_join(struct kobject *kobj)   /* 添加一个kobject到它的kset链表的末尾**/
{
	if (!kobj->kset)
		return;

	kset_get(kobj->kset);  /* 增加kset引用计数 */
	spin_lock(&kobj->kset->list_lock);   /* 获取kset的自旋锁 */
	list_add_tail(&kobj->entry, &kobj->kset->list);   /* 将新的kobject入口添加到kset链表里 */
	spin_unlock(&kobj->kset->list_lock);
}

/* remove the kobject from its kset's list */
static void kobj_kset_leave(struct kobject *kobj)  /* 将kobject从kset链表中移除 */
{
	if (!kobj->kset)
		return;

	spin_lock(&kobj->kset->list_lock);
	list_del_init(&kobj->entry);   /* 从链表中删除kobject */
	spin_unlock(&kobj->kset->list_lock);
	kset_put(kobj->kset);  /* 减小kset引用计数 */
}

static void kobject_init_internal(struct kobject *kobj) /* 初始化 kobject内部的成员*/
{
	if (!kobj)
		return;
	kref_init(&kobj->kref);  /* 引用计数初始化(增加引用计数)，可见当创建一个kobject的时候，就增加了一次引用 */
	INIT_LIST_HEAD(&kobj->entry);   /* 将kobject插入链表头 */
	
	/* kobject其他成员的初始化 */
	kobj->state_in_sysfs = 0;    /* 表该kobject所代表的内核对象没有在sysfs中建立一个入口点 */  
	kobj->state_add_uevent_sent = 0;  
	kobj->state_remove_uevent_sent = 0;
	kobj->state_initialized = 1;  /* 表示该内核对象已被初始化 */
}


static int kobject_add_internal(struct kobject *kobj) /* 添加  kobject内部成员，创建文件系统结构*/
{
	int error = 0;
	struct kobject *parent;

	if (!kobj)
		return -ENOENT;

	if (!kobj->name || !kobj->name[0]) {  /* name不存在则出错处理 */
		WARN(1, "kobject: (%p): attempted to be registered with empty "
			 "name!\n", kobj);
		return -EINVAL;
	}

	parent = kobject_get(kobj->parent);  /* 增加父节点引用计数 并且返回kobject的父节点*/

	/* join kset if set, use it as parent if we do not already have one */
	if (kobj->kset) {  /* 如果 kobject的成员 kset存在但是当前kobject->parent=NULL*/
		if (!parent)
			parent = kobject_get(&kobj->kset->kobj);   /* 则增加引用计数并且返回kobject的kset指向的父节点 */
		kobj_kset_join(kobj);  /* 将kobject添加到kset链表的末尾*/
		kobj->parent = parent;  /* kobject->parent= kobj->kset->kobj即kobject的parent指向kobject的成员kset的成员kobj(kset->kobj为kobject->parent的父节点)*/
	}

	pr_debug("kobject: '%s' (%p): %s: parent: '%s', set: '%s'\n",
		 kobject_name(kobj), kobj, __func__,
		 parent ? kobject_name(parent) : "<NULL>",
		 kobj->kset ? kobject_name(&kobj->kset->kobj) : "<NULL>");

	error = create_dir(kobj);   /* 如果kobject->parent=NULL且kobj->kset->kobj不存在，则该对象在sysfs文件树中就处于根目录的位置
	                                               (创建目录----sysfs入口) */
	if (error) {
		kobj_kset_leave(kobj);  /* 如果创建目录出现错误则将kobject从kset链表中移除 */
		kobject_put(parent);   /* 减小父节点的计数 */
		kobj->parent = NULL;  /* 将kobject的parent设为空 */

		/* be noisy on error issues */
		if (error == -EEXIST)
			printk(KERN_ERR "%s failed for %s with "
			       "-EEXIST, don't try to register things with "
			       "the same name in the same directory.\n",
			       __func__, kobject_name(kobj));
		else
			printk(KERN_ERR "%s failed for %s (%d)\n",
			       __func__, kobject_name(kobj), error);
		dump_stack();
	} else
		kobj->state_in_sysfs = 1;   /* 表示该kobject所代表的内核对象成功的在sysfs文件系统中建立一个入口点*/

	return error;
}

/**
 * kobject_set_name_vargs - Set the name of an kobject
 * @kobj: struct kobject to set the name of
 * @fmt: format string used to build the name
 * @vargs: vargs to format the string.
 */
int kobject_set_name_vargs(struct kobject *kobj, const char *fmt,   /* 用初始化后的名字设置kobject */
				  va_list vargs)
{
	const char *old_name = kobj->name;
	char *s;

	if (kobj->name && !fmt)   /* kobj->name 被设置并且fmt 为空则做出错处理*/
		return 0;

	kobj->name = kvasprintf(GFP_KERNEL, fmt, vargs);  /* 设置kobject的名字*/
	if (!kobj->name)
		return -ENOMEM;

	/* ewww... some of these buggers have '/' in the name ... */
	while ((s = strchr(kobj->name, '/')))
		s[0] = '!';

	kfree(old_name);
	return 0;
}

/**
 * kobject_set_name - Set the name of a kobject
 * @kobj: struct kobject to set the name of
 * @fmt: format string used to build the name
 *
 * This sets the name of the kobject.  If you have already added the
 * kobject to the system, you must call kobject_rename() in order to
 * change the name of the kobject.
 */
 /* 设置kobject的名字即kobject的name成员，这是在sysfs入口中使用的名字，它可能会操作失败，所以应该检查返回值*/                                                                                                           
int kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list vargs;  /* 定义一个va_list变量 ，以便对可能传递数量和类型不定的部分的字符串进行访问*/
	int retval;

	va_start(vargs, fmt);  /* 初始化变量vargs(初始化过程把vargs设置为指向可变参数部分的第一个参数)*/
	retval = kobject_set_name_vargs(kobj, fmt, vargs);   /* 用初始化后的名字设置kobject */
	va_end(vargs);/* 访问完毕最后一个可变参数后，掉用该宏来结束访问 */

	return retval;
}
EXPORT_SYMBOL(kobject_set_name);

/**
 * kobject_init - initialize a kobject structure
 * @kobj: pointer to the kobject to initialize
 * @ktype: pointer to the ktype for this kobject.
 *
 * This function will properly initialize a kobject such that it can then
 * be passed to the kobject_add() call.
 *
 * After this function is called, the kobject MUST be cleaned up by a call
 * to kobject_put(), not by a call to kfree directly to ensure that all of
 * the memory is cleaned up properly.
 */
 /*
 *kobject的初始化：
 *（1）首先将整个kobject设置为0，这通常使用memset函数，如果忘了对该结构清零初始化，则以后使用kobject时，
 *              可能会发生一些奇怪的错误
 *（2）然后调用kobject_init和kobject_set_name函数，以便设置该结构内部的一些成员(name、ktype、kset、parent)
 */
    
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)  /* 初始化kobject */
{
	char *err_str;

	if (!kobj) {   /* 如果传进来的参数kobj为空，则打印做出错处理 */
		err_str = "invalid kobject pointer!";
		goto error;
	}
	if (!ktype) {/* 如果传进来的参数ktype为空，则打印做出错处理 */
		err_str = "must have a ktype to be initialized properly!\n";
		goto error;
	}
	if (kobj->state_initialized) {  /* 如果struct kobject的成员state_initialized已经被初始化则做出相应的处理 */
		/* do not error out as sometimes we can recover */
		printk(KERN_ERR "kobject (%p): tried to init an initialized "
		       "object, something is seriously wrong.\n", kobj);
		dump_stack();
	}

	kobject_init_internal(kobj);  /* 初始化 kobject内部的成员*/
	kobj->ktype = ktype;  /* 初始化struct kobject的成员ktype(属性) */
	return;

error:
	printk(KERN_ERR "kobject (%p): %s\n", kobj, err_str);  /* %p表示输出这个指针 */
	dump_stack();
}
EXPORT_SYMBOL(kobject_init);

static int kobject_add_varg(struct kobject *kobj, struct kobject *parent,  /* 添加名字初始化后的kobject */
			    const char *fmt, va_list vargs)
{
	int retval;

	retval = kobject_set_name_vargs(kobj, fmt, vargs);   /* 初始化kobject 的格式化后的名字 */
	if (retval) {
		printk(KERN_ERR "kobject: can not set name properly!\n");
		return retval;
	}
	kobj->parent = parent;   /* parent指向kobject的父节点 */
	return kobject_add_internal(kobj);  /* 添加  kobject内部成员，返回创建文件系统结构*/
}

/**
 * kobject_add - the main kobject add function
 * @kobj: the kobject to add
 * @parent: pointer to the parent of the kobject.
 * @fmt: format to name the kobject with.
 *
 * The kobject name is set and added to the kobject hierarchy in this
 * function.
 *
 * If @parent is set, then the parent of the @kobj will be set to it.
 * If @parent is NULL, then the parent of the @kobj will be set to the
 * kobject associted with the kset assigned to this kobject.  If no kset
 * is assigned to the kobject, then the kobject will be located in the
 * root of the sysfs tree.
 *
 * If this function returns an error, kobject_put() must be called to
 * properly clean up the memory associated with the object.
 * Under no instance should the kobject that is passed to this function
 * be directly freed with a call to kfree(), that can leak memory.
 *
 * Note, no "add" uevent will be created with this call, the caller should set
 * up all of the necessary sysfs files for the object and then call
 * kobject_uevent() with the UEVENT_ADD parameter to ensure that
 * userspace is properly notified of this kobject's creation.
 */
 /*
 *底层sysfs操作操作：
 *    kobject是隐藏在sysfs虚拟文件系统后的机制，对于sysfs中的目录，内核都会存在一个对应的kobject，每个kobject都
 *     输出一个或多个属性，它们在kobject的sysfs目录中表现为文件，其中内容是由内核生成。
 *     只要调用kobject_add就能在sysfs中显示kobject。那么是如何在sysfs创建入口的呢？
 *（1）kobject在sysfs中的入口始终是一个目录，因此对kobject_add的调用将在sysfs中创建一个目录，通常这个目录包
 *             含一个或多个属性。
 *（2）分配给kobject的名字是sysfs的目录名，这样处于sysfs分层结构相同部分中的kobject必须有唯一的名字。
 *（3）sysfs入口在目录中的位置对应于kobject的parent指针。
 */
int kobject_add(struct kobject *kobj, struct kobject *parent,/* 添加kobject，该函数的参数parent指向kobject的父节点，fmt指向kobject的
                                                                                                      成员name。调用该函数会增加引用计数 */
		const char *fmt, ...)
{
	va_list args;
	int retval;

	if (!kobj)
		return -EINVAL;

	if (!kobj->state_initialized) {
		printk(KERN_ERR "kobject '%s' (%p): tried to add an "
		       "uninitialized object, something is seriously wrong.\n",
		       kobject_name(kobj), kobj);
		dump_stack();
		return -EINVAL;
	}
	va_start(args, fmt);
	retval = kobject_add_varg(kobj, parent, fmt, args);  /* 添加格式化后的kobject */
	va_end(args);

	return retval;
}
EXPORT_SYMBOL(kobject_add);

/**
 * kobject_init_and_add - initialize a kobject structure and add it to the kobject hierarchy
 * @kobj: pointer to the kobject to initialize
 * @ktype: pointer to the ktype for this kobject.
 * @parent: pointer to the parent of this kobject.
 * @fmt: the name of the kobject.
 *
 * This function combines the call to kobject_init() and
 * kobject_add().  The same type of error handling after a call to
 * kobject_add() and kobject lifetime rules are the same here.
 */
 /* 如果我们已经静态的创建了kobject，则可调用kobject_init_and_add来注册kobject */
int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
			 struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int retval;

	kobject_init(kobj, ktype);   /* 初始化kobject */

	va_start(args, fmt);
	retval = kobject_add_varg(kobj, parent, fmt, args);   /* 添加名字初始化后的kobject */
	va_end(args);

	return retval;
}
EXPORT_SYMBOL_GPL(kobject_init_and_add);

/**
 * kobject_rename - change the name of an object
 * @kobj: object in question.
 * @new_name: object's new name
 *
 * It is the responsibility of the caller to provide mutual
 * exclusion between two different calls of kobject_rename
 * on the same kobject and to ensure that new_name is valid and
 * won't conflict with other kobjects.
 */
int kobject_rename(struct kobject *kobj, const char *new_name)  /* 重新给kobject命名 */
{
	int error = 0;
	const char *devpath = NULL;
	const char *dup_name = NULL, *name;
	char *devpath_string = NULL;
	char *envp[2];

	kobj = kobject_get(kobj);
	if (!kobj)
		return -EINVAL;
	if (!kobj->parent)
		return -EINVAL;

	devpath = kobject_get_path(kobj, GFP_KERNEL);
	if (!devpath) {
		error = -ENOMEM;
		goto out;
	}
	devpath_string = kmalloc(strlen(devpath) + 15, GFP_KERNEL);
	if (!devpath_string) {
		error = -ENOMEM;
		goto out;
	}
	sprintf(devpath_string, "DEVPATH_OLD=%s", devpath);
	envp[0] = devpath_string;
	envp[1] = NULL;

	name = dup_name = kstrdup(new_name, GFP_KERNEL);
	if (!name) {
		error = -ENOMEM;
		goto out;
	}

	error = sysfs_rename_dir(kobj, new_name);
	if (error)
		goto out;

	/* Install the new kobject name */
	dup_name = kobj->name;
	kobj->name = name;

	/* This function is mostly/only used for network interface.
	 * Some hotplug package track interfaces by their name and
	 * therefore want to know when the name is changed by the user. */
	kobject_uevent_env(kobj, KOBJ_MOVE, envp);

out:
	kfree(dup_name);
	kfree(devpath_string);
	kfree(devpath);
	kobject_put(kobj);

	return error;
}
EXPORT_SYMBOL_GPL(kobject_rename);

/**
 * kobject_move - move object to another parent
 * @kobj: object in question.
 * @new_parent: object's new parent (can be NULL)
 */
int kobject_move(struct kobject *kobj, struct kobject *new_parent)   /* 从另一个 */
{
	int error;
	struct kobject *old_parent;
	const char *devpath = NULL;
	char *devpath_string = NULL;
	char *envp[2];

	kobj = kobject_get(kobj);
	if (!kobj)
		return -EINVAL;
	new_parent = kobject_get(new_parent);
	if (!new_parent) {
		if (kobj->kset)
			new_parent = kobject_get(&kobj->kset->kobj);
	}
	/* old object path */
	devpath = kobject_get_path(kobj, GFP_KERNEL);
	if (!devpath) {
		error = -ENOMEM;
		goto out;
	}
	devpath_string = kmalloc(strlen(devpath) + 15, GFP_KERNEL);
	if (!devpath_string) {
		error = -ENOMEM;
		goto out;
	}
	sprintf(devpath_string, "DEVPATH_OLD=%s", devpath);
	envp[0] = devpath_string;
	envp[1] = NULL;
	error = sysfs_move_dir(kobj, new_parent);
	if (error)
		goto out;
	old_parent = kobj->parent;
	kobj->parent = new_parent;
	new_parent = NULL;
	kobject_put(old_parent);
	kobject_uevent_env(kobj, KOBJ_MOVE, envp);
out:
	kobject_put(new_parent);
	kobject_put(kobj);
	kfree(devpath_string);
	kfree(devpath);
	return error;
}

/**
 * kobject_del - unlink kobject from hierarchy.
 * @kobj: object.
 */
 /* 该函数将在sysfs文件树中把kobj对应的目录删除，另外如果kobj隶属于某一kset的话，将其从kset的链表中删除*/
void kobject_del(struct kobject *kobj)
{
	if (!kobj)
		return;

	sysfs_remove_dir(kobj);   /* 删除sysfs中的kobject目录 */
	kobj->state_in_sysfs = 0;    /* 清除成功在sysfs文件系统中建立一个入口点的标志位 */
	kobj_kset_leave(kobj);    /* 将kobject从kset链表中删除 */
	kobject_put(kobj->parent);  /* 减少kobj->parent的引用计数 */
	kobj->parent = NULL;        /* 将 kobj->parent 设置为空*/
}

/**
 * kobject_get - increment refcount for object.
 * @kobj: object.
 */
struct kobject *kobject_get(struct kobject *kobj) /* 对该函数的调用将增加kobject的引用计数，并返回指向kobject父节点的指针，
                                                                                    如果kobject已经处于被销毁的过程中，则调用失败并返回NULL */
{
	if (kobj)
		kref_get(&kobj->kref);
	return kobj;
}

/*
 * kobject_cleanup - free kobject resources.
 * @kobj: object to cleanup
 */
static void kobject_cleanup(struct kobject *kobj)/* 释放kobject资源 */
{
	struct kobj_type *t = get_ktype(kobj);
	const char *name = kobj->name;

	pr_debug("kobject: '%s' (%p): %s\n",
		 kobject_name(kobj), kobj, __func__);

	if (t && !t->release)
		pr_debug("kobject: '%s' (%p): does not have a release() "
			 "function, it is broken and must be fixed.\n",
			 kobject_name(kobj), kobj);

	/* send "remove" if the caller did not do it but sent "add" */
	if (kobj->state_add_uevent_sent && !kobj->state_remove_uevent_sent) {
		pr_debug("kobject: '%s' (%p): auto cleanup 'remove' event\n",
			 kobject_name(kobj), kobj);
		kobject_uevent(kobj, KOBJ_REMOVE);
	}

	/* remove from sysfs if the caller did not do it */
	if (kobj->state_in_sysfs) {
		pr_debug("kobject: '%s' (%p): auto cleanup kobject_del\n",
			 kobject_name(kobj), kobj);
		kobject_del(kobj);   /* 把kobject从kset中删除 */
	}

	if (t && t->release) {
		pr_debug("kobject: '%s' (%p): calling ktype release\n",
			 kobject_name(kobj), kobj);
		t->release(kobj);   /* 调用struct kobj_type 的release函数 */
	}

	/* free name if we allocated it */
	if (name) {
		pr_debug("kobject: '%s': free name\n", name);
		kfree(name);   /* 释放名字占用的空间 */
	}
}

static void kobject_release(struct kref *kref) /* 释放函数*/
{
	kobject_cleanup(container_of(kref, struct kobject, kref));  /* 释放kobject资源 */
}

/**
 * kobject_put - decrement refcount for object.
 * @kobj: object.
 *
 * Decrement the refcount, and if 0, call kobject_cleanup().
 */
void kobject_put(struct kobject *kobj) /* 当引用被释放时，调用该函数减少引用次数，并在可能的情况下释放对象，
                                                                    请记住kobject_init设置引用计数为1，所以当创建kobject时，如果不再需要初始的引
                                                                    用 就调用相应的kobject_put函数*/
{
	if (kobj) {
		if (!kobj->state_initialized)
			WARN(1, KERN_WARNING "kobject: '%s' (%p): is not "
			       "initialized, yet kobject_put() is being "
			       "called.\n", kobject_name(kobj), kobj);
		kref_put(&kobj->kref, kobject_release);     /* 减少计数 */ 
	}
}

static void dynamic_kobj_release(struct kobject *kobj)  /*释放函数*/
{
	pr_debug("kobject: (%p): %s\n", kobj, __func__);
	kfree(kobj);  /* 释放kobject所占用的内存 */
}

static struct kobj_type dynamic_kobj_ktype = {   /* 初始化一个struct kobj_type  */
	.release	= dynamic_kobj_release,
	.sysfs_ops	= &kobj_sysfs_ops,     /* 实现这些属性(文件)的操作函数 */
};

/**
 * kobject_create - create a struct kobject dynamically
 *
 * This function creates a kobject structure dynamically and sets it up
 * to be a "dynamic" kobject with a default release function set up.
 *
 * If the kobject was not able to be created, NULL will be returned.
 * The kobject structure returned from here must be cleaned up with a
 * call to kobject_put() and not kfree(), as kobject_init() has
 * already been called on this structure.
 */
 /* 创建kobject 即分配并初始化一个kobject对象，如果调用该函数产生一个kobject对象，那么调用者将无法为该kobject
      对象另行指定kobj_type，因为该函数为产生的kobject对象指定了一个默认的kobj_type对象dynamic_kobj_ktype，这个行为
      将影响kobject对象上sysfs文件操作，如果调用者需要明确指定一个自己的kobj_type对象该kobject对象，那么还是应
      该使用其他函数，比如调用kobject_init_and_add函数*/
struct kobject *kobject_create(void)    
{
	struct kobject *kobj;

	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);   /* 为kobject分配内存 */
	if (!kobj)
		return NULL;

	kobject_init(kobj, &dynamic_kobj_ktype);   /* 初始化kobject */
	return kobj;
}

/**
 * kobject_create_and_add - create a struct kobject dynamically and register it with sysfs
 *
 * @name: the name for the kset
 * @parent: the parent kobject of this kobject, if any.
 *
 * This function creates a kobject structure dynamically and registers it
 * with sysfs.  When you are finished with this structure, call
 * kobject_put() and the structure will be dynamically freed when
 * it is no longer being used.
 *
 * If the kobject was not able to be created, NULL will be returned.
 */
 /*
   *该函数创建kobject，挂到父kobject，并设置kobj_type;在文件系统中为其创建目录和属性文件等;
   *另外，如果我们已经静态定义了一艘创建的kobject，则可调用kobject_init_and_add来注册kobject
   */
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)   /*  创建并添加一个kobject  */
{
	struct kobject *kobj;
	int retval;

	kobj = kobject_create();   /* 创建kobject */
	if (!kobj)
		return NULL;

	retval = kobject_add(kobj, parent, "%s", name);   /* 添加一个kobject(实际上是添加到其父节点所代表的kset中去) */
	if (retval) {
		printk(KERN_WARNING "%s: kobject_add error: %d\n",
		       __func__, retval);
		kobject_put(kobj);   /* 如果添加出错则减小引用计数 */
		kobj = NULL;
	}
	return kobj;
}
EXPORT_SYMBOL_GPL(kobject_create_and_add);

/**
 * kset_init - initialize a kset for use
 * @k: kset
 */
void kset_init(struct kset *k)/* 初始化kset 对象*/
{
	kobject_init_internal(&k->kobj);
	INIT_LIST_HEAD(&k->list);   /* 将kset添加到kset链表中 */
	spin_lock_init(&k->list_lock);
}

/* default kobject attribute operations */
static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->show)
		ret = kattr->show(kobj, kattr, buf);
	return ret;
}

static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->store)
		ret = kattr->store(kobj, kattr, buf, count);
	return ret;
}

struct sysfs_ops kobj_sysfs_ops = {    /* 实现struct kobj_type的属性(文件)的操作函数 */
	.show	= kobj_attr_show,     /* 当用户空间读取一个属性时，内核会使用指向kobject 的指针和正确
	                                                     的属性结构来调用show函数，该函数将把指定的值编码后放入缓冲
	                                                     区，然后把实际长度作为返回值返回。attr指向属性，可用于判断
	                                                     所请求的是哪个属性。 */
	.store	= kobj_attr_store,    /* 该函数将保存在缓冲区的数据解码，并调用各种实用方法
	                                                    保存新值，返回实际解码的字节数 ,只有当拥有属性的写权
	                                                    限时，才能调用该函数*/
};

/**
 * kset_register - initialize and add a kset.
 * @k: kset.
 */
 /* 初始化和注册一个kset对象(kset对象与单个kobject对象不一样的地方在于，将一个kset对象向系统注册时，
      如果linux内核编译时启用了CONFIG_HOTPLUG，那么需要将这一事件通知用户空间，这个过程由kobject_uevent
      函数完成，如果一个kobject对象不属于任何一个kset，那么这个孤立的kobject对象将无法通过uevent机制向
      用户空间发送event消息；) */
int kset_register(struct kset *k)  
{
	int err;

	if (!k)
		return -EINVAL;

	kset_init(k);   /* 初始化kset */   
	err = kobject_add_internal(&k->kobj);   /* 将kset内嵌的kobject注册到文件系统(函数会为代表kset对象的k->kobj在sysfs文件树
	                                                                     中生成一个新目录) */
	if (err)
		return err;
	kobject_uevent(&k->kobj, KOBJ_ADD);   /* 注册kset会产生一个热插拔事件 */
	return 0;
}

/**
 * kset_unregister - remove a kset.
 * @k: kset.
 */
void kset_unregister(struct kset *k)  /* 注销 */
{
	if (!k)
		return;
	kobject_put(&k->kobj);   /* 减小引用计数，为0则释放其内存空间 */
}

/**
 * kset_find_obj - search for object in kset.
 * @kset: kset we're looking in.
 * @name: object's name.
 *
 * Lock kset via @kset->subsys, and iterate over @kset->list,
 * looking for a matching kobject. If matching object is found
 * take a reference and return the object.
 */
struct kobject *kset_find_obj(struct kset *kset, const char *name)   /* 在kset链表中寻找kobject */
{
	struct kobject *k;
	struct kobject *ret = NULL;

	spin_lock(&kset->list_lock);
	list_for_each_entry(k, &kset->list, entry) {   /* 遍历kset链表 */
		if (kobject_name(k) && !strcmp(kobject_name(k), name)) {
			ret = kobject_get(k);
			break;
		}
	}
	spin_unlock(&kset->list_lock);
	return ret;
}

static void kset_release(struct kobject *kobj)    /* kset的释放函数 */
{
	struct kset *kset = container_of(kobj, struct kset, kobj);
	pr_debug("kobject: '%s' (%p): %s\n",
		 kobject_name(kobj), kobj, __func__);
	kfree(kset);
}

static struct kobj_type kset_ktype = {   /* 设置struct kobj_type结构成员 */
	.sysfs_ops	= &kobj_sysfs_ops,
	.release = kset_release,    /* kset的释放函数 */
};

/**
 * kset_create - create a struct kset dynamically
 *
 * @name: the name for the kset
 * @uevent_ops: a struct kset_uevent_ops for the kset
 * @parent_kobj: the parent kobject of this kset, if any.
 *
 * This function creates a kset structure dynamically.  This structure can
 * then be registered with the system and show up in sysfs with a call to
 * kset_register().  When you are finished with this structure, if
 * kset_register() has been called, call kset_unregister() and the
 * structure will be dynamically freed when it is no longer being used.
 *
 * If the kset was not able to be created, NULL will be returned.
 */
static struct kset *kset_create(const char *name,   /* 动态的创建一个struct kset 结构 */
				struct kset_uevent_ops *uevent_ops,
				struct kobject *parent_kobj)
{
	struct kset *kset;   /* 定义一个指向struct kset的指针 */

	kset = kzalloc(sizeof(*kset), GFP_KERNEL);  /* 为struct kset 分配内存 */
	if (!kset)
		return NULL;
	kobject_set_name(&kset->kobj, name);   /* 设置struct kset 名字*/
	kset->uevent_ops = uevent_ops;   /* 设置热插拔事件操作函数 */
	kset->kobj.parent = parent_kobj;  /* 使 kset->kobj.parent 指针指向传进来的struct kobject*/

	/*
	 * The kobject of this kset will have a type of kset_ktype and belong to
	 * no kset itself.  That way we can properly free it when it is
	 * finished being used.
	 */
	kset->kobj.ktype = &kset_ktype;   /* 设置 kset->kobj.ktype */
	kset->kobj.kset = NULL;

	return kset;
}

/**
 * kset_create_and_add - create a struct kset dynamically and add it to sysfs
 *
 * @name: the name for the kset
 * @uevent_ops: a struct kset_uevent_ops for the kset
 * @parent_kobj: the parent kobject of this kset, if any.
 *
 * This function creates a kset structure dynamically and registers it
 * with sysfs.  When you are finished with this structure, call
 * kset_unregister() and the structure will be dynamically freed when it
 * is no longer being used.
 *
 * If the kset was not able to be created, NULL will be returned.
 */
struct kset *kset_create_and_add(const char *name,   /* 动态的创建一个 kset 对象并把它添加到sysfs文件系统中*/
				 struct kset_uevent_ops *uevent_ops,
				 struct kobject *parent_kobj)
{
	struct kset *kset;
	int error;

	kset = kset_create(name, uevent_ops, parent_kobj);   /* 创建一个kset */
	if (!kset)
		return NULL;
	error = kset_register(kset);  /* 注册这个kset */
	if (error) {
		kfree(kset);
		return NULL;
	}
	return kset;
}
EXPORT_SYMBOL_GPL(kset_create_and_add);

EXPORT_SYMBOL(kobject_get);
EXPORT_SYMBOL(kobject_put);
EXPORT_SYMBOL(kobject_del);

EXPORT_SYMBOL(kset_register);
EXPORT_SYMBOL(kset_unregister);
