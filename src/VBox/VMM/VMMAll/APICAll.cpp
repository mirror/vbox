/* $Id$ */
/** @file
 * APIC - Advanced Programmable Interrupt Controller - All Contexts.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include "APICInternal.h"
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/vmcpuset.h>

/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
/** An ordered array of valid LVT masks. */
static const uint32_t g_au32LvtValidMasks[] =
{
    XAPIC_LVT_TIMER,
    XAPIC_LVT_THERMAL,
    XAPIC_LVT_PERF,
    XAPIC_LVT_LINT0,
    XAPIC_LVT_LINT1,
    XAPIC_LVT_ERROR
};
#endif

#if 0
/** @todo CMCI */
static const uint32_t g_au32LvtExtValidMask[] =
{
    XAPIC_LVT_CMCI
};
#endif


/**
 * Checks if a vector is set in an APIC 256-bit sparse register.
 *
 * @returns true if the specified vector is set, false otherwise.
 * @param   pApicReg        The APIC 256-bit spare register.
 * @param   uVector         The vector to check if set.
 */
DECLINLINE(bool) apicTestVectorInReg(volatile XAPIC256BITREG *pApicReg, uint8_t uVector)
{
    volatile uint8_t *pbBitmap = (volatile uint8_t *)&pApicReg->u[0];
    return ASMBitTest(pbBitmap + XAPIC_REG256_VECTOR_OFF(uVector), XAPIC_REG256_VECTOR_BIT(uVector));
}


/**
 * Sets the vector in an APIC 256-bit sparse register.
 *
 * @param   pApicReg        The APIC 256-bit spare register.
 * @param   uVector         The vector to set.
 */
DECLINLINE(void) apicSetVectorInReg(volatile XAPIC256BITREG *pApicReg, uint8_t uVector)
{
    volatile uint8_t *pbBitmap = (volatile uint8_t *)&pApicReg->u[0];
    ASMAtomicBitSet(pbBitmap + XAPIC_REG256_VECTOR_OFF(uVector), XAPIC_REG256_VECTOR_BIT(uVector));
}


/**
 * Clears the vector in an APIC 256-bit sparse register.
 *
 * @param   pApicReg        The APIC 256-bit spare register.
 * @param   uVector         The vector to clear.
 */
DECLINLINE(void) apicClearVectorInReg(volatile XAPIC256BITREG *pApicReg, uint8_t uVector)
{
    volatile uint8_t *pbBitmap = (volatile uint8_t *)&pApicReg->u[0];
    ASMAtomicBitClear(pbBitmap + XAPIC_REG256_VECTOR_OFF(uVector), XAPIC_REG256_VECTOR_BIT(uVector));
}


/**
 * Checks if a vector is set in an APIC Pending Interrupt Bitmap (PIB).
 *
 * @returns true if the specified vector is set, false otherwise.
 * @param   pvPib           Opaque pointer to the PIB.
 * @param   uVector         The vector to check if set.
 */
DECLINLINE(bool) apicTestVectorInPib(volatile void *pvPib, uint8_t uVector)
{
    return ASMBitTest(pvPib, uVector);
}


/**
 * Atomically tests and sets the PIB notification bit.
 *
 * @returns true if the bit was already set, false otherwise.
 * @param   pvPib           Opaque pointer to the PIB.
 */
DECLINLINE(bool) apicSetNotificationBitInPib(volatile void *pvPib)
{
    return ASMAtomicBitTestAndSet(pvPib, XAPIC_PIB_NOTIFICATION_BIT);
}


/**
 * Atomically tests and clears the PIB notification bit.
 *
 * @returns true if the bit was already set, false otherwise.
 */
DECLINLINE(bool) apicClearNotificationBitInPib(volatile void *pvPib)
{
    return ASMAtomicBitTestAndClear(pvPib, XAPIC_PIB_NOTIFICATION_BIT);
}


/**
 * Sets the vector in an APIC Pending Interrupt Bitmap (PIB).
 *
 * @param   pvPib           Opaque pointer to the PIB.
 * @param   uVector         The vector to set.
 */
DECLINLINE(void) apicSetVectorInPib(volatile void *pvPib, uint8_t uVector)
{
    ASMAtomicBitSet(pvPib, uVector);
}


/**
 * Clears the vector in an APIC Pending Interrupt Bitmap (PIB).
 *
 * @param   pvPib           Opaque pointer to the PIB.
 * @param   uVector         The vector to clear.
 */
DECLINLINE(void) apicClearVectorInPib(volatile void *pvPib, uint8_t uVector)
{
    ASMAtomicBitClear(pvPib, uVector);
}


/**
 * Atomically OR's a fragment (32 vectors) into an APIC 256-bit sparse
 * register.
 *
 * @param   pApicReg        The APIC 256-bit spare register.
 * @param   idxFragment     The index of the 32-bit fragment in @a
 *                          pApicReg.
 * @param   u32Fragment     The 32-bit vector fragment.
 */
DECLINLINE(void) apicOrVectorsToReg(volatile XAPIC256BITREG *pApicReg, size_t idxFragment, uint32_t u32Fragment)
{
    Assert(idxFragment < RT_ELEMENTS(pApicReg->u));
    ASMAtomicOrU32(&pApicReg->u[idxFragment].u32Reg, u32Fragment);
}


/**
 * Reports and returns appropriate error code for invalid MSR accesses.
 *
 * @returns Strict VBox status code.
 * @retval  VINF_CPUM_R3_MSR_WRITE if the MSR write could not be serviced in the
 *          current context (raw-mode or ring-0).
 * @retval  VINF_CPUM_R3_MSR_READ if the MSR read could not be serviced in the
 *          current context (raw-mode or ring-0).
 * @retval  VERR_CPUM_RAISE_GP_0 on failure, the caller is expected to take the
 *          appropriate actions.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The MSR being accessed.
 * @param   enmAccess       The invalid-access type.
 */
static VBOXSTRICTRC apicMsrAccessError(PVMCPU pVCpu, uint32_t u32Reg, APICMSRACCESS enmAccess)
{
    static struct
    {
        const char *pszBefore;   /* The error message before printing the MSR index */
        const char *pszAfter;    /* The error message after printing the MSR index */
        int         rcR0;        /* The ring-0 error code */
    } const s_aAccess[] =
    {
        { "read MSR",                      " while not in x2APIC mode", VINF_CPUM_R3_MSR_READ  },
        { "write MSR",                     " while not in x2APIC mode", VINF_CPUM_R3_MSR_WRITE },
        { "read reserved/unknown MSR",     "",                          VINF_CPUM_R3_MSR_READ  },
        { "write reserved/unknown MSR",    "",                          VINF_CPUM_R3_MSR_WRITE },
        { "read write-only MSR",           "",                          VINF_CPUM_R3_MSR_READ  },
        { "write read-only MSR",           "",                          VINF_CPUM_R3_MSR_WRITE },
        { "read reserved bits of MSR",     "",                          VINF_CPUM_R3_MSR_READ  },
        { "write reserved bits of MSR",    "",                          VINF_CPUM_R3_MSR_WRITE },
        { "write an invalid value to MSR", "",                          VINF_CPUM_R3_MSR_WRITE }
    };
    AssertCompile(RT_ELEMENTS(s_aAccess) == APICMSRACCESS_COUNT);

    size_t const i = enmAccess;
    Assert(i < RT_ELEMENTS(s_aAccess));
#ifdef IN_RING3
    LogRelMax(5, ("APIC%u: Attempt to %s (%#x)%s -> #GP(0)\n", pVCpu->idCpu, s_aAccess[i].pszBefore, u32Reg,
                  s_aAccess[i].pszAfter));
    return VERR_CPUM_RAISE_GP_0;
#else
    return s_aAccess[i].rcR0;
#endif
}


/**
 * Gets the APIC mode given the base MSR value.
 *
 * @returns The APIC mode.
 * @param   uApicBaseMsr        The APIC Base MSR value.
 */
static APICMODE apicGetMode(uint64_t uApicBaseMsr)
{
    uint32_t const uMode   = MSR_APICBASE_GET_MODE(uApicBaseMsr);
    APICMODE const enmMode = (APICMODE)uMode;
#ifdef VBOX_STRICT
    /* Paranoia. */
    switch (uMode)
    {
        case APICMODE_DISABLED:
        case APICMODE_INVALID:
        case APICMODE_XAPIC:
        case APICMODE_X2APIC:
            break;
        default:
            AssertMsgFailed(("Invalid mode"));
    }
#endif
    return enmMode;
}


/**
 * Returns whether the APIC is hardware enabled or not.
 *
 * @returns true if enabled, false otherwise.
 */
DECLINLINE(bool) apicIsEnabled(PVMCPU pVCpu)
{
    PCAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    return MSR_APICBASE_IS_ENABLED(pApicCpu->uApicBaseMsr);
}


/**
 * Finds the most significant set bit in an APIC 256-bit sparse register.
 *
 * @returns @a rcNotFound if no bit was set, 0-255 otherwise.
 * @param   pReg            The APIC 256-bit sparse register.
 * @param   rcNotFound      What to return when no bit is set.
 */
static int apicGetLastSetBit(volatile const XAPIC256BITREG *pReg, int rcNotFound)
{
    unsigned const cBitsPerFragment = sizeof(pReg->u[0].u32Reg) * 8;
    ssize_t const  cFragments       = RT_ELEMENTS(pReg->u);
    for (ssize_t i = cFragments - 1; i >= 0; i--)
    {
        uint32_t const uFragment = pReg->u[i].u32Reg;
        if (uFragment)
        {
            unsigned idxSetBit = ASMBitLastSetU32(uFragment);
            --idxSetBit;
            idxSetBit += (i * cBitsPerFragment);
            return idxSetBit;
        }
    }
    return rcNotFound;
}


/**
 * Gets the highest priority pending interrupt.
 *
 * @returns true if any interrupt is pending, false otherwise.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pu8PendingIntr      Where to store the interrupt vector if the
 *                              interrupt is pending.
 */
static bool apicGetHighestPendingInterrupt(PVMCPU pVCpu, uint8_t *pu8PendingIntr)
{
    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    int const irrv = apicGetLastSetBit(&pXApicPage->irr, -1);
    if (irrv >= 0)
    {
        Assert(irrv <= (int)UINT8_MAX);
        *pu8PendingIntr = (uint8_t)irrv;
        return true;
    }
    return false;
}


/**
 * Reads a 32-bit register at a specified offset.
 *
 * @returns The value at the specified offset.
 * @param   pXApicPage      The xAPIC page.
 * @param   offReg          The offset of the register being read.
 */
DECLINLINE(uint32_t) apicReadRaw32(PCXAPICPAGE pXApicPage, uint16_t offReg)
{
    Assert(offReg < sizeof(*pXApicPage) - sizeof(uint32_t));
    uint8_t  const *pbXApic =  (const uint8_t *)pXApicPage;
    uint32_t const  uValue  = *(const uint32_t *)(pbXApic + offReg);
    return uValue;
}


/**
 * Writes a 32-bit register at a specified offset.
 *
 * @param   pXApicPage      The xAPIC page.
 * @param   offReg          The offset of the register being written.
 * @param   uReg            The value of the register.
 */
