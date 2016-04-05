/* $Id$ */
/** @file
 * UsbTestServ - Remote USB test configuration and execution server, USB gadget host interface
 *               for USB/IP.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/types.h>

#include "UsbTestServiceGadgetHostInternal.h"

/*********************************************************************************************************************************
*   Constants And Macros, Structures and Typedefs                                                                                *
*********************************************************************************************************************************/

/**
 * Internal UTS gadget host instance data.
 */
typedef struct UTSGADGETHOSTTYPEINT
{
    /** Handle to the USB/IP daemon process. */
    RTPROCESS                 hProcUsbIp;
} UTSGADGETHOSTTYPEINT;

/** Default port of the USB/IP server. */
#define UTS_GADGET_HOST_USBIP_PORT_DEF 3240

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Load a kernel module on a Linux host.
 *
 * @returns IPRT status code.
 * @param   pszModule    The module to load.
 */
static int utsGadgetHostUsbIpLoadModule(const char *pszModule)
{
    RTPROCESS hProcModprobe = NIL_RTPROCESS;
    const char *apszArgv[3];

    apszArgv[0] = "modprobe";
    apszArgv[1] = pszModule;
    apszArgv[2] = NULL;

    int rc = RTProcCreate("modprobe", apszArgv, RTENV_DEFAULT, RTPROC_FLAGS_SEARCH_PATH, &hProcModprobe);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS ProcSts;
        rc = RTProcWait(hProcModprobe, RTPROCWAIT_FLAGS_BLOCK, &ProcSts);
        if (RT_SUCCESS(rc))
        {
            /* Evaluate the process status. */
            if (   ProcSts.enmReason != RTPROCEXITREASON_NORMAL
                || ProcSts.iStatus != 0)
                rc = VERR_UNRESOLVED_ERROR; /** @todo: Log and give finer grained status code. */
        }
    }

    return rc;
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnInit}
 */
static DECLCALLBACK(int) utsGadgetHostUsbIpInit(PUTSGADGETHOSTTYPEINT pIf, PCUTSGADGETCFGITEM paCfg)
{
    int rc = VINF_SUCCESS;
    uint16_t uPort = 0;

    pIf->hProcUsbIp = NIL_RTPROCESS;

    rc = utsGadgetCfgQueryU16Def(paCfg, "UsbIp/Port", &uPort, UTS_GADGET_HOST_USBIP_PORT_DEF);
    if (RT_FAILURE(rc))
    {
        /* Make sure the kernel drivers are loaded. */
        rc = utsGadgetHostUsbIpLoadModule("usbip-core.ko");
        if (RT_SUCCESS(rc))
        {
            rc = utsGadgetHostUsbIpLoadModule("usbip-host.ko");
            if (RT_SUCCESS(rc))
            {
                char aszPort[10];
                char aszPidFile[64];
                const char *apszArgv[5];

                RTStrPrintf(aszPort, RT_ELEMENTS(aszPort), "%u", uPort);
                RTStrPrintf(aszPidFile, RT_ELEMENTS(aszPidFile), "/var/run/usbipd-%u.pid", uPort);
                /* Start the USB/IP server process. */
                apszArgv[0] = "usbipd";
                apszArgv[1] = "--tcp-port";
                apszArgv[2] = aszPort;
                apszArgv[3] = "--pid-file";
                apszArgv[4] = aszPidFile;
                rc = RTProcCreate("usbipd", apszArgv, RTENV_DEFAULT, RTPROC_FLAGS_SEARCH_PATH, &pIf->hProcUsbIp);
                if (RT_SUCCESS(rc))
                {
                    /* We are done setting it up at the moment. */
                }
            }
        }
    }

    return rc;
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnTerm}
 */
static DECLCALLBACK(void) utsGadgetHostUsbIpTerm(PUTSGADGETHOSTTYPEINT pIf)
{
    /* Kill the process. */
    RTProcTerminate(pIf->hProcUsbIp);
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnGadgetAdd}
 */
static DECLCALLBACK(int) utsGadgetHostUsbIpGadgetAdd(PUTSGADGETHOSTTYPEINT pIf, UTSGADGET hGadget)
{
    /* Nothing to do so far. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnGadgetRemove}
 */
static DECLCALLBACK(int) utsGadgetHostUsbIpGadgetRemove(PUTSGADGETHOSTTYPEINT pIf, UTSGADGET hGadget)
{
    /* Nothing to do so far. */
    return VINF_SUCCESS;  
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnGadgetConnect}
 */
static DECLCALLBACK(int) utsGadgetHostUsbIpGadgetConnect(PUTSGADGETHOSTTYPEINT pIf, UTSGADGET hGadget)
{
   return VINF_SUCCESS; 
}


/**
 * @interface_method_impl{UTSGADGETHOSTIF,pfnGadgetDisconnect}
 */
static DECLCALLBACK(int) utsGadgetHostUsbIpGadgetDisconnect(PUTSGADGETHOSTTYPEINT pIf, UTSGADGET hGadget)
{
    return VINF_SUCCESS;
}



/**
 * The gadget host interface callback table.
 */
const UTSGADGETHOSTIF g_UtsGadgetHostIfUsbIp =
{
    /** enmType */
    UTSGADGETHOSTTYPE_USBIP,
    /** pszDesc */
    "UTS USB/IP gadget host",
    /** cbIf */
    sizeof(UTSGADGETHOSTTYPEINT),
    /** pfnInit */
    utsGadgetHostUsbIpInit,
    /** pfnTerm */
    utsGadgetHostUsbIpTerm,
    /** pfnGadgetAdd */
    utsGadgetHostUsbIpGadgetAdd,
    /** pfnGadgetRemove */
    utsGadgetHostUsbIpGadgetRemove,
    /** pfnGadgetConnect */
    utsGadgetHostUsbIpGadgetConnect,
    /** pfnGadgetDisconnect */
    utsGadgetHostUsbIpGadgetDisconnect
};
