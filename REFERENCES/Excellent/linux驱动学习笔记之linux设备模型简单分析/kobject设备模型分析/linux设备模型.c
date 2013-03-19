
/*****************************************************linux�豸ģ��***********************************************************/

/*
 *1��kobject������豸ģ�͵Ļ����ṹ��kobject�ṹ���ܴ���������Լ���֧�ֵĴ��������
 *
 *��1����������ü�����ͨ��һ���ں˶��󱻴���ʱ��������֪���ö������ʱ�䣬���ٴ˶�����������ڵ�һ��������ʹ�����ü��������ں���û�д�����иö���ʱ��
 *     �ö��󽫽����Լ�����Ч�������ڣ����ҿ���ɾ����
 *
 *��2��sysfs��������sysfs����ʾ��ÿ��Ŀ¼����Ӧһ��kobject����������linux�豸ģ�͵Ĳ�νṹ��
 *
 *��3�����ݽṹ�������豸ģ����һ�����ӵ����ݽṹ��ͨ�������Ĵ������Ӷ�����һ�����ε���ϵ�ṹ��kobjectʵ���˸ýṹ�������Ǿۺ���һ��
 *
 *��4���Ȳ���¼�������ϵͳ�е�Ӳ�����Ȳ��ʱ����kset(����Ƕ��kobject)�ں˶���Ŀ����£��������¼���֪ͨ�û��ռ�
 *
 *�ýṹһ��û�е������壬����Ƕ�뵽�����豸�����ṹ���У������ַ��豸��
 *
 *
 */
--------------------------------------------struct kobject---------------------------------------------------- 
struct kobject {
	const char		*name;         //����
	struct list_head	entry;     //�б�ͷ 
	struct kobject		*parent;   //ָ���豸�ֲ�ʱ����һ����Ľڵ㣨����һ��kobject�ṹ��ʾusb�豸����ô�ó�Ա����ָ���˱�ʾusb�������Ķ��󣬶�usb�豸�ǲ���usb�������ϵģ���
	                             //��ָ������Ҫ����;����sysfs�ֲ�ṹ�ж�λ�����Դ����ں��й���һ�������νṹ�����ҿ��Խ��������֮��Ĺ�ϵ���ֳ�����
	                             //�����sysfs������:һ���û��ռ���ļ�ϵͳ��������ʾ�ں���kobject����Ĳ��                        
	                                                       
	                                                       
	                             
	struct kset		*kset;         //�ýṹ�ȽϹ��Ķ���ľۼ��ڼ��ϣ�kset������sysfs�г��֣�һ��������kset��������ӵ�ϵͳ�У�����sysfs�д���һ��Ŀ¼
	struct kobj_type	*ktype;    //���ڱ�������
	struct sysfs_dirent	*sd;
	struct kref		kref;          //�������ü���
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
	struct kset_uevent_ops *uevent_ops;//�ýṹ��Ĳ��������������Ȳ���¼���ʵ�ʿ���
};
-------------------------------------------------------------------------------------------------------------

---------------------------------------------struct kobj_type------------------------------------------------
struct kobj_type {  //�����kobject ���ͽ��̸���
	void (*release)(struct kobject *kobj);  //�ͷź���
	struct sysfs_ops *sysfs_ops;     //���ֶ�����ʵ����Щ���ԵĲ���
	struct attribute **default_attrs;//���������б����ڴ��������͵�ÿһ��kobject��default_attrs�����е����һ��Ԫ�ر���������䣩
};

struct sysfs_ops {//����ʵ����Щ���ԵĲ���
	ssize_t	(*show)(struct kobject *kobj, struct attribute *attr,char *buffer);//���û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject ��ָ�����ȷ�����Խṹ������show������
	                                                                           //�ú�������ָ����ֵ�������뻺������Ȼ���ʵ�ʳ�����Ϊ����ֵ���ء�
	                                                                           //attrָ�����ԣ��������ж�����������ĸ����ԡ�
	ssize_t	(*store)(struct kobject *kobj,struct attribute *attr,const char *buffer, size_t size);//�ú����������ڻ����������ݽ��룬�����ø���ʵ�÷���������ֵ������ʵ�ʽ�����ֽ���
	                                                                                              //ֻ�е�ӵ�����Ե�дȨ��ʱ�����ܵ��øú���
};