DECLINLINE(void) apicWriteRaw32(PXAPICPAGE pXApicPage, uint16_t offReg, uint32_t uReg)
{
    Assert(offReg < sizeof(*pXApicPage) - sizeof(uint32_t));
    uint8_t *pbXApic = (uint8_t *)pXApicPage;
    *(uint32_t *)(pbXApic + offReg) = uReg;
}


/**
 * Sets an error in the internal ESR of the specified APIC.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uError          The error.
 * @thread  Any.
 */
DECLINLINE(void) apicSetError(PVMCPU pVCpu, uint32_t uError)
{
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    ASMAtomicOrU32(&pApicCpu->uEsrInternal, uError);
}


/**
 * Clears all errors in the internal ESR.
 *
 * @returns The value of the internal ESR before clearing.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(uint32_t) apicClearAllErrors(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    return ASMAtomicXchgU32(&pApicCpu->uEsrInternal, 0);
}


/**
 * Signals the guest if a pending interrupt is ready to be serviced.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void apicSignalNextPendingIntr(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    if (pXApicPage->svr.u.fApicSoftwareEnable)
    {
        int const irrv = apicGetLastSetBit(&pXApicPage->irr, -1 /* rcNotFound */);
        if (irrv >= 0)
        {
            Assert(irrv <= (int)UINT8_MAX);
            uint8_t const uVector = irrv;
            uint8_t const uPpr    = pXApicPage->ppr.u8Ppr;
            if (   !uPpr
                ||  XAPIC_PPR_GET_PP(uVector) > XAPIC_PPR_GET_PP(uPpr))
            {
                APICSetInterruptFF(pVCpu, PDMAPICIRQ_HARDWARE);
            }
        }
    }
    else
        APICClearInterruptFF(pVCpu, PDMAPICIRQ_HARDWARE);
}


/**
 * Sets the Spurious-Interrupt Vector Register (SVR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uSvr            The SVR value.
 */
static VBOXSTRICTRC apicSetSvr(PVMCPU pVCpu, uint32_t uSvr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    uint32_t   uValidMask = XAPIC_SVR;
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    if (pXApicPage->version.u.fEoiBroadcastSupression)
        uValidMask |= XAPIC_SVR_SUPRESS_EOI_BROADCAST;

    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uSvr & ~uValidMask))
        return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_SVR, APICMSRACCESS_WRITE_RSVD_BITS);

    apicWriteRaw32(pXApicPage, XAPIC_OFF_SVR, uSvr);
    if (!pXApicPage->svr.u.fApicSoftwareEnable)
    {
        /** @todo CMCI. */
        pXApicPage->lvt_timer.u.u1Mask   = 1;
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
        pXApicPage->lvt_thermal.u.u1Mask = 1;
#endif
        pXApicPage->lvt_perf.u.u1Mask    = 1;
        pXApicPage->lvt_lint0.u.u1Mask   = 1;
        pXApicPage->lvt_lint1.u.u1Mask   = 1;
        pXApicPage->lvt_error.u.u1Mask   = 1;
    }
    return VINF_SUCCESS;
}


/**
 * Sends an interrupt to one or more APICs.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uVector             The interrupt vector.
 * @param   enmTriggerMode      The trigger mode.
 * @param   enmDeliveryMode     The delivery mode.
 * @param   pDestCpuSet         The destination CPU set.
 * @param   rcRZ                The return code if the operation cannot be
 *                              performed in the current context.
 */
static VBOXSTRICTRC apicSendIntr(PVMCPU pVCpu, uint8_t uVector, XAPICTRIGGERMODE enmTriggerMode,
                                 XAPICDELIVERYMODE enmDeliveryMode, PCVMCPUSET pDestCpuSet, int rcRZ)
{
    VBOXSTRICTRC  rcStrict = VINF_SUCCESS;
    PVM           pVM      = pVCpu->CTX_SUFF(pVM);
    VMCPUID const cCpus    = pVM->cCpus;
    switch (enmDeliveryMode)
    {
        case XAPICDELIVERYMODE_FIXED:
        {
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
            {
                if (   VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu)
                    && apicIsEnabled(&pVM->aCpus[idCpu]))
                    APICPostInterrupt(&pVM->aCpus[idCpu], uVector, enmTriggerMode);
            }
            break;
        }

        case XAPICDELIVERYMODE_LOWEST_PRIO:
        {
            VMCPUID const idCpu = VMCPUSET_FIND_FIRST_PRESENT(pDestCpuSet);
            if (   idCpu < pVM->cCpus
                && apicIsEnabled(&pVM->aCpus[idCpu]))
                APICPostInterrupt(&pVM->aCpus[idCpu], uVector, enmTriggerMode);
            break;
        }

        case XAPICDELIVERYMODE_SMI:
        {
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
            {
                if (VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu))
                    APICSetInterruptFF(&pVM->aCpus[idCpu], PDMAPICIRQ_SMI);
            }
            break;
        }

        case XAPICDELIVERYMODE_NMI:
        {
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
            {
                if (   VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu)
                    && apicIsEnabled(&pVM->aCpus[idCpu]))
                    APICSetInterruptFF(&pVM->aCpus[idCpu], PDMAPICIRQ_NMI);
            }
            break;
        }

        case XAPICDELIVERYMODE_INIT:
        {
#ifdef IN_RING3
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
                if (VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu))
                    VMMR3SendInitIpi(pVM, idCpu);
#else
            /* We need to return to ring-3 to deliver the INIT. */
            rcStrict = rcRZ;
#endif
            break;
        }

        case XAPICDELIVERYMODE_STARTUP:
        {
#ifdef IN_RING3
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
                if (VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu))
                    VMMR3SendStartupIpi(pVM, idCpu, uVector);
#else
            /* We need to return to ring-3 to deliver the SIPI. */
            rcStrict = rcRZ;
#endif
            break;
        }

        case XAPICDELIVERYMODE_EXTINT:
        {
            for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
                if (VMCPUSET_IS_PRESENT(pDestCpuSet, idCpu))
                    APICSetInterruptFF(&pVM->aCpus[idCpu], PDMAPICIRQ_EXTINT);
            break;
        }

        default:
        {
            AssertMsgFailed(("APIC: apicSendIntr: Unknown delivery mode %#x\n", enmDeliveryMode));
            break;
        }
    }

    /*
     * If an illegal vector is programmed, set the 'send illegal vector' error here if the
     * interrupt is being sent by an APIC.
     *
     * The 'receive illegal vector' will be set on the target APIC when the interrupt
     * gets generated, see APICPostInterrupt().
     *
     * See Intel spec. 10.5.3 "Error Handling".
     */
    if (   rcStrict != rcRZ
        && pVCpu)
    {
        if (RT_UNLIKELY(uVector <= XAPIC_ILLEGAL_VECTOR_END))
            apicSetError(pVCpu, XAPIC_ESR_SEND_ILLEGAL_VECTOR);
    }
    return rcStrict;
}


/**
 * Checks if this APIC belongs to a logical destination.
 *
 * @returns true if the APIC belongs to the logical
 *          destination, false otherwise.
 * @param   pVCpu                   The cross context virtual CPU structure.
 * @param   fDest                   The destination mask.
 *
 * @thread  Any.
 */
static bool apicIsLogicalDest(PVMCPU pVCpu, uint32_t fDest)
{
    if (XAPIC_IN_X2APIC_MODE(pVCpu))
    {
        /*
         * Flat logical mode is not supported in x2APIC mode.
         * In clustered logical mode, the 32-bit logical ID in the LDR is interpreted as follows:
         *    - High 16 bits is the cluster ID.
         *    - Low 16 bits: each bit represents a unique APIC within the cluster.
         */
        PCX2APICPAGE pX2ApicPage = VMCPU_TO_CX2APICPAGE(pVCpu);
        uint32_t const u32Ldr    = pX2ApicPage->ldr.u32LogicalApicId;
        if (X2APIC_LDR_GET_CLUSTER_ID(u32Ldr) == (fDest & X2APIC_LDR_CLUSTER_ID))
            return RT_BOOL(u32Ldr & fDest & X2APIC_LDR_LOGICAL_ID);
        return false;
    }

#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    /*
     * In both flat and clustered logical mode, a destination mask of all set bits indicates a broadcast.
     * See AMD spec. 16.6.1 "Receiving System and IPI Interrupts".
     */
    Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));
    if ((fDest & XAPIC_LDR_FLAT_LOGICAL_ID) == XAPIC_LDR_FLAT_LOGICAL_ID)
        return true;

    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    XAPICDESTFORMAT enmDestFormat = (XAPICDESTFORMAT)pXApicPage->dfr.u.u4Model;
    if (enmDestFormat == XAPICDESTFORMAT_FLAT)
    {
        /* The destination mask is interpreted as a bitmap of 8 unique logical APIC IDs. */
        uint8_t const u8Ldr = pXApicPage->ldr.u.u8LogicalApicId;
        return RT_BOOL(u8Ldr & fDest & XAPIC_LDR_FLAT_LOGICAL_ID);
    }
    else
    {
        /*
         * In clustered logical mode, the 8-bit logical ID in the LDR is interpreted as follows:
         *    - High 4 bits is the cluster ID.
         *    - Low 4 bits: each bit represents a unique APIC within the cluster.
         */
        Assert(enmDestFormat == XAPICDESTFORMAT_CLUSTER);
        uint8_t const u8Ldr = pXApicPage->ldr.u.u8LogicalApicId;
        if (XAPIC_LDR_CLUSTERED_GET_CLUSTER_ID(u8Ldr) == (fDest & XAPIC_LDR_CLUSTERED_CLUSTER_ID))
            return RT_BOOL(u8Ldr & fDest & XAPIC_LDR_CLUSTERED_LOGICAL_ID);
        return false;
    }
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
}


/**
 * Figures out the set of destination CPUs for a given destination mode, format
 * and delivery mode setting.
 *
 * @param   pVM             The cross context VM structure.
 * @param   fDestMask       The destination mask.
 * @param   fBroadcastMask  The broadcast mask.
 * @param   enmDestMode     The destination mode.
 * @param   enmDeliveryMode The delivery mode.
 * @param   pDestCpuSet     The destination CPU set to update.
 */
