/** $Id$ */
/** @file
 * VBox stream I/O devices: Host serial driver
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
# include <errno.h>
# include <termios.h>
# include <sys/types.h>
# include <fcntl.h>
# include <string.h>
# include <unistd.h>
# include <sys/poll.h>
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
    /** Receive thread. */
    PPDMTHREAD                  pReceiveThread;
    /** Send thread. */
    PPDMTHREAD                  pSendThread;
    /** Send event semephore */
    RTSEMEVENT                  SendSem;

    /** the device path */
    char                        *pszDevicePath;

#ifdef RT_OS_LINUX
    /** the device handle */
    RTFILE                      DeviceFile;
    /** The read end of the control pipe */
    RTFILE                      WakeupPipeR;
    /** The write end of the control pipe */
    RTFILE                      WakeupPipeW;
#elif RT_OS_WINDOWS
    /** the device handle */
    HANDLE                      hDeviceFile;
    /** The event semaphore for waking up the receive thread */
    HANDLE                      hHaltEventSem;
    /** The event semaphore for overlapped receiving */
    HANDLE                      hEventReceive;
    /** The event semaphore for overlapped sending */
    HANDLE                      hEventSend;
#endif

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
    const uint8_t *pbBuffer = (const uint8_t *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    for (uint32_t i=0;i<cbWrite;i++)
    {
        uint32_t idx = pData->iSendQueueHead;

        pData->aSendQueue[idx] = pbBuffer[i];
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

    SetCommState(pData->hDeviceFile, comSetup);
    RTMemFree(comSetup);
#endif /* RT_OS_WINDOWS */

    return VINF_SUCCESS;
}

/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Send thread loop.
 *
 * @returns VINF_SUCCESS.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvHostSerialSendThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    HANDLE haWait[2];
    haWait[0] = pData->hEventSend;
    haWait[1] = pData->hHaltEventSem;
#endif

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        int rc = RTSemEventWait(pData->SendSem, RT_INDEFINITE_WAIT);
        if (VBOX_FAILURE(rc))
            break;

        /*
         * Write the character to the host device.
         */
        while (   pThread->enmState == PDMTHREADSTATE_RUNNING
               && pData->iSendQueueTail != pData->iSendQueueHead)
        {
            unsigned cbProcessed = 1;

#if defined(RT_OS_LINUX)

            rc = RTFileWrite(pData->DeviceFile, &pData->aSendQueue[pData->iSendQueueTail], cbProcessed, NULL);

#elif defined(RT_OS_WINDOWS)

            DWORD cbBytesWritten;
            OVERLAPPED overlapped;
            memset(&overlapped, 0, sizeof(overlapped));
            overlapped.hEvent = pData->hEventSend;

            if (!WriteFile(pData->hDeviceFile, &pData->aSendQueue[pData->iSendQueueTail], cbProcessed, &cbBytesWritten, &overlapped))
            {
                DWORD dwRet = GetLastError();
                if (dwRet == ERROR_IO_PENDING)
                {
                    /*
                     * write blocked, wait ...
                     */
                    dwRet = WaitForMultipleObjects(2, haWait, FALSE, INFINITE);
                    if (dwRet != WAIT_OBJECT_0)
                        break;
                }
                else
                    rc = RTErrConvertFromWin32(dwRet);
            }

#endif

            if (VBOX_SUCCESS(rc))
            {
                Assert(cbProcessed);
                pData->iSendQueueTail++;
                pData->iSendQueueTail &= CHAR_MAX_SEND_QUEUE_MASK;
            }
            else if (VBOX_FAILURE(rc))
            {
                LogRel(("HostSerial#%d: Serial Write failed with %Vrc; terminating send thread\n", pDrvIns->iInstance, rc));
                return VINF_SUCCESS;
            }
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
static DECLCALLBACK(int) drvHostSerialWakeupSendThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);
    int rc;

    rc = RTSemEventSignal(pData->SendSem);
    if (RT_FAILURE(rc))
        return rc;

#ifdef RT_OS_WINDOWS
    if (!SetEvent(pData->hHaltEventSem))
        return RTErrConvertFromWin32(GetLastError());
#endif

    return VINF_SUCCESS;
}

/* -=-=-=-=- receive thread -=-=-=-=- */

