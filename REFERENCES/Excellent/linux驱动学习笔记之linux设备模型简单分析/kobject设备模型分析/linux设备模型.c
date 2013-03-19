
/*****************************************************linux设备模型***********************************************************/

/*
 *1：kobject是组成设备模型的基本结构，kobject结构所能处理的任务以及所支持的代码包括：
 *
 *（1）对象的引用计数：通常一个内核对象被创建时，不可能知道该对象存活的时间，跟踪此对象的生命周期的一个方法是使用引用计数，当内核中没有代码持有该对象时，
 *     该对象将结束自己的有效生命周期，并且可以删除。
 *
 *（2）sysfs表述：在sysfs中显示的每个目录都对应一个kobject，它体现了linux设备模型的层次结构。
 *
 *（3）数据结构关联：设备模型是一个复杂的数据结构，通过在其间的大量连接而构成一个多层次的体系结构，kobject实现了该结构并把它们聚合在一起。
 *
 *（4）热插拔事件处理：当系统中的硬件被热插拔时，在kset(其内嵌有kobject)内核对象的控制下，将产生事件以通知用户空间
 *
 *该结构一般没有单独定义，而是嵌入到其他设备描述结构体中，比如字符设备。
 *
 *
 */
--------------------------------------------struct kobject---------------------------------------------------- 
struct kobject {
	const char		*name;         //名字
	struct list_head	entry;     //列表头 
	struct kobject		*parent;   //指向设备分层时的上一层其的节点（比如一个kobject结构表示usb设备，那么该成员可能指向了表示usb集线器的对象，而usb设备是插在usb集线器上的），
	                             //该指针最重要的用途是在sysfs分层结构中定位对象，以此在内核中构造一个对象层次结构，并且可以将多个对象之间的关系表现出来，
	                             //这既是sysfs的真相:一个用户空间的文件系统，用来表示内核中kobject对象的层次                        
	                                                       
	                                                       
	                             
	struct kset		*kset;         //该结构比较关心对象的聚集于集合，kset总是在sysfs中出现，一旦设置了kset并把它添加到系统中，将在sysfs中创建一个目录
	struct kobj_type	*ktype;    //用于保存属性
	struct sysfs_dirent	*sd;
	struct kref		kref;          //对象引用计数
	unsigned int state_initialized:1;
	unsigned int state_in_sysfs:1;
	unsigned int state_add_uevent_sent:1;
	unsigned int state_remove_uevent_sent:1;
	unsigned int uevent_suppress:1;
};
-------------------------------------------------------------------------------------------------------------

-------------------------------------------struct kset-------------------------------------------------------
struct kset {
	struct list_head list;
	spinlock_t list_lock;
	struct kobject kobj;
	struct kset_uevent_ops *uevent_ops;//该结构体的操作函数用来对热插拔事件的实际控制
};
-------------------------------------------------------------------------------------------------------------

---------------------------------------------struct kobj_type------------------------------------------------
struct kobj_type {  //负责对kobject 类型进程跟踪
	void (*release)(struct kobject *kobj);  //释放函数
	struct sysfs_ops *sysfs_ops;     //该字段用于实现这些属性的操作
	struct attribute **default_attrs;//保存属性列表，用于创建该类型的每一个kobject（default_attrs链表中的最后一个元素必须用零填充）
};

struct sysfs_ops {//用于实现这些属性的操作
	ssize_t	(*show)(struct kobject *kobj, struct attribute *attr,char *buffer);//当用户空间读取一个属性时，内核会使用指向kobject 的指针和正确的属性结构来调用show函数，
	                                                                           //该函数将把指定的值编码后放入缓冲区，然后把实际长度作为返回值返回。
	                                                                           //attr指向属性，可用于判断所请求的是哪个属性。
	ssize_t	(*store)(struct kobject *kobj,struct attribute *attr,const char *buffer, size_t size);//该函数将保存在缓冲区的数据解码，并调用各种实用方法保存新值，返回实际解码的字节数
	                                                                                              //只有当拥有属性的写权限时，才能调用该函数
};

