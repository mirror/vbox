/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
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

#include "VirtualBoxBase.h"
#include "SchemaDefs.h"

#include <iprt/semaphore.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxVideo.h>

class Console;

enum {
    ResizeStatus_Void,
    ResizeStatus_InProgress,
    ResizeStatus_UpdateDisplayData
};

typedef struct _DISPLAYFBINFO
{
    uint32_t u32Offset;
    uint32_t u32MaxFramebufferSize;
    uint32_t u32InformationSize;

    ComPtr<IFramebuffer> pFramebuffer;
    bool fDisabled;

    LONG xOrigin;
    LONG yOrigin;

    ULONG w;
    ULONG h;

    uint16_t u16BitsPerPixel;
    uint8_t *pu8FramebufferVRAM;
    uint32_t u32LineSize;

    uint16_t flags;

    VBOXVIDEOINFOHOSTEVENTS *pHostEvents;

    volatile uint32_t u32ResizeStatus;

    /* The Framebuffer has default format and must be updates immediately. */
    bool fDefaultFormat;

    struct {
        /* The rectangle that includes all dirty rectangles. */
        int32_t xLeft;
        int32_t xRight;
        int32_t yTop;
        int32_t yBottom;
    } dirtyRect;

    struct {
        bool fPending;
        ULONG pixelFormat;
        void *pvVRAM;
        uint32_t bpp;
        uint32_t cbLine;
        int w;
        int h;
        uint16_t flags;
    } pendingResize;

#ifdef VBOX_WITH_HGSMI
    bool fVBVAEnabled;
    uint32_t cVBVASkipUpdate;
    struct {
       int32_t xLeft;
       int32_t yTop;
       int32_t xRight;
       int32_t yBottom;
    } vbvaSkippedRect;
    PVBVAHOSTFLAGS pVBVAHostFlags;
#endif /* VBOX_WITH_HGSMI */
} DISPLAYFBINFO;

class ATL_NO_VTABLE Display :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEventListener),
    VBOX_SCRIPTABLE_IMPL(IDisplay)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Display, IDisplay)

    DECLARE_NOT_AGGREGATABLE(Display)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Display)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IDisplay)
        COM_INTERFACE_ENTRY(IEventListener)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Display)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();
    int  registerSSM(PVM pVM);

    // public methods only for internal purposes
    int handleDisplayResize (unsigned uScreenId, uint32_t bpp, void *pvVRAM, uint32_t cbLine, int w, int h, uint16_t flags);
    void handleDisplayUpdateLegacy (int x, int y, int cx, int cy);
    void handleDisplayUpdate (unsigned uScreenId, int x, int y, int w, int h);
#ifdef VBOX_WITH_VIDEOHWACCEL
    void handleVHWACommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCommand);
#endif
#ifdef VBOX_WITH_CRHGSMI
    void handleCrHgsmiCommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CMD pCmd, uint32_t cbCmd);
    void handleCrHgsmiControlProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CTL pCtl, uint32_t cbCtl);

    void handleCrHgsmiCommandCompletion(int32_t result, uint32_t u32Function, PVBOXHGCMSVCPARM pParam);
    void handleCrHgsmiControlCompletion(int32_t result, uint32_t u32Function, PVBOXHGCMSVCPARM pParam);
#endif
    IFramebuffer *getFramebuffer()
    {
        return maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer;
    }
    void getFramebufferDimensions(int32_t *px1, int32_t *py1, int32_t *px2,
                                  int32_t *py2);

    int handleSetVisibleRegion(uint32_t cRect, PRTRECT pRect);
    int handleQueryVisibleRegion(uint32_t *pcRect, PRTRECT pRect);

    int VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory);
    void VideoAccelFlush (void);

    bool VideoAccelAllowed (void);

    void VideoAccelVRDP (bool fEnable);

    // IEventListener methods
    STDMETHOD(HandleEvent)(IEvent * aEvent);

    // IDisplay methods
    STDMETHOD(GetScreenResolution)(ULONG aScreenId, ULONG *aWidth, ULONG *aHeight, ULONG *aBitsPerPixel);
    STDMETHOD(SetFramebuffer)(ULONG aScreenId, IFramebuffer *aFramebuffer);
    STDMETHOD(GetFramebuffer)(ULONG aScreenId, IFramebuffer **aFramebuffer, LONG *aXOrigin, LONG *aYOrigin);
    STDMETHOD(SetVideoModeHint)(ULONG width, ULONG height, ULONG bitsPerPixel, ULONG display);
    STDMETHOD(TakeScreenShot)(ULONG aScreenId, BYTE *address, ULONG width, ULONG height);
    STDMETHOD(TakeScreenShotToArray)(ULONG aScreenId, ULONG width, ULONG height, ComSafeArrayOut(BYTE, aScreenData));
    STDMETHOD(TakeScreenShotPNGToArray)(ULONG aScreenId, ULONG width, ULONG height, ComSafeArrayOut(BYTE, aScreenData));
    STDMETHOD(DrawToScreen)(ULONG aScreenId, BYTE *address, ULONG x, ULONG y, ULONG width, ULONG height);
    STDMETHOD(InvalidateAndUpdate)();
    STDMETHOD(ResizeCompleted)(ULONG aScreenId);
    STDMETHOD(SetSeamlessMode)(BOOL enabled);

    STDMETHOD(CompleteVHWACommand)(BYTE *pCommand);

    static const PDMDRVREG  DrvReg;

private:

    void updateDisplayData(void);

#ifdef VBOX_WITH_CRHGSMI
    void setupCrHgsmiData(void);
    void destructCrHgsmiData(void);
