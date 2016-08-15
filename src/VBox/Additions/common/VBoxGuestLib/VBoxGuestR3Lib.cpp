/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Core.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
#if defined(RT_OS_WINDOWS)
# include <iprt/win/windows.h>

#elif defined(RT_OS_OS2)
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>

#elif defined(RT_OS_DARWIN) \
   || defined(RT_OS_FREEBSD) \
   || defined(RT_OS_HAIKU) \
   || defined(RT_OS_LINUX) \
   || defined(RT_OS_NETBSD) \
   || defined(RT_OS_SOLARIS)
# include <sys/types.h>
# include <sys/stat.h>
# if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_NETBSD)
   /** @todo check this on solaris+freebsd as well. */
#  include <sys/ioctl.h>
# endif
# if defined(RT_OS_DARWIN)
#  include <mach/mach_port.h>
#  include <IOKit/IOKitLib.h>
# endif
# include <errno.h>
# include <unistd.h>
#endif

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <VBox/log.h>
#include "VBGLR3Internal.h"

#ifdef VBOX_VBGLR3_XFREE86
/* Rather than try to resolve all the header file conflicts, I will just
   prototype what we need here. */
# define XF86_O_RDWR  0x0002
typedef void *pointer;
extern "C" int xf86open(const char *, int, ...);
extern "C" int xf86close(int);
extern "C" int xf86ioctl(int, unsigned long, pointer);
# define VBOX_VBGLR3_XSERVER
#elif defined(VBOX_VBGLR3_XORG)
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/ioctl.h>
# define xf86open open
# define xf86close close
# define xf86ioctl ioctl
# define XF86_O_RDWR O_RDWR
# define VBOX_VBGLR3_XSERVER
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The VBoxGuest device handle. */
#ifdef VBOX_VBGLR3_XSERVER
static int g_File = -1;
#elif defined(RT_OS_WINDOWS)
static HANDLE g_hFile = INVALID_HANDLE_VALUE;
#else
static RTFILE g_File = NIL_RTFILE;
#endif
/** User counter.
 * A counter of the number of times the library has been initialised, for use with
 * X.org drivers, where the library may be shared by multiple independent modules
 * inside a single process space.
 */
static uint32_t volatile g_cInits = 0;
#ifdef RT_OS_DARWIN
/** I/O Kit connection handle. */
static io_connect_t g_uConnection = 0;
#endif



/**
 * Implementation of VbglR3Init and VbglR3InitUser
 */
