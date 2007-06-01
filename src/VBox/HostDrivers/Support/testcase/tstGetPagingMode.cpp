/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the interface for querying host paging mode
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <VBox/err.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>


int main(int argc, char **argv)
{
    int rc;
    RTR3Init(false);
    rc = SUPInit();
    if (VBOX_SUCCESS(rc))
    {
        SUPPAGINGMODE enmMode = SUPGetPagingMode();
        switch (enmMode)
        {
            case SUPPAGINGMODE_INVALID:
                RTPrintf("SUPPAGINGMODE_INVALID\n");
                break;
            case SUPPAGINGMODE_32_BIT:
                RTPrintf("SUPPAGINGMODE_32_BIT\n");
                break;
            case SUPPAGINGMODE_32_BIT_GLOBAL:
                RTPrintf("SUPPAGINGMODE_32_BIT_GLOBAL\n");
                break;
            case SUPPAGINGMODE_PAE:
                RTPrintf("SUPPAGINGMODE_PAE\n");
                break;
            case SUPPAGINGMODE_PAE_GLOBAL:
                RTPrintf("SUPPAGINGMODE_PAE_GLOBAL\n");
                break;
            case SUPPAGINGMODE_PAE_NX:
                RTPrintf("SUPPAGINGMODE_PAE_NX\n");
                break;
            case SUPPAGINGMODE_PAE_GLOBAL_NX:
                RTPrintf("SUPPAGINGMODE_PAE_GLOBAL_NX\n");
                break;
            case SUPPAGINGMODE_AMD64:
                RTPrintf("SUPPAGINGMODE_AMD64\n");
                break;
            case SUPPAGINGMODE_AMD64_GLOBAL:
                RTPrintf("SUPPAGINGMODE_AMD64_GLOBAL\n");
                break;
            case SUPPAGINGMODE_AMD64_NX:
                RTPrintf("SUPPAGINGMODE_AMD64_NX\n");
                break;
            case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                RTPrintf("SUPPAGINGMODE_AMD64_GLOBAL_NX\n");
                break;
            default:
                RTPrintf("Unknown mode %d\n", enmMode);
                rc = VERR_INTERNAL_ERROR;
                break;
        }

        int rc2 = SUPTerm();
        RTPrintf("SUPTerm -> rc=%Vrc\n", rc2);
    }
    else
        RTPrintf("SUPInit -> rc=%Vrc\n", rc);

    return !VBOX_SUCCESS(rc);
}

