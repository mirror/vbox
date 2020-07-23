/* $Id$ */
/** @file
 * Private guest drag and drop code, used by GuestDnDTarget +
 * GuestDnDSource.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_GuestDnDPrivate_h
#define MAIN_INCLUDED_GuestDnDPrivate_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <VBox/hgcmsvc.h> /* For PVBOXHGCMSVCPARM. */
#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/GuestHost/DragAndDropDefs.h>
#include <VBox/HostServices/DragAndDropSvc.h>

/**
 * Forward prototype declarations.
 */
class Guest;
class GuestDnDBase;
class GuestDnDResponse;
class GuestDnDSource;
class GuestDnDTarget;
class Progress;

/**
 * Type definitions.
 */

/** List (vector) of MIME types. */
typedef std::vector<com::Utf8Str> GuestDnDMIMEList;

/*
 ** @todo Put most of the implementations below in GuestDnDPrivate.cpp!
 */

class GuestDnDCallbackEvent
{
public:

    GuestDnDCallbackEvent(void)
        : m_SemEvent(NIL_RTSEMEVENT)
        , m_Rc(VINF_SUCCESS) { }

    virtual ~GuestDnDCallbackEvent(void);

public:

    int Reset(void);

    int Notify(int rc = VINF_SUCCESS);

    int Result(void) const { return m_Rc; }

    int Wait(RTMSINTERVAL msTimeout);

protected:

    /** Event semaphore to notify on error/completion. */
    RTSEMEVENT m_SemEvent;
    /** Callback result. */
    int        m_Rc;
};

/**
 * Struct for handling the (raw) meta data.
 */
struct GuestDnDMetaData
{
    GuestDnDMetaData(void)
        : pvData(NULL)
        , cbData(0)
        , cbAllocated(0) { }

    virtual ~GuestDnDMetaData(void)
    {
        reset();
    }

    size_t add(const void *pvDataAdd, size_t cbDataAdd)
    {
        LogFlowThisFunc(("pvDataAdd=%p, cbDataAdd=%zu\n", pvDataAdd, cbDataAdd));

        if (!cbDataAdd)
            return 0;
        AssertPtrReturn(pvDataAdd, 0);

        int rc = resize(cbAllocated + cbDataAdd);
        if (RT_FAILURE(rc))
            return 0;

        Assert(cbAllocated >= cbData + cbDataAdd);
        memcpy((uint8_t *)pvData + cbData, pvDataAdd, cbDataAdd);

        cbData += cbDataAdd;

        return cbData;
    }

    size_t add(const std::vector<BYTE> &vecAdd)
    {
        if (!vecAdd.size())
            return 0;

        if (vecAdd.size() > UINT32_MAX) /* Paranoia. */
            return 0;

        return add(&vecAdd.front(), (uint32_t)vecAdd.size());
    }

    void reset(void)
    {
        strFmt = "";

        if (pvData)
        {
            Assert(cbAllocated);
            RTMemFree(pvData);
            pvData = NULL;
        }

        cbAllocated = 0;
        cbData      = 0;
    }

    int resize(size_t cbSize)
    {
        if (!cbSize)
        {
            reset();
            return VINF_SUCCESS;
        }

        if (cbSize == cbAllocated)
            return VINF_SUCCESS;

        if (cbSize > _32M) /* Meta data can be up to 32MB. */
            return VERR_INVALID_PARAMETER;

        void *pvTmp = NULL;
        if (!cbAllocated)
        {
            Assert(cbData == 0);
            pvTmp = RTMemAllocZ(cbSize);
        }
        else
        {
            AssertPtr(pvData);
            pvTmp = RTMemRealloc(pvData, cbSize);
            RT_BZERO(pvTmp, cbSize);
        }

        if (pvTmp)
        {
            pvData      = pvTmp;
            cbAllocated = cbSize;
            return VINF_SUCCESS;
        }

        return VERR_NO_MEMORY;
    }

