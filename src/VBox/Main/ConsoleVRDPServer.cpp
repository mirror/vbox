/** @file
 *
 * VBox Console VRDP Helper class
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

#include "ConsoleVRDPServer.h"
#include "ConsoleImpl.h"
#include "DisplayImpl.h"

#include "Logging.h"

#include <iprt/asm.h>
#include <iprt/ldr.h>

#include <VBox/err.h>


// ConsoleVRDPServer
////////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_VRDP
RTLDRMOD ConsoleVRDPServer::mVRDPLibrary;
int  (VBOXCALL *ConsoleVRDPServer::mpfnVRDPStartServer)     (IConsole *pConsole, IVRDPServer *pVRDPServer, HVRDPSERVER *phServer);
int  (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSetFramebuffer)  (HVRDPSERVER hServer, IFramebuffer *pFramebuffer, uint32_t fFlags);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSetCallback)     (HVRDPSERVER hServer, VRDPSERVERCALLBACK *pcallback, void *pvUser);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPShutdownServer)  (HVRDPSERVER hServer);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendUpdateBitmap)(HVRDPSERVER hServer, unsigned x, unsigned y, unsigned w, unsigned h);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendResize)      (HVRDPSERVER hServer);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendAudioSamples)(HVRDPSERVER hserver, void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendAudioVolume) (HVRDPSERVER hserver, uint16_t left, uint16_t right);
#ifdef VRDP_MC
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendUSBRequest)  (HVRDPSERVER hserver, uint32_t u32ClientId, void *pvParms, uint32_t cbParms);
#else
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendUSBRequest)  (HVRDPSERVER hserver, void *pvParms, uint32_t cbParms);
#endif /* VRDP_MC */
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPSendUpdate)      (HVRDPSERVER hServer, void *pvUpdate, uint32_t cbUpdate);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPQueryInfo)       (HVRDPSERVER hserver, uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut);
void (VBOXCALL *ConsoleVRDPServer::mpfnVRDPClipboard)       (HVRDPSERVER hserver, uint32_t u32Function, uint32_t u32Format, const void *pvData, uint32_t cbData, uint32_t *pcbActualRead);
#endif /* VBOX_VRDP */

ConsoleVRDPServer::ConsoleVRDPServer (Console *console)
{
    mConsole = console;

#ifdef VRDP_MC
    int rc = RTCritSectInit (&mCritSect);
    AssertRC (rc);
    
    mcClipboardRefs = 0;
    mpfnClipboardCallback = NULL;

#ifdef VBOX_WITH_USB
    mUSBBackends.pHead = NULL;
    mUSBBackends.pTail = NULL;
    
    mUSBBackends.thread = NIL_RTTHREAD;
    mUSBBackends.fThreadRunning = false;
    mUSBBackends.event = 0;
#endif
#else
#ifdef VBOX_WITH_USB
    mRemoteUSBBackend = NULL;
#endif
#endif /* VRDP_MC */

#ifdef VBOX_VRDP
    mhServer = 0;
#endif

    mAuthLibrary = 0;
}

ConsoleVRDPServer::~ConsoleVRDPServer ()
{
#ifdef VRDP_MC
    Stop ();

    if (RTCritSectIsInitialized (&mCritSect))
    {
        RTCritSectDelete (&mCritSect);
        memset (&mCritSect, 0, sizeof (mCritSect));
    }
#else
#ifdef VBOX_VRDP
    Stop ();
    /* No unloading of anything because we might still have live object around. */
#endif
#endif /* VRDP_MC */
}

int ConsoleVRDPServer::Launch (void)
{
    LogFlowMember(("ConsoleVRDPServer::Launch\n"));
#ifdef VBOX_VRDP
    int rc = VINF_SUCCESS;
    IVRDPServer *vrdpserver = mConsole->getVRDPServer ();
    Assert(vrdpserver);
    BOOL vrdpEnabled = FALSE;

    HRESULT rc2 = vrdpserver->COMGETTER(Enabled) (&vrdpEnabled);
    AssertComRC(rc2);

    if (SUCCEEDED (rc2)
        && vrdpEnabled
        && loadVRDPLibrary ())
    {
        rc = mpfnVRDPStartServer(mConsole, vrdpserver, &mhServer);

        if (VBOX_SUCCESS(rc))
        {
            LogFlow(("VRDP server created: %p, will set mFramebuffer\n", mhServer));

            IFramebuffer *framebuffer = mConsole->getDisplay()->getFramebuffer();

            mpfnVRDPSetFramebuffer (mhServer, framebuffer,
                framebuffer? VRDP_EXTERNAL_FRAMEBUFFER: VRDP_INTERNAL_FRAMEBUFFER);

            LogFlow(("Framebuffer %p set for the VRDP server\n", framebuffer));
            
#ifdef VBOX_WITH_USB
#ifdef VRDP_MC
            remoteUSBThreadStart ();
#endif /* VRDP_MC */
#endif /* VBOX_WITH_USB */
        }
        else
            AssertMsgFailed(("Could not start VRDP server: rc = %Vrc\n", rc));
    }
#else
    int rc = VERR_NOT_SUPPORTED;
#endif
    return rc;
}

