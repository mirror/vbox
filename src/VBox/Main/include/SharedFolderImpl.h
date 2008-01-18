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

#ifndef ____H_SHAREDFOLDERIMPL
#define ____H_SHAREDFOLDERIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"
#include <VBox/shflsvc.h>

class Machine;
class Console;
class VirtualBox;

class ATL_NO_VTABLE SharedFolder :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <SharedFolder, ISharedFolder>,
    public VirtualBoxSupportTranslation <SharedFolder>,
    public ISharedFolder
{
public:

    struct Data
    {
        Data() {}

        const Bstr mName;
        const Bstr mHostPath;
        BOOL       mWritable;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (SharedFolder)

    DECLARE_NOT_AGGREGATABLE(SharedFolder)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SharedFolder)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ISharedFolder)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (SharedFolder)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aMachine, const BSTR aName, const BSTR aHostPath, BOOL aWritable);
    HRESULT initCopy (Machine *aMachine, SharedFolder *aThat);
    HRESULT init (Console *aConsole, const BSTR aName, const BSTR aHostPath, BOOL aWritable);
    HRESULT init (VirtualBox *aVirtualBox, const BSTR aName, const BSTR aHostPath, BOOL aWritable);
    void uninit();

    // ISharedFolder properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(HostPath)) (BSTR *aHostPath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);
    STDMETHOD(COMGETTER(Writable)) (BOOL *aWritable);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    // public methods that don't need a lock (because access constant data)
    // (ensure there is a caller added before calling them!)

    const Bstr &name() const { return mData.mName; }
    const Bstr &hostPath() const { return mData.mHostPath; }
    BOOL writable() const { return mData.mWritable; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SharedFolder"; }

protected:

    HRESULT protectedInit (VirtualBoxBaseWithChildrenNEXT *aParent,
                           const BSTR aName, const BSTR aHostPath, BOOL aWritable);

private:

    VirtualBoxBaseWithChildrenNEXT *const mParent;

    /* weak parents (only one of them is not null) */
    const ComObjPtr <Machine, ComWeakRef> mMachine;
    const ComObjPtr <Console, ComWeakRef> mConsole;
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    Data mData;
};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (SharedFolder)

    STDMETHOD(FindByName) (INPTR BSTR aName, ISharedFolder **aSharedFolder)
    {
        if (!aName)
            return E_INVALIDARG;
        if (!aSharedFolder)
            return E_POINTER;

        *aSharedFolder = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr name;
            (*it)->COMGETTER(Name) (name.asOutParam());
            if (name == aName)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, SharedFolderCollection::tr (
                "Could not find the shared folder '%ls'"), aName);

        return found.queryInterfaceTo (aSharedFolder);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (SharedFolder)

#endif // ____H_SHAREDFOLDERIMPL
