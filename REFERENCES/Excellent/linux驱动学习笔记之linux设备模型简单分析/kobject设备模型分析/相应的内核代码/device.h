/*
 * device.h - generic, centralized driver model
 *
 * Copyright (c) 2001-2003 Patrick Mochel <mochel@osdl.org>
 * Copyright (c) 2004-2007 Greg Kroah-Hartman <gregkh@suse.de>
 *
 * This file is released under the GPLv2
 *
 * See Documentation/driver-model/ for more information.
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <linux/ioport.h>
#include <linux/kobject.h>
#include <linux/klist.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/semaphore.h>
#include <asm/atomic.h>
#include <asm/device.h>

#define BUS_ID_SIZE		20

struct device;
struct device_private;
struct device_driver;
struct driver_private;
struct class;
struct class_private;
struct bus_type;
struct bus_type_private;

struct bus_attribute { /* 总线的属性，它代表着该总线特有的信息与配置 */
	struct attribute	attr; /* attr是一个attribute结构，它给出了名字、所有者、二进制属性的权限 */
	ssize_t (*show)(struct bus_type *bus, char *buf); /* 显示属性；当用户空间读取一个属性时，内核会使用指向kobject 的指针和
	                                                                                 正确的属性结构来调用show函数， 该函数将把指定的值编码后放入缓冲区，
	                                                                                 然后把实际长度作为返回值返回。attr指向属性，可用于判断所请求的是
	                                                                                 哪个属性。*/
	                                                                                 
	ssize_t (*store)(struct bus_type *bus, const char *buf, size_t count); /* 设置属性；该函数将保存在缓冲区的数据解码，并调用各种实
	                                                                                                              用方法保存新值，返回实际解码的字节数 只有当拥有属性的
	                                                                                                              写权限时，才能调用该函数*/
};

#define BUS_ATTR(_name, _mode, _show, _store)	\   /* 该宏可用于创建和初始化总线属性结构 ，该宏将定义一个以bus_attr_开头
	                                                                                      的总线属性对象*/
struct bus_attribute bus_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check bus_create_file(struct bus_type *,   /* 创建属于总线的任何属性 文件*/
					struct bus_attribute *);
extern void bus_remove_file(struct bus_type *, struct bus_attribute *); /* 删除属性 文件*/

/*
 *总线、设备和驱动程序：
 *    在设备模型中，所有的设备都通过总线相连，甚至是内部虚拟总线platform。总线可以相互插入 
 *（比如usb控制器通常是一个PCI设备），设备模型展示了总线和它们所控制的设备之间的连接
 *
 *（1）总线：
 *在linux设备模型中，用bus_type结构表示总线,如下所示：
 */
struct bus_type {/* 该结构表示总线 */
	const char		*name;/* 总线的名字 */
	struct bus_attribute	*bus_attrs;/* 总线的属性 */
	struct device_attribute	*dev_attrs;/* 设备的属性 */
	struct driver_attribute	*drv_attrs;/* 驱动的属性 */

	int (*match)(struct device *dev, struct device_driver *drv);/* 匹配函数，匹配则返回非0值（在调用实际的硬件时，mach函数通
	                                                                                                   常对设备提供的硬件ID和驱动所支持的ID做某种比较；但比如
	                                                                                                   platform中使用名字来进行匹配，如果名字相同则说明支持该设备） */
	                                                                                                   
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);/* 热插拔事件处理函数，上面已经介绍了 */
																									   
	int (*probe)(struct device *dev); /*探测函数（其指向下层提供的函数，当mach函数匹配了总线和设备
	                                                       则用它来探测设备）  */
	int (*remove)(struct device *dev);/* 移除设备 */
														   
	void (*shutdown)(struct device *dev);/* 关闭设备 */

	int (*suspend)(struct device *dev, pm_message_t state);/* 挂起函数 */
	
	int (*suspend_late)(struct device *dev, pm_message_t state);/*  */
	int (*resume_early)(struct device *dev);/*  */
	
	int (*resume)(struct device *dev);/* 恢复函数 */

	struct dev_pm_ops *pm;/*总线上一组跟电源管理相关的操作集，用来对总线上的设备进行电源管理  */

	struct bus_type_private *p;/* 该总线的私有结构 (一个用来管理总线上设备与驱动的数据结构)*/
};

