/** @file
 * Virtualization Infrastructure Driver (VID) API.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


#ifndef ___iprt_nt_vid_h
#define ___iprt_nt_vid_h

#include "hyperv.h"


/**
 * Output from VidMessageSlotMap.
 */
typedef struct VID_MAPPED_MESSAGE_SLOT
{
    /** The message block mapping. */
    struct _HV_MESSAGE *pMsgBlock;
    /** Copy of input iCpu. */
    uint32_t            iCpu;
    /** Explicit padding.   */
    uint32_t            uParentAdvisory;
} VID_MAPPED_MESSAGE_SLOT;
/** Pointer to VidMessageSlotMap output structure. */
typedef VID_MAPPED_MESSAGE_SLOT *PVID_MAPPED_MESSAGE_SLOT;


/** @name VID_MESSAGE_MAPPING_HEADER::enmVidMsgType values (wild guess).
 * @{ */
/** Type mask, strips flags. */
#define VID_MESSAGE_TYPE_MASK                   UINT32_C(0x00ffffff)
/** No return message necessary. */
#define VID_MESSAGE_TYPE_FLAG_NO_RETURN         UINT32_C(0x01000000)
/** Observed message values. */
typedef enum
{
    /** Invalid zero value. */
    VidMessageInvalid = 0,
    /** Guessing this means a message from the hypervisor.  */
    VidMessageHypervisorMessage   = 0x00000c | VID_MESSAGE_TYPE_FLAG_NO_RETURN,
    /** Guessing this means stop request completed.  Message length is 1 byte. */
    VidMessageStopRequestComplete = 0x00000d | VID_MESSAGE_TYPE_FLAG_NO_RETURN,
} VID_MESSAGE_TYPE;
AssertCompileSize(VID_MESSAGE_TYPE, 4);
/** @} */

/**
 * Header of the message mapping returned by VidMessageSlotMap.
 */
typedef struct VID_MESSAGE_MAPPING_HEADER
{
    /** Current guess is that this is VID_MESSAGE_TYPE. */
    VID_MESSAGE_TYPE    enmVidMsgType;
    /** The message size or so it seems (0x100). */
    uint32_t            cbMessage;
    /** So far these have been zero. */
    uint32_t            aZeroPPadding[2+4];
} VID_MESSAGE_MAPPING_HEADER;
AssertCompileSize(VID_MESSAGE_MAPPING_HEADER, 32);

/**
 * VID processor status (VidGetVirtualProcessorRunningStatus).
 *
 * @note This is used internally in VID.SYS, in 17101 it's at offset 8 in their
 *       'pVCpu' structure.
 */
typedef enum
{
    VidProcessorStatusStopped = 0,
    VidProcessorStatusRunning,
    VidProcessorStatusSuspended,
    VidProcessorStatusUndefined = 0xffff
} VID_PROCESSOR_STATUS;
AssertCompileSize(VID_PROCESSOR_STATUS, 4);


RT_C_DECLS_BEGIN

/** Calling convention. */
#ifndef WINAPI
# define VIDAPI __stdcall
#else
# define VIDAPI WINAPI
#endif

/** Partition handle. */
#ifndef WINAPI
typedef void *VID_PARTITION_HANDLE;
#else
typedef HANDLE VID_PARTITION_HANDLE;
#endif

/**
 * Gets the partition ID.
 *
 * The partition ID is the numeric identifier used when making hypercalls to the
 * hypervisor.
 */
DECLIMPORT(BOOL) VIDAPI VidGetHvPartitionId(VID_PARTITION_HANDLE hPartition, HV_PARTITION_ID *pidPartition);

/**
 * Starts asynchronous execution of a virtual CPU.
 */
DECLIMPORT(BOOL) VIDAPI VidStartVirtualProcessor(VID_PARTITION_HANDLE hPartition, HV_VP_INDEX iCpu);

/**
 * Stops the asynchronous execution of a virtual CPU.
 *
 * @retval ERROR_VID_STOP_PENDING if busy with intercept, check messages.
 */
DECLIMPORT(BOOL) VIDAPI VidStopVirtualProcessor(VID_PARTITION_HANDLE hPartition, HV_VP_INDEX iCpu);

/**
 * WHvCreateVirtualProcessor boils down to a call to VidMessageSlotMap and
 * some internal WinHvPlatform state fiddling.
 *
 * Looks like it maps memory and returns the pointer to it.
 * VidMessageSlotHandleAndGetNext is later used to wait for the next message and
 * put (??) it into that memory mapping.
 *
 * @returns Success indicator (details in LastStatusValue and LastErrorValue).
 *
 * @param   hPartition  The partition handle.
 * @param   pOutput     Where to return the pointer to the message memory
 *                      mapping.  The CPU index is also returned here.
 * @param   iCpu        The CPU to wait-and-get messages for.
 */
DECLIMPORT(BOOL) VIDAPI VidMessageSlotMap(VID_PARTITION_HANDLE hPartition, PVID_MAPPED_MESSAGE_SLOT pOutput, HV_VP_INDEX iCpu);

/**
 * This is used by WHvRunVirtualProcessor to wait for the next exit msg.
 *
 * The message appears in the memory mapping returned by VidMessageSlotMap.
 *
 * @returns Success indicator (details only in LastErrorValue - LastStatusValue
 *          is not set).
 * @retval  STATUS_TIMEOUT for STATUS_TIMEOUT as well as STATUS_USER_APC and
 *          STATUS_ALERTED.
 *
 * @param   hPartition  The partition handle.
 * @param   iCpu        The CPU to wait-and-get messages for.
 * @param   fFlags      Flags. At least one of the two flags must be set:
 *                          - VID_MSHAGN_F_GET_NEXT_MESSAGE (bit 0)
 *                          - VID_MSHAGN_F_HANDLE_MESSAGE (bit 1)
 * @param   cMillies    The timeout, presumably in milliseconds.
 *
 * @todo    Would be awfully nice if someone at Microsoft could hit at the
 *          flags here.
 * @note
 */
DECLIMPORT(BOOL) VIDAPI VidMessageSlotHandleAndGetNext(VID_PARTITION_HANDLE hPartition, HV_VP_INDEX iCpu,
                                                       uint32_t fFlags, uint32_t cMillies);
/** @name VID_MSHAGN_F_GET_XXX - Flags for VidMessageSlotHandleAndGetNext
 * @{ */
/** This will try get the next message, waiting if necessary.
 * It is subject to NtAlertThread processing when it starts waiting.  */
#define VID_MSHAGN_F_GET_NEXT_MESSAGE   RT_BIT_32(0)
/** ACK the message as handled and resume execution/whatever.
 * This is executed before VID_MSHAGN_F_GET_NEXT_MESSAGE and should not be
 * subject to NtAlertThread side effects. */
#define VID_MSHAGN_F_HANDLE_MESSAGE     RT_BIT_32(1)
/** @} */

/**
 * Gets the processor running status.
 *
 * This is probably only available in special builds, as one of the early I/O
 * control dispatching routines will not let it thru.  Lower down routines does
 * implement it, so it's possible to patch it into working.  This works for
 * build 17101: eb vid+12180 0f 84 98 00 00 00
 *
 * @retval STATUS_NOT_IMPLEMENTED
 */
DECLIMPORT(BOOL) VIDAPI VidGetVirtualProcessorRunningStatus(VID_PARTITION_HANDLE hPartition, HV_VP_INDEX iCpu,
                                                            VID_PROCESSOR_STATUS *penmStatus);

RT_C_DECLS_END

#endif