struct attribute {
	const char		*name;    //属性的名字，在kobject的sysfs目录中显示
	struct module		*owner; //指向该模块的指针，该模块负责实现这些属性 
	mode_t			mode;       //用于属性的保护位，对于只读属性，mode通常为S_IRUGO；对于可写，mode通常为S_IWUSR
};
-------------------------------------------------------------------------------------------------------------
/*
 *kobject的初始化：
 *（1）首先将整个kobject设置为0，这通常使用memset函数，如果忘了对该结构清零初始化，则以后使用kobject时，可能会发生一些奇怪的错误
 *（2）然后调用kobject_init和kobject_set_name函数，以便设置该结构内部的一些成员(name、ktype、kset、parent)
 */
     void kobject_init(struct kobject *kobj, struct kobj_type *ktype);//该函数设置kobject的引用计数为1，并初始化初始化该结构的成员
     int kobject_set_name(struct kobject *kobj, const char *fmt, ...);//设置kobject的名字，这是在sysfs入口中使用的名字，它可能会操作失败，所以应该检查返回值
     
/*
 *kobject的创建者需要直接或者间接的设置ktype、kset、parent成员。
 *
 *对引用的计数的操作：
 *kobject的一个重要函数是为包含它的结构设置引用计数，只要对象的引用计数存在，对象就必须继续存在，底层控制kobject引用计数的函数有:*/
 
  struct kobject *kobject_get(struct kobject *kobj);//对该函数的调用将增加kobject的引用计数，并返回指向kobject的指针，如果kobject已经处于被销毁的过程中，则调用失败并返回NULL
  void kobject_put(struct kobject *kobj);//当引用被释放时，调用该函数减少引用次数，并在可能的情况下释放对象，请记住kobject_init设置引用计数为1，所以当创建kobject时，如果不再需要初始的引用
                                         //就调用相应的kobject_put函数
                                         
/*                                         
 *有一个问题需要注意，就是当引用计数为0时，kobject将采取什么操作？
 *答案就是因为引用计数不为创建kobject的代码所直接控制，当kobject的最后一个引用计数不再存在时，必须异步的通知——使用kobject的release方法实现（struct kobj_type->release，该函数释放kobject资源），
 *
 *    另外，由上面的数据结构可知struct kobject成员kset也包含了struct kobject的指针，那么kset也会提供 struct kobj_type指针，如下的宏可用来查找指定kobject的 kobj_type指针：
 *    struct kobj_type *get_ktype(struct kobject *kobj);
 */

-----------------------------------------------------------------------------------------------------------
/*
 *kobject层次结构、kset和子系统：
 *（1）通常，内核用kobject结构将各个对象连接起来组成一个分层的结构体系，从而与模型化的子系统相匹配，有两种机制用于连接：parent指针和kset。在kobject的结构的parent成员中，保存了另外一个kobject
 *
 *     结构的指针，这个结构表述了分层结构中上一层的节点，而parent最重要的用途是在sysfs分层结构中定位对象
 *
 *（2）kset像是kobj_type结构的扩充，一个kset是嵌入相同类型结构的kobject集合，但kobj_type结构关系的是对象的类型，而kset结构关心的是对象的聚集和集合，因此kset的主要功能是包容（可认为它是
 *
 *     kobject的顶层容器类），实际上，kset包含了自己的kobject并且可以用多种处理kobject的方法处理kset。需要注意的是，kset总是在sysfs中出现，一旦设置了kset并把它添加到系统中，将在sysfs中创建
 *
 *     一个目录。kobject不必在sysfs中表示，但是kset中的每个kobject成员都将在sysfs中得到表述。     
 *
 *（3）创建一个对象时，通常把一个kobject添加到kset中去，这个过程有两个步骤：先把kobject的kset成员指向目的kset，然后将kobject传递给下面的函数: */
       int kobject_add(struct kobject *kobj, struct kobject *parent,const char *fmt, ...);//该函数的参数parent指向kobject的父节点，fmt指向kobject的成员name。调用该函数会增加引用计数
       
       void kobject_del(struct kobject *kobj);  //该函数把kobject从kset中删除以清除引用
       
       void kset_init(struct kset *k);      //初始化kset
       int kset_register(struct kset *k);   //初始化和增加kset(注册)
       void kset_unregister(struct kset *k);//注销
       struct kset *kset_get(struct kset *k);//增加kset引用计数
       void kset_put(struct kset *k);        //减少kset引用计数    

