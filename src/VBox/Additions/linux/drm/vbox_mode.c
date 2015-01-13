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
 * ast_mode.c
 * with the following copyright and permission notice:
 *
 * Copyright 2012 Red Hat Inc.
 * Parts based on xf86-video-vbox
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
    int width, height, cBPP, pitch;
    unsigned iCrtc;
    uint16_t fFlags;

    LogFunc(("vboxvideo: %d: vbox_crtc=%p, CRTC_FB(crtc)=%p\n", __LINE__,
             vbox_crtc, CRTC_FB(crtc)));
    vbox = crtc->dev->dev_private;
    width = mode->hdisplay ? mode->hdisplay : 640;
    height = mode->vdisplay ? mode->vdisplay : 480;
    iCrtc = vbox_crtc->crtc_id;
    cBPP = crtc->enabled ? CRTC_FB(crtc)->bits_per_pixel : 32;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
    pitch = crtc->enabled ? CRTC_FB(crtc)->pitch : width * cBPP / 8;
#else
    pitch = crtc->enabled ? CRTC_FB(crtc)->pitches[0] : width * cBPP / 8;
#endif
    /* if (vbox_crtc->crtc_id == 0 && crtc->enabled)
        VBoxVideoSetModeRegisters(width, height, pitch * 8 / cBPP,
                                  CRTC_FB(crtc)->bits_per_pixel, 0,
                                  crtc->x, crtc->y); */
    fFlags = VBVA_SCREEN_F_ACTIVE;
    fFlags |= (crtc->enabled ? 0 : VBVA_SCREEN_F_DISABLED);
    VBoxHGSMIProcessDisplayInfo(&vbox->Ctx, vbox_crtc->crtc_id,
                                crtc->x, crtc->y,
                                crtc->x * cBPP / 8 + crtc->y * pitch,
                                pitch, width, height,
                                vbox_crtc->fBlanked ? 0 : cBPP, fFlags);
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
    p = VBoxHGSMIBufferAlloc(&vbox->Ctx, sizeof(VBVAINFOVIEW), HGSMI_CH_VBVA,
                             VBVA_INFO_VIEW);
    if (p)
    {
        VBVAINFOVIEW *pInfo = (VBVAINFOVIEW *)p;
        pInfo->u32ViewIndex = vbox_crtc->crtc_id;
        pInfo->u32ViewOffset = vbox_crtc->offFB;
        pInfo->u32ViewSize =   vbox->vram_size - vbox_crtc->offFB
                             + vbox_crtc->crtc_id * VBVA_MIN_BUFFER_SIZE;
        pInfo->u32MaxScreenSize = vbox->vram_size - vbox_crtc->offFB;
        VBoxHGSMIBufferSubmit(&vbox->Ctx, p);
        VBoxHGSMIBufferFree(&vbox->Ctx, p);
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
    unsigned long flags;

    LogFunc(("vboxvideo: %d: vbox_crtc=%p, mode=%d\n", __LINE__, vbox_crtc,
             mode));
    switch (mode)
    {
    case DRM_MODE_DPMS_ON:
        vbox_crtc->fBlanked = false;
        break;
    case DRM_MODE_DPMS_STANDBY:
    case DRM_MODE_DPMS_SUSPEND:
    case DRM_MODE_DPMS_OFF:
        vbox_crtc->fBlanked = true;
        break;
    }
    spin_lock_irqsave(&vbox->dev_lock, flags);
    vbox_do_modeset(crtc, &crtc->hwmode);
    spin_unlock_irqrestore(&vbox->dev_lock, flags);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}

static bool vbox_crtc_mode_fixup(struct drm_crtc *crtc,
                const struct drm_display_mode *mode,
                struct drm_display_mode *adjusted_mode)
{
    return true;
}

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

    vbox_fb = to_vbox_framebuffer(CRTC_FB(crtc));
    obj = vbox_fb->obj;
    bo = gem_to_vbox_bo(obj);

    ret = vbox_bo_reserve(bo, false);
    if (ret)
        return ret;

    ret = vbox_bo_pin(bo, TTM_PL_FLAG_VRAM, &gpu_addr);
    if (ret)
    {
        vbox_bo_unreserve(bo);
        return ret;
    }

    if (&vbox->fbdev->afb == vbox_fb)
    {
        /* if pushing console in kmap it */
        ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &bo->kmap);
        if (ret)
            DRM_ERROR("failed to kmap fbcon\n");
    }
    vbox_bo_unreserve(bo);

    /* vbox_set_start_address_crt1(crtc, (u32)gpu_addr); */
    vbox_crtc->offFB = gpu_addr;

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
    unsigned long flags;
    int rc = 0;

    LogFunc(("vboxvideo: %d: vbox=%p\n", __LINE__, vbox));
    vbox_crtc_mode_set_base(crtc, x, y, old_fb);
    spin_lock_irqsave(&vbox->dev_lock, flags);
    rc = vbox_set_view(crtc);
    if (!rc)
        vbox_do_modeset(crtc, mode);
    spin_unlock_irqrestore(&vbox->dev_lock, flags);
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


