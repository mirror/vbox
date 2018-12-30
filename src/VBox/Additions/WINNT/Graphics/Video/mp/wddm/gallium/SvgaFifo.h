/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver VMSVGA FIFO operations.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___SvgaFifo_h__
#define ___SvgaFifo_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Svga.h"

NTSTATUS SvgaFifoInit(PVBOXWDDM_EXT_VMSVGA pSvga);

void *SvgaFifoReserve(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t cbReserve);
void SvgaFifoCommit(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t cbActual);

#endif
