/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the page allocation interface
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
#include <string.h>


int main(int argc, char **argv)
{
    int cErrors = 0;
    int rc = 0;
    RTR3Init(true, _1M*168);
    rc = SUPInit(NULL, _1M*168);
    cErrors += rc != 0;
    if (!rc)
    {
        void *pv;
        rc = SUPPageAlloc(1, &pv);
        cErrors += rc != 0;
        if (!rc)
        {
            memset(pv, 0xff, PAGE_SIZE);
            rc = SUPPageFree(pv);
            cErrors += rc != 0;
            if (rc)
                RTPrintf("tstPage: SUPPageFree() failed rc=%d\n", rc);
        }
        else
            RTPrintf("tstPage: SUPPageAlloc(1,) failed rc=%d\n", rc);

        /*
         * Big chunk.
         */
        rc = SUPPageAlloc(1023, &pv);
        cErrors += rc != 0;
        if (!rc)
        {
            memset(pv, 0xfe, 1023 << PAGE_SHIFT);
            rc = SUPPageFree(pv);
            cErrors += rc != 0;
            if (rc)
                RTPrintf("tstPage: SUPPageFree() failed rc=%d\n", rc);
        }
        else
            RTPrintf("tstPage: SUPPageAlloc(1,) failed rc=%d\n", rc);


        //rc = SUPTerm();
        cErrors += rc != 0;
        if (rc)
            RTPrintf("tstPage: SUPTerm failed rc=%d\n", rc);
    }
    else
        RTPrintf("tstPage: SUPInit failed rc=%d\n", rc);

    if (!cErrors)
        RTPrintf("tstPage: SUCCESS\n");
    else
        RTPrintf("tstPage: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
