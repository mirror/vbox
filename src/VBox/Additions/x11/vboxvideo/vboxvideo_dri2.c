/** @file $Id: vboxvideo_dri2.c$
 *
 * VirtualBox X11 Additions graphics driver, DRI2 support
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vboxvideo.h"
#include <drm.h>
#include <dri2.h>

static void VBOXDRICopyRegion(DrawablePtr pDraw, RegionPtr pRegion,
                              DRI2BufferPtr pDest, DRI2BufferPtr pSrc)
{
}

static DRI2Buffer2Ptr VBOXDRICreateBuffer(DrawablePtr pDraw,
                                          unsigned int cAttachment,
                                          unsigned int cFormat)
{
    return calloc(1, sizeof(DRI2Buffer2Rec));
}

static void VBOXDRIDestroyBuffer(DrawablePtr pDraw, DRI2Buffer2Ptr pBuffer)
{
    free(pBuffer);
}

/** As long as we are using our fake DRI driver inside of Mesa, we only want
 *  to implement the minimum here to make Mesa load it.  Notably we just set
 *  "DRI2Info.fd" to -1 as we do not need authentication to work. */
Bool VBOXDRIScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRI2InfoRec DRI2Info;

    memset(&DRI2Info, 0, sizeof(DRI2Info));
    DRI2Info.version = 3;
    DRI2Info.fd = -1;
    DRI2Info.driverName = VBOX_DRI_DRIVER_NAME;
    DRI2Info.deviceName = "/dev/dri/card0";  /** @todo: do this right. */
    DRI2Info.CopyRegion = VBOXDRICopyRegion;
    DRI2Info.Wait = NULL;
    DRI2Info.CreateBuffer = VBOXDRICreateBuffer;
    DRI2Info.DestroyBuffer = VBOXDRIDestroyBuffer;
    return DRI2ScreenInit(pScreen, &DRI2Info);
}

void
VBOXDRICloseScreen(ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRI2CloseScreen(pScreen);
}