/**
 * Receive thread loop.
 *
 * This thread pushes data from the host serial device up the driver
 * chain toward the serial device.
 *
 * @returns VINF_SUCCESS.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvHostSerialReceiveThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);
    uint8_t abBuffer[256];
    uint8_t *pbBuffer = NULL;
    size_t cbRemaining = 0; /* start by reading host data */
    int rc = VINF_SUCCESS;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    HANDLE haWait[2];
    haWait[0] = pData->hEventReceive;
    haWait[1] = pData->hHaltEventSem;
#endif

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (!cbRemaining)
        {
            /* Get a block of data from the host serial device. */

#if defined(RT_OS_LINUX)

            size_t cbRead;
            struct pollfd aFDs[2];
            aFDs[0].fd      = pData->DeviceFile;
            aFDs[0].events  = POLLIN;
            aFDs[0].revents = 0;
            aFDs[1].fd      = pData->WakeupPipeR;
            aFDs[1].events  = POLLIN | POLLERR | POLLHUP;
            aFDs[1].revents = 0;
            rc = poll(aFDs, ELEMENTS(aFDs), -1);
            if (rc < 0)
            {
                /* poll failed for whatever reason */
                break;
            }
            /* this might have changed in the meantime */
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
                break;
            if (rc > 0 && aFDs[1].revents)
            {
                if (aFDs[1].revents & (POLLHUP | POLLERR | POLLNVAL))
                    break;
                /* notification to terminate -- drain the pipe */
                char ch;
                size_t cbRead;
                RTFileRead(pData->WakeupPipeR, &ch, 1, &cbRead);
                continue;
            }
            rc = RTFileRead(pData->DeviceFile, abBuffer, sizeof(abBuffer), &cbRead);
            if (VBOX_FAILURE(rc))
            {
                LogRel(("HostSerial#%d: Read failed with %Vrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                break;
            }
            cbRemaining = cbRead;

#elif defined(RT_OS_WINDOWS)

            DWORD dwEventMask = 0;
            DWORD dwNumberOfBytesTransferred;
            OVERLAPPED overlapped;

            memset(&overlapped, 0, sizeof(overlapped));
            overlapped.hEvent = pData->hEventReceive;

            if (!WaitCommEvent(pData->hDeviceFile, &dwEventMask, &overlapped))
            {
                DWORD dwRet = GetLastError();
                if (dwRet == ERROR_IO_PENDING)
                {
                    dwRet = WaitForMultipleObjects(2, haWait, FALSE, INFINITE);
                    if (dwRet != WAIT_OBJECT_0)
                    {
                        /* notification to terminate */
                        break;
                    }
                }
                else
                {
                    LogRel(("HostSerial#%d: Wait failed with error %Vrc; terminating the worker thread.\n", pDrvIns->iInstance, RTErrConvertFromWin32(dwRet)));
                    break;
                }
            }
            /* this might have changed in the meantime */
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
                break;
            if (!ReadFile(pData->hDeviceFile, abBuffer, sizeof(abBuffer), &dwNumberOfBytesTransferred, &overlapped))
            {
                LogRel(("HostSerial#%d: Read failed with error %Vrc; terminating the worker thread.\n", pDrvIns->iInstance, RTErrConvertFromWin32(GetLastError())));
                break;
            }
            cbRemaining = dwNumberOfBytesTransferred;

#endif

            Log(("Read %d bytes.\n", cbRemaining));
            pbBuffer = abBuffer;
        }
        else
        {
            /* Send data to the guest. */
            size_t cbProcessed = cbRemaining;
            rc = pData->pDrvCharPort->pfnNotifyRead(pData->pDrvCharPort, pbBuffer, &cbProcessed);
            if (VBOX_SUCCESS(rc))
            {
                Assert(cbProcessed); Assert(cbProcessed <= cbRemaining);
                pbBuffer += cbProcessed;
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
                LogRel(("HostSerial#%d: NotifyRead failed with %Vrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                break;
            }
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
static DECLCALLBACK(int) drvHostSerialWakeupReceiveThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);
#ifdef RT_OS_LINUX
    return RTFileWrite(pData->WakeupPipeW, "", 1, NULL);
#elif defined(RT_OS_WINDOWS)
    if (!SetEvent(pData->hHaltEventSem))
        return RTErrConvertFromWin32(GetLastError());
    return VINF_SUCCESS;
#else
# error adapt me!
#endif
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
#ifdef RT_OS_LINUX
    pData->DeviceFile  = NIL_RTFILE;
    pData->WakeupPipeR = NIL_RTFILE;
    pData->WakeupPipeW = NIL_RTFILE;
#endif
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
#ifdef RT_OS_WINDOWS

    pData->hHaltEventSem = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pData->hHaltEventSem != NULL, VERR_NO_MEMORY);

    pData->hEventReceive = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pData->hEventReceive != NULL, VERR_NO_MEMORY);

    pData->hEventSend = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pData->hEventSend != NULL, VERR_NO_MEMORY);

    HANDLE hFile = CreateFile(pData->pszDevicePath,
                              GENERIC_READ | GENERIC_WRITE,
                              0, // must be opened with exclusive access
                              NULL, // no SECURITY_ATTRIBUTES structure
                              OPEN_EXISTING, // must use OPEN_EXISTING
                              FILE_FLAG_OVERLAPPED, // overlapped I/O
                              NULL); // no template file
    if (hFile == INVALID_HANDLE_VALUE)
        rc = RTErrConvertFromWin32(GetLastError());
    else
    {
        pData->hDeviceFile = hFile;
        /* for overlapped read */
        if (!SetCommMask(hFile, EV_RXCHAR))
        {
            LogRel(("HostSerial#%d: SetCommMask failed with error %d.\n", pDrvIns->iInstance, GetLastError()));
            return VERR_FILE_IO_ERROR;
        }
        rc = VINF_SUCCESS;
    }

#else

    pData->DeviceFile = NIL_RTFILE;
    rc = RTFileOpen(&pData->DeviceFile, pData->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READWRITE);

#endif

    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Could not open host device %s, rc=%Vrc\n", pData->pszDevicePath, rc));
        switch (rc)
        {
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
    int aFDs[2];
    if (pipe(aFDs) != 0)
    {
        int rc = RTErrConvertFromErrno(errno);
        AssertRC(rc);
        return rc;
    }
    pData->WakeupPipeR = aFDs[0];
    pData->WakeupPipeW = aFDs[1];

#elif defined(RT_OS_WINDOWS)

    /* Set the COMMTIMEOUTS to get non blocking I/O */
    COMMTIMEOUTS comTimeout;

    comTimeout.ReadIntervalTimeout         = MAXDWORD;
    comTimeout.ReadTotalTimeoutMultiplier  = 0;
    comTimeout.ReadTotalTimeoutConstant    = 0;
    comTimeout.WriteTotalTimeoutMultiplier = 0;
    comTimeout.WriteTotalTimeoutConstant   = 0;

    SetCommTimeouts(pData->hDeviceFile, &comTimeout);

#endif

    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pData->pDrvCharPort = (PPDMICHARPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_CHAR_PORT);
    if (!pData->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("HostSerial#%d has no char port interface above"), pDrvIns->iInstance);

    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pData->pReceiveThread, pData, drvHostSerialReceiveThread, drvHostSerialWakeupReceiveThread, 0, RTTHREADTYPE_IO, "Char Receive");
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create receive thread"), pDrvIns->iInstance);

    rc = RTSemEventCreate(&pData->SendSem);
    AssertRC(rc);

    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pData->pSendThread, pData, drvHostSerialSendThread, drvHostSerialWakeupSendThread, 0, RTTHREADTYPE_IO, "Serial Send");
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
    PDRVHOSTSERIAL pData = PDMINS2DATA(pDrvIns, PDRVHOSTSERIAL);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /* Empty the send queue */
    pData->iSendQueueTail = pData->iSendQueueHead = 0;

    RTSemEventDestroy(pData->SendSem);
    pData->SendSem = NIL_RTSEMEVENT;

#if defined(RT_OS_LINUX)

    if (pData->WakeupPipeW != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->WakeupPipeW);
        AssertRC(rc);
        pData->WakeupPipeW = NIL_RTFILE;
    }
    if (pData->WakeupPipeR != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->WakeupPipeR);
        AssertRC(rc);
        pData->WakeupPipeR = NIL_RTFILE;
    }
    if (pData->DeviceFile != NIL_RTFILE)
    {
        int rc = RTFileClose(pData->DeviceFile);
        AssertRC(rc);
        pData->DeviceFile = NIL_RTFILE;
    }

#elif defined(RT_OS_WINDOWS)

    CloseHandle(pData->hEventReceive);
    CloseHandle(pData->hEventSend);
    CancelIo(pData->hDeviceFile);
    CloseHandle(pData->hDeviceFile);

#endif
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

