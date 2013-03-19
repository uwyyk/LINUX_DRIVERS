/*
 * kernel userspace event delivery
 *
 * Copyright (C) 2004 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2004 Novell, Inc.  All rights reserved.
 * Copyright (C) 2004 IBM, Inc. All rights reserved.
 *
 * Licensed under the GNU GPL v2.
 *
 * Authors:
 *	Robert Love		<rml@novell.com>
 *	Kay Sievers		<kay.sievers@vrfy.org>
 *	Arjan van de Ven	<arjanv@redhat.com>
 *	Greg Kroah-Hartman	<greg@kroah.com>
 */

#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/module.h>

#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/sock.h>


u64 uevent_seqnum;
char uevent_helper[UEVENT_HELPER_PATH_LEN] = CONFIG_UEVENT_HELPER_PATH;
static DEFINE_SPINLOCK(sequence_lock);
#if defined(CONFIG_NET)
static struct sock *uevent_sock;
#endif

/* the strings here must match the enum in include/linux/kobject.h */
static const char *kobject_actions[] = {
	[KOBJ_ADD] =		"add",
	[KOBJ_REMOVE] =		"remove",
	[KOBJ_CHANGE] =		"change",
	[KOBJ_MOVE] =		"move",
	[KOBJ_ONLINE] =		"online",
	[KOBJ_OFFLINE] =	"offline",
};

/**
 * kobject_action_type - translate action string to numeric type
 *
 * @buf: buffer containing the action string, newline is ignored
 * @len: length of buffer
 * @type: pointer to the location to store the action type
 *
 * Returns 0 if the action string was recognized.
 */
int kobject_action_type(const char *buf, size_t count,
			enum kobject_action *type)
{
	enum kobject_action action;
	int ret = -EINVAL;

	if (count && (buf[count-1] == '\n' || buf[count-1] == '\0'))
		count--;

	if (!count)
		goto out;

	for (action = 0; action < ARRAY_SIZE(kobject_actions); action++) {
		if (strncmp(kobject_actions[action], buf, count) != 0)
			continue;
		if (kobject_actions[action][count] != '\0')
			continue;
		*type = action;
		ret = 0;
		break;
	}
out:
	return ret;
}

/**
 * kobject_uevent_env - send an uevent with environmental data
 *
 * @action: action that is happening
 * @kobj: struct kobject that the action is happening to
 * @envp_ext: pointer to environmental data
 *
 * Returns 0 if kobject_uevent() is completed with success or the
 * corresponding error when it fails.
 */
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,   /* 发送一次热插拔事件并且设置事件的环境变量 */
}
		       char  *envp_ext[] )
{
	struct kobj_uevent_env *env;
	const char *action_string = kobject_actions[action];   /* action =KOBJ_ADD =0 --->action_string=add*/
	const char *devpath = NULL;
	const char *subsystem;
	struct kobject *top_kobj;
	struct kset *kset;
	struct kset_uevent_ops *uevent_ops;   /* 事件操作函数 */
	u64 seq;
	int i = 0;
	int retval = 0;

	pr_debug("kobject: '%s' (%p): %s\n",
		 kobject_name(kobj), kobj, __func__);

	/* search the kset we belong to */
	top_kobj = kobj;
	/* 如果kobj->kset为空并且kobj->parent 存在则继续向上层寻找直到kobj->kset存在即该循环用来查找kobj所隶属
	    的最顶层kset*/
	while (!top_kobj->kset && top_kobj->parent)  
		top_kobj = top_kobj->parent;

	if (!top_kobj->kset) {  /* 如果当前kobj没有隶属的kset，那么它将不能使用uevent机制，则做错误处理 */
		pr_debug("kobject: '%s' (%p): %s: attempted to send uevent "
			 "without kset!\n", kobject_name(kobj), kobj,
			 __func__);
		return -EINVAL;
	}

	kset = top_kobj->kset;  /* 获取找到的kset */
	uevent_ops = kset->uevent_ops;  /* 获取找到的kset的热插拔事件处理函数 */

	/* skip the event, if uevent_suppress is set*/
	if (kobj->uevent_suppress) {    /* 设置该字段为1时则忽略这个热插拔事件 */
		pr_debug("kobject: '%s' (%p): %s: uevent_suppress "
				 "caused the event to drop!\n",
				 kobject_name(kobj), kobj, __func__);
		return 0;
	}
	/* skip the event, if the filter returns zero. */
	if (uevent_ops && uevent_ops->filter)   /* 当uevent_ops->filter函数(过滤事件) 函数存在*/
		if (!uevent_ops->filter(kset, kobj)) { /* 则调用该过滤事件函数，如果该函数返回0，则忽略这个事件 */
			pr_debug("kobject: '%s' (%p): %s: filter function "
				 "caused the event to drop!\n",
				 kobject_name(kobj), kobj, __func__);
			return 0;
		}

	/* originating subsystem */
	/* 获取子系统的名字，如果不存在则返回 */
	if (uevent_ops && uevent_ops->name)   /* 如果事件处理函数指针name指向的函数存在 */
		subsystem = uevent_ops->name(kset, kobj);  /* 则调用该函数来返回一个 合适传递给用户空间的字符串*/
	else
		subsystem = kobject_name(&kset->kobj);   /* 否则就使用kobject的名字 */
	if (!subsystem) {
		pr_debug("kobject: '%s' (%p): %s: unset subsystem caused the "
			 "event to drop!\n", kobject_name(kobj), kobj,
			 __func__);
		return 0;
	}

	/* environment buffer */
	/* 分配环境变量缓冲区 */
	env = kzalloc(sizeof(struct kobj_uevent_env), GFP_KERNEL);
	if (!env)
		return -ENOMEM;

	/* complete object path */
	devpath = kobject_get_path(kobj, GFP_KERNEL);  /* 获取引发事件的kobject在sysfs中路径 */
	if (!devpath) {
		retval = -ENOENT;
		goto exit;
	}

	/* default keys */
	/*设置环境变量*/
	retval = add_uevent_var(env, "ACTION=%s", action_string);  /* 添加环境变量ACTION=add  or  remove */
	if (retval)
		goto exit;
	retval = add_uevent_var(env, "DEVPATH=%s", devpath);     /* 添加路径环境变量DEVPATH */
	if (retval)
		goto exit;
	retval = add_uevent_var(env, "SUBSYSTEM=%s", subsystem); /*添加子系统环境变量 SUBSYSTEM*/
	if (retval)
		goto exit;

	/* keys passed in from the caller */
	if (envp_ext) {
		for (i = 0; envp_ext[i]; i++) {
			retval = add_uevent_var(env, "%s", envp_ext[i]);   /* 添加传进来的环境变量 */
			if (retval)
				goto exit;
		}
	}

	/* let the kset specific function add its stuff */
	/* 此处在向用户空间发送event消息之前，给kset最后一次机会以完成一些私有事情 */
	if (uevent_ops && uevent_ops->uevent) {
		retval = uevent_ops->uevent(kset, kobj, env); 
		if (retval) {
			pr_debug("kobject: '%s' (%p): %s: uevent() returned "
				 "%d\n", kobject_name(kobj), kobj,
				 __func__, retval);
			goto exit;
		}
	}

	/*
	 * Mark "add" and "remove" events in the object to ensure proper
	 * events to userspace during automatic cleanup. If the object did
	 * send an "add" event, "remove" will automatically generated by
	 * the core, if not already done by the caller.
	 *
	 *
	 *
	 *
	 static const char *kobject_actions[] = {
	        [KOBJ_ADD] =		"add",
	        [KOBJ_REMOVE] =		"remove",
	        [KOBJ_CHANGE] =		"change",
	        [KOBJ_MOVE] =		"move",
	        [KOBJ_ONLINE] =		"online",
	        [KOBJ_OFFLINE] =	"offline",
               };
	 */
	if (action == KOBJ_ADD)    /* action =KOBJ_ADD */
		kobj->state_add_uevent_sent = 1;   /* 设置添加设备相应的标志位 */
	else if (action == KOBJ_REMOVE)
		kobj->state_remove_uevent_sent = 1;/* 设置移除设备相应的标志位 */

	/* we will send an event, so request a new sequence number */
	spin_lock(&sequence_lock);
	seq = ++uevent_seqnum;
	spin_unlock(&sequence_lock);
	retval = add_uevent_var(env, "SEQNUM=%llu", (unsigned long long)seq);
	if (retval)
		goto exit;

#if defined(CONFIG_NET)  /* 如果配置了这个宏，表明内核打算使用netlink机制实现uevent消息发送 */
	/* send netlink message */
	if (uevent_sock) {
		struct sk_buff *skb;
		size_t len;

		/* allocate message with the maximum possible size */
		len = strlen(action_string) + strlen(devpath) + 2;
		skb = alloc_skb(len + env->buflen, GFP_KERNEL);
		if (skb) {
			char *scratch;

			/* add header */
			scratch = skb_put(skb, len);
			sprintf(scratch, "%s@%s", action_string, devpath);

			/* copy keys to our continuous event payload buffer */
			for (i = 0; i < env->envp_idx; i++) {
				len = strlen(env->envp[i]) + 1;
				scratch = skb_put(skb, len);
				strcpy(scratch, env->envp[i]);
			}

			NETLINK_CB(skb).dst_group = 1;
			retval = netlink_broadcast(uevent_sock, skb, 0, 1,
						   GFP_KERNEL);
			/* ENOBUFS should be handled in userspace */
			if (retval == -ENOBUFS)
				retval = 0;
		} else
			retval = -ENOMEM;
	}
#endif

	/* call uevent_helper, usually only enabled during early boot */
	if (uevent_helper[0]) {   /* uevent_helper就是我们的应用程序：比如/sbin/mdev (想搞清楚如果相应热插拔事件
	                                            的就需要bosybox的mdev.c(在busybox的说明文档mdev.txt中有这样一句话echo /bin/mdev > /proc/sys/kernel/hotplug)) */
		char *argv [3];
              /* 加打印语句，打印环境变量 */
	       printk("shenchaoping: uevent_helper=%s\n", uevent_helper);
		for(i=0;env->envp[i] ;i++)
	               printk("env->envp[%d]=%s\n", i,env->envp[i]);

		
		argv [0] = uevent_helper;  /* uevent_helper="/sbin/hotplug",但是现在的大部分linux系统都没这个文件  */
		argv [1] = (char *)subsystem;
		argv [2] = NULL;
		retval = add_uevent_var(env, "HOME=/");
		if (retval)
			goto exit;
		retval = add_uevent_var(env,
					"PATH=/sbin:/bin:/usr/sbin:/usr/bin");
		if (retval)
			goto exit;

		retval = call_usermodehelper(argv[0], argv,   /* 该函数从内核空间运行一个用户进程(调用 argv[0]所指向的应用程序)*/
					     env->envp, UMH_WAIT_EXEC);
	}

exit:
	kfree(devpath);
	kfree(env);
	return retval;
}
EXPORT_SYMBOL_GPL(kobject_uevent_env);

/**
 * kobject_uevent - notify userspace by ending an uevent
 *
 * @action: action that is happening
 * @kobj: struct kobject that the action is happening to
 *
 * Returns 0 if kobject_uevent() is completed with success or the
 * corresponding error when it fails.
 */
 /* 热插拔事件 (当一个设备动态的加入系统时，设备驱动程序可以检查这个设备状态的变化(加入或移除
      )，然后通过某种机制使得用户空间找到该设备对应的设备驱动并加载之)，它通过发送一个uevent消息
      和调用call_usermodehelper用来与用户空间沟通(udev和/sbin/hotplug机制)*/
int kobject_uevent(struct kobject *kobj, enum kobject_action action)    
{
	return kobject_uevent_env(kobj, action, NULL);    /* 发送一次热插拔事件并且设置事件的环境变量 */
}
EXPORT_SYMBOL_GPL(kobject_uevent);

/**
 * add_uevent_var - add key value string to the environment buffer
 * @env: environment buffer structure
 * @format: printf format for the key=value pair
 *
 * Returns 0 if environment variable was added successfully or -ENOMEM
 * if no space was available.
 */
int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
{
	va_list args;
	int len;

	if (env->envp_idx >= ARRAY_SIZE(env->envp)) {
		WARN(1, KERN_ERR "add_uevent_var: too many keys\n");
		return -ENOMEM;
	}

	va_start(args, format);
	len = vsnprintf(&env->buf[env->buflen],
			sizeof(env->buf) - env->buflen,
			format, args);
	va_end(args);

	if (len >= (sizeof(env->buf) - env->buflen)) {
		WARN(1, KERN_ERR "add_uevent_var: buffer size too small\n");
		return -ENOMEM;
	}

	env->envp[env->envp_idx++] = &env->buf[env->buflen];
	env->buflen += len + 1;
	return 0;
}
EXPORT_SYMBOL_GPL(add_uevent_var);

#if defined(CONFIG_NET)
static int __init kobject_uevent_init(void)
{
	uevent_sock = netlink_kernel_create(&init_net, NETLINK_KOBJECT_UEVENT,
					    1, NULL, NULL, THIS_MODULE);
	if (!uevent_sock) {
		printk(KERN_ERR
		       "kobject_uevent: unable to create netlink socket!\n");
		return -ENODEV;
	}
	netlink_set_nonroot(NETLINK_KOBJECT_UEVENT, NL_NONROOT_RECV);
	return 0;
}

postcore_initcall(kobject_uevent_init);
#endif
