/* $Id$ */
/** @file
 * VBox stream I/O devices: Host serial driver
 *
 * Contributed by: Alexander Eichner
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_SERIAL
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/file.h>
#include <iprt/alloc.h>

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
# include <errno.h>
# ifdef RT_OS_SOLARIS
#  include <sys/termios.h>
# else
#  include <termios.h>
# endif
# include <sys/types.h>
# include <fcntl.h>
# include <string.h>
# include <unistd.h>
# ifdef RT_OS_DARWIN
#  include <sys/select.h>
# else
#  include <sys/poll.h>
# endif
# include <sys/ioctl.h>
# include <pthread.h>

# ifdef RT_OS_LINUX
/*
 * TIOCM_LOOP is not defined in the above header files for some reason but in asm/termios.h.
 * But inclusion of this file however leads to compilation errors because of redefinition of some
 * structs. Thatswhy it is defined here until a better solution is found.
 */
#  ifndef TIOCM_LOOP
#   define TIOCM_LOOP 0x8000
#  endif
# endif /* linux */

#elif defined(RT_OS_WINDOWS)
# include <Windows.h>
#endif

#include "../Builtins.h"


/** Size of the send fifo queue (in bytes) */
#ifdef RT_OS_DARWIN
  /** @todo This is really desperate, but it seriously looks like
   * the data is arriving way too fast for us to push over. 9600
   * baud and zoc reports sending at 12000+ chars/sec... */
# define CHAR_MAX_SEND_QUEUE             0x400
# define CHAR_MAX_SEND_QUEUE_MASK        0x3ff
#else
# define CHAR_MAX_SEND_QUEUE             0x80
# define CHAR_MAX_SEND_QUEUE_MASK        0x7f
#endif

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
    PPDMTHREAD                  pRecvThread;
    /** Send thread. */
    PPDMTHREAD                  pSendThread;
    /** Status lines monitor thread. */
    PPDMTHREAD                  pMonitorThread;
    /** Send event semephore */
    RTSEMEVENT                  SendSem;

    /** the device path */
    char                        *pszDevicePath;

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    /** the device handle */
    RTFILE                      DeviceFile;
# ifdef RT_OS_DARWIN
    /** The device handle used for reading.
     * Used to prevent the read selecto from blocking the writes. */
    RTFILE                      DeviceFileR;
# endif
    /** The read end of the control pipe */
    RTFILE                      WakeupPipeR;
    /** The write end of the control pipe */
    RTFILE                      WakeupPipeW;
# ifndef RT_OS_LINUX
    /** The current line status.
     * Used by the polling version of drvHostSerialMonitorThread.  */
    int                         fStatusLines;
# endif
#elif defined(RT_OS_WINDOWS)
    /** the device handle */
    HANDLE                      hDeviceFile;
    /** The event semaphore for waking up the receive thread */
    HANDLE                      hHaltEventSem;
    /** The event semaphore for overlapped receiving */
    HANDLE                      hEventRecv;
    /** For overlapped receiving */
    OVERLAPPED                  overlappedRecv;
    /** The event semaphore for overlapped sending */
    HANDLE                      hEventSend;
    /** For overlapped sending */
    OVERLAPPED                  overlappedSend;
#endif

    /** Internal send FIFO queue */
    uint8_t volatile            aSendQueue[CHAR_MAX_SEND_QUEUE];
    uint32_t volatile           iSendQueueHead;
    uint32_t volatile           iSendQueueTail;

    /** Read/write statistics */
    STAMCOUNTER                 StatBytesRead;
    STAMCOUNTER                 StatBytesWritten;
#ifdef RT_OS_DARWIN
    /** The number of bytes we've dropped because the send queue
     * was full. */
    STAMCOUNTER                 StatSendOverflows;
#endif
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
    PDRVHOSTSERIAL    pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_CHAR:
            return &pThis->IChar;
        default:
            return NULL;
    }
}


/* -=-=-=-=- IChar -=-=-=-=- */

/** @copydoc PDMICHAR::pfnWrite */
static DECLCALLBACK(int) drvHostSerialWrite(PPDMICHAR pInterface, const void *pvBuf, size_t cbWrite)
{
    PDRVHOSTSERIAL pThis = PDMICHAR_2_DRVHOSTSERIAL(pInterface);
    const uint8_t *pbBuffer = (const uint8_t *)pvBuf;

    LogFlow(("%s: pvBuf=%#p cbWrite=%d\n", __FUNCTION__, pvBuf, cbWrite));

    for (uint32_t i = 0; i < cbWrite; i++)
    {
#ifdef RT_OS_DARWIN /* don't wanna break the others here. */
        uint32_t iHead = ASMAtomicUoReadU32(&pThis->iSendQueueHead);
        uint32_t iTail = ASMAtomicUoReadU32(&pThis->iSendQueueTail);
        int32_t  cbAvail = iTail > iHead
                         ? iTail - iHead - 1
                         : CHAR_MAX_SEND_QUEUE - (iHead - iTail) - 1;
        if (cbAvail <= 0)
        {
#ifdef DEBUG
            uint64_t volatile u64Now = RTTimeNanoTS(); NOREF(u64Now);
#endif
            Log(("%s: dropping %d chars (cbAvail=%d iHead=%d iTail=%d)\n", __FUNCTION__, cbWrite - i , cbAvail, iHead, iTail));
            STAM_COUNTER_INC(&pThis->StatSendOverflows);
            break;
        }

        pThis->aSendQueue[iHead] = pbBuffer[i];
        STAM_COUNTER_INC(&pThis->StatBytesWritten);
        ASMAtomicWriteU32(&pThis->iSendQueueHead, (iHead + 1) & CHAR_MAX_SEND_QUEUE_MASK);
        if (cbAvail < CHAR_MAX_SEND_QUEUE / 4)
        {
            RTSemEventSignal(pThis->SendSem);
            RTThreadYield();
        }
#else /* old code */
        uint32_t idx = ASMAtomicUoReadU32(&pThis->iSendQueueHead);

        pThis->aSendQueue[idx] = pbBuffer[i];
        idx = (idx + 1) & CHAR_MAX_SEND_QUEUE_MASK;

        STAM_COUNTER_INC(&pThis->StatBytesWritten);
        ASMAtomicWriteU32(&pThis->iSendQueueHead, idx);
#endif /* old code */
    }
    RTSemEventSignal(pThis->SendSem);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostSerialSetParameters(PPDMICHAR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits)
{
    PDRVHOSTSERIAL pThis = PDMICHAR_2_DRVHOSTSERIAL(pInterface);
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    struct termios *termiosSetup;
    int baud_rate;
#elif defined(RT_OS_WINDOWS)
    LPDCB comSetup;
#endif

    LogFlow(("%s: Bps=%u chParity=%c cDataBits=%u cStopBits=%u\n", __FUNCTION__, Bps, chParity, cDataBits, cStopBits));

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
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
#ifdef RT_OS_SOLARIS
    /* turn off other extended special characters in line mode */
    termiosSetup->c_lflag &= ~(IEXTEN);
#endif

    tcsetattr(pThis->DeviceFile, TCSANOW, termiosSetup);
    RTMemTmpFree(termiosSetup);
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

    SetCommState(pThis->hDeviceFile, comSetup);
    RTMemTmpFree(comSetup);
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
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    /* Make sure that the halt event semaphore is reset. */
    DWORD dwRet = WaitForSingleObject(pThis->hHaltEventSem, 0);

    HANDLE haWait[2];
    haWait[0] = pThis->hEventSend;
    haWait[1] = pThis->hHaltEventSem;
#endif

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        int rc = RTSemEventWait(pThis->SendSem, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            break;

        /*
         * Write the character to the host device.
         */
#ifdef RT_OS_DARWIN
        while (pThread->enmState == PDMTHREADSTATE_RUNNING)
        {
            /* copy the send queue so we get a linear buffer with the maximal size. */
            uint8_t  abBuf[sizeof(pThis->aSendQueue)];
            uint32_t cb = 0;
            uint32_t iTail = ASMAtomicUoReadU32(&pThis->iSendQueueTail);
            uint32_t iHead = ASMAtomicUoReadU32(&pThis->iSendQueueHead);
            if (iTail == iHead)
                break;
            do
            {
                abBuf[cb++] = pThis->aSendQueue[iTail];
                iTail = (iTail + 1) & CHAR_MAX_SEND_QUEUE_MASK;
            } while (iTail != iHead);

            ASMAtomicWriteU32(&pThis->iSendQueueTail, iTail);

            /* write it. */
#ifdef DEBUG
            uint64_t volatile u64Now = RTTimeNanoTS(); NOREF(u64Now);
#endif
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)

            size_t cbWritten;
            rc = RTFileWrite(pThis->DeviceFile, abBuf, cb, &cbWritten);
            if (rc == VERR_TRY_AGAIN)
                cbWritten = 0;
            if (cbWritten < cb && (RT_SUCCESS(rc) || rc == VERR_TRY_AGAIN))
            {
                /* ok, block till the device is ready for more (O_NONBLOCK) effect. */
                rc = VINF_SUCCESS;
                uint8_t const *pbSrc = &abBuf[0];
                while (pThread->enmState == PDMTHREADSTATE_RUNNING)
                {
                    /* advance */
                    cb -= cbWritten;
                    pbSrc += cbWritten;

                    /* wait */
                    fd_set WrSet;
                    FD_ZERO(&WrSet);
                    FD_SET(pThis->DeviceFile, &WrSet);
                    fd_set XcptSet;
                    FD_ZERO(&XcptSet);
                    FD_SET(pThis->DeviceFile, &XcptSet);
                    rc = select(pThis->DeviceFile + 1, NULL, &WrSet, &XcptSet, NULL);
                    /** @todo check rc? */

                    /* try write more */
                    rc = RTFileWrite(pThis->DeviceFile, pbSrc, cb, &cbWritten);
                    if (rc == VERR_TRY_AGAIN)
                        cbWritten = 0;
                    else if (RT_FAILURE(rc))
                        break;
                    else if (cbWritten >= cb)
                        break;
                    rc = VINF_SUCCESS;
                } /* wait/write loop */
            }

#elif defined(RT_OS_WINDOWS)
            /* perform an overlapped write operation. */
            DWORD cbWritten;
            memset(&pThis->overlappedSend, 0, sizeof(pThis->overlappedSend));
            pThis->overlappedSend.hEvent = pThis->hEventSend;
            if (!WriteFile(pThis->hDeviceFile, abBuf, cb, &cbWritten, &pThis->overlappedSend))
            {
                dwRet = GetLastError();
                if (dwRet == ERROR_IO_PENDING)
                {
                    /*
                     * write blocked, wait for completion or wakeup...
                     */
                    dwRet = WaitForMultipleObjects(2, haWait, FALSE, INFINITE);
                    if (dwRet != WAIT_OBJECT_0)
                    {
                        AssertMsg(pThread->enmState != PDMTHREADSTATE_RUNNING, ("The halt event sempahore is set but the thread is still in running state\n"));
                        break;
                    }
                }
                else
                    rc = RTErrConvertFromWin32(dwRet);
            }

#endif /* RT_OS_WINDOWS */
            if (RT_FAILURE(rc))
            {
                LogRel(("HostSerial#%d: Serial Write failed with %Rrc; terminating send thread\n", pDrvIns->iInstance, rc));
                return rc;
            }
        } /* write loop */

#else /* old code */
        uint32_t iTail;
        while (   pThread->enmState == PDMTHREADSTATE_RUNNING
               && (iTail = ASMAtomicUoReadU32(&pThis->iSendQueueTail)) != ASMAtomicUoReadU32(&pThis->iSendQueueHead))
        {
            /** @todo process more than one byte? */
            unsigned cbProcessed = 1;
            uint8_t abBuf[1];
            abBuf[0] = pThis->aSendQueue[iTail];

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)

            rc = RTFileWrite(pThis->DeviceFile, abBuf, cbProcessed, NULL);

#elif defined(RT_OS_WINDOWS)

            DWORD cbBytesWritten;
            memset(&pThis->overlappedSend, 0, sizeof(pThis->overlappedSend));
            pThis->overlappedSend.hEvent = pThis->hEventSend;

            if (!WriteFile(pThis->hDeviceFile, abBuf, cbProcessed, &cbBytesWritten, &pThis->overlappedSend))
            {
                dwRet = GetLastError();
                if (dwRet == ERROR_IO_PENDING)
                {
                    /*
                     * write blocked, wait ...
                     */
                    dwRet = WaitForMultipleObjects(2, haWait, FALSE, INFINITE);
                    if (dwRet != WAIT_OBJECT_0)
                    {
                        AssertMsg(pThread->enmState != PDMTHREADSTATE_RUNNING, ("The halt event sempahore is set but the thread is still in running state\n"));
                        break;
                    }
                }
                else
                    rc = RTErrConvertFromWin32(dwRet);
            }

#endif

            if (RT_SUCCESS(rc))
            {
                Assert(cbProcessed == 1);
                ASMAtomicWriteU32(&pThis->iSendQueueTail, (iTail + 1) & CHAR_MAX_SEND_QUEUE_MASK);
            }
            else if (RT_FAILURE(rc))
            {
                LogRel(("HostSerial#%d: Serial Write failed with %Rrc; terminating send thread\n", pDrvIns->iInstance, rc));
                return rc;
            }
        }
#endif /* old code */
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
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    int rc;

    rc = RTSemEventSignal(pThis->SendSem);
    if (RT_FAILURE(rc))
        return rc;

#ifdef RT_OS_WINDOWS
    if (!SetEvent(pThis->hHaltEventSem))
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
static DECLCALLBACK(int) drvHostSerialRecvThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    uint8_t abBuffer[256];
    uint8_t *pbBuffer = NULL;
    size_t cbRemaining = 0; /* start by reading host data */
    int rc = VINF_SUCCESS;
    int rcThread = VINF_SUCCESS;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    /* Make sure that the halt event semaphore is reset. */
    DWORD dwRet = WaitForSingleObject(pThis->hHaltEventSem, 0);

    HANDLE ahWait[2];
    ahWait[0] = pThis->hEventRecv;
    ahWait[1] = pThis->hHaltEventSem;
#endif

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (!cbRemaining)
        {
            /* Get a block of data from the host serial device. */

#if defined(RT_OS_DARWIN) /* poll is broken on x86 darwin, returns POLLNVAL. */
            fd_set RdSet;
            FD_ZERO(&RdSet);
            FD_SET(pThis->DeviceFileR, &RdSet);
            FD_SET(pThis->WakeupPipeR, &RdSet);
            fd_set XcptSet;
            FD_ZERO(&XcptSet);
            FD_SET(pThis->DeviceFileR, &XcptSet);
            FD_SET(pThis->WakeupPipeR, &XcptSet);
# if 1 /* it seems like this select is blocking the write... */
            rc = select(RT_MAX(pThis->WakeupPipeR, pThis->DeviceFileR) + 1, &RdSet, NULL, &XcptSet, NULL);
# else
            struct timeval tv = { 0, 1000 };
            rc = select(RT_MAX(pThis->WakeupPipeR, pThis->DeviceFileR) + 1, &RdSet, NULL, &XcptSet, &tv);
# endif
            if (rc == -1)
            {
                int err = errno;
                rcThread = RTErrConvertFromErrno(err);
                LogRel(("HostSerial#%d: select failed with errno=%d / %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, err, rcThread));
                break;
            }

            /* this might have changed in the meantime */
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
                break;
            if (rc == 0)
                continue;

            /* drain the wakeup pipe */
            size_t cbRead;
            if (   FD_ISSET(pThis->WakeupPipeR, &RdSet)
                || FD_ISSET(pThis->WakeupPipeR, &XcptSet))
            {
                rc = RTFileRead(pThis->WakeupPipeR, abBuffer, 1, &cbRead);
                if (RT_FAILURE(rc))
                {
                    LogRel(("HostSerial#%d: draining the wakekup pipe failed with %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                    rcThread = rc;
                    break;
                }
                continue;
            }

            /* read data from the serial port. */
            rc = RTFileRead(pThis->DeviceFileR, abBuffer, sizeof(abBuffer), &cbRead);
            if (RT_FAILURE(rc))
            {
                LogRel(("HostSerial#%d: (1) Read failed with %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                rcThread = rc;
                break;
            }
            cbRemaining = cbRead;

#elif defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)

            size_t cbRead;
            struct pollfd aFDs[2];
            aFDs[0].fd      = pThis->DeviceFile;
            aFDs[0].events  = POLLIN;
            aFDs[0].revents = 0;
            aFDs[1].fd      = pThis->WakeupPipeR;
            aFDs[1].events  = POLLIN | POLLERR | POLLHUP;
            aFDs[1].revents = 0;
            rc = poll(aFDs, RT_ELEMENTS(aFDs), -1);
            if (rc < 0)
            {
                int err = errno;
                rcThread = RTErrConvertFromErrno(err);
                LogRel(("HostSerial#%d: poll failed with errno=%d / %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, err, rcThread));
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
                RTFileRead(pThis->WakeupPipeR, &abBuffer, 1, &cbRead);
                continue;
            }
            rc = RTFileRead(pThis->DeviceFile, abBuffer, sizeof(abBuffer), &cbRead);
            if (RT_FAILURE(rc))
            {
                /* don't terminate worker thread when data unavailable */
                if (rc == VERR_TRY_AGAIN)
                    continue;

                LogRel(("HostSerial#%d: (2) Read failed with %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                rcThread = rc;
                break;
            }
            cbRemaining = cbRead;

#elif defined(RT_OS_WINDOWS)

            DWORD dwEventMask = 0;
            DWORD dwNumberOfBytesTransferred;

            memset(&pThis->overlappedRecv, 0, sizeof(pThis->overlappedRecv));
            pThis->overlappedRecv.hEvent = pThis->hEventRecv;

            if (!WaitCommEvent(pThis->hDeviceFile, &dwEventMask, &pThis->overlappedRecv))
            {
                dwRet = GetLastError();
                if (dwRet == ERROR_IO_PENDING)
                {
                    dwRet = WaitForMultipleObjects(2, ahWait, FALSE, INFINITE);
                    if (dwRet != WAIT_OBJECT_0)
                    {
                        /* notification to terminate */
                        AssertMsg(pThread->enmState != PDMTHREADSTATE_RUNNING, ("The halt event sempahore is set but the thread is still in running state\n"));
                        break;
                    }
                }
                else
                {
                    rcThread = RTErrConvertFromWin32(dwRet);
                    LogRel(("HostSerial#%d: Wait failed with error %Rrc; terminating the worker thread.\n", pDrvIns->iInstance, rcThread));
                    break;
                }
            }
            /* this might have changed in the meantime */
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
                break;

            /* Check the event */
            if (dwEventMask & EV_RXCHAR)
            {
                if (!ReadFile(pThis->hDeviceFile, abBuffer, sizeof(abBuffer), &dwNumberOfBytesTransferred, &pThis->overlappedRecv))
                {
                    rcThread = RTErrConvertFromWin32(GetLastError());
                    LogRel(("HostSerial#%d: Read failed with error %Rrc; terminating the worker thread.\n", pDrvIns->iInstance, rcThread));
                    break;
                }
                cbRemaining = dwNumberOfBytesTransferred;
            }
            else
            {
                /* The status lines have changed. Notify the device. */
                DWORD dwNewStatusLinesState = 0;
                uint32_t uNewStatusLinesState = 0;

                /* Get the new state */
                if (GetCommModemStatus(pThis->hDeviceFile, &dwNewStatusLinesState))
                {
                    if (dwNewStatusLinesState & MS_RLSD_ON)
                        uNewStatusLinesState |= PDM_ICHAR_STATUS_LINES_DCD;
                    if (dwNewStatusLinesState & MS_RING_ON)
                        uNewStatusLinesState |= PDM_ICHAR_STATUS_LINES_RI;
                    if (dwNewStatusLinesState & MS_DSR_ON)
                        uNewStatusLinesState |= PDM_ICHAR_STATUS_LINES_DSR;
                    if (dwNewStatusLinesState & MS_CTS_ON)
                        uNewStatusLinesState |= PDM_ICHAR_STATUS_LINES_CTS;
                    rc = pThis->pDrvCharPort->pfnNotifyStatusLinesChanged(pThis->pDrvCharPort, uNewStatusLinesState);
                    if (RT_FAILURE(rc))
                    {
                        /* Notifying device failed, continue but log it */
                        LogRel(("HostSerial#%d: Notifying device failed with error %Rrc; continuing.\n", pDrvIns->iInstance, rc));
                    }
                }
                else
                {
                    /* Getting new state failed, continue but log it */
                    LogRel(("HostSerial#%d: Getting status lines state failed with error %Rrc; continuing.\n", pDrvIns->iInstance, RTErrConvertFromWin32(GetLastError())));
                }
            }
#endif

            Log(("Read %d bytes.\n", cbRemaining));
            pbBuffer = abBuffer;
        }
        else
        {
            /* Send data to the guest. */
            size_t cbProcessed = cbRemaining;
            rc = pThis->pDrvCharPort->pfnNotifyRead(pThis->pDrvCharPort, pbBuffer, &cbProcessed);
            if (RT_SUCCESS(rc))
            {
                Assert(cbProcessed); Assert(cbProcessed <= cbRemaining);
                pbBuffer += cbProcessed;
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
                LogRel(("HostSerial#%d: NotifyRead failed with %Rrc, terminating the worker thread.\n", pDrvIns->iInstance, rc));
                rcThread = rc;
                break;
            }
        }
    }

    return rcThread;
}

/**
 * Unblock the send thread so it can respond to a state change.
 *
 * @returns a VBox status code.
 * @param     pDrvIns     The driver instance.
 * @param     pThread     The send thread.
 */
static DECLCALLBACK(int) drvHostSerialWakeupRecvThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    return RTFileWrite(pThis->WakeupPipeW, "", 1, NULL);
#elif defined(RT_OS_WINDOWS)
    if (!SetEvent(pThis->hHaltEventSem))
        return RTErrConvertFromWin32(GetLastError());
    return VINF_SUCCESS;
#else
# error adapt me!
#endif
}

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
/* -=-=-=-=- Monitor thread -=-=-=-=- */

/**
 * Monitor thread loop.
 *
 * This thread monitors the status lines and notifies the device
 * if they change.
 *
 * @returns VINF_SUCCESS.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) drvHostSerialMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    int rc = VINF_SUCCESS;
    unsigned uStatusLinesToCheck = 0;

    uStatusLinesToCheck = TIOCM_CAR | TIOCM_RNG | TIOCM_LE | TIOCM_CTS;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        uint32_t newStatusLine = 0;
        unsigned int statusLines;

# ifdef RT_OS_LINUX
        /*
         * Wait for status line change.
         */
        rc = ioctl(pThis->DeviceFile, TIOCMIWAIT, uStatusLinesToCheck);
        if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            break;
        if (rc < 0)
        {
ioctl_error:
            PDMDrvHlpVMSetRuntimeError(pDrvIns, 0 /*fFlags*/, "DrvHostSerialFail",
                                       N_("Ioctl failed for serial host device '%s' (%Rrc). The device will not work properly"),
                                       pThis->pszDevicePath, RTErrConvertFromErrno(errno));
            break;
        }

        rc = ioctl(pThis->DeviceFile, TIOCMGET, &statusLines);
        if (rc < 0)
            goto ioctl_error;
# else  /* !RT_OS_LINUX */
        /*
         * Poll for the status line change.
         */
        rc = ioctl(pThis->DeviceFile, TIOCMGET, &statusLines);
        if (rc < 0)
        {
            PDMDrvHlpVMSetRuntimeError(pDrvIns, 0 /*fFlags*/, "DrvHostSerialFail",
                                       N_("Ioctl failed for serial host device '%s' (%Rrc). The device will not work properly"),
                                       pThis->pszDevicePath, RTErrConvertFromErrno(errno));
            break;
        }
        if (!((statusLines ^ pThis->fStatusLines) & uStatusLinesToCheck))
        {
            PDMR3ThreadSleep(pThread, 500); /* 0.5 sec */
            continue;
        }
        pThis->fStatusLines = statusLines;
# endif /* !RT_OS_LINUX */

        if (statusLines & TIOCM_CAR)
            newStatusLine |= PDM_ICHAR_STATUS_LINES_DCD;
        if (statusLines & TIOCM_RNG)
            newStatusLine |= PDM_ICHAR_STATUS_LINES_RI;
        if (statusLines & TIOCM_LE)
            newStatusLine |= PDM_ICHAR_STATUS_LINES_DSR;
        if (statusLines & TIOCM_CTS)
            newStatusLine |= PDM_ICHAR_STATUS_LINES_CTS;
        rc = pThis->pDrvCharPort->pfnNotifyStatusLinesChanged(pThis->pDrvCharPort, newStatusLine);
    }

    return VINF_SUCCESS;
}

/**
 * Unblock the monitor thread so it can respond to a state change.
 * We need to execute this code exactly once during initialization.
 * But we don't want to block --- therefore this dedicated thread.
 *
 * @returns a VBox status code.
 * @param     pDrvIns     The driver instance.
 * @param     pThread     The send thread.
 */
static DECLCALLBACK(int) drvHostSerialWakeupMonitorThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
# ifdef RT_OS_LINUX
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    int rc = VINF_SUCCESS;
#  if 0
     unsigned int uSerialLineFlags;
     unsigned int uSerialLineStatus;
     unsigned int uIoctl;
#  endif

    /*
     * Linux is a bit difficult as the thread is sleeping in an ioctl call.
     * So there is no way to have a wakeup pipe.
     *
     * 1. Thatswhy we set the serial device into loopback mode and change one of the
     *    modem control bits.
     *    This should make the ioctl call return.
     *
     * 2. We still got reports about long shutdown times. It may bepossible
     *    that the loopback mode is not implemented on all devices.
     *    The next possible solution is to close the device file to make the ioctl
     *    return with EBADF and be able to suspend the thread.
     *
     * 3. The second approach doesn't work too, the ioctl doesn't return.
     *    But it seems that the ioctl is interruptible (return code in errno is EINTR).
     */

#  if 0 /* Disabled because it does not work for all. */
    /* Get current status of control lines. */
    rc = ioctl(pThis->DeviceFile, TIOCMGET, &uSerialLineStatus);
    if (rc < 0)
        goto ioctl_error;

    uSerialLineFlags = TIOCM_LOOP;
    rc = ioctl(pThis->DeviceFile, TIOCMBIS, &uSerialLineFlags);
    if (rc < 0)
        goto ioctl_error;

    /*
     * Change current level on the RTS pin to make the ioctl call return in the
     * monitor thread.
     */
    uIoctl = (uSerialLineStatus & TIOCM_CTS) ? TIOCMBIC : TIOCMBIS;
    uSerialLineFlags = TIOCM_RTS;

    rc = ioctl(pThis->DeviceFile, uIoctl, &uSerialLineFlags);
    if (rc < 0)
        goto ioctl_error;

    /* Change RTS back to the previous level. */
    uIoctl = (uIoctl == TIOCMBIC) ? TIOCMBIS : TIOCMBIC;

    rc = ioctl(pThis->DeviceFile, uIoctl, &uSerialLineFlags);
    if (rc < 0)
        goto ioctl_error;

    /*
     * Set serial device into normal state.
     */
    uSerialLineFlags = TIOCM_LOOP;
    rc = ioctl(pThis->DeviceFile, TIOCMBIC, &uSerialLineFlags);
    if (rc >= 0)
        return VINF_SUCCESS;

ioctl_error:
    PDMDrvHlpVMSetRuntimeError(pDrvIns, 0 /*fFlags*/, "DrvHostSerialFail",
                                N_("Ioctl failed for serial host device '%s' (%Rrc). The device will not work properly"),
                                pThis->pszDevicePath, RTErrConvertFromErrno(errno));
#  endif

#  if 0
    /* Close file to make ioctl return. */
    RTFileClose(pData->DeviceFile);
    /* Open again to make use after suspend possible again. */
    rc = RTFileOpen(&pData->DeviceFile, pData->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READWRITE);
    AssertMsgRC(rc, ("Opening device file again failed rc=%Rrc\n", rc));

    if (RT_FAILURE(rc))
        PDMDrvHlpVMSetRuntimeError(pDrvIns, false, "DrvHostSerialFail",
                                    N_("Opening failed for serial host device '%s' (%Rrc). The device will not work"),
                                    pData->pszDevicePath, rc);
#  endif

    rc = RTThreadPoke(pThread->Thread);
    if (RT_FAILURE(rc))
        PDMDrvHlpVMSetRuntimeError(pDrvIns, 0 /*fFlags*/, "DrvHostSerialFail",
                                    N_("Suspending serial monitor thread failed for serial device '%s' (%Rrc). The shutdown may take longer than expected"),
                                    pThis->pszDevicePath, RTErrConvertFromErrno(rc));

# else  /* !RT_OS_LINUX*/

    /* In polling mode there is nobody to wake up (PDMThread will cancel the sleep). */
    NOREF(pDrvIns);
    NOREF(pThread);

# endif /* RT_OS_LINUX */

    return VINF_SUCCESS;
}
#endif /* RT_OS_LINUX || RT_OS_DARWIN || RT_OS_SOLARIS */

/**
 * Set the modem lines.
 *
 * @returns VBox status code
 * @param pInterface        Pointer to the interface structure.
 * @param RequestToSend     Set to true if this control line should be made active.
 * @param DataTerminalReady Set to true if this control line should be made active.
 */
static DECLCALLBACK(int) drvHostSerialSetModemLines(PPDMICHAR pInterface, bool RequestToSend, bool DataTerminalReady)
{
    PDRVHOSTSERIAL pThis = PDMICHAR_2_DRVHOSTSERIAL(pInterface);

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    int modemStateSet = 0;
    int modemStateClear = 0;

    if (RequestToSend)
        modemStateSet |= TIOCM_RTS;
    else
        modemStateClear |= TIOCM_RTS;

    if (DataTerminalReady)
        modemStateSet |= TIOCM_DTR;
    else
        modemStateClear |= TIOCM_DTR;

    if (modemStateSet)
        ioctl(pThis->DeviceFile, TIOCMBIS, &modemStateSet);

    if (modemStateClear)
        ioctl(pThis->DeviceFile, TIOCMBIC, &modemStateClear);
#elif defined(RT_OS_WINDOWS)
    if (RequestToSend)
        EscapeCommFunction(pThis->hDeviceFile, SETRTS);
    else
        EscapeCommFunction(pThis->hDeviceFile, CLRRTS);

    if (DataTerminalReady)
        EscapeCommFunction(pThis->hDeviceFile, SETDTR);
    else
        EscapeCommFunction(pThis->hDeviceFile, CLRDTR);
#endif

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
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /*
     * Init basic data members and interfaces.
     */
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    pThis->DeviceFile  = NIL_RTFILE;
# ifdef RT_OS_DARWIN
    pThis->DeviceFileR = NIL_RTFILE;
# endif
    pThis->WakeupPipeR = NIL_RTFILE;
    pThis->WakeupPipeW = NIL_RTFILE;
#endif
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvHostSerialQueryInterface;
    /* IChar. */
    pThis->IChar.pfnWrite                   = drvHostSerialWrite;
    pThis->IChar.pfnSetParameters           = drvHostSerialSetParameters;
    pThis->IChar.pfnSetModemLines           = drvHostSerialSetModemLines;

/** @todo Initialize all members with NIL values!! The destructor is ALWAYS called. */

    /*
     * Query configuration.
     */
    /* Device */
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "DevicePath", &pThis->pszDevicePath);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"DevicePath\" string returned %Rra.\n", rc));
        return rc;
    }

    /*
     * Open the device
     */
#ifdef RT_OS_WINDOWS

    pThis->hHaltEventSem = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pThis->hHaltEventSem != NULL, VERR_NO_MEMORY);

    pThis->hEventRecv = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pThis->hEventRecv != NULL, VERR_NO_MEMORY);

    pThis->hEventSend = CreateEvent(NULL, FALSE, FALSE, NULL);
    AssertReturn(pThis->hEventSend != NULL, VERR_NO_MEMORY);

    HANDLE hFile = CreateFile(pThis->pszDevicePath,
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
        pThis->hDeviceFile = hFile;
        /* for overlapped read */
        if (!SetCommMask(hFile, EV_RXCHAR | EV_CTS | EV_DSR | EV_RING | EV_RLSD))
        {
            LogRel(("HostSerial#%d: SetCommMask failed with error %d.\n", pDrvIns->iInstance, GetLastError()));
            return VERR_FILE_IO_ERROR;
        }
        rc = VINF_SUCCESS;
    }

