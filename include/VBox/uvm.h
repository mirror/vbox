/* $Id$ */
/** @file
 * GVM - The Global VM Data.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#ifndef ___VBox_uvm_h
#define ___VBox_uvm_h

#include <VBox/types.h>


/**
 * Per virtual CPU ring-3 (user mode) data.
 */
typedef struct UVMCPU
{
    /** Pointer to the UVM structure.  */
    PUVM                            pUVM;
    /** Pointer to the VM structure.  */
    PVM                             pVM;
    /** Pointer to the VMCPU structure.  */
    PVMCPU                          pVCpu;
    /** The virtual CPU ID.  */
    RTCPUID                         idCpu;

    /** The VM internal data. */
    struct
    {
#ifdef ___VMInternal_h
        struct VMINTUSERPERVMCPU    s;
#endif
        uint8_t                     padding[768];
    } vm;
} UVMCPU;
/** Pointer to the per virtual CPU ring-3 (user mode) data. */
typedef UVMCPU *PUVMCPU;


/**
 * The ring-3 (user mode) VM structure.
 *
 * This structure is similar to VM and GVM except that it resides in swappable
 * user memory. The main purpose is to assist bootstrapping, where it allows us
 * to start EMT much earlier and gives PDMLdr somewhere to put it's VMMR0 data.
 * It is also a nice place to put big things that are user mode only.
 */
typedef struct UVM
{
    /** Magic / eye-catcher (UVM_MAGIC). */
    uint32_t        u32Magic;
    /** The number of virtual CPUs. */
    uint32_t        cCpus;
    /** The ring-3 mapping of the shared VM structure. */
    PVM             pVM;
    /** Pointer to the next VM.
     * We keep a per process list of VM for the event that a process could
     * contain more than one VM.
     * @todo move this into vm.s!
     */
    struct UVM     *pNext;

    /** The VM internal data. */
    struct
    {
#ifdef ___VMInternal_h
        struct VMINTUSERPERVM   s;
#endif
        uint8_t                 padding[768];
    } vm;

    /** The MM data. */
    struct
    {
#ifdef ___MMInternal_h
        struct MMUSERPERVM      s;
#endif
        uint8_t                 padding[32];
    } mm;

    /** The PDM data. */
    struct
    {
#ifdef ___PDMInternal_h
        struct PDMUSERPERVM     s;
#endif
        uint8_t                 padding[32];
    } pdm;

    /** The STAM data. */
    struct
    {
#ifdef ___STAMInternal_h
        struct STAMUSERPERVM    s;
#endif
        uint8_t                 padding[256];
    } stam;

    /** Per virtual CPU data. */
    UVMCPU                      aCpus[1];
} UVM;

/** The UVM::u32Magic value (Brad Mehldau). */
#define UVM_MAGIC       0x19700823

#endif

