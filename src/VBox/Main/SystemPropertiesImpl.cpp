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
#include <iprt/process.h>
#include <iprt/ldr.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>
#include <VBox/VBoxHDD.h>

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
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    setDefaultMachineFolder(Utf8Str::Null);
    setDefaultHardDiskFolder(Utf8Str::Null);
    setDefaultHardDiskFormat(Utf8Str::Null);

    setRemoteDisplayAuthLibrary(Utf8Str::Null);

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
    if (RT_SUCCESS(vrc))
    {
        for (unsigned i = 0; i < cEntries; ++ i)
        {
            ComObjPtr<MediumFormat> hdf;
            rc = hdf.createObject();
            CheckComRCBreakRC (rc);

            rc = hdf->init (&aVDInfo [i]);
            CheckComRCBreakRC (rc);

            mMediumFormats.push_back (hdf);
        }
    }

    /* Driver defaults which are OS specific */
#if defined (RT_OS_WINDOWS)
# ifdef VBOX_WITH_WINMM
    mDefaultAudioDriver = AudioDriverType_WinMM;
# else /* VBOX_WITH_WINMM */
    mDefaultAudioDriver = AudioDriverType_DirectSound;
# endif /* !VBOX_WITH_WINMM */
#elif defined (RT_OS_SOLARIS)
    mDefaultAudioDriver = AudioDriverType_SolAudio;
#elif defined (RT_OS_LINUX)
# if defined (VBOX_WITH_PULSE)
    /* Check for the pulse library & that the pulse audio daemon is running. */
    if (RTProcIsRunningByName ("pulseaudio") &&
        RTLdrIsLoadable ("libpulse.so.0"))
        mDefaultAudioDriver = AudioDriverType_Pulse;
    else
# endif /* VBOX_WITH_PULSE */
# if defined (VBOX_WITH_ALSA)
        /* Check if we can load the ALSA library */
        if (RTLdrIsLoadable ("libasound.so.2"))
            mDefaultAudioDriver = AudioDriverType_ALSA;
        else
# endif /* VBOX_WITH_ALSA */
            mDefaultAudioDriver = AudioDriverType_OSS;
#elif defined (RT_OS_DARWIN)
    mDefaultAudioDriver = AudioDriverType_CoreAudio;
#elif defined (RT_OS_OS2)
    mDefaultAudioDriver = AudioDriverType_MMP;
#elif defined (RT_OS_FREEBSD)
    mDefaultAudioDriver = AudioDriverType_OSS;
#else
    mDefaultAudioDriver = AudioDriverType_Null;
#endif

    /* Confirm a successful initialization */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SystemProperties::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent).setNull();
}

// ISystemProperties properties
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP SystemProperties::COMGETTER(MinGuestRAM)(ULONG *minRAM)
{
    if (!minRAM)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MIN_IN_MB >= SchemaDefs::MinGuestRAM);
    *minRAM = MM_RAM_MIN_IN_MB;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestRAM)(ULONG *maxRAM)
{
    if (!maxRAM)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MAX_IN_MB <= SchemaDefs::MaxGuestRAM);
    ULONG maxRAMSys = MM_RAM_MAX_IN_MB;
    ULONG maxRAMArch = maxRAMSys;
#if HC_ARCH_BITS == 32 && !defined(RT_OS_DARWIN)
# ifdef RT_OS_WINDOWS
    maxRAMArch = UINT32_C(1500);
# else
    maxRAMArch = UINT32_C(2560);
