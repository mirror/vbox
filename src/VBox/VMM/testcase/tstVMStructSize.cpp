/* $Id$ */
/** @file
 * tstVMStructSize - testcase for check structure sizes/alignment
 *                   and to verify that HC and GC uses the same
 *                   representation of the structures.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cfgm.h>
#include <VBox/cpum.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/trpm.h>
#include <VBox/vmm.h>
#include <VBox/stam.h>
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include "CFGMInternal.h"
#include "CPUMInternal.h"
#include "MMInternal.h"
#include "PGMInternal.h"
#include "SELMInternal.h"
#include "TRPMInternal.h"
#include "TMInternal.h"
#include "IOMInternal.h"
#include "REMInternal.h"
#include "SSMInternal.h"
#include "HWACCMInternal.h"
#include "PATMInternal.h"
#ifdef VBOX_WITH_VMI
# include "PARAVInternal.h"
#endif
#include "VMMInternal.h"
#include "DBGFInternal.h"
#include "STAMInternal.h"
#include "VMInternal.h"
#include "CSAMInternal.h"
#include "EMInternal.h"
#include "REMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/x86.h>

#include "tstHelp.h"
#include <stdio.h>



int main()
{
    int rc = 0;
    printf("tstVMStructSize: TESTING\n");

    printf("struct VM: %d bytes\n", (int)sizeof(VM));

#define CHECK_PADDING_VM(member) \
    do \
    { \
        CHECK_PADDING(VM, member); \
        CHECK_MEMBER_ALIGNMENT(VM, member, 32); \
    } while (0)


#define CHECK_PADDING_VMCPU(member) \
    do \
    { \
        CHECK_PADDING(VMCPU, member); \
        CHECK_MEMBER_ALIGNMENT(VMCPU, member, 32); \
    } while (0)

#define CHECK_CPUMCTXCORE(member) \
    do { \
        if (RT_OFFSETOF(CPUMCTX, member) - RT_OFFSETOF(CPUMCTX, edi) != RT_OFFSETOF(CPUMCTXCORE, member)) \
        { \
            printf("CPUMCTX/CORE:: %s!\n", #member); \
            rc++; \
        } \
    } while (0)

#define PRINT_OFFSET(strct, member) \
    do \
    { \
        printf("%s::%s offset %#x (%d) sizeof %d\n",  #strct, #member, (int)RT_OFFSETOF(strct, member), (int)RT_OFFSETOF(strct, member), (int)RT_SIZEOFMEMB(strct, member)); \
    } while (0)



    CHECK_SIZE(uint128_t, 128/8);
    CHECK_SIZE(int128_t, 128/8);
    CHECK_SIZE(uint64_t, 64/8);
    CHECK_SIZE(int64_t, 64/8);
    CHECK_SIZE(uint32_t, 32/8);
    CHECK_SIZE(int32_t, 32/8);
    CHECK_SIZE(uint16_t, 16/8);
    CHECK_SIZE(int16_t, 16/8);
    CHECK_SIZE(uint8_t, 8/8);
    CHECK_SIZE(int8_t, 8/8);

    CHECK_SIZE(X86DESC, 8);
    CHECK_SIZE(X86DESC64, 16);
    CHECK_SIZE(VBOXIDTE, 8);
    CHECK_SIZE(VBOXIDTR, 10);
    CHECK_SIZE(VBOXGDTR, 10);
    CHECK_SIZE(VBOXTSS, 136);
    CHECK_SIZE(X86FXSTATE, 512);
    CHECK_SIZE(RTUUID, 16);
    CHECK_SIZE(X86PTE, 4);
    CHECK_SIZE(X86PD, PAGE_SIZE);
    CHECK_SIZE(X86PDE, 4);
    CHECK_SIZE(X86PT, PAGE_SIZE);
    CHECK_SIZE(X86PTEPAE, 8);
    CHECK_SIZE(X86PTPAE, PAGE_SIZE);
    CHECK_SIZE(X86PDEPAE, 8);
    CHECK_SIZE(X86PDPAE, PAGE_SIZE);
    CHECK_SIZE(X86PDPE, 8);
    CHECK_SIZE(X86PDPT, PAGE_SIZE);
    CHECK_SIZE(X86PML4E, 8);
    CHECK_SIZE(X86PML4, PAGE_SIZE);

    CHECK_PADDING_VM(cfgm);
    CHECK_PADDING_VM(cpum);
    CHECK_PADDING_VM(dbgf);
    CHECK_PADDING_VM(em);
    CHECK_PADDING_VM(iom);
    CHECK_PADDING_VM(mm);
    CHECK_PADDING_VM(pdm);
    CHECK_PADDING_VM(pgm);
    CHECK_PADDING_VM(selm);
    CHECK_PADDING_VM(tm);
    CHECK_PADDING_VM(trpm);
    CHECK_PADDING_VM(vm);
    CHECK_PADDING_VM(vmm);
    CHECK_PADDING_VM(ssm);
    CHECK_PADDING_VM(rem);
    CHECK_PADDING_VM(hwaccm);
    CHECK_PADDING_VM(patm);
    CHECK_PADDING_VM(csam);

    CHECK_PADDING_VMCPU(cpum);
    CHECK_PADDING_VMCPU(pgm);
    CHECK_PADDING_VMCPU(em);
    CHECK_PADDING_VMCPU(hwaccm);
    CHECK_PADDING_VMCPU(trpm);
    CHECK_PADDING_VMCPU(tm);
    CHECK_PADDING_VMCPU(vmm);

    CHECK_MEMBER_ALIGNMENT(VM, selm.s.Tss, 16);
    PRINT_OFFSET(VM, selm.s.Tss);
    PVM pVM;
    if ((RT_OFFSETOF(VM, selm.s.Tss) & PAGE_OFFSET_MASK) > PAGE_SIZE - sizeof(pVM->selm.s.Tss))
    {
        printf("SELM:Tss is crossing a page!\n");
        rc++;
    }
    PRINT_OFFSET(VM, selm.s.TssTrap08);
    if ((RT_OFFSETOF(VM, selm.s.TssTrap08) & PAGE_OFFSET_MASK) > PAGE_SIZE - sizeof(pVM->selm.s.TssTrap08))
    {
        printf("SELM:TssTrap08 is crossing a page!\n");
        rc++;
    }
    CHECK_MEMBER_ALIGNMENT(VM, trpm.s.aIdt, 16);
    CHECK_MEMBER_ALIGNMENT(VM, cpum, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0], 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[1], 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].cpum, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].hwaccm, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].pgm, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].em, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].tm, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].vmm, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].cpum.s.Host, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].cpum.s.Guest, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[1].cpum.s.Host, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[1].cpum.s.Guest, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[0].cpum.s.Hyper, 64);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus[1].cpum.s.Hyper, 64);
    CHECK_MEMBER_ALIGNMENT(VM, cpum.s.GuestEntry, 64);

    CHECK_MEMBER_ALIGNMENT(VMCPU, vmm.s.u64CallHostArg, 8);
    CHECK_MEMBER_ALIGNMENT(VMCPU, vmm.s.CallHostR0JmpBuf, 8);
    CHECK_MEMBER_ALIGNMENT(VM, vmm.s.u64LastYield, 8);
    CHECK_MEMBER_ALIGNMENT(VM, vmm.s.StatRunRC, 8);
    CHECK_MEMBER_ALIGNMENT(VM, StatTotalQemuToGC, 8);
    CHECK_MEMBER_ALIGNMENT(VM, rem.s.uPendingExcptCR2, 8);
    CHECK_MEMBER_ALIGNMENT(VM, rem.s.StatsInQEMU, 8);
    CHECK_MEMBER_ALIGNMENT(VM, rem.s.Env, 32);
    CHECK_MEMBER_ALIGNMENT(VM, aCpus, 64);

    /* vmcpu */
    CHECK_MEMBER_ALIGNMENT(VMCPU, cpum, 64);
    CHECK_MEMBER_ALIGNMENT(VMCPU, vmm, 32);
    CHECK_MEMBER_ALIGNMENT(VMCPU, pgm, 32);
    CHECK_MEMBER_ALIGNMENT(VMCPU, em, 32);
    CHECK_MEMBER_ALIGNMENT(VMCPU, hwaccm, 32);
    CHECK_MEMBER_ALIGNMENT(VMCPU, tm, 32);
    CHECK_SIZE_ALIGNMENT(VMCPU, 32);

    /* cpumctx */
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, fpu, 32);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, edi, 32);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, idtr, 4);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, SysEnter, 8);
    CHECK_CPUMCTXCORE(eax);
    CHECK_CPUMCTXCORE(ebx);
    CHECK_CPUMCTXCORE(ecx);
    CHECK_CPUMCTXCORE(edx);
    CHECK_CPUMCTXCORE(ebp);
    CHECK_CPUMCTXCORE(esp);
    CHECK_CPUMCTXCORE(edi);
    CHECK_CPUMCTXCORE(esi);
    CHECK_CPUMCTXCORE(eip);
    CHECK_CPUMCTXCORE(eflags);
    CHECK_CPUMCTXCORE(cs);
    CHECK_CPUMCTXCORE(ds);
    CHECK_CPUMCTXCORE(es);
    CHECK_CPUMCTXCORE(fs);
    CHECK_CPUMCTXCORE(gs);
    CHECK_CPUMCTXCORE(ss);
    CHECK_CPUMCTXCORE(r8);
    CHECK_CPUMCTXCORE(r9);
    CHECK_CPUMCTXCORE(r10);
    CHECK_CPUMCTXCORE(r11);
    CHECK_CPUMCTXCORE(r12);
    CHECK_CPUMCTXCORE(r13);
    CHECK_CPUMCTXCORE(r14);
    CHECK_CPUMCTXCORE(r15);
    CHECK_CPUMCTXCORE(esHid);
    CHECK_CPUMCTXCORE(csHid);
    CHECK_CPUMCTXCORE(ssHid);
    CHECK_CPUMCTXCORE(dsHid);
    CHECK_CPUMCTXCORE(fsHid);
    CHECK_CPUMCTXCORE(gsHid);

