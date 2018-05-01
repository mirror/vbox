/* $Id$ */
/** @file
 * VBox serial devices: Host serial driver
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_SERIAL
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmserialifs.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/serialport.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Char driver instance data.
 *
 * @implements  PDMICHARCONNECTOR
 */
typedef struct DRVHOSTSERIAL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMICHARPORT               pDrvCharPort;
    /** Our char interface. */
    PDMICHARCONNECTOR           ICharConnector;
    /** I/O thread. */
    PPDMTHREAD                  pIoThrd;
    /** The serial port handle. */
    RTSERIALPORT                hSerialPort;
    /** the device path */
    char                        *pszDevicePath;


    /** Internal send FIFO queue */
    uint8_t volatile            u8SendByte;
    bool volatile               fSending;
    uint8_t                     Alignment[2];

    /** The read queue. */
    uint8_t                     abReadBuf[256];
    /** Read buffer currently used. */
    size_t                      cbReadBufUsed;
    /** Current offset into the read buffer. */
    uint32_t                    offReadBuf;

    /** Read/write statistics */
    STAMCOUNTER                 StatBytesRead;
    STAMCOUNTER                 StatBytesWritten;
} DRVHOSTSERIAL, *PDRVHOSTSERIAL;



/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostSerialQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTSERIAL  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMICHARCONNECTOR, &pThis->ICharConnector);
    return NULL;
}


/* -=-=-=-=- ICharConnector -=-=-=-=- */

/** @interface_method_impl{PDMICHARCONNECTOR,pfnWrite} */
static DECLCALLBACK(int) drvHostSerialWrite(PPDMICHARCONNECTOR pInterface, const void *pvBuf, size_t cbWrite)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ICharConnector);
    const uint8_t *pbBuffer = (const uint8_t *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    for (uint32_t i = 0; i < cbWrite; i++)
    {
        if (ASMAtomicXchgBool(&pThis->fSending, true))
            return VERR_BUFFER_OVERFLOW;

        pThis->u8SendByte = pbBuffer[i];
        RTSerialPortEvtPollInterrupt(pThis->hSerialPort);
        STAM_COUNTER_INC(&pThis->StatBytesWritten);
    }
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) drvHostSerialSetParameters(PPDMICHARCONNECTOR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ICharConnector);
    RTSERIALPORTCFG Cfg;

    Cfg.uBaudRate = Bps;

    switch (chParity)
    {
        case 'E':
            Cfg.enmParity = RTSERIALPORTPARITY_EVEN;
            break;
        case 'O':
            Cfg.enmParity = RTSERIALPORTPARITY_ODD;
            break;
        case 'N':
            Cfg.enmParity = RTSERIALPORTPARITY_NONE;
            break;
        default:
            AssertMsgFailed(("Unsupported parity setting %c\n", chParity)); /* Should not happen. */
            Cfg.enmParity = RTSERIALPORTPARITY_NONE;
    }

    switch (cDataBits)
    {
        case 5:
            Cfg.enmDataBitCount = RTSERIALPORTDATABITS_5BITS;
            break;
        case 6:
            Cfg.enmDataBitCount = RTSERIALPORTDATABITS_6BITS;
            break;
        case 7:
            Cfg.enmDataBitCount = RTSERIALPORTDATABITS_7BITS;
            break;
        case 8:
            Cfg.enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
            break;
        default:
            AssertMsgFailed(("Unsupported data bit count %u\n", cDataBits)); /* Should not happen. */
            Cfg.enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
    }

    if (cStopBits == 2)
        Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_TWO;
    else
        Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;

    int rc = RTSerialPortCfgSet(pThis->hSerialPort, &Cfg, NULL);
    if (RT_FAILURE(rc))
        LogRelMax(10, ("HostSerial#%u: Failed to change settings to %u:%u%c%u (rc=%Rrc)\n",
                       pThis->pDrvIns->iInstance, Bps, cDataBits, chParity, cStopBits, rc));
    return rc;
}

/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * I/O thread loop.
 *
 * @returns VINF_SUCCESS.
 * @param   pDrvIns     PDM driver instance data.
 * @param   pThread     The PDM thread data.
 */
