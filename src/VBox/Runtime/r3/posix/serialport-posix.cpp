/* $Id$ */
/** @file
 * IPRT - Serial Port API, POSIX Implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/serialport.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include "internal/magics.h"

#include <errno.h>
#ifdef RT_OS_SOLARIS
# include <sys/termios.h>
#else
# include <termios.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#ifdef RT_OS_DARWIN
# include <sys/select.h>
#else
# include <sys/poll.h>
#endif
#include <sys/ioctl.h>
#include <pthread.h>

#ifdef RT_OS_LINUX
/*
 * TIOCM_LOOP is not defined in the above header files for some reason but in asm/termios.h.
 * But inclusion of this file however leads to compilation errors because of redefinition of some
 * structs. That's why it is defined here until a better solution is found.
 */
# ifndef TIOCM_LOOP
#  define TIOCM_LOOP 0x8000
# endif
/* For linux custom baudrate code we also need serial_struct */
# include <linux/serial.h>
#endif /* linux */

/** Define fallback if not supported. */
#if !defined(CMSPAR)
# define CMSPAR 0
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal serial port state.
 */
typedef struct RTSERIALPORTINTERNAL
{
    /** Magic value (RTSERIALPORT_MAGIC). */
    uint32_t            u32Magic;
    /** Flags given while opening the serial port. */
    uint32_t            fOpenFlags;
    /** The file descriptor of the serial port. */
    int                 iFd;
    /** The status line monitor thread if enabled. */
    RTTHREAD            hMonThrd;
    /** Flag whether the monitoring thread should shutdown. */
    volatile bool       fMonThrdShutdown;
    /** The current active config (we assume no one changes this behind our back). */
    struct termios      PortCfg;
} RTSERIALPORTINTERNAL;
/** Pointer to the internal serial port state. */
typedef RTSERIALPORTINTERNAL *PRTSERIALPORTINTERNAL;


/**
 * Baud rate conversion table descriptor.
 */
typedef struct RTSERIALPORTBRATECONVDESC
{
    /** The platform independent baud rate used by the RTSerialPort* API. */
    uint32_t            uBaudRateCfg;
    /** The speed identifier used in the termios structure. */
    speed_t             iSpeedTermios;
} RTSERIALPORTBRATECONVDESC;
/** Pointer to a baud rate converions table descriptor. */
typedef RTSERIALPORTBRATECONVDESC *PRTSERIALPORTBRATECONVDESC;
/** Pointer to a const baud rate conversion table descriptor. */
typedef const RTSERIALPORTBRATECONVDESC *PCRTSERIALPORTBRATECONVDESC;

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/

/** The baud rate conversion table. */
static const RTSERIALPORTBRATECONVDESC s_rtSerialPortBaudrateConv[] =
{
    { 50,     B50     },
    { 75,     B75     },
    { 110,    B110    },
    { 134,    B134    },
    { 150,    B150    },
    { 200,    B200    },
    { 300,    B300    },
    { 600,    B600    },
    { 1200,   B1200   },
    { 1800,   B1800   },
    { 2400,   B2400   },
    { 4800,   B4800   },
    { 9600,   B9600   },
    { 19200,  B19200  },
    { 38400,  B38400  },
    { 57600,  B57600  },
    { 115200, B115200 }
};



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Converts the given termios speed identifier to the baud rate used in the API.
 *
 * @returns Baud rate or 0 if not a standard baud rate
 */
DECLINLINE(uint32_t) rtSerialPortGetBaudrateFromTermiosSpeed(speed_t enmSpeed)
{
    for (unsigned i = 0; i < RT_ELEMENTS(s_rtSerialPortBaudrateConv); i++)
    {
        if (s_rtSerialPortBaudrateConv[i].iSpeedTermios == enmSpeed)
            return s_rtSerialPortBaudrateConv[i].uBaudRateCfg;
    }

    return 0;
}


/**
 * Converts the given baud rate to proper termios speed identifier.
 *
 * @returns Speed identifier if available or B0 if no matching speed for the baud rate
 *          could be found.
 * @param   uBaudRate               The baud rate to convert.
 */
DECLINLINE(speed_t) rtSerialPortGetTermiosSpeedFromBaudrate(uint32_t uBaudRate)
{
    for (unsigned i = 0; i < RT_ELEMENTS(s_rtSerialPortBaudrateConv); i++)
    {
        if (s_rtSerialPortBaudrateConv[i].uBaudRateCfg == uBaudRate)
            return s_rtSerialPortBaudrateConv[i].iSpeedTermios;
    }

    return B0;
}


