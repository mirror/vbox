/** @file
 *
 * Testcase helpers.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "tstHelp.h"
#include <stdio.h>

void tstDumpCtx(PCPUMCTX pCtx, const char *pszComment)
{
    printf("*** CPU State dump *** %s:\n"
           "cs:eip=%04x:%08x ss:esp=%04x:%08x   tr=%04x\n"
           "  gdtr=%04x:%08x   idtr=%04x:%08x ldtr=%04x\n"
           "ds=%04x es=%04x fs=%04x gs=%04x\n"
           "cr0=%08x cr2=%08x cr3=%08x cr4=%08x\n"
           "eax=%08x ebx=%08x ecx=%08x edx=%08x\n"
           "esi=%08x edi=%08x ebp=%08x efl=%08x\n",
            pszComment,
            pCtx->cs, pCtx->eip, pCtx->ss, pCtx->esp, pCtx->tr,
            pCtx->gdtr.cbGdt, pCtx->gdtr.pGdt, pCtx->idtr.cbIdt, pCtx->idtr.pIdt, pCtx->ldtr,
            pCtx->ds, pCtx->es, pCtx->fs, pCtx->gs,
            pCtx->cr0, pCtx->cr2, pCtx->cr3, pCtx->cr4,
            pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx,
            pCtx->esi, pCtx->edi, pCtx->ebp, pCtx->eflags.u32);
}