static DECLCALLBACK(int) drvHostSerialIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (pThis->offReadBuf < pThis->cbReadBufUsed)
        {
            /* Try to send data to the guest. */
            size_t cbProcessed = pThis->cbReadBufUsed - pThis->offReadBuf;
            int rc = pThis->pDrvCharPort->pfnNotifyRead(pThis->pDrvCharPort, &pThis->abReadBuf[pThis->offReadBuf], &cbProcessed);
            if (RT_SUCCESS(rc))
            {
                Assert(cbProcessed); Assert(cbProcessed <= pThis->cbReadBufUsed);
                pThis->offReadBuf += cbProcessed;
                STAM_COUNTER_ADD(&pThis->StatBytesRead, cbProcessed);
            }
            else if (rc != VERR_TIMEOUT)
                LogRelMax(10, ("HostSerial#%d: NotifyRead failed with %Rrc, expect errorneous device behavior.\n",
                               pDrvIns->iInstance, rc));
        }

        uint32_t fEvtFlags = RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED | RTSERIALPORT_EVT_F_BREAK_DETECTED;

        /* Wait until there is room again if there is anyting to send. */
        if (pThis->fSending)
            fEvtFlags |= RTSERIALPORT_EVT_F_DATA_TX;

        /* Try to receive more if there is still room. */
        if (pThis->cbReadBufUsed < sizeof(pThis->abReadBuf))
            fEvtFlags |= RTSERIALPORT_EVT_F_DATA_RX;

        uint32_t fEvtsRecv = 0;
        int rc = RTSerialPortEvtPoll(pThis->hSerialPort, fEvtFlags, &fEvtsRecv,
                                     pThis->offReadBuf < pThis->cbReadBufUsed ? 100 : RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            if (fEvtsRecv & RTSERIALPORT_EVT_F_DATA_TX)
            {
                Assert(pThis->fSending);
                size_t cbWritten = 0;
                uint8_t bSend = pThis->u8SendByte;
                rc = RTSerialPortWriteNB(pThis->hSerialPort, &bSend, 1, &cbWritten);
                if (RT_SUCCESS(rc))
                {
                    Assert(cbWritten == 1);
                    ASMAtomicXchgBool(&pThis->fSending, false);
                }
                else
                    LogRelMax(10, ("HostSerial#%d: Sending data failed even though the serial port is marked as writeable (rc=%Rrc)\n",
                                   pThis->pDrvIns->iInstance, rc));
            }

            if (fEvtsRecv & RTSERIALPORT_EVT_F_DATA_RX)
            {
                /* Move all remaining data in the buffer to the front to make up as much room as possible. */
                if (pThis->offReadBuf)
                {
                    memmove(&pThis->abReadBuf[0], &pThis->abReadBuf[pThis->offReadBuf], pThis->cbReadBufUsed - pThis->offReadBuf);
                    pThis->cbReadBufUsed -= pThis->offReadBuf;
                    pThis->offReadBuf = 0;
                }
                size_t cbToRead = sizeof(pThis->abReadBuf) - pThis->cbReadBufUsed;
                size_t cbRead = 0;
                rc = RTSerialPortReadNB(pThis->hSerialPort, &pThis->abReadBuf[pThis->cbReadBufUsed], cbToRead, &cbRead);
                if (RT_SUCCESS(rc))
                    pThis->cbReadBufUsed += cbRead;
                else
                    LogRelMax(10, ("HostSerial#%d: Reading data failed even though the serial port is marked as readable (rc=%Rrc)\n",
                                   pThis->pDrvIns->iInstance, rc));
            }

            if (fEvtsRecv & RTSERIALPORT_EVT_F_BREAK_DETECTED)
                pThis->pDrvCharPort->pfnNotifyBreak(pThis->pDrvCharPort);

            if (fEvtsRecv & RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED)
            {
                /* The status lines have changed. Notify the device. */
                uint32_t fStsLines = 0;
                rc = RTSerialPortQueryStatusLines(pThis->hSerialPort, &fStsLines);
                if (RT_SUCCESS(rc))
                {
                    uint32_t fPdmStsLines = 0;

                    if (fStsLines & RTSERIALPORT_STS_LINE_DCD)
                        fPdmStsLines |= PDMICHARPORT_STATUS_LINES_DCD;
                    if (fStsLines & RTSERIALPORT_STS_LINE_RI)
                        fPdmStsLines |= PDMICHARPORT_STATUS_LINES_RI;
                    if (fStsLines & RTSERIALPORT_STS_LINE_DSR)
                        fPdmStsLines |= PDMICHARPORT_STATUS_LINES_DSR;
                    if (fStsLines & RTSERIALPORT_STS_LINE_CTS)
                        fPdmStsLines |= PDMICHARPORT_STATUS_LINES_CTS;

                    rc = pThis->pDrvCharPort->pfnNotifyStatusLinesChanged(pThis->pDrvCharPort, fPdmStsLines);
                    if (RT_FAILURE(rc))
                    {
                        /* Notifying device failed, continue but log it */
                        LogRelMax(10, ("HostSerial#%d: Notifying device about changed status lines failed with error %Rrc; continuing.\n",
                                       pDrvIns->iInstance, rc));
                    }
                }
                else
                    LogRelMax(10, ("HostSerial#%d: Getting status lines state failed with error %Rrc; continuing.\n", pDrvIns->iInstance, rc));
            }

            if (fEvtsRecv & RTSERIALPORT_EVT_F_STATUS_LINE_MONITOR_FAILED)
                LogRel(("HostSerial#%d: Status line monitoring failed at a lower level and is disabled\n", pDrvIns->iInstance));
        }
        else if (rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED)
        {
            /* Getting interrupted or running into a timeout are no error conditions. */
            rc = VINF_SUCCESS;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns a VBox status code.
 * @param     pDrvIns     The driver instance.
 * @param     pThread     The send thread.
 */
static DECLCALLBACK(int) drvHostSerialWakeupIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    return RTSerialPortEvtPollInterrupt(pThis->hSerialPort);;
}


