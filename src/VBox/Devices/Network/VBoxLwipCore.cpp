/* $Id$ */
/** @file
 * VBox Lwip Core Initiatetor/Finilizer.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/**
 * @todo: this should be somehow shared with with DevINIP, because
 * we want that every NAT and DevINIP instance uses a initialized LWIP
 * initialization of LWIP should happen on iLWIPInitiatorCounter 0 -> 1.
 * see pfnConstruct/Destruct. 
 *
 * @note: see comment to DevINIP.cpp:DevINIPConfigured 
 * @note: perhaps initilization stuff would be better move out of NAT driver,
 *  because we have to deal with attaching detaching NAT driver at runtime.
 */
#include <iprt/types.h>
#include "VBoxLwipCore.h"
/* @todo: lwip or nat ? */
#define LOG_GROUP LOG_GROUP_DRV_NAT
#include <iprt/critsect.h>
#include <iprt/timer.h>
#include <VBox/err.h>
#include <VBox/log.h>

extern "C" {
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "netif/etharp.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp_impl.h"
#include "lwip/tcpip.h"
}

typedef struct {
    PFNRT1 pfn;
    void *pvUser;
} LWIPCOREUSERCALLBACK, *PLWIPCOREUSERCALLBACK; 


typedef struct LWIPCORE 
{
    int iLWIPInitiatorCounter;
    PRTTIMER LwipArpTimer;
    PRTTIMER LwipTcpFastTimer;
    PRTTIMER LwipTcpSlowTimer;
    /* semaphore to coordinate 'tcpip' thread initialization */
    sys_sem_t LwipTcpIpSem;
    /* Initalization user defined callback */
    LWIPCOREUSERCALLBACK userInitClbk;
    /* Finitialization user defined callback */
    LWIPCOREUSERCALLBACK userFiniClbk;
    RTCRITSECT csLwipCore;
} LWIPCORE;

static LWIPCORE g_LwipCore;


/* Lwip Core Timer functions */
void tmrLwipCoreArp(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    lwip_etharp_tmr();
}


void tmrLwipCoreTcpFast(PRTTIMER pTimer, void *pvUser, uint64_t iTick) 
{
    lwip_tcp_fasttmr();
}


void tmrLwipCoreTcpSlow(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    lwip_tcp_slowtmr();
}

/**
 * @note: this function executed on TCPIP thread.
 */
static DECLCALLBACK(void) lwipCoreInitDone(void *pvArg)
{
    sys_sem_t *pLwipSem = (sys_sem_t *)pvArg;
    LogFlowFunc(("ENTER: pvArg:%p\n", pvArg));
    AssertPtrReturnVoid(pvArg);
    lwip_sys_sem_signal(pLwipSem);
    LogFlowFuncLeave();
}

/**
 * @note: this function executed on TCPIP thread.
 */
static DECLCALLBACK(void) lwipCoreFiniDone(void *pvArg)
{
    sys_sem_t *pLwipSem = (sys_sem_t *)pvArg;
    LogFlowFunc(("ENTER: pvArg:%p\n", pvArg));
    AssertPtrReturnVoid(pvArg);
    lwip_sys_sem_signal(pLwipSem);
    LogFlowFuncLeave();
}

/**
 * @note: this function executed on TCPIP thread.
 */
static DECLCALLBACK(void) lwipCoreUserCallback(void *pvArg)
{
    PLWIPCOREUSERCALLBACK pUserClbk = (PLWIPCOREUSERCALLBACK)pvArg;
    LogFlowFunc(("ENTER: pvArg:%p\n", pvArg));
    
    if (pUserClbk->pfn)
        pUserClbk->pfn(pUserClbk->pvUser);

    /* we've finished on tcpip thread and want to wake up, waiters on EMT/main */
    lwip_sys_sem_signal(&g_LwipCore.LwipTcpIpSem);
    AssertPtrReturnVoid(pvArg);
    LogFlowFuncLeave();
}

