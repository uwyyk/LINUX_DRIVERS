/*
 * kobject.h - generic kernel object infrastructure.
 *
 * Copyright (c) 2002-2003 Patrick Mochel
 * Copyright (c) 2002-2003 Open Source Development Labs
 * Copyright (c) 2006-2008 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (c) 2006-2008 Novell Inc.
 *
 * This file is released under the GPLv2.
 *
 * Please read Documentation/kobject.txt before using the kobject
 * interface, ESPECIALLY the parts about reference counts and object
 * destructors.
 */

#ifndef _KOBJECT_H_
#define _KOBJECT_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/kref.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <asm/atomic.h>

#define UEVENT_HELPER_PATH_LEN		256
#define UEVENT_NUM_ENVP			32	/* number of env pointers */
#define UEVENT_BUFFER_SIZE		2048	/* buffer for the variables */

/* path to the userspace helper executed on an event */
extern char uevent_helper[];

/* counter to tag the uevent, read only except for the kobject core */
extern u64 uevent_seqnum;

/*
 * The actions here must match the index to the string array
 * in lib/kobject_uevent.c
 *
 * Do not add new actions here without checking with the driver-core
 * maintainers. Action strings are not meant to express subsystem
 * or device specific properties. In most cases you want to send a
 * kobject_uevent_env(kobj, KOBJ_CHANGE, env) with additional event
 * specific variables added to the event environment.
 */
enum kobject_action {
	KOBJ_ADD,              /* ���*/
	KOBJ_REMOVE,         /* �Ƴ� */
	KOBJ_CHANGE,        /* �仯 */
	KOBJ_MOVE,             /*�ƶ� */
	KOBJ_ONLINE,
	KOBJ_OFFLINE,
	KOBJ_MAX
};


/*
 *1��kobject������豸ģ�͵Ļ����ṹ��kobject�ṹ���ܴ���������Լ���֧�ֵĴ��������
 *
 *��1����������ü�����ͨ��һ���ں˶��󱻴���ʱ��������֪���ö������ʱ�䣬���ٴ˶�����������ڵ�
 *             һ��������ʹ�����ü��������ں���û�д�����иö���ʱ��
 *            �ö��󽫽����Լ�����Ч�������ڣ����ҿ���ɾ����
 *
 *��2��sysfs��������sysfs����ʾ��ÿ�����󶼶�Ӧһ��kobject�������������ں˽�������������
 *
 *��3�����ݽṹ�������豸ģ����һ�����ӵ����ݽṹ��ͨ�������Ĵ������Ӷ�����һ�����ε���ϵ
 *            �ṹ��kobjectʵ���˸ýṹ�������Ǿۺ���һ��
 *
 *��4���Ȳ���¼�������ϵͳ�е�Ӳ�����Ȳ��ʱ����kobject��ϵͳ�����£��������¼���֪ͨ�û��ռ�
 *
 *            �ýṹһ��û�е������壬����Ƕ�뵽�����豸�����ṹ���У������ַ��豸��
 *
 *
 */
struct kobject { /* ��ʾһ���ں�(linux�豸ģ�͵ĺ��� ������ͨ�����÷���Ƕ�ڱ�ʾĳһ�������
                                �ݽṹ�У�����Ƕ����struct cdev�ṹ����)*/
									
	const char		*name;           /* ������ʾ�ں˶�������֣�������ں˶������ϵͳ����ô����n
	                                                        ame���������sysfs�ļ�ϵͳ��(������ʽ��һ���µ�Ŀ¼��) */
																
	struct list_head	entry;             /* ������һϵ�е��ں˶��󹹳����� */
															
	struct kobject		*parent;  /* ָ���豸�ֲ�ʱ����һ����Ľڵ㣨����һ��kobject�ṹ��ʾusb�豸����ô�ó�Ա
	                                                       ����ָ���˱�ʾusb�������Ķ��󣬶�usb�豸�ǲ���usb�������ϵģ�����ָ������
	                                                       Ҫ����;����sysfs�ֲ�ṹ�ж�λ�����Դ����ں��й���һ�������νṹ��
	                                                       ���ҿ��Խ��������֮��Ĺ�ϵ���ֳ����������sysfs������:һ���û��ռ���ļ�
	                                                       ϵͳ��������ʾ�ں���kobject����Ĳ��*/
	                                                       
	struct kset		*kset;             /* kset�������һ��subsystem������������һϵ��ͬ���͵�kobject(�ýṹ
	                                                       �ȽϹ��Ķ���ľۼ��ڼ��ϣ�kset������sysfs�г��֣�һ��������
	                                                       kset���� ����ӵ�ϵͳ�У�����sysfs�д���һ��Ŀ¼ */
	                                                       
																
	struct kobj_type	*ktype;           /* �䶨���˸��ں˶����һ��sysfs�ļ�ϵͳ�Ĳ������������ԣ���Ȼ
	                                                        ��ͬ���͵��ں˶�����в�ͬ��ktype����������kobject��������ں�
	                                                        ��������ʣ��ں�ͨ��ktype��Ա��kobject�����sysfs�ļ�������������
	                                                        �ļ���������*/
	                                                        
	struct sysfs_dirent	*sd;                /* ������ʾ�ں˶�����sysfs�ļ�ϵͳ�ж�Ӧ��Ŀ¼���ʵ�� */
															
	struct kref		kref;              /*	��ʾ�ں� ��������ü������ں�ͨ���ó�Ա׷���ں˶������������ */
	
	unsigned int state_initialized:1;  /* ��ʾ��kobject��������ں˶����ʼ����״̬��1��ʾ�Ѿ�����ʼ����
	                                                       0��ʾ��δ��ʼ��*/
														   	
	unsigned int state_in_sysfs:1;        /* ��ʾ��kobject��������ں˶�����û����sysfs�ļ�ϵͳ�н���һ����ڵ�
	                                                         (��sysfs���������ļ��ṹ�ɹ�����λ��־λ) */
														   
	unsigned int state_add_uevent_sent:1;   /* ��������豸�¼� ��־λ*/
																		  
	unsigned int state_remove_uevent_sent:1;  /* �����Ƴ��豸�¼���־λ */
	
	unsigned int uevent_suppress:1;   /* �����kobject����������ĳһkset����ô����״̬�仯���Ե���������
	                                                          ��kset�������û��ռ䷢��event��Ϣ��uevent_suppress������ʾ����kobject
	                                                          ״̬�����仯ʱ���Ƿ��������ڵ�kset���û��ռ䷢��event��Ϣ(����
	                                                          ���ֶ�Ϊ1ʱ���������Ȳ���¼� )*/
};

extern int kobject_set_name(struct kobject *kobj, const char *name, ...)
			    __attribute__((format(printf, 2, 3)));
extern int kobject_set_name_vargs(struct kobject *kobj, const char *fmt,
				  va_list vargs);

static inline const char *kobject_name(const struct kobject *kobj)
{
	return kobj->name;
}

extern void kobject_init(struct kobject *kobj, struct kobj_type *ktype);
extern int __must_check kobject_add(struct kobject *kobj,
				    struct kobject *parent,
				    const char *fmt, ...);
extern int __must_check kobject_init_and_add(struct kobject *kobj,
					     struct kobj_type *ktype,
					     struct kobject *parent,
					     const char *fmt, ...);

extern void kobject_del(struct kobject *kobj);

extern struct kobject * __must_check kobject_create(void);
extern struct kobject * __must_check kobject_create_and_add(const char *name,
						struct kobject *parent);

extern int __must_check kobject_rename(struct kobject *, const char *new_name);
extern int __must_check kobject_move(struct kobject *, struct kobject *);

extern struct kobject *kobject_get(struct kobject *kobj);
extern void kobject_put(struct kobject *kobj);

extern char *kobject_get_path(struct kobject *kobj, gfp_t flag);

