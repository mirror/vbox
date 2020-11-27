/* $Id$ */
/** @file
 * IPRT - Utility for running a (simple) HTTP server.
 *
 * Use this setup to best see what's going on:
 *    VBOX_LOG=rt_http=~0
 *    VBOX_LOG_DEST="nofile stderr"
 *    VBOX_LOG_FLAGS="unbuffered enabled thread msprog"
 *
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <signal.h>

#include <iprt/http.h>
#include <iprt/http-server.h>

#include <iprt/net.h> /* To make use of IPv4Addr in RTGETOPTUNION. */

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#endif


/*********************************************************************************************************************************
*   Definitations                                                                                                                *
*********************************************************************************************************************************/
typedef struct HTTPSERVERDATA
{
    /** The absolute path of the HTTP server's root directory. */
    char szPathRootAbs[RTPATH_MAX];
    /** The relative current working directory (CWD) to szRootDir. */
    char szCWD[RTPATH_MAX];
    union
    {
        RTFILE File;
        RTDIR Dir;
    } h;
} HTTPSERVERDATA;
typedef HTTPSERVERDATA *PHTTPSERVERDATA;

/**
 * Enumeration specifying the VFS handle type of the HTTP server.
 */
typedef enum HTTPSERVERVFSHANDLETYPE
{
    HTTPSERVERVFSHANDLETYPE_INVALID    = 0,
    HTTPSERVERVFSHANDLETYPE_FILE,
    HTTPSERVERVFSHANDLETYPE_DIR,
    /** The usual 32-bit hack. */
    HTTPSERVERVFSHANDLETYPE_32BIT_HACK = 0x7fffffff
} HTTPSERVERVFSHANDLETYPE;

/**
 * Structure for keeping a VFS handle of the HTTP server.
 */
typedef struct HTTPSERVERVFSHANDLE
{
    /** The type of the handle, stored in the union below. */
    HTTPSERVERVFSHANDLETYPE enmType;
    union
    {
        /** The VFS (chain) handle to use for this file. */
        RTVFSFILE hVfsFile;
        /** The VFS (chain) handle to use for this directory. */
        RTVFSDIR  hVfsDir;
    } u;
} HTTPSERVERVFSHANDLE;
typedef HTTPSERVERVFSHANDLE *PHTTPSERVERVFSHANDLE;

/**
 * HTTP directory entry.
 */
typedef struct RTHTTPDIRENTRY
{
    /** The information about the entry. */
    RTFSOBJINFO Info;
    /** Symbolic link target (allocated after the name). */
    const char *pszTarget;
    /** Owner if applicable (allocated after the name). */
    const char *pszOwner;
    /** Group if applicable (allocated after the name). */
    const char *pszGroup;
    /** The length of szName. */
    size_t      cchName;
    /** The entry name. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    char        szName[RT_FLEXIBLE_ARRAY];
} RTHTTPDIRENTRY;
/** Pointer to a HTTP directory entry. */
typedef RTHTTPDIRENTRY *PRTHTTPDIRENTRY;
/** Pointer to a HTTP directory entry pointer. */
typedef PRTHTTPDIRENTRY *PPRTHTTPDIRENTRY;

/**
 * Collection of HTTP directory entries.
 * Used for also caching stuff.
 */
typedef struct RTHTTPDIRCOLLECTION
{
    /** Current size of papEntries. */
    size_t                cEntries;
    /** Memory allocated for papEntries. */
    size_t                cEntriesAllocated;
    /** Current entries pending sorting and display. */
    PPRTHTTPDIRENTRY       papEntries;

    /** Total number of bytes allocated for the above entries. */
    uint64_t              cbTotalAllocated;
    /** Total number of file content bytes.    */
    uint64_t              cbTotalFiles;

} RTHTTPDIRCOLLECTION;
/** Pointer to a directory collection. */
typedef RTHTTPDIRCOLLECTION *PRTHTTPDIRCOLLECTION;
/** Pointer to a directory entry collection pointer. */
typedef PRTHTTPDIRCOLLECTION *PPRTHTTPDIRCOLLECTION;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Set by the signal handler when the HTTP server shall be terminated. */
static volatile bool  g_fCanceled = false;
static HTTPSERVERDATA g_HttpServerData;


#ifdef RT_OS_WINDOWS
static BOOL WINAPI signalHandler(DWORD dwCtrlType) RT_NOTHROW_DEF
{
    bool fEventHandled = FALSE;
    switch (dwCtrlType)
    {
        /* User pressed CTRL+C or CTRL+BREAK or an external event was sent
         * via GenerateConsoleCtrlEvent(). */
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_C_EVENT:
            ASMAtomicWriteBool(&g_fCanceled, true);
            fEventHandled = TRUE;
            break;
        default:
            break;
        /** @todo Add other events here. */
    }

    return fEventHandled;
}
#else /* !RT_OS_WINDOWS */
/**
 * Signal handler that sets g_fCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Don't do anything
 * unnecessary here.
 */
static void signalHandler(int iSignal) RT_NOTHROW_DEF
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fCanceled, true);
}
#endif

