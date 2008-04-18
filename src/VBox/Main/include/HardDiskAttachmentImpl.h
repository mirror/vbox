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
    HRESULT init (HardDisk *aHD, StorageBus_T aBus, LONG aChannel, LONG aDevice, BOOL aDirty);

    // IHardDiskAttachment properties
    STDMETHOD(COMGETTER(HardDisk))   (IHardDisk **aHardDisk);
    STDMETHOD(COMGETTER(Bus))        (StorageBus_T *aBus);
    STDMETHOD(COMGETTER(Channel))    (LONG *aChannel);
    STDMETHOD(COMGETTER(Device))     (LONG *aDevice);

    // public methods for internal purposes only
    // (ensure there is a caller and a read or write lock before calling them!)

    BOOL isDirty() const { return mDirty; }
    void setDirty (BOOL aDirty) { mDirty = aDirty; }

    const ComObjPtr <HardDisk> &hardDisk() const { return mHardDisk; }
    StorageBus_T bus() const { return mBus; }
    LONG channel() const { return mChannel; }
    LONG device() const { return mDevice; }

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
    StorageBus_T mBus;
    LONG mChannel;
    LONG mDevice;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (HardDiskAttachment)

#endif // ____H_HARDDISKATTACHMENTIMPL
