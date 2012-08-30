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
 * vboxvideo_device.c
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)

#include "vboxvideo_drv.h"

int vboxvideo_modeset_init(struct vboxvideo_device *gdev)
{
    struct drm_encoder *encoder;
    struct drm_connector *connector;
    int i;

    drm_mode_config_init(gdev->ddev);
    gdev->mode_info.mode_config_initialized = true;

    gdev->ddev->mode_config.max_width = VBOXVIDEO_MAX_FB_WIDTH;
    gdev->ddev->mode_config.max_height = VBOXVIDEO_MAX_FB_HEIGHT;

    gdev->ddev->mode_config.fb_base = gdev->mc.aper_base;

    /* allocate crtcs */
    for (i = 0; i < gdev->num_crtc; i++)
    {
        vboxvideo_crtc_init(gdev->ddev, i);
    }

    encoder = vboxvideo_dac_init(gdev->ddev);
    if (!encoder) {
        VBOXVIDEO_ERROR("vboxvideo_dac_init failed\n");
        return -1;
    }

    connector = vboxvideo_vga_init(gdev->ddev);
    if (!connector) {
        VBOXVIDEO_ERROR("vboxvideo_vga_init failed\n");
        return -1;
    }
    return 0;
}

void vboxvideo_modeset_fini(struct vboxvideo_device *gdev)
{
    if (gdev->mode_info.mode_config_initialized)
    {
        drm_mode_config_cleanup(gdev->ddev);
        gdev->mode_info.mode_config_initialized = false;
    }
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */
