/* $Id$ */
/** @file
 * TRPM - Internal header file.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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

/** First interrupt handler. Used for validating input. */
#define TRPM_HANDLER_INT_BASE  0x20


/** @name   TRPMGCTrapIn* flags.
 * The lower bits are offsets into the CPUMCTXCORE structure.
 * @{ */
/** The mask for the operation. */
#define TRPM_TRAP_IN_OP_MASK    0xffff
/** Traps on MOV GS, eax. */
#define TRPM_TRAP_IN_MOV_GS     1
/** Traps on MOV FS, eax. */
#define TRPM_TRAP_IN_MOV_FS     2
/** Traps on MOV ES, eax. */
#define TRPM_TRAP_IN_MOV_ES     3
/** Traps on MOV DS, eax. */
#define TRPM_TRAP_IN_MOV_DS     4
/** Traps on IRET. */
#define TRPM_TRAP_IN_IRET       5
/** Set if this is a V86 resume. */
#define TRPM_TRAP_IN_V86        RT_BIT(30)
/** @} */


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
    RTGCUINT                uActiveErrorCode; /**< @todo don't use RTGCUINT */

    /** CR2 at the time of the active exception. */
    RTGCUINTPTR             uActiveCR2;

    /** Saved trap vector number. */
    RTGCUINT                uSavedVector; /**< @todo don't use RTGCUINT */

    /** Saved errorcode. */
    RTGCUINT                uSavedErrorCode;

    /** Saved cr2. */
    RTGCUINTPTR             uSavedCR2;

    /** Saved trap type. */
    TRPMEVENT               enmSavedType;

    /** Instruction length for software interrupts and software exceptions
     * (\#BP, \#OF) */
    uint8_t                 cbInstr;

    /** Saved instruction length. */
    uint8_t                 cbSavedInstr;

    /** Padding. */
    uint8_t                 au8Padding[2];

    /** Previous trap vector # - for debugging. */
    RTGCUINT                uPrevVector;
} TRPMCPU;

/** Pointer to TRPMCPU Data. */
typedef TRPMCPU *PTRPMCPU;


#ifdef IN_RING0

/**
 * Calls the interrupt gate as if we received an interrupt while in Ring-0.
 *
 * @param   uIP     The interrupt gate IP.
 * @param   SelCS   The interrupt gate CS.
 * @param   RSP     The interrupt gate RSP. ~0 if no stack switch should take place. (only AMD64)
 */
DECLASM(void) trpmR0DispatchHostInterrupt(RTR0UINTPTR uIP, RTSEL SelCS, RTR0UINTPTR RSP);

/**
 * Issues a software interrupt to the specified interrupt vector.
 *
 * @param   uActiveVector   The vector number.
 */
DECLASM(void) trpmR0DispatchHostInterruptSimple(RTUINT uActiveVector);

#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_TRPMInternal_h */
