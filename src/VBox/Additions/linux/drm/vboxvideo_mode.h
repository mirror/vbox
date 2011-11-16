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
 * glint_mode.h
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

#ifndef __DRM_VBOXVIDEO_MODE_H__
#define __DRM_VBOXVIDEO_MODE_H__

#include <VBox/Hardware/VBoxVideoVBE.h>
#include "drm/drmP.h"

#define VBOXVIDEO_MAX_FB_HEIGHT VBE_DISPI_MAX_YRES
#define VBOXVIDEO_MAX_FB_WIDTH  VBE_DISPI_MAX_XRES

#define to_vboxvideo_crtc(x)    container_of(x, struct vboxvideo_crtc, base)
#define to_vboxvideo_encoder(x) container_of(x, struct vboxvideo_encoder, base)

#define VBOXVIDEO_DPMS_CLEARED (-1)

struct vboxvideo_crtc
{
    struct drm_crtc   base;
    int               crtc_id;
    int               last_dpms;
    bool              enabled;
};

struct vboxvideo_mode_info
{
    bool                    mode_config_initialized;
    struct vboxvideo_crtc  *crtcs[VBOX_VIDEO_MAX_SCREENS];
};

struct vboxvideo_encoder
{
    struct drm_encoder base;
    int                last_dpms;
};

struct vboxvideo_connector {
    struct drm_connector  base;
};

#endif                /* __DRM_VBOXVIDEO_H__ */
