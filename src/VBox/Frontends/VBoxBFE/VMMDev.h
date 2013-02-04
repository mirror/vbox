/* $Id$ */
/** @file
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of VMMDev: driver interface to VMM device
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VMMDEV
#define ____H_VMMDEV

#include <VBox/vmm/pdm.h>

class VMMDev
{
public:
    VMMDev();
    ~VMMDev();
    static const PDMDRVREG  DrvReg;

    /** Pointer to the associated VMMDev driver. */
    struct DRVMAINVMMDEV *mpDrv;

    bool fSharedFolderActive;
    bool isShFlActive()
    {
        return fSharedFolderActive;
    }

    PPDMIVMMDEVPORT getVMMDevPort();

    int hgcmLoadService (const char *pszServiceLibrary, const char *pszServiceName);
    int hgcmHostCall (const char *pszServiceName, uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms);

private:
    static DECLCALLBACK(void)   UpdateMouseCapabilities(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities);
    static DECLCALLBACK(void)   UpdatePointerShape(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                   uint32_t xHot, uint32_t yHot,
                                                   uint32_t width, uint32_t height,
                                                   void *pShape);
    static DECLCALLBACK(int)    VideoModeSupported(PPDMIVMMDEVCONNECTOR pInterface, uint32_t display, uint32_t width, uint32_t height,
                                                   uint32_t bpp, bool *fSupported);
    static DECLCALLBACK(int)    GetHeightReduction(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *heightReduction);
    static DECLCALLBACK(int)    QueryBalloonSize(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pu32BalloonSize);

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);
};

extern VMMDev *gVMMDev;

#endif // !____H_VMMDEV