/**
 * Installs a custom signal handler to get notified
 * whenever the user wants to intercept the program.
 *
 * @todo Make this handler available for all VBoxManage modules?
 */
static int signalHandlerInstall(void)
{
    g_fCanceled = false;

    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)signalHandler, TRUE /* Add handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to install console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   signalHandler);
    signal(SIGTERM,  signalHandler);
# ifdef SIGBREAK
    signal(SIGBREAK, signalHandler);
# endif
#endif
    return rc;
}

/**
 * Uninstalls a previously installed signal handler.
 */
static int signalHandlerUninstall(void)
{
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)NULL, FALSE /* Remove handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to uninstall console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   SIG_DFL);
    signal(SIGTERM,  SIG_DFL);
# ifdef SIGBREAK
    signal(SIGBREAK, SIG_DFL);
# endif
#endif
    return rc;
}

static int dirOpen(const char *pszPathAbs, PRTVFSDIR phVfsDir)
{
    return RTVfsChainOpenDir(pszPathAbs, 0 /*fFlags*/, phVfsDir, NULL /* poffError */, NULL /* pErrInfo */);
}

static int dirClose(RTVFSDIR hVfsDir)
{
    RTVfsDirRelease(hVfsDir);

    return VINF_SUCCESS;
}

static int dirRead(RTVFSDIR hVfsDir, char **ppszEntry, PRTFSOBJINFO pInfo)
{
    size_t          cbDirEntryAlloced = sizeof(RTDIRENTRYEX);
    PRTDIRENTRYEX   pDirEntry         = (PRTDIRENTRYEX)RTMemTmpAlloc(cbDirEntryAlloced);
    if (!pDirEntry)
        return VERR_NO_MEMORY;

    int rc;

    for (;;)
    {
        size_t cbDirEntry = cbDirEntryAlloced;
        rc = RTVfsDirReadEx(hVfsDir, pDirEntry, &cbDirEntry, RTFSOBJATTRADD_UNIX);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_BUFFER_OVERFLOW)
            {
                RTMemTmpFree(pDirEntry);
                cbDirEntryAlloced = RT_ALIGN_Z(RT_MIN(cbDirEntry, cbDirEntryAlloced) + 64, 64);
                pDirEntry         = (PRTDIRENTRYEX)RTMemTmpAlloc(cbDirEntryAlloced);
                if (pDirEntry)
                    continue;
            }
            else if (rc != VERR_NO_MORE_FILES)
                break;
        }

        *ppszEntry = RTStrDup(pDirEntry->szName);
        AssertPtrReturn(*ppszEntry, VERR_NO_MEMORY);

        *pInfo = pDirEntry->Info;

        break;

    } /* for */

    RTMemTmpFree(pDirEntry);
    pDirEntry = NULL;

    return rc;
}

