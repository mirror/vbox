/* $Id$ */
/** @file
 * VMM Testcase.
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
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMM"

VMMR3DECL(int) VMMDoTest(PVM pVM);

/** PDMR3LdrEnumModules callback, see FNPDMR3ENUM. */
static DECLCALLBACK(int)
tstVMMLdrEnum(PVM pVM, const char *pszFilename, const char *pszName, RTUINTPTR ImageBase, size_t cbImage, bool fGC, void *pvUser)
{
    NOREF(pVM); NOREF(pszFilename); NOREF(fGC); NOREF(pvUser);
    RTPrintf("tstVMM: %RTptr %s\n", ImageBase, pszName);
    return VINF_SUCCESS;
}


int main(int argc, char **argv)
{
    int     rcRet = 0;                  /* error count. */

    RTR3InitAndSUPLib();

    /*
     * Create empty VM.
     */
    RTPrintf(TESTCASE ": Initializing...\n");
    PVM pVM;
    int rc = VMR3Create(1, NULL, NULL, NULL, NULL, &pVM);
    if (RT_SUCCESS(rc))
    {
        PDMR3LdrEnumModules(pVM, tstVMMLdrEnum, NULL);
        RTStrmFlush(g_pStdOut);
        RTThreadSleep(256);

        /*
         * Do testing.
         */
        RTPrintf(TESTCASE ": Testing...\n");
        PVMREQ pReq1 = NULL;
        rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)VMMDoTest, 1, pVM);
        AssertRC(rc);
        VMR3ReqFree(pReq1);

        STAMR3Dump(pVM, "*");

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!RT_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%Rrc\n", rc);
            rcRet++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%Rrc\n", rc);
        rcRet++;
    }

    return rcRet;
}
