/* $Id$ */
/** @file
 * Header file for FTP client / server implementations.
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

#ifndef IPRT_INCLUDED_ftp_h
#define IPRT_INCLUDED_ftp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_ftp   RTFTPServer - FTP server implementation.
 * @ingroup grp_rt
 * @{
 */

/** @todo the following three definitions may move the iprt/types.h later. */
/** FTP server handle. */
typedef R3PTRTYPE(struct RTFTPSERVERINTERNAL *) RTFTPSERVER;
/** Pointer to a FTP server handle. */
typedef RTFTPSERVER                            *PRTFTPSERVER;
/** Nil FTP client handle. */
#define NIL_RTFTPSERVER                         ((RTFTPSERVER)0)

/** Maximum length (in characters) a command can have (without parameters). */
#define RTFTPSERVER_MAX_CMD_LEN                 64

/**
 * Enumeration for defining the current server connection mode.
 */
typedef enum RTFTPSERVER_CONNECTION_MODE
{
    /** Normal mode, nothing to transfer. */
    RTFTPSERVER_CONNECTION_MODE_NORMAL = 0,
    /** Server is in passive mode (is listening). */
    RTFTPSERVER_CONNECTION_MODE_PASSIVE,
    /** Server connects via port to the client. */
    RTFTPSERVER_CONNECTION_MODE_MODE_PORT,
    /** The usual 32-bit hack. */
    RTFTPSERVER_CONNECTION_MODE_32BIT_HACK = 0x7fffffff
} RTFTPSERVER_CONNECTION_MODE;

/**
 * Enumeration for defining the data transfer mode.
 */
typedef enum RTFTPSERVER_TRANSFER_MODE
{
    RTFTPSERVER_TRANSFER_MODE_UNKNOWN = 0,
    RTFTPSERVER_TRANSFER_MODE_STREAM,
    RTFTPSERVER_TRANSFER_MODE_BLOCK,
    RTFTPSERVER_TRANSFER_MODE_COMPRESSED,
    /** The usual 32-bit hack. */
    RTFTPSERVER_DATA_MODE_32BIT_HACK = 0x7fffffff
} RTFTPSERVER_DATA_MODE;

/**
 * Enumeration for defining the data type.
 */
typedef enum RTFTPSERVER_DATA_TYPE
{
    RTFTPSERVER_DATA_TYPE_UNKNOWN = 0,
    RTFTPSERVER_DATA_TYPE_ASCII,
    RTFTPSERVER_DATA_TYPE_EBCDIC,
    RTFTPSERVER_DATA_TYPE_IMAGE,
    RTFTPSERVER_DATA_TYPE_LOCAL,
    /** The usual 32-bit hack. */
    RTFTPSERVER_DATA_TYPE_32BIT_HACK = 0x7fffffff
} RTFTPSERVER_DATA_TYPE;

/**
 * Enumeration for FTP server reply codes.
 *
 ** @todo Might needs more codes, not complete yet.
 */
typedef enum RTFTPSERVER_REPLY
{
    /** Invalid reply type, do not use. */
    RTFTPSERVER_REPLY_INVALID                        = 0,
    /** Command not implemented, superfluous at this site. */
    RTFTPSERVER_REPLY_ERROR_CMD_NOT_IMPL_SUPERFLUOUS = 202,
    /** Command okay. */
    RTFTPSERVER_REPLY_OKAY                           = 200,
    /** Service ready for new user. */
    RTFTPSERVER_REPLY_READY_FOR_NEW_USER             = 220,
    /** Closing data connection. */
    RTFTPSERVER_REPLY_CLOSING_DATA_CONN              = 226,
    /** User logged in, proceed. */
    RTFTPSERVER_REPLY_LOGGED_IN_PROCEED              = 230,
    /** User name okay, need password. */
    RTFTPSERVER_REPLY_USERNAME_OKAY_NEED_PASSWORD    = 331,
    /** Connection closed; transfer aborted. */
    RTFTPSERVER_REPLY_CONN_CLOSED_TRANSFER_ABORTED   = 426,
    /** Syntax error, command unrecognized. */
    RTFTPSERVER_REPLY_ERROR_CMD_NOT_RECOGNIZED       = 500,
    /** Syntax error in parameters or arguments. */
    RTFTPSERVER_REPLY_ERROR_INVALID_PARAMETERS       = 501,
    /** Command not implemented. */
    RTFTPSERVER_REPLY_ERROR_CMD_NOT_IMPL             = 502,
    /** Bad sequence of commands. */
    RTFTPSERVER_REPLY_ERROR_BAD_SEQUENCE             = 503,
    /** Command not implemented for that parameter. */
    RTFTPSERVER_REPLY_ERROR_CMD_NOT_IMPL_PARAM       = 504,
    /** Not logged in. */
    RTFTPSERVER_REPLY_NOT_LOGGED_IN                  = 530,
    /** The usual 32-bit hack. */
    RTFTPSERVER_REPLY_32BIT_HACK                     = 0x7fffffff
} RTFTPSERVER_REPLY;

/**
 * Structure for maintaining a FTP server client state.
 */
typedef struct RTFTPSERVERCLIENTSTATE
{
    /** Timestamp (in ms) of last command issued by the client. */
    uint64_t      tsLastCmdMs;
} RTFTPSERVERCLIENTSTATE;
/** Pointer to a FTP server client state. */
typedef RTFTPSERVERCLIENTSTATE *PRTFTPSERVERCLIENTSTATE;

/**
 * Structure for storing FTP server callback data.
 */
typedef struct RTFTPCALLBACKDATA
{
    /** Pointer to the client state. */
    PRTFTPSERVERCLIENTSTATE  pClient;
    /** Saved user pointer. */
    void                    *pvUser;
    /** Size (in bytes) of data at user pointer. */
    size_t                   cbUser;
} RTFTPCALLBACKDATA;
/** Pointer to FTP server callback data. */
typedef RTFTPCALLBACKDATA *PRTFTPCALLBACKDATA;

/**
 * Function callback table for the FTP server implementation.
 *
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct RTFTPSERVERCALLBACKS
{
    /** User pointer to data. Optional and can be NULL. */
    void  *pvUser;
    /** Size (in bytes) of user data pointing at. Optional and can be 0. */
    size_t cbUser;
    DECLCALLBACKMEMBER(int,  pfnOnUserConnect)(PRTFTPCALLBACKDATA pData, const char *pcszUser);
    DECLCALLBACKMEMBER(int,  pfnOnUserAuthenticate)(PRTFTPCALLBACKDATA pData, const char *pcszUser, const char *pcszPassword);
    DECLCALLBACKMEMBER(int,  pfnOnUserDisconnect)(PRTFTPCALLBACKDATA pData);
    DECLCALLBACKMEMBER(int,  pfnOnPathSetCurrent)(PRTFTPCALLBACKDATA pData, const char *pcszCWD);
    DECLCALLBACKMEMBER(int,  pfnOnPathGetCurrent)(PRTFTPCALLBACKDATA pData, char *pszPWD, size_t cbPWD);
    DECLCALLBACKMEMBER(int,  pfnOnList)(PRTFTPCALLBACKDATA pData, void **ppvData, size_t *pcbData);
} RTFTPSERVERCALLBACKS;
/** Pointer to a FTP server callback data table. */
typedef RTFTPSERVERCALLBACKS *PRTFTPSERVERCALLBACKS;

/**
 * Creates a FTP server instance.
 *
 * @returns IPRT status code.
 * @param   phFTPServer         Where to store the FTP server handle.
 * @param   pcszAddress         The address for creating a listening socket.
 *                              If NULL or empty string the server is bound to all interfaces.
 * @param   uPort               The port for creating a listening socket.
 * @param   pCallbacks          Callback table to use.
 */
RTR3DECL(int) RTFtpServerCreate(PRTFTPSERVER phFTPServer, const char *pcszAddress, uint16_t uPort,
                                PRTFTPSERVERCALLBACKS pCallbacks);

/**
 * Destroys a FTP server instance.
 *
 * @returns IPRT status code.
 * @param   hFTPServer          Handle to the FTP server handle.
 */
RTR3DECL(int) RTFtpServerDestroy(RTFTPSERVER hFTPServer);

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_ftp_h */