#else

    rc = RTFileOpen(&pThis->DeviceFile, pThis->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READWRITE);
# ifdef RT_OS_DARWIN
    if (RT_SUCCESS(rc))
        rc = RTFileOpen(&pThis->DeviceFileR, pThis->pszDevicePath, RTFILE_O_OPEN | RTFILE_O_READ);
# endif


#endif

    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Could not open host device %s, rc=%Rrc\n", pThis->pszDevicePath, rc));
        switch (rc)
        {
            case VERR_ACCESS_DENIED:
                return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
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

    /* Set to non blocking I/O */
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)

    fcntl(pThis->DeviceFile, F_SETFL, O_NONBLOCK);
# ifdef RT_OS_DARWIN
    fcntl(pThis->DeviceFileR, F_SETFL, O_NONBLOCK);
# endif
    int aFDs[2];
    if (pipe(aFDs) != 0)
    {
        int rc = RTErrConvertFromErrno(errno);
        AssertRC(rc);
        return rc;
    }
    pThis->WakeupPipeR = aFDs[0];
    pThis->WakeupPipeW = aFDs[1];

#elif defined(RT_OS_WINDOWS)

    /* Set the COMMTIMEOUTS to get non blocking I/O */
    COMMTIMEOUTS comTimeout;

    comTimeout.ReadIntervalTimeout         = MAXDWORD;
    comTimeout.ReadTotalTimeoutMultiplier  = 0;
    comTimeout.ReadTotalTimeoutConstant    = 0;
    comTimeout.WriteTotalTimeoutMultiplier = 0;
    comTimeout.WriteTotalTimeoutConstant   = 0;

    SetCommTimeouts(pThis->hDeviceFile, &comTimeout);