struct attribute {
	const char		*name;    //���Ե����֣���kobject��sysfsĿ¼����ʾ
	struct module		*owner; //ָ���ģ���ָ�룬��ģ�鸺��ʵ����Щ���� 
	mode_t			mode;       //�������Եı���λ������ֻ�����ԣ�modeͨ��ΪS_IRUGO�����ڿ�д��modeͨ��ΪS_IWUSR
};
-------------------------------------------------------------------------------------------------------------
/*
 *kobject�ĳ�ʼ����
 *��1�����Ƚ�����kobject����Ϊ0����ͨ��ʹ��memset������������˶Ըýṹ�����ʼ�������Ժ�ʹ��kobjectʱ�����ܻᷢ��һЩ��ֵĴ���
 *��2��Ȼ�����kobject_init��kobject_set_name�������Ա����øýṹ�ڲ���һЩ��Ա(name��ktype��kset��parent)
 */
     void kobject_init(struct kobject *kobj, struct kobj_type *ktype);//�ú�������kobject�����ü���Ϊ1������ʼ����ʼ���ýṹ�ĳ�Ա
     int kobject_set_name(struct kobject *kobj, const char *fmt, ...);//����kobject�����֣�������sysfs�����ʹ�õ����֣������ܻ����ʧ�ܣ�����Ӧ�ü�鷵��ֵ
     
/*
 *kobject�Ĵ�������Ҫֱ�ӻ��߼�ӵ�����ktype��kset��parent��Ա��
 *
 *�����õļ����Ĳ�����
 *kobject��һ����Ҫ������Ϊ�������Ľṹ�������ü�����ֻҪ��������ü������ڣ�����ͱ���������ڣ��ײ����kobject���ü����ĺ�����:*/
 
  struct kobject *kobject_get(struct kobject *kobj);//�Ըú����ĵ��ý�����kobject�����ü�����������ָ��kobject��ָ�룬���kobject�Ѿ����ڱ����ٵĹ����У������ʧ�ܲ�����NULL
  void kobject_put(struct kobject *kobj);//�����ñ��ͷ�ʱ�����øú����������ô��������ڿ��ܵ�������ͷŶ������סkobject_init�������ü���Ϊ1�����Ե�����kobjectʱ�����������Ҫ��ʼ������
                                         //�͵�����Ӧ��kobject_put����
                                         
/*                                         
 *��һ��������Ҫע�⣬���ǵ����ü���Ϊ0ʱ��kobject����ȡʲô������
 *�𰸾�����Ϊ���ü�����Ϊ����kobject�Ĵ�����ֱ�ӿ��ƣ���kobject�����һ�����ü������ٴ���ʱ�������첽��֪ͨ����ʹ��kobject��release����ʵ�֣�struct kobj_type->release���ú����ͷ�kobject��Դ����
 *
 *    ���⣬����������ݽṹ��֪struct kobject��ԱksetҲ������struct kobject��ָ�룬��ôksetҲ���ṩ struct kobj_typeָ�룬���µĺ����������ָ��kobject�� kobj_typeָ�룺
 *    struct kobj_type *get_ktype(struct kobject *kobj);
 */

-----------------------------------------------------------------------------------------------------------
/*
 *kobject��νṹ��kset����ϵͳ��
 *��1��ͨ�����ں���kobject�ṹ���������������������һ���ֲ�Ľṹ��ϵ���Ӷ���ģ�ͻ�����ϵͳ��ƥ�䣬�����ֻ����������ӣ�parentָ���kset����kobject�Ľṹ��parent��Ա�У�����������һ��kobject
 *
 *     �ṹ��ָ�룬����ṹ�����˷ֲ�ṹ����һ��Ľڵ㣬��parent����Ҫ����;����sysfs�ֲ�ṹ�ж�λ����
 *
 *��2��kset����kobj_type�ṹ�����䣬һ��kset��Ƕ����ͬ���ͽṹ��kobject���ϣ���kobj_type�ṹ��ϵ���Ƕ�������ͣ���kset�ṹ���ĵ��Ƕ���ľۼ��ͼ��ϣ����kset����Ҫ�����ǰ��ݣ�����Ϊ����
 *
 *     kobject�Ķ��������ࣩ��ʵ���ϣ�kset�������Լ���kobject���ҿ����ö��ִ���kobject�ķ�������kset����Ҫע����ǣ�kset������sysfs�г��֣�һ��������kset��������ӵ�ϵͳ�У�����sysfs�д���
 *
 *     һ��Ŀ¼��kobject������sysfs�б�ʾ������kset�е�ÿ��kobject��Ա������sysfs�еõ�������     
 *
 *��3������һ������ʱ��ͨ����һ��kobject��ӵ�kset��ȥ������������������裺�Ȱ�kobject��kset��Աָ��Ŀ��kset��Ȼ��kobject���ݸ�����ĺ���: */
       int kobject_add(struct kobject *kobj, struct kobject *parent,const char *fmt, ...);//�ú����Ĳ���parentָ��kobject�ĸ��ڵ㣬fmtָ��kobject�ĳ�Աname�����øú������������ü���
       
       void kobject_del(struct kobject *kobj);  //�ú�����kobject��kset��ɾ�����������
       
       void kset_init(struct kset *k);      //��ʼ��kset
       int kset_register(struct kset *k);   //��ʼ��������kset(ע��)
       void kset_unregister(struct kset *k);//ע��
       struct kset *kset_get(struct kset *k);//����kset���ü���
       void kset_put(struct kset *k);        //����kset���ü���    

