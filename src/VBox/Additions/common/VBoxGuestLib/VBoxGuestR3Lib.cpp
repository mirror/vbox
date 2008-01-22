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

# include <iprt/alloca.h>
# include <iprt/string.h>
#elif defined(RT_OS_LINUX)
# include <sys/stat.h>
# include <fcntl.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/time.h>
# include <sys/resource.h>
#elif defined(RT_OS_SOLARIS)
# include <sys/types.h>
# include <sys/stat.h>
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <signal.h>
# include <fcntl.h>
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

     /* PORTME */
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

#elif defined(RT_OS_SOLARIS)
    VBGLBIGREQ Hdr;
    Hdr.u32Magic = VBGLBIGREQ_MAGIC;
    Hdr.cbData = cbData;
    Hdr.pvDataR3 = pvData;

    int rc = ioctl((int)g_File, iFunction, &Hdr);
    if (rc == -1)
        rc = errno;
    return rc;

#else
    /* Default implementation (linux, solaris). */
    int rc2 = VERR_INTERNAL_ERROR;
    int rc = RTFileIoCtl(g_File, (int)iFunction, pvData, cbData, &rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
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


VBGLR3DECL(int) VbglR3GetMouseStatus(uint32_t *pfFeatures, uint32_t *px, uint32_t *py)
{
    VMMDevReqMouseStatus Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_GetMouseStatus);
    Req.mouseFeatures = 0;
    Req.pointerXPos = 0;
    Req.pointerYPos = 0;
    int rc = VbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
    {
        if (pfFeatures)
            *pfFeatures = Req.mouseFeatures;
        if (px)
            *px = Req.pointerXPos;
        if (py)
            *py = Req.pointerYPos;
    }
    return rc;
}


VBGLR3DECL(int) VbglR3SetMouseStatus(uint32_t fFeatures)
{
    VMMDevReqMouseStatus Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_SetMouseStatus);
    Req.mouseFeatures = fFeatures;
    Req.pointerXPos = 0;
    Req.pointerYPos = 0;
    return VbglR3GRPerform(&Req.header);
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


/**
 * Write to the backdoor logger from ring 3 guest code.
 *
 * @returns IPRT status code
 *
 * @remarks This currently does not accept more than 255 bytes of data at
 *          one time. It should probably be rewritten to use pass a pointer
 *          in the IOCtl.
 */
VBGLR3DECL(int) VbglR3WriteLog(const char *pch, size_t cb)
{
    /*
     * Solaris does not accept more than 255 bytes of data per ioctl request,
     * so split large string into 128 byte chunks to prevent truncation.
     */
#define STEP 128 /** @todo increase to 512 when solaris ioctl code is fixed. (darwin limits us to 1024 IIRC) */
    int rc = VINF_SUCCESS;
    for (size_t off = 0; off < cb && RT_SUCCESS(rc); off += STEP)
    {
        size_t cbStep = RT_MIN(cb - off, STEP);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cbStep), (char *)pch + off, cbStep);
    }
#undef STEP
    return rc;
}


/**
 * Change the IRQ filter mask.
 *
 * @returns IPRT status code
 * @param   fOr     The OR mask.
 * @param   fNo     The NOT mask.
 */
VBGLR3DECL(int) VbglR3CtlFilterMask(uint32_t fOr, uint32_t fNot)
{
    VBoxGuestFilterMaskInfo Info;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CTL_FILTER_MASK, &Info, sizeof(Info));
}


/**
 * Daemonize the process for running in the background.
 *
 * @returns 0 on success
 *
 * @param   nochdir     Pass 0 to change working directory to root.
 * @param   noclose     Pass 0 to redirect standard file streams to /dev/null.
 */
