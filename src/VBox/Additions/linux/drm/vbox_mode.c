/* $Id$ */
/** @file
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
 * ast_mode.c
 * with the following copyright and permission notice:
 *
 * Copyright 2012 Red Hat Inc.
 * Parts based on xf86-video-ast
 * Copyright (c) 2005 ASPEED Technology Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 */
/*
 * Authors: Dave Airlie <airlied@redhat.com>
 */
#include "vbox_drv.h"

#include <VBox/VBoxVideo.h>

#include <linux/export.h>
#include <drm/drm_crtc_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
# include <drm/drm_plane_helper.h>
#endif

static int vbox_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
                            uint32_t handle, uint32_t width, uint32_t height,
                            int32_t hot_x, int32_t hot_y);
static int vbox_cursor_move(struct drm_crtc *crtc, int x, int y);

/** Set a graphics mode.  Poke any required values into registers, do an HGSMI
 * mode set and tell the host we support advanced graphics functions.
 */
static void vbox_do_modeset(struct drm_crtc *crtc,
                            const struct drm_display_mode *mode)
{
    struct vbox_crtc   *vbox_crtc = to_vbox_crtc(crtc);
    struct vbox_private *vbox;
    int width, height, bpp, pitch;
    unsigned crtc_id;
    uint16_t flags;

    LogFunc(("vboxvideo: %d: vbox_crtc=%p, CRTC_FB(crtc)=%p\n", __LINE__,
             vbox_crtc, CRTC_FB(crtc)));
    vbox = crtc->dev->dev_private;
    width = mode->hdisplay ? mode->hdisplay : 640;
    height = mode->vdisplay ? mode->vdisplay : 480;
    crtc_id = vbox_crtc->crtc_id;
    bpp = crtc->enabled ? CRTC_FB(crtc)->bits_per_pixel : 32;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
    pitch = crtc->enabled ? CRTC_FB(crtc)->pitch : width * bpp / 8;
#else
    pitch = crtc->enabled ? CRTC_FB(crtc)->pitches[0] : width * bpp / 8;
#endif
    /* This is the old way of setting graphics modes.  It assumed one screen
     * and a frame-buffer at the start of video RAM.  On older versions of
     * VirtualBox, certain parts of the code still assume that the first
     * screen is programmed this way, so try to fake it. */
    if (   vbox_crtc->crtc_id == 0
        && crtc->enabled
        && vbox_crtc->fb_offset / pitch < 0xffff - crtc->y
        && vbox_crtc->fb_offset % (bpp / 8) == 0)
        VBoxVideoSetModeRegisters(width, height, pitch * 8 / bpp,
                          CRTC_FB(crtc)->bits_per_pixel, 0,
                          vbox_crtc->fb_offset % pitch / bpp * 8 + crtc->x,
                          vbox_crtc->fb_offset / pitch + crtc->y);
    flags = VBVA_SCREEN_F_ACTIVE;
    flags |= (crtc->enabled && !vbox_crtc->blanked ? 0 : VBVA_SCREEN_F_BLANK);
    flags |= (vbox_crtc->disconnected ? VBVA_SCREEN_F_DISABLED : 0);
    VBoxHGSMIProcessDisplayInfo(&vbox->submit_info, vbox_crtc->crtc_id,
                                crtc->x, crtc->y,
                                crtc->x * bpp / 8 + crtc->y * pitch,
                                pitch, width, height,
                                vbox_crtc->blanked ? 0 : bpp, flags);
    VBoxHGSMIReportFlagsLocation(&vbox->submit_info, vbox->host_flags_offset);
    vbox_enable_caps(vbox);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}