/*
 *��ϵͳ��
 *�ں��е���ϵͳ����block_subsys���Կ��豸��˵��/sys/block����devices_subsys��/sys/devices���豸�ֲ�ṹ�ĺ��ģ��Լ��ں���֪�������ڸ������ߵ��ض���ϵͳ����������ʦ���贴��һ���µ�
 *��ϵͳ������Ҫ���һ�����ࡣ
 *ÿ��kset����������һ����ϵͳ�����ͨ��kset�ṹ�����ҵ�����kset��ÿ����ϵͳ��
 */

/*
 *�ײ�sysfs����������
 *    kobject��������sysfs�����ļ�ϵͳ��Ļ��ƣ�����sysfs�е�Ŀ¼���ں˶������һ����Ӧ��kobject��ÿ��kobject�����һ���������ԣ�������kobject��sysfsĿ¼�б���Ϊ�ļ���
 *
 *�������������ں����ɡ�ֻҪ����kobject_add������sysfs����ʾkobject����ô�������sysfs������ڵ��أ�
 *
 *��1��kobject��sysfs�е����ʼ����һ��Ŀ¼����˶�kobject_add�ĵ��ý���sysfs�д���һ��Ŀ¼��ͨ�����Ŀ¼����һ���������ԣ����Ծ���ĳ���ļ�����
 *
 *��2�������kobject�����֣�kobject_set_name����sysfs��Ŀ¼������������sysfs�ֲ�ṹ��ͬ�����е�kobject������Ψһ�����֡�
 *
 *��3��sysfs�����Ŀ¼�е�λ�ö�Ӧ��kobject��parentָ�롣
 *
 *��4����֮��kset��Ƕ��kobject����ʾ����Ľڵ㣬����kset��Ҫ�������Ƕkobject��ע����ksetʱ�����һ���¼���ע���˵�kobject�������һ���¼������¼������ջ����kset�ĳ�Ա����ָ��
 *     uevent_opsָ��Ľṹ�еĺ���������¼���ͨ���û��ռ��hotplug������(ʵ���ǵ���/sbin/mdev)
 */

/*
 *������kobject��ʱ�򣬶����ÿ��kobjectһϵ��Ĭ�ϵ������ļ�����Щ���Ա�����struct kobj_type�С�
 *
 *���ϣ����kobject��sysfsĿ¼������µ������ļ���ֻ��Ҫ��дһ��attribute�ṹ���������ݸ����溯����*/
 
 int sysfs_create_file(struct kobject *kobj,const struct attribute *attr);//����ú������óɹ���ʹ��attribute�ṹ�е����ִ��������ļ�������0�����򷵻�һ�����Ĵ�����
 
 void sysfs_remove_file(struct kobject *kobj,const struct attribute *attr);//ɾ�������ļ�
 

-----------------------------------------------------------------------------------------------------------
/*
 *    sysfs��Լ��Ҫ���������Զ�ֻ�ܰ���һ���ɶ����ı���ʽ��Ҳ����˵���Դ���һ�����Դ���������������Ե�Ҫ���Ǻ��ٷ����ģ����ǵ��������û��ռ���豸֮�䴫�ݲ��ɸı������ʱ���п���
 *�����������󣨱������豸���ع̼��������������ϵͳ�������������豸���Ϳ��������û��ռ����ͨ���Ȳ�λ��ƣ�ʹ�ö����Ƶ�sysfs���Խ��̼����봫�ݸ��ںˡ�
 */
 
struct bin_attribute { //�ýṹ������������������
	struct attribute	attr;//attr��һ��attribute�ṹ�������������֡������ߡ����������Ե�Ȩ��
	size_t			size;      //���������Ե���󳤶ȣ����û�����ֵ��������Ϊ0��
	void			*private;    //ָ��˽������
	ssize_t (*read)(struct kobject *, struct bin_attribute *, //��
			char *, loff_t, size_t);
	ssize_t (*write)(struct kobject *, struct bin_attribute *,//д
			 char *, loff_t, size_t);
	int (*mmap)(struct kobject *, struct bin_attribute *attr, //ӳ��
		    struct vm_area_struct *vma);
};

