/* $Id$ */
/** @file
 * VBox WDDM Miniport driver
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#include <VBoxVideo.h>
#include <HGSMI.h>

typedef struct _VBOXMP_DEVEXT *PVBOXMP_DEVEXT;

/* DMA commands are currently submitted over HGSMI */
typedef struct VBOXVDMAINFO
{
    BOOL      fEnabled;
} VBOXVDMAINFO, *PVBOXVDMAINFO;

int vboxVdmaCreate (PVBOXMP_DEVEXT pDevExt, VBOXVDMAINFO *pInfo
        );
int vboxVdmaDisable(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaEnable(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);
int vboxVdmaDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXVDMAINFO pInfo);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVdma_h */
