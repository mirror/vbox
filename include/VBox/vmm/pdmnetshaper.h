/** @file
 * PDM - Pluggable Device Manager, Network Shaper.
 */

/*
 * Copyright (C) 2007-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_pdmnetshaper_h
#define ___VBox_vmm_pdmnetshaper_h

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/vmm/pdmnetifs.h>
#include <iprt/assert.h>
#include <iprt/sg.h>


#define PDM_NETSHAPER_MIN_BUCKET_SIZE 65536 /* bytes */
#define PDM_NETSHAPER_MAX_LATENCY     100   /* milliseconds */

RT_C_DECLS_BEGIN

typedef struct PDMNSFILTER
{
    /** [R3] Pointer to the next group in the list. */
    struct PDMNSFILTER      *pNext;
    /** [R3] Pointer to the bandwidth group. */
    struct PDMNSBWGROUP     *pBwGroupR3;
    /** Becomes true when filter fails to obtain bandwidth. */
    bool                     fChoked;
    /** The driver this filter is aggregated into. */
    PPDMINETWORKDOWN         pIDrvNet;
} PDMNSFILTER;

/** @defgroup grp_pdm_net_shaper  The PDM Network Shaper API
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM filter handle. */
typedef struct PDMNSFILTER *PPDMNSFILTER;
/** Pointer to a network shaper. */
typedef struct PDMNETSHAPER *PPDMNETSHAPER;


/**
 * Obtain bandwidth in a bandwidth group.
 *
 * @returns VBox status code.
 * @param   pFilter         Pointer to the filter that allocates bandwidth.
 * @param   cbTransfer      Number of bytes to allocate.
 */
VMMR3DECL(bool) PDMR3NsAllocateBandwidth(PPDMNSFILTER pFilter, uint32_t cbTransfer);

/**
 * Attach network filter driver from bandwidth group.
 *
 * @returns VBox status code.
 * @param   pVM             Handle of VM.
 * @param   pDrvIns         The driver instance.
 * @param   pcszBwGroup     Name of the bandwidth group to attach to.
 * @param   pFilter         Pointer to the filter we attach.
 */
VMMR3DECL(int) PDMR3NsAttach(PVM pVM, PPDMDRVINS pDrvIns, const char *pcszBwGroup, PPDMNSFILTER pFilter);

/**
 * Detach network filter driver from bandwidth group.
 *
 * @returns VBox status code.
 * @param   pVM             Handle of VM.
 * @param   pDrvIns         The driver instance.
 * @param   pFilter         Pointer to the filter we detach.
 */
VMMR3DECL(int) PDMR3NsDetach(PVM pVM, PPDMDRVINS pDrvIns, PPDMNSFILTER pFilter);

/**
 * Adjusts the maximum rate for the bandwidth group.
 *
 * @returns VBox status code.
 * @param   pVM                   Handle of VM.
 * @param   pcszBwGroup           Name of the bandwidth group to attach to.
 * @param   cbTransferPerSecMax   Maximum number of bytes per second to be transmitted.
 */
VMMR3DECL(int) PDMR3NsBwGroupSetLimit(PVM pVM, const char *pcszBwGroup, uint64_t cbTransferPerSecMax);

/** @} */

RT_C_DECLS_END

#endif