/*
 *子系统：
 *内核中的子系统包括block_subsys（对块设备来说是/sys/block）、devices_subsys（/sys/devices，设备分层结构的核心）以及内核所知晓的用于各种总线的特定子系统。驱动工程师无需创建一个新的
 *子系统而是需要添加一个新类。
 *每个kset都必须属于一个子系统，因此通过kset结构可以找到包含kset的每个子系统。
 */

/*
 *底层sysfs操作操作：
 *    kobject是隐藏在sysfs虚拟文件系统后的机制，对于sysfs中的目录，内核都会存在一个对应的kobject，每个kobject都输出一个或多个属性，它们在kobject的sysfs目录中表现为文件，
 *
 *其中内容是由内核生成。只要调用kobject_add就能在sysfs中显示kobject。那么是如何在sysfs创建入口的呢？
 *
 *（1）kobject在sysfs中的入口始终是一个目录，因此对kobject_add的调用将在sysfs中创建一个目录，通常这个目录包含一个或多个属性（属性就是某种文件）。
 *
 *（2）分配给kobject的名字（kobject_set_name）是sysfs的目录名，这样处于sysfs分层结构相同部分中的kobject必须有唯一的名字。
 *
 *（3）sysfs入口在目录中的位置对应于kobject的parent指针。
 *
 *（4）总之，kset内嵌了kobject来表示自身的节点，创建kset就要完成其内嵌kobject，注册了kset时会产生一个事件（注册了的kobject不会产生一个事件），事件而最终会调用kset的成员函数指针
 *     uevent_ops指向的结构中的函数，这个事件是通过用户空间的hotplug程序处理(实际是调用/sbin/mdev)
 */

/*
 *当创建kobject的时候，都会给每个kobject一系列默认的属性文件，这些属性保存在struct kobj_type中。
 *
 *如果希望在kobject的sysfs目录中添加新的属性文件，只需要填写一个attribute结构并把它传递给下面函数：*/
 
 int sysfs_create_file(struct kobject *kobj,const struct attribute *attr);//如果该函数调用成功将使用attribute结构中的名字创建属性文件并返回0，否则返回一个负的错误码
 
 void sysfs_remove_file(struct kobject *kobj,const struct attribute *attr);//删除属性文件
 

-----------------------------------------------------------------------------------------------------------
/*
 *    sysfs的约定要求所有属性都只能包含一个可读的文本格式，也就是说，对创建一个可以处理大量二进制属性的要求是很少发生的，但是当我们在用户空间和设备之间传递不可改变的数据时，有可能
 *产生这种需求（比如向设备上载固件），如果我们在系统中遇到这样的设备，就可以运行用户空间程序（通过热插拔机制）使用二进制的sysfs属性将固件代码传递给内核。
 */
 
struct bin_attribute { //该结构用于描述二进制属性
	struct attribute	attr;//attr是一个attribute结构，它给出了名字、所有者、二进制属性的权限
	size_t			size;      //二进制属性的最大长度（如果没有最大值，则设置为0）
	void			*private;    //指向私有数据
	ssize_t (*read)(struct kobject *, struct bin_attribute *, //读
			char *, loff_t, size_t);
	ssize_t (*write)(struct kobject *, struct bin_attribute *,//写
			 char *, loff_t, size_t);
	int (*mmap)(struct kobject *, struct bin_attribute *attr, //映射
		    struct vm_area_struct *vma);
};

int __must_check sysfs_create_bin_file(struct kobject *kobj, struct bin_attribute *attr);//创建二进制属性
void sysfs_remove_bin_file(struct kobject *kobj, struct bin_attribute *attr);//删除二进制属性

/*
 *符号链接：
 *sysfs文件系统具有常用的树形结构以反映kobject之间的组织层次关系，通常内核中各对象之间的关系比这复杂（比如一个sysfs的子树（/sys/devices）表示所有系统知晓的设备，
 *
 *而在其他的子树（/sys/bus）表示了设备的驱动程序）。但是这些树并不能表示驱动程序及其管理设备之间的关系，为了表示这种关系还需要其他的指针，在sysfs中用过符号链接
 *实现了这个目的。
 */
