/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
 * glint_framebuffer.c
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
#include "drm/drmP.h"
#include "drm/drm.h"
#include "drm/drm_crtc_helper.h"

#include "vboxvideo.h"
#include "vboxvideo_drv.h"

static void vboxvideo_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
    drm_framebuffer_cleanup(fb);
}

static int vboxvideo_user_framebuffer_create_handle(struct drm_framebuffer *fb,
                                                    struct drm_file *file_priv,
                                                    unsigned int *handle)
{
    return 0;
}

static const struct drm_framebuffer_funcs vboxvideo_fb_funcs = {
    .destroy = vboxvideo_user_framebuffer_destroy,
    .create_handle = vboxvideo_user_framebuffer_create_handle,
};

int vboxvideo_framebuffer_init(struct drm_device *dev,
                               struct vboxvideo_framebuffer *gfb,
                               struct DRM_MODE_FB_CMD *mode_cmd)
{
    int ret = drm_framebuffer_init(dev, &gfb->base, &vboxvideo_fb_funcs);
    if (ret) {
        VBOXVIDEO_ERROR("drm_framebuffer_init failed: %d\n", ret);
        return ret;
    }
    drm_helper_mode_fill_fb_struct(&gfb->base, mode_cmd);

    return 0;
}
