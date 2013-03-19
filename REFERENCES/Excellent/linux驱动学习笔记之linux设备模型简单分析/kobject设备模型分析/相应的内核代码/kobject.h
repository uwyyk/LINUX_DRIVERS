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
	KOBJ_ADD,              /* 添加*/
	KOBJ_REMOVE,         /* 移除 */
	KOBJ_CHANGE,        /* 变化 */
	KOBJ_MOVE,             /*移动 */
	KOBJ_ONLINE,
	KOBJ_OFFLINE,
	KOBJ_MAX
};


/*
 *1：kobject是组成设备模型的基本结构，kobject结构所能处理的任务以及所支持的代码包括：
 *
 *（1）对象的引用计数：通常一个内核对象被创建时，不可能知道该对象存活的时间，跟踪此对象的生命周期的
 *             一个方法是使用引用计数，当内核中没有代码持有该对象时，
 *            该对象将结束自己的有效生命周期，并且可以删除。
 *
 *（2）sysfs表述：在sysfs中显示的每个对象都对应一个kobject，它被用来与内核交互并创建它。
 *
 *（3）数据结构关联：设备模型是一个复杂的数据结构，通过在其间的大量连接而构成一个多层次的体系
 *            结构，kobject实现了该结构并把它们聚合在一起。
 *
 *（4）热插拔事件处理：当系统中的硬件被热插拔时，在kobject子系统控制下，将产生事件以通知用户空间
 *
 *            该结构一般没有单独定义，而是嵌入到其他设备描述结构体中，比如字符设备。
 *
 *
 */
struct kobject { /* 表示一个内核(linux设备模型的核心 ，其最通常的用法是嵌在表示某一对象的数
                                据结构中，比如嵌入在struct cdev结构体中)*/
									
	const char		*name;           /* 用来表示内核对象的名字，如果该内核对象加入系统，那么它的n
	                                                        ame将会出现在sysfs文件系统中(表现形式是一个新的目录名) */
																
	struct list_head	entry;             /* 用来将一系列的内核对象构成链表 */
															
	struct kobject		*parent;  /* 指向设备分层时的上一层其的节点（比如一个kobject结构表示usb设备，那么该成员
	                                                       可能指向了表示usb集线器的对象，而usb设备是插在usb集线器上的），该指针最重
	                                                       要的用途是在sysfs分层结构中定位对象，以此在内核中构造一个对象层次结构，
	                                                       并且可以将多个对象之间的关系表现出来，这既是sysfs的真相:一个用户空间的文件
	                                                       系统，用来表示内核中kobject对象的层次*/
	                                                       
	struct kset		*kset;             /* kset对象代表一个subsystem，其中容纳了一系列同类型的kobject(该结构
	                                                       比较关心对象的聚集于集合，kset总是在sysfs中出现，一旦设置了
	                                                       kset并把 它添加到系统中，将在sysfs中创建一个目录 */
	                                                       
																
	struct kobj_type	*ktype;           /* 其定义了该内核对象的一组sysfs文件系统的操作函数和属性，显然
	                                                        不同类型的内核对象会有不同的ktype，用以体现kobject所代表的内核
	                                                        对象的特质，内核通过ktype成员将kobject对象的sysfs文件操作与其属性
	                                                        文件关联起来*/
	                                                        
	struct sysfs_dirent	*sd;                /* 用来表示内核对象在sysfs文件系统中对应的目录项的实例 */
															
	struct kref		kref;              /*	表示内核 对象的引用计数，内核通过该成员追踪内核对象的生命周期 */
	
	unsigned int state_initialized:1;  /* 表示该kobject所代表的内核对象初始化的状态，1表示已经被初始化，
	                                                       0表示尚未初始化*/
														   	
	unsigned int state_in_sysfs:1;        /* 表示该kobject所代表的内核对象有没有在sysfs文件系统中建立一个入口点
	                                                         (在sysfs创建属性文件结构成功则置位标志位) */
														   
	unsigned int state_add_uevent_sent:1;   /* 产生添加设备事件 标志位*/
																		  
	unsigned int state_remove_uevent_sent:1;  /* 产生移除设备事件标志位 */
	
