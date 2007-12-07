/* $Id$ */
/** @file
 * VMM Testcase.
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
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMM"

VMMR3DECL(int) VMMDoTest(PVM pVM);


int main(int argc, char **argv)
{
    int     rcRet = 0;                  /* error count. */

    RTR3Init();

    /*
     * Create empty VM.
     */
    RTPrintf(TESTCASE ": Initializing...\n");
    PVM pVM;
    int rc = VMR3Create(NULL, NULL, NULL, NULL, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Do testing.
         */
        RTPrintf(TESTCASE ": Testing...\n");
        PVMREQ pReq1 = NULL;
        rc = VMR3ReqCall(pVM, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)VMMDoTest, 1, pVM);
        AssertRC(rc);
        VMR3ReqFree(pReq1);

        STAMR3Dump(pVM, "*");

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%d\n", rc);
            rcRet++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%d\n", rc);
        rcRet++;
    }

    return rcRet;
}
