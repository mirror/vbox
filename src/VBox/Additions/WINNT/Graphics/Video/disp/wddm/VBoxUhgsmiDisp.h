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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxUhgsmiDisp_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxUhgsmiDisp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxUhgsmiBase.h"
#include "VBoxDispD3DCmn.h"

typedef struct VBOXUHGSMI_PRIVATE_D3D
{
    VBOXUHGSMI_PRIVATE_BASE BasePrivate;
    struct VBOXWDDMDISP_DEVICE *pDevice;
} VBOXUHGSMI_PRIVATE_D3D, *PVBOXUHGSMI_PRIVATE_D3D;

void vboxUhgsmiD3DInit(PVBOXUHGSMI_PRIVATE_D3D pHgsmi, struct VBOXWDDMDISP_DEVICE *pDevice);

void vboxUhgsmiD3DEscInit(PVBOXUHGSMI_PRIVATE_D3D pHgsmi, struct VBOXWDDMDISP_DEVICE *pDevice);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxUhgsmiDisp_h */
