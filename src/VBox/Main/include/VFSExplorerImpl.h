/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VFSEXPLORERIMPL
#define ____H_VFSEXPLORERIMPL

#include "VirtualBoxBase.h"

class ATL_NO_VTABLE VFSExplorer :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IVFSExplorer)
{
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VFSExplorer, IVFSExplorer)

    DECLARE_NOT_AGGREGATABLE(VFSExplorer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VFSExplorer)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IVFSExplorer)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(VFSExplorer)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return BaseFinalConstruct(); }
    void FinalRelease() { uninit(); BaseFinalRelease(); }

    HRESULT init(VFSType_T aType, Utf8Str aFilePath, Utf8Str aHostname, Utf8Str aUsername, Utf8Str aPassword, VirtualBox *aVirtualBox);
    void uninit();

    /* IVFSExplorer properties */
    STDMETHOD(COMGETTER(Path))(BSTR *aPath);
    STDMETHOD(COMGETTER(Type))(VFSType_T *aType);

    /* IVFSExplorer methods */
    STDMETHOD(Update)(IProgress **aProgress);

    STDMETHOD(Cd)(IN_BSTR aDir, IProgress **aProgress);
    STDMETHOD(CdUp)(IProgress **aProgress);

    STDMETHOD(EntryList)(ComSafeArrayOut(BSTR, aNames), ComSafeArrayOut(VFSFileType_T, aTypes), ComSafeArrayOut(ULONG, aSizes), ComSafeArrayOut(ULONG, aModes));

    STDMETHOD(Exists)(ComSafeArrayIn(IN_BSTR, aNames), ComSafeArrayOut(BSTR, aExists));

    STDMETHOD(Remove)(ComSafeArrayIn(IN_BSTR, aNames), IProgress **aProgress);

    /* public methods only for internal purposes */

    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }

private:
    /* Private member vars */
    VirtualBox * const  mVirtualBox;

    struct TaskVFSExplorer;  /* Worker thread helper */
    struct Data;
    Data *m;

    /* Private member methods */
    VFSFileType_T RTToVFSFileType(int aType) const;

    HRESULT updateFS(TaskVFSExplorer *aTask);
    HRESULT deleteFS(TaskVFSExplorer *aTask);
    HRESULT updateS3(TaskVFSExplorer *aTask);
    HRESULT deleteS3(TaskVFSExplorer *aTask);
};

#endif /* ____H_VFSEXPLORERIMPL */

