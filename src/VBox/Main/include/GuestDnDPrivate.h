/* $Id$ */
/** @file
 * Private guest drag and drop code, used by GuestDnDTarget +
 * GuestDnDSource.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTDNDPRIVATE
#define ____H_GUESTDNDPRIVATE

#include <iprt/dir.h>
#include <iprt/file.h>

#include "VBox/hgcmsvc.h" /* For PVBOXHGCMSVCPARM. */
#include "VBox/GuestHost/DragAndDrop.h"

/**
 * Forward prototype declarations.
 */
class Guest;
class GuestDnDBase;
class GuestDnDResponse;
class GuestDnDSource;
class GuestDnDTarget;
class Progress;

class GuestDnDCallbackEvent
{
public:

    GuestDnDCallbackEvent(void)
        : mSemEvent(NIL_RTSEMEVENT)
        , mRc(VINF_SUCCESS) { }

    virtual ~GuestDnDCallbackEvent(void);

public:

    int Reset(void);

    int Notify(int rc = VINF_SUCCESS);

    int Result(void) const { return mRc; }

    int Wait(RTMSINTERVAL msTimeout);

protected:

    /** Event semaphore to notify on error/completion. */
    RTSEMEVENT mSemEvent;
    /** Callback result. */
    int        mRc;
};

/**
 * Structure for keeping the (URI) data to be sent/received.
 */
typedef struct GuestDnDData
{
    GuestDnDData(void)
        : cbToProcess(0)
        , cbProcessed(0) { }

    void Reset(void)
    {
        vecData.clear();
        cbToProcess = 0;
        cbProcessed = 0;
    }

    /** Array (vector) of guest DnD data. This might be an URI list, according
     *  to the format being set. */
    std::vector<BYTE>         vecData;
    /** Overall size (in bytes) of data to send. */
    uint64_t                  cbToProcess;
    /** Overall size (in bytes) of processed file data. */
    uint64_t                  cbProcessed;

} GuestDnDData;

/**
 * Structure for keeping around URI (list) data.
 */
typedef struct GuestDnDURIData
{
    GuestDnDURIData(void)
        : pvScratchBuf(NULL)
        , cbScratchBuf(0) { }

    virtual ~GuestDnDURIData(void)
    {
        Reset();
    }

    void Reset(void)
    {
        lstURI.Clear();
#if 0 /* Currently the scratch buffer will be maintained elswewhere. */
        if (pvScratchBuf)
        {
            RTMemFree(pvScratchBuf);
            pvScratchBuf = NULL;
        }
        cbScratchBuf = 0;
#else
        pvScratchBuf = NULL;
        cbScratchBuf = 0;
#endif
    }

    DNDDIRDROPPEDFILES              mDropDir;
    /** (Non-recursive) List of root URI objects to receive. */
    DnDURIList                      lstURI;
    /** Current object to receive. */
    DnDURIObject                    objURI;
    /** Pointer to an optional scratch buffer to use for
     *  doing the actual chunk transfers. */
    void                           *pvScratchBuf;
    /** Size (in bytes) of scratch buffer. */
    size_t                          cbScratchBuf;

} GuestDnDURIData;

/**
 * Context structure for sending data to the guest.
 */
typedef struct SENDDATACTX
{
    /** Pointer to guest target class this context belongs to. */
    GuestDnDTarget                     *mpTarget;
    /** Pointer to guest response class this context belongs to. */
    GuestDnDResponse                   *mpResp;
    /** Flag indicating whether a file transfer is active and
     *  initiated by the host. */
    bool                                mIsActive;
    /** Target (VM) screen ID. */
    uint32_t                            mScreenID;
    /** Drag'n drop format to send. */
    com::Utf8Str                        mFormat;
    /** Drag'n drop data to send.
     *  This can be arbitrary data or an URI list. */
    GuestDnDData                        mData;
    /** URI data structure. */
    GuestDnDURIData                     mURI;
    /** Callback event to use. */
    GuestDnDCallbackEvent               mCallback;

} SENDDATACTX, *PSENDDATACTX;

