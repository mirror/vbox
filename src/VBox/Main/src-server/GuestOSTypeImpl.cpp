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

#include "GuestOSTypeImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include <iprt/cpp/utils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

GuestOSType::GuestOSType()
    : mOSType(VBOXOSTYPE_Unknown)
    , mOSHint(VBOXOSHINT_NONE)
    , mRAMSize(0), mVRAMSize(0)
    , mHDDSize(0), mMonitorCount(0)
    , mNetworkAdapterType(NetworkAdapterType_Am79C973)
    , mNumSerialEnabled(0)
    , mDvdStorageControllerType(StorageControllerType_PIIX3)
    , mDvdStorageBusType(StorageBus_IDE)
    , mHdStorageControllerType(StorageControllerType_PIIX3)
    , mHdStorageBusType(StorageBus_IDE)
    , mChipsetType(ChipsetType_PIIX3)
    , mAudioControllerType(AudioControllerType_AC97)
{
}

GuestOSType::~GuestOSType()
{
}

HRESULT GuestOSType::FinalConstruct()
{
    return BaseFinalConstruct();
}

void GuestOSType::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest OS type object.
 *
 * @returns COM result indicator
 * @param aFamilyId          os family short name string
 * @param aFamilyDescription os family name string
 * @param aId                os short name string
 * @param aDescription       os name string
 * @param aOSType            global OS type ID
 * @param aOSHint            os configuration hint
 * @param aRAMSize           recommended RAM size in megabytes
 * @param aVRAMSize          recommended video memory size in megabytes
 * @param aHDDSize           recommended HDD size in bytes
 */
HRESULT GuestOSType::init(const Global::OSType &ostype)/*const char *aFamilyId, const char *aFamilyDescription,
                          const char *aId, const char *aDescription,
                          VBOXOSTYPE aOSType, uint32_t aOSHint,
                          uint32_t aRAMSize, uint32_t aVRAMSize, uint64_t aHDDSize,
                          NetworkAdapterType_T aNetworkAdapterType,
                          uint32_t aNumSerialEnabled,
                          StorageControllerType_T aDvdStorageControllerType,
                          StorageBus_T aDvdStorageBusType,
                          StorageControllerType_T aHdStorageControllerType,
                          StorageBus_T aHdStorageBusType,
                          ChipsetType_T aChipsetType
                          AudioControllerType_T aAudioControllerType*/
{
#if 0
    LogFlowThisFunc(("aFamilyId='%s', aFamilyDescription='%s', "
                      "aId='%s', aDescription='%s', "
                      "aType=%d, aOSHint=%x, "
                      "aRAMSize=%d, aVRAMSize=%d, aHDDSize=%lld, "
                      "aNetworkAdapterType=%d, aNumSerialEnabled=%d, "
                      "aStorageControllerType=%d\n",
                      aFamilyId, aFamilyDescription,
                      aId, aDescription,
                      aOSType, aOSHint,
                      aRAMSize, aVRAMSize, aHDDSize,
                      aNetworkAdapterType,
                      aNumSerialEnabled,
                      aStorageControllerType));
#endif

    ComAssertRet(ostype.familyId && ostype.familyDescription && ostype.id && ostype.description, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mFamilyID)                  = ostype.familyId;
    unconst(mFamilyDescription)         = ostype.familyDescription;
    unconst(mID)                        = ostype.id;
    unconst(mDescription)               = ostype.description;
    unconst(mOSType)                    = ostype.osType;
    unconst(mOSHint)                    = ostype.osHint;
    unconst(mRAMSize)                   = ostype.recommendedRAM;
    unconst(mVRAMSize)                  = ostype.recommendedVRAM;
    unconst(mHDDSize)                   = ostype.recommendedHDD;
    unconst(mNetworkAdapterType)        = ostype.networkAdapterType;
    unconst(mNumSerialEnabled)          = ostype.numSerialEnabled;
    unconst(mDvdStorageControllerType)  = ostype.dvdStorageControllerType;
    unconst(mDvdStorageBusType)         = ostype.dvdStorageBusType;
    unconst(mHdStorageControllerType)   = ostype.hdStorageControllerType;
    unconst(mHdStorageBusType)          = ostype.hdStorageBusType;
    unconst(mChipsetType)               = ostype.chipsetType;
    unconst(mAudioControllerType)       = ostype.audioControllerType;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void GuestOSType::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

// IGuestOSType properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestOSType::COMGETTER(FamilyId)(BSTR *aFamilyId)
{
    CheckComArgOutPointerValid(aFamilyId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mFamilyID is constant during life time, no need to lock */
    mFamilyID.cloneTo(aFamilyId);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(FamilyDescription)(BSTR *aFamilyDescription)
{
    CheckComArgOutPointerValid(aFamilyDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mFamilyDescription is constant during life time, no need to lock */
    mFamilyDescription.cloneTo(aFamilyDescription);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mID is constant during life time, no need to lock */
    mID.cloneTo(aId);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mDescription is constant during life time, no need to lock */
    mDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Is64Bit)(BOOL *aIs64Bit)
{
    CheckComArgOutPointerValid(aIs64Bit);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mIs64Bit is constant during life time, no need to lock */
    *aIs64Bit = !!(mOSHint & VBOXOSHINT_64BIT);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedIOAPIC)(BOOL *aRecommendedIOAPIC)
{
    CheckComArgOutPointerValid(aRecommendedIOAPIC);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mRecommendedIOAPIC is constant during life time, no need to lock */
    *aRecommendedIOAPIC = !!(mOSHint & VBOXOSHINT_IOAPIC);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedVirtEx)(BOOL *aRecommendedVirtEx)
{
    CheckComArgOutPointerValid(aRecommendedVirtEx);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mRecommendedVirtEx is constant during life time, no need to lock */
    *aRecommendedVirtEx = !!(mOSHint & VBOXOSHINT_HWVIRTEX);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedRAM)(ULONG *aRAMSize)
{
    CheckComArgOutPointerValid(aRAMSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mRAMSize is constant during life time, no need to lock */
    *aRAMSize = mRAMSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedVRAM)(ULONG *aVRAMSize)
{
    CheckComArgOutPointerValid(aVRAMSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mVRAMSize is constant during life time, no need to lock */
    *aVRAMSize = mVRAMSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Recommended2DVideoAcceleration)(BOOL *aRecommended2DVideoAcceleration)
{
    CheckComArgOutPointerValid(aRecommended2DVideoAcceleration);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Constant during life time, no need to lock */
    *aRecommended2DVideoAcceleration = !!(mOSHint & VBOXOSHINT_ACCEL2D);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Recommended3DAcceleration)(BOOL *aRecommended3DAcceleration)
{
    CheckComArgOutPointerValid(aRecommended3DAcceleration);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Constant during life time, no need to lock */
    *aRecommended3DAcceleration = !!(mOSHint & VBOXOSHINT_ACCEL3D);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedHDD)(LONG64 *aHDDSize)
{
    CheckComArgOutPointerValid(aHDDSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mHDDSize is constant during life time, no need to lock */
    *aHDDSize = mHDDSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(AdapterType)(NetworkAdapterType_T *aNetworkAdapterType)
{
    CheckComArgOutPointerValid(aNetworkAdapterType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mNetworkAdapterType is constant during life time, no need to lock */
    *aNetworkAdapterType = mNetworkAdapterType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedPae)(BOOL *aRecommendedPae)
{
    CheckComArgOutPointerValid(aRecommendedPae);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* recommended PAE is constant during life time, no need to lock */
    *aRecommendedPae = !!(mOSHint & VBOXOSHINT_PAE);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedFirmware)(FirmwareType_T *aFirmwareType)
{
    CheckComArgOutPointerValid(aFirmwareType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* firmware type is constant during life time, no need to lock */
    if (mOSHint & VBOXOSHINT_EFI)
        *aFirmwareType = FirmwareType_EFI;
    else
        *aFirmwareType = FirmwareType_BIOS;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedDvdStorageController)(StorageControllerType_T * aStorageControllerType)
{
    CheckComArgOutPointerValid(aStorageControllerType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* storage controller type is constant during life time, no need to lock */
    *aStorageControllerType = mDvdStorageControllerType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedDvdStorageBus)(StorageBus_T * aStorageBusType)
{
    CheckComArgOutPointerValid(aStorageBusType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* storage controller type is constant during life time, no need to lock */
    *aStorageBusType = mDvdStorageBusType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedHdStorageController)(StorageControllerType_T * aStorageControllerType)
{
    CheckComArgOutPointerValid(aStorageControllerType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* storage controller type is constant during life time, no need to lock */
    *aStorageControllerType = mHdStorageControllerType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedHdStorageBus)(StorageBus_T * aStorageBusType)
{
    CheckComArgOutPointerValid(aStorageBusType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* storage controller type is constant during life time, no need to lock */
    *aStorageBusType = mHdStorageBusType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedUsbHid)(BOOL *aRecommendedUsbHid)
{
    CheckComArgOutPointerValid(aRecommendedUsbHid);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* HID type is constant during life time, no need to lock */
    *aRecommendedUsbHid = !!(mOSHint & VBOXOSHINT_USBHID);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedHpet)(BOOL *aRecommendedHpet)
{
    CheckComArgOutPointerValid(aRecommendedHpet);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* HPET recommendation is constant during life time, no need to lock */
    *aRecommendedHpet = !!(mOSHint & VBOXOSHINT_HPET);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedUsbTablet)(BOOL *aRecommendedUsbTablet)
{
    CheckComArgOutPointerValid(aRecommendedUsbTablet);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* HID type is constant during life time, no need to lock */
    *aRecommendedUsbTablet = !!(mOSHint & VBOXOSHINT_USBTABLET);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedRtcUseUtc)(BOOL *aRecommendedRtcUseUtc)
{
    CheckComArgOutPointerValid(aRecommendedRtcUseUtc);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Value is constant during life time, no need to lock */
    *aRecommendedRtcUseUtc = !!(mOSHint & VBOXOSHINT_RTCUTC);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedChipset) (ChipsetType_T *aChipsetType)
{
    CheckComArgOutPointerValid(aChipsetType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* chipset type is constant during life time, no need to lock */
    *aChipsetType = mChipsetType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedAudioController) (AudioControllerType_T *aAudioController)
{
    CheckComArgOutPointerValid(aAudioController);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aAudioController = mAudioControllerType;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedFloppy)(BOOL *aRecommendedFloppy)
{
    CheckComArgOutPointerValid(aRecommendedFloppy);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Value is constant during life time, no need to lock */
    *aRecommendedFloppy = !!(mOSHint & VBOXOSHINT_FLOPPY);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedUsb)(BOOL *aRecommendedUsb)
{
    CheckComArgOutPointerValid(aRecommendedUsb);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Value is constant during life time, no need to lock */
    *aRecommendedUsb = !(mOSHint & VBOXOSHINT_NOUSB);

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