/**
 * Tries to set the default config on the given serial port.
 *
 * @returns IPRT status code.
 * @param   pThis                   The internal serial port instance data.
 */
static int rtSerialPortSetDefaultCfg(PRTSERIALPORTINTERNAL pThis)
{
    pThis->PortCfg.c_iflag = INPCK; /* Input parity checking. */
    cfsetispeed(&pThis->PortCfg, B9600);
    cfsetospeed(&pThis->PortCfg, B9600);
    pThis->PortCfg.c_cflag = CS8 | CLOCAL; /* 8 data bits, ignore modem control lines. */
    if (pThis->fOpenFlags & RT_SERIALPORT_OPEN_F_READ)
        pThis->PortCfg.c_cflag |= CREAD;   /* Enable receiver. */

    /* Set to raw input mode. */
    pThis->PortCfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ECHOK | ISIG | IEXTEN);
    pThis->PortCfg.c_cc[VMIN]  = 0; /* Achieve non-blocking behavior. */
    pThis->PortCfg.c_cc[VTIME] = 0;

    int rc = VINF_SUCCESS;
    int rcPsx = tcflush(pThis->iFd, TCIOFLUSH);
    if (!rcPsx)
    {
        rcPsx = tcsetattr(pThis->iFd, TCSANOW, &pThis->PortCfg);
        if (rcPsx == -1)
            rc = RTErrConvertFromErrno(errno);

        if (RT_SUCCESS(rc))
        {
            if (pThis->fOpenFlags & RT_SERIALPORT_OPEN_F_ENABLE_LOOPBACK)
            {
                int fTiocmSet = TIOCM_LOOP;
                rcPsx = ioctl(pThis->iFd, TIOCMBIS, &fTiocmSet);
                if (rcPsx == -1)
                    rc = RTErrConvertFromErrno(errno);
            }
            else
            {
                /* Make sure it is clear. */
                int fTiocmClear = TIOCM_LOOP;
                rcPsx = ioctl(pThis->iFd, TIOCMBIC, &fTiocmClear);
                if (rcPsx == -1)
                    rc = RTErrConvertFromErrno(errno);
            }
        }
    }
    else
        rc = RTErrConvertFromErrno(errno);

    return rc;
}


/**
 * Converts the given serial port config to the appropriate termios counterpart.
 *
 * @returns IPRT status code.
 * @param   pCfg                    Pointer to the serial port config descriptor.
 * @param   pTermios                Pointer to the termios structure to fill.
 * @param   pErrInfo                Additional error to be set when the conversion fails.
 */
static int rtSerialPortCfg2Termios(PCRTSERIALPORTCFG pCfg, struct termios *pTermios, PRTERRINFO pErrInfo)
{
    RT_NOREF(pErrInfo); /** @todo: Make use of the error info. */
    speed_t enmSpeed = rtSerialPortGetTermiosSpeedFromBaudrate(pCfg->uBaudRate);
    if (enmSpeed != B0)
    {
        tcflag_t const fCFlagMask = (CS5 | CS6 | CS7 | CS8 | CSTOPB | PARENB | PARODD | CMSPAR);
        tcflag_t fCFlagNew = 0;

        switch (pCfg->enmDataBitCount)
        {
            case RTSERIALPORTDATABITS_5BITS:
                fCFlagNew |= CS5;
                break;
            case RTSERIALPORTDATABITS_6BITS:
                fCFlagNew |= CS6;
                break;
            case RTSERIALPORTDATABITS_7BITS:
                fCFlagNew |= CS7;
                break;
            case RTSERIALPORTDATABITS_8BITS:
                fCFlagNew |= CS8;
                break;
            default:
                AssertFailed();
                return VERR_INVALID_PARAMETER;
        }

        switch (pCfg->enmParity)
        {
            case RTSERIALPORTPARITY_NONE:
                break;
            case RTSERIALPORTPARITY_EVEN:
                fCFlagNew |= PARENB;
                break;
            case RTSERIALPORTPARITY_ODD:
                fCFlagNew |= PARENB | PARODD;
                break;
#if CMSPAR != 0
            case RTSERIALPORTPARITY_MARK:
                fCFlagNew |= PARENB | CMSPAR | PARODD;
                break;
            case RTSERIALPORTPARITY_SPACE:
                fCFlagNew |= PARENB | CMSPAR;
                break;
#else
            case RTSERIALPORTPARITY_MARK:
            case RTSERIALPORTPARITY_SPACE:
                return VERR_NOT_SUPPORTED;
#endif
            default:
                AssertFailed();
                return VERR_INVALID_PARAMETER;
        }

        switch (pCfg->enmStopBitCount)
        {
            case RTSERIALPORTSTOPBITS_ONE:
                break;
            case RTSERIALPORTSTOPBITS_ONEPOINTFIVE:
                if (pCfg->enmDataBitCount == RTSERIALPORTDATABITS_5BITS)
                    fCFlagNew |= CSTOPB;
                else
                    return VERR_NOT_SUPPORTED;
                break;
            case RTSERIALPORTSTOPBITS_TWO:
                if (pCfg->enmDataBitCount != RTSERIALPORTDATABITS_5BITS)
                    fCFlagNew |= CSTOPB;
                else
                    return VERR_NOT_SUPPORTED;
                break;
            default:
                AssertFailed();
                return VERR_INVALID_PARAMETER;
        }

        /* Assign new flags. */
        pTermios->c_cflag = (pTermios->c_cflag & ~fCFlagMask) | fCFlagNew;
    }
    else
        return VERR_SERIALPORT_INVALID_BAUDRATE;

#ifdef RT_OS_LINUX
    /** @todo: Handle custom baudrates supported by Linux. */
#endif

    return VINF_SUCCESS;
}