int __must_check sysfs_create_link(struct kobject *kobj, struct kobject *target,const char *name);//在sysfs中创建符号链接（该函数创建了一个链接（称为name）指向target的sysfs入口，并作为kobj的一个属性）
void sysfs_remove_link(struct kobject *kobj, const char *name);//删除符号链接

-----------------------------------------------------------------------------------------------------------
/*
 *热插拔事件：
 *（1）一个热插拔事件是从内核空间发送到用户空间的通知，它表明系统配置出现了变化。无论kobject被创建还是被删除，都会产生这种事件，比如当数码相机通过USB线插入系统时或者用户切换控制台时或者
 *
 *     当给磁盘分区时都会产生这类事件。热插拔事件会导致对/sbin/hotplug(在rcS脚本里我们已经将它定位到了/sbin/mdev)程序的调用，该程序通过加装驱动程序，创建设备节点，挂载分区或者其他正确的动作来响应事件
 *
 *（2）下面讨论一个重要的kobject函数用来产生这些事件。当我们把kobject传递给kobject_add或kobject_del时，才会真正产生这些事件。在事件被传递到用户空间之前，处理kobject（或者准确些，是kobject）
 *
 *     的代码能够为用户空间添加信息或者完全禁止事件的产生。
 *
 *（3）热插拔事件的创建通常被总线驱动程序级别的逻辑所控制。
 *
 *（4）对热插拔事件的实际控制是由保存在struct kset结构体的成员struct kset_uevent_ops 结构中函数来完成
 */

struct kset_uevent_ops {
	int (*filter)(struct kset *kset, struct kobject *kobj);//无论什么时候，当内核要为指定的 kobject产生事件时，都要调用该函数；如果该函数返回0则将不产生事件。
	                                                       //因此该函数给kset代码一个机会用于决定是否向用户空间传递特定事件。
	                                                       
	const char *(*name)(struct kset *kset, struct kobject *kobj);//调用用户空间的热插拔程序时，相关子系统的名字将作为唯一的参数传递给它，该函数负责提供这个名字，它将返回一个合适传递给用户空间的字符串
	int (*uevent)(struct kset *kset, struct kobject *kobj,struct kobj_uevent_env *env);//任何热插拔脚本所需要知道的信息将通过环境变量传递，该函数会在调用脚本前，提供添加环境变量的机会
	                                                                                   //如果要在envp中添加任何变量，请确保在最后一个新变量后加入NULL，这样内核就知道哪里是结束点了
	                                                                                   //该函数通常返回0，返回非0将终止热插拔事件的产生
	                                                                                   //struct kobj_uevent_env {
	                                                                                   //                         char *envp[UEVENT_NUM_ENVP];   //其保存其他环境变量
                                                                                     //                         int envp_idx;                  //该变量说明有多少个变量入口
	                                                                                   //                         char buf[UEVENT_BUFFER_SIZE];  //存放编码后的变量
	                                                                                   //                         int buflen;                    //存放编码后的变量的长度
                                                                                     //                       };
};


-----------------------------------------------------------------------------------------------------------
/*
 *总线、设备和驱动程序：
 *    在设备模型中，所有的设备都通过总线相连，甚至是内部虚拟总线platform。总线可以相互插入（比如usb控制器通常是一个PCI设备），
 *设备模型展示了总线和它们所控制的设备之间的连接
 *
 *（1）总线：
 *在linux设备模型中，用bus_type结构表示总线,如下所示：
 */
---------------------------------------------总线---------------------------------------------------------------
----------------------------------------struct bus_type--------------------------------------------- 
struct bus_type {//该结构表示总线
	const char		*name;                //总线的名字
	struct bus_attribute	*bus_attrs;   //总线的属性
	struct device_attribute	*dev_attrs; //设备的属性
	struct driver_attribute	*drv_attrs; //驱动的属性

