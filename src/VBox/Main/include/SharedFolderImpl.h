/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include <VBox/shflsvc.h>

class Console;

class ATL_NO_VTABLE SharedFolder :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(ISharedFolder)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(SharedFolder, ISharedFolder)

    DECLARE_NOT_AGGREGATABLE(SharedFolder)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SharedFolder)
        VBOX_DEFAULT_INTERFACE_ENTRIES  (ISharedFolder)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (SharedFolder)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aMachine, const Utf8Str &aName, const Utf8Str &aHostPath, bool aWritable, bool aAutoMount, bool fFailOnError);
    HRESULT initCopy(Machine *aMachine, SharedFolder *aThat);
    HRESULT init(Console *aConsole, const Utf8Str &aName, const Utf8Str &aHostPath, bool aWritable, bool aAutoMount, bool fFailOnError);
//     HRESULT init(VirtualBox *aVirtualBox, const Utf8Str &aName, const Utf8Str &aHostPath, bool aWritable, bool aAutoMount, bool fFailOnError);
    void uninit();

    // ISharedFolder properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(HostPath)) (BSTR *aHostPath);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);
    STDMETHOD(COMGETTER(Writable)) (BOOL *aWritable);
    STDMETHOD(COMGETTER(AutoMount)) (BOOL *aAutoMount);
    STDMETHOD(COMGETTER(LastAccessError)) (BSTR *aLastAccessError);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

    /**
     * Public internal method. Returns the shared folder's name. Needs caller! Locking not necessary.
     * @return
     */
    const Utf8Str& getName() const;

    /**
     * Public internal method. Returns the shared folder's host path. Needs caller! Locking not necessary.
     * @return
     */
    const Utf8Str& getHostPath() const;

    /**
     * Public internal method. Returns true if the shared folder is writable. Needs caller and locking!
     * @return
     */
    bool isWritable() const;

    /**
     * Public internal method. Returns true if the shared folder is auto-mounted. Needs caller and locking!
     * @return
     */
    bool isAutoMounted() const;

protected:

    HRESULT protectedInit(VirtualBoxBase *aParent,
                          const Utf8Str &aName,
                          const Utf8Str &aHostPath,
                          bool aWritable,
                          bool aAutoMount,
                          bool fFailOnError);
private:

    VirtualBoxBase * const  mParent;

    /* weak parents (only one of them is not null) */
    Machine * const         mMachine;
    Console * const         mConsole;
    VirtualBox * const      mVirtualBox;

    struct Data;            // opaque data struct, defined in SharedFolderImpl.cpp
    Data *m;
};

#endif // ____H_SHAREDFOLDERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
