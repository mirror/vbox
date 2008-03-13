/* $Id$ */

/** @file
 *
 * VBox SATAController COM Class declaration.
 */

/*
 * Copyright (C) 2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_SATACONTROLLERIMPL
#define ____H_SATACONTROLLERIMPL

#include "VirtualBoxBase.h"

#include <list>

class Machine;

class ATL_NO_VTABLE SATAController :
    public VirtualBoxBaseWithChildrenNEXT,
    public VirtualBoxSupportErrorInfoImpl <SATAController, ISATAController>,
    public VirtualBoxSupportTranslation <SATAController>,
    public ISATAController
{
private:

    struct Data
    {
        /* Constructor. */
        Data() : mEnabled (FALSE),
                 mPortIde0Master (0),
                 mPortIde0Slave (1),
                 mPortIde1Master (2),
                 mPortIde1Slave (3) { }

        bool operator== (const Data &that) const
        {
            return this == &that || ((mEnabled == that.mEnabled) &&
                    (mPortIde0Master == that.mPortIde0Master) &&
                    (mPortIde0Slave  == that.mPortIde0Slave)  &&
                    (mPortIde1Master == that.mPortIde1Master) &&
                    (mPortIde1Slave  == that.mPortIde1Slave));
        }

        /** Enabled indicator. */
        BOOL  mEnabled;
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

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (SATAController)

    DECLARE_NOT_AGGREGATABLE (SATAController)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SATAController)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (ISATAController)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (SATAController)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent);
    HRESULT init (Machine *aParent, SATAController *aThat);
    HRESULT initCopy (Machine *aParent, SATAController *aThat);
    void uninit();

    // ISATAController properties
    STDMETHOD(COMGETTER(Enabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL aEnabled);

    // ISATAController methods
    STDMETHOD(GetIDEEmulationPort) (LONG DevicePosition, LONG *aPortNumber);
    STDMETHOD(SetIDEEmulationPort) (LONG DevicePosition, LONG aPortNumber);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aMachineNode);
    HRESULT saveSettings (settings::Key &aMachineNode);

    bool isModified();
    bool isReallyModified();
    bool rollback();
    void commit();
    void copyFrom (SATAController *aThat);

    HRESULT onMachineRegistered (BOOL aRegistered);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    /** @note this doesn't require a read lock since mParent is constant. */
    const ComObjPtr <Machine, ComWeakRef> &parent() { return mParent; };

    const Backupable<Data> &data() { return mData; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SATAController"; }

private:

    void printList();

    /** Parent object. */
    const ComObjPtr<Machine, ComWeakRef> mParent;
    /** Peer object. */
    const ComObjPtr <SATAController> mPeer;
    /** Data. */
    Backupable <Data> mData;

};

#endif //!____H_SATACONTROLLERIMPL