extern int __must_check bus_register(struct bus_type *bus);
extern void bus_unregister(struct bus_type *bus);

extern int __must_check bus_rescan_devices(struct bus_type *bus);

/* iterator helpers for buses */

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
		     int (*fn)(struct device *dev, void *data));
struct device *bus_find_device(struct bus_type *bus, struct device *start,
			       void *data,
			       int (*match)(struct device *dev, void *data));
struct device *bus_find_device_by_name(struct bus_type *bus,
				       struct device *start,
				       const char *name);

int __must_check bus_for_each_drv(struct bus_type *bus,
				  struct device_driver *start, void *data,
				  int (*fn)(struct device_driver *, void *));

void bus_sort_breadthfirst(struct bus_type *bus,
			   int (*compare)(const struct device *a,
					  const struct device *b));
/*
 * Bus notifiers: Get notified of addition/removal of devices
 * and binding/unbinding of drivers to devices.
 * In the long run, it should be a replacement for the platform
 * notify hooks.
 */
struct notifier_block;

extern int bus_register_notifier(struct bus_type *bus,
				 struct notifier_block *nb);
extern int bus_unregister_notifier(struct bus_type *bus,
				   struct notifier_block *nb);

/* All 4 notifers below get called with the target struct device *
 * as an argument. Note that those functions are likely to be called
 * with the device semaphore held in the core, so be careful.
 */
#define BUS_NOTIFY_ADD_DEVICE		0x00000001 /* device added */
#define BUS_NOTIFY_DEL_DEVICE		0x00000002 /* device removed */
#define BUS_NOTIFY_BOUND_DRIVER		0x00000003 /* driver bound to device */
#define BUS_NOTIFY_UNBIND_DRIVER	0x00000004 /* driver about to be
						      unbound */

extern struct kset *bus_get_kset(struct bus_type *bus);
extern struct klist *bus_get_device_klist(struct bus_type *bus);

/*
 *驱动程序结构的嵌入：
 *对于大多数驱动程序核心结构来说，device_driver结构通常被包含在高层和总线相关的结构中。
 */
struct device_driver {/* 表示设备驱动 */
	const char		*name; /* 驱动程序的名称 */
	struct bus_type		*bus;/* 该驱动程序所属的总线类型 */

	struct module		*owner;/* 驱动所在的内核模块*/
	/* used for built-in modules */
	const char 		*mod_name;

	int (*probe) (struct device *dev);          /* 探测函数 ，探测设备以判断当前的设备和驱动是否真的匹配以及当前设备
	                                                                   是否处于工作状态，探测成功则返回0(当在总线bus将该驱动与对应的设备进
	                                                                   行绑定时，内核会首先调用bus中的probe函数(如果该bus实现了probe函数)，如果
	                                                                   bus没有实现自己的probe函数，那么内核会调用驱动程序中实现的probe函数)*/
	                                                                   
	int (*remove) (struct device *dev);       /*驱动程序所定义的卸载函数，当调用driver_unregister从系统中卸载一个驱动对象
						                               时，内核会首先调用bus中的remove函数(如果该bus实现了remove函数)，如果bus没
						                               有实现自己的remove函数，那么内核会调用驱动程序中实现的remove函数*/
	void (*shutdown) (struct device *dev); /* 关闭设备函数 */
	int (*suspend) (struct device *dev, pm_message_t state);/* 挂起设备函数 */
	int (*resume) (struct device *dev);/* 恢复设备函数 */
	struct attribute_group **groups;/* */

	struct dev_pm_ops *pm;  /* 电源管理相关的操作集，用来对设备进行电源管理 */

	struct driver_private *p;   /* 驱动私有结构指针 */
};


