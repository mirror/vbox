/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-0 Windows backend.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include <iprt/nt/nt.h>
#include <iprt/nt/hyperv.h>
#include <iprt/nt/vid.h>

#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/apic.h>
#include "NEMInternal.h"
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/param.h>

#include <iprt/dbg.h>
#include <iprt/memobj.h>
#include <iprt/string.h>


/* Assert compile context sanity. */
#ifndef RT_OS_WINDOWS
# error "Windows only file!"
#endif
#ifndef RT_ARCH_AMD64
# error "AMD64 only file!"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static uint64_t (*g_pfnHvlInvokeHypercall)(uint64_t uCallInfo, uint64_t HCPhysInput, uint64_t HCPhysOutput);


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
NEM_TMPL_STATIC int nemR0WinMapPages(PGVM pGVM, PVM pVM, PGVMCPU pGVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst,
                                     uint32_t cPages, uint32_t fFlags);
NEM_TMPL_STATIC int nemR0WinUnmapPages(PGVM pGVM, PGVMCPU pGVCpu, RTGCPHYS GCPhys, uint32_t cPages);


/*
 * Instantate the code we share with ring-0.
 */
#include "../VMMAll/NEMAllNativeTemplate-win.cpp.h"


/**
 * Called by NEMR3Init to make sure we've got what we need.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) NEMR0InitVM(PGVM pGVM, PVM pVM)
{
    AssertCompile(sizeof(pGVM->nem.s) <= sizeof(pGVM->nem.padding));
    AssertCompile(sizeof(pGVM->aCpus[0].nem.s) <= sizeof(pGVM->aCpus[0].nem.padding));

    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, 0);
    AssertRCReturn(rc, rc);

    /*
     * We want to perform hypercalls here.  The NT kernel started to expose a very low
     * level interface to do this thru somewhere between build 14271 and 16299.  Since
     * we need build 17083 to get anywhere at all, the exact build is not relevant here.
     */
    RTDBGKRNLINFO hKrnlInfo;
    rc = RTR0DbgKrnlInfoOpen(&hKrnlInfo, 0);
    if (RT_SUCCESS(rc))
    {
        rc = RTR0DbgKrnlInfoQuerySymbol(hKrnlInfo, NULL, "HvlInvokeHypercall", (void **)&g_pfnHvlInvokeHypercall);
        RTR0DbgKrnlInfoRelease(hKrnlInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Allocate a page for each VCPU to place hypercall data on.
             */
            for (VMCPUID i = 0; i < pGVM->cCpus; i++)
            {
                PGVMCPU pGVCpu = &pGVM->aCpus[i];
                rc = RTR0MemObjAllocPage(&pGVCpu->nem.s.hHypercallDataMemObj, PAGE_SIZE, false /*fExecutable*/);
                if (RT_SUCCESS(rc))
                {
                    pGVCpu->nem.s.HCPhysHypercallData = RTR0MemObjGetPagePhysAddr(pGVCpu->nem.s.hHypercallDataMemObj, 0 /*iPage*/);
                    pGVCpu->nem.s.pbHypercallData     = (uint8_t *)RTR0MemObjAddress(pGVCpu->nem.s.hHypercallDataMemObj);
                    AssertStmt(pGVCpu->nem.s.HCPhysHypercallData != NIL_RTHCPHYS, rc = VERR_INTERNAL_ERROR_3);
                    AssertStmt(pGVCpu->nem.s.pbHypercallData, rc = VERR_INTERNAL_ERROR_3);
                }
                else
                    pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
                if (RT_FAILURE(rc))
                {
                    /* bail. */
                    do
                    {
                        RTR0MemObjFree(pGVCpu->nem.s.hHypercallDataMemObj, true /*fFreeMappings*/);
                        pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
                        pGVCpu->nem.s.HCPhysHypercallData  = NIL_RTHCPHYS;
                        pGVCpu->nem.s.pbHypercallData      = NULL;
                    } while (i-- > 0);
                    return rc;
                }
            }
            /*
             * So far, so good.
             */
            return rc;
        }

        rc = VERR_NEM_MISSING_KERNEL_API;
    }

    RT_NOREF(pGVM, pVM);
    return rc;
}


/**
 * Perform an I/O control operation on the partition handle (VID.SYS).
 *
 * @returns NT status code.
 * @param   pGVM            The ring-0 VM structure.
 * @param   uFunction       The function to perform.
 * @param   pvInput         The input buffer.  This must point within the VM
 *                          structure so we can easily convert to a ring-3
 *                          pointer if necessary.
 * @param   cbInput         The size of the input.  @a pvInput must be NULL when
 *                          zero.
 * @param   pvOutput        The output buffer.  This must also point within the
 *                          VM structure for ring-3 pointer magic.
 * @param   cbOutput        The size of the output.  @a pvOutput must be NULL
 *                          when zero.
 */
DECLINLINE(NTSTATUS) nemR0NtPerformIoControl(PGVM pGVM, uint32_t uFunction, void *pvInput, uint32_t cbInput,
                                             void *pvOutput, uint32_t cbOutput)
{
#ifdef RT_STRICT
    /*
     * Input and output parameters are part of the VM CPU structure.
     */
    PVM          pVM  = pGVM->pVM;
    size_t const cbVM = RT_UOFFSETOF(VM, aCpus[pGVM->cCpus]);
    if (pvInput)
        AssertReturn(((uintptr_t)pvInput + cbInput) - (uintptr_t)pVM <= cbVM, VERR_INVALID_PARAMETER);
    if (pvOutput)
        AssertReturn(((uintptr_t)pvOutput + cbOutput) - (uintptr_t)pVM <= cbVM, VERR_INVALID_PARAMETER);
#endif

    int32_t rcNt = STATUS_UNSUCCESSFUL;
    int rc = SUPR0IoCtlPerform(pGVM->nem.s.pIoCtlCtx, uFunction,
                               pvInput,
                               pvInput  ? (uintptr_t)pvInput  + pGVM->nem.s.offRing3ConversionDelta : NIL_RTR3PTR,
                               cbInput,
                               pvOutput,
                               pvOutput ? (uintptr_t)pvOutput + pGVM->nem.s.offRing3ConversionDelta : NIL_RTR3PTR,
                               cbOutput,
                               &rcNt);
    if (RT_SUCCESS(rc) || !NT_SUCCESS((NTSTATUS)rcNt))
        return (NTSTATUS)rcNt;
    return STATUS_UNSUCCESSFUL;
}


