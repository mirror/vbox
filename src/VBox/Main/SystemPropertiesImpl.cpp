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

#include "SystemPropertiesImpl.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

// generated header
#include "SchemaDefs.h"

#include <iprt/path.h>
#include <iprt/dir.h>
#include <VBox/param.h>
#include <VBox/err.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::FinalConstruct()
{
    return S_OK;
}

void SystemProperties::FinalRelease()
{
    if (isReady())
        uninit ();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the system information object.
 *
 * @returns COM result indicator
 */
HRESULT SystemProperties::init (VirtualBox *aParent)
{
    LogFlowMember (("SystemProperties::init()\n"));

    ComAssertRet (aParent, E_FAIL);

    AutoWriteLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = aParent;

    setDefaultVDIFolder (NULL);
    setDefaultMachineFolder (NULL);
    setRemoteDisplayAuthLibrary (NULL);

    mHWVirtExEnabled = false;
    mLogHistoryCount = 3;

    setReady(true);
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SystemProperties::uninit()
{
    LogFlowMember (("SystemProperties::uninit()\n"));

    AutoWriteLock alock (this);
    AssertReturn (isReady(), (void) 0);

    setReady (false);
}

// ISystemProperties properties
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP SystemProperties::COMGETTER(MinGuestRAM)(ULONG *minRAM)
{
    if (!minRAM)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *minRAM = SchemaDefs::MinGuestRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestRAM)(ULONG *maxRAM)
{
    if (!maxRAM)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *maxRAM = SchemaDefs::MaxGuestRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestVRAM)(ULONG *minVRAM)
{
    if (!minVRAM)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *minVRAM = SchemaDefs::MinGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestVRAM)(ULONG *maxVRAM)
{
    if (!maxVRAM)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *maxVRAM = SchemaDefs::MaxGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestMonitors)(ULONG *maxMonitors)
{
    if (!maxMonitors)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *maxMonitors = SchemaDefs::MaxGuestMonitors;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxVDISize)(ULONG64 *maxVDISize)
{
    if (!maxVDISize)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    /** The BIOS supports currently 32 bit LBA numbers (implementing the full
     * 48 bit range is in theory trivial, but the crappy compiler makes things
     * more difficult). This translates to almost 2 TBytes (to be on the safe
     * side, the reported limit is 1 MiByte less than that, as the total number
     * of sectors should fit in 32 bits, too), which should bei enough for
     * the moment. The virtual ATA disks support complete LBA48 (although for
     * example iSCSI is also currently limited to 32 bit LBA), so the
     * theoretical maximum disk size is 128 PiByte. The user interface cannot
     * cope with this in a reasonable way yet. */
    *maxVDISize = 2048 * 1024 - 1;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(NetworkAdapterCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *count = SchemaDefs::NetworkAdapterCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(SerialPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *count = SchemaDefs::SerialPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(ParallelPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *count = SchemaDefs::ParallelPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxBootPosition)(ULONG *aMaxBootPosition)
{
    if (!aMaxBootPosition)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    *aMaxBootPosition = SchemaDefs::MaxBootPosition;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultVDIFolder) (BSTR *aDefaultVDIFolder)
{
    if (!aDefaultVDIFolder)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mDefaultVDIFolderFull.cloneTo (aDefaultVDIFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultVDIFolder) (INPTR BSTR aDefaultVDIFolder)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = setDefaultVDIFolder (aDefaultVDIFolder);
    if (FAILED (rc))
        return rc;

    alock.unlock();
    return mParent->saveSettings();
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultMachineFolder) (BSTR *aDefaultMachineFolder)
{
    if (!aDefaultMachineFolder)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mDefaultMachineFolderFull.cloneTo (aDefaultMachineFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultMachineFolder) (INPTR BSTR aDefaultMachineFolder)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = setDefaultMachineFolder (aDefaultMachineFolder);
    if (FAILED (rc))
        return rc;

    alock.unlock();
    return mParent->saveSettings();
}

STDMETHODIMP SystemProperties::COMGETTER(RemoteDisplayAuthLibrary) (BSTR *aRemoteDisplayAuthLibrary)
{
    if (!aRemoteDisplayAuthLibrary)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mRemoteDisplayAuthLibrary.cloneTo (aRemoteDisplayAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(RemoteDisplayAuthLibrary) (INPTR BSTR aRemoteDisplayAuthLibrary)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = setRemoteDisplayAuthLibrary (aRemoteDisplayAuthLibrary);
    if (FAILED (rc))
        return rc;

    alock.unlock();
    return mParent->saveSettings();
}

STDMETHODIMP SystemProperties::COMGETTER(WebServiceAuthLibrary) (BSTR *aWebServiceAuthLibrary)
{
    if (!aWebServiceAuthLibrary)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    mWebServiceAuthLibrary.cloneTo (aWebServiceAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(WebServiceAuthLibrary) (INPTR BSTR aWebServiceAuthLibrary)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = setWebServiceAuthLibrary (aWebServiceAuthLibrary);
    if (FAILED (rc))
        return rc;

    alock.unlock();
    return mParent->saveSettings();
}

STDMETHODIMP SystemProperties::COMGETTER(HWVirtExEnabled) (BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    *enabled = mHWVirtExEnabled;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(HWVirtExEnabled) (BOOL enabled)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    mHWVirtExEnabled = enabled;

    alock.unlock();
    return mParent->saveSettings();
}

STDMETHODIMP SystemProperties::COMGETTER(LogHistoryCount) (ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    *count = mLogHistoryCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(LogHistoryCount) (ULONG count)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    mLogHistoryCount = count;

    alock.unlock();
    return mParent->saveSettings();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::loadSettings (const settings::Key &aGlobal)
{
    using namespace settings;

    AutoWriteLock alock (this);
    CHECK_READY();

    AssertReturn (!aGlobal.isNull(), E_FAIL);

    HRESULT rc = S_OK;

    Key properties = aGlobal.key ("SystemProperties");

    Bstr bstr;

    bstr = properties.stringValue ("defaultVDIFolder");
    rc = setDefaultVDIFolder (bstr);
    CheckComRCReturnRC (rc);

    bstr = properties.stringValue ("defaultMachineFolder");
    rc = setDefaultMachineFolder (bstr);
    CheckComRCReturnRC (rc);

    bstr = properties.stringValue ("remoteDisplayAuthLibrary");
    rc = setRemoteDisplayAuthLibrary (bstr);
    CheckComRCReturnRC (rc);

    bstr = properties.stringValue ("webServiceAuthLibrary");
    rc = setWebServiceAuthLibrary (bstr);
    CheckComRCReturnRC (rc);

    /* Note: not <BOOL> because Win32 defines BOOL as int */
    mHWVirtExEnabled = properties.valueOr <bool> ("HWVirtExEnabled", false);

    mLogHistoryCount = properties.valueOr <ULONG> ("LogHistoryCount", 3);

    return S_OK;
}

HRESULT SystemProperties::saveSettings (settings::Key &aGlobal)
{
    using namespace settings;

    AutoWriteLock alock (this);
    CHECK_READY();

    ComAssertRet (!aGlobal.isNull(), E_FAIL);

    /* first, delete the entry */
    Key properties = aGlobal.findKey ("SystemProperties");
    if (!properties.isNull())
        properties.zap();
    /* then, recreate it */
    properties = aGlobal.createKey ("SystemProperties");

    if (mDefaultVDIFolder)
        properties.setValue <Bstr> ("defaultVDIFolder", mDefaultVDIFolder);

    if (mDefaultMachineFolder)
        properties.setValue <Bstr> ("defaultMachineFolder", mDefaultMachineFolder);

    if (mRemoteDisplayAuthLibrary)
        properties.setValue <Bstr> ("remoteDisplayAuthLibrary", mRemoteDisplayAuthLibrary);

    if (mWebServiceAuthLibrary)
        properties.setValue <Bstr> ("webServiceAuthLibrary", mWebServiceAuthLibrary);

    properties.setValue <bool> ("HWVirtExEnabled", !!mHWVirtExEnabled);

    properties.setValue <ULONG> ("LogHistoryCount", mLogHistoryCount);

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::setDefaultVDIFolder (const BSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "VDI";

    // get the full file name
    char folder [RTPATH_MAX];
    int vrc = RTPathAbsEx (mParent->homeDir(), path, folder, sizeof (folder));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Cannot set the default VDI folder to '%ls' (%Vrc)"),
            path.raw(), vrc);

    mDefaultVDIFolder = path;
    mDefaultVDIFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setDefaultMachineFolder (const BSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "Machines";

    // get the full file name
    char folder [RTPATH_MAX];
    int vrc = RTPathAbsEx (mParent->homeDir(), path, folder, sizeof (folder));
    if (VBOX_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Cannot set the default machines folder to '%ls' (%Vrc)"),
            path.raw(), vrc);

    mDefaultMachineFolder = path;
    mDefaultMachineFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setRemoteDisplayAuthLibrary (const BSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "VRDPAuth";

    mRemoteDisplayAuthLibrary = path;

    return S_OK;
}

HRESULT SystemProperties::setWebServiceAuthLibrary (const BSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "VRDPAuth";

    mWebServiceAuthLibrary = path;

    return S_OK;
}
