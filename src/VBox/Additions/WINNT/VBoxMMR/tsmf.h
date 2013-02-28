/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___TSMF_H
#define ___TSMF_H

/*
 * BEGIN: TSMFRAW defines for both the guest and the server.
 */

/* TSMFRAW functions. */
#define TSMFRAW_FN_CHANNEL_CREATE 1 /* Create a TSMF channel. */
#define TSMFRAW_FN_CHANNEL_CLOSE  2 /* Close the channel. */
#define TSMFRAW_FN_CHANNEL_DATA   3 /* Send data over the channel. */

/* The header of all tsmfraw transport messages. This is used by both the requests and the responses. */
#pragma pack(1)
typedef struct TSMFRAWMSGHDR
{
    DWORD u32Function;       /* TSMFRAW_FN_* */
    DWORD u32ChannelHandle;  /* The TSMF channel handle. */
} TSMFRAWMSGHDR;

/* u32ChannelHandle is assigned by the guest. */
typedef struct TSMFRAWCREATEREQ
{
    TSMFRAWMSGHDR hdr;
} TSMFRAWCREATEREQ;

/* u32Result is 0, if the channel creation failed. */
typedef struct TSMFRAWCREATERSP
{
    TSMFRAWMSGHDR hdr;
    DWORD u32Result;         /* How the request completed. */
} TSMFRAWCREATERSP;

/* The server must close the channel. */
typedef struct TSMFRAWCLOSE
{
    TSMFRAWMSGHDR hdr;
} TSMFRAWCLOSE;

/* Either the guest sends data to the client or the server forwards the received data to the guest. */
typedef struct TSMFRAWDATA
{
    TSMFRAWMSGHDR hdr;
    DWORD u32DataSize;       /* The size of data. */
    DWORD u32DataOffset;     /* Relative to the structure start. */
    /* u32DataSize bytes follow. */
} TSMFRAWDATA;

#pragma pack()

typedef struct TSMFRAWDATA TSMFRAWDATA;

#endif /* ___TSMF_H */
