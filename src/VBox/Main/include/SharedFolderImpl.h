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

#ifndef ____H_SHAREDFOLDERIMPL
#define ____H_SHAREDFOLDERIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"
#include <VBox/shflsvc.h>

class Machine;
class Console;
class VirtualBox;

class ATL_NO_VTABLE SharedFolder :
    public VirtualBoxSupportErrorInfoImpl <SharedFolder, ISharedFolder>,
    public VirtualBoxSupportTranslation <SharedFolder>,
    public VirtualBoxBase,
    public ISharedFolder
{
public:

    // to satisfy the ComObjPtr template (we have const members)
    SharedFolder() {}

    DECLARE_NOT_AGGREGATABLE(SharedFolder)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SharedFolder)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(ISharedFolder)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aMachine, const BSTR aName, const BSTR aHostPath);
    HRESULT initCopy (Machine *aMachine, SharedFolder *aThat);
    HRESULT init (Console *aConsole, const BSTR aName, const BSTR aHostPath);
    HRESULT init (VirtualBox *aVirtualBox, const BSTR aName, const BSTR aHostPath);
    void uninit();

    // ISharedFolder properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(HostPath)) (BSTR *aHostPath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);

    // public methods for internal purposes only

    const Bstr &name() const { return mName; }
    const Bstr &hostPath() const { return mHostPath; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SharedFolder"; }

protected:

    HRESULT protectedInit (VirtualBoxBaseWithChildren *aParent,
                           const BSTR aName, const BSTR aHostPath);

private:

    VirtualBoxBaseWithChildren *mParent;

    /* weak parents (only one of them is not null) */
    ComObjPtr <Machine, ComWeakRef> mMachine;
    ComObjPtr <Console, ComWeakRef> mConsole;
    ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    const Bstr mName;
    const Bstr mHostPath;
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
                "Shared folder named '%ls' could not be found"), aName);

        return found.queryInterfaceTo (aSharedFolder);
    }

COM_DECL_READONLY_ENUM_AND_COLLECTION_END (SharedFolder)

#endif // ____H_SHAREDFOLDERIMPL
