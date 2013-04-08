
/* $Id$  */
/** @file
 * VirtualBox Main - Guest error information.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTERRORINFOIMPL
#define ____H_GUESTERRORINFOIMPL

#include "VirtualBoxBase.h"
#include "VirtualBoxErrorInfoImpl.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestErrorInfo :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestErrorInfo)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestErrorInfo, IGuestErrorInfo)
    DECLARE_NOT_AGGREGATABLE(GuestErrorInfo)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestErrorInfo)
        COM_INTERFACE_ENTRY(IGuestErrorInfo)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestErrorInfo)

    int     init(LONG uResult, const Utf8Str &strMessage);

    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IGuestErrorInfo interface.
     * @{ */
    STDMETHOD(COMGETTER(Result))(LONG *aResult);
    STDMETHOD(COMGETTER(Text))(BSTR *aText);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:
    /** @name Private internal methods.
     * @{ */
    /** @}  */

    struct Data
    {
        ULONG   mResult;
        Utf8Str mText;
    } mData;
};

#endif /* !____H_GUESTERRORINFOIMPL */