static void apicGetDestCpuSet(PVM pVM, uint32_t fDestMask, uint32_t fBroadcastMask, XAPICDESTMODE enmDestMode,
                              XAPICDELIVERYMODE enmDeliveryMode, PVMCPUSET pDestCpuSet)
{
    VMCPUSET_EMPTY(pDestCpuSet);

    /*
     * Physical destination mode only supports either a broadcast or a single target.
     *    - Broadcast with lowest-priority delivery mode is not supported[1], we deliver it
     *      as a regular broadcast like in fixed delivery mode.
     *    - For a single target, lowest-priority delivery mode makes no sense. We deliver
     *      to the target like in fixed delivery mode.
     *
     * [1] See Intel spec. 10.6.2.1 "Physical Destination Mode".
     */
    if (   enmDestMode == XAPICDESTMODE_PHYSICAL
        && enmDeliveryMode == XAPICDELIVERYMODE_LOWEST_PRIO)
    {
        AssertMsgFailed(("APIC: Lowest-priority delivery using physical destination mode!"));
        enmDeliveryMode = XAPICDELIVERYMODE_FIXED;
    }

    uint32_t const cCpus = pVM->cCpus;
    if (enmDeliveryMode == XAPICDELIVERYMODE_LOWEST_PRIO)
    {
        Assert(enmDestMode == XAPICDESTMODE_LOGICAL);
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
        VMCPUID idCpuLowestTpr = NIL_VMCPUID;
        uint8_t u8LowestTpr    = UINT8_C(0xff);
        for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        {
            PVMCPU pVCpuDest = &pVM->aCpus[idCpu];
            if (apicIsLogicalDest(pVCpuDest, fDestMask))
            {
                PCXAPICPAGE   pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpuDest);
                uint8_t const u8Tpr      = pXApicPage->tpr.u8Tpr;         /* PAV */

                /* If there is a tie for lowest priority, the local APIC with the highest ID is chosen.
                   See AMD spec. 16.6.2 "Lowest Priority Messages and Arbitration". */
                if (u8LowestTpr <= u8Tpr)
                {
                    u8LowestTpr    = u8Tpr;
                    idCpuLowestTpr = idCpu;
                }
            }
        }
        if (idCpuLowestTpr != NIL_VMCPUID)
            VMCPUSET_ADD(pDestCpuSet, idCpuLowestTpr);
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
        return;
    }

    /*
     * x2APIC:
     *    - In both physical and logical destination mode, a destination mask of 0xffffffff implies a broadcast[1].
     * xAPIC:
     *    - In physical destination mode, a destination mask of 0xff implies a broadcast[2].
     *    - In both flat and clustered logical mode, a destination mask of 0xff implies a broadcast[3].
     *
     * [1] See Intel spec. 10.12.9 "ICR Operation in x2APIC Mode".
     * [2] See Intel spec. 10.6.2.1 "Physical Destination Mode".
     * [2] See AMD spec. 16.6.1 "Receiving System and IPI Interrupts".
     */
    if ((fDestMask & fBroadcastMask) == fBroadcastMask)
    {
        VMCPUSET_FILL(pDestCpuSet);
        return;
    }

    if (enmDestMode == XAPICDESTMODE_PHYSICAL)
    {
        /* The destination mask is interpreted as the physical APIC ID of a single target. */
#if 1
        /* Since our physical APIC ID is read-only to software, set the corresponding bit in the CPU set. */
        if (RT_LIKELY(fDestMask < cCpus))
            VMCPUSET_ADD(pDestCpuSet, fDestMask);
#else
        /* The physical APIC ID may not match our VCPU ID, search through the list of targets. */
        for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        {
            PVMCPU pVCpuDest = &pVM->aCpus[idCpu];
            if (XAPIC_IN_X2APIC_MODE(pVCpuDest))
            {
                PCX2APICPAGE pX2ApicPage = VMCPU_TO_CX2APICPAGE(pVCpuDest);
                if (pX2ApicPage->id.u32ApicId == fDestMask)
                    VMCPUSET_ADD(pDestCpuSet, pVCpuDest->idCpu);
            }
            else
            {
                PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpuDest);
                if (pXApicPage->id.u8ApicId == (uint8_t)fDestMask)
                    VMCPUSET_ADD(pDestCpuSet, pVCpuDest->idCpu);
            }
        }
#endif
    }
    else
    {
        Assert(enmDestMode == XAPICDESTMODE_LOGICAL);

        /* A destination mask of all 0's implies no target APICs (since it's interpreted as a bitmap or partial bitmap). */
        if (RT_UNLIKELY(!fDestMask))
            return;

        /* The destination mask is interpreted as a bitmap of software-programmable logical APIC ID of the target APICs. */
        for (VMCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        {
            PVMCPU pVCpuDest = &pVM->aCpus[idCpu];
            if (apicIsLogicalDest(pVCpuDest, fDestMask))
                VMCPUSET_ADD(pDestCpuSet, pVCpuDest->idCpu);
        }
    }
}


/**
 * Sends an Interprocessor Interrupt (IPI) using values from the Interrupt
 * Command Register (ICR).
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   rcRZ            The return code if the operation cannot be
 *                          performed in the current context.
 */
static VBOXSTRICTRC apicSendIpi(PVMCPU pVCpu, int rcRZ)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    XAPICDELIVERYMODE  const enmDeliveryMode  = (XAPICDELIVERYMODE)pXApicPage->icr_lo.u.u3DeliveryMode;
    XAPICDESTMODE const      enmDestMode      = (XAPICDESTMODE)pXApicPage->icr_lo.u.u1DestMode;
    XAPICINITLEVEL           enmInitLevel     = (XAPICINITLEVEL)pXApicPage->icr_lo.u.u1Level;
    XAPICTRIGGERMODE         enmTriggerMode   = (XAPICTRIGGERMODE)pXApicPage->icr_lo.u.u1TriggerMode;
    XAPICDESTSHORTHAND const enmDestShorthand = (XAPICDESTSHORTHAND)pXApicPage->icr_lo.u.u2DestShorthand;
    uint8_t const            uVector          = pXApicPage->icr_lo.u.u8Vector;

    PX2APICPAGE pX2ApicPage = VMCPU_TO_X2APICPAGE(pVCpu);
    uint32_t const fDest    = XAPIC_IN_X2APIC_MODE(pVCpu) ? pX2ApicPage->icr_hi.u32IcrHi : pXApicPage->icr_hi.u.u8Dest;

#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    /*
     * INIT Level De-assert is not support on Pentium 4 and Xeon processors.
     */
    if (RT_UNLIKELY(   enmDeliveryMode == XAPICDELIVERYMODE_INIT_LEVEL_DEASSERT
                    && enmInitLevel    == XAPICINITLEVEL_DEASSERT
                    && enmTriggerMode  == XAPICTRIGGERMODE_LEVEL))
    {
        return VINF_SUCCESS;
    }

    enmInitLevel   = XAPICINITLEVEL_ASSERT;
    enmTriggerMode = XAPICTRIGGERMODE_EDGE;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif

    /*
     * The destination and delivery modes are ignored/by-passed when a destination shorthand is specified.
     * See Intel spec. 10.6.2.3 "Broadcast/Self Delivery Mode".
     */
    VMCPUSET DestCpuSet;
    switch (enmDestShorthand)
    {
        case XAPICDESTSHORTHAND_NONE:
        {
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            uint32_t const fBroadcastMask = XAPIC_IN_X2APIC_MODE(pVCpu) ? X2APIC_ID_BROADCAST_MASK : XAPIC_ID_BROADCAST_MASK;
            apicGetDestCpuSet(pVM, fDest, fBroadcastMask, enmDestMode, enmDeliveryMode, &DestCpuSet);
            break;
        }

        case XAPICDESTSHORTHAND_SELF:
        {
            VMCPUSET_EMPTY(&DestCpuSet);
            VMCPUSET_ADD(&DestCpuSet, pVCpu->idCpu);
            break;
        }

        case XAPIDDESTSHORTHAND_ALL_INCL_SELF:
        {
            VMCPUSET_FILL(&DestCpuSet);
            break;
        }

        case XAPICDESTSHORTHAND_ALL_EXCL_SELF:
        {
            VMCPUSET_FILL(&DestCpuSet);
            VMCPUSET_DEL(&DestCpuSet, pVCpu->idCpu);
            break;
        }
    }

    return apicSendIntr(pVCpu, uVector, enmTriggerMode, enmDeliveryMode, &DestCpuSet, rcRZ);
}


/**
 * Sets the Interrupt Command Register (ICR) high dword.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uIcrHi          The ICR high dword.
 */
static VBOXSTRICTRC apicSetIcrHi(PVMCPU pVCpu, uint32_t uIcrHi)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    pXApicPage->icr_hi.all.u32IcrHi = uIcrHi & XAPIC_ICR_HI_DEST;
    return VINF_SUCCESS;
}


/**
 * Sets the Interrupt Command Register (ICR) low dword.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uIcrLo          The ICR low dword.
 * @param   rcRZ            The return code if the operation cannot be performed
 *                          in the current context.
 */
static VBOXSTRICTRC apicSetIcrLo(PVMCPU pVCpu, uint32_t uIcrLo, int rcRZ)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PXAPICPAGE pXApicPage  = VMCPU_TO_XAPICPAGE(pVCpu);
    pXApicPage->icr_lo.all.u32IcrLo = uIcrLo & XAPIC_ICR_LO_WR;

    apicSendIpi(pVCpu, rcRZ);
    return VINF_SUCCESS;
}


/**
 * Sets the Interrupt Command Register (ICR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u64Icr          The ICR (High and Low combined).
 * @param   rcRZ            The return code if the operation cannot be performed
 *                          in the current context.
 */
static VBOXSTRICTRC apicSetIcr(PVMCPU pVCpu, uint64_t u64Icr, int rcRZ)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(XAPIC_IN_X2APIC_MODE(pVCpu));

    /* Validate. */
    uint32_t const uLo = RT_LO_U32(u64Icr);
    if (RT_LIKELY(!(uLo & ~XAPIC_ICR_LO_WR)))
    {
        /* Update high dword first, then update the low dword which sends the IPI. */
        PX2APICPAGE pX2ApicPage = VMCPU_TO_X2APICPAGE(pVCpu);
        pX2ApicPage->icr_hi.u32IcrHi = RT_HI_U32(u64Icr);
        return apicSetIcrLo(pVCpu, uLo,  rcRZ);
    }
    return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_ICR, APICMSRACCESS_WRITE_RSVD_BITS);
}


/**
 * Sets the Error Status Register (ESR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uValue          The ESR value.
 */
static VBOXSTRICTRC apicSetEsr(PVMCPU pVCpu, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uValue & ~XAPIC_ESR_WO))
        return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_ESR, APICMSRACCESS_WRITE_RSVD_BITS);

    /*
     * Writes to the ESR causes the internal state to be updated in the register,
     * clearing the original state. See AMD spec. 16.4.6 "APIC Error Interrupts".
     */
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    pXApicPage->esr.all.u32Errors = apicClearAllErrors(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Updates the Processor Priority Register (PPR).
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void apicUpdatePpr(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    /* See Intel spec 10.8.3.1 "Task and Processor Priorities". */
    PXAPICPAGE    pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    uint8_t const uIsrv      = apicGetLastSetBit(&pXApicPage->isr, 0 /* rcNotFound */);
    uint8_t       uPpr;
    if (XAPIC_TPR_GET_TP(pXApicPage->tpr.u8Tpr) >= XAPIC_PPR_GET_PP(uIsrv))
        uPpr = pXApicPage->tpr.u8Tpr;
    else
        uPpr = XAPIC_PPR_GET_PP(uIsrv);
    pXApicPage->ppr.u8Ppr = uPpr;
}


/**
 * Gets the Processor Priority Register (PPR).
 *
 * @returns The PPR value.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static uint8_t apicGetPpr(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    /*
     * With virtualized APIC registers or with TPR virtualization, the hardware may
     * update ISR/TPR transparently. We thus re-calculate the PPR which may be out of sync.
     * See Intel spec. 29.2.2 "Virtual-Interrupt Delivery".
     */
    PCAPIC pApic = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    if (pApic->fVirtApicRegsEnabled)        /** @todo re-think this */
        apicUpdatePpr(pVCpu);
    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    return pXApicPage->ppr.u8Ppr;
}


