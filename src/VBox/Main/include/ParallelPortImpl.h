/* $Id$ */

/** @file
 * VirtualBox COM class implementation.
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

#ifndef ____H_PARALLELPORTIMPL
#define ____H_PARALLELPORTIMPL

#include "VirtualBoxBase.h"

class Machine;

class ATL_NO_VTABLE ParallelPort :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <ParallelPort, IParallelPort>,
    public VirtualBoxSupportTranslation <ParallelPort>,
    VBOX_SCRIPTABLE_IMPL(IParallelPort)
{
public:

    struct Data
    {
        Data()
            : mSlot (0)
            , mEnabled (FALSE)
            , mIRQ (4)
            , mIOBase (0x378)
        {}

        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mSlot == that.mSlot &&
                    mEnabled == that.mEnabled &&
                    mIRQ == that.mIRQ &&
                    mIOBase == that.mIOBase &&
                    mPath == that.mPath);
        }

        ULONG mSlot;
        BOOL  mEnabled;
        ULONG mIRQ;
        ULONG mIOBase;
        Bstr  mPath;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (ParallelPort)

    DECLARE_NOT_AGGREGATABLE(ParallelPort)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(ParallelPort)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (IParallelPort)
        COM_INTERFACE_ENTRY2 (IDispatch, IParallelPort)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (ParallelPort)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent, ULONG aSlot);
    HRESULT init (Machine *aParent, ParallelPort *aThat);
    HRESULT initCopy (Machine *parent, ParallelPort *aThat);
    void uninit();

    // IParallelPort properties
    STDMETHOD(COMGETTER(Slot))    (ULONG     *aSlot);
    STDMETHOD(COMGETTER(Enabled)) (BOOL      *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL       aEnabled);
    STDMETHOD(COMGETTER(IRQ))     (ULONG     *aIRQ);
    STDMETHOD(COMSETTER(IRQ))     (ULONG      aIRQ);
    STDMETHOD(COMGETTER(IOBase))  (ULONG     *aIOBase);
    STDMETHOD(COMSETTER(IOBase))  (ULONG      aIOBase);
    STDMETHOD(COMGETTER(Path))    (BSTR      *aPath);
    STDMETHOD(COMSETTER(Path))    (IN_BSTR aPath);

    // public methods only for internal purposes

    HRESULT loadSettings (const settings::Key &aPortNode);
    HRESULT saveSettings (settings::Key &aPortNode);

    bool isModified() { AutoWriteLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoWriteLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (ParallelPort *aThat);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"ParallelPort"; }

private:

    HRESULT checkSetPath (CBSTR aPath);

    const ComObjPtr <Machine, ComWeakRef> mParent;
    const ComObjPtr <ParallelPort> mPeer;

    Backupable <Data> mData;
};

#endif // ____H_FLOPPYDRIVEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
