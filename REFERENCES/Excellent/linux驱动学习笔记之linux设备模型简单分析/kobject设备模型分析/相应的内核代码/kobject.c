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
static int populate_dir(struct kobject *kobj)   /* ����Ŀ¼���� */
{
	struct kobj_type *t = get_ktype(kobj);  /* ��ȡ kobject ��Աktype��ָ��*/
	struct attribute *attr;
	int error = 0;
	int i;

	if (t && t->default_attrs) {
		for (i = 0; (attr = t->default_attrs[i]) != NULL; i++) {  /* ������������ */
			error = sysfs_create_file(kobj, attr);   /*���������ļ��� ����ú������óɹ���ʹ��attribute��
                                                                                      ���е����ִ����ļ�������0�����򷵻�һ�����Ĵ����� */
			if (error)
				break;
		}
	}
	return error;
}

static int create_dir(struct kobject *kobj)     /* ����Ŀ¼----sysfs��� */
{
	int error = 0;
	if (kobject_name(kobj)) {
		error = sysfs_create_dir(kobj);  /* ��sysfs�д���Ŀ¼ */
		if (!error) {
			error = populate_dir(kobj);  /* ����Ŀ¼���� */
			if (error)
				sysfs_remove_dir(kobj);  /* ������ִ������Ƴ�������Ŀ¼ */
		}
	}
	return error;
}

static int get_kobj_path_length(struct kobject *kobj)   /* ��ȡ·������ */
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

static void fill_kobj_path(struct kobject *kobj, char *path, int length)   /* ��·���ַ�����д��path */
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
char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask)  /* ��ȡ�����¼���kobject��sysfs��·�� */
{
	char *path;
	int len;

	len = get_kobj_path_length(kobj);   /* ��ȡ·���ĳ��� */
	if (len == 0)
		return NULL;
	path = kzalloc(len, gfp_mask);  /* ������·�����ַ��� */
	if (!path)
		return NULL;
	fill_kobj_path(kobj, path, len);  /* ��·���ַ�����ŵ� path ��Ȼ�󷵻�*/

	return path;
}
EXPORT_SYMBOL_GPL(kobject_get_path);

/* add the kobject to its kset's list */
static void kobj_kset_join(struct kobject *kobj)   /* ���һ��kobject������kset�����ĩβ**/
{
	if (!kobj->kset)
		return;

	kset_get(kobj->kset);  /* ����kset���ü��� */
	spin_lock(&kobj->kset->list_lock);   /* ��ȡkset�������� */
	list_add_tail(&kobj->entry, &kobj->kset->list);   /* ���µ�kobject�����ӵ�kset������ */
	spin_unlock(&kobj->kset->list_lock);
}

/* remove the kobject from its kset's list */
static void kobj_kset_leave(struct kobject *kobj)  /* ��kobject��kset�������Ƴ� */
{
	if (!kobj->kset)
		return;

	spin_lock(&kobj->kset->list_lock);
	list_del_init(&kobj->entry);   /* ��������ɾ��kobject */
	spin_unlock(&kobj->kset->list_lock);
	kset_put(kobj->kset);  /* ��Сkset���ü��� */
}

static void kobject_init_internal(struct kobject *kobj) /* ��ʼ�� kobject�ڲ��ĳ�Ա*/
{
	if (!kobj)
		return;
	kref_init(&kobj->kref);  /* ���ü�����ʼ��(�������ü���)���ɼ�������һ��kobject��ʱ�򣬾�������һ������ */
	INIT_LIST_HEAD(&kobj->entry);   /* ��kobject��������ͷ */
	
	/* kobject������Ա�ĳ�ʼ�� */
	kobj->state_in_sysfs = 0;    /* ���kobject��������ں˶���û����sysfs�н���һ����ڵ� */  
	kobj->state_add_uevent_sent = 0;  
	kobj->state_remove_uevent_sent = 0;
	kobj->state_initialized = 1;  /* ��ʾ���ں˶����ѱ���ʼ�� */
}