/**
 * Context structure for receiving data from the guest.
 */
typedef struct RECVDATACTX
{
    /** Pointer to guest source class this context belongs to. */
    GuestDnDSource                     *mpSource;
    /** Pointer to guest response class this context belongs to. */
    GuestDnDResponse                   *mpResp;
    /** Flag indicating whether a file transfer is active and
     *  initiated by the host. */
    bool                                mIsActive;
    /** Drag'n drop format to send. */
    com::Utf8Str                        mFormat;
    /** Desired drop action to perform on the host.
     *  Needed to tell the guest if data has to be
     *  deleted e.g. when moving instead of copying. */
    uint32_t                            mAction;
    /** Drag'n drop received from the guest.
     *  This can be arbitrary data or an URI list. */
    GuestDnDData                        mData;
    /** URI data structure. */
    GuestDnDURIData                     mURI;
    /** Callback event to use. */
    GuestDnDCallbackEvent               mCallback;

} RECVDATACTX, *PRECVDATACTX;

/**
 * Simple structure for a buffered guest DnD message.
 */
class GuestDnDMsg
{
public:

    GuestDnDMsg(void)
        : uMsg(0)
        , cParms(0)
        , cParmsAlloc(0)
        , paParms(NULL) { }

    virtual ~GuestDnDMsg(void)
    {
        if (paParms)
        {
            /* Remove deep copies. */
            for (uint32_t i = 0; i < cParms; i++)
            {
                if (   paParms[i].type == VBOX_HGCM_SVC_PARM_PTR
                    && paParms[i].u.pointer.addr)
                {
                    RTMemFree(paParms[i].u.pointer.addr);
                }
            }

            delete paParms;
        }
    }

public:

    PVBOXHGCMSVCPARM getNextParam(void)
    {
        if (cParms >= cParmsAlloc)
        {
            paParms = (PVBOXHGCMSVCPARM)RTMemRealloc(paParms, (cParmsAlloc + 4) * sizeof(VBOXHGCMSVCPARM));
            if (!paParms)
                throw VERR_NO_MEMORY;
            RT_BZERO(&paParms[cParmsAlloc], 4 * sizeof(VBOXHGCMSVCPARM));
            cParmsAlloc += 4;
        }

        return &paParms[cParms++];
    }

    uint32_t getCount(void) const { return cParms; }
    PVBOXHGCMSVCPARM getParms(void) const { return paParms; }
    uint32_t getType(void) const { return uMsg; }

    int setNextPointer(void *pvBuf, uint32_t cbBuf)
    {
        AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
        AssertReturn(cbBuf, VERR_INVALID_PARAMETER);

        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        void *pvTmp = RTMemDup(pvBuf, cbBuf);
        if (!pvTmp)
        {
            RTMemFree(pParm);
            return VERR_NO_MEMORY;
        }

        pParm->setPointer(pvTmp, cbBuf);
        return VINF_SUCCESS;
    }

    int setNextString(const char *pszString)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        char *pszTemp = RTStrDup(pszString);
        if (!pszTemp)
        {
            RTMemFree(pParm);
            return VERR_NO_MEMORY;
        }

        pParm->setString(pszTemp);
        return VINF_SUCCESS;
    }

    int setNextUInt32(uint32_t u32Val)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        pParm->setUInt32(u32Val);
        return VINF_SUCCESS;
    }

    int setNextUInt64(uint64_t u64Val)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        pParm->setUInt64(u64Val);
        return VINF_SUCCESS;
    }

    void setType(uint32_t uMsgType) { uMsg = uMsgType; }

protected:

    /** Message type. */
    uint32_t                    uMsg;
    /** Message parameters. */
    uint32_t                    cParms;
    /** Size of array. */
    uint32_t                    cParmsAlloc;
    /** Array of HGCM parameters */
    PVBOXHGCMSVCPARM            paParms;
};

