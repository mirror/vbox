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
 * ast_drv.h
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
#ifndef __VBOX_DRV_H__
#define __VBOX_DRV_H__

#include <VBox/VBoxVideoGuest.h>

#include <iprt/log.h>

#include "the-linux-kernel.h"

#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>

#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_memory.h>
#include <drm/ttm/ttm_module.h>

/* #include "vboxvideo.h" */

#include "product-generated.h"

#define DRIVER_AUTHOR       VBOX_VENDOR

#define DRIVER_NAME         "vboxvideo"
#define DRIVER_DESC         VBOX_PRODUCT " Graphics Card"
#define DRIVER_DATE         "20130823"

#define DRIVER_MAJOR        1
#define DRIVER_MINOR        0
#define DRIVER_PATCHLEVEL   0

#define VBOX_MAX_CURSOR_WIDTH  64
#define VBOX_MAX_CURSOR_HEIGHT 64

struct vbox_fbdev;

struct vbox_private
{
    struct drm_device *dev;

    void __iomem *vram;
    HGSMIGUESTCOMMANDCONTEXT Ctx;
    struct VBVABUFFERCONTEXT *paVBVACtx;
    bool fAnyX;
    unsigned cCrtcs;
    bool vga2_clone;
    /** Amount of available VRAM, including space used for buffers. */
    uint32_t full_vram_size;
    /** Amount of available VRAM, not including space used for buffers. */
    uint32_t vram_size;
    /** Is HGSMI currently disabled? */
    bool fDisableHGSMI;

    struct vbox_fbdev *fbdev;

    int fb_mtrr;

    struct
    {
        struct drm_global_reference mem_global_ref;
        struct ttm_bo_global_ref bo_global_ref;
        struct ttm_bo_device bdev;
    } ttm;

    spinlock_t dev_lock;
};

int vbox_driver_load(struct drm_device *dev, unsigned long flags);
int vbox_driver_unload(struct drm_device *dev);

struct vbox_gem_object;

struct vbox_connector
{
    struct drm_connector base;
    char szName[32];
    /** Device attribute for sysfs file used for receiving mode hints from user
     * space. */
    struct device_attribute deviceAttribute;
    struct
    {
        uint16_t cX;
        uint16_t cY;
    } modeHint;
};

struct vbox_crtc
{
    struct drm_crtc base;
    bool fBlanked;
    unsigned crtc_id;
    uint32_t offFB;
    struct drm_gem_object *cursor_bo;
    uint64_t cursor_addr;
    int cursor_width, cursor_height;
    u8 offset_x, offset_y;
};

struct vbox_encoder
{
    struct drm_encoder base;
};

struct vbox_framebuffer
{
    struct drm_framebuffer base;
    struct drm_gem_object *obj;
};

struct vbox_fbdev
{
    struct drm_fb_helper helper;
    struct vbox_framebuffer afb;
    struct list_head fbdev_list;
    void *sysram;
    int size;
    struct ttm_bo_kmap_obj mapping;
    int x1, y1, x2, y2; /* dirty rect */
};

#define to_vbox_crtc(x) container_of(x, struct vbox_crtc, base)
#define to_vbox_connector(x) container_of(x, struct vbox_connector, base)
#define to_vbox_encoder(x) container_of(x, struct vbox_encoder, base)
#define to_vbox_framebuffer(x) container_of(x, struct vbox_framebuffer, base)

extern int vbox_mode_init(struct drm_device *dev);
extern void vbox_mode_fini(struct drm_device *dev);
extern void VBoxRefreshModes(struct drm_device *pDev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
# define DRM_MODE_FB_CMD drm_mode_fb_cmd
#else
# define DRM_MODE_FB_CMD drm_mode_fb_cmd2
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
# define CRTC_FB(crtc) (crtc)->fb
#else
# define CRTC_FB(crtc) (crtc)->primary->fb
#endif

void vbox_framebuffer_dirty_rectangles(struct drm_framebuffer *fb,
                                       struct drm_clip_rect *pRects,
                                       unsigned cRects);

int vbox_framebuffer_init(struct drm_device *dev,
             struct vbox_framebuffer *vbox_fb,
             struct DRM_MODE_FB_CMD *mode_cmd,
             struct drm_gem_object *obj);

int vbox_fbdev_init(struct drm_device *dev);
void vbox_fbdev_fini(struct drm_device *dev);
void vbox_fbdev_set_suspend(struct drm_device *dev, int state);

struct vbox_bo
{
    struct ttm_buffer_object bo;
    struct ttm_placement placement;
    struct ttm_bo_kmap_obj kmap;
    struct drm_gem_object gem;
    u32 placements[3];
    int pin_count;
};
#define gem_to_vbox_bo(gobj) container_of((gobj), struct vbox_bo, gem)

static inline struct vbox_bo * vbox_bo(struct ttm_buffer_object *bo)
{
    return container_of(bo, struct vbox_bo, bo);
}


#define to_vbox_obj(x) container_of(x, struct vbox_gem_object, base)

extern int vbox_dumb_create(struct drm_file *file,
               struct drm_device *dev,
               struct drm_mode_create_dumb *args);
extern int vbox_dumb_destroy(struct drm_file *file,
                struct drm_device *dev,
                uint32_t handle);

extern void vbox_gem_free_object(struct drm_gem_object *obj);
extern int vbox_dumb_mmap_offset(struct drm_file *file,
                struct drm_device *dev,
                uint32_t handle,
                uint64_t *offset);

#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

int vbox_mm_init(struct vbox_private *vbox);
void vbox_mm_fini(struct vbox_private *vbox);

int vbox_bo_create(struct drm_device *dev, int size, int align,
          uint32_t flags, struct vbox_bo **pvboxbo);

int vbox_gem_create(struct drm_device *dev,
           u32 size, bool iskernel,
           struct drm_gem_object **obj);

int vbox_bo_pin(struct vbox_bo *bo, u32 pl_flag, u64 *gpu_addr);
int vbox_bo_unpin(struct vbox_bo *bo);

int vbox_bo_reserve(struct vbox_bo *bo, bool no_wait);
void vbox_bo_unreserve(struct vbox_bo *bo);
void vbox_ttm_placement(struct vbox_bo *bo, int domain);
int vbox_bo_push_sysram(struct vbox_bo *bo);
int vbox_mmap(struct file *filp, struct vm_area_struct *vma);

/* vbox post */
void vbox_post_gpu(struct drm_device *dev);
#endif
