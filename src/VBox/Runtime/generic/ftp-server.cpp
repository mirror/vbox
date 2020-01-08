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
#include <iprt/poll.h>
#include <iprt/socket.h>
#include <iprt/string.h>
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
    RTFTPSERVER_CMD_LS,
    /** Sends a nop ("no operation") to the server. */
    RTFTPSERVER_CMD_NOOP,
    /** Sets the password for authentication. */
    RTFTPSERVER_CMD_PASS,
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
    /** Sets the (data) representation type. */
    RTFTPSERVER_CMD_TYPE,
    /** Sets the user name for authentication. */
    RTFTPSERVER_CMD_USER,
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


static int rtFTPServerSendReply(PRTFTPSERVERCLIENTSTATE pClient, RTFTPSERVER_REPLY enmReply)
{
    RT_NOREF(enmReply);

    RTTcpWrite(pClient->hSocket, "hello\n", sizeof("hello\n") - 1);

    int rc =  0;
    RT_NOREF(rc);

    return rc;
}

static int rtFTPServerDoLogin(PRTFTPSERVERCLIENTSTATE pClient)
{
    int rc = rtFTPServerSendReply(pClient, RTFTPSERVER_REPLY_READY_FOR_NEW_USER);

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