/**
 * 2nd part of the initialization, after we've got a partition handle.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) NEMR0InitVMPart2(PGVM pGVM, PVM pVM)
{
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, 0);
    AssertRCReturn(rc, rc);
    SUPR0Printf("NEMR0InitVMPart2\n"); LogRel(("2: NEMR0InitVMPart2\n"));

    /*
     * Copy and validate the I/O control information from ring-3.
     */
    NEMWINIOCTL Copy = pVM->nem.s.IoCtlGetHvPartitionId;
    AssertLogRelReturn(Copy.uFunction != 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbInput == 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbOutput == sizeof(HV_PARTITION_ID), VERR_NEM_INIT_FAILED);
    pGVM->nem.s.IoCtlGetHvPartitionId = Copy;

    Copy = pVM->nem.s.IoCtlStartVirtualProcessor;
    AssertLogRelReturn(Copy.uFunction != 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbInput == sizeof(HV_VP_INDEX), VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbOutput == 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlGetHvPartitionId.uFunction, VERR_NEM_INIT_FAILED);
    pGVM->nem.s.IoCtlStartVirtualProcessor = Copy;

    Copy = pVM->nem.s.IoCtlStopVirtualProcessor;
    AssertLogRelReturn(Copy.uFunction != 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbInput == sizeof(HV_VP_INDEX), VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbOutput == 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlGetHvPartitionId.uFunction, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlStartVirtualProcessor.uFunction, VERR_NEM_INIT_FAILED);
    pGVM->nem.s.IoCtlStopVirtualProcessor = Copy;

    Copy = pVM->nem.s.IoCtlMessageSlotHandleAndGetNext;
    AssertLogRelReturn(Copy.uFunction != 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbInput == sizeof(VID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT), VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.cbOutput == 0, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlGetHvPartitionId.uFunction, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlStartVirtualProcessor.uFunction, VERR_NEM_INIT_FAILED);
    AssertLogRelReturn(Copy.uFunction != pGVM->nem.s.IoCtlStopVirtualProcessor.uFunction, VERR_NEM_INIT_FAILED);
    pGVM->nem.s.IoCtlMessageSlotHandleAndGetNext = Copy;

    /*
     * Setup of an I/O control context for the partition handle for later use.
     */
    rc = SUPR0IoCtlSetupForHandle(pGVM->pSession, pVM->nem.s.hPartitionDevice, 0, &pGVM->nem.s.pIoCtlCtx);
    AssertLogRelRCReturn(rc, rc);
    pGVM->nem.s.offRing3ConversionDelta = (uintptr_t)pVM->pVMR3 - (uintptr_t)pGVM->pVM;

    /*
     * Get the partition ID.
     */
    PVMCPU pVCpu = &pGVM->pVM->aCpus[0];
    NTSTATUS rcNt = nemR0NtPerformIoControl(pGVM, pGVM->nem.s.IoCtlGetHvPartitionId.uFunction, NULL, 0,
                                            &pVCpu->nem.s.uIoCtlBuf.idPartition, sizeof(pVCpu->nem.s.uIoCtlBuf.idPartition));
    AssertLogRelMsgReturn(NT_SUCCESS(rcNt), ("IoCtlGetHvPartitionId failed: %#x\n", rcNt), VERR_NEM_INIT_FAILED);
    pGVM->nem.s.idHvPartition = pVCpu->nem.s.uIoCtlBuf.idPartition;
    AssertLogRelMsgReturn(pGVM->nem.s.idHvPartition == pVM->nem.s.idHvPartition,
                          ("idHvPartition mismatch: r0=%#RX64, r3=%#RX64\n", pGVM->nem.s.idHvPartition, pVM->nem.s.idHvPartition),
                          VERR_NEM_INIT_FAILED);


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
    pGVM->nem.s.idHvPartition = HV_PARTITION_ID_INVALID;

    /* Clean up I/O control context. */
    if (pGVM->nem.s.pIoCtlCtx)
    {
        int rc = SUPR0IoCtlCleanup(pGVM->nem.s.pIoCtlCtx);
        AssertRC(rc);
        pGVM->nem.s.pIoCtlCtx = NULL;
    }

    /* Free the hypercall pages. */
    VMCPUID i = pGVM->cCpus;
    while (i-- > 0)
    {
        PGVMCPU pGVCpu = &pGVM->aCpus[i];
        if (pGVCpu->nem.s.pbHypercallData)
        {
            pGVCpu->nem.s.pbHypercallData = NULL;
            int rc = RTR0MemObjFree(pGVCpu->nem.s.hHypercallDataMemObj, true /*fFreeMappings*/);
            AssertRC(rc);
        }
        pGVCpu->nem.s.hHypercallDataMemObj = NIL_RTR0MEMOBJ;
        pGVCpu->nem.s.HCPhysHypercallData  = NIL_RTHCPHYS;
    }
}


#if 0 /* for debugging GPA unmapping.  */
static int nemR3WinDummyReadGpa(PGVM pGVM, PGVMCPU pGVCpu, RTGCPHYS GCPhys)
{
    PHV_INPUT_READ_GPA  pIn  = (PHV_INPUT_READ_GPA)pGVCpu->nem.s.pbHypercallData;
    PHV_OUTPUT_READ_GPA pOut = (PHV_OUTPUT_READ_GPA)(pIn + 1);
    pIn->PartitionId            = pGVM->nem.s.idHvPartition;
    pIn->VpIndex                = pGVCpu->idCpu;
    pIn->ByteCount              = 0x10;
    pIn->BaseGpa                = GCPhys;
    pIn->ControlFlags.AsUINT64  = 0;
    pIn->ControlFlags.CacheType = HvCacheTypeX64WriteCombining;
    memset(pOut, 0xfe, sizeof(*pOut));
    uint64_t volatile uResult = g_pfnHvlInvokeHypercall(HvCallReadGpa, pGVCpu->nem.s.HCPhysHypercallData,
                                                        pGVCpu->nem.s.HCPhysHypercallData + sizeof(*pIn));
    LogRel(("nemR3WinDummyReadGpa: %RGp -> %#RX64; code=%u rsvd=%u abData=%.16Rhxs\n",
            GCPhys, uResult, pOut->AccessResult.ResultCode, pOut->AccessResult.Reserved, pOut->Data));
    __debugbreak();

    return uResult != 0 ? VERR_READ_ERROR : VINF_SUCCESS;
}
#endif


/**
 * Worker for NEMR0MapPages and others.
 */