/**
 * Converts the given termios structure to an appropriate serial port config.
 *
 * @returns IPRT status code.
 * @param   pTermios                The termios structure to convert.
 * @param   pCfg                    The serial port config to fill in.
 */
static int rtSerialPortTermios2Cfg(struct termios *pTermios, PRTSERIALPORTCFG pCfg)
{
    int rc = VINF_SUCCESS;
    bool f5DataBits = false;
    speed_t enmSpeedIn = cfgetispeed(pTermios);
    Assert(enmSpeedIn == cfgetospeed(pTermios)); /* Should always be the same. */

    pCfg->uBaudRate = rtSerialPortGetBaudrateFromTermiosSpeed(enmSpeedIn);
    if (!pCfg->uBaudRate)
        rc = VERR_SERIALPORT_INVALID_BAUDRATE;

    switch (pTermios->c_cflag & CSIZE)
    {
        case CS5:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_5BITS;
            f5DataBits = true;
            break;
        case CS6:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_6BITS;
            break;
        case CS7:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_7BITS;
            break;
        case CS8:
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_8BITS;
            break;
        default:
            AssertFailed(); /* Should not happen. */
            pCfg->enmDataBitCount = RTSERIALPORTDATABITS_INVALID;
            rc = RT_FAILURE(rc) ? rc : VERR_INVALID_PARAMETER;
    }

    /* Convert parity. */
    if (pTermios->c_cflag & PARENB)
    {
        /*
         * CMSPAR is not supported on all systems, especially OS X. As configuring
         * mark/space parity there is not supported and we start from a known config
         * when opening the serial port it is not required to check for this here.
         */
#if CMSPAR == 0
        bool fCmsParSet = RT_BOOL(pTermios->c_cflag & CMSPAR);
#else
        bool fCmsParSet = false;
#endif
        if (pTermios->c_cflag & PARODD)
            pCfg->enmParity = fCmsParSet ? RTSERIALPORTPARITY_MARK : RTSERIALPORTPARITY_ODD;
        else
            pCfg->enmParity = fCmsParSet ? RTSERIALPORTPARITY_SPACE: RTSERIALPORTPARITY_EVEN;
    }
    else
        pCfg->enmParity = RTSERIALPORTPARITY_NONE;

    /*
     * 1.5 stop bits are used with a data count of 5 bits when a UART derived from the 8250
     * is used.
     */
    if (pTermios->c_cflag & CSTOPB)
        pCfg->enmStopBitCount = f5DataBits ? RTSERIALPORTSTOPBITS_ONEPOINTFIVE : RTSERIALPORTSTOPBITS_TWO;
    else
        pCfg->enmStopBitCount = RTSERIALPORTSTOPBITS_ONE;

    return rc;
}


