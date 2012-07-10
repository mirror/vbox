
/* $Id$ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTDIRECTORYIMPL
#define ____H_GUESTDIRECTORYIMPL

#include "VirtualBoxBase.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestDirectory :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestDirectory)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestDirectory, IGuestDirectory)
    DECLARE_NOT_AGGREGATABLE(GuestDirectory)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestDirectory)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestDirectory)
        COM_INTERFACE_ENTRY(IDirectory)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestDirectory)

    HRESULT init(void);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IDirectory interface.
     * @{ */
    STDMETHOD(COMGETTER(DirectoryName))(BSTR *aName);

    STDMETHOD(Read)(IGuestFsObjInfo **aInfo);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    struct Data
    {
        Utf8Str                 mName;
        ComPtr<IGuestFsObjInfo> mFsObjInfo;
    } mData;
};

#endif /* !____H_GUESTDIRECTORYIMPL */

