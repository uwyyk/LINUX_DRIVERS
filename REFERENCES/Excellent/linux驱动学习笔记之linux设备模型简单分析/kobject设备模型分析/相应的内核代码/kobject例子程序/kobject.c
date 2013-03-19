
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/kernel.h>

static struct kobject *child=NULL;
static struct kobject *parent=NULL;
static int __init kobj_init(void)
{
 parent=kobject_create_and_add("my_kobj", NULL);  /* ������/sys�´���һ�� my_kobjĿ¼*/
 child=kobject_create_and_add("child_kobj",parent); /* ������/sys/my_kobj�´���һ�� child_kobjĿ¼*/
 return 0;
}

static void __exit kobj_exit(void)
{
  kobject_del(parent);  /* ж����ģ������������Ŀ¼��ʧ */
  kobject_del(child);
}

module_init(kobj_init);
module_exit(kobj_exit);
MODULE_LICENSE("GPL");






