/* $Id$ */

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

#ifndef ____H_FLOPPYDRIVEIMPL
#define ____H_FLOPPYDRIVEIMPL

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE FloppyDrive :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <FloppyDrive, IFloppyDrive>,
    public VirtualBoxSupportTranslation <FloppyDrive>,
    public IFloppyDrive
{
public:

    struct Data
    {
        Data()
        {
            mEnabled    = true;
            mDriveState = DriveState_NotMounted;
        }

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mDriveState == that.mDriveState &&
                    mFloppyImage.equalsTo (that.mFloppyImage) &&
                    mHostDrive.equalsTo (that.mHostDrive));
        }

        BOOL mEnabled;
        ComPtr <IFloppyImage> mFloppyImage;
        ComPtr <IHostFloppyDrive> mHostDrive;
        DriveState_T mDriveState;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (FloppyDrive)

    DECLARE_NOT_AGGREGATABLE(FloppyDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(FloppyDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IFloppyDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (FloppyDrive)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent);
    HRESULT init (Machine *aParent, FloppyDrive *aThat);
    HRESULT initCopy (Machine *parent, FloppyDrive *aThat);
    void uninit();

    // IFloppyDrive properties
    STDMETHOD(COMGETTER(Enabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL aEnabled);
    STDMETHOD(COMGETTER(State)) (DriveState_T *aDriveState);

    // IFloppyDrive methods
    STDMETHOD(MountImage) (INPTR GUIDPARAM aImageId);
    STDMETHOD(CaptureHostDrive) (IHostFloppyDrive *aHostFloppyDrive);
    STDMETHOD(Unmount)();
    STDMETHOD(GetImage) (IFloppyImage **aFloppyImage);
    STDMETHOD(GetHostDrive) (IHostFloppyDrive **aHostFloppyDrive);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aMachineNode);
    HRESULT saveSettings (settings::Key &aMachineNode);

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (FloppyDrive *aThat);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    Backupable <Data> &data() { return mData; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"FloppyDrive"; }

private:

    HRESULT unmount();

    const ComObjPtr <Machine, ComWeakRef> mParent;
    const ComObjPtr <FloppyDrive> mPeer;

    Backupable <Data> mData;
};

#endif // ____H_FLOPPYDRIVEIMPL