int __must_check sysfs_create_bin_file(struct kobject *kobj, struct bin_attribute *attr);//��������������
void sysfs_remove_bin_file(struct kobject *kobj, struct bin_attribute *attr);//ɾ������������

/*
 *�������ӣ�
 *sysfs�ļ�ϵͳ���г��õ����νṹ�Է�ӳkobject֮�����֯��ι�ϵ��ͨ���ں��и�����֮��Ĺ�ϵ���⸴�ӣ�����һ��sysfs��������/sys/devices����ʾ����ϵͳ֪�����豸��
 *
 *����������������/sys/bus����ʾ���豸���������򣩡�������Щ�������ܱ�ʾ��������������豸֮��Ĺ�ϵ��Ϊ�˱�ʾ���ֹ�ϵ����Ҫ������ָ�룬��sysfs���ù���������
 *ʵ�������Ŀ�ġ�
 */
int __must_check sysfs_create_link(struct kobject *kobj, struct kobject *target,const char *name);//��sysfs�д����������ӣ��ú���������һ�����ӣ���Ϊname��ָ��target��sysfs��ڣ�����Ϊkobj��һ�����ԣ�
void sysfs_remove_link(struct kobject *kobj, const char *name);//ɾ����������

-----------------------------------------------------------------------------------------------------------
/*
 *�Ȳ���¼���
 *��1��һ���Ȳ���¼��Ǵ��ں˿ռ䷢�͵��û��ռ��֪ͨ��������ϵͳ���ó����˱仯������kobject���������Ǳ�ɾ����������������¼������統�������ͨ��USB�߲���ϵͳʱ�����û��л�����̨ʱ����
 *
 *     �������̷���ʱ������������¼����Ȳ���¼��ᵼ�¶�/sbin/hotplug(��rcS�ű��������Ѿ�������λ����/sbin/mdev)����ĵ��ã��ó���ͨ����װ�������򣬴����豸�ڵ㣬���ط�������������ȷ�Ķ�������Ӧ�¼�
 *
 *��2����������һ����Ҫ��kobject��������������Щ�¼��������ǰ�kobject���ݸ�kobject_add��kobject_delʱ���Ż�����������Щ�¼������¼������ݵ��û��ռ�֮ǰ������kobject������׼ȷЩ����kobject��
 *
 *     �Ĵ����ܹ�Ϊ�û��ռ������Ϣ������ȫ��ֹ�¼��Ĳ�����
 *
 *��3���Ȳ���¼��Ĵ���ͨ���������������򼶱���߼������ơ�
 *
 *��4�����Ȳ���¼���ʵ�ʿ������ɱ�����struct kset�ṹ��ĳ�Աstruct kset_uevent_ops �ṹ�к��������
 */

struct kset_uevent_ops {
	int (*filter)(struct kset *kset, struct kobject *kobj);//����ʲôʱ�򣬵��ں�ҪΪָ���� kobject�����¼�ʱ����Ҫ���øú���������ú�������0�򽫲������¼���
	                                                       //��˸ú�����kset����һ���������ھ����Ƿ����û��ռ䴫���ض��¼���
	                                                       
	const char *(*name)(struct kset *kset, struct kobject *kobj);//�����û��ռ���Ȳ�γ���ʱ�������ϵͳ�����ֽ���ΪΨһ�Ĳ������ݸ������ú��������ṩ������֣���������һ�����ʴ��ݸ��û��ռ���ַ���
	int (*uevent)(struct kset *kset, struct kobject *kobj,struct kobj_uevent_env *env);//�κ��Ȳ�νű�����Ҫ֪������Ϣ��ͨ�������������ݣ��ú������ڵ��ýű�ǰ���ṩ��ӻ��������Ļ���
	                                                                                   //���Ҫ��envp������κα�������ȷ�������һ���±��������NULL�������ں˾�֪�������ǽ�������
	                                                                                   //�ú���ͨ������0�����ط�0����ֹ�Ȳ���¼��Ĳ���
	                                                                                   //struct kobj_uevent_env {
	                                                                                   //                         char *envp[UEVENT_NUM_ENVP];   //�䱣��������������
                                                                                     //                         int envp_idx;                  //�ñ���˵���ж��ٸ��������
	                                                                                   //                         char buf[UEVENT_BUFFER_SIZE];  //��ű����ı���
	                                                                                   //                         int buflen;                    //��ű����ı����ĳ���
                                                                                     //                       };
};