void ConsoleVRDPServer::SetCallback (void)
{
#ifdef VBOX_VRDP
    /* This is called after VM is created and allows the server to accept client connection. */
    if (mhServer && mpfnVRDPSetCallback)
    {
        mpfnVRDPSetCallback (mhServer, mConsole->getVrdpServerCallback (), mConsole);
    }
#endif
}

void ConsoleVRDPServer::Stop (void)
{
    Assert(VALID_PTR(this)); /** @todo r=bird: there are(/was) some odd cases where this buster was invalid on
                              * linux. Just remove this when it's 100% sure that problem has been fixed. */
#ifdef VBOX_VRDP
    if (mhServer)
    {
        HVRDPSERVER hServer = mhServer;

        /* Reset the handle to avoid further calls to the server. */
        mhServer = 0;

        mpfnVRDPShutdownServer (hServer);
    }
#endif

#ifdef VBOX_WITH_USB
#ifdef VRDP_MC
    remoteUSBThreadStop ();
#else
    /* Delete the USB backend object if it was not deleted properly. */
    if (mRemoteUSBBackend)
    {
        Log(("ConsoleVRDPServer::Stop: deleting USB backend\n"));

        mRemoteUSBBackend->ReleaseUSB ();
        delete mRemoteUSBBackend;
        mRemoteUSBBackend = NULL;
    }
#endif /* VRDP_MC */
#endif /* VBOX_WITH_USB */

    mpfnAuthEntry = NULL;
    mpfnAuthEntry2 = NULL;

    if (mAuthLibrary)
    {
        RTLdrClose(mAuthLibrary);
        mAuthLibrary = 0;
    }
}

#ifdef VRDP_MC
/* Worker thread for Remote USB. The thread polls the clients for
 * the list of attached USB devices.
 * The thread is also responsible for attaching/detaching devices
 * to/from the VM.
 *
 * It is expected that attaching/detaching is not a frequent operation.
 *
 * The thread is always running when the VRDP server is active.
 *
 * The thread scans backends and requests the device list every 2 seconds.
 *
 * When device list is available, the thread calls the Console to process it.
 *
 */
#define VRDP_DEVICE_LIST_PERIOD_MS (2000)

#ifdef VBOX_WITH_USB
static DECLCALLBACK(int) threadRemoteUSB (RTTHREAD self, void *pvUser)
{
    ConsoleVRDPServer *pOwner = (ConsoleVRDPServer *)pvUser;

    LogFlow(("Console::threadRemoteUSB: start. owner = %p.\n", pOwner));
    
    pOwner->notifyRemoteUSBThreadRunning (self);

    while (pOwner->isRemoteUSBThreadRunning ())
    {
        RemoteUSBBackend *pRemoteUSBBackend = NULL;
        
        while ((pRemoteUSBBackend = pOwner->usbBackendGetNext (pRemoteUSBBackend)) != NULL)
        {
            pRemoteUSBBackend->PollRemoteDevices ();
        }

        pOwner->waitRemoteUSBThreadEvent (VRDP_DEVICE_LIST_PERIOD_MS);

        LogFlow(("Console::threadRemoteUSB: iteration. owner = %p.\n", pOwner));
    }

    return VINF_SUCCESS;
}

void ConsoleVRDPServer::notifyRemoteUSBThreadRunning (RTTHREAD thread)
{
#ifdef VBOX_WITH_USB
    mUSBBackends.thread = thread;
    mUSBBackends.fThreadRunning = true;
    int rc = RTThreadUserSignal (thread);
    AssertRC (rc);
#endif
}
    
bool ConsoleVRDPServer::isRemoteUSBThreadRunning (void)
{
    return mUSBBackends.fThreadRunning;
}
    
void ConsoleVRDPServer::waitRemoteUSBThreadEvent (unsigned cMillies)
{
    int rc = RTSemEventWait (mUSBBackends.event, cMillies);
    Assert (VBOX_SUCCESS(rc) || rc == VERR_TIMEOUT);
    NOREF(rc);
}

void ConsoleVRDPServer::remoteUSBThreadStart (void)
{
    int rc = RTSemEventCreate (&mUSBBackends.event);

    if (VBOX_FAILURE (rc))
    {
        AssertFailed ();
        mUSBBackends.event = 0;
    }
    
    if (VBOX_SUCCESS (rc))
    {
        rc = RTThreadCreate (&mUSBBackends.thread, threadRemoteUSB, this, 65536,
                             RTTHREADTYPE_VRDP_IO, RTTHREADFLAGS_WAITABLE, "remote usb");
    }
    
    if (VBOX_FAILURE (rc))
    {
        LogRel(("Warning: could not start the remote USB thread, rc = %Vrc!!!\n", rc));
        mUSBBackends.thread = NIL_RTTHREAD;
    }
    else
    {
        /* Wait until the thread is ready. */
        rc = RTThreadUserWait (mUSBBackends.thread, 60000);
        AssertRC (rc);
        Assert (mUSBBackends.fThreadRunning || VBOX_FAILURE (rc));
    }
}

