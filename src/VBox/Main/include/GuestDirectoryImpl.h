/* $Id$ */
/** @file
 * VirtualBox Main - Guest directory handling implementation.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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

#include "GuestDirectoryWrap.h"
#include "GuestFsObjInfoImpl.h"
#include "GuestProcessImpl.h"

class GuestSession;

/**
 * TODO
 */
class ATL_NO_VTABLE GuestDirectory :
    public GuestDirectoryWrap,
    public GuestObject
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestDirectory)

    int     init(Console *pConsole, GuestSession *pSession, ULONG aObjectID, const GuestDirectoryOpenInfo &openInfo);
    void    uninit(void);

    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int            i_callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int            i_onRemove(void);

    int            i_closeInternal(int *pGuestRc);
    int            i_readInternal(ComObjPtr<GuestFsObjInfo> &fsObjInfo, int *pGuestRc);
    /** @}  */

public:
    /** @name Public static internal methods.
     * @{ */
    static Utf8Str i_guestErrorToString(int guestRc);
    static HRESULT i_setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    /** @}  */

private:

    /** Wrapped @name IGuestDirectory properties
     * @{ */
    HRESULT getDirectoryName(com::Utf8Str &aDirectoryName);
    HRESULT getFilter(com::Utf8Str &aFilter);
    /** @}  */

    /** Wrapped @name IGuestDirectory methods.
     * @{ */
    HRESULT close();
    HRESULT read(ComPtr<IFsObjInfo> &aObjInfo);
    /** @}  */

    struct Data
    {
        /** The directory's open info. */
        GuestDirectoryOpenInfo     mOpenInfo;
        /** The process tool instance to use. */
        GuestProcessTool           mProcessTool;
    } mData;
};

#endif /* !____H_GUESTDIRECTORYIMPL */

