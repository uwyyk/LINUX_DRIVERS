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

struct bus_attribute { /* ���ߵ����ԣ��������Ÿ��������е���Ϣ������ */
	struct attribute	attr; /* attr��һ��attribute�ṹ�������������֡������ߡ����������Ե�Ȩ�� */
	ssize_t (*show)(struct bus_type *bus, char *buf); /* ��ʾ���ԣ����û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject ��ָ���
	                                                                                 ��ȷ�����Խṹ������show������ �ú�������ָ����ֵ�������뻺������
	                                                                                 Ȼ���ʵ�ʳ�����Ϊ����ֵ���ء�attrָ�����ԣ��������ж����������
	                                                                                 �ĸ����ԡ�*/
	                                                                                 
	ssize_t (*store)(struct bus_type *bus, const char *buf, size_t count); /* �������ԣ��ú����������ڻ����������ݽ��룬�����ø���ʵ
	                                                                                                              �÷���������ֵ������ʵ�ʽ�����ֽ��� ֻ�е�ӵ�����Ե�
	                                                                                                              дȨ��ʱ�����ܵ��øú���*/
};

#define BUS_ATTR(_name, _mode, _show, _store)	\   /* �ú�����ڴ����ͳ�ʼ���������Խṹ ���ú꽫����һ����bus_attr_��ͷ
	                                                                                      ���������Զ���*/
struct bus_attribute bus_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check bus_create_file(struct bus_type *,   /* �����������ߵ��κ����� �ļ�*/
					struct bus_attribute *);
extern void bus_remove_file(struct bus_type *, struct bus_attribute *); /* ɾ������ �ļ�*/

/*
 *���ߡ��豸����������
 *    ���豸ģ���У����е��豸��ͨ�������������������ڲ���������platform�����߿����໥���� 
 *������usb������ͨ����һ��PCI�豸�����豸ģ��չʾ�����ߺ����������Ƶ��豸֮�������
 *
 *��1�����ߣ�
 *��linux�豸ģ���У���bus_type�ṹ��ʾ����,������ʾ��
 */
struct bus_type {/* �ýṹ��ʾ���� */
	const char		*name;/* ���ߵ����� */
	struct bus_attribute	*bus_attrs;/* ���ߵ����� */
	struct device_attribute	*dev_attrs;/* �豸������ */
	struct driver_attribute	*drv_attrs;/* ���������� */

	int (*match)(struct device *dev, struct device_driver *drv);/* ƥ�亯����ƥ���򷵻ط�0ֵ���ڵ���ʵ�ʵ�Ӳ��ʱ��mach����ͨ
	                                                                                                   �����豸�ṩ��Ӳ��ID��������֧�ֵ�ID��ĳ�ֱȽϣ�������
	                                                                                                   platform��ʹ������������ƥ�䣬���������ͬ��˵��֧�ָ��豸�� */
	                                                                                                   
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);/* �Ȳ���¼��������������Ѿ������� */
																									   
	int (*probe)(struct device *dev); /*̽�⺯������ָ���²��ṩ�ĺ�������mach����ƥ�������ߺ��豸
	                                                       ��������̽���豸��  */
	int (*remove)(struct device *dev);/* �Ƴ��豸 */
														   
	void (*shutdown)(struct device *dev);/* �ر��豸 */

	int (*suspend)(struct device *dev, pm_message_t state);/* ������ */
	
	int (*suspend_late)(struct device *dev, pm_message_t state);/*  */
	int (*resume_early)(struct device *dev);/*  */
	
	int (*resume)(struct device *dev);/* �ָ����� */

	struct dev_pm_ops *pm;/*������һ�����Դ������صĲ������������������ϵ��豸���е�Դ����  */

	struct bus_type_private *p;/* �����ߵ�˽�нṹ (һ�����������������豸�����������ݽṹ)*/
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
 *��������ṹ��Ƕ�룺
 *���ڴ��������������Ľṹ��˵��device_driver�ṹͨ���������ڸ߲��������صĽṹ�С�
 */
struct device_driver {/* ��ʾ�豸���� */
	const char		*name; /* ������������� */
	struct bus_type		*bus;/* ������������������������ */

