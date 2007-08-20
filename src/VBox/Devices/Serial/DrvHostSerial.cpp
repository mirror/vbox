/** @file
 *
 * VBox stream I/O devices:
 * Host serial driver
 *
 * Contributed by: Alexander Eichner
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_SERIAL
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/semaphore.h>
#include <iprt/file.h>
#include <iprt/alloc.h>

#ifdef RT_OS_LINUX
# include <termios.h>
# include <sys/types.h>
# include <fcntl.h>
# include <string.h>
# include <unistd.h>
#elif defined(RT_OS_WINDOWS)
# include <windows.h>
#endif

#include "Builtins.h"


/** Size of the send fifo queue (in bytes) */
#define CHAR_MAX_SEND_QUEUE             0x80
#define CHAR_MAX_SEND_QUEUE_MASK        0x7f

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Char driver instance data.
 */
typedef struct DRVHOSTSERIAL
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the char port interface of the driver/device above us. */
    PPDMICHARPORT               pDrvCharPort;
    /** Our char interface. */
    PDMICHAR                    IChar;
    /** Flag to notify the receive thread it should terminate. */
    volatile bool               fShutdown;
    /** Receive thread ID. */
    RTTHREAD                    ReceiveThread;
    /** Send thread ID. */
    RTTHREAD                    SendThread;
    /** Send event semephore */
    RTSEMEVENT                  SendSem;

    /** the device path */
    char                        *pszDevicePath;
    /** the device handle */
    RTFILE                      DeviceFile;

    /** Internal send FIFO queue */
    uint8_t                     aSendQueue[CHAR_MAX_SEND_QUEUE];
    uint32_t                    iSendQueueHead;
    uint32_t                    iSendQueueTail;

    /** Read/write statistics */
    STAMCOUNTER                 StatBytesRead;
    STAMCOUNTER                 StatBytesWritten;
} DRVHOSTSERIAL, *PDRVHOSTSERIAL;


/** Converts a pointer to DRVCHAR::IChar to a PDRVHOSTSERIAL. */
#define PDMICHAR_2_DRVHOSTSERIAL(pInterface) ( (PDRVHOSTSERIAL)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTSERIAL, IChar)) )


/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) drvHostSerialQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTSERIAL    pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_CHAR:
            return &pData->IChar;
        default:
            return NULL;
    }
}


/* -=-=-=-=- IChar -=-=-=-=- */