/**
 * The status line monitor thread worker.
 *
 * @returns IPRT status code.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
static DECLCALLBACK(int) rtSerialPortStsLineMonitorThrd(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf);
    PRTSERIALPORTINTERNAL pThis = (PRTSERIALPORTINTERNAL)pvUser;
    unsigned long const fStsLinesChk = TIOCM_CAR | TIOCM_RNG | TIOCM_DSR | TIOCM_CTS;
    int rc = VINF_SUCCESS;
    uint32_t fStsLinesOld = 0;
    uint32_t cStsLineGetErrors = 0;
#ifdef RT_OS_LINUX
    bool fPoll = false;
#else
    const bool fPoll = true;
#endif

    int rcPsx = ioctl(pThis->iFd, TIOCMGET, &fStsLinesOld);
    if (rcPsx == -1)
    {
        ASMAtomicXchgBool(&pThis->fMonThrdShutdown, true);
        return RTErrConvertFromErrno(errno);
    }

    while (   !pThis->fMonThrdShutdown
           && RT_SUCCESS(rc))
    {
# ifdef RT_OS_LINUX
        /*
         * Wait for status line change.
         *
         * XXX In Linux, if a thread calls tcsetattr while the monitor thread is
         * waiting in ioctl for a modem status change then 8250.c wrongly disables
         * modem irqs and so the monitor thread never gets released. The workaround
         * is to send a signal after each tcsetattr.
         *
         * TIOCMIWAIT doesn't work for the DSR line with TIOCM_DSR set
         * (see http://lxr.linux.no/#linux+v4.7/drivers/usb/class/cdc-acm.c#L949)
         * However as it is possible to query the line state we will not just clear
         * the TIOCM_DSR bit from the lines to check but resort to the polling
         * approach just like on other hosts.
         */
        if (!fPoll)
        {
            rcPsx = ioctl(pThis->iFd, TIOCMIWAIT, fStsLinesChk);
            if (!rcPsx)
            { /** @todo: Notify any event waiter. */ }
            else if (rcPsx == -1 && errno != EINTR)
                fPoll = true;
        }
        else
#endif
        {
            uint32_t fStsLines = 0;
            rcPsx = ioctl(pThis->iFd, TIOCMGET, &fStsLines);
            if (!rcPsx)
            {
                cStsLineGetErrors = 0; /* Reset the error counter once we had one successful query. */

                if (((fStsLines ^ fStsLinesOld) & fStsLinesChk))
                {
                    /** @todo: Notify any event waiter. */
                    fStsLinesOld = fStsLines;
                }
                else /* No change, sleep for a bit. */
                    RTThreadSleep(100 /*ms*/);
            }
            else if (rcPsx == -1 && errno != EINTR)
            {
                /*
                 * If querying the status line fails too often we have to shut down the
                 * thread and notify the user of the serial port.
                 */
                if (cStsLineGetErrors++ >= 10)
                { /** @todo: Send error notification */ }

                RTThreadSleep(100 /*ms*/);
            }
        }
    }

    ASMAtomicXchgBool(&pThis->fMonThrdShutdown, true);
    return rc;
}


/**
 * Creates the status line monitoring thread.
 *
 * @returns IPRT status code.
 * @param   pThis                   The internal serial port instance data.
 */
static int rtSerialPortMonitorThreadCreate(PRTSERIALPORTINTERNAL pThis)
{
    pThis->fMonThrdShutdown = false;
    int rc = RTThreadCreate(&pThis->hMonThrd, rtSerialPortStsLineMonitorThrd, pThis, 0 /*cbStack*/,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "IPRT-SerPortMon");
    if (RT_SUCCESS(rc))
    {
        /* Wait for the thread to start up. */
        rc = RTThreadUserWait(pThis->hMonThrd, 20*RT_MS_1SEC);
        if (   rc == VERR_TIMEOUT
            || pThis->fMonThrdShutdown)
        {
            /* Startup failed, try to reap the thread. */
            int rcThrd;
            rc = RTThreadWait(pThis->hMonThrd, 20*RT_MS_1SEC, &rcThrd);
            if (RT_SUCCESS(rc))
                rc = rcThrd;
            else
                rc = VERR_INTERNAL_ERROR;
            /* The thread is lost otherwise. */
        }
    }

    return rc;
}


/**
 * Shuts down the status line monitor thread.
 *
 * @returns nothing.
 * @param   pThis                   The internal serial port instance data.
 */
static void rtSerialPortMonitorThreadShutdown(PRTSERIALPORTINTERNAL pThis)
{
    bool fShutDown = ASMAtomicXchgBool(&pThis->fMonThrdShutdown, true);
    if (!fShutDown)
    {
        int rc = RTThreadPoke(pThis->hMonThrd);
        AssertRC(rc);
    }

    int rcThrd = VINF_SUCCESS;
    int rc = RTThreadWait(pThis->hMonThrd, 20*RT_MS_1SEC, &rcThrd);
    AssertRC(rc);
    AssertRC(rcThrd);
}


RTDECL(int)  RTSerialPortOpen(PRTSERIALPORT phSerialPort, const char *pszPortAddress, uint32_t fFlags)
{
    AssertPtrReturn(phSerialPort, VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszPortAddress) && *pszPortAddress != '\0', VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RT_SERIALPORT_OPEN_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & RT_SERIALPORT_OPEN_F_READ) || (fFlags & RT_SERIALPORT_OPEN_F_WRITE),
                 VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PRTSERIALPORTINTERNAL pThis = (PRTSERIALPORTINTERNAL)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        int fPsxFlags = O_NOCTTY;

        if ((fFlags & RT_SERIALPORT_OPEN_F_READ) && !(fFlags & RT_SERIALPORT_OPEN_F_WRITE))
            fPsxFlags |= O_RDONLY;
        else if (!(fFlags & RT_SERIALPORT_OPEN_F_READ) && (fFlags & RT_SERIALPORT_OPEN_F_WRITE))
            fPsxFlags |= O_WRONLY;
        else
            fPsxFlags |= O_RDWR;

        pThis->fOpenFlags = fFlags;
        pThis->iFd        = open(pszPortAddress, fPsxFlags);
        if (pThis->iFd != -1)
        {
            rc = rtSerialPortSetDefaultCfg(pThis);
            if (   RT_SUCCESS(rc)
                && (fFlags & RT_SERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING))
                rc = rtSerialPortMonitorThreadCreate(pThis);

            if (RT_SUCCESS(rc))
            {
                *phSerialPort = pThis;
                return VINF_SUCCESS;
            }

            close(pThis->iFd);
        }
        else
            rc = RTErrConvertFromErrno(errno);

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)  RTSerialPortClose(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    if (pThis == NIL_RTSERIALPORT)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the cleanup.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSERIALPORT_MAGIC_DEAD, RTSERIALPORT_MAGIC), VERR_INVALID_HANDLE);

    if (pThis->fOpenFlags & RT_SERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING)
        rtSerialPortMonitorThreadShutdown(pThis);

    close(pThis->iFd);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(RTHCINTPTR) RTSerialPortToNative(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, -1);

    return pThis->iFd;
}


RTDECL(int) RTSerialPortRead(RTSERIALPORT hSerialPort, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    RT_NOREF(hSerialPort, pvBuf, cbToRead, pcbRead);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortReadNB(RTSERIALPORT hSerialPort, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToRead > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);

    *pcbRead = 0;

    int rc = VINF_SUCCESS;
    ssize_t cbThisRead = read(pThis->iFd, pvBuf, cbToRead);
    if (cbThisRead > 0)
    {
        /*
         * The read data needs to be scanned for the BREAK condition marker encoded in the data stream,
         * if break detection was enabled during open.
         */
        if (pThis->fOpenFlags & RT_SERIALPORT_OPEN_F_DETECT_BREAK_CONDITION)
        { /** @todo */ }

        *pcbRead = cbThisRead;
    }
    else if (cbThisRead == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        rc = VINF_TRY_AGAIN;
    else
        rc = RTErrConvertFromErrno(errno);

    return rc;
}


RTDECL(int) RTSerialPortWrite(RTSERIALPORT hSerialPort, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    RT_NOREF(hSerialPort, pvBuf, cbToWrite, pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortWriteNB(RTSERIALPORT hSerialPort, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToWrite > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);

    *pcbWritten = 0;

    int rc = VINF_SUCCESS;
    ssize_t cbThisWrite = write(pThis->iFd, pvBuf, cbToWrite);
    if (cbThisWrite > 0)
        *pcbWritten = cbThisWrite;
    else if (cbThisWrite == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        rc = VINF_TRY_AGAIN;
    else
        rc = RTErrConvertFromErrno(errno);

    return rc;
}


RTDECL(int) RTSerialPortCfgQueryCurrent(RTSERIALPORT hSerialPort, PRTSERIALPORTCFG pCfg)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    return rtSerialPortTermios2Cfg(&pThis->PortCfg, pCfg);
}


RTDECL(int) RTSerialPortCfgSet(RTSERIALPORT hSerialPort, PCRTSERIALPORTCFG pCfg, PRTERRINFO pErrInfo)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    struct termios PortCfgNew; RT_ZERO(PortCfgNew);
    int rc = rtSerialPortCfg2Termios(pCfg, &PortCfgNew, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        int rcPsx = tcsetattr(pThis->iFd, TCSANOW, &PortCfgNew);
        if (rcPsx == -1)
            rc = RTErrConvertFromErrno(errno);
        else
            memcpy(&pThis->PortCfg, &PortCfgNew, sizeof(struct termios));

#ifdef RT_OS_LINUX
        /*
         * XXX In Linux, if a thread calls tcsetattr while the monitor thread is
         * waiting in ioctl for a modem status change then 8250.c wrongly disables
         * modem irqs and so the monitor thread never gets released. The workaround
         * is to send a signal after each tcsetattr.
         */
        if (pThis->fOpenFlags & RT_SERIALPORT_OPEN_F_SUPPORT_STATUS_LINE_MONITORING)
            RTThreadPoke(pThis->hMonThrd);
#endif
    }

    return rc;
}


RTDECL(int) RTSerialPortEvtPoll(RTSERIALPORT hSerialPort, RTSERIALPORTEVT *penmEvt, RTMSINTERVAL msTimeout)
{
    RT_NOREF(hSerialPort, penmEvt, msTimeout);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortEvtPollInterrupt(RTSERIALPORT hSerialPort)
{
    RT_NOREF(hSerialPort);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortChgBreakCondition(RTSERIALPORT hSerialPort, bool fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    int rcPsx = ioctl(pThis->iFd, fSet ? TIOCSBRK : TIOCCBRK);
    if (rcPsx == -1)
        rc = RTErrConvertFromErrno(errno);

    return rc;
}


RTDECL(int) RTSerialPortChgStatusLines(RTSERIALPORT hSerialPort, uint32_t fClear, uint32_t fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    int fTiocmSet = 0;
    int fTiocmClear = 0;

    if (fClear & RT_SERIALPORT_CHG_STS_LINES_F_RTS)
        fTiocmClear |= TIOCM_RTS;
    if (fClear & RT_SERIALPORT_CHG_STS_LINES_F_DTR)
        fTiocmClear |= TIOCM_DTR;

    if (fSet & RT_SERIALPORT_CHG_STS_LINES_F_RTS)
        fTiocmSet |= TIOCM_RTS;
    if (fSet & RT_SERIALPORT_CHG_STS_LINES_F_DTR)
        fTiocmSet |= TIOCM_DTR;

    int rcPsx = ioctl(pThis->iFd, TIOCMBIS, &fTiocmSet);
    if (!rcPsx)
    {
        rcPsx = ioctl(pThis->iFd, TIOCMBIC, &fTiocmClear);
        if (rcPsx == -1)
            rc = RTErrConvertFromErrno(errno);
    }
    return rc;
}


RTDECL(int) RTSerialPortQueryStatusLines(RTSERIALPORT hSerialPort, uint32_t *pfStsLines)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfStsLines, VERR_INVALID_POINTER);

    *pfStsLines = 0;

    int rc = VINF_SUCCESS;
    int fStsLines = 0;
    int rcPsx = ioctl(pThis->iFd, TIOCMGET, &fStsLines);
    if (!rcPsx)
    {
        *pfStsLines |= (fStsLines & TIOCM_CAR) ? RT_SERIALPORT_STS_LINE_DCD : 0;
        *pfStsLines |= (fStsLines & TIOCM_RNG) ? RT_SERIALPORT_STS_LINE_RI  : 0;
        *pfStsLines |= (fStsLines & TIOCM_DSR) ? RT_SERIALPORT_STS_LINE_DSR : 0;
        *pfStsLines |= (fStsLines & TIOCM_CTS) ? RT_SERIALPORT_STS_LINE_CTS : 0;
    }
    else
        rc = RTErrConvertFromErrno(errno);

    return rc;
}