void ConsoleVRDPServer::remoteUSBThreadStop (void)
{
    mUSBBackends.fThreadRunning = false;
    
    if (mUSBBackends.thread != NIL_RTTHREAD)
    {
        Assert (mUSBBackends.event != 0);
        
        RTSemEventSignal (mUSBBackends.event);
        
        int rc = RTThreadWait (mUSBBackends.thread, 60000, NULL);
        AssertRC (rc);
        
        mUSBBackends.thread = NIL_RTTHREAD;
    }
    
    if (mUSBBackends.event)
    {
        RTSemEventDestroy (mUSBBackends.event);
        mUSBBackends.event = 0;
    }
}
#endif /* VBOX_WITH_USB */
#endif /* VRDP_MC */

VRDPAuthResult ConsoleVRDPServer::Authenticate (const Guid &uuid, VRDPAuthGuestJudgement guestJudgement,
                                                const char *pszUser, const char *pszPassword, const char *pszDomain,
                                                uint32_t u32ClientId)
{
    VRDPAUTHUUID rawuuid;

    memcpy (rawuuid, ((Guid &)uuid).ptr (), sizeof (rawuuid));

    LogFlow(("ConsoleVRDPServer::Authenticate: uuid = %Vuuid, guestJudgement = %d, pszUser = %s, pszPassword = %s, pszDomain = %s, u32ClientId = %d\n",
             rawuuid, guestJudgement, pszUser, pszPassword, pszDomain, u32ClientId));

    /*
     * Called only from VRDP input thread. So thread safety is not required.
     */

    if (!mAuthLibrary)
    {
        /* Load the external authentication library. */

        ComPtr<IMachine> machine;
        mConsole->COMGETTER(Machine)(machine.asOutParam());

        ComPtr<IVirtualBox> virtualBox;
        machine->COMGETTER(Parent)(virtualBox.asOutParam());

        ComPtr<ISystemProperties> systemProperties;
        virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

        Bstr authLibrary;
        systemProperties->COMGETTER(RemoteDisplayAuthLibrary)(authLibrary.asOutParam());

        Utf8Str filename = authLibrary;

        LogRel(("VRDPAUTH: ConsoleVRDPServer::Authenticate: loading external authentication library '%ls'\n", authLibrary.raw()));

        int rc = RTLdrLoad (filename.raw(), &mAuthLibrary);
        if (VBOX_FAILURE (rc))
            LogRel(("VRDPAUTH: Failed to load external authentication library. Error code: %Vrc\n", rc));

        if (VBOX_SUCCESS (rc))
        {
            /* Get the entry point. */
            mpfnAuthEntry2 = NULL;
            int rc2 = RTLdrGetSymbol(mAuthLibrary, "VRDPAuth2", (void**)&mpfnAuthEntry2);
            if (VBOX_FAILURE (rc2))
            {
                LogRel(("VRDPAUTH: Could not resolve import '%s'. Error code: %Vrc\n", "VRDPAuth2", rc2));
                rc = rc2;
            }

            /* Get the entry point. */
            mpfnAuthEntry = NULL;
            rc2 = RTLdrGetSymbol(mAuthLibrary, "VRDPAuth", (void**)&mpfnAuthEntry);
            if (VBOX_FAILURE (rc2))
            {
                LogRel(("VRDPAUTH: Could not resolve import '%s'. Error code: %Vrc\n", "VRDPAuth", rc2));
                rc = rc2;
            }

            if (mpfnAuthEntry2 || mpfnAuthEntry)
            {
                LogRel(("VRDPAUTH: Using entry point '%s'.\n", mpfnAuthEntry2? "VRDPAuth2": "VRDPAuth"));
                rc = VINF_SUCCESS;
            }
        }

        if (VBOX_FAILURE (rc))
        {
            mConsole->reportAuthLibraryError (filename.raw(), rc);

            mpfnAuthEntry = NULL;
            mpfnAuthEntry2 = NULL;

            if (mAuthLibrary)
            {
                RTLdrClose(mAuthLibrary);
                mAuthLibrary = 0;
            }

            return VRDPAuthAccessDenied;
        }
    }

    Assert (mAuthLibrary && (mpfnAuthEntry || mpfnAuthEntry2));

    VRDPAuthResult result = mpfnAuthEntry?
                                mpfnAuthEntry (&rawuuid, guestJudgement, pszUser, pszPassword, pszDomain):
                                mpfnAuthEntry2 (&rawuuid, guestJudgement, pszUser, pszPassword, pszDomain, true, u32ClientId);

    switch (result)
    {
        case VRDPAuthAccessDenied:
            LogRel(("VRDPAUTH: external authentication module returned 'access denied'\n"));
            break;
        case VRDPAuthAccessGranted:
            LogRel(("VRDPAUTH: external authentication module returned 'access granted'\n"));
            break;
        case VRDPAuthDelegateToGuest:
            LogRel(("VRDPAUTH: external authentication module returned 'delegate request to guest'\n"));
            break;
        default:
            LogRel(("VRDPAUTH: external authentication module returned incorrect return code %d\n", result));
            result = VRDPAuthAccessDenied;
    }

    LogFlow(("ConsoleVRDPServer::Authenticate: result = %d\n", result));

    return result;
}