	int (*match)(struct device *dev, struct device_driver *drv);     //匹配函数，匹配则返回非0值（在调用实际的硬件时，mach函数通常对设备提供的硬件ID和驱动所支持的ID做某种比较；
	                                                                 //但比如platform中使用名字来进行匹配，如果名字相同则说明支持该设备）
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);  //热插拔事件处理函数，上面已经介绍了
	int (*probe)(struct device *dev);                                //探测函数（其指向下层提供的函数，当mach函数匹配了总线和设备则用它来探测设备）
	int (*remove)(struct device *dev);                               //移除设备
	void (*shutdown)(struct device *dev);                            //关闭设备

	int (*suspend)(struct device *dev, pm_message_t state);          //挂起函数
	int (*suspend_late)(struct device *dev, pm_message_t state);
	int (*resume_early)(struct device *dev);
	int (*resume)(struct device *dev);                               //恢复函数

	struct dev_pm_ops *pm;

	struct bus_type_private *p;    //把部分私有字段封装到这个结构里
};
---------------------------------------------------------------------------------------------------

------------------------------------struct bus_attribute-------------------------------------------
struct bus_attribute {//总线的属性
	struct attribute	attr;  //attr是一个attribute结构，它给出了名字、所有者、二进制属性的权限
	ssize_t (*show)(struct bus_type *bus, char *buf);//显示属性；当用户空间读取一个属性时，内核会使用指向kobject 的指针和正确的属性结构来调用show函数，
	                                                 //该函数将把指定的值编码后放入缓冲区，然后把实际长度作为返回值返回。
	                                                 //attr指向属性，可用于判断所请求的是哪个属性。
	ssize_t (*store)(struct bus_type *bus, const char *buf, size_t count);//设置属性；该函数将保存在缓冲区的数据解码，并调用各种实用方法保存新值，返回实际解码的字节数
	                                                                      //只有当拥有属性的写权限时，才能调用该函数
};

//属性相关操作函数：
#define BUS_ATTR(_name, _mode, _show, _store) \   //该宏可用于创建和初始化总线属性结构
									struct bus_attribute bus_attr_##_name = __ATTR(_name, _mode, _show, _store);
									
int __must_check bus_create_file(struct bus_type *,struct bus_attribute *);//创建属于总线的任何属性

void bus_remove_file(struct bus_type *, struct bus_attribute *);//删除属性	
--------------------------------------------------------------------------------------------------

//总线的注册：
int bus_register(struct bus_type *bus);//注册（设置好struct bus_type后，使用该函数注册（注意只有非常少的 bus_type成员需要初始化，它们中的大多数都由设备
                                       //模型核心所控制，但是必须为总线指定名字、match、uevent等一些必要的成员））
                                       //这个调用可能会失败，因此必须检查它的返回值，如果成功，新的总线子系统将添加到系统中，可以在/sys/bus目录看到它
void bus_unregister(struct bus_type *bus);//注销

/*
 *对设备和驱动程序的迭代：
 *在编写总线层代码会发现不得不对注册到总线的所有设备和驱动程序执行某些操作，为了操作注册到总线的每个设备，可调用下面函数
 */
int bus_for_each_dev(struct bus_type *bus, struct device *start,void *data, int (*fn)(struct device *, void *));//该函数迭代了在总线上的每个设备，将相关的device结构传递给fn，同时传递data值，如果start是NULL。
                                                                                                                //将从总线的第一个设备开始迭代，否则将从start后的第一个设备开始迭代。
                                                                                                                //如果fn返回一个非0值，将停止迭代，而这个值也会从该函数返回

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,void *data, int (*fn)(struct device_driver *, void *));//该函数用于驱动程序的迭代，该函数的工作方式与上面的函数相似，
                                                                                                                              //只是它的工作对象是驱动程序
                                                                                                                              
                                                                                                                              
------------------------------------------------设备------------------------------------------------------------
/*
 *（2）设备：
 *linux系统中每个设备都用device结构来表示，如下所示：
 */
-----------------------------------------struct device---------------------------------------------
struct device {  //表示各种设备
	struct device		*parent;   //设备的父设备----指的是该设备所属的设备；在大多数情况下，一个父设备通常是某种总线或者主机控制器；如果该指针为NULL则表示该设备是顶层设备，但这种情况很少见。

	struct device_private	*p;

	struct kobject kobj;      //表示该设备并把它连接到结构体系中kobject
	const char		*init_name; //设备的初始名称
	struct device_type	*type;//设备类型

	struct semaphore	sem;	/* semaphore to synchronize calls to  //信号量
					 * its driver.
					 */

