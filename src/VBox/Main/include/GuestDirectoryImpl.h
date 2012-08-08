
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
#include "GuestCtrlImplPrivate.h"

class GuestProcess;
class GuestSession;

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

    int     init(GuestSession *aSession, const Utf8Str &strPath, const Utf8Str &strFilter = "", uint32_t uFlags = 0);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IDirectory interface.
     * @{ */
    STDMETHOD(COMGETTER(DirectoryName))(BSTR *aName);
    STDMETHOD(COMGETTER(Filter))(BSTR *aFilter);
    STDMETHOD(Close)(void);
    STDMETHOD(Read)(IFsObjInfo **aInfo);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    /** @name Private internal methods.
     * @{ */
    int parseData(GuestProcessStreamBlock &streamBlock);
    /** @}  */

    struct Data
    {
        GuestSession              *mParent;
        Utf8Str                    mName;
        Utf8Str                    mFilter;
        uint32_t                   mFlags;
        /** The stdout stream object which contains all
         *  read out data for parsing. Must be persisent
         *  between several read() calls. */
        GuestProcessStream         mStream;
        /** The guest process which is responsible for
         *  getting the stdout stream. */
        ComObjPtr<GuestProcess>    mProcess;
    } mData;
};

#endif /* !____H_GUESTDIRECTORYIMPL */