static int vbglR3Init(const char *pszDeviceName)
{
    uint32_t cInits = ASMAtomicIncU32(&g_cInits);
    Assert(cInits > 0);
    if (cInits > 1)
    {
        /*
         * This will fail if two (or more) threads race each other calling VbglR3Init.
         * However it will work fine for single threaded or otherwise serialized
         * processed calling us more than once.
         */
#ifdef RT_OS_WINDOWS
        if (g_hFile == INVALID_HANDLE_VALUE)
#elif !defined (VBOX_VBGLR3_XSERVER)
        if (g_File == NIL_RTFILE)
#else
        if (g_File == -1)
#endif
            return VERR_INTERNAL_ERROR;
        return VINF_SUCCESS;
    }
#if defined(RT_OS_WINDOWS)
    if (g_hFile != INVALID_HANDLE_VALUE)
#elif !defined(VBOX_VBGLR3_XSERVER)
    if (g_File != NIL_RTFILE)
#else
    if (g_File != -1)
#endif
        return VERR_INTERNAL_ERROR;

#if defined(RT_OS_WINDOWS)
    /*
     * Have to use CreateFile here as we want to specify FILE_FLAG_OVERLAPPED
     * and possible some other bits not available thru iprt/file.h.
     */
    HANDLE hFile = CreateFile(pszDeviceName,
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                              NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return VERR_OPEN_FAILED;
    g_hFile = hFile;

#elif defined(RT_OS_OS2)
    /*
     * We might wish to compile this with Watcom, so stick to
     * the OS/2 APIs all the way. And in any case we have to use
     * DosDevIOCtl for the requests, why not use Dos* for everything.
     */
    HFILE hf = NULLHANDLE;
    ULONG ulAction = 0;
    APIRET rc = DosOpen((PCSZ)pszDeviceName, &hf, &ulAction, 0, FILE_NORMAL,
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
    g_File = (RTFILE)hf;

#elif defined(RT_OS_DARWIN)
    /*
     * Darwin is kind of special we need to engage the device via I/O first
     * before we open it via the BSD device node.
     */
    mach_port_t MasterPort;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &MasterPort);
    if (kr != kIOReturnSuccess)
        return VERR_GENERAL_FAILURE;

    CFDictionaryRef ClassToMatch = IOServiceMatching("org_virtualbox_VBoxGuest");
    if (!ClassToMatch)
        return VERR_GENERAL_FAILURE;

    io_service_t ServiceObject = IOServiceGetMatchingService(kIOMasterPortDefault, ClassToMatch);
    if (!ServiceObject)
        return VERR_NOT_FOUND;

    io_connect_t uConnection;
    kr = IOServiceOpen(ServiceObject, mach_task_self(), VBOXGUEST_DARWIN_IOSERVICE_COOKIE, &uConnection);
    IOObjectRelease(ServiceObject);
    if (kr != kIOReturnSuccess)
        return VERR_OPEN_FAILED;

    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszDeviceName, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
    {
        IOServiceClose(uConnection);
        return rc;
    }
    g_File = hFile;
    g_uConnection = uConnection;

#elif defined(VBOX_VBGLR3_XSERVER)
    int File = xf86open(pszDeviceName, XF86_O_RDWR);
    if (File == -1)
        return VERR_OPEN_FAILED;
    g_File = File;

#else

    /* The default implementation. (linux, solaris, freebsd, netbsd, haiku) */
    RTFILE File;
    int rc = RTFileOpen(&File, pszDeviceName, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return rc;
    g_File = File;

#endif

#ifndef VBOX_VBGLR3_XSERVER
    /*
     * Create release logger
     */
    PRTLOGGER pReleaseLogger;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    int rc2 = RTLogCreate(&pReleaseLogger, 0, "all", "VBOX_RELEASE_LOG",
                          RT_ELEMENTS(s_apszGroups), &s_apszGroups[0], RTLOGDEST_USER, NULL);
    /* This may legitimately fail if we are using the mini-runtime. */
    if (RT_SUCCESS(rc2))
        RTLogRelSetDefaultInstance(pReleaseLogger);
#endif

    return VINF_SUCCESS;
}


/**
 * Open the VBox R3 Guest Library.  This should be called by system daemons
 * and processes.
 */
VBGLR3DECL(int) VbglR3Init(void)
{
    return vbglR3Init(VBOXGUEST_DEVICE_NAME);
}


/**
 * Open the VBox R3 Guest Library.  Equivalent to VbglR3Init, but for user
 * session processes.
 */
VBGLR3DECL(int) VbglR3InitUser(void)
{
    return vbglR3Init(VBOXGUEST_USER_DEVICE_NAME);
}


VBGLR3DECL(void) VbglR3Term(void)
{
    /*
     * Decrement the reference count and see if we're the last one out.
     */
    uint32_t cInits = ASMAtomicDecU32(&g_cInits);
    if (cInits > 0)
        return;
#if !defined(VBOX_VBGLR3_XSERVER)
    AssertReturnVoid(!cInits);

# if defined(RT_OS_WINDOWS)
    HANDLE hFile = g_hFile;
    g_hFile = INVALID_HANDLE_VALUE;
    AssertReturnVoid(hFile != INVALID_HANDLE_VALUE);
    BOOL fRc = CloseHandle(hFile);
    Assert(fRc); NOREF(fRc);

# elif defined(RT_OS_OS2)
    RTFILE File = g_File;
    g_File = NIL_RTFILE;
    AssertReturnVoid(File != NIL_RTFILE);
    APIRET rc = DosClose((uintptr_t)File);
    AssertMsg(!rc, ("%ld\n", rc));

#elif defined(RT_OS_DARWIN)
    io_connect_t    uConnection = g_uConnection;
    RTFILE          hFile       = g_File;
    g_uConnection = 0;
    g_File        = NIL_RTFILE;
    kern_return_t kr = IOServiceClose(uConnection);
    AssertMsg(kr == kIOReturnSuccess, ("%#x (%d)\n", kr, kr)); NOREF(kr);
    int rc = RTFileClose(hFile);
    AssertRC(rc);

# else /* The IPRT case. */
    RTFILE File = g_File;
    g_File = NIL_RTFILE;
    AssertReturnVoid(File != NIL_RTFILE);
    int rc = RTFileClose(File);
    AssertRC(rc);
# endif

#else  /* VBOX_VBGLR3_XSERVER */
    int File = g_File;
    g_File = -1;
    if (File == -1)
        return;
    xf86close(File);
#endif /* VBOX_VBGLR3_XSERVER */
}


/**
 * Internal wrapper around various OS specific ioctl implementations.
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
#if defined(RT_OS_WINDOWS)
    DWORD cbReturned = 0;
    if (!DeviceIoControl(g_hFile, iFunction, pvData, (DWORD)cbData, pvData, (DWORD)cbData, &cbReturned, NULL))
    {
/** @todo The passing of error codes needs to be tested and fixed (as does *all* the other hosts except for
 * OS/2).  The idea is that the VBox status codes in ring-0 should be transferred without loss down to
 * ring-3. However, it's not vitally important right now (obviously, since the other guys has been
 * ignoring it for 1+ years now).  On Linux and Solaris the transfer is done, but it is currently not
 * lossless, so still needs fixing. */
        DWORD LastErr = GetLastError();
        return RTErrConvertFromWin32(LastErr);
    }

    return VINF_SUCCESS;

#elif defined(RT_OS_OS2)
    ULONG cbOS2Parm = cbData;
    int32_t vrc = VERR_INTERNAL_ERROR;
    ULONG cbOS2Data = sizeof(vrc);
    APIRET rc = DosDevIOCtl((uintptr_t)g_File, VBOXGUEST_IOCTL_CATEGORY, iFunction,
                            pvData, cbData, &cbOS2Parm,
                            &vrc, sizeof(vrc), &cbOS2Data);
    if (RT_LIKELY(!rc))
        return vrc;
    return RTErrConvertFromOS2(rc);

#else
# if defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD)
    VBGLBIGREQ Hdr;
    Hdr.u32Magic = VBGLBIGREQ_MAGIC;
    Hdr.cbData = cbData;
    Hdr.pvDataR3 = pvData;
#  if HC_ARCH_BITS == 32
    Hdr.u32Padding = 0;
#  endif
    pvData = &Hdr;

/** @todo test status code passing! Check that the kernel doesn't do any
 *        error checks using specific errno values, and just pass an VBox
 *        error instead of an errno.h one. Alternatively, extend/redefine the
 *        header with an error code return field (much better alternative
 *        actually). */
# elif defined(RT_OS_DARWIN) || defined(RT_OS_LINUX)
    NOREF(cbData);
# endif

# if defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD) || defined(RT_OS_NETBSD) || defined(RT_OS_DARWIN) || defined(RT_OS_LINUX)
#  ifdef VBOX_VBGLR3_XSERVER
    int rc = xf86ioctl((int)g_File, iFunction, pvData);
#  else
    if (g_File == NIL_RTFILE)
        return VERR_INVALID_HANDLE;
    int rc = ioctl(RTFileToNative(g_File), iFunction, pvData);
#  endif
# elif defined(RT_OS_HAIKU)
    /* The ioctl hook in Haiku does take the len parameter when specified,
     * so just use it. */
    int rc = ioctl((int)g_File, iFunction, pvData, cbData);
# else
#  error Port me!
# endif
    if (RT_LIKELY(rc == 0))
        return VINF_SUCCESS;

    /* Positive values are negated VBox error status codes. */
    if (rc > 0)
        rc = -rc;
    else
# ifdef VBOX_VBGLR3_XSERVER
        rc = VERR_FILE_IO_ERROR;
#  else
        rc = RTErrConvertFromErrno(errno);
# endif
    return rc;

#endif
}