#if HC_ARCH_BITS == 32
    /* CPUMHOSTCTX - lss pair */
    if (RT_OFFSETOF(CPUMHOSTCTX, esp) + 4 != RT_OFFSETOF(CPUMHOSTCTX, ss))
    {
        printf("error: CPUMHOSTCTX lss has been split up!\n");
        rc++;
    }
#endif
    CHECK_SIZE_ALIGNMENT(CPUMCTX, 64);
    CHECK_SIZE_ALIGNMENT(CPUMHOSTCTX, 64);
    CHECK_SIZE_ALIGNMENT(CPUMCTXMSR, 64);

    /* pdm */
    CHECK_MEMBER_ALIGNMENT(PDMDEVINS, achInstanceData, 64);
    CHECK_PADDING(PDMDEVINS, Internal);
    CHECK_MEMBER_ALIGNMENT(PDMUSBINS, achInstanceData, 16);
    CHECK_PADDING(PDMUSBINS, Internal);
    CHECK_MEMBER_ALIGNMENT(PDMDRVINS, achInstanceData, 16);
    CHECK_PADDING(PDMDRVINS, Internal);
    CHECK_PADDING2(PDMCRITSECT);
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, idx, sizeof(uint16_t));
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, pvPageR3, sizeof(RTHCPTR));
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, GCPhys, sizeof(RTGCPHYS));
    CHECK_SIZE(PGMPAGE, 16);
    CHECK_MEMBER_ALIGNMENT(PGMRAMRANGE, aPages, 16);
    CHECK_MEMBER_ALIGNMENT(PGMMMIO2RANGE, RamRange, 16);

    /* misc */
    CHECK_MEMBER_ALIGNMENT(REM, aGCPtrInvalidatedPages, 8);
    CHECK_PADDING3(EMCPU, u.FatalLongJump, u.achPaddingFatalLongJump);
    CHECK_PADDING3(REMHANDLERNOTIFICATION, u.PhysicalRegister, u.padding);
    CHECK_PADDING3(REMHANDLERNOTIFICATION, u.PhysicalDeregister, u.padding);
    CHECK_PADDING3(REMHANDLERNOTIFICATION, u.PhysicalModify, u.padding);
    CHECK_SIZE_ALIGNMENT(VMMR0JMPBUF, 8);
    CHECK_SIZE_ALIGNMENT(PATCHINFO, 8);
