/* $Id$ */
/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispCm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispCm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef struct VBOXWDDMDISP_DEVICE *PVBOXWDDMDISP_DEVICE;
typedef struct VBOXWDDMDISP_CONTEXT *PVBOXWDDMDISP_CONTEXT;

HRESULT vboxDispCmInit();
HRESULT vboxDispCmTerm();

HRESULT vboxDispCmCtxDestroy(PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_CONTEXT pContext);
HRESULT vboxDispCmCtxCreate(PVBOXWDDMDISP_DEVICE pDevice, PVBOXWDDMDISP_CONTEXT pContext);

HRESULT vboxDispCmCmdGet(PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD pCmd, uint32_t cbCmd, DWORD dwMilliseconds);

HRESULT vboxDispCmCmdInterruptWait();

void vboxDispCmLog(LPCSTR pszMsg);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispCm_h */
