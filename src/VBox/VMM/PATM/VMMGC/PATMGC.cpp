/** @file
 *
 * PATM - Dynamic Guest OS Patching Manager - Guest Context
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
#define LOG_GROUP LOG_GROUP_PATM
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/patm.h>
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include "PATMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/err.h>
#include <VBox/em.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/**
 * #PF Virtual Handler callback for Guest access a page monitored by PATM
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
PATMGCDECL(int) PATMGCMonitorPage(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    pVM->patm.s.pvFaultMonitor = pvFault;
    return VINF_PATM_CHECK_PATCH_PAGE;
}


/**
 * Checks if the write is located on a page with was patched before.
 * (if so, then we are not allowed to turn on r/w)
 *
 * @returns VBox status
 * @param   pVM         The VM to operate on.
 * @param   pRegFrame   CPU context
 * @param   GCPtr       GC pointer to write address
 * @param   cbWrite     Nr of bytes to write
 *
 */
PATMGCDECL(int) PATMGCHandleWriteToPatchPage(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR GCPtr, uint32_t cbWrite)
{
    bool                 ret = false;
    RTGCUINTPTR          pWritePageStart, pWritePageEnd;
    PPATMPATCHPAGE       pPatchPage;

    /* Quick boundary check */
    if (    GCPtr < pVM->patm.s.pPatchedInstrGCLowest
        ||  GCPtr > pVM->patm.s.pPatchedInstrGCHighest
       )
       return VERR_PATCH_NOT_FOUND;

    STAM_PROFILE_ADV_START(&pVM->patm.s.StatPatchWriteDetect, a);

    pWritePageStart = (RTGCUINTPTR)GCPtr & PAGE_BASE_GC_MASK;
    pWritePageEnd   = ((RTGCUINTPTR)GCPtr + cbWrite - 1) & PAGE_BASE_GC_MASK;

    pPatchPage = (PPATMPATCHPAGE)RTAvloGCPtrGet(CTXSUFF(&pVM->patm.s.PatchLookupTree)->PatchTreeByPage, (RTGCPTR)pWritePageStart);
    if (    !pPatchPage
        &&  pWritePageStart != pWritePageEnd
       )
    {
        pPatchPage = (PPATMPATCHPAGE)RTAvloGCPtrGet(CTXSUFF(&pVM->patm.s.PatchLookupTree)->PatchTreeByPage, (RTGCPTR)pWritePageEnd);
    }

#ifdef LOG_ENABLED
    if (pPatchPage)
        Log(("PATMIsWriteToPatchPage: Found page %VGv for write to %VGv %d bytes\n", pPatchPage->Core.Key, GCPtr, cbWrite));
#endif

    if (pPatchPage)
    {
        if (    pPatchPage->pLowestAddrGC  <= (RTGCPTR)((RTGCUINTPTR)GCPtr + cbWrite)
            ||  pPatchPage->pHighestAddrGC > GCPtr)
        {
            /* This part of the page was not patched; try to emulate the instruction. */
            uint32_t cb;

            LogFlow(("PATMHandleWriteToPatchPage: Interpret %VGv accessing %VGv\n", pRegFrame->eip, GCPtr));
            int rc = EMInterpretInstruction(pVM, pRegFrame, GCPtr, &cb);
            if (rc == VINF_SUCCESS)
            {
                STAM_COUNTER_INC(&pVM->patm.s.StatPatchWriteInterpreted);
                STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
                return VINF_SUCCESS;
            }
            STAM_COUNTER_INC(&pVM->patm.s.StatPatchWriteInterpretedFailed);
        }
        STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
        return VINF_EM_RAW_EMULATE_INSTR;
    }

    STAM_PROFILE_ADV_STOP(&pVM->patm.s.StatPatchWriteDetect, a);
    return VERR_PATCH_NOT_FOUND;
}
