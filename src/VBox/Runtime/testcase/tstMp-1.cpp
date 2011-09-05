/* $Id$ */
/** @file
 * IPRT Testcase - RTMp.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned g_cErrors = 0;


int main()
{
    RTR3InitExeNoArguments(0);
    RTPrintf("tstMp-1: TESTING...\n");

    /*
     * Present and possible CPUs.
     */
    RTCPUID cCpus = RTMpGetCount();
    if (cCpus > 0)
        RTPrintf("tstMp-1: RTMpGetCount -> %d\n", (int)cCpus);
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetCount -> %d\n", (int)cCpus);
        g_cErrors++;
        cCpus = 1;
    }

    RTCPUSET Set;
    PRTCPUSET pSet = RTMpGetSet(&Set);
    if (pSet == &Set)
    {
        if ((RTCPUID)RTCpuSetCount(&Set) != cCpus)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetSet returned a set with a different cpu count; %d, expected %d\n",
                     RTCpuSetCount(&Set), cCpus);
            g_cErrors++;
        }
        RTPrintf("tstMp-1: Possible CPU mask:\n");
        for (int iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
        {
            RTCPUID idCpu = RTMpCpuIdFromSetIndex(iCpu);
            if (RTCpuSetIsMemberByIndex(&Set, iCpu))
            {
                RTPrintf("tstMp-1: %2d - id %d: %u/%u MHz", iCpu, (int)idCpu,
                         RTMpGetCurFrequency(idCpu), RTMpGetMaxFrequency(idCpu));
                if (RTMpIsCpuPresent(idCpu))
                    RTPrintf(RTMpIsCpuOnline(idCpu) ? " online\n" : " offline\n");
                else
                {
                    if (!RTMpIsCpuOnline(idCpu))
                        RTPrintf(" absent\n");
                    else
                    {
                        RTPrintf(" online but absent!\n");
                        RTPrintf("tstMp-1: FAILURE: Cpu with index %d is report as !RTIsCpuPresent while RTIsCpuOnline returns true!\n", iCpu);
                        g_cErrors++;
                    }
                }
                if (!RTMpIsCpuPossible(idCpu))
                {
                    RTPrintf("tstMp-1: FAILURE: Cpu with index %d is returned by RTCpuSet but not RTMpIsCpuPossible!\n", iCpu);
                    g_cErrors++;
                }
            }
            else if (RTMpIsCpuPossible(idCpu))
            {
                RTPrintf("tstMp-1: FAILURE: Cpu with index %d is returned by RTMpIsCpuPossible but not RTCpuSet!\n", iCpu);
                g_cErrors++;
            }
            else if (RTMpGetCurFrequency(idCpu) != 0)
            {
                RTPrintf("tstMp-1: FAILURE: RTMpGetCurFrequency(%d[idx=%d]) didn't return 0 as it should\n", (int)idCpu, iCpu);
                g_cErrors++;
            }
            else if (RTMpGetMaxFrequency(idCpu) != 0)
            {
                RTPrintf("tstMp-1: FAILURE: RTMpGetMaxFrequency(%d[idx=%d]) didn't return 0 as it should\n", (int)idCpu, iCpu);
                g_cErrors++;
            }
        }
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetSet -> %p, expected %p\n", pSet, &Set);
        g_cErrors++;
        RTCpuSetEmpty(&Set);
        RTCpuSetAdd(&Set, RTMpCpuIdFromSetIndex(0));
    }

    /*
     * Online CPUs.
     */
    RTCPUID cCpusOnline = RTMpGetOnlineCount();
    if (cCpusOnline > 0)
    {
        if (cCpusOnline <= cCpus)
            RTPrintf("tstMp-1: RTMpGetOnlineCount -> %d\n", (int)cCpusOnline);
        else
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetOnlineCount -> %d, expected <= %d\n", (int)cCpusOnline, (int)cCpus);
            g_cErrors++;
            cCpusOnline = cCpus;
        }
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetOnlineCount -> %d\n", (int)cCpusOnline);
        g_cErrors++;
        cCpusOnline = 1;
    }

    RTCPUSET SetOnline;
    pSet = RTMpGetOnlineSet(&SetOnline);
    if (pSet == &SetOnline)
    {
        if (RTCpuSetCount(&SetOnline) <= 0)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetOnlineSet returned an empty set!\n");
            g_cErrors++;
        }
        else if ((RTCPUID)RTCpuSetCount(&SetOnline) > cCpus)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetOnlineSet returned a too high value; %d, expected <= %d\n",
                     RTCpuSetCount(&SetOnline), cCpus);
            g_cErrors++;
        }
        RTPrintf("tstMp-1: Online CPU mask:\n");
        for (int iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
            if (RTCpuSetIsMemberByIndex(&SetOnline, iCpu))
            {
                RTCPUID idCpu = RTMpCpuIdFromSetIndex(iCpu);
                RTPrintf("tstMp-1: %2d - id %d: %u/%u MHz %s\n", iCpu, (int)idCpu, RTMpGetCurFrequency(idCpu),
                         RTMpGetMaxFrequency(idCpu), RTMpIsCpuOnline(idCpu) ? "online" : "offline");
                if (!RTCpuSetIsMemberByIndex(&Set, iCpu))
                {
                    RTPrintf("tstMp-1: FAILURE: online cpu with index %2d is not a member of the possible cpu set!\n", iCpu);
                    g_cErrors++;
                }
            }

        /* There isn't any sane way of testing RTMpIsCpuOnline really... :-/ */
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetOnlineSet -> %p, expected %p\n", pSet, &Set);
        g_cErrors++;
    }

    /*
     * Present CPUs.
     */
    RTCPUID cCpusPresent = RTMpGetPresentCount();
    if (cCpusPresent > 0)
    {
        if (    cCpusPresent <= cCpus
            &&  cCpusPresent >= cCpusOnline)
            RTPrintf("tstMp-1: RTMpGetPresentCount -> %d\n", (int)cCpusPresent);
        else
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetPresentCount -> %d, expected <= %d and >= %d\n", (int)cCpusPresent, (int)cCpus, (int)cCpusOnline);
            g_cErrors++;
        }
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetPresentCount -> %d\n", (int)cCpusPresent);
        g_cErrors++;
        cCpusPresent = 1;
    }

    RTCPUSET SetPresent;
    pSet = RTMpGetPresentSet(&SetPresent);
    if (pSet == &SetPresent)
    {
        if (RTCpuSetCount(&SetPresent) <= 0)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetPresentSet returned an empty set!\n");
            g_cErrors++;
        }
        else if ((RTCPUID)RTCpuSetCount(&SetPresent) != cCpusPresent)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetPresentSet returned a bad value; %d, expected = %d\n",
                     RTCpuSetCount(&SetPresent), cCpusPresent);
            g_cErrors++;
        }
        RTPrintf("tstMp-1: Present CPU mask:\n");
        for (int iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
            if (RTCpuSetIsMemberByIndex(&SetPresent, iCpu))
            {
                RTCPUID idCpu = RTMpCpuIdFromSetIndex(iCpu);
                RTPrintf("tstMp-1: %2d - id %d: %u/%u MHz %s\n", iCpu, (int)idCpu, RTMpGetCurFrequency(idCpu),
                         RTMpGetMaxFrequency(idCpu), RTMpIsCpuPresent(idCpu) ? "present" : "absent");
                if (!RTCpuSetIsMemberByIndex(&Set, iCpu))
                {
                    RTPrintf("tstMp-1: FAILURE: online cpu with index %2d is not a member of the possible cpu set!\n", iCpu);
                    g_cErrors++;
                }
            }

        /* There isn't any sane way of testing RTMpIsCpuPresent really... :-/ */
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetPresentSet -> %p, expected %p\n", pSet, &Set);
        g_cErrors++;
    }


    /* Find an online cpu for the next test. */
    RTCPUID idCpuOnline;
    for (idCpuOnline = 0; idCpuOnline < RTCPUSET_MAX_CPUS; idCpuOnline++)
        if (RTMpIsCpuOnline(idCpuOnline))
            break;

    /*
     * Quick test of RTMpGetDescription.
     */
    char szBuf[64];
    int rc = RTMpGetDescription(idCpuOnline, &szBuf[0], sizeof(szBuf));
    if (RT_SUCCESS(rc))
    {
        RTPrintf("tstMp-1: RTMpGetDescription -> '%s'\n", szBuf);

        size_t cch = strlen(szBuf);
        rc = RTMpGetDescription(idCpuOnline, &szBuf[0], cch);
        if (rc != VERR_BUFFER_OVERFLOW)
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetDescription -> %Rrc, expected VERR_BUFFER_OVERFLOW\n", rc);
            g_cErrors++;
        }
        rc = RTMpGetDescription(idCpuOnline, &szBuf[0], cch + 1);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstMp-1: FAILURE: RTMpGetDescription -> %Rrc, expected VINF_SUCCESS\n", rc);
            g_cErrors++;
        }
    }
    else
    {
        RTPrintf("tstMp-1: FAILURE: RTMpGetDescription -> %Rrc\n", rc);
        g_cErrors++;
    }


    if (!g_cErrors)
        RTPrintf("tstMp-1: SUCCESS\n", g_cErrors);
    else
        RTPrintf("tstMp-1: FAILURE - %d errors\n", g_cErrors);
    return !!g_cErrors;
}

