/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ____H_VFSEXPLORERIMPL
#define ____H_VFSEXPLORERIMPL

#include "VirtualBoxBase.h"

class VirtualBox;

class ATL_NO_VTABLE VFSExplorer :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<VFSExplorer, IVFSExplorer>,
    public VirtualBoxSupportTranslation<VFSExplorer>,
    VBOX_SCRIPTABLE_IMPL(IVFSExplorer)
{
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (VFSExplorer)

    DECLARE_NOT_AGGREGATABLE(VFSExplorer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VFSExplorer)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVFSExplorer)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(VFSExplorer)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

    HRESULT init(VFSType_T aType, Utf8Str aFilePath, Utf8Str aHostname, Utf8Str aUsername, Utf8Str aPassword, VirtualBox *aVirtualBox);
    void uninit();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"VFSExplorer"; }

    /* IVFSExplorer properties */
    STDMETHOD(COMGETTER(Path))(BSTR *aPath);
    STDMETHOD(COMGETTER(Type))(VFSType_T *aType);

    /* IVFSExplorer methods */
    STDMETHOD(Update)(IProgress **aProgress);

    STDMETHOD(Cd)(IN_BSTR aDir, IProgress **aProgress);
    STDMETHOD(CdUp)(IProgress **aProgress);

    STDMETHOD(EntryList)(ComSafeArrayOut(BSTR, aNames), ComSafeArrayOut(VFSFileType_T, aTypes));

    STDMETHOD(Exists)(ComSafeArrayIn(IN_BSTR, aNames), ComSafeArrayOut(BSTR, aExists));

    STDMETHOD(Remove)(ComSafeArrayIn(IN_BSTR, aNames), IProgress **aProgress);

private:
    /* Private member vars */
    const ComObjPtr<VirtualBox, ComWeakRef> mVirtualBox;

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

