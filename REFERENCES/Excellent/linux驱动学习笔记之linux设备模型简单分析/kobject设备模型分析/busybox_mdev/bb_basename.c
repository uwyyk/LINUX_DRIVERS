/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

const char *bb_basename(const char *name)   /* name= path=/sys/DEVPATH，DEVPATH=class/..../dev*/
{
	const char *cp = strrchr(name, '/');   /* 从字符串name的末尾开始查找第一个"/ "，返回指向它的指针*/  
	if (cp)
		return cp + 1;   /* 指向"/ "后面的字符串  */
	return name;
}
