/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_DISPLAYIMPL
#define ____H_DISPLAYIMPL

#include "VirtualBoxBase.h"
#include "SchemaDefs.h"
#include <iprt/semaphore.h>
#include <VBox/pdmdrv.h>
#include <VBox/VBoxGuest.h>
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

    LONG xOrigin;
    LONG yOrigin;

    ULONG w;
    ULONG h;

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

} DISPLAYFBINFO;

class ATL_NO_VTABLE Display :
    public IConsoleCallback,
    public VirtualBoxSupportErrorInfoImpl <Display, IDisplay>,
    public VirtualBoxSupportTranslation <Display>,
    public VirtualBoxBase,
    public IDisplay
{

public:

    DECLARE_NOT_AGGREGATABLE(Display)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Display)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IDisplay)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *parent);
    void uninit();

    // public methods only for internal purposes
    int handleDisplayResize (unsigned uScreenId, uint32_t bpp, void *pvVRAM, uint32_t cbLine, int w, int h);
    void handleDisplayUpdate (int x, int y, int cx, int cy);
    IFramebuffer *getFramebuffer()
    {
        return maFramebuffers[VBOX_VIDEO_PRIMARY_SCREEN].pFramebuffer;
    }

    int VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory);
    void VideoAccelFlush (void);

    bool VideoAccelAllowed (void);

#ifdef VBOX_VRDP
    void VideoAccelVRDP (bool fEnable);
#endif /* VBOX_VRDP */

    // IConsoleCallback methods
    STDMETHOD(OnMousePointerShapeChange)(BOOL visible, BOOL alpha, ULONG xHot, ULONG yHot,
                                         ULONG width, ULONG height, BYTE *shape)
    {
        return S_OK;
    }

    STDMETHOD(OnMouseCapabilityChange)(BOOL supportsAbsolute, BOOL needsHostCursor)
    {
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        return S_OK;
    }

    STDMETHOD(OnStateChange)(MachineState_T machineState);

    STDMETHOD(OnAdditionsStateChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnDVDDriveChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnFloppyDriveChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnNetworkAdapterChange) (INetworkAdapter *aNetworkAdapter)
    {
        return S_OK;
    }

    STDMETHOD(OnSerialPortChange) (ISerialPort *aSerialPort)
    {
        return S_OK;
    }

    STDMETHOD(OnParallelPortChange) (IParallelPort *aParallelPort)
    {
        return S_OK;
    }

    STDMETHOD(OnVRDPServerChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnUSBControllerChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnUSBDeviceStateChange)(IUSBDevice *device, BOOL attached,
                                      IVirtualBoxErrorInfo *message)
    {
        return S_OK;
    }

    STDMETHOD(OnSharedFolderChange) (Scope_T aScope)
    {
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, INPTR BSTR id, INPTR BSTR message)
    {
        return S_OK;
    }

    STDMETHOD(OnCanShowWindow)(BOOL *canShow)
    {
        if (canShow)
            *canShow = TRUE;
        return S_OK;
    }

    STDMETHOD(OnShowWindow)(ULONG64 *winId)
    {
        if (winId)
            *winId = 0;
        return S_OK;
    }

    // IDisplay properties
    STDMETHOD(COMGETTER(Width)) (ULONG *width);
    STDMETHOD(COMGETTER(Height)) (ULONG *height);
    STDMETHOD(COMGETTER(BitsPerPixel)) (ULONG *bitsPerPixel);

    // IDisplay methods
    STDMETHOD(SetupInternalFramebuffer)(ULONG depth);
    STDMETHOD(LockFramebuffer)(BYTE **address);
    STDMETHOD(UnlockFramebuffer)();
    STDMETHOD(RegisterExternalFramebuffer)(IFramebuffer *frameBuf);
    STDMETHOD(SetFramebuffer)(ULONG aScreenId, IFramebuffer *aFramebuffer);
    STDMETHOD(GetFramebuffer)(ULONG aScreenId, IFramebuffer **aFramebuffer, LONG *aXOrigin, LONG *aYOrigin);
    STDMETHOD(SetVideoModeHint)(ULONG width, ULONG height, ULONG bitsPerPixel, ULONG display);
    STDMETHOD(TakeScreenShot)(BYTE *address, ULONG width, ULONG height);
    STDMETHOD(DrawToScreen)(BYTE *address, ULONG x, ULONG y, ULONG width, ULONG height);
    STDMETHOD(InvalidateAndUpdate)();
    STDMETHOD(ResizeCompleted)(ULONG aScreenId);
    STDMETHOD(UpdateCompleted)();
    STDMETHOD(SetSeamlessMode)(BOOL enabled);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Display"; }

    static const PDMDRVREG  DrvReg;

private:

    void updateDisplayData (bool aCheckParams = false);

    static DECLCALLBACK(int) changeFramebuffer (Display *that, IFramebuffer *aFB,
                                                bool aInternal, unsigned uScreenId);

    static DECLCALLBACK(void*) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)   drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)  drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)   displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                     uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);
    static DECLCALLBACK(void)  displayProcessAdapterDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize);
    static DECLCALLBACK(void)  displayProcessDisplayDataCallback(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId);

    ComObjPtr <Console, ComWeakRef> mParent;
    /** Pointer to the associated display driver. */
    struct DRVMAINDISPLAY  *mpDrv;
    /** Pointer to the device instance for the VMM Device. */
    PPDMDEVINS              mpVMMDev;
    /** Set after the first attempt to find the VMM Device. */
    bool                    mfVMMDevInited;
    bool mInternalFramebuffer;

    unsigned mcMonitors;
    DISPLAYFBINFO maFramebuffers[SchemaDefs::MaxGuestMonitors];

    bool mFramebufferOpened;
    /** bitmask of acceleration operations supported by current framebuffer */
    ULONG mSupportedAccelOps;
    RTSEMEVENTMULTI mUpdateSem;

    /* arguments of the last handleDisplayResize() call */
    void *mLastAddress;
    uint32_t mLastBytesPerLine;
    uint32_t mLastBitsPerPixel;
    int mLastWidth;
    int mLastHeight;

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

    bool vbvaFetchCmd (VBVACMDHDR **ppHdr, uint32_t *pcbCmd);
    void vbvaReleaseCmd (VBVACMDHDR *pHdr, int32_t cbCmd);

    void handleResizeCompletedEMT (void);
};

#endif // ____H_DISPLAYIMPL
