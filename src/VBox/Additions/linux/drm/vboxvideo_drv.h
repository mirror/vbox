/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
 * This code is based on
 * glint_drv.h
 * with the following copyright and permission notice:
 *
 * Copyright 2010 Matt Turner.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Matt Turner
 */

#ifndef __DRM_VBOXVIDEO_DRV_H__
#define __DRM_VBOXVIDEO_DRV_H__

/* General customization:
 */

#include "vboxvideo.h"

#include "product-generated.h"

#define DRIVER_AUTHOR       VBOX_VENDOR

#define DRIVER_NAME         "vboxvideo"
#define DRIVER_DESC         VBOX_PRODUCT " Graphics Card"
#define DRIVER_DATE         "20090303"

#define DRIVER_MAJOR        1
#define DRIVER_MINOR        0
#define DRIVER_PATCHLEVEL   0

#define VBOXVIDEO_INFO(fmt, arg...)  DRM_INFO(DRIVER_NAME ": " fmt, ##arg)
#define VBOXVIDEO_ERROR(fmt, arg...) DRM_ERROR(DRIVER_NAME ": " fmt, ##arg)

/** @todo does this make sense?  What exactly is this connector? */
#define VBOXVIDEOFB_CONN_LIMIT VBOX_VIDEO_MAX_SCREENS

/* vboxvideo_crtc.c */
void vboxvideo_crtc_init(struct drm_device *dev, int index);

/* vboxvideo_dac.c */
struct drm_encoder *vboxvideo_dac_init(struct drm_device *dev);

/* vboxvideo_device.c */
int vboxvideo_device_init(struct vboxvideo_device *gdev,
                          struct drm_device *ddev,
                          struct pci_dev *pdev,
                          uint32_t flags);
void vboxvideo_device_fini(struct vboxvideo_device *gdev);

/* vboxvideo_display.c */
int vboxvideo_modeset_init(struct vboxvideo_device *gdev);
void vboxvideo_modeset_fini(struct vboxvideo_device *gdev);

/* vboxvideo_kms.c */
int vboxvideo_driver_load(struct drm_device *dev, unsigned long flags);
int vboxvideo_driver_unload(struct drm_device *dev);
extern struct drm_ioctl_desc vboxvideo_ioctls[];
extern int vboxvideo_max_ioctl;

/* vboxvideo_vga.c */
struct drm_connector *vboxvideo_vga_init(struct drm_device *dev);

#define vboxvideo_PCI_IDS \
    {0x80ee, 0xbeef, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, \
    {0, 0, 0}

#endif  /* __DRM_VBOXVIDEO_DRV_H__ */