static int vbox_set_view(struct drm_crtc *crtc)
{
    struct vbox_crtc   *vbox_crtc = to_vbox_crtc(crtc);
    struct vbox_private *vbox = crtc->dev->dev_private;
    void *p;

    LogFunc(("vboxvideo: %d: vbox_crtc=%p\n", __LINE__, vbox_crtc));
    /* Tell the host about the view.  This design originally targeted the
     * Windows XP driver architecture and assumed that each screen would have
     * a dedicated frame buffer with the command buffer following it, the whole
     * being a "view".  The host works out which screen a command buffer belongs
     * to by checking whether it is in the first view, then whether it is in the
     * second and so on.  The first match wins.  We cheat around this by making
     * the first view be the managed memory plus the first command buffer, the
     * second the same plus the second buffer and so on. */
    p = VBoxHGSMIBufferAlloc(&vbox->submit_info, sizeof(VBVAINFOVIEW), HGSMI_CH_VBVA,
                             VBVA_INFO_VIEW);
    if (p)
    {
        VBVAINFOVIEW *pInfo = (VBVAINFOVIEW *)p;
        pInfo->u32ViewIndex = vbox_crtc->crtc_id;
        pInfo->u32ViewOffset = vbox_crtc->fb_offset;
        pInfo->u32ViewSize =   vbox->vram_size - vbox_crtc->fb_offset
                             + vbox_crtc->crtc_id * VBVA_MIN_BUFFER_SIZE;
        pInfo->u32MaxScreenSize = vbox->vram_size - vbox_crtc->fb_offset;
        VBoxHGSMIBufferSubmit(&vbox->submit_info, p);
        VBoxHGSMIBufferFree(&vbox->submit_info, p);
    }
    else
        return -ENOMEM;
    LogFunc(("vboxvideo: %d: p=%p\n", __LINE__, p));
    return 0;
}

static void vbox_crtc_load_lut(struct drm_crtc *crtc)
{

}

static void vbox_crtc_dpms(struct drm_crtc *crtc, int mode)
{
    struct vbox_crtc *vbox_crtc = to_vbox_crtc(crtc);
    struct vbox_private *vbox = crtc->dev->dev_private;

    LogFunc(("vboxvideo: %d: vbox_crtc=%p, mode=%d\n", __LINE__, vbox_crtc,
             mode));
    switch (mode) {
    case DRM_MODE_DPMS_ON:
        vbox_crtc->blanked = false;
        break;
    case DRM_MODE_DPMS_STANDBY:
    case DRM_MODE_DPMS_SUSPEND:
    case DRM_MODE_DPMS_OFF:
        vbox_crtc->blanked = true;
        break;
    }
    mutex_lock(&vbox->hw_mutex);
    vbox_do_modeset(crtc, &crtc->hwmode);
    mutex_unlock(&vbox->hw_mutex);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}

static bool vbox_crtc_mode_fixup(struct drm_crtc *crtc,
                const struct drm_display_mode *mode,
                struct drm_display_mode *adjusted_mode)
{
    return true;
}

/* We move buffers which are not in active use out of VRAM to save memory. */
static int vbox_crtc_do_set_base(struct drm_crtc *crtc,
                struct drm_framebuffer *fb,
                int x, int y, int atomic)
{
    struct vbox_private *vbox = crtc->dev->dev_private;
    struct vbox_crtc *vbox_crtc = to_vbox_crtc(crtc);
    struct drm_gem_object *obj;
    struct vbox_framebuffer *vbox_fb;
    struct vbox_bo *bo;
    int ret;
    u64 gpu_addr;

    LogFunc(("vboxvideo: %d: fb=%p, vbox_crtc=%p\n", __LINE__, fb, vbox_crtc));
    /* push the previous fb to system ram */
    if (!atomic && fb) {
        vbox_fb = to_vbox_framebuffer(fb);
        obj = vbox_fb->obj;
        bo = gem_to_vbox_bo(obj);
        ret = vbox_bo_reserve(bo, false);
        if (ret)
            return ret;
        vbox_bo_push_sysram(bo);
        vbox_bo_unreserve(bo);
    }

    vbox_fb = to_vbox_framebuffer(CRTC_FB(crtc));
    obj = vbox_fb->obj;
    bo = gem_to_vbox_bo(obj);

    ret = vbox_bo_reserve(bo, false);
    if (ret)
        return ret;

    ret = vbox_bo_pin(bo, TTM_PL_FLAG_VRAM, &gpu_addr);
    if (ret) {
        vbox_bo_unreserve(bo);
        return ret;
    }

    if (&vbox->fbdev->afb == vbox_fb) {
        /* if pushing console in kmap it */
        ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &bo->kmap);
        if (ret)
            DRM_ERROR("failed to kmap fbcon\n");
        vbox_disable_accel(vbox);
        vbox_disable_caps(vbox);
    }
    else {
        vbox_enable_accel(vbox);
        vbox_enable_caps(vbox);
    }
    vbox_bo_unreserve(bo);

    /* vbox_set_start_address_crt1(crtc, (u32)gpu_addr); */
    vbox_crtc->fb_offset = gpu_addr;
    if (vbox_crtc->crtc_id == 0)
        VBoxHGSMIUpdateInputMapping(&vbox->submit_info, 0, 0,
                                    CRTC_FB(crtc)->width,
                                    CRTC_FB(crtc)->height);

    LogFunc(("vboxvideo: %d: vbox_fb=%p, obj=%p, bo=%p, gpu_addr=%u\n",
             __LINE__, vbox_fb, obj, bo, (unsigned)gpu_addr));
    return 0;
}

