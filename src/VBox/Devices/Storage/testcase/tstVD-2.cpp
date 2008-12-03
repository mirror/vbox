/** @file
 *
 * Simple VBox HDD container test utility. Only fast tests.
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

#include <VBox/err.h>
#include <VBox/VBoxHDD-new.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include "stdio.h"
#include "stdlib.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;


static int tstVDBackendInfo(void)
{
    int rc;
#define MAX_BACKENDS 100
    VDBACKENDINFO aVDInfo[MAX_BACKENDS];
    unsigned cEntries;

#define CHECK(str) \
    do \
    { \
        RTPrintf("%s rc=%Rrc\n", str, rc); \
        if (RT_FAILURE(rc)) \
            return rc; \
    } while (0)

    rc = VDBackendInfo(MAX_BACKENDS, aVDInfo, &cEntries);
    CHECK("VDBackendInfo()");

    for (unsigned i=0; i < cEntries; i++)
    {
        RTPrintf("Backend %u: name=%s capabilities=%#06x extensions=",
                 i, aVDInfo[i].pszBackend, aVDInfo[i].uBackendCaps);
        if (aVDInfo[i].papszFileExtensions)
        {
            const char *const *papsz = aVDInfo[i].papszFileExtensions;
            while (*papsz != NULL)
            {
                if (papsz != aVDInfo[i].papszFileExtensions)
                    RTPrintf(",");
                RTPrintf("%s", *papsz);
                papsz++;
            }
            if (papsz == aVDInfo[i].papszFileExtensions)
                RTPrintf("<EMPTY>");
        }
        else
            RTPrintf("<NONE>");
        RTPrintf(" config=");
        if (aVDInfo[i].paConfigInfo)
        {
            PCVDCONFIGINFO pa = aVDInfo[i].paConfigInfo;
            while (pa->pszKey != NULL)
            {
                if (pa != aVDInfo[i].paConfigInfo)
                    RTPrintf(",");
                RTPrintf("(key=%s type=", pa->pszKey);
                switch (pa->enmValueType)
                {
                    case VDCFGVALUETYPE_INTEGER:
                        RTPrintf("integer");
                        break;
                    case VDCFGVALUETYPE_STRING:
                        RTPrintf("string");
                        break;
                    case VDCFGVALUETYPE_BYTES:
                        RTPrintf("bytes");
                        break;
                    default:
                        RTPrintf("INVALID!");
                }
                if (pa->pszDefaultValue)
                    RTPrintf("%s", pa->pszDefaultValue);
                else
                    RTPrintf("<NONE>");
                RTPrintf(" flags=");
                if (!pa->uKeyFlags)
                    RTPrintf("none");
                unsigned cFlags = 0;
                if (pa->uKeyFlags & VD_CFGKEY_MANDATORY)
                {
                    if (cFlags)
                        RTPrintf(",");
                    RTPrintf("mandatory");
                    cFlags++;
                }
                if (pa->uKeyFlags & VD_CFGKEY_EXPERT)
                {
                    if (cFlags)
                        RTPrintf(",");
                    RTPrintf("expert");
                    cFlags++;
                }
                RTPrintf(")");
                pa++;
            }
            if (pa == aVDInfo[i].paConfigInfo)
                RTPrintf("<EMPTY>");
        }
        else
            RTPrintf("<NONE>");
        RTPrintf("\n");
    }

#undef CHECK
    return 0;
}


int main(int argc, char *argv[])
{
    int rc;

    RTR3Init();
    RTPrintf("tstVD-2: TESTING...\n");

    rc = tstVDBackendInfo();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVD-2: getting backend info test failed! rc=%Rrc\n", rc);
        g_cErrors++;
    }

    rc = VDShutdown();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVD-2: unloading backends failed! rc=%Rrc\n", rc);
        g_cErrors++;
    }
    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstVD-2: SUCCESS\n");
    else
        RTPrintf("tstVD-2: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