extern int __must_check driver_register(struct device_driver *drv);
extern void driver_unregister(struct device_driver *drv);

extern struct device_driver *get_driver(struct device_driver *drv);
extern void put_driver(struct device_driver *drv);
extern struct device_driver *driver_find(const char *name,
					 struct bus_type *bus);
extern int driver_probe_done(void);
extern void wait_for_device_probe(void);


/* sysfs interface for exporting driver attributes */

struct driver_attribute {/* 驱动的属性 */
	struct attribute attr;    /*  */
	ssize_t (*show)(struct device_driver *driver, char *buf); 
	                                                /* 显示属性；当用户空间读取一个属性时，内核会使用指向kobject 的指针和正确的属性
	                                                    结构来调用show函数，该函数将把指定的值编码后放入缓冲区，然后把实际长度作为
	                                                    返回值返回。attr指向属性，可用于判断所请求的是哪个属性。 */
	                                                    
	ssize_t (*store)(struct device_driver *driver, const char *buf, size_t count);
	                                                                     /* 设置属性；该函数将保存在缓冲区的数据解码，并调用各种实用方法保存
	                                                                         新值，返回实际解码的字节数只有当拥有属性的写权限时，才能调用该函数 */
	                                                                      
			
};

#define DRIVER_ATTR(_name, _mode, _show, _store)	\  /* 该宏可用于创建和初始化驱动属性对象*/
struct driver_attribute driver_attr_##_name =		\
	__ATTR(_name, _mode, _show, _store)

extern int __must_check driver_create_file(struct device_driver *driver,
					   struct driver_attribute *attr);
extern void driver_remove_file(struct device_driver *driver,
			       struct driver_attribute *attr);

extern int __must_check driver_add_kobj(struct device_driver *drv,
					struct kobject *kobj,
					const char *fmt, ...);

extern int __must_check driver_for_each_device(struct device_driver *drv,
					       struct device *start,
					       void *data,
					       int (*fn)(struct device *dev,
							 void *));
struct device *driver_find_device(struct device_driver *drv,
				  struct device *start, void *data,
				  int (*match)(struct device *dev, void *data));

/*
 * device classes
 */
 /*
 *（1）类是一个设备的高层视图，它抽象出了底层的实现细节。类允许用户空间使用设备所提供的功能，
 *            而不关心设备是如何连接的以及它们是如何工作的。
 *
 *（2）几乎所有的类都显示在/sys/class目录中（例如：不管网络接口的类型是什么，所有的网络接口都集中在
 *             /sys/class/net下；输入设备可以在/sys/class/input下找到，而串行设备都集中在/sys/class/tty）
 *
 *（3）类成员通常被上层代码控制，而不需要来自驱动程序的明确支持
 *
 *（4）在许多情况下，类子系统是向用户空间导出信息的最好的方法，当子系统创建一个类时，它将完全拥
 *             有这个类，因此根本不用担心哪个模块拥有哪些属性
 */
struct class { /* 设备类(相对于device，class是一种更高层次的抽象，用于对设备进行功能上的划分，class用来作为
                           具有同类型功能设备的一个容器(/sys/class目录下:系统中设备类型(如网卡、声卡设备等)))*/
						   	
	const char		*name; /* 类的名字(即创建的设备文件的文件名，此设备文件位于/dev目录下) */
	struct module		*owner;/* 指向模块 ，一般为THIS_MODULE*/

	struct class_attribute		*class_attrs;/* 类的对象的属性 */
	struct device_attribute		*dev_attrs;  /* 类对象所对应的逻辑设备属性 */
	struct kobject			*dev_kobj; /* kobject内核对象 */

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);/* 当热插拔事件产生时，可用该函数来添加环境变量 */

	void (*class_release)(struct class *class); /* 释放类本身 */
	void (*dev_release)(struct device *dev);   /* 把设备从类中删除 */

	int (*suspend)(struct device *dev, pm_message_t state);/*  挂起函数*/
	int (*resume)(struct device *dev);/* 恢复函数 */

	struct dev_pm_ops *pm;
	struct class_private *p;  /* 类的私有结构指针 */
};

