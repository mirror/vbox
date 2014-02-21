
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
#include "GuestProcessImpl.h"

class GuestSession;

/**
 * TODO
 */
class ATL_NO_VTABLE GuestDirectory :
    public VirtualBoxBase,
    public GuestObject,
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

    int     init(Console *pConsole, GuestSession *pSession, ULONG uDirID, const GuestDirectoryOpenInfo &openInfo);
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
    int            callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    static Utf8Str guestErrorToString(int guestRc);
    int            onRemove(void);
    static HRESULT setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    /** @}  */

private:

    /** @name Private internal methods.
     * @{ */
    /** @}  */

    struct Data
    {
        /** The directory's open info. */
        GuestDirectoryOpenInfo     mOpenInfo;
        /** The directory's ID. */
        uint32_t                   mID;
        GuestProcessTool           mProcessTool;
    } mData;
};

#endif /* !____H_GUESTDIRECTORYIMPL */

