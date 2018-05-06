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
 * @implements  PDMISERIALCONNECTOR
 */
typedef struct DRVHOSTSERIAL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the serial port interface of the driver/device above us. */
    PPDMISERIALPORT             pDrvSerialPort;
    /** Our serial interface. */
    PDMISERIALCONNECTOR         ISerialConnector;
    /** I/O thread. */
    PPDMTHREAD                  pIoThrd;
    /** The serial port handle. */
    RTSERIALPORT                hSerialPort;
    /** the device path */
    char                        *pszDevicePath;

    /** Amount of data available for sending from the device/driver above. */
    volatile size_t             cbAvailWr;
    /** Small send buffer. */
    uint8_t                     abTxBuf[16];
    /** Amount of data in the buffer. */
    size_t                      cbTxUsed;

    /** The read queue. */
    uint8_t                     abReadBuf[256];
    /** Current offset to write to next. */
    volatile uint32_t           offWrite;
    /** Current offset into the read buffer. */
    volatile uint32_t           offRead;
    /** Current amount of data in the buffer. */
    volatile size_t             cbReadBuf;

    /** Read/write statistics */
    STAMCOUNTER                 StatBytesRead;
    STAMCOUNTER                 StatBytesWritten;
} DRVHOSTSERIAL, *PDRVHOSTSERIAL;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Returns number of bytes free in the read buffer and pointer to the start of the free space
 * in the read buffer.
 *
 * @returns Number of bytes free in the buffer.
 * @param   pThis               The host serial driver instance.
 * @param   ppv                 Where to return the pointer if there is still free space.
 */
DECLINLINE(size_t) drvHostSerialReadBufGetWrite(PDRVHOSTSERIAL pThis, void **ppv)
{
    if (ppv)
        *ppv = &pThis->abReadBuf[pThis->offWrite];

    size_t cbFree = sizeof(pThis->abReadBuf) - ASMAtomicReadZ(&pThis->cbReadBuf);
    if (cbFree)
        cbFree = RT_MIN(cbFree, sizeof(pThis->abReadBuf) - pThis->offWrite);

    return cbFree;
}


/**
 * Returns number of bytes used in the read buffer and pointer to the next byte to read.
 *
 * @returns Number of bytes free in the buffer.
 * @param   pThis               The host serial driver instance.
 * @param   ppv                 Where to return the pointer to the next data to read.
 */
DECLINLINE(size_t) drvHostSerialReadBufGetRead(PDRVHOSTSERIAL pThis, void **ppv)
{
    if (ppv)
        *ppv = &pThis->abReadBuf[pThis->offRead];

    size_t cbUsed = ASMAtomicReadZ(&pThis->cbReadBuf);
    if (cbUsed)
        cbUsed = RT_MIN(cbUsed, sizeof(pThis->abReadBuf) - pThis->offRead);

    return cbUsed;
}


/**
 * Advances the write position of the read buffer by the given amount of bytes.
 *
 * @returns nothing.
 * @param   pThis               The host serial driver instance.
 * @param   cbAdv               Number of bytes to advance.
 */
DECLINLINE(void) drvHostSerialReadBufWriteAdv(PDRVHOSTSERIAL pThis, size_t cbAdv)
{
    uint32_t offWrite = ASMAtomicReadU32(&pThis->offWrite);
    offWrite = (offWrite + cbAdv) % sizeof(pThis->abReadBuf);
    ASMAtomicWriteU32(&pThis->offWrite, offWrite);
    ASMAtomicAddZ(&pThis->cbReadBuf, cbAdv);
}


/**
 * Advances the read position of the read buffer by the given amount of bytes.
 *
 * @returns nothing.
 * @param   pThis               The host serial driver instance.
 * @param   cbAdv               Number of bytes to advance.
 */