struct class_dev_iter {
	struct klist_iter		ki;
	const struct device_type	*type;
};

extern struct kobject *sysfs_dev_block_kobj;
extern struct kobject *sysfs_dev_char_kobj;
extern int __must_check __class_register(struct class *class,
					 struct lock_class_key *key);
extern void class_unregister(struct class *class); /* 类注销 */

/* This is a #define to keep the compiler from merging different
 * instances of the __key variable */
  /* 类注册 */
#define class_register(class)			\  
({						\
	static struct lock_class_key __key;	\
	__class_register(class, &__key);	\
})

extern void class_dev_iter_init(struct class_dev_iter *iter,
				struct class *class,
				struct device *start,
				const struct device_type *type);
extern struct device *class_dev_iter_next(struct class_dev_iter *iter);
extern void class_dev_iter_exit(struct class_dev_iter *iter);

extern int class_for_each_device(struct class *class, struct device *start,
				 void *data,
				 int (*fn)(struct device *dev, void *data));
extern struct device *class_find_device(struct class *class,
					struct device *start, void *data,
					int (*match)(struct device *, void *));

struct class_attribute {    /* 类属性 */
	struct attribute attr;
	ssize_t (*show)(struct class *class, char *buf);
	ssize_t (*store)(struct class *class, const char *buf, size_t count);
};

#define CLASS_ATTR(_name, _mode, _show, _store)			\   /* 该宏可用于创建和初始化类属性结构 */
struct class_attribute class_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check class_create_file(struct class *class,
					  const struct class_attribute *attr);
extern void class_remove_file(struct class *class,
			      const struct class_attribute *attr);

struct class_interface {  /* 类接口 */
	struct list_head	node;
	struct class		*class;

	int (*add_dev)		(struct device *, struct class_interface *);
	void (*remove_dev)	(struct device *, struct class_interface *);
};

extern int __must_check class_interface_register(struct class_interface *);  /* 注册接口类 */
extern void class_interface_unregister(struct class_interface *); /* 注销接口类 */

extern struct class * __must_check __class_create(struct module *owner,
						  const char *name,
						  struct lock_class_key *key);
extern void class_destroy(struct class *cls);

/* This is a #define to keep the compiler from merging different
 * instances of the __key variable */
 /* 该宏用于动态创建设备的逻辑类(生成一个类对象，其主要用途是将同类型的设备添加其中)，并完成部分
     字段的初始化，然后将其添加进linux内核系统，此函数的执行效果就是在/sys/class下创建一个新的文件夹，
     此文件夹的名字为此函数的第二个 参数，但此文件夹为空*/
   
#define class_create(owner, name)		\
({						\
	static struct lock_class_key __key;	\
	__class_create(owner, name, &__key);	\
})

/*
 * The type of device, "struct device" is embedded in. A class
 * or bus can contain devices of different types
 * like "partitions" and "disks", "mouse" and "event".
 * This identifies the device type and carries type-specific
 * information, equivalent to the kobj_type of a kobject.
 * If "name" is specified, the uevent will contain it in
 * the DEVTYPE variable.
 */
struct device_type {  /* 设备类型，其内嵌在struct device */
	const char *name;  /* 设备类型名字 */
	struct attribute_group **groups;  /* 属性 */
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);  /* 热插拔事件处理函数 */
	void (*release)(struct device *dev);

	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);

	struct dev_pm_ops *pm;
};

