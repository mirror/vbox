
/* $Id$ */
/** @file
 * VirtualBox Main - Guest file handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTFILEIMPL
#define ____H_GUESTFILEIMPL

#include "VirtualBoxBase.h"

#include "GuestFsObjInfoImpl.h"
#include "GuestCtrlImplPrivate.h"

class Console;
class GuestSession;
class GuestProcess;

/**
 * TODO
 */
class ATL_NO_VTABLE GuestFile :
    public VirtualBoxBase,
    public GuestObject,
    VBOX_SCRIPTABLE_IMPL(IGuestFile)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestFile, IGuestFile)
    DECLARE_NOT_AGGREGATABLE(GuestFile)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestFile)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestFile)
        COM_INTERFACE_ENTRY(IFile)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestFile)

    int     init(Console *pConsole, GuestSession *pSession, ULONG uFileID, const GuestFileOpenInfo &openInfo);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IFile interface.
     * @{ */
    STDMETHOD(COMGETTER(CreationMode))(ULONG *aCreationMode);
    STDMETHOD(COMGETTER(Disposition))(ULONG *aDisposition);
    STDMETHOD(COMGETTER(FileName))(BSTR *aFileName);
    STDMETHOD(COMGETTER(InitialSize))(LONG64 *aInitialSize);
    STDMETHOD(COMGETTER(Offset))(LONG64 *aOffset);
    STDMETHOD(COMGETTER(OpenMode))(ULONG *aOpenMode);

    STDMETHOD(Close)(void);
    STDMETHOD(QueryInfo)(IFsObjInfo **aInfo);
    STDMETHOD(Read)(ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(ReadAt)(LONG64 aOffset, ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Seek)(LONG64 aOffset, FileSeekType_T aType);
    STDMETHOD(SetACL)(IN_BSTR aACL);
    STDMETHOD(Write)(ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    STDMETHOD(WriteAt)(LONG64 aOffset, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int             callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    static uint32_t getDispositionFromString(const Utf8Str &strDisposition);
    static uint32_t getOpenModeFromString(const Utf8Str &strOpenMode);
    int             onFileNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, GuestCtrlCallback *pCallback, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int             onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, GuestCtrlCallback *pCallback, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int             openFile(int *pGuestRc);
    /** @}  */

private:

    struct Data
    {
        /** The internal console object. */
        Console                *mConsole;
        /** The associate session this file belongs to. */
        GuestSession           *mSession;
        /** All related callbacks to this file. */
        GuestCtrlCallbacks      mCallbacks;
        /** The file's open info. */
        GuestFileOpenInfo       mOpenInfo;
        /** The file's initial size on open. */
        uint64_t                mInitialSize;
        /** The file's current offset. */
        uint64_t                mOffCurrent;
    } mData;
};

#endif /* !____H_GUESTFILEIMPL */

