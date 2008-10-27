/* $Id $ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "HardDiskFormatImpl.h"
#include "Logging.h"

#include <VBox/VBoxHDD-new.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HardDiskFormat)

HRESULT HardDiskFormat::FinalConstruct()
{
    return S_OK;
}

void HardDiskFormat::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the hard disk format object.
 *
 * @param aVDInfo  Pointer to a backend info object.
 */
HRESULT HardDiskFormat::init (VDBACKENDINFO *aVDInfo)
{
    LogFlowThisFunc (("aVDInfo=%p\n", aVDInfo));

    ComAssertRet (aVDInfo, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    /* The ID of the backend */
    unconst (mData.id) = aVDInfo->pszBackend;
    /* The capabilities of the backend */
    unconst (mData.capabilities) = aVDInfo->uBackendCaps;
    /* Save the supported file extensions in a list */
    if (aVDInfo->papszFileExtensions)
    {
        const char *const *papsz = aVDInfo->papszFileExtensions;
        while (*papsz != NULL)
        {
            unconst (mData.fileExtensions).push_back (*papsz);
            ++ papsz;
        }
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HardDiskFormat::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mData.fileExtensions).clear();
    unconst (mData.capabilities) = 0;
    unconst (mData.id).setNull();
}

// IHardDiskFormat properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDiskFormat::COMGETTER(Id)(BSTR *aId)
{
    if (!aId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.id is const, no need to lock */

    mData.id.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP HardDiskFormat::
COMGETTER(FileExtensions)(ComSafeArrayOut (BSTR, aFileExtensions))
{
    if (ComSafeArrayOutIsNull (aFileExtensions))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.fileExtensions is const, no need to lock */

    com::SafeArray <BSTR> fileExtentions (mData.fileExtensions.size());
    int i = 0;
    for (BstrList::const_iterator it = mData.fileExtensions.begin();
        it != mData.fileExtensions.end(); ++ it, ++ i)
        (*it).cloneTo (&fileExtentions [i]);
    fileExtentions.detachTo (ComSafeArrayOutArg (aFileExtensions));

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportUuid)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_UUID;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportCreateFixed)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_CREATE_FIXED;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportCreateDynamic)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_CREATE_DYNAMIC;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportCreateSplit2G)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_CREATE_SPLIT_2G;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportDiff)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_DIFF;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportASync)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_ASYNC;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportFile)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_FILE;

    return S_OK;
}

STDMETHODIMP HardDiskFormat::COMGETTER(SupportConfig)(BOOL *aBool)
{
    if (!aBool)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mData.capabilities is const, no need to lock */

    *aBool = mData.capabilities & VD_CAP_CONFIG;

    return S_OK;
}

// IHardDiskFormat methods
/////////////////////////////////////////////////////////////////////////////

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

