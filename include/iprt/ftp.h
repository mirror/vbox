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
 * Creates a FTP server instance.
 *
 * @returns IPRT status code.
 * @param   phFTPServer         Where to store the FTP server handle.
 * @param   pcszAddress         The address for creating a listening socket.
 *                              If NULL or empty string the server is bound to all interfaces.
 * @param   uPort               The port for creating a listening socket.
 * @param   pcszPathRoot        Root path of the FTP server serving.
 */
RTR3DECL(int) RTFTPServerCreate(PRTFTPSERVER phFTPServer, const char *pcszAddress, uint16_t uPort,
                                const char *pcszPathRoot);

/**
 * Destroys a FTP server instance.
 *
 * @returns IPRT status code.
 * @param   hFTPServer          Handle to the FTP server handle.
 */
RTR3DECL(int) RTFTPServerDestroy(RTFTPSERVER hFTPServer);

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_ftp_h */