#endif

    /*
     * Get the ICharPort interface of the above driver/device.
     */
    pThis->pDrvCharPort = (PPDMICHARPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_CHAR_PORT);
    if (!pThis->pDrvCharPort)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE, RT_SRC_POS, N_("HostSerial#%d has no char port interface above"), pDrvIns->iInstance);

    /*
     * Create the receive, send and monitor threads pluss the related send semaphore.
     */
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pRecvThread, pThis, drvHostSerialRecvThread, drvHostSerialWakeupRecvThread, 0, RTTHREADTYPE_IO, "SerRecv");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create receive thread"), pDrvIns->iInstance);

    rc = RTSemEventCreate(&pThis->SendSem);
    AssertRC(rc);

    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pSendThread, pThis, drvHostSerialSendThread, drvHostSerialWakeupSendThread, 0, RTTHREADTYPE_IO, "SerSend");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create send thread"), pDrvIns->iInstance);

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    /* Linux & darwin needs a separate thread which monitors the status lines. */
# ifndef RT_OS_LINUX
    ioctl(pThis->DeviceFile, TIOCMGET, &pThis->fStatusLines);
# endif
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pMonitorThread, pThis, drvHostSerialMonitorThread, drvHostSerialWakeupMonitorThread, 0, RTTHREADTYPE_IO, "SerMon");
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("HostSerial#%d cannot create monitor thread"), pDrvIns->iInstance);
#endif

    /*
     * Register release statistics.
     */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten,    STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes written",         "/Devices/HostSerial%d/Written", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead,       STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes read",            "/Devices/HostSerial%d/Read", pDrvIns->iInstance);
