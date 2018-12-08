/* $Id$ */
/** @file
 * VirtualBox Main - RPC Channel Hook Hack, VBoxProxyStub.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_RPCCHANNELHOOK
#define ____H_RPCCHANNELHOOK

#include <iprt/cdefs.h>

#ifndef RT_OS_WINDOWS
# error "Oops"
#endif

#include <iprt/win/windows.h>
#include <ObjIdl.h>


RT_C_DECLS_BEGIN

/* {CEDA3E95-A46A-4C41-AA01-EFFD856E455C} */
static const GUID RPC_CHANNEL_EXTENSION_GUID =
{ 0xceda3e95, 0xa46a, 0x4c41,{ 0xaa, 0x1, 0xef, 0xfd, 0x85, 0x6e, 0x45, 0x5c } };

/** C wrapper for using in proxy (C code). */
void SetupClientRpcChannelHook(void);

RT_C_DECLS_END


#ifdef __cplusplus

/**
 * This is RPC channel hook implementation for registering VirtualBox API clients.
 *
 * This channel hook instantiated in COM client process by VBoxProxyStub
 * We use it to catch a moment when a new VirtualBox object sucessfully instantiated to
 * register new API client in a VBoxSDS clients list.
 */
class CRpcChannelHook : public IChannelHook
{
public:
    CRpcChannelHook(REFGUID channelHookID = RPC_CHANNEL_EXTENSION_GUID): m_ChannelHookID(channelHookID) {};

    /* IUnknown Interface methods */
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    /* IChannelHook Interface methods */
    STDMETHOD_(void, ClientGetSize)(REFGUID uExtent, REFIID riid, ULONG *pDataSize);
    STDMETHOD_(void, ClientFillBuffer)(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer);
    STDMETHOD_(void, ClientNotify)(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer, DWORD lDataRep, HRESULT hrFault);

    STDMETHOD_(void, ServerNotify)(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer, DWORD lDataRep);
    STDMETHOD_(void, ServerGetSize)(REFGUID uExtent, REFIID riid, HRESULT hrFault, ULONG *pDataSize);
    STDMETHOD_(void, ServerFillBuffer)(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer, HRESULT hrFault);

protected:
    static bool IsChannelHookRegistered();
    static void RegisterChannelHook();

protected:
    const GUID              m_ChannelHookID;
    static volatile bool    s_fChannelRegistered;
    static volatile bool    s_fVBpoxSDSCalledOnce;
    static CRpcChannelHook  s_RpcChannelHook;

    /* C wrapper*/
    friend void SetupClientRpcChannelHook(void);
};

#endif /* __cplusplus */

#endif
