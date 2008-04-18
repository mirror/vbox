/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "FloppyImageImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/cpputils.h>
#include <VBox/err.h>
#include <VBox/param.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (FloppyImage)

HRESULT FloppyImage::FinalConstruct()
{
    mAccessible = FALSE;
    return S_OK;
}

void FloppyImage::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the floppy image object.
 *
 *  @param aParent
 *      parent object
 *  @param aFilePath
 *      local file system path to the image file
 *      (can be relative to the VirtualBox config dir)
 *  @param aRegistered
 *      whether this object is being initialized by the VirtualBox init code
 *      because it is present in the registry
 *  @param aId
 *      ID of the DVD image to assign
 *
 *  @return          COM result indicator
 */
HRESULT FloppyImage::init (VirtualBox *aParent, const BSTR aFilePath,
                           BOOL aRegistered, const Guid &aId)
{
    LogFlowThisFunc (("aFilePath={%ls}, aId={%s}\n",
                      aFilePath, aId.toString().raw()));

    ComAssertRet (aParent && aFilePath && !!aId, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    HRESULT rc = S_OK;

    /* share the parent weakly */
    unconst (mParent) = aParent;

    /* register with parent early, since uninit() will unconditionally
     * unregister on failure */
    mParent->addDependentChild (this);

    unconst (mImageFile) = aFilePath;
    unconst (mUuid) = aId;

    /* get the full file name */
    char filePathFull [RTPATH_MAX];
    int vrc = RTPathAbsEx (mParent->homeDir(), Utf8Str (aFilePath),
                           filePathFull, sizeof (filePathFull));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL, tr ("Invalid image file path: '%ls' (%Vrc)"),
                                 aFilePath, vrc);

    unconst (mImageFileFull) = filePathFull;
    LogFlowThisFunc (("...filePathFull={%ls}\n", mImageFileFull.raw()));

    if (!aRegistered)
    {
        /* check whether the given file exists or not */
        RTFILE file;
        vrc = RTFileOpen (&file, filePathFull,
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (VBOX_FAILURE (vrc))
        {
            /* here we come when the image was just opened by
             * IVirtualBox::OpenFloppyImage(). fail in this case */
            rc = setError (E_FAIL,
                tr ("Could not open the floppy image '%ls' (%Vrc)"),
                mImageFileFull.raw(), vrc);
        }
        else
            RTFileClose (file);
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void FloppyImage::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mParent->removeDependentChild (this);

    unconst (mParent).setNull();
}

// IFloppyImage properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyImage::COMGETTER(Id) (GUIDPARAMOUT aId)
{
    if (!aId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mUuid is constant during life time, no need to lock */
    mUuid.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP FloppyImage::COMGETTER(FilePath) (BSTR *aFilePath)
{
    if (!aFilePath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mImageFileFull.cloneTo (aFilePath);

    return S_OK;
}

STDMETHODIMP FloppyImage::COMGETTER(Accessible) (BOOL *aAccessible)
{
    if (!aAccessible)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    HRESULT rc = S_OK;

    /* check whether the given image file exists or not */
    RTFILE file;
    int vrc = RTFileOpen (&file, Utf8Str (mImageFileFull),
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (VBOX_FAILURE (vrc))
    {
        Log (("FloppyImage::COMGETTER(Accessible): WARNING: '%ls' "
              "is not accessible (%Vrc)\n", mImageFileFull.raw(), vrc));
        mAccessible = FALSE;
    }
    else
    {
        mAccessible = TRUE;
        RTFileClose (file);
    }

    *aAccessible = mAccessible;

    return rc;
}

STDMETHODIMP FloppyImage::COMGETTER(Size) (ULONG *aSize)
{
    if (!aSize)
        return E_POINTER;

    HRESULT rc = S_OK;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    RTFILE file;
    int vrc = RTFileOpen (&file, Utf8Str (mImageFileFull),
                          RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);

    if (VBOX_FAILURE (vrc))
        rc = setError (E_FAIL,
            tr ("Failed to open floppy image '%ls' (%Vrc)\n"),
                mImageFileFull.raw(), vrc);
    else
    {
        uint64_t u64Size = 0;

        vrc = RTFileGetSize (file, &u64Size);

        if (VBOX_SUCCESS (vrc))
            *aSize = (ULONG) u64Size;
        else
            rc = setError (E_FAIL,
                tr ("Failed to determine size of floppy image '%ls' (%Vrc)\n"),
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
void FloppyImage::updatePath (const char *aNewFullPath, const char *aNewPath)
{
    AssertReturnVoid (aNewFullPath);
    AssertReturnVoid (aNewPath);

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    unconst (mImageFileFull) = aNewFullPath;
    unconst (mImageFile) = aNewPath;
}

