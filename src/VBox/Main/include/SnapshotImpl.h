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
 */

#ifndef ____H_SNAPSHOTIMPL
#define ____H_SNAPSHOTIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include <list>

class SnapshotMachine;

class ATL_NO_VTABLE Snapshot :
    public VirtualBoxSupportErrorInfoImpl <Snapshot, ISnapshot>,
    public VirtualBoxSupportTranslation <Snapshot>,
    public VirtualBoxBaseWithTypedChildren <Snapshot>,
    public ISnapshot
{
public:

    struct Data
    {
        Data();
        ~Data();

        Guid mId;
        Bstr mName;
        Bstr mDescription;
        LONG64 mTimeStamp;
        ComObjPtr <SnapshotMachine> mMachine;
    };

    typedef VirtualBoxBaseWithTypedChildren <Snapshot>::DependentChildren
        SnapshotList;

    DECLARE_NOT_AGGREGATABLE(Snapshot)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Snapshot)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ISnapshot)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer only for internal purposes
    HRESULT init (const Guid &aId, INPTR BSTR aName, INPTR BSTR aDescription,
                  LONG64 aTimeStamp, SnapshotMachine *aMachine,
                  Snapshot *aParent);
    void uninit();

    void discard();

    // ISnapshot properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT aId);
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMSETTER(Name)) (INPTR BSTR aName);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (INPTR BSTR aDescription);
    STDMETHOD(COMGETTER(TimeStamp)) (LONG64 *aTimeStamp);
    STDMETHOD(COMGETTER(Online)) (BOOL *aOnline);
    STDMETHOD(COMGETTER(Machine)) (IMachine **aMachine);
    STDMETHOD(COMGETTER(Parent)) (ISnapshot **aParent);
    STDMETHOD(COMGETTER(Children)) (ISnapshotCollection **aChildren);

    // ISnapshot methods

    // public methods only for internal purposes

    /** Do |AutoLock alock (this);| before acceessing the returned data! */
    const Data &data() const { return mData; }

    const Bstr &stateFilePath() const;

    ComObjPtr <Snapshot> parent() const { return (Snapshot *) mParent; }

    /** Shortcut to #dependentChildrenLock() */
    AutoLock::Handle &childrenLock() const { return dependentChildrenLock(); }

    /**
     *  Shortcut to #dependentChildren().
     *  Do |AutoLock alock (childrenLock());| before acceessing the returned list!
     */
    const SnapshotList &children() const { return dependentChildren(); }

    ULONG descendantCount();
    ComObjPtr <Snapshot> findChildOrSelf (INPTR GUIDPARAM aId);
    ComObjPtr <Snapshot> findChildOrSelf (INPTR BSTR aName);

    bool isDVDImageUsed (const Guid &aId);
    bool isFloppyImageUsed (const Guid &aId);

    void updateSavedStatePaths (const char *aOldPath, const char *aNewPath);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Snapshot"; }

private:

    ComObjPtr <Snapshot, ComWeakRef> mParent;

    Data mData;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION (Snapshot)

#endif // ____H_SNAPSHOTIMPL

