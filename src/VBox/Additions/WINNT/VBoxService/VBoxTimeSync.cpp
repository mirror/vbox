/* $Id$ */
/** @file
 * VBoxTimeSync - Virtual machine (guest) time synchronization with the host.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "VBoxService.h"
#include "VBoxTimeSync.h"

static VBOXTIMESYNCCONTEXT gCtx = {0};

#define TICKSPERSEC  10000000
#define TICKSPERMSEC 10000
#define SECSPERDAY   86400
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (uint64_t)SECSPERDAY)
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)

int vboxTimeSyncInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Assert(pEnv);
    Assert(ppInstance);

    Log(("vboxTimeSyncThread: Init.\n"));

    *pfStartThread = true;
    *ppInstance = &gCtx;

    gCtx.pEnv = pEnv;
    return 0;
}

void vboxTimeSyncDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    Assert(pEnv);

    VBOXTIMESYNCCONTEXT *pCtx = (VBOXTIMESYNCCONTEXT *)pInstance;
    Assert(pCtx);

    Log(("vboxTimeSyncThread: Destroyed.\n"));
    return;
}

unsigned __stdcall vboxTimeSyncThread(void *pInstance)
{
    Assert(pInstance);

    VBOXTIMESYNCCONTEXT *pCtx = (VBOXTIMESYNCCONTEXT *)pInstance;
    bool fTerminate = false;
    DWORD dwCnt = 60;       /* Do the first sync right after starting! */
    DWORD cbReturned = 0;

    LogRel(("vboxTimeSyncThread: Started.\n"));

    if (NULL == pCtx) {
        Log(("vboxTimeSyncThread: Invalid context!\n"));
        return -1;
    }

    do
    {
        if (dwCnt++ < 60)   /* Wait 60 secs. */
        {
            /* Sleep a bit to not eat too much CPU. */
            if (NULL == pCtx->pEnv->hStopEvent)
                Log(("vboxTimeSyncThread: Invalid stop event!\n"));

            if (WaitForSingleObject (pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
            {
                Log(("vboxTimeSyncThread: Got stop event, terminating ...\n"));
                fTerminate = true;
                break;
            }

            continue;
        }

        dwCnt = 0;

        RTTIMESPEC hostTimeSpec;
        int rc = VbglR3GetHostTime(&hostTimeSpec);  /* Get time in milliseconds. */
        if (RT_FAILURE(rc))
        {
            LogRel(("vboxTimeSyncThread: Could not query host time! Error: %ld\n", GetLastError()));
            continue;
        }
        else
        {
            /* Adjust priviledges of this process to adjust the time. */
            HANDLE hToken = NULL;       /* Process token. */
            TOKEN_PRIVILEGES tp;        /* Token provileges. */
            TOKEN_PRIVILEGES oldtp;     /* Old token privileges. */
            DWORD dwSize = sizeof (TOKEN_PRIVILEGES);          
            LUID luid = {0};

            if (!OpenProcessToken(GetCurrentProcess(), 
                                  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            {
               LogRel (("vboxTimeSyncThread: Opening process token failed with code %ld!\n", GetLastError()));
               continue;
            }
            if (!LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &luid))
            {
               LogRel (("vboxTimeSyncThread: Looking up token privileges failed with code %ld!\n", GetLastError()));
               CloseHandle (hToken);
               continue;
            }
            
            ZeroMemory (&tp, sizeof (tp));
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            
            /* Adjust Token privileges. */
            if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), 
                     &oldtp, &dwSize))
            {
               LogRel (("vboxTimeSyncThread: Adjusting token privileges failed with code %ld!\n", GetLastError()));
               CloseHandle (hToken);
               continue;
            }

            /* Set new system time. */
            LARGE_INTEGER hostTime;
            hostTime.QuadPart = RTTimeSpecGetNano(&hostTimeSpec) * (uint64_t)TICKSPERMSEC + (uint64_t)TICKS_1601_TO_1970;

            SYSTEMTIME st = {0};
            FILETIME ft = {0};

            RTTimeSpecGetNtFileTime(&hostTimeSpec, &ft);           
            if (FALSE == FileTimeToSystemTime(&ft,&st))
                LogRel(("vboxTimeSyncThread: Cannot convert system times! Error: %ld\n", GetLastError()));
             
            Log(("vboxTimeSyncThread: Synching time with host time (msec/UTC): %llu\n", hostTime));

            //if (0 == SetSystemTimeAdjustment(hostTime.QuadPart, FALSE))
            if(FALSE == SetSystemTime(&st))
            {
                DWORD dwErr = GetLastError();
                switch(dwErr)
                {

                case ERROR_PRIVILEGE_NOT_HELD:
                    LogRel(("vboxTimeSyncThread: Setting time failed! Required priviledge missing!\n"));
                    break;

                default:
                    LogRel(("vboxTimeSyncThread: Setting time failed! Last error: %ld\n", dwErr));
                    break;
                }
            }
            else
            {
                Log(("vboxTimeSyncThread: Sync successful!\n"));
            }

            //SetSystemTimeAdjustment(0 /* Ignored. */, TRUE);

            /* Disable SE_SYSTEMTIME_NAME again. */
            if (!AdjustTokenPrivileges(hToken, FALSE, &oldtp, dwSize, NULL, NULL))
               LogRel (("vboxTimeSyncThread: Adjusting back token privileges failed with code %ld!\n", GetLastError()));
               
            if (hToken)
                CloseHandle (hToken);
        }
    }
    while (!fTerminate);
    return 0;
}