void ConsoleVRDPServer::AuthDisconnect (const Guid &uuid, uint32_t u32ClientId)
{
    VRDPAUTHUUID rawuuid;

    memcpy (rawuuid, ((Guid &)uuid).ptr (), sizeof (rawuuid));

    LogFlow(("ConsoleVRDPServer::AuthDisconnect: uuid = %Vuuid, u32ClientId = %d\n",
             rawuuid, u32ClientId));

    Assert (mAuthLibrary && (mpfnAuthEntry || mpfnAuthEntry2));

    if (mpfnAuthEntry2)
        mpfnAuthEntry2 (&rawuuid, VRDPAuthGuestNotAsked, NULL, NULL, NULL, false, u32ClientId);
}

#ifdef VRDP_MC
int ConsoleVRDPServer::lockConsoleVRDPServer (void)
{
    int rc = RTCritSectEnter (&mCritSect);
    AssertRC (rc);
    return rc;
}

void ConsoleVRDPServer::unlockConsoleVRDPServer (void)
{
    RTCritSectLeave (&mCritSect);
}

DECLCALLBACK(int) ConsoleVRDPServer::ClipboardCallback (void *pvCallback,
                                                        uint32_t u32ClientId,
                                                        uint32_t u32Function,
                                                        uint32_t u32Format,
                                                        const void *pvData,
                                                        uint32_t cbData)
{
    LogFlowFunc(("pvCallback = %p, u32ClientId = %d, u32Function = %d, u32Format = 0x%08X, pvData = %p, cbData = %d\n",
                 pvCallback, u32ClientId, u32Function, u32Format, pvData, cbData));

    int rc = VINF_SUCCESS;
    
    ConsoleVRDPServer *pServer = static_cast <ConsoleVRDPServer *>(pvCallback);
    
    NOREF(u32ClientId);
    
    switch (u32Function)
    {
        case VRDP_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE:
        {
            if (pServer->mpfnClipboardCallback)
            {
                pServer->mpfnClipboardCallback (VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE,
                                                u32Format,
                                                (void *)pvData,
                                                cbData);
            }
        } break; 
        
        case VRDP_CLIPBOARD_FUNCTION_DATA_READ:
        {
            if (pServer->mpfnClipboardCallback)
            {
                pServer->mpfnClipboardCallback (VBOX_CLIPBOARD_EXT_FN_DATA_READ,
                                                u32Format,
                                                (void *)pvData,
                                                cbData);
            }
        } break; 

        default: 
            rc = VERR_NOT_SUPPORTED;
    }
    
    return rc;
}

DECLCALLBACK(int) ConsoleVRDPServer::ClipboardServiceExtension (void *pvExtension,
                                                                uint32_t u32Function,
                                                                void *pvParms,
                                                                uint32_t cbParms)
{
    LogFlowFunc(("pvExtension = %p, u32Function = %d, pvParms = %p, cbParms = %d\n",
                 pvExtension, u32Function, pvParms, cbParms));
    
    int rc = VINF_SUCCESS;
    
#ifdef VBOX_VRDP
    ConsoleVRDPServer *pServer = static_cast <ConsoleVRDPServer *>(pvExtension);
    
    VBOXCLIPBOARDEXTPARMS *pParms = (VBOXCLIPBOARDEXTPARMS *)pvParms;
    
    switch (u32Function)
    {
        case VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK:
        {
            pServer->mpfnClipboardCallback = (PFNVRDPCLIPBOARDEXTCALLBACK)pParms->pvData;
        } break;

        case VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE:
        {
            /* The guest announces clipboard formats. This must be delivered to all clients. */
            if (mpfnVRDPClipboard)
            {
                mpfnVRDPClipboard (pServer->mhServer, 
                                   VRDP_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE,
                                   pParms->u32Format,
                                   NULL,
                                   0,
                                   NULL);
            }
        } break;

        case VBOX_CLIPBOARD_EXT_FN_DATA_READ:
        {
            /* The clipboard service expects that the pvData buffer will be filled
             * with clipboard data. The server returns the data from the client that
             * announced the requested format most recently.
             */
            if (mpfnVRDPClipboard)
            {
                mpfnVRDPClipboard (pServer->mhServer, 
                                   VRDP_CLIPBOARD_FUNCTION_DATA_READ,
                                   pParms->u32Format,
                                   pParms->pvData,
                                   pParms->cbData,
                                   &pParms->cbData);
            }
        } break;

        case VBOX_CLIPBOARD_EXT_FN_DATA_WRITE:
        {
            if (mpfnVRDPClipboard)
            {
                mpfnVRDPClipboard (pServer->mhServer, 
                                   VRDP_CLIPBOARD_FUNCTION_DATA_WRITE,
                                   pParms->u32Format,
                                   pParms->pvData,
                                   pParms->cbData,
                                   NULL);
            }
        } break;
        
        default: 
            rc = VERR_NOT_SUPPORTED;
    }
#endif /* VBOX_VRDP */

    return rc;
}

