/** @file
 * Guest control service:
 * Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_HostService_GuestControlService_h
#define ___VBox_HostService_GuestControlService_h

#include <VBox/types.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest2.h>
#include <VBox/hgcmsvc.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

/** Everything defined in this file lives in this namespace. */
namespace guestControl {

/******************************************************************************
* Typedefs, constants and inlines                                             *
******************************************************************************/

/**
 * Process status when executed in the guest.
 */
enum eProcessStatus
{
    /** Process is in an undefined state. */
    PROC_STS_UNDEFINED = 0,
    /** Process has been started. */
    PROC_STS_STARTED = 1,
    /** Process terminated normally. */
    PROC_STS_TEN = 2,
    /** Process terminated via signal. */
    PROC_STS_TES = 3,
    /** Process terminated abnormally. */
    PROC_STS_TEA = 4,
    /** Process timed out and was killed. */
    PROC_STS_TOK = 5,
    /** Process timed out and was not killed successfully. */
    PROC_STS_TOA = 6,
    /** @todo */
    PROC_STS_DWN = 7,
    /** Something went wrong (error code in flags). */
    PROC_STS_ERROR = 8
};

typedef struct _VBoxGuestCtrlCallbackHeader
{
    /** Magic number to identify the structure. */
    uint32_t u32Magic;
    /** Context ID to identify callback data. */
    uint32_t u32ContextID;
    /** Atomic flag whether callback was called. */
    bool bCalled;
} HOSTCCALLBACKHEADER, *PHOSTCCALLBACKHEADER;

/**
 * Data structure to pass to the service extension callback.  We use this to
 * notify the host of changes to properties.
 */
typedef struct _VBoxGuestCtrlExecCallbackData
{
    /** Callback data header. */
    HOSTCCALLBACKHEADER hdr;
    /** The process ID (PID). */
    uint32_t u32PID;
    /* The process status. */
    uint32_t u32Status;    
    /** Optional flags (not used atm). */
    uint32_t u32Flags;
    /** Optional data buffer (not used atm). */
    void *pvData;
    /** Size of optional data buffer (not used atm). */
    uint32_t cbData;
    
} HOSTEXECCALLBACKDATA, *PHOSTEXECCALLBACKDATA;

enum
{
    /** Magic number for sanity checking the HOSTEXECCALLBACKDATA structure */
    HOSTEXECCALLBACKDATAMAGIC = 0x26011982
};

/**
 * The service functions which are callable by host.
 */
enum eHostFn
{
    /**
     * The host wants to execute something in the guest. This can be a command line
     * or starting a program.
     */
    HOST_EXEC_CMD = 1,
    /**
     * Sends input data for stdin to a running process executed by HOST_EXEC_CMD.
     */
    HOST_EXEC_SEND_STDIN = 2,
    /**
     * Gets the current status of a running process, e.g.
     * new data on stdout/stderr, process terminated etc.
     */
    HOST_EXEC_GET_STATUS = 3
};

/**
 * The service functions which are called by guest.  The numbers may not change,
 * so we hardcode them.
 */
enum eGuestFn
{
    /**
     * TODO
     */
    GUEST_GET_HOST_MSG = 1,
    /**
     * TODO
     */
    GUEST_EXEC_SEND_STDOUT = 3,
    /**
     * TODO
     */
    GUEST_EXEC_SEND_STDERR = 4,
    /**
     * TODO
     */
    GUEST_EXEC_SEND_STATUS = 5
};

/**
 * Sub host commands.  These commands are stored as first (=0) parameter in a GUEST_GET_HOST_MSG
 * so that the guest can react dynamically to requests from the host.
 */
enum eGetHostMsgFn
{
    /**
     * The host wants to execute something in the guest. This can be a command line
     * or starting a program.
     */
    GETHOSTMSG_EXEC_CMD = 1,
    /**
     * Sends input data for stdin to a running process executed by HOST_EXEC_CMD.
     */
    GETHOSTMSG_EXEC_STDIN = 2
};

/*
 * HGCM parameter structures.
 */
#pragma pack (1)
typedef struct _VBoxGuestCtrlHGCMMsgType
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The returned command the host wants to
     * execute on the guest.
     */
    HGCMFunctionParameter msg;       /* OUT uint32_t */

    HGCMFunctionParameter num_parms; /* OUT uint32_t */

} VBoxGuestCtrlHGCMMsgType;

typedef struct _VBoxGuestCtrlHGCMMsgExecCmd
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter context;

    HGCMFunctionParameter cmd;

    HGCMFunctionParameter flags;

    HGCMFunctionParameter num_args;

    HGCMFunctionParameter args;

    HGCMFunctionParameter num_env;
    /** Size (in bytes) of environment block, including terminating zeros. */
    HGCMFunctionParameter cb_env;

    HGCMFunctionParameter env;

    HGCMFunctionParameter std_in;

    HGCMFunctionParameter std_out;

    HGCMFunctionParameter std_err;

    HGCMFunctionParameter username;

    HGCMFunctionParameter password;

    HGCMFunctionParameter timeout;

} VBoxGuestCtrlHGCMMsgExecCmd;

typedef struct _VBoxGuestCtrlHGCMMsgExecStatus
{
    VBoxGuestHGCMCallInfo hdr;
    /** Context ID. */
    HGCMFunctionParameter context;
    /** The process ID (PID). */
    HGCMFunctionParameter pid;
    /** The process status. */
    HGCMFunctionParameter status;
    /** Optional flags (based on status). */
    HGCMFunctionParameter flags;
    /** Optional data buffer (not used atm). */
    HGCMFunctionParameter data;

} VBoxGuestCtrlHGCMMsgExecStatus;
#pragma pack ()

/* Structure for buffering execution requests in the host service. */
typedef struct _VBoxGuestCtrlParamBuffer
{
    uint32_t uParmCount;
    VBOXHGCMSVCPARM *pParms;
} VBOXGUESTCTRPARAMBUFFER, *PVBOXGUESTCTRPARAMBUFFER;

} /* namespace guestControl */

#endif  /* ___VBox_HostService_GuestControlService_h defined */