VBGLR3DECL(int) VbglR3Daemonize(int nochdir, int noclose)
{
#if defined(RT_OS_LINUX)
    /** rlimit structure for finding out how many open files we may have. */
    struct rlimit rlim;

    /* To make sure that we are not currently a session leader, we must first fork and let
       the parent process exit, as a newly created child is never session leader.  This will
       allow us to call setsid() later. */
    if (fork() != 0)
    {
        exit(0);
    }
    /* Find the maximum number of files we can have open and close them all. */
    if (0 != getrlimit(RLIMIT_NOFILE, &rlim))
    {
        /* For some reason the call failed.  In that case we will just close the three
           standard files and hope. */
        rlim.rlim_cur = 3;
    }
    for (unsigned int i = 0; i < rlim.rlim_cur; ++i)
    {
        close(i);
    }
    /* Change to the root directory to avoid keeping the one we were started in open. */
    chdir("/");
    /* Set our umask to zero. */
    umask(0);
    /* And open /dev/null on stdin/out/err. */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    dup(1);
    /* Detach from the controlling terminal by creating our own session, to avoid receiving
       signals from the old session. */
    setsid();
    /* And fork again, letting the parent exit, to make us a child of init and avoid zombies. */
    if (fork() != 0)
    {
        exit(0);
    }
    NOREF(nochdir);
    NOREF(noclose);

    return 0;

#elif defined(RT_OS_OS2)
    PPIB pPib;
    PTIB pTib;
    DosGetInfoBlocks(&pTib, &pPib);

    /* Get the full path to the executable. */
    char szExe[CCHMAXPATH];
    APIRET rc = DosQueryModuleName(pPib->pib_hmte, sizeof(szExe), szExe);
    if (rc)
    {
        errno = EDOOFUS;
        return -1;
    }

    /* calc the length of the command line. */
    char *pch = pPib->pib_pchcmd;
    size_t cch0 = strlen(pch);
    pch += cch0 + 1;
    size_t cch1 = strlen(pch);
    pch += cch1 + 1;
    char *pchArgs;
    if (cch1 && *pch)
    {
        do  pch = strchr(pch, '\0') + 1;
        while (*pch);

        size_t cchTotal = pch - pPib->pib_pchcmd;
        pchArgs = (char *)alloca(cchTotal + sizeof("--daemonized\0\0"));
        memcpy(pchArgs, pPib->pib_pchcmd, cchTotal - 1);
        memcpy(pchArgs + cchTotal - 1, "--daemonized\0\0", sizeof("--daemonized\0\0"));
    }
    else
    {
        size_t cchTotal = pch - pPib->pib_pchcmd + 1;
        pchArgs = (char *)alloca(cchTotal + sizeof(" --daemonized "));
        memcpy(pchArgs, pPib->pib_pchcmd, cch0 + 1);
        pch = pchArgs + cch0 + 1;
        memcpy(pch, " --daemonized ", sizeof(" --daemonized ") - 1);
        pch += sizeof(" --daemonized ") - 1;
        if (cch1)
            memcpy(pch, pPib->pib_pchcmd + cch0 + 1, cch1 + 2);
        else
            pch[0] = pch[1] = '\0';
    }

    /* spawn a detach process  */
    char szObj[128];
    RESULTCODES ResCodes = { 0, 0 };
    szObj[0] = '\0';
    rc = DosExecPgm(szObj, sizeof(szObj), EXEC_BACKGROUND, (PCSZ)pchArgs, NULL, &ResCodes, (PCSZ)szExe);
    if (rc)
    {
        /** @todo Change this to some standard log/print error?? */
        /* VBoxServiceError("DosExecPgm failed with rc=%d and szObj='%s'\n", rc, szObj); */
        errno = EDOOFUS;
        return -1;
    }
    DosExit(EXIT_PROCESS, 0);
    return -1;

#elif defined(RT_OS_SOLARIS)
    if (getppid() == 1) /* We already belong to init process */
        return -1;

    pid_t pid = fork();
    if (pid < 0)         /* The fork() failed. Bad. */
        return -1;

    if (pid > 0)         /* Quit parent process */
        exit(0);

    /*
     * The orphaned child becomes a daemon after attaching to init. We need to get
     * rid of signals, file descriptors & other stuff we inherited from the parent.
     */
    pid_t newpgid = setsid();
    if (newpgid < 0)     /* Failed to create new sesion */
        return -1;

    /* BSD daemon style. */
    if (!noclose)
    {
        /* Open stdin(0), stdout(1) and stderr(2) to /dev/null */
        int fd = open("/dev/null", O_RDWR);
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2)
            close(fd);
    }

    /* Switch our current directory to root */
    if (!nochdir)
        chdir("/");     /* @todo Check if switching to '/' is the convention for Solaris daemons. */

    /* Set file permission to something secure, as we need to run as root on Solaris */
    umask(027);
    return 0;
#endif
}

