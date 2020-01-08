/* $Id$ */
/** @file
 * Generic FTP server (RFC 959) implementation.
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

/**
 * Known limitations so far:
 * - UTF-8 support only.
 * - No support for writing / modifying ("DELE", "MKD", "RMD", "STOR", ++).
 * - No FTPS / SFTP support.
 * - No passive mode ("PASV") support.
 * - No proxy support.
 * - No FXP support.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FTP
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/ftp.h>
#include <iprt/mem.h>
#include <iprt/log.h>
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/tcp.h>

#include "internal/magics.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Internal FTP server instance.
 */
typedef struct RTFTPSERVERINTERNAL
{
    /** Magic value. */
    uint32_t            u32Magic;
    /** Pointer to TCP server instance. */
    PRTTCPSERVER        pTCPServer;
} RTFTPSERVERINTERNAL;
/** Pointer to an internal FTP server instance. */
typedef RTFTPSERVERINTERNAL *PRTFTPSERVERINTERNAL;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTFTPSERVER_VALID_RETURN_RC(hFTPServer, a_rc) \
    do { \
        AssertPtrReturn((hFTPServer), (a_rc)); \
        AssertReturn((hFTPServer)->u32Magic == RTFTPSERVER_MAGIC, (a_rc)); \
    } while (0)

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTFTPSERVER_VALID_RETURN(hFTPServer) RTFTPSERVER_VALID_RETURN_RC((hFTPServer), VERR_INVALID_HANDLE)

/** Validates a handle and returns (void) if not valid. */
#define RTFTPSERVER_VALID_RETURN_VOID(hFTPServer) \
    do { \
        AssertPtrReturnVoid(hFTPServer); \
        AssertReturnVoid((hFTPServer)->u32Magic == RTFTPSERVER_MAGIC); \
    } while (0)

/** Supported FTP server command IDs.
 *  Alphabetically, named after their official command names. */
typedef enum RTFTPSERVER_CMD
{
    /** Invalid command, do not use. Always must come first. */
    RTFTPSERVER_CMD_INVALID = 0,
    /** Aborts the current command on the server. */
    RTFTPSERVER_CMD_ABOR,
    /** Changes the current working directory. */
    RTFTPSERVER_CMD_CDUP,
    /** Changes the current working directory. */
    RTFTPSERVER_CMD_CWD,
    /** Lists a directory. */
    RTFTPSERVER_CMD_LIST,
    /** Sets the transfer mode. */
    RTFTPSERVER_CMD_MODE,
    /** Sends a nop ("no operation") to the server. */
    RTFTPSERVER_CMD_NOOP,
    /** Sets the password for authentication. */
    RTFTPSERVER_CMD_PASS,
    /** Sets the port to use for the data connection. */
    RTFTPSERVER_CMD_PORT,
    /** Gets the current working directory. */
    RTFTPSERVER_CMD_PWD,
    /** Terminates the session (connection). */
    RTFTPSERVER_CMD_QUIT,
    /** Retrieves a specific file. */
    RTFTPSERVER_CMD_RETR,
    /** Recursively gets a directory (and its contents). */
    RTFTPSERVER_CMD_RGET,
    /** Retrieves the current status of a transfer. */
    RTFTPSERVER_CMD_STAT,
    /** Gets the server's OS info. */
    RTFTPSERVER_CMD_SYST,
    /** Sets the (data) representation type. */
    RTFTPSERVER_CMD_TYPE,
    /** Sets the user name for authentication. */
    RTFTPSERVER_CMD_USER,
    /** End marker. */
    RTFTPSERVER_CMD_LAST,
    /** The usual 32-bit hack. */
    RTFTPSERVER_CMD_32BIT_HACK = 0x7fffffff
} RTFTPSERVER_CMD;

/**
 * Structure for maintaining an internal FTP server client state.
 */