-----------------------------------------------------------------------------------------------------------
/*
 *���ߡ��豸����������
 *    ���豸ģ���У����е��豸��ͨ�������������������ڲ���������platform�����߿����໥���루����usb������ͨ����һ��PCI�豸����
 *�豸ģ��չʾ�����ߺ����������Ƶ��豸֮�������
 *
 *��1�����ߣ�
 *��linux�豸ģ���У���bus_type�ṹ��ʾ����,������ʾ��
 */
---------------------------------------------����---------------------------------------------------------------
----------------------------------------struct bus_type--------------------------------------------- 
struct bus_type {//�ýṹ��ʾ����
	const char		*name;                //���ߵ�����
	struct bus_attribute	*bus_attrs;   //���ߵ�����
	struct device_attribute	*dev_attrs; //�豸������
	struct driver_attribute	*drv_attrs; //����������

	int (*match)(struct device *dev, struct device_driver *drv);     //ƥ�亯����ƥ���򷵻ط�0ֵ���ڵ���ʵ�ʵ�Ӳ��ʱ��mach����ͨ�����豸�ṩ��Ӳ��ID��������֧�ֵ�ID��ĳ�ֱȽϣ�
	                                                                 //������platform��ʹ������������ƥ�䣬���������ͬ��˵��֧�ָ��豸��
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);  //�Ȳ���¼��������������Ѿ�������
	int (*probe)(struct device *dev);                                //̽�⺯������ָ���²��ṩ�ĺ�������mach����ƥ�������ߺ��豸��������̽���豸��
	int (*remove)(struct device *dev);                               //�Ƴ��豸
	void (*shutdown)(struct device *dev);                            //�ر��豸

	int (*suspend)(struct device *dev, pm_message_t state);          //������
	int (*suspend_late)(struct device *dev, pm_message_t state);
	int (*resume_early)(struct device *dev);
	int (*resume)(struct device *dev);                               //�ָ�����

	struct dev_pm_ops *pm;

	struct bus_type_private *p;    //�Ѳ���˽���ֶη�װ������ṹ��
};
---------------------------------------------------------------------------------------------------

------------------------------------struct bus_attribute-------------------------------------------
struct bus_attribute {//���ߵ�����
	struct attribute	attr;  //attr��һ��attribute�ṹ�������������֡������ߡ����������Ե�Ȩ��
	ssize_t (*show)(struct bus_type *bus, char *buf);//��ʾ���ԣ����û��ռ��ȡһ������ʱ���ں˻�ʹ��ָ��kobject ��ָ�����ȷ�����Խṹ������show������
	                                                 //�ú�������ָ����ֵ�������뻺������Ȼ���ʵ�ʳ�����Ϊ����ֵ���ء�
	                                                 //attrָ�����ԣ��������ж�����������ĸ����ԡ�
	ssize_t (*store)(struct bus_type *bus, const char *buf, size_t count);//�������ԣ��ú����������ڻ����������ݽ��룬�����ø���ʵ�÷���������ֵ������ʵ�ʽ�����ֽ���
	                                                                      //ֻ�е�ӵ�����Ե�дȨ��ʱ�����ܵ��øú���
};

//������ز���������
#define BUS_ATTR(_name, _mode, _show, _store) \   //�ú�����ڴ����ͳ�ʼ���������Խṹ
									struct bus_attribute bus_attr_##_name = __ATTR(_name, _mode, _show, _store);
									
int __must_check bus_create_file(struct bus_type *,struct bus_attribute *);//�����������ߵ��κ�����

void bus_remove_file(struct bus_type *, struct bus_attribute *);//ɾ������	
--------------------------------------------------------------------------------------------------

//���ߵ�ע�᣺
int bus_register(struct bus_type *bus);//ע�ᣨ���ú�struct bus_type��ʹ�øú���ע�ᣨע��ֻ�зǳ��ٵ� bus_type��Ա��Ҫ��ʼ���������еĴ���������豸
                                       //ģ�ͺ��������ƣ����Ǳ���Ϊ����ָ�����֡�match��uevent��һЩ��Ҫ�ĳ�Ա����
                                       //������ÿ��ܻ�ʧ�ܣ���˱��������ķ���ֵ������ɹ����µ�������ϵͳ����ӵ�ϵͳ�У�������/sys/busĿ¼������
void bus_unregister(struct bus_type *bus);//ע��

/*
 *���豸����������ĵ�����
 *�ڱ�д���߲����ᷢ�ֲ��ò���ע�ᵽ���ߵ������豸����������ִ��ĳЩ������Ϊ�˲���ע�ᵽ���ߵ�ÿ���豸���ɵ������溯��
 */
