/* $Id$ */
/** @file
 * VBoxNetFltNobj.h - Notify Object for Bridged Networking Driver.
 * Used to filter Bridged Networking Driver bindings
 */
/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxNetFltNobj_h___
#define ___VBoxNetFltNobj_h___

#include <windows.h>
/* atl stuff */
#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>

#include "VBoxNetFltNobjT.h"
#include "VBoxNetFltNobjRc.h"

#define VBOXNETFLTNOTIFY_ONFAIL_BINDDEFAULT false

/*
 * VirtualBox Bridging driver notify object.
 * Needed to make our driver bind to "real" host adapters only
 */
class ATL_NO_VTABLE VBoxNetFltNobj :
    public CComObjectRootEx<CComObjectThreadModel>,
    public CComCoClass<VBoxNetFltNobj, &CLSID_VBoxNetFltNobj>,
    public INetCfgComponentControl,
    public INetCfgComponentNotifyBinding
{
public:
    VBoxNetFltNobj();
    ~VBoxNetFltNobj();

    BEGIN_COM_MAP(VBoxNetFltNobj)
        COM_INTERFACE_ENTRY(INetCfgComponentControl)
        COM_INTERFACE_ENTRY(INetCfgComponentNotifyBinding)
    END_COM_MAP()

    DECLARE_REGISTRY_RESOURCEID(IDR_VBOXNETFLT_NOBJ)

    /* INetCfgComponentControl methods */
    STDMETHOD(Initialize)(IN INetCfgComponent *pNetCfgComponent, IN INetCfg *pNetCfg, IN BOOL bInstalling);
    STDMETHOD(ApplyRegistryChanges)();
    STDMETHOD(ApplyPnpChanges)(IN INetCfgPnpReconfigCallback *pCallback);
    STDMETHOD(CancelChanges)();

    /* INetCfgComponentNotifyBinding methods */
    STDMETHOD(NotifyBindingPath)(IN DWORD dwChangeFlag, IN INetCfgBindingPath *pNetCfgBP);
    STDMETHOD(QueryBindingPath)(IN DWORD dwChangeFlag, IN INetCfgBindingPath *pNetCfgBP);
private:

    void init(IN INetCfgComponent *pNetCfgComponent, IN INetCfg *pNetCfg, IN BOOL bInstalling);
    void cleanup();

    /* these two used to maintain the component info passed to
     * INetCfgComponentControl::Initialize */
    INetCfg *mpNetCfg;
    INetCfgComponent *mpNetCfgComponent;
    BOOL mbInstalling;
};

#endif /* #ifndef ___VBoxNetFltNobj_h___ */