static int kobject_add_internal(struct kobject *kobj) /* ���  kobject�ڲ���Ա�������ļ�ϵͳ�ṹ*/
{
	int error = 0;
	struct kobject *parent;

	if (!kobj)
		return -ENOENT;

	if (!kobj->name || !kobj->name[0]) {  /* name������������� */
		WARN(1, "kobject: (%p): attempted to be registered with empty "
			 "name!\n", kobj);
		return -EINVAL;
	}

	parent = kobject_get(kobj->parent);  /* ���Ӹ��ڵ����ü��� ���ҷ���kobject�ĸ��ڵ�*/

	/* join kset if set, use it as parent if we do not already have one */
	if (kobj->kset) {  /* ��� kobject�ĳ�Ա kset���ڵ��ǵ�ǰkobject->parent=NULL*/
		if (!parent)
			parent = kobject_get(&kobj->kset->kobj);   /* ���������ü������ҷ���kobject��ksetָ��ĸ��ڵ� */
		kobj_kset_join(kobj);  /* ��kobject��ӵ�kset�����ĩβ*/
		kobj->parent = parent;  /* kobject->parent= kobj->kset->kobj��kobject��parentָ��kobject�ĳ�Աkset�ĳ�Աkobj(kset->kobjΪkobject->parent�ĸ��ڵ�)*/
	}

	pr_debug("kobject: '%s' (%p): %s: parent: '%s', set: '%s'\n",
		 kobject_name(kobj), kobj, __func__,
		 parent ? kobject_name(parent) : "<NULL>",
		 kobj->kset ? kobject_name(&kobj->kset->kobj) : "<NULL>");

	error = create_dir(kobj);   /* ���kobject->parent=NULL��kobj->kset->kobj�����ڣ���ö�����sysfs�ļ����оʹ��ڸ�Ŀ¼��λ��
	                                               (����Ŀ¼----sysfs���) */
	if (error) {
		kobj_kset_leave(kobj);  /* �������Ŀ¼���ִ�����kobject��kset�������Ƴ� */
		kobject_put(parent);   /* ��С���ڵ�ļ��� */
		kobj->parent = NULL;  /* ��kobject��parent��Ϊ�� */

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
		kobj->state_in_sysfs = 1;   /* ��ʾ��kobject��������ں˶���ɹ�����sysfs�ļ�ϵͳ�н���һ����ڵ�*/

	return error;
}

/**
 * kobject_set_name_vargs - Set the name of an kobject
 * @kobj: struct kobject to set the name of
 * @fmt: format string used to build the name
 * @vargs: vargs to format the string.
 */
int kobject_set_name_vargs(struct kobject *kobj, const char *fmt,   /* �ó�ʼ�������������kobject */
				  va_list vargs)
{
	const char *old_name = kobj->name;
	char *s;

	if (kobj->name && !fmt)   /* kobj->name �����ò���fmt Ϊ������������*/
		return 0;

	kobj->name = kvasprintf(GFP_KERNEL, fmt, vargs);  /* ����kobject������*/
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
 /* ����kobject�����ּ�kobject��name��Ա��������sysfs�����ʹ�õ����֣������ܻ����ʧ�ܣ�����Ӧ�ü�鷵��ֵ*/                                                                                                           
int kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list vargs;  /* ����һ��va_list���� ���Ա�Կ��ܴ������������Ͳ����Ĳ��ֵ��ַ������з���*/
	int retval;

	va_start(vargs, fmt);  /* ��ʼ������vargs(��ʼ�����̰�vargs����Ϊָ��ɱ�������ֵĵ�һ������)*/
	retval = kobject_set_name_vargs(kobj, fmt, vargs);   /* �ó�ʼ�������������kobject */
	va_end(vargs);/* ����������һ���ɱ�����󣬵��øú����������� */

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
 *kobject�ĳ�ʼ����
 *��1�����Ƚ�����kobject����Ϊ0����ͨ��ʹ��memset������������˶Ըýṹ�����ʼ�������Ժ�ʹ��kobjectʱ��
 *              ���ܻᷢ��һЩ��ֵĴ���
 *��2��Ȼ�����kobject_init��kobject_set_name�������Ա����øýṹ�ڲ���һЩ��Ա(name��ktype��kset��parent)
 */
    
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)  /* ��ʼ��kobject */
{
	char *err_str;

	if (!kobj) {   /* ����������Ĳ���kobjΪ�գ����ӡ�������� */
		err_str = "invalid kobject pointer!";
		goto error;
	}
	if (!ktype) {/* ����������Ĳ���ktypeΪ�գ����ӡ�������� */
		err_str = "must have a ktype to be initialized properly!\n";
		goto error;
	}
	if (kobj->state_initialized) {  /* ���struct kobject�ĳ�Աstate_initialized�Ѿ�����ʼ����������Ӧ�Ĵ��� */
		/* do not error out as sometimes we can recover */
		printk(KERN_ERR "kobject (%p): tried to init an initialized "
		       "object, something is seriously wrong.\n", kobj);
		dump_stack();
	}

	kobject_init_internal(kobj);  /* ��ʼ�� kobject�ڲ��ĳ�Ա*/
	kobj->ktype = ktype;  /* ��ʼ��struct kobject�ĳ�Աktype(����) */
	return;

error:
	printk(KERN_ERR "kobject (%p): %s\n", kobj, err_str);  /* %p��ʾ������ָ�� */
	dump_stack();
}
EXPORT_SYMBOL(kobject_init);

static int kobject_add_varg(struct kobject *kobj, struct kobject *parent,  /* ������ֳ�ʼ�����kobject */
			    const char *fmt, va_list vargs)
{
	int retval;

	retval = kobject_set_name_vargs(kobj, fmt, vargs);   /* ��ʼ��kobject �ĸ�ʽ��������� */
	if (retval) {
		printk(KERN_ERR "kobject: can not set name properly!\n");
		return retval;
	}
	kobj->parent = parent;   /* parentָ��kobject�ĸ��ڵ� */
	return kobject_add_internal(kobj);  /* ���  kobject�ڲ���Ա�����ش����ļ�ϵͳ�ṹ*/
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
 *�ײ�sysfs����������
 *    kobject��������sysfs�����ļ�ϵͳ��Ļ��ƣ�����sysfs�е�Ŀ¼���ں˶������һ����Ӧ��kobject��ÿ��kobject��
 *     ���һ���������ԣ�������kobject��sysfsĿ¼�б���Ϊ�ļ����������������ں����ɡ�
 *     ֻҪ����kobject_add������sysfs����ʾkobject����ô�������sysfs������ڵ��أ�
 *��1��kobject��sysfs�е����ʼ����һ��Ŀ¼����˶�kobject_add�ĵ��ý���sysfs�д���һ��Ŀ¼��ͨ�����Ŀ¼��
 *             ��һ���������ԡ�
 *��2�������kobject��������sysfs��Ŀ¼������������sysfs�ֲ�ṹ��ͬ�����е�kobject������Ψһ�����֡�
 *��3��sysfs�����Ŀ¼�е�λ�ö�Ӧ��kobject��parentָ�롣
 */
int kobject_add(struct kobject *kobj, struct kobject *parent,/* ���kobject���ú����Ĳ���parentָ��kobject�ĸ��ڵ㣬fmtָ��kobject��
                                                                                                      ��Աname�����øú������������ü��� */
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
	retval = kobject_add_varg(kobj, parent, fmt, args);  /* ��Ӹ�ʽ�����kobject */
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
 /* ��������Ѿ���̬�Ĵ�����kobject����ɵ���kobject_init_and_add��ע��kobject */
int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
			 struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int retval;

	kobject_init(kobj, ktype);   /* ��ʼ��kobject */

	va_start(args, fmt);
	retval = kobject_add_varg(kobj, parent, fmt, args);   /* ������ֳ�ʼ�����kobject */
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
int kobject_rename(struct kobject *kobj, const char *new_name)  /* ���¸�kobject���� */
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
int kobject_move(struct kobject *kobj, struct kobject *new_parent)   /* ����һ�� */
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
 /* �ú�������sysfs�ļ����а�kobj��Ӧ��Ŀ¼ɾ�����������kobj������ĳһkset�Ļ��������kset��������ɾ��*/
void kobject_del(struct kobject *kobj)
{
	if (!kobj)
		return;

	sysfs_remove_dir(kobj);   /* ɾ��sysfs�е�kobjectĿ¼ */
	kobj->state_in_sysfs = 0;    /* ����ɹ���sysfs�ļ�ϵͳ�н���һ����ڵ�ı�־λ */
	kobj_kset_leave(kobj);    /* ��kobject��kset������ɾ�� */
	kobject_put(kobj->parent);  /* ����kobj->parent�����ü��� */
	kobj->parent = NULL;        /* �� kobj->parent ����Ϊ��*/
}

/**
 * kobject_get - increment refcount for object.
 * @kobj: object.
 */
struct kobject *kobject_get(struct kobject *kobj) /* �Ըú����ĵ��ý�����kobject�����ü�����������ָ��kobject���ڵ��ָ�룬
                                                                                    ���kobject�Ѿ����ڱ����ٵĹ����У������ʧ�ܲ�����NULL */
{
	if (kobj)
		kref_get(&kobj->kref);
	return kobj;
}

/*
 * kobject_cleanup - free kobject resources.
 * @kobj: object to cleanup
 */
static void kobject_cleanup(struct kobject *kobj)/* �ͷ�kobject��Դ */
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
		kobject_del(kobj);   /* ��kobject��kset��ɾ�� */
	}

	if (t && t->release) {
		pr_debug("kobject: '%s' (%p): calling ktype release\n",
			 kobject_name(kobj), kobj);
		t->release(kobj);   /* ����struct kobj_type ��release���� */
	}

	/* free name if we allocated it */
	if (name) {
		pr_debug("kobject: '%s': free name\n", name);
		kfree(name);   /* �ͷ�����ռ�õĿռ� */
	}
}

static void kobject_release(struct kref *kref) /* �ͷź���*/
{
	kobject_cleanup(container_of(kref, struct kobject, kref));  /* �ͷ�kobject��Դ */
}

/**
 * kobject_put - decrement refcount for object.
 * @kobj: object.
 *
 * Decrement the refcount, and if 0, call kobject_cleanup().
 */
void kobject_put(struct kobject *kobj) /* �����ñ��ͷ�ʱ�����øú����������ô��������ڿ��ܵ�������ͷŶ���
                                                                    ���סkobject_init�������ü���Ϊ1�����Ե�����kobjectʱ�����������Ҫ��ʼ����
                                                                    �� �͵�����Ӧ��kobject_put����*/
{
	if (kobj) {
		if (!kobj->state_initialized)
			WARN(1, KERN_WARNING "kobject: '%s' (%p): is not "
			       "initialized, yet kobject_put() is being "
			       "called.\n", kobject_name(kobj), kobj);
		kref_put(&kobj->kref, kobject_release);     /* ���ټ��� */ 
	}
}

static void dynamic_kobj_release(struct kobject *kobj)  /*�ͷź���*/
{
	pr_debug("kobject: (%p): %s\n", kobj, __func__);
	kfree(kobj);  /* �ͷ�kobject��ռ�õ��ڴ� */
}

static struct kobj_type dynamic_kobj_ktype = {   /* ��ʼ��һ��struct kobj_type  */
	.release	= dynamic_kobj_release,
	.sysfs_ops	= &kobj_sysfs_ops,     /* ʵ����Щ����(�ļ�)�Ĳ������� */
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
 /* ����kobject �����䲢��ʼ��һ��kobject����������øú�������һ��kobject������ô�����߽��޷�Ϊ��kobject
      ��������ָ��kobj_type����Ϊ�ú���Ϊ������kobject����ָ����һ��Ĭ�ϵ�kobj_type����dynamic_kobj_ktype�������Ϊ
      ��Ӱ��kobject������sysfs�ļ������������������Ҫ��ȷָ��һ���Լ���kobj_type�����kobject������ô����Ӧ
      ��ʹ�������������������kobject_init_and_add����*/
struct kobject *kobject_create(void)    
{
	struct kobject *kobj;

	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);   /* Ϊkobject�����ڴ� */
	if (!kobj)
		return NULL;

	kobject_init(kobj, &dynamic_kobj_ktype);   /* ��ʼ��kobject */
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
   *�ú�������kobject���ҵ���kobject��������kobj_type;���ļ�ϵͳ��Ϊ�䴴��Ŀ¼�������ļ���;
   *���⣬��������Ѿ���̬������һ�Ҵ�����kobject����ɵ���kobject_init_and_add��ע��kobject
   */
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)   /*  ���������һ��kobject  */
{
	struct kobject *kobj;
	int retval;

	kobj = kobject_create();   /* ����kobject */
	if (!kobj)
		return NULL;

	retval = kobject_add(kobj, parent, "%s", name);   /* ���һ��kobject(ʵ��������ӵ��丸�ڵ��������kset��ȥ) */
	if (retval) {
		printk(KERN_WARNING "%s: kobject_add error: %d\n",
		       __func__, retval);
		kobject_put(kobj);   /* �����ӳ������С���ü��� */
		kobj = NULL;
	}
	return kobj;
}
EXPORT_SYMBOL_GPL(kobject_create_and_add);

/**
 * kset_init - initialize a kset for use
 * @k: kset
 */
void kset_init(struct kset *k)/* ��ʼ��kset ����*/
{
	kobject_init_internal(&k->kobj);
	INIT_LIST_HEAD(&k->list);   /* ��kset��ӵ�kset������ */
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

struct sysfs_ops kobj_sysfs_ops = {    /* ʵ��struct kobj_type������(�ļ�)�Ĳ������� */
	.show	= kobj_attr_show,     /* ���û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject ��ָ�����ȷ
	                                                     �����Խṹ������show�������ú�������ָ����ֵ�������뻺��
	                                                     ����Ȼ���ʵ�ʳ�����Ϊ����ֵ���ء�attrָ�����ԣ��������ж�
	                                                     ����������ĸ����ԡ� */
	.store	= kobj_attr_store,    /* �ú����������ڻ����������ݽ��룬�����ø���ʵ�÷���
	                                                    ������ֵ������ʵ�ʽ�����ֽ��� ,ֻ�е�ӵ�����Ե�дȨ
	                                                    ��ʱ�����ܵ��øú���*/
};

/**
 * kset_register - initialize and add a kset.
 * @k: kset.
 */
 /* ��ʼ����ע��һ��kset����(kset�����뵥��kobject����һ���ĵط����ڣ���һ��kset������ϵͳע��ʱ��
      ���linux�ں˱���ʱ������CONFIG_HOTPLUG����ô��Ҫ����һ�¼�֪ͨ�û��ռ䣬���������kobject_uevent
      ������ɣ����һ��kobject���������κ�һ��kset����ô���������kobject�����޷�ͨ��uevent������
      �û��ռ䷢��event��Ϣ��) */
int kset_register(struct kset *k)  
{
	int err;

	if (!k)
		return -EINVAL;

	kset_init(k);   /* ��ʼ��kset */   
	err = kobject_add_internal(&k->kobj);   /* ��kset��Ƕ��kobjectע�ᵽ�ļ�ϵͳ(������Ϊ����kset�����k->kobj��sysfs�ļ���
	                                                                     ������һ����Ŀ¼) */
	if (err)
		return err;
	kobject_uevent(&k->kobj, KOBJ_ADD);   /* ע��kset�����һ���Ȳ���¼� */
	return 0;
}

/**
 * kset_unregister - remove a kset.
 * @k: kset.
 */
void kset_unregister(struct kset *k)  /* ע�� */
{
	if (!k)
		return;
	kobject_put(&k->kobj);   /* ��С���ü�����Ϊ0���ͷ����ڴ�ռ� */
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
struct kobject *kset_find_obj(struct kset *kset, const char *name)   /* ��kset������Ѱ��kobject */
{
	struct kobject *k;
	struct kobject *ret = NULL;

	spin_lock(&kset->list_lock);
	list_for_each_entry(k, &kset->list, entry) {   /* ����kset���� */
		if (kobject_name(k) && !strcmp(kobject_name(k), name)) {
			ret = kobject_get(k);
			break;
		}
	}
	spin_unlock(&kset->list_lock);
	return ret;
}

static void kset_release(struct kobject *kobj)    /* kset���ͷź��� */
{
	struct kset *kset = container_of(kobj, struct kset, kobj);
	pr_debug("kobject: '%s' (%p): %s\n",
		 kobject_name(kobj), kobj, __func__);
	kfree(kset);
}

static struct kobj_type kset_ktype = {   /* ����struct kobj_type�ṹ��Ա */
	.sysfs_ops	= &kobj_sysfs_ops,
	.release = kset_release,    /* kset���ͷź��� */
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
static struct kset *kset_create(const char *name,   /* ��̬�Ĵ���һ��struct kset �ṹ */
				struct kset_uevent_ops *uevent_ops,
				struct kobject *parent_kobj)
{
	struct kset *kset;   /* ����һ��ָ��struct kset��ָ�� */

	kset = kzalloc(sizeof(*kset), GFP_KERNEL);  /* Ϊstruct kset �����ڴ� */
	if (!kset)
		return NULL;
	kobject_set_name(&kset->kobj, name);   /* ����struct kset ����*/
	kset->uevent_ops = uevent_ops;   /* �����Ȳ���¼��������� */
	kset->kobj.parent = parent_kobj;  /* ʹ kset->kobj.parent ָ��ָ�򴫽�����struct kobject*/

	/*
	 * The kobject of this kset will have a type of kset_ktype and belong to
	 * no kset itself.  That way we can properly free it when it is
	 * finished being used.
	 */
	kset->kobj.ktype = &kset_ktype;   /* ���� kset->kobj.ktype */
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
struct kset *kset_create_and_add(const char *name,   /* ��̬�Ĵ���һ�� kset ���󲢰�����ӵ�sysfs�ļ�ϵͳ��*/
				 struct kset_uevent_ops *uevent_ops,
				 struct kobject *parent_kobj)
{
	struct kset *kset;
	int error;

	kset = kset_create(name, uevent_ops, parent_kobj);   /* ����һ��kset */
	if (!kset)
		return NULL;
	error = kset_register(kset);  /* ע�����kset */
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
