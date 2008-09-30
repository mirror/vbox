/** @file
 *
 * VBox Debugger GUI, dummy testcase.
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
#include <qapplication.h>
#include <VBox/dbggui.h>
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

    RTR3InitAndSUPLib();
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
        PDBGGUI pGui;
        PCDBGGUIVT pGuiVT;
        rc = DBGGuiCreateForVM(pVM, &pGui, &pGuiVT);
        if (RT_SUCCESS(rc))
        {
            if (argc <= 1 || argc == 2)
            {
                RTPrintf(TESTCASE ": calling pfnShowCommandLine...\n");
                rc = pGuiVT->pfnShowCommandLine(pGui);
                if (RT_FAILURE(rc))
                {
                    RTPrintf(TESTCASE ": error: pfnShowCommandLine failed! rc=%Rrc\n", rc);
                    cErrors++;
                }
            }

            if (argc <= 1 || argc == 3)
            {
                RTPrintf(TESTCASE ": calling pfnShowStatistics...\n");
                pGuiVT->pfnShowStatistics(pGui);
                if (RT_FAILURE(rc))
                {
                    RTPrintf(TESTCASE ": error: pfnShowStatistics failed! rc=%Rrc\n", rc);
                    cErrors++;
                }
            }

            pGuiVT->pfnAdjustRelativePos(pGui, 0, 0, 640, 480);
            RTPrintf(TESTCASE ": calling App.exec()...\n");
            App.exec();
        }
        else
        {
            RTPrintf(TESTCASE ": error: DBGGuiCreateForVM failed! rc=%Rrc\n", rc);
            cErrors++;
        }

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%Rrc\n", rc);
            cErrors++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%Rrc\n", rc);
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

