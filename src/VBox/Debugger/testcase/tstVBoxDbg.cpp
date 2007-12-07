/** @file
 *
 * VBox Debugger GUI, dummy testcase.
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
#include <qapplication.h>
#include <VBox/dbg.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/runtime.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>


#define TESTCASE "tstVBoxDbg"


int main(int argc, char **argv)
{
    int     cErrors = 0;                  /* error count. */

    RTR3Init();
    RTPrintf(TESTCASE ": TESTING...\n");

    /*
     * Create empty VM.
     */
    PVM pVM;
    int rc = VMR3Create(NULL, NULL, NULL, NULL, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Instantiate the debugger GUI bits and run them.
         */
        QApplication App(argc, argv);
//        DBGGuiCreate(pVM, true, NULL);
        App.exec();

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%d\n", rc);
            cErrors++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%d\n", rc);
        cErrors++;
    }

    /*
     * Summay and exit.
     */
    if (!cErrors)
        RTPrintf(TESTCASE ": SUCCESS\n");
    else
        RTPrintf(TESTCASE ": FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

