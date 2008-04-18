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

#include "SharedFolderImpl.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "ConsoleImpl.h"

#include "Logging.h"

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

SharedFolder::SharedFolder()
    : mParent (NULL)
{
}

SharedFolder::~SharedFolder()
{
}

HRESULT SharedFolder::FinalConstruct()
{
    return S_OK;
}

void SharedFolder::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the shared folder object.
 *
 *  @param aMachine     parent Machine object
 *  @param aName        logical name of the shared folder
 *  @param aHostPath    full path to the shared folder on the host
 *  @param aWritable    writable if true, readonly otherwise
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (Machine *aMachine,
                            const BSTR aName, const BSTR aHostPath, BOOL aWritable)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mMachine) = aMachine;

    HRESULT rc = protectedInit (aMachine, aName, aHostPath, aWritable);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the shared folder object given another object
 *  (a kind of copy constructor).  This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @param aMachine     parent Machine object
 *  @param aThat        shared folder object to copy
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::initCopy (Machine *aMachine, SharedFolder *aThat)
{
    ComAssertRet (aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mMachine) = aMachine;

    HRESULT rc = protectedInit (aMachine, aThat->mData.mName,
                                aThat->mData.mHostPath, aThat->mData.mWritable);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the shared folder object.
 *
 *  @param aConsole     Console parent object
 *  @param aName        logical name of the shared folder
 *  @param aHostPath    full path to the shared folder on the host
 *  @param aWritable    writable if true, readonly otherwise
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (Console *aConsole,
                            const BSTR aName, const BSTR aHostPath, BOOL aWritable)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mConsole) = aConsole;

    HRESULT rc = protectedInit (aConsole, aName, aHostPath, aWritable);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Initializes the shared folder object.
 *
 *  @param aVirtualBox  VirtualBox parent object
 *  @param aName        logical name of the shared folder
 *  @param aHostPath    full path to the shared folder on the host
 *  @param aWritable    writable if true, readonly otherwise
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (VirtualBox *aVirtualBox,
                            const BSTR aName, const BSTR aHostPath, BOOL aWritable)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mVirtualBox) = aVirtualBox;

    HRESULT rc = protectedInit (aVirtualBox, aName, aHostPath, aWritable);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Helper for init() methods.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT SharedFolder::protectedInit (VirtualBoxBaseWithChildrenNEXT *aParent,
                                     const BSTR aName, const BSTR aHostPath, BOOL aWritable)
{
    LogFlowThisFunc (("aName={%ls}, aHostPath={%ls}, aWritable={%d}\n",
                      aName, aHostPath, aWritable));

    ComAssertRet (aParent && aName && aHostPath, E_INVALIDARG);

    Utf8Str hostPath = Utf8Str (aHostPath);
    size_t hostPathLen = hostPath.length();

    /* Remove the trailng slash unless it's a root directory
     * (otherwise the comparison with the RTPathAbs() result will fail at least
     * on Linux). Note that this isn't really necessary for the shared folder
     * itself, since adding a mapping eventually results into a
     * RTDirOpenFiltered() call (see HostServices/SharedFolders) that seems to
     * accept both the slashified paths and not. */
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
    if (hostPathLen > 2 &&
        RTPATH_IS_SEP (hostPath.raw()[hostPathLen - 1]) &&
        RTPATH_IS_VOLSEP (hostPath.raw()[hostPathLen - 2]))
        ;
#else
    if (hostPathLen == 1 && RTPATH_IS_SEP (hostPath[0]))
        ;
#endif
    else
        RTPathStripTrailingSlash (hostPath.mutableRaw());

    /* Check whether the path is full (absolute) */
    char hostPathFull [RTPATH_MAX];
    int vrc = RTPathAbsEx (NULL, hostPath,
                           hostPathFull, sizeof (hostPathFull));
    if (VBOX_FAILURE (vrc))
        return setError (E_INVALIDARG,
            tr ("Invalid shared folder path: '%s' (%Vrc)"), hostPath.raw(), vrc);

    if (RTPathCompare (hostPath, hostPathFull) != 0)
        return setError (E_INVALIDARG,
            tr ("Shared folder path '%s' is not absolute"), hostPath.raw());

    unconst (mParent) = aParent;

    /* register with parent */
    mParent->addDependentChild (this);

    unconst (mData.mName) = aName;
    unconst (mData.mHostPath) = hostPath;
    mData.mWritable = aWritable;

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SharedFolder::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    if (mParent)
        mParent->removeDependentChild (this);

    unconst (mParent) = NULL;

    unconst (mMachine).setNull();
    unconst (mConsole).setNull();
    unconst (mVirtualBox).setNull();
}

// ISharedFolder properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SharedFolder::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mName is constant during life time, no need to lock */
    mData.mName.cloneTo (aName);

    return S_OK;
}

STDMETHODIMP SharedFolder::COMGETTER(HostPath) (BSTR *aHostPath)
{
    if (!aHostPath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mHostPath is constant during life time, no need to lock */
    mData.mHostPath.cloneTo (aHostPath);

    return S_OK;
}

STDMETHODIMP SharedFolder::COMGETTER(Accessible) (BOOL *aAccessible)
{
    if (!aAccessible)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mName and mHostPath are constant during life time, no need to lock */

    /* check whether the host path exists */
    Utf8Str hostPath = Utf8Str (mData.mHostPath);
    char hostPathFull [RTPATH_MAX];
    int vrc = RTPathExists(hostPath) ? RTPathReal (hostPath, hostPathFull,
                                                   sizeof (hostPathFull))
                                     : VERR_PATH_NOT_FOUND;
    if (VBOX_SUCCESS (vrc))
    {
        *aAccessible = TRUE;
        return S_OK;
    }

    HRESULT rc = S_OK;
    if (vrc != VERR_PATH_NOT_FOUND)
        rc = setError (E_FAIL,
            tr ("Invalid shared folder path: '%s' (%Vrc)"), hostPath.raw(), vrc);

    LogWarningThisFunc (("'%s' is not accessible (%Vrc)\n", hostPath.raw(), vrc));

    *aAccessible = FALSE;
    return S_OK;
}

STDMETHODIMP SharedFolder::COMGETTER(Writable) (BOOL *aWritable)
{
    if (!aWritable)
        return E_POINTER;

    *aWritable = mData.mWritable;

    return S_OK;
}