	struct bus_type	*bus;		/* type of bus device is on */           //标识该设备连接在何种类型的总线上
	struct device_driver *driver;	/* which driver has allocated this //管理该设备的驱动程序
					   device */
	void		*driver_data;	/* data private to the driver */           //由设备驱动程序使用的私有数据
	void		*platform_data;	/* Platform specific data, device        //platform总线设备私有数据
					   core doesn't touch it */
	struct dev_pm_info	power;

#ifdef CONFIG_NUMA
	int		numa_node;	/* NUMA node this device is close to */
#endif
	u64		*dma_mask;	/* dma mask (if dma'able device) */
	u64		coherent_dma_mask;/* Like dma_mask, but for
					     alloc_coherent mappings as
					     not all hardware supports
					     64 bit addresses for consistent
					     allocations such descriptors. */

	struct device_dma_parameters *dma_parms;

	struct list_head	dma_pools;	/* dma pools (if dma'ble) */

	struct dma_coherent_mem	*dma_mem; /* internal for coherent mem
					     override */
	/* arch specific additions */
	struct dev_archdata	archdata;

	dev_t			devt;	/* dev_t, creates the sysfs "dev" */        //设备号
 
	spinlock_t		devres_lock;   //自旋锁
	struct list_head	devres_head;

	struct klist_node	knode_class;
	struct class		*class;                         //类
	struct attribute_group	**groups;	/* optional groups */    //组属性

	void	(*release)(struct device *dev);         //当指向设备的最后一次引用被删除时，内核调用该方法，它将从内嵌的kobject的release方法中调用，
	                                              //所有向核心注册的device结构都必须要提供该函数，否则内核将打印错误信息。
};
----------------------------------------------------------------------------------------------------

//设备注册：
int device_register(struct device *dev);//注册
void device_unregister(struct device *dev);//注销

-----------------------------------struct device_attribute------------------------------------------
struct device_attribute {//设备的属性，sys中的设备入口可以有属性
	struct attribute	attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count);
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \ //该宏可用于创建和初始化设备属性结构
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

int __must_check device_create_file(struct device *device, struct device_attribute *entry);//创建属于设备的任何属性

void device_remove_file(struct device *dev,struct device_attribute *attr);//删除属性

int __must_check device_create_bin_file(struct device *dev,struct bin_attribute *attr);//创建属于设备的任何二进制属性

void device_remove_bin_file(struct device *dev,struct bin_attribute *attr);//删除二进制属性
---------------------------------------------------------------------------------------------------

/*
 *设备结构的嵌入：
 *在阅读驱动代码的时候可以发现单纯用device结构表示的设备是很少见的，比如pci_dev或usb_device结构定义，均会发现其中隐藏着device结构。
 */

------------------------------------------------设备驱动------------------------------------------------------
--------------------------------------struct device_driver------------------------------------------
struct device_driver {//表示设备驱动
	const char		*name;     //驱动程序的名称
	struct bus_type		*bus;  //该驱动程序所操作的总线类型

	struct module		*owner;  //指向模块
	const char 		*mod_name;	/* used for built-in modules *///模块名称

	int (*probe) (struct device *dev);                        //探测函数
	int (*remove) (struct device *dev);                       //移除函数
	void (*shutdown) (struct device *dev);                    //关闭函数
	int (*suspend) (struct device *dev, pm_message_t state);  //挂起函数
	int (*resume) (struct device *dev);                       //恢复函数
	struct attribute_group **groups;   //组属性

	struct dev_pm_ops *pm;

	struct driver_private *p;
};
-------------------------------------------------------------------------------------------------------------
//注册设备驱动：
int driver_register(struct device_driver *drv);//注册
void driver_unregister(struct device_driver *drv);//注销

----------------------------------struct driver_attribute----------------------------------------
struct driver_attribute {//驱动的属性
	struct attribute attr;
	ssize_t (*show)(struct device_driver *driver, char *buf);
	ssize_t (*store)(struct device_driver *driver, const char *buf,
			 size_t count);
};

#define DRIVER_ATTR(_name, _mode, _show, _store)	\  //该宏可用于创建和初始化驱动属性结构
struct driver_attribute driver_attr_##_name =	__ATTR(_name, _mode, _show, _store)

int __must_check driver_create_file(struct device_driver *driver,struct driver_attribute *attr);//创建属于驱动的任何属性

void driver_remove_file(struct device_driver *driver,struct driver_attribute *attr);	//删除属性							
------------------------------------------------------------------------------------------------
/*
 *驱动程序结构的嵌入：
 *对于大多数驱动程序核心结构来说，device_driver结构通常被包含在高层和总线相关的结构中。
 */
-------------------------------------------------------------------------------------------------------------


------------------------------------------------------类-----------------------------------------------------
/*
 *（1）类是一个设备的高层视图，它抽象出了底层的实现细节。类允许用户空间所使用设备所提供的功能，而不关心设备是如何连接的以及它们是如何工作的。
 *
 *（2）几乎所有的类都显示在/sys/class目录中（例如：不管网络接口的类型是什么，所有的网络接口都集中在/sys/class/net下；输入设备可以在/sys/class/input下找到，而串行设备都集中在/sys/class/tty）
 *
 *（3）类成员通常被上层代码控制，而不需要来自驱动程序的明确支持
 *
 *（4）在许多情况下，类子系统是向用户空间导出信息的最好的方法，当子系统创建一个类时，它将完全拥有这个类，因此根本不用担心哪个模块拥有哪些属性
 */
#include <linux/device.h>
struct class {//类
	const char		*name; //类的名字
	struct module		*owner; //指向模块

	struct class_attribute		*class_attrs;  //类的属性
	struct device_attribute		*dev_attrs;    //设备属性
	struct kobject			*dev_kobj;           

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);  //当热插拔事件产生时，可用该函数来添加环境变量

	void (*class_release)(struct class *class);  //释放类本身
	void (*dev_release)(struct device *dev);     //把设备从类中删除

	int (*suspend)(struct device *dev, pm_message_t state);  //挂起函数
	int (*resume)(struct device *dev);  //恢复函数

	struct dev_pm_ops *pm;
	struct class_private *p;
};

struct class_attribute {
	struct attribute attr;
	ssize_t (*show)(struct class *class, char *buf);
	ssize_t (*store)(struct class *class, const char *buf, size_t count);
};

#define CLASS_ATTR(_name, _mode, _show, _store)			\  //该宏可用于创建和初始化类属性结构
struct class_attribute class_attr_##_name = __ATTR(_name, _mode, _show, _store)

int __must_check class_create_file(struct class *class,   const struct class_attribute *attr);//创建属于类的任何属性
void class_remove_file(struct class *class, const struct class_attribute *attr); //删除属性	

//类的注册：
#define class_register(class);//注册	
void class_unregister(struct class *cls);//注销
-------------------------------------------------------------------------------------------------------------
/*
 *下面分析并总结设备模型的层次：
 *（1）定义一个struct bus_type类型的变量xxx_bus_type并初始化，然后在入口函数xxx_driver_init中调用bus_register(&xxx_bus_type)来向驱动核心注册总线（例如注册总线为PCI，那么将在/sys/bus/pci
 *     中创建一个sysfs目录，其中包含两个目录：devices和drivers）
 *
 *（2）定义一个xxx_driver结构（如下所示），该结构中包含device_driver结构，至少需要在xxx_register_driver函数（我们设计的驱动程序的注册）中对其进行初始化,然在调用driver_register来注册驱动
 *     struct xxx_driver {    //xxx驱动 
 *	                struct list_head node;
 *	                char *name;       // 驱动名称 
 *	                const struct xxx_device_id *id_table;	 //设备ID，从设备读取的ID与定义的ID匹配的话会调用probe函数 
 *	                int  (*probe)  (struct xxx_dev *dev, const struct xxx_device_id *id);	
 *	                void (*remove) (struct xxx_dev *dev);	/*/
 *	                int  (*suspend) (struct xxx_dev *dev, pm_message_t state);	
 * 	                int  (*suspend_late) (struct xxx_dev *dev, pm_message_t state);
 *	                int  (*resume_early) (struct xxx_dev *dev);
 *	                int  (*resume) (struct xxx_dev *dev);	               
 *	                void (*shutdown) (struct xxx_dev *dev);
 *	                struct device_driver	driver;
 *                  ........
 *                  ........
 *                  };