static int dirEntryWrite(char *pszBuf, size_t cbBuf,
                         const char *pszEntry, const PRTFSOBJINFO pInfo, size_t *pcbWritten)
{
    RT_NOREF(pInfo);

    ssize_t cch = RTStrPrintf2(pszBuf, cbBuf, "201: %s\r\n", pszEntry);
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    /*
    Content-type:
    Last-Modified:
    Content-Length:
    */

    *pcbWritten = (size_t)cch;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) onGetRequest(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq)
{
    PHTTPSERVERDATA pThis = (PHTTPSERVERDATA)pData->pvUser;
    Assert(pData->cbUser == sizeof(HTTPSERVERDATA));

    /* Construct absolute path. */
    char *pszPathAbs = NULL;
    if (RTStrAPrintf(&pszPathAbs, "%s/%s", pThis->szPathRootAbs, pReq->pszUrl) <= 0)
        return VERR_NO_MEMORY;

    RTVFSDIR hVfsDir;
    int rc = dirOpen(pszPathAbs, &hVfsDir);
    if (RT_SUCCESS(rc))
    {
        pReq->pvBody      = RTMemAlloc(_4K);
        AssertPtrReturn(pReq->pvBody, VERR_NO_MEMORY); /** @todo Leaks stuff. */
        pReq->cbBodyAlloc = _4K;
        pReq->cbBodyUsed  =  0;

        char *pszEntry  = NULL;
        RTFSOBJINFO fsObjInfo;

        while (RT_SUCCESS(rc = dirRead(hVfsDir, &pszEntry, &fsObjInfo)))
        {
            char *pszBody = (char *)pReq->pvBody;

            size_t cbWritten;
            rc = dirEntryWrite(&pszBody[pReq->cbBodyUsed], pReq->cbBodyAlloc - pReq->cbBodyUsed, pszEntry, &fsObjInfo, &cbWritten);
            if (rc == VERR_BUFFER_OVERFLOW)
            {
                pReq->cbBodyAlloc *= 2; /** @todo Improve this. */
                pReq->pvBody       = RTMemRealloc(pReq->pvBody, pReq->cbBodyAlloc);
                AssertPtrBreakStmt(pReq->pvBody, rc = VERR_NO_MEMORY);

                pszBody = (char *)pReq->pvBody;

                rc = dirEntryWrite(&pszBody[pReq->cbBodyUsed], pReq->cbBodyAlloc - pReq->cbBodyUsed, pszEntry, &fsObjInfo, &cbWritten);
            }

            if (RT_SUCCESS(rc))
                pReq->cbBodyUsed += cbWritten;

            RTStrFree(pszEntry);

            if (RT_FAILURE(rc))
                break;
        }

        if (rc == VERR_NO_MORE_FILES) /* All entries consumed? */
            rc = VINF_SUCCESS;

        dirClose(hVfsDir);
    }

    RTStrFree(pszPathAbs);
    return rc;
}

static DECLCALLBACK(int) onHeadRequest(PRTHTTPCALLBACKDATA pData, PRTHTTPSERVERREQ pReq)
{
    RT_NOREF(pData, pReq);

    return VINF_SUCCESS;
}

DECLCALLBACK(int) onOpen(PRTHTTPCALLBACKDATA pData, const char *pszUrl, uint64_t *pidObj)
{
    PHTTPSERVERDATA pThis = (PHTTPSERVERDATA)pData->pvUser;
    Assert(pData->cbUser == sizeof(HTTPSERVERDATA));

    /* Construct absolute path. */
    char *pszPathAbs = NULL;
    if (RTStrAPrintf(&pszPathAbs, "%s/%s", pThis->szPathRootAbs, pszUrl) <= 0)
        return VERR_NO_MEMORY;

    int rc = RTFileOpen(&pThis->h.File, pszPathAbs,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
        *pidObj = 42;

    RTStrFree(pszPathAbs);
    return rc;
}

DECLCALLBACK(int) onRead(PRTHTTPCALLBACKDATA pData, uint64_t idObj, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    PHTTPSERVERDATA pThis = (PHTTPSERVERDATA)pData->pvUser;
    Assert(pData->cbUser == sizeof(HTTPSERVERDATA));

    AssertReturn(idObj == 42, VERR_NOT_FOUND);

    return RTFileRead(pThis->h.File, pvBuf, cbBuf, pcbRead);
}

DECLCALLBACK(int) onClose(PRTHTTPCALLBACKDATA pData, uint64_t idObj)
{
    PHTTPSERVERDATA pThis = (PHTTPSERVERDATA)pData->pvUser;
    Assert(pData->cbUser == sizeof(HTTPSERVERDATA));

    AssertReturn(idObj == 42, VERR_NOT_FOUND);

    int rc = RTFileClose(pThis->h.File);
    if (RT_SUCCESS(rc))
        pThis->h.File = NIL_RTFILE;

    return rc;
}

DECLCALLBACK(int) onQueryInfo(PRTHTTPCALLBACKDATA pData, const char *pszUrl, PRTFSOBJINFO pObjInfo)
{
    PHTTPSERVERDATA pThis = (PHTTPSERVERDATA)pData->pvUser;
    Assert(pData->cbUser == sizeof(HTTPSERVERDATA));

    /* Construct absolute path. */
    char *pszPathAbs = NULL;
    if (RTStrAPrintf(&pszPathAbs, "%s/%s", pThis->szPathRootAbs, pszUrl) <= 0)
        return VERR_NO_MEMORY;

    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszPathAbs,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileQueryInfo(hFile, pObjInfo, RTFSOBJATTRADD_NOTHING);

        RTFileClose(hFile);
    }

    return rc;
}