	unsigned int uevent_suppress:1;   /* 如果该kobject对象隶属于某一kset，那么它的状态变化可以导致其所在
	                                                          的kset对象向用户空间发送event消息，uevent_suppress用来表示当该kobject
	                                                          状态发生变化时，是否让其所在的kset向用户空间发送event消息(设置
	                                                          该字段为1时则忽略这个热插拔事件 )*/
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

struct kobj_type {  /*kobject的类型属性(负责对kobject 类型进程跟踪)*/
	void (*release)(struct kobject *kobj);  /* 指向释放函数 */
	struct sysfs_ops *sysfs_ops;              /* 该字段用于实现这些属性(文件)的操作 */
	struct attribute **default_attrs;         /* 保存属性列表，用于创建该类型的每一个kobject（default_attrs链表中的最后一个
	                                                               元素必须用零填充），该成员描述了kobject拥有的所有属性 */
};

struct kobj_uevent_env {  /*用来保存 kobject热插拔事件的 环境变量*/
	char *envp[UEVENT_NUM_ENVP];  /* 其保存其他环境变量 */
	int envp_idx;                               /* 该变量说明有多少个变量入口 */
	char buf[UEVENT_BUFFER_SIZE];   /* 存放编码后的变量 */
	int buflen;                                    /*  存放编码后的变量的长度*/
};
/*
 *热插拔事件：
 *（1）一个热插拔事件是从内核空间发送到用户空间的通知，它表明系统配置出现了变化。无论kobject被创建
 *             还是被删除，都会产生这种事件，比如当数码相机通过USB线插入系统时或者用户切换控制台时或者
 *             当给磁盘分区时都会产生这类事件。热插拔事件会导致对/sbin/hotplug(/sbin/mdev)程序的调用，该程序通过
 *             加装驱动程序，创建设备节点，挂载分区或者其他正确的动作来响应事件.
 *（2）下面讨论一个重要的kobject函数用来产生这些事件。当我们把kobject传递给kobject_add或kobject_del时，才会真正
 *             产生这些事件。在事件被传递到用户空间之前，处理kobject（或者准确些，是kobject）
 *            的代码能够为用户空间添加信息或者完全禁止事件的产生。
 *（3）热插拔事件的创建通常被总线驱动程序级别的逻辑所控制
 *（4）对热插拔事件的实际控制是由保存在struct kset结构体的成员struct kset_uevent_ops 结构中函数来完成
 */
struct kset_uevent_ops {   /* struct kset的成员，其是对于热插拔事件支持的操作函数 */

	int (*filter)(struct kset *kset, struct kobject *kobj);  /* 事件过滤函数，无论什么时候，当内核要为指定的 kobject产生事件时，
	                                                                                    都要调用该函数；如果该函数返回0则将不产生事件。因此该函数给
	                                                                                    kset代码一个机会用于决定是否向用户空间传递特定事件。 */ 
	                                                                                    
	const char *(*name)(struct kset *kset, struct kobject *kobj);/* 调用用户空间的热插拔程序时，相关子系统的名字将作为唯一的
	                                                                                                参数传递给它，该函数负责提供这个名字，它将返回一个合适传
	                                                                                                递给用户空间的字符串 */

       int (*uevent)(struct kset *kset, struct kobject *kobj,
		      struct kobj_uevent_env *env);                        /*任何热插拔脚本所需要知道的信息将通过环境变量传递，该函数会在
                                                                                          调用脚本前，提供添加环境变量的机会,如果要在envp中添加任何变量，
                                                                                          请确保在最后一个新变量后加入NULL，这样内核就知道哪里是结束点了.
                                                                                          该函数通常返回0，返回非0将终止热插拔事件的产生.
                                                                                          struct kobj_uevent_env {
	                                                                                                                        char *envp[UEVENT_NUM_ENVP];   //其保存其他环境变量
                                                                                                                               int envp_idx;                                //该变量说明有多少个变量入口
	                                                                                                                        char buf[UEVENT_BUFFER_SIZE];   //存放编码后的变量
	                                                                                                                        int buflen;                                    //存放编码后的变量的长度
                                                                                                                             }*/
};

struct kobj_attribute {   /* kobject属性 */
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
struct kset {   /* 是kobject的重要成员，被嵌入到一个特殊的子系统(它关心的是对象的集合与聚合且总是在sysfs中
                            出现，一旦设置了kset并把它添加到系统中，将在sysfs中创建一个目录；kobject不必在sysfs中表示，
                            但是kset中的每一个 kobject成员都将在sysfs中得到表述) */
	struct list_head list; /* 用于用于将kobject对象构建成链表*/
	spinlock_t list_lock;  /* 用于对kset的list链表进行访问操作时用来作为互斥保护使用的自旋锁 */
	struct kobject kobj;  /* kobject对象 */
	struct kset_uevent_ops *uevent_ops;  /*定义了一组函数，当kset的某些kobject对象发生变化需要通知用户
	                                                               空间时，调用其中的函数来完成*/
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

static inline struct kset *kset_get(struct kset *k)  /* 增加kset引用计数 */
{
	return k ? to_kset(kobject_get(&k->kobj)) : NULL;
}

static inline void kset_put(struct kset *k)  /* 减少kset引用计数 */
{
	kobject_put(&k->kobj);
}

static inline struct kobj_type *get_ktype(struct kobject *kobj)  /* 获取 kobject 成员ktype的指针*/
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
