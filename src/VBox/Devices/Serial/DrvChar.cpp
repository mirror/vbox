/* $Id$ */
/** @file
 * Driver that adapts PDMISTREAM into PDMICHARCONNECTOR / PDMICHARPORT.
 *
 * Converts synchronous calls (PDMICHARCONNECTOR::pfnWrite, PDMISTREAM::pfnRead)
 * into asynchronous ones.
 *
 * Note that we don't use a send buffer here to be able to handle
 * dropping of bytes for xmit at device level.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_CHAR
#include <VBox/vmm/pdmdrv.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Converts a pointer to DRVCHAR::ICharConnector to a PDRVCHAR. */
#define PDMICHAR_2_DRVCHAR(pInterface)  RT_FROM_MEMBER(pInterface, DRVCHAR, ICharConnector)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Char driver instance data.
 *
 * @implements PDMICHARCONNECTOR
 */
typedef struct DRVCHAR
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMICHARPORT               pDrvCharPort;
    /** Pointer to the stream interface of the driver below us. */
    PPDMISTREAM                 pDrvStream;
    /** Our char interface. */
    PDMICHARCONNECTOR           ICharConnector;
    /** Flag to notify the receive thread it should terminate. */
    volatile bool               fShutdown;
    /** Receive thread ID. */
    RTTHREAD                    ReceiveThread;
    /** Send thread ID. */
    RTTHREAD                    SendThread;
    /** Send event semaphore */
    RTSEMEVENT                  SendSem;

    /** Internal send FIFO queue */
    uint8_t volatile            u8SendByte;
    bool volatile               fSending;
    uint8_t                     Alignment[2];

    /** Read/write statistics */
    STAMCOUNTER                 StatBytesRead;
    STAMCOUNTER                 StatBytesWritten;
} DRVCHAR, *PDRVCHAR;
AssertCompileMemberAlignment(DRVCHAR, StatBytesRead, 8);




/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvCharQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVCHAR    pThis = PDMINS_2_DATA(pDrvIns, PDRVCHAR);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMICHARCONNECTOR, &pThis->ICharConnector);
    return NULL;
}


/* -=-=-=-=- ICharConnector -=-=-=-=- */

/** @copydoc PDMICHARCONNECTOR::pfnWrite */
static DECLCALLBACK(int) drvCharWrite(PPDMICHARCONNECTOR pInterface, const void *pvBuf, size_t cbWrite)
{
    PDRVCHAR pThis = PDMICHAR_2_DRVCHAR(pInterface);
    const char *pbBuffer = (const char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    for (uint32_t i = 0; i < cbWrite; i++)
    {
        if (ASMAtomicXchgBool(&pThis->fSending, true))
            return VERR_BUFFER_OVERFLOW;

        pThis->u8SendByte = pbBuffer[i];
        RTSemEventSignal(pThis->SendSem);
        STAM_COUNTER_INC(&pThis->StatBytesWritten);
    }
    return VINF_SUCCESS;
}

/** @copydoc PDMICHARCONNECTOR::pfnSetParameters */
static DECLCALLBACK(int) drvCharSetParameters(PPDMICHARCONNECTOR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits)
{
    /*PDRVCHAR pThis = PDMICHAR_2_DRVCHAR(pInterface); - unused*/

    LogFlow(("%s: Bps=%u chParity=%c cDataBits=%u cStopBits=%u\n", __FUNCTION__, Bps, chParity, cDataBits, cStopBits));
    return VINF_SUCCESS;
}


/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Send thread loop - pushes data down thru the driver chain.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvCharSendLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVCHAR pThis = (PDRVCHAR)pvUser;

    int rc = VINF_SUCCESS;
    while (!pThis->fShutdown)
    {
        RTMSINTERVAL cMillies = (rc == VERR_TIMEOUT) ? 50 : RT_INDEFINITE_WAIT;
        rc = RTSemEventWait(pThis->SendSem, cMillies);
        if (    RT_FAILURE(rc)
             && rc != VERR_TIMEOUT)
            break;

        /*
         * Write the character to the attached stream (if present).
         */
        if (    pThis->fShutdown
            ||  !pThis->pDrvStream)
            break;

        size_t cbProcessed = 1;
        uint8_t ch = pThis->u8SendByte;
        rc = pThis->pDrvStream->pfnWrite(pThis->pDrvStream, &ch, &cbProcessed);
        if (RT_SUCCESS(rc))
        {
            ASMAtomicXchgBool(&pThis->fSending, false);
            Assert(cbProcessed == 1);
        }
        else if (rc == VERR_TIMEOUT)
        {
            /* Normal case, just means that the stream didn't accept a new
             * character before the timeout elapsed. Just retry. */

            /* do not change the rc status here, otherwise the (rc == VERR_TIMEOUT) branch
             * in the wait above will never get executed */
            /* rc = VINF_SUCCESS; */
        }
        else
        {
            LogRel(("Write failed with %Rrc; skipping\n", rc));
            break;
        }
    }

    return VINF_SUCCESS;
}

/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvCharReceiveLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVCHAR    pThis = (PDRVCHAR)pvUser;
    char        abBuffer[256];
    char       *pbRemaining = abBuffer;
    size_t      cbRemaining = 0;
    int         rc;

    while (!pThis->fShutdown)
    {
        if (!cbRemaining)
        {
            /* Get block of data from stream driver. */
            if (pThis->pDrvStream)
            {
                pbRemaining = abBuffer;
                cbRemaining = sizeof(abBuffer);
                rc = pThis->pDrvStream->pfnRead(pThis->pDrvStream, abBuffer, &cbRemaining);
                if (RT_FAILURE(rc))
                {
                    LogFlow(("Read failed with %Rrc\n", rc));
                    break;
                }
            }
            else
                RTThreadSleep(100);
        }
        else
        {
            /* Send data to guest. */
            size_t cbProcessed = cbRemaining;
            rc = pThis->pDrvCharPort->pfnNotifyRead(pThis->pDrvCharPort, pbRemaining, &cbProcessed);
            if (RT_SUCCESS(rc))
            {
                Assert(cbProcessed);
                pbRemaining += cbProcessed;
                cbRemaining -= cbProcessed;
                STAM_COUNTER_ADD(&pThis->StatBytesRead, cbProcessed);
            }
            else if (rc == VERR_TIMEOUT)
            {
                /* Normal case, just means that the guest didn't accept a new
                 * character before the timeout elapsed. Just retry. */
                rc = VINF_SUCCESS;
            }
            else
            {
                LogFlow(("NotifyRead failed with %Rrc\n", rc));
                break;
            }
        }
    }

    return VINF_SUCCESS;
}

/**
 * Set the modem lines.
 *
 * @returns VBox status code
 * @param pInterface        Pointer to the interface structure.
 * @param RequestToSend     Set to true if this control line should be made active.
 * @param DataTerminalReady Set to true if this control line should be made active.
 */
static DECLCALLBACK(int) drvCharSetModemLines(PPDMICHARCONNECTOR pInterface, bool RequestToSend, bool DataTerminalReady)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}

/**
 * Sets the TD line into break condition.
 *
 * @returns VBox status code.
 * @param   pInterface  Pointer to the interface structure containing the called function pointer.
 * @param   fBreak      Set to true to let the device send a break false to put into normal operation.
 * @thread  Any thread.
 */
static DECLCALLBACK(int) drvCharSetBreak(PPDMICHARCONNECTOR pInterface, bool fBreak)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}

/* -=-=-=-=- driver interface -=-=-=-=- */

/**
 * Destruct a char driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvCharDestruct(PPDMDRVINS pDrvIns)
{
    PDRVCHAR pThis = PDMINS_2_DATA(pDrvIns, PDRVCHAR);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    /*
     * Tell the threads to shut down.
     */
    pThis->fShutdown = true;
    if (pThis->SendSem != NIL_RTSEMEVENT)
    {
        RTSemEventSignal(pThis->SendSem);
    }

    /*
     * Wait for the threads.
     * ASSUMES that PDM destroys the driver chain from the bottom and up.
     */
    if (pThis->ReceiveThread != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(pThis->ReceiveThread, 30000, NULL);
        if (RT_SUCCESS(rc))
            pThis->ReceiveThread = NIL_RTTHREAD;
        else
            LogRel(("Char%d: receive thread did not terminate (%Rrc)\n", pDrvIns->iInstance, rc));
    }

    if (pThis->SendThread != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(pThis->SendThread, 30000, NULL);
        if (RT_SUCCESS(rc))
            pThis->SendThread = NIL_RTTHREAD;
        else
            LogRel(("Char%d: send thread did not terminate (%Rrc)\n", pDrvIns->iInstance, rc));
    }

    if (pThis->SendSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pThis->SendSem);
        pThis->SendSem = NIL_RTSEMEVENT;
    }
}


/**
 * Construct a char driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvCharConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVCHAR pThis = PDMINS_2_DATA(pDrvIns, PDRVCHAR);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init basic data members and interfaces.
     */
    pThis->fShutdown                        = false;
    pThis->ReceiveThread                    = NIL_RTTHREAD;
    pThis->SendThread                       = NIL_RTTHREAD;
    pThis->SendSem                          = NIL_RTSEMEVENT;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvCharQueryInterface;
    /* ICharConnector. */
    pThis->ICharConnector.pfnWrite          = drvCharWrite;
    pThis->ICharConnector.pfnSetParameters  = drvCharSetParameters;
    pThis->ICharConnector.pfnSetModemLines  = drvCharSetModemLines;
    pThis->ICharConnector.pfnSetBreak       = drvCharSetBreak;

    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pThis->pDrvCharPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMICHARPORT);
    if (!pThis->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("Char#%d has no char port interface above"), pDrvIns->iInstance);

    /*
     * Attach driver below and query its stream interface.
     */
    PPDMIBASE pBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBase);
    if (RT_FAILURE(rc))
        return rc; /* Don't call PDMDrvHlpVMSetError here as we assume that the driver already set an appropriate error */
    pThis->pDrvStream = PDMIBASE_QUERY_INTERFACE(pBase, PDMISTREAM);
    if (!pThis->pDrvStream)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW, RT_SRC_POS, N_("Char#%d has no stream interface below"), pDrvIns->iInstance);

    /*
     * Don't start the receive thread if the driver doesn't support reading
     */
    if (pThis->pDrvStream->pfnRead)
    {
        rc = RTThreadCreate(&pThis->ReceiveThread, drvCharReceiveLoop, (void *)pThis, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "CharRecv");
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Char#%d cannot create receive thread"), pDrvIns->iInstance);
    }

    rc = RTSemEventCreate(&pThis->SendSem);
    AssertRCReturn(rc, rc);

    rc = RTThreadCreate(&pThis->SendThread, drvCharSendLoop, (void *)pThis, 0,
                        RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "CharSend");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("Char#%d cannot create send thread"), pDrvIns->iInstance);


    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten,    STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes written",         "/Devices/Char%d/Written", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead,       STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes read",            "/Devices/Char%d/Read", pDrvIns->iInstance);

    return VINF_SUCCESS;
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvChar =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "Char",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Generic char driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVCHAR),
    /* pfnConstruct */
    drvCharConstruct,
    /* pfnDestruct */
    drvCharDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