    /** Format string of this meta data. */
    com::Utf8Str strFmt;
    /** Pointer to allocated meta data. */
    void        *pvData;
    /** Used bytes of meta data. Must not exceed cbAllocated. */
    size_t       cbData;
    /** Size (in bytes) of allocated meta data. */
    size_t       cbAllocated;
};

/**
 * Struct for accounting shared DnD data to be sent/received.
 */
struct GuestDnDData
{
    GuestDnDData(void)
        : cbExtra(0)
        , cbProcessed(0) { }

    virtual ~GuestDnDData(void)
    {
        reset();
    }

    size_t addProcessed(size_t cbDataAdd)
    {
        const size_t cbTotal = Meta.cbData + cbExtra; RT_NOREF(cbTotal);
        AssertReturn(cbProcessed + cbDataAdd <= cbTotal, 0);
        cbProcessed += cbDataAdd;
        return cbProcessed;
    }

    bool isComplete(void) const
    {
        const size_t cbTotal = Meta.cbData + cbExtra;
        LogFlowFunc(("cbProcessed=%zu, cbTotal=%zu\n", cbProcessed, cbTotal));
        AssertReturn(cbProcessed <= cbTotal, true);
        return (cbProcessed == cbTotal);
    }

    uint8_t getPercentComplete(void) const
    {
        const size_t cbTotal = Meta.cbData + cbExtra;
        return (uint8_t)(cbProcessed * 100 / RT_MAX(cbTotal, 1));
    }

    size_t getRemaining(void) const
    {
        const size_t cbTotal = Meta.cbData + cbExtra;
        AssertReturn(cbProcessed <= cbTotal, 0);
        return cbTotal - cbProcessed;
    }

    size_t getTotal(void) const
    {
        return Meta.cbData + cbExtra;
    }

    void reset(void)
    {
        Meta.reset();

        cbExtra     = 0;
        cbProcessed = 0;
    }

    /** For storing the actual meta data.
     *  This might be an URI list or just plain raw data,
     *  according to the format being sent. */
    GuestDnDMetaData   Meta;
    /** Extra data to send/receive (in bytes). Can be 0 for raw data.
     *  For (file) transfers this is the total size for all files. */
    size_t             cbExtra;
    /** Overall size (in bytes) of processed data. */
    size_t             cbProcessed;
};

/** Initial object context state / no state set. */
#define DND_OBJ_STATE_NONE           0
/** The header was received / sent. */
#define DND_OBJ_STATE_HAS_HDR        RT_BIT(0)
/** Validation mask for object context state. */
#define DND_OBJ_STATE_VALID_MASK     UINT32_C(0x00000001)

/**
 * Class for keeping around DnD (file) transfer data.
 */
struct GuestDnDTransferData
{

public:

    GuestDnDTransferData(void)
        : cObjToProcess(0)
        , cObjProcessed(0)
        , pvScratchBuf(NULL)
        , cbScratchBuf(0) { }

    virtual ~GuestDnDTransferData(void)
    {
        destroy();
    }

    int init(size_t cbBuf = _64K)
    {
        reset();

        pvScratchBuf = RTMemAlloc(cbBuf);
        if (!pvScratchBuf)
            return VERR_NO_MEMORY;

        cbScratchBuf = cbBuf;
        return VINF_SUCCESS;
    }

    void destroy(void)
    {
        reset();

        if (pvScratchBuf)
        {
            Assert(cbScratchBuf);
            RTMemFree(pvScratchBuf);
            pvScratchBuf = NULL;
        }
        cbScratchBuf = 0;
    }

    void reset(void)
    {
        LogFlowFuncEnter();

        cObjToProcess = 0;
        cObjProcessed = 0;
    }

    bool isComplete(void) const
    {
        return (cObjProcessed == cObjToProcess);
    }

