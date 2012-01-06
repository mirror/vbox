/** @file $Id$
 *
 * VirtualBox Additions Linux kernel driver, DRM support
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * tdfx_drv.c -- tdfx driver -*- linux-c -*-
 * Created: Thu Oct  7 10:38:32 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
# include <generated/autoconf.h>
#else
# ifndef AUTOCONF_INCLUDED
#  include <linux/autoconf.h>
# endif
#endif
#include <linux/module.h>
#include "version-generated.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#  ifdef RHEL_RELEASE_CODE
#   if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,1)
#    define DRM_RHEL61
#   endif
#  endif
# endif

#include "drm/drmP.h"
#include "vboxvideo_drm.h"

static struct pci_device_id pciidlist[] = {
        vboxvideo_PCI_IDS
};

int vboxvideo_driver_load(struct drm_device * dev, unsigned long flags)
{
# if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 28)
    return drm_vblank_init(dev, 1);
#else
    return 0;
#endif
}

static struct drm_driver driver =
{
    /* .driver_features = DRIVER_USE_MTRR, */
    .load = vboxvideo_driver_load,
    .reclaim_buffers = drm_core_reclaim_buffers,
    /* As of Linux 2.6.37, always the internal functions are used. */
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 37) && !defined(DRM_RHEL61)
    .get_map_ofs = drm_core_get_map_ofs,
    .get_reg_ofs = drm_core_get_reg_ofs,
#endif
    .fops =
    {
        .owner = THIS_MODULE,
        .open = drm_open,
        .release = drm_release,
        /* This was changed with Linux 2.6.33 but Fedora backported this
         * change to their 2.6.32 kernel. */
#if defined(DRM_UNLOCKED) || LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 33)
        .unlocked_ioctl = drm_ioctl,
#else
        .ioctl = drm_ioctl,
#endif
        .mmap = drm_mmap,
        .poll = drm_poll,
        .fasync = drm_fasync,
    },
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 39) && !defined(DRM_RHEL61)
    .pci_driver =
    {
        .name = DRIVER_NAME,
        .id_table = pciidlist,
    },
#endif
    .name = DRIVER_NAME,
    .desc = DRIVER_DESC,
    .date = DRIVER_DATE,
    .major = DRIVER_MAJOR,
    .minor = DRIVER_MINOR,
    .patchlevel = DRIVER_PATCHLEVEL,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 39) && !defined(DRM_RHEL61)
static struct pci_driver pci_driver =
{
    .name = DRIVER_NAME,
    .id_table = pciidlist,
};
#endif

static int __init vboxvideo_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 39) && !defined(DRM_RHEL61)
    return drm_init(&driver);
#else
    return drm_pci_init(&driver, &pci_driver);
#endif
}

static void __exit vboxvideo_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 39) && !defined(DRM_RHEL61)
    drm_exit(&driver);
#else
    drm_pci_exit(&driver, &pci_driver);
#endif
}

module_init(vboxvideo_init);
module_exit(vboxvideo_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */

#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING);
#endif
MODULE_LICENSE("GPL and additional rights");
