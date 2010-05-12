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
#ifndef ___VBoxDispD3DIf_h___
#define ___VBoxDispD3DIf_h___

/* D3D headers */
#include <D3D9.h>

/* D3D functionality the VBOXDISPD3D provides */
typedef HRESULT WINAPI FNVBOXDISPD3DCREATE9EX(UINT SDKVersion, IDirect3D9Ex **ppD3D);
typedef FNVBOXDISPD3DCREATE9EX *PFNVBOXDISPD3DCREATE9EX;

typedef struct VBOXDISPD3D
{
    /* D3D functionality the VBOXDISPD3D provides */
    PFNVBOXDISPD3DCREATE9EX pfnDirect3DCreate9Ex;

    /* module handle */
    HMODULE hD3DLib;
} VBOXDISPD3D;

HRESULT VBoxDispD3DOpen(VBOXDISPD3D *pD3D);
void VBoxDispD3DClose(VBOXDISPD3D *pD3D);

#endif /* ifndef ___VBoxDispD3DIf_h___ */
