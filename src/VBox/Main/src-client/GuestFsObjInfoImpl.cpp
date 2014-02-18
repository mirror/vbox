/* $Id$ */
/** @file
 * VirtualBox Main - Guest file system object information handling.
 */

/*
 * Copyright (C) 2012-2014 Oracle Corporation
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
#include "GuestFsObjInfoImpl.h"
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

DEFINE_EMPTY_CTOR_DTOR(GuestFsObjInfo)

HRESULT GuestFsObjInfo::FinalConstruct(void)
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestFsObjInfo::FinalRelease(void)
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

int GuestFsObjInfo::init(const GuestFsObjData &objData)
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    mData = objData;

    /* Confirm a successful initialization when it's the case. */
    autoInitSpan.setSucceeded();

    return VINF_SUCCESS;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestFsObjInfo::uninit(void)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// implementation of wrapped private getters/setters for attributes
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestFsObjInfo::getAccessTime(LONG64 *aAccessTime)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aAccessTime = mData.mAccessTime;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getAllocatedSize(LONG64 *aAllocatedSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aAllocatedSize = mData.mAllocatedSize;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getBirthTime(LONG64 *aBirthTime)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aBirthTime = mData.mBirthTime;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getChangeTime(LONG64 *aChangeTime)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aChangeTime = mData.mChangeTime;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}



HRESULT GuestFsObjInfo::getDeviceNumber(ULONG *aDeviceNumber)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aDeviceNumber = mData.mDeviceNumber;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getFileAttributes(com::Utf8Str &aFileAttributes)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    aFileAttributes = mData.mFileAttrs;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getGenerationId(ULONG *aGenerationId)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aGenerationId = mData.mGenerationID;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getGID(ULONG *aGID)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aGID = mData.mGID;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getGroupName(com::Utf8Str &aGroupName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    aGroupName = mData.mGroupName;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getHardLinks(ULONG *aHardLinks)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aHardLinks = mData.mNumHardLinks;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getModificationTime(LONG64 *aModificationTime)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aModificationTime = mData.mModificationTime;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getName(com::Utf8Str &aName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    aName = mData.mName;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getNodeId(LONG64 *aNodeId)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aNodeId = mData.mNodeID;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getNodeIdDevice(ULONG *aNodeIdDevice)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aNodeIdDevice = mData.mNodeIDDevice;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getObjectSize(LONG64 *aObjectSize)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aObjectSize = mData.mObjectSize;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getType(FsObjType_T *aType)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aType = mData.mType;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getUID(ULONG *aUID)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aUID = mData.mUID;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getUserFlags(ULONG *aUserFlags)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    *aUserFlags = mData.mUserFlags;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT GuestFsObjInfo::getUserName(com::Utf8Str &aUserName)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else

    aUserName = mData.mUserName;

    return S_OK;
#endif /* VBOX_WITH_GUEST_CONTROL */
}
