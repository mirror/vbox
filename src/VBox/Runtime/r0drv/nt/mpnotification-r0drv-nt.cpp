/* $Id$ */
/** @file
 * IPRT - Multiprocessor Event Notifications, Ring-0 Driver, NT.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-nt-kernel.h"

#include <iprt/mp.h>
#include <iprt/err.h>
#include <iprt/cpuset.h>
#include "r0drv/mp-r0drv.h"
#include "internal-r0drv-nt.h"


#if /*NTDDI_VERSION >= NTDDI_WS08*/ 0
/* The following is 100% untested code which probably doesn't even compile. */

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Typedef of KeRegisterProcessorChangeCallback. */
typedef PROCESSOR_CALLBACK_HANDLE (__stdcall *PFNMYKEREGISTERPROCESSORCHANGECALLBACK)(IN PPROCESSOR_CALLBACK_FUNCTION, IN PVOID, IN ULONG);
/** Typedef of KeDeregisterProcessorChangeCallback. */
typedef VOID (__stdcall *PFNMYKEDEREGISTERPROCESSORCHANGECALLBACK)(IN PROCESSOR_CALLBACK_HANDLE);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The pointer to KeRegisterProcessorChangeCallback if found. */
static PFNMYKEREGISTERPROCESSORCHANGECALLBACK   g_pfnKeRegisterProcessorChangeCallback = NULL;
/** The pointer to KeDeregisterProcessorChangeCallback if found. */
static PFNMYKEDEREGISTERPROCESSORCHANGECALLBACK g_pfnKeDeregisterProcessorChangeCallback = NULL;
/** The callback handle. */
static PROCESSOR_CALLBACK_HANDLE g_hCallback = NULL;


/**
 * The native callback.
 *
 * @param   pNotifierBlock  Pointer to g_NotifierBlock.
 * @param   ulNativeEvent   The native event.
 * @param   pvCpu           The cpu id cast into a pointer value.
 */
static VOID __stdcall rtMpNotificationNtCallback(IN PVOID pvUser,
                                                 IN PKE_PROCESSOR_CHANGE_NOTIFY_CONTEXT pChangeContext,
                                                 IN OUT PNTSTATUS pOperationStatus)
{
    NOREF(pvUser);
    AssertPtr(pChangeContext);
    AssertPtrNull(pOperationStatus);

    RTCPUID idCpu = pChangeContext->NtNumber;
    switch (pChangeContext->State)
    {
        case KeProcessorAddStartNotify:
        case KeProcessorAddFailureNotify:
            break;

        case KeProcessorAddCompleteNotify:
            rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, idCpu);
            RTCpuSetAdd(&g_rtMpNtCpuSet, idCpu);
            break;

        //case KeProcessorDelCompleteNotify:
        //    rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, idCpu);
        //    break;

        default:
           AssertMsgFailed(("Unexpected state=%d idCpu=%d\n", pChangeContext->State, (int)idCpu));
           break;
    }

    *pOperationStatus = STATUS_SUCCESS;
    return 0;
}


int rtR0MpNotificationNativeInit(void *pvOS)
{
    /*
     * Try resolve the symbols.
     */
    UNICODE_STRING RoutineName;
    RtlInitUnicodeString(&RoutineName, L"KeRegisterProcessorChangeCallback");
    g_pfnKeRegisterProcessorChangeCallback = (PFNMYKEREGISTERPROCESSORCHANGECALLBACK)MmGetSystemRoutineAddress(&RoutineName);
    if (g_pfnKeRegisterProcessorChangeCallback)
    {
        RtlInitUnicodeString(&RoutineName, L"KeDeregisterProcessorChangeCallback");
        g_pfnKeDeregisterProcessorChangeCallback = (PFNMYKEDEREGISTERPROCESSORCHANGECALLBACK)MmGetSystemRoutineAddress(&RoutineName);
        if (g_pfnKeDeregisterProcessorChangeCallback)
        {
            /*
             * Try call it.
             */
            NTSTATUS ntRc = 0;
            g_hCallback = g_pfnKeRegisterProcessorChangeCallback(rtMpNotificationNtCallback, &ntRc, KE_PROCESSOR_CHANGE_ADD_EXISTING);
            if (g_hCallback != NULL)
                return VINF_SUCCESS;

            /* Genuine failure. */
            int rc = RTErrConvertFromNtStatus(ntRc);
            AssertMsgFailed(("ntRc=%#x rc=%d\n", ntRc, rc));
            return rc;
        }

        /* this shouldn't happen. */
        AssertFailed();
    }

    /* Not supported - success. */
    g_pfnKeRegisterProcessorChangeCallback = NULL;
    g_pfnKeDeregisterProcessorChangeCallback = NULL;
    return VINF_SUCCESS;
}


void rtR0MpNotificationNativeTerm(void *pvOS)
{
    if (    g_pfnKeDeregisterProcessorChangeCallback
        &&  g_hCallback)
    {
        g_pfnKeDeregisterProcessorChangeCallback(g_hCallback);
        g_hCallback = NULL;
    }
}

#else   /* Not supported */

int rtR0MpNotificationNativeInit(void *pvOS)
{
    NOREF(pvOS);
    return VINF_SUCCESS;
}

void rtR0MpNotificationNativeTerm(void *pvOS)
{
    NOREF(pvOS);
}

#endif  /* Not supported */