/** Guest DnD callback function definition. */
typedef DECLCALLBACKPTR(int, PFNGUESTDNDCALLBACK) (uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser);

/**
 * Structure for keeping a guest DnD callback.
 * Each callback can handle one HGCM message, however, multiple HGCM messages can be registered
 * to the same callback (function).
 */
typedef struct GuestDnDCallback
{
    GuestDnDCallback(void)
        : uMessgage(0)
        , pfnCallback(NULL)
        , pvUser(NULL) { }

    GuestDnDCallback(PFNGUESTDNDCALLBACK pvCB, uint32_t uMsg, void *pvUsr = NULL)
        : uMessgage(uMsg)
        , pfnCallback(pvCB)
        , pvUser(pvUsr) { }

    /** The HGCM message ID to handle. */
    uint32_t             uMessgage;
    /** Pointer to callback function. */
    PFNGUESTDNDCALLBACK  pfnCallback;
    /** Pointer to user-supplied data. */
    void                *pvUser;

} GuestDnDCallback;

/** Contains registered callback pointers for specific HGCM message types. */
typedef std::map<uint32_t, GuestDnDCallback> GuestDnDCallbackMap;

class GuestDnDResponse
{

public:

    GuestDnDResponse(const ComObjPtr<Guest>& pGuest);
    virtual ~GuestDnDResponse(void);

public:

    int notifyAboutGuestResponse(void) const;
    int waitForGuestResponse(RTMSINTERVAL msTimeout = 500) const;

    void setAllActions(uint32_t a) { m_allActions = a; }
    uint32_t allActions(void) const { return m_allActions; }

    void setDefAction(uint32_t a) { m_defAction = a; }
    uint32_t defAction(void) const { return m_defAction; }

    void setFormat(const Utf8Str &strFormat) { m_strFormat = strFormat; }
    Utf8Str format(void) const { return m_strFormat; }

    void reset(void);

    bool isProgressCanceled(void) const;
    int setCallback(uint32_t uMsg, PFNGUESTDNDCALLBACK pfnCallback, void *pvUser = NULL);
    int setProgress(unsigned uPercentage, uint32_t uState, int rcOp = VINF_SUCCESS, const Utf8Str &strMsg = "");
    HRESULT resetProgress(const ComObjPtr<Guest>& pParent);
    HRESULT queryProgressTo(IProgress **ppProgress);

public:

    /** @name HGCM callback handling.
       @{ */
    int onDispatch(uint32_t u32Function, void *pvParms, uint32_t cbParms);
    /** @}  */

protected:

    /** Pointer to context this class is tied to. */
    void                 *m_pvCtx;
    RTSEMEVENT            m_EventSem;
    uint32_t              m_defAction;
    uint32_t              m_allActions;
    Utf8Str               m_strFormat;

    /** Pointer to IGuest parent object. */
    ComObjPtr<Guest>      m_parent;
    /** Pointer to associated progress object. Optional. */
    ComObjPtr<Progress>   m_progress;
    /** Callback map. */
    GuestDnDCallbackMap   m_mapCallbacks;
};

/**
 * Private singleton class for the guest's DnD
 * implementation. Can't be instanciated directly, only via
 * the factory pattern.
 */
class GuestDnD
{
public:

    static GuestDnD *createInstance(const ComObjPtr<Guest>& pGuest)
    {
        Assert(NULL == GuestDnD::s_pInstance);
        GuestDnD::s_pInstance = new GuestDnD(pGuest);
        return GuestDnD::s_pInstance;
    }

    static void destroyInstance(void)
    {
        if (GuestDnD::s_pInstance)
        {
            delete GuestDnD::s_pInstance;
            GuestDnD::s_pInstance = NULL;
        }
    }

    static inline GuestDnD *getInstance(void)
    {
        AssertPtr(GuestDnD::s_pInstance);
        return GuestDnD::s_pInstance;
    }

protected:

    GuestDnD(const ComObjPtr<Guest>& pGuest);
    virtual ~GuestDnD(void);

public:

    /** @name Public helper functions.
     * @{ */
    int                        adjustScreenCoordinates(ULONG uScreenId, ULONG *puX, ULONG *puY) const;
    int                        hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const;
    GuestDnDResponse          *response(void) { return m_pResponse; }
    std::vector<com::Utf8Str>  defaultFormats(void) const { return m_strDefaultFormats; }
    /** @}  */

public:

    /** @name Static low-level HGCM callback handler.
     * @{ */
    static DECLCALLBACK(int)   notifyDnDDispatcher(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
    /** @}  */

    /** @name Static helper methods.
     * @{ */
    static com::Utf8Str        toFormatString(const std::vector<com::Utf8Str> &lstSupportedFormats, const std::vector<com::Utf8Str> &lstFormats);
    static void                toFormatVector(const std::vector<com::Utf8Str> &lstSupportedFormats, const com::Utf8Str &strFormats, std::vector<com::Utf8Str> &vecformats);
    static DnDAction_T         toMainAction(uint32_t uAction);
    static void                toMainActions(uint32_t uActions, std::vector<DnDAction_T> &vecActions);
    static uint32_t            toHGCMAction(DnDAction_T enmAction);
    static void                toHGCMActions(DnDAction_T enmDefAction, uint32_t *puDefAction, const std::vector<DnDAction_T> vecAllowedActions, uint32_t *puAllowedActions);
    /** @}  */

protected:

    /** @name Singleton properties.
     * @{ */
    /** List of supported default MIME/Content-type formats. */
    std::vector<com::Utf8Str>  m_strDefaultFormats;
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>     m_pGuest;
    /** The current (last) response from the guest. At the
     *  moment we only support only response a time (ARQ-style). */
    GuestDnDResponse          *m_pResponse;
    /** @}  */

private:

    /** Staic pointer to singleton instance. */
    static GuestDnD           *s_pInstance;
};

/** Access to the GuestDnD's singleton instance. */
#define GuestDnDInst() GuestDnD::getInstance()

/** List of pointers to guest DnD Messages. */
typedef std::list<GuestDnDMsg *> GuestDnDMsgList;

/**
 * IDnDBase class implementation for sharing code between
 * IGuestDnDSource and IGuestDnDTarget implementation.
 */
class GuestDnDBase
{
protected:

    GuestDnDBase(void);

protected:

    /** Shared (internal) IDnDBase method implementations.
     * @{ */
    HRESULT i_isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported);
    HRESULT i_getFormats(std::vector<com::Utf8Str> &aFormats);
    HRESULT i_addFormats(const std::vector<com::Utf8Str> &aFormats);
    HRESULT i_removeFormats(const std::vector<com::Utf8Str> &aFormats);

    HRESULT i_getProtocolVersion(ULONG *puVersion);
    /** @}  */

protected:

    int getProtocolVersion(uint32_t *puVersion);

    /** @name Functions for handling a simple host HGCM message queue.
     * @{ */
    int msgQueueAdd(GuestDnDMsg *pMsg);
    GuestDnDMsg *msgQueueGetNext(void);
    void msgQueueRemoveNext(void);
    void msgQueueClear(void);
    /** @}  */

    int sendCancel(void);
    int waitForEvent(RTMSINTERVAL msTimeout, GuestDnDCallbackEvent &Event, GuestDnDResponse *pResp);

protected:

    /** @name Public attributes (through getters/setters).
     * @{ */
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>          m_pGuest;
    /** List of supported MIME/Content-type formats. */
    std::vector<com::Utf8Str>       m_strFormats;
    /** @}  */

    struct
    {
        /** Flag indicating whether a drop operation currently
         *  is in progress or not. */
        bool                        mfTransferIsPending;
        /** The DnD protocol version to use, depending on the
         *  installed Guest Additions. */
        uint32_t                    mProtocolVersion;
        /** Outgoing message queue. */
        GuestDnDMsgList             mListOutgoing;
    } mDataBase;
};
#endif /* ____H_GUESTDNDPRIVATE */