/**
 * Sets the Task Priority Register (TPR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uTpr            The TPR value.
 */
static VBOXSTRICTRC apicSetTpr(PVMCPU pVCpu, uint32_t uTpr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uTpr & ~XAPIC_TPR))
        return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_TPR, APICMSRACCESS_WRITE_RSVD_BITS);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    pXApicPage->tpr.u8Tpr = uTpr;
    apicUpdatePpr(pVCpu);
    apicSignalNextPendingIntr(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Sets the End-Of-Interrupt (EOI) register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uEoi            The EOI value.
 */
static VBOXSTRICTRC apicSetEoi(PVMCPU pVCpu, uint32_t uEoi)
{
    VMCPU_ASSERT_EMT(pVCpu);

    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uEoi & ~XAPIC_EOI_WO))
        return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_EOI, APICMSRACCESS_WRITE_RSVD_BITS);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    int isrv = apicGetLastSetBit(&pXApicPage->isr, -1 /* rcNotFound */);
    if (isrv >= 0)
    {
        /*
         * Dispensing the spurious-interrupt vector does not affect the ISR.
         * See Intel spec. 10.9 "Spurious Interrupt".
         */
        uint8_t const uVector = isrv;
        if (uVector != pXApicPage->svr.u.u8SpuriousVector)
        {
            apicClearVectorInReg(&pXApicPage->isr, uVector);
            apicUpdatePpr(pVCpu);
            bool fLevelTriggered = apicTestVectorInReg(&pXApicPage->tmr, uVector);
            if (fLevelTriggered)
            {
                /** @todo We need to broadcast EOI to IO APICs here. */
                apicClearVectorInReg(&pXApicPage->tmr, uVector);
            }

            apicSignalNextPendingIntr(pVCpu);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Sets the Logical Destination Register (LDR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uLdr            The LDR value.
 *
 * @remarks LDR is read-only in x2APIC mode.
 */
static VBOXSTRICTRC apicSetLdr(PVMCPU pVCpu, uint32_t uLdr)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    apicWriteRaw32(pXApicPage, XAPIC_OFF_LDR, uLdr & XAPIC_LDR);
    return VINF_SUCCESS;
}


/**
 * Sets the Destination Format Register (DFR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uDfr            The DFR value.
 *
 * @remarks DFR is not available in x2APIC mode.
 */
static VBOXSTRICTRC apicSetDfr(PVMCPU pVCpu, uint32_t uDfr)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    apicWriteRaw32(pXApicPage, XAPIC_OFF_DFR, uDfr & XAPIC_DFR);
    return VINF_SUCCESS;
}


/**
 * Sets the Timer Divide Configuration Register (DCR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uTimerDcr       The timer DCR value.
 */
static VBOXSTRICTRC apicSetTimerDcr(PVMCPU pVCpu, uint32_t uTimerDcr)
{
    VMCPU_ASSERT_EMT(pVCpu);
    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uTimerDcr & ~XAPIC_TIMER_DCR))
        return apicMsrAccessError(pVCpu, MSR_IA32_X2APIC_TIMER_DCR, APICMSRACCESS_WRITE_RSVD_BITS);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    apicWriteRaw32(pXApicPage, XAPIC_OFF_TIMER_DCR, uTimerDcr);
    return VINF_SUCCESS;
}


/**
 * Gets the timer's Current Count Register (CCR).
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   rcBusy          The busy return code for the timer critical section.
 * @param   puValue         Where to store the LVT timer CCR.
 */
static VBOXSTRICTRC apicGetTimerCcr(PVMCPU pVCpu, int rcBusy, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(puValue);

    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    *puValue = 0;

    /* In TSC-deadline mode, CCR returns 0, see Intel spec. 10.5.4.1 "TSC-Deadline Mode". */
    if (pXApicPage->lvt_timer.u.u2TimerMode == XAPIC_TIMER_MODE_TSC_DEADLINE)
        return VINF_SUCCESS;

    /* If the initial-count register is 0, CCR returns 0 as it cannot exceed the ICR. */
    uint32_t const uInitialCount = pXApicPage->timer_icr.u32InitialCount;
    if (!uInitialCount)
        return VINF_SUCCESS;

    /*
     * Reading the virtual-sync clock requires locking its timer because it's not
     * a simple atomic operation, see tmVirtualSyncGetEx().
     *
     * We also need to lock before reading the timer CCR, see apicR3TimerCallback().
     */
    PCAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    PTMTIMER  pTimer   = CTX_SUFF(pApicCpu->pTimer);

    int rc = TMTimerLock(pTimer, rcBusy);
    if (rc == VINF_SUCCESS)
    {
        /* If the current-count register is 0, it implies the timer expired. */
        uint32_t const uCurrentCount = pXApicPage->timer_ccr.u32CurrentCount;
        if (uCurrentCount)
        {
            uint64_t const cTicksElapsed = TMTimerGet(pApicCpu->CTX_SUFF(pTimer)) - pApicCpu->u64TimerInitial;
            TMTimerUnlock(pTimer);
            uint8_t  const uTimerShift   = apicGetTimerShift(pXApicPage);
            uint64_t const uDelta        = cTicksElapsed >> uTimerShift;
            if (uInitialCount > uDelta)
                *puValue = uInitialCount - uDelta;
        }
        else
            TMTimerUnlock(pTimer);
    }
    return rc;
}


/**
 * Sets the timer's Initial-Count Register (ICR).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   rcBusy          The busy return code for the timer critical section.
 * @param   uInitialCount   The timer ICR.
 */
static VBOXSTRICTRC apicSetTimerIcr(PVMCPU pVCpu, int rcBusy, uint32_t uInitialCount)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PAPIC      pApic      = VM_TO_APIC(CTX_SUFF(pVCpu->pVM));
    PAPICCPU   pApicCpu   = VMCPU_TO_APICCPU(pVCpu);
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    PTMTIMER   pTimer     = CTX_SUFF(pApicCpu->pTimer);

    /* In TSC-deadline mode, timer ICR writes are ignored, see Intel spec. 10.5.4.1 "TSC-Deadline Mode". */
    if (   pApic->fSupportsTscDeadline
        && pXApicPage->lvt_timer.u.u2TimerMode == XAPIC_TIMER_MODE_TSC_DEADLINE)
        return VINF_SUCCESS;

    /*
     * The timer CCR may be modified by apicR3TimerCallback() in parallel,
     * so obtain the lock -before- updating it here to be consistent with the
     * timer ICR. We rely on CCR being consistent in apicGetTimerCcr().
     */
    int rc = TMTimerLock(pTimer, rcBusy);
    if (rc == VINF_SUCCESS)
    {
        pXApicPage->timer_icr.u32InitialCount = uInitialCount;
        pXApicPage->timer_ccr.u32CurrentCount = uInitialCount;
        if (uInitialCount)
            APICStartTimer(pApicCpu, uInitialCount);
        else
            APICStopTimer(pApicCpu);
        TMTimerUnlock(pTimer);
    }
    return rc;
}


/**
 * Sets an LVT entry.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offLvt          The LVT entry offset in the xAPIC page.
 * @param   uLvt            The LVT value to set.
 */
static VBOXSTRICTRC apicSetLvtEntry(PVMCPU pVCpu, uint16_t offLvt, uint32_t uLvt)
{
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    VMCPU_ASSERT_EMT(pVCpu);
    AssertMsg(   offLvt == XAPIC_OFF_LVT_TIMER
              || offLvt == XAPIC_OFF_LVT_THERMAL
              || offLvt == XAPIC_OFF_LVT_PERF
              || offLvt == XAPIC_OFF_LVT_LINT0
              || offLvt == XAPIC_OFF_LVT_LINT1
              || offLvt == XAPIC_OFF_LVT_ERROR,
             ("APIC%u: apicSetLvtEntry: invalid offset, offLvt=%#x, uLvt=%#x\n", pVCpu->idCpu, offLvt, uLvt));

    /*
     * If TSC-deadline mode isn't support, ignore the bit in xAPIC mode
     * and raise #GP(0) in x2APIC mode.
     */
    PCAPIC pApic = VM_TO_APIC(CTX_SUFF(pVCpu->pVM));
    if (offLvt == XAPIC_OFF_LVT_TIMER)
    {
        if (   !pApic->fSupportsTscDeadline
            && (uLvt & XAPIC_LVT_TIMER_TSCDEADLINE))
        {
            if (XAPIC_IN_X2APIC_MODE(pVCpu))
                return apicMsrAccessError(pVCpu, XAPIC_GET_X2APIC_MSR(offLvt), APICMSRACCESS_WRITE_RSVD_BITS);
            uLvt &= ~XAPIC_LVT_TIMER_TSCDEADLINE;
            /** @todo TSC-deadline timer mode transition */
        }
    }

    /*
     * Validate rest of the LVT bits.
     */
    uint16_t const idxLvt = (offLvt - XAPIC_OFF_LVT_START) >> 4;
    AssertReturn(idxLvt < RT_ELEMENTS(g_au32LvtValidMasks), VERR_OUT_OF_RANGE);

    if (   XAPIC_IN_X2APIC_MODE(pVCpu)
        && (uLvt & ~g_au32LvtValidMasks[idxLvt]))
        return apicMsrAccessError(pVCpu, XAPIC_GET_X2APIC_MSR(offLvt), APICMSRACCESS_WRITE_RSVD_BITS);

    uLvt &= g_au32LvtValidMasks[idxLvt];

    /*
     * In the software-disabled state, LVT mask-bit must remain set and attempts to clear the mask
     * bit must be ignored. See Intel spec. 10.4.7.2 "Local APIC State After It Has Been Software Disabled".
     */
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    AssertCompile(RT_OFFSETOF(XAPICPAGE, svr) == RT_OFFSETOF(X2APICPAGE, svr));
    if (!pXApicPage->svr.u.fApicSoftwareEnable)
        uLvt |= XAPIC_LVT_MASK;

    /*
     * It is unclear whether we should signal a 'send illegal vector' error here and ignore updating
     * the LVT entry when the delivery mode is 'fixed'[1] or update it in addition to signaling the
     * error or not signal the error at all. For now, we'll allow setting illegal vectors into the LVT
     * but set the 'send illegal vector' error here. The 'receive illegal vector' error will be set if
     * the interrupt for the vector happens to be generated, see APICPostInterrupt().
     *
     * [1] See Intel spec. 10.5.2 "Valid Interrupt Vectors".
     */
    if (RT_UNLIKELY(   XAPIC_LVT_GET_VECTOR(uLvt) <= XAPIC_ILLEGAL_VECTOR_END
                    && XAPIC_LVT_GET_DELIVERY_MODE(uLvt) == XAPICDELIVERYMODE_FIXED))
        apicSetError(pVCpu, XAPIC_ESR_SEND_ILLEGAL_VECTOR);

    apicWriteRaw32(pXApicPage, offLvt, uLvt);
    return VINF_SUCCESS;
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif  /* XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4 */
}