/** @copydoc PDMICHAR::pfnWrite */
static DECLCALLBACK(int) drvHostSerialWrite(PPDMICHAR pInterface, const void *pvBuf, size_t cbWrite)
{
    PDRVHOSTSERIAL pData = PDMICHAR_2_DRVHOSTSERIAL(pInterface);
    const char *pBuffer = (const char *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    for (uint32_t i=0;i<cbWrite;i++)
    {
        uint32_t idx = pData->iSendQueueHead;

        pData->aSendQueue[idx] = pBuffer[i];
        idx = (idx + 1) & CHAR_MAX_SEND_QUEUE_MASK;

        STAM_COUNTER_INC(&pData->StatBytesWritten);
        ASMAtomicXchgU32(&pData->iSendQueueHead, idx);
    }
    RTSemEventSignal(pData->SendSem);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostSerialSetParameters(PPDMICHAR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits)
{
    PDRVHOSTSERIAL pData = PDMICHAR_2_DRVHOSTSERIAL(pInterface);
#ifdef RT_OS_LINUX
    struct termios *termiosSetup;
    int baud_rate;
#elif defined(RT_OS_WINDOWS)
    LPDCB comSetup;
#endif

    LogFlow(("%s: Bps=%u chParity=%c cDataBits=%u cStopBits=%u\n", __FUNCTION__, Bps, chParity, cDataBits, cStopBits));
 
#ifdef RT_OS_LINUX
    termiosSetup = (struct termios *)RTMemTmpAllocZ(sizeof(struct termios));
  
    /* Enable receiver */
    termiosSetup->c_cflag |= (CLOCAL | CREAD);

    switch (Bps) {
        case 50:
            baud_rate = B50;
            break;
        case 75:
            baud_rate = B75;
            break;
        case 110:
            baud_rate = B110;
            break;
        case 134:
            baud_rate = B134;
            break;
        case 150:
            baud_rate = B150;
            break;
        case 200:
            baud_rate = B200;
            break;
        case 300:
            baud_rate = B300;
            break;
        case 600:
            baud_rate = B600;
            break;
        case 1200:
            baud_rate = B1200;
            break;
        case 1800:
            baud_rate = B1800;
            break;
        case 2400:
            baud_rate = B2400;
            break;
        case 4800:
            baud_rate = B4800;
            break;
        case 9600:
            baud_rate = B9600;
            break;
        case 19200:
            baud_rate = B19200;
            break;
        case 38400:
            baud_rate = B38400;
            break;
        case 57600:
            baud_rate = B57600;
            break;
        case 115200:
            baud_rate = B115200;
            break;
        default:
            baud_rate = B9600;
    }

    cfsetispeed(termiosSetup, baud_rate);
    cfsetospeed(termiosSetup, baud_rate);

    switch (chParity) {
        case 'E':
            termiosSetup->c_cflag |= PARENB;
            break;
        case 'O':
            termiosSetup->c_cflag |= (PARENB | PARODD);
            break;
        case 'N':
            break;
        default:
            break;
    }

    switch (cDataBits) {
        case 5:
            termiosSetup->c_cflag |= CS5;
            break;
        case 6:
            termiosSetup->c_cflag |= CS6;
            break;
        case 7:
            termiosSetup->c_cflag |= CS7;
            break;
        case 8:
            termiosSetup->c_cflag |= CS8;
            break;
        default:
            break;
    }

    switch (cStopBits) {
        case 2:
            termiosSetup->c_cflag |= CSTOPB;
        default:
            break;
    }

    /* set serial port to raw input */
    termiosSetup->c_lflag = ~(ICANON | ECHO | ECHOE | ISIG);

    tcsetattr(pData->DeviceFile, TCSANOW, termiosSetup);
    RTMemFree(termiosSetup);
#elif defined(RT_OS_WINDOWS)
    comSetup = (LPDCB)RTMemTmpAllocZ(sizeof(DCB));

    comSetup->DCBlength = sizeof(DCB);

    switch (Bps) {
        case 110:
            comSetup->BaudRate = CBR_110;
            break;
        case 300:
            comSetup->BaudRate = CBR_300;
            break;
        case 600:
            comSetup->BaudRate = CBR_600;
            break;
        case 1200:
            comSetup->BaudRate = CBR_1200;
            break;
        case 2400:
            comSetup->BaudRate = CBR_2400;
            break;
        case 4800:
            comSetup->BaudRate = CBR_4800;
            break;
        case 9600:
            comSetup->BaudRate = CBR_9600;
            break;
        case 14400:
            comSetup->BaudRate = CBR_14400;
            break;
        case 19200:
            comSetup->BaudRate = CBR_19200;
            break;
        case 38400:
            comSetup->BaudRate = CBR_38400;
            break;
        case 57600:
            comSetup->BaudRate = CBR_57600;
            break;
        case 115200:
            comSetup->BaudRate = CBR_115200;
            break;
        default:
            comSetup->BaudRate = CBR_9600;
    }

    comSetup->fBinary = TRUE;
    comSetup->fOutxCtsFlow = FALSE;
    comSetup->fOutxDsrFlow = FALSE;
    comSetup->fDtrControl = DTR_CONTROL_DISABLE;
    comSetup->fDsrSensitivity = FALSE;
    comSetup->fTXContinueOnXoff = TRUE;
    comSetup->fOutX = FALSE;
    comSetup->fInX = FALSE;
    comSetup->fErrorChar = FALSE;
    comSetup->fNull = FALSE;
    comSetup->fRtsControl = RTS_CONTROL_DISABLE;
    comSetup->fAbortOnError = FALSE;
    comSetup->wReserved = 0;
    comSetup->XonLim = 5;
    comSetup->XoffLim = 5;
    comSetup->ByteSize = cDataBits;

    switch (chParity) {
        case 'E':
            comSetup->Parity = EVENPARITY;
            break;
        case 'O':
            comSetup->Parity = ODDPARITY;
            break;
        case 'N':
            comSetup->Parity = NOPARITY;
            break;
        default:
            break;
    }
 
    switch (cStopBits) {
        case 1:
            comSetup->StopBits = ONESTOPBIT;
            break;
        case 2:
            comSetup->StopBits = TWOSTOPBITS;
            break;
        default:
            break;
    }

    comSetup->XonChar = 0;
    comSetup->XoffChar = 0;
    comSetup->ErrorChar = 0;
    comSetup->EofChar = 0;
    comSetup->EvtChar = 0;

    SetCommState((HANDLE)pData->DeviceFile, comSetup);
    RTMemFree(comSetup);
#endif /* RT_OS_WINDOWS */

    return VINF_SUCCESS;
}

/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Send thread loop.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvHostSerialSendLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVHOSTSERIAL pData = (PDRVHOSTSERIAL)pvUser;

    for(;;)
    {
        int rc = RTSemEventWait(pData->SendSem, RT_INDEFINITE_WAIT);
        if (VBOX_FAILURE(rc))
            break;

        /*
         * Write the character to the host device.
         */
        if (!pData->fShutdown)
        {
            while (pData->iSendQueueTail != pData->iSendQueueHead)
            {
                size_t cbProcessed = 1;

                rc = RTFileWrite(pData->DeviceFile, &pData->aSendQueue[pData->iSendQueueTail], cbProcessed, NULL);
                if (VBOX_SUCCESS(rc))
                {
                    Assert(cbProcessed);
                    pData->iSendQueueTail++;
                    pData->iSendQueueTail &= CHAR_MAX_SEND_QUEUE_MASK;
                }
                else if (VBOX_FAILURE(rc))
                {
                    LogFlow(("Write failed with %Vrc; skipping\n", rc));
                    break;
                }
            }
        }
        else
            break;
    }

    pData->SendThread = NIL_RTTHREAD;

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
static DECLCALLBACK(int) drvHostSerialReceiveLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    PDRVHOSTSERIAL pData = (PDRVHOSTSERIAL)pvUser;
    char aBuffer[256], *pBuffer;
    size_t cbRemaining, cbProcessed, cbRead;
    int rc;

    cbRemaining = 0;
    pBuffer = aBuffer;
    while (!pData->fShutdown)
    {
        if (!cbRemaining)
        {
            /* Get block of data from serial device. */
            cbRemaining = sizeof(aBuffer);
            rc = RTFileRead(pData->DeviceFile, aBuffer, cbRemaining, &cbRead);
            if (VBOX_FAILURE(rc))
            {
                LogFlow(("Read failed with %Vrc\n", rc));
                break;
            } else {
                cbRemaining = cbRead;
            }
            pBuffer = aBuffer;
        }
        else
        {
            /* Send data to guest. */
            cbProcessed = cbRemaining;
            rc = pData->pDrvCharPort->pfnNotifyRead(pData->pDrvCharPort, pBuffer, &cbProcessed);
            if (VBOX_SUCCESS(rc))
            {
                Assert(cbProcessed);
                pBuffer += cbProcessed;
                cbRemaining -= cbProcessed;
                STAM_COUNTER_ADD(&pData->StatBytesRead, cbProcessed);
            }
            else if (rc == VERR_TIMEOUT)
            {
                /* Normal case, just means that the guest didn't accept a new
                 * character before the timeout elapsed. Just retry. */
                rc = VINF_SUCCESS;
            }
            else
            {
                LogFlow(("NotifyRead failed with %Vrc\n", rc));
                break;
            }
        }
    }

    pData->ReceiveThread = NIL_RTTHREAD;

    return VINF_SUCCESS;
}


/* -=-=-=-=- driver interface -=-=-=-=- */

/**
 * Construct a char driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed,
 *                      pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to
 *                      obtain the configuration of the driver instance. It's
 *                      also found in pDrvIns->pCfgHandle as it's expected to
 *                      be used frequently in this function.
 */
static DECLCALLBACK(int) drvHostSerialConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /*
     * Init basic data members and interfaces.
     */
    pData->ReceiveThread                    = NIL_RTTHREAD;
    pData->fShutdown                        = false;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvHostSerialQueryInterface;
    /* IChar. */
    pData->IChar.pfnWrite                   = drvHostSerialWrite;
    pData->IChar.pfnSetParameters           = drvHostSerialSetParameters;

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "DevicePath", &pData->pszDevicePath);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Vra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
    rc = RTFileOpen(&pData->DeviceFile, pData->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READWRITE);

    if (VBOX_FAILURE(rc)) {
        pData->DeviceFile = NIL_RTFILE;
        AssertMsgFailed(("Could not open host device %s, rc=%Vrc\n", pData->pszDevicePath, rc));
        switch (rc) {
            case VERR_ACCESS_DENIED:
                return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
#ifdef RT_OS_LINUX
                                           N_("Cannot open host device '%s' for read/write access. Check the permissions "
                                              "of that device ('/bin/ls -l %s'): Most probably you need to be member "
                                              "of the device group. Make sure that you logout/login after changing "
                                              "the group settings of the current user"),
#else
                                           N_("Cannot open host device '%s' for read/write access. Check the permissions "
                                              "of that device"),
#endif
                                           pData->pszDevicePath, pData->pszDevicePath);
           default:
                return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                           N_("Failed to open host device '%s'"), 
                                           pData->pszDevicePath);
        }
    }

    /* Set to non blocking I/O */
