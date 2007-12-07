/* $Id$ */
/** @file
 * VM - Virtual Machine All Contexts.
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
 *    @endcode
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
 *    @endcode
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
    va_list va2;
    va_copy(va2, args); /* Have to make a copy here or GCC will break. */
    PVMREQ pReq;
    VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3SetErrorV, 7,  /* ASSUMES 3 source pos args! */
                pVM, rc, RT_SRC_POS_ARGS, pszFormat, &va2);
    VMR3ReqFree(pReq);
    va_end(va2);

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


/**
 * Sets the runtime error message.
 * As opposed VMSetError(), this method is intended to inform the VM user about
 * errors and error-like conditions that happen at an arbitrary point during VM
 * execution (like "host memory low" or "out of host disk space").
 *
 * The @a fFatal parameter defines whether the error is fatal or not. If it is
 * true, then it is expected that the caller has already paused the VM execution
 * before calling this method. The VM user is supposed to power off the VM
 * immediately after it has received the runtime error notification via the
 * FNVMATRUNTIMEERROR callback.
 *
 * If @a fFatal is false, then the paused state of the VM defines the kind of
 * the error. If the VM is paused before calling this method, it means that
 * the VM user may try to fix the error condition (i.e. free more host memory)
 * and then resume the VM execution. If the VM is not paused before calling
 * this method, it means that the given error is a warning about an error
 * condition that may happen soon but that doesn't directly affect the
 * VM execution by the time of the call.
 *
 * The @a pszErrorID parameter defines an unique error identificator.
 * It is used by the front-ends to show a proper message to the end user
 * containig possible actions (for example, Retry/Ignore). For this reason,
 * an error ID assigned once to some particular error condition should not
 * change in the future. The format of this parameter is "someErrorCondition". 
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   ...             Error message arguments.
 *
 * @return  VBox status code (whether the error has been successfully set
 *          and delivered to callbacks or not).
 *
 * @thread  Any
 * @todo    r=bird: The pausing/suspending of the VM should be done here, we'll just end 
 *                  up duplicating code all over the place otherwise. In the case of 
 *                  devices/drivers/etc they might not be trusted to pause/suspend the 
 *                  vm even. Change fFatal to fFlags and define action flags and a fatal flag.
 *                      
 *                  Also, why a string ID and not an enum?
 */
VMDECL(int) VMSetRuntimeError(PVM pVM, bool fFatal, const char *pszErrorID,
                              const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pVM, fFatal, pszErrorID, pszFormat, args);
    va_end(args);
    return rc;
}


/**
 * va_list version of VMSetRuntimeError.
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 *
 * @return  VBox status code (whether the error has been successfully set
 *          and delivered to callbacks or not).
 *
 * @thread  Any
 */
VMDECL(int) VMSetRuntimeErrorV(PVM pVM, bool fFatal, const char *pszErrorID,
                               const char *pszFormat, va_list args)
{
#ifdef IN_RING3
    /*
     * Switch to EMT.
     */
    va_list va2;
    va_copy(va2, args); /* Have to make a copy here or GCC will break. */
    PVMREQ pReq;
    VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)vmR3SetRuntimeErrorV, 5,
                pVM, fFatal, pszErrorID, pszFormat, &va2);
    VMR3ReqFree(pReq);
    va_end(va2);

#else
    /*
     * We're already on the EMT thread and can safely create a VMRUNTIMEERROR chunk.
     */
    vmSetRuntimeErrorCopy(pVM, fFatal, pszErrorID, pszFormat, args);

# ifdef IN_GC
    VMMGCCallHost(pVM, VMMCALLHOST_VM_SET_RUNTIME_ERROR, 0);
# elif defined(IN_RING0)
    VMMR0CallHost(pVM, VMMCALLHOST_VM_SET_RUNTIME_ERROR, 0);
# else
# endif
#endif
    return VINF_SUCCESS;
}


/**
 * Copies the error to a VMRUNTIMEERROR structure.
 *
 * This is mainly intended for Ring-0 and GC where the error must be copied to
 * memory accessible from ring-3. But it's just possible that we might add
 * APIs for retrieving the VMRUNTIMEERROR copy later.
 *
 * @param   pVM             VM handle. Must be non-NULL.
 * @param   fFatal          Whether it is a fatal error or not.
 * @param   pszErrorID      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 * @thread  EMT
 */
void vmSetRuntimeErrorCopy(PVM pVM, bool fFatal, const char *pszErrorID,
                           const char *pszFormat, va_list args)
{
#if 0 /// @todo implement Ring-0 and GC VMSetError
    /*
     * Create the untranslated message copy.
     */
    /* free any old message. */
    MMHyperFree(pVM, MMHyperR32Ctx(pVM, pVM->vm.s.pRuntimeErrorR3));
    pVM->vm.s.pRuntimeErrorR3 = NULL;

    /* calc reasonable start size. */
    size_t cchErrorID = pszErrorID ? strlen(pszErrorID) : 0;
    size_t cchFormat = strlen(pszFormat);
    size_t cb = sizeof(VMRUNTIMEERROR)
              + cchErrorID + 1
              + cchFormat + 32;

    /* allocate it */
    void *pv;
    int rc2 = MMHyperAlloc(pVM, cb, 0, MM_TAG_VM, &pv);
    if (VBOX_SUCCESS(rc2))
    {
        /* initialize it. */
        PVMRUNTIMEERROR pErr = (PVMRUNTIMEERROR)pv;
        pErr->cbAllocated = cb;
        pErr->off = sizeof(PVMRUNTIMEERROR);
        pErr->offErrorID = = 0;

        if (cchErrorID)
        {
            pErr->offErrorID = pErr->off;
            memcpy((uint8_t *)pErr + pErr->off, pszErrorID, cchErrorID + 1);
            pErr->off += cchErrorID + 1;
        }

        pErr->offMessage = pErr->off;

        /* format the message (pErr might be reallocated) */
        VMSETRUNTIMEERRORFMTARGS Args;
        Args.pVM = pVM;
        Args.pErr = pErr;

        va_list va2;
        va_copy(va2, args);
        RTStrFormatV(vmSetRuntimeErrorFmtOut, &pErr, NULL, NULL, &pszFormatTmp, args);
        va_end(va2);

        /* done. */
        pVM->vm.s.pErrorRuntimeR3 = MMHyper2HC(pVM, (uintptr_t)pArgs.pErr);
    }
#endif
}

