/** $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions.
 */

/*
 * Copyright (C) 2007 innotek GmbH
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
#ifdef RT_OS_OS2
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>
#endif 

#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <VBox/VBoxGuest.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The VBoxGuest device handle. */
static RTFILE g_File = NIL_RTFILE;


VBGLR3DECL(int) VbglR3Init(void)
{
    if (g_File != NIL_RTFILE)
        return VINF_SUCCESS;

#if defined(RT_OS_OS2)
    /*
     * We might wish to compile this with Watcom, so stick to 
     * the OS/2 APIs all the way. And in any case we have to use 
     * DosDevIOCtl for the requests, why not use Dos* for everything.
     */
    HFILE hf = NULLHANDLE;
    ULONG ulAction = 0;
    APIRET rc = DosOpen((PCSZ)VBOXGUEST_DEVICE_NAME, &hf, &ulAction, 0, FILE_NORMAL,
                        OPEN_ACTION_OPEN_IF_EXISTS,
                        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
                        NULL);
    if (rc)
        return RTErrConvertFromOS2(rc);

    if (hf < 16)
    {
        HFILE ahfs[16];
        unsigned i;
        for (i = 0; i < RT_ELEMENTS(ahfs); i++)
        {
            ahfs[i] = 0xffffffff;
            rc = DosDupHandle(hf, &ahfs[i]);
            if (rc)
                break;
        }

        if (i-- > 1)
        {
            ULONG fulState = 0;
            rc = DosQueryFHState(ahfs[i], &fulState);
            if (!rc)
            {
                fulState |= OPEN_FLAGS_NOINHERIT;
                fulState &= OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_NOINHERIT; /* Turn off non-participating bits. */
                rc = DosSetFHState(ahfs[i], fulState);
            }
            if (!rc)
            {
                rc = DosClose(hf);
                AssertMsg(!rc, ("%ld\n", rc));
                hf = ahfs[i];
            }
            else
                i++;
            while (i-- > 0)
                DosClose(ahfs[i]);
        }
    }
    g_File = hf;

#else 
    /* the default implemenation. */
    RTFILE File;
    int rc = RTFileOpen(&File, VBOXGUEST_DEVICE_NAME, RTFILE_O_DENY_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;
    g_File = File;

#endif 

    return VINF_SUCCESS;
}


VBGLR3DECL(void) VbglR3Term(void)
{
    RTFILE File = g_File;
    g_File = NIL_RTFILE;
    if (File == NIL_RTFILE)
        return;
#if defined(RT_OS_OS2)
    APIRET rc = DosClose(File);
    AssertMsg(!rc, ("%ld\n", rc));
#else
    int rc = RTFileClose(File);
    AssertRC(rc);
#endif
}


/**
 * Internal wrapper around various OS specific ioctl implemenations.
 * 
 * @returns VBox status code as returned by VBoxGuestCommonIOCtl, or
 *          an failure returned by the OS specific ioctl APIs.
 * 
 * @param   iFunction   The requested function.
 * @param   pvData      The input and output data buffer.
 * @param   cbData      The size of the buffer.
 * 
 * @remark  Exactly how the VBoxGuestCommonIOCtl is ferried back
 *          here is OS specific. On BSD and Darwin we can use errno,  
 *          while on OS/2 we use the 2nd buffer of the IOCtl.
 */
int vbglR3DoIOCtl(unsigned iFunction, void *pvData, size_t cbData)
{
#ifdef RT_OS_OS2
    ULONG cbOS2Parm = cbData;
    int32_t vrc = VERR_INTERNAL_ERROR;
    ULONG cbOS2Data = sizeof(vrc);
    APIRET rc = DosDevIOCtl(g_File, VBOXGUEST_IOCTL_CATEGORY, iFunction, 
                            pvData, cbData, &cbOS2Parm,
                            &vrc, sizeof(vrc), &cbOS2Data);
    if (RT_LIKELY(!rc))
        return vrc;
    return RTErrConvertFromOS2(rc);

#elif defined(RT_OS_LINUX)
    int rc2 = VERR_INTERNAL_ERROR;
    int rc = RTFileIoCtl(g_File, (int)iFunction, pvData, cbData, &rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
#else
# error "PORTME"
# if 0 /* if this works for anyone I'd be amazed. */
    int rc2 = VERR_INTERNAL_ERROR;
    int rc = RTFileIoCtl(g_File, (int)iFunction, pvData, cbData, &rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
# endif 
#endif
}


VBGLR3DECL(int) VbglR3GRAlloc(VMMDevRequestHeader **ppReq, uint32_t cb, VMMDevRequestType enmReqType)
{
    VMMDevRequestHeader *pReq;

    AssertPtrReturn(ppReq, VERR_INVALID_PARAMETER);
    AssertMsgReturn(cb >= sizeof(VMMDevRequestHeader), ("%#x vs %#zx\n", cb, sizeof(VMMDevRequestHeader)), 
                    VERR_INVALID_PARAMETER);

    pReq = (VMMDevRequestHeader *)RTMemTmpAlloc(cb);
    if (RT_UNLIKELY(!pReq))
        return VERR_NO_MEMORY;

    pReq->size        = cb;
    pReq->version     = VMMDEV_REQUEST_HEADER_VERSION;
    pReq->requestType = enmReqType;
    pReq->rc          = VERR_GENERAL_FAILURE;
    pReq->reserved1   = 0;
    pReq->reserved2   = 0;

    *ppReq = pReq;

    return VINF_SUCCESS;
}


VBGLR3DECL(int) VbglR3GRPerform(VMMDevRequestHeader *pReq)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_VMMREQUEST(pReq->size), pReq, pReq->size);
}


VBGLR3DECL(void) VbglR3GRFree(VMMDevRequestHeader *pReq)
{
    RTMemTmpFree(pReq);
}


VBGLR3DECL(int) VbglR3GetHostTime(PRTTIMESPEC pTime)
{
    VMMDevReqHostTime Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_GetHostTime);
    Req.time = UINT64_MAX;
    int rc = VbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
        RTTimeSpecSetMilli(pTime, (int64_t)Req.time);
    return rc;
}


/**
 * Cause any pending WaitEvent calls (VBOXGUEST_IOCTL_WAITEVENT) to return 
 * with a VERR_INTERRUPTED status.
 * 
 * Can be used in combination with a termination flag variable for interrupting 
 * event loops. Avoiding race conditions is the responsibility of the caller.
 *
 * @returns IPRT status code
 */
VBGLR3DECL(int) VbglR3InterruptEventWaits(void)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_WAITEVENT_INTERRUPT_ALL, 0, 0);
}