DECLINLINE(void) drvHostSerialReadBufReadAdv(PDRVHOSTSERIAL pThis, size_t cbAdv)
{
    uint32_t offRead = ASMAtomicReadU32(&pThis->offRead);
    offRead = (offRead + cbAdv) % sizeof(pThis->abReadBuf);
    ASMAtomicWriteU32(&pThis->offRead, offRead);
    ASMAtomicSubZ(&pThis->cbReadBuf, cbAdv);
}


/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostSerialQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTSERIAL  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISERIALCONNECTOR, &pThis->ISerialConnector);
    return NULL;
}


/* -=-=-=-=- ISerialConnector -=-=-=-=- */

/** @interface_method_impl{PDMISERIALCONNECTOR,pfnDataAvailWrNotify} */
static DECLCALLBACK(int) drvHostSerialDataAvailWrNotify(PPDMISERIALCONNECTOR pInterface, size_t cbAvail)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);

    int rc = VINF_SUCCESS;
    size_t cbAvailOld = ASMAtomicAddZ(&pThis->cbAvailWr, cbAvail);
    if (!cbAvailOld)
        rc = RTSerialPortEvtPollInterrupt(pThis->hSerialPort);

    return rc;
}


/**
 * @interface_method_impl{PDMISERIALCONNECTOR,pfnReadRdr}
 */
static DECLCALLBACK(int) drvHostSerialReadRdr(PPDMISERIALCONNECTOR pInterface, void *pvBuf,
                                              size_t cbRead, size_t *pcbRead)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);
    int rc = VINF_SUCCESS;
    uint8_t *pbDst = (uint8_t *)pvBuf;
    size_t cbReadAll = 0;

    do
    {
        void *pvSrc = NULL;
        size_t cbThisRead = RT_MIN(drvHostSerialReadBufGetRead(pThis, &pvSrc), cbRead);
        if (cbThisRead)
        {
            memcpy(pbDst, pvSrc, cbThisRead);
            cbRead    -= cbThisRead;
            pbDst     += cbThisRead;
            cbReadAll += cbThisRead;
            drvHostSerialReadBufReadAdv(pThis, cbThisRead);
        }
        else
            break;
    } while (cbRead > 0);

    *pcbRead = cbReadAll;
    /* Kick the I/O thread if there is nothing to read to recalculate the poll flags. */
    if (!drvHostSerialReadBufGetRead(pThis, NULL))
        rc = RTSerialPortEvtPollInterrupt(pThis->hSerialPort);

    STAM_COUNTER_ADD(&pThis->StatBytesRead, cbReadAll);
    return rc;
}


/**
 * @interface_method_impl{PDMISERIALCONNECTOR,pfnChgParams}
 */
static DECLCALLBACK(int) drvHostSerialChgParams(PPDMISERIALCONNECTOR pInterface, uint32_t uBps,
                                                PDMSERIALPARITY enmParity, unsigned cDataBits,
                                                PDMSERIALSTOPBITS enmStopBits)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);
    RTSERIALPORTCFG Cfg;

    Cfg.uBaudRate = uBps;

    switch (enmParity)
    {
        case PDMSERIALPARITY_EVEN:
            Cfg.enmParity = RTSERIALPORTPARITY_EVEN;
            break;
        case PDMSERIALPARITY_ODD:
            Cfg.enmParity = RTSERIALPORTPARITY_ODD;
            break;
        case PDMSERIALPARITY_NONE:
            Cfg.enmParity = RTSERIALPORTPARITY_NONE;
            break;
        case PDMSERIALPARITY_MARK:
            Cfg.enmParity = RTSERIALPORTPARITY_MARK;
            break;
        case PDMSERIALPARITY_SPACE:
            Cfg.enmParity = RTSERIALPORTPARITY_SPACE;
            break;
        default:
            AssertMsgFailed(("Unsupported parity setting %d\n", enmParity)); /* Should not happen. */
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

    switch (enmStopBits)
    {
        case PDMSERIALSTOPBITS_ONE:
            Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;
            break;
        case PDMSERIALSTOPBITS_ONEPOINTFIVE:
            Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONEPOINTFIVE;
            break;
        case PDMSERIALSTOPBITS_TWO:
            Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_TWO;
            break;
        default:
            AssertMsgFailed(("Unsupported stop bit count %d\n", enmStopBits)); /* Should not happen. */
            Cfg.enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;
    }

    return RTSerialPortCfgSet(pThis->hSerialPort, &Cfg, NULL);
}


