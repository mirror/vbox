/* $Id$ */
/** @file
 * VBox Console COM Class implementation - Guest drag'n drop target.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "GuestImpl.h"
#include "GuestDnDTargetImpl.h"
#include "VirtualBoxErrorInfoImpl.h"

#include "Global.h"
#include "AutoCaller.h"

#include <algorithm>        /* For std::find(). */

#include <iprt/file.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/uri.h>
#include <iprt/cpp/utils.h> /* For unconst(). */

#include <VBox/com/array.h>

#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/Service.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


/**
 * Base class for a target task.
 */
class GuestDnDTargetTask
{
public:

    GuestDnDTargetTask(GuestDnDTarget *pTarget)
        : mTarget(pTarget),
          mRC(VINF_SUCCESS) { }

    virtual ~GuestDnDTargetTask(void) { }

    int getRC(void) const { return mRC; }
    bool isOk(void) const { return RT_SUCCESS(mRC); }
    const ComObjPtr<GuestDnDTarget> &getTarget(void) const { return mTarget; }

protected:

    const ComObjPtr<GuestDnDTarget>     mTarget;
    int                                 mRC;
};

/**
 * Task structure for sending data to a target using
 * a worker thread.
 */
class SendDataTask : public GuestDnDTargetTask
{
public:

    SendDataTask(GuestDnDTarget *pTarget, PSENDDATACTX pCtx)
        : GuestDnDTargetTask(pTarget),
          mpCtx(pCtx) { }

    virtual ~SendDataTask(void)
    {
        if (mpCtx)
        {
            delete mpCtx;
            mpCtx = NULL;
        }
    }


    PSENDDATACTX getCtx(void) { return mpCtx; }

protected:

    /** Pointer to send data context. */
    PSENDDATACTX mpCtx;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDnDTarget)

