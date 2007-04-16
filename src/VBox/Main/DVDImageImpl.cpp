/** @file
 *
 * VirtualBox COM class implementation
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

#include "DVDImageImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/file.h>
#include <iprt/path.h>
#include <VBox/err.h>
#include <VBox/param.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT DVDImage::FinalConstruct()
{
    mAccessible = FALSE;
    return S_OK;
}

void DVDImage::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the DVD image object.
 *
 *  @param parent
 *      parent object
 *  @param filePath
 *      local file system path to the image file
 *      (can be relative to the VirtualBox config dir)
 *  @param isRegistered
 *      whether this object is being initialized by the VirtualBox init code
 *      because it is present in the registry
 *  @param id
 *      ID of the DVD image to assign
 *
 *  @return          COM result indicator
 */
HRESULT DVDImage::init (VirtualBox *parent, const BSTR filePath,
                        BOOL isRegistered, const Guid &id)
{
    LogFlowMember (("DVDImage::init(): filePath={%ls}, id={%s}\n",
                    filePath, id.toString().raw()));

    ComAssertRet (parent && filePath && !!id, E_INVALIDARG);

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    HRESULT rc = S_OK;

    mParent = parent;

    unconst (mImageFile) = filePath;
    unconst (mUuid) = id;

    /* get the full file name */
    char filePathFull [RTPATH_MAX];
    int vrc = RTPathAbsEx (mParent->homeDir(), Utf8Str (filePath),
                           filePathFull, sizeof (filePathFull));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("Invalid image file path: '%ls' (%Vrc)"),
                                 filePath, vrc);

    unconst (mImageFileFull) = filePathFull;
    LogFlowMember (("              filePathFull={%ls}\n", mImageFileFull.raw()));

    if (!isRegistered)
    {
        /* check whether the given file exists or not */
        RTFILE file;
        vrc = RTFileOpen (&file, filePathFull,
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (VBOX_FAILURE (vrc))
        {
            /* here we come when the image was just opened by
             * IVirtualBox::OpenDVDImage(). fail in this case */
            rc = setError (E_FAIL,
                tr ("Could not open the CD/DVD image '%ls' (%Vrc)"),
                mImageFileFull.raw(), vrc);
        }
        else
            RTFileClose (file);
    }

    if (SUCCEEDED (rc))
    {
        mParent->addDependentChild (this);
    }

    setReady (SUCCEEDED (rc));

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void DVDImage::uninit()
{
    LogFlowMember (("DVDImage::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("DVDImage::uninit(): isReady=%d\n", isReady()));

    if (!isReady())
        return;

    setReady (false);

    alock.leave();
    mParent->removeDependentChild (this);
    alock.enter();

    mParent.setNull();
}

// IDVDImage properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DVDImage::COMGETTER(Id) (GUIDPARAMOUT id)
{
    if (!id)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mUuid.cloneTo (id);
    return S_OK;
}

STDMETHODIMP DVDImage::COMGETTER(FilePath) (BSTR *filePath)
{
    if (!filePath)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mImageFileFull.cloneTo (filePath);
    return S_OK;
}

STDMETHODIMP DVDImage::COMGETTER(Accessible) (BOOL *accessible)
{
    if (!accessible)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    HRESULT rc = S_OK;

    /* check whether the given image file exists or not */
    RTFILE file;
    int vrc = RTFileOpen (&file, Utf8Str (mImageFileFull),
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (VBOX_FAILURE (vrc))
    {
        Log (("DVDImage::COMGETTER(Accessible): WARNING: '%ls' "
              "is not accessible (%Vrc)\n", mImageFileFull.raw(), vrc));
        mAccessible = FALSE;
    }
    else
    {
        mAccessible = TRUE;
        RTFileClose (file);
    }

    *accessible = mAccessible;

    return rc;
}

STDMETHODIMP DVDImage::COMGETTER(Size) (ULONG64 *size)
{
    if (!size)
        return E_POINTER;

    HRESULT rc = S_OK;

    AutoLock alock (this);
    CHECK_READY();

    RTFILE file;
    int vrc = RTFileOpen (&file, Utf8Str (mImageFileFull),
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);

    if (VBOX_FAILURE (vrc))
        rc = setError (E_FAIL, tr("Failed to open ISO image '%ls' (%Vrc)\n"),
                       mImageFileFull.raw(), vrc);
    else
    {
        AssertCompile (sizeof (uint64_t) == sizeof (ULONG64));

        uint64_t u64Size = 0;

        vrc = RTFileGetSize (file, &u64Size);

        if (VBOX_SUCCESS (vrc))
            *size = u64Size;
        else
            rc = setError (E_FAIL,
                tr ("Failed to determine size of ISO image '%ls' (%Vrc)\n"),
                    mImageFileFull.raw(), vrc);

        RTFileClose (file);
    }

    return rc;
}

// public methods for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Changes the stored path values of this image to reflect the new location.
 *  Intended to be called only by VirtualBox::updateSettings() if a machine's
 *  name change causes directory renaming that affects this image.
 *
 *  @param aNewFullPath new full path to this image file
 *  @param aNewPath     new path to this image file relative to the VirtualBox
 *                      settings directory (when possible)
 *
 *  @note Locks this object for writing.
 */
void DVDImage::updatePath (const char *aNewFullPath, const char *aNewPath)
{
    AssertReturnVoid (aNewFullPath);
    AssertReturnVoid (aNewPath);

    AutoLock alock (this);
    AssertReturnVoid (isReady());

    unconst (mImageFileFull) = aNewFullPath;
    unconst (mImageFile) = aNewPath;
}