void ConsoleVRDPServer::ClipboardCreate (uint32_t u32ClientId, PFNVRDPCLIPBOARDCALLBACK *ppfn, void **ppv)
{
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        if (mcClipboardRefs == 0)
        {
            rc = HGCMHostRegisterServiceExtension (&mhClipboard, "VBoxSharedClipboard", ClipboardServiceExtension, this);
            
            if (VBOX_SUCCESS (rc))
            {
                mcClipboardRefs++;
            }
        }
        
        if (VBOX_SUCCESS (rc))
        {
            *ppfn = ClipboardCallback;
            *ppv = this;
        }

        unlockConsoleVRDPServer ();
    }
}

void ConsoleVRDPServer::ClipboardDelete (uint32_t u32ClientId)
{
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        mcClipboardRefs--;
        
        if (mcClipboardRefs == 0)
        {
            HGCMHostUnregisterServiceExtension (mhClipboard);
        }

        unlockConsoleVRDPServer ();
    }
}

/* That is called on INPUT thread of the VRDP server.
 * The ConsoleVRDPServer keeps a list of created backend instances.
 */
void ConsoleVRDPServer::USBBackendCreate (uint32_t u32ClientId, PFNVRDPUSBCALLBACK *ppfn, void **ppv)
{
#ifdef VBOX_WITH_USB
    LogFlow(("ConsoleVRDPServer::USBBackendCreate: u32ClientId = %d\n", u32ClientId));
    
    /* Create a new instance of the USB backend for the new client. */
    RemoteUSBBackend *pRemoteUSBBackend = new RemoteUSBBackend (mConsole, this, u32ClientId);

    if (pRemoteUSBBackend)
    {
        pRemoteUSBBackend->AddRef (); /* 'Release' called in USBBackendDelete. */
            
        /* Append the new instance in the list. */
        int rc = lockConsoleVRDPServer ();
        
        if (VBOX_SUCCESS (rc))
        {
            pRemoteUSBBackend->pNext = mUSBBackends.pHead;
            if (mUSBBackends.pHead)
            {
                mUSBBackends.pHead->pPrev = pRemoteUSBBackend;
            }
            else
            {
                mUSBBackends.pTail = pRemoteUSBBackend;
            }
            
            mUSBBackends.pHead = pRemoteUSBBackend;
            
            unlockConsoleVRDPServer ();
            
            pRemoteUSBBackend->QueryVRDPCallbackPointer (ppfn, ppv);
        }

        if (VBOX_FAILURE (rc))
        {
            pRemoteUSBBackend->Release ();
        }
    }
#endif
}

void ConsoleVRDPServer::USBBackendDelete (uint32_t u32ClientId)
{
#ifdef VBOX_WITH_USB
    LogFlow(("ConsoleVRDPServer::USBBackendDelete: u32ClientId = %d\n", u32ClientId));

    RemoteUSBBackend *pRemoteUSBBackend = NULL;
    
    /* Find the instance. */
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        pRemoteUSBBackend = usbBackendFind (u32ClientId);
        
        if (pRemoteUSBBackend)
        {
            /* Notify that it will be deleted. */
            pRemoteUSBBackend->NotifyDelete ();
        }
        
        unlockConsoleVRDPServer ();
    }
    
    if (pRemoteUSBBackend)
    {
        /* Here the instance has been excluded from the list and can be dereferenced. */
        pRemoteUSBBackend->Release ();
    }
#endif
}

void *ConsoleVRDPServer::USBBackendRequestPointer (uint32_t u32ClientId, const Guid *pGuid)
{
#ifdef VBOX_WITH_USB
    RemoteUSBBackend *pRemoteUSBBackend = NULL;
    
    /* Find the instance. */
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        pRemoteUSBBackend = usbBackendFind (u32ClientId);
        
        if (pRemoteUSBBackend)
        {
            /* Inform the backend instance that it is referenced by the Guid. */
            bool fAdded = pRemoteUSBBackend->addUUID (pGuid);
            
            if (fAdded)
            {
                /* Reference the instance because its pointer is being taken. */
                pRemoteUSBBackend->AddRef (); /* 'Release' is called in USBBackendReleasePointer. */
            }
            else
            {
                pRemoteUSBBackend = NULL;
            }
        }
        
        unlockConsoleVRDPServer ();
    }
    
    if (pRemoteUSBBackend)
    {
        return pRemoteUSBBackend->GetBackendCallbackPointer ();
    }
    
#endif
    return NULL;
}

void ConsoleVRDPServer::USBBackendReleasePointer (const Guid *pGuid)
{
#ifdef VBOX_WITH_USB
    RemoteUSBBackend *pRemoteUSBBackend = NULL;
    
    /* Find the instance. */
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        pRemoteUSBBackend = usbBackendFindByUUID (pGuid);
        
        if (pRemoteUSBBackend)
        {
            pRemoteUSBBackend->removeUUID (pGuid);
        }
        
        unlockConsoleVRDPServer ();
        
        if (pRemoteUSBBackend)
        {
            pRemoteUSBBackend->Release ();
        }
    }
#endif
}

