/** @file
 *
 * VBox Console VRDP Helper class and implementation of IRemoteDisplayInfo
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

#ifndef ____H_CONSOLEVRDPSERVER
#define ____H_CONSOLEVRDPSERVER

#include "RemoteUSBBackend.h"

#include <VBox/VRDPAuth.h>

// ConsoleVRDPServer
///////////////////////////////////////////////////////////////////////////////

/* Member of Console. Helper class for VRDP server management. Not a COM class. */
class ConsoleVRDPServer
{
public:
    ConsoleVRDPServer (Console *console);
    ~ConsoleVRDPServer ();

    int Launch (void);
    void Stop (void);

    VRDPAuthResult Authenticate (const Guid &uuid, VRDPAuthGuestJudgement guestJudgement,
                                 const char *pszUser, const char *pszPassword, const char *pszDomain);

#ifdef VRDP_MC
    void USBBackendCreate (uint32_t u32ClientId, PFNVRDPUSBCALLBACK *ppfn, void **ppv);
    void USBBackendDelete (uint32_t u32ClientId);
    
    void *USBBackendRequestPointer (uint32_t u32ClientId, const Guid *pGuid);
    void USBBackendReleasePointer (const Guid *pGuid);
    
    /* Private interface for the RemoteUSBBackend destructor. */
    void usbBackendRemoveFromList (RemoteUSBBackend *pRemoteUSBBackend);
    
    /* Private methods for the Remote USB thread. */
    RemoteUSBBackend *usbBackendGetNext (RemoteUSBBackend *pRemoteUSBBackend);
    
    void notifyRemoteUSBThreadRunning (RTTHREAD thread);
    bool isRemoteUSBThreadRunning (void);
    void waitRemoteUSBThreadEvent (unsigned cMillies);
    
#else
    void CreateUSBBackend (PFNVRDPUSBCALLBACK *ppfn, void **ppv);
    void DeleteUSBBackend (void);

    void *GetUSBBackendPointer (void);
#endif /* VRDP_MC */

    /*
     * Forwarders to VRDP server library.
     */
    void SendUpdate (void *pvUpdate, uint32_t cbUpdate) const;
    void SendResize (void) const;
    void SendUpdateBitmap (uint32_t x, uint32_t y, uint32_t w, uint32_t h) const;
    void SetFramebuffer (IFramebuffer *framebuffer, uint32_t fFlags) const;

    void SendAudioSamples (void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format) const;
    void SendAudioVolume (uint16_t left, uint16_t right) const;
#ifdef VRDP_MC
    void SendUSBRequest (uint32_t u32ClientId, void *pvParms, uint32_t cbParms) const;
#else
    void SendUSBRequest (void *pvParms, uint32_t cbParms) const;
#endif /* VRDP_MC */

    void QueryInfo (uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut) const;

private:
    /* Note: This is not a ComObjPtr here, because the ConsoleVRDPServer object
     * is actually just a part of the Console.
     */
    Console *mConsole;

#ifdef VBOX_VRDP
    HVRDPSERVER mhServer;

    static bool loadVRDPLibrary (void);

    /** Static because will never load this more than once! */
    static RTLDRMOD mVRDPLibrary;

    // VRDP API function pointers
    static int  (VBOXCALL *mpfnVRDPStartServer)     (IConsole *pConsole, IVRDPServer *pVRDPServer, HVRDPSERVER *phServer);
    static int  (VBOXCALL *mpfnVRDPSetFramebuffer)  (HVRDPSERVER hServer, IFramebuffer *pFramebuffer, uint32_t fFlags);
    static void (VBOXCALL *mpfnVRDPSetCallback)     (HVRDPSERVER hServer, VRDPSERVERCALLBACK *pcallback, void *pvUser);
    static void (VBOXCALL *mpfnVRDPShutdownServer)  (HVRDPSERVER hServer);
    static void (VBOXCALL *mpfnVRDPSendUpdateBitmap)(HVRDPSERVER hServer, unsigned x, unsigned y, unsigned w, unsigned h);
    static void (VBOXCALL *mpfnVRDPSendResize)      (HVRDPSERVER hServer);
    static void (VBOXCALL *mpfnVRDPSendAudioSamples)(HVRDPSERVER hserver, void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format);
    static void (VBOXCALL *mpfnVRDPSendAudioVolume) (HVRDPSERVER hserver, uint16_t left, uint16_t right);
#ifdef VRDP_MC
    static void (VBOXCALL *mpfnVRDPSendUSBRequest)  (HVRDPSERVER hserver, uint32_t u32ClientId, void *pvParms, uint32_t cbParms);
#else
    static void (VBOXCALL *mpfnVRDPSendUSBRequest)  (HVRDPSERVER hserver, void *pvParms, uint32_t cbParms);
#endif /* VRDP_MC */
    static void (VBOXCALL *mpfnVRDPSendUpdate)      (HVRDPSERVER hServer, void *pvUpdate, uint32_t cbUpdate);
    static void (VBOXCALL *mpfnVRDPQueryInfo)       (HVRDPSERVER hserver, uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut);
#endif

#ifdef VRDP_MC
    RTCRITSECT mCritSect;