static const struct drm_crtc_helper_funcs vbox_crtc_helper_funcs =
{
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

static const struct drm_crtc_funcs vbox_crtc_funcs =
{
    .cursor_move = vbox_cursor_move,
#ifdef DRM_IOCTL_MODE_CURSOR2
    .cursor_set2 = vbox_cursor_set2,
#endif
    .reset = vbox_crtc_reset,
    .set_config = drm_crtc_helper_set_config,
    /* .gamma_set = vbox_crtc_gamma_set, */
    .destroy = vbox_crtc_destroy,
};

int vbox_crtc_init(struct drm_device *pDev, unsigned i)
{
    struct vbox_crtc *pCrtc;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    pCrtc = kzalloc(sizeof(struct vbox_crtc), GFP_KERNEL);
    if (!pCrtc)
        return -ENOMEM;
    pCrtc->crtc_id = i;

    drm_crtc_init(pDev, &pCrtc->base, &vbox_crtc_funcs);
    drm_mode_crtc_set_gamma_size(&pCrtc->base, 256);
    drm_crtc_helper_add(&pCrtc->base, &vbox_crtc_helper_funcs);
    LogFunc(("vboxvideo: %d: pCrtc=%p\n", __LINE__, pCrtc));

    return 0;
}


static void vbox_encoder_destroy(struct drm_encoder *encoder)
{
    LogFunc(("vboxvideo: %d: encoder=%p\n", __LINE__, encoder));
    drm_encoder_cleanup(encoder);
    kfree(encoder);
}


static struct drm_encoder *vbox_best_single_encoder(struct drm_connector *connector)
{
    int enc_id = connector->encoder_ids[0];
    struct drm_mode_object *obj;
    struct drm_encoder *encoder;

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, connector));
    /* pick the encoder ids */
    if (enc_id)
    {
        obj = drm_mode_object_find(connector->dev, enc_id, DRM_MODE_OBJECT_ENCODER);
        if (!obj)
            return NULL;
        encoder = obj_to_encoder(obj);
        LogFunc(("vboxvideo: %d: encoder=%p\n", __LINE__, encoder));
        return encoder;
    }
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return NULL;
}


