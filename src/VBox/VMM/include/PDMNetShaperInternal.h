/* $Id$ */
/** @file
 * PDM Network Shaper - Internal data structures and functions common for both R0 and R3 parts.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
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

/**
 * Bandwidth group instance data
 */
typedef struct PDMNSBWGROUP
{
    /** Pointer to the next group in the list. */
    R3PTRTYPE(struct PDMNSBWGROUP *)            pNextR3;
    /** Pointer to the shared UVM structure. */
    R3PTRTYPE(struct PDMNETSHAPER *)            pShaperR3;
    /** Critical section protecting all members below. */
    PDMCRITSECT                                 Lock;
    /** Pointer to the first filter attached to this group. */
    R3PTRTYPE(struct PDMNSFILTER *)             pFiltersHeadR3;
    /** Bandwidth group name. */
    R3PTRTYPE(char *)                           pszNameR3;
    /** Maximum number of bytes filters are allowed to transfer. */
    volatile uint64_t                           cbPerSecMax;
    /** Number of bytes we are allowed to transfer in one burst. */
    volatile uint32_t                           cbBucket;
    /** Number of bytes we were allowed to transfer at the last update. */
    volatile uint32_t                           cbTokensLast;
    /** Timestamp of the last update */
    volatile uint64_t                           tsUpdatedLast;
    /** Reference counter - How many filters are associated with this group. */
    volatile uint32_t                           cRefs;
} PDMNSBWGROUP;
/** Pointer to a bandwidth group. */
typedef PDMNSBWGROUP *PPDMNSBWGROUP;