HRESULT GuestDnDTarget::FinalConstruct(void)
{
    /* Set the maximum block size our guests can handle to 64K. This always has
     * been hardcoded until now. */
    /* Note: Never ever rely on information from the guest; the host dictates what and
     *       how to do something, so try to negogiate a sensible value here later. */
    m_cbBlockSize = _32K; /** @todo Make this configurable. */

    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDnDTarget::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestDnDTarget::init(const ComObjPtr<Guest>& pGuest)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(m_pGuest) = pGuest;

    /* Confirm a successful initialization when it's the case. */
    autoInitSpan.setSucceeded();

    return VINF_SUCCESS;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestDnDTarget::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// implementation of wrapped IDnDBase methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDTarget::isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_isFormatSupported(aFormat, aSupported);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::getFormats(std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::addFormats(const std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_addFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::removeFormats(const std::vector<com::Utf8Str> &aFormats)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_removeFormats(aFormats);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::getProtocolVersion(ULONG *aProtocolVersion)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return GuestDnDBase::i_getProtocolVersion(aProtocolVersion);
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

// implementation of wrapped IDnDTarget methods.
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDTarget::enter(ULONG aScreenId, ULONG aX, ULONG aY,
                              DnDAction_T aDefaultAction,
                              const std::vector<DnDAction_T> &aAllowedActions,
                              const std::vector<com::Utf8Str> &aFormats,
                              DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */
    if (aDefaultAction == DnDAction_Ignore)
        return setError(E_INVALIDARG, tr("No default action specified"));
    if (!aAllowedActions.size())
        return setError(E_INVALIDARG, tr("Number of allowed actions is empty"));
    if (!aFormats.size())
        return setError(E_INVALIDARG, tr("Number of supported formats is empty"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Determine guest DnD protocol to use. */
    GuestDnDBase::getProtocolVersion(&mData.mProtocolVersion);

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the supported format list. */
    Utf8Str strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    /* Adjust the coordinates in a multi-monitor setup. */
    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_ENTER,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    LogFlowFunc(("hr=%Rhrc, resAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::move(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T aDefaultAction,
                             const std::vector<DnDAction_T> &aAllowedActions,
                             const std::vector<com::Utf8Str> &aFormats,
                             DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions. */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the supported format list. */
    RTCString strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_MOVE,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
                resAction = GuestDnD::toMainAction(pResp->defAction());
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    LogFlowFunc(("hr=%Rhrc, *pResultAction=%ld\n", hr, resAction));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::leave(ULONG uScreenId)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;
    int rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_LEAVE,
                                      0 /* cParms */, NULL /* paParms */);
    if (RT_SUCCESS(rc))
    {
        GuestDnDResponse *pResp = GuestDnDInst()->response();
        if (pResp)
            pResp->waitForGuestResponse();
    }

    LogFlowFunc(("hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

HRESULT GuestDnDTarget::drop(ULONG aScreenId, ULONG aX, ULONG aY,
                             DnDAction_T aDefaultAction,
                             const std::vector<DnDAction_T> &aAllowedActions,
                             const std::vector<com::Utf8Str> &aFormats,
                             com::Utf8Str &aFormat, DnDAction_T *aResultAction)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /* Input validation. */

    /* Everything else is optional. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Default action is ignoring. */
    DnDAction_T resAction = DnDAction_Ignore;

    /* Check & convert the drag & drop actions. */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    GuestDnD::toHGCMActions(aDefaultAction, &uDefAction,
                            aAllowedActions, &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the supported format list. */
    Utf8Str strFormats = GuestDnD::toFormatString(m_strFormats, aFormats);
    /* If there is no valid supported format, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    /* Adjust the coordinates in a multi-monitor setup. */
    int rc = GuestDnDInst()->adjustScreenCoordinates(aScreenId, &aX, &aY);
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[8];
        int i = 0;
        paParms[i++].setUInt32(aScreenId);
        paParms[i++].setUInt32(aX);
        paParms[i++].setUInt32(aY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        rc = GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_DROPPED,
                                      i, paParms);
        if (RT_SUCCESS(rc))
        {
            GuestDnDResponse *pResp = GuestDnDInst()->response();
            if (pResp && RT_SUCCESS(pResp->waitForGuestResponse()))
            {
                resAction = GuestDnD::toMainAction(pResp->defAction());
                aFormat = pResp->format();

                LogFlowFunc(("resFormat=%s, resAction=%RU32\n",
                             pResp->format().c_str(), pResp->defAction()));
            }
        }
    }

    if (aResultAction)
        *aResultAction = resAction;

    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

/* static */
DECLCALLBACK(int) GuestDnDTarget::i_sendDataThread(RTTHREAD Thread, void *pvUser)
{
    LogFlowFunc(("pvUser=%p\n", pvUser));

    SendDataTask *pTask = (SendDataTask *)pvUser;
    AssertPtrReturn(pTask, VERR_INVALID_POINTER);

    const ComObjPtr<GuestDnDTarget> pTarget(pTask->getTarget());
    Assert(!pTarget.isNull());

    int rc;

    AutoCaller autoCaller(pTarget);
    if (SUCCEEDED(autoCaller.rc()))
    {
        rc = pTarget->i_sendData(pTask->getCtx());
        /* Nothing to do here anymore. */
    }
    else
         rc = VERR_COM_INVALID_OBJECT_STATE;

    LogFlowFunc(("pTarget=%p returning rc=%Rrc\n", (GuestDnDTarget *)pTarget, rc));

    delete pTask;
    return rc;
}

/**
 * Initiates a data transfer from the host to the guest. The source is the host whereas the target is the
 * guest in this case.
 *
 * @return  HRESULT
 * @param   aScreenId
 * @param   aFormat
 * @param   aData
 * @param   aProgress
 */
HRESULT GuestDnDTarget::sendData(ULONG aScreenId, const com::Utf8Str &aFormat, const std::vector<BYTE> &aData,
                                 ComPtr<IProgress> &aProgress)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    /** @todo Add input validation. */
    /** @todo Check if another sendData() call currently is being processed. */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;
    int vrc;

    /* Note: At the moment we only support one response at a time. */
    GuestDnDResponse *pResp = GuestDnDInst()->response();
    if (pResp)
    {
        pResp->resetProgress(m_pGuest);

        try
        {
            PSENDDATACTX pSendCtx = new SENDDATACTX;
            RT_BZERO(pSendCtx, sizeof(SENDDATACTX));

            pSendCtx->mpTarget  = this;
            pSendCtx->mpResp    = pResp;
            pSendCtx->mScreenID = aScreenId;
            pSendCtx->mFormat   = aFormat;
            pSendCtx->mData     = aData;

            SendDataTask *pTask = new SendDataTask(this, pSendCtx);
            AssertReturn(pTask->isOk(), pTask->getRC());

            vrc = RTThreadCreate(NULL, GuestDnDTarget::i_sendDataThread,
                                 (void *)pTask, 0, RTTHREADTYPE_MAIN_WORKER, 0, "dndTgtSndData");
            if (RT_SUCCESS(vrc))
            {
                hr = pResp->queryProgressTo(aProgress.asOutParam());
                ComAssertComRC(hr);

                /* Note: pTask is now owned by the worker thread. */
            }
            else if (pSendCtx)
                delete pSendCtx;
        }
        catch(std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }

        /*if (RT_FAILURE(vrc)) ** @todo SetError(...) */
    }
    /** @todo SetError(...) */

    return hr;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

int GuestDnDTarget::i_cancelOperation(void)
{
    /** @todo Check for pending cancel requests. */

#if 0 /** @todo Later. */
    /* Cancel any outstanding waits for guest responses first. */
    if (pResp)
        pResp->notifyAboutGuestResponse();
#endif

    LogFlowFunc(("Cancelling operation, telling guest ...\n"));
    return GuestDnDInst()->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_CANCEL, 0 /* cParms */, NULL /*paParms*/);
}

int GuestDnDTarget::i_sendData(PSENDDATACTX pCtx)
{
    AssertPtrReturn(pCtx,  VERR_INVALID_POINTER);

#define DATA_IS_VALID_BREAK(x) \
    if (!x) \
    { \
        LogFlowFunc(("Invalid URI data value for \"" #x "\"\n")); \
        rc = VERR_INVALID_PARAMETER; \
        break; \
    }

    GuestDnD *pInst = GuestDnDInst();
    if (!pInst)
        return VERR_INVALID_POINTER;

    int rc;

    ASMAtomicWriteBool(&pCtx->mIsActive, true);

    do
    {
        const char *pszFormat = pCtx->mFormat.c_str();
        DATA_IS_VALID_BREAK(pszFormat);
        uint32_t cbFormat = pCtx->mFormat.length() + 1;

        /* Do we need to build up a file tree? */
        bool fHasURIList = DnDMIMEHasFileURLs(pszFormat, cbFormat);
        if (fHasURIList)
        {
            rc = i_sendURIData(pCtx);
        }
        else
        {
            GuestDnDMsg Msg;

            size_t cbDataTotal = pCtx->mData.size();
            DATA_IS_VALID_BREAK(cbDataTotal);

            /* Just copy over the raw data. */
            Msg.setType(DragAndDropSvc::HOST_DND_HG_SND_DATA);
            Msg.setNextUInt32(pCtx->mScreenID);
            Msg.setNextPointer((void *)pCtx->mFormat.c_str(), (uint32_t)pCtx->mFormat.length() + 1);
            Msg.setNextUInt32((uint32_t)pCtx->mFormat.length() + 1);
            Msg.setNextPointer((void*)&pCtx->mData.front(), (uint32_t)cbDataTotal);
            Msg.setNextUInt32(cbDataTotal);

            LogFlowFunc(("%zu total bytes of raw data to transfer\n", cbDataTotal));

            /* Make the initial call to the guest by sending the actual data. This might
             * be an URI list which in turn can lead to more data to send afterwards. */
            rc = pInst->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
            if (RT_FAILURE(rc))
                break;
        }

    } while (0);

    ASMAtomicWriteBool(&pCtx->mIsActive, false);

#undef DATA_IS_VALID_BREAK

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendDirectory(PSENDDATACTX pCtx, GuestDnDMsg *pMsg, DnDURIObject &aDirectory)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    RTCString strPath = aDirectory.GetDestPath();
    if (strPath.isEmpty())
        return VERR_INVALID_PARAMETER;
    if (strPath.length() >= RTPATH_MAX) /* Note: Maximum is RTPATH_MAX on guest side. */
        return VERR_BUFFER_OVERFLOW;

    LogFlowFunc(("Sending directory \"%s\" using protocol v%RU32 ...\n", strPath.c_str(), mData.mProtocolVersion));

    pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    pMsg->setNextString(strPath.c_str());                  /* path */
    pMsg->setNextUInt32((uint32_t)(strPath.length() + 1)); /* path length - note: Maximum is RTPATH_MAX on guest side. */
    pMsg->setNextUInt32(aDirectory.GetMode());             /* mode */

    return VINF_SUCCESS;
}

int GuestDnDTarget::i_sendFile(PSENDDATACTX pCtx, GuestDnDMsg *pMsg, DnDURIObject &aFile)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    RTCString strPathSrc = aFile.GetSourcePath();
    if (strPathSrc.isEmpty())
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;

    LogFlowFunc(("Sending \"%s\" (%RU32 bytes buffer) using protocol v%RU32 ...\n",
                 strPathSrc.c_str(), m_cbBlockSize, mData.mProtocolVersion));

    bool fOpen = aFile.IsOpen();
    if (!fOpen)
    {
        LogFlowFunc(("Opening \"%s\" ...\n", strPathSrc.c_str()));
        rc = aFile.OpenEx(strPathSrc, DnDURIObject::File, DnDURIObject::Source,
                          RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, 0 /* fFlags */);           
    }

    bool fSendFileData = false;
    if (RT_SUCCESS(rc)) 
    {
        if (mData.mProtocolVersion >= 2)
        {
            if (!fOpen) 
            {
                /*
                 * Since protocol v2 the file header and the actual file contents are
                 * separate messages, so send the file header first.
                 * The just registered callback will be called by the guest afterwards.
                 */
                pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
                pMsg->setNextUInt32(0);                                            /* context ID */
                rc = pMsg->setNextString(aFile.GetDestPath().c_str());             /* pvName */
                AssertRC(rc);
                pMsg->setNextUInt32((uint32_t)(aFile.GetDestPath().length() + 1)); /* cbName */
                pMsg->setNextUInt32(0);                                            /* uFlags */
                pMsg->setNextUInt32(aFile.GetMode());                              /* fMode */
                pMsg->setNextUInt64(aFile.GetSize());                              /* uSize */

                LogFlowFunc(("Sending file header ...\n"));
            }
            else
            {
                /* File header was sent, so only send the actual file data. */
                fSendFileData = true;
            }
        }
        else /* Protocol v1. */
        {
            /* Always send the file data, every time. */
            fSendFileData = true;
        }
    }

    if (   RT_SUCCESS(rc)
        && fSendFileData)
    {
        rc = i_sendFileData(pCtx, pMsg, aFile);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendFileData(PSENDDATACTX pCtx, GuestDnDMsg *pMsg, DnDURIObject &aFile)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    GuestDnDResponse *pResp = pCtx->mpResp;
    AssertPtr(pResp);

    /** @todo Don't allow concurrent reads per context! */

    /* Something to transfer? */
    if (   pCtx->mURI.lstURI.IsEmpty()
        || !pCtx->mIsActive)
    {
        return VERR_WRONG_ORDER;
    }

    /*
     * Start sending stuff.
     */

    /* Set the message type. */
    pMsg->setType(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

    /* Protocol version 1 sends the file path *every* time with a new file chunk.
     * In protocol version 2 we only do this once with HOST_DND_HG_SND_FILE_HDR. */
    if (mData.mProtocolVersion <= 1)
    {
        pMsg->setNextString(aFile.GetSourcePath().c_str());                  /* pvName */
        pMsg->setNextUInt32((uint32_t)(aFile.GetSourcePath().length() + 1)); /* cbName */
    }
    else
    {
        /* Protocol version 2 also sends the context ID. Currently unused. */
        pMsg->setNextUInt32(0);                                              /* context ID */
    }

    uint32_t cbRead = 0;

    int rc = aFile.Read(pCtx->mURI.pvScratchBuf, pCtx->mURI.cbScratchBuf, &cbRead);
    if (RT_SUCCESS(rc))
    {
        pCtx->mURI.cbProcessed += cbRead;

        if (mData.mProtocolVersion <= 1)
        {
            pMsg->setNextPointer(pCtx->mURI.pvScratchBuf, cbRead);  /* pvData */
            pMsg->setNextUInt32(cbRead);                            /* cbData */
            pMsg->setNextUInt32(aFile.GetMode());                   /* fMode */
        }
        else
        {
            pMsg->setNextPointer(pCtx->mURI.pvScratchBuf, cbRead); /* pvData */
            pMsg->setNextUInt32(cbRead);                           /* cbData */
        }

        if (aFile.IsComplete()) /* Done reading? */
        {
            LogFlowFunc(("File \"%s\" complete\n", aFile.GetSourcePath().c_str()));
            rc = VINF_EOF;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
DECLCALLBACK(int) GuestDnDTarget::i_sendURIDataCallback(uint32_t uMsg, void *pvParms, size_t cbParms, void *pvUser)
{
    PSENDDATACTX pCtx = (PSENDDATACTX)pvUser;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    GuestDnDTarget *pThis = pCtx->mpTarget;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("pThis=%p, uMsg=%RU32\n", pThis, uMsg));

    int rc = VINF_SUCCESS;

    switch (uMsg)
    {
        case DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG:
        {
            DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSG pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSG>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGGETNEXTHOSTMSG) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            GuestDnDMsg *pMsg;
            try
            {
                pMsg = new GuestDnDMsg();
                rc = pThis->i_sendURIDataLoop(pCtx, pMsg);
                if (RT_SUCCESS(rc))
                {
                    rc = pThis->addMsg(pMsg);
                    if (RT_SUCCESS(rc)) /* Return message type & required parameter count to the guest. */
                    {
                        LogFlowFunc(("GUEST_DND_GET_NEXT_HOST_MSG -> %RU32 (%RU32 params)\n", pMsg->getType(), pMsg->getCount()));
                        pCBData->uMsg   = pMsg->getType();
                        pCBData->cParms = pMsg->getCount();
                    }
                }

                if (RT_FAILURE(rc))
                {
                    if (rc == VERR_NO_DATA) /* All URI objects processed? */
                    {
                        /* Unregister this callback. */
                        AssertPtr(pCtx->mpResp);
                        int rc2 = pCtx->mpResp->setCallback(uMsg, NULL /* PFNGUESTDNDCALLBACK */);
                        if (RT_FAILURE(rc2))
                            LogFlowFunc(("Error: Unable to unregister callback for message %RU32, rc=%Rrc\n", uMsg, rc2));
                    }

                    delete pMsg;
                }
            }
            catch(std::bad_alloc & /*e*/)
            {
                rc = VERR_NO_MEMORY;
            }
            break;
        }
        case DragAndDropSvc::HOST_DND_HG_SND_DIR:
        case DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR:
        case DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSGDATA pCBData
                = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGGETNEXTHOSTMSGDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGGETNEXTHOSTMSGDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            LogFlowFunc(("pCBData->uMsg=%RU32, paParms=%p, cParms=%RU32\n", pCBData->uMsg, pCBData->paParms, pCBData->cParms));

            GuestDnDMsg *pMsg = pThis->nextMsg();
            if (pMsg)
            {
                /*
                 * Sanity checks.
                 */
                if (   pCBData->uMsg    != uMsg
                    || pCBData->paParms == NULL
                    || pCBData->cParms  != pMsg->getCount())
                {
                    rc = VERR_INVALID_PARAMETER;
                }

                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("Returning uMsg=%RU32\n", uMsg));
                    rc = HGCM::Message::copyParms(pMsg->getCount(), pMsg->getParms(), pCBData->paParms);
                    if (RT_SUCCESS(rc))
                    {
                        pCBData->cParms = pMsg->getCount();
                        pThis->removeNext();
                    }
                    else
                        LogFlowFunc(("Copying parameters failed with rc=%Rrc\n", rc));
                }
            }
            else
                rc = VERR_NO_DATA;

            LogFlowFunc(("Processing next message ended with rc=%Rrc\n", rc));
            break;
        }
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_FAILURE(rc))
    {
        if (pCtx->mURI.SemEvent != NIL_RTSEMEVENT)
        {
            LogFlowFunc(("Signalling ...\n"));
            int rc2 = RTSemEventSignal(pCtx->mURI.SemEvent);
            AssertRC(rc2);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc; /* Tell the guest. */
}

int GuestDnDTarget::i_sendURIData(PSENDDATACTX pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);
    AssertPtr(pCtx->mpResp);

#define URI_DATA_IS_VALID_BREAK(x) \
    if (!x) \
    { \
        LogFlowFunc(("Invalid URI data value for \"" #x "\"\n")); \
        rc = VERR_INVALID_PARAMETER; \
        break; \
    }

    void *pvBuf = RTMemAlloc(m_cbBlockSize);
    if (!pvBuf)
        return VERR_NO_MEMORY;

    int rc;

#define REGISTER_CALLBACK(x) \
    rc = pCtx->mpResp->setCallback(x, i_sendURIDataCallback, pCtx); \
    if (RT_FAILURE(rc)) \
        return rc;

#define UNREGISTER_CALLBACK(x) \
    rc = pCtx->mpResp->setCallback(x, NULL); \
    AssertRC(rc);

    /*
     * Register callbacks.
     */
    /* Generic callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG);
    /* Host callbacks. */
    REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    if (mData.mProtocolVersion >= 2)
        REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
    REGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

    do
    {
        /*
         * Set our scratch buffer.
         */
        pCtx->mURI.pvScratchBuf = pvBuf;
        pCtx->mURI.cbScratchBuf = m_cbBlockSize;

        /* Create event semaphore. */
        pCtx->mURI.SemEvent = NIL_RTSEMEVENT;
        rc = RTSemEventCreate(&pCtx->mURI.SemEvent);
        if (RT_FAILURE(rc))
            break;

        /*
         * Extract URI list from byte data.
         */
        DnDURIList &lstURI = pCtx->mURI.lstURI; /* Use the URI list from the context. */

        const char *pszList = (const char *)&pCtx->mData.front();
        URI_DATA_IS_VALID_BREAK(pszList);

        uint32_t cbList = pCtx->mData.size();
        URI_DATA_IS_VALID_BREAK(cbList);

        RTCList<RTCString> lstURIOrg = RTCString(pszList, cbList).split("\r\n");
        URI_DATA_IS_VALID_BREAK(!lstURIOrg.isEmpty());

        rc = lstURI.AppendURIPathsFromList(lstURIOrg, 0 /* fFlags */);
        if (RT_SUCCESS(rc))
            LogFlowFunc(("URI root objects: %zu, total bytes (raw data to transfer): %zu\n",
                         lstURI.RootCount(), lstURI.TotalBytes()));
        else
            break;

        pCtx->mURI.cbProcessed = 0;
        pCtx->mURI.cProcessed  = 0;
        pCtx->mURI.cbToProcess = lstURI.TotalBytes();

        /*
         * The first message always is the meta info for the data. The meta
         * info *only* contains the root elements of an URI list.
         *
         * After the meta data we generate the messages required to send the data itself.
         */
        Assert(!lstURI.IsEmpty());
        RTCString strData = lstURI.RootToString().c_str();
        size_t    cbData  = strData.length() + 1; /* Include terminating zero. */

        GuestDnDMsg Msg;
        Msg.setType(DragAndDropSvc::HOST_DND_HG_SND_DATA);
        Msg.setNextUInt32(pCtx->mScreenID);
        Msg.setNextPointer((void *)pCtx->mFormat.c_str(), (uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextUInt32((uint32_t)pCtx->mFormat.length() + 1);
        Msg.setNextPointer((void*)strData.c_str(), (uint32_t)cbData);
        Msg.setNextUInt32((uint32_t)cbData);

        rc = GuestDnDInst()->hostCall(Msg.getType(), Msg.getCount(), Msg.getParms());
        if (RT_SUCCESS(rc))
        {
            /*
             * Wait until our callback i_sendURIDataCallback triggered the
             * wait event.
             */
            LogFlowFunc(("Waiting for URI callback ...\n"));
            rc = RTSemEventWait(pCtx->mURI.SemEvent, RT_INDEFINITE_WAIT);
            LogFlowFunc(("URI callback done\n"));
        }

    } while (0);

     if (pCtx->mURI.SemEvent != NIL_RTSEMEVENT)
     {
         RTSemEventDestroy(pCtx->mURI.SemEvent);
         pCtx->mURI.SemEvent = NIL_RTSEMEVENT;
     }

    /*
     * Unregister callbacksagain.
     */
    /* Guest callbacks. */
    UNREGISTER_CALLBACK(DragAndDropSvc::GUEST_DND_GET_NEXT_HOST_MSG);
    /* Host callbacks. */
    UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_DIR);
    if (mData.mProtocolVersion >= 2)
        UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_HDR);
    UNREGISTER_CALLBACK(DragAndDropSvc::HOST_DND_HG_SND_FILE_DATA);

#undef REGISTER_CALLBACK
#undef UNREGISTER_CALLBACK

    /* Destroy temporary scratch buffer. */
    if (pvBuf)
        RTMemFree(pvBuf);

#undef URI_DATA_IS_VALID_BREAK

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnDTarget::i_sendURIDataLoop(PSENDDATACTX pCtx, GuestDnDMsg *pMsg)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    DnDURIList &lstURI = pCtx->mURI.lstURI;

    int rc;

    uint64_t cbTotal = pCtx->mURI.cbToProcess;
    uint8_t uPercent = pCtx->mURI.cbProcessed * 100 / (cbTotal ? cbTotal : 1);

    LogFlowFunc(("%RU64 / %RU64 -- %RU8%%\n", pCtx->mURI.cbProcessed, cbTotal, uPercent));

    bool fComplete = (uPercent >= 100) || lstURI.IsEmpty();

    if (pCtx->mpResp)
    {
        int rc2 = pCtx->mpResp->setProgress(uPercent,
                                              fComplete
                                            ? DragAndDropSvc::DND_PROGRESS_COMPLETE
                                            : DragAndDropSvc::DND_PROGRESS_RUNNING);
        AssertRC(rc2);
    }

    if (fComplete)
    {
        LogFlowFunc(("Last URI item processed, bailing out\n"));
        return VERR_NO_DATA;
    }

    Assert(!lstURI.IsEmpty());
    DnDURIObject &curObj = lstURI.First();

    uint32_t fMode = curObj.GetMode();
    LogFlowFunc(("Processing srcPath=%s, dstPath=%s, fMode=0x%x, cbSize=%RU32, fIsDir=%RTbool, fIsFile=%RTbool\n",
                 curObj.GetSourcePath().c_str(), curObj.GetDestPath().c_str(),
                 fMode, curObj.GetSize(),
                 RTFS_IS_DIRECTORY(fMode), RTFS_IS_FILE(fMode)));

    if (RTFS_IS_DIRECTORY(fMode))
    {
        rc = i_sendDirectory(pCtx, pMsg, curObj);
    }
    else if (RTFS_IS_FILE(fMode))
    {
        rc = i_sendFile(pCtx, pMsg, curObj);
    }
    else
    {
        AssertMsgFailed(("fMode=0x%x is not supported for srcPath=%s, dstPath=%s\n",
                         fMode, curObj.GetSourcePath().c_str(), curObj.GetDestPath().c_str()));
        rc = VERR_NOT_SUPPORTED;
    }

    bool fRemove = false; /* Remove current entry? */
    if (   curObj.IsComplete()
        || RT_FAILURE(rc))
    {
        fRemove = true;
    }

    if (fRemove)
    {
        LogFlowFunc(("Removing \"%s\" from list, rc=%Rrc\n", curObj.GetSourcePath().c_str(), rc));
        lstURI.RemoveFirst();
    }

    if (   pCtx->mpResp
        && pCtx->mpResp->isProgressCanceled())
    {
        LogFlowFunc(("Cancelling ...\n"));

        rc = i_cancelOperation();
        if (RT_SUCCESS(rc))
            rc = VERR_CANCELLED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

HRESULT GuestDnDTarget::cancel(BOOL *aVeto)
{
#if !defined(VBOX_WITH_DRAG_AND_DROP)
    ReturnComNotImplemented();
#else /* VBOX_WITH_DRAG_AND_DROP */

    int rc = i_cancelOperation();

    if (aVeto)
        *aVeto = FALSE; /** @todo */

    return RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
#endif /* VBOX_WITH_DRAG_AND_DROP */
}