/*
 *（3）定义一个xxx_dev结构（用于描述xxx设备，如下所示），其包含了struct	device结构，在相关函数中对其初始化后调用device_register对其进行注册设备
 *     struct xxx_dev {
 *                     .......
 *                     .......
 *                     struct xxx_driver *driver;
 *                     struct	device	dev;
 *                     ......
 *                     ......
 *                    };
 *
 *（4）最后，在能与xxx总线交换机的特定体系架构代码的帮助下，xxx核心开始探测xxx地址空间(最终会调用bus的probe字段或device_driver的probe字段实现的方法)，查找所有的xxx设备，当一个xxx设备被找到时，xxx核心在内存中
 *
 *    在内存创建一个xxx_dev类型的结构变量。 这个结构中与总线相关的的成员由xxx的核心初始化，并且device结构变量的parent变量被设置为该xxx设备所在的总线设备。
 *
 *（5）在device_register函数中，驱动程序核心对device中的许多成员进行初始化，向kobject核心注册设备kset(这将产生一个热插拔事件)，然后将该设备添加到设备链表中，该设备链表为包含该设
 *
 *     备的父节点所拥有。完成这些工作后，所有的设备都可以通过正确的顺序访问，并且知道每个设备都挂在层次结构的哪一点上。
 *
 *（6）接着设备将被添加到与总线相关的所有设备链表中即xxx_bus_type链表，这个链表包含了所有向总线注册的设备，遍历这个链表并且为每个驱动程序调用该总线的mach函数来绑定一个设备。对于
 *
 *     xxx_bus_type总线来说，xxx核心在把设备提交给驱动程序核心前，将mach函数指向xxx_bus_mach函数。
 *
 *（7）xxx_bus_mach函数将把驱动程序核心传递给它的device结构转换为xxx_dev结构。它还把device_driver结构转换为xxx_driver结构，并且查看设备和驱动程序中的xxx设备相关信息，以确定驱动程序
 *
 *     是否支持这类设备。如果这样的匹配工作没能正确执行，该函数会向驱动程序核心返回0，接着驱动程序核心考虑在其链表中的下一个驱动程序。
 *
 *（8）如果匹配工作圆满完成，函数向驱动程序核心返回1.这将导致驱动核心将device结构中的driver指针指向这个驱动程序，然后调用device_driver结构中指定的probe函数。
 *
 *（9）在xxx驱动程序向驱动程序核心注册前，probe变量被设置为指向xxx_device_probe函数。该函数将device结构转换为xxx_dev结构，并且把在device中设置的driver结构转换为xxx_driver结构。它也
 *
 *     可能检测这个驱动程序的状态，以确保其能支持这个设备，增加设备引用次数，然后用绑定的xxx_dev结构指针为参数，调用xxx驱动程序的probe函数。
 *
 *（10）如果xxx驱动程序的probe函数出于某种原因，判定不能处理这个设备，其将返回负的错误值给驱动程序核心，这将导致驱动程序核心继续在驱动程序列表中搜索，以匹配这个设备。如果probe函数探测
 *
 *      到了设备，为了能正常操作设备，它将做所有初始化工作，然后向驱动程序核心返回0.这会使驱动程序核心将该设备添加到与此驱动程序绑定的设备链表中，并且在sysfs中的drivers目录到当前控制
 *
 *      设备之间建立符号链接，这个符号链接使得用户知道哪个驱动程序被绑定到了哪个设备上。
 *
 *（11）当删除一个xxx设备时（xxx_unregister_driver），调用相关函数，该函数做些xxx相关的清理工作，然后使用指向xxx_dev中device结构指针，调用device_unregister函数。在device_unregister函数中
 *
 *      ，驱动核心只是删除了从绑定设备的驱动程序（如果有绑定的话）到sysfs文件符合链接，从内部设备链表中删除了该设备，并且以device结构中的kobject结构指针为参数，调用kobject_del函数，
 *
 *      该函数引起了用户空间hotplug调用。kobject从系统中删除，然后删除全部与kobject相关的sysfs属性文件和目录，而这些目录和文件都是kobject以前创建的。kobject_del函数还删除了设备
 *
 *      的kobject引用，如果该引用是最后一个，就要调用该xxx设备的release函数，该函数释放了xxx_dev结构所占用的空间，这样xxx设备被完全从系统删除。
 */
****************************************************************************************************************************************************