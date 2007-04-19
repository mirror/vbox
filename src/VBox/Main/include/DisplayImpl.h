/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_DISPLAYIMPL
#define ____H_DISPLAYIMPL

#include "VirtualBoxBase.h"
#include <iprt/semaphore.h>
#include <VBox/pdm.h>
#include <VBox/VBoxGuest.h>

class Console;

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
    int handleDisplayResize (uint32_t bpp, void *pvVRAM, uint32_t cbLine, int w, int h);
    void handleDisplayUpdate (int x, int y, int cx, int cy);
    IFramebuffer *getFramebuffer()
    {
        return mFramebuffer;
    }

    int VideoAccelEnable (bool fEnable, VBVAMEMORY *pVbvaMemory);
    void VideoAccelFlush (void);

    bool VideoAccelAllowed (void);

#ifdef VBOX_VRDP
#ifdef VRDP_MC
    void VideoAccelVRDP (bool fEnable);
#else
    void VideoAccelVRDP (bool fEnable, uint32_t fu32SupportedOrders);
#endif /* VRDP_MC */
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

    STDMETHOD(OnStateChange)(MachineState_T machineState);

    STDMETHOD(OnAdditionsStateChange)()
    {
        return S_OK;
    }

    STDMETHOD(OnKeyboardLedsChange)(BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
        return S_OK;
    }

    STDMETHOD(OnRuntimeError)(BOOL fatal, INPTR BSTR id, INPTR BSTR message)
    {
        return S_OK;
    }

    // IDisplay properties
    STDMETHOD(COMGETTER(Width)) (ULONG *width);
    STDMETHOD(COMGETTER(Height)) (ULONG *height);
    STDMETHOD(COMGETTER(ColorDepth)) (ULONG *colorDepth);

    // IDisplay methods
    STDMETHOD(SetupInternalFramebuffer)(ULONG depth);
    STDMETHOD(LockFramebuffer)(BYTE **address);
    STDMETHOD(UnlockFramebuffer)();
    STDMETHOD(RegisterExternalFramebuffer)(IFramebuffer *frameBuf);
    STDMETHOD(SetVideoModeHint)(ULONG width, ULONG height, ULONG colorDepth);
    STDMETHOD(TakeScreenShot)(BYTE *address, ULONG width, ULONG height);
    STDMETHOD(DrawToScreen)(BYTE *address, ULONG x, ULONG y, ULONG width, ULONG height);
    STDMETHOD(InvalidateAndUpdate)();
    STDMETHOD(ResizeCompleted)();
    STDMETHOD(UpdateCompleted)();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Display"; }

    static const PDMDRVREG  DrvReg;

private:

    void updateDisplayData (bool aCheckParams = false);

    static DECLCALLBACK(int) changeFramebuffer (Display *that, IFramebuffer *aFB,
                                                bool aInternal);

    static DECLCALLBACK(void*) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)   drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)  drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)   displayResizeCallback(PPDMIDISPLAYCONNECTOR pInterface, uint32_t bpp, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayUpdateCallback(PPDMIDISPLAYCONNECTOR pInterface,
                                                     uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);
    static DECLCALLBACK(void)  displayRefreshCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayResetCallback(PPDMIDISPLAYCONNECTOR pInterface);
    static DECLCALLBACK(void)  displayLFBModeChangeCallback(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled);

    ComObjPtr <Console, ComWeakRef> mParent;
    /** Pointer to the associated display driver. */
    struct DRVMAINDISPLAY  *mpDrv;
    /** Pointer to the device instance for the VMM Device. */
    PPDMDEVINS              mpVMMDev;
    /** Set after the first attempt to find the VMM Device. */
    bool                    mfVMMDevInited;
    bool mInternalFramebuffer;
    ComPtr<IFramebuffer> mFramebuffer;
    bool mFramebufferOpened;
    /** bitmask of acceleration operations supported by current framebuffer */
    ULONG mSupportedAccelOps;
    RTSEMEVENTMULTI mUpdateSem;

    /* arguments of the last handleDisplayResize() call */
    void *mLastAddress;
    uint32_t mLastLineSize;
    uint32_t mLastColorDepth;
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
    volatile uint32_t mu32ResizeStatus;
    
    enum {
        ResizeStatus_Void,
        ResizeStatus_InProgress,
        ResizeStatus_UpdateDisplayData
    };
};

#endif // ____H_DISPLAYIMPL
