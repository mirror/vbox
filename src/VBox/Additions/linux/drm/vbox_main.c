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
 * ast_main.c
 * with the following copyright and permission notice:
 *
 * Copyright 2012 Red Hat Inc.
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

#include <VBox/VBoxVideoGuest.h>
#include <VBox/VBoxVideo.h>

#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>

static void vbox_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
    struct vbox_framebuffer *vbox_fb = to_vbox_framebuffer(fb);
    if (vbox_fb->obj)
        drm_gem_object_unreference_unlocked(vbox_fb->obj);

    LogFunc(("vboxvideo: %d: vbox_fb=%p, vbox_fb->obj=%p\n", __LINE__,
             vbox_fb, vbox_fb->obj));
    drm_framebuffer_cleanup(fb);
    kfree(fb);
}

/** Send information about dirty rectangles to VBVA.  If necessary we enable
 * VBVA first, as this is normally disabled after a mode set in case a user
 * takes over the console that is not aware of VBVA (i.e. the VESA BIOS). */
void vbox_framebuffer_dirty_rectangles(struct drm_framebuffer *fb,
                                       struct drm_clip_rect *rects,
                                       unsigned num_rects)
{
    struct vbox_private *vbox = fb->dev->dev_private;
    unsigned i;
    unsigned long flags;

    LogFunc(("vboxvideo: %d: fb=%p, num_rects=%u, vbox=%p\n", __LINE__, fb,
             num_rects, vbox));
    spin_lock_irqsave(&vbox->dev_lock, flags);
    for (i = 0; i < num_rects; ++i)
    {
        struct drm_crtc *crtc;
        list_for_each_entry(crtc, &fb->dev->mode_config.crtc_list, head)
        {
            unsigned crtc_id = to_vbox_crtc(crtc)->crtc_id;
            struct VBVABUFFER *pVBVA = vbox->vbva_info[crtc_id].pVBVA;
            VBVACMDHDR cmdHdr;

            if (!pVBVA)
            {
                LogFunc(("vboxvideo: enabling VBVA.\n"));
                pVBVA = (struct VBVABUFFER *) (  ((uint8_t *)vbox->vram)
                                               + vbox->vram_size
                                               + crtc_id * VBVA_MIN_BUFFER_SIZE);
                if (!VBoxVBVAEnable(&vbox->vbva_info[crtc_id], &vbox->submit_info, pVBVA, crtc_id))
                    AssertReleaseMsgFailed(("VBoxVBVAEnable failed - heap allocation error, very old host or driver error.\n"));
                /* Assume that if the user knows to send dirty rectangle information
                 * they can also handle hot-plug events. */
                VBoxHGSMISendCapsInfo(&vbox->submit_info, VBVACAPS_VIDEO_MODE_HINTS | VBVACAPS_DISABLE_CURSOR_INTEGRATION);
            }
            if (   CRTC_FB(crtc) != fb
                || rects[i].x1 >   crtc->x
                                  + crtc->hwmode.hdisplay
                || rects[i].y1 >   crtc->y
                                  + crtc->hwmode.vdisplay
                || rects[i].x2 < crtc->x
                || rects[i].y2 < crtc->y)
                continue;
            cmdHdr.x = (int16_t)rects[i].x1;
            cmdHdr.y = (int16_t)rects[i].y1;
            cmdHdr.w = (uint16_t)rects[i].x2 - rects[i].x1;
            cmdHdr.h = (uint16_t)rects[i].y2 - rects[i].y1;
            if (VBoxVBVABufferBeginUpdate(&vbox->vbva_info[crtc_id],
                                          &vbox->submit_info))
            {
                VBoxVBVAWrite(&vbox->vbva_info[crtc_id], &vbox->submit_info, &cmdHdr,
                              sizeof(cmdHdr));
                VBoxVBVABufferEndUpdate(&vbox->vbva_info[crtc_id]);
            }
        }
    }
    spin_unlock_irqrestore(&vbox->dev_lock, flags);
    LogFunc(("vboxvideo: %d\n", __LINE__));
}

static int vbox_user_framebuffer_dirty(struct drm_framebuffer *fb,
                                       struct drm_file *file_priv,
                                       unsigned flags, unsigned color,
                                       struct drm_clip_rect *rects,
                                       unsigned num_rects)
{
    LogFunc(("vboxvideo: %d, flags=%u\n", __LINE__, flags));
    vbox_framebuffer_dirty_rectangles(fb, rects, num_rects);
    return 0;
}

