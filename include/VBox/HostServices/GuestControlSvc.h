/** @file
 * Guest control service - Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___VBox_HostService_GuestControlService_h
#define ___VBox_HostService_GuestControlService_h

#include <VBox/types.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest2.h>
#include <VBox/hgcmsvc.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

/* Everything defined in this file lives in this namespace. */
namespace guestControl {

/******************************************************************************
* Typedefs, constants and inlines                                             *
******************************************************************************/

/**
 * Process status when executed in the guest.
 * Note: Has to match Main's ExecuteProcessStatus_*!
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
    /** Service/OS is stopping, process was killed. */
    PROC_STS_DWN = 7,
    /** Something went wrong (error code in flags). */
    PROC_STS_ERROR = 8
};

/**
 * Input flags, set by the host. This is needed for
 * handling flags on the guest side.
 * Note: Has to match Main's ProcessInputFlag_* flags!
 */
#define INPUT_FLAG_NONE             0
#define INPUT_FLAG_EOF              RT_BIT(0)

/**
 * Execution flags.
 * Note: Has to match Main's ExecuteProcessFlag_* flags!
 */
#define EXECUTEPROCESSFLAG_NONE             0x0
#define EXECUTEPROCESSFLAG_WAIT_START       RT_BIT(0)
#define EXECUTEPROCESSFLAG_IGNORE_ORPHANED  RT_BIT(1)
#define EXECUTEPROCESSFLAG_HIDDEN           RT_BIT(2)
#define EXECUTEPROCESSFLAG_NO_PROFILE       RT_BIT(3)
#define EXECUTEPROCESSFLAG_WAIT_STDOUT      RT_BIT(4)
#define EXECUTEPROCESSFLAG_WAIT_STDERR      RT_BIT(5)

/**
 * Pipe handle IDs used internally for referencing to
 * a certain pipe buffer.
 */
#define OUTPUT_HANDLE_ID_STDOUT_DEPRECATED  0 /* Needed for VBox hosts < 4.1.0. */
#define OUTPUT_HANDLE_ID_STDOUT             1
#define OUTPUT_HANDLE_ID_STDERR             2

/**
 * Defines for guest process array lengths.
 */
#define GUESTPROCESS_MAX_CMD_LEN            _1K
#define GUESTPROCESS_MAX_ARGS_LEN           _1K
#define GUESTPROCESS_MAX_ENV_LEN            _64K
#define GUESTPROCESS_MAX_USER_LEN           128
#define GUESTPROCESS_MAX_PASSWORD_LEN       128

/** @name Internal tools built into VBoxService which are used in order to
 *        accomplish tasks host<->guest.
 * @{
 */
#define VBOXSERVICE_TOOL_CAT        "vbox_cat"
#define VBOXSERVICE_TOOL_LS         "vbox_ls"
#define VBOXSERVICE_TOOL_MKDIR      "vbox_mkdir"
#define VBOXSERVICE_TOOL_STAT       "vbox_stat"
/** @} */

/**
 * Input status, reported by the client.
 */
enum eInputStatus
{
    /** Input is in an undefined state. */
    INPUT_STS_UNDEFINED = 0,
    /** Input was written (partially, see cbProcessed). */
    INPUT_STS_WRITTEN = 1,
    /** Input failed with an error (see flags for rc). */
    INPUT_STS_ERROR = 20,
    /** Process has abandoned / terminated input handling. */
    INPUT_STS_TERMINATED = 21,
    /** Too much input data. */
    INPUT_STS_OVERFLOW = 30
};

/**
 * The guest control callback data header. Must come first
 * on each callback structure defined below this struct.
 */
typedef struct VBoxGuestCtrlCallbackHeader
{
    /** Magic number to identify the structure. */
    uint32_t u32Magic;
    /** Context ID to identify callback data. */
    uint32_t u32ContextID;
} CALLBACKHEADER;
typedef CALLBACKHEADER *PCALLBACKHEADER;

typedef struct VBoxGuestCtrlCallbackDataClientDisconnected
{
    /** Callback data header. */
    CALLBACKHEADER hdr;
} CALLBACKDATACLIENTDISCONNECTED;
typedef CALLBACKDATACLIENTDISCONNECTED *PCALLBACKDATACLIENTDISCONNECTED;

/**
 * Data structure to pass to the service extension callback.  We use this to
 * notify the host of changes to properties.
 */