    /** Number of objects to process. */
    uint64_t cObjToProcess;
    /** Number of objects already processed. */
    uint64_t cObjProcessed;
    /** Pointer to an optional scratch buffer to use for
     *  doing the actual chunk transfers. */
    void    *pvScratchBuf;
    /** Size (in bytes) of scratch buffer. */
    size_t   cbScratchBuf;
};

struct GuestDnDTransferSendData : public GuestDnDTransferData
{
    GuestDnDTransferSendData()
        : fObjState(0)
    {
        RT_ZERO(List);
    }

    virtual ~GuestDnDTransferSendData()
    {
        destroy();
    }

    void destroy(void)
    {
        DnDTransferListDestroy(&List);
    }

    void reset(void)
    {
        DnDTransferListReset(&List);
        fObjState = 0;

        GuestDnDTransferData::reset();
    }

    /** Transfer List to handle. */
    DNDTRANSFERLIST                     List;
    /** Current state of object in transfer.
     *  This is needed for keeping compatibility to old(er) DnD HGCM protocols.
     *
     *  At the moment we only support transferring one object at a time. */
    uint32_t                            fObjState;
};

/**
 * Context structure for sending data to the guest.
 */
struct GuestDnDSendCtx : public GuestDnDData
{
    /** Pointer to guest target class this context belongs to. */
    GuestDnDTarget                     *pTarget;
    /** Pointer to guest response class this context belongs to. */
    GuestDnDResponse                   *pResp;
    /** Flag indicating whether a file transfer is active and
     *  initiated by the host. */
    bool                                fIsActive;
    /** Target (VM) screen ID. */
    uint32_t                            uScreenID;
    /** Transfer data structure. */
    GuestDnDTransferSendData            Transfer;
    /** Callback event to use. */
    GuestDnDCallbackEvent               EventCallback;
};

struct GuestDnDTransferRecvData : public GuestDnDTransferData
{
    GuestDnDTransferRecvData()
    {
        RT_ZERO(DroppedFiles);
        RT_ZERO(List);
        RT_ZERO(ObjCur);
    }

    virtual ~GuestDnDTransferRecvData()
    {
        destroy();
    }

    void destroy(void)
    {
        DnDTransferListDestroy(&List);
    }

    void reset(void)
    {
        DnDDroppedFilesClose(&DroppedFiles);
        DnDTransferListReset(&List);
        DnDTransferObjectReset(&ObjCur);

        GuestDnDTransferData::reset();
    }

    /** The "VirtualBox Dropped Files" directory on the host we're going
     *  to utilize for transferring files from guest to the host. */
    DNDDROPPEDFILES                     DroppedFiles;
    /** Transfer List to handle.
     *  Currently we only support one transfer list at a time. */
    DNDTRANSFERLIST                     List;
    /** Current transfer object being handled.
     *  Currently we only support one transfer object at a time. */
    DNDTRANSFEROBJECT                   ObjCur;
};

/**
 * Context structure for receiving data from the guest.
 */
struct GuestDnDRecvCtx : public GuestDnDData
{
    /** Pointer to guest source class this context belongs to. */
    GuestDnDSource                     *pSource;
    /** Pointer to guest response class this context belongs to. */
    GuestDnDResponse                   *pResp;
    /** Flag indicating whether a file transfer is active and
     *  initiated by the host. */
    bool                                fIsActive;
    /** Formats offered by the guest (and supported by the host). */
    GuestDnDMIMEList                    lstFmtOffered;
    /** Original drop format requested to receive from the guest. */
    com::Utf8Str                        strFmtReq;
    /** Intermediate drop format to be received from the guest.
     *  Some original drop formats require a different intermediate
     *  drop format:
     *
     *  Receiving a file link as "text/plain"  requires still to
     *  receive the file from the guest as "text/uri-list" first,
     *  then pointing to the file path on the host with the data
     *  in "text/plain" format returned. */
    com::Utf8Str                        strFmtRecv;
    /** Desired drop action to perform on the host.
     *  Needed to tell the guest if data has to be
     *  deleted e.g. when moving instead of copying. */
    VBOXDNDACTION                       enmAction;
    /** Transfer data structure. */
    GuestDnDTransferRecvData            Transfer;
    /** Callback event to use. */
    GuestDnDCallbackEvent               EventCallback;
};

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
        reset();
    }

