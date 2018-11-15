/* $Id$ */
/** @file
 * CPUM - SVM.
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/log.h>
#include <VBox/vmm/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vmm/iem.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/vm.h>


/**
 * Gets the permission bits for the specified MSR in the specified MSR bitmap.
 *
 * @returns VBox status code.
 * @param   pvMsrBitmap     Pointer to the MSR bitmap.
 * @param   idMsr           The MSR.
 * @param   penmRead        Where to store the read permissions. Optional, can be
 *                          NULL.
 * @param   penmWrite       Where to store the write permissions. Optional, can be
 *                          NULL.
 */
VMM_INT_DECL(int) CPUMVmxGetMsrPermission(void const *pvMsrBitmap, uint32_t idMsr, PVMXMSREXITREAD penmRead,
                                          PVMXMSREXITWRITE penmWrite)
{
    AssertPtrReturn(pvMsrBitmap, VERR_INVALID_PARAMETER);

    int32_t iBit;
    uint8_t const *pbMsrBitmap = (uint8_t *)pvMsrBitmap;

    /*
     * MSR Layout:
     *   Byte index            MSR range            Interpreted as
     * 0x000 - 0x3ff    0x00000000 - 0x00001fff    Low MSR read bits.
     * 0x400 - 0x7ff    0xc0000000 - 0xc0001fff    High MSR read bits.
     * 0x800 - 0xbff    0x00000000 - 0x00001fff    Low MSR write bits.
     * 0xc00 - 0xfff    0xc0000000 - 0xc0001fff    High MSR write bits.
     *
     * A bit corresponding to an MSR within the above range causes a VM-exit
     * if the bit is 1 on executions of RDMSR/WRMSR.
     *
     * If an MSR falls out of the MSR range, it always cause a VM-exit.
     *
     * See Intel spec. 24.6.9 "MSR-Bitmap Address".
     */
    if (idMsr <= 0x00001fff)
        iBit = idMsr;
    else if (   idMsr >= 0xc0000000
             && idMsr <= 0xc0001fff)
    {
        iBit = (idMsr - 0xc0000000);
        pbMsrBitmap += 0x400;
    }
    else
    {
        if (penmRead)
            *penmRead = VMXMSREXIT_INTERCEPT_READ;
        if (penmWrite)
            *penmWrite = VMXMSREXIT_INTERCEPT_WRITE;
        Log(("CPUMVmxGetMsrPermission: Warning! Out of range MSR %#RX32\n", idMsr));
        return VINF_SUCCESS;
    }

    /* Validate the MSR bit position. */
    Assert(iBit <= 0x1fff);

    /* Get the MSR read permissions. */
    if (penmRead)
    {
        if (ASMBitTest(pbMsrBitmap, iBit))
            *penmRead = VMXMSREXIT_INTERCEPT_READ;
        else
            *penmRead = VMXMSREXIT_PASSTHRU_READ;
    }

    /* Get the MSR write permissions. */
    if (penmWrite)
    {
        if (ASMBitTest(pbMsrBitmap + 0x800, iBit))
            *penmWrite = VMXMSREXIT_INTERCEPT_WRITE;
        else
            *penmWrite = VMXMSREXIT_PASSTHRU_WRITE;
    }

    return VINF_SUCCESS;
}


/**
 * Gets the permission bits for the specified I/O port from the given I/O bitmaps.
 *
 * @returns @c true if the I/O port access must cause a VM-exit, @c false otherwise.
 * @param   pvIoBitmapA     Pointer to I/O bitmap A.
 * @param   pvIoBitmapB     Pointer to I/O bitmap B.
 * @param   uPort           The I/O port being accessed.
 * @param   cbAccess        The size of the I/O access in bytes (1, 2 or 4 bytes).
 */
VMM_INT_DECL(bool) CPUMVmxGetIoBitmapPermission(void const *pvIoBitmapA, void const *pvIoBitmapB, uint16_t uPort,
                                                uint8_t cbAccess)
{
    Assert(cbAccess == 1 || cbAccess == 2 || cbAccess == 4);

    /*
     * If the I/O port access wraps around the 16-bit port I/O space,
     * we must cause a VM-exit.
     *
     * See Intel spec. 25.1.3 "Instructions That Cause VM Exits Conditionally".
     */
    /** @todo r=ramshankar: Reading 1, 2, 4 bytes at ports 0xffff, 0xfffe and 0xfffc
     *        respectively are valid and do not constitute a wrap around from what I
     *        understand. Verify this later. */
    uint32_t const uPortLast = uPort + cbAccess;
    if (uPortLast > 0x10000)
        return true;

    /* Read the appropriate bit from the corresponding IO bitmap. */
    void const *pvIoBitmap = uPort < 0x8000 ? pvIoBitmapA : pvIoBitmapB;
    return ASMBitTest(pvIoBitmap, uPort);
}

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/**
 * @callback_method_impl{FNPGMPHYSHANDLER, VMX APIC-access page accesses}
 *
 * @remarks The @a pvUser argument is currently unused.
 */
PGM_ALL_CB2_DECL(VBOXSTRICTRC) cpumVmxApicAccessPageHandler(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysFault, void *pvPhys,
                                                            void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType,
                                                            PGMACCESSORIGIN enmOrigin, void *pvUser)
{
    RT_NOREF4(pVM, pvPhys, enmOrigin, pvUser);

    uint16_t const offAccess = (GCPhysFault & PAGE_OFFSET_MASK);
    bool const fWrite = RT_BOOL(enmAccessType == PGMACCESSTYPE_WRITE);
    VBOXSTRICTRC rcStrict = IEMExecVmxVirtApicAccessMem(pVCpu, offAccess, cbBuf, pvBuf, fWrite);
    if (rcStrict == VINF_VMX_MODIFIES_BEHAVIOR)
        rcStrict = VINF_SUCCESS;
    return rcStrict;
}
#endif


/**
 * Registers the PGM physical page handelr for teh VMX APIC-access page.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   GCPhysApicAccess    The guest-physical address of the APIC-access page.
 */
VMM_INT_DECL(VBOXSTRICTRC) CPUMVmxApicAccessPageRegister(PVMCPU pVCpu, RTGCPHYS GCPhysApicAccess)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    int rc = PGMHandlerPhysicalRegister(pVM, GCPhysApicAccess, GCPhysApicAccess, pVM->cpum.s.hVmxApicAccessPage,
                                        NIL_RTR3PTR /* pvUserR3 */, NIL_RTR0PTR /* pvUserR0 */,  NIL_RTRCPTR /* pvUserRC */,
                                        NULL /* pszDesc */);
    return rc;
}


/**
 * Registers the PGM physical page handelr for teh VMX APIC-access page.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   GCPhysApicAccess    The guest-physical address of the APIC-access page.
 */
VMM_INT_DECL(VBOXSTRICTRC) CPUMVmxApicAccessPageDeregister(PVMCPU pVCpu, RTGCPHYS GCPhysApicAccess)
{
    /** @todo NSTVMX: If there's anything else to do while APIC-access page is
     *        de-registered, do it here. */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (PGMHandlerPhysicalIsRegistered(pVM, GCPhysApicAccess))
        return PGMHandlerPhysicalDeregister(pVM, GCPhysApicAccess);
    return VINF_SUCCESS;
}

