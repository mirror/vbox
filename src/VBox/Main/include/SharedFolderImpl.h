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

#ifndef ____H_SHAREDFOLDERIMPL
#define ____H_SHAREDFOLDERIMPL

#include "VirtualBoxBase.h"
#include <VBox/shflsvc.h>

class Machine;
class Console;
class VirtualBox;

class ATL_NO_VTABLE SharedFolder :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <SharedFolder, ISharedFolder>,
    public VirtualBoxSupportTranslation <SharedFolder>,
    VBOX_SCRIPTABLE_IMPL(ISharedFolder)
{
public:

    struct Data
    {
        Data() {}

        const Bstr name;
        const Bstr hostPath;
        BOOL       writable;
        Bstr       lastAccessError;
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
    HRESULT init (Machine *aMachine, CBSTR aName, CBSTR aHostPath, BOOL aWritable);
    HRESULT initCopy (Machine *aMachine, SharedFolder *aThat);
    HRESULT init (Console *aConsole, CBSTR aName, CBSTR aHostPath, BOOL aWritable);
    HRESULT init (VirtualBox *aVirtualBox, CBSTR aName, CBSTR aHostPath, BOOL aWritable);
    void uninit();

    // ISharedFolder properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(HostPath)) (BSTR *aHostPath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);
    STDMETHOD(COMGETTER(Writable)) (BOOL *aWritable);
    STDMETHOD(COMGETTER(LastAccessError)) (BSTR *aLastAccessError);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    // public methods that don't need a lock (because access constant data)
    // (ensure there is a caller added before calling them!)

    const Bstr &name() const { return m.name; }
    const Bstr &hostPath() const { return m.hostPath; }
    BOOL writable() const { return m.writable; }

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"SharedFolder"; }

protected:

    HRESULT protectedInit (VirtualBoxBaseWithChildrenNEXT *aParent,
                           CBSTR aName, CBSTR aHostPath, BOOL aWritable);

private:

    VirtualBoxBaseWithChildrenNEXT *const mParent;

    /* weak parents (only one of them is not null) */
    const ComObjPtr <Machine, ComWeakRef> mMachine;
    const ComObjPtr <Console, ComWeakRef> mConsole;
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    Data m;
};

#endif // ____H_SHAREDFOLDERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