static const struct drm_framebuffer_funcs vbox_fb_funcs = {
    .destroy = vbox_user_framebuffer_destroy,
    .dirty = vbox_user_framebuffer_dirty,
};


int vbox_framebuffer_init(struct drm_device *dev,
             struct vbox_framebuffer *vbox_fb,
             struct DRM_MODE_FB_CMD *mode_cmd,
             struct drm_gem_object *obj)
{
    int ret;

    LogFunc(("vboxvideo: %d: dev=%p, vbox_fb=%p, obj=%p\n", __LINE__, dev,
             vbox_fb, obj));
    drm_helper_mode_fill_fb_struct(&vbox_fb->base, mode_cmd);
    vbox_fb->obj = obj;
    ret = drm_framebuffer_init(dev, &vbox_fb->base, &vbox_fb_funcs);
    if (ret) {
        DRM_ERROR("framebuffer init failed %d\n", ret);
        LogFunc(("vboxvideo: %d\n", __LINE__));
        return ret;
    }
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return 0;
}

static struct drm_framebuffer *
vbox_user_framebuffer_create(struct drm_device *dev,
           struct drm_file *filp,
           struct drm_mode_fb_cmd2 *mode_cmd)
{
    struct drm_gem_object *obj;
    struct vbox_framebuffer *vbox_fb;
    int ret;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    obj = drm_gem_object_lookup(dev, filp, mode_cmd->handles[0]);
    if (obj == NULL)
        return ERR_PTR(-ENOENT);

    vbox_fb = kzalloc(sizeof(*vbox_fb), GFP_KERNEL);
    if (!vbox_fb) {
        drm_gem_object_unreference_unlocked(obj);
        return ERR_PTR(-ENOMEM);
    }

    ret = vbox_framebuffer_init(dev, vbox_fb, mode_cmd, obj);
    if (ret) {
        drm_gem_object_unreference_unlocked(obj);
        kfree(vbox_fb);
        return ERR_PTR(ret);
    }
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return &vbox_fb->base;
}

static const struct drm_mode_config_funcs vbox_mode_funcs = {
    .fb_create = vbox_user_framebuffer_create,
};

static void disableVBVA(struct vbox_private *pVBox)
{
    unsigned i;

    if (pVBox->vbva_info)
    {
        for (i = 0; i < pVBox->num_crtcs; ++i)
            VBoxVBVADisable(&pVBox->vbva_info[i], &pVBox->submit_info, i);
        kfree(pVBox->vbva_info);
        pVBox->vbva_info = NULL;
    }
}

static int vbox_vbva_init(struct vbox_private *vbox)
{
    unsigned i;
    bool fRC = true;
    LogFunc(("vboxvideo: %d: vbox=%p, vbox->num_crtcs=%u, vbox->vbva_info=%p\n",
             __LINE__, vbox, (unsigned)vbox->num_crtcs, vbox->vbva_info));
    if (!vbox->vbva_info)
    {
        vbox->vbva_info = kzalloc(  sizeof(struct VBVABUFFERCONTEXT)
                                  * vbox->num_crtcs,
                                  GFP_KERNEL);
        if (!vbox->vbva_info)
            return -ENOMEM;
    }
    /* Take a command buffer for each screen from the end of usable VRAM. */
    vbox->vram_size -= vbox->num_crtcs * VBVA_MIN_BUFFER_SIZE;
    for (i = 0; i < vbox->num_crtcs; ++i)
        VBoxVBVASetupBufferContext(&vbox->vbva_info[i],
                                   vbox->vram_size + i * VBVA_MIN_BUFFER_SIZE,
                                   VBVA_MIN_BUFFER_SIZE);
    LogFunc(("vboxvideo: %d: vbox->vbva_info=%p, vbox->vram_size=%u\n",
             __LINE__, vbox->vbva_info, (unsigned)vbox->vram_size));
    return 0;
}


/** Allocation function for the HGSMI heap and data. */
static DECLCALLBACK(void *) hgsmiEnvAlloc(void *pvEnv, HGSMISIZE cb)
{
    NOREF(pvEnv);
    return kmalloc(cb, GFP_KERNEL);
}


/** Free function for the HGSMI heap and data. */
static DECLCALLBACK(void) hgsmiEnvFree(void *pvEnv, void *pv)
{
    NOREF(pvEnv);
    kfree(pv);
}


/** Pointers to the HGSMI heap and data manipulation functions. */
static HGSMIENV g_hgsmiEnv =
{
    NULL,
    hgsmiEnvAlloc,
    hgsmiEnvFree
};