#if 0
    PRINT_OFFSET(VM, fForcedActions);
    PRINT_OFFSET(VM, StatQemuToGC);
    PRINT_OFFSET(VM, StatGCToQemu);
#endif


    /* hwaccm - 32-bit gcc won't align uint64_t naturally, so check. */
    CHECK_MEMBER_ALIGNMENT(HWACCM, u64RegisterMask, 8);
    CHECK_MEMBER_ALIGNMENT(HWACCM, vmx.hostCR4, 8);
    CHECK_MEMBER_ALIGNMENT(HWACCM, vmx.msr.feature_ctrl, 8);
    CHECK_MEMBER_ALIGNMENT(HWACCMCPU, StatEntry, 8);
    CHECK_MEMBER_ALIGNMENT(HWACCMCPU, vmx.proc_ctls, 8);
    CHECK_MEMBER_ALIGNMENT(HWACCMCPU, Event.intInfo, 8);

    /* Make sure the set is large enough and has the correct size. */
    CHECK_SIZE(VMCPUSET, 32);
    if (sizeof(VMCPUSET) * 8 < VMM_MAX_CPU_COUNT)
    {
        printf("error: VMCPUSET is too small for VMM_MAX_CPU_COUNT=%u!\n", VMM_MAX_CPU_COUNT);
        rc++;
    }

    /*
     * Compare HC and GC.
     */
    printf("tstVMStructSize: Comparing HC and GC...\n");
#include "tstVMStructGC.h"

    /*
     * Report result.
     */
    if (rc)
        printf("tstVMStructSize: FAILURE - %d errors\n", rc);
    else
        printf("tstVMStructSize: SUCCESS\n");
    return rc;
}

