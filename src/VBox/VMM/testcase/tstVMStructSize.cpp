/* $Id$ */
/** @file
 * tstVMStructSize - testcase for check structure sizes/alignment
 *                   and to verify that HC and GC uses the same
 *                   representation of the structures.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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

    CHECK_SIZE(VBOXDESC, 8);
    CHECK_SIZE(VBOXIDTE, 8);
    CHECK_SIZE(VBOXIDTR, 6);
    CHECK_SIZE(VBOXGDTR, 6);
    CHECK_SIZE(VBOXPTE, 4);
    CHECK_SIZE(VBOXPDE, 4);
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
    CHECK_SIZE(X86PDPTR, PAGE_SIZE);
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
    CHECK_PADDING_VM(stam);
    CHECK_PADDING_VM(tm);
    CHECK_PADDING_VM(trpm);
    CHECK_PADDING_VM(vm);
    CHECK_PADDING_VM(vmm);
    CHECK_PADDING_VM(ssm);
    CHECK_PADDING_VM(rem);
    CHECK_PADDING_VM(hwaccm);
    CHECK_PADDING_VM(patm);
    CHECK_PADDING_VM(csam);
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
    CHECK_MEMBER_ALIGNMENT(VM, cpum.s.Host, 32);
    CHECK_MEMBER_ALIGNMENT(VM, cpum.s.Guest, 32);
    CHECK_MEMBER_ALIGNMENT(VM, cpum.s.Hyper, 32);
    CHECK_MEMBER_ALIGNMENT(VM, vmm.s.CritSectVMLock, 8);
    CHECK_MEMBER_ALIGNMENT(VM, vmm.s.CallHostR0JmpBuf, 8);
    CHECK_MEMBER_ALIGNMENT(VM, vmm.s.StatRunGC, 8);
    CHECK_MEMBER_ALIGNMENT(VM, StatTotalQemuToGC, 8);
    CHECK_MEMBER_ALIGNMENT(VM, rem.s.StatsInQEMU, 8);
    CHECK_MEMBER_ALIGNMENT(VM, rem.s.Env, 32);

    /* cpumctx */
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, fpu, 32);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, edi, 32);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, idtr, 4);
    CHECK_MEMBER_ALIGNMENT(CPUMCTX, SysEnter, 8);
    CHECK_SIZE_ALIGNMENT(CPUMCTX, 32);
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

#if HC_ARCH_BITS == 32
    /* CPUMHOSTCTX - lss pair */
    if (RT_OFFSETOF(CPUMHOSTCTX, esp) + 4 != RT_OFFSETOF(CPUMHOSTCTX, ss))
    {
        printf("error: CPUMHOSTCTX lss has been split up!\n");
        rc++;
    }
#endif

    /* pdm */
    CHECK_MEMBER_ALIGNMENT(PDMDEVINS, achInstanceData, 16);
    CHECK_PADDING(PDMDEVINS, Internal);
    CHECK_MEMBER_ALIGNMENT(PDMUSBINS, achInstanceData, 16);
    CHECK_PADDING(PDMUSBINS, Internal);
    CHECK_MEMBER_ALIGNMENT(PDMDRVINS, achInstanceData, 16);
    CHECK_PADDING(PDMDRVINS, Internal);
    CHECK_PADDING2(PDMCRITSECT);
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, idx, sizeof(uint16_t));
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, pvPageHC, sizeof(RTHCPTR));
    CHECK_MEMBER_ALIGNMENT(PGMPOOLPAGE, GCPhys, sizeof(RTGCPHYS));

    /* misc */
    CHECK_MEMBER_ALIGNMENT(REM, aGCPtrInvalidatedPages, 8);
    CHECK_PADDING3(EM, u.FatalLongJump, u.achPaddingFatalLongJump);
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