# endif
#endif
    *maxRAM = RT_MIN(maxRAMSys, maxRAMArch);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestVRAM)(ULONG *minVRAM)
{
    if (!minVRAM)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *minVRAM = SchemaDefs::MinGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestVRAM)(ULONG *maxVRAM)
{
    if (!maxVRAM)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *maxVRAM = SchemaDefs::MaxGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestCPUCount)(ULONG *minCPUCount)
{
    if (!minCPUCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *minCPUCount = SchemaDefs::MinCPUCount; // VMM_MIN_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestCPUCount)(ULONG *maxCPUCount)
{
    if (!maxCPUCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *maxCPUCount = SchemaDefs::MaxCPUCount; // VMM_MAX_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestMonitors)(ULONG *maxMonitors)
{
    if (!maxMonitors)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *maxMonitors = SchemaDefs::MaxGuestMonitors;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxVDISize)(ULONG64 *maxVDISize)
{
    if (!maxVDISize)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

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

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::NetworkAdapterCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(SerialPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::SerialPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(ParallelPortCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *count = SchemaDefs::ParallelPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxBootPosition)(ULONG *aMaxBootPosition)
{
    CheckComArgOutPointerValid(aMaxBootPosition);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    *aMaxBootPosition = SchemaDefs::MaxBootPosition;

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxDevicesPerPortForStorageBus (StorageBus_T aBus, ULONG *aMaxDevicesPerPort)
{
    CheckComArgOutPointerValid(aMaxDevicesPerPort);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        case StorageBus_SCSI:
        {
            /* SATA and both SCSI controllers only support one device per port. */
            *aMaxDevicesPerPort = 1;
            break;
        }
        case StorageBus_IDE:
        case StorageBus_Floppy:
        {
            /* The IDE and Floppy controllers support 2 devices. One as master
             * and one as slave (or floppy drive 0 and 1). */
            *aMaxDevicesPerPort = 2;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMinPortCountForStorageBus (StorageBus_T aBus, ULONG *aMinPortCount)
{
    CheckComArgOutPointerValid(aMinPortCount);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        {
            *aMinPortCount = 1;
            break;
        }
        case StorageBus_SCSI:
        {
            *aMinPortCount = 16;
            break;
        }
        case StorageBus_IDE:
        {
            *aMinPortCount = 2;
            break;
        }
        case StorageBus_Floppy:
        {
            *aMinPortCount = 1;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxPortCountForStorageBus (StorageBus_T aBus, ULONG *aMaxPortCount)
{
    CheckComArgOutPointerValid(aMaxPortCount);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        {
            *aMaxPortCount = 30;
            break;
        }
        case StorageBus_SCSI:
        {
            *aMaxPortCount = 16;
            break;
        }
        case StorageBus_IDE:
        {
            *aMaxPortCount = 2;
            break;
        }
        case StorageBus_Floppy:
        {
            *aMaxPortCount = 1;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxInstancesOfStorageBus(StorageBus_T aBus, ULONG *aMaxInstances)
{
    CheckComArgOutPointerValid(aMaxInstances);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        case StorageBus_SCSI:
        case StorageBus_IDE:
        case StorageBus_Floppy:
        {
            /** @todo raise the limits ASAP, per bus type */
            *aMaxInstances = 1;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetDeviceTypesForStorageBus(StorageBus_T aBus,
                                 ComSafeArrayOut(DeviceType_T, aDeviceTypes))
{
    CheckComArgOutSafeArrayPointerValid(aDeviceTypes);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        case StorageBus_IDE:
        {
            com::SafeArray<DeviceType_T> saDeviceTypes(2);
            saDeviceTypes[0] = DeviceType_DVD;
            saDeviceTypes[1] = DeviceType_HardDisk;
            saDeviceTypes.detachTo(ComSafeArrayOutArg(aDeviceTypes));
            break;
        }
        case StorageBus_SCSI:
        {
            com::SafeArray<DeviceType_T> saDeviceTypes(1);
            saDeviceTypes[0] = DeviceType_HardDisk;
            saDeviceTypes.detachTo(ComSafeArrayOutArg(aDeviceTypes));
            break;
        }
        case StorageBus_Floppy:
        {
            com::SafeArray<DeviceType_T> saDeviceTypes(1);
            saDeviceTypes[0] = DeviceType_Floppy;
            saDeviceTypes.detachTo(ComSafeArrayOutArg(aDeviceTypes));
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultMachineFolder) (BSTR *aDefaultMachineFolder)
{
    CheckComArgOutPointerValid(aDefaultMachineFolder);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m_strDefaultMachineFolderFull.cloneTo(aDefaultMachineFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultMachineFolder) (IN_BSTR aDefaultMachineFolder)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultMachineFolder (aDefaultMachineFolder);
    if (SUCCEEDED(rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultHardDiskFolder) (BSTR *aDefaultHardDiskFolder)
{
    CheckComArgOutPointerValid(aDefaultHardDiskFolder);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m_strDefaultHardDiskFolderFull.cloneTo(aDefaultHardDiskFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultHardDiskFolder) (IN_BSTR aDefaultHardDiskFolder)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultHardDiskFolder (aDefaultHardDiskFolder);
    if (SUCCEEDED(rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::
COMGETTER(MediumFormats) (ComSafeArrayOut(IMediumFormat *, aMediumFormats))
{
    if (ComSafeArrayOutIsNull(aMediumFormats))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IMediumFormat> mediumFormats (mMediumFormats);
    mediumFormats.detachTo(ComSafeArrayOutArg(aMediumFormats));

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultHardDiskFormat) (BSTR *aDefaultHardDiskFormat)
{
    CheckComArgOutPointerValid(aDefaultHardDiskFormat);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m_strDefaultHardDiskFormat.cloneTo(aDefaultHardDiskFormat);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultHardDiskFormat) (IN_BSTR aDefaultHardDiskFormat)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setDefaultHardDiskFormat (aDefaultHardDiskFormat);
    if (SUCCEEDED(rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(RemoteDisplayAuthLibrary) (BSTR *aRemoteDisplayAuthLibrary)
{
    CheckComArgOutPointerValid(aRemoteDisplayAuthLibrary);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m_strRemoteDisplayAuthLibrary.cloneTo(aRemoteDisplayAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(RemoteDisplayAuthLibrary) (IN_BSTR aRemoteDisplayAuthLibrary)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setRemoteDisplayAuthLibrary (aRemoteDisplayAuthLibrary);
    if (SUCCEEDED(rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(WebServiceAuthLibrary) (BSTR *aWebServiceAuthLibrary)
{
    CheckComArgOutPointerValid(aWebServiceAuthLibrary);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m_strWebServiceAuthLibrary.cloneTo(aWebServiceAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(WebServiceAuthLibrary) (IN_BSTR aWebServiceAuthLibrary)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    HRESULT rc = setWebServiceAuthLibrary (aWebServiceAuthLibrary);
    if (SUCCEEDED(rc))
        rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(LogHistoryCount) (ULONG *count)
{
    if (!count)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *count = mLogHistoryCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(LogHistoryCount) (ULONG count)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mParent, this);

    mLogHistoryCount = count;

    HRESULT rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultAudioDriver) (AudioDriverType_T *aAudioDriver)
{
    if (!aAudioDriver)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    *aAudioDriver = mDefaultAudioDriver;

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::loadSettings(const settings::SystemProperties &data)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    HRESULT rc = S_OK;

    rc = setDefaultMachineFolder(data.strDefaultMachineFolder);
    CheckComRCReturnRC(rc);

    rc = setDefaultHardDiskFolder(data.strDefaultHardDiskFolder);
    CheckComRCReturnRC(rc);

    rc = setDefaultHardDiskFormat(data.strDefaultHardDiskFormat);
    CheckComRCReturnRC(rc);

    rc = setRemoteDisplayAuthLibrary(data.strRemoteDisplayAuthLibrary);
    CheckComRCReturnRC(rc);

    rc = setWebServiceAuthLibrary(data.strWebServiceAuthLibrary);
    CheckComRCReturnRC(rc);

    mLogHistoryCount = data.ulLogHistoryCount;

    return S_OK;
}

HRESULT SystemProperties::saveSettings(settings::SystemProperties &data)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    data.strDefaultMachineFolder = m_strDefaultMachineFolder;
    data.strDefaultHardDiskFolder = m_strDefaultHardDiskFolder;
    data.strDefaultHardDiskFormat = m_strDefaultHardDiskFormat;
    data.strRemoteDisplayAuthLibrary = m_strRemoteDisplayAuthLibrary;
    data.strWebServiceAuthLibrary = m_strWebServiceAuthLibrary;
    data.ulLogHistoryCount = mLogHistoryCount;

    return S_OK;
}

/**
 * Returns a medium format object corresponding to the given format
 * identifier or null if no such format.
 *
 * @param aFormat   Format identifier.
 *
 * @return ComObjPtr<MediumFormat>
 */
ComObjPtr<MediumFormat> SystemProperties::mediumFormat (CBSTR aFormat)
{
    ComObjPtr<MediumFormat> format;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), format);

    AutoReadLock alock(this);

    for (MediumFormatList::const_iterator it = mMediumFormats.begin();
         it != mMediumFormats.end(); ++ it)
    {
        /* MediumFormat is all const, no need to lock */

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

HRESULT SystemProperties::setDefaultMachineFolder(const Utf8Str &aPath)
{
    Utf8Str path(aPath);
    if (path.isEmpty())
        path = "Machines";

    /* get the full file name */
    Utf8Str folder;
    int vrc = mParent->calculateFullPath(path, folder);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Invalid default machine folder '%s' (%Rrc)"),
                        path.raw(),
                        vrc);

    m_strDefaultMachineFolder = path;
    m_strDefaultMachineFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setDefaultHardDiskFolder(const Utf8Str &aPath)
{
    Utf8Str path(aPath);
    if (path.isEmpty())
        path = "HardDisks";

    /* get the full file name */
    Utf8Str folder;
    int vrc = mParent->calculateFullPath(path, folder);
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Invalid default hard disk folder '%s' (%Rrc)"),
                        path.raw(),
                        vrc);

    m_strDefaultHardDiskFolder = path;
    m_strDefaultHardDiskFolderFull = folder;

    return S_OK;
}

HRESULT SystemProperties::setDefaultHardDiskFormat(const Utf8Str &aFormat)
{
    if (!aFormat.isEmpty())
        m_strDefaultHardDiskFormat = aFormat;
    else
        m_strDefaultHardDiskFormat = "VDI";

    return S_OK;
}

HRESULT SystemProperties::setRemoteDisplayAuthLibrary(const Utf8Str &aPath)
{
    if (!aPath.isEmpty())
        m_strRemoteDisplayAuthLibrary = aPath;
    else
        m_strRemoteDisplayAuthLibrary = "VRDPAuth";

    return S_OK;
}

HRESULT SystemProperties::setWebServiceAuthLibrary(const Utf8Str &aPath)
{
    if (!aPath.isEmpty())
        m_strWebServiceAuthLibrary = aPath;
    else
        m_strWebServiceAuthLibrary = "VRDPAuth";

    return S_OK;
}

