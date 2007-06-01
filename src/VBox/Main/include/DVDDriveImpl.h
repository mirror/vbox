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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_DVDDRIVEIMPL
#define ____H_DVDDRIVEIMPL

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE DVDDrive :
    public VirtualBoxSupportErrorInfoImpl <DVDDrive, IDVDDrive>,
    public VirtualBoxSupportTranslation <DVDDrive>,
    public VirtualBoxBase,
    public IDVDDrive
{
public:

    struct Data
    {
        Data() {
            mDriveState = DriveState_NotMounted;
            mPassthrough = false;
        }

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mDriveState == that.mDriveState &&
                    mDVDImage.equalsTo (that.mDVDImage) &&
                    mHostDrive.equalsTo (that.mHostDrive));
        }

        ComPtr <IDVDImage> mDVDImage;
        ComPtr <IHostDVDDrive> mHostDrive;
        DriveState_T mDriveState;
        BOOL mPassthrough;
    };

    DECLARE_NOT_AGGREGATABLE(DVDDrive)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(DVDDrive)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IDVDDrive)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *parent);
    HRESULT init (Machine *parent, DVDDrive *that);
    HRESULT initCopy (Machine *parent, DVDDrive *that);
    void uninit();

    // IDVDDrive properties
    STDMETHOD(COMGETTER(State)) (DriveState_T *driveState);
    STDMETHOD(COMGETTER(Passthrough)) (BOOL *passthrough);
    STDMETHOD(COMSETTER(Passthrough)) (BOOL passthrough);

    // IDVDDrive methods
    STDMETHOD(MountImage)(INPTR GUIDPARAM imageId);
    STDMETHOD(CaptureHostDrive)(IHostDVDDrive *hostDVDDrive);
    STDMETHOD(Unmount)();
    STDMETHOD(GetImage)(IDVDImage **dvdImage);
    STDMETHOD(GetHostDrive)(IHostDVDDrive **hostDVDDrive);

    // public methods only for internal purposes

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (DVDDrive *aThat);

    Backupable <Data> &data() { return mData; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"DVDDrive"; }

private:

    HRESULT unmount();

    ComObjPtr <Machine, ComWeakRef> mParent;
    ComObjPtr <DVDDrive> mPeer;
    Backupable <Data> mData;
};

#endif // ____H_DVDDRIVEIMPL

