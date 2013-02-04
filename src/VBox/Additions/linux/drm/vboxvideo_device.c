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
 * vboxvideo_device.c
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)

#include "the-linux-kernel.h"

#include "vboxvideo_drv.h"

#include <VBox/VBoxVideoGuest.h>

int vboxvideo_device_init(struct vboxvideo_device *gdev,
              struct drm_device *ddev,
              struct pci_dev *pdev,
              uint32_t flags)
{
    gdev->dev      = &pdev->dev;
    gdev->ddev     = ddev;
    gdev->pdev     = pdev;
    gdev->flags    = flags;
    gdev->num_crtc = 1;

    /** @todo hardware initialisation goes here once we start doing more complex
     *        stuff.
     */
    gdev->fAnyX        = VBoxVideoAnyWidthAllowed();
    gdev->mc.vram_size = VBoxVideoGetVRAMSize();

    return 0;
}

void vboxvideo_device_fini(struct vboxvideo_device *gdev)
{

}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) */
