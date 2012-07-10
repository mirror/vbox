
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

#ifndef ____H_GUESTPROCESSIMPL
#define ____H_GUESTPROCESSIMPL

#include "VirtualBoxBase.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestProcess :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestProcess)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestProcess, IGuestProcess)
    DECLARE_NOT_AGGREGATABLE(GuestProcess)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestProcess)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestProcess)
        COM_INTERFACE_ENTRY(IProcess)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestProcess)
    HRESULT init(void);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IProcess interface.
     * @{ */
    STDMETHOD(COMGETTER(Arguments))(ComSafeArrayOut(BSTR, aArguments));
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(BSTR, aEnvironment));
    STDMETHOD(COMGETTER(ExecutablePath))(BSTR *aExecutablePath);
    STDMETHOD(COMGETTER(ExitCode))(LONG *aExitCode);
    STDMETHOD(COMGETTER(PID))(ULONG *aPID);
    STDMETHOD(COMGETTER(Status))(ProcessStatus *aStatus);

    STDMETHOD(Read)(ULONG aHandle, ULONG aSize, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Terminate)(void);
    STDMETHOD(WaitFor)(ComSafeArrayOut(ProcessWaitForFlag, aFlags), ULONG aTimeoutMS, ProcessWaitReason *aReason);
    STDMETHOD(Write)(ULONG aHandle, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    typedef std::map <Utf8Str, Utf8Str> MyStringMap;

    struct Data
    {
        Bstr                 mName;
    } mData;
};

#endif /* !____H_GUESTPROCESSIMPL */

