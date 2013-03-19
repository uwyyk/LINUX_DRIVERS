
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
 parent=kobject_create_and_add("my_kobj", NULL);  /* 可以在/sys下创建一个 my_kobj目录*/
 child=kobject_create_and_add("child_kobj",parent); /* 可以在/sys/my_kobj下创建一个 child_kobj目录*/
 return 0;
}

static void __exit kobj_exit(void)
{
  kobject_del(parent);  /* 卸载了模块后，上面的两个目录消失 */
  kobject_del(child);
}

module_init(kobj_init);
module_exit(kobj_exit);
MODULE_LICENSE("GPL");