NEM_TMPL_STATIC int nemR0WinMapPages(PGVM pGVM, PVM pVM, PGVMCPU pGVCpu, RTGCPHYS GCPhysSrc, RTGCPHYS GCPhysDst,
                                     uint32_t cPages, uint32_t fFlags)
{
    /*
     * Validate.
     */
    AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

    AssertReturn(cPages > 0, VERR_OUT_OF_RANGE);
    AssertReturn(cPages <= NEM_MAX_MAP_PAGES, VERR_OUT_OF_RANGE);
    AssertReturn(!(fFlags & ~(HV_MAP_GPA_MAYBE_ACCESS_MASK & ~HV_MAP_GPA_DUNNO_ACCESS)), VERR_INVALID_FLAGS);
    AssertMsgReturn(!(GCPhysDst & X86_PAGE_OFFSET_MASK), ("GCPhysDst=%RGp\n", GCPhysDst), VERR_OUT_OF_RANGE);
    AssertReturn(GCPhysDst < _1E, VERR_OUT_OF_RANGE);
    if (GCPhysSrc != GCPhysDst)
    {
        AssertMsgReturn(!(GCPhysSrc & X86_PAGE_OFFSET_MASK), ("GCPhysSrc=%RGp\n", GCPhysSrc), VERR_OUT_OF_RANGE);
        AssertReturn(GCPhysSrc < _1E, VERR_OUT_OF_RANGE);
    }

    /*
     * Compose and make the hypercall.
     * Ring-3 is not allowed to fill in the host physical addresses of the call.
     */
    HV_INPUT_MAP_GPA_PAGES *pMapPages = (HV_INPUT_MAP_GPA_PAGES *)pGVCpu->nem.s.pbHypercallData;
    AssertPtrReturn(pMapPages, VERR_INTERNAL_ERROR_3);
    pMapPages->TargetPartitionId    = pGVM->nem.s.idHvPartition;
    pMapPages->TargetGpaBase        = GCPhysDst >> X86_PAGE_SHIFT;
    pMapPages->MapFlags             = fFlags;
    pMapPages->u32ExplicitPadding   = 0;
    for (uint32_t iPage = 0; iPage < cPages; iPage++, GCPhysSrc += X86_PAGE_SIZE)
    {
        RTHCPHYS HCPhys = NIL_RTGCPHYS;
        int rc = PGMPhysGCPhys2HCPhys(pVM, GCPhysSrc, &HCPhys);
        AssertRCReturn(rc, rc);
        pMapPages->PageList[iPage] = HCPhys >> X86_PAGE_SHIFT;
    }

    uint64_t uResult = g_pfnHvlInvokeHypercall(HvCallMapGpaPages | ((uint64_t)cPages << 32),
                                               pGVCpu->nem.s.HCPhysHypercallData, 0);
    Log6(("NEMR0MapPages: %RGp/%RGp L %u prot %#x -> %#RX64\n",
          GCPhysDst, GCPhysSrc - cPages * X86_PAGE_SIZE, cPages, fFlags, uResult));
    if (uResult == ((uint64_t)cPages << 32))
        return VINF_SUCCESS;

    LogRel(("g_pfnHvlInvokeHypercall/MapGpaPages -> %#RX64\n", uResult));
    return VERR_NEM_MAP_PAGES_FAILED;
}


/**
 * Maps pages into the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0MapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu)
{
    /*
     * Unpack the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];

        RTGCPHYS const          GCPhysSrc = pVCpu->nem.s.Hypercall.MapPages.GCPhysSrc;
        RTGCPHYS const          GCPhysDst = pVCpu->nem.s.Hypercall.MapPages.GCPhysDst;
        uint32_t const          cPages    = pVCpu->nem.s.Hypercall.MapPages.cPages;
        HV_MAP_GPA_FLAGS const  fFlags    = pVCpu->nem.s.Hypercall.MapPages.fFlags;

        /*
         * Do the work.
         */
        rc = nemR0WinMapPages(pGVM, pVM, pGVCpu, GCPhysSrc, GCPhysDst, cPages, fFlags);
    }
    return rc;
}


/**
 * Worker for NEMR0UnmapPages and others.
 */
NEM_TMPL_STATIC int nemR0WinUnmapPages(PGVM pGVM, PGVMCPU pGVCpu, RTGCPHYS GCPhys, uint32_t cPages)
{
    /*
     * Validate input.
     */
    AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

    AssertReturn(cPages > 0, VERR_OUT_OF_RANGE);
    AssertReturn(cPages <= NEM_MAX_UNMAP_PAGES, VERR_OUT_OF_RANGE);
    AssertMsgReturn(!(GCPhys & X86_PAGE_OFFSET_MASK), ("%RGp\n", GCPhys), VERR_OUT_OF_RANGE);
    AssertReturn(GCPhys < _1E, VERR_OUT_OF_RANGE);

    /*
     * Compose and make the hypercall.
     */
    HV_INPUT_UNMAP_GPA_PAGES *pUnmapPages = (HV_INPUT_UNMAP_GPA_PAGES *)pGVCpu->nem.s.pbHypercallData;
    AssertPtrReturn(pUnmapPages, VERR_INTERNAL_ERROR_3);
    pUnmapPages->TargetPartitionId    = pGVM->nem.s.idHvPartition;
    pUnmapPages->TargetGpaBase        = GCPhys >> X86_PAGE_SHIFT;
    pUnmapPages->fFlags               = 0;

    uint64_t uResult = g_pfnHvlInvokeHypercall(HvCallUnmapGpaPages | ((uint64_t)cPages << 32),
                                               pGVCpu->nem.s.HCPhysHypercallData, 0);
    Log6(("NEMR0UnmapPages: %RGp L %u -> %#RX64\n", GCPhys, cPages, uResult));
    if (uResult == ((uint64_t)cPages << 32))
    {
#if 1       /* Do we need to do this? Hopefully not... */
        uint64_t volatile uR = g_pfnHvlInvokeHypercall(HvCallUncommitGpaPages | ((uint64_t)cPages << 32),
                                                       pGVCpu->nem.s.HCPhysHypercallData, 0);
        AssertMsg(uR == ((uint64_t)cPages << 32), ("uR=%#RX64\n", uR));
#endif
        return VINF_SUCCESS;
    }

    LogRel(("g_pfnHvlInvokeHypercall/UnmapGpaPages -> %#RX64\n", uResult));
    return VERR_NEM_UNMAP_PAGES_FAILED;
}


/**
 * Unmaps pages from the guest physical address space.
 *
 * Generally the caller will be under the PGM lock already, so no extra effort
 * is needed to make sure all changes happens under it.
 *
 * @returns VBox status code.
 * @param   pGVM            The ring-0 VM handle.
 * @param   pVM             The cross context VM handle.
 * @param   idCpu           The calling EMT.  Necessary for getting the
 *                          hypercall page and arguments.
 * @thread  EMT(idCpu)
 */
VMMR0_INT_DECL(int) NEMR0UnmapPages(PGVM pGVM, PVM pVM, VMCPUID idCpu)
{
    /*
     * Unpack the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];

        RTGCPHYS const GCPhys = pVCpu->nem.s.Hypercall.UnmapPages.GCPhys;
        uint32_t const cPages = pVCpu->nem.s.Hypercall.UnmapPages.cPages;

        /*
         * Do the work.
         */
        rc = nemR0WinUnmapPages(pGVM, pGVCpu, GCPhys, cPages);
    }
    return rc;
}


/**
 * Worker for NEMR0ExportState.
 *
 * Intention is to use it internally later.
 *
 * @returns VBox status code.
 * @param   pGVM        The ring-0 VM handle.
 * @param   pGVCpu      The irng-0 VCPU handle.
 * @param   pCtx        The CPU context structure to import into.
 * @param   fWhat       What to export. To be defined, UINT64_MAX for now.
 */
