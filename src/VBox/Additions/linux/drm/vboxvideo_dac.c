/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver, DAC functions
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
 * glint_dac.c
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

#include "drm/drm_crtc_helper.h"

static void vboxvideo_dac_dpms(struct drm_encoder *encoder, int mode)
{
    struct drm_device *dev = encoder->dev;
    struct vboxvideo_device *gdev = dev->dev_private;
    struct vboxvideo_encoder *vboxvideo_encoder = to_vboxvideo_encoder(encoder);

    if (mode == vboxvideo_encoder->last_dpms) /* Don't do unnecesary mode changes. */
        return;

    vboxvideo_encoder->last_dpms = mode;

    switch (mode) {
    case DRM_MODE_DPMS_STANDBY:
    case DRM_MODE_DPMS_SUSPEND:
    case DRM_MODE_DPMS_OFF:
        /* Do nothing for now */
        break;
    case DRM_MODE_DPMS_ON:
        /* Do nothing for now */
        break;
    }
}

static bool vboxvideo_dac_mode_fixup(struct drm_encoder *encoder,
                                     struct drm_display_mode *mode,
                                     struct drm_display_mode *adjusted_mode)
{
    return true;
}

static void vboxvideo_dac_mode_set(struct drm_encoder *encoder,
                                   struct drm_display_mode *mode,
                                   struct drm_display_mode *adjusted_mode)
{

}

static void vboxvideo_dac_prepare(struct drm_encoder *encoder)
{

}

static void vboxvideo_dac_commit(struct drm_encoder *encoder)
{

}

void vboxvideo_encoder_destroy(struct drm_encoder *encoder)
{
    struct vboxvideo_encoder *vboxvideo_encoder = to_vboxvideo_encoder(encoder);
    drm_encoder_cleanup(encoder);
    kfree(vboxvideo_encoder);
}

static const struct drm_encoder_helper_funcs vboxvideo_dac_helper_funcs = {
    .dpms = vboxvideo_dac_dpms,
    .mode_fixup = vboxvideo_dac_mode_fixup,
    .mode_set = vboxvideo_dac_mode_set,
    .prepare = vboxvideo_dac_prepare,
    .commit = vboxvideo_dac_commit,
};

static const struct drm_encoder_funcs vboxvideo_dac_encoder_funcs = {
    .destroy = vboxvideo_encoder_destroy,
};

struct drm_encoder *vboxvideo_dac_init(struct drm_device *dev)
{
    struct drm_encoder *encoder;
    struct vboxvideo_encoder *vboxvideo_encoder;

    vboxvideo_encoder = kzalloc(sizeof(struct vboxvideo_encoder), GFP_KERNEL);
    if (!vboxvideo_encoder)
        return NULL;

    vboxvideo_encoder->last_dpms = VBOXVIDEO_DPMS_CLEARED;
    encoder = &vboxvideo_encoder->base;
    encoder->possible_crtcs = 0x1;

    drm_encoder_init(dev, encoder, &vboxvideo_dac_encoder_funcs, DRM_MODE_ENCODER_DAC);
    drm_encoder_helper_add(encoder, &vboxvideo_dac_helper_funcs);

    return encoder;
}

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */
