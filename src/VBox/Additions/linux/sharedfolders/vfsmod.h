/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef VFSMOD_H
#define VFSMOD_H

#define elog(fmt, ...) \
printk (KERN_ERR "vboxvfs: %s: " fmt, __func__, __VA_ARGS__)
#define elog2(s) printk (KERN_ERR "vboxvfs: %s: " s, __func__)
#define elog3(...) printk (KERN_ERR "vboxvfs: " __VA_ARGS__)

#ifdef ALIGN
#undef ALIGN
#endif

#define CMC_API __attribute__ ((cdecl, regparm (0)))

#define DBGC if (0)
#define TRACE() DBGC printk (KERN_DEBUG "%s\n", __func__)

/* Following casts are here to prevent assignment of void * to
   pointers of arbitrary type */
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
#define GET_GLOB_INFO(sb) ((struct sf_glob_info *) (sb)->u.generic_sbp)
#define SET_GLOB_INFO(sb, sf_g) (sb)->u.generic_sbp = sf_g
#else
#define GET_GLOB_INFO(sb) ((struct sf_glob_info *) (sb)->s_fs_info)
#define SET_GLOB_INFO(sb, sf_g) (sb)->s_fs_info = sf_g
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19) || defined(KERNEL_FC6)
/* FC6 kernel 2.6.18, vanilla kernel 2.6.19+ */
#define GET_INODE_INFO(i) ((struct sf_inode_info *) (i)->i_private)
#define SET_INODE_INFO(i, sf_i) (i)->i_private = sf_i
#else
/* vanilla kernel up to 2.6.18 */
#define GET_INODE_INFO(i) ((struct sf_inode_info *) (i)->u.generic_ip)
#define SET_INODE_INFO(i, sf_i) (i)->u.generic_ip = sf_i
#endif

#endif /* vfsmod.h */