#ifdef RT_OS_LINUX
    fcntl(pData->DeviceFile, F_SETFL, O_NONBLOCK);
#elif defined(RT_OS_WINDOWS)
    /* Set the COMMTIMEOUTS to get non blocking I/O */
    COMMTIMEOUTS comTimeout;

    comTimeout.ReadIntervalTimeout         = MAXDWORD; 
    comTimeout.ReadTotalTimeoutMultiplier  = 0;
    comTimeout.ReadTotalTimeoutConstant    = 0;
    comTimeout.WriteTotalTimeoutMultiplier = 0;
    comTimeout.WriteTotalTimeoutConstant   = 0;

    SetCommTimeouts((HANDLE)pData->DeviceFile, &comTimeout);
#endif

    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pData->pDrvCharPort = (PPDMICHARPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_CHAR_PORT);
    if (!pData->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("HostSerial#%d has no char port interface above"), pDrvIns->iInstance);

    rc = RTThreadCreate(&pData->ReceiveThread, drvHostSerialReceiveLoop, (void *)pData, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "Char Receive");
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create receive thread"), pDrvIns->iInstance);

    rc = RTSemEventCreate(&pData->SendSem);
    AssertRC(rc);

    rc = RTThreadCreate(&pData->SendThread, drvHostSerialSendLoop, (void *)pData, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "Serial Send");
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create send thread"), pDrvIns->iInstance);
  
  
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatBytesWritten,    STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes written",         "/Devices/HostSerial%d/Written", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pData->StatBytesRead,       STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes read",            "/Devices/HostSerial%d/Read", pDrvIns->iInstance);

    return VINF_SUCCESS;
}


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
    PDRVHOSTSERIAL     pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    pData->fShutdown = true;
    if (pData->ReceiveThread)
    {
        RTThreadWait(pData->ReceiveThread, 1000, NULL);
        if (pData->ReceiveThread != NIL_RTTHREAD)
            LogRel(("HostSerial%d: receive thread did not terminate\n", pDrvIns->iInstance));
    }

    /* Empty the send queue */
    pData->iSendQueueTail = pData->iSendQueueHead = 0;

    RTSemEventSignal(pData->SendSem);
    RTSemEventDestroy(pData->SendSem);
    pData->SendSem = NIL_RTSEMEVENT;

    if (pData->SendThread)
    {
        RTThreadWait(pData->SendThread, 1000, NULL);
        if (pData->SendThread != NIL_RTTHREAD)
            LogRel(("HostSerial%d: send thread did not terminate\n", pDrvIns->iInstance));
    }
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostSerial =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "Host Serial",
    /* pszDescription */
    "Host serial driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_CHAR,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVHOSTSERIAL),
    /* pfnConstruct */
    drvHostSerialConstruct,
    /* pfnDestruct */
    drvHostSerialDestruct,
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
    /* pfnDetach */
    NULL,
    /** pfnPowerOff */
    NULL
};

