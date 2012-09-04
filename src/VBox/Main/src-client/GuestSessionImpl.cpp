
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
#include "GuestImpl.h"
#include "GuestSessionImpl.h"
#include "GuestCtrlImplPrivate.h"

#include "Global.h"
#include "AutoCaller.h"
#include "ProgressImpl.h"

#include <memory> /* For auto_ptr. */

#include <iprt/env.h>
#include <iprt/file.h> /* For CopyTo/From. */

#include <VBox/com/array.h>
#include <VBox/version.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestSession)

HRESULT GuestSession::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestSession::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestSession::init(Guest *aGuest, ULONG aSessionID,
                       Utf8Str aUser, Utf8Str aPassword, Utf8Str aDomain, Utf8Str aName)
{
    LogFlowThisFuncEnter();

    AssertPtrReturn(aGuest, VERR_INVALID_POINTER);

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), VERR_OBJECT_DESTROYED);

    mData.mTimeout = 30 * 60 * 1000; /* Session timeout is 30 mins by default. */
    mData.mParent = aGuest;
    mData.mId = aSessionID;

    mData.mCredentials.mUser = aUser;
    mData.mCredentials.mPassword = aPassword;
    mData.mCredentials.mDomain = aDomain;
    mData.mName = aName;
    mData.mNumObjects = 0;

    /* Confirm a successful initialization when it's the case. */
    autoInitSpan.setSucceeded();

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestSession::uninit(void)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_GUEST_CONTROL
    LogFlowThisFunc(("Closing directories (%RU64 total)\n",
                     mData.mDirectories.size()));
    for (SessionDirectories::iterator itDirs = mData.mDirectories.begin();
         itDirs != mData.mDirectories.end(); ++itDirs)
    {
#ifdef DEBUG
        ULONG cRefs = (*itDirs)->AddRef();
        LogFlowThisFunc(("pFile=%p, cRefs=%RU32\n", (*itDirs), cRefs));
        (*itDirs)->Release();
#endif
        (*itDirs)->uninit();
    }
    mData.mDirectories.clear();

    LogFlowThisFunc(("Closing files (%RU64 total)\n",
                     mData.mFiles.size()));
    for (SessionFiles::iterator itFiles = mData.mFiles.begin();
         itFiles != mData.mFiles.end(); ++itFiles)
    {
#ifdef DEBUG
        ULONG cRefs = (*itFiles)->AddRef();
        LogFlowThisFunc(("pFile=%p, cRefs=%RU32\n", (*itFiles), cRefs));
        (*itFiles)->Release();
#endif
        (*itFiles)->uninit();
    }
    mData.mFiles.clear();

    LogFlowThisFunc(("Closing processes (%RU64 total)\n",
                     mData.mProcesses.size()));
    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
#ifdef DEBUG
        ULONG cRefs = itProcs->second->AddRef();
        LogFlowThisFunc(("pProcess=%p, cRefs=%RU32\n", itProcs->second, cRefs));
        itProcs->second->Release();
#endif
        itProcs->second->uninit();
    }
    mData.mProcesses.clear();

    LogFlowThisFunc(("mNumObjects=%RU32\n", mData.mNumObjects));
#endif
    LogFlowFuncLeaveRC(rc);
}

// implementation of public getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestSession::COMGETTER(User)(BSTR *aUser)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aUser);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mCredentials.mUser.cloneTo(aUser);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Domain)(BSTR *aDomain)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aDomain);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mCredentials.mDomain.cloneTo(aDomain);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Name)(BSTR *aName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mName.cloneTo(aName);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Id)(ULONG *aId)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aId = mData.mId;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Timeout)(ULONG *aTimeout)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aTimeout);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTimeout = mData.mTimeout;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMSETTER(Timeout)(ULONG aTimeout)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mTimeout = aTimeout;

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Environment)(ComSafeArrayOut(BSTR, aEnvironment))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aEnvironment);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    size_t cEnvVars = mData.mEnvironment.Size();
    LogFlowThisFunc(("%s cEnvVars=%RU32\n", mData.mName.c_str(), cEnvVars));
    com::SafeArray<BSTR> environment(cEnvVars);

    for (size_t i = 0; i < cEnvVars; i++)
    {
        Bstr strEnv(mData.mEnvironment.Get(i));
        strEnv.cloneTo(&environment[i]);
    }
    environment.detachTo(ComSafeArrayOutArg(aEnvironment));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMSETTER(Environment)(ComSafeArrayIn(IN_BSTR, aValues))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<IN_BSTR> environment(ComSafeArrayInArg(aValues));

    int rc = VINF_SUCCESS;
    for (size_t i = 0; i < environment.size() && RT_SUCCESS(rc); i++)
    {
        Utf8Str strEnv(environment[i]);
        if (!strEnv.isEmpty()) /* Silently skip empty entries. */
            rc = mData.mEnvironment.Set(strEnv);
    }

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
    LogFlowFuncLeaveRC(hr);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Processes)(ComSafeArrayOut(IGuestProcess *, aProcesses))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aProcesses);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestProcess> collection(mData.mProcesses);
    collection.detachTo(ComSafeArrayOutArg(aProcesses));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Directories)(ComSafeArrayOut(IGuestDirectory *, aDirectories))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aDirectories);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestDirectory> collection(mData.mDirectories);
    collection.detachTo(ComSafeArrayOutArg(aDirectories));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::COMGETTER(Files)(ComSafeArrayOut(IGuestFile *, aFiles))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    CheckComArgOutSafeArrayPointerValid(aFiles);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IGuestFile> collection(mData.mFiles);
    collection.detachTo(ComSafeArrayOutArg(aFiles));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

