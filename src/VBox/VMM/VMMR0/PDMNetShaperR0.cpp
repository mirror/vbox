/* $Id$ */
/** @file
 * PDM Network Shaper - Limit network traffic according to bandwidth
 * group settings [R0 part].
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_SHAPER

#include <VBox/vmm/pdm.h>
#include <VBox/log.h>
#include <iprt/time.h>

#include <VBox/vmm/pdmnetshaper.h>
#include <VBox/vmm/pdmnetshaperint.h>


/**
 * Obtain bandwidth in a bandwidth group (R0 version).
 *
 * @returns VBox status code.
 * @param   pFilter         Pointer to the filter that allocates bandwidth.
 * @param   cbTransfer      Number of bytes to allocate.
 */
VMMR0DECL(bool) PDMR0NsAllocateBandwidth(PPDMNSFILTER pFilter, size_t cbTransfer)
{
    return pdmNsAllocateBandwidth(pFilter, cbTransfer);
}
