/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of VMDisplay class
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_DISPLAYIMPL
#define ____H_DISPLAYIMPL

#include <iprt/semaphore.h>
#include <VBox/pdm.h>

#include "Framebuffer.h"
struct _VBVACMDHDR;

class VMDisplay
{

public:

    VMDisplay();
    ~VMDisplay();

    // public methods only for internal purposes
    int handleDisplayResize (int w, int h);
    void handleDisplayUpdate (int x, int y, int cx, int cy);

    int VideoAccelEnable (bool fEnable, struct _VBVAMEMORY *pVbvaMemory);
    void VideoAccelFlush (void);
    bool VideoAccelAllowed (void);

    void updatePointerShape(bool fVisible, bool fAlpha, uint32_t xHot, uint32_t yHot, uint32_t width, uint32_t height, void *pShape);
    void SetVideoModeHint(ULONG aWidth, ULONG aHeight, ULONG aBitsPerPixel, ULONG aDisplay);

    static const PDMDRVREG  DrvReg;

    uint32_t getWidth();
    uint32_t getHeight();
    uint32_t getBitsPerPixel();

    STDMETHODIMP RegisterExternalFramebuffer(Framebuffer *Framebuffer);
    STDMETHODIMP InvalidateAndUpdate();
    STDMETHODIMP ResizeCompleted();

    void resetFramebuffer();

    void setRunning(void) { mfMachineRunning = true; };


private:

    void updateDisplayData();

    static DECLCALLBACK(void*) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)   drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(int)   displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                     uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);
    static DECLCALLBACK(void)  displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize);
    static DECLCALLBACK(void)  displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId);

    static DECLCALLBACK(void) doInvalidateAndUpdate(struct DRVMAINDISPLAY  *mpDrv);
    /** Pointer to the associated display driver. */
    struct DRVMAINDISPLAY  *mpDrv;

    Framebuffer *mFramebuffer;
    bool mInternalFramebuffer, mFramebufferOpened;

    ULONG mSupportedAccelOps;
    RTSEMEVENTMULTI mUpdateSem;

    struct _VBVAMEMORY *mpVbvaMemory;
    bool        mfVideoAccelEnabled;

    struct _VBVAMEMORY *mpPendingVbvaMemory;
    bool        mfPendingVideoAccelEnable;
    bool        mfMachineRunning;

    uint8_t    *mpu8VbvaPartial;
    uint32_t   mcbVbvaPartial;

    bool vbvaFetchCmd (struct _VBVACMDHDR **ppHdr, uint32_t *pcbCmd);
    void vbvaReleaseCmd (struct _VBVACMDHDR *pHdr, int32_t cbCmd);

    void handleResizeCompletedEMT (void);
    volatile uint32_t mu32ResizeStatus;
    
    enum {
        ResizeStatus_Void,
        ResizeStatus_InProgress,
        ResizeStatus_UpdateDisplayData
    };
};

extern VMDisplay *gDisplay;

#endif // ____H_DISPLAYIMPL
