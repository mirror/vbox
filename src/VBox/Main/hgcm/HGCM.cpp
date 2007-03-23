/** @file
 *
 * HGCM (Host-Guest Communication Manager)
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


/*
 * NOT FOR REVIEWING YET. A LOT OF TODO/MISSED/INCOMPLETE/SKETCH CODE INSIDE!
 */

#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
#include "Logging.h"

#ifdef __WIN__
#include <malloc.h> /* For alloca() */
#else
#include <alloca.h>
#endif /* __WIN__ */

#include "hgcm/HGCM.h"
#include "hgcm/HGCMThread.h"

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#include <iprt/alloc.h>
#include <iprt/avl.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include <VBox/VBoxGuest.h>

/**
 *
 * Service location types:
 *
 *  LOCAL SERVICE
 *      service DLL is loaded by the VM process,
 *      and directly called by the VM HGCM instance.
 */

/**
 * A service gets one thread, which synchronously delivers messages to
 * the service. This is good for serialization.
 *
 * Some services may want to process messages asynchronously, and will want
 * a next message to be delivered, while a previous message is still being
 * processed.
 *
 * The dedicated service thread delivers a next message when service
 * returns after fetching a previous one. The service will call a message
 * completion callback when message is actually processed. So returning
 * from the service call means only that the service is processing message.
 *
 * 'Message processed' condition is indicated by service, which call the
 * callback, even if the callback is called synchronously in the dedicated
 * thread.
 *
 * This message completion callback is only valid for Call requests.
 * Connect and Disconnect are processed sznchronously by service.
 *
 */

/** @todo services registration, only registered service dll can be loaded */

/** @todo a registered LOCAL service must also get a thread, for now
 * a thread per service (later may be there will be an option to
 * have a thread per client for a service, however I do not think it's
 * really necessary).
 * The requests will be queued and executed by the service thread,
 * an IRQ notification will be ussued when a request is completed.
 *
 * May be other services (like VRDP acceleration) should still use
 * the EMT thread, because they have their own threads for long
 * operations.
 * So we have to distinguish those services during
 * registration process (external/internal registration).
 * External dlls will always have its own thread,
 * internal (trusted) services will choose between having executed
 * on EMT or on a separate thread.
 *
 */

/* The maximum allowed size of a service name in bytes. */
#define VBOX_HGCM_SVC_NAME_MAX_BYTES 1024

/** Internal helper service object. HGCM code would use it to
 *  hold information about services and communicate with services.
 *  The HGCMService is an (in future) abstract class that implements
 *  common functionality. There will be derived classes for specific
 *  service types (Local, etc).
 */

/** @todo should be HGCMObject */
class HGCMService
{
    private:
        VBOXHGCMSVCHELPERS m_svcHelpers;

        static HGCMService *sm_pSvcListHead;
        static HGCMService *sm_pSvcListTail;

#ifdef HGCMSS
       static int sm_cServices;
#endif /* HGCMSS */

        HGCMTHREADHANDLE m_thread;
        friend DECLCALLBACK(void) hgcmServiceThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser);

        uint32_t volatile m_u32RefCnt;

        HGCMService *m_pSvcNext;
        HGCMService *m_pSvcPrev;

        char *m_pszSvcName;
        char *m_pszSvcLibrary;
        
        PPDMIHGCMPORT m_pHGCMPort;

        RTLDRMOD m_hLdrMod;
        PFNVBOXHGCMSVCLOAD m_pfnLoad;

        VBOXHGCMSVCFNTABLE m_fntable;

        int m_cClients;
        int m_cClientsAllocated;

        uint32_t *m_paClientIds;

        int loadServiceDLL (void);
        void unloadServiceDLL (void);

        int InstanceCreate (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort);
        void InstanceDestroy (void);

        HGCMService ();
        ~HGCMService () {};

        bool EqualToLoc (HGCMServiceLocation *loc);

        static DECLCALLBACK(void) svcHlpCallComplete (VBOXHGCMCALLHANDLE callHandle, int32_t rc);
        
    public:

        static void Reset (void);

#ifdef HGCMSS
        static int SaveState (PSSMHANDLE pSSM);
        static int LoadState (PSSMHANDLE pSSM);
#endif /* HGCMSS */
    
        static int FindService (HGCMService **ppsvc, HGCMServiceLocation *loc);
        static HGCMService *FindServiceByName (const char *pszServiceName);
        static int LoadService (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort);
        void ReleaseService (void);

        uint32_t SizeOfClient (void) { return m_fntable.cbClient; };

        void DisconnectAll (void);

#ifdef HGCMSS
        int CreateAndConnectClient (uint32_t *pu32ClientIdOut, uint32_t u32ClientIdIn);
#endif /* HGCMSS */
                    
        int Connect (uint32_t u32ClientID);
        int Disconnect (uint32_t u32ClientID);
        int GuestCall (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[], bool fBlock);
        int HostCall (PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[]);

        int SaveState(uint32_t u32ClientID, PSSMHANDLE pSSM);
        int LoadState(uint32_t u32ClientID, PSSMHANDLE pSSM);
};


class HGCMClient: public HGCMObject
{
    public:
        HGCMClient () : HGCMObject(HGCMOBJ_CLIENT) {};
        ~HGCMClient ();

        int Init (HGCMService *pSvc);

        /** Service that the client is connected to. */
        HGCMService *pService;

        /** Client specific data. */
        void *pvData;
};

HGCMClient::~HGCMClient ()
{
    RTMemFree (pvData);
}

int HGCMClient::Init (HGCMService *pSvc)
{
    pService = pSvc;

    if (pService->SizeOfClient () > 0)
    {
        pvData = RTMemAllocZ (pService->SizeOfClient ());

        if (!pvData)
        {
           return VERR_NO_MEMORY;
        }
    }

    return VINF_SUCCESS;
}


#define HGCM_CLIENT_DATA(pService, pClient) (pClient->pvData)


/*
 * Messages processed by worker threads.
 */

#define HGCMMSGID_SVC_LOAD       (0)
#define HGCMMSGID_SVC_UNLOAD     (1)
#define HGCMMSGID_SVC_CONNECT    (2)
#define HGCMMSGID_SVC_DISCONNECT (3)
#define HGCMMSGID_GUESTCALL      (4)

class HGCMMsgSvcLoad: public HGCMMsgCore
{
};

class HGCMMsgSvcUnload: public HGCMMsgCore
{
};

class HGCMMsgSvcConnect: public HGCMMsgCore
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgSvcDisconnect: public HGCMMsgCore
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgHeader: public HGCMMsgCore
{
    public:
        HGCMMsgHeader () : pCmd (NULL), pHGCMPort (NULL) {};
        
        /* Command pointer/identifier. */
        PVBOXHGCMCMD pCmd;

        /* Port to be informed on message completion. */
        PPDMIHGCMPORT pHGCMPort;
};


class HGCMMsgCall: public HGCMMsgHeader
{
    public:
        /* client identifier */
        uint32_t u32ClientID;

        /* function number */
        uint32_t u32Function;

        /* number of parameters */
        uint32_t cParms;

        VBOXHGCMSVCPARM *paParms;
};


/*
 * Messages processed by main HGCM thread.
 */

#define HGCMMSGID_CONNECT    (10)
#define HGCMMSGID_DISCONNECT (11)
#define HGCMMSGID_LOAD       (12)
#define HGCMMSGID_HOSTCALL   (13)
#define HGCMMSGID_LOADSTATE  (14)
#define HGCMMSGID_SAVESTATE  (15)
#define HGCMMSGID_RESET      (16)

class HGCMMsgConnect: public HGCMMsgHeader
{
    public:
        /* service location */
        HGCMSERVICELOCATION *pLoc;

        /* client identifier */
        uint32_t *pu32ClientID;

};

class HGCMMsgDisconnect: public HGCMMsgHeader
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgLoadSaveState: public HGCMMsgHeader
{
    public:
        /* client identifier */
        uint32_t    u32ClientID;
        PSSMHANDLE  pSSM;
};

class HGCMMsgLoad: public HGCMMsgHeader
{
    public:
        virtual ~HGCMMsgLoad ()
        {
            RTStrFree (pszServiceLibrary);
            RTStrFree (pszServiceName);
        }
        
        int Init (const char *pszName, const char *pszLibrary)
        {
            pszServiceName = RTStrDup (pszName);
            pszServiceLibrary = RTStrDup (pszLibrary);
            
            if (!pszServiceName || !pszServiceLibrary)
            {
                RTStrFree (pszServiceLibrary);
                RTStrFree (pszServiceName);
                pszServiceLibrary = NULL;
                pszServiceName = NULL;
                return VERR_NO_MEMORY;
            }
            
            return VINF_SUCCESS;
        }
        
        char *pszServiceName;
        char *pszServiceLibrary;
};

class HGCMMsgHostCall: public HGCMMsgHeader
{
    public:
        char *pszServiceName;
        
        /* function number */
        uint32_t u32Function;

        /* number of parameters */
        uint32_t cParms;

        VBOXHGCMSVCPARM *paParms;
};

class HGCMMsgReset: public HGCMMsgHeader
{
};

/* static */ DECLCALLBACK(void) HGCMService::svcHlpCallComplete (VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
   HGCMMsgCore *pMsgCore = (HGCMMsgCore *)callHandle;

   if (   pMsgCore->MsgId () == HGCMMSGID_GUESTCALL
       || pMsgCore->MsgId () == HGCMMSGID_HOSTCALL)
   {
       /* Only call the completion for these messages. The helper 
        * is called by the service, and the service does not get
        * any other messages.
        */
       hgcmMsgComplete (pMsgCore, rc);
   }
   else
   {
       AssertFailed ();
   }
}

HGCMService *HGCMService::sm_pSvcListHead = NULL;
HGCMService *HGCMService::sm_pSvcListTail = NULL;
#ifdef HGCMSS
int HGCMService::sm_cServices = 0;
#endif /* HGCMSS */

HGCMService::HGCMService ()
    :
    m_thread     (0),
    m_u32RefCnt  (0),
    m_pSvcNext   (NULL),
    m_pSvcPrev   (NULL),
    m_pszSvcName (NULL),
    m_pszSvcLibrary (NULL),
    m_hLdrMod    (NIL_RTLDRMOD),
    m_pfnLoad    (NULL),
    m_cClients   (0),
    m_cClientsAllocated (0),
    m_paClientIds (NULL)
{
    memset (&m_fntable, 0, sizeof (m_fntable));
}


HGCMMsgCore *hgcmMessageAlloc (uint32_t u32MsgId)
{
    switch (u32MsgId)
    {
        case HGCMMSGID_SVC_LOAD:        return new HGCMMsgSvcLoad ();
        case HGCMMSGID_SVC_UNLOAD:      return new HGCMMsgSvcUnload ();
        case HGCMMSGID_SVC_CONNECT:     return new HGCMMsgSvcConnect ();
        case HGCMMSGID_SVC_DISCONNECT:  return new HGCMMsgSvcDisconnect ();
        case HGCMMSGID_GUESTCALL:       return new HGCMMsgCall ();

        case HGCMMSGID_CONNECT:         return new HGCMMsgConnect ();
        case HGCMMSGID_DISCONNECT:      return new HGCMMsgDisconnect ();
        case HGCMMSGID_LOAD:            return new HGCMMsgLoad ();
        case HGCMMSGID_HOSTCALL:        return new HGCMMsgHostCall ();
        case HGCMMSGID_LOADSTATE:      
        case HGCMMSGID_SAVESTATE:       return new HGCMMsgLoadSaveState ();
        case HGCMMSGID_RESET:           return new HGCMMsgReset ();
        default:
            Log(("hgcmMessageAlloc::Unsupported message number %08X\n", u32MsgId));
            AssertReleaseMsgFailed(("Msg id = %08X\n", u32MsgId));
    }

    return NULL;
}

static bool g_fResetting = false;
static bool g_fSaveState = false;

static DECLCALLBACK(void) hgcmMsgCompletionCallback (int32_t result, HGCMMsgCore *pMsgCore)
{
    /* Call the VMMDev port interface to issue IRQ notification. */
    HGCMMsgHeader *pMsgHdr = (HGCMMsgHeader *)pMsgCore;

    LogFlow(("MAIN::hgcmMsgCompletionCallback: message %p\n", pMsgCore));

    if (pMsgHdr->pHGCMPort && !g_fResetting)
    {
        pMsgHdr->pHGCMPort->pfnCompleted (pMsgHdr->pHGCMPort, g_fSaveState? VINF_HGCM_SAVE_STATE: result, pMsgHdr->pCmd);
    }

    return;
}


DECLCALLBACK(void) hgcmServiceThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser)
{
    int rc = VINF_SUCCESS;

    HGCMService *pSvc = (HGCMService *)pvUser;

    AssertRelease(pSvc != NULL);

    HGCMMsgCore *pMsgCore = NULL;

    bool bUnloaded = false;

    while (!bUnloaded)
    {
        rc = hgcmMsgGet (ThreadHandle, &pMsgCore);

        if (VBOX_FAILURE (rc))
        {
            Log(("hgcmServiceThread: message get failed, rc = %Vrc\n", rc));

            RTThreadSleep(100);

            continue;
        }

        /* Cache required information to avoid unnecessary pMsgCore access. */
        uint32_t u32MsgId = pMsgCore->MsgId ();

        switch (u32MsgId)
        {
            case HGCMMSGID_SVC_LOAD:
            {
                LogFlow(("HGCMMSGID_SVC_LOAD\n"));
                rc = pSvc->loadServiceDLL ();
            } break;

            case HGCMMSGID_SVC_UNLOAD:
            {
                LogFlow(("HGCMMSGID_SVC_UNLOAD\n"));
                pSvc->unloadServiceDLL ();
                bUnloaded = true;
            } break;

            case HGCMMSGID_SVC_CONNECT:
            {
                LogFlow(("HGCMMSGID_SVC_CONNECT\n"));

                HGCMMsgSvcConnect *pMsg = (HGCMMsgSvcConnect *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                if (pClient)
                {
                    rc = pSvc->m_fntable.pfnConnect (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient));

                    hgcmObjDereference (pClient);
                }
            } break;

            case HGCMMSGID_SVC_DISCONNECT:
            {
                LogFlow(("HGCMMSGID_SVC_DISCONNECT\n"));

                HGCMMsgSvcDisconnect *pMsg = (HGCMMsgSvcDisconnect *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                if (pClient)
                {
                    rc = pSvc->m_fntable.pfnDisconnect (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient));

                    hgcmObjDereference (pClient);
                }
            } break;

            case HGCMMSGID_GUESTCALL:
            {
                LogFlow(("HGCMMSGID_GUESTCALL\n"));

                HGCMMsgCall *pMsg = (HGCMMsgCall *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                if (pClient)
                {
                    pSvc->m_fntable.pfnCall ((VBOXHGCMCALLHANDLE)pMsg, pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient), pMsg->u32Function, pMsg->cParms, pMsg->paParms);

                    hgcmObjDereference (pClient);
                }
                else
                {
                    rc = VERR_HGCM_INVALID_CLIENT_ID;
                }
            } break;

            case HGCMMSGID_HOSTCALL:
            {
                LogFlow(("HGCMMSGID_HOSTCALL\n"));

                HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)pMsgCore;

                pSvc->m_fntable.pfnHostCall ((VBOXHGCMCALLHANDLE)pMsg, 0, NULL, pMsg->u32Function, pMsg->cParms, pMsg->paParms);

                rc = VINF_SUCCESS;
            } break;

            case HGCMMSGID_LOADSTATE:
            {
                LogFlow(("HGCMMSGID_LOADSTATE\n"));

                HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)pMsgCore;
                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                rc = VINF_SUCCESS;

                if (pClient)
                {
                    if (pSvc->m_fntable.pfnLoadState)
                    {
                        rc = pSvc->m_fntable.pfnLoadState (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient), pMsg->pSSM);
                    }

                    hgcmObjDereference (pClient);
                }
                break;
            }

            case HGCMMSGID_SAVESTATE:
            {
                LogFlow(("HGCMMSGID_SAVESTATE\n"));

                HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)pMsgCore;
                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                rc = VINF_SUCCESS;

                if (pClient)
                {
                    if (pSvc->m_fntable.pfnSaveState)
                    {
                        g_fSaveState = true;
                        rc = pSvc->m_fntable.pfnSaveState (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient), pMsg->pSSM);
                        g_fSaveState = false;
                    }

                    hgcmObjDereference (pClient);
                }
                break;
            }

            default:
            {
                Log(("hgcmServiceThread::Unsupported message number %08X\n", u32MsgId));
                rc = VERR_NOT_SUPPORTED;
            } break;
        }

        if (   u32MsgId != HGCMMSGID_GUESTCALL
            && u32MsgId != HGCMMSGID_HOSTCALL)
        {
            /* For HGCMMSGID_GUESTCALL & HGCMMSGID_HOSTCALL the service
             * calls the completion helper. Other messages have to be
             * completed here.
             */
            hgcmMsgComplete (pMsgCore, rc);
        }
    }

    return;
}

int HGCMService::InstanceCreate (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort)
{
    int rc = VINF_SUCCESS;

    LogFlow(("HGCMService::InstanceCreate: name %s, lib %s\n", pszServiceName, pszServiceLibrary));

    char achThreadName[14];

    RTStrPrintf (achThreadName, sizeof (achThreadName), "HGCM%08X", this);

    rc = hgcmThreadCreate (&m_thread, achThreadName, hgcmServiceThread, this);

    if (VBOX_SUCCESS(rc))
    {
        m_pszSvcName    = RTStrDup (pszServiceName);
        m_pszSvcLibrary = RTStrDup (pszServiceLibrary);

        if (!m_pszSvcName || !m_pszSvcLibrary)
        {
            RTStrFree (m_pszSvcLibrary);
            m_pszSvcLibrary = NULL;
            
            RTStrFree (m_pszSvcName);
            m_pszSvcName = NULL;
            
            rc = VERR_NO_MEMORY;
        }
        else
        {
            m_pHGCMPort = pHGCMPort;
            
            m_svcHelpers.pfnCallComplete = svcHlpCallComplete;
            m_svcHelpers.pvInstance      = this;
            
            /* Execute the load request on the service thread. */
            HGCMMSGHANDLE hMsg;

            rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_LOAD, hgcmMessageAlloc);

            if (VBOX_SUCCESS(rc))
            {
                rc = hgcmMsgSend (hMsg);
            }
        }
    }
    else
    {
        Log(("HGCMService::InstanceCreate: FAILURE: service thread creation\n"));
    }

    if (VBOX_FAILURE(rc))
    {
        InstanceDestroy ();
    }

    LogFlow(("HGCMService::InstanceCreate rc = %Vrc\n", rc));

    return rc;
}

void HGCMService::InstanceDestroy (void)
{
    HGCMMSGHANDLE hMsg;

    LogFlow(("HGCMService::InstanceDestroy\n"));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_UNLOAD, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        rc = hgcmMsgSend (hMsg);
    }

    RTStrFree (m_pszSvcLibrary);
    m_pszSvcLibrary = NULL;
    RTStrFree (m_pszSvcName);
    m_pszSvcName = NULL;
    
    // @todo Adjust the list sm_cServices--;

    LogFlow(("HGCMService::InstanceDestroy completed\n"));
}

bool HGCMService::EqualToLoc (HGCMServiceLocation *loc)
{
    if (!loc || (loc->type != VMMDevHGCMLoc_LocalHost && loc->type != VMMDevHGCMLoc_LocalHost_Existing))
    {
        return false;
    }

    if (strcmp (m_pszSvcName, loc->u.host.achName) != 0)
    {
        return false;
    }

    return true;
}

/** Services are searched by FindService function which is called
 *  by the main HGCM thread during CONNECT message processing.
 *  Reference count of the service is increased.
 *  Services are loaded by the LoadService function.
 *  Note: both methods are executed by the main HGCM thread.
 */
/* static */ int HGCMService::FindService (HGCMService **ppsvc, HGCMServiceLocation *loc)
{
    HGCMService *psvc = NULL;

    LogFlow(("HGCMService::FindService: loc = %p\n", loc));

    if (!loc || (loc->type != VMMDevHGCMLoc_LocalHost && loc->type != VMMDevHGCMLoc_LocalHost_Existing))
    {
        return VERR_INVALID_PARAMETER;
    }

    LogFlow(("HGCMService::FindService: name %s\n", loc->u.host.achName));

    /* Look at already loaded services. */
    psvc = sm_pSvcListHead;

    while (psvc)
    {
        if (psvc->EqualToLoc (loc))
        {
            break;
        }

        psvc = psvc->m_pSvcNext;
    }

    LogFlow(("HGCMService::FindService: lookup in the list is %p\n", psvc));
    
    if (psvc)
    {
        ASMAtomicIncU32 (&psvc->m_u32RefCnt);
        
        *ppsvc = psvc;
        
        return VINF_SUCCESS;
    }
    
    return VERR_ACCESS_DENIED;
}

/* static */ HGCMService *HGCMService::FindServiceByName (const char *pszServiceName)
{
    HGCMService *psvc = sm_pSvcListHead;

    while (psvc)
    {
        if (strcmp (psvc->m_pszSvcName, pszServiceName) == 0)
        {
            break;
        }

        psvc = psvc->m_pSvcNext;
    }
    
    return psvc;
}

/* static */ int HGCMService::LoadService (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort)
{
    int rc = VINF_SUCCESS;

    HGCMService *psvc = NULL;

    LogFlow(("HGCMService::LoadService: name = %s, lib %s\n", pszServiceName, pszServiceLibrary));
    
    /* Look at already loaded services to avoid double loading. */
    psvc = FindServiceByName (pszServiceName);

    if (psvc)
    {
        LogFlow(("HGCMService::LoadService: Service already exists %p!!!\n", psvc));
    }
    else
    {
        psvc = new HGCMService ();

        if (!psvc)
        {
            Log(("HGCMService::Load: memory allocation for the service failed!!!\n"));
            rc = VERR_NO_MEMORY;
        }

        if (VBOX_SUCCESS(rc))
        {
            rc = psvc->InstanceCreate (pszServiceLibrary, pszServiceName, pHGCMPort);

            if (VBOX_SUCCESS(rc))
            {
                /* Insert the just created service to list for future references. */
                psvc->m_pSvcNext = sm_pSvcListHead;
                psvc->m_pSvcPrev = NULL;
                
                if (sm_pSvcListHead)
                {
                    sm_pSvcListHead->m_pSvcPrev = psvc;
                }
                else
                {
                    sm_pSvcListTail = psvc;
                }

                sm_pSvcListHead = psvc;
                
#ifdef HGCMSS
                sm_cServices++;
#endif /* HGCMSS */
                
                LogFlow(("HGCMService::LoadService: service %p\n", psvc));
            }
        }
    }

    return rc;
}

/* static */ void HGCMService::Reset (void)
{
    /* This is called when the VM is being reset,
     * that is no more requests from guest is expected.
     * Scan al services and disconnect all clients.
     */
    
    g_fResetting = true;

    HGCMService *psvc = sm_pSvcListHead;

    while (psvc)
    {
        psvc->DisconnectAll ();

        psvc = psvc->m_pSvcNext;
    }
    
    g_fResetting = false;
}

#ifdef HGCMSS
/* static */ int HGCMService::SaveState (PSSMHANDLE pSSM)
{
    /* Save the current handle count and restore afterwards to avoid client id conflicts. */
    int rc = SSMR3PutU32(pSSM, hgcmObjQueryHandleCount());
    AssertRCReturn(rc, rc);

    LogFlowFunc(("%d services to be saved:\n", sm_cServices));
    
    /* Save number of services. */
    rc = SSMR3PutU32(pSSM, sm_cServices);
    AssertRCReturn(rc, rc);

    /* Save every service. */
    HGCMService *pSvc = sm_pSvcListHead;

    while (pSvc)
    {
        LogFlowFunc(("Saving service [%s]\n", pSvc->m_pszSvcName));

        /* Save the length of the service name. */
        rc = SSMR3PutU32(pSSM, strlen(pSvc->m_pszSvcName) + 1);
        AssertRCReturn(rc, rc);

        /* Save the name of the service. */
        rc = SSMR3PutStrZ(pSSM, pSvc->m_pszSvcName);
        AssertRCReturn(rc, rc);

        /* Save the number of clients. */
        rc = SSMR3PutU32(pSSM, pSvc->m_cClients);
        AssertRCReturn(rc, rc);

        /* Call the service for every client. Normally a service must not have
         * a global state to be saved: only per client info is relevant.
         * The global state of a service is configured during VM startup.
         */
        int i;

        for (i = 0; i < pSvc->m_cClients; i++)
        {
            uint32_t u32ClientID = pSvc->m_paClientIds[i];

            Log(("client id 0x%08X\n", u32ClientID));

            /* Save the client id. */
            rc = SSMR3PutU32(pSSM, u32ClientID);
            AssertRCReturn(rc, rc);

            /* Call the service, so the operation is executed by the service thread. */
            rc = pSvc->SaveState (u32ClientID, pSSM);
            AssertRCReturn(rc, rc);
        }

        pSvc = pSvc->m_pSvcNext;
    }

    return VINF_SUCCESS;
}

/* static */ int HGCMService::LoadState (PSSMHANDLE pSSM)
{
    /* Restore handle count to avoid client id conflicts. */
    uint32_t u32;

    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);

    hgcmObjSetHandleCount(u32);

    /* Get the number of services. */
    uint32_t cServices;

    rc = SSMR3GetU32(pSSM, &cServices);
    AssertRCReturn(rc, rc);
    
    LogFlowFunc(("%d services to be restored:\n", cServices));

    while (cServices--)
    {
        /* Get the length of the service name. */
        rc = SSMR3GetU32(pSSM, &u32);
        AssertRCReturn(rc, rc);
        AssertReturn(u32 <= VBOX_HGCM_SVC_NAME_MAX_BYTES, VERR_SSM_UNEXPECTED_DATA);
        
        char *pszServiceName = (char *)alloca (u32);

        /* Get the service name. */
        rc = SSMR3GetStrZ(pSSM, pszServiceName, u32);
        AssertRCReturn(rc, rc);
        
        LogFlowFunc(("Restoring service [%s]\n", pszServiceName));
    
        /* Resolve the service instance. */
        HGCMService *pSvc = FindServiceByName (pszServiceName);
        AssertReturn(pSvc, VERR_SSM_UNEXPECTED_DATA);
            
        /* Get the number of clients. */
        uint32_t cClients;
        rc = SSMR3GetU32(pSSM, &cClients);
        AssertRCReturn(rc, rc);
        
        while (cClients--)
        {
            /* Get the client id. */
            uint32_t u32ClientID;
            rc = SSMR3GetU32(pSSM, &u32ClientID);
            AssertRCReturn(rc, rc);

            /* Connect the client. */
            rc = pSvc->CreateAndConnectClient (NULL, u32ClientID);
            AssertRCReturn(rc, rc);

            /* Call the service, so the operation is executed by the service thread. */
            rc = pSvc->LoadState (u32ClientID, pSSM);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}
#endif /* HGCMSS */

void HGCMService::ReleaseService (void)
{
    uint32_t u32RefCnt = ASMAtomicDecU32 (&m_u32RefCnt);

    AssertRelease(u32RefCnt != ~0U);

    if (u32RefCnt == 0)
    {
        /** @todo We do not unload services. Cache them all. Unloading will be later. HGCMObject! */

        LogFlow(("HGCMService::ReleaseService: no more references.\n"));

//        InstanceDestroy ();

//        delete this;
    }
}

/** Helper function to load a local service DLL.
 *
 *  @return VBox code
 */
int HGCMService::loadServiceDLL (void)
{
    LogFlow(("HGCMService::loadServiceDLL: m_pszSvcLibrary = %s\n", m_pszSvcLibrary));

    if (m_pszSvcLibrary == NULL)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    rc = RTLdrLoad (m_pszSvcLibrary, &m_hLdrMod);

    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("HGCMService::loadServiceDLL: successfully loaded the library.\n"));

        m_pfnLoad = NULL;

        rc = RTLdrGetSymbol (m_hLdrMod, VBOX_HGCM_SVCLOAD_NAME, (void**)&m_pfnLoad);

        if (VBOX_FAILURE (rc) || !m_pfnLoad)
        {
            Log(("HGCMService::loadServiceDLL: Error resolving the service entry point %s, rc = %d, m_pfnLoad = %p\n", VBOX_HGCM_SVCLOAD_NAME, rc, m_pfnLoad));

            if (VBOX_SUCCESS(rc))
            {
                /* m_pfnLoad was NULL */
                rc = VERR_SYMBOL_NOT_FOUND;
            }
        }

        if (VBOX_SUCCESS(rc))
        {
            memset (&m_fntable, 0, sizeof (m_fntable));

            m_fntable.cbSize     = sizeof (m_fntable);
            m_fntable.u32Version = VBOX_HGCM_SVC_VERSION;
            m_fntable.pHelpers   = &m_svcHelpers;

            rc = m_pfnLoad (&m_fntable);

            LogFlow(("HGCMService::loadServiceDLL: m_pfnLoad rc = %Vrc\n", rc));

            if (VBOX_SUCCESS (rc))
            {
                if (   m_fntable.pfnUnload == NULL
                    || m_fntable.pfnConnect == NULL
                    || m_fntable.pfnDisconnect == NULL
                    || m_fntable.pfnCall == NULL
                   )
                {
                    Log(("HGCMService::loadServiceDLL: at least one of function pointers is NULL\n"));

                    rc = VERR_INVALID_PARAMETER;

                    if (m_fntable.pfnUnload)
                    {
                        m_fntable.pfnUnload ();
                    }
                }
            }
        }
    }
    else
    {
        LogFlow(("HGCMService::loadServiceDLL: failed to load service library. The service is not available.\n"));
        m_hLdrMod = NIL_RTLDRMOD;
    }

    if (VBOX_FAILURE(rc))
    {
        unloadServiceDLL ();
    }

    return rc;
}

/** Helper function to free a local service DLL.
 *
 *  @return VBox code
 */
void HGCMService::unloadServiceDLL (void)
{
    if (m_hLdrMod)
    {
        RTLdrClose (m_hLdrMod);
    }

    memset (&m_fntable, 0, sizeof (m_fntable));
    m_pfnLoad = NULL;
    m_hLdrMod = NIL_RTLDRMOD;
}

#ifdef HGCMSS
/* Create a new client instance and connect it to the service.
 *
 * @param pu32ClientIdOut If not NULL, then the method must generate a new handle for the client.
 *                        If NULL, use the given 'u32ClientIdIn' handle.
 * @param u32ClientIdIn   The handle for the client, when 'pu32ClientIdOut' is NULL.
 *
 */
int HGCMService::CreateAndConnectClient (uint32_t *pu32ClientIdOut, uint32_t u32ClientIdIn)
{
    /* Allocate a client information structure */
    HGCMClient *pClient = new HGCMClient ();

    if (!pClient)
    {
        LogWarningFunc(("Could not allocate HGCMClient!!!\n"));
        return VERR_NO_MEMORY;
    }

    uint32_t handle;
    
    if (pu32ClientIdOut != NULL)
    {
        handle = hgcmObjGenerateHandle (pClient);
    }
    else
    {
        handle = hgcmObjAssignHandle (pClient, u32ClientIdIn);
    }

    AssertRelease(handle);

    int rc = pClient->Init (this);

    if (VBOX_SUCCESS(rc))
    {
        rc = Connect (handle);
    }

    if (VBOX_FAILURE(rc))
    {
        hgcmObjDeleteHandle (handle);
    }
    else
    {
        if (pu32ClientIdOut != NULL)
        {
            *pu32ClientIdOut = handle;
        }
    }
    
    return rc;
}
#endif /* HGCMSS */

