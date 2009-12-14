/* $Id$ */
/** @file
 * IPRT - Lock Validator.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/lockvalidator.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/



RTDECL(void) RTLockValidatorInit(PRTLOCKVALIDATORREC pRec, RTLOCKVALIDATORCLASS hClass,
                                 uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->u32Magic      = RTLOCKVALIDATORREC_MAGIC;
    pRec->uLine         = 0;
    pRec->pszFile       = NULL;
    pRec->pszFunction   = NULL;
    pRec->uId           = 0;
    pRec->hThread       = NIL_RTTHREAD;
    pRec->pDown         = NULL;
    pRec->hClass        = hClass;
    pRec->uSubClass     = uSubClass;
    pRec->u32Reserved   = UINT32_MAX;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
}


RTDECL(int)  RTLockValidatorCreate(PRTLOCKVALIDATORREC *ppRec, RTLOCKVALIDATORCLASS hClass,
                                   uint32_t uSubClass, const char *pszName, void *pvLock)
{
    PRTLOCKVALIDATORREC pRec;
    *ppRec = pRec = (PRTLOCKVALIDATORREC)RTMemAlloc(sizeof(*pRec));
    if (!pRec)
        return VERR_NO_MEMORY;

    RTLockValidatorInit(pRec, hClass, uSubClass, pszName, pvLock);
    pRec->u32Reserved = UINT32_MAX / 2;

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorDelete(PRTLOCKVALIDATORREC pRec)
{
    Assert(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);
    Assert(pRec->u32Reserved == UINT32_MAX);

    ASMAtomicWriteU32(&pRec->u32Magic, RTLOCKVALIDATORREC_MAGIC_DEAD);
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
    ASMAtomicWriteU32(&pRec->u32Reserved, 0);
}


RTDECL(void) RTLockValidatorDestroy(PRTLOCKVALIDATORREC *ppRec)
{
    PRTLOCKVALIDATORREC pRec = *ppRec;
    *ppRec = NULL;
    if (pRec)
    {
        Assert(pRec->u32Reserved == UINT32_MAX / 2);
        pRec->u32Reserved = UINT32_MAX;
        RTLockValidatorDelete(pRec);
        RTMemFree(pRec);
    }
}


RTDECL(int) RTLockValidatorCheckOrder(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorSetOwner(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    AssertReturnVoid(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);
    Assert(pRec->hThread == NIL_RTTHREAD);

    /*
     * Update the record.
     */
    ASMAtomicUoWriteU32(&pRec->uLine, iLine);
    ASMAtomicUoWritePtr((void * volatile *)&pRec->pszFile,      pszFile);
    ASMAtomicUoWritePtr((void * volatile *)&pRec->pszFunction,  pszFunction);
    ASMAtomicUoWritePtr((void * volatile *)&pRec->uId,          (void *)uId);

    if (hThread == NIL_RTTHREAD)
    {
        hThread = RTThreadSelf();
        if (RT_UNLIKELY(hThread == NIL_RTTHREAD))
            RTThreadAdopt(RTTHREADTYPE_DEFAULT, 0, NULL, &hThread);
    }
    ASMAtomicWriteHandle(&pRec->hThread, hThread);

    /*
     * Push the lock onto the lock stack.
     */
    /** @todo push it onto the per-thread lock stack. */
}


RTDECL(void) RTLockValidatorUnsetOwner(PRTLOCKVALIDATORREC pRec)
{
    AssertReturnVoid(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);
    Assert(pRec->hThread != NIL_RTTHREAD);

    /*
     * Pop (remove) the lock.
     */
    /** @todo remove it from the per-thread stack/whatever. */

    /*
     * Update the record.
     */
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
}

