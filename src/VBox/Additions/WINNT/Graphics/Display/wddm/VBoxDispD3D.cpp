/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#include <windows.h>
#include <d3dtypes.h>
#include <D3dumddi.h>

#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxDispD3D.h"

#ifdef VBOXWDDMDISP_DEBUG
# include <stdio.h>
#endif

/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance,
                    DWORD     dwReason,
                    LPVOID    lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            /* there __try __except are just to ensure the library does not assertion fail in case VBoxGuest is not present
             * and VbglR3Init / VbglR3Term assertion fail */
            __try
            {
//                LogRel(("VBoxDispD3D: DLL loaded.\n"));
                RTR3Init();
//                VbglR3Init();
            }
            __except (EXCEPTION_CONTINUE_EXECUTION)
            {
            }

            break;
        }

        case DLL_PROCESS_DETACH:
        {
            /* there __try __except are just to ensure the library does not assertion fail in case VBoxGuest is not present
             * and VbglR3Init / VbglR3Term assertion fail */
//            __try
//            {
//                LogRel(("VBoxDispD3D: DLL unloaded.\n"));
//                VbglR3Term();
//            }
//            __except (EXCEPTION_CONTINUE_EXECUTION)
//            {
//            }
            /// @todo RTR3Term();
            break;
        }

        default:
            break;
    }
    return TRUE;
}

static HRESULT APIENTRY vboxWddmDispGetCaps (HANDLE hAdapter, CONST D3DDDIARG_GETCAPS* pData)
{
    vboxVDbgPrint(("==>"__FUNCTION__", hAdapter(0x%p), caps type(%d)\n", hAdapter, pData->Type));
    AssertBreakpoint();

    switch (pData->Type)
    {
        case D3DDDICAPS_DDRAW:
        case D3DDDICAPS_DDRAW_MODE_SPECIFIC:
        case D3DDDICAPS_GETFORMATCOUNT:
        case D3DDDICAPS_GETFORMATDATA:
        case D3DDDICAPS_GETMULTISAMPLEQUALITYLEVELS:
        case D3DDDICAPS_GETD3DQUERYCOUNT:
        case D3DDDICAPS_GETD3DQUERYDATA:
        case D3DDDICAPS_GETD3D3CAPS:
        case D3DDDICAPS_GETD3D5CAPS:
        case D3DDDICAPS_GETD3D6CAPS:
        case D3DDDICAPS_GETD3D7CAPS:
        case D3DDDICAPS_GETD3D8CAPS:
        case D3DDDICAPS_GETD3D9CAPS:
        case D3DDDICAPS_GETDECODEGUIDCOUNT:
        case D3DDDICAPS_GETDECODEGUIDS:
        case D3DDDICAPS_GETDECODERTFORMATCOUNT:
        case D3DDDICAPS_GETDECODERTFORMATS:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFOCOUNT:
        case D3DDDICAPS_GETDECODECOMPRESSEDBUFFERINFO:
        case D3DDDICAPS_GETDECODECONFIGURATIONCOUNT:
        case D3DDDICAPS_GETDECODECONFIGURATIONS:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORDEVICEGUIDS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTFORMATS:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATCOUNT:
        case D3DDDICAPS_GETVIDEOPROCESSORRTSUBSTREAMFORMATS:
        case D3DDDICAPS_GETVIDEOPROCESSORCAPS:
        case D3DDDICAPS_GETPROCAMPRANGE:
        case D3DDDICAPS_FILTERPROPERTYRANGE:
        case D3DDDICAPS_GETEXTENSIONGUIDCOUNT:
        case D3DDDICAPS_GETEXTENSIONGUIDS:
        case D3DDDICAPS_GETEXTENSIONCAPS:
        case D3DDDICAPS_GETGAMMARAMPCAPS:
            break;
        default:
            AssertBreakpoint();
    }

    vboxVDbgPrint(("<=="__FUNCTION__", hAdapter(0x%p), caps type(%d)\n", hAdapter, pData->Type));

    return S_OK;
}

static HRESULT APIENTRY vboxWddmDispCreateDevice (IN HANDLE hAdapter, IN D3DDDIARG_CREATEDEVICE* pCreateData)
{
    vboxVDbgPrint(("==>"__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    AssertBreakpoint();

    vboxVDbgPrint(("<=="__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    return E_FAIL;
}

static HRESULT APIENTRY vboxWddmDispCloseAdapter (IN HANDLE hAdapter)
{
    vboxVDbgPrint(("==>"__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    AssertBreakpoint();

    RTMemFree(hAdapter);

    vboxVDbgPrint(("<=="__FUNCTION__", hAdapter(0x%p)\n", hAdapter));

    return S_OK;
}

HRESULT APIENTRY OpenAdapter (__inout D3DDDIARG_OPENADAPTER*  pOpenData)
{
    vboxVDbgPrint(("==>"__FUNCTION__"\n"));

    AssertBreakpoint();

    PVBOXWDDMDISP_ADAPTER pAdapter = (PVBOXWDDMDISP_ADAPTER)RTMemAllocZ(sizeof (VBOXWDDMDISP_ADAPTER));
    Assert(pAdapter);
    if (!pAdapter)
    {
        vboxVDbgPrintR((__FUNCTION__": RTMemAllocZ returned NULL\n"));
        return E_OUTOFMEMORY;
    }

    pAdapter->hAdapter = pOpenData->hAdapter;
    pAdapter->uIfVersion = pOpenData->Interface;
    pAdapter->uRtVersion= pOpenData->Version;
    pAdapter->RtCallbacks = *pOpenData->pAdapterCallbacks;

    pOpenData->hAdapter = pAdapter;
    pOpenData->pAdapterFuncs->pfnGetCaps = vboxWddmDispGetCaps;
    pOpenData->pAdapterFuncs->pfnCreateDevice = vboxWddmDispCreateDevice;
    pOpenData->pAdapterFuncs->pfnCloseAdapter = vboxWddmDispCloseAdapter;
    pOpenData->DriverVersion = D3D_UMD_INTERFACE_VERSION;

    vboxVDbgPrint(("<=="__FUNCTION__", pAdapter(0x%p)\n", pAdapter));

    return S_OK;
}

#ifdef VBOXWDDMDISP_DEBUG
VOID vboxVDbgDoPrint(LPCSTR szString, ...)
{
    char szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), szString, pArgList);
    va_end(pArgList);

    OutputDebugStringA(szBuffer);
}
#endif
