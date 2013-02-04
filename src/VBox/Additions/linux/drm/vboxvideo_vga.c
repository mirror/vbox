/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver, VGA functions
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
 * glint_vga.c
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

static int vboxvideo_vga_get_modes(struct drm_connector *connector)
{
    /* return 0 modes, so that we don't have to implement DDC/I2C yet. */
    return 0;
}

static int vboxvideo_vga_mode_valid(struct drm_connector *connector,
                                    struct drm_display_mode *mode)
{
    /* XXX check mode bandwidth */
    /* XXX verify against max DAC output frequency */
    return MODE_OK;
}

struct drm_encoder *vboxvideo_connector_best_encoder(struct drm_connector
                                                           *connector)
{
    int enc_id = connector->encoder_ids[0];
    struct drm_mode_object *obj;
    struct drm_encoder *encoder;

    /* pick the encoder ids */
    if (enc_id) {
        obj = drm_mode_object_find(connector->dev, enc_id, DRM_MODE_OBJECT_ENCODER);
        if (!obj)
            return NULL;
        encoder = obj_to_encoder(obj);
        return encoder;
    }
    return NULL;
}

static enum drm_connector_status vboxvideo_vga_detect(struct drm_connector
                                                            *connector)
{
    return connector_status_connected;
}

static void vboxvideo_connector_destroy(struct drm_connector *connector)
{
    drm_connector_cleanup(connector);
    kfree(connector);
}

struct drm_connector_helper_funcs vboxvideo_vga_connector_helper_funcs =
{
    .get_modes    = vboxvideo_vga_get_modes,
    .mode_valid   = vboxvideo_vga_mode_valid,
    .best_encoder = vboxvideo_connector_best_encoder,
};

struct drm_connector_funcs vboxvideo_vga_connector_funcs =
{
    .dpms       = drm_helper_connector_dpms,
    .detect     = vboxvideo_vga_detect,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy    = vboxvideo_connector_destroy,
};

struct drm_connector *vboxvideo_vga_init(struct drm_device *dev)
{
    struct drm_connector *connector;
    struct vboxvideo_connector *vboxvideo_connector;

    vboxvideo_connector = kzalloc(sizeof(struct vboxvideo_connector),
                                  GFP_KERNEL);
    if (!vboxvideo_connector)
        return NULL;

    connector = &vboxvideo_connector->base;

    drm_connector_init(dev, connector,
        &vboxvideo_vga_connector_funcs, DRM_MODE_CONNECTOR_VGA);

    drm_connector_helper_add(connector, &vboxvideo_vga_connector_helper_funcs);

    return connector;
}

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */
