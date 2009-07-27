/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef ____H_SNAPSHOTIMPL
#define ____H_SNAPSHOTIMPL

#include "VirtualBoxBase.h"

#include <iprt/time.h>

class SnapshotMachine;
class VirtualBox;
namespace settings
{
    class Key;
}

class ATL_NO_VTABLE Snapshot :
    public VirtualBoxSupportErrorInfoImpl<Snapshot, ISnapshot>,
    public VirtualBoxSupportTranslation<Snapshot>,
    public VirtualBoxBase, // WithTypedChildren<Snapshot>,
    VBOX_SCRIPTABLE_IMPL(ISnapshot)
{
public:
    DECLARE_NOT_AGGREGATABLE(Snapshot)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Snapshot)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (ISnapshot)
        COM_INTERFACE_ENTRY2 (IDispatch, ISnapshot)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    Snapshot()
        : m(NULL)
    { };
    ~Snapshot()
    { };

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer only for internal purposes
    HRESULT init(VirtualBox *aVirtualBox,
                 const Guid &aId,
                 IN_BSTR aName,
                 IN_BSTR aDescription,
                 RTTIMESPEC aTimeStamp,
                 SnapshotMachine *aMachine,
                 Snapshot *aParent);
    void uninit();

    void beginDiscard();

    // ISnapshot properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (IN_BSTR aName);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (IN_BSTR aDescription);
    STDMETHOD(COMGETTER(TimeStamp)) (LONG64 *aTimeStamp);
    STDMETHOD(COMGETTER(Online)) (BOOL *aOnline);
    STDMETHOD(COMGETTER(Machine)) (IMachine **aMachine);
    STDMETHOD(COMGETTER(Parent)) (ISnapshot **aParent);
    STDMETHOD(COMGETTER(Children)) (ComSafeArrayOut (ISnapshot *, aChildren));

    // ISnapshot methods

    // public methods only for internal purposes

    /** Do |AutoWriteLock alock (this);| before acceessing the returned data! */
//     const Data &data() const { return mData; }

    const Bstr &stateFilePath() const;

    ComObjPtr<Snapshot> parent() const
    {
        return (Snapshot*)mParent;
    }

    ULONG getGrandChildrenCount();

    ComPtr<SnapshotMachine> getSnapshotMachine();

    Guid getId() const;
    Bstr getName() const;
    RTTIMESPEC getTimeStamp() const;

    /**
     *  Shortcut to #dependentChildren().
     *  Do |AutoWriteLock alock (childrenLock());| before acceessing the returned list!
     */
//     const SnapshotList &children() const { return dependentChildren(); }

//     ULONG descendantCount();
    ComObjPtr<Snapshot> findChildOrSelf(IN_GUID aId);
    ComObjPtr<Snapshot> findChildOrSelf(IN_BSTR aName);

    void updateSavedStatePaths(const char *aOldPath,
                               const char *aNewPath);
    void updateSavedStatePathsImpl(const char *aOldPath,
                                   const char *aNewPath);

    HRESULT saveSnapshot(settings::Key &aNode, bool aAttrsOnly);
    HRESULT saveSnapshotImpl(settings::Key &aNode, bool aAttrsOnly);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName()
    {
        return L"Snapshot";
    }

private:

    /** weak VirtualBox parent */
    const ComObjPtr<VirtualBox, ComWeakRef> mVirtualBox;

    ComObjPtr<Snapshot, ComWeakRef> mParent;

    struct Data;            // opaque, defined in SnapshotImpl.cpp
    Data *m;
};

#endif // ____H_SNAPSHOTIMPL

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
