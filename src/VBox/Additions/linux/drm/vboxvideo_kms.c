/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver, KMS support
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
 * glint_kms.c
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

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)

#include "vboxvideo_drv.h"

int vboxvideo_driver_load(struct drm_device * dev, unsigned long flags)
{
    struct vboxvideo_device *gdev;
    int r;

    gdev = kzalloc(sizeof(struct vboxvideo_device), GFP_KERNEL);
    if (gdev == NULL) {
        return -ENOMEM;
    }
    dev->dev_private = (void *)gdev;

    r = vboxvideo_device_init(gdev, dev, dev->pdev, flags);
    if (r) {
        dev_err(&dev->pdev->dev, "Fatal error during GPU init\n");
        goto out;
    }

    r = vboxvideo_modeset_init(gdev);
    if (r) {
        dev_err(&dev->pdev->dev, "Fatal error during modeset init\n");
        goto out;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
    r = drm_vblank_init(dev, 1);
    if (r)
        dev_err(&dev->pdev->dev, "Fatal error during vblank init\n");
#endif
out:
    if (r)
        vboxvideo_driver_unload(dev);
    return r;
}

int vboxvideo_driver_unload(struct drm_device * dev)
{
    struct vboxvideo_device *gdev = dev->dev_private;

    if (gdev == NULL)
        return 0;
    vboxvideo_modeset_fini(gdev);
    vboxvideo_device_fini(gdev);
    kfree(gdev);
    dev->dev_private = NULL;
    return 0;
}

struct drm_ioctl_desc vboxvideo_ioctls[] = {
};
int vboxvideo_max_ioctl = DRM_ARRAY_SIZE(vboxvideo_ioctls);

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */
