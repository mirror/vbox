/** @file
 * Base class for an host-guest service.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_HostService_Service_h
#define ___VBox_HostService_Service_h

#include <memory>  /* for auto_ptr */

#include <VBox/log.h>
#include <VBox/hgcmsvc.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/cpp/utils.h>


namespace HGCM
{

/**
 * Structure for keeping a HGCM service context.
 */
typedef struct VBOXHGCMSVCTX
{
    /** HGCM helper functions. */
    PVBOXHGCMSVCHELPERS pHelpers;
    /*
     * Callback function supplied by the host for notification of updates
     * to properties.
     */
    PFNHGCMSVCEXT       pfnHostCallback;
    /** User data pointer to be supplied to the host callback function. */
    void               *pvHostData;
} VBOXHGCMSVCTX, *PVBOXHGCMSVCTX;

/**
 * Base class encapsulating and working with a HGCM message.
 */
class Message
{
public:

    Message(void);

    Message(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[]);

    virtual ~Message(void);

    uint32_t GetParamCount(void) const;

    int GetData(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[]) const;

    int GetParmU32(uint32_t uParm, uint32_t *pu32Info) const;

    int GetParmU64(uint32_t uParm, uint64_t *pu64Info) const;

    int GetParmPtr(uint32_t uParm, void **ppvAddr, uint32_t *pcbSize) const;

    uint32_t GetType(void) const;

public:

    static int CopyParms(PVBOXHGCMSVCPARM paParmsDst, uint32_t cParmsDst,
                         PVBOXHGCMSVCPARM paParmsSrc, uint32_t cParmsSrc,
                         bool fDeepCopy);

protected:

    int initData(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[]);

    void reset();

protected:

    /** Stored message type. */
    uint32_t         m_uMsg;
    /** Number of stored HGCM parameters. */
    uint32_t         m_cParms;
    /** Stored HGCM parameters. */
    PVBOXHGCMSVCPARM m_paParms;
};

/**
 * Class for keeping and tracking a HGCM client.
 */
class Client
{
public:

    Client(uint32_t uClientID);

    virtual ~Client(void);

public:

    int Complete(VBOXHGCMCALLHANDLE hHandle, int rcOp = VINF_SUCCESS);

    int CompleteDeferred(int rcOp = VINF_SUCCESS);

    uint32_t GetClientID(void) const;

    VBOXHGCMCALLHANDLE GetHandle(void) const;

    uint32_t GetMsgType(void) const;

    uint32_t GetMsgParamCount(void) const;

    uint32_t GetProtocolVer(void) const;

    bool IsDeferred(void) const;

    void SetDeferred(VBOXHGCMCALLHANDLE hHandle, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    void SetProtocolVer(uint32_t uVersion);

    void SetSvcContext(const VBOXHGCMSVCTX &SvcCtx);

public:

    int SetDeferredMsgInfo(uint32_t uMsg, uint32_t cParms);

    int SetDeferredMsgInfo(const Message *pMessage);

protected:

    int completeInternal(VBOXHGCMCALLHANDLE hHandle, int rcOp);

    void reset(void);

protected:

    /** The client's HGCM client ID. */
    uint32_t           m_uClientID;
    /** Optional protocol version the client uses. Set to 0 by default. */
    uint32_t           m_uProtocolVer;
    /** The HGCM service context this client is bound to. */
    VBOXHGCMSVCTX      m_SvcCtx;
    /** Flag indicating whether this client currently is deferred mode,
     *  meaning that it did not return to the caller yet. */
    bool               m_fDeferred;
    /** Structure for keeping the client's deferred state.
     *  A client is in a deferred state when it asks for the next HGCM message,
     *  but the service can't provide it yet. That way a client will block (on the guest side, does not return)
     *  until the service can complete the call. */
    struct
    {
        /** The client's HGCM call handle. Needed for completing a deferred call. */
        VBOXHGCMCALLHANDLE hHandle;
        /** Message type (function number) to use when completing the deferred call. */
        uint32_t           uType;
        /** Parameter count to use when completing the deferred call. */
        uint32_t           cParms;
        /** Parameters to use when completing the deferred call. */
        PVBOXHGCMSVCPARM   paParms;
    } m_Deferred;
};

template <class T>
class AbstractService: public RTCNonCopyable
{
public:
    /**
     * @copydoc VBOXHGCMSVCLOAD
     */
    static DECLCALLBACK(int) svcLoad(VBOXHGCMSVCFNTABLE *pTable)
    {
        LogFlowFunc(("ptable = %p\n", pTable));
        int rc = VINF_SUCCESS;

        if (!VALID_PTR(pTable))
            rc = VERR_INVALID_PARAMETER;
        else
        {
            LogFlowFunc(("ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", pTable->cbSize, pTable->u32Version));

            if (   pTable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
                || pTable->u32Version != VBOX_HGCM_SVC_VERSION)
                rc = VERR_VERSION_MISMATCH;
            else
            {
                RT_GCC_NO_WARN_DEPRECATED_BEGIN
                std::auto_ptr<AbstractService> apService;
                /* No exceptions may propagate outside. */
                try
                {
                    apService = std::auto_ptr<AbstractService>(new T(pTable->pHelpers));
                } catch (int rcThrown)
                {
                    rc = rcThrown;
                } catch (...)
                {
                    rc = VERR_UNRESOLVED_ERROR;
                }
                RT_GCC_NO_WARN_DEPRECATED_END
                if (RT_SUCCESS(rc))
                {
                    /*
                     * We don't need an additional client data area on the host,
                     * because we're a class which can have members for that :-).
                     */
                    pTable->cbClient = 0;

                    /* These functions are mandatory */
                    pTable->pfnUnload             = svcUnload;
                    pTable->pfnConnect            = svcConnect;
                    pTable->pfnDisconnect         = svcDisconnect;
                    pTable->pfnCall               = svcCall;
                    /* Clear obligatory functions. */
                    pTable->pfnHostCall           = NULL;
                    pTable->pfnSaveState          = NULL;
                    pTable->pfnLoadState          = NULL;
                    pTable->pfnRegisterExtension  = NULL;

                    /* Let the service itself initialize. */
                    rc = apService->init(pTable);

                    /* Only on success stop the auto release of the auto_ptr. */
                    if (RT_SUCCESS(rc))
                        pTable->pvService = apService.release();
                }
            }
        }

        LogFlowFunc(("returning %Rrc\n", rc));
        return rc;
    }
    virtual ~AbstractService() {};

protected:
    explicit AbstractService(PVBOXHGCMSVCHELPERS pHelpers)
    {
        RT_ZERO(m_SvcCtx);
        m_SvcCtx.pHelpers = pHelpers;
    }
    virtual int  init(VBOXHGCMSVCFNTABLE *ptable) { RT_NOREF1(ptable); return VINF_SUCCESS; }
    virtual int  uninit() { return VINF_SUCCESS; }
    virtual int  clientConnect(uint32_t u32ClientID, void *pvClient) = 0;
    virtual int  clientDisconnect(uint32_t u32ClientID, void *pvClient) = 0;
    virtual void guestCall(VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]) = 0;
    virtual int  hostCall(uint32_t eFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    { RT_NOREF3(eFunction, cParms, paParms); return VINF_SUCCESS; }

    /** Type definition for use in callback functions. */
    typedef AbstractService SELF;
    /** The HGCM service context this service is bound to. */
    VBOXHGCMSVCTX m_SvcCtx;

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnUnload
     * Simply deletes the service object
     */
    static DECLCALLBACK(int) svcUnload(void *pvService)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        int rc = pSelf->uninit();
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            delete pSelf;
        return rc;
    }

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnConnect
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcConnect(void *pvService,
                                        uint32_t u32ClientID,
                                        void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc(("pvService=%p, u32ClientID=%u, pvClient=%p\n", pvService, u32ClientID, pvClient));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        int rc = pSelf->clientConnect(u32ClientID, pvClient);
        LogFlowFunc(("rc=%Rrc\n", rc));
        return rc;
    }

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnConnect
     * Stub implementation of pfnConnect and pfnDisconnect.
     */
    static DECLCALLBACK(int) svcDisconnect(void *pvService,
                                           uint32_t u32ClientID,
                                           void *pvClient)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc(("pvService=%p, u32ClientID=%u, pvClient=%p\n", pvService, u32ClientID, pvClient));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        int rc = pSelf->clientDisconnect(u32ClientID, pvClient);
        LogFlowFunc(("rc=%Rrc\n", rc));
        return rc;
    }

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnCall
     * Wraps to the call member function
     */
    static DECLCALLBACK(void) svcCall(void * pvService,
                                      VBOXHGCMCALLHANDLE callHandle,
                                      uint32_t u32ClientID,
                                      void *pvClient,
                                      uint32_t u32Function,
                                      uint32_t cParms,
                                      VBOXHGCMSVCPARM paParms[])
    {
        AssertLogRelReturnVoid(VALID_PTR(pvService));
        LogFlowFunc(("pvService=%p, callHandle=%p, u32ClientID=%u, pvClient=%p, u32Function=%u, cParms=%u, paParms=%p\n", pvService, callHandle, u32ClientID, pvClient, u32Function, cParms, paParms));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        pSelf->guestCall(callHandle, u32ClientID, pvClient, u32Function, cParms, paParms);
        LogFlowFunc(("returning\n"));
    }

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnHostCall
     * Wraps to the hostCall member function
     */
    static DECLCALLBACK(int) svcHostCall(void *pvService,
                                         uint32_t u32Function,
                                         uint32_t cParms,
                                         VBOXHGCMSVCPARM paParms[])
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc(("pvService=%p, u32Function=%u, cParms=%u, paParms=%p\n", pvService, u32Function, cParms, paParms));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        int rc = pSelf->hostCall(u32Function, cParms, paParms);
        LogFlowFunc(("rc=%Rrc\n", rc));
        return rc;
    }

    /**
     * @copydoc VBOXHGCMSVCFNTABLE::pfnRegisterExtension
     * Installs a host callback for notifications of property changes.
     */
    static DECLCALLBACK(int) svcRegisterExtension(void *pvService,
                                                  PFNHGCMSVCEXT pfnExtension,
                                                  void *pvExtension)
    {
        AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
        LogFlowFunc(("pvService=%p, pfnExtension=%p, pvExtention=%p\n", pvService, pfnExtension, pvExtension));
        SELF *pSelf = reinterpret_cast<SELF *>(pvService);
        pSelf->m_SvcCtx.pfnHostCallback = pfnExtension;
        pSelf->m_SvcCtx.pvHostData      = pvExtension;
        return VINF_SUCCESS;
    }

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(AbstractService);
};

}
#endif /* !___VBox_HostService_Service_h */