	struct module		*owner;/* �������ڵ��ں�ģ��*/
	/* used for built-in modules */
	const char 		*mod_name;

	int (*probe) (struct device *dev);          /* ̽�⺯�� ��̽���豸���жϵ�ǰ���豸�������Ƿ����ƥ���Լ���ǰ�豸
	                                                                   �Ƿ��ڹ���״̬��̽��ɹ��򷵻�0(��������bus�����������Ӧ���豸��
	                                                                   �а�ʱ���ں˻����ȵ���bus�е�probe����(�����busʵ����probe����)�����
	                                                                   busû��ʵ���Լ���probe��������ô�ں˻��������������ʵ�ֵ�probe����)*/
	                                                                   
	int (*remove) (struct device *dev);       /*���������������ж�غ�����������driver_unregister��ϵͳ��ж��һ����������
						                               ʱ���ں˻����ȵ���bus�е�remove����(�����busʵ����remove����)�����busû
						                               ��ʵ���Լ���remove��������ô�ں˻��������������ʵ�ֵ�remove����*/
	void (*shutdown) (struct device *dev); /* �ر��豸���� */
	int (*suspend) (struct device *dev, pm_message_t state);/* �����豸���� */
	int (*resume) (struct device *dev);/* �ָ��豸���� */
	struct attribute_group **groups;/* */

	struct dev_pm_ops *pm;  /* ��Դ������صĲ��������������豸���е�Դ���� */

	struct driver_private *p;   /* ����˽�нṹָ�� */
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

struct driver_attribute {/* ���������� */
	struct attribute attr;    /*  */
	ssize_t (*show)(struct device_driver *driver, char *buf); 
	                                                /* ��ʾ���ԣ����û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject ��ָ�����ȷ������
	                                                    �ṹ������show�������ú�������ָ����ֵ�������뻺������Ȼ���ʵ�ʳ�����Ϊ
	                                                    ����ֵ���ء�attrָ�����ԣ��������ж�����������ĸ����ԡ� */
	                                                    
	ssize_t (*store)(struct device_driver *driver, const char *buf, size_t count);
	                                                                     /* �������ԣ��ú����������ڻ����������ݽ��룬�����ø���ʵ�÷�������
	                                                                         ��ֵ������ʵ�ʽ�����ֽ���ֻ�е�ӵ�����Ե�дȨ��ʱ�����ܵ��øú��� */
	                                                                      
			
};

#define DRIVER_ATTR(_name, _mode, _show, _store)	\  /* �ú�����ڴ����ͳ�ʼ���������Զ���*/
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
 *��1������һ���豸�ĸ߲���ͼ����������˵ײ��ʵ��ϸ�ڡ��������û��ռ�ʹ���豸���ṩ�Ĺ��ܣ�
 *            ���������豸��������ӵ��Լ���������ι����ġ�
 *
 *��2���������е��඼��ʾ��/sys/classĿ¼�У����磺��������ӿڵ�������ʲô�����е�����ӿڶ�������
 *             /sys/class/net�£������豸������/sys/class/input���ҵ����������豸��������/sys/class/tty��
 *
 *��3�����Աͨ�����ϲ������ƣ�������Ҫ���������������ȷ֧��
 *
 *��4�����������£�����ϵͳ�����û��ռ䵼����Ϣ����õķ���������ϵͳ����һ����ʱ��������ȫӵ
 *             ������࣬��˸������õ����ĸ�ģ��ӵ����Щ����
 */
struct class { /* �豸��(�����device��class��һ�ָ��߲�εĳ������ڶ��豸���й����ϵĻ��֣�class������Ϊ
                           ����ͬ���͹����豸��һ������(/sys/classĿ¼��:ϵͳ���豸����(�������������豸��)))*/
						   	
	const char		*name; /* �������(���������豸�ļ����ļ��������豸�ļ�λ��/devĿ¼��) */
	struct module		*owner;/* ָ��ģ�� ��һ��ΪTHIS_MODULE*/