typedef struct VBoxGuestCtrlCallbackDataExecStatus
{
    /** Callback data header. */
    CALLBACKHEADER hdr;
    /** The process ID (PID). */
    uint32_t u32PID;
    /** The process status. */
    uint32_t u32Status;
    /** Optional flags, varies, based on u32Status. */
    uint32_t u32Flags;
    /** Optional data buffer (not used atm). */
    void *pvData;
    /** Size of optional data buffer (not used atm). */
    uint32_t cbData;
} CALLBACKDATAEXECSTATUS;
typedef CALLBACKDATAEXECSTATUS *PCALLBACKDATAEXECSTATUS;

typedef struct VBoxGuestCtrlCallbackDataExecOut
{
    /** Callback data header. */
    CALLBACKHEADER hdr;
    /** The process ID (PID). */
    uint32_t u32PID;
    /** The handle ID (stdout/stderr). */
    uint32_t u32HandleId;
    /** Optional flags (not used atm). */
    uint32_t u32Flags;
    /** Optional data buffer. */
    void *pvData;
    /** Size (in bytes) of optional data buffer. */
    uint32_t cbData;
} CALLBACKDATAEXECOUT;
typedef CALLBACKDATAEXECOUT *PCALLBACKDATAEXECOUT;

typedef struct VBoxGuestCtrlCallbackDataExecInStatus
{
    /** Callback data header. */
    CALLBACKHEADER hdr;
    /** The process ID (PID). */
    uint32_t u32PID;
    /** Current input status. */
    uint32_t u32Status;
    /** Optional flags. */
    uint32_t u32Flags;
    /** Size (in bytes) of processed input data. */
    uint32_t cbProcessed;
} CALLBACKDATAEXECINSTATUS;
typedef CALLBACKDATAEXECINSTATUS *PCALLBACKDATAEXECINSTATUS;

enum eVBoxGuestCtrlCallbackDataMagic
{
    CALLBACKDATAMAGIC_CLIENT_DISCONNECTED = 0x08041984,

    CALLBACKDATAMAGIC_EXEC_STATUS = 0x26011982,
    CALLBACKDATAMAGIC_EXEC_OUT = 0x11061949,
    CALLBACKDATAMAGIC_EXEC_IN_STATUS = 0x19091951
};

enum eVBoxGuestCtrlCallbackType
{
    VBOXGUESTCTRLCALLBACKTYPE_UNKNOWN = 0,

    VBOXGUESTCTRLCALLBACKTYPE_EXEC_START = 1,
    VBOXGUESTCTRLCALLBACKTYPE_EXEC_OUTPUT = 2,
    VBOXGUESTCTRLCALLBACKTYPE_EXEC_INPUT_STATUS = 3
};

/**
 * The service functions which are callable by host.
 */
enum eHostFn
{
    /**
     * The host asks the client to cancel all pending waits and exit.
     */
    HOST_CANCEL_PENDING_WAITS = 0,

    /*
     * Execution handling.
     */

    /**
     * The host wants to execute something in the guest. This can be a command line
     * or starting a program.
     */
    HOST_EXEC_CMD = 100,
    /**
     * Sends input data for stdin to a running process executed by HOST_EXEC_CMD.
     */
    HOST_EXEC_SET_INPUT = 101,
    /**
     * Gets the current status of a running process, e.g.
     * new data on stdout/stderr, process terminated etc.
     */
    HOST_EXEC_GET_OUTPUT = 102
};

/**
 * The service functions which are called by guest.  The numbers may not change,
 * so we hardcode them.
 */
enum eGuestFn
{
    /**
     * Guest waits for a new message the host wants to process on the guest side.
     * This is a blocking call and can be deferred.
     */
    GUEST_GET_HOST_MSG = 1,
    /**
     * Guest asks the host to cancel all pending waits the guest itself waits on.
     * This becomes necessary when the guest wants to quit but still waits for
     * commands from the host.
     */
    GUEST_CANCEL_PENDING_WAITS = 2,
    /**
     * Guest disconnected (terminated normally or due to a crash HGCM
     * detected when calling service::clientDisconnect().
     */
    GUEST_DISCONNECTED = 3,

    /*
     * Process execution.
     */

    /**
     * Guests sends output from an executed process.
     */
    GUEST_EXEC_SEND_OUTPUT = 100,
    /**
     * Guest sends a status update of an executed process to the host.
     */
    GUEST_EXEC_SEND_STATUS = 101,
    /**
     * Guests sends an input status notification to the host.
     */
    GUEST_EXEC_SEND_INPUT_STATUS = 102
};

/*
 * HGCM parameter structures.
 */
#pragma pack (1)

