/* $Id$ */
/** @file
 * VBox - Testcase for internal networking, simple NetFlt trunk creation.
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
#include <VBox/intnet.h>
#include <VBox/sup.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <iprt/initterm.h>
#include <iprt/alloc.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/param.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int g_cErrors = 0;


int main(int argc, char **argv)
{
    /*
     * Init the runtime, open the support driver and load VMMR0.r0.
     */
    RTR3Init(false, 0);
    RTPrintf("tstIntNet-1: TESTING...\n");

    /*
     * Open the session, load ring-0 and issue the request.
     */
    PSUPDRVSESSION pSession;
    int rc = SUPInit(&pSession, 0);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: SUPInit -> %Rrc\n", rc);
        return 1;
    }

    char szPath[RTPATH_MAX];
    rc = RTPathProgram(szPath, sizeof(szPath) - sizeof("/../VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: RTPathProgram -> %Rrc\n", rc);
        return 1;
    }

    rc = SUPLoadVMM(strcat(szPath, "/../VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstIntNet-1: SUPLoadVMM(\"%s\") -> %Rrc\n", szPath, rc);
        return 1;
    }

    /*
     * Create the request, picking the network and trunk names from argv[2]
     * and argv[1] if present.
     */
    INTNETOPENREQ OpenReq;
    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.pSession = pSession; /* later */
    if (argc >= 2)
        strncpy(OpenReq.szNetwork, argv[2], sizeof(OpenReq.szNetwork));
    else
        strncpy(OpenReq.szNetwork, "tstIntNet-1", sizeof(OpenReq.szNetwork));
    if (argc >= 2)
        strncpy(OpenReq.szNetwork, argv[2], sizeof(OpenReq.szNetwork));
    else
#ifdef RT_OS_DARWIN
        strncpy(OpenReq.szTrunk, "en0", sizeof(OpenReq.szTrunk));
#elif defined(RT_OS_LINUX)
        strncpy(OpenReq.szTrunk, "eth0", sizeof(OpenReq.szTrunk));
#else
        strncpy(OpenReq.szTrunk, "em0", sizeof(OpenReq.szTrunk));
#endif
    OpenReq.enmTrunkType = kIntNetTrunkType_NetFlt;
    OpenReq.fFlags = 0;
    OpenReq.cbSend = 0;
    OpenReq.cbRecv = 0;
    OpenReq.hIf = INTNET_HANDLE_INVALID;

    /*
     * Issue the request.
     */
    RTPrintf("tstIntNet-1: attempting to open/create network \"%s\" with NetFlt trunk \"%s\"...\n",
             OpenReq.szNetwork, OpenReq.szTrunk);
    RTThreadSleep(250);
    rc = SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_OPEN, 0, &OpenReq.Hdr);
    if (RT_SUCCESS(rc))
    {
        RTPrintf("tstIntNet-1: successfully opened/created \"%s\" with NetFlt trunk \"%s\" - hIf=%#x",
                 OpenReq.szNetwork, OpenReq.szTrunk, OpenReq.hIf);

    }
    else
    {
        RTPrintf("stdIntNet-1: SUPCallVMMR0Ex(,VMMR0_DO_INTNET_OPEN,) failed, rc=%Rrc\n", rc);
        g_cErrors++;
    }

    RTThreadSleep(1000);
    SUPTerm(false /* not forced */);

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstIntNet-1: SUCCESS\n");
    else
        RTPrintf("tstIntNet-1: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

