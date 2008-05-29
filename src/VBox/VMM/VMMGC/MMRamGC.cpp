/* $Id$ */
/** @file
 * MMRamGC - Guest Context Ram access Routines, pair for MMRamGCA.asm.
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
#define LOG_GROUP LOG_GROUP_MM
#include <VBox/mm.h>
#include <VBox/cpum.h>
#include <VBox/trpm.h>
#include <VBox/em.h>
#include "MMInternal.h"
#include <VBox/vm.h>
#include <VBox/pgm.h>

#include <iprt/assert.h>
#include <VBox/param.h>
#include <VBox/err.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) mmGCRamTrap0eHandler(PVM pVM, PCPUMCTXCORE pRegFrame);

DECLASM(void) MMGCRamReadNoTrapHandler_EndProc(void);
DECLASM(void) MMGCRamWriteNoTrapHandler_EndProc(void);
DECLASM(void) EMGCEmulateLockCmpXchg_EndProc(void);
DECLASM(void) EMGCEmulateLockCmpXchg_Error(void);
DECLASM(void) EMGCEmulateCmpXchg_EndProc(void);
DECLASM(void) EMGCEmulateCmpXchg_Error(void);
DECLASM(void) EMGCEmulateLockCmpXchg8b_EndProc(void);
DECLASM(void) EMGCEmulateLockCmpXchg8b_Error(void);
DECLASM(void) EMGCEmulateCmpXchg8b_EndProc(void);
DECLASM(void) EMGCEmulateCmpXchg8b_Error(void);
DECLASM(void) EMGCEmulateLockXAdd_EndProc(void);
DECLASM(void) EMGCEmulateLockXAdd_Error(void);
DECLASM(void) EMGCEmulateXAdd_EndProc(void);
DECLASM(void) EMGCEmulateXAdd_Error(void);
DECLASM(void) EMEmulateLockOr_EndProc(void);
DECLASM(void) EMEmulateLockOr_Error(void);
DECLASM(void) EMEmulateLockBtr_EndProc(void);
DECLASM(void) EMEmulateLockBtr_Error(void);
DECLASM(void) MMGCRamRead_Error(void);
DECLASM(void) MMGCRamWrite_Error(void);


/**
 * Install MMGCRam Hypervisor page fault handler for normal working
 * of MMGCRamRead and MMGCRamWrite calls.
 * This handler will be automatically removed at page fault.
 * In other case it must be removed by MMGCRamDeregisterTrapHandler call.
 *
 * @param   pVM         VM handle.
 */
MMGCDECL(void) MMGCRamRegisterTrapHandler(PVM pVM)
{
    TRPMGCSetTempHandler(pVM, 0xe, mmGCRamTrap0eHandler);
}

/**
 * Remove MMGCRam Hypervisor page fault handler.
 * See description of MMGCRamRegisterTrapHandler call.
 *
 * @param   pVM         VM handle.
 */
MMGCDECL(void) MMGCRamDeregisterTrapHandler(PVM pVM)
{
    TRPMGCSetTempHandler(pVM, 0xe, NULL);
}


/**
 * Read data in guest context with #PF control.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pDst        Where to store the readed data.
 * @param   pSrc        Pointer to the data to read.
 * @param   cb          Size of data to read, only 1/2/4/8 is valid.
 */
MMGCDECL(int) MMGCRamRead(PVM pVM, void *pDst, void *pSrc, size_t cb)
{
    int rc;

    TRPMSaveTrap(pVM);  /* save the current trap info, because it will get trashed if our access failed. */

    MMGCRamRegisterTrapHandler(pVM);
    rc = MMGCRamReadNoTrapHandler(pDst, pSrc, cb);
    MMGCRamDeregisterTrapHandler(pVM);
    if (VBOX_FAILURE(rc))
        TRPMRestoreTrap(pVM);

    return rc;
}

/**
 * Write data in guest context with #PF control.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pDst        Where to write the data.
 * @param   pSrc        Pointer to the data to write.
 * @param   cb          Size of data to write, only 1/2/4 is valid.
 */
MMGCDECL(int) MMGCRamWrite(PVM pVM, void *pDst, void *pSrc, size_t cb)
{
    int rc;

    TRPMSaveTrap(pVM);  /* save the current trap info, because it will get trashed if our access failed. */

    MMGCRamRegisterTrapHandler(pVM);
    rc = MMGCRamWriteNoTrapHandler(pDst, pSrc, cb);
    MMGCRamDeregisterTrapHandler(pVM);
    if (VBOX_FAILURE(rc))
        TRPMRestoreTrap(pVM);

    /*
     * And mark the relevant guest page as accessed and dirty.
     */
    PGMGstModifyPage(pVM, (RTGCPTR)pDst, cb, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));

    return rc;
}


/**
 * \#PF Handler for servicing traps inside MMGCRamReadNoTrapHandler and MMGCRamWriteNoTrapHandler functions.
 *
 * @internal
 */
DECLCALLBACK(int) mmGCRamTrap0eHandler(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    /*
     * Page fault inside MMGCRamRead()? Resume at *_Error.
     */
    if (    (uintptr_t)&MMGCRamReadNoTrapHandler < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&MMGCRamReadNoTrapHandler_EndProc)
    {
        /* Must be a read violation. */
        AssertReturn(!(TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW), VERR_INTERNAL_ERROR); 
        pRegFrame->eip = (uintptr_t)&MMGCRamRead_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside MMGCRamWrite()? Resume at _Error.
     */
    if (    (uintptr_t)&MMGCRamWriteNoTrapHandler < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&MMGCRamWriteNoTrapHandler_EndProc)
    {
        /* Must be a write violation. */
        AssertReturn(TRPMGetErrorCode(pVM) & X86_TRAP_PF_RW, VERR_INTERNAL_ERROR); 
        pRegFrame->eip = (uintptr_t)&MMGCRamWrite_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateLockCmpXchg()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateLockCmpXchg < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateLockCmpXchg_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateLockCmpXchg_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateCmpXchg()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateCmpXchg < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateCmpXchg_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateCmpXchg_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateLockCmpXchg8b()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateLockCmpXchg8b < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateLockCmpXchg8b_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateLockCmpXchg8b_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateCmpXchg8b()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateCmpXchg8b < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateCmpXchg8b_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateCmpXchg8b_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateLockXAdd()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateLockXAdd < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateLockXAdd_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateLockXAdd_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMGCEmulateXAdd()? Resume at _Error.
     */
    if (    (uintptr_t)&EMGCEmulateXAdd < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMGCEmulateXAdd_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMGCEmulateXAdd_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMEmulateLockOr()? Resume at *_Error.
     */
    if (    (uintptr_t)&EMEmulateLockOr < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMEmulateLockOr_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMEmulateLockOr_Error;
        return VINF_SUCCESS;
    }

    /*
     * Page fault inside EMEmulateLockBtr()? Resume at *_Error.
     */
    if (    (uintptr_t)&EMEmulateLockBtr < (uintptr_t)pRegFrame->eip
        &&  (uintptr_t)pRegFrame->eip < (uintptr_t)&EMEmulateLockBtr_EndProc)
    {
        pRegFrame->eip = (uintptr_t)&EMEmulateLockBtr_Error;
        return VINF_SUCCESS;
    }

    /* 
     * #PF is not handled - cause guru meditation. 
     */
    return VERR_INTERNAL_ERROR;
}