int bus_for_each_dev(struct bus_type *bus, struct device *start,void *data, int (*fn)(struct device *, void *));//�ú����������������ϵ�ÿ���豸������ص�device�ṹ���ݸ�fn��ͬʱ����dataֵ�����start��NULL��
                                                                                                                //�������ߵĵ�һ���豸��ʼ���������򽫴�start��ĵ�һ���豸��ʼ������
                                                                                                                //���fn����һ����0ֵ����ֹͣ�����������ֵҲ��Ӹú�������

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,void *data, int (*fn)(struct device_driver *, void *));//�ú���������������ĵ������ú����Ĺ�����ʽ������ĺ������ƣ�
                                                                                                                              //ֻ�����Ĺ�����������������
                                                                                                                              
                                                                                                                              
------------------------------------------------�豸------------------------------------------------------------
/*
 *��2���豸��
 *linuxϵͳ��ÿ���豸����device�ṹ����ʾ��������ʾ��
 */
-----------------------------------------struct device---------------------------------------------
struct device {  //��ʾ�����豸
	struct device		*parent;   //�豸�ĸ��豸----ָ���Ǹ��豸�������豸���ڴ��������£�һ�����豸ͨ����ĳ�����߻��������������������ָ��ΪNULL���ʾ���豸�Ƕ����豸��������������ټ���

	struct device_private	*p;

	struct kobject kobj;      //��ʾ���豸���������ӵ��ṹ��ϵ��kobject
	const char		*init_name; //�豸�ĳ�ʼ����
	struct device_type	*type;//�豸����

	struct semaphore	sem;	/* semaphore to synchronize calls to  //�ź���
					 * its driver.
					 */

	struct bus_type	*bus;		/* type of bus device is on */           //��ʶ���豸�����ں������͵�������
	struct device_driver *driver;	/* which driver has allocated this //������豸����������
					   device */
	void		*driver_data;	/* data private to the driver */           //���豸��������ʹ�õ�˽������
	void		*platform_data;	/* Platform specific data, device        //platform�����豸˽������
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

	dev_t			devt;	/* dev_t, creates the sysfs "dev" */        //�豸��
 
	spinlock_t		devres_lock;   //������
	struct list_head	devres_head;

	struct klist_node	knode_class;
	struct class		*class;                         //��
	struct attribute_group	**groups;	/* optional groups */    //������

	void	(*release)(struct device *dev);         //��ָ���豸�����һ�����ñ�ɾ��ʱ���ں˵��ø÷�������������Ƕ��kobject��release�����е��ã�
	                                              //���������ע���device�ṹ������Ҫ�ṩ�ú����������ں˽���ӡ������Ϣ��
};
----------------------------------------------------------------------------------------------------

//�豸ע�᣺
int device_register(struct device *dev);//ע��
void device_unregister(struct device *dev);//ע��

-----------------------------------struct device_attribute------------------------------------------
struct device_attribute {//�豸�����ԣ�sys�е��豸��ڿ���������
	struct attribute	attr;
	ssize_t (*show)(struct device *dev, struct device_attribute *attr,
			char *buf);
	ssize_t (*store)(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count);
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \ //�ú�����ڴ����ͳ�ʼ���豸���Խṹ
struct device_attribute dev_attr_##_name = __ATTR(_name, _mode, _show, _store)

int __must_check device_create_file(struct device *device, struct device_attribute *entry);//���������豸���κ�����

void device_remove_file(struct device *dev,struct device_attribute *attr);//ɾ������

int __must_check device_create_bin_file(struct device *dev,struct bin_attribute *attr);//���������豸���κζ���������

void device_remove_bin_file(struct device *dev,struct bin_attribute *attr);//ɾ������������
---------------------------------------------------------------------------------------------------

/*
 *�豸�ṹ��Ƕ�룺
 *���Ķ����������ʱ����Է��ֵ�����device�ṹ��ʾ���豸�Ǻ��ټ��ģ�����pci_dev��usb_device�ṹ���壬���ᷢ������������device�ṹ��
 */

------------------------------------------------�豸����------------------------------------------------------
--------------------------------------struct device_driver------------------------------------------
struct device_driver {//��ʾ�豸����
	const char		*name;     //�������������
	struct bus_type		*bus;  //��������������������������

	struct module		*owner;  //ָ��ģ��
	const char 		*mod_name;	/* used for built-in modules *///ģ������

	int (*probe) (struct device *dev);                        //̽�⺯��
	int (*remove) (struct device *dev);                       //�Ƴ�����
	void (*shutdown) (struct device *dev);                    //�رպ���
	int (*suspend) (struct device *dev, pm_message_t state);  //������
	int (*resume) (struct device *dev);                       //�ָ�����
	struct attribute_group **groups;   //������

	struct dev_pm_ops *pm;