#if 0
/**
 * Sets an LVT entry in the extended LVT range.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offLvt          The LVT entry offset in the xAPIC page.
 * @param   uValue          The LVT value to set.
 */
static int apicSetLvtExtEntry(PVMCPU pVCpu, uint16_t offLvt, uint32_t uLvt)
{
    VMCPU_ASSERT_EMT(pVCpu);
    AssertMsg(offLvt == XAPIC_OFF_CMCI, ("APIC%u: apicSetLvt1Entry: invalid offset %#x\n", pVCpu->idCpu, offLvt));

    /** @todo support CMCI. */
    return VERR_NOT_IMPLEMENTED;
}
#endif


/**
 * Hints TM about the APIC timer frequency.
 *
 * @param   pApicCpu        The APIC CPU state.
 * @param   uInitialCount   The new initial count.
 * @param   uTimerShift     The new timer shift.
 * @thread  Any.
 */
static void apicHintTimerFreq(PAPICCPU pApicCpu, uint32_t uInitialCount, uint8_t uTimerShift)
{
    Assert(pApicCpu);
    Assert(TMTimerIsLockOwner(CTX_SUFF(pApicCpu->pTimer)));

    if (   pApicCpu->uHintedTimerInitialCount != uInitialCount
        || pApicCpu->uHintedTimerShift        != uTimerShift)
    {
        uint32_t uHz;
        if (uInitialCount)
        {
            uint64_t cTicksPerPeriod = (uint64_t)uInitialCount << uTimerShift;
            uHz = TMTimerGetFreq(pApicCpu->CTX_SUFF(pTimer)) / cTicksPerPeriod;
        }
        else
            uHz = 0;

        TMTimerSetFrequencyHint(pApicCpu->CTX_SUFF(pTimer), uHz);
        pApicCpu->uHintedTimerInitialCount = uInitialCount;
        pApicCpu->uHintedTimerShift = uTimerShift;
    }
}


/**
 * Reads an APIC register.
 *
 * @returns VBox status code.
 * @param   pApicDev        The APIC device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
static int apicReadRegister(PAPICDEV pApicDev, PVMCPU pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(offReg <= XAPIC_OFF_MAX_VALID);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    uint32_t   uValue = 0;
    int        rc = VINF_SUCCESS;
    switch (offReg)
    {
        case XAPIC_OFF_ID:
        case XAPIC_OFF_VERSION:
        case XAPIC_OFF_TPR:
        case XAPIC_OFF_EOI:
        case XAPIC_OFF_RRD:
        case XAPIC_OFF_LDR:
        case XAPIC_OFF_DFR:
        case XAPIC_OFF_SVR:
        case XAPIC_OFF_ISR0:    case XAPIC_OFF_ISR1:    case XAPIC_OFF_ISR2:    case XAPIC_OFF_ISR3:
        case XAPIC_OFF_ISR4:    case XAPIC_OFF_ISR5:    case XAPIC_OFF_ISR6:    case XAPIC_OFF_ISR7:
        case XAPIC_OFF_TMR0:    case XAPIC_OFF_TMR1:    case XAPIC_OFF_TMR2:    case XAPIC_OFF_TMR3:
        case XAPIC_OFF_TMR4:    case XAPIC_OFF_TMR5:    case XAPIC_OFF_TMR6:    case XAPIC_OFF_TMR7:
        case XAPIC_OFF_IRR0:    case XAPIC_OFF_IRR1:    case XAPIC_OFF_IRR2:    case XAPIC_OFF_IRR3:
        case XAPIC_OFF_IRR4:    case XAPIC_OFF_IRR5:    case XAPIC_OFF_IRR6:    case XAPIC_OFF_IRR7:
        case XAPIC_OFF_ESR:
        case XAPIC_OFF_ICR_LO:
        case XAPIC_OFF_ICR_HI:
        case XAPIC_OFF_LVT_TIMER:
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
        case XAPIC_OFF_LVT_THERMAL:
#endif
        case XAPIC_OFF_LVT_PERF:
        case XAPIC_OFF_LVT_LINT0:
        case XAPIC_OFF_LVT_LINT1:
        case XAPIC_OFF_LVT_ERROR:
        case XAPIC_OFF_TIMER_ICR:
        case XAPIC_OFF_TIMER_DCR:
        {
            Assert(   !XAPIC_IN_X2APIC_MODE(pVCpu)
                   || (   offReg != XAPIC_OFF_DFR
                       && offReg != XAPIC_OFF_ICR_HI
                       && offReg != XAPIC_OFF_EOI));
            uValue = apicReadRaw32(pXApicPage, offReg);
            break;
        }

        case XAPIC_OFF_PPR:
        {
            uValue = apicGetPpr(pVCpu);
            break;
        }

        case XAPIC_OFF_TIMER_CCR:
        {
            Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));
            rc = VBOXSTRICTRC_VAL(apicGetTimerCcr(pVCpu, VINF_IOM_R3_MMIO_READ, &uValue));
            break;
        }

        case XAPIC_OFF_APR:
        {
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
            /* Unsupported on Pentium 4 and Xeon CPUs, invalid in x2APIC mode. */
            Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
            break;
        }

        default:
        {
            Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));
            rc = PDMDevHlpDBGFStop(pApicDev->CTX_SUFF(pDevIns), RT_SRC_POS, "offReg=%#x Id=%u\n", offReg, pVCpu->idCpu);
            apicSetError(pVCpu, XAPIC_ESR_ILLEGAL_REG_ADDRESS);
            break;
        }
    }

    *puValue = uValue;
    return rc;
}


/**
 * Writes an APIC register.
 *
 * @returns Strict VBox status code.
 * @param   pApicDev        The APIC device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
static VBOXSTRICTRC apicWriteRegister(PAPICDEV pApicDev, PVMCPU pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(offReg <= XAPIC_OFF_MAX_VALID);
    Assert(!XAPIC_IN_X2APIC_MODE(pVCpu));

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case XAPIC_OFF_TPR:
        {
            rcStrict = apicSetTpr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_LVT_TIMER:
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
        case XAPIC_OFF_LVT_THERMAL:
#endif
        case XAPIC_OFF_LVT_PERF:
        case XAPIC_OFF_LVT_LINT0:
        case XAPIC_OFF_LVT_LINT1:
        case XAPIC_OFF_LVT_ERROR:
        {
            rcStrict = apicSetLvtEntry(pVCpu, offReg, uValue);
            break;
        }

        case XAPIC_OFF_TIMER_ICR:
        {
            rcStrict = apicSetTimerIcr(pVCpu, VINF_IOM_R3_MMIO_WRITE, uValue);
            break;
        }

        case XAPIC_OFF_EOI:
        {
            rcStrict = apicSetEoi(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_LDR:
        {
            rcStrict = apicSetLdr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_DFR:
        {
            rcStrict = apicSetDfr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_SVR:
        {
            rcStrict = apicSetSvr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_ICR_LO:
        {
            rcStrict = apicSetIcrLo(pVCpu, uValue, VINF_IOM_R3_MMIO_WRITE);
            break;
        }

        case XAPIC_OFF_ICR_HI:
        {
            rcStrict = apicSetIcrHi(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_TIMER_DCR:
        {
            rcStrict = apicSetTimerDcr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_ESR:
        {
            rcStrict = apicSetEsr(pVCpu, uValue);
            break;
        }

        case XAPIC_OFF_APR:
        case XAPIC_OFF_RRD:
        {
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
            /* Unsupported on Pentium 4 and Xeon CPUs but writes do -not- set an illegal register access error. */
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif
            break;
        }

        /* Unavailable/reserved in xAPIC mode: */
        case X2APIC_OFF_SELF_IPI:
        /* Read-only registers: */
        case XAPIC_OFF_ID:
        case XAPIC_OFF_VERSION:
        case XAPIC_OFF_PPR:
        case XAPIC_OFF_ISR0:    case XAPIC_OFF_ISR1:    case XAPIC_OFF_ISR2:    case XAPIC_OFF_ISR3:
        case XAPIC_OFF_ISR4:    case XAPIC_OFF_ISR5:    case XAPIC_OFF_ISR6:    case XAPIC_OFF_ISR7:
        case XAPIC_OFF_TMR0:    case XAPIC_OFF_TMR1:    case XAPIC_OFF_TMR2:    case XAPIC_OFF_TMR3:
        case XAPIC_OFF_TMR4:    case XAPIC_OFF_TMR5:    case XAPIC_OFF_TMR6:    case XAPIC_OFF_TMR7:
        case XAPIC_OFF_IRR0:    case XAPIC_OFF_IRR1:    case XAPIC_OFF_IRR2:    case XAPIC_OFF_IRR3:
        case XAPIC_OFF_IRR4:    case XAPIC_OFF_IRR5:    case XAPIC_OFF_IRR6:    case XAPIC_OFF_IRR7:
        case XAPIC_OFF_TIMER_CCR:
        default:
        {
            rcStrict = PDMDevHlpDBGFStop(pApicDev->CTX_SUFF(pDevIns), RT_SRC_POS, "APIC%u: offReg=%#x\n", pVCpu->idCpu, offReg);
            apicSetError(pVCpu, XAPIC_ESR_ILLEGAL_REG_ADDRESS);
            break;
        }
    }

    return rcStrict;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnReadMsrR3}
 */