RemoteUSBBackend *ConsoleVRDPServer::usbBackendGetNext (RemoteUSBBackend *pRemoteUSBBackend)
{
    LogFlow(("ConsoleVRDPServer::usbBackendGetNext: pBackend = %p\n", pRemoteUSBBackend));

    RemoteUSBBackend *pNextRemoteUSBBackend = NULL;
#ifdef VBOX_WITH_USB
        
    int rc = lockConsoleVRDPServer ();
        
    if (VBOX_SUCCESS (rc))
    {
        if (pRemoteUSBBackend == NULL)
        {
            /* The first backend in the list is requested. */
            pNextRemoteUSBBackend = mUSBBackends.pHead;
        }
        else
        {
            /* Get pointer to the next backend. */
            pNextRemoteUSBBackend = (RemoteUSBBackend *)pRemoteUSBBackend->pNext;
        }
        
        if (pNextRemoteUSBBackend)
        {
            pNextRemoteUSBBackend->AddRef ();
        }
        
        unlockConsoleVRDPServer ();
        
        if (pRemoteUSBBackend)
        {
            pRemoteUSBBackend->Release ();
        }
    }
#endif
    
    return pNextRemoteUSBBackend;
}

#ifdef VBOX_WITH_USB
/* Internal method. Called under the ConsoleVRDPServerLock. */
RemoteUSBBackend *ConsoleVRDPServer::usbBackendFind (uint32_t u32ClientId)
{
    RemoteUSBBackend *pRemoteUSBBackend = mUSBBackends.pHead;
        
    while (pRemoteUSBBackend)
    {
        if (pRemoteUSBBackend->ClientId () == u32ClientId)
        {
            break;
        }
            
        pRemoteUSBBackend = (RemoteUSBBackend *)pRemoteUSBBackend->pNext;
    }
    
    return pRemoteUSBBackend;
}

/* Internal method. Called under the ConsoleVRDPServerLock. */
RemoteUSBBackend *ConsoleVRDPServer::usbBackendFindByUUID (const Guid *pGuid)
{
    RemoteUSBBackend *pRemoteUSBBackend = mUSBBackends.pHead;
        
    while (pRemoteUSBBackend)
    {
        if (pRemoteUSBBackend->findUUID (pGuid))
        {
            break;
        }
            
        pRemoteUSBBackend = (RemoteUSBBackend *)pRemoteUSBBackend->pNext;
    }
    
    return pRemoteUSBBackend;
}
#endif

/* Internal method. Called by the backend destructor. */
void ConsoleVRDPServer::usbBackendRemoveFromList (RemoteUSBBackend *pRemoteUSBBackend)
{
#ifdef VBOX_WITH_USB
    int rc = lockConsoleVRDPServer ();
    AssertRC (rc);
        
    /* Exclude the found instance from the list. */
    if (pRemoteUSBBackend->pNext)
    {
        pRemoteUSBBackend->pNext->pPrev = pRemoteUSBBackend->pPrev;
    }
    else
    {
        mUSBBackends.pTail = (RemoteUSBBackend *)pRemoteUSBBackend->pPrev;
    }
                
    if (pRemoteUSBBackend->pPrev)
    {
        pRemoteUSBBackend->pPrev->pNext = pRemoteUSBBackend->pNext;
    }
    else
    {
        mUSBBackends.pHead = (RemoteUSBBackend *)pRemoteUSBBackend->pNext;
    }
                
    pRemoteUSBBackend->pNext = pRemoteUSBBackend->pPrev = NULL;
    
    unlockConsoleVRDPServer ();
#endif
}
#else // VRDP_MC
void ConsoleVRDPServer::CreateUSBBackend (PFNVRDPUSBCALLBACK *ppfn, void **ppv)
{
#ifdef VBOX_WITH_USB
    Assert(mRemoteUSBBackend == NULL);

    mRemoteUSBBackend = new RemoteUSBBackend (mConsole, this);

    if (mRemoteUSBBackend)
    {
        int rc = mRemoteUSBBackend->InterceptUSB (ppfn, ppv);

        if (VBOX_FAILURE (rc))
        {
            delete mRemoteUSBBackend;
            mRemoteUSBBackend = NULL;
        }
    }
#endif /* VBOX_WITH_USB */
}

void ConsoleVRDPServer::DeleteUSBBackend (void)
{
#ifdef VBOX_WITH_USB
    LogFlow(("ConsoleVRDPServer::DeleteUSBBackend: %p\n", mRemoteUSBBackend));

    if (mRemoteUSBBackend)
    {
        mRemoteUSBBackend->ReleaseUSB ();
        delete mRemoteUSBBackend;
        mRemoteUSBBackend = NULL;
    }
#endif /* VBOX_WITH_USB */
}

void *ConsoleVRDPServer::GetUSBBackendPointer (void)
{
#ifdef VBOX_WITH_USB
    Assert (mRemoteUSBBackend); /* Must be called only if the object exists. */
    return mRemoteUSBBackend->GetRemoteBackendCallback ();
#else
    return NULL;
#endif 
}
#endif /* VRDP_MC */



