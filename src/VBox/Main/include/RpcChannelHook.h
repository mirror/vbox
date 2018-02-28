/* $Id$ */
/** @file
* VBox COM Rpc Channel Hook definition
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

#ifdef RT_OS_WINDOWS
#include <iprt/win/windows.h>
#include <ObjIdl.h>


typedef void* PRPC_CHANNEL_HOOK;

// {CEDA3E95-A46A-4C41-AA01-EFFD856E455C}
static const GUID RPC_CHANNEL_EXTENSION_GUID =
{ 0xceda3e95, 0xa46a, 0x4c41,{ 0xaa, 0x1, 0xef, 0xfd, 0x85, 0x6e, 0x45, 0x5c } };

#if defined(__cplusplus)
extern "C"
{
#endif // #if defined(__cplusplus)

    // C wrapper for using in proxy
    void SetupClientRpcChannelHook(void);

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)


#if defined(__cplusplus)

/*
* This is RPC channel hook implementation for registering VirtualBox API clients.
* This channel hook instantiated in COM client process by VBoxProxyStub
* We use it to catch a moment when a new VirtualBox object sucessfully instantiated to
* register new API client in a VBoxSDS clients list.
*/
class CRpcChannelHook : public IChannelHook
{
public:
    CRpcChannelHook(REFGUID channelHookID = RPC_CHANNEL_EXTENSION_GUID): m_ChannelHookID(channelHookID) {};

    /* IUnknown Interface methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IChannelHook Interface methods */
    STDMETHODIMP_(void) ClientGetSize(REFGUID uExtent, REFIID riid, ULONG *pDataSize);
    STDMETHODIMP_(void) ClientFillBuffer(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer);
    STDMETHODIMP_(void) ClientNotify(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer, DWORD lDataRep, HRESULT hrFault);

    STDMETHODIMP_(void) ServerNotify(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer, DWORD lDataRep);
    STDMETHODIMP_(void) ServerGetSize(REFGUID uExtent, REFIID riid, HRESULT hrFault, ULONG *pDataSize);
    STDMETHODIMP_(void) ServerFillBuffer(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer, HRESULT hrFault);

protected:
    static bool IsChannelHookRegistered();
    static void RegisterChannelHook();

protected:
    const GUID m_ChannelHookID;
    static volatile bool s_bChannelRegistered;
    static volatile bool s_bVBpoxSDSCalledOnce;
    static CRpcChannelHook s_RpcChannelHook;

    /* C wrapper*/
    friend void SetupClientRpcChannelHook(void);
};


#endif // #if defined(__cplusplus)


#endif // #ifdef RT_OS_WINDOWS

#endif // ____H_RPCCHANNELHOOK
