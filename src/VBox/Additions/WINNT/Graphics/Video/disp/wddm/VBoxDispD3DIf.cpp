/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
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
#ifdef VBOX_WDDM_WOW64
    pD3D->hD3DLib = LoadLibraryW(L"VBoxD3D9wddm-x86.dll");
#else
    pD3D->hD3DLib = LoadLibraryW(L"VBoxD3D9wddm.dll");
#endif
    Assert(pD3D->hD3DLib);
    if (pD3D->hD3DLib)
    {
        pD3D->pfnDirect3DCreate9Ex = (PFNVBOXDISPD3DCREATE9EX)GetProcAddress(pD3D->hD3DLib, "Direct3DCreate9Ex");
        Assert(pD3D->pfnDirect3DCreate9Ex);
        if (pD3D->pfnDirect3DCreate9Ex)
        {
            pD3D->pfnVBoxWineExD3DDev9CreateTexture = (PFNVBOXWINEEXD3DDEV9_CREATETEXTURE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9CreateTexture");
            Assert(pD3D->pfnVBoxWineExD3DDev9CreateTexture);
            if (pD3D->pfnVBoxWineExD3DDev9CreateTexture)
            {
                pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture = (PFNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9CreateCubeTexture");
                Assert(pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture);
                if (pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture)
                {
                    pD3D->pfnVBoxWineExD3DDev9Flush = (PFNVBOXWINEEXD3DDEV9_FLUSH)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9Flush");
                    Assert(pD3D->pfnVBoxWineExD3DDev9Flush);
                    if (pD3D->pfnVBoxWineExD3DDev9Flush)
                    {
                        pD3D->pfnVBoxWineExD3DDev9Update = (PFNVBOXWINEEXD3DDEV9_UPDATE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9Update");
                        Assert(pD3D->pfnVBoxWineExD3DDev9Update);
                        if (pD3D->pfnVBoxWineExD3DDev9Update)
                        {
                            pD3D->pfnVBoxWineExD3DSwapchain9Present = (PFNVBOXWINEEXD3DSWAPCHAIN9_PRESENT)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DSwapchain9Present");
                            Assert(pD3D->pfnVBoxWineExD3DSwapchain9Present);
                            if (pD3D->pfnVBoxWineExD3DSwapchain9Present)
                            {
                                return S_OK;
                            }
                        }
                    }
                }
            }
        }
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
