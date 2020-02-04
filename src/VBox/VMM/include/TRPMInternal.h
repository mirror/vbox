/* $Id$ */
/** @file
 * TRPM - Internal header file.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_TRPMInternal_h
#define VMM_INCLUDED_SRC_include_TRPMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_trpm_int   Internals
 * @ingroup grp_trpm
 * @internal
 * @{
 */

/**
 * TRPM Data (part of VM)
 *
 * @note This used to be a big deal when we had raw-mode, now it's a dud. :-)
 */
typedef struct TRPM
{
#ifdef VBOX_WITH_STATISTICS
    /** Statistics for interrupt handlers (allocated on the hypervisor heap) - R3
     * pointer. */
    R3PTRTYPE(PSTAMCOUNTER) paStatForwardedIRQR3;
#endif
    uint64_t                u64Dummy;
} TRPM;

/** Pointer to TRPM Data. */
typedef TRPM *PTRPM;


/**
 * Per CPU data for TRPM.
 */
typedef struct TRPMCPU
{
    /** Active Interrupt or trap vector number.
     * If not UINT32_MAX this indicates that we're currently processing a
     * interrupt, trap, fault, abort, whatever which have arrived at that
     * vector number.
     */
    uint32_t                uActiveVector;

    /** Active trap type. */
    TRPMEVENT               enmActiveType;

    /** Errorcode for the active interrupt/trap. */
    uint32_t                uActiveErrorCode;

    /** Instruction length for software interrupts and software exceptions
     * (\#BP, \#OF) */
    uint8_t                 cbInstr;

    /** Whether this \#DB trap is caused due to INT1/ICEBP. */
    bool                    fIcebp;

    /** CR2 at the time of the active exception. */
    RTGCUINTPTR             uActiveCR2;
} TRPMCPU;

/** Pointer to TRPMCPU Data. */
typedef TRPMCPU *PTRPMCPU;
/** Pointer to const TRPMCPU Data. */
typedef const TRPMCPU *PCTRPMCPU;

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_TRPMInternal_h */
