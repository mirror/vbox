/* $Id$ */

/** @file
 *
 * VBox StorageController COM Class declaration.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ____H_STORAGECONTROLLERIMPL
#define ____H_STORAGECONTROLLERIMPL

#include "VirtualBoxBase.h"

#include <list>

class Machine;

class ATL_NO_VTABLE StorageController :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <StorageController, IStorageController>,
    public VirtualBoxSupportTranslation <StorageController>,
    VBOX_SCRIPTABLE_IMPL(IStorageController)
{
private:

    struct Data
    {
        /* Constructor. */
        Data() : mStorageBus (StorageBus_IDE),
                 mStorageControllerType (StorageControllerType_PIIX4),
                 mPortCount (2),
                 mPortIde0Master (0),
                 mPortIde0Slave (1),
                 mPortIde1Master (2),
                 mPortIde1Slave (3) { }

        bool operator== (const Data &that) const
        {
            return this == &that || ((mStorageControllerType == that.mStorageControllerType) &&
                    (mName           == that.mName) &&
                    (mPortCount   == that.mPortCount) &&
                    (mPortIde0Master == that.mPortIde0Master) &&
                    (mPortIde0Slave  == that.mPortIde0Slave)  &&
                    (mPortIde1Master == that.mPortIde1Master) &&
                    (mPortIde1Slave  == that.mPortIde1Slave));
        }

        /** Uniuqe name of the storage controller. */
        Bstr mName;
        /** The connection type of thestorage controller. */
        StorageBus_T mStorageBus;
        /** Type of the Storage controller. */
        StorageControllerType_T mStorageControllerType;
        /** Number of usable ports. */
        ULONG mPortCount;

        /** The following is only for the SATA controller atm. */
        /** Port which acts as primary master for ide emulation. */
        ULONG mPortIde0Master;
        /** Port which acts as primary slave for ide emulation. */
        ULONG mPortIde0Slave;
        /** Port which acts as secondary master for ide emulation. */
        ULONG mPortIde1Master;
        /** Port which acts as secondary slave for ide emulation. */
        ULONG mPortIde1Slave;
    };

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (StorageController)

    DECLARE_NOT_AGGREGATABLE (StorageController)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(StorageController)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (IStorageController)
        COM_INTERFACE_ENTRY2 (IDispatch, IStorageController)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (StorageController)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent, IN_BSTR aName,
                  StorageBus_T aBus);
    HRESULT init (Machine *aParent, StorageController *aThat, bool aReshare = false);
    HRESULT initCopy (Machine *aParent, StorageController *aThat);
    void uninit();

    // IStorageController properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(Bus)) (StorageBus_T *aBus);
    STDMETHOD(COMGETTER(ControllerType)) (StorageControllerType_T *aControllerType);
    STDMETHOD(COMSETTER(ControllerType)) (StorageControllerType_T aControllerType);
    STDMETHOD(COMGETTER(MaxDevicesPerPortCount)) (ULONG *aMaxDevices);
    STDMETHOD(COMGETTER(MinPortCount)) (ULONG *aMinPortCount);
    STDMETHOD(COMGETTER(MaxPortCount)) (ULONG *aMaxPortCount);
    STDMETHOD(COMGETTER(PortCount)) (ULONG *aPortCount);
    STDMETHOD(COMSETTER(PortCount)) (ULONG aPortCount);
    STDMETHOD(COMGETTER(Instance)) (ULONG *aInstance);
    STDMETHOD(COMSETTER(Instance)) (ULONG aInstance);

    // StorageController methods
    STDMETHOD(GetIDEEmulationPort) (LONG DevicePosition, LONG *aPortNumber);
    STDMETHOD(SetIDEEmulationPort) (LONG DevicePosition, LONG aPortNumber);

    // public methods only for internal purposes

    const Bstr &name() const { return mData->mName; }
    StorageControllerType_T controllerType() const { return mData->mStorageControllerType; }

    bool isModified() { AutoWriteLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoWriteLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    void unshare();

    /** @note this doesn't require a read lock since mParent is constant. */
    const ComObjPtr <Machine, ComWeakRef> &parent() { return mParent; };

    const Backupable<Data> &data() { return mData; }
    ComObjPtr <StorageController> peer() { return mPeer; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"StorageController"; }

private:

    void printList();

    /** Parent object. */
    const ComObjPtr<Machine, ComWeakRef> mParent;
    /** Peer object. */
    const ComObjPtr <StorageController> mPeer;
    /** Data. */
    Backupable <Data> mData;

    /* Instance number of the device in the running VM. */
    ULONG mInstance;
};

#endif //!____H_STORAGECONTROLLERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