/** Do we support the 4.3 plus mode hint reporting interface? */
static bool haveHGSMImode_hintAndCursorReportingInterface(struct vbox_private *pVBox)
{
    uint32_t fmode_hintReporting, fCursorReporting;

    return    RT_SUCCESS(VBoxQueryConfHGSMI(&pVBox->submit_info, VBOX_VBVA_CONF32_MODE_HINT_REPORTING, &fmode_hintReporting))
           && RT_SUCCESS(VBoxQueryConfHGSMI(&pVBox->submit_info, VBOX_VBVA_CONF32_GUEST_CURSOR_REPORTING, &fCursorReporting))
           && fmode_hintReporting == VINF_SUCCESS
           && fCursorReporting == VINF_SUCCESS;
}


/** Set up our heaps and data exchange buffers in VRAM before handing the rest
 *  to the memory manager. */
static int setupAcceleration(struct vbox_private *pVBox)
{
    uint32_t offBase, offGuestHeap, cbGuestHeap, host_flags_offset;
    void *pvGuestHeap;

    VBoxHGSMIGetBaseMappingInfo(pVBox->full_vram_size, &offBase, NULL,
                                &offGuestHeap, &cbGuestHeap, &host_flags_offset);
    pvGuestHeap =   ((uint8_t *)pVBox->vram) + offBase + offGuestHeap;
    pVBox->host_flags_offset = offBase + host_flags_offset;
    if (RT_FAILURE(VBoxHGSMISetupGuestContext(&pVBox->submit_info, pvGuestHeap,
                                              cbGuestHeap,
                                              offBase + offGuestHeap,
                                              &g_hgsmiEnv)))
        return -ENOMEM;
    /* Reduce available VRAM size to reflect the guest heap. */
    pVBox->vram_size = offBase;
    /* Linux drm represents monitors as a 32-bit array. */
    pVBox->num_crtcs = RT_MIN(VBoxHGSMIGetMonitorCount(&pVBox->submit_info), 32);
    if (!haveHGSMImode_hintAndCursorReportingInterface(pVBox))
        return -ENOTSUPP;
    pVBox->last_mode_hints = kzalloc(sizeof(VBVAMODEHINT) * pVBox->num_crtcs, GFP_KERNEL);
    if (!pVBox->last_mode_hints)
        return -ENOMEM;
    return vbox_vbva_init(pVBox);
}


int vbox_driver_load(struct drm_device *dev, unsigned long flags)
{
    struct vbox_private *vbox;
    int ret = 0;

    LogFunc(("vboxvideo: %d: dev=%p\n", __LINE__, dev));
    if (!VBoxHGSMIIsSupported())
        return -ENODEV;
    vbox = kzalloc(sizeof(struct vbox_private), GFP_KERNEL);
    if (!vbox)
        return -ENOMEM;

    dev->dev_private = vbox;
    vbox->dev = dev;

    spin_lock_init(&vbox->dev_lock);
    /* I hope this won't interfere with the memory manager. */
    vbox->vram = pci_iomap(dev->pdev, 0, 0);
    if (!vbox->vram) {
        ret = -EIO;
        goto out_free;
    }
    vbox->full_vram_size = VBoxVideoGetVRAMSize();
    vbox->any_pitch = VBoxVideoAnyWidthAllowed();
    DRM_INFO("VRAM %08x\n", vbox->full_vram_size);

    ret = setupAcceleration(vbox);
    if (ret)
        goto out_free;

    ret = vbox_mm_init(vbox);
    if (ret)
        goto out_free;

    drm_mode_config_init(dev);

    dev->mode_config.funcs = (void *)&vbox_mode_funcs;
    dev->mode_config.min_width = 64;
    dev->mode_config.min_height = 64;
    dev->mode_config.preferred_depth = 24;
    dev->mode_config.max_width = VBE_DISPI_MAX_XRES;
    dev->mode_config.max_height = VBE_DISPI_MAX_YRES;

    ret = vbox_mode_init(dev);
    if (ret)
        goto out_free;

    ret = vbox_fbdev_init(dev);
    if (ret)
        goto out_free;
    LogFunc(("vboxvideo: %d: vbox=%p, vbox->vram=%p, vbox->full_vram_size=%u\n",
             __LINE__, vbox, vbox->vram, (unsigned)vbox->full_vram_size));
    return 0;
out_free:
    if (vbox->vram)
        pci_iounmap(dev->pdev, vbox->vram);
    kfree(vbox);
    dev->dev_private = NULL;
    LogFunc(("vboxvideo: %d: ret=%d\n", __LINE__, ret));
    return ret;
}

