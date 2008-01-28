/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Core.
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
#elif defined(RT_OS_SOLARIS)
# include <sys/types.h>
# include <sys/stat.h>
# include <errno.h>
# include <unistd.h>
#endif

#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#ifdef VBOX_VBGLR3_XFREE86
/* Definitions for X server library functions such as xf86open and mappings to C functions. */
/* Make the headers C++-compatible */
# define class xf86_vbox_class
# define bool xf86_vbox_bool
# define private xf86_vbox_private
# define new xf86_vbox_new
extern "C"
{
# include "xf86.h"
# include "xf86_OSproc.h"
# include "xf86Resources.h"
# include "xf86_ansic.h"
}
# undef class
# undef bool
# undef private
# undef new
#else
# include <iprt/file.h>
# include <iprt/assert.h>
# include <iprt/thread.h>
#endif
#include <VBox/VBoxGuest.h>
#include "VBGLR3Internal.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The VBoxGuest device handle. */
#ifdef VBOX_VBGLR3_XFREE86
static int g_File = -1;
#else
static RTFILE g_File = NIL_RTFILE;
#endif
/**
 * A counter of the number of times the library has been initialised, for use with
 * X.org drivers, where the library may be shared by multiple independant modules
 * inside a single process space.
 */
static uint32_t g_cInits = 0;

VBGLR3DECL(int) VbglR3Init(void)
{
    uint32_t cInits = ASMAtomicIncU32(&g_cInits);
#ifdef VBOX_VBGLR3_XFREE86
    if (1 != cInits)
        return VINF_SUCCESS;
    if (-1 != g_File)
        return VERR_INTERNAL_ERROR;
#else
    Assert(cInits > 0);
    if (1 > cInits)
    {
        /* This will not work when the library is shared inside a multi-threaded
           process.  Hopefully no-one will try that, as we can't use the threads
           APIs here. */
        if (NIL_RTFILE == g_File)
            return VERR_INTERNAL_ERROR;
        return VINF_SUCCESS;
    }
    if (NIL_RTFILE != g_File)
        return VERR_INTERNAL_ERROR;
#endif

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

#elif defined(VBOX_VBGLR3_XFREE86)
    int File = open(VBOXGUEST_DEVICE_NAME, O_RDWR);
    if (-1 == File)
    {
        return VERR_UNRESOLVED_ERROR;
    }
    g_File = File;

#else
    /* the default implemenation. (linux, solaris) */
    RTFILE File;
    int rc = RTFileOpen(&File, VBOXGUEST_DEVICE_NAME, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;
    g_File = File;

#endif

    return VINF_SUCCESS;
}


VBGLR3DECL(void) VbglR3Term(void)
{
    uint32_t cInits = ASMAtomicDecU32(&g_cInits);
    if (cInits > 0)
        return;
#ifndef VBOX_VBGLR3_XFREE86
    AssertReturnVoid(0 == cInits);
    RTFILE File = g_File;
    g_File = NIL_RTFILE;
    AssertReturnVoid(NIL_RTFILE != File);
#else
    int File = g_File;
    g_File = -1;
    if (-1 == File)
        return;
#endif
#if defined(RT_OS_OS2)
    APIRET rc = DosClose(File);
    AssertMsg(!rc, ("%ld\n", rc));
#elif defined(VBOX_VBGLR3_XFREE86)
    /* if (-1 == close(File))
    {
        int iErr = errno;
        AssertRC(RTErrConvertFromErrno(iErr));
    } */
    close(File);  /* iprt is not available here. */
    File = -1;
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

#elif defined(RT_OS_SOLARIS)
    VBGLBIGREQ Hdr;
    Hdr.u32Magic = VBGLBIGREQ_MAGIC;
    Hdr.cbData = cbData;
    Hdr.pvDataR3 = pvData;

    int rc = ioctl((int)g_File, iFunction, &Hdr);
    if (rc == -1)
    {
        rc = errno;
        return RTErrConvertFromErrno(rc);
    }
    return VINF_SUCCESS;

#else
    /* Default implementation - PORTME: Do not use this without testings that error passing works! */
# ifdef VBOX_VBGLR3_XFREE86
    int rc = ioctl(g_File, (int) iFunction, pvData);
    if (rc == -1)
    {
        rc = errno;
        return RTErrConvertFromErrno(rc);
    }
    return VINF_SUCCESS;
# else
    int rc2 = VERR_INTERNAL_ERROR;
    int rc = RTFileIoCtl(g_File, (int)iFunction, pvData, cbData, &rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
# endif
#endif
}

