/* $Id$ */

/** @file
 *
 * VBox StorageController COM Class declaration.
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_STORAGECONTROLLERIMPL
#define ____H_STORAGECONTROLLERIMPL

#include "VirtualBoxBase.h"

class ATL_NO_VTABLE StorageController :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IStorageController)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(StorageController, IStorageController)

    DECLARE_NOT_AGGREGATABLE (StorageController)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(StorageController)
        VBOX_DEFAULT_INTERFACE_ENTRIES (IStorageController)
    END_COM_MAP()

    StorageController() { };
    ~StorageController() { };

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent,
                 const Utf8Str &aName,
                 StorageBus_T aBus,
                 ULONG aInstance,
                 bool fBootable);
    HRESULT init(Machine *aParent,
                 StorageController *aThat,
                 bool aReshare = false);
    HRESULT initCopy(Machine *aParent,
                     StorageController *aThat);
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
    STDMETHOD(COMGETTER(UseHostIOCache)) (BOOL *fUseHostIOCache);
    STDMETHOD(COMSETTER(UseHostIOCache)) (BOOL fUseHostIOCache);
    STDMETHOD(COMGETTER(Bootable)) (BOOL *fBootable);

    // public methods only for internal purposes

    const Utf8Str &getName() const;
    StorageControllerType_T getControllerType() const;
    StorageBus_T getStorageBus() const;
    ULONG getInstance() const;
    bool getBootable() const;

    HRESULT checkPortAndDeviceValid(LONG aControllerPort,
                                    LONG aDevice);

    void setBootable(BOOL fBootable);

    void rollback();
    void commit();
    HRESULT getIDEEmulationPort (LONG DevicePosition, LONG *aPortNumber);
    HRESULT setIDEEmulationPort (LONG DevicePosition, LONG aPortNumber);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    void unshare();

    /** @note this doesn't require a read lock since mParent is constant. */
    Machine* getMachine();

    ComObjPtr<StorageController> getPeer();

private:

    void printList();

    struct Data;
    Data *m;
};

#endif //!____H_STORAGECONTROLLERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
