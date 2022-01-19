/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-0 Windows backend.
 */

/*
 * Copyright (C) 2018-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NEM
#define VMCPU_INCL_CPUM_GST_CTX
#define IsEqualLocaleName(a, b) (0) /* W10 WDK hack, the header wants _wcsicmp  */
#include <iprt/nt/nt.h>
#include <iprt/nt/hyperv.h>
#include <iprt/nt/vid.h>
#include <winerror.h>

#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/dbgftrace.h>
#include "NEMInternal.h"
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/param.h>

#include <iprt/ctype.h>
#include <iprt/critsect.h>
#include <iprt/dbg.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/string.h>
#include <iprt/time.h>
#define PIMAGE_NT_HEADERS32 PIMAGE_NT_HEADERS32_PECOFF
#include <iprt/formats/pecoff.h>


/* Assert compile context sanity. */
#ifndef RT_OS_WINDOWS
# error "Windows only file!"
#endif
#ifndef RT_ARCH_AMD64
# error "AMD64 only file!"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
typedef uint32_t DWORD; /* for winerror.h constants */


/*
 * Instantate the code we share with ring-0.
 */
#undef NEM_WIN_TEMPLATE_MODE_OWN_RUN_API
#include "../VMMAll/NEMAllNativeTemplate-win.cpp.h"


/**
 * Module initialization for NEM.
 */
VMMR0_INT_DECL(int)  NEMR0Init(void)
{
    return VINF_SUCCESS;
}


/**
 * Module termination for NEM.
 */
VMMR0_INT_DECL(void) NEMR0Term(void)
{
}


/**
 * Called by NEMR3Init to make sure we've got what we need.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) NEMR0InitVM(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->nemr0.s) <= sizeof(pGVM->nemr0.padding));
    AssertCompile(sizeof(pGVM->aCpus[0].nemr0.s) <= sizeof(pGVM->aCpus[0].nemr0.padding));

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    return rc;
}


/**
 * 2nd part of the initialization, after we've got a partition handle.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) NEMR0InitVMPart2(PGVM pGVM)
{
    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    return rc;
}


/**
 * Cleanup the NEM parts of the VM in ring-0.
 *
 * This is always called and must deal the state regardless of whether
 * NEMR0InitVM() was called or not.  So, take care here.
 *
 * @param   pGVM            The ring-0 VM handle.
 */
VMMR0_INT_DECL(void) NEMR0CleanupVM(PGVM pGVM)
{
    RT_NOREF(pGVM);
}


/**
 * Maps pages into the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0MapPages(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return  VERR_NOT_IMPLEMENTED;
}


/**
 * Unmaps pages from the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0UnmapPages(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return  VERR_NOT_IMPLEMENTED;
}


/**
 * Export the state to the native API (out of CPUMCTX).
 *
 * @returns VBox status code
 * @param   pGVM        The ring-0 VM handle.
 * @param   idCpu       The calling EMT.  Necessary for getting the
 *                      hypercall page and arguments.
 */
VMMR0_INT_DECL(int)  NEMR0ExportState(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Import the state from the native API (back to CPUMCTX).
 *
 * @returns VBox status code
 * @param   pGVM        The ring-0 VM handle.
 * @param   idCpu       The calling EMT.  Necessary for getting the
 *                      hypercall page and arguments.
 * @param   fWhat       What to import, CPUMCTX_EXTRN_XXX. Set
 *                      CPUMCTX_EXTERN_ALL for everything.
 */
VMMR0_INT_DECL(int) NEMR0ImportState(PGVM pGVM, VMCPUID idCpu, uint64_t fWhat)
{
    RT_NOREF(pGVM, idCpu, fWhat);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Queries the TSC and TSC_AUX values, putting the results in .
 *
 * @returns VBox status code
 * @param   pGVM        The ring-0 VM handle.
 * @param   idCpu       The calling EMT.  Necessary for getting the
 *                      hypercall page and arguments.
 */
VMMR0_INT_DECL(int) NEMR0QueryCpuTick(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets the TSC register to @a uPausedTscValue on all CPUs.
 *
 * @returns VBox status code
 * @param   pGVM            The ring-0 VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @param   uPausedTscValue The TSC value at the time of pausing.
 */
VMMR0_INT_DECL(int) NEMR0ResumeCpuTickOnAll(PGVM pGVM, VMCPUID idCpu, uint64_t uPausedTscValue)
{
    RT_NOREF(pGVM, idCpu, uPausedTscValue);
    return VERR_NOT_IMPLEMENTED;
}


VMMR0_INT_DECL(VBOXSTRICTRC) NEMR0RunGuestCode(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Updates statistics in the VM structure.
 *
 * @returns VBox status code.
 * @param   pGVM        The ring-0 VM handle.
 * @param   idCpu       The calling EMT, or NIL.  Necessary for getting the hypercall
 *                      page and arguments.
 */
VMMR0_INT_DECL(int)  NEMR0UpdateStatistics(PGVM pGVM, VMCPUID idCpu)
{
    RT_NOREF(pGVM, idCpu);
    return VINF_SUCCESS;
}


/**
 * Debug only interface for poking around and exploring Hyper-V stuff.
 *
 * @param   pGVM        The ring-0 VM handle.
 * @param   idCpu       The calling EMT.
 * @param   u64Arg      What to query.  0 == registers.
 */
VMMR0_INT_DECL(int) NEMR0DoExperiment(PGVM pGVM, VMCPUID idCpu, uint64_t u64Arg)
{
    RT_NOREF(pGVM, idCpu, u64Arg);
    return VERR_NOT_SUPPORTED;
}

