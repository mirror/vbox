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

#ifndef ____H_FLOPPYDRIVEIMPL
#define ____H_FLOPPYDRIVEIMPL

#include "VirtualBoxBase.h"

#include "MediumImpl.h"

class Machine;

class ATL_NO_VTABLE FloppyDrive :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <FloppyDrive, IFloppyDrive>,
    public VirtualBoxSupportTranslation <FloppyDrive>,
    VBOX_SCRIPTABLE_IMPL(IFloppyDrive)
{
public:

    struct Data
    {
        Data()
        {
            enabled = true;
            state = DriveState_NotMounted;
        }

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (state == that.state &&
                    image.equalsTo (that.image) &&
                    hostDrive.equalsTo (that.hostDrive));
        }

        BOOL enabled;
        ComObjPtr<FloppyImage> image;
        ComPtr <IHostFloppyDrive> hostDrive;
        DriveState_T state;
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
    STDMETHOD(COMGETTER(State)) (DriveState_T *aState);

    // IFloppyDrive methods
    STDMETHOD(MountImage) (IN_GUID aImageId);
    STDMETHOD(CaptureHostDrive) (IHostFloppyDrive *aHostFloppyDrive);
    STDMETHOD(Unmount)();
    STDMETHOD(GetImage) (IFloppyImage **aFloppyImage);
    STDMETHOD(GetHostDrive) (IHostFloppyDrive **aHostFloppyDrive);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aMachineNode);
    HRESULT saveSettings (settings::Key &aMachineNode);

    bool isModified() { AutoWriteLock alock (this); return m.isBackedUp(); }
    bool isReallyModified() { AutoWriteLock alock (this); return m.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (FloppyDrive *aThat);

    HRESULT unmount();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    Backupable <Data> &data() { return m; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"FloppyDrive"; }

private:

    const ComObjPtr <Machine, ComWeakRef> mParent;
    const ComObjPtr <FloppyDrive> mPeer;

    Backupable <Data> m;
};

#endif // ____H_FLOPPYDRIVEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