	struct driver_private *p;
};
-------------------------------------------------------------------------------------------------------------
//ע���豸������
int driver_register(struct device_driver *drv);//ע��
void driver_unregister(struct device_driver *drv);//ע��

----------------------------------struct driver_attribute----------------------------------------
struct driver_attribute {//����������
	struct attribute attr;
	ssize_t (*show)(struct device_driver *driver, char *buf);
	ssize_t (*store)(struct device_driver *driver, const char *buf,
			 size_t count);
};

#define DRIVER_ATTR(_name, _mode, _show, _store)	\  //�ú�����ڴ����ͳ�ʼ���������Խṹ
struct driver_attribute driver_attr_##_name =	__ATTR(_name, _mode, _show, _store)

int __must_check driver_create_file(struct device_driver *driver,struct driver_attribute *attr);//���������������κ�����

void driver_remove_file(struct device_driver *driver,struct driver_attribute *attr);	//ɾ������							
------------------------------------------------------------------------------------------------
/*
 *��������ṹ��Ƕ�룺
 *���ڴ��������������Ľṹ��˵��device_driver�ṹͨ���������ڸ߲��������صĽṹ�С�
 */
-------------------------------------------------------------------------------------------------------------


------------------------------------------------------��-----------------------------------------------------
/*
 *��1������һ���豸�ĸ߲���ͼ����������˵ײ��ʵ��ϸ�ڡ��������û��ռ���ʹ���豸���ṩ�Ĺ��ܣ����������豸��������ӵ��Լ���������ι����ġ�
 *
 *��2���������е��඼��ʾ��/sys/classĿ¼�У����磺��������ӿڵ�������ʲô�����е�����ӿڶ�������/sys/class/net�£������豸������/sys/class/input���ҵ����������豸��������/sys/class/tty��
 *
 *��3�����Աͨ�����ϲ������ƣ�������Ҫ���������������ȷ֧��
 *
 *��4�����������£�����ϵͳ�����û��ռ䵼����Ϣ����õķ���������ϵͳ����һ����ʱ��������ȫӵ������࣬��˸������õ����ĸ�ģ��ӵ����Щ����
 */
#include <linux/device.h>
struct class {//��
	const char		*name; //�������
	struct module		*owner; //ָ��ģ��

	struct class_attribute		*class_attrs;  //�������
	struct device_attribute		*dev_attrs;    //�豸����
	struct kobject			*dev_kobj;           

	int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);  //���Ȳ���¼�����ʱ�����øú�������ӻ�������

	void (*class_release)(struct class *class);  //�ͷ��౾��
	void (*dev_release)(struct device *dev);     //���豸������ɾ��

	int (*suspend)(struct device *dev, pm_message_t state);  //������
	int (*resume)(struct device *dev);  //�ָ�����

	struct dev_pm_ops *pm;
	struct class_private *p;
};

struct class_attribute {
	struct attribute attr;
	ssize_t (*show)(struct class *class, char *buf);
	ssize_t (*store)(struct class *class, const char *buf, size_t count);
};

#define CLASS_ATTR(_name, _mode, _show, _store)			\  //�ú�����ڴ����ͳ�ʼ�������Խṹ
struct class_attribute class_attr_##_name = __ATTR(_name, _mode, _show, _store)

int __must_check class_create_file(struct class *class,   const struct class_attribute *attr);//������������κ�����
void class_remove_file(struct class *class, const struct class_attribute *attr); //ɾ������	