// private methods
/////////////////////////////////////////////////////////////////////////////

int GuestSession::directoryRemoveFromList(GuestDirectory *pDirectory)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (SessionDirectories::iterator itDirs = mData.mDirectories.begin();
         itDirs != mData.mDirectories.end(); ++itDirs)
    {
        if (pDirectory == (*itDirs))
        {
            Bstr strName;
            HRESULT hr = (*itDirs)->COMGETTER(DirectoryName)(strName.asOutParam());
            ComAssertComRC(hr);

            Assert(mData.mDirectories.size());
            LogFlowFunc(("Removing directory \"%s\" (Session: %RU32) (now total %ld directories)\n",
                         Utf8Str(strName).c_str(), mData.mId, mData.mDirectories.size() - 1));

            mData.mDirectories.erase(itDirs);
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::directoryCreateInternal(const Utf8Str &strPath, uint32_t uMode, uint32_t uFlags, int *pGuestRc)
{
    LogFlowThisFunc(("strPath=%s, uMode=%x, uFlags=%x\n",
                     strPath.c_str(), uMode, uFlags));

    GuestProcessStartupInfo procInfo;
    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_MKDIR);
    procInfo.mFlags   = ProcessCreateFlag_Hidden;

    int vrc = VINF_SUCCESS;

    /* Construct arguments. */
    if (uFlags & DirectoryCreateFlag_Parents)
        procInfo.mArguments.push_back(Utf8Str("--parents")); /* We also want to create the parent directories. */
    if (uMode)
    {
        procInfo.mArguments.push_back(Utf8Str("--mode")); /* Set the creation mode. */

        char szMode[16];
        if (RTStrPrintf(szMode, sizeof(szMode), "%o", uMode))
        {
            procInfo.mArguments.push_back(Utf8Str(szMode));
        }
        else
            vrc = VERR_BUFFER_OVERFLOW;
    }
    procInfo.mArguments.push_back(strPath); /* The directory we want to create. */

    int guestRc;
    if (RT_SUCCESS(vrc))
    {
        GuestProcessTool procTool;
        vrc = procTool.Init(this, procInfo, false /* Async */, &guestRc);
        if (RT_SUCCESS(vrc))
        {
            if (RT_SUCCESS(guestRc))
                vrc = procTool.Wait(GUESTPROCESSTOOL_FLAG_NONE, &guestRc);
        }

        if (RT_SUCCESS(vrc))
        {
            if (RT_SUCCESS(guestRc))
                guestRc = procTool.TerminatedOk(NULL /* Exit code */);
        }

        if (pGuestRc)
            *pGuestRc = guestRc;
    }

    LogFlowFuncLeaveRC(vrc);
    if (RT_FAILURE(vrc))
        return vrc;
    return RT_SUCCESS(guestRc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Special guest control rc needed! */
}

int GuestSession::directoryQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    int vrc = fsQueryInfoInternal(strPath, objData, pGuestRc);
    if (RT_SUCCESS(vrc))
    {
        vrc = objData.mType == FsObjType_Directory
            ? VINF_SUCCESS : VERR_NOT_A_DIRECTORY;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::objectCreateTempInternal(const Utf8Str &strTemplate, const Utf8Str &strPath,
                                           bool fDirectory, const Utf8Str &strName, int *pGuestRc)
{
    GuestProcessStartupInfo procInfo;
    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_MKTEMP);
    procInfo.mFlags   = ProcessCreateFlag_WaitForStdOut;
    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    if (fDirectory)
        procInfo.mArguments.push_back(Utf8Str("-d"));
    if (strPath.length()) /* Otherwise use /tmp or equivalent. */
    {
        procInfo.mArguments.push_back(Utf8Str("-t"));
        procInfo.mArguments.push_back(strPath);
    }
    procInfo.mArguments.push_back(strTemplate);

    GuestProcessTool procTool; int guestRc;
    int vrc = procTool.Init(this, procInfo, false /* Async */, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        if (RT_SUCCESS(guestRc))
            vrc = procTool.Wait(GUESTPROCESSTOOL_FLAG_NONE, &guestRc);
    }

    if (RT_SUCCESS(vrc))
    {
        if (RT_SUCCESS(guestRc))
            guestRc = procTool.TerminatedOk(NULL /* Exit code */);
    }

    if (pGuestRc)
        *pGuestRc = guestRc;

    if (RT_FAILURE(vrc))
        return vrc;
    return RT_SUCCESS(guestRc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Special guest control rc needed! */
}

int GuestSession::directoryOpenInternal(const Utf8Str &strPath, const Utf8Str &strFilter,
                                        uint32_t uFlags, ComObjPtr<GuestDirectory> &pDirectory)
{
    LogFlowThisFunc(("strPath=%s, strPath=%s, uFlags=%x\n",
                     strPath.c_str(), strFilter.c_str(), uFlags));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Create the directory object. */
    HRESULT hr = pDirectory.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    int vrc = pDirectory->init(this /* Parent */,
                               strPath, strFilter, uFlags);
    if (RT_FAILURE(vrc))
        return vrc;

    /* Add the created directory to our vector. */
    mData.mDirectories.push_back(pDirectory);

    LogFlowFunc(("Added new directory \"%s\" (Session: %RU32)\n",
                 strPath.c_str(), mData.mId));

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::dispatchToProcess(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData)
{
    LogFlowFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    uint32_t uProcessID = VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(uContextID);
#ifdef DEBUG
    LogFlowFunc(("uProcessID=%RU32 (%RU32 total)\n",
                 uProcessID, mData.mProcesses.size()));
#endif
    int rc;
    SessionProcesses::const_iterator itProc
        = mData.mProcesses.find(uProcessID);
    if (itProc != mData.mProcesses.end())
    {
        ComObjPtr<GuestProcess> pProcess(itProc->second);
        Assert(!pProcess.isNull());

        alock.release();
        rc = pProcess->callbackDispatcher(uContextID, uFunction, pvData, cbData);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestSession::fileRemoveFromList(GuestFile *pFile)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (SessionFiles::iterator itFiles = mData.mFiles.begin();
         itFiles != mData.mFiles.end(); ++itFiles)
    {
        if (pFile == (*itFiles))
        {
            Bstr strName;
            HRESULT hr = (*itFiles)->COMGETTER(FileName)(strName.asOutParam());
            ComAssertComRC(hr);

            Assert(mData.mNumObjects);
            LogFlowThisFunc(("Removing file \"%s\" (Session: %RU32) (now total %ld files, %ld objects)\n",
                             Utf8Str(strName).c_str(), mData.mId, mData.mFiles.size() - 1, mData.mNumObjects - 1));
#ifdef DEBUG
            ULONG cRefs = pFile->AddRef();
            LogFlowThisFunc(("pObject=%p, cRefs=%RU32\n", pFile, cRefs));
            pFile->Release();
#endif
            mData.mFiles.erase(itFiles);
            mData.mNumObjects--;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::fileRemoveInternal(const Utf8Str &strPath, int *pGuestRc)
{
    GuestProcessStartupInfo procInfo;
    GuestProcessStream      streamOut;

    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_RM);
    procInfo.mFlags   = ProcessCreateFlag_WaitForStdOut;
    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    procInfo.mArguments.push_back(strPath); /* The file we want to remove. */

    GuestProcessTool procTool; int guestRc;
    int vrc = procTool.Init(this, procInfo, false /* Async */, &guestRc);
    if (RT_SUCCESS(vrc))
        vrc = procTool.Wait(GUESTPROCESSTOOL_FLAG_NONE, &guestRc);

    if (RT_SUCCESS(vrc))
    {
        if (RT_SUCCESS(guestRc))
            guestRc = procTool.TerminatedOk(NULL /* Exit code */);
    }

    if (pGuestRc)
        *pGuestRc = guestRc;

    if (RT_FAILURE(vrc))
        return vrc;
    return RT_SUCCESS(guestRc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Special guest control rc needed! */
}

int GuestSession::fileOpenInternal(const Utf8Str &strPath, const Utf8Str &strOpenMode, const Utf8Str &strDisposition,
                                   uint32_t uCreationMode, int64_t iOffset, ComObjPtr<GuestFile> &pFile, int *pGuestRc)
{
    LogFlowThisFunc(("strPath=%s, strOpenMode=%s, strDisposition=%s, uCreationMode=%x, iOffset=%RI64\n",
                     strPath.c_str(), strOpenMode.c_str(), strDisposition.c_str(), uCreationMode, iOffset));

    /* Create the directory object. */
    HRESULT hr = pFile.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    int vrc = pFile->init(this /* Parent */,
                          strPath, strOpenMode, strDisposition, uCreationMode, iOffset, pGuestRc);
    if (RT_FAILURE(vrc))
        return vrc;
    /** @todo Handle guestRc. */

    /* Add the created directory to our vector. */
    mData.mFiles.push_back(pFile);
    mData.mNumObjects++;
    Assert(mData.mNumObjects <= VBOX_GUESTCTRL_MAX_OBJECTS);

    LogFlowFunc(("Added new file \"%s\" (Session: %RU32) (now total %ld files, %ld objects)\n",
                 strPath.c_str(), mData.mId, mData.mProcesses.size(), mData.mNumObjects));

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::fileQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    int vrc = fsQueryInfoInternal(strPath, objData, pGuestRc);
    if (RT_SUCCESS(vrc))
    {
        vrc = objData.mType == FsObjType_File
            ? VINF_SUCCESS : VERR_NOT_A_FILE;
    }

    LogFlowFuncLeaveRC(vrc);
    return vrc;
}

int GuestSession::fileQuerySizeInternal(const Utf8Str &strPath, int64_t *pllSize, int *pGuestRc)
{
    AssertPtrReturn(pllSize, VERR_INVALID_POINTER);

    GuestFsObjData objData;
    int vrc = fileQueryInfoInternal(strPath, objData, pGuestRc);
    if (RT_SUCCESS(vrc))
        *pllSize = objData.mObjectSize;

    return vrc;
}

int GuestSession::fsQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc)
{
    LogFlowThisFunc(("strPath=%s\n", strPath.c_str()));

    /** @todo Merge this with IGuestFile::queryInfo(). */
    GuestProcessStartupInfo procInfo;
    procInfo.mCommand = Utf8Str(VBOXSERVICE_TOOL_STAT);
    procInfo.mFlags   = ProcessCreateFlag_WaitForStdOut;

    /* Construct arguments. */
    procInfo.mArguments.push_back(Utf8Str("--machinereadable"));
    procInfo.mArguments.push_back(strPath);

    GuestProcessTool procTool; int guestRc;
    int vrc = procTool.Init(this, procInfo, false /* Async */, &guestRc);
    if (RT_SUCCESS(vrc))
        vrc = procTool.Wait(GUESTPROCESSTOOL_FLAG_NONE, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        guestRc = procTool.TerminatedOk(NULL /* Exit code */);
        if (RT_SUCCESS(guestRc))
        {
            GuestProcessStreamBlock curBlock;
            vrc = procTool.GetCurrentBlock(OUTPUT_HANDLE_ID_STDOUT,  curBlock);
            /** @todo Check for more / validate blocks! */
            if (RT_SUCCESS(vrc))
                vrc = objData.FromStat(curBlock);
        }
    }

    if (pGuestRc)
        *pGuestRc = guestRc;

    LogFlowFuncLeaveRC(vrc);
    if (RT_FAILURE(vrc))
        return vrc;
    return RT_SUCCESS(guestRc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Special guest control rc needed! */
}

const GuestCredentials& GuestSession::getCredentials(void)
{
    return mData.mCredentials;
}

const GuestEnvironment& GuestSession::getEnvironment(void)
{
    return mData.mEnvironment;
}

Utf8Str GuestSession::getName(void)
{
    return mData.mName;
}

int GuestSession::processRemoveFromList(GuestProcess *pProcess)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_NOT_FOUND;

    ULONG uPID;
    HRESULT hr = pProcess->COMGETTER(PID)(&uPID);

    LogFlowFunc(("Closing process (PID=%RU32) ...\n", uPID));

    for (SessionProcesses::iterator itProcs = mData.mProcesses.begin();
         itProcs != mData.mProcesses.end(); ++itProcs)
    {
        if (pProcess == itProcs->second)
        {
            GuestProcess *pCurProc = itProcs->second;
            AssertPtr(pCurProc);

            hr = pCurProc->COMGETTER(PID)(&uPID);
            ComAssertComRC(hr);

            Assert(mData.mNumObjects);
            LogFlowFunc(("Removing process (Session: %RU32) with process ID=%RU32, guest PID=%RU32 (now total %ld processes, %ld objects)\n",
                         mData.mId, pCurProc->getProcessID(), uPID, mData.mProcesses.size() - 1, mData.mNumObjects - 1));

            mData.mProcesses.erase(itProcs);
            mData.mNumObjects--;

            rc = VINF_SUCCESS;
            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates but does *not* start the process yet. See GuestProcess::startProcess() or
 * GuestProcess::startProcessAsync() for that.
 *
 * @return  IPRT status code.
 * @param   procInfo
 * @param   pProcess
 */
int GuestSession::processCreateExInteral(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProcess)
{
    LogFlowFunc(("mCmd=%s, mFlags=%x, mTimeoutMS=%RU32\n",
                 procInfo.mCommand.c_str(), procInfo.mFlags, procInfo.mTimeoutMS));
#ifdef DEBUG
    if (procInfo.mArguments.size())
    {
        LogFlowFunc(("Arguments:"));
        ProcessArguments::const_iterator it = procInfo.mArguments.begin();
        while (it != procInfo.mArguments.end())
        {
            LogFlow((" %s", (*it).c_str()));
            it++;
        }
        LogFlow(("\n"));
    }
#endif

    /* Validate flags. */
    if (procInfo.mFlags)
    {
        if (   !(procInfo.mFlags & ProcessCreateFlag_IgnoreOrphanedProcesses)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForProcessStartOnly)
            && !(procInfo.mFlags & ProcessCreateFlag_Hidden)
            && !(procInfo.mFlags & ProcessCreateFlag_NoProfile)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdOut)
            && !(procInfo.mFlags & ProcessCreateFlag_WaitForStdErr))
        {
            return VERR_INVALID_PARAMETER;
        }
    }

    /* Adjust timeout. If set to 0, we define
     * an infinite timeout. */
    if (procInfo.mTimeoutMS == 0)
        procInfo.mTimeoutMS = UINT32_MAX;

    /** @tood Implement process priority + affinity. */

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_MAX_PROCS_REACHED;
    if (mData.mNumObjects >= VBOX_GUESTCTRL_MAX_OBJECTS)
        return rc;

    /* Create a new (host-based) process ID and assign it. */
    uint32_t uNewProcessID = 0;
    ULONG uTries = 0;

    for (;;)
    {
        /* Is the context ID already used? */
        if (!processExists(uNewProcessID, NULL /* pProgress */))
        {
            /* Callback with context ID was not found. This means
             * we can use this context ID for our new callback we want
             * to add below. */
            rc = VINF_SUCCESS;
            break;
        }
        uNewProcessID++;
        if (uNewProcessID == VBOX_GUESTCTRL_MAX_OBJECTS)
            uNewProcessID = 0;

        if (++uTries == UINT32_MAX)
            break; /* Don't try too hard. */
    }

    if (RT_FAILURE(rc))
        return rc;

    /* Create the process object. */
    HRESULT hr = pProcess.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    rc = pProcess->init(mData.mParent->getConsole() /* Console */, this /* Session */,
                        uNewProcessID, procInfo);
    if (RT_FAILURE(rc))
        return rc;

    /* Add the created process to our map. */
    mData.mProcesses[uNewProcessID] = pProcess;
    mData.mNumObjects++;
    Assert(mData.mNumObjects <= VBOX_GUESTCTRL_MAX_OBJECTS);

    LogFlowFunc(("Added new process (Session: %RU32) with process ID=%RU32 (now total %ld processes, %ld objects)\n",
                 mData.mId, uNewProcessID, mData.mProcesses.size(), mData.mNumObjects));

    return rc;
}

inline bool GuestSession::processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess)
{
    SessionProcesses::const_iterator it = mData.mProcesses.find(uProcessID);
    if (it != mData.mProcesses.end())
    {
        if (pProcess)
            *pProcess = it->second;
        return true;
    }
    return false;
}

inline int GuestSession::processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess)
{
    AssertReturn(uPID, false);
    /* pProcess is optional. */

    SessionProcesses::iterator itProcs = mData.mProcesses.begin();
    for (; itProcs != mData.mProcesses.end(); itProcs++)
    {
        ComObjPtr<GuestProcess> pCurProc = itProcs->second;
        AutoCaller procCaller(pCurProc);
        if (procCaller.rc())
            return VERR_COM_INVALID_OBJECT_STATE;

        ULONG uCurPID;
        HRESULT hr = pCurProc->COMGETTER(PID)(&uCurPID);
        ComAssertComRC(hr);

        if (uCurPID == uPID)
        {
            if (pProcess)
                *pProcess = pCurProc;
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

int GuestSession::startTaskAsync(const Utf8Str &strTaskDesc,
                                 GuestSessionTask *pTask, ComObjPtr<Progress> &pProgress)
{
    LogFlowThisFunc(("strTaskDesc=%s, pTask=%p\n", strTaskDesc.c_str(), pTask));

    AssertPtrReturn(pTask, VERR_INVALID_POINTER);

    /* Create the progress object. */
    HRESULT hr = pProgress.createObject();
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    hr = pProgress->init(static_cast<IGuestSession*>(this),
                         Bstr(strTaskDesc).raw(),
                         TRUE /* aCancelable */);
    if (FAILED(hr))
        return VERR_COM_UNEXPECTED;

    /* Initialize our worker task. */
    std::auto_ptr<GuestSessionTask> task(pTask);

    int rc = task->RunAsync(strTaskDesc, pProgress);
    if (RT_FAILURE(rc))
        return rc;

    /* Don't destruct on success. */
    task.release();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Queries/collects information prior to establishing a guest session.
 * This is necessary to know which guest control protocol version to use,
 * among other things (later).
 *
 * @return  IPRT status code.
 */
int GuestSession::queryInfo(void)
{
#if 1
    /* Since the new functions were not implemented yet, force Main to use protocol ver 1. */
    mData.mProtocolVersion = 1;
#else
    /*
     * Try querying the guest control protocol version running on the guest.
     * This is done using the Guest Additions version
     */
    ComObjPtr<Guest> pGuest = mData.mParent;
    Assert(!pGuest.isNull());

    uint32_t uVerAdditions = pGuest->getAdditionsVersion();
    mData.mProtocolVersion = (   VBOX_FULL_VERSION_GET_MAJOR(uVerAdditions) >= 4
                              && VBOX_FULL_VERSION_GET_MINOR(uVerAdditions) >= 2) /** @todo What's about v5.0 ? */
                           ? 2  /* Guest control 2.0. */
                           : 1; /* Legacy guest control (VBox < 4.2). */
    /* Build revision is ignored. */

    /* Tell the user but don't bitch too often. */
    static short s_gctrlLegacyWarning = 0;
    if (s_gctrlLegacyWarning++ < 3) /** @todo Find a bit nicer text. */
        LogRel((tr("Warning: Guest Additions are older (%ld.%ld) than host capabilities for guest control, please upgrade them. Using protocol version %ld now\n"),
                VBOX_FULL_VERSION_GET_MAJOR(uVerAdditions), VBOX_FULL_VERSION_GET_MINOR(uVerAdditions), mData.mProtocolVersion));
#endif
    return VINF_SUCCESS;
}

// implementation of public methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestSession::Close(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Remove ourselves from the session list. */
    mData.mParent->sessionRemove(this);

    /*
     * Release autocaller before calling uninit.
     */
    autoCaller.release();

    uninit();

    LogFlowFuncLeave();
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::CopyFrom(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgOutPointerValid(aProgress);

    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aSource) == NULL || *(aSource) == '\0'))
        return setError(E_INVALIDARG, tr("No source specified"));
    if (RT_UNLIKELY((aDest) == NULL || *(aDest) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = CopyFileFlag_None;
    if (aFlags)
    {
        com::SafeArray<CopyFileFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    try
    {
        ComObjPtr<Progress> pProgress;
        SessionTaskCopyFrom *pTask = new SessionTaskCopyFrom(this /* GuestSession */,
                                                             Utf8Str(aSource), Utf8Str(aDest), fFlags);
        int rc = startTaskAsync(Utf8StrFmt(tr("Copying \"%ls\" from guest to \"%ls\" on the host"), aSource, aDest),
                                pTask, pProgress);
        if (RT_SUCCESS(rc))
        {
            /* Return progress to the caller. */
            hr = pProgress.queryInterfaceTo(aProgress);
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR,
                          tr("Starting task for copying file \"%ls\" from guest to \"%ls\" on the host failed: %Rrc"), rc);
    }
    catch(std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::CopyTo(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    CheckComArgStrNotEmptyOrNull(aSource);
    CheckComArgStrNotEmptyOrNull(aDest);
    CheckComArgOutPointerValid(aProgress);

    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aSource) == NULL || *(aSource) == '\0'))
        return setError(E_INVALIDARG, tr("No source specified"));
    if (RT_UNLIKELY((aDest) == NULL || *(aDest) == '\0'))
        return setError(E_INVALIDARG, tr("No destination specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = CopyFileFlag_None;
    if (aFlags)
    {
        com::SafeArray<CopyFileFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    try
    {
        ComObjPtr<Progress> pProgress;
        SessionTaskCopyTo *pTask = new SessionTaskCopyTo(this /* GuestSession */,
                                                         Utf8Str(aSource), Utf8Str(aDest), fFlags);
        AssertPtrReturn(pTask, VERR_NO_MEMORY);
        int rc = startTaskAsync(Utf8StrFmt(tr("Copying \"%ls\" from host to \"%ls\" on the guest"), aSource, aDest),
                                pTask, pProgress);
        if (RT_SUCCESS(rc))
        {
            /* Return progress to the caller. */
            hr = pProgress.queryInterfaceTo(aProgress);
        }
        else
            hr = setError(VBOX_E_IPRT_ERROR,
                          tr("Starting task for copying file \"%ls\" from host to \"%ls\" on the guest failed: %Rrc"), rc);
    }
    catch(std::bad_alloc &)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryCreate(IN_BSTR aPath, ULONG aMode,
                                           ComSafeArrayIn(DirectoryCreateFlag_T, aFlags))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to create specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryCreateFlag_None;
    if (aFlags)
    {
        com::SafeArray<DirectoryCreateFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];

        if (fFlags)
        {
            if (!(fFlags & DirectoryCreateFlag_Parents))
                return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), fFlags);
        }
    }

    HRESULT hr = S_OK;

    ComObjPtr <GuestDirectory> pDirectory; int guestRc;
    int rc = directoryCreateInternal(Utf8Str(aPath), (uint32_t)aMode, fFlags, &guestRc);
    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            case VERR_INVALID_PARAMETER:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Invalid parameters given"));
               break;

            case VERR_BROKEN_PIPE:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Unexpectedly aborted"));
               break;

            case VERR_CANT_CREATE:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: Could not create directory"));
               break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Directory creation failed: %Rrc"), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryCreateTemp(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aPath, BOOL aSecure, BSTR *aDirectory)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aTemplate) == NULL || *(aTemplate) == '\0'))
        return setError(E_INVALIDARG, tr("No template specified"));
    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory name specified"));
    CheckComArgOutPointerValid(aDirectory);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    Utf8Str strName; int guestRc;
    int rc = objectCreateTempInternal(Utf8Str(aTemplate),
                                      Utf8Str(aPath),
                                      true /* Directory */, strName, &guestRc);
    if (RT_SUCCESS(rc))
    {
        strName.cloneTo(aDirectory);
    }
    else
    {
        switch (rc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Temporary directory creation \"%s\" with template \"%s\" failed: %Rrc"),
                             Utf8Str(aPath).c_str(), Utf8Str(aTemplate).c_str(), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryExists(IN_BSTR aPath, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to check existence for specified"));
    CheckComArgOutPointerValid(aExists);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestFsObjData objData; int guestRc;
    int rc = directoryQueryInfoInternal(Utf8Str(aPath), objData, &guestRc);
    if (RT_SUCCESS(rc))
    {
        *aExists = objData.mType == FsObjType_Directory;
    }
    else
    {
        switch (rc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying directory existence \"%s\" failed: %Rrc"),
                             Utf8Str(aPath).c_str(), rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryOpen(IN_BSTR aPath, IN_BSTR aFilter, ComSafeArrayIn(DirectoryOpenFlag_T, aFlags), IGuestDirectory **aDirectory)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to open specified"));
    if (RT_UNLIKELY((aFilter) != NULL && *(aFilter) != '\0'))
        return setError(E_INVALIDARG, tr("Directory filters are not implemented yet"));
    CheckComArgOutPointerValid(aDirectory);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    uint32_t fFlags = DirectoryOpenFlag_None;
    if (aFlags)
    {
        com::SafeArray<DirectoryOpenFlag_T> flags(ComSafeArrayInArg(aFlags));
        for (size_t i = 0; i < flags.size(); i++)
            fFlags |= flags[i];

        if (fFlags)
            return setError(E_INVALIDARG, tr("Open flags (%#x) not implemented yet"), fFlags);
    }

    HRESULT hr = S_OK;

    ComObjPtr <GuestDirectory> pDirectory;
    int rc = directoryOpenInternal(Utf8Str(aPath), Utf8Str(aFilter), fFlags, pDirectory);
    if (RT_SUCCESS(rc))
    {
        /* Return directory object to the caller. */
        hr = pDirectory.queryInterfaceTo(aDirectory);
    }
    else
    {
        switch (rc)
        {
            case VERR_INVALID_PARAMETER:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Opening directory \"%s\" failed; invalid parameters given",
                                                   Utf8Str(aPath).c_str()));
               break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Opening directory \"%s\" failed: %Rrc"),
                             Utf8Str(aPath).c_str(),rc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryQueryInfo(IN_BSTR aPath, IGuestFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No directory to query information for specified"));
    CheckComArgOutPointerValid(aInfo);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestFsObjData objData; int guestRc;
    int vrc = directoryQueryInfoInternal(Utf8Str(aPath), objData, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        if (objData.mType == FsObjType_Directory)
        {
            ComObjPtr<GuestFsObjInfo> pFsObjInfo;
            hr = pFsObjInfo.createObject();
            if (FAILED(hr)) return hr;

            vrc = pFsObjInfo->init(objData);
            if (RT_SUCCESS(vrc))
            {
                hr = pFsObjInfo.queryInterfaceTo(aInfo);
                if (FAILED(hr)) return hr;
            }
        }
    }

    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            case VERR_NOT_A_DIRECTORY:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Element \"%s\" exists but is not a directory",
                                                    Utf8Str(aPath).c_str()));
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Querying directory information for \"%s\" failed: %Rrc"),
                              Utf8Str(aPath).c_str(), vrc);
                break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRemove(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRemoveRecursive(IN_BSTR aPath, ComSafeArrayIn(DirectoryRemoveRecFlag_T, aFlags), IProgress **aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectoryRename(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::DirectorySetACL(IN_BSTR aPath, IN_BSTR aACL)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentClear(void)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mEnvironment.Clear();

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentGet(IN_BSTR aName, BSTR *aValue)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aName) == NULL || *(aName) == '\0'))
        return setError(E_INVALIDARG, tr("No value name specified"));

    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Bstr strValue(mData.mEnvironment.Get(Utf8Str(aName)));
    strValue.cloneTo(aValue);

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentSet(IN_BSTR aName, IN_BSTR aValue)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aName) == NULL || *(aName) == '\0'))
        return setError(E_INVALIDARG, tr("No value name specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = mData.mEnvironment.Set(Utf8Str(aName), Utf8Str(aValue));

    HRESULT hr = RT_SUCCESS(rc) ? S_OK : VBOX_E_IPRT_ERROR;
    LogFlowFuncLeaveRC(hr);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::EnvironmentUnset(IN_BSTR aName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mEnvironment.Unset(Utf8Str(aName));

    LogFlowFuncLeaveRC(S_OK);
    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileCreateTemp(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aPath, BOOL aSecure, IGuestFile **aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileExists(IN_BSTR aPath, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to check existence for specified"));
    CheckComArgOutPointerValid(aExists);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestFsObjData objData; int guestRc;
    int vrc = fileQueryInfoInternal(Utf8Str(aPath), objData, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        *aExists = TRUE;
        return S_OK;
    }

    HRESULT hr = S_OK;

    switch (vrc)
    {
        case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
            hr = GuestProcess::setErrorExternal(this, guestRc);
            break;

        case VERR_NOT_A_FILE:
            *aExists = FALSE;
            break;

        default:
            hr = setError(VBOX_E_IPRT_ERROR, tr("Querying file information for \"%s\" failed: %Rrc"),
                          Utf8Str(aPath).c_str(), vrc);
            break;
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileRemove(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to remove specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    int guestRc;
    int vrc = fileRemoveInternal(Utf8Str(aPath), &guestRc);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Removing file \"%s\" failed: %Rrc"),
                              Utf8Str(aPath).c_str(), vrc);
                break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileOpen(IN_BSTR aPath, IN_BSTR aOpenMode, IN_BSTR aDisposition, ULONG aCreationMode, LONG64 aOffset, IGuestFile **aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to open specified"));
    if (RT_UNLIKELY((aOpenMode) == NULL || *(aOpenMode) == '\0'))
        return setError(E_INVALIDARG, tr("No open mode specified"));
    if (RT_UNLIKELY((aDisposition) == NULL || *(aDisposition) == '\0'))
        return setError(E_INVALIDARG, tr("No disposition mode specified"));

    CheckComArgOutPointerValid(aFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /** @todo Validate open mode. */
    /** @todo Validate disposition mode. */

    /** @todo Validate creation mode. */
    uint32_t uCreationMode = 0;

    HRESULT hr = S_OK;

    ComObjPtr <GuestFile> pFile; int guestRc;
    int vrc = fileOpenInternal(Utf8Str(aPath), Utf8Str(aOpenMode), Utf8Str(aDisposition),
                               aCreationMode, aOffset, pFile, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        /* Return directory object to the caller. */
        hr = pFile.queryInterfaceTo(aFile);
    }
    else
    {
        switch (vrc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Opening file \"%s\" failed: %Rrc"),
                              Utf8Str(aPath).c_str(), vrc);
                break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileQueryInfo(IN_BSTR aPath, IGuestFsObjInfo **aInfo)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to query information for specified"));
    CheckComArgOutPointerValid(aInfo);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    GuestFsObjData objData; int guestRc;
    int vrc = fileQueryInfoInternal(Utf8Str(aPath), objData, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        ComObjPtr<GuestFsObjInfo> pFsObjInfo;
        hr = pFsObjInfo.createObject();
        if (FAILED(hr)) return hr;

        vrc = pFsObjInfo->init(objData);
        if (RT_SUCCESS(vrc))
        {
            hr = pFsObjInfo.queryInterfaceTo(aInfo);
            if (FAILED(hr)) return hr;
        }
    }

    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            case VERR_NOT_A_FILE:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Element exists but is not a file"));
                break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying file information failed: %Rrc"), vrc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileQuerySize(IN_BSTR aPath, LONG64 *aSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aPath) == NULL || *(aPath) == '\0'))
        return setError(E_INVALIDARG, tr("No file to query size for specified"));
    CheckComArgOutPointerValid(aSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hr = S_OK;

    int64_t llSize; int guestRc;
    int vrc = fileQuerySizeInternal(Utf8Str(aPath), &llSize, &guestRc);
    if (RT_SUCCESS(vrc))
    {
        *aSize = llSize;
    }
    else
    {
        switch (vrc)
        {
            case VERR_GENERAL_FAILURE: /** @todo Special guest control rc needed! */
                hr = GuestProcess::setErrorExternal(this, guestRc);
                break;

            default:
               hr = setError(VBOX_E_IPRT_ERROR, tr("Querying file size failed: %Rrc"), vrc);
               break;
        }
    }

    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileRename(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags))
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::FileSetACL(IN_BSTR aPath, IN_BSTR aACL)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessCreate(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                         ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS, IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    com::SafeArray<LONG> affinity;

    return ProcessCreateEx(aCommand, ComSafeArrayInArg(aArguments), ComSafeArrayInArg(aEnvironment),
                           ComSafeArrayInArg(aFlags), aTimeoutMS, ProcessPriority_Default, ComSafeArrayAsInParam(affinity), aProcess);
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessCreateEx(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                                           ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS,
                                           ProcessPriority_T aPriority, ComSafeArrayIn(LONG, aAffinity),
                                           IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    if (RT_UNLIKELY((aCommand) == NULL || *(aCommand) == '\0'))
        return setError(E_INVALIDARG, tr("No command to execute specified"));
    CheckComArgOutPointerValid(aProcess);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    GuestProcessStartupInfo procInfo;
    procInfo.mCommand = Utf8Str(aCommand);

    if (aArguments)
    {
        com::SafeArray<IN_BSTR> arguments(ComSafeArrayInArg(aArguments));
        for (size_t i = 0; i < arguments.size(); i++)
            procInfo.mArguments.push_back(Utf8Str(arguments[i]));
    }

    int rc = VINF_SUCCESS;

    /*
     * Create the process environment:
     * - Apply the session environment in a first step, and
     * - Apply environment variables specified by this call to
     *   have the chance of overwriting/deleting session entries.
     */
    procInfo.mEnvironment = mData.mEnvironment; /* Apply original session environment. */

    if (aEnvironment)
    {
        com::SafeArray<IN_BSTR> environment(ComSafeArrayInArg(aEnvironment));
        for (size_t i = 0; i < environment.size() && RT_SUCCESS(rc); i++)
            rc = procInfo.mEnvironment.Set(Utf8Str(environment[i]));
    }

    HRESULT hr = S_OK;

    if (RT_SUCCESS(rc))
    {
        if (aFlags)
        {
            com::SafeArray<ProcessCreateFlag_T> flags(ComSafeArrayInArg(aFlags));
            for (size_t i = 0; i < flags.size(); i++)
                procInfo.mFlags |= flags[i];
        }

        procInfo.mTimeoutMS = aTimeoutMS;

        if (aAffinity)
        {
            com::SafeArray<LONG> affinity(ComSafeArrayInArg(aAffinity));
            for (size_t i = 0; i < affinity.size(); i++)
                procInfo.mAffinity[i] = affinity[i]; /** @todo Really necessary? Later. */
        }

        procInfo.mPriority = aPriority;

        ComObjPtr<GuestProcess> pProcess;
        rc = processCreateExInteral(procInfo, pProcess);
        if (RT_SUCCESS(rc))
        {
            /* Return guest session to the caller. */
            HRESULT hr2 = pProcess.queryInterfaceTo(aProcess);
            if (FAILED(hr2))
                rc = VERR_COM_OBJECT_NOT_FOUND;

            if (RT_SUCCESS(rc))
                rc = pProcess->startProcessAsync();
        }
    }

    if (RT_FAILURE(rc))
    {
        switch (rc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Maximum number of guest objects per session (%ld) reached"),
                                                    VBOX_GUESTCTRL_MAX_OBJECTS);
                break;

            /** @todo Add more errors here. */

            default:
                hr = setError(VBOX_E_IPRT_ERROR, tr("Could not create guest process, rc=%Rrc"), rc);
                break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::ProcessGet(ULONG aPID, IGuestProcess **aProcess)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFunc(("aPID=%RU32\n", aPID));

    CheckComArgOutPointerValid(aProcess);
    if (aPID == 0)
        return setError(E_INVALIDARG, tr("No valid process ID (PID) specified"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hr = S_OK;

    ComObjPtr<GuestProcess> pProcess;
    int rc = processGetByPID(aPID, &pProcess);
    if (RT_FAILURE(rc))
        hr = setError(E_INVALIDARG, tr("No process with PID %RU32 found"), aPID);

    /* This will set (*aProcess) to NULL if pProgress is NULL. */
    HRESULT hr2 = pProcess.queryInterfaceTo(aProcess);
    if (SUCCEEDED(hr))
        hr = hr2;

    LogFlowThisFunc(("aProcess=%p, hr=%Rhrc\n", *aProcess, hr));
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkCreate(IN_BSTR aSource, IN_BSTR aTarget, SymlinkType_T aType)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkExists(IN_BSTR aSymlink, BOOL *aExists)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRead(IN_BSTR aSymlink, ComSafeArrayIn(SymlinkReadFlag_T, aFlags), BSTR *aTarget)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRemoveDirectory(IN_BSTR aPath)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

STDMETHODIMP GuestSession::SymlinkRemoveFile(IN_BSTR aFile)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_CONTROL */
}