/**
 * Set the modem lines.
 *
 * @returns VBox status code
 * @param pInterface        Pointer to the interface structure.
 * @param fRts              Set to true if this control line should be made active.
 * @param fDtr              Set to true if this control line should be made active.
 */
static DECLCALLBACK(int) drvHostSerialSetModemLines(PPDMICHARCONNECTOR pInterface, bool fRts, bool fDtr)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ICharConnector);

    uint32_t fClear = 0;
    uint32_t fSet = 0;

    if (fRts)
        fSet |= RTSERIALPORT_CHG_STS_LINES_F_RTS;
    else
        fClear |= RTSERIALPORT_CHG_STS_LINES_F_RTS;

    if (fDtr)
        fSet |= RTSERIALPORT_CHG_STS_LINES_F_DTR;
    else
        fClear |= RTSERIALPORT_CHG_STS_LINES_F_DTR;

    return RTSerialPortChgStatusLines(pThis->hSerialPort, fClear, fSet);
}


/**
 * Sets the TD line into break condition.
 *
 * @returns VBox status code.
 * @param   pInterface  Pointer to the interface structure containing the called function pointer.
 * @param   fBreak      Set to true to let the device send a break false to put into normal operation.
 * @thread  Any thread.
 */
static DECLCALLBACK(int) drvHostSerialSetBreak(PPDMICHARCONNECTOR pInterface, bool fBreak)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ICharConnector);

    return RTSerialPortChgBreakCondition(pThis->hSerialPort, fBreak);
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
static DECLCALLBACK(void) drvHostSerialDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pThis->hSerialPort != NIL_RTSERIALPORT)
    {
        RTSerialPortClose(pThis->hSerialPort);
        pThis->hSerialPort = NIL_RTSERIALPORT;
    }

    if (pThis->pszDevicePath)
    {
        MMR3HeapFree(pThis->pszDevicePath);
        pThis->pszDevicePath = NULL;
    }
}


/**
 * Construct a char driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostSerialConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF1(fFlags);
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init basic data members and interfaces.
     */
    pThis->hSerialPort   = NIL_RTSERIALPORT;
    pThis->offReadBuf    = 0;
    pThis->cbReadBufUsed = 0;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvHostSerialQueryInterface;
    /* ICharConnector. */
    pThis->ICharConnector.pfnWrite          = drvHostSerialWrite;
    pThis->ICharConnector.pfnSetParameters  = drvHostSerialSetParameters;
    pThis->ICharConnector.pfnSetModemLines  = drvHostSerialSetModemLines;
    pThis->ICharConnector.pfnSetBreak       = drvHostSerialSetBreak;

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfg, "DevicePath", &pThis->pszDevicePath);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Rra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
    uint32_t fOpenFlags =   RTSERIALPORT_OPEN_F_READ
                          | RTSERIALPORT_OPEN_F_WRITE
                          | RTSERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING
                          | RTSERIALPORT_OPEN_F_DETECT_BREAK_CONDITION;
    rc = RTSerialPortOpen(&pThis->hSerialPort, pThis->pszDevicePath, fOpenFlags);
    if (rc == VERR_NOT_SUPPORTED)
    {
        /*
         * For certain devices (or pseudo terminals) status line monitoring does not work
         * so try again without it.
         */
        fOpenFlags &= ~RTSERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING;
        rc = RTSerialPortOpen(&pThis->hSerialPort, pThis->pszDevicePath, fOpenFlags);
    }

    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Could not open host device %s, rc=%Rrc\n", pThis->pszDevicePath, rc));
        switch (rc)
        {
            case VERR_ACCESS_DENIED:
                return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
                                           N_("Cannot open host device '%s' for read/write access. Check the permissions "
                                              "of that device ('/bin/ls -l %s'): Most probably you need to be member "
                                              "of the device group. Make sure that you logout/login after changing "
                                              "the group settings of the current user"),
#else
                                           N_("Cannot open host device '%s' for read/write access. Check the permissions "
                                              "of that device"),
#endif
                                           pThis->pszDevicePath, pThis->pszDevicePath);
           default:
                return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                           N_("Failed to open host device '%s'"),
                                           pThis->pszDevicePath);
        }
    }

    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pThis->pDrvCharPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMICHARPORT);
    if (!pThis->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("HostSerial#%d has no char port interface above"), pDrvIns->iInstance);

    /*
     * Create the I/O thread.
     */
    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pIoThrd, pThis, drvHostSerialIoThread, drvHostSerialWakeupIoThread, 0, RTTHREADTYPE_IO, "SerIo");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create I/O thread"), pDrvIns->iInstance);

    /*
     * Register release statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES,
                           "Nr of bytes written",         "/Devices/HostSerial%d/Written", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead,    STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES,
                           "Nr of bytes read",            "/Devices/HostSerial%d/Read", pDrvIns->iInstance);

    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostSerial =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "Host Serial",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Host serial driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTSERIAL),
    /* pfnConstruct */
    drvHostSerialConstruct,
    /* pfnDestruct */
    drvHostSerialDestruct,
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

