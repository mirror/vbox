/* $Id$ */
/** @file
 * VirtualBox Main - interface for guest directory entries, VBoxC.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include "GuestDirEntryImpl.h"
#include "GuestCtrlImplPrivate.h"
#include "Global.h"

#include "AutoCaller.h"
#include "Logging.h"


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(GuestDirEntry)

HRESULT GuestDirEntry::FinalConstruct()
{
    LogFlowThisFunc(("\n"));
    return BaseFinalConstruct();
}

void GuestDirEntry::FinalRelease()
{
    LogFlowThisFuncEnter();
    uninit();
    BaseFinalRelease();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

HRESULT GuestDirEntry::init(Guest *aParent, GuestProcessStreamBlock &streamBlock)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    /* Enclose the state transition NotReady->InInit->Ready. */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    mData.mNodeId = streamBlock.GetInt64("node_id");
    const char *pszName = streamBlock.GetString("name");
    if (pszName)
    {
        mData.mName =  BstrFmt("%s", pszName);
        mData.mType = GuestDirEntry::fileTypeToEntryType(streamBlock.GetString("ftype"));

        /* Confirm a successful initialization when it's the case. */
        autoInitSpan.setSucceeded();

        return S_OK;
    }

    return E_FAIL; /** @todo Find a better rc! */
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void GuestDirEntry::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady. */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

STDMETHODIMP GuestDirEntry::COMGETTER(NodeId)(LONG64 *aNodeId)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aNodeId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aNodeId = mData.mNodeId;

    return S_OK;
}

STDMETHODIMP GuestDirEntry::COMGETTER(Name)(BSTR *aName)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP GuestDirEntry::COMGETTER(Type)(GuestDirEntryType_T *aType)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aType = mData.mType;

    return S_OK;
}

GuestDirEntryType_T GuestDirEntry::fileTypeToEntryType(const char *pszFileType)
{
    GuestDirEntryType_T retType = GuestDirEntryType_Unknown;

    if (!pszFileType)
        return retType;

    if (!RTStrICmp(pszFileType, "-"))
        retType = GuestDirEntryType_File;
    else if (!RTStrICmp(pszFileType, "d"))
        retType = GuestDirEntryType_Directory;
    else if (!RTStrICmp(pszFileType, "l"))
        retType = GuestDirEntryType_Symlink;
    /** @todo Add more types here. */

    return retType;
}

