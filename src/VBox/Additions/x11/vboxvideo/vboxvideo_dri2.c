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
#include <xf86drm.h>
#include <drm.h>
#include <dri2.h>
#include <fcntl.h>
#include <unistd.h>

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

/* We need to pass a constant path string to the screen initialisation function.
 * The format is hard-coded in "drmOpen" in libdrm, and libdrm contains a
 * comment to say that open should be done manually in future and not using
 * "drmOpen", so we will do it manually but also hard-coding the format.  The
 * maximum minor number (15) is also hard-coded. */
#define PATH(minor) "/dev/dri/card" #minor
const char *devicePaths[] =
{
    PATH(0), PATH(1), PATH(2), PATH(3), PATH(4), PATH(5), PATH(6), PATH(7),
    PATH(8), PATH(9), PATH(10), PATH(11), PATH(12), PATH(13), PATH(14), PATH(15)
};
#undef PATH

/** As long as we are using our fake DRI driver inside of Mesa, we only want
 *  to implement the minimum here to make Mesa load it.  Notably we just set
 *  "DRI2Info.fd" to -1 as we do not need authentication to work. */
Bool VBOXDRIScreenInit(ScrnInfoPtr pScrn, ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRI2InfoRec DRI2Info;
    unsigned i;

    memset(&DRI2Info, 0, sizeof(DRI2Info));
    for (i = 0; i < RT_ELEMENTS(devicePaths); ++i)
    {
        int fd = open(devicePaths[i], O_RDWR);
        if (fd >= 0)
        {
            drmVersionPtr pVersion = drmGetVersion(fd);
            if (   pVersion
                && pVersion->name_len
                && !strcmp(pVersion->name, VBOX_DRM_DRIVER_NAME))
            {
                TRACE_LOG("Opened drm device %s\n", devicePaths[i]);
                DRI2Info.deviceName = devicePaths[i];
                /* Keep the driver open and hope that the path won't change. */
                pVBox->drmFD = fd;
                drmFreeVersion(pVersion);
                break;
            }
            close(fd);
            drmFreeVersion(pVersion);
        }
    }
    if (!DRI2Info.deviceName)
        return FALSE;
    DRI2Info.version = 3;
    DRI2Info.fd = -1;
    DRI2Info.driverName = VBOX_DRI_DRIVER_NAME;
    DRI2Info.CopyRegion = VBOXDRICopyRegion;
    DRI2Info.Wait = NULL;
    DRI2Info.CreateBuffer = VBOXDRICreateBuffer;
    DRI2Info.DestroyBuffer = VBOXDRIDestroyBuffer;
    return DRI2ScreenInit(pScreen, &DRI2Info);
}

void VBOXDRICloseScreen(ScreenPtr pScreen, VBOXPtr pVBox)
{
    DRI2CloseScreen(pScreen);
    if (pVBox->drmFD)
        close(pVBox->drmFD);
}
