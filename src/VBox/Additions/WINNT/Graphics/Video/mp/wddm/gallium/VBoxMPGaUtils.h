/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver miscellaneous helpers and common includes.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaUtils_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaUtils_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/nt/ntddk.h>
#include <iprt/nt/dispmprt.h>

#define LOG_GROUP LOG_GROUP_DRV_MINIPORT
#include <VBox/log.h>

extern volatile uint32_t g_fu32GaLogControl;
#define GALOG_(_msg, _log) do {                   \
    if (RT_LIKELY(g_fu32GaLogControl == 0))       \
    { /* likely */ }                              \
    else if (RT_BOOL(g_fu32GaLogControl & 1))     \
        _log(_msg);                               \
} while (0)

#define GALOG_EXACT(_msg) GALOG_(_msg, LogRel)
#define GALOG(_msg) GALOG_(_msg, LogRelFunc)

#define GALOGREL_EXACT(a) LogRel(a)
#define GALOGREL(a) LogRelFunc(a)

void *GaMemAlloc(uint32_t cbSize);
void *GaMemAllocZero(uint32_t cbSize);
void GaMemFree(void *pvMem);

NTSTATUS GaIdAlloc(uint32_t *pu32Bits,
                   uint32_t cbBits,
                   uint32_t u32Limit,
                   uint32_t *pu32Id);
NTSTATUS GaIdFree(uint32_t *pu32Bits,
                  uint32_t cbBits,
                  uint32_t u32Limit,
                  uint32_t u32Id);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaUtils_h */