typedef struct RTFTPSERVERCLIENTSTATE
{
    /** Pointer to internal server state. */
    PRTFTPSERVERINTERNAL pServer;
    /** Socket handle the client is bound to. */
    RTSOCKET             hSocket;
} RTFTPSERVERCLIENTSTATE;
/** Pointer to an internal FTP server client state. */
typedef RTFTPSERVERCLIENTSTATE *PRTFTPSERVERCLIENTSTATE;

/** Function pointer declaration for a specific FTP server command handler. */
typedef DECLCALLBACK(int) FNRTFTPSERVERCMD(PRTFTPSERVERCLIENTSTATE pClient);
/** Pointer to a FNRTFTPSERVERCMD(). */
typedef FNRTFTPSERVERCMD *PFNRTFTPSERVERCMD;

/**
 * Function prototypes for command handlers.
 */
static FNRTFTPSERVERCMD rtFTPServerHandleABOR;
static FNRTFTPSERVERCMD rtFTPServerHandleCDUP;
static FNRTFTPSERVERCMD rtFTPServerHandleCWD;
static FNRTFTPSERVERCMD rtFTPServerHandleLIST;
static FNRTFTPSERVERCMD rtFTPServerHandleMODE;
static FNRTFTPSERVERCMD rtFTPServerHandleNOOP;
static FNRTFTPSERVERCMD rtFTPServerHandlePORT;
static FNRTFTPSERVERCMD rtFTPServerHandlePWD;
static FNRTFTPSERVERCMD rtFTPServerHandleQUIT;
static FNRTFTPSERVERCMD rtFTPServerHandleRETR;
static FNRTFTPSERVERCMD rtFTPServerHandleRGET;
static FNRTFTPSERVERCMD rtFTPServerHandleSTAT;
static FNRTFTPSERVERCMD rtFTPServerHandleSYST;
static FNRTFTPSERVERCMD rtFTPServerHandleTYPE;

/**
 * Structure for maintaining a single command entry for the command table.
 */
typedef struct RTFTPSERVER_CMD_ENTRY
{
    /** Command ID. */
    RTFTPSERVER_CMD    enmCmd;
    /** Command represented as ASCII string. */
    char               szCmd[RTFTPSERVER_MAX_CMD_LEN];
    /** Function pointer invoked to handle the command. */
    PFNRTFTPSERVERCMD  pfnCmd;
} RTFTPSERVER_CMD_ENTRY;

/**
 * Table of handled commands.
 */
const RTFTPSERVER_CMD_ENTRY g_aCmdMap[] =
{
    { RTFTPSERVER_CMD_ABOR,     "ABOR",         rtFTPServerHandleABOR },
    { RTFTPSERVER_CMD_CDUP,     "CDUP",         rtFTPServerHandleCDUP },
    { RTFTPSERVER_CMD_CWD,      "CWD",          rtFTPServerHandleCWD  },
    { RTFTPSERVER_CMD_LIST,     "LIST",         rtFTPServerHandleLIST },
    { RTFTPSERVER_CMD_MODE,     "MODE",         rtFTPServerHandleMODE },
    { RTFTPSERVER_CMD_NOOP,     "NOOP",         rtFTPServerHandleNOOP },
    { RTFTPSERVER_CMD_PORT,     "PORT",         rtFTPServerHandlePORT },
    { RTFTPSERVER_CMD_PWD,      "PWD",          rtFTPServerHandlePWD  },
    { RTFTPSERVER_CMD_QUIT,     "QUIT",         rtFTPServerHandleQUIT },
    { RTFTPSERVER_CMD_RETR,     "RETR",         rtFTPServerHandleRETR },
    { RTFTPSERVER_CMD_RGET,     "RGET",         rtFTPServerHandleRGET },
    { RTFTPSERVER_CMD_STAT,     "STAT",         rtFTPServerHandleSTAT },
    { RTFTPSERVER_CMD_SYST,     "SYST",         rtFTPServerHandleSYST },
    { RTFTPSERVER_CMD_TYPE,     "TYPE",         rtFTPServerHandleTYPE },
    { RTFTPSERVER_CMD_LAST,     "",             NULL }
};


