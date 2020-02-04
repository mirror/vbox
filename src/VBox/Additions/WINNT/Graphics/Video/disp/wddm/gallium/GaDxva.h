/* $Id$ */
/** @file
 * VirtualBox WDDM DXVA for the Gallium based driver.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <d3dumddi.h>

typedef struct VBOXWDDMDISP_DEVICE *PVBOXWDDMDISP_DEVICE;

HRESULT VBoxDxvaGetDeviceGuidCount(UINT *pcGuids);
HRESULT VBoxDxvaGetDeviceGuids(GUID *paGuids, UINT cbGuids);

HRESULT VBoxDxvaGetOutputFormatCount(UINT *pcFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream);
HRESULT VBoxDxvaGetOutputFormats(D3DDDIFORMAT *paFormats, UINT cbFormats, DXVADDI_VIDEOPROCESSORINPUT const *pVPI, bool fSubstream);

HRESULT VBoxDxvaGetCaps(DXVADDI_VIDEOPROCESSORCAPS *pVideoProcessorCaps, DXVADDI_VIDEOPROCESSORINPUT const *pVPI);

HRESULT VBoxDxvaCreateVideoProcessDevice(PVBOXWDDMDISP_DEVICE pDevice, D3DDDIARG_CREATEVIDEOPROCESSDEVICE *pData);
HRESULT VBoxDxvaDestroyVideoProcessDevice(PVBOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor);
HRESULT VBoxDxvaVideoProcessBeginFrame(PVBOXWDDMDISP_DEVICE pDevice, HANDLE hVideoProcessor);
HRESULT VBoxDxvaVideoProcessEndFrame(PVBOXWDDMDISP_DEVICE pDevice, D3DDDIARG_VIDEOPROCESSENDFRAME *pData);
HRESULT VBoxDxvaSetVideoProcessRenderTarget(PVBOXWDDMDISP_DEVICE pDevice, const D3DDDIARG_SETVIDEOPROCESSRENDERTARGET *pData);
HRESULT VBoxDxvaVideoProcessBlt(PVBOXWDDMDISP_DEVICE pDevice, const D3DDDIARG_VIDEOPROCESSBLT *pData);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDxva_h */