VMMDECL(VBOXSTRICTRC) APICReadMsr(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(u32Reg >= MSR_IA32_X2APIC_START && u32Reg <= MSR_IA32_X2APIC_END);
    Assert(pu64Value);

    PCAPIC pApic = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    if (pApic->fRZEnabled)
    { /* likely */}
    else
        return VINF_CPUM_R3_MSR_READ;

    STAM_COUNTER_INC(&VMCPU_TO_APICCPU(pVCpu)->StatMsrRead);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    if (RT_LIKELY(XAPIC_IN_X2APIC_MODE(pVCpu)))
    {
        switch (u32Reg)
        {
            /* Special handling for x2APIC: */
            case MSR_IA32_X2APIC_ICR:
            {
                PCX2APICPAGE pX2ApicPage = VMCPU_TO_CX2APICPAGE(pVCpu);
                uint64_t const uHi = pX2ApicPage->icr_hi.u32IcrHi;
                uint64_t const uLo = pX2ApicPage->icr_lo.all.u32IcrLo;
                *pu64Value = RT_MAKE_U64(uLo, uHi);
                break;
            }

            /* Special handling, compatible with xAPIC: */
            case MSR_IA32_X2APIC_TIMER_CCR:
            {
                uint32_t uValue;
                rcStrict = apicGetTimerCcr(pVCpu, VINF_CPUM_R3_MSR_READ, &uValue);
                *pu64Value = uValue;
                break;
            }

            /* Special handling, compatible with xAPIC: */
            case MSR_IA32_X2APIC_PPR:
            {
                *pu64Value = apicGetPpr(pVCpu);
                break;
            }

            /* Raw read, compatible with xAPIC: */
            case MSR_IA32_X2APIC_ID:
            case MSR_IA32_X2APIC_VERSION:
            case MSR_IA32_X2APIC_TPR:
            case MSR_IA32_X2APIC_LDR:
            case MSR_IA32_X2APIC_SVR:
            case MSR_IA32_X2APIC_ISR0:  case MSR_IA32_X2APIC_ISR1:  case MSR_IA32_X2APIC_ISR2:  case MSR_IA32_X2APIC_ISR3:
            case MSR_IA32_X2APIC_ISR4:  case MSR_IA32_X2APIC_ISR5:  case MSR_IA32_X2APIC_ISR6:  case MSR_IA32_X2APIC_ISR7:
            case MSR_IA32_X2APIC_TMR0:  case MSR_IA32_X2APIC_TMR1:  case MSR_IA32_X2APIC_TMR2:  case MSR_IA32_X2APIC_TMR3:
            case MSR_IA32_X2APIC_TMR4:  case MSR_IA32_X2APIC_TMR5:  case MSR_IA32_X2APIC_TMR6:  case MSR_IA32_X2APIC_TMR7:
            case MSR_IA32_X2APIC_IRR0:  case MSR_IA32_X2APIC_IRR1:  case MSR_IA32_X2APIC_IRR2:  case MSR_IA32_X2APIC_IRR3:
            case MSR_IA32_X2APIC_IRR4:  case MSR_IA32_X2APIC_IRR5:  case MSR_IA32_X2APIC_IRR6:  case MSR_IA32_X2APIC_IRR7:
            case MSR_IA32_X2APIC_ESR:
            case MSR_IA32_X2APIC_LVT_TIMER:
            case MSR_IA32_X2APIC_LVT_THERMAL:
            case MSR_IA32_X2APIC_LVT_PERF:
            case MSR_IA32_X2APIC_LVT_LINT0:
            case MSR_IA32_X2APIC_LVT_LINT1:
            case MSR_IA32_X2APIC_LVT_ERROR:
            case MSR_IA32_X2APIC_TIMER_ICR:
            case MSR_IA32_X2APIC_TIMER_DCR:
            {
                PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
                uint16_t const offReg = X2APIC_GET_XAPIC_OFF(u32Reg);
                *pu64Value = apicReadRaw32(pXApicPage, offReg);
                break;
            }

            /* Write-only MSRs: */
            case MSR_IA32_X2APIC_SELF_IPI:
            case MSR_IA32_X2APIC_EOI:
            {
                rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_READ_WRITE_ONLY);
                break;
            }

            /* Reserved MSRs: */
            case MSR_IA32_X2APIC_LVT_CMCI:
            default:
            {
                rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_READ_RSVD_OR_UNKNOWN);
                break;
            }
        }
    }
    else
        rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_INVALID_READ_MODE);

    return rcStrict;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnWriteMsrR3}
 */
VMMDECL(VBOXSTRICTRC) APICWriteMsr(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(u32Reg >= MSR_IA32_X2APIC_START && u32Reg <= MSR_IA32_X2APIC_END);

    PCAPIC pApic = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    if (pApic->fRZEnabled)
    { /* likely */ }
    else
        return VINF_CPUM_R3_MSR_WRITE;

    STAM_COUNTER_INC(&VMCPU_TO_APICCPU(pVCpu)->StatMsrWrite);

    /*
     * In x2APIC mode, we need to raise #GP(0) for writes to reserved bits, unlike MMIO
     * accesses where they are ignored. Hence, we need to validate each register before
     * invoking the generic/xAPIC write functions.
     *
     * Bits 63:32 of all registers except the ICR are reserved, we'll handle this common
     * case first and handle validating the remaining bits on a per-register basis.
     * See Intel spec. 10.12.1.2 "x2APIC Register Address Space".
     */
    if (   u32Reg != MSR_IA32_X2APIC_ICR
        && RT_HI_U32(u64Value))
        return apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_WRITE_RSVD_BITS);

    uint32_t     u32Value = RT_LO_U32(u64Value);
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    if (RT_LIKELY(XAPIC_IN_X2APIC_MODE(pVCpu)))
    {
        switch (u32Reg)
        {
            case MSR_IA32_X2APIC_TPR:
            {
                rcStrict = apicSetTpr(pVCpu, u32Value);
                break;
            }

            case MSR_IA32_X2APIC_ICR:
            {
                rcStrict = apicSetIcr(pVCpu, u64Value, VINF_CPUM_R3_MSR_WRITE);
                break;
            }

            case MSR_IA32_X2APIC_SVR:
            {
                rcStrict = apicSetSvr(pVCpu, u32Value);
                break;
            }

            case MSR_IA32_X2APIC_ESR:
            {
                rcStrict = apicSetEsr(pVCpu, u32Value);
                break;
            }

            case MSR_IA32_X2APIC_TIMER_DCR:
            {
                rcStrict = apicSetTimerDcr(pVCpu, u32Value);
                break;
            }

            case MSR_IA32_X2APIC_LVT_TIMER:
            case MSR_IA32_X2APIC_LVT_THERMAL:
            case MSR_IA32_X2APIC_LVT_PERF:
            case MSR_IA32_X2APIC_LVT_LINT0:
            case MSR_IA32_X2APIC_LVT_LINT1:
            case MSR_IA32_X2APIC_LVT_ERROR:
            {
                rcStrict = apicSetLvtEntry(pVCpu, X2APIC_GET_XAPIC_OFF(u32Reg), u32Value);
                break;
            }

            case MSR_IA32_X2APIC_TIMER_ICR:
            {
                rcStrict = apicSetTimerIcr(pVCpu, VINF_CPUM_R3_MSR_WRITE, u32Value);
                break;
            }

            /* Write-only MSRs: */
            case MSR_IA32_X2APIC_SELF_IPI:
            {
                uint8_t const uVector = XAPIC_SELF_IPI_GET_VECTOR(u32Value);
                APICPostInterrupt(pVCpu, uVector, XAPICTRIGGERMODE_EDGE);
                rcStrict = VINF_SUCCESS;
                break;
            }

            case MSR_IA32_X2APIC_EOI:
            {
                rcStrict = apicSetEoi(pVCpu, u32Value);
                break;
            }

            /* Read-only MSRs: */
            case MSR_IA32_X2APIC_ID:
            case MSR_IA32_X2APIC_VERSION:
            case MSR_IA32_X2APIC_PPR:
            case MSR_IA32_X2APIC_LDR:
            case MSR_IA32_X2APIC_ISR0:  case MSR_IA32_X2APIC_ISR1:  case MSR_IA32_X2APIC_ISR2:  case MSR_IA32_X2APIC_ISR3:
            case MSR_IA32_X2APIC_ISR4:  case MSR_IA32_X2APIC_ISR5:  case MSR_IA32_X2APIC_ISR6:  case MSR_IA32_X2APIC_ISR7:
            case MSR_IA32_X2APIC_TMR0:  case MSR_IA32_X2APIC_TMR1:  case MSR_IA32_X2APIC_TMR2:  case MSR_IA32_X2APIC_TMR3:
            case MSR_IA32_X2APIC_TMR4:  case MSR_IA32_X2APIC_TMR5:  case MSR_IA32_X2APIC_TMR6:  case MSR_IA32_X2APIC_TMR7:
            case MSR_IA32_X2APIC_IRR0:  case MSR_IA32_X2APIC_IRR1:  case MSR_IA32_X2APIC_IRR2:  case MSR_IA32_X2APIC_IRR3:
            case MSR_IA32_X2APIC_IRR4:  case MSR_IA32_X2APIC_IRR5:  case MSR_IA32_X2APIC_IRR6:  case MSR_IA32_X2APIC_IRR7:
            case MSR_IA32_X2APIC_TIMER_CCR:
            {
                rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_WRITE_READ_ONLY);
                break;
            }

            /* Reserved MSRs: */
            case MSR_IA32_X2APIC_LVT_CMCI:
            default:
            {
                rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_WRITE_RSVD_OR_UNKNOWN);
                break;
            }
        }
    }
    else
        rcStrict = apicMsrAccessError(pVCpu, u32Reg, APICMSRACCESS_INVALID_WRITE_MODE);

    return rcStrict;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnSetBaseMsrR3}
 */
VMMDECL(VBOXSTRICTRC) APICSetBaseMsr(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint64_t u64BaseMsr)
{
    Assert(pVCpu);
    PAPICCPU pApicCpu   = VMCPU_TO_APICCPU(pVCpu);
    PAPIC    pApic      = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    APICMODE enmOldMode = apicGetMode(pApicCpu->uApicBaseMsr);
    APICMODE enmNewMode = apicGetMode(u64BaseMsr);
    uint64_t uBaseMsr   = pApicCpu->uApicBaseMsr;

    /** @todo probably go back to ring-3 for all cases regardless of
     *        fRZEnabled. Writing this MSR is not something guests
     *        typically do often, and therefore is not performance
     *        critical. We'll have better diagnostics in ring-3. */
    if (!pApic->fRZEnabled)
        return VINF_CPUM_R3_MSR_WRITE;

    /*
     * We do not support re-mapping the APIC base address because:
     *    - We'll have to manage all the mappings ourselves in the APIC (reference counting based unmapping etc.)
     *      i.e. we can only unmap the MMIO region if no other APIC is mapped on that location.
     *    - It's unclear how/if IOM can fallback to handling regions as regular memory (if the MMIO
     *      region remains mapped but doesn't belong to the called VCPU's APIC).
     */
    /** @todo Handle per-VCPU APIC base relocation. */
    if (MSR_APICBASE_GET_PHYSADDR(uBaseMsr) != XAPIC_APICBASE_PHYSADDR)
    {
#ifdef IN_RING3
        LogRelMax(5, ("APIC%u: Attempt to relocate base to %#RGp, unsupported -> #GP(0)\n", pVCpu->idCpu,
                      MSR_APICBASE_GET_PHYSADDR(uBaseMsr)));
        return VERR_CPUM_RAISE_GP_0;
#else
        return VINF_CPUM_R3_MSR_WRITE;
#endif
    }

    /*
     * Act on state transition.
     */
    /** @todo We need to update the CPUID according to the state, which we
     *        currently don't do as CPUMSetGuestCpuIdFeature() is setting
     *        per-VM CPUID bits while we need per-VCPU specific bits. */
    if (enmNewMode != enmOldMode)
    {
        switch (enmNewMode)
        {
            case APICMODE_DISABLED:
            {
#ifdef IN_RING3
                /*
                 * The APIC state needs to be reset (especially the APIC ID as x2APIC APIC ID bit layout
                 * is different). We can start with a clean slate identical to the state after a power-up/reset.
                 *
                 * See Intel spec. 10.4.3 "Enabling or Disabling the Local APIC".
                 */
                APICR3Reset(pVCpu);
                uBaseMsr &= ~(MSR_APICBASE_XAPIC_ENABLE_BIT | MSR_APICBASE_X2APIC_ENABLE_BIT);
#else
                return VINF_CPUM_R3_MSR_WRITE;
#endif
                break;
            }

            case APICMODE_XAPIC:
            {
                if (enmOldMode != APICMODE_DISABLED)
                {
                    Log(("APIC%u: Can only transition to xAPIC state from disabled state\n", pVCpu->idCpu));
                    return apicMsrAccessError(pVCpu, MSR_IA32_APICBASE, APICMSRACCESS_WRITE_INVALID);
                }
                uBaseMsr |= MSR_APICBASE_XAPIC_ENABLE_BIT;
                break;
            }

            case APICMODE_X2APIC:
            {
                uBaseMsr |= MSR_APICBASE_X2APIC_ENABLE_BIT;

                /*
                 * The APIC ID needs updating when entering x2APIC mode.
                 * Software written APIC ID in xAPIC mode isn't preseved.
                 * The APIC ID becomes read-only to software in x2APIC mode.
                 *
                 * See Intel spec. 10.12.5.1 "x2APIC States".
                 */
                PX2APICPAGE pX2ApicPage = VMCPU_TO_X2APICPAGE(pVCpu);
                ASMMemZero32(&pX2ApicPage->id, sizeof(pX2ApicPage->id));
                pX2ApicPage->id.u32ApicId = pVCpu->idCpu;

                /*
                 * LDR initialization occurs when entering x2APIC mode.
                 * See Intel spec. 10.12.10.2 "Deriving Logical x2APIC ID from the Local x2APIC ID".
                 */
                pX2ApicPage->ldr.u32LogicalApicId = ((pX2ApicPage->id.u32ApicId & UINT32_C(0xffff0)) << 16)
                                                  | (UINT32_C(1) << pX2ApicPage->id.u32ApicId & UINT32_C(0xf));
                break;
            }

            case APICMODE_INVALID:
            default:
            {
                Log(("APIC%u: Invalid state transition attempted\n", pVCpu->idCpu));
                return apicMsrAccessError(pVCpu, MSR_IA32_APICBASE, APICMSRACCESS_WRITE_INVALID);
            }
        }
    }

    ASMAtomicWriteU64(&pApicCpu->uApicBaseMsr, uBaseMsr);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnGetBaseMsrR3}
 */
