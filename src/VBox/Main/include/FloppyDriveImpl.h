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

#ifndef ____H_FLOPPYDRIVEIMPL
#define ____H_FLOPPYDRIVEIMPL

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE FloppyDrive :
    public VirtualBoxSupportErrorInfoImpl <FloppyDrive, IFloppyDrive>,
    public VirtualBoxSupportTranslation <FloppyDrive>,
    public VirtualBoxBase,
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

    DECLARE_NOT_AGGREGATABLE(FloppyDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(FloppyDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IFloppyDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *parent);
    HRESULT init (Machine *parent, FloppyDrive *that);
    HRESULT initCopy (Machine *parent, FloppyDrive *that);
    void uninit();

    // IFloppyDrive properties
    STDMETHOD(COMGETTER(Enabled)) (BOOL *enabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL enabled);
    STDMETHOD(COMGETTER(State)) (DriveState_T *driveState);

    // IFloppyDrive methods
    STDMETHOD(MountImage)(INPTR GUIDPARAM imageId);
    STDMETHOD(CaptureHostDrive)(IHostFloppyDrive *hostFloppyDrive);
    STDMETHOD(Unmount)();
    STDMETHOD(GetImage)(IFloppyImage **floppyImage);
    STDMETHOD(GetHostDrive)(IHostFloppyDrive **hostFloppyDrive);

    // public methods only for internal purposes

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (FloppyDrive *aThat);

    Backupable <Data> &data() { return mData; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"FloppyDrive"; }

private:

    HRESULT unmount();

    ComObjPtr <Machine, ComWeakRef> mParent;
    ComObjPtr <FloppyDrive> mPeer;
    Backupable <Data> mData;
};

#endif // ____H_FLOPPYDRIVEIMPL
