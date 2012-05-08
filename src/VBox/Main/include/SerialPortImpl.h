/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_SERIALPORTIMPL
#define ____H_SERIALPORTIMPL

#include "VirtualBoxBase.h"

class GuestOSType;

namespace settings
{
    struct SerialPort;
}

class ATL_NO_VTABLE SerialPort :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(ISerialPort)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(SerialPort, ISerialPort)

    DECLARE_NOT_AGGREGATABLE(SerialPort)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(SerialPort)
        VBOX_DEFAULT_INTERFACE_ENTRIES (ISerialPort)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (SerialPort)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Machine *aParent, ULONG aSlot);
    HRESULT init (Machine *aParent, SerialPort *aThat);
    HRESULT initCopy (Machine *parent, SerialPort *aThat);
    void uninit();

    // ISerialPort properties
    STDMETHOD(COMGETTER(Slot))     (ULONG     *aSlot);
    STDMETHOD(COMGETTER(Enabled))  (BOOL      *aEnabled);
    STDMETHOD(COMSETTER(Enabled))  (BOOL       aEnabled);
    STDMETHOD(COMGETTER(HostMode)) (PortMode_T *aHostMode);
    STDMETHOD(COMSETTER(HostMode)) (PortMode_T  aHostMode);
    STDMETHOD(COMGETTER(IRQ))      (ULONG     *aIRQ);
    STDMETHOD(COMSETTER(IRQ))      (ULONG      aIRQ);
    STDMETHOD(COMGETTER(IOBase) )  (ULONG     *aIOBase);
    STDMETHOD(COMSETTER(IOBase))   (ULONG      aIOBase);
    STDMETHOD(COMGETTER(Path))     (BSTR      *aPath);
    STDMETHOD(COMSETTER(Path))     (IN_BSTR aPath);
    STDMETHOD(COMGETTER(Server))   (BOOL      *aServer);
    STDMETHOD(COMSETTER(Server))   (BOOL       aServer);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::SerialPort &data);
    HRESULT saveSettings(settings::SerialPort &data);

    bool isModified();
    void rollback();
    void commit();
    void copyFrom(SerialPort *aThat);

    void applyDefaults (GuestOSType *aOsType);

    // public methods for internal purposes only
    // (ensure there is a caller and a read lock before calling them!)

private:
    HRESULT checkSetPath(const Utf8Str &str);

    struct Data;
    Data *m;
};

#endif // ____H_SERIALPORTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