VMMDECL(uint64_t) APICGetBaseMsr(PPDMDEVINS pDevIns, PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    PCAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    return pApicCpu->uApicBaseMsr;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnSetTprR3}
 */
VMMDECL(void) APICSetTpr(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint8_t u8Tpr)
{
    apicSetTpr(pVCpu, u8Tpr);
}


/**
 * @interface_method_impl{PDMAPICREG,pfnGetTprR3}
 */
VMMDECL(uint8_t) APICGetTpr(PPDMDEVINS pDevIns, PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);
    PCXAPICPAGE pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    return pXApicPage->tpr.u8Tpr;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnGetTimerFreqR3}
 */
VMMDECL(uint64_t) APICGetTimerFreq(PPDMDEVINS pDevIns)
{
    PVM      pVM      = PDMDevHlpGetVM(pDevIns);
    PVMCPU   pVCpu    = &pVM->aCpus[0];
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    uint64_t uTimer   = TMTimerGetFreq(pApicCpu->CTX_SUFF(pTimer));
    return uTimer;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnBusDeliverR3}
 * @remarks This is a private interface between the IOAPIC and the APIC.
 */
VMMDECL(int) APICBusDeliver(PPDMDEVINS pDevIns, uint8_t uDest, uint8_t uDestMode, uint8_t uDeliveryMode, uint8_t uVector,
                            uint8_t uPolarity, uint8_t uTriggerMode, uint32_t uTagSrc)
{
    NOREF(uPolarity);
    NOREF(uTagSrc);
    PVM pVM = PDMDevHlpGetVM(pDevIns);

    /*
     * The destination field (mask) in the IO APIC redirectable table entry is 8-bits.
     * Hence, the broadcast mask is 0xff.
     * See IO APIC spec. 3.2.4. "IOREDTBL[23:0] - I/O Redirectable Table Registers".
     */
    XAPICTRIGGERMODE  enmTriggerMode  = (XAPICTRIGGERMODE)uTriggerMode;
    XAPICDELIVERYMODE enmDeliveryMode = (XAPICDELIVERYMODE)uDeliveryMode;
    XAPICDESTMODE     enmDestMode     = (XAPICDESTMODE)uDestMode;
    uint32_t          fDestMask       = uDest;
    uint32_t          fBroadcastMask  = UINT32_C(0xff);

    VMCPUSET DestCpuSet;
    apicGetDestCpuSet(pVM, fDestMask, fBroadcastMask, enmDestMode, enmDeliveryMode, &DestCpuSet);
    VBOXSTRICTRC rcStrict = apicSendIntr(NULL /* pVCpu */, uVector, enmTriggerMode, enmDeliveryMode, &DestCpuSet, VINF_SUCCESS);
    return VBOXSTRICTRC_VAL(rcStrict);
}


/**
 * @interface_method_impl{PDMAPICREG,pfnLocalInterruptR3}
 * @remarks This is a private interface between the PIC and the APIC.
 */
VMMDECL(VBOXSTRICTRC) APICLocalInterrupt(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint8_t u8Pin, uint8_t u8Level, int rcRZ)
{
    NOREF(pDevIns);
    AssertReturn(u8Pin <= 1, VERR_INVALID_PARAMETER);
    AssertReturn(u8Level <= 1, VERR_INVALID_PARAMETER);

    LogFlow(("APIC%u: APICLocalInterrupt\n", pVCpu->idCpu));

    PCXAPICPAGE  pXApicPage = VMCPU_TO_CXAPICPAGE(pVCpu);
    VBOXSTRICTRC rcStrict   = VINF_SUCCESS;

    /* If the APIC is enabled, the interrupt is subject to LVT programming. */
    if (apicIsEnabled(pVCpu))
    {
        /* Pick the LVT entry corresponding to the interrupt pin. */
        static const uint16_t s_au16LvtOffsets[] =
        {
            XAPIC_OFF_LVT_LINT0,
            XAPIC_OFF_LVT_LINT1
        };
        Assert(u8Pin < RT_ELEMENTS(s_au16LvtOffsets));
        uint16_t const offLvt = s_au16LvtOffsets[u8Pin];
        uint32_t const uLvt   = apicReadRaw32(pXApicPage, offLvt);

        /* If software hasn't masked the interrupt in the LVT entry, proceed interrupt processing. */
        if (!XAPIC_LVT_IS_MASKED(uLvt))
        {
            XAPICDELIVERYMODE const enmDeliveryMode = XAPIC_LVT_GET_DELIVERY_MODE(uLvt);
            XAPICTRIGGERMODE        enmTriggerMode  = XAPIC_LVT_GET_TRIGGER_MODE(uLvt);

            switch (enmDeliveryMode)
            {
                case XAPICDELIVERYMODE_FIXED:
                {
                    /* Level-sensitive interrupts are not supported for LINT1. See Intel spec. 10.5.1 "Local Vector Table". */
                    if (offLvt == XAPIC_OFF_LVT_LINT1)
                        enmTriggerMode = XAPICTRIGGERMODE_EDGE;
                    /** @todo figure out what "If the local APIC is not used in conjunction with an I/O APIC and fixed
                              delivery mode is selected; the Pentium 4, Intel Xeon, and P6 family processors will always
                              use level-sensitive triggering, regardless if edge-sensitive triggering is selected."
                              means. */
                    /* fallthru */
                }
                case XAPICDELIVERYMODE_SMI:
                case XAPICDELIVERYMODE_NMI:
                case XAPICDELIVERYMODE_INIT:    /** @todo won't work in R0/RC because callers don't care about rcRZ. */
                case XAPICDELIVERYMODE_EXTINT:
                {
                    VMCPUSET DestCpuSet;
                    VMCPUSET_EMPTY(&DestCpuSet);
                    VMCPUSET_ADD(&DestCpuSet, pVCpu->idCpu);
                    uint8_t const uVector = XAPIC_LVT_GET_VECTOR(uLvt);

                    Log4(("APIC%u: APICLocalInterrupt: Sending interrupt. enmDeliveryMode=%u u8Pin=%u uVector=%u\n",
                          pVCpu->idCpu, enmDeliveryMode, u8Pin, uVector));

                    rcStrict = apicSendIntr(pVCpu, uVector, enmTriggerMode, enmDeliveryMode, &DestCpuSet, rcRZ);
                    break;
                }

                /* Reserved/unknown delivery modes: */
                case XAPICDELIVERYMODE_LOWEST_PRIO:
                case XAPICDELIVERYMODE_STARTUP:
                default:
                {
                    rcStrict = VERR_INTERNAL_ERROR_3;
                    AssertMsgFailed(("APIC%u: LocalInterrupt: Invalid delivery mode %#x on LINT%d\n", pVCpu->idCpu,
                                     enmDeliveryMode, u8Pin));
                    break;
                }
            }
        }
    }
    else
    {
        /* The APIC is disabled, pass it through the CPU. */
        LogFlow(("APIC%u: APICLocalInterrupt: APIC hardware-disabled, passing interrupt to CPU. u8Pin=%u u8Level=%u\n", u8Pin,
              u8Level));
        if (u8Level)
            APICSetInterruptFF(pVCpu, PDMAPICIRQ_EXTINT);
        else
            APICClearInterruptFF(pVCpu, PDMAPICIRQ_EXTINT);
    }

    return rcStrict;
}


/**
 * @interface_method_impl{PDMAPICREG,pfnHasPendingIrqR3}
 */
VMMDECL(bool) APICHasPendingIrq(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint8_t *pu8PendingIrq)
{
    return apicGetHighestPendingInterrupt(pVCpu, pu8PendingIrq);
}


/**
 * @interface_method_impl{PDMAPICREG,pfnGetInterruptR3}
 */
VMMDECL(int) APICGetInterrupt(PPDMDEVINS pDevIns, PVMCPU pVCpu, uint32_t *puTagSrc)
{
    VMCPU_ASSERT_EMT(pVCpu);

    LogFlow(("APIC%u: APICGetInterrupt\n", pVCpu->idCpu));

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    if (   apicIsEnabled(pVCpu)
        && pXApicPage->svr.u.fApicSoftwareEnable)
    {
        APICUpdatePendingInterrupts(pVCpu);
        int const irrv = apicGetLastSetBit(&pXApicPage->irr, -1);
        if (irrv >= 0)
        {
            Assert(irrv <= (int)UINT8_MAX);
            uint8_t const uVector = irrv;

            /** @todo this cannot possibly happen for anything other than ExtINT
             *        interrupts right? */
            uint8_t const uTpr = pXApicPage->tpr.u8Tpr;
            if (uTpr > 0 && uVector <= uTpr)
            {
                Log4(("APIC%u: APICGetInterrupt: Returns spurious vector %#x\n", pVCpu->idCpu,
                      pXApicPage->svr.u.u8SpuriousVector));
                return pXApicPage->svr.u.u8SpuriousVector;
            }

            uint8_t const uPpr = pXApicPage->ppr.u8Ppr;
            if (   !uPpr
                ||  XAPIC_PPR_GET_PP(uVector) > XAPIC_PPR_GET_PP(uPpr))
            {
                apicClearVectorInReg(&pXApicPage->irr, uVector);
                apicSetVectorInReg(&pXApicPage->isr, uVector);
                apicUpdatePpr(pVCpu);
                apicSignalNextPendingIntr(pVCpu);

                Log4(("APIC%u: APICGetInterrupt: Returns vector %#x\n", pVCpu->idCpu, uVector));
                return uVector;
            }
        }
    }

    return -1;
}