struct kobj_type {  /*kobject����������(�����kobject ���ͽ��̸���)*/
	void (*release)(struct kobject *kobj);  /* ָ���ͷź��� */
	struct sysfs_ops *sysfs_ops;              /* ���ֶ�����ʵ����Щ����(�ļ�)�Ĳ��� */
	struct attribute **default_attrs;         /* ���������б����ڴ��������͵�ÿһ��kobject��default_attrs�����е����һ��
	                                                               Ԫ�ر���������䣩���ó�Ա������kobjectӵ�е��������� */
};

struct kobj_uevent_env {  /*�������� kobject�Ȳ���¼��� ��������*/
	char *envp[UEVENT_NUM_ENVP];  /* �䱣�������������� */
	int envp_idx;                               /* �ñ���˵���ж��ٸ�������� */
	char buf[UEVENT_BUFFER_SIZE];   /* ��ű����ı��� */
	int buflen;                                    /*  ��ű����ı����ĳ���*/
};
/*
 *�Ȳ���¼���
 *��1��һ���Ȳ���¼��Ǵ��ں˿ռ䷢�͵��û��ռ��֪ͨ��������ϵͳ���ó����˱仯������kobject������
 *             ���Ǳ�ɾ����������������¼������統�������ͨ��USB�߲���ϵͳʱ�����û��л�����̨ʱ����
 *             �������̷���ʱ������������¼����Ȳ���¼��ᵼ�¶�/sbin/hotplug(/sbin/mdev)����ĵ��ã��ó���ͨ��
 *             ��װ�������򣬴����豸�ڵ㣬���ط�������������ȷ�Ķ�������Ӧ�¼�.
 *��2����������һ����Ҫ��kobject��������������Щ�¼��������ǰ�kobject���ݸ�kobject_add��kobject_delʱ���Ż�����
 *             ������Щ�¼������¼������ݵ��û��ռ�֮ǰ������kobject������׼ȷЩ����kobject��
 *            �Ĵ����ܹ�Ϊ�û��ռ������Ϣ������ȫ��ֹ�¼��Ĳ�����
 *��3���Ȳ���¼��Ĵ���ͨ���������������򼶱���߼�������
 *��4�����Ȳ���¼���ʵ�ʿ������ɱ�����struct kset�ṹ��ĳ�Աstruct kset_uevent_ops �ṹ�к��������
 */
struct kset_uevent_ops {   /* struct kset�ĳ�Ա�����Ƕ����Ȳ���¼�֧�ֵĲ������� */

	int (*filter)(struct kset *kset, struct kobject *kobj);  /* �¼����˺���������ʲôʱ�򣬵��ں�ҪΪָ���� kobject�����¼�ʱ��
	                                                                                    ��Ҫ���øú���������ú�������0�򽫲������¼�����˸ú�����
	                                                                                    kset����һ���������ھ����Ƿ����û��ռ䴫���ض��¼��� */ 
	                                                                                    
	const char *(*name)(struct kset *kset, struct kobject *kobj);/* �����û��ռ���Ȳ�γ���ʱ�������ϵͳ�����ֽ���ΪΨһ��
	                                                                                                �������ݸ������ú��������ṩ������֣���������һ�����ʴ�
	                                                                                                �ݸ��û��ռ���ַ��� */

       int (*uevent)(struct kset *kset, struct kobject *kobj,
		      struct kobj_uevent_env *env);                        /*�κ��Ȳ�νű�����Ҫ֪������Ϣ��ͨ�������������ݣ��ú�������
                                                                                          ���ýű�ǰ���ṩ��ӻ��������Ļ���,���Ҫ��envp������κα�����
                                                                                          ��ȷ�������һ���±��������NULL�������ں˾�֪�������ǽ�������.
                                                                                          �ú���ͨ������0�����ط�0����ֹ�Ȳ���¼��Ĳ���.
                                                                                          struct kobj_uevent_env {
	                                                                                                                        char *envp[UEVENT_NUM_ENVP];   //�䱣��������������
                                                                                                                               int envp_idx;                                //�ñ���˵���ж��ٸ��������
	                                                                                                                        char buf[UEVENT_BUFFER_SIZE];   //��ű����ı���
	                                                                                                                        int buflen;                                    //��ű����ı����ĳ���
                                                                                                                             }*/
};

