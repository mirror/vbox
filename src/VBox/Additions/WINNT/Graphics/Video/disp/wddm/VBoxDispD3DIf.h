/* $Id$ */
/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DIf_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VBOX_WITH_MESA3D
#include "gallium/VBoxGallium.h"
#endif

/* D3D headers */
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/win/d3d9.h>
#include <d3dumddi.h>
#include "../../common/wddm/VBoxMPIf.h"

typedef struct VBOXWDDMDISP_FORMATS
{
    uint32_t cFormatOps;
    const struct _FORMATOP* paFormatOps;
    uint32_t cSurfDescs;
    struct _DDSURFACEDESC *paSurfDescs;
} VBOXWDDMDISP_FORMATS, *PVBOXWDDMDISP_FORMATS;

typedef struct VBOXWDDMDISP_D3D *PVBOXWDDMDISP_D3D;
typedef void FNVBOXDISPD3DBACKENDCLOSE(PVBOXWDDMDISP_D3D pD3D);
typedef FNVBOXDISPD3DBACKENDCLOSE *PFNVBOXDISPD3DBACKENDCLOSE;

typedef struct VBOXWDDMDISP_D3D
{
    PFNVBOXDISPD3DBACKENDCLOSE pfnD3DBackendClose;

    D3DCAPS9 Caps;
    UINT cMaxSimRTs;

#ifdef VBOX_WITH_MESA3D
    /* Gallium backend. */
    IGalliumStack *pGalliumStack;
#endif
} VBOXWDDMDISP_D3D;

void VBoxDispD3DGlobalInit(void);
void VBoxDispD3DGlobalTerm(void);
HRESULT VBoxDispD3DGlobalOpen(PVBOXWDDMDISP_D3D pD3D, PVBOXWDDMDISP_FORMATS pFormats, VBOXWDDM_QAI const *pAdapterInfo);
void VBoxDispD3DGlobalClose(PVBOXWDDMDISP_D3D pD3D, PVBOXWDDMDISP_FORMATS pFormats);

#ifdef VBOX_WITH_VIDEOHWACCEL
HRESULT VBoxDispD3DGlobal2DFormatsInit(struct VBOXWDDMDISP_ADAPTER *pAdapter);
void VBoxDispD3DGlobal2DFormatsTerm(struct VBOXWDDMDISP_ADAPTER *pAdapter);
#endif

#ifdef DEBUG
void vboxDispCheckCapsLevel(const D3DCAPS9 *pCaps);
#endif

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DIf_h */