int vbox_driver_unload(struct drm_device *dev)
{
    struct vbox_private *vbox = dev->dev_private;

    LogFunc(("vboxvideo: %d\n", __LINE__));
    vbox_mode_fini(dev);
    vbox_fbdev_fini(dev);
    drm_mode_config_cleanup(dev);

    disableVBVA(vbox);
    vbox_mm_fini(vbox);
    pci_iounmap(dev->pdev, vbox->vram);
    kfree(vbox);
    LogFunc(("vboxvideo: %d\n", __LINE__));
    return 0;
}

int vbox_gem_create(struct drm_device *dev,
           u32 size, bool iskernel,
           struct drm_gem_object **obj)
{
    struct vbox_bo *vboxbo;
    int ret;

    LogFunc(("vboxvideo: %d: dev=%p, size=%u, iskernel=%u\n", __LINE__,
             dev, (unsigned)size, (unsigned)iskernel));
    *obj = NULL;

    size = roundup(size, PAGE_SIZE);
    if (size == 0)
        return -EINVAL;

    ret = vbox_bo_create(dev, size, 0, 0, &vboxbo);
    if (ret) {
        if (ret != -ERESTARTSYS)
            DRM_ERROR("failed to allocate GEM object\n");
        return ret;
    }
    *obj = &vboxbo->gem;
    LogFunc(("vboxvideo: %d: obj=%p\n", __LINE__, obj));
    return 0;
}

int vbox_dumb_create(struct drm_file *file,
            struct drm_device *dev,
            struct drm_mode_create_dumb *args)
{
    int ret;
    struct drm_gem_object *gobj;
    u32 handle;

    LogFunc(("vboxvideo: %d: args->width=%u, args->height=%u, args->bpp=%u\n",
             __LINE__, (unsigned)args->width, (unsigned)args->height,
             (unsigned)args->bpp));
    args->pitch = args->width * ((args->bpp + 7) / 8);
    args->size = args->pitch * args->height;

    ret = vbox_gem_create(dev, args->size, false,
                 &gobj);
    if (ret)
        return ret;

    ret = drm_gem_handle_create(file, gobj, &handle);
    drm_gem_object_unreference_unlocked(gobj);
    if (ret)
        return ret;

    args->handle = handle;
    LogFunc(("vboxvideo: %d: args->handle=%u\n", __LINE__,
             (unsigned)args->handle));
    return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
int vbox_dumb_destroy(struct drm_file *file,
             struct drm_device *dev,
             uint32_t handle)
{
    LogFunc(("vboxvideo: %d: dev=%p, handle=%u\n", __LINE__, dev,
             (unsigned)handle));
    return drm_gem_handle_delete(file, handle);
}
#endif

static void vbox_bo_unref(struct vbox_bo **bo)
{
    struct ttm_buffer_object *tbo;

    if ((*bo) == NULL)
        return;

    LogFunc(("vboxvideo: %d: bo=%p\n", __LINE__, bo));
    tbo = &((*bo)->bo);
    ttm_bo_unref(&tbo);
    if (tbo == NULL)
        *bo = NULL;

}
void vbox_gem_free_object(struct drm_gem_object *obj)
{
    struct vbox_bo *vbox_bo = gem_to_vbox_bo(obj);

    LogFunc(("vboxvideo: %d: vbox_bo=%p\n", __LINE__, vbox_bo));
    vbox_bo_unref(&vbox_bo);
}


static inline u64 vbox_bo_mmap_offset(struct vbox_bo *bo)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
    return bo->bo.addr_space_offset;
#else
    return drm_vma_node_offset_addr(&bo->bo.vma_node);
#endif
}
int
vbox_dumb_mmap_offset(struct drm_file *file,
             struct drm_device *dev,
             uint32_t handle,
             uint64_t *offset)
{
    struct drm_gem_object *obj;
    int ret;
    struct vbox_bo *bo;

    LogFunc(("vboxvideo: %d: dev=%p, handle=%u\n", __LINE__,
             dev, (unsigned)handle));
    mutex_lock(&dev->struct_mutex);
    obj = drm_gem_object_lookup(dev, file, handle);
    if (obj == NULL) {
        ret = -ENOENT;
        goto out_unlock;
    }

    bo = gem_to_vbox_bo(obj);
    *offset = vbox_bo_mmap_offset(bo);

    drm_gem_object_unreference(obj);
    ret = 0;
out_unlock:
    mutex_unlock(&dev->struct_mutex);
    LogFunc(("vboxvideo: %d: bo=%p, offset=%llu\n", __LINE__,
             bo, (unsigned long long)*offset));
    return ret;

}