static int rtFTPServerSendReplyRc(PRTFTPSERVERCLIENTSTATE pClient, RTFTPSERVER_REPLY enmReply)
{
    char szReply[32];
    RTStrPrintf2(szReply, sizeof(szReply), "%RU32\r\n", enmReply);

    return RTTcpWrite(pClient->hSocket, szReply, strlen(szReply) + 1);
}

static int rtFTPServerSendReplyStr(PRTFTPSERVERCLIENTSTATE pClient, const char *pcszStr)
{
    char *pszReply;
    int rc = RTStrAPrintf(&pszReply, "%s\r\n", pcszStr);
    if (RT_SUCCESS(rc))
    {
        rc = RTTcpWrite(pClient->hSocket, pszReply, strlen(pszReply) + 1);
        RTStrFree(pszReply);
        return rc;
    }

    return VERR_NO_MEMORY;
}

static int rtFTPServerLookupUser(PRTFTPSERVERCLIENTSTATE pClient, const char *pcszUser)
{
    RT_NOREF(pClient, pcszUser);

    LogFunc(("User='%s'\n", pcszUser));

    return VINF_SUCCESS; /** @todo Implement lookup. */
}

static int rtFTPServerAuthenticate(PRTFTPSERVERCLIENTSTATE pClient, const char *pcszUser, const char *pcszPassword)
{
    RT_NOREF(pClient, pcszUser, pcszPassword);

    LogFunc(("User='%s', Password='%s'\n", pcszUser, pcszPassword));

    return VINF_SUCCESS; /** @todo Implement authentication. */
}

static int rtFTPServerHandleABOR(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleCDUP(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleCWD(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleLIST(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleMODE(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleNOOP(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandlePORT(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandlePWD(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleQUIT(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleRETR(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleRGET(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleSTAT(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerHandleSYST(PRTFTPSERVERCLIENTSTATE pClient)
{
    char szOSInfo[64];
    int rc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szOSInfo, sizeof(szOSInfo));
    if (RT_SUCCESS(rc))
        rc = rtFTPServerSendReplyStr(pClient, szOSInfo);

    return rc;
}

static int rtFTPServerHandleTYPE(PRTFTPSERVERCLIENTSTATE pClient)
{
    RT_NOREF(pClient);

    /** @todo Anything to do here? */
    return VINF_SUCCESS;
}

static int rtFTPServerDoLogin(PRTFTPSERVERCLIENTSTATE pClient)
{
    LogFlowFuncEnter();

    /* Send welcome message. */
    int rc = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_READY_FOR_NEW_USER);
    if (RT_SUCCESS(rc))
    {
        size_t cbRead;

        char szUser[64];
        rc = RTTcpRead(pClient->hSocket, szUser, sizeof(szUser), &cbRead);
        if (RT_SUCCESS(rc))
        {
            rc = rtFTPServerLookupUser(pClient, szUser);
            if (RT_SUCCESS(rc))
            {
                rc = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_USERNAME_OKAY_NEED_PASSWORD);
                if (RT_SUCCESS(rc))
                {
                    char szPass[64];
                    rc = RTTcpRead(pClient->hSocket, szPass, sizeof(szPass), &cbRead);
                    {
                        if (RT_SUCCESS(rc))
                        {
                            rc = rtFTPServerAuthenticate(pClient, szUser, szPass);
                            if (RT_SUCCESS(rc))
                            {
                                rc = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_LOGGED_IN_PROCEED);
                            }
                        }
                    }
                }
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        int rc2 = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_NOT_LOGGED_IN);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

static int rtFTPServerProcessCommands(PRTFTPSERVERCLIENTSTATE pClient)
{
    int rc;

    for (;;)
    {
        size_t cbRead;
        char   szCmd[RTFTPSERVER_MAX_CMD_LEN];
        rc = RTTcpRead(pClient->hSocket, szCmd, sizeof(szCmd), &cbRead);
        if (   RT_SUCCESS(rc)
            && strlen(szCmd))
        {
            /* A tiny bit of sanitation. */
            RTStrStripL(szCmd);

            /* First, terminate string by finding the command end marker (telnet style). */
            /** @todo Not sure if this is entirely correct and/or needs tweaking; good enough for now as it seems. */
            char *pszCmdEnd = RTStrIStr(szCmd, "\r\n");
            if (pszCmdEnd)
                *pszCmdEnd = '\0';

            /* Second, determine if there is any parameters following. */
            char *pszCmdParms = RTStrIStr(szCmd, " ");
            /* pszCmdParms can be NULL if command has not parameters. */
            RT_NOREF(pszCmdParms);

            unsigned i = 0;
            for (; i < RT_ELEMENTS(g_aCmdMap); i++)
            {
                if (!RTStrICmp(szCmd, g_aCmdMap[i].szCmd))
                {
                    rc = g_aCmdMap[i].pfnCmd(pClient);
                    break;
                }
            }

            if (i == RT_ELEMENTS(g_aCmdMap))
            {
                int rc2 = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_ERROR_CMD_NOT_IMPL);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }

            if (g_aCmdMap[i].enmCmd == RTFTPSERVER_CMD_QUIT)
                break;
        }
        else
        {
            int rc2 = rtFTPServerSendReplyRc(pClient, RTFTPSERVER_REPLY_ERROR_CMD_NOT_RECOGNIZED);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

    return rc;
}

static DECLCALLBACK(int) rtFTPServerThread(RTSOCKET hSocket, void *pvUser)
{
    PRTFTPSERVERINTERNAL pThis = (PRTFTPSERVERINTERNAL)pvUser;
    RTFTPSERVER_VALID_RETURN(pThis);

    RTFTPSERVERCLIENTSTATE ClientState;
    RT_ZERO(ClientState);

    ClientState.pServer = pThis;
    ClientState.hSocket = hSocket;

    int rc = rtFTPServerDoLogin(&ClientState);
    if (RT_SUCCESS(rc))
        rc = rtFTPServerProcessCommands(&ClientState);

    return rc;
}

RTR3DECL(int) RTFTPServerCreate(PRTFTPSERVER phFTPServer, const char *pcszAddress, uint16_t uPort,
                                const char *pcszPathRoot)
{
    AssertPtrReturn(phFTPServer,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcszAddress,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcszPathRoot, VERR_INVALID_POINTER);
    AssertReturn   (uPort,        VERR_INVALID_PARAMETER);

    int rc;

    PRTFTPSERVERINTERNAL pThis = (PRTFTPSERVERINTERNAL)RTMemAllocZ(sizeof(RTFTPSERVERINTERNAL));
    if (pThis)
    {
        pThis->u32Magic = RTFTPSERVER_MAGIC;

        rc = RTTcpServerCreate(pcszAddress, uPort, RTTHREADTYPE_DEFAULT, "ftpsrv",
                               rtFTPServerThread, pThis /* pvUser */, &pThis->pTCPServer);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

RTR3DECL(int) RTFTPServerDestroy(RTFTPSERVER hFTPServer)
{
    if (hFTPServer == NIL_RTFTPSERVER)
        return VINF_SUCCESS;

    PRTFTPSERVERINTERNAL pThis = hFTPServer;
    RTFTPSERVER_VALID_RETURN(pThis);

    AssertPtr(pThis->pTCPServer);

    int rc = RTTcpServerDestroy(pThis->pTCPServer);
    if (RT_SUCCESS(rc))
    {
        pThis->u32Magic = RTFTPSERVER_MAGIC_DEAD;

        RTMemFree(pThis);
    }

    return rc;
}