static const struct drm_encoder_funcs vbox_enc_funcs =
{
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


static const struct drm_encoder_helper_funcs vbox_enc_helper_funcs =
{
    .dpms = vbox_encoder_dpms,
    .mode_fixup = vbox_mode_fixup,
    .prepare = vbox_encoder_prepare,
    .commit = vbox_encoder_commit,
    .mode_set = vbox_encoder_mode_set,
};

struct drm_encoder *vbox_encoder_init(struct drm_device *dev, unsigned i)
{
    struct vbox_encoder *vbox_encoder;

    LogFunc(("vboxvideo: %d: dev=%d\n", __LINE__));
    vbox_encoder = kzalloc(sizeof(struct vbox_encoder), GFP_KERNEL);
    if (!vbox_encoder)
        return NULL;

    drm_encoder_init(dev, &vbox_encoder->base, &vbox_enc_funcs,
             DRM_MODE_ENCODER_DAC);
    drm_encoder_helper_add(&vbox_encoder->base, &vbox_enc_helper_funcs);

    vbox_encoder->base.possible_crtcs = 1 << i;
    LogFunc(("vboxvideo: %d: vbox_encoder=%p\n", __LINE__, vbox_encoder));
    return &vbox_encoder->base;
}

static int vbox_get_modes(struct drm_connector *pConnector)
{
    struct vbox_connector *pVBoxConnector = NULL;
    struct drm_display_mode *pMode = NULL;
    unsigned cModes = 0;

    LogFunc(("vboxvideo: %d: pConnector=%p\n", __LINE__, pConnector));
    pVBoxConnector = to_vbox_connector(pConnector);
    cModes = drm_add_modes_noedid(pConnector, 1024, 768);
    if (pVBoxConnector->modeHint.cX && pVBoxConnector->modeHint.cY)
    {
        pMode = drm_cvt_mode(pConnector->dev, pVBoxConnector->modeHint.cX,
                             pVBoxConnector->modeHint.cY, 60, false, false,
                             false);
        if (pMode)
        {
            pMode->type |= DRM_MODE_TYPE_PREFERRED;
            drm_mode_probed_add(pConnector, pMode);
            ++cModes;
        }
    }
    return cModes;
}

static int vbox_mode_valid(struct drm_connector *connector,
              struct drm_display_mode *mode)
{
    return MODE_OK;
}

static void vbox_connector_destroy(struct drm_connector *pConnector)
{
    struct vbox_connector *pVBoxConnector = NULL;

    LogFunc(("vboxvideo: %d: connector=%p\n", __LINE__, pConnector));
    pVBoxConnector = to_vbox_connector(pConnector);
    device_remove_file(pConnector->dev->dev, &pVBoxConnector->deviceAttribute);
    drm_sysfs_connector_remove(pConnector);
    drm_connector_cleanup(pConnector);
    kfree(pConnector);
}

static enum drm_connector_status
vbox_connector_detect(struct drm_connector *connector, bool force)
{
    return connector_status_connected;
}

static const struct drm_connector_helper_funcs vbox_connector_helper_funcs =
{
    .mode_valid = vbox_mode_valid,
    .get_modes = vbox_get_modes,
    .best_encoder = vbox_best_single_encoder,
};

static const struct drm_connector_funcs vbox_connector_funcs =
{
    .dpms = drm_helper_connector_dpms,
    .detect = vbox_connector_detect,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = vbox_connector_destroy,
};

ssize_t vbox_connector_write_sysfs(struct device *pDev,
                                   struct device_attribute *pAttr,
                                   const char *psz, size_t cch)
{
    struct vbox_connector *pVBoxConnector;
    struct drm_device *pDrmDev;
    struct vbox_private *pVBox;
    int cX, cY;
    char ch;

    LogFunc(("vboxvideo: %d: pDev=%p, pAttr=%p, psz=%s, cch=%llu\n", __LINE__,
             pDev, pAttr, psz, (unsigned long long)cch));
    pVBoxConnector = container_of(pAttr, struct vbox_connector,
                                  deviceAttribute);
    pDrmDev = pVBoxConnector->base.dev;
    pVBox = pDrmDev->dev_private;
    if (sscanf(psz, "%dx%d\n%c", &cX, &cY, &ch) != 2)
        return -EINVAL;
    if (   cX < 64 || cX > VBE_DISPI_MAX_XRES
        || cY < 64 || cY > VBE_DISPI_MAX_YRES)
        return -EINVAL;
    pVBoxConnector->modeHint.cX = (uint16_t)cX;
    pVBoxConnector->modeHint.cY = (uint16_t)cY;
    drm_helper_hpd_irq_event(pVBoxConnector->base.dev);
    if (pVBox->fbdev)
        drm_fb_helper_hotplug_event(&pVBox->fbdev->helper);
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return cch;
}

int vbox_connector_init(struct drm_device *pDev, unsigned cScreen,
                        struct drm_encoder *pEncoder)
{
    struct vbox_connector *pVBoxConnector;
    struct drm_connector *pConnector;
    int rc;

    LogFunc(("vboxvideo: %d: pDev=%p, pEncoder=%p\n", __LINE__, pDev,
             pEncoder));
    pVBoxConnector = kzalloc(sizeof(struct vbox_connector), GFP_KERNEL);
    if (!pVBoxConnector)
        return -ENOMEM;

    pConnector = &pVBoxConnector->base;

    /*
     * Set up the sysfs file we use for getting video mode hints from user
     * space.
     */
    snprintf(pVBoxConnector->szName, sizeof(pVBoxConnector->szName),
             "vbox_screen_%u", cScreen);
    pVBoxConnector->deviceAttribute.attr.name = pVBoxConnector->szName;
    pVBoxConnector->deviceAttribute.attr.mode = S_IWUSR;
    pVBoxConnector->deviceAttribute.show      = NULL;
    pVBoxConnector->deviceAttribute.store     = vbox_connector_write_sysfs;
    rc = device_create_file(pDev->dev, &pVBoxConnector->deviceAttribute);
    if (rc < 0)
    {
        kfree(pVBoxConnector);
        return rc;
    }
    drm_connector_init(pDev, pConnector, &vbox_connector_funcs,
                       DRM_MODE_CONNECTOR_VGA);
    drm_connector_helper_add(pConnector, &vbox_connector_helper_funcs);

    pConnector->interlace_allowed = 0;
    pConnector->doublescan_allowed = 0;

    drm_sysfs_connector_add(pConnector);

    /* The connector supports hot-plug detection: we promise to call
     * "drm_helper_hpd_irq_event" when hot-plugging occurs. */
    pConnector->polled = DRM_CONNECTOR_POLL_HPD;

    drm_mode_connector_attach_encoder(pConnector, pEncoder);

    LogFunc(("vboxvideo: %d: pConnector=%p\n", __LINE__, pConnector));
    return 0;
}

#if 0
/* allocate cursor cache and pin at start of VRAM */
int vbox_cursor_init(struct drm_device *dev)
{
    struct vbox_private *vbox = dev->dev_private;
    int size;
    int ret;
    struct drm_gem_object *obj;
    struct vbox_bo *bo;
    uint64_t gpu_addr;

    size = (AST_HWC_SIZE + AST_HWC_SIGNATURE_SIZE) * AST_DEFAULT_HWC_NUM;

    ret = vbox_gem_create(dev, size, true, &obj);
    if (ret)
        return ret;
    bo = gem_to_vbox_bo(obj);
    ret = vbox_bo_reserve(bo, false);
    if (unlikely(ret != 0))
        goto fail;

    ret = vbox_bo_pin(bo, TTM_PL_FLAG_VRAM, &gpu_addr);
    vbox_bo_unreserve(bo);
    if (ret)
        goto fail;

    /* kmap the object */
    ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &vbox->cache_kmap);
    if (ret)
        goto fail;

    vbox->cursor_cache = obj;
    vbox->cursor_cache_gpu_addr = gpu_addr;
    DRM_DEBUG_KMS("pinned cursor cache at %llx\n", vbox->cursor_cache_gpu_addr);
    return 0;
fail:
    return ret;
}

