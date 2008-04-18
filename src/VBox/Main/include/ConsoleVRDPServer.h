/** @file
 *
 * VBox Console VRDP Helper class and implementation of IRemoteDisplayInfo
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

#ifndef ____H_CONSOLEVRDPSERVER
#define ____H_CONSOLEVRDPSERVER

#include "RemoteUSBBackend.h"
#include <hgcm/HGCM.h>

#include <VBox/VRDPAuth.h>

#include <VBox/HostServices/VBoxClipboardExt.h>

#include "SchemaDefs.h"

// ConsoleVRDPServer
///////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_VRDP
typedef struct _VRDPInputSynch
{
    int cGuestNumLockAdaptions;
    int cGuestCapsLockAdaptions;

    bool fGuestNumLock;
    bool fGuestCapsLock;
    bool fGuestScrollLock;

    bool fClientNumLock;
    bool fClientCapsLock;
    bool fClientScrollLock;
} VRDPInputSynch;
#endif /* VBOX_VRDP */        

/* Member of Console. Helper class for VRDP server management. Not a COM class. */
class ConsoleVRDPServer
{
public:
    ConsoleVRDPServer (Console *console);
    ~ConsoleVRDPServer ();

    int Launch (void);

    void NotifyAbsoluteMouse (bool fGuestWantsAbsolute)
    {
#ifdef VBOX_VRDP
        m_fGuestWantsAbsolute = fGuestWantsAbsolute;
#else
        NOREF(fGuestWantsAbsolute);
#endif /* VBOX_VRDP */        
    }

    void NotifyKeyboardLedsChange (BOOL fNumLock, BOOL fCapsLock, BOOL fScrollLock)
    {
#ifdef VBOX_VRDP
        bool fGuestNumLock    = (fNumLock != FALSE);
        bool fGuestCapsLock   = (fCapsLock != FALSE);
        bool fGuestScrollLock = (fScrollLock != FALSE);

        /* Might need to resynch in case the guest itself changed the LED status. */
        if (m_InputSynch.fClientNumLock != fGuestNumLock)
        {
            m_InputSynch.cGuestNumLockAdaptions = 2;
        }

        if (m_InputSynch.fClientCapsLock != fGuestCapsLock)
        {
            m_InputSynch.cGuestCapsLockAdaptions = 2;
        }

        m_InputSynch.fGuestNumLock    = fGuestNumLock;
        m_InputSynch.fGuestCapsLock   = fGuestCapsLock;
        m_InputSynch.fGuestScrollLock = fGuestScrollLock;
#else
        NOREF(fNumLock);
        NOREF(fCapsLock);
        NOREF(fScrollLock);
#endif /* VBOX_VRDP */        
    }

    void EnableConnections (void);
    void MousePointerUpdate (const VRDPCOLORPOINTER *pPointer);
    void MousePointerHide (void);

    void Stop (void);

    VRDPAuthResult Authenticate (const Guid &uuid, VRDPAuthGuestJudgement guestJudgement,
                                 const char *pszUser, const char *pszPassword, const char *pszDomain,
                                 uint32_t u32ClientId);

    void AuthDisconnect (const Guid &uuid, uint32_t u32ClientId);

    void USBBackendCreate (uint32_t u32ClientId, void **ppvIntercept);
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
    
    void ClipboardCreate (uint32_t u32ClientId);
    void ClipboardDelete (uint32_t u32ClientId);

    /*
     * Forwarders to VRDP server library.
     */
    void SendUpdate (unsigned uScreenId, void *pvUpdate, uint32_t cbUpdate) const;
    void SendResize (void) const;
    void SendUpdateBitmap (unsigned uScreenId, uint32_t x, uint32_t y, uint32_t w, uint32_t h) const;

    void SendAudioSamples (void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format) const;
    void SendAudioVolume (uint16_t left, uint16_t right) const;
    void SendUSBRequest (uint32_t u32ClientId, void *pvParms, uint32_t cbParms) const;

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

    static PFNVRDPCREATESERVER mpfnVRDPCreateServer;
    
    static VRDPENTRYPOINTS_1 *mpEntryPoints;
    static VRDPCALLBACKS_1 mCallbacks;

    static DECLCALLBACK(int)  VRDPCallbackQueryProperty     (void *pvCallback, uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut);
    static DECLCALLBACK(int)  VRDPCallbackClientLogon       (void *pvCallback, uint32_t u32ClientId, const char *pszUser, const char *pszPassword, const char *pszDomain);
    static DECLCALLBACK(void) VRDPCallbackClientConnect     (void *pvCallback, uint32_t u32ClientId);
    static DECLCALLBACK(void) VRDPCallbackClientDisconnect  (void *pvCallback, uint32_t u32ClientId, uint32_t fu32Intercepted);
    static DECLCALLBACK(int)  VRDPCallbackIntercept         (void *pvCallback, uint32_t u32ClientId, uint32_t fu32Intercept, void **ppvIntercept);
    static DECLCALLBACK(int)  VRDPCallbackUSB               (void *pvCallback, void *pvIntercept, uint32_t u32ClientId, uint8_t u8Code, const void *pvRet, uint32_t cbRet);
    static DECLCALLBACK(int)  VRDPCallbackClipboard         (void *pvCallback, void *pvIntercept, uint32_t u32ClientId, uint32_t u32Function, uint32_t u32Format, const void *pvData, uint32_t cbData);
    static DECLCALLBACK(bool) VRDPCallbackFramebufferQuery  (void *pvCallback, unsigned uScreenId, VRDPFRAMEBUFFERINFO *pInfo);
    static DECLCALLBACK(void) VRDPCallbackFramebufferLock   (void *pvCallback, unsigned uScreenId);
    static DECLCALLBACK(void) VRDPCallbackFramebufferUnlock (void *pvCallback, unsigned uScreenId);
    static DECLCALLBACK(void) VRDPCallbackInput             (void *pvCallback, int type, const void *pvInput, unsigned cbInput);
    static DECLCALLBACK(void) VRDPCallbackVideoModeHint     (void *pvCallback, unsigned cWidth, unsigned cHeight,  unsigned cBitsPerPixel, unsigned uScreenId);
    
    bool m_fGuestWantsAbsolute;
    int m_mousex;
    int m_mousey;
    
    IFramebuffer *maFramebuffers[SchemaDefs::MaxGuestMonitors];
    
    IConsoleCallback *mConsoleCallback;

    VRDPInputSynch m_InputSynch;
#endif /* VBOX_VRDP */

    RTCRITSECT mCritSect;

    int lockConsoleVRDPServer (void);
    void unlockConsoleVRDPServer (void);
    
    int mcClipboardRefs;
    HGCMSVCEXTHANDLE mhClipboard;
    PFNVRDPCLIPBOARDEXTCALLBACK mpfnClipboardCallback;

    static DECLCALLBACK(int) ClipboardCallback (void *pvCallback, uint32_t u32ClientId, uint32_t u32Function, uint32_t u32Format, const void *pvData, uint32_t cbData);
    static DECLCALLBACK(int) ClipboardServiceExtension (void *pvExtension, uint32_t u32Function, void *pvParm, uint32_t cbParms);
    
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

    /* External authentication library handle. The library is loaded in the
     * Authenticate method and unloaded at the object destructor.
     */
    RTLDRMOD mAuthLibrary;
    PVRDPAUTHENTRY mpfnAuthEntry;
    PVRDPAUTHENTRY2 mpfnAuthEntry2;
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
