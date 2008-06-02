/* $Id$ */
/** @file
 * VBox Support Driver - Contiguous Memory Testcase (ring-3).
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
#include <VBox/sup.h>
#include <VBox/param.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char **argv)
{
    int rc;
    int rcRet = 0;

    RTR3Init(false);
    rc = SUPInit();
    RTPrintf("tstContiguous: SUPInit -> rc=%Vrc\n", rc);
    rcRet += rc != 0;
    if (!rc)
    {
        /*
         * Allocate a bit of contiguous memory.
         */
        RTHCPHYS HCPhys;
        void *pv = SUPContAlloc(8, &HCPhys);
        rcRet += pv == NULL || HCPhys == 0;
        if (pv && HCPhys)
        {
            memset(pv, 0xff, PAGE_SIZE * 8);
            pv = SUPContAlloc(5, &HCPhys);
            rcRet += pv == NULL || HCPhys == 0;
            if (pv && HCPhys)
            {
                memset(pv, 0x7f, PAGE_SIZE * 5);
                rc = SUPContFree(pv, 5);
                rcRet += rc != 0;
                if (rc)
                    RTPrintf("tstContiguous: SUPContFree failed! rc=%Vrc\n", rc);

                void *apv[128];
                for (unsigned i = 0; i < RT_ELEMENTS(apv); i++)
                {
                    apv[i] = SUPContAlloc(1 + (i % 11), &HCPhys);
                    if (!apv[i])
                    {
                        RTPrintf("tstContiguous: i=%d: failed to allocate %d pages\n", i, 1 + (i % 11));
                        rcRet++;
                    }
                }
                for (unsigned i = 0; i < RT_ELEMENTS(apv); i++)
                    if (apv[i])
                    {
                        rc = SUPContFree(apv[i], 1 + (i % 11));
                        rcRet += rc != 0;
                        if (rc)
                            RTPrintf("tstContiguous: i=%d SUPContFree failed! rc=%Vrc\n", i, rc);
                    }
            }
            else
                RTPrintf("tstContiguous: SUPContAlloc (2nd) failed!\n");
        }
        else
            RTPrintf("tstContiguous: SUPContAlloc failed!\n");

        rc = SUPTerm();
        RTPrintf("tstContiguous: SUPTerm -> rc=%Vrc\n", rc);
        rcRet += rc != 0;
    }

    return rcRet ? 1 : 0;
}
