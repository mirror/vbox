/** @file
 *
 * VM - Virtual Machine All Contexts.
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
#define LOG_GROUP LOG_GROUP_VM
#include "VMInternal.h"
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <iprt/assert.h>
#include <iprt/string.h>


/**
 * Sets the error message.
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @codeend
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   ...             Error message arguments.
 * @thread  Any
 */
VMDECL(int) VMSetError(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pVM, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc == rc2); NOREF(rc2);
    va_end(args);
    return rc;
}


/**
 * Sets the error message.
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @codeend
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 * @thread  Any
 */
VMDECL(int) VMSetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args)
{
#ifdef IN_RING3
    /*
     * Switch to EMT.
     */
    PVMREQ pReq;
    VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3SetErrorV, 7,  /* ASSUMES 3 source pos args! */
                pVM, rc, RT_SRC_POS_ARGS, pszFormat, &args);
    VMR3ReqFree(pReq);

#else
    /*
     * We're already on the EMT thread and can safely create a VMERROR chunk.
     */
    vmSetErrorCopy(pVM, rc, RT_SRC_POS_ARGS, pszFormat, args);

# ifdef IN_GC
    VMMGCCallHost(pVM, VMMCALLHOST_VM_SET_ERROR, 0);
# elif defined(IN_RING0)
    VMMR0CallHost(pVM, VMMCALLHOST_VM_SET_ERROR, 0);
# else
# endif
#endif
    return rc;
}


/**
 * Copies the error to a VMERROR structure.
 *
 * This is mainly intended for Ring-0 and GC where the error must be copied to
 * memory accessible from ring-3. But it's just possible that we might add
 * APIs for retrieving the VMERROR copy later.
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   rc              VBox status code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 * @thread  EMT
 */
void vmSetErrorCopy(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args)
{
#if 0 /// @todo implement Ring-0 and GC VMSetError
    /*
     * Create the untranslated message copy.
     */
    /* free any old message. */
    MMHyperFree(pVM, MMHyperR32Ctx(pVM, pVM->vm.s.pError));
    pVM->vm.s.pError = NULL;

    /* calc reasonable start size. */
    size_t cchFile = pszFile ? strlen(pszFile) : 0;
    size_t cchFunction = pszFunction ? strlen(pszFunction) : 0;
    size_t cchFormat = strlen(pszFormat);
    size_t cb = sizeof(VMERROR)
              + cchFile + 1
              + cchFunction + 1
              + cchFormat + 32;

    /* allocate it */
    void *pv;
    int rc2 = MMHyperAlloc(pVM, cb, 0, MM_TAG_VM, &pv);
    if (VBOX_SUCCESS(rc2))
    {
        /* initialize it. */
        PVMERROR pErr = (PVMERROR)pv;
        pErr->cbAllocated = cb;
        pErr->iLine = iLine;
        pErr->off = sizeof(VMERROR);
        pErr->offFile = pErr->offFunction = 0;

        if (cchFile)
        {
            pErr->offFile = pErr->off;
            memcpy((uint8_t *)pErr + pErr->off, pszFile, cchFile + 1);
            pErr->off += cchFile + 1;
        }

        if (cchFunction)
        {
            pErr->offFunction = pErr->off;
            memcpy((uint8_t *)pErr + pErr->off, pszFunction, cchFunction + 1);
            pErr->off += cchFunction + 1;
        }

        pErr->offMessage = pErr->off;

        /* format the message (pErr might be reallocated) */
        VMSETERRORFMTARGS Args;
        Args.pVM = pVM;
        Args.pErr = pErr;

        va_list va2;
        va_copy(va2, args);
        RTStrFormatV(vmSetErrorFmtOut, &pErr, NULL, NULL, &pszFormatTmp, args);
        va_end(va2);

        /* done. */
        pVM->vm.s.pErrorR3 = MMHyper2HC(pVM, (uintptr_t)pArgs.pErr);
    }
#endif
}

