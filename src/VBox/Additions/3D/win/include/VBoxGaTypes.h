/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_3D_WIN_VBoxGaTypes_h
#define GA_INCLUDED_3D_WIN_VBoxGaTypes_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GA_MAX_SURFACE_FACES 6
#define GA_MAX_MIP_LEVELS 24

typedef struct GASURFCREATE
{
    uint32_t flags;  /* SVGA3dSurfaceFlags */
    uint32_t format; /* SVGA3dSurfaceFormat */
    uint32_t usage;  /* SVGA_SURFACE_USAGE_* */
    uint32_t mip_levels[GA_MAX_SURFACE_FACES];
} GASURFCREATE;

typedef struct GASURFSIZE
{
    uint32_t cWidth;
    uint32_t cHeight;
    uint32_t cDepth;
    uint32_t u32Reserved;
} GASURFSIZE;

#define GA_FENCE_STATUS_NULL      0 /* Fence not found */
#define GA_FENCE_STATUS_IDLE      1
#define GA_FENCE_STATUS_SUBMITTED 2
#define GA_FENCE_STATUS_SIGNALED  3

typedef struct GAFENCEQUERY
{
    /* IN: The miniport's handle of the fence.
     * Assigned by the miniport. Not DXGK fence id!
     */
    uint32_t u32FenceHandle;

    /* OUT: The miniport's sequence number associated with the command buffer.
     */
    uint32_t u32SubmittedSeqNo;

    /* OUT: The miniport's sequence number associated with the last command buffer completed on host.
     */
    uint32_t u32ProcessedSeqNo;

    /* OUT: GA_FENCE_STATUS_*. */
    uint32_t u32FenceStatus;
} GAFENCEQUERY;

#ifdef __cplusplus
}
#endif

#endif /* !GA_INCLUDED_3D_WIN_VBoxGaTypes_h */