	struct class_attribute		*class_attrs;/* ��Ķ�������� */
	struct device_attribute		*dev_attrs;  /* ���������Ӧ���߼��豸���� */
	struct kobject			*dev_kobj; /* kobject�ں˶��� */

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);/* ���Ȳ���¼�����ʱ�����øú�������ӻ������� */

	void (*class_release)(struct class *class); /* �ͷ��౾�� */
	void (*dev_release)(struct device *dev);   /* ���豸������ɾ�� */

	int (*suspend)(struct device *dev, pm_message_t state);/*  ������*/
	int (*resume)(struct device *dev);/* �ָ����� */

	struct dev_pm_ops *pm;
	struct class_private *p;  /* ���˽�нṹָ�� */
};

struct class_dev_iter {
	struct klist_iter		ki;
	const struct device_type	*type;
};

extern struct kobject *sysfs_dev_block_kobj;
extern struct kobject *sysfs_dev_char_kobj;
extern int __must_check __class_register(struct class *class,
					 struct lock_class_key *key);
extern void class_unregister(struct class *class); /* ��ע�� */

/* This is a #define to keep the compiler from merging different
 * instances of the __key variable */
  /* ��ע�� */
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

struct class_attribute {    /* ������ */
	struct attribute attr;
	ssize_t (*show)(struct class *class, char *buf);
	ssize_t (*store)(struct class *class, const char *buf, size_t count);
};

#define CLASS_ATTR(_name, _mode, _show, _store)			\   /* �ú�����ڴ����ͳ�ʼ�������Խṹ */
struct class_attribute class_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check class_create_file(struct class *class,
					  const struct class_attribute *attr);
extern void class_remove_file(struct class *class,
			      const struct class_attribute *attr);

struct class_interface {  /* ��ӿ� */
	struct list_head	node;
	struct class		*class;

	int (*add_dev)		(struct device *, struct class_interface *);
	void (*remove_dev)	(struct device *, struct class_interface *);
};

extern int __must_check class_interface_register(struct class_interface *);  /* ע��ӿ��� */
extern void class_interface_unregister(struct class_interface *); /* ע���ӿ��� */

extern struct class * __must_check __class_create(struct module *owner,
						  const char *name,
						  struct lock_class_key *key);
extern void class_destroy(struct class *cls);

/* This is a #define to keep the compiler from merging different
 * instances of the __key variable */
 /* �ú����ڶ�̬�����豸���߼���(����һ�����������Ҫ��;�ǽ�ͬ���͵��豸�������)������ɲ���
     �ֶεĳ�ʼ����Ȼ������ӽ�linux�ں�ϵͳ���˺�����ִ��Ч��������/sys/class�´���һ���µ��ļ��У�
     ���ļ��е�����Ϊ�˺����ĵڶ��� �����������ļ���Ϊ��*/
   
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
struct device_type {  /* �豸���ͣ�����Ƕ��struct device */
	const char *name;  /* �豸�������� */
	struct attribute_group **groups;  /* ���� */
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);  /* �Ȳ���¼������� */
	void (*release)(struct device *dev);

	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);

	struct dev_pm_ops *pm;
};

/* interface for exporting device attributes */
struct device_attribute {/* �豸�����ԣ�sys�е��豸��ڿ��������� */
	struct attribute	attr;  /*  */
	ssize_t (*show)(struct device *dev, struct device_attribute *attr, char *buf);
	                                                                                                      /*��ʾ���ԣ����û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject 
	                                                                                                       ��ָ�����ȷ�����Խṹ������show�������ú�������ָ����ֵ
	                                                                                                       �������뻺������Ȼ���ʵ�ʳ�����Ϊ����ֵ���ء�attrָ����
	                                                                                                       �ԣ��������ж�����������ĸ����ԡ� */
	                                              
			
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,  const char *buf, size_t count);
										                                    /* �������ԣ��ú����������ڻ����������ݽ��룬�����ø���ʵ
	                                                                                                        �÷���������ֵ������ʵ�ʽ�����ֽ���ֻ�е�ӵ�����Ե�д
	                                                                                                        Ȩ��ʱ�����ܵ��øú���*/ 
	                                                                      
			
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \  /* �ú�����ڴ����ͳ�ʼ���豸���Խṹ */
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

