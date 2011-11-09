/* $Id$ */
/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SystemPropertiesImpl.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

// generated header
#include "SchemaDefs.h"

#include <iprt/dir.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h>
#include <VBox/vd.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

SystemProperties::SystemProperties()
    : mParent(NULL),
      m(new settings::SystemProperties)
{
}

SystemProperties::~SystemProperties()
{
    delete m;
}


HRESULT SystemProperties::FinalConstruct()
{
    return BaseFinalConstruct();
}

void SystemProperties::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the system information object.
 *
 * @returns COM result indicator
 */
HRESULT SystemProperties::init(VirtualBox *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    setDefaultMachineFolder(Utf8Str::Empty);
    setDefaultHardDiskFormat(Utf8Str::Empty);

    setVRDEAuthLibrary(Utf8Str::Empty);
    setDefaultVRDEExtPack(Utf8Str::Empty);

    m->ulLogHistoryCount = 3;

    HRESULT rc = S_OK;

    /* Fetch info of all available hd backends. */

    /// @todo NEWMEDIA VDBackendInfo needs to be improved to let us enumerate
    /// any number of backends

    VDBACKENDINFO aVDInfo[100];
    unsigned cEntries;
    int vrc = VDBackendInfo(RT_ELEMENTS(aVDInfo), aVDInfo, &cEntries);
    AssertRC(vrc);
    if (RT_SUCCESS(vrc))
    {
        for (unsigned i = 0; i < cEntries; ++ i)
        {
            ComObjPtr<MediumFormat> hdf;
            rc = hdf.createObject();
            if (FAILED(rc)) break;

            rc = hdf->init(&aVDInfo[i]);
            if (FAILED(rc)) break;

            m_llMediumFormats.push_back(hdf);
        }
    }

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

    unconst(mParent) = NULL;
}

// ISystemProperties properties
/////////////////////////////////////////////////////////////////////////////


STDMETHODIMP SystemProperties::COMGETTER(MinGuestRAM)(ULONG *minRAM)
{
    CheckComArgOutPointerValid(minRAM);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MIN_IN_MB >= SchemaDefs::MinGuestRAM);
    *minRAM = MM_RAM_MIN_IN_MB;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestRAM)(ULONG *maxRAM)
{
    CheckComArgOutPointerValid(maxRAM);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    AssertCompile(MM_RAM_MAX_IN_MB <= SchemaDefs::MaxGuestRAM);
    ULONG maxRAMSys = MM_RAM_MAX_IN_MB;
    ULONG maxRAMArch = maxRAMSys;
    *maxRAM = RT_MIN(maxRAMSys, maxRAMArch);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestVRAM)(ULONG *minVRAM)
{
    CheckComArgOutPointerValid(minVRAM);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *minVRAM = SchemaDefs::MinGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestVRAM)(ULONG *maxVRAM)
{
    CheckComArgOutPointerValid(maxVRAM);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *maxVRAM = SchemaDefs::MaxGuestVRAM;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MinGuestCPUCount)(ULONG *minCPUCount)
{
    CheckComArgOutPointerValid(minCPUCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *minCPUCount = SchemaDefs::MinCPUCount; // VMM_MIN_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestCPUCount)(ULONG *maxCPUCount)
{
    CheckComArgOutPointerValid(maxCPUCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *maxCPUCount = SchemaDefs::MaxCPUCount; // VMM_MAX_CPU_COUNT

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxGuestMonitors)(ULONG *maxMonitors)
{
    CheckComArgOutPointerValid(maxMonitors);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *maxMonitors = SchemaDefs::MaxGuestMonitors;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(InfoVDSize)(LONG64 *infoVDSize)
{
    CheckComArgOutPointerValid(infoVDSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /*
     * The BIOS supports currently 32 bit LBA numbers (implementing the full
     * 48 bit range is in theory trivial, but the crappy compiler makes things
     * more difficult). This translates to almost 2 TiBytes (to be on the safe
     * side, the reported limit is 1 MiByte less than that, as the total number
     * of sectors should fit in 32 bits, too), which should be enough for the
     * moment. Since the MBR partition tables support only 32bit sector numbers
     * and thus the BIOS can only boot from disks smaller than 2T this is a
     * rather hard limit.
     *
     * The virtual ATA/SATA disks support complete LBA48, and SCSI supports
     * LBA64 (almost, more like LBA55 in practice), so the theoretical maximum
     * disk size is 128 PiByte/16 EiByte. The GUI works nicely with 6 orders
     * of magnitude, but not with 11..13 orders of magnitude.
    */
    /* no need to lock, this is const */
    *infoVDSize = 2 * _1T - _1M;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(SerialPortCount)(ULONG *count)
{
    CheckComArgOutPointerValid(count);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *count = SchemaDefs::SerialPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(ParallelPortCount)(ULONG *count)
{
    CheckComArgOutPointerValid(count);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *count = SchemaDefs::ParallelPortCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(MaxBootPosition)(ULONG *aMaxBootPosition)
{
    CheckComArgOutPointerValid(aMaxBootPosition);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    *aMaxBootPosition = SchemaDefs::MaxBootPosition;

    return S_OK;
}


STDMETHODIMP SystemProperties::GetMaxNetworkAdapters(ChipsetType_T aChipset, ULONG *count)
{
    CheckComArgOutPointerValid(count);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need for locking, no state */
    uint32_t uResult = Global::getMaxNetworkAdapters(aChipset);
    if (uResult == 0)
        AssertMsgFailed(("Invalid chipset type %d\n", aChipset));

    *count = uResult;

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxNetworkAdaptersOfType(ChipsetType_T aChipset, NetworkAttachmentType_T aType, ULONG *count)
{
    CheckComArgOutPointerValid(count);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need for locking, no state */
    uint32_t uResult = Global::getMaxNetworkAdapters(aChipset);
    if (uResult == 0)
        AssertMsgFailed(("Invalid chipset type %d\n", aChipset));

    switch (aType)
    {
        case NetworkAttachmentType_NAT:
        case NetworkAttachmentType_Internal:
            /* chipset default is OK */
            break;
        case NetworkAttachmentType_Bridged:
            /* Maybe use current host interface count here? */
            break;
        case NetworkAttachmentType_HostOnly:
            uResult = RT_MIN(uResult, 8);
            break;
        default:
            AssertMsgFailed(("Unhandled attachment type %d\n", aType));
    }

    *count = uResult;

    return S_OK;
}


STDMETHODIMP SystemProperties::GetMaxDevicesPerPortForStorageBus(StorageBus_T aBus,
                                                                 ULONG *aMaxDevicesPerPort)
{
    CheckComArgOutPointerValid(aMaxDevicesPerPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        case StorageBus_SCSI:
        case StorageBus_SAS:
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

STDMETHODIMP SystemProperties::GetMinPortCountForStorageBus(StorageBus_T aBus,
                                                            ULONG *aMinPortCount)
{
    CheckComArgOutPointerValid(aMinPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

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
        case StorageBus_SAS:
        {
            *aMinPortCount = 8;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxPortCountForStorageBus(StorageBus_T aBus,
                                                            ULONG *aMaxPortCount)
{
    CheckComArgOutPointerValid(aMaxPortCount);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

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
        case StorageBus_SAS:
        {
            *aMaxPortCount = 8;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::GetMaxInstancesOfStorageBus(ChipsetType_T aChipset,
                                                           StorageBus_T  aBus,
                                                           ULONG *aMaxInstances)
{
    CheckComArgOutPointerValid(aMaxInstances);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ULONG cCtrs = 0;

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_SATA:
        case StorageBus_SCSI:
        case StorageBus_SAS:
            cCtrs = aChipset == ChipsetType_ICH9 ? 8 : 1;
            break;
        case StorageBus_IDE:
        case StorageBus_Floppy:
        {
            cCtrs = 1;
            break;
        }
        default:
            AssertMsgFailed(("Invalid bus type %d\n", aBus));
    }

    *aMaxInstances = cCtrs;

    return S_OK;
}

STDMETHODIMP SystemProperties::GetDeviceTypesForStorageBus(StorageBus_T aBus,
                                                           ComSafeArrayOut(DeviceType_T, aDeviceTypes))
{
    CheckComArgOutSafeArrayPointerValid(aDeviceTypes);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    switch (aBus)
    {
        case StorageBus_IDE:
        case StorageBus_SATA:
        {
            com::SafeArray<DeviceType_T> saDeviceTypes(2);
            saDeviceTypes[0] = DeviceType_DVD;
            saDeviceTypes[1] = DeviceType_HardDisk;
            saDeviceTypes.detachTo(ComSafeArrayOutArg(aDeviceTypes));
            break;
        }
        case StorageBus_SCSI:
        case StorageBus_SAS:
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

STDMETHODIMP SystemProperties::GetDefaultIoCacheSettingForStorageController(StorageControllerType_T aControllerType, BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, this is const */
    switch (aControllerType)
    {
        case StorageControllerType_LsiLogic:
        case StorageControllerType_BusLogic:
        case StorageControllerType_IntelAhci:
        case StorageControllerType_LsiLogicSas:
            *aEnabled = false;
            break;
        case StorageControllerType_PIIX3:
        case StorageControllerType_PIIX4:
        case StorageControllerType_ICH6:
        case StorageControllerType_I82078:
            *aEnabled = true;
            break;
        default:
            AssertMsgFailed(("Invalid controller type %d\n", aControllerType));
    }
    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultMachineFolder)(BSTR *aDefaultMachineFolder)
{
    CheckComArgOutPointerValid(aDefaultMachineFolder);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strDefaultMachineFolder.cloneTo(aDefaultMachineFolder);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultMachineFolder)(IN_BSTR aDefaultMachineFolder)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = setDefaultMachineFolder(aDefaultMachineFolder);
    alock.release();

    if (SUCCEEDED(rc))
    {
        // VirtualBox::saveSettings() needs vbox write lock
        AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(MediumFormats)(ComSafeArrayOut(IMediumFormat *, aMediumFormats))
{
    CheckComArgOutSafeArrayPointerValid(aMediumFormats);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IMediumFormat> mediumFormats(m_llMediumFormats);
    mediumFormats.detachTo(ComSafeArrayOutArg(aMediumFormats));

    return S_OK;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultHardDiskFormat)(BSTR *aDefaultHardDiskFormat)
{
    CheckComArgOutPointerValid(aDefaultHardDiskFormat);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strDefaultHardDiskFormat.cloneTo(aDefaultHardDiskFormat);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultHardDiskFormat)(IN_BSTR aDefaultHardDiskFormat)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = setDefaultHardDiskFormat(aDefaultHardDiskFormat);
    alock.release();

    if (SUCCEEDED(rc))
    {
        // VirtualBox::saveSettings() needs vbox write lock
        AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(FreeDiskSpaceWarning)(LONG64 *aFreeSpace)
{
    CheckComArgOutPointerValid(aFreeSpace);

    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMSETTER(FreeDiskSpaceWarning)(LONG64 /* aFreeSpace */)
{
    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMGETTER(FreeDiskSpacePercentWarning)(ULONG *aFreeSpacePercent)
{
    CheckComArgOutPointerValid(aFreeSpacePercent);

    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMSETTER(FreeDiskSpacePercentWarning)(ULONG /* aFreeSpacePercent */)
{
    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMGETTER(FreeDiskSpaceError)(LONG64 *aFreeSpace)
{
    CheckComArgOutPointerValid(aFreeSpace);

    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMSETTER(FreeDiskSpaceError)(LONG64 /* aFreeSpace */)
{
    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMGETTER(FreeDiskSpacePercentError)(ULONG *aFreeSpacePercent)
{
    CheckComArgOutPointerValid(aFreeSpacePercent);

    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMSETTER(FreeDiskSpacePercentError)(ULONG /* aFreeSpacePercent */)
{
    ReturnComNotImplemented();
}

STDMETHODIMP SystemProperties::COMGETTER(VRDEAuthLibrary)(BSTR *aVRDEAuthLibrary)
{
    CheckComArgOutPointerValid(aVRDEAuthLibrary);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strVRDEAuthLibrary.cloneTo(aVRDEAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(VRDEAuthLibrary)(IN_BSTR aVRDEAuthLibrary)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = setVRDEAuthLibrary(aVRDEAuthLibrary);
    alock.release();

    if (SUCCEEDED(rc))
    {
        // VirtualBox::saveSettings() needs vbox write lock
        AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(WebServiceAuthLibrary)(BSTR *aWebServiceAuthLibrary)
{
    CheckComArgOutPointerValid(aWebServiceAuthLibrary);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strWebServiceAuthLibrary.cloneTo(aWebServiceAuthLibrary);

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(WebServiceAuthLibrary)(IN_BSTR aWebServiceAuthLibrary)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = setWebServiceAuthLibrary(aWebServiceAuthLibrary);
    alock.release();

    if (SUCCEEDED(rc))
    {
        // VirtualBox::saveSettings() needs vbox write lock
        AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
        rc = mParent->saveSettings();
    }

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultVRDEExtPack)(BSTR *aExtPack)
{
    CheckComArgOutPointerValid(aExtPack);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        Utf8Str strExtPack(m->strDefaultVRDEExtPack);
        if (strExtPack.isNotEmpty())
        {
            if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
                hrc = S_OK;
            else
#ifdef VBOX_WITH_EXTPACK
                hrc = mParent->getExtPackManager()->checkVrdeExtPack(&strExtPack);
#else
                hrc = setError(E_FAIL, tr("The extension pack '%s' does not exist"), strExtPack.c_str());
#endif
        }
        else
        {
#ifdef VBOX_WITH_EXTPACK
            hrc = mParent->getExtPackManager()->getDefaultVrdeExtPack(&strExtPack);
#endif
            if (strExtPack.isEmpty())
            {
                /*
                 * Klugde - check if VBoxVRDP.dll/.so/.dylib is installed.
                 * This is hardcoded uglyness, sorry.
                 */
                char szPath[RTPATH_MAX];
                int vrc = RTPathAppPrivateArch(szPath, sizeof(szPath));
                if (RT_SUCCESS(vrc))
                    vrc = RTPathAppend(szPath, sizeof(szPath), "VBoxVRDP");
                if (RT_SUCCESS(vrc))
                    vrc = RTStrCat(szPath, sizeof(szPath), RTLdrGetSuff());
                if (RT_SUCCESS(vrc) && RTFileExists(szPath))
                {
                    /* Illegal extpack name, so no conflict. */
                    strExtPack = VBOXVRDP_KLUDGE_EXTPACK_NAME;
                }
            }
        }

        if (SUCCEEDED(hrc))
            strExtPack.cloneTo(aExtPack);
    }

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(DefaultVRDEExtPack)(IN_BSTR aExtPack)
{
    CheckComArgNotNull(aExtPack);
    Utf8Str strExtPack(aExtPack);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        if (strExtPack.isNotEmpty())
        {
            if (strExtPack.equals(VBOXVRDP_KLUDGE_EXTPACK_NAME))
                hrc = S_OK;
            else
#ifdef VBOX_WITH_EXTPACK
                hrc = mParent->getExtPackManager()->checkVrdeExtPack(&strExtPack);
#else
                hrc = setError(E_FAIL, tr("The extension pack '%s' does not exist"), strExtPack.c_str());
#endif
        }
        if (SUCCEEDED(hrc))
        {
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            hrc = setDefaultVRDEExtPack(aExtPack);
            if (SUCCEEDED(hrc))
            {
                /* VirtualBox::saveSettings() needs the VirtualBox write lock. */
                alock.release();
                AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
                hrc = mParent->saveSettings();
            }
        }
    }

    return hrc;
}

STDMETHODIMP SystemProperties::COMGETTER(LogHistoryCount)(ULONG *count)
{
    CheckComArgOutPointerValid(count);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *count = m->ulLogHistoryCount;

    return S_OK;
}

STDMETHODIMP SystemProperties::COMSETTER(LogHistoryCount)(ULONG count)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->ulLogHistoryCount = count;
    alock.release();

    // VirtualBox::saveSettings() needs vbox write lock
    AutoWriteLock vboxLock(mParent COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mParent->saveSettings();

    return rc;
}

STDMETHODIMP SystemProperties::COMGETTER(DefaultAudioDriver)(AudioDriverType_T *aAudioDriver)
{
    CheckComArgOutPointerValid(aAudioDriver);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAudioDriver = settings::MachineConfigFile::getHostDefaultAudioDriver();

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

HRESULT SystemProperties::loadSettings(const settings::SystemProperties &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    rc = setDefaultMachineFolder(data.strDefaultMachineFolder);
    if (FAILED(rc)) return rc;

    rc = setDefaultHardDiskFormat(data.strDefaultHardDiskFormat);
    if (FAILED(rc)) return rc;

    rc = setVRDEAuthLibrary(data.strVRDEAuthLibrary);
    if (FAILED(rc)) return rc;

    rc = setWebServiceAuthLibrary(data.strWebServiceAuthLibrary);
    if (FAILED(rc)) return rc;

    rc = setDefaultVRDEExtPack(data.strDefaultVRDEExtPack);
    if (FAILED(rc)) return rc;

    m->ulLogHistoryCount = data.ulLogHistoryCount;

    return S_OK;
}

HRESULT SystemProperties::saveSettings(settings::SystemProperties &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *m;

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
ComObjPtr<MediumFormat> SystemProperties::mediumFormat(const Utf8Str &aFormat)
{
    ComObjPtr<MediumFormat> format;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), format);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    for (MediumFormatList::const_iterator it = m_llMediumFormats.begin();
         it != m_llMediumFormats.end();
         ++ it)
    {
        /* MediumFormat is all const, no need to lock */

        if ((*it)->getId().compare(aFormat, Utf8Str::CaseInsensitive) == 0)
        {
            format = *it;
            break;
        }
    }

    return format;
}

/**
 * Returns a medium format object corresponding to the given file extension or
 * null if no such format.
 *
 * @param aExt   File extension.
 *
 * @return ComObjPtr<MediumFormat>
 */
ComObjPtr<MediumFormat> SystemProperties::mediumFormatFromExtension(const Utf8Str &aExt)
{
    ComObjPtr<MediumFormat> format;

    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), format);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    bool fFound = false;
    for (MediumFormatList::const_iterator it = m_llMediumFormats.begin();
         it != m_llMediumFormats.end() && !fFound;
         ++it)
    {
        /* MediumFormat is all const, no need to lock */
        MediumFormat::StrList aFileList = (*it)->getFileExtensions();
        for (MediumFormat::StrList::const_iterator it1 = aFileList.begin();
             it1 != aFileList.end();
             ++it1)
        {
            if ((*it1).compare(aExt, Utf8Str::CaseInsensitive) == 0)
            {
                format = *it;
                fFound = true;
                break;
            }
        }
    }

    return format;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the user's home directory. Wrapper around RTPathUserHome().
 * @param strPath
 * @return
 */
HRESULT SystemProperties::getUserHomeDirectory(Utf8Str &strPath)
{
    char szHome[RTPATH_MAX];
    int vrc = RTPathUserHome(szHome, sizeof(szHome));
    if (RT_FAILURE(vrc))
        return setError(E_FAIL,
                        tr("Cannot determine user home directory (%Rrc)"),
                        vrc);
    strPath = szHome;
    return S_OK;
}

/**
 * Internal implementation to set the default machine folder. Gets called
 * from the public attribute setter as well as loadSettings(). With 4.0,
 * the "default default" machine folder has changed, and we now require
 * a full path always.
 * @param aPath
 * @return
 */
HRESULT SystemProperties::setDefaultMachineFolder(const Utf8Str &strPath)
{
    Utf8Str path(strPath);      // make modifiable
    if (    path.isEmpty()          // used by API calls to reset the default
         || path == "Machines"      // this value (exactly like this, without path) is stored
                                    // in VirtualBox.xml if user upgrades from before 4.0 and
                                    // has not changed the default machine folder
       )
    {
        // new default with VirtualBox 4.0: "$HOME/VirtualBox VMs"
        HRESULT rc = getUserHomeDirectory(path);
        if (FAILED(rc)) return rc;
        path += RTPATH_SLASH_STR "VirtualBox VMs";
    }

    if (!RTPathStartsWithRoot(path.c_str()))
        return setError(E_INVALIDARG,
                        tr("Given default machine folder '%s' is not fully qualified"),
                        path.c_str());

    m->strDefaultMachineFolder = path;

    return S_OK;
}

HRESULT SystemProperties::setDefaultHardDiskFormat(const Utf8Str &aFormat)
{
    if (!aFormat.isEmpty())
        m->strDefaultHardDiskFormat = aFormat;
    else
        m->strDefaultHardDiskFormat = "VDI";

    return S_OK;
}

HRESULT SystemProperties::setVRDEAuthLibrary(const Utf8Str &aPath)
{
    if (!aPath.isEmpty())
        m->strVRDEAuthLibrary = aPath;
    else
        m->strVRDEAuthLibrary = "VBoxAuth";

    return S_OK;
}

HRESULT SystemProperties::setWebServiceAuthLibrary(const Utf8Str &aPath)
{
    if (!aPath.isEmpty())
        m->strWebServiceAuthLibrary = aPath;
    else
        m->strWebServiceAuthLibrary = "VBoxAuth";

    return S_OK;
}

HRESULT SystemProperties::setDefaultVRDEExtPack(const Utf8Str &aExtPack)
{
    m->strDefaultVRDEExtPack = aExtPack;

    return S_OK;
}
