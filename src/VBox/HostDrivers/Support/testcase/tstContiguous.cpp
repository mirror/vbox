/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test contiguous memory
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
#include <VBox/sup.h>
#include <VBox/param.h>
#include <iprt/runtime.h>
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
        void *pv = SUPContAlloc(PAGE_SIZE * 8 + 1, &HCPhys);
        rcRet += pv == NULL || HCPhys == 0;
        if (pv && HCPhys)
        {
            memset(pv, 0xff, PAGE_SIZE * 8 + 1);
            pv = SUPContAlloc(PAGE_SIZE * 5 + 2, &HCPhys);
            rcRet += pv == NULL || HCPhys == 0;
            if (pv && HCPhys)
            {
                memset(pv, 0x7f, PAGE_SIZE * 5 + 2);
                rc = SUPContFree(pv);
                rcRet += rc != 0;
                if (rc)
                    RTPrintf("tstContiguous: SUPContFree failed! rc=%Vrc\n", rc);
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

    /*
     * 2nd part
     */
    if (!rcRet)
    {
        rc = SUPInit();
        RTPrintf("tstContiguous: SUPInit -> rc=%Vrc\n", rc);
        rcRet += rc != 0;
        if (!rc)
        {
            for (int i = 0; i < 256; i++)
            {
                RTHCPHYS HCPhys = 0;
                void *pv = SUPContAlloc(PAGE_SIZE + 512 + i, &HCPhys);
                rcRet += pv == NULL || HCPhys == 0;
                if (pv && HCPhys)
                {
                    memset(pv, 0x7f, PAGE_SIZE + 512 + i);
                    rc = SUPContFree(pv);
                    rcRet += rc != 0;
                    if (rc)
                        RTPrintf("tstContiguous: %d: SUPContFree failed! rc=%Vrc\n", i, rc);
                }
                else
                    RTPrintf("tstContiguous: %d: SUPContAlloc failed!\n", i);
            }

            rc = SUPTerm();
            RTPrintf("tstContiguous: SUPTerm -> rc=%Vrc\n", rc);
            rcRet += rc != 0;
        }
    }

    return rcRet;
}