/**
 * This function initialize lwip core once 
 * further NAT instancies should just add netifs configured according 
 * their needs.
 *
 * We're on EMT-n or main thread of the network service, we want execute
 * anything on tcpip thread.
 */
int vboxLwipCoreInitialize(PFNRT1 pfnCallback, void *pvCallbackArg)
{
    int rc = VINF_SUCCESS;
    int lwipRc = ERR_OK;
    LogFlowFuncEnter();

    if (!RTCritSectIsInitialized(&g_LwipCore.csLwipCore))
    {
        AssertReturn(g_LwipCore.iLWIPInitiatorCounter == 0, VERR_INTERNAL_ERROR);
        rc = RTCritSectInit(&g_LwipCore.csLwipCore);

        AssertRCReturn(rc, rc);
    }

    RTCritSectEnter(&g_LwipCore.csLwipCore);
    
    g_LwipCore.iLWIPInitiatorCounter++;

    if (g_LwipCore.iLWIPInitiatorCounter == 1)
    {
        lwipRc = lwip_sys_sem_new(&g_LwipCore.LwipTcpIpSem, 0);
        /* @todo: VERR_INTERNAL_ERROR perhaps should be replaced with right error code */
        if (lwipRc != ERR_OK) 
        {
            rc = VERR_INTERNAL_ERROR;
            goto done;
        }

        lwip_tcpip_init(lwipCoreInitDone, &g_LwipCore.LwipTcpIpSem,
                        lwipCoreFiniDone, &g_LwipCore.LwipTcpIpSem);
        lwip_sys_sem_wait(&g_LwipCore.LwipTcpIpSem, 0);
    } /* end of if (g_LwipCore.iLWIPInitiatorCounter == 1) */


    /* tcpip thread launched */
    g_LwipCore.userInitClbk.pfn = pfnCallback;
    g_LwipCore.userInitClbk.pvUser = pvCallbackArg;

    /* _block mean that we might be blocked on post to mailbox and the last 1 is our 
     * agreement with this fact
     */
    lwipRc = tcpip_callback_with_block(lwipCoreUserCallback, &g_LwipCore.userInitClbk, 1);
    if (lwipRc != ERR_OK) 
    {
        rc = VERR_INTERNAL_ERROR;
        goto done;
    }


    /* we're waiting the result here */
    lwipRc = lwip_sys_sem_wait(&g_LwipCore.LwipTcpIpSem, 0);
    if (lwipRc != ERR_OK) 
    {
        rc = VERR_INTERNAL_ERROR;
        goto done;
    }
    
done:
    RTCritSectLeave(&g_LwipCore.csLwipCore);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * This function decrement lwip reference counter 
 * and calls tcpip thread termination function.
 */
void vboxLwipCoreFinalize(PFNRT1 pfnCallback, void *pvCallbackArg)
{
    int lwipRc = ERR_OK;
    LogFlowFuncEnter();
    AssertReleaseReturnVoid(   RTCritSectIsInitialized(&g_LwipCore.csLwipCore)
                            && g_LwipCore.iLWIPInitiatorCounter >= 1);
    RTCritSectEnter(&g_LwipCore.csLwipCore);
    
    g_LwipCore.iLWIPInitiatorCounter--;
    g_LwipCore.userFiniClbk.pfn = pfnCallback;
    g_LwipCore.userFiniClbk.pvUser = pvCallbackArg;
    
    lwipRc = tcpip_callback_with_block(lwipCoreUserCallback, &g_LwipCore.userFiniClbk, 1);

    if (lwipRc == ERR_OK)
        lwip_sys_sem_wait(&g_LwipCore.LwipTcpIpSem, 0);
    
    if (g_LwipCore.iLWIPInitiatorCounter == 0)
    {
        tcpip_terminate();
        RTCritSectLeave(&g_LwipCore.csLwipCore);
        RTCritSectDelete(&g_LwipCore.csLwipCore);
    }
    else
        RTCritSectLeave(&g_LwipCore.csLwipCore);

    LogFlowFuncLeave();
}