static int vbox_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
                 struct drm_framebuffer *old_fb)
{
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return vbox_crtc_do_set_base(crtc, old_fb, x, y, 0);
}

static int vbox_crtc_mode_set(struct drm_crtc *crtc,
                 struct drm_display_mode *mode,
                 struct drm_display_mode *adjusted_mode,
                 int x, int y,
                 struct drm_framebuffer *old_fb)
{
    struct vbox_private *vbox = crtc->dev->dev_private;
    int rc = 0;

    LogFunc(("vboxvideo: %d: vbox=%p\n", __LINE__, vbox));
    vbox_crtc_mode_set_base(crtc, x, y, old_fb);
    mutex_lock(&vbox->hw_mutex);
    rc = vbox_set_view(crtc);
    if (!rc)
        vbox_do_modeset(crtc, mode);
    mutex_unlock(&vbox->hw_mutex);
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return rc;
}

static void vbox_crtc_disable(struct drm_crtc *crtc)
{

}

static void vbox_crtc_prepare(struct drm_crtc *crtc)
{

}

static void vbox_crtc_commit(struct drm_crtc *crtc)
{

}


static const struct drm_crtc_helper_funcs vbox_crtc_helper_funcs = {
    .dpms = vbox_crtc_dpms,
    .mode_fixup = vbox_crtc_mode_fixup,
    .mode_set = vbox_crtc_mode_set,
    /* .mode_set_base = vbox_crtc_mode_set_base, */
    .disable = vbox_crtc_disable,
    .load_lut = vbox_crtc_load_lut,
    .prepare = vbox_crtc_prepare,
    .commit = vbox_crtc_commit,

};

static void vbox_crtc_reset(struct drm_crtc *crtc)
{

}


static void vbox_crtc_destroy(struct drm_crtc *crtc)
{
    drm_crtc_cleanup(crtc);
    kfree(crtc);
}

static const struct drm_crtc_funcs vbox_crtc_funcs = {
    .cursor_move = vbox_cursor_move,
#ifdef DRM_IOCTL_MODE_CURSOR2
    .cursor_set2 = vbox_cursor_set2,
#endif
    .reset = vbox_crtc_reset,
    .set_config = drm_crtc_helper_set_config,
    /* .gamma_set = vbox_crtc_gamma_set, */
    .destroy = vbox_crtc_destroy,
};

static struct vbox_crtc *vbox_crtc_init(struct drm_device *dev, unsigned i)
{
    struct vbox_crtc *vbox_crtc;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    vbox_crtc = kzalloc(sizeof(struct vbox_crtc), GFP_KERNEL);
    if (!vbox_crtc)
        return NULL;
    vbox_crtc->crtc_id = i;

    drm_crtc_init(dev, &vbox_crtc->base, &vbox_crtc_funcs);
    drm_mode_crtc_set_gamma_size(&vbox_crtc->base, 256);
    drm_crtc_helper_add(&vbox_crtc->base, &vbox_crtc_helper_funcs);
    LogFunc(("vboxvideo: %d: crtc=%p\n", __LINE__, vbox_crtc));

    return vbox_crtc;
}