/**
 * @interface_method_impl{PDMISERIALCONNECTOR,pfnChgModemLines}
 */
static DECLCALLBACK(int) drvHostSerialChgModemLines(PPDMISERIALCONNECTOR pInterface, bool fRts, bool fDtr)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);

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
 * @interface_method_impl{PDMISERIALCONNECTOR,pfnChgBrk}
 */
static DECLCALLBACK(int) drvHostSerialChgBrk(PPDMISERIALCONNECTOR pInterface, bool fBrk)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);

    return RTSerialPortChgBreakCondition(pThis->hSerialPort, fBrk);
}


/**
 * @interface_method_impl{PDMISERIALCONNECTOR,pfnQueryStsLines}
 */
static DECLCALLBACK(int) drvHostSerialQueryStsLines(PPDMISERIALCONNECTOR pInterface, uint32_t *pfStsLines)
{
    PDRVHOSTSERIAL pThis = RT_FROM_MEMBER(pInterface, DRVHOSTSERIAL, ISerialConnector);

    return RTSerialPortQueryStatusLines(pThis->hSerialPort, pfStsLines);
}


/* -=-=-=-=- I/O thread -=-=-=-=- */

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
        uint32_t fEvtFlags = RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED | RTSERIALPORT_EVT_F_BREAK_DETECTED;

        /* Wait until there is room again if there is anyting to send. */
        if (   pThis->cbAvailWr
            || pThis->cbTxUsed)
            fEvtFlags |= RTSERIALPORT_EVT_F_DATA_TX;

        /* Try to receive more if there is still room. */
        if (drvHostSerialReadBufGetWrite(pThis, NULL) > 0)
            fEvtFlags |= RTSERIALPORT_EVT_F_DATA_RX;

        uint32_t fEvtsRecv = 0;
        int rc = RTSerialPortEvtPoll(pThis->hSerialPort, fEvtFlags, &fEvtsRecv, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc))
        {
            if (fEvtsRecv & RTSERIALPORT_EVT_F_DATA_TX)
            {
                if (pThis->cbAvailWr)
                {
                    /* Stuff as much data into the TX buffer as we can. */
                    size_t cbToFetch = RT_ELEMENTS(pThis->abTxBuf) - pThis->cbTxUsed;
                    size_t cbFetched = 0;
                    rc = pThis->pDrvSerialPort->pfnReadWr(pThis->pDrvSerialPort, &pThis->abTxBuf[pThis->cbTxUsed], cbToFetch,
                                                          &cbFetched);
                    AssertRC(rc);

                    ASMAtomicSubZ(&pThis->cbAvailWr, cbFetched);
                    pThis->cbTxUsed += cbFetched;
                }

                size_t cbProcessed = 0;
                rc = RTSerialPortWriteNB(pThis->hSerialPort, &pThis->abTxBuf[0], pThis->cbTxUsed, &cbProcessed);
                if (RT_SUCCESS(rc))
                {
                    pThis->cbTxUsed -= cbProcessed;
                    if (pThis->cbTxUsed)
                    {
                        /* Move the data in the TX buffer to the front to fill the end again. */
                        memmove(&pThis->abTxBuf[0], &pThis->abTxBuf[cbProcessed], pThis->cbTxUsed);
                    }
                    else
                        pThis->pDrvSerialPort->pfnDataSentNotify(pThis->pDrvSerialPort);
                    STAM_COUNTER_ADD(&pThis->StatBytesWritten, cbProcessed);
                }
                else
                {
                    LogRelMax(10, ("HostSerial#%d: Sending data failed even though the serial port is marked as writeable (rc=%Rrc)\n",
                                   pThis->pDrvIns->iInstance, rc));
                    break;
                }
            }

            if (fEvtsRecv & RTSERIALPORT_EVT_F_DATA_RX)
            {
                void *pvDst = NULL;
                size_t cbToRead = drvHostSerialReadBufGetWrite(pThis, &pvDst);
                size_t cbRead = 0;
                rc = RTSerialPortReadNB(pThis->hSerialPort, pvDst, cbToRead, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    drvHostSerialReadBufWriteAdv(pThis, cbRead);
                    /* Notify the device/driver above. */
                    rc = pThis->pDrvSerialPort->pfnDataAvailRdrNotify(pThis->pDrvSerialPort, cbRead);
                    AssertRC(rc);
                }
                else
                    LogRelMax(10, ("HostSerial#%d: Reading data failed even though the serial port is marked as readable (rc=%Rrc)\n",
                                   pThis->pDrvIns->iInstance, rc));
            }

            if (fEvtsRecv & RTSERIALPORT_EVT_F_BREAK_DETECTED)
                pThis->pDrvSerialPort->pfnNotifyBrk(pThis->pDrvSerialPort);

            if (fEvtsRecv & RTSERIALPORT_EVT_F_STATUS_LINE_CHANGED)
            {
                /* The status lines have changed. Notify the device. */
                uint32_t fStsLines = 0;
                rc = RTSerialPortQueryStatusLines(pThis->hSerialPort, &fStsLines);
                if (RT_SUCCESS(rc))
                {
                    uint32_t fPdmStsLines = 0;

                    if (fStsLines & RTSERIALPORT_STS_LINE_DCD)
                        fPdmStsLines |= PDMISERIALPORT_STS_LINE_DCD;
                    if (fStsLines & RTSERIALPORT_STS_LINE_RI)
                        fPdmStsLines |= PDMISERIALPORT_STS_LINE_RI;
                    if (fStsLines & RTSERIALPORT_STS_LINE_DSR)
                        fPdmStsLines |= PDMISERIALPORT_STS_LINE_DSR;
                    if (fStsLines & RTSERIALPORT_STS_LINE_CTS)
                        fPdmStsLines |= PDMISERIALPORT_STS_LINE_CTS;

                    rc = pThis->pDrvSerialPort->pfnNotifyStsLinesChanged(pThis->pDrvSerialPort, fPdmStsLines);
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

    return RTSerialPortEvtPollInterrupt(pThis->hSerialPort);
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
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

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
    pThis->pDrvIns                               = pDrvIns;
    pThis->hSerialPort                           = NIL_RTSERIALPORT;
    pThis->cbAvailWr                             = 0;
    pThis->cbTxUsed                              = 0;
    pThis->offWrite                              = 0;
    pThis->offRead                               = 0;
    pThis->cbReadBuf                             = 0;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface             = drvHostSerialQueryInterface;
    /* ISerialConnector. */
    pThis->ISerialConnector.pfnDataAvailWrNotify = drvHostSerialDataAvailWrNotify;
    pThis->ISerialConnector.pfnReadRdr           = drvHostSerialReadRdr;
    pThis->ISerialConnector.pfnChgParams         = drvHostSerialChgParams;
    pThis->ISerialConnector.pfnChgModemLines     = drvHostSerialChgModemLines;
    pThis->ISerialConnector.pfnChgBrk            = drvHostSerialChgBrk;
    pThis->ISerialConnector.pfnQueryStsLines     = drvHostSerialQueryStsLines;

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
     * Get the ISerialPort interface of the above driver/device.
     */
    pThis->pDrvSerialPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMISERIALPORT);
    if (!pThis->pDrvSerialPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("HostSerial#%d has no serial port interface above"), pDrvIns->iInstance);

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

