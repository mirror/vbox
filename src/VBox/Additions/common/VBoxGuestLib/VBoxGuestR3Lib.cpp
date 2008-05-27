/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Core.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
#ifdef RT_OS_OS2
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>
#elif defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
# include <sys/types.h>
# include <sys/stat.h>
# include <errno.h>
# include <unistd.h>
#endif

#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <VBox/VBoxGuest.h>
#include <VBox/log.h>
#include "VBGLR3Internal.h"

#ifdef VBOX_VBGLR3_XFREE86
/* Rather than try to resolve all the header file conflicts, I will just
   prototype what we need here. */
# define XF86_O_RDWR  0x0002
typedef void *pointer;
extern "C" int xf86open(const char*, int,...);
extern "C" int xf86close(int);
extern "C" int xf86ioctl(int, unsigned long, pointer);
#endif

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The VBoxGuest device handle. */
#ifdef VBOX_VBGLR3_XFREE86
static int g_File = -1;
#else
static RTFILE g_File = NIL_RTFILE;
#endif
/** User counter.
 * A counter of the number of times the library has been initialised, for use with
 * X.org drivers, where the library may be shared by multiple independant modules
 * inside a single process space.
 */
static uint32_t volatile g_cInits = 0;


VBGLR3DECL(int) VbglR3Init(void)
{
    uint32_t cInits = ASMAtomicIncU32(&g_cInits);
#ifndef VBOX_VBGLR3_XFREE86
    Assert(cInits > 0);
#endif
    if (cInits > 1)
    {
        /*
         * This will fail if two (or more) threads race each other calling VbglR3Init.
         * However it will work fine for single threaded or otherwise serialized
         * processed calling us more than once.
         */
#ifndef VBOX_VBGLR3_XFREE86
        if (g_File == NIL_RTFILE)
#else
        if (g_File == -1)
#endif
            return VERR_INTERNAL_ERROR;
        return VINF_SUCCESS;
    }
#ifndef VBOX_VBGLR3_XFREE86
    if (g_File != NIL_RTFILE)
#else
    if (g_File != -1)
#endif
        return VERR_INTERNAL_ERROR;

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

#elif defined(VBOX_VBGLR3_XFREE86) && !defined(RT_OS_FREEBSD)
    int File = xf86open(VBOXGUEST_DEVICE_NAME, XF86_O_RDWR);
    if (File == -1)
        return VERR_OPEN_FAILED;
    g_File = File;

#elif defined(RT_OS_FREEBSD)
    /*
     * Try open the BSD device.
     */
# if defined(VBOX_VBGLR3_XFREE86)
    int File = 0;
# else
    RTFILE File = 0;
# endif
    int rc;
    char szDevice[sizeof(VBOXGUEST_DEVICE_NAME) + 16];
    for (unsigned iUnit = 0; iUnit < 1024; iUnit++)
    {
        RTStrPrintf(szDevice, sizeof(szDevice), VBOXGUEST_DEVICE_NAME "%d", iUnit);
# if defined(VBOX_VBGLR3_XFREE86)
        File = xf86open(szDevice, XF86_O_RDWR);
        if (File >= 0)
            break;
# else
        rc = RTFileOpen(&File, szDevice, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
            break;
# endif
    }

# if defined(VBOX_VBGLR3_XFREE86)
    if (File == -1)
        return VERR_OPEN_FAILED;
# else
    if (RT_FAILURE(rc))
        return rc;
# endif

    g_File = File;

#else
    /* the default implemenation. (linux, solaris) */
    RTFILE File;
    int rc = RTFileOpen(&File, VBOXGUEST_DEVICE_NAME, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;
    g_File = File;

#endif

    /*
     * Create release logger
     */
    PRTLOGGER pReleaseLogger;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    int rc2 = RTLogCreate(&pReleaseLogger, 0, "all", "VBOX_RELEASE_LOG",
                          RT_ELEMENTS(s_apszGroups), &s_apszGroups[0],
                          RTLOGDEST_USER, NULL);
    /* This may legitimately fail if we are using the mini-runtime. */
    if (RT_SUCCESS(rc2))
        RTLogRelSetDefaultInstance(pReleaseLogger);

    return VINF_SUCCESS;
}


VBGLR3DECL(void) VbglR3Term(void)
{
    uint32_t cInits = ASMAtomicDecU32(&g_cInits);
    if (cInits > 0)
        return;
#ifndef VBOX_VBGLR3_XFREE86
    AssertReturnVoid(!cInits);
    RTFILE File = g_File;
    g_File = NIL_RTFILE;
    AssertReturnVoid(File != NIL_RTFILE);

#else
    int File = g_File;
    g_File = -1;
    if (File == -1)
        return;
#endif

#if defined(RT_OS_OS2)
    APIRET rc = DosClose(File);
    AssertMsg(!rc, ("%ld\n", rc));

#elif defined(VBOX_VBGLR3_XFREE86)
    xf86close(File);
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

#elif defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    VBGLBIGREQ Hdr;
    Hdr.u32Magic = VBGLBIGREQ_MAGIC;
    Hdr.cbData = cbData;
    Hdr.pvDataR3 = pvData;

/** @todo test status code passing! */
    int rc = ioctl((int)g_File, iFunction, &Hdr);
    if (rc == -1)
    {
        rc = errno;
        return RTErrConvertFromErrno(rc);
    }
    return VINF_SUCCESS;

#elif defined(VBOX_VBGLR3_XFREE86)
    /* PORTME - This is preferred over the RTFileIOCtl variant below, just be careful with the (int). */
/** @todo test status code passing! */
    int rc = xf86ioctl(g_File, iFunction, pvData);
    if (rc == -1)
        return VERR_FILE_IO_ERROR;  /* This is purely legacy stuff, it has to work and no more. */
    return VINF_SUCCESS;

#else
    /* Default implementation - PORTME: Do not use this without testings that passing errors works! */
/** @todo test status code passing! */
    int rc2 = VERR_INTERNAL_ERROR;
    int rc = RTFileIoCtl(g_File, (int)iFunction, pvData, cbData, &rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
#endif
}

