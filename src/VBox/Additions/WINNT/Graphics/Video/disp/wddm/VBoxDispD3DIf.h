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

#ifndef ___VBoxDispD3DIf_h___
#define ___VBoxDispD3DIf_h___

/* D3D headers */
#include <iprt/critsect.h>
#include <iprt/semaphore.h>

#       define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#       define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#       define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#       define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#       pragma warning(disable : 4163)
#include <D3D9.h>
#       pragma warning(default : 4163)
#       undef  _InterlockedExchange
#       undef  _InterlockedExchangeAdd
#       undef  _InterlockedCompareExchange
#       undef  _InterlockedAddLargeStatistic
#       undef  _interlockedbittestandset
#       undef  _interlockedbittestandreset
#       undef  _interlockedbittestandset64
#       undef  _interlockedbittestandreset64

#include "../../../Wine/vbox/VBoxWineEx.h"

/* D3D functionality the VBOXDISPD3D provides */
typedef HRESULT WINAPI FNVBOXDISPD3DCREATE9EX(UINT SDKVersion, IDirect3D9Ex **ppD3D);
typedef FNVBOXDISPD3DCREATE9EX *PFNVBOXDISPD3DCREATE9EX;

typedef struct VBOXDISPD3D
{
    /* D3D functionality the VBOXDISPD3D provides */
    PFNVBOXDISPD3DCREATE9EX pfnDirect3DCreate9Ex;

    PFNVBOXWINEEXD3DDEV9_CREATETEXTURE pfnVBoxWineExD3DDev9CreateTexture;

    PFNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE pfnVBoxWineExD3DDev9CreateCubeTexture;

    PFNVBOXWINEEXD3DDEV9_FLUSH pfnVBoxWineExD3DDev9Flush;

    PFNVBOXWINEEXD3DDEV9_UPDATE pfnVBoxWineExD3DDev9Update;

    PFNVBOXWINEEXD3DDEV9_TERM pfnVBoxWineExD3DDev9Term;

    PFNVBOXWINEEXD3DRC9_SETSHRCSTATE pfnVBoxWineExD3DRc9SetShRcState;

    PFNVBOXWINEEXD3DSWAPCHAIN9_PRESENT pfnVBoxWineExD3DSwapchain9Present;

    /* module handle */
    HMODULE hD3DLib;
} VBOXDISPD3D;

typedef struct VBOXWDDMDISP_FORMATS
{
    uint32_t cFormstOps;
    const struct _FORMATOP* paFormstOps;
    uint32_t cSurfDescs;
    struct _DDSURFACEDESC *paSurfDescs;
} VBOXWDDMDISP_FORMATS, *PVBOXWDDMDISP_FORMATS;

typedef struct VBOXWDDMDISP_D3D
{
    VBOXDISPD3D D3D;
    IDirect3D9Ex * pD3D9If;
    D3DCAPS9 Caps;
    UINT cMaxSimRTs;
} VBOXWDDMDISP_D3D, *PVBOXWDDMDISP_D3D;

void VBoxDispD3DGlobalInit();
void VBoxDispD3DGlobalTerm();
HRESULT VBoxDispD3DGlobalOpen(PVBOXWDDMDISP_D3D pD3D, PVBOXWDDMDISP_FORMATS pFormats);
void VBoxDispD3DGlobalClose(PVBOXWDDMDISP_D3D pD3D, PVBOXWDDMDISP_FORMATS pFormats);

HRESULT VBoxDispD3DOpen(VBOXDISPD3D *pD3D);
void VBoxDispD3DClose(VBOXDISPD3D *pD3D);

#ifdef VBOX_WITH_VIDEOHWACCEL
HRESULT VBoxDispD3DGlobal2DFormatsInit(struct VBOXWDDMDISP_ADAPTER *pAdapter);
void VBoxDispD3DGlobal2DFormatsTerm(struct VBOXWDDMDISP_ADAPTER *pAdapter);
#endif

#endif /* ifndef ___VBoxDispD3DIf_h___ */