int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /* Use some sane defaults. */
    char     szAddress[64] = "localhost";
    uint16_t uPort         = 8080;

    RT_ZERO(g_HttpServerData);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--address",      'a', RTGETOPT_REQ_IPV4ADDR }, /** @todo Use a string for DNS hostnames? */
        /** @todo Implement IPv6 support? */
        { "--port",         'p', RTGETOPT_REQ_UINT16 },
        { "--root-dir",     'r', RTGETOPT_REQ_STRING },
        { "--verbose",      'v', RTGETOPT_REQ_NOTHING }
    };

    RTEXITCODE      rcExit          = RTEXITCODE_SUCCESS;
    unsigned        uVerbosityLevel = 1;

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'a':
                RTStrPrintf2(szAddress, sizeof(szAddress), "%RU8.%RU8.%RU8.%RU8", /** @todo Improve this. */
                             ValueUnion.IPv4Addr.au8[0], ValueUnion.IPv4Addr.au8[1], ValueUnion.IPv4Addr.au8[2], ValueUnion.IPv4Addr.au8[3]);
                break;

            case 'p':
                uPort = ValueUnion.u16;
                break;

            case 'r':
                RTStrCopy(g_HttpServerData.szPathRootAbs, sizeof(g_HttpServerData.szPathRootAbs), ValueUnion.psz);
                break;

            case 'v':
                uVerbosityLevel++;
                break;

            case 'h':
                RTPrintf("Usage: %s [options]\n"
                         "\n"
                         "Options:\n"
                         "  -a, --address (default: localhost)\n"
                         "      Specifies the address to use for listening.\n"
                         "  -p, --port (default: 8080)\n"
                         "      Specifies the port to use for listening.\n"
                         "  -r, --root-dir (default: current dir)\n"
                         "      Specifies the root directory being served.\n"
                         "  -v, --verbose\n"
                         "      Controls the verbosity level.\n"
                         "  -h, -?, --help\n"
                         "      Display this help text and exit successfully.\n"
                         "  -V, --version\n"
                         "      Display the revision and exit successfully.\n"
                         , RTPathFilename(argv[0]));
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("$Revision$\n");
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    if (!strlen(g_HttpServerData.szPathRootAbs))
    {
        /* By default use the current directory as serving root directory. */
        rc = RTPathGetCurrent(g_HttpServerData.szPathRootAbs, sizeof(g_HttpServerData.szPathRootAbs));
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Retrieving current directory failed: %Rrc", rc);
    }

    /* Initialize CWD. */
    RTStrPrintf2(g_HttpServerData.szCWD, sizeof(g_HttpServerData.szCWD), "/");

    /* Install signal handler. */
    rc = signalHandlerInstall();
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the HTTP server instance.
         */
        RTHTTPSERVERCALLBACKS Callbacks;
        RT_ZERO(Callbacks);

        Callbacks.pfnOpen          = onOpen;
        Callbacks.pfnRead          = onRead;
        Callbacks.pfnClose         = onClose;
        Callbacks.pfnQueryInfo     = onQueryInfo;
        Callbacks.pfnOnGetRequest  = onGetRequest;
        Callbacks.pfnOnHeadRequest = onHeadRequest;

        g_HttpServerData.h.File = NIL_RTFILE;
        g_HttpServerData.h.Dir  = NIL_RTDIR;

        RTHTTPSERVER hHTTPServer;
        rc = RTHttpServerCreate(&hHTTPServer, szAddress, uPort, &Callbacks,
                                &g_HttpServerData, sizeof(g_HttpServerData));
        if (RT_SUCCESS(rc))
        {
            RTPrintf("Starting HTTP server at %s:%RU16 ...\n", szAddress, uPort);
            RTPrintf("Root directory is '%s'\n", g_HttpServerData.szPathRootAbs);

            RTPrintf("Running HTTP server ...\n");

            for (;;)
            {
                RTThreadSleep(200);

                if (g_fCanceled)
                    break;
            }

            RTPrintf("Stopping HTTP server ...\n");

            int rc2 = RTHttpServerDestroy(hHTTPServer);
            if (RT_SUCCESS(rc))
                rc = rc2;

            RTPrintf("Stopped HTTP server\n");
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTHttpServerCreate failed: %Rrc", rc);

        int rc2 = signalHandlerUninstall();
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    /* Set rcExit on failure in case we forgot to do so before. */
    if (RT_FAILURE(rc))
        rcExit = RTEXITCODE_FAILURE;

    return rcExit;
}