void ConsoleVRDPServer::SendUpdate (void *pvUpdate, uint32_t cbUpdate) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendUpdate)
        mpfnVRDPSendUpdate (mhServer, pvUpdate, cbUpdate);
#endif
}

void ConsoleVRDPServer::SendResize (void) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendResize)
        mpfnVRDPSendResize (mhServer);
#endif
}

void ConsoleVRDPServer::SendUpdateBitmap (uint32_t x, uint32_t y, uint32_t w, uint32_t h) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendUpdateBitmap)
        mpfnVRDPSendUpdateBitmap (mhServer, x, y, w, h);
#endif
}

void ConsoleVRDPServer::SetFramebuffer (IFramebuffer *framebuffer, uint32_t fFlags) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSetFramebuffer)
        mpfnVRDPSetFramebuffer (mhServer, framebuffer, fFlags);
#endif
}

void ConsoleVRDPServer::SendAudioSamples (void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendAudioSamples)
        mpfnVRDPSendAudioSamples (mhServer, pvSamples, cSamples, format);
#endif
}

void ConsoleVRDPServer::SendAudioVolume (uint16_t left, uint16_t right) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendAudioVolume)
        mpfnVRDPSendAudioVolume (mhServer, left, right);
#endif
}

#ifdef VRDP_MC
void ConsoleVRDPServer::SendUSBRequest (uint32_t u32ClientId, void *pvParms, uint32_t cbParms) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendUSBRequest)
        mpfnVRDPSendUSBRequest (mhServer, u32ClientId, pvParms, cbParms);
#endif
}
#else
void ConsoleVRDPServer::SendUSBRequest (void *pvParms, uint32_t cbParms) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPSendUSBRequest)
        mpfnVRDPSendUSBRequest (mhServer, pvParms, cbParms);
#endif
}
#endif /* VRDP_MC */

void ConsoleVRDPServer::QueryInfo (uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut) const
{
#ifdef VBOX_VRDP
    if (mpfnVRDPQueryInfo)
        mpfnVRDPQueryInfo (mhServer, index, pvBuffer, cbBuffer, pcbOut);
#endif
}

#ifdef VBOX_VRDP
/* note: static function now! */
bool ConsoleVRDPServer::loadVRDPLibrary (void)
{
    int rc = VINF_SUCCESS;

    if (!mVRDPLibrary)
    {
        rc = RTLdrLoad("VBoxVRDP", &mVRDPLibrary);

        if (VBOX_SUCCESS(rc))
        {
            LogFlow(("VRDPServer::loadLibrary(): successfully loaded VRDP library.\n"));

            struct SymbolEntry
            {
                const char *name;
                void **ppfn;
            };

            #define DEFSYMENTRY(a) { #a, (void**)&mpfn##a }

            static const struct SymbolEntry symbols[] =
            {
                DEFSYMENTRY(VRDPStartServer),
                DEFSYMENTRY(VRDPSetFramebuffer),
                DEFSYMENTRY(VRDPSetCallback),
                DEFSYMENTRY(VRDPShutdownServer),
                DEFSYMENTRY(VRDPSendUpdate),
                DEFSYMENTRY(VRDPSendUpdateBitmap),
                DEFSYMENTRY(VRDPSendResize),
                DEFSYMENTRY(VRDPSendAudioSamples),
                DEFSYMENTRY(VRDPSendAudioVolume),
                DEFSYMENTRY(VRDPSendUSBRequest),
                DEFSYMENTRY(VRDPQueryInfo),
                DEFSYMENTRY(VRDPClipboard)
            };

            #undef DEFSYMENTRY

            for (unsigned i = 0; i < ELEMENTS(symbols); i++)
            {
                rc = RTLdrGetSymbol(mVRDPLibrary, symbols[i].name, symbols[i].ppfn);

                AssertMsgRC(rc, ("Error resolving VRDP symbol %s\n", symbols[i].name));

                if (VBOX_FAILURE(rc))
                {
                    break;
                }
            }
        }
        else
        {
            LogFlow(("VRDPServer::loadLibrary(): failed to load VRDP library! VRDP not available.\n"));
            mVRDPLibrary = NULL;
        }
    }

    // just to be safe
    if (VBOX_FAILURE(rc))
    {
        if (mVRDPLibrary)
        {
            RTLdrClose (mVRDPLibrary);
            mVRDPLibrary = NULL;
        }
    }

    return (mVRDPLibrary != NULL);
}
#endif /* VBOX_VRDP */

/*
 * IRemoteDisplayInfo implementation.
 */
// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT RemoteDisplayInfo::FinalConstruct()
{
    return S_OK;
}

void RemoteDisplayInfo::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest object.
 */