int HGCMService::Connect (uint32_t u32ClientID)
{
    HGCMMSGHANDLE hMsg;

    LogFlow(("MAIN::HGCMService::Connect: client id = %d\n", u32ClientID));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_CONNECT, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgSvcConnect *pMsg = (HGCMMsgSvcConnect *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
        
        if (VBOX_SUCCESS (rc))
        {
            /* Add the client Id to the array. */
            if (m_cClients == m_cClientsAllocated)
            {
                m_paClientIds = (uint32_t *)RTMemRealloc (m_paClientIds, (m_cClientsAllocated + 64) * sizeof (m_paClientIds[0]));
                Assert(m_paClientIds);
            }
            
            m_paClientIds[m_cClients] = u32ClientID;
            m_cClients++;
        }
    }
    else
    {
        Log(("MAIN::HGCMService::Connect: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

int HGCMService::Disconnect (uint32_t u32ClientID)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::HGCMService::Disconnect: client id = %d\n", u32ClientID));

    HGCMMSGHANDLE hMsg;

    rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_DISCONNECT, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgSvcDisconnect *pMsg = (HGCMMsgSvcDisconnect *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
        
        /* Remove the client id from the array. */
        int i;
        for (i = 0; i < m_cClients; i++)
        {
            if (m_paClientIds[i] == u32ClientID)
            {
                m_cClients--;
                
                if (m_cClients > i)
                {
                    memmove (&m_paClientIds[i], &m_paClientIds[i + 1], m_cClients - i);
                }
                
                break;
            }
        }
    }
    else
    {
        Log(("MAIN::HGCMService::Disconnect: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

void HGCMService::DisconnectAll (void)
{
    while (m_cClients && m_paClientIds)
    {
        Log(("MAIN::HGCMService::DisconnectAll: id %d\n", m_paClientIds[0]));
        Disconnect (m_paClientIds[0]);
    }
}

/* Forward the call request to the dedicated service thread.
 */
int HGCMService::GuestCall (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fBlock)
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::Call\n"));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_GUESTCALL, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgCall *pMsg = (HGCMMsgCall *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->u32ClientID      = u32ClientID;
        pMsg->u32Function      = u32Function;
        pMsg->cParms           = cParms;
        pMsg->paParms = paParms;

        hgcmObjDereference (pMsg);

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::HGCMService::Call: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

/* Forward the call request to the dedicated service thread.
 */
int HGCMService::HostCall (PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::HostCall %s\n", m_pszSvcName));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_HOSTCALL, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pCmd      = NULL; /* Not used for host calls. */
        pMsg->pHGCMPort = NULL; /* Not used for host calls. */

        pMsg->u32Function      = u32Function;
        pMsg->cParms           = cParms;
        pMsg->paParms          = paParms;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::Call: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

int HGCMService::SaveState(uint32_t u32ClientID, PSSMHANDLE pSSM)
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::SaveState %s\n", m_pszSvcName));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SAVESTATE, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;
        pMsg->pSSM        = pSSM;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::SaveState: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

int HGCMService::LoadState (uint32_t u32ClientID, PSSMHANDLE pSSM)
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::LoadState %s\n", m_pszSvcName));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_LOADSTATE, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;
        pMsg->pSSM        = pSSM;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::LoadState: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

/* Main HGCM thread that processes CONNECT/DISCONNECT requests. */
static DECLCALLBACK(void) hgcmThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser)
{
    NOREF(pvUser);

    int rc = VINF_SUCCESS;

    HGCMMsgCore *pMsgCore = NULL;

    for (;;)
    {
        LogFlow(("hgcmThread: Going to get a message\n"));

        rc = hgcmMsgGet (ThreadHandle, &pMsgCore);

        if (VBOX_FAILURE (rc))
        {
            Log(("hgcmThread: message get failed, rc = %Vrc\n"));
            RTThreadSleep(100);
            continue;
        }

        uint32_t u32MsgId = pMsgCore->MsgId ();

        switch (u32MsgId)
        {
            case HGCMMSGID_CONNECT:
            {
                LogFlow(("HGCMMSGID_CONNECT\n"));

                HGCMMsgConnect *pMsg = (HGCMMsgConnect *)pMsgCore;

                /* Search if the service exists.
                 * Create an information structure for the client.
                 * Inform the service about the client.
                 * Generate and return the client id.
                 */

                Log(("MAIN::hgcmThread:HGCMMSGID_CONNECT: location type = %d\n", pMsg->pLoc->type));

                HGCMService *pService = NULL;

                rc = HGCMService::FindService (&pService, pMsg->pLoc);

                if (VBOX_SUCCESS (rc))
                {
#ifdef HGCMSS
                    rc = pService->CreateAndConnectClient (pMsg->pu32ClientID, 0);
#else
                    /* Allocate a client information structure */

                    HGCMClient *pClient = new HGCMClient ();

                    if (!pClient)
                    {
                        Log(("hgcmConnect::Could not allocate HGCMClient\n"));
                        rc = VERR_NO_MEMORY;
                    }
                    else
                    {
                        uint32_t handle = hgcmObjGenerateHandle (pClient);

                        AssertRelease(handle);

                        rc = pClient->Init (pService);

                        if (VBOX_SUCCESS(rc))
                        {
                            rc = pService->Connect (handle);
                        }

                        if (VBOX_FAILURE(rc))
                        {
                            hgcmObjDeleteHandle (handle);
                        }
                        else
                        {
                            *pMsg->pu32ClientID = handle;
                        }
                    }
#endif /* HGCMSS */
                }

                if (VBOX_FAILURE(rc))
                {
                    LogFlow(("HGCMMSGID_CONNECT: FAILURE rc = %Vrc\n", rc));

                    if (pService)
                    {
                        pService->ReleaseService ();
                    }
                }

            } break;

            case HGCMMSGID_DISCONNECT:
            {
                LogFlow(("HGCMMSGID_DISCONNECT\n"));

                HGCMMsgDisconnect *pMsg = (HGCMMsgDisconnect *)pMsgCore;

                Log(("MAIN::hgcmThread:HGCMMSGID_DISCONNECT: client id = %d\n", pMsg->u32ClientID));

                /* Forward call to the service dedicated HGCM thread. */
                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID, HGCMOBJ_CLIENT);

                if (!pClient)
                {
                    Log(("MAIN::hgcmThread:HGCMMSGID_DISCONNECT: FAILURE resolving client id\n"));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = pClient->pService->Disconnect (pMsg->u32ClientID);

                    pClient->pService->ReleaseService ();

                    hgcmObjDereference (pClient);

                    hgcmObjDeleteHandle (pMsg->u32ClientID);
                }

            } break;

            case HGCMMSGID_LOAD:
            {
                LogFlow(("HGCMMSGID_LOAD\n"));

                HGCMMsgLoad *pMsg = (HGCMMsgLoad *)pMsgCore;

                rc = HGCMService::LoadService (pMsg->pszServiceName, pMsg->pszServiceLibrary, pMsg->pHGCMPort);
            } break;

            case HGCMMSGID_HOSTCALL:
            {
                LogFlow(("HGCMMSGID_HOSTCALL at hgcmThread\n"));

                HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)pMsgCore;

                HGCMService *pService = HGCMService::FindServiceByName (pMsg->pszServiceName);

                if (pService)
                {
                    LogFlow(("HGCMMSGID_HOSTCALL found service, forwarding the call.\n"));
                    pService->HostCall (NULL, 0, pMsg->u32Function, pMsg->cParms, pMsg->paParms);
                }
            } break;

            case HGCMMSGID_RESET:
            {
                LogFlow(("HGCMMSGID_RESET\n"));

                HGCMMsgReset *pMsg = (HGCMMsgReset *)pMsgCore;

                HGCMService::Reset ();
            } break;

#ifdef HGCMSS
            case HGCMMSGID_LOADSTATE:
            {
                LogFlow(("HGCMMSGID_LOADSTATE\n"));

                HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)pMsgCore;

                rc = HGCMService::LoadState (pMsg->pSSM);
            } break;

            case HGCMMSGID_SAVESTATE:
            {
                LogFlow(("HGCMMSGID_SAVESTATE\n"));

                HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)pMsgCore;

                rc = HGCMService::SaveState (pMsg->pSSM);
            } break;
#endif /* HGCMSS */
            
            default:
            {
                Log(("hgcmThread: Unsupported message number %08X!!!\n", u32MsgId));
                rc = VERR_NOT_SUPPORTED;
            } break;
        }

        hgcmMsgComplete (pMsgCore, rc);
    }

    return;
}

static HGCMTHREADHANDLE g_hgcmThread = 0;

/*
 * Find a service and inform it about a client connection.
 * Main HGCM thread will process this request for serialization.
 */
int hgcmConnectInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pLoc, uint32_t *pu32ClientID, bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmConnectInternal: pHGCMPort = %p, pCmd = %p, loc = %p, pu32ClientID = %p\n",
             pHGCMPort, pCmd, pLoc, pu32ClientID));

    if (!pHGCMPort || !pCmd || !pLoc || !pu32ClientID)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_CONNECT, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgConnect *pMsg = (HGCMMsgConnect *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->pLoc = pLoc;
        pMsg->pu32ClientID = pu32ClientID;

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcmConnectInternal: hgcmMsgPost returned %Vrc\n", rc));

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::hgcmConnectInternal:Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

/*
 * Tell a service that the client is disconnecting.
 * Main HGCM thread will process this request for serialization.
 */
int hgcmDisconnectInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmDisconnectInternal: pHGCMPort = %p, pCmd = %p, u32ClientID = %d\n",
             pHGCMPort, pCmd, u32ClientID));

    if (!pHGCMPort || !pCmd)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_DISCONNECT, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgDisconnect *pMsg = (HGCMMsgDisconnect *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->u32ClientID = u32ClientID;

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        hgcmObjDereference (pMsg);

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::hgcmDisconnectInternal: Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

#ifdef HGCMSS
static int hgcmLoadSaveStateSend (PSSMHANDLE pSSM, uint32_t u32MsgId)
{
    /* Forward the request to the main HGCM thread. */
    LogFlowFunc(("u32MsgId = %d\n", u32MsgId));

    HGCMMSGHANDLE hMsg = 0;

    int rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, u32MsgId, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgLoadSaveState *pMsg = (HGCMMsgLoadSaveState *)hgcmObjReference (hMsg, HGCMOBJ_MSG);
        AssertRelease(pMsg);

        pMsg->u32ClientID = 0; /* @todo not used. */
        pMsg->pSSM        = pSSM;

        rc = hgcmMsgSend (hMsg);

        hgcmObjDereference (pMsg);
    }

    LogFlowFunc(("rc = %Vrc\n", rc));

    return rc;
}