static void vbox_encoder_destroy(struct drm_encoder *encoder)
{
    LogFunc(("vboxvideo: %d: encoder=%p\n", __LINE__, encoder));
    drm_encoder_cleanup(encoder);
    kfree(encoder);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
static struct drm_encoder *drm_encoder_find(struct drm_device *dev, uint32_t id)
{
     struct drm_mode_object *mo;
     mo = drm_mode_object_find(dev, id, DRM_MODE_OBJECT_ENCODER);
     return mo ? obj_to_encoder(mo) : NULL;
}
#endif

static struct drm_encoder *vbox_best_single_encoder(struct drm_connector *connector)
{
    int enc_id = connector->encoder_ids[0];

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    /* pick the encoder ids */
    if (enc_id)
        return drm_encoder_find(connector->dev, enc_id);
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return NULL;
}


static const struct drm_encoder_funcs vbox_enc_funcs = {
    .destroy = vbox_encoder_destroy,
};

static void vbox_encoder_dpms(struct drm_encoder *encoder, int mode)
{

}

static bool vbox_mode_fixup(struct drm_encoder *encoder,
               const struct drm_display_mode *mode,
               struct drm_display_mode *adjusted_mode)
{
    return true;
}

static void vbox_encoder_mode_set(struct drm_encoder *encoder,
                   struct drm_display_mode *mode,
                   struct drm_display_mode *adjusted_mode)
{
}

static void vbox_encoder_prepare(struct drm_encoder *encoder)
{

}

static void vbox_encoder_commit(struct drm_encoder *encoder)
{

}


static const struct drm_encoder_helper_funcs vbox_enc_helper_funcs = {
    .dpms = vbox_encoder_dpms,
    .mode_fixup = vbox_mode_fixup,
    .prepare = vbox_encoder_prepare,
    .commit = vbox_encoder_commit,
    .mode_set = vbox_encoder_mode_set,
};

static struct drm_encoder *vbox_encoder_init(struct drm_device *dev, unsigned i)
{
    struct vbox_encoder *vbox_encoder;

    LogFunc(("vboxvideo: %d: dev=%d\n", __LINE__));
    vbox_encoder = kzalloc(sizeof(struct vbox_encoder), GFP_KERNEL);
    if (!vbox_encoder)
        return NULL;

    drm_encoder_init(dev, &vbox_encoder->base, &vbox_enc_funcs,
                     DRM_MODE_ENCODER_DAC
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
                     , NULL
#endif
                     );
    drm_encoder_helper_add(&vbox_encoder->base, &vbox_enc_helper_funcs);

    vbox_encoder->base.possible_crtcs = 1 << i;
    LogFunc(("vboxvideo: %d: vbox_encoder=%p\n", __LINE__, vbox_encoder));
    return &vbox_encoder->base;
}

static int vbox_get_modes(struct drm_connector *connector)
{
    struct vbox_connector *vbox_connector = NULL;
    struct drm_display_mode *mode = NULL;
    struct vbox_private *vbox = NULL;
    unsigned num_modes = 0;
    int preferred_width, preferred_height;

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    vbox_connector = to_vbox_connector(connector);
    vbox = connector->dev->dev_private;
    if (!vbox->fbdev_init)
        return drm_add_modes_noedid(connector, 800, 600);
    num_modes = drm_add_modes_noedid(connector, 2560, 1600);
    preferred_width = vbox_connector->mode_hint.width ? vbox_connector->mode_hint.width : 1024;
    preferred_height = vbox_connector->mode_hint.height ? vbox_connector->mode_hint.height : 768;
    mode = drm_cvt_mode(connector->dev, preferred_width, preferred_height, 60, false,
                         false, false);
    if (mode)
    {
        mode->type |= DRM_MODE_TYPE_PREFERRED;
        drm_mode_probed_add(connector, mode);
        ++num_modes;
    }
    return num_modes;
}

static int vbox_mode_valid(struct drm_connector *connector,
              struct drm_display_mode *mode)
{
    return MODE_OK;
}

static void vbox_connector_destroy(struct drm_connector *connector)
{
    struct vbox_connector *vbox_connector = NULL;

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    vbox_connector = to_vbox_connector(connector);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
    drm_sysfs_connector_remove(connector);
#else
    drm_connector_unregister(connector);
#endif
    drm_connector_cleanup(connector);
    kfree(connector);
}

static enum drm_connector_status
vbox_connector_detect(struct drm_connector *connector, bool force)
{
    struct vbox_connector *vbox_connector = NULL;

    (void) force;
    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    vbox_connector = to_vbox_connector(connector);
    return vbox_connector->mode_hint.disconnected ?
                connector_status_disconnected : connector_status_connected;
}

static int vbox_fill_modes(struct drm_connector *connector, uint32_t max_x, uint32_t max_y)
{
    struct vbox_connector *vbox_connector;
    struct drm_device *dev;
    struct drm_display_mode *mode, *iterator;

    LogFunc(("vboxvideo: %d: connector=%p, max_x=%lu, max_y = %lu\n", __LINE__,
             connector, (unsigned long)max_x, (unsigned long)max_y));
    vbox_connector = to_vbox_connector(connector);
    dev = vbox_connector->base.dev;
    list_for_each_entry_safe(mode, iterator, &connector->modes, head)
    {
        list_del(&mode->head);
        drm_mode_destroy(dev, mode);
    }
    return drm_helper_probe_single_connector_modes(connector, max_x, max_y);
}

static const struct drm_connector_helper_funcs vbox_connector_helper_funcs = {
    .mode_valid = vbox_mode_valid,
    .get_modes = vbox_get_modes,
    .best_encoder = vbox_best_single_encoder,
};

static const struct drm_connector_funcs vbox_connector_funcs = {
    .dpms = drm_helper_connector_dpms,
    .detect = vbox_connector_detect,
    .fill_modes = vbox_fill_modes,
    .destroy = vbox_connector_destroy,
};

static int vbox_connector_init(struct drm_device *dev,
                               struct vbox_crtc *vbox_crtc,
                               struct drm_encoder *encoder)
{
    struct vbox_connector *vbox_connector;
    struct drm_connector *connector;

    LogFunc(("vboxvideo: %d: dev=%p, encoder=%p\n", __LINE__, dev,
             encoder));
    vbox_connector = kzalloc(sizeof(struct vbox_connector), GFP_KERNEL);
    if (!vbox_connector)
        return -ENOMEM;

    connector = &vbox_connector->base;
    vbox_connector->vbox_crtc = vbox_crtc;

    drm_connector_init(dev, connector, &vbox_connector_funcs,
                       DRM_MODE_CONNECTOR_VGA);
    drm_connector_helper_add(connector, &vbox_connector_helper_funcs);

    connector->interlace_allowed = 0;
    connector->doublescan_allowed = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
    drm_mode_create_suggested_offset_properties(dev);
    drm_object_attach_property(&connector->base,
                               dev->mode_config.suggested_x_property, 0);
    drm_object_attach_property(&connector->base,
                               dev->mode_config.suggested_y_property, 0);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
    drm_sysfs_connector_add(connector);
#else
    drm_connector_register(connector);
#endif

    drm_mode_connector_attach_encoder(connector, encoder);

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    return 0;
}

int vbox_mode_init(struct drm_device *dev)
{
    struct vbox_private *vbox = dev->dev_private;
    struct drm_encoder *encoder;
    struct vbox_crtc *vbox_crtc;
    unsigned i;
    /* vbox_cursor_init(dev); */
    LogFunc(("vboxvideo: %d: dev=%p\n", __LINE__, dev));
    for (i = 0; i < vbox->num_crtcs; ++i)
    {
        vbox_crtc = vbox_crtc_init(dev, i);
        if (!vbox_crtc)
            return -ENOMEM;
        encoder = vbox_encoder_init(dev, i);
        if (!encoder)
            return -ENOMEM;
        vbox_connector_init(dev, vbox_crtc, encoder);
    }
    return 0;
}

void vbox_mode_fini(struct drm_device *dev)
{
    /* vbox_cursor_fini(dev); */
}


void vbox_refresh_modes(struct drm_device *dev)
{
    struct vbox_private *vbox = dev->dev_private;
    struct drm_crtc *crtci;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    mutex_lock(&vbox->hw_mutex);
    list_for_each_entry(crtci, &dev->mode_config.crtc_list, head)
        vbox_do_modeset(crtci, &crtci->hwmode);
    mutex_unlock(&vbox->hw_mutex);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}


/** Copy the ARGB image and generate the mask, which is needed in case the host
 *  does not support ARGB cursors.  The mask is a 1BPP bitmap with the bit set
 *  if the corresponding alpha value in the ARGB image is greater than 0xF0. */
static void copy_cursor_image(u8 *src, u8 *dst, int width, int height,
                              size_t mask_size)
{
    unsigned i, j;
    size_t line_size = (width + 7) / 8;

    memcpy(dst + mask_size, src, width * height * 4);
    for (i = 0; i < height; ++i)
        for (j = 0; j < width; ++j)
            if (((uint32_t *)src)[i * width + j] > 0xf0000000)
                dst[i * line_size + j / 8] |= (0x80 >> (j % 8));
}

static int vbox_cursor_set2(struct drm_crtc *crtc, struct drm_file *file_priv,
                            uint32_t handle, uint32_t width, uint32_t height,
                            int32_t hot_x, int32_t hot_y)
{
    struct vbox_private *vbox = crtc->dev->dev_private;
    struct vbox_crtc *vbox_crtc = to_vbox_crtc(crtc);
    struct drm_gem_object *obj;
    struct vbox_bo *bo;
    int ret, rc;
    struct ttm_bo_kmap_obj uobj_map;
    u8 *src;
    u8 *dst = NULL;
    u32 caps = 0;
    size_t data_size, mask_size;
    bool src_isiomem;

    if (!handle) {
        bool cursor_enabled = false;
        struct drm_crtc *crtci;

        /* Hide cursor. */
        vbox_crtc->cursor_enabled = false;
        list_for_each_entry(crtci, &vbox->dev->mode_config.crtc_list, head)
            if (to_vbox_crtc(crtci)->cursor_enabled)
                cursor_enabled = true;
        if (!cursor_enabled)
            VBoxHGSMIUpdatePointerShape(&vbox->submit_info, 0, 0, 0, 0, 0, NULL, 0);
        return 0;
    }
    vbox_crtc->cursor_enabled = true;
    if (   width > VBOX_MAX_CURSOR_WIDTH || height > VBOX_MAX_CURSOR_HEIGHT
        || width == 0 || hot_x > width || height == 0 || hot_y > height)
        return -EINVAL;
    rc = VBoxQueryConfHGSMI(&vbox->submit_info,
                            VBOX_VBVA_CONF32_CURSOR_CAPABILITIES, &caps);
    ret = -RTErrConvertToErrno(rc);
    if (ret)
        return ret;
    if (   caps & VMMDEV_MOUSE_HOST_CANNOT_HWPOINTER
        || !(caps & VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE))
        return -EINVAL;

    obj = drm_gem_object_lookup(crtc->dev, file_priv, handle);
    if (obj)
    {
        bo = gem_to_vbox_bo(obj);
        ret = vbox_bo_reserve(bo, false);
        if (!ret)
        {
            /* The mask must be calculated based on the alpha channel, one bit
             * per ARGB word, and must be 32-bit padded. */
            mask_size  = ((width + 7) / 8 * height + 3) & ~3;
            data_size = width * height * 4 + mask_size;
            dst = kmalloc(data_size, GFP_KERNEL);
            if (dst)
            {
                ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &uobj_map);
                if (!ret)
                {
                    src = ttm_kmap_obj_virtual(&uobj_map, &src_isiomem);
                    if (!src_isiomem)
                    {
                        uint32_t flags =   VBOX_MOUSE_POINTER_VISIBLE
                                          | VBOX_MOUSE_POINTER_SHAPE
                                          | VBOX_MOUSE_POINTER_ALPHA;
                        copy_cursor_image(src, dst, width, height, mask_size);
                        rc = VBoxHGSMIUpdatePointerShape(&vbox->submit_info, flags,
                                                         hot_x, hot_y, width,
                                                         height, dst, data_size);
                        ret = -RTErrConvertToErrno(rc);
                    }
                    else
                        DRM_ERROR("src cursor bo should be in main memory\n");
                    ttm_bo_kunmap(&uobj_map);
                }
                kfree(dst);
            }
            vbox_bo_unreserve(bo);
        }
        drm_gem_object_unreference_unlocked(obj);
    }
    else
    {
        DRM_ERROR("Cannot find cursor object %x for crtc\n", handle);
        ret = -ENOENT;
    }
    return ret;
}

static int vbox_cursor_move(struct drm_crtc *crtc,
               int x, int y)
{
    struct vbox_private *vbox = crtc->dev->dev_private;

    VBoxHGSMICursorPosition(&vbox->submit_info, true, x + crtc->x,
                            y + crtc->y, NULL, NULL);
    return 0;
}