HRESULT RemoteDisplayInfo::init (Console *aParent)
{
    LogFlowMember (("RemoteDisplayInfo::init (%p)\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void RemoteDisplayInfo::uninit()
{
    LogFlowMember (("RemoteDisplayInfo::uninit()\n"));

    AutoLock alock (this);
    AssertReturn (isReady(), (void) 0);

    mParent.setNull();

    setReady (false);
}

// IRemoteDisplayInfo properties
/////////////////////////////////////////////////////////////////////////////

#define IMPL_GETTER_BOOL(_aType, _aName, _aIndex)                         \
    STDMETHODIMP RemoteDisplayInfo::COMGETTER(_aName) (_aType *a##_aName) \
    {                                                                     \
        if (!a##_aName)                                                   \
            return E_POINTER;                                             \
                                                                          \
        AutoLock alock (this);                                            \
        CHECK_READY();                                                    \
                                                                          \
        uint32_t value;                                                   \
        uint32_t cbOut = 0;                                               \
                                                                          \
        mParent->consoleVRDPServer ()->QueryInfo                          \
            (_aIndex, &value, sizeof (value), &cbOut);                    \
                                                                          \
        *a##_aName = cbOut? !!value: FALSE;                               \
                                                                          \
        return S_OK;                                                      \
    }

#define IMPL_GETTER_SCALAR(_aType, _aName, _aIndex)                       \
    STDMETHODIMP RemoteDisplayInfo::COMGETTER(_aName) (_aType *a##_aName) \
    {                                                                     \
        if (!a##_aName)                                                   \
            return E_POINTER;                                             \
                                                                          \
        AutoLock alock (this);                                            \
        CHECK_READY();                                                    \
                                                                          \
        _aType value;                                                     \
        uint32_t cbOut = 0;                                               \
                                                                          \
        mParent->consoleVRDPServer ()->QueryInfo                          \
            (_aIndex, &value, sizeof (value), &cbOut);                    \
                                                                          \
        *a##_aName = cbOut? value: 0;                                     \
                                                                          \
        return S_OK;                                                      \
    }

#define IMPL_GETTER_BSTR(_aType, _aName, _aIndex)                         \
    STDMETHODIMP RemoteDisplayInfo::COMGETTER(_aName) (_aType *a##_aName) \
    {                                                                     \
        if (!a##_aName)                                                   \
            return E_POINTER;                                             \
                                                                          \
        AutoLock alock (this);                                            \
        CHECK_READY();                                                    \
                                                                          \
        uint32_t cbOut = 0;                                               \
                                                                          \
        mParent->consoleVRDPServer ()->QueryInfo                          \
            (_aIndex, NULL, 0, &cbOut);                                   \
                                                                          \
        if (cbOut == 0)                                                   \
        {                                                                 \
            Bstr str("");                                                 \
            str.cloneTo (a##_aName);                                      \
            return S_OK;                                                  \
        }                                                                 \
                                                                          \
        char *pchBuffer = (char *)RTMemTmpAlloc (cbOut);                  \
                                                                          \
        if (!pchBuffer)                                                   \
        {                                                                 \
            Log(("RemoteDisplayInfo::"                                    \
                 #_aName                                                  \
                 ": Failed to allocate memory %d bytes\n", cbOut));       \
            return E_OUTOFMEMORY;                                         \
        }                                                                 \
                                                                          \
        mParent->consoleVRDPServer ()->QueryInfo                          \
            (_aIndex, pchBuffer, cbOut, &cbOut);                          \
                                                                          \
        Bstr str(pchBuffer);                                              \
                                                                          \
        str.cloneTo (a##_aName);                                          \
                                                                          \
        RTMemTmpFree (pchBuffer);                                         \
                                                                          \
        return S_OK;                                                      \
    }

IMPL_GETTER_BOOL   (BOOL,    Active,             VRDP_QI_ACTIVE);
IMPL_GETTER_SCALAR (ULONG,   NumberOfClients,    VRDP_QI_NUMBER_OF_CLIENTS);
IMPL_GETTER_SCALAR (LONG64,  BeginTime,          VRDP_QI_BEGIN_TIME);
IMPL_GETTER_SCALAR (LONG64,  EndTime,            VRDP_QI_END_TIME);
IMPL_GETTER_SCALAR (ULONG64, BytesSent,          VRDP_QI_BYTES_SENT);
IMPL_GETTER_SCALAR (ULONG64, BytesSentTotal,     VRDP_QI_BYTES_SENT_TOTAL);
IMPL_GETTER_SCALAR (ULONG64, BytesReceived,      VRDP_QI_BYTES_RECEIVED);
IMPL_GETTER_SCALAR (ULONG64, BytesReceivedTotal, VRDP_QI_BYTES_RECEIVED_TOTAL);
IMPL_GETTER_BSTR   (BSTR,    User,               VRDP_QI_USER);
IMPL_GETTER_BSTR   (BSTR,    Domain,             VRDP_QI_DOMAIN);
IMPL_GETTER_BSTR   (BSTR,    ClientName,         VRDP_QI_CLIENT_NAME);
IMPL_GETTER_BSTR   (BSTR,    ClientIP,           VRDP_QI_CLIENT_IP);
IMPL_GETTER_SCALAR (ULONG,   ClientVersion,      VRDP_QI_CLIENT_VERSION);
IMPL_GETTER_SCALAR (ULONG,   EncryptionStyle,    VRDP_QI_ENCRYPTION_STYLE);

#undef IMPL_GETTER_BSTR
#undef IMPL_GETTER_SCALAR
