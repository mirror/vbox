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

#include "SharedFolderImpl.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "ConsoleImpl.h"

#include "Logging.h"

#include <iprt/param.h>
#include <iprt/path.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

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
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (Machine *aMachine,
                            const BSTR aName, const BSTR aHostPath)
{
    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mMachine = aMachine;
    return protectedInit (aMachine, aName, aHostPath);
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

    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mMachine = aMachine;
    return protectedInit (aMachine, aThat->mName, aThat->mHostPath);
}

/**
 *  Initializes the shared folder object.
 *
 *  @param aConsole     Console parent object
 *  @param aName        logical name of the shared folder
 *  @param aHostPath    full path to the shared folder on the host
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (Console *aConsole,
                            const BSTR aName, const BSTR aHostPath)
{
    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mConsole = aConsole;
    return protectedInit (aConsole, aName, aHostPath);
}

/**
 *  Initializes the shared folder object.
 *
 *  @param aVirtualBox  VirtualBox parent object
 *  @param aName        logical name of the shared folder
 *  @param aHostPath    full path to the shared folder on the host
 *
 *  @return          COM result indicator
 */
HRESULT SharedFolder::init (VirtualBox *aVirtualBox,
                            const BSTR aName, const BSTR aHostPath)
{
    AutoLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mVirtualBox = aVirtualBox;
    return protectedInit (aVirtualBox, aName, aHostPath);
}

/**
 *  Helper for init() methods.
 *
 *  @note
 *      Must be called from under the object's lock!
 */
HRESULT SharedFolder::protectedInit (VirtualBoxBaseWithChildren *aParent,
                                     const BSTR aName, const BSTR aHostPath)
{
    LogFlowThisFunc (("aName={%ls}, aHostPath={%ls}\n", aName, aHostPath));

    ComAssertRet (aParent && aName && aHostPath, E_INVALIDARG);

    Utf8Str hostPath = Utf8Str (aHostPath);
    size_t hostPathLen = hostPath.length();
    
    /* Remove the trailng slash unless it's a root directory
     * (otherwise the comparison with the RTPathAbs() result will fail at least
     * on Linux). Note that this isn't really necessary for the shared folder
     * itself, since adding a mapping eventually results into a
     * RTDirOpenFiltered() call (see HostServices/SharedFolders) that seems to
     * accept both the slashified paths and not. */
#if defined (__OS2__) || defined (__WIN__)
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

    mParent = aParent;
    unconst (mName) = aName;
    unconst (mHostPath) = hostPath;

    mParent->addDependentChild (this);

    setReady (true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SharedFolder::uninit()
{
    LogFlowMember (("SharedFolder::uninit()\n"));

    AutoLock alock (this);

    LogFlowMember (("SharedFolder::uninit(): isReady=%d\n", isReady()));
    if (!isReady())
        return;

    setReady (false);

    alock.leave();
    mParent->removeDependentChild (this);
}

// ISharedFolder properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SharedFolder::COMGETTER(Name) (BSTR *aName)
{
    if (!aName)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mName.cloneTo (aName);
    return S_OK;
}

STDMETHODIMP SharedFolder::COMGETTER(HostPath) (BSTR *aHostPath)
{
    if (!aHostPath)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    mHostPath.cloneTo (aHostPath);
    return S_OK;
}

STDMETHODIMP SharedFolder::COMGETTER(Accessible) (BOOL *aAccessible)
{
    if (!aAccessible)
        return E_POINTER;

    AutoLock alock (this);
    CHECK_READY();

    // check whether the host path exists
    Utf8Str hostPath = Utf8Str (mHostPath);
    char hostPathFull [RTPATH_MAX];
    int vrc = RTPathReal (hostPath, hostPathFull, sizeof (hostPathFull));
    if (VBOX_SUCCESS (vrc))
    {
        *aAccessible = TRUE;
        return S_OK;
    }

    HRESULT rc = S_OK;
    if (vrc != VERR_PATH_NOT_FOUND)
        rc = setError (E_FAIL,
            tr ("Invalid shared folder path: '%s' (%Vrc)"), hostPath.raw(), vrc);

    Log (("SharedFolder::COMGETTER(Accessible): WARNING: '%s' "
          "is not accessible (%Vrc)\n", hostPath.raw(), vrc));

    *aAccessible = FALSE;
    return S_OK;
}

