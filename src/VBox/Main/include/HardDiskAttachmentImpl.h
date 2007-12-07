/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_HARDDISKATTACHMENTIMPL
#define ____H_HARDDISKATTACHMENTIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include "HardDiskImpl.h"

class ATL_NO_VTABLE HardDiskAttachment :
    public VirtualBoxSupportErrorInfoImpl <HardDiskAttachment, IHardDiskAttachment>,
    public VirtualBoxSupportTranslation <HardDiskAttachment>,
    public VirtualBoxBase,
    public IHardDiskAttachment
{
public:

    DECLARE_EMPTY_CTOR_DTOR (HardDiskAttachment)

    DECLARE_NOT_AGGREGATABLE(HardDiskAttachment)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HardDiskAttachment)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDiskAttachment)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (HardDisk *aHD, DiskControllerType_T aCtl, LONG aDev, BOOL aDirty);

    // IHardDiskAttachment properties
    STDMETHOD(COMGETTER(HardDisk)) (IHardDisk **aHardDisk);
    STDMETHOD(COMGETTER(Controller)) (DiskControllerType_T *aController);
    STDMETHOD(COMGETTER(DeviceNumber)) (LONG *aDeviceNumber);

    // public methods for internal purposes only

    BOOL isDirty() const { return mDirty; }

    const ComObjPtr <HardDisk> &hardDisk() const { return mHardDisk; }
    DiskControllerType_T controller() const { return mController; }
    LONG deviceNumber() const { return mDeviceNumber; }

    /** @note Don't forget to lock this object! */
    void updateHardDisk (const ComObjPtr <HardDisk> &aHardDisk, BOOL aDirty)
    {
        mHardDisk = aHardDisk;
        mDirty = aDirty;
    }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HardDiskAttachment"; }

private:

    BOOL mDirty;
    ComObjPtr <HardDisk> mHardDisk;
    DiskControllerType_T mController;
    LONG mDeviceNumber;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (HardDiskAttachment)

#endif // ____H_HARDDISKATTACHMENTIMPL
