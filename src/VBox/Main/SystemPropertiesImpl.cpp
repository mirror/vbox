/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SystemProperties)

HRESULT SystemProperties::FinalConstruct()
{
    return S_OK;
}

void SystemProperties::FinalRelease()
{
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
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;

    setDefaultMachineFolder (NULL);
    setDefaultHardDiskFolder (NULL);
    setDefaultHardDiskFormat (NULL);

    setRemoteDisplayAuthLibrary (NULL);

    mHWVirtExEnabled = false;
    mLogHistoryCount = 3;

    HRESULT rc = S_OK;

    /* Fetch info of all available hd backends. */

    /// @todo NEWMEDIA VDBackendInfo needs to be improved to let us enumerate
    /// any number of backends

    /// @todo We currently leak memory because it's not actually clear what to
    /// free in structures returned by VDBackendInfo. Must be fixed ASAP!

    VDBACKENDINFO aVDInfo [100];
    unsigned cEntries;
    int vrc = VDBackendInfo (RT_ELEMENTS (aVDInfo), aVDInfo, &cEntries);
    AssertRC (vrc);
    if (RT_SUCCESS (vrc))
    {
        for (unsigned i = 0; i < cEntries; ++ i)
        {
            ComObjPtr <HardDiskFormat> hdf;
            rc = hdf.createObject();
            CheckComRCBreakRC (rc);

            rc = hdf->init (&aVDInfo [i]);
            CheckComRCBreakRC (rc);

            mHardDiskFormats.push_back (hdf);
        }
    }

    /* Confirm a successful initialization */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SystemProperties::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst (mParent).setNull();
}