/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
VMMDECL(int) APICReadMmio(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(GCPhysAddr & 0xf));
    Assert(cb == 4);

    PAPICDEV pApicDev = PDMINS_2_DATA(pDevIns, PAPICDEV);
    PVMCPU   pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = (GCPhysAddr & 0xff0);
    uint32_t uValue   = 0;

#ifdef VBOX_WITH_STATISTICS
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    STAM_COUNTER_INC(&CTXSUFF(pApicCpu->StatMmioRead));
#endif

    Log4(("APIC%u: ApicReadMmio: offReg=%#RX16\n", pVCpu->idCpu, offReg));

    int rc = apicReadRegister(pApicDev, pVCpu, offReg, &uValue);
    *(uint32_t *)pv = uValue;
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
VMMDECL(int) APICWriteMmio(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(GCPhysAddr & 0xf));
    Assert(cb == 4);

    PAPICDEV pApicDev = PDMINS_2_DATA(pDevIns, PAPICDEV);
    PVMCPU   pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = (GCPhysAddr & 0xff0);
    uint32_t uValue   = *(uint32_t *)pv;

#ifdef VBOX_WITH_STATISTICS
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    STAM_COUNTER_INC(&CTXSUFF(pApicCpu->StatMmioWrite));
#endif

    LogRel(("APIC%u: APICWriteMmio: offReg=%#RX16\n", pVCpu->idCpu, offReg));

    int rc = VBOXSTRICTRC_VAL(apicWriteRegister(pApicDev, pVCpu, offReg, uValue));
    return rc;
}


/**
 * Sets the interrupt pending force-flag and pokes the EMT if required.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmType         The IRQ type.
 */
VMMDECL(void) APICSetInterruptFF(PVMCPU pVCpu, PDMAPICIRQ enmType)
{
    PVM      pVM      = CTX_SUFF(pVCpu->pVM);
    PAPICDEV pApicDev = VM_TO_APICDEV(pVM);
    CTX_SUFF(pApicDev->pApicHlp)->pfnSetInterruptFF(pApicDev->CTX_SUFF(pDevIns), enmType, pVCpu->idCpu);
}


/**
 * Clears the interrupt pending force-flag.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmType         The IRQ type.
 */
VMMDECL(void) APICClearInterruptFF(PVMCPU pVCpu, PDMAPICIRQ enmType)
{
    PVM      pVM      = CTX_SUFF(pVCpu->pVM);
    PAPICDEV pApicDev = VM_TO_APICDEV(pVM);
    CTX_SUFF(pApicDev->pApicHlp)->pfnClearInterruptFF(pApicDev->CTX_SUFF(pDevIns), enmType, pVCpu->idCpu);
}


/**
 * Posts an interrupt to a target APIC.
 *
 * This function handles interrupts received from the system bus or
 * interrupts generated locally from the LVT or via a self IPI.
 *
 * Don't use this function to try and deliver ExtINT style interrupts.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uVector             The vector of the interrupt to be posted.
 * @param   enmTriggerMode      The trigger mode of the interrupt.
 *
 * @thread  Any.
 */
VMM_INT_DECL(void) APICPostInterrupt(PVMCPU pVCpu, uint8_t uVector, XAPICTRIGGERMODE enmTriggerMode)
{
    Assert(pVCpu);

    PCAPIC   pApic    = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    /* Validate the vector. See Intel spec. 10.5.2 "Valid Interrupt Vectors". */
    if (RT_LIKELY(uVector > XAPIC_ILLEGAL_VECTOR_END))
    {
        Log4(("APIC%u: APICPostInterrupt: uVector=%#x\n", pVCpu->idCpu, uVector));
        if (enmTriggerMode == XAPICTRIGGERMODE_EDGE)
        {
            apicSetVectorInPib(CTX_SUFF(pApicCpu->pvApicPib), uVector);
            bool const fAlreadySet = apicSetNotificationBitInPib(CTX_SUFF(pApicCpu->pvApicPib));
            if (fAlreadySet)
                return;

            if (pApic->fPostedIntrsEnabled)
            { /** @todo posted-interrupt call to hardware */ }
            else
                APICSetInterruptFF(pVCpu, PDMAPICIRQ_HARDWARE);
        }
        else
        {
            /*
             * Level-triggered interrupts requires updating of the TMR and thus cannot be
             * delivered asynchronously.
             */
            apicSetVectorInPib(&pApicCpu->ApicPibLevel.aVectorBitmap[0], uVector);
            bool const fAlreadySet = apicSetNotificationBitInPib(&pApicCpu->ApicPibLevel.aVectorBitmap[0]);
            if (fAlreadySet)
                return;

            APICSetInterruptFF(pVCpu, PDMAPICIRQ_HARDWARE);
        }
    }
    else
        apicSetError(pVCpu, XAPIC_ESR_RECV_ILLEGAL_VECTOR);
}


/**
 * Starts the APIC timer.
 *
 * @param   pApicCpu        The APIC CPU state.
 * @param   uInitialCount   The timer's Initial-Count Register (ICR), must be >
 *                          0.
 * @thread  Any.
 */
VMM_INT_DECL(void) APICStartTimer(PAPICCPU pApicCpu, uint32_t uInitialCount)
{
    Assert(pApicCpu);
    Assert(TMTimerIsLockOwner(CTX_SUFF(pApicCpu->pTimer)));
    Assert(uInitialCount > 0);

    PCXAPICPAGE    pXApicPage   = APICCPU_TO_CXAPICPAGE(pApicCpu);
    uint8_t  const uTimerShift  = apicGetTimerShift(pXApicPage);
    uint64_t const cTicksToNext = (uint64_t)uInitialCount << uTimerShift;

    /*
     * The assumption here is that the timer doesn't tick during this call
     * and thus setting a relative time to fire next is accurate. The advantage
     * however is updating u64TimerInitial 'atomically' while setting the next
     * tick.
     */
    PTMTIMER pTimer = CTX_SUFF(pApicCpu->pTimer);
    TMTimerSetRelative(pTimer, cTicksToNext, &pApicCpu->u64TimerInitial);
    apicHintTimerFreq(pApicCpu, uInitialCount, uTimerShift);
}


/**
 * Stops the APIC timer.
 *
 * @param   pApicCpu        The APIC CPU state.
 * @thread  Any.
 */
VMM_INT_DECL(void) APICStopTimer(PAPICCPU pApicCpu)
{
    Assert(pApicCpu);
    Assert(TMTimerIsLockOwner(CTX_SUFF(pApicCpu->pTimer)));

    PTMTIMER pTimer = CTX_SUFF(pApicCpu->pTimer);
    TMTimerStop(pTimer);    /* This will reset the hint, no need to explicitly call TMTimerSetFrequencyHint(). */
    pApicCpu->uHintedTimerInitialCount = 0;
    pApicCpu->uHintedTimerShift = 0;
}


/**
 * Updates the CPUID bits necessary for the given APIC mode.
 *
 * @param   pVM         The cross context VM structure.
 * @param   enmMode     The APIC mode.
 */
VMM_INT_DECL(void) APICUpdateCpuIdForMode(PVM pVM, APICMODE enmMode)
{
    /* The CPUID bits being updated to reflect the current state is a bit vague. See @bugref{8245#c32}. */
    /** @todo This needs to be done on a per-VCPU basis! */
    switch (enmMode)
    {
        case APICMODE_DISABLED:
            CPUMClearGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_APIC);
            break;

        case APICMODE_XAPIC:
        case APICMODE_X2APIC:
            CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_APIC);
            CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_X2APIC);
            break;

        default:
            AssertMsgFailed(("Invalid APIC mode: %d\n", (int)enmMode));
            break;
    }
}


/**
 * Queues a pending interrupt as in-service.
 *
 * This function should only be needed without virtualized APIC
 * registers. With virtualized APIC registers, it's sufficient to keep
 * the interrupts pending in the IRR as the hardware takes care of
 * virtual interrupt delivery.
 *
 * @returns true if the interrupt was queued to in-service interrupts,
 *          false otherwise.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   u8PendingIntr       The pending interrupt to queue as
 *                              in-service.
 *
 * @remarks This assumes the caller has done the necessary checks and
 *          is ready to take actually service the interrupt (TPR,
 *          interrupt shadow etc.)
 */
VMMDECL(bool) APICQueueInterruptToService(PVMCPU pVCpu, uint8_t u8PendingIntr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PAPIC pApic = VM_TO_APIC(CTX_SUFF(pVCpu->pVM));
    Assert(!pApic->fVirtApicRegsEnabled);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    bool const fIsPending = apicTestVectorInReg(&pXApicPage->irr, u8PendingIntr);
    if (fIsPending)
    {
        apicClearVectorInReg(&pXApicPage->irr, u8PendingIntr);
        apicSetVectorInReg(&pXApicPage->isr, u8PendingIntr);
        apicUpdatePpr(pVCpu);
        return true;
    }
    return false;
}


/**
 * Dequeues a pending interrupt from in-service.
 *
 * This undoes APICQueueInterruptToService() for premature VM-exits before event
 * injection.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   u8PendingIntr       The pending interrupt to dequeue from
 *                              in-service.
 */
VMMDECL(void) APICDequeueInterruptFromService(PVMCPU pVCpu, uint8_t u8PendingIntr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PAPIC pApic = VM_TO_APIC(CTX_SUFF(pVCpu->pVM));
    Assert(!pApic->fVirtApicRegsEnabled);

    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    bool const fInService = apicTestVectorInReg(&pXApicPage->isr, u8PendingIntr);
    if (fInService)
    {
        apicClearVectorInReg(&pXApicPage->isr, u8PendingIntr);
        apicSetVectorInReg(&pXApicPage->irr, u8PendingIntr);
        apicUpdatePpr(pVCpu);
    }
}


/**
 * Updates pending interrupts from the pending interrupt bitmap to the IRR.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 */
VMMDECL(void) APICUpdatePendingInterrupts(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PAPICCPU   pApicCpu   = VMCPU_TO_APICCPU(pVCpu);
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
    for (;;)
    {
        bool const fAlreadySet = apicClearNotificationBitInPib(CTX_SUFF(pApicCpu->pvApicPib));
        if (!fAlreadySet)
            break;

        PAPICPIB pPib = (PAPICPIB)CTX_SUFF(pApicCpu->pvApicPib);
        for (size_t i = 0; i < RT_ELEMENTS(pPib->aVectorBitmap); i++)
        {
            uint32_t const uFragment = ASMAtomicXchgU32(&pPib->aVectorBitmap[i], 0);
            if (uFragment)
                apicOrVectorsToReg(&pXApicPage->irr, i, uFragment);
        }
    }
}


/**
 * Gets the highest priority pending interrupt.
 *
 * @returns true if any interrupt is pending, false otherwise.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pu8PendingIntr      Where to store the interrupt vector if the
 *                              interrupt is pending.
 */
VMMDECL(bool) APICGetHighestPendingInterrupt(PVMCPU pVCpu, uint8_t *pu8PendingIntr)
{
    VMCPU_ASSERT_EMT(pVCpu);
    return apicGetHighestPendingInterrupt(pVCpu, pu8PendingIntr);
}

