/** @file
 * PDM - Pluggable Device Manager, Network Shaper.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vmm_pdmnetshaper_h
#define VBOX_INCLUDED_vmm_pdmnetshaper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/vmm/pdmnetifs.h>
#include <iprt/list.h>
#include <iprt/sg.h>


/** @defgroup grp_pdm_net_shaper  The PDM Network Shaper API
 * @ingroup grp_pdm
 * @{
 */


#define PDM_NETSHAPER_MIN_BUCKET_SIZE UINT32_C(65536) /**< bytes */
#define PDM_NETSHAPER_MAX_LATENCY     UINT32_C(100)   /**< milliseconds */

RT_C_DECLS_BEGIN

/**
 * A network shaper filter entry.
 *
 * This is used by DrvNetShaper and any similar drivers.
 */
typedef struct PDMNSFILTER
{
    /** Entry in the group's filter list.
     * Both members are NULL when not associated with a group. */
    RTLISTNODER3                        ListEntry;
    /** The group index + 1.
     * @note For safety reasons the value zero is invalid and this is 1-based
     *       (like pascal) rather than 0-based indexing.
     * @note Volatile to prevent re-reading after validation. */
    uint32_t volatile                   iGroup;
    /** Set when the filter fails to obtain bandwidth.
     * This will then cause pIDrvNetR3 to be called before long.  */
    bool                                fChoked;
    /** Aligment padding. */
    bool                                afPadding[3];
    /** The driver this filter is aggregated into (ring-3). */
    R3PTRTYPE(PPDMINETWORKDOWN)         pIDrvNetR3;
} PDMNSFILTER;

VMM_INT_DECL(bool)      PDMNetShaperAllocateBandwidth(PVMCC pVM, PPDMNSFILTER pFilter, size_t cbTransfer);
VMMR3_INT_DECL(int)     PDMR3NsAttach(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, PPDMNSFILTER pFilter);
VMMR3_INT_DECL(int)     PDMR3NsDetach(PVM pVM, PPDMDRVINS pDrvIns, PPDMNSFILTER pFilter);
VMMR3DECL(int)          PDMR3NsBwGroupSetLimit(PUVM pUVM, const char *pszName, uint64_t cbPerSecMax);

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_pdmnetshaper_h */

