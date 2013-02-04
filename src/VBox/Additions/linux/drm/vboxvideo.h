/** @file $Id$
 *
 * VirtualBox Additions Linux kernel video driver
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
 * glint.h
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

#ifndef __DRM_VBOXVIDEO_H__
#define __DRM_VBOXVIDEO_H__

#include "vboxvideo_mode.h"
#include "drm/drmP.h"

struct vboxvideo_mc
{
    resource_size_t    aper_size;
    resource_size_t    aper_base;
    u32                vram_size;
};

struct vboxvideo_device
{
    struct device      *dev;
    struct drm_device  *ddev;
    struct pci_dev     *pdev;
    unsigned long       flags;
    /** @todo move this into flags */
    bool                fAnyX;

    struct vboxvideo_mc mc;
    struct vboxvideo_mode_info  mode_info;

    int				    num_crtc;
};

#endif                /* __DRM_VBOXVIDEO_H__ */