void vbox_cursor_fini(struct drm_device *dev)
{
    struct vbox_private *vbox = dev->dev_private;
    ttm_bo_kunmap(&vbox->cache_kmap);
    drm_gem_object_unreference_unlocked(vbox->cursor_cache);
}
#endif

int vbox_mode_init(struct drm_device *pDev)
{
    struct vbox_private *pVBox = pDev->dev_private;
    struct drm_encoder *pEncoder;
    unsigned i;
    /* vbox_cursor_init(dev); */
    LogFunc(("vboxvideo: %d: pDev=%p\n", __LINE__, pDev));
    for (i = 0; i < pVBox->cCrtcs; ++i)
    {
        vbox_crtc_init(pDev, i);
        pEncoder = vbox_encoder_init(pDev, i);
        if (pEncoder)
            vbox_connector_init(pDev, i, pEncoder);
    }
    return 0;
}

void vbox_mode_fini(struct drm_device *dev)
{
    /* vbox_cursor_fini(dev); */
}


void VBoxRefreshModes(struct drm_device *pDev)
{
    struct vbox_private *vbox = pDev->dev_private;
    struct drm_crtc *crtci;
    unsigned long flags;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    spin_lock_irqsave(&vbox->dev_lock, flags);
    list_for_each_entry(crtci, &pDev->mode_config.crtc_list, head)
        vbox_do_modeset(crtci, &crtci->hwmode);
    spin_unlock_irqrestore(&vbox->dev_lock, flags);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}


/** Copy the ARGB image and generate the mask, which is needed in case the host
 *  does not support ARGB cursors.  The mask is a 1BPP bitmap with the bit set
 *  if the corresponding alpha value in the ARGB image is greater than 0xF0. */
static void copy_cursor_image(u8 *src, u8 *dst, int width, int height,
                              size_t cbMask)
{
    unsigned i, j;
    size_t cbLine = (width + 7) / 8;

    memcpy(dst + cbMask, src, width * height * 4);
    for (i = 0; i < height; ++i)
        for (j = 0; j < width; ++j)
            if (((uint32_t *)src)[i * width + j] > 0xf0000000)
                dst[i * cbLine + j / 8] |= (0x80 >> (j % 8));
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
    size_t cbData, cbMask;
    bool src_isiomem;

    if (!handle) {
        /* Hide cursor. */
        VBoxHGSMIUpdatePointerShape(&vbox->Ctx, 0, 0, 0, 0, 0, NULL, 0);
        return 0;
    }
    if (   width > VBOX_MAX_CURSOR_WIDTH || height > VBOX_MAX_CURSOR_HEIGHT
        || width == 0 || hot_x > width || height == 0 || hot_y > height)
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
            cbMask  = ((width + 7) / 8 * height + 3) & ~3;
            cbData = width * height * 4 + cbMask;
            dst = kmalloc(cbData, GFP_KERNEL);
            if (dst)
            {
                ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &uobj_map);
                if (!ret)
                {
                    src = ttm_kmap_obj_virtual(&uobj_map, &src_isiomem);
                    if (!src_isiomem)
                    {
                        uint32_t fFlags =   VBOX_MOUSE_POINTER_VISIBLE
                                          | VBOX_MOUSE_POINTER_SHAPE
                                          | VBOX_MOUSE_POINTER_ALPHA;
                        copy_cursor_image(src, dst, width, height, cbMask);
                        rc = VBoxHGSMIUpdatePointerShape(&vbox->Ctx, fFlags,
                                                         hot_x, hot_y, width,
                                                         height, dst, cbData);
                        ret = RTErrConvertToErrno(rc);
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
    return 0;
}
