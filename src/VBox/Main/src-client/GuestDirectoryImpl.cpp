
/* $Id$ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include "GuestDirectoryImpl.h"
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "AutoCaller.h"

#include <VBox/com/array.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDirectory)

HRESULT GuestDirectory::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDirectory::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestDirectory::init(GuestSession *aSession,
                         const Utf8Str &strPath, const Utf8Str &strFilter, uint32_t uFlags)
{
    LogFlowThisFunc(("strPath=%s, strFilter=%s, uFlags=%x\n",
                     strPath.c_str(), strFilter.c_str(), uFlags));

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    mData.mSession = aSession;
    mData.mName    = strPath;
    mData.mFilter  = strFilter;
    mData.mFlags   = uFlags;

    /* Start the directory process on the guest. */
    GuestProcessStartupInfo procInfo;
    procInfo.mName      = Utf8StrFmt(tr("Reading directory \"%s\"", strPath.c_str()));
    procInfo.mCommand   = Utf8Str(VBOXSERVICE_TOOL_LS);
    procInfo.mTimeoutMS = 5 * 60 * 1000; /* 5 minutes timeout. */
    procInfo.mFlags     = ProcessCreateFlag_WaitForStdOut;

    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    /* We want the long output format which contains all the object details. */
    procInfo.mArguments.push_back(Utf8Str("-l"));
#if 0 /* Flags are not supported yet. */
    if (uFlags & DirectoryOpenFlag_NoSymlinks)
        procInfo.mArguments.push_back(Utf8Str("--nosymlinks")); /** @todo What does GNU here? */
#endif
    /** @todo Recursion support? */
    procInfo.mArguments.push_back(strPath); /* The directory we want to open. */

    /*
     * Start the process asynchronously and keep it around so that we can use
     * it later in subsequent read() calls.
     * Note: No guest rc available because operation is asynchronous.
     */
    int rc = mData.mProcessTool.Init(mData.mSession, procInfo,
                                     true /* Async */, NULL /* Guest rc */);
    if (RT_SUCCESS(rc))
    {
        /* Confirm a successful initialization when it's the case. */
        autoInitSpan.setSucceeded();
        return rc;
    }

    autoInitSpan.setFailed();
    return rc;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestDirectory::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    LogFlowThisFuncLeave();
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestDirectory::COMGETTER(DirectoryName)(BSTR *aName)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP GuestDirectory::COMGETTER(Filter)(BSTR *aFilter)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aFilter);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mFilter.cloneTo(aFilter);

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestDirectory::Close(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AssertPtr(mData.mSession);
    int rc = mData.mSession->directoryRemoveFromList(this);

    mData.mProcessTool.Terminate();

    /*
     * Release autocaller before calling uninit.
     */
    autoCaller.release();

    uninit();

    LogFlowFuncLeaveRC(rc);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestDirectory::Read(IFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestProcessStreamBlock curBlock;
    int guestRc;

    int rc = mData.mProcessTool.WaitEx(GUESTPROCESSTOOL_FLAG_STDOUT_BLOCK,
                                       &curBlock, &guestRc);

    /*
     * Note: The guest process can still be around to serve the next
     *       upcoming stream block next time.
     */
    if (   RT_SUCCESS(rc)
        && !mData.mProcessTool.IsRunning())
    {
        rc = mData.mProcessTool.TerminatedOk(NULL /* Exit code */);
        if (rc == VERR_NOT_EQUAL)
            rc = VERR_ACCESS_DENIED;
    }

    if (RT_SUCCESS(rc))
    {
        if (curBlock.GetCount()) /* Did we get content? */
        {
            GuestFsObjData objData;
            rc = objData.FromLs(curBlock);
            if (RT_FAILURE(rc))
                rc = VERR_PATH_NOT_FOUND;

            if (RT_SUCCESS(rc))
            {
                /* Create the object. */
                ComObjPtr<GuestFsObjInfo> pFsObjInfo;
                HRESULT hr2 = pFsObjInfo.createObject();
                if (FAILED(hr2))
                    rc = VERR_COM_UNEXPECTED;

                if (RT_SUCCESS(rc))
                    rc = pFsObjInfo->init(objData);

                if (RT_SUCCESS(rc))
                {
                    /* Return info object to the caller. */
                    hr2 = pFsObjInfo.queryInterfaceTo(aInfo);
                    if (FAILED(hr2))
                        rc = VERR_COM_UNEXPECTED;
                }
            }
        }
        else
        {
            /* Nothing to read anymore. Tell the caller. */
            rc = VERR_NO_MORE_FILES;
        }
    }

    HRESULT hr = S_OK;

    if (RT_FAILURE(rc)) /** @todo Add more errors here. */
    {
        switch (rc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            case VERR_ACCESS_DENIED:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Reading directory \"%s\" failed: Unable to read / access denied"),
                              mData.mName.c_str());
                break;

            case VERR_PATH_NOT_FOUND:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Reading directory \"%s\" failed: Path not found"),
                              mData.mName.c_str());
                break;

            case VERR_NO_MORE_FILES:
                /* See SDK reference. */
                hr = setError(VBOX_E_OBJECT_NOT_FOUND, tr("No more entries for directory \"%s\""),
                              mData.mName.c_str());
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Error while reading directory \"%s\": %Rrc\n"),
                              mData.mName.c_str(), rc);
                break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

