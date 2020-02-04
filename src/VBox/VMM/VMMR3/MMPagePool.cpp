/* $Id$ */
/** @file
 * MM - Memory Manager - Page Pool (what's left of it).
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MM_POOL
#include <VBox/vmm/mm.h>
#include "MMInternal.h"
#include <VBox/vmm/vm.h>
#include <iprt/errcore.h>
#include <VBox/log.h>


/**
 * Gets the HC pointer to the dummy page.
 *
 * The dummy page is used as a place holder to prevent potential bugs
 * from doing really bad things to the system.
 *
 * @returns Pointer to the dummy page.
 * @param   pVM         The cross context VM structure.
 * @thread  The Emulation Thread.
 */
VMMR3DECL(void *) MMR3PageDummyHCPtr(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    if (!pVM->mm.s.pvDummyPage)
    {
        int rc = MMHyperAlloc(pVM, PAGE_SIZE, PAGE_SIZE, MM_TAG_PGM, &pVM->mm.s.pvDummyPage);
        AssertRelease(RT_SUCCESS(rc));
        AssertRelease(pVM->mm.s.pvDummyPage);
        pVM->mm.s.HCPhysDummyPage = MMR3HyperHCVirt2HCPhys(pVM, pVM->mm.s.pvDummyPage);
        AssertRelease(!(pVM->mm.s.HCPhysDummyPage & ~X86_PTE_PAE_PG_MASK));
    }
    return pVM->mm.s.pvDummyPage;
}


/**
 * Gets the HC Phys to the dummy page.
 *
 * The dummy page is used as a place holder to prevent potential bugs
 * from doing really bad things to the system.
 *
 * @returns Pointer to the dummy page.
 * @param   pVM         The cross context VM structure.
 * @thread  The Emulation Thread.
 */
VMMR3DECL(RTHCPHYS) MMR3PageDummyHCPhys(PVM pVM)
{
    VM_ASSERT_EMT(pVM);
    if (!pVM->mm.s.pvDummyPage)
        MMR3PageDummyHCPtr(pVM);
    return pVM->mm.s.HCPhysDummyPage;
}

