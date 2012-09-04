
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

#ifndef ____H_GUESTFILEIMPL
#define ____H_GUESTFILEIMPL

#include "VirtualBoxBase.h"

#include "GuestFsObjInfoImpl.h"

class GuestSession;
class GuestProcess;

/**
 * TODO
 */
class ATL_NO_VTABLE GuestFile :
    public VirtualBoxBase,
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

    int     init(GuestSession *pSession, const Utf8Str &strPath, const Utf8Str &strOpenMode, const Utf8Str &strDisposition, uint32_t uCreationMode, int64_t iOffset, int *pGuestRc);
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
    static uint32_t getDispositionFromString(const Utf8Str &strDisposition);
    static uint32_t getOpenModeFromString(const Utf8Str &strOpenMode);
    /** @}  */

private:

    struct Data
    {
        /** The associate session this file belongs to. */
        GuestSession           *mSession;
        uint32_t                mCreationMode;
        uint32_t                mDisposition;
        Utf8Str                 mFileName;
        int64_t                 mInitialSize;
        uint32_t                mOpenMode;
        int64_t                 mOffset;
    } mData;
};

#endif /* !____H_GUESTFILEIMPL */