int hgcmSaveStateInternal (PSSMHANDLE pSSM)
{
    return hgcmLoadSaveStateSend (pSSM, HGCMMSGID_SAVESTATE);
}

int hgcmLoadStateInternal (PSSMHANDLE pSSM)
{
    return hgcmLoadSaveStateSend (pSSM, HGCMMSGID_LOADSTATE);
}
#else
int hgcmSaveStateInternal (uint32_t u32ClientID, PSSMHANDLE pSSM)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmSaveStateInternal: u32ClientID = %d\n", u32ClientID));

    HGCMClient *pClient = (HGCMClient *)hgcmObjReference (u32ClientID, HGCMOBJ_CLIENT);

    if (!pClient)
    {
        Log(("MAIN::hgcmCallInternal: FAILURE resolving client id %d\n", u32ClientID));
        return VERR_HGCM_INVALID_CLIENT_ID;
    }

    AssertRelease(pClient->pService);

    rc = pClient->pService->SaveState (u32ClientID, pSSM);

    hgcmObjDereference (pClient);

    return rc;
}

int hgcmLoadStateInternal (uint32_t u32ClientID, PSSMHANDLE pSSM)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmLoadStateInternal: u32ClientID = %d\n", u32ClientID));

    HGCMClient *pClient = (HGCMClient *)hgcmObjReference (u32ClientID, HGCMOBJ_CLIENT);

    if (!pClient)
    {
        Log(("MAIN::hgcmCallInternal: FAILURE resolving client id %d\n", u32ClientID));
        return VERR_HGCM_INVALID_CLIENT_ID;
    }

    AssertRelease(pClient->pService);

    rc = pClient->pService->LoadState (u32ClientID, pSSM);

    hgcmObjDereference (pClient);
    return rc;
}
#endif /* HGCMSS */

int hgcmLoadInternal (const char *pszServiceName, const char *pszServiceLibrary)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmLoadInternal: name = %s, lib = %s\n",
             pszServiceName, pszServiceLibrary));

    if (!pszServiceName || !pszServiceLibrary)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_LOAD, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgLoad *pMsg = (HGCMMsgLoad *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pHGCMPort = NULL; /* Not used by the call. */
        
        rc = pMsg->Init (pszServiceName, pszServiceLibrary);
        
        if (VBOX_SUCCESS (rc))
        {
            rc = hgcmMsgSend (hMsg);
        }

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcm:LoadInternal: hgcmMsgSend returned %Vrc\n", rc));
    }
    else
    {
        Log(("MAIN::hgcmLoadInternal:Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

/*
 * Call a service.
 * The service dedicated thread will process this request.
 */
int hgcmGuestCallInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[], bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmCallInternal: pHGCMPort = %p, pCmd = %p, u32ClientID = %d, u32Function = %d, cParms = %d, aParms = %p\n",
             pHGCMPort, pCmd, u32ClientID, u32Function, cParms, aParms));

    if (!pHGCMPort || !pCmd)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMClient *pClient = (HGCMClient *)hgcmObjReference (u32ClientID, HGCMOBJ_CLIENT);

    if (!pClient)
    {
        Log(("MAIN::hgcmCallInternal: FAILURE resolving client id %d\n", u32ClientID));
        return VERR_HGCM_INVALID_CLIENT_ID;
    }

    AssertRelease(pClient->pService);

    rc = pClient->pService->GuestCall (pHGCMPort, pCmd, u32ClientID, u32Function, cParms, aParms, fBlock);

    hgcmObjDereference (pClient);

    return rc;
}

/*
 * Call a service.
 * The service dedicated thread will process this request.
 */
int hgcmHostCallInternal (const char *pszServiceName, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmHostCallInternal: service = %s, u32Function = %d, cParms = %d, aParms = %p\n",
             pszServiceName, u32Function, cParms, aParms));

    if (!pszServiceName)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_HOSTCALL, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pHGCMPort = NULL; /* Not used. */
        
        pMsg->pszServiceName = (char *)pszServiceName;
        pMsg->u32Function = u32Function;
        pMsg->cParms = cParms;
        pMsg->paParms = &aParms[0];
        
        rc = hgcmMsgSend (hMsg);

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcm:HostCallInternal: hgcmMsgSend returned %Vrc\n", rc));
    }
    else
    {
        Log(("MAIN::hgcmHostCallInternal:Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}


int hgcmInit (void)
{
    int rc = VINF_SUCCESS;

    Log(("MAIN::hgcmInit\n"));

    rc = hgcmThreadInit ();

    if (VBOX_FAILURE(rc))
    {
        Log(("FAILURE: Can't init worker threads.\n"));
    }
    else
    {
        /* Start main HGCM thread that will process Connect/Disconnect requests. */

        rc = hgcmThreadCreate (&g_hgcmThread, "MainHGCMthread", hgcmThread, NULL);

        if (VBOX_FAILURE (rc))
        {
            Log(("FAILURE: HGCM initialization.\n"));
        }
    }

    LogFlow(("MAIN::hgcmInit: rc = %Vrc\n", rc));

    return rc;
}

int hgcmReset (void)
{
    int rc = VINF_SUCCESS;
    Log(("MAIN::hgcmReset\n"));
    
    /* Disconnect all clients. */
    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_RESET, hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgReset *pMsg = (HGCMMsgReset *)hgcmObjReference (hMsg, HGCMOBJ_MSG);

        AssertRelease(pMsg);

        pMsg->pHGCMPort = NULL; /* Not used by the call. */
        
        rc = hgcmMsgSend (hMsg);

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcmReset: hgcmMsgSend returned %Vrc\n", rc));
    }
    else
    {
        Log(("MAIN::hgcmReset:Message allocation failed: %Vrc\n", rc));
    }
    
    LogFlow(("MAIN::hgcmReset: rc = %Vrc\n", rc));
    return rc;
}