struct kobj_attribute {   /* kobject���� */
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count);
};

extern struct sysfs_ops kobj_sysfs_ops;

/**
 * struct kset - a set of kobjects of a specific type, belonging to a specific subsystem.
 *
 * A kset defines a group of kobjects.  They can be individually
 * different "types" but overall these kobjects all want to be grouped
 * together and operated on in the same manner.  ksets are used to
 * define the attribute callbacks and other common events that happen to
 * a kobject.
 *
 * @list: the list of all kobjects for this kset
 * @list_lock: a lock for iterating over the kobjects
 * @kobj: the embedded kobject for this kset (recursion, isn't it fun...)
 * @uevent_ops: the set of uevent operations for this kset.  These are
 * called whenever a kobject has something happen to it so that the kset
 * can add new environment variables, or filter out the uevents if so
 * desired.
 */
struct kset {   /* ��kobject����Ҫ��Ա����Ƕ�뵽һ���������ϵͳ(�����ĵ��Ƕ���ļ�����ۺ���������sysfs��
                            ���֣�һ��������kset��������ӵ�ϵͳ�У�����sysfs�д���һ��Ŀ¼��kobject������sysfs�б�ʾ��
                            ����kset�е�ÿһ�� kobject��Ա������sysfs�еõ�����) */
	struct list_head list; /* �������ڽ�kobject���󹹽�������*/
	spinlock_t list_lock;  /* ���ڶ�kset��list������з��ʲ���ʱ������Ϊ���Ᵽ��ʹ�õ������� */
	struct kobject kobj;  /* kobject���� */
	struct kset_uevent_ops *uevent_ops;  /*������һ�麯������kset��ĳЩkobject�������仯��Ҫ֪ͨ�û�
	                                                               �ռ�ʱ���������еĺ��������*/
};

extern void kset_init(struct kset *kset);
extern int __must_check kset_register(struct kset *kset);
extern void kset_unregister(struct kset *kset);
extern struct kset * __must_check kset_create_and_add(const char *name,
						struct kset_uevent_ops *u,
						struct kobject *parent_kobj);

static inline struct kset *to_kset(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct kset, kobj) : NULL;
}

static inline struct kset *kset_get(struct kset *k)  /* ����kset���ü��� */
{
	return k ? to_kset(kobject_get(&k->kobj)) : NULL;
}

static inline void kset_put(struct kset *k)  /* ����kset���ü��� */
{
	kobject_put(&k->kobj);
}

static inline struct kobj_type *get_ktype(struct kobject *kobj)  /* ��ȡ kobject ��Աktype��ָ��*/
{
	return kobj->ktype;
}

extern struct kobject *kset_find_obj(struct kset *, const char *);

/* The global /sys/kernel/ kobject for people to chain off of */
extern struct kobject *kernel_kobj;
/* The global /sys/kernel/mm/ kobject for people to chain off of */
extern struct kobject *mm_kobj;
/* The global /sys/hypervisor/ kobject for people to chain off of */
extern struct kobject *hypervisor_kobj;
/* The global /sys/power/ kobject for people to chain off of */
extern struct kobject *power_kobj;
/* The global /sys/firmware/ kobject for people to chain off of */
extern struct kobject *firmware_kobj;

#if defined(CONFIG_HOTPLUG)
int kobject_uevent(struct kobject *kobj, enum kobject_action action);
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,
			char *envp[]);

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
	__attribute__((format (printf, 2, 3)));

int kobject_action_type(const char *buf, size_t count,
			enum kobject_action *type);
#else
static inline int kobject_uevent(struct kobject *kobj,
				 enum kobject_action action)
{ return 0; }
static inline int kobject_uevent_env(struct kobject *kobj,
				      enum kobject_action action,
				      char *envp[])
{ return 0; }

static inline int add_uevent_var(struct kobj_uevent_env *env,
				 const char *format, ...)
{ return 0; }

static inline int kobject_action_type(const char *buf, size_t count,
				      enum kobject_action *type)
{ return -EINVAL; }
#endif

#endif /* _KOBJECT_H_ */