/* interface for exporting device attributes */
struct device_attribute {/* 设备的属性，sys中的设备入口可以有属性 */
	struct attribute	attr;  /*  */
	ssize_t (*show)(struct device *dev, struct device_attribute *attr, char *buf);
	                                                                                                      /*显示属性；当用户空间读取一个属性时，内核会使用指向kobject 
	                                                                                                       的指针和正确的属性结构来调用show函数，该函数将把指定的值
	                                                                                                       编码后放入缓冲区，然后把实际长度作为返回值返回。attr指向属
	                                                                                                       性，可用于判断所请求的是哪个属性。 */
	                                              
			
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,  const char *buf, size_t count);
										                                    /* 设置属性；该函数将保存在缓冲区的数据解码，并调用各种实
	                                                                                                        用方法保存新值，返回实际解码的字节数只有当拥有属性的写
	                                                                                                        权限时，才能调用该函数*/ 
	                                                                      
			
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \  /* 该宏可用于创建和初始化设备属性结构 */
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check device_create_file(struct device *device,  /* 创建属于设备的任何属性 */
					   struct device_attribute *entry);
extern void device_remove_file(struct device *dev, /* 删除属性 */
			       struct device_attribute *attr);
extern int __must_check device_create_bin_file(struct device *dev, /* 创建属于设备的任何二进制属性 */
					       struct bin_attribute *attr);
extern void device_remove_bin_file(struct device *dev, /* 删除二进制属性 */
				   struct bin_attribute *attr);
extern int device_schedule_callback_owner(struct device *dev,
		void (*func)(struct device *dev), struct module *owner);

/* This is a macro to avoid include problems with THIS_MODULE */
#define device_schedule_callback(dev, func)			\
	device_schedule_callback_owner(dev, func, THIS_MODULE)

/* device resource management */
typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

#ifdef CONFIG_DEBUG_DEVRES
extern void *__devres_alloc(dr_release_t release, size_t size, gfp_t gfp,
			     const char *name);
#define devres_alloc(release, size, gfp) \
	__devres_alloc(release, size, gfp, #release)
#else
extern void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp);
#endif
extern void devres_free(void *res);
extern void devres_add(struct device *dev, void *res);
extern void *devres_find(struct device *dev, dr_release_t release,
			 dr_match_t match, void *match_data);
extern void *devres_get(struct device *dev, void *new_res,
			dr_match_t match, void *match_data);
extern void *devres_remove(struct device *dev, dr_release_t release,
			   dr_match_t match, void *match_data);
extern int devres_destroy(struct device *dev, dr_release_t release,
			  dr_match_t match, void *match_data);

/* devres group */
extern void * __must_check devres_open_group(struct device *dev, void *id,
					     gfp_t gfp);
extern void devres_close_group(struct device *dev, void *id);
extern void devres_remove_group(struct device *dev, void *id);
extern int devres_release_group(struct device *dev, void *id);

/* managed kzalloc/kfree for device drivers, no kmalloc, always use kzalloc */
extern void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
extern void devm_kfree(struct device *dev, void *p);

struct device_dma_parameters {
	/*
	 * a low level driver may set these to teach IOMMU code about
	 * sg limitations.
	 */
	unsigned int max_segment_size;
	unsigned long segment_boundary_mask;
};
/*
 *设备结构的嵌入：
 *在阅读驱动代码的时候可以发现单纯用device结构表示的设备是很少见的，比如pci_dev或usb_device结构定义，
 *均会发现其中隐藏着device结构。
 */
struct device { /* 表示设备 */
	struct device		*parent;/* 设备的父设备----指的是该设备所属的设备；在大多数情况下，一个父设备通常是某
	                                                     种总线或者主机控制器；如果该指针为NULL则表示该设备是顶层设备，但这种情况很
	                                                     少见。 */


	struct device_private	*p;  /* 设备私有结构，其指向该设备的驱动相关的数据 */

	struct kobject kobj;/* 表示该设备并把它连接到结构体系中的kobject内核对象 */
	const char		*init_name; /* initial name of the device *//* 设备对象的名称，在将设备对象加入到系统中时，内核会把init_name
	                                                    设置成kobj成员的名称，后者在sysfs中表现为一个目录*/
	struct device_type	*type;/*设备类型  */

	struct semaphore	sem;	/* semaphore to synchronize calls to its driver*//* 信号量 */
					 
					 

	struct bus_type	*bus;		/* type of bus device is on *//*标识该设备连接在何种类型的总线上  */
	struct device_driver *driver;	/* which driver has allocated this device */   /* 用以表示当前设备是否已经与它的driver进行了绑定，如果该
	                                                                                                                       值为NULL则说明当前设备还没有找到它的driver*/
					   
