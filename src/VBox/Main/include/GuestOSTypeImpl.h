/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTOSTYPEIMPL
#define ____H_GUESTOSTYPEIMPL

#include "VirtualBoxBase.h"
#include "Global.h"

#include <VBox/ostypes.h>

class ATL_NO_VTABLE GuestOSType :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestOSType)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestOSType, IGuestOSType)

    DECLARE_NOT_AGGREGATABLE(GuestOSType)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(GuestOSType)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestOSType)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(GuestOSType)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(const Global::OSType &ostype);
    void uninit();

    // IGuestOSType properties
    STDMETHOD(COMGETTER(FamilyId))(BSTR *aFamilyId);
    STDMETHOD(COMGETTER(FamilyDescription))(BSTR *aFamilyDescription);
    STDMETHOD(COMGETTER(Id))(BSTR *aId);
    STDMETHOD(COMGETTER(Description))(BSTR *aDescription);
    STDMETHOD(COMGETTER(Is64Bit))(BOOL *aIs64Bit);
    STDMETHOD(COMGETTER(RecommendedIOAPIC))(BOOL *aRecommendedIOAPIC);
    STDMETHOD(COMGETTER(RecommendedVirtEx))(BOOL *aRecommendedVirtEx);
    STDMETHOD(COMGETTER(RecommendedRAM))(ULONG *aRAMSize);
    STDMETHOD(COMGETTER(RecommendedVRAM))(ULONG *aVRAMSize);
    STDMETHOD(COMGETTER(Recommended2DVideoAcceleration))(BOOL *aRecommended2DVideoAcceleration);
    STDMETHOD(COMGETTER(Recommended3DAcceleration))(BOOL *aRecommended3DAcceleration);
    STDMETHOD(COMGETTER(RecommendedHDD))(LONG64 *aHDDSize);
    STDMETHOD(COMGETTER(AdapterType))(NetworkAdapterType_T *aNetworkAdapterType);
    STDMETHOD(COMGETTER(RecommendedFirmware))(FirmwareType_T *aFirmwareType);
    STDMETHOD(COMGETTER(RecommendedDvdStorageBus))(StorageBus_T *aStorageBusType);
    STDMETHOD(COMGETTER(RecommendedDvdStorageController))(StorageControllerType_T *aStorageControllerType);
    STDMETHOD(COMGETTER(RecommendedHdStorageBus))(StorageBus_T *aStorageBusType);
    STDMETHOD(COMGETTER(RecommendedHdStorageController))(StorageControllerType_T *aStorageControllerType);
    STDMETHOD(COMGETTER(RecommendedPae))(BOOL *aRecommendedExtHw);
    STDMETHOD(COMGETTER(RecommendedUsbHid))(BOOL *aRecommendedUsbHid);
    STDMETHOD(COMGETTER(RecommendedHpet))(BOOL *aRecommendedHpet);
    STDMETHOD(COMGETTER(RecommendedUsbTablet))(BOOL *aRecommendedUsbTablet);
    STDMETHOD(COMGETTER(RecommendedRtcUseUtc))(BOOL *aRecommendedRtcUseUtc);
    STDMETHOD(COMGETTER(RecommendedChipset)) (ChipsetType_T *aChipsetType);
    STDMETHOD(COMGETTER(RecommendedAudioController)) (AudioControllerType_T *aAudioController);
    STDMETHOD(COMGETTER(RecommendedFloppy)) (BOOL *aRecommendedFloppy);
    STDMETHOD(COMGETTER(RecommendedUsb)) (BOOL *aRecommendedUsb);

    // public methods only for internal purposes
    const Bstr &id() const { return mID; }
    bool is64Bit() const { return !!(mOSHint & VBOXOSHINT_64BIT); }
    bool recommendedIOAPIC() const { return !!(mOSHint & VBOXOSHINT_IOAPIC); }
    bool recommendedVirtEx() const { return !!(mOSHint & VBOXOSHINT_HWVIRTEX); }
    bool recommendedEFI() const { return !!(mOSHint & VBOXOSHINT_EFI); }
    NetworkAdapterType_T networkAdapterType() const { return mNetworkAdapterType; }
    uint32_t numSerialEnabled() const { return mNumSerialEnabled; }

private:

    const Bstr mFamilyID;
    const Bstr mFamilyDescription;
    const Bstr mID;
    const Bstr mDescription;
    const VBOXOSTYPE mOSType;
    const uint32_t mOSHint;
    const uint32_t mRAMSize;
    const uint32_t mVRAMSize;
    const uint64_t mHDDSize;
    const uint32_t mMonitorCount;
    const NetworkAdapterType_T mNetworkAdapterType;
    const uint32_t mNumSerialEnabled;
    const StorageControllerType_T mDvdStorageControllerType;
    const StorageBus_T mDvdStorageBusType;
    const StorageControllerType_T mHdStorageControllerType;
    const StorageBus_T mHdStorageBusType;
    const ChipsetType_T mChipsetType;
    const AudioControllerType_T mAudioControllerType;
};

#endif // ____H_GUESTOSTYPEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