    int lockConsoleVRDPServer (void);
    void unlockConsoleVRDPServer (void);

#ifdef VBOX_WITH_USB
    RemoteUSBBackend *usbBackendFindByUUID (const Guid *pGuid);
    RemoteUSBBackend *usbBackendFind (uint32_t u32ClientId);

    typedef struct _USBBackends
    {
        RemoteUSBBackend *pHead;
        RemoteUSBBackend *pTail;
        
        RTTHREAD thread;
        
        bool fThreadRunning;
        
        RTSEMEVENT event;
    } USBBackends;

    USBBackends mUSBBackends;

    void remoteUSBThreadStart (void);
    void remoteUSBThreadStop (void);
#endif /* VBOX_WITH_USB */
#else
#ifdef VBOX_WITH_USB
    /* The remote USB backend object is created only if the client
     * asks for USB channel. The object is created in the vrdp_InterceptUSB
     * callback.
     */
    RemoteUSBBackend *mRemoteUSBBackend;
#endif /* VBOX_WITH_USB */
#endif /* VRDP_MC */

    /* External authentication library handle. The library is loaded in the
     * Authenticate  method and unloaded at the object destructor.
     */
    RTLDRMOD mAuthLibrary;
    PVRDPAUTHENTRY mpfnAuthEntry;
};


class Console;

class ATL_NO_VTABLE RemoteDisplayInfo :
    public VirtualBoxSupportErrorInfoImpl <RemoteDisplayInfo, IRemoteDisplayInfo>,
    public VirtualBoxSupportTranslation <RemoteDisplayInfo>,
    public VirtualBoxBase,
    public IRemoteDisplayInfo
{
public:

    DECLARE_NOT_AGGREGATABLE(RemoteDisplayInfo)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(RemoteDisplayInfo)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IRemoteDisplayInfo)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    /* Public initializer/uninitializer for internal purposes only. */
    HRESULT init (Console *aParent);
    void uninit();

    /* IRemoteDisplayInfo properties */
    #define DECL_GETTER(_aType, _aName) STDMETHOD(COMGETTER(_aName)) (_aType *a##_aName)
        DECL_GETTER (BOOL,    Active);
        DECL_GETTER (ULONG,   NumberOfClients);
        DECL_GETTER (LONG64,  BeginTime);
        DECL_GETTER (LONG64,  EndTime);
        DECL_GETTER (ULONG64, BytesSent);
        DECL_GETTER (ULONG64, BytesSentTotal);
        DECL_GETTER (ULONG64, BytesReceived);
        DECL_GETTER (ULONG64, BytesReceivedTotal);
        DECL_GETTER (BSTR,    User);
        DECL_GETTER (BSTR,    Domain);
        DECL_GETTER (BSTR,    ClientName);
        DECL_GETTER (BSTR,    ClientIP);
        DECL_GETTER (ULONG,   ClientVersion);
        DECL_GETTER (ULONG,   EncryptionStyle);
    #undef DECL_GETTER

    /* For VirtualBoxSupportErrorInfoImpl. */
    static const wchar_t *getComponentName() { return L"RemoteDisplayInfo"; }

private:

    ComObjPtr <Console, ComWeakRef> mParent;
};

#endif // ____H_CONSOLEVRDPSERVER
