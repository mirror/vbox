/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "VBoxDispD3DIf.h"
#include "VBoxDispD3DCmn.h"

#include <iprt/assert.h>

void VBoxDispD3DClose(VBOXDISPD3D *pD3D)
{
    FreeLibrary(pD3D->hD3DLib);
    pD3D->hD3DLib = NULL;
}


HRESULT VBoxDispD3DOpen(VBOXDISPD3D *pD3D)
{
    pD3D->hD3DLib = LoadLibraryW(L"VBoxD3D9.dll");
    Assert(pD3D->hD3DLib);
    if (pD3D->hD3DLib)
    {
        pD3D->pfnDirect3DCreate9Ex = (PFNVBOXDISPD3DCREATE9EX)GetProcAddress(pD3D->hD3DLib, "Direct3DCreate9Ex");
        Assert(pD3D->pfnDirect3DCreate9Ex);
        if (pD3D->pfnDirect3DCreate9Ex)
            return S_OK;
        else
        {
            DWORD winErr = GetLastError();
            vboxVDbgPrintR((__FUNCTION__": GetProcAddressW (for Direct3DCreate9Ex) failed, winErr = (%d)", winErr));
        }

        VBoxDispD3DClose(pD3D);
    }
    else
    {
        DWORD winErr = GetLastError();
        vboxVDbgPrintR((__FUNCTION__": LoadLibraryW failed, winErr = (%d)", winErr));
    }

    return E_FAIL;
}