public:

    PVBOXHGCMSVCPARM getNextParam(void)
    {
        if (cParms >= cParmsAlloc)
        {
            if (!paParms)
                paParms = (PVBOXHGCMSVCPARM)RTMemAlloc(4 * sizeof(VBOXHGCMSVCPARM));
            else
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

    void reset(void)
    {
        if (paParms)
        {
            /* Remove deep copies. */
            for (uint32_t i = 0; i < cParms; i++)
            {
                if (   paParms[i].type == VBOX_HGCM_SVC_PARM_PTR
                    && paParms[i].u.pointer.size)
                {
                    AssertPtr(paParms[i].u.pointer.addr);
                    RTMemFree(paParms[i].u.pointer.addr);
                }
            }

            RTMemFree(paParms);
            paParms = NULL;
        }

        uMsg = cParms = cParmsAlloc = 0;
    }

    int setNextPointer(void *pvBuf, uint32_t cbBuf)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        void *pvTmp = NULL;
        if (cbBuf)
        {
            AssertPtr(pvBuf);
            pvTmp = RTMemDup(pvBuf, cbBuf);
            if (!pvTmp)
                return VERR_NO_MEMORY;
        }

        HGCMSvcSetPv(pParm, pvTmp, cbBuf);
        return VINF_SUCCESS;
    }

    int setNextString(const char *pszString)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        char *pszTemp = RTStrDup(pszString);
        if (!pszTemp)
            return VERR_NO_MEMORY;

        HGCMSvcSetStr(pParm, pszTemp);
        return VINF_SUCCESS;
    }

    int setNextUInt32(uint32_t u32Val)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        HGCMSvcSetU32(pParm, u32Val);
        return VINF_SUCCESS;
    }

    int setNextUInt64(uint64_t u64Val)
    {
        PVBOXHGCMSVCPARM pParm = getNextParam();
        if (!pParm)
            return VERR_NO_MEMORY;

        HGCMSvcSetU64(pParm, u64Val);
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
typedef DECLCALLBACKPTR(int, PFNGUESTDNDCALLBACK,(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser));

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

/** @todo r=andy This class needs to go, as this now is too inflexible when it comes to all
 *               the callback handling/dispatching. It's part of the initial code and only adds
 *               unnecessary complexity. */
class GuestDnDResponse
{

public:

    GuestDnDResponse(const ComObjPtr<Guest>& pGuest);
    virtual ~GuestDnDResponse(void);

public:

    int notifyAboutGuestResponse(void) const;
    int waitForGuestResponse(RTMSINTERVAL msTimeout = 500) const;

    void setActionsAllowed(VBOXDNDACTIONLIST a) { m_dndLstActionsAllowed = a; }
    VBOXDNDACTIONLIST getActionsAllowed(void) const { return m_dndLstActionsAllowed; }

    void setActionDefault(VBOXDNDACTION a) { m_dndActionDefault = a; }
    VBOXDNDACTION getActionDefault(void) const { return m_dndActionDefault; }

    void setFormats(const GuestDnDMIMEList &lstFormats) { m_lstFormats = lstFormats; }
    GuestDnDMIMEList formats(void) const { return m_lstFormats; }

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
    /** Event for waiting for response. */
    RTSEMEVENT            m_EventSem;
    /** Default action to perform in case of a
     *  successful drop. */
    VBOXDNDACTION         m_dndActionDefault;
    /** Actions supported by the guest in case of a successful drop. */
    VBOXDNDACTIONLIST     m_dndLstActionsAllowed;
    /** Format(s) requested/supported from the guest. */
    GuestDnDMIMEList      m_lstFormats;
    /** Pointer to IGuest parent object. */
    ComObjPtr<Guest>      m_pParent;
    /** Pointer to associated progress object. Optional. */
    ComObjPtr<Progress>   m_pProgress;
    /** Callback map. */
    GuestDnDCallbackMap   m_mapCallbacks;
};

/**
 * Private singleton class for the guest's DnD
 * implementation. Can't be instanciated directly, only via
 * the factory pattern.
 *
 ** @todo Move this into GuestDnDBase.
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
    HRESULT           adjustScreenCoordinates(ULONG uScreenId, ULONG *puX, ULONG *puY) const;
    int               hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const;
    GuestDnDResponse *response(void) { return m_pResponse; }
    GuestDnDMIMEList  defaultFormats(void) const { return m_strDefaultFormats; }
    /** @}  */

public:

    /** @name Static low-level HGCM callback handler.
     * @{ */
    static DECLCALLBACK(int)   notifyDnDDispatcher(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
    /** @}  */

    /** @name Static helper methods.
     * @{ */
    static bool                     isFormatInFormatList(const com::Utf8Str &strFormat, const GuestDnDMIMEList &lstFormats);
    static GuestDnDMIMEList         toFormatList(const com::Utf8Str &strFormats);
    static com::Utf8Str             toFormatString(const GuestDnDMIMEList &lstFormats);
    static GuestDnDMIMEList         toFilteredFormatList(const GuestDnDMIMEList &lstFormatsSupported, const GuestDnDMIMEList &lstFormatsWanted);
    static GuestDnDMIMEList         toFilteredFormatList(const GuestDnDMIMEList &lstFormatsSupported, const com::Utf8Str &strFormatsWanted);
    static DnDAction_T              toMainAction(VBOXDNDACTION dndAction);
    static std::vector<DnDAction_T> toMainActions(VBOXDNDACTIONLIST dndActionList);
    static VBOXDNDACTION            toHGCMAction(DnDAction_T enmAction);
    static void                     toHGCMActions(DnDAction_T enmDefAction, VBOXDNDACTION *pDefAction, const std::vector<DnDAction_T> vecAllowedActions, VBOXDNDACTIONLIST *pLstAllowedActions);
    /** @}  */

protected:

    /** @name Singleton properties.
     * @{ */
    /** List of supported default MIME/Content-type formats. */
    GuestDnDMIMEList           m_strDefaultFormats;
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
#define GUESTDNDINST() GuestDnD::getInstance()

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
    HRESULT i_getFormats(GuestDnDMIMEList &aFormats);
    HRESULT i_addFormats(const GuestDnDMIMEList &aFormats);
    HRESULT i_removeFormats(const GuestDnDMIMEList &aFormats);

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
    int updateProgress(GuestDnDData *pData, GuestDnDResponse *pResp, size_t cbDataAdd = 0);
    int waitForEvent(GuestDnDCallbackEvent *pEvent, GuestDnDResponse *pResp, RTMSINTERVAL msTimeout);

protected:

    /** @name Public attributes (through getters/setters).
     * @{ */
    /** Pointer to guest implementation. */
    const ComObjPtr<Guest>          m_pGuest;
    /** List of supported MIME types by the source. */
    GuestDnDMIMEList                m_lstFmtSupported;
    /** List of offered MIME types to the counterpart. */
    GuestDnDMIMEList                m_lstFmtOffered;
    /** @}  */

    /**
     * Internal stuff.
     */
    struct
    {
        /** Number of active transfers (guest->host or host->guest). */
        uint32_t                    cTransfersPending;
        /** The DnD protocol version to use, depending on the
         *  installed Guest Additions. See DragAndDropSvc.h for
         *  a protocol changelog. */
        uint32_t                    uProtocolVersion;
        /** Outgoing message queue (FIFO). */
        GuestDnDMsgList             lstMsgOut;
    } m_DataBase;
};
#endif /* !MAIN_INCLUDED_GuestDnDPrivate_h */

