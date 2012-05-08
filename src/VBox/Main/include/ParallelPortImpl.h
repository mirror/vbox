/* $Id$ */

/** @file
 * VirtualBox COM class implementation.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_PARALLELPORTIMPL
#define ____H_PARALLELPORTIMPL

#include "VirtualBoxBase.h"

namespace settings
{
    struct ParallelPort;
}

class ATL_NO_VTABLE ParallelPort :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IParallelPort)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(ParallelPort, IParallelPort)

    DECLARE_NOT_AGGREGATABLE(ParallelPort)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(ParallelPort)
        VBOX_DEFAULT_INTERFACE_ENTRIES (IParallelPort)
    END_COM_MAP()

    ParallelPort() {}
    ~ParallelPort() {}

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent, ULONG aSlot);
    HRESULT init (Machine *aParent, ParallelPort *aThat);
    HRESULT initCopy (Machine *parent, ParallelPort *aThat);
    void uninit();

    // IParallelPort properties
    STDMETHOD(COMGETTER(Slot))    (ULONG     *aSlot);
    STDMETHOD(COMGETTER(Enabled)) (BOOL      *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL       aEnabled);
    STDMETHOD(COMGETTER(IRQ))     (ULONG     *aIRQ);
    STDMETHOD(COMSETTER(IRQ))     (ULONG      aIRQ);
    STDMETHOD(COMGETTER(IOBase))  (ULONG     *aIOBase);
    STDMETHOD(COMSETTER(IOBase))  (ULONG      aIOBase);
    STDMETHOD(COMGETTER(Path))    (BSTR      *aPath);
    STDMETHOD(COMSETTER(Path))    (IN_BSTR aPath);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::ParallelPort &data);
    HRESULT saveSettings(settings::ParallelPort &data);

    bool isModified();
    void rollback();
    void commit();
    void copyFrom(ParallelPort *aThat);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

private:
    HRESULT checkSetPath(const Utf8Str &str);

    struct Data;
    Data *m;
};

#endif // ____H_PARALLELPORTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