//���ע�᣺
#define class_register(class);//ע��	
void class_unregister(struct class *cls);//ע��
-------------------------------------------------------------------------------------------------------------
/*
 *����������ܽ��豸ģ�͵Ĳ�Σ�
 *��1������һ��struct bus_type���͵ı���xxx_bus_type����ʼ����Ȼ������ں���xxx_driver_init�е���bus_register(&xxx_bus_type)������������ע�����ߣ�����ע������ΪPCI����ô����/sys/bus/pci
 *     �д���һ��sysfsĿ¼�����а�������Ŀ¼��devices��drivers��
 *
 *��2������һ��xxx_driver�ṹ��������ʾ�����ýṹ�а���device_driver�ṹ��������Ҫ��xxx_register_driver������������Ƶ����������ע�ᣩ�ж�����г�ʼ��,Ȼ�ڵ���driver_register��ע������
 *     struct xxx_driver {    //xxx���� 
 *	                struct list_head node;
 *	                char *name;       // �������� 
 *	                const struct xxx_device_id *id_table;	 //�豸ID�����豸��ȡ��ID�붨���IDƥ��Ļ������probe���� 
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
 *��3������һ��xxx_dev�ṹ����������xxx�豸��������ʾ�����������struct	device�ṹ������غ����ж����ʼ�������device_register�������ע���豸
 *     struct xxx_dev {
 *                     .......
 *                     .......
 *                     struct xxx_driver *driver;
 *                     struct	device	dev;
 *                     ......
 *                     ......
 *                    };
 *
 *��4�����������xxx���߽��������ض���ϵ�ܹ�����İ����£�xxx���Ŀ�ʼ̽��xxx��ַ�ռ�(���ջ����bus��probe�ֶλ�device_driver��probe�ֶ�ʵ�ֵķ���)���������е�xxx�豸����һ��xxx�豸���ҵ�ʱ��xxx�������ڴ���
 *
 *    ���ڴ洴��һ��xxx_dev���͵Ľṹ������ ����ṹ����������صĵĳ�Ա��xxx�ĺ��ĳ�ʼ��������device�ṹ������parent����������Ϊ��xxx�豸���ڵ������豸��
 *
 *��5����device_register�����У�����������Ķ�device�е�����Ա���г�ʼ������kobject����ע���豸kset(�⽫����һ���Ȳ���¼�)��Ȼ�󽫸��豸��ӵ��豸�����У����豸����Ϊ��������
 *
 *     ���ĸ��ڵ���ӵ�С������Щ���������е��豸������ͨ����ȷ��˳����ʣ�����֪��ÿ���豸�����ڲ�νṹ����һ���ϡ�
 *
 *��6�������豸������ӵ���������ص������豸�����м�xxx_bus_type��������������������������ע����豸���������������Ϊÿ������������ø����ߵ�mach��������һ���豸������
 *
 *     xxx_bus_type������˵��xxx�����ڰ��豸�ύ�������������ǰ����mach����ָ��xxx_bus_mach������
 *
 *��7��xxx_bus_mach������������������Ĵ��ݸ�����device�ṹת��Ϊxxx_dev�ṹ��������device_driver�ṹת��Ϊxxx_driver�ṹ�����Ҳ鿴�豸�����������е�xxx�豸�����Ϣ����ȷ����������
 *
 *     �Ƿ�֧�������豸�����������ƥ�乤��û����ȷִ�У��ú�����������������ķ���0����������������Ŀ������������е���һ����������
 *
 *��8�����ƥ�乤��Բ����ɣ�����������������ķ���1.�⽫�����������Ľ�device�ṹ�е�driverָ��ָ�������������Ȼ�����device_driver�ṹ��ָ����probe������
 *
 *��9����xxx���������������������ע��ǰ��probe����������Ϊָ��xxx_device_probe�������ú�����device�ṹת��Ϊxxx_dev�ṹ�����Ұ���device�����õ�driver�ṹת��Ϊxxx_driver�ṹ����Ҳ
 *
 *     ���ܼ��������������״̬����ȷ������֧������豸�������豸���ô�����Ȼ���ð󶨵�xxx_dev�ṹָ��Ϊ����������xxx���������probe������
 *
 *��10�����xxx���������probe��������ĳ��ԭ���ж����ܴ�������豸���佫���ظ��Ĵ���ֵ������������ģ��⽫��������������ļ��������������б�����������ƥ������豸�����probe����̽��
 *
 *      �����豸��Ϊ�������������豸�����������г�ʼ��������Ȼ��������������ķ���0.���ʹ����������Ľ����豸��ӵ������������󶨵��豸�����У�������sysfs�е�driversĿ¼����ǰ����
 *
 *      �豸֮�佨���������ӣ������������ʹ���û�֪���ĸ��������򱻰󶨵����ĸ��豸�ϡ�
 *
 *��11����ɾ��һ��xxx�豸ʱ��xxx_unregister_driver����������غ������ú�����Щxxx��ص���������Ȼ��ʹ��ָ��xxx_dev��device�ṹָ�룬����device_unregister��������device_unregister������
 *
 *      ����������ֻ��ɾ���˴Ӱ��豸��������������а󶨵Ļ�����sysfs�ļ��������ӣ����ڲ��豸������ɾ���˸��豸��������device�ṹ�е�kobject�ṹָ��Ϊ����������kobject_del������
 *
 *      �ú����������û��ռ�hotplug���á�kobject��ϵͳ��ɾ����Ȼ��ɾ��ȫ����kobject��ص�sysfs�����ļ���Ŀ¼������ЩĿ¼���ļ�����kobject��ǰ�����ġ�kobject_del������ɾ�����豸
 *
 *      ��kobject���ã���������������һ������Ҫ���ø�xxx�豸��release�������ú����ͷ���xxx_dev�ṹ��ռ�õĿռ䣬����xxx�豸����ȫ��ϵͳɾ����
 */
****************************************************************************************************************************************************