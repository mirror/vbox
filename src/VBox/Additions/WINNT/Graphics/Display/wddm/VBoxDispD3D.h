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
#ifndef ___VBoxDispD3D_h___
#define ___VBoxDispD3D_h___

#include "VBoxDispD3DIf.h"

#include <iprt/cdefs.h>

typedef struct VBOXWDDMDISP_ADAPTER
{
    HANDLE hAdapter;
    UINT uIfVersion;
    UINT uRtVersion;
    VBOXDISPD3D D3D;
    IDirect3D9Ex * pD3D9If;
    D3DDDI_ADAPTERCALLBACKS RtCallbacks;
} VBOXWDDMDISP_ADAPTER, *PVBOXWDDMDISP_ADAPTER;

typedef struct VBOXWDDMDISP_DEVICE
{
    HANDLE hDevice;
    PVBOXWDDMDISP_ADAPTER pAdapter;
    UINT u32IfVersion;
    UINT uRtVersion;
    D3DDDI_DEVICECALLBACKS RtCallbacks;
    VOID *pvCmdBuffer;
    UINT cbCmdBuffer;
    D3DDDI_CREATEDEVICEFLAGS fFlags;
} VBOXWDDMDISP_DEVICE, *PVBOXWDDMDISP_DEVICE;

DECLINLINE(bool) vboxDispD3DIs3DEnabled(VBOXWDDMDISP_ADAPTER * pAdapter)
{
    return !!(pAdapter->pD3D9If);
}

#endif /* #ifndef ___VBoxDispD3D_h___ */