static int nemR0WinExportState(PGVM pGVM, PGVMCPU pGVCpu, PCPUMCTX pCtx, uint64_t fWhat)
{
    PVMCPU                     pVCpu  = &pGVM->pVM->aCpus[pGVCpu->idCpu];
    HV_INPUT_SET_VP_REGISTERS *pInput = (HV_INPUT_SET_VP_REGISTERS *)pGVCpu->nem.s.pbHypercallData;
    AssertPtrReturn(pInput, VERR_INTERNAL_ERROR_3);

    pInput->PartitionId = pGVM->nem.s.idHvPartition;
    pInput->VpIndex     = pGVCpu->idCpu;
    pInput->RsvdZ       = 0;

    RT_NOREF_PV(fWhat); /** @todo group selection. */

    /* GPRs */
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[0]);
    pInput->Elements[0].Name                = HvX64RegisterRax;
    pInput->Elements[0].Value.Reg64         = pCtx->rax;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[1]);
    pInput->Elements[1].Name                = HvX64RegisterRcx;
    pInput->Elements[1].Value.Reg64         = pCtx->rcx;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[2]);
    pInput->Elements[2].Name                = HvX64RegisterRdx;
    pInput->Elements[2].Value.Reg64         = pCtx->rdx;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[3]);
    pInput->Elements[3].Name                = HvX64RegisterRbx;
    pInput->Elements[3].Value.Reg64         = pCtx->rbx;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[4]);
    pInput->Elements[4].Name                = HvX64RegisterRsp;
    pInput->Elements[4].Value.Reg64         = pCtx->rsp;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[5]);
    pInput->Elements[5].Name                = HvX64RegisterRbp;
    pInput->Elements[5].Value.Reg64         = pCtx->rbp;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[6]);
    pInput->Elements[6].Name                = HvX64RegisterRsi;
    pInput->Elements[6].Value.Reg64         = pCtx->rsi;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[7]);
    pInput->Elements[7].Name                = HvX64RegisterRdi;
    pInput->Elements[7].Value.Reg64         = pCtx->rdi;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[8]);
    pInput->Elements[8].Name                = HvX64RegisterR8;
    pInput->Elements[8].Value.Reg64         = pCtx->r8;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[9]);
    pInput->Elements[9].Name                = HvX64RegisterR9;
    pInput->Elements[9].Value.Reg64         = pCtx->r9;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[10]);
    pInput->Elements[10].Name                = HvX64RegisterR10;
    pInput->Elements[10].Value.Reg64         = pCtx->r10;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[11]);
    pInput->Elements[11].Name                = HvX64RegisterR11;
    pInput->Elements[11].Value.Reg64         = pCtx->r11;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[12]);
    pInput->Elements[12].Name                = HvX64RegisterR12;
    pInput->Elements[12].Value.Reg64         = pCtx->r12;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[13]);
    pInput->Elements[13].Name                = HvX64RegisterR13;
    pInput->Elements[13].Value.Reg64         = pCtx->r13;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[14]);
    pInput->Elements[14].Name                = HvX64RegisterR14;
    pInput->Elements[14].Value.Reg64         = pCtx->r14;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[15]);
    pInput->Elements[15].Name                = HvX64RegisterR15;
    pInput->Elements[15].Value.Reg64         = pCtx->r15;

    /* RIP & Flags */
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[16]);
    pInput->Elements[16].Name                = HvX64RegisterRip;
    pInput->Elements[16].Value.Reg64         = pCtx->rip;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[17]);
    pInput->Elements[17].Name                = HvX64RegisterRflags;
    pInput->Elements[17].Value.Reg64         = pCtx->rflags.u;

    /* Segments */
#define COPY_OUT_SEG(a_idx, a_enmName, a_SReg) \
        do { \
            HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[a_idx]); \
            pInput->Elements[a_idx].Name                     = a_enmName; \
            pInput->Elements[a_idx].Value.Segment.Base       = (a_SReg).u64Base; \
            pInput->Elements[a_idx].Value.Segment.Limit      = (a_SReg).u32Limit; \
            pInput->Elements[a_idx].Value.Segment.Selector   = (a_SReg).Sel; \
            pInput->Elements[a_idx].Value.Segment.Attributes = (a_SReg).Attr.u; \
        } while (0)
    COPY_OUT_SEG(18, HvX64RegisterEs,   pCtx->es);
    COPY_OUT_SEG(19, HvX64RegisterCs,   pCtx->cs);
    COPY_OUT_SEG(20, HvX64RegisterSs,   pCtx->ss);
    COPY_OUT_SEG(21, HvX64RegisterDs,   pCtx->ds);
    COPY_OUT_SEG(22, HvX64RegisterFs,   pCtx->fs);
    COPY_OUT_SEG(23, HvX64RegisterGs,   pCtx->gs);
    COPY_OUT_SEG(24, HvX64RegisterLdtr, pCtx->ldtr);
    COPY_OUT_SEG(25, HvX64RegisterTr,   pCtx->tr);

    uintptr_t iReg = 26;
    /* Descriptor tables. */
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Value.Table.Pad[0]   = 0;
    pInput->Elements[iReg].Value.Table.Pad[1]   = 0;
    pInput->Elements[iReg].Value.Table.Pad[2]   = 0;
    pInput->Elements[iReg].Name                 = HvX64RegisterIdtr;
    pInput->Elements[iReg].Value.Table.Limit    = pCtx->idtr.cbIdt;
    pInput->Elements[iReg].Value.Table.Base     = pCtx->idtr.pIdt;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Value.Table.Pad[0]   = 0;
    pInput->Elements[iReg].Value.Table.Pad[1]   = 0;
    pInput->Elements[iReg].Value.Table.Pad[2]   = 0;
    pInput->Elements[iReg].Name                 = HvX64RegisterGdtr;
    pInput->Elements[iReg].Value.Table.Limit    = pCtx->gdtr.cbGdt;
    pInput->Elements[iReg].Value.Table.Base     = pCtx->gdtr.pGdt;
    iReg++;

    /* Control registers. */
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCr0;
    pInput->Elements[iReg].Value.Reg64          = pCtx->cr0;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCr2;
    pInput->Elements[iReg].Value.Reg64          = pCtx->cr2;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCr3;
    pInput->Elements[iReg].Value.Reg64          = pCtx->cr3;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCr4;
    pInput->Elements[iReg].Value.Reg64          = pCtx->cr4;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCr8;
    pInput->Elements[iReg].Value.Reg64          = CPUMGetGuestCR8(pVCpu);
    iReg++;

    /* Debug registers. */