extern int __must_check device_create_file(struct device *device,  /* ���������豸���κ����� */
					   struct device_attribute *entry);
extern void device_remove_file(struct device *dev, /* ɾ������ */
			       struct device_attribute *attr);
extern int __must_check device_create_bin_file(struct device *dev, /* ���������豸���κζ��������� */
					       struct bin_attribute *attr);
extern void device_remove_bin_file(struct device *dev, /* ɾ������������ */
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
 *�豸�ṹ��Ƕ�룺
 *���Ķ����������ʱ����Է��ֵ�����device�ṹ��ʾ���豸�Ǻ��ټ��ģ�����pci_dev��usb_device�ṹ���壬
 *���ᷢ������������device�ṹ��
 */
struct device { /* ��ʾ�豸 */
	struct device		*parent;/* �豸�ĸ��豸----ָ���Ǹ��豸�������豸���ڴ��������£�һ�����豸ͨ����ĳ
	                                                     �����߻��������������������ָ��ΪNULL���ʾ���豸�Ƕ����豸�������������
	                                                     �ټ��� */


	struct device_private	*p;  /* �豸˽�нṹ����ָ����豸��������ص����� */

	struct kobject kobj;/* ��ʾ���豸���������ӵ��ṹ��ϵ�е�kobject�ں˶��� */
	const char		*init_name; /* initial name of the device *//* �豸��������ƣ��ڽ��豸������뵽ϵͳ��ʱ���ں˻��init_name
	                                                    ���ó�kobj��Ա�����ƣ�������sysfs�б���Ϊһ��Ŀ¼*/
	struct device_type	*type;/*�豸����  */

	struct semaphore	sem;	/* semaphore to synchronize calls to its driver*//* �ź��� */
					 
					 

	struct bus_type	*bus;		/* type of bus device is on *//*��ʶ���豸�����ں������͵�������  */
	struct device_driver *driver;	/* which driver has allocated this device */   /* ���Ա�ʾ��ǰ�豸�Ƿ��Ѿ�������driver�����˰󶨣������
	                                                                                                                       ֵΪNULL��˵����ǰ�豸��û���ҵ�����driver*/
					   
	void		*driver_data;	/* data private to the driver */ /* ���豸��������ʹ�õ�˽������ */
	void		*platform_data;	/* Platform specific data, device  core doesn't touch it *//* platform�����豸˽������ */  
					   
	struct dev_pm_info	power;/*  */

#ifdef CONFIG_NUMA
	int		numa_node;	/* NUMA node this device is close to *//*  */
#endif
	u64		*dma_mask;	/* dma mask (if dma'able device) *//*  */
	u64		coherent_dma_mask;/* Like dma_mask, but for alloc_coherent mappings as not all hardware supports 64 bit addresses for consistent allocations such descriptors. */
	               /*  */
					     
					     
					     
					    

	struct device_dma_parameters *dma_parms;

	struct list_head	dma_pools;	/* dma pools (if dma'ble) */  /* �豸���� */

	struct dma_coherent_mem	*dma_mem; /* internal for coherent mem
					     override */
	/* arch specific additions */
	struct dev_archdata	archdata;

	dev_t			devt;	/* dev_t, creates the sysfs "dev" */ /* �豸�� */

	spinlock_t		devres_lock;/*������  */
	struct list_head	devres_head;

	struct klist_node	knode_class;
	struct class		*class;                 /*��  */
	struct attribute_group	**groups;	/* optional groups *//* ������ */

	void	(*release)(struct device *dev);/* ��ָ���豸�����һ�����ñ�ɾ��ʱ���ں˵��ø÷�������������Ƕ��kobject��
	                                                             release�����е��ã� ���������ע���device�ṹ������Ҫ�ṩ�ú����������ں�
	                                                             ����ӡ������Ϣ��*/
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
static inline void set_dev_node(struct device *dev, int node)   /*�����豸�ڵ�*/
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

 /*�ú��������ж��豸�Ƿ��Ѿ�ע��*/
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