typedef struct VBoxGuestCtrlHGCMMsgType
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The returned command the host wants to
     * run on the guest.
     */
    HGCMFunctionParameter msg;       /* OUT uint32_t */
    /** Number of parameters the message needs. */
    HGCMFunctionParameter num_parms; /* OUT uint32_t */

} VBoxGuestCtrlHGCMMsgType;

/**
 * Asks the guest control host service to cancel all pending (outstanding)
 * waits which were not processed yet.  This is handy for a graceful shutdown.
 */
typedef struct VBoxGuestCtrlHGCMMsgCancelPendingWaits
{
    VBoxGuestHGCMCallInfo hdr;
} VBoxGuestCtrlHGCMMsgCancelPendingWaits;

/**
 * Executes a command inside the guest.
 */
typedef struct VBoxGuestCtrlHGCMMsgExecCmd
{
    VBoxGuestHGCMCallInfo hdr;
    /** Context ID. */
    HGCMFunctionParameter context;
    /** The command to execute on the guest. */
    HGCMFunctionParameter cmd;
    /** Execution flags (see IGuest::ExecuteProcessFlag_*). */
    HGCMFunctionParameter flags;
    /** Number of arguments. */
    HGCMFunctionParameter num_args;
    /** The actual arguments. */
    HGCMFunctionParameter args;
    /** Number of environment value pairs. */
    HGCMFunctionParameter num_env;
    /** Size (in bytes) of environment block, including terminating zeros. */
    HGCMFunctionParameter cb_env;
    /** The actual environment block. */
    HGCMFunctionParameter env;
    /** The user name to run the executed command under. */
    HGCMFunctionParameter username;
    /** The user's password. */
    HGCMFunctionParameter password;
    /** Timeout (in msec) which either specifies the
     *  overall lifetime of the process or how long it
     *  can take to bring the process up and running -
     *  (depends on the IGuest::ExecuteProcessFlag_*). */
    HGCMFunctionParameter timeout;

} VBoxGuestCtrlHGCMMsgExecCmd;

/**
 * Injects input to a previously executed process via stdin.
 */
typedef struct VBoxGuestCtrlHGCMMsgExecIn
{
    VBoxGuestHGCMCallInfo hdr;
    /** Context ID. */
    HGCMFunctionParameter context;
    /** The process ID (PID) to send the input to. */
    HGCMFunctionParameter pid;
    /** Input flags (see IGuest::ProcessInputFlag_*). */
    HGCMFunctionParameter flags;
    /** Data buffer. */
    HGCMFunctionParameter data;
    /** Actual size of data (in bytes). */
    HGCMFunctionParameter size;

} VBoxGuestCtrlHGCMMsgExecIn;

/**
 * Retrieves ouptut from a previously executed process
 * from stdout/stderr.
 */
typedef struct VBoxGuestCtrlHGCMMsgExecOut
{
    VBoxGuestHGCMCallInfo hdr;
    /** Context ID. */
    HGCMFunctionParameter context;
    /** The process ID (PID). */
    HGCMFunctionParameter pid;
    /** The pipe handle ID (stdout/stderr). */
    HGCMFunctionParameter handle;
    /** Optional flags. */
    HGCMFunctionParameter flags;
    /** Data buffer. */
    HGCMFunctionParameter data;

} VBoxGuestCtrlHGCMMsgExecOut;

/**
 * Reports the current status of a (just) started
 * or terminated process.
 */
typedef struct VBoxGuestCtrlHGCMMsgExecStatus
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

/**
 * Reports back the status of data written to a process.
 */
typedef struct VBoxGuestCtrlHGCMMsgExecStatusIn
{
    VBoxGuestHGCMCallInfo hdr;
    /** Context ID. */
    HGCMFunctionParameter context;
    /** The process ID (PID). */
    HGCMFunctionParameter pid;
    /** Status of the operation. */
    HGCMFunctionParameter status;
    /** Optional flags. */
    HGCMFunctionParameter flags;
    /** Data written. */
    HGCMFunctionParameter written;

} VBoxGuestCtrlHGCMMsgExecStatusIn;

#pragma pack ()

/**
 * Structure for buffering execution requests in the host service.
 */
typedef struct VBoxGuestCtrlParamBuffer
{
    uint32_t uMsg;
    uint32_t uParmCount;
    PVBOXHGCMSVCPARM pParms;
} VBOXGUESTCTRPARAMBUFFER;
typedef VBOXGUESTCTRPARAMBUFFER *PVBOXGUESTCTRPARAMBUFFER;

} /* namespace guestControl */

#endif  /* !___VBox_HostService_GuestControlService_h */

