/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the page allocation interface
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
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
            rc = SUPPageFree(pv, 1);
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
            rc = SUPPageFree(pv, 1023);
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