#ifdef RT_OS_DARWIN /* new Write code, not darwin specific. */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatSendOverflows,   STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES, "Nr of bytes overflowed",      "/Devices/HostSerial%d/SendOverflow", pDrvIns->iInstance);
#endif

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
    PDRVHOSTSERIAL pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTSERIAL);

    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /* Empty the send queue */
    pThis->iSendQueueTail = pThis->iSendQueueHead = 0;

    RTSemEventDestroy(pThis->SendSem);
    pThis->SendSem = NIL_RTSEMEVENT;

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)

    if (pThis->WakeupPipeW != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->WakeupPipeW);
        AssertRC(rc);
        pThis->WakeupPipeW = NIL_RTFILE;
    }
    if (pThis->WakeupPipeR != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->WakeupPipeR);
        AssertRC(rc);
        pThis->WakeupPipeR = NIL_RTFILE;
    }
# if defined(RT_OS_DARWIN)
    if (pThis->DeviceFileR != NIL_RTFILE)
    {
        if (pThis->DeviceFileR != pThis->DeviceFile)
        {
            int rc = RTFileClose(pThis->DeviceFileR);
            AssertRC(rc);
        }
        pThis->DeviceFileR = NIL_RTFILE;
    }
# endif
    if (pThis->DeviceFile != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->DeviceFile);
        AssertRC(rc);
        pThis->DeviceFile = NIL_RTFILE;
    }

#elif defined(RT_OS_WINDOWS)

    CloseHandle(pThis->hEventRecv);
    CloseHandle(pThis->hEventSend);
    CancelIo(pThis->hDeviceFile);
    CloseHandle(pThis->hDeviceFile);

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