/** @todo fixme. Figure out what the hyper-v version of KVM_SET_GUEST_DEBUG would be. */
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr0;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR0(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[0];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr1;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR1(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr2;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR2(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[2];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr3;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR3(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[3];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr6;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR6(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[6];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterDr7;
    //pInput->Elements[iReg].Value.Reg64        = CPUMGetHyperDR7(pVCpu);
    pInput->Elements[iReg].Value.Reg64          = pCtx->dr[7];
    iReg++;

    /* Vector state. */
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm0;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[0].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[0].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm1;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[1].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[1].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm2;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[2].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[2].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm3;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[3].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[3].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm4;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[4].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[4].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm5;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[5].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[5].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm6;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[6].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[6].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm7;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[7].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[7].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm8;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[8].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[8].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm9;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[9].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[9].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm10;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[10].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[10].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm11;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[11].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[11].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm12;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[12].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[12].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm13;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[13].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[13].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm14;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[14].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[14].uXmm.s.Hi;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterXmm15;
    pInput->Elements[iReg].Value.Reg128.Low64   = pCtx->pXStateR0->x87.aXMM[15].uXmm.s.Lo;
    pInput->Elements[iReg].Value.Reg128.High64  = pCtx->pXStateR0->x87.aXMM[15].uXmm.s.Hi;
    iReg++;

    /* Floating point state. */
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx0;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[0].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[0].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx1;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[1].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[1].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx2;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[2].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[2].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx3;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[3].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[3].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx4;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[4].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[4].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx5;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[5].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[5].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx6;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[6].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[6].au64[1];
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                      = HvX64RegisterFpMmx7;
    pInput->Elements[iReg].Value.Fp.AsUINT128.Low64  = pCtx->pXStateR0->x87.aRegs[7].au64[0];
    pInput->Elements[iReg].Value.Fp.AsUINT128.High64 = pCtx->pXStateR0->x87.aRegs[7].au64[1];
    iReg++;

    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                            = HvX64RegisterFpControlStatus;
    pInput->Elements[iReg].Value.FpControlStatus.FpControl = pCtx->pXStateR0->x87.FCW;
    pInput->Elements[iReg].Value.FpControlStatus.FpStatus  = pCtx->pXStateR0->x87.FSW;
    pInput->Elements[iReg].Value.FpControlStatus.FpTag     = pCtx->pXStateR0->x87.FTW;
    pInput->Elements[iReg].Value.FpControlStatus.Reserved  = pCtx->pXStateR0->x87.FTW >> 8;
    pInput->Elements[iReg].Value.FpControlStatus.LastFpOp  = pCtx->pXStateR0->x87.FOP;
    pInput->Elements[iReg].Value.FpControlStatus.LastFpRip = (pCtx->pXStateR0->x87.FPUIP)
                                                           | ((uint64_t)pCtx->pXStateR0->x87.CS << 32)
                                                           | ((uint64_t)pCtx->pXStateR0->x87.Rsrvd1 << 48);
    iReg++;

    HV_REGISTER_ASSOC_ZERO_PADDING(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                                        = HvX64RegisterXmmControlStatus;
    pInput->Elements[iReg].Value.XmmControlStatus.LastFpRdp            = (pCtx->pXStateR0->x87.FPUDP)
                                                                       | ((uint64_t)pCtx->pXStateR0->x87.DS << 32)
                                                                       | ((uint64_t)pCtx->pXStateR0->x87.Rsrvd2 << 48);
    pInput->Elements[iReg].Value.XmmControlStatus.XmmStatusControl     = pCtx->pXStateR0->x87.MXCSR;
    pInput->Elements[iReg].Value.XmmControlStatus.XmmStatusControlMask = pCtx->pXStateR0->x87.MXCSR_MASK; /** @todo ??? (Isn't this an output field?) */
    iReg++;

    /* MSRs */
    // HvX64RegisterTsc - don't touch
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterEfer;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrEFER;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterKernelGsBase;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrKERNELGSBASE;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterApicBase;
    pInput->Elements[iReg].Value.Reg64          = APICGetBaseMsrNoCheck(pVCpu);
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterPat;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrPAT;
    iReg++;
    /// @todo HvX64RegisterSysenterCs
    /// @todo HvX64RegisterSysenterEip
    /// @todo HvX64RegisterSysenterEsp
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterStar;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrSTAR;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterLstar;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrLSTAR;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterCstar;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrCSTAR;
    iReg++;
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvX64RegisterSfmask;
    pInput->Elements[iReg].Value.Reg64          = pCtx->msrSFMASK;
    iReg++;

    /* event injection (always clear it). */
    HV_REGISTER_ASSOC_ZERO_PADDING_AND_HI64(&pInput->Elements[iReg]);
    pInput->Elements[iReg].Name                 = HvRegisterPendingInterruption;
    pInput->Elements[iReg].Value.Reg64          = 0;
    iReg++;
    /// @todo HvRegisterInterruptState
    /// @todo HvRegisterPendingEvent0
    /// @todo HvRegisterPendingEvent1

    /*
     * Set the registers.
     */
    Assert((uintptr_t)&pInput->Elements[iReg] - (uintptr_t)pGVCpu->nem.s.pbHypercallData < PAGE_SIZE); /* max is 127 */

    /*
     * Make the hypercall.
     */
    uint64_t uResult = g_pfnHvlInvokeHypercall(HV_MAKE_CALL_INFO(HvCallSetVpRegisters, iReg),
                                               pGVCpu->nem.s.HCPhysHypercallData, 0 /*GCPhysOutput*/);
    AssertLogRelMsgReturn(uResult == HV_MAKE_CALL_REP_RET(iReg),
                          ("uResult=%RX64 iRegs=%#x\n", uResult, iReg),
                          VERR_NEM_SET_REGISTERS_FAILED);
    return VINF_SUCCESS;
}


/**
 * Export the state to the native API (out of CPUMCTX).
 *
 * @returns VBox status code
 * @param   pGVM        The ring-0 VM handle.
 * @param   pVM         The cross context VM handle.
 * @param   idCpu       The calling EMT.  Necessary for getting the
 *                      hypercall page and arguments.
 * @param   fWhat       What to export. To be defined, UINT64_MAX for now.
 */
VMMR0_INT_DECL(int)  NEMR0ExportState(PGVM pGVM, PVM pVM, VMCPUID idCpu, uint64_t fWhat)
{
    /*
     * Validate the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];
        AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

        /*
         * Call worker.
         */
        rc = nemR0WinExportState(pGVM, pGVCpu, CPUMQueryGuestCtxPtr(pVCpu), fWhat);
    }
    return rc;
}


/**
 * Worker for NEMR0ImportState.
 *
 * Intention is to use it internally later.
 *
 * @returns VBox status code.
 * @param   pGVM        The ring-0 VM handle.
 * @param   pGVCpu      The irng-0 VCPU handle.
 * @param   pCtx        The CPU context structure to import into.
 * @param   fWhat       What to import. To be defined, UINT64_MAX for now.
 */
static int nemR0WinImportState(PGVM pGVM, PGVMCPU pGVCpu, PCPUMCTX pCtx, uint64_t fWhat)
{
    HV_INPUT_GET_VP_REGISTERS *pInput = (HV_INPUT_GET_VP_REGISTERS *)pGVCpu->nem.s.pbHypercallData;
    AssertPtrReturn(pInput, VERR_INTERNAL_ERROR_3);

    pInput->PartitionId = pGVM->nem.s.idHvPartition;
    pInput->VpIndex     = pGVCpu->idCpu;
    pInput->fFlags      = 0;

    RT_NOREF_PV(fWhat); /** @todo group selection. */

    /* GPRs */
    pInput->Names[0]  = HvX64RegisterRax;
    pInput->Names[1]  = HvX64RegisterRcx;
    pInput->Names[2]  = HvX64RegisterRdx;
    pInput->Names[3]  = HvX64RegisterRbx;
    pInput->Names[4]  = HvX64RegisterRsp;
    pInput->Names[5]  = HvX64RegisterRbp;
    pInput->Names[6]  = HvX64RegisterRsi;
    pInput->Names[7]  = HvX64RegisterRdi;
    pInput->Names[8]  = HvX64RegisterR8;
    pInput->Names[9]  = HvX64RegisterR9;
    pInput->Names[10] = HvX64RegisterR10;
    pInput->Names[11] = HvX64RegisterR11;
    pInput->Names[12] = HvX64RegisterR12;
    pInput->Names[13] = HvX64RegisterR13;
    pInput->Names[14] = HvX64RegisterR14;
    pInput->Names[15] = HvX64RegisterR15;

    /* RIP & Flags */
    pInput->Names[16] = HvX64RegisterRip;
    pInput->Names[17] = HvX64RegisterRflags;

    /* Segments */
    pInput->Names[18] = HvX64RegisterEs;
    pInput->Names[19] = HvX64RegisterCs;
    pInput->Names[20] = HvX64RegisterSs;
    pInput->Names[21] = HvX64RegisterDs;
    pInput->Names[22] = HvX64RegisterFs;
    pInput->Names[23] = HvX64RegisterGs;
    pInput->Names[24] = HvX64RegisterLdtr;
    pInput->Names[25] = HvX64RegisterTr;

    /* Descriptor tables. */
    pInput->Names[26] = HvX64RegisterIdtr;
    pInput->Names[27] = HvX64RegisterGdtr;

    /* Control registers. */
    pInput->Names[28] = HvX64RegisterCr0;
    pInput->Names[29] = HvX64RegisterCr2;
    pInput->Names[30] = HvX64RegisterCr3;
    pInput->Names[31] = HvX64RegisterCr4;
    pInput->Names[32] = HvX64RegisterCr8;

    /* Debug registers. */
    pInput->Names[33] = HvX64RegisterDr0;
    pInput->Names[34] = HvX64RegisterDr1;
    pInput->Names[35] = HvX64RegisterDr2;
    pInput->Names[36] = HvX64RegisterDr3;
    pInput->Names[37] = HvX64RegisterDr6;
    pInput->Names[38] = HvX64RegisterDr7;

    /* Vector state. */
    pInput->Names[39] = HvX64RegisterXmm0;
    pInput->Names[40] = HvX64RegisterXmm1;
    pInput->Names[41] = HvX64RegisterXmm2;
    pInput->Names[42] = HvX64RegisterXmm3;
    pInput->Names[43] = HvX64RegisterXmm4;
    pInput->Names[44] = HvX64RegisterXmm5;
    pInput->Names[45] = HvX64RegisterXmm6;
    pInput->Names[46] = HvX64RegisterXmm7;
    pInput->Names[47] = HvX64RegisterXmm8;
    pInput->Names[48] = HvX64RegisterXmm9;
    pInput->Names[49] = HvX64RegisterXmm10;
    pInput->Names[50] = HvX64RegisterXmm11;
    pInput->Names[51] = HvX64RegisterXmm12;
    pInput->Names[52] = HvX64RegisterXmm13;
    pInput->Names[53] = HvX64RegisterXmm14;
    pInput->Names[54] = HvX64RegisterXmm15;

    /* Floating point state. */
    pInput->Names[55] = HvX64RegisterFpMmx0;
    pInput->Names[56] = HvX64RegisterFpMmx1;
    pInput->Names[57] = HvX64RegisterFpMmx2;
    pInput->Names[58] = HvX64RegisterFpMmx3;
    pInput->Names[59] = HvX64RegisterFpMmx4;
    pInput->Names[60] = HvX64RegisterFpMmx5;
    pInput->Names[61] = HvX64RegisterFpMmx6;
    pInput->Names[62] = HvX64RegisterFpMmx7;
    pInput->Names[63] = HvX64RegisterFpControlStatus;
    pInput->Names[64] = HvX64RegisterXmmControlStatus;

    /* MSRs */
    // HvX64RegisterTsc - don't touch
    pInput->Names[65] = HvX64RegisterEfer;
    pInput->Names[66] = HvX64RegisterKernelGsBase;
    pInput->Names[67] = HvX64RegisterApicBase;
    pInput->Names[68] = HvX64RegisterPat;
    pInput->Names[69] = HvX64RegisterSysenterCs;
    pInput->Names[70] = HvX64RegisterSysenterEip;
    pInput->Names[71] = HvX64RegisterSysenterEsp;
    pInput->Names[72] = HvX64RegisterStar;
    pInput->Names[73] = HvX64RegisterLstar;
    pInput->Names[74] = HvX64RegisterCstar;
    pInput->Names[75] = HvX64RegisterSfmask;

    /* event injection */
    pInput->Names[76] = HvRegisterPendingInterruption;
    pInput->Names[77] = HvRegisterInterruptState;
    pInput->Names[78] = HvRegisterInterruptState;
    pInput->Names[79] = HvRegisterPendingEvent0;
    pInput->Names[80] = HvRegisterPendingEvent1;
    unsigned const cRegs   = 81;
    size_t const   cbInput = RT_ALIGN_Z(RT_OFFSETOF(HV_INPUT_GET_VP_REGISTERS, Names[cRegs]), 32);

    HV_REGISTER_VALUE *paValues = (HV_REGISTER_VALUE *)((uint8_t *)pInput + cbInput);
    Assert((uintptr_t)&paValues[cRegs] - (uintptr_t)pGVCpu->nem.s.pbHypercallData < PAGE_SIZE); /* (max is around 168 registers) */
    RT_BZERO(paValues, cRegs * sizeof(paValues[0]));

    /*
     * Make the hypercall.
     */
    uint64_t uResult = g_pfnHvlInvokeHypercall(HV_MAKE_CALL_INFO(HvCallGetVpRegisters, cRegs),
                                               pGVCpu->nem.s.HCPhysHypercallData,
                                               pGVCpu->nem.s.HCPhysHypercallData + cbInput);
    AssertLogRelMsgReturn(uResult == HV_MAKE_CALL_REP_RET(cRegs),
                          ("uResult=%RX64 cRegs=%#x\n", uResult, cRegs),
                          VERR_NEM_GET_REGISTERS_FAILED);

    /*
     * Copy information to the CPUM context.
     */
    PVMCPU pVCpu = &pGVM->pVM->aCpus[pGVCpu->idCpu];

    /* GPRs */
    Assert(pInput->Names[0]  == HvX64RegisterRax);
    Assert(pInput->Names[15] == HvX64RegisterR15);
    pCtx->rax = paValues[0].Reg64;
    pCtx->rcx = paValues[1].Reg64;
    pCtx->rdx = paValues[2].Reg64;
    pCtx->rbx = paValues[3].Reg64;
    pCtx->rsp = paValues[4].Reg64;
    pCtx->rbp = paValues[5].Reg64;
    pCtx->rsi = paValues[6].Reg64;
    pCtx->rdi = paValues[7].Reg64;
    pCtx->r8  = paValues[8].Reg64;
    pCtx->r9  = paValues[9].Reg64;
    pCtx->r10 = paValues[10].Reg64;
    pCtx->r11 = paValues[11].Reg64;
    pCtx->r12 = paValues[12].Reg64;
    pCtx->r13 = paValues[13].Reg64;
    pCtx->r14 = paValues[14].Reg64;
    pCtx->r15 = paValues[15].Reg64;

    /* RIP & Flags */
    Assert(pInput->Names[16] == HvX64RegisterRip);
    pCtx->rip      = paValues[16].Reg64;
    pCtx->rflags.u = paValues[17].Reg64;

    /* Segments */
#define COPY_BACK_SEG(a_idx, a_enmName, a_SReg) \
        do { \
            Assert(pInput->Names[a_idx] == a_enmName); \
            (a_SReg).u64Base  = paValues[a_idx].Segment.Base; \
            (a_SReg).u32Limit = paValues[a_idx].Segment.Limit; \
            (a_SReg).ValidSel = (a_SReg).Sel = paValues[a_idx].Segment.Selector; \
            (a_SReg).Attr.u   = paValues[a_idx].Segment.Attributes; \
            (a_SReg).fFlags   = CPUMSELREG_FLAGS_VALID; \
        } while (0)
    COPY_BACK_SEG(18, HvX64RegisterEs,   pCtx->es);
    COPY_BACK_SEG(19, HvX64RegisterCs,   pCtx->cs);
    COPY_BACK_SEG(20, HvX64RegisterSs,   pCtx->ss);
    COPY_BACK_SEG(21, HvX64RegisterDs,   pCtx->ds);
    COPY_BACK_SEG(22, HvX64RegisterFs,   pCtx->fs);
    COPY_BACK_SEG(23, HvX64RegisterGs,   pCtx->gs);
    COPY_BACK_SEG(24, HvX64RegisterLdtr, pCtx->ldtr);
    COPY_BACK_SEG(25, HvX64RegisterTr,   pCtx->tr);

    /* Descriptor tables. */
    Assert(pInput->Names[26] == HvX64RegisterIdtr);
    pCtx->idtr.cbIdt = paValues[26].Table.Limit;
    pCtx->idtr.pIdt  = paValues[26].Table.Base;
    Assert(pInput->Names[27] == HvX64RegisterGdtr);
    pCtx->gdtr.cbGdt = paValues[27].Table.Limit;
    pCtx->gdtr.pGdt  = paValues[27].Table.Base;

    /* Control registers. */
    Assert(pInput->Names[28] == HvX64RegisterCr0);
    bool fMaybeChangedMode = false;
    bool fFlushTlb         = false;
    bool fFlushGlobalTlb   = false;
    if (pCtx->cr0 != paValues[28].Reg64)
    {
        CPUMSetGuestCR0(pVCpu, paValues[28].Reg64);
        fMaybeChangedMode = true;
        fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
    }
    Assert(pInput->Names[29] == HvX64RegisterCr2);
    pCtx->cr2 = paValues[29].Reg64;
    if (pCtx->cr3 != paValues[30].Reg64)
    {
        CPUMSetGuestCR3(pVCpu, paValues[30].Reg64);
        fFlushTlb = true;
    }
    if (pCtx->cr4 != paValues[31].Reg64)
    {
        CPUMSetGuestCR4(pVCpu, paValues[31].Reg64);
        fMaybeChangedMode = true;
        fFlushTlb = fFlushGlobalTlb = true; /// @todo fix this
    }
    APICSetTpr(pVCpu, (uint8_t)paValues[32].Reg64 << 4);

    /* Debug registers. */
    Assert(pInput->Names[33] == HvX64RegisterDr0);
/** @todo fixme */
    if (pCtx->dr[0] != paValues[33].Reg64)
        CPUMSetGuestDR0(pVCpu, paValues[33].Reg64);
    if (pCtx->dr[1] != paValues[34].Reg64)
        CPUMSetGuestDR1(pVCpu, paValues[34].Reg64);
    if (pCtx->dr[2] != paValues[35].Reg64)
        CPUMSetGuestDR2(pVCpu, paValues[35].Reg64);
    if (pCtx->dr[3] != paValues[36].Reg64)
        CPUMSetGuestDR3(pVCpu, paValues[36].Reg64);
    Assert(pInput->Names[37] == HvX64RegisterDr6);
    Assert(pInput->Names[38] == HvX64RegisterDr7);
    if (pCtx->dr[6] != paValues[37].Reg64)
        CPUMSetGuestDR6(pVCpu, paValues[37].Reg64);
    if (pCtx->dr[7] != paValues[38].Reg64)
        CPUMSetGuestDR6(pVCpu, paValues[38].Reg64);

    /* Vector state. */
    Assert(pInput->Names[39] == HvX64RegisterXmm0);
    Assert(pInput->Names[54] == HvX64RegisterXmm15);
    pCtx->pXStateR0->x87.aXMM[0].uXmm.s.Lo  = paValues[39].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[0].uXmm.s.Hi  = paValues[39].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[1].uXmm.s.Lo  = paValues[40].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[1].uXmm.s.Hi  = paValues[40].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[2].uXmm.s.Lo  = paValues[41].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[2].uXmm.s.Hi  = paValues[41].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[3].uXmm.s.Lo  = paValues[42].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[3].uXmm.s.Hi  = paValues[42].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[4].uXmm.s.Lo  = paValues[43].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[4].uXmm.s.Hi  = paValues[43].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[5].uXmm.s.Lo  = paValues[44].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[5].uXmm.s.Hi  = paValues[44].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[6].uXmm.s.Lo  = paValues[45].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[6].uXmm.s.Hi  = paValues[45].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[7].uXmm.s.Lo  = paValues[46].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[7].uXmm.s.Hi  = paValues[46].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[8].uXmm.s.Lo  = paValues[47].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[8].uXmm.s.Hi  = paValues[47].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[9].uXmm.s.Lo  = paValues[48].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[9].uXmm.s.Hi  = paValues[48].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[10].uXmm.s.Lo = paValues[49].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[10].uXmm.s.Hi = paValues[49].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[11].uXmm.s.Lo = paValues[50].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[11].uXmm.s.Hi = paValues[50].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[12].uXmm.s.Lo = paValues[51].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[12].uXmm.s.Hi = paValues[51].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[13].uXmm.s.Lo = paValues[52].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[13].uXmm.s.Hi = paValues[52].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[14].uXmm.s.Lo = paValues[53].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[14].uXmm.s.Hi = paValues[53].Reg128.High64;
    pCtx->pXStateR0->x87.aXMM[15].uXmm.s.Lo = paValues[54].Reg128.Low64;
    pCtx->pXStateR0->x87.aXMM[15].uXmm.s.Hi = paValues[54].Reg128.High64;

    /* Floating point state. */
    Assert(pInput->Names[55] == HvX64RegisterFpMmx0);
    Assert(pInput->Names[62] == HvX64RegisterFpMmx7);
    pCtx->pXStateR0->x87.aRegs[0].au64[0] = paValues[55].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[0].au64[1] = paValues[55].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[1].au64[0] = paValues[56].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[1].au64[1] = paValues[56].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[2].au64[0] = paValues[57].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[2].au64[1] = paValues[57].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[3].au64[0] = paValues[58].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[3].au64[1] = paValues[58].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[4].au64[0] = paValues[59].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[4].au64[1] = paValues[59].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[5].au64[0] = paValues[60].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[5].au64[1] = paValues[60].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[6].au64[0] = paValues[61].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[6].au64[1] = paValues[61].Fp.AsUINT128.High64;
    pCtx->pXStateR0->x87.aRegs[7].au64[0] = paValues[62].Fp.AsUINT128.Low64;
    pCtx->pXStateR0->x87.aRegs[7].au64[1] = paValues[62].Fp.AsUINT128.High64;

    Assert(pInput->Names[63] == HvX64RegisterFpControlStatus);
    pCtx->pXStateR0->x87.FCW        = paValues[63].FpControlStatus.FpControl;
    pCtx->pXStateR0->x87.FSW        = paValues[63].FpControlStatus.FpStatus;
    pCtx->pXStateR0->x87.FTW        = paValues[63].FpControlStatus.FpTag
                                    /*| (paValues[63].FpControlStatus.Reserved << 8)*/;
    pCtx->pXStateR0->x87.FOP        = paValues[63].FpControlStatus.LastFpOp;
    pCtx->pXStateR0->x87.FPUIP      = (uint32_t)paValues[63].FpControlStatus.LastFpRip;
    pCtx->pXStateR0->x87.CS         = (uint16_t)(paValues[63].FpControlStatus.LastFpRip >> 32);
    pCtx->pXStateR0->x87.Rsrvd1     = (uint16_t)(paValues[63].FpControlStatus.LastFpRip >> 48);

    Assert(pInput->Names[64] == HvX64RegisterXmmControlStatus);
    pCtx->pXStateR0->x87.FPUDP      = (uint32_t)paValues[64].XmmControlStatus.LastFpRdp;
    pCtx->pXStateR0->x87.DS         = (uint16_t)(paValues[64].XmmControlStatus.LastFpRdp >> 32);
    pCtx->pXStateR0->x87.Rsrvd2     = (uint16_t)(paValues[64].XmmControlStatus.LastFpRdp >> 48);
    pCtx->pXStateR0->x87.MXCSR      = paValues[64].XmmControlStatus.XmmStatusControl;
    pCtx->pXStateR0->x87.MXCSR_MASK = paValues[64].XmmControlStatus.XmmStatusControlMask; /** @todo ??? (Isn't this an output field?) */

    /* MSRs */
    // HvX64RegisterTsc - don't touch
    Assert(pInput->Names[65] == HvX64RegisterEfer);
    if (paValues[65].Reg64 != pCtx->msrEFER)
    {
        pCtx->msrEFER = paValues[65].Reg64;
        fMaybeChangedMode = true;
    }

    Assert(pInput->Names[66] == HvX64RegisterKernelGsBase);
    pCtx->msrKERNELGSBASE = paValues[66].Reg64;

    Assert(pInput->Names[67] == HvX64RegisterApicBase);
    if (paValues[67].Reg64 != APICGetBaseMsrNoCheck(pVCpu))
    {
        VBOXSTRICTRC rc2 = APICSetBaseMsr(pVCpu, paValues[67].Reg64);
        Assert(rc2 == VINF_SUCCESS); NOREF(rc2);
    }

    Assert(pInput->Names[68] == HvX64RegisterPat);
    pCtx->msrPAT    = paValues[68].Reg64;
    /// @todo HvX64RegisterSysenterCs
    /// @todo HvX64RegisterSysenterEip
    /// @todo HvX64RegisterSysenterEsp
    Assert(pInput->Names[72] == HvX64RegisterStar);
    pCtx->msrSTAR   = paValues[72].Reg64;
    Assert(pInput->Names[73] == HvX64RegisterLstar);
    pCtx->msrLSTAR  = paValues[73].Reg64;
    Assert(pInput->Names[74] == HvX64RegisterCstar);
    pCtx->msrCSTAR  = paValues[74].Reg64;
    Assert(pInput->Names[75] == HvX64RegisterSfmask);
    pCtx->msrSFMASK = paValues[75].Reg64;

    /// @todo HvRegisterPendingInterruption
    Assert(pInput->Names[76] == HvRegisterPendingInterruption);
    if (paValues[76].PendingInterruption.InterruptionPending)
    {
        Log7(("PendingInterruption: type=%u vector=%#x errcd=%RTbool/%#x instr-len=%u nested=%u\n",
              paValues[76].PendingInterruption.InterruptionType, paValues[76].PendingInterruption.InterruptionVector,
              paValues[76].PendingInterruption.DeliverErrorCode, paValues[76].PendingInterruption.ErrorCode,
              paValues[76].PendingInterruption.InstructionLength, paValues[76].PendingInterruption.NestedEvent));
        AssertMsg((paValues[76].PendingInterruption.AsUINT64 & UINT64_C(0xfc00)) == 0,
                  ("%#RX64\n", paValues[76].PendingInterruption.AsUINT64));
    }

    /// @todo HvRegisterInterruptState
    /// @todo HvRegisterPendingEvent0
    /// @todo HvRegisterPendingEvent1

    int rc = VINF_SUCCESS;
    if (fMaybeChangedMode)
    {
        rc = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
        if (rc == VINF_PGM_CHANGE_MODE)
            rc = VERR_NEM_CHANGE_PGM_MODE;
        else
            AssertRC(rc);
    }
    if (fFlushTlb && rc == VINF_SUCCESS)
        rc = VERR_NEM_FLUSH_TLB; /* Calling PGMFlushTLB w/o long jump setup doesn't work, ring-3 does it. */

    return rc;
}


/**
 * Import the state from the native API (back to CPUMCTX).
 *
 * @returns VBox status code
 * @param   pGVM        The ring-0 VM handle.
 * @param   pVM         The cross context VM handle.
 * @param   idCpu       The calling EMT.  Necessary for getting the
 *                      hypercall page and arguments.
 * @param   fWhat       What to import. To be defined, UINT64_MAX for now.
 */
VMMR0_INT_DECL(int)  NEMR0ImportState(PGVM pGVM, PVM pVM, VMCPUID idCpu, uint64_t fWhat)
{
    /*
     * Validate the call.
     */
    int rc = GVMMR0ValidateGVMandVMandEMT(pGVM, pVM, idCpu);
    if (RT_SUCCESS(rc))
    {
        PVMCPU  pVCpu  = &pVM->aCpus[idCpu];
        PGVMCPU pGVCpu = &pGVM->aCpus[idCpu];
        AssertReturn(g_pfnHvlInvokeHypercall, VERR_NEM_MISSING_KERNEL_API);

        /*
         * Call worker.
         */
        rc = nemR0WinImportState(pGVM, pGVCpu, CPUMQueryGuestCtxPtr(pVCpu), fWhat);
    }
    return rc;
}

