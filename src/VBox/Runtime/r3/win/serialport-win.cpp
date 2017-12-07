/* $Id$ */
/** @file
 * IPRT - Serial Port API, Windows Implementation.
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
#include <iprt/time.h>
#include "internal/magics.h"

#include <iprt/win/windows.h>


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
    /** The device handle. */
    HANDLE              hDev;
    /** The overlapped I/O structure. */
    OVERLAPPED          Overlapped;
    /** The event handle to wait on for the overlapped I/O operations of the device. */
    HANDLE              hEvtDev;
    /** The event handle to wait on for waking up waiting threads externally. */
    HANDLE              hEvtIntr;
    /** Events pending which were not queried yet. */
    volatile uint32_t   fEvtsPending;
    /** The current active port config. */
    DCB                 PortCfg;
} RTSERIALPORTINTERNAL;
/** Pointer to the internal serial port state. */
typedef RTSERIALPORTINTERNAL *PRTSERIALPORTINTERNAL;



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Tries to set the default config on the given serial port.
 *
 * @returns IPRT status code.
 * @param   pThis                   The internal serial port instance data.
 */
static int rtSerialPortSetDefaultCfg(PRTSERIALPORTINTERNAL pThis)
{
    if (!PurgeComm(pThis->hDev, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR))
        return RTErrConvertFromWin32(GetLastError());

    pThis->PortCfg.DCBlength = sizeof(pThis->PortCfg);
    if (!GetCommState(pThis->hDev, &pThis->PortCfg))
        return RTErrConvertFromWin32(GetLastError());

    pThis->PortCfg.BaudRate    = CBR_9600;
    pThis->PortCfg.fBinary     = TRUE;
    pThis->PortCfg.fParity     = TRUE;
    pThis->PortCfg.fDtrControl = DTR_CONTROL_DISABLE;
    pThis->PortCfg.ByteSize    = 8;
    pThis->PortCfg.Parity      = NOPARITY;

    int rc = VINF_SUCCESS;
    if (!SetCommState(pThis->hDev, &pThis->PortCfg))
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


RTDECL(int)  RTSerialPortOpen(PRTSERIALPORT phSerialPort, const char *pszPortAddress, uint32_t fFlags)
{
    AssertPtrReturn(phSerialPort, VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszPortAddress) && *pszPortAddress != '\0', VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RTSERIALPORT_OPEN_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & RTSERIALPORT_OPEN_F_READ) || (fFlags & RTSERIALPORT_OPEN_F_WRITE),
                 VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    PRTSERIALPORTINTERNAL pThis = (PRTSERIALPORTINTERNAL)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->fOpenFlags   = fFlags;
        pThis->fEvtsPending = 0;
        pThis->hEvtDev      = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (pThis->hEvtDev)
        {
            pThis->hEvtIntr = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (pThis->hEvtIntr)
            {
                DWORD fWinFlags = 0;

                if (fFlags & RTSERIALPORT_OPEN_F_WRITE)
                    fWinFlags |= GENERIC_WRITE;
                if (fFlags & RTSERIALPORT_OPEN_F_READ)
                    fWinFlags |= GENERIC_READ;

                pThis->hDev = CreateFile(pszPortAddress,
                                         fWinFlags,
                                         0, /* Must be opened with exclusive access. */
                                         NULL, /* No SECURITY_ATTRIBUTES structure. */
                                         OPEN_EXISTING, /* Must use OPEN_EXISTING. */
                                         FILE_FLAG_OVERLAPPED, /* Overlapped I/O. */
                                         NULL);
                if (pThis->hDev)
                    rc = rtSerialPortSetDefaultCfg(pThis);
                else
                    rc = RTErrConvertFromWin32(GetLastError());

                CloseHandle(pThis->hEvtDev);
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());

            CloseHandle(pThis->hEvtDev);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());

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

    CloseHandle(pThis->hDev);
    CloseHandle(pThis->hEvtDev);
    CloseHandle(pThis->hEvtIntr);
    pThis->hDev     = NULL;
    pThis->hEvtDev  = NULL;
    pThis->hEvtIntr = NULL;
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(RTHCINTPTR) RTSerialPortToNative(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, -1);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, -1);

    return (RTHCINTPTR)pThis->hDev;
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

    return VERR_NOT_IMPLEMENTED;
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

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortCfgQueryCurrent(RTSERIALPORT hSerialPort, PRTSERIALPORTCFG pCfg)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    RT_NOREF(pCfg);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortCfgSet(RTSERIALPORT hSerialPort, PCRTSERIALPORTCFG pCfg, PRTERRINFO pErrInfo)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    RT_NOREF(pCfg, pErrInfo);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortEvtPoll(RTSERIALPORT hSerialPort, uint32_t fEvtMask, uint32_t *pfEvtsRecv,
                                RTMSINTERVAL msTimeout)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fEvtMask & ~RTSERIALPORT_EVT_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfEvtsRecv, VERR_INVALID_POINTER);

    RT_NOREF(msTimeout);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTSerialPortEvtPollInterrupt(RTSERIALPORT hSerialPort)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    if (!SetEvent(pThis->hEvtIntr))
        return RTErrConvertFromWin32(GetLastError());

    return VINF_SUCCESS;
}


RTDECL(int) RTSerialPortChgBreakCondition(RTSERIALPORT hSerialPort, bool fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    BOOL fSucc = FALSE;
    if (fSet)
        fSucc = SetCommBreak(pThis->hDev);
    else
        fSucc = ClearCommBreak(pThis->hDev);

    int rc = VINF_SUCCESS;
    if (!fSucc)
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}


RTDECL(int) RTSerialPortChgStatusLines(RTSERIALPORT hSerialPort, uint32_t fClear, uint32_t fSet)
{
    PRTSERIALPORTINTERNAL pThis = hSerialPort;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTSERIALPORT_MAGIC, VERR_INVALID_HANDLE);

    BOOL fSucc = TRUE;
    if (fSet & RTSERIALPORT_CHG_STS_LINES_F_DTR)
        fSucc = EscapeCommFunction(pThis->hDev, SETDTR);
    if (   fSucc
        && (fSet & RTSERIALPORT_CHG_STS_LINES_F_RTS))
        fSucc = EscapeCommFunction(pThis->hDev, SETRTS);

    if (   fSucc
        && (fClear & RTSERIALPORT_CHG_STS_LINES_F_DTR))
        fSucc = EscapeCommFunction(pThis->hDev, CLRDTR);
    if (   fSucc
        && (fClear & RTSERIALPORT_CHG_STS_LINES_F_RTS))
        fSucc = EscapeCommFunction(pThis->hDev, CLRRTS);

    int rc = VINF_SUCCESS;
    if (!fSucc)
        rc = RTErrConvertFromWin32(GetLastError());

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
    DWORD fStsLinesQueried = 0;

    /* Get the new state */
    if (GetCommModemStatus(pThis->hDev, &fStsLinesQueried))
    {
        *pfStsLines |= (fStsLinesQueried & MS_RLSD_ON) ? RTSERIALPORT_STS_LINE_DCD : 0;
        *pfStsLines |= (fStsLinesQueried & MS_RING_ON) ? RTSERIALPORT_STS_LINE_RI  : 0;
        *pfStsLines |= (fStsLinesQueried & MS_DSR_ON) ? RTSERIALPORT_STS_LINE_DSR : 0;
        *pfStsLines |= (fStsLinesQueried & MS_CTS_ON) ? RTSERIALPORT_STS_LINE_CTS : 0;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}