	void		*driver_data;	/* data private to the driver */ /* 由设备驱动程序使用的私有数据 */
	void		*platform_data;	/* Platform specific data, device  core doesn't touch it *//* platform总线设备私有数据 */  
					   
	struct dev_pm_info	power;/*  */

#ifdef CONFIG_NUMA
	int		numa_node;	/* NUMA node this device is close to *//*  */
#endif
	u64		*dma_mask;	/* dma mask (if dma'able device) *//*  */
	u64		coherent_dma_mask;/* Like dma_mask, but for alloc_coherent mappings as not all hardware supports 64 bit addresses for consistent allocations such descriptors. */
	               /*  */
					     
					     
					     
					    

	struct device_dma_parameters *dma_parms;

	struct list_head	dma_pools;	/* dma pools (if dma'ble) */  /* 设备队列 */

	struct dma_coherent_mem	*dma_mem; /* internal for coherent mem
					     override */
	/* arch specific additions */
	struct dev_archdata	archdata;

	dev_t			devt;	/* dev_t, creates the sysfs "dev" */ /* 设备号 */

	spinlock_t		devres_lock;/*自旋锁  */
	struct list_head	devres_head;

	struct klist_node	knode_class;
	struct class		*class;                 /*类  */
	struct attribute_group	**groups;	/* optional groups *//* 组属性 */

	void	(*release)(struct device *dev);/* 当指向设备的最后一次引用被删除时，内核调用该方法，它将从内嵌的kobject的
	                                                             release方法中调用， 所有向核心注册的device结构都必须要提供该函数，否则内核
	                                                             将打印错误信息。*/
};

/* Get the wakeup routines, which depend on struct device */
#include <linux/pm_wakeup.h>

static inline const char *dev_name(const struct device *dev)
{
	return kobject_name(&dev->kobj);
}

extern int dev_set_name(struct device *dev, const char *name, ...)
			__attribute__((format(printf, 2, 3)));

#ifdef CONFIG_NUMA
static inline int dev_to_node(struct device *dev)
{
	return dev->numa_node;
}
static inline void set_dev_node(struct device *dev, int node)   /*设置设备节点*/
{
	dev->numa_node = node;
}
#else
static inline int dev_to_node(struct device *dev)
{
	return -1;
}
static inline void set_dev_node(struct device *dev, int node)
{
}
#endif

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

static inline unsigned int dev_get_uevent_suppress(const struct device *dev)
{
	return dev->kobj.uevent_suppress;
}

static inline void dev_set_uevent_suppress(struct device *dev, int val)
{
	dev->kobj.uevent_suppress = val;
}

 /*该函数用于判断设备是否已经注册*/
static inline int device_is_registered(struct device *dev)
{
	return dev->kobj.state_in_sysfs;
}

void driver_init(void);

/*
 * High level routines for use by the bus drivers
 */
extern int __must_check device_register(struct device *dev);
extern void device_unregister(struct device *dev);
extern void device_initialize(struct device *dev);
extern int __must_check device_add(struct device *dev);
extern void device_del(struct device *dev);
extern int device_for_each_child(struct device *dev, void *data,
		     int (*fn)(struct device *dev, void *data));
extern struct device *device_find_child(struct device *dev, void *data,
				int (*match)(struct device *dev, void *data));
extern int device_rename(struct device *dev, char *new_name);
extern int device_move(struct device *dev, struct device *new_parent,
		       enum dpm_order dpm_order);

/*
 * Root device objects for grouping under /sys/devices
 */
extern struct device *__root_device_register(const char *name,
					     struct module *owner);
static inline struct device *root_device_register(const char *name)
{
	return __root_device_register(name, THIS_MODULE);
}
extern void root_device_unregister(struct device *root);

/*
 * Manual binding of a device to driver. See drivers/base/bus.c
 * for information on use.
 */
