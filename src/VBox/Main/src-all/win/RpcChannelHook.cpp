/* $Id$ */
/** @file
* VBox Global COM Class implementation.
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


#include <iprt\assert.h>
#include <iprt\process.h>
#include <iprt\string.h>
#include <iprt/asm.h>
#include "Logging.h"
#include <VBox/com/VirtualBox.h>

#include <string>

#include "VirtualBox.h"
#include "RpcChannelHook.h"

#ifdef RT_OS_WINDOWS

volatile bool CRpcChannelHook::s_fChannelRegistered = false;
volatile bool CRpcChannelHook::s_fVBpoxSDSCalledOnce = false;
CRpcChannelHook CRpcChannelHook::s_RpcChannelHook;


/*
 * RpcChannelHook IUnknown interface implementation.
 */

STDMETHODIMP CRpcChannelHook::QueryInterface(REFIID riid, void **ppv)
{
    if (riid == IID_IUnknown)
    {
        *ppv = (IUnknown*)this;
    }
    else if (riid == IID_IChannelHook)
    {
        *ppv = (IChannelHook*)this;
    }
    else
    {
        *ppv = 0;
        return E_NOINTERFACE;
    }
    this->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CRpcChannelHook::AddRef(void)
{
    return 2;
}

STDMETHODIMP_(ULONG) CRpcChannelHook::Release(void)
{
    return 1;
}


/**
 * This is a function that we can call from non-C++ proxy stub code.
 *
 * @note Warning! function is not thread safe!
 * @todo Consider using RTONCE to serialize this (though it's likely unncessary
 *       due to COM).
 */
void SetupClientRpcChannelHook(void)
{
    // register single hook only
    if (!CRpcChannelHook::IsChannelHookRegistered())
    {
        HRESULT hr = CoRegisterChannelHook(RPC_CHANNEL_EXTENSION_GUID, &CRpcChannelHook::s_RpcChannelHook);
        NOREF(hr);
        Assert(SUCCEEDED(hr));
        CRpcChannelHook::RegisterChannelHook();
        LogFunc(("Registered RPC client channel hook \n"));
    }
}


/*
 * Internal methods.
 */

bool CRpcChannelHook::IsChannelHookRegistered()
{
    return ASMAtomicReadBool(&s_fChannelRegistered);
}


void CRpcChannelHook::RegisterChannelHook()
{
    ASMAtomicWriteBool(&s_fChannelRegistered, true);
}

/* RpcChannelHook IChannelHook interface implementation.*/


STDMETHODIMP_(void) CRpcChannelHook::ClientGetSize(REFGUID uExtent, REFIID riid, ULONG *pDataSize)
{
    NOREF(riid);
    Assert(uExtent == m_ChannelHookID);
    Assert(pDataSize);
    if (uExtent == m_ChannelHookID)
    {
        if (pDataSize)
        {
            *pDataSize = 0;
        }
    }
}

/**
 * This is callback of RPC channel hook called on COM client when COM method call
 * finished on server and response returned.
 * We use it to catch a moment when a new VirtualBox object sucessfully instantiated.
 * This callback is called in client process - VirtualBox.exe, VBoxManage or custom client
 * If it happend we register new API client in VBoxSDS.
 * Parameters:
 * @param   uExtent         Unique ID of our RPC channel.
 * @param   riid            Interface ID of called server interface (IVirtualBox
 *                          in our case).
 * @param   cbDataBuffer    ???????????????????
 * @param   pDataBuffer     NULL, such as we have nothing to transfer from
 *                          server side.
 * @param   lDataRep        ???????????
 * @param   hrFault         result of called COM server method.
 */
STDMETHODIMP_(void) CRpcChannelHook::ClientNotify(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer,
                                                  DWORD lDataRep, HRESULT hrFault)
{
    NOREF(cbDataSize);
    NOREF(pDataBuffer);
    NOREF(lDataRep);

    /*
     * Check that it created VirtualBox and this is first method called on server
     * (CreateInstance)
     */
    if (   riid == IID_IVirtualBox
        && uExtent == m_ChannelHookID
        && SUCCEEDED(hrFault)
        && !ASMAtomicReadBool(&s_fVBpoxSDSCalledOnce) )
    {
        LogFunc(("Finished call of VirtualBox method\n"));

        ASMAtomicWriteBool(&s_fVBpoxSDSCalledOnce, true);

        /*
         * Connect to VBoxSDS.
         * Note: VBoxSDS can handle duplicate calls
         */
        ComPtr<IVirtualBoxClientList> ptrClientList;
        HRESULT hrc = CoCreateInstance(CLSID_VirtualBoxClientList, NULL, CLSCTX_LOCAL_SERVER, IID_IVirtualBoxClientList,
                                       (void **)ptrClientList.asOutParam());
        if (SUCCEEDED(hrc))
        {
            RTPROCESS pid = RTProcSelf();
            hrc = ptrClientList->RegisterClient(pid);
            LogFunc(("Called VBoxSDS RegisterClient() : hr=%Rhrf\n", hrc));
        }
        else
        {
            LogFunc(("Error to connect to VBoxSDS: hr=%Rhrf\n", hrc));
        }
    }
}

STDMETHODIMP_(void) CRpcChannelHook::ClientFillBuffer(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer)
{
    RT_NOREF(uExtent, riid, pDataSize, pDataBuffer);
}

STDMETHODIMP_(void) CRpcChannelHook::ServerGetSize(REFGUID uExtent, REFIID riid, HRESULT hrFault, ULONG *pDataSize)
{
    // Nothing to send to client side from server side
    RT_NOREF(uExtent, riid, hrFault);
    *pDataSize = 0;
}

STDMETHODIMP_(void) CRpcChannelHook::ServerNotify(REFGUID uExtent, REFIID riid, ULONG cbDataSize, void *pDataBuffer,
                                                  DWORD lDataRep)
{
    // Nothing to do on server side
    RT_NOREF(uExtent, riid, cbDataSize, pDataBuffer, lDataRep);
}

STDMETHODIMP_(void) CRpcChannelHook::ServerFillBuffer(REFGUID uExtent, REFIID riid, ULONG *pDataSize, void *pDataBuffer,
                                                      HRESULT hrFault)
{
    // Nothing to send to client side from server side
    RT_NOREF(uExtent, riid, pDataSize, pDataBuffer, hrFault);
}

#endif
