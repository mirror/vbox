/* $Id$ */
/** @file
 * VBoxTrayMsg - Globally registered messages (RPC) to/from VBoxTray.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBOXTRAY_MSG_H
#define ___VBOXTRAY_MSG_H

#define VBOXTRAY_PIPE_IPC               "\\\\.\\pipe\\VBoxTrayIPC"
#define VBOXTRAY_PIPE_IPC_BUFSIZE       64 * 1024

enum VBOXTRAYIPCMSGTYPE
{
    /** Asks the IPC thread to quit. */
    VBOXTRAYIPCMSGTYPE_QUIT           = 10,
    /** TODO */
    VBOXTRAYIPCMSGTYPE_SHOWBALLOONMSG = 100
};

/* VBoxTray's IPC header. */
typedef struct _VBOXTRAYIPCHEADER
{
    /** Message type. */
    ULONG ulMsg;
    /** Version of message type. */
    ULONG ulVer;
} VBOXTRAYIPCHEADER, *PVBOXTRAYIPCHEADER;

typedef struct _VBOXTRAYIPCMSG_SHOWBALLOONMSG
{
    /** Message body. */
    TCHAR    szBody[256];
    /** Message body. */
    TCHAR    szTitle[64];
    /** Message type. */
    ULONG    ulType;
    /** Flags; not used yet. */
    ULONG    ulFlags;
    /** Time to show the message (in msec). */
    ULONG    ulShowMS;
} VBOXTRAYIPCMSG_SHOWBALLOONMSG, *PVBOXTRAYIPCMSG_SHOWBALLOONMSG;

#endif /* !___VBOXTRAY_MSG_H */