extern int __must_check device_bind_driver(struct device *dev);
extern void device_release_driver(struct device *dev);
extern int  __must_check device_attach(struct device *dev);
extern int __must_check driver_attach(struct device_driver *drv);
extern int __must_check device_reprobe(struct device *dev);

/*
 * Easy functions for dynamically creating devices on the fly
 */
extern struct device *device_create_vargs(struct class *cls,
					  struct device *parent,
					  dev_t devt,
					  void *drvdata,
					  const char *fmt,
					  va_list vargs);
extern struct device *device_create(struct class *cls, struct device *parent,
				    dev_t devt, void *drvdata,
				    const char *fmt, ...)
				    __attribute__((format(printf, 5, 6)));
extern void device_destroy(struct class *cls, dev_t devt);

/*
 * Platform "fixup" functions - allow the platform to have their say
 * about devices and actions that the general device layer doesn't
 * know about.
 */
/* Notify platform of device discovery */
extern int (*platform_notify)(struct device *dev);

extern int (*platform_notify_remove)(struct device *dev);


/**
 * get_device - atomically increment the reference count for the device.
 *
 */
extern struct device *get_device(struct device *dev);
extern void put_device(struct device *dev);

extern void wait_for_device_probe(void);

/* drivers/base/power/shutdown.c */
extern void device_shutdown(void);

/* drivers/base/sys.c */
extern void sysdev_shutdown(void);

/* debugging and troubleshooting/diagnostic helpers. */
extern const char *dev_driver_string(const struct device *dev);
#define dev_printk(level, dev, format, arg...)	\
	printk(level "%s %s: " format , dev_driver_string(dev) , \
	       dev_name(dev) , ## arg)

#define dev_emerg(dev, format, arg...)		\
	dev_printk(KERN_EMERG , dev , format , ## arg)
#define dev_alert(dev, format, arg...)		\
	dev_printk(KERN_ALERT , dev , format , ## arg)
#define dev_crit(dev, format, arg...)		\
	dev_printk(KERN_CRIT , dev , format , ## arg)
#define dev_err(dev, format, arg...)		\
	dev_printk(KERN_ERR , dev , format , ## arg)
#define dev_warn(dev, format, arg...)		\
	dev_printk(KERN_WARNING , dev , format , ## arg)
#define dev_notice(dev, format, arg...)		\
	dev_printk(KERN_NOTICE , dev , format , ## arg)
#define dev_info(dev, format, arg...)		\
	dev_printk(KERN_INFO , dev , format , ## arg)

#if defined(DEBUG)
#define dev_dbg(dev, format, arg...)		\
	dev_printk(KERN_DEBUG , dev , format , ## arg)
#elif defined(CONFIG_DYNAMIC_DEBUG)
#define dev_dbg(dev, format, ...) do { \
	dynamic_dev_dbg(dev, format, ##__VA_ARGS__); \
	} while (0)
#else
#define dev_dbg(dev, format, arg...)		\
	({ if (0) dev_printk(KERN_DEBUG, dev, format, ##arg); 0; })
#endif

#ifdef VERBOSE_DEBUG
#define dev_vdbg	dev_dbg
#else

#define dev_vdbg(dev, format, arg...)		\
	({ if (0) dev_printk(KERN_DEBUG, dev, format, ##arg); 0; })
#endif

/*
 * dev_WARN() acts like dev_printk(), but with the key difference
 * of using a WARN/WARN_ON to get the message out, including the
 * file/line information and a backtrace.
 */
#define dev_WARN(dev, format, arg...) \
	WARN(1, "Device: %s\n" format, dev_driver_string(dev), ## arg);

/* Create alias, so I can be autoloaded. */
#define MODULE_ALIAS_CHARDEV(major,minor) \
	MODULE_ALIAS("char-major-" __stringify(major) "-" __stringify(minor))
#define MODULE_ALIAS_CHARDEV_MAJOR(major) \
	MODULE_ALIAS("char-major-" __stringify(major) "-*")
#endif /* _DEVICE_H_ */
