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
 * glint_fbdev.c
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
#include "drm/drm_fb_helper.h"

#include <linux/fb.h>

#include "vboxvideo.h"
#include "vboxvideo_drv.h"

struct vboxvideo_fbdev {
    struct drm_fb_helper helper;
    struct vboxvideo_framebuffer gfb;
    struct list_head fbdev_list;
    struct vboxvideo_device *gdev;
};

static struct fb_ops vboxvideofb_ops = {
    .owner = THIS_MODULE,
    .fb_check_var = drm_fb_helper_check_var,
    .fb_set_par = drm_fb_helper_set_par,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
    .fb_pan_display = drm_fb_helper_pan_display,
    .fb_blank = drm_fb_helper_blank,
    .fb_setcmap = drm_fb_helper_setcmap,
};

static int vboxvideofb_create(struct vboxvideo_fbdev *gfbdev,
              struct drm_fb_helper_surface_size *sizes)
{
    struct vboxvideo_device *gdev = gfbdev->gdev;
    struct fb_info *info;
    struct drm_framebuffer *fb;
    struct drm_map_list *r_list, *list_t;
    struct drm_local_map *map = NULL;
    struct DRM_MODE_FB_CMD mode_cmd;
    struct device *device = &gdev->pdev->dev;
    __u32 pitch;
    unsigned int fb_pitch;
    int ret;

    mode_cmd.width = sizes->surface_width;
    mode_cmd.height = sizes->surface_height;
    pitch = mode_cmd.width * ((sizes->surface_bpp + 7) / 8);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
    mode_cmd.bpp = sizes->surface_bpp;
    mode_cmd.depth = sizes->surface_depth;
    mode_cmd.pitch = mode_cmd.width * ((mode_cmd.bpp + 7) / 8);
#else
    mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
                                                      sizes->surface_depth);
    mode_cmd.pitches[0] = mode_cmd.width * ((sizes->surface_bpp + 7) / 8);
#endif

    info = framebuffer_alloc(0, device);
    if (info == NULL)
        return -ENOMEM;

    info->par = gfbdev;

    ret = vboxvideo_framebuffer_init(gdev->ddev, &gfbdev->gfb, &mode_cmd);
    if (ret)
        return ret;

    fb = &gfbdev->gfb.base;
    if (!fb) {
        VBOXVIDEO_INFO("fb is NULL\n");
    }

    /* setup helper */
    gfbdev->helper.fb = fb;
    gfbdev->helper.fbdev = info;

    strcpy(info->fix.id, "vboxvideodrmfb");

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
    fb_pitch = fb->pitch;
#else
    fb_pitch = fb->pitches[0];
#endif
    drm_fb_helper_fill_fix(info, fb_pitch, fb->depth);

    info->flags = FBINFO_DEFAULT;
    info->fbops = &vboxvideofb_ops;

    drm_fb_helper_fill_var(info, &gfbdev->helper, sizes->fb_width, sizes->fb_height);

    /* setup aperture base/size for vesafb takeover */
    info->apertures = alloc_apertures(1);
    if (!info->apertures) {
        ret = -ENOMEM;
        goto out_iounmap;
    }
    info->apertures->ranges[0].base = gdev->ddev->mode_config.fb_base;
    info->apertures->ranges[0].size = gdev->mc.vram_size;

    list_for_each_entry_safe(r_list, list_t, &gdev->ddev->maplist, head) {
        map = r_list->map;
        if (map->type == _DRM_FRAME_BUFFER) {
            map->handle = ioremap_nocache(map->offset, map->size);
            if (!map->handle) {
                VBOXVIDEO_ERROR("fb: can't remap framebuffer\n");
                return -1;
            }
            break;
        }
    }

    info->fix.smem_start = map->offset;
    info->fix.smem_len = map->size;
    if (!info->fix.smem_len) {
        VBOXVIDEO_ERROR("%s: can't count memory\n", info->fix.id);
        goto out_iounmap;
    }
    info->screen_base = map->handle;
    if (!info->screen_base) {
        VBOXVIDEO_ERROR("%s: can't remap framebuffer\n", info->fix.id);
        goto out_iounmap;
    }

    info->fix.mmio_start = 0;
    info->fix.mmio_len = 0;

    ret = fb_alloc_cmap(&info->cmap, 256, 0);
    if (ret) {
        VBOXVIDEO_ERROR("%s: can't allocate color map\n", info->fix.id);
        ret = -ENOMEM;
        goto out_iounmap;
    }

    DRM_INFO("fb mappable at 0x%lX\n",  info->fix.smem_start);
    DRM_INFO("vram aper at 0x%lX\n",  (unsigned long)info->fix.smem_start);
    DRM_INFO("size %lu\n", (unsigned long)info->fix.smem_len);
    DRM_INFO("fb depth is %d\n", fb->depth);
    DRM_INFO("   pitch is %d\n", fb_pitch);

    return 0;
out_iounmap:
    iounmap(map->handle);
    return ret;
}

static int vboxvideo_fb_find_or_create_single(struct drm_fb_helper *helper,
                                              struct drm_fb_helper_surface_size *sizes)
{
    struct vboxvideo_fbdev *gfbdev = (struct vboxvideo_fbdev *)helper;
    int new_fb = 0;
    int ret;

    if (!helper->fb) {
        ret = vboxvideofb_create(gfbdev, sizes);
        if (ret)
            return ret;
        new_fb = 1;
    }
    return new_fb;
}

static int vboxvideo_fbdev_destroy(struct drm_device *dev, struct vboxvideo_fbdev *gfbdev)
{
    struct fb_info *info;
    struct vboxvideo_framebuffer *gfb = &gfbdev->gfb;

    if (gfbdev->helper.fbdev) {
        info = gfbdev->helper.fbdev;

        unregister_framebuffer(info);
        if (info->cmap.len)
            fb_dealloc_cmap(&info->cmap);
        framebuffer_release(info);
    }

    drm_fb_helper_fini(&gfbdev->helper);
    drm_framebuffer_cleanup(&gfb->base);

    return 0;
}

static struct drm_fb_helper_funcs vboxvideo_fb_helper_funcs = {
    .gamma_set = vboxvideo_crtc_fb_gamma_set,
    .gamma_get = vboxvideo_crtc_fb_gamma_get,
    .fb_probe = vboxvideo_fb_find_or_create_single,
};

int vboxvideo_fbdev_init(struct vboxvideo_device *gdev)
{
    struct vboxvideo_fbdev *gfbdev;
    int bpp_sel = 32;
    int ret;

    gfbdev = kzalloc(sizeof(struct vboxvideo_fbdev), GFP_KERNEL);
    if (!gfbdev)
        return -ENOMEM;

    gfbdev->gdev = gdev;
    gdev->mode_info.gfbdev = gfbdev;
    gfbdev->helper.funcs = &vboxvideo_fb_helper_funcs;

    ret = drm_fb_helper_init(gdev->ddev, &gfbdev->helper,
                             gdev->num_crtc,
                             VBOXVIDEOFB_CONN_LIMIT);
    if (ret) {
        kfree(gfbdev);
        return ret;
    }
    drm_fb_helper_single_add_all_connectors(&gfbdev->helper);
    drm_fb_helper_initial_config(&gfbdev->helper, bpp_sel);

    return 0;
}

void vboxvideo_fbdev_fini(struct vboxvideo_device *gdev)
{
    if (!gdev->mode_info.gfbdev)
        return;

    vboxvideo_fbdev_destroy(gdev->ddev, gdev->mode_info.gfbdev);
    kfree(gdev->mode_info.gfbdev);
    gdev->mode_info.gfbdev = NULL;
}
