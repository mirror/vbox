/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Convert L4 errno to iprt status code.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <l4/env/errno.h>
#include <l4/sys/ipc.h>

#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/err.h>


/**
 * Converts a L4 errno to a iprt status code.
 *
 * @returns iprt status code.
 * @param   uNativeCode l4 errno.
 */
RTDECL(int)  RTErrConvertFromL4Errno(unsigned uNativeCode)
{
    /* very fast check for no error. */
    if (uNativeCode == 0)
        return VINF_SUCCESS;

    /*
     * Process error codes.
     */
    switch (uNativeCode)  /* L4 error code */
    {
        case L4_EUNKNOWN:           return VERR_UNRESOLVED_ERROR;
        case L4_EPERM:              return VERR_ACCESS_DENIED;
        case L4_ENOENT:             return VERR_FILE_NOT_FOUND;
        case L4_EINVAL_OFFS:        return VERR_L4_INVALID_DS_OFFSET;
        case L4_EIO:                return VERR_DEV_IO_ERROR;
        case L4_EIPC:               return VERR_IPC;
        case L4_ENOMAP:             return VERR_NO_MEMORY;
        case L4_ENOTHREAD:          return VERR_MAX_THRDS_REACHED;
        case L4_EINVAL:             return VERR_INVALID_PARAMETER;
        case L4_EUSED:              return VERR_RESOURCE_IN_USE;
        case L4_ENOMEM:             return VERR_NO_MEMORY;
        case L4_IPC_ENOT_EXISTENT:  return VERR_IPC_PROCESS_NOT_FOUND;
        case L4_IPC_RETIMEOUT:      return VERR_IPC_RECEIVE_TIMEOUT;
        case L4_IPC_SETIMEOUT:      return VERR_IPC_SEND_TIMEOUT;
        case L4_IPC_RECANCELED:     return VERR_IPC_RECEIVE_CANCELLED;
        case L4_IPC_SECANCELED:     return VERR_IPC_SEND_CANCELLED;
        case L4_IPC_REABORTED:      return VERR_IPC_RECEIVE_ABORTED;
        case L4_IPC_SEABORTED:      return VERR_IPC_SEND_ABORTED;
        case L4_IPC_REMAPFAILED:    return VERR_IPC_RECEIVE_MAP_FAILED;
        case L4_IPC_SEMAPFAILED:    return VERR_IPC_SEND_MAP_FAILED;
        case L4_IPC_RESNDPFTO:      return VERR_IPC_RECEIVE_SEND_PF_TIMEOUT;
        case L4_IPC_SESNDPFTO:      return VERR_IPC_SEND_SEND_PF_TIMEOUT;
        case L4_IPC_REMSGCUT:       return VINF_IPC_RECEIVE_MSG_CUT;
        case L4_IPC_SEMSGCUT:       return VINF_IPC_SEND_MSG_CUT;
        case L4_ENODM:              return VERR_L4_DS_MANAGER_NOT_FOUND;
        default:
            AssertMsgFailed(("Unhandled error code %d\n", uNativeCode));
            return VERR_UNRESOLVED_ERROR;
    }
}