#endif

    static DECLCALLBACK(int)   changeFramebuffer(Display *that, IFramebuffer *aFB, unsigned uScreenId);

    static DECLCALLBACK(void*) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)   drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)  drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)   displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                     uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);
    static DECLCALLBACK(void)  displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize);
    static DECLCALLBACK(void)  displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId);

#ifdef VBOX_WITH_VIDEOHWACCEL
    static DECLCALLBACK(void)  displayVHWACommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCommand);
#endif

#ifdef VBOX_WITH_CRHGSMI
    static DECLCALLBACK(void)  displayCrHgsmiCommandProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CMD pCmd, uint32_t cbCmd);
    static DECLCALLBACK(void)  displayCrHgsmiControlProcess(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CTL pCtl, uint32_t cbCtl);

    static DECLCALLBACK(void) displayCrHgsmiCommandCompletion(int32_t result, uint32_t u32Function, PVBOXHGCMSVCPARM pParam, void *pvContext);
    static DECLCALLBACK(void) displayCrHgsmiControlCompletion(int32_t result, uint32_t u32Function, PVBOXHGCMSVCPARM pParam, void *pvContext);
#endif

#ifdef VBOX_WITH_HGSMI
    static DECLCALLBACK(int)   displayVBVAEnable(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, PVBVAHOSTFLAGS pHostFlags);
    static DECLCALLBACK(void)  displayVBVADisable(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId);
    static DECLCALLBACK(void)  displayVBVAUpdateBegin(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId);
    static DECLCALLBACK(void)  displayVBVAUpdateProcess(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, const PVBVACMDHDR pCmd, size_t cbCmd);
    static DECLCALLBACK(void)  displayVBVAUpdateEnd(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, int32_t x, int32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(int)   displayVBVAResize(PPDMIDISPLAYCONNECTOR pInterface, const PVBVAINFOVIEW pView, const PVBVAINFOSCREEN pScreen, void *pvVRAM);
    static DECLCALLBACK(int)   displayVBVAMousePointerShape(PPDMIDISPLAYCONNECTOR pInterface, bool fVisible, bool fAlpha, uint32_t xHot, uint32_t yHot, uint32_t cx, uint32_t cy, const void *pvShape);
#endif


    static DECLCALLBACK(void)  displaySSMSaveScreenshot(PSSMHANDLE pSSM, void *pvUser);
    static DECLCALLBACK(int)   displaySSMLoadScreenshot(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass);
    static DECLCALLBACK(void)  displaySSMSave(PSSMHANDLE pSSM, void *pvUser);
    static DECLCALLBACK(int)   displaySSMLoad(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass);

    Console * const         mParent;
    /** Pointer to the associated display driver. */
    struct DRVMAINDISPLAY  *mpDrv;
    /** Pointer to the device instance for the VMM Device. */
    PPDMDEVINS              mpVMMDev;
    /** Set after the first attempt to find the VMM Device. */
    bool                    mfVMMDevInited;

    unsigned mcMonitors;
    DISPLAYFBINFO maFramebuffers[SchemaDefs::MaxGuestMonitors];

    bool mFramebufferOpened;

    /* arguments of the last handleDisplayResize() call */
    void *mLastAddress;
    uint32_t mLastBytesPerLine;
    uint32_t mLastBitsPerPixel;
    int mLastWidth;
    int mLastHeight;
    uint16_t mLastFlags;

    VBVAMEMORY *mpVbvaMemory;
    bool        mfVideoAccelEnabled;
    bool        mfVideoAccelVRDP;
    uint32_t    mfu32SupportedOrders;

    int32_t volatile mcVideoAccelVRDPRefs;

    VBVAMEMORY *mpPendingVbvaMemory;
    bool        mfPendingVideoAccelEnable;
    bool        mfMachineRunning;

    uint8_t    *mpu8VbvaPartial;
    uint32_t   mcbVbvaPartial;

#ifdef VBOX_WITH_CRHGSMI
    /* for fast host hgcm calls */
    HGCMCVSHANDLE mhCrOglSvc;
#endif

    bool vbvaFetchCmd (VBVACMDHDR **ppHdr, uint32_t *pcbCmd);
    void vbvaReleaseCmd (VBVACMDHDR *pHdr, int32_t cbCmd);

    void handleResizeCompletedEMT (void);

    RTCRITSECT mVBVALock;
    volatile uint32_t mfu32PendingVideoAccelDisable;

    int vbvaLock(void);
    void vbvaUnlock(void);

public:
    static int displayTakeScreenshotEMT(Display *pDisplay, ULONG aScreenId, uint8_t **ppu8Data, size_t *pcbData, uint32_t *pu32Width, uint32_t *pu32Height);

private:
    static void InvalidateAndUpdateEMT(Display *pDisplay);
    static int drawToScreenEMT(Display *pDisplay, ULONG aScreenId, BYTE *address, ULONG x, ULONG y, ULONG width, ULONG height);

    int videoAccelRefreshProcess(void);

    /* Functions run under VBVA lock. */
    int videoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory);
    void videoAccelFlush (void);

#ifdef VBOX_WITH_HGSMI
    volatile uint32_t mu32UpdateVBVAFlags;
#endif
};

void gdImageCopyResampled (uint8_t *dst, uint8_t *src,
                           int dstX, int dstY,
                           int srcX, int srcY,
                           int dstW, int dstH, int srcW, int srcH);


void BitmapScale32 (uint8_t *dst,
                        int dstW, int dstH,
                        const uint8_t *src,
                        int iDeltaLine,
                        int srcW, int srcH);

int DisplayMakePNG(uint8_t *pu8Data, uint32_t cx, uint32_t cy,
                   uint8_t **ppu8PNG, uint32_t *pcbPNG, uint32_t *pcxPNG, uint32_t *pcyPNG,
                   uint8_t fLimitSize);

#endif // ____H_DISPLAYIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