// ISystemProperties properties
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP SystemProperties::COMGETTER(MinGuestRAM)(ULONG *minRAM)
{
    if (!minRAM)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MIN_IN_MB >= SchemaDefs::MinGuestRAM);
    *minRAM = MM_RAM_MIN_IN_MB;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestRAM)(ULONG *maxRAM)
{
    if (!maxRAM)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MAX_IN_MB <= SchemaDefs::MaxGuestRAM);
    *maxRAM = MM_RAM_MAX_IN_MB;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestVRAM)(ULONG *minVRAM)
{
    if (!minVRAM)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *minVRAM = SchemaDefs::MinGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestVRAM)(ULONG *maxVRAM)
{
    if (!maxVRAM)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *maxVRAM = SchemaDefs::MaxGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestCPUCount)(ULONG *minCPUCount)
{
    if (!minCPUCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *minCPUCount = SchemaDefs::MinCPUCount; // VMM_MIN_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestCPUCount)(ULONG *maxCPUCount)
{
    if (!maxCPUCount)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *maxCPUCount = SchemaDefs::MaxCPUCount; // VMM_MAX_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestMonitors)(ULONG *maxMonitors)
{
    if (!maxMonitors)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *maxMonitors = SchemaDefs::MaxGuestMonitors;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxVDISize)(ULONG64 *maxVDISize)
{
    if (!maxVDISize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /** The BIOS supports currently 32 bit LBA numbers (implementing the full
     * 48 bit range is in theory trivial, but the crappy compiler makes things
     * more difficult). This translates to almost 2 TBytes (to be on the safe
     * side, the reported limit is 1 MiByte less than that, as the total number
     * of sectors should fit in 32 bits, too), which should bei enough for
     * the moment. The virtual ATA disks support complete LBA48 (although for
     * example iSCSI is also currently limited to 32 bit LBA), so the
     * theoretical maximum disk size is 128 PiByte. The user interface cannot
     * cope with this in a reasonable way yet. */
    /* no need to lock, this is const */
    *maxVDISize = 2048 * 1024 - 1;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(NetworkAdapterCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::NetworkAdapterCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(SerialPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::SerialPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(ParallelPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::ParallelPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxBootPosition)(ULONG *aMaxBootPosition)
{
    CheckComArgOutPointerValid(aMaxBootPosition);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, this is const */
    *aMaxBootPosition = SchemaDefs::MaxBootPosition;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultMachineFolder) (BSTR *aDefaultMachineFolder)
{
    CheckComArgOutPointerValid(aDefaultMachineFolder);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mDefaultMachineFolderFull.cloneTo (aDefaultMachineFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultMachineFolder) (IN_BSTR aDefaultMachineFolder)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultMachineFolder (aDefaultMachineFolder);
    if (SUCCEEDED (rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultHardDiskFolder) (BSTR *aDefaultHardDiskFolder)
{
    CheckComArgOutPointerValid(aDefaultHardDiskFolder);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mDefaultHardDiskFolderFull.cloneTo (aDefaultHardDiskFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultHardDiskFolder) (IN_BSTR aDefaultHardDiskFolder)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultHardDiskFolder (aDefaultHardDiskFolder);
    if (SUCCEEDED (rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::
COMGETTER(HardDiskFormats) (ComSafeArrayOut (IHardDiskFormat *, aHardDiskFormats))
{
    if (ComSafeArrayOutIsNull (aHardDiskFormats))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    SafeIfaceArray <IHardDiskFormat> hardDiskFormats (mHardDiskFormats);
    hardDiskFormats.detachTo (ComSafeArrayOutArg (aHardDiskFormats));

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultHardDiskFormat) (BSTR *aDefaultHardDiskFormat)
{
    CheckComArgOutPointerValid(aDefaultHardDiskFormat);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mDefaultHardDiskFormat.cloneTo (aDefaultHardDiskFormat);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultHardDiskFormat) (IN_BSTR aDefaultHardDiskFormat)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultHardDiskFormat (aDefaultHardDiskFormat);
    if (SUCCEEDED (rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(RemoteDisplayAuthLibrary) (BSTR *aRemoteDisplayAuthLibrary)
{
    CheckComArgOutPointerValid(aRemoteDisplayAuthLibrary);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mRemoteDisplayAuthLibrary.cloneTo (aRemoteDisplayAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(RemoteDisplayAuthLibrary) (IN_BSTR aRemoteDisplayAuthLibrary)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setRemoteDisplayAuthLibrary (aRemoteDisplayAuthLibrary);
    if (SUCCEEDED (rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(WebServiceAuthLibrary) (BSTR *aWebServiceAuthLibrary)
{
    CheckComArgOutPointerValid(aWebServiceAuthLibrary);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    mWebServiceAuthLibrary.cloneTo (aWebServiceAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(WebServiceAuthLibrary) (IN_BSTR aWebServiceAuthLibrary)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setWebServiceAuthLibrary (aWebServiceAuthLibrary);
    if (SUCCEEDED (rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(HWVirtExEnabled) (BOOL *enabled)
{
    if (!enabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *enabled = mHWVirtExEnabled;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(HWVirtExEnabled) (BOOL enabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    mHWVirtExEnabled = enabled;

    HRESULT rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(LogHistoryCount) (ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *count = mLogHistoryCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(LogHistoryCount) (ULONG count)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    mLogHistoryCount = count;

    HRESULT rc = mParent->saveSettings();

    return rc;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::loadSettings (const settings::Key &aGlobal)
{
    using namespace settings;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (!aGlobal.isNull(), E_FAIL);

    HRESULT rc = S_OK;

    Key properties = aGlobal.key ("SystemProperties");

    Bstr bstr;

    bstr = properties.stringValue ("defaultMachineFolder");
    rc = setDefaultMachineFolder (bstr);
    CheckComRCReturnRC (rc);

    bstr = properties.stringValue ("defaultHardDiskFolder");
    rc = setDefaultHardDiskFolder (bstr);
    CheckComRCReturnRC (rc);

    bstr = properties.stringValue ("defaultHardDiskFormat");
    rc = setDefaultHardDiskFormat (bstr);
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

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    ComAssertRet (!aGlobal.isNull(), E_FAIL);

    /* first, delete the entry */
    Key properties = aGlobal.findKey ("SystemProperties");
    if (!properties.isNull())
        properties.zap();
    /* then, recreate it */
    properties = aGlobal.createKey ("SystemProperties");

    if (mDefaultMachineFolder)
        properties.setValue <Bstr> ("defaultMachineFolder", mDefaultMachineFolder);

    if (mDefaultHardDiskFolder)
        properties.setValue <Bstr> ("defaultHardDiskFolder", mDefaultHardDiskFolder);

    if (mDefaultHardDiskFormat)
        properties.setValue <Bstr> ("defaultHardDiskFormat", mDefaultHardDiskFormat);

    if (mRemoteDisplayAuthLibrary)
        properties.setValue <Bstr> ("remoteDisplayAuthLibrary", mRemoteDisplayAuthLibrary);

    if (mWebServiceAuthLibrary)
        properties.setValue <Bstr> ("webServiceAuthLibrary", mWebServiceAuthLibrary);

    properties.setValue <bool> ("HWVirtExEnabled", !!mHWVirtExEnabled);

    properties.setValue <ULONG> ("LogHistoryCount", mLogHistoryCount);

    return S_OK;
}

/**
 * Rerurns a hard disk format object corresponding to the given format
 * identifier or null if no such format.
 *
 * @param aFormat   Format identifier.
 *
 * @return ComObjPtr<HardDiskFormat>
 */
ComObjPtr <HardDiskFormat> SystemProperties::hardDiskFormat (CBSTR aFormat)
{
    ComObjPtr <HardDiskFormat> format;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), format);

    AutoReadLock alock (this);

    for (HardDiskFormatList::const_iterator it = mHardDiskFormats.begin();
         it != mHardDiskFormats.end(); ++ it)
    {
        /* HardDiskFormat is all const, no need to lock */

        if ((*it)->id().compareIgnoreCase (aFormat) == 0)
        {
            format = *it;
            break;
        }
    }

    return format;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::setDefaultMachineFolder (CBSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "Machines";

    /* get the full file name */
    Utf8Str folder;
    int vrc = mParent->calculateFullPath (path, folder);
    if (RT_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid default machine folder '%ls' (%Rrc)"),
            path.raw(), vrc);

    mDefaultMachineFolder = path;
    mDefaultMachineFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setDefaultHardDiskFolder (CBSTR aPath)
{
    Utf8Str path;
    if (aPath && *aPath)
        path = aPath;
    else
        path = "HardDisks";

    /* get the full file name */
    Utf8Str folder;
    int vrc = mParent->calculateFullPath (path, folder);
    if (RT_FAILURE (vrc))
        return setError (E_FAIL,
            tr ("Invalid default hard disk folder '%ls' (%Rrc)"),
            path.raw(), vrc);

    mDefaultHardDiskFolder = path;
    mDefaultHardDiskFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setDefaultHardDiskFormat (CBSTR aFormat)
{
    if (aFormat && *aFormat)
        mDefaultHardDiskFormat = aFormat;
    else
        mDefaultHardDiskFormat = "VDI";

    return S_OK;
}

HRESULT SystemProperties::setRemoteDisplayAuthLibrary (CBSTR aPath)
{
    if (aPath && *aPath)
        mRemoteDisplayAuthLibrary = aPath;
    else
        mRemoteDisplayAuthLibrary = "VRDPAuth";

    return S_OK;
}

HRESULT SystemProperties::setWebServiceAuthLibrary (CBSTR aPath)
{
    if (aPath && *aPath)
        mWebServiceAuthLibrary = aPath;
    else
        mWebServiceAuthLibrary = "VRDPAuth";

    return S_OK;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
