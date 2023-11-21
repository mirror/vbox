/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "primpl.h"
#include <errno.h>

void _MD_unix_map_default_error(int err)
{
    PRErrorCode prError;

    switch (err ) {
        case EACCES:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case EADDRINUSE:
            prError = PR_ADDRESS_IN_USE_ERROR;
            break;
        case EADDRNOTAVAIL:
            prError = PR_ADDRESS_NOT_AVAILABLE_ERROR;
            break;
        case EAFNOSUPPORT:
            prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
            break;
        case EAGAIN:
            prError = PR_WOULD_BLOCK_ERROR;
            break;
        /*
         * On QNX and Neutrino, EALREADY is defined as EBUSY.
         */
#if EALREADY != EBUSY
        case EALREADY:
            prError = PR_ALREADY_INITIATED_ERROR;
            break;
#endif
        case EBADF:
            prError = PR_BAD_DESCRIPTOR_ERROR;
            break;
#ifdef EBADMSG
        case EBADMSG:
            prError = PR_IO_ERROR;
            break;
#endif
        case EBUSY:
            prError = PR_FILESYSTEM_MOUNTED_ERROR;
            break;
        case ECONNABORTED:
            prError = PR_CONNECT_ABORTED_ERROR;
            break;
        case ECONNREFUSED:
            prError = PR_CONNECT_REFUSED_ERROR;
            break;
        case ECONNRESET:
            prError = PR_CONNECT_RESET_ERROR;
            break;
        case EDEADLK:
            prError = PR_DEADLOCK_ERROR;
            break;
#ifdef EDIRCORRUPTED
        case EDIRCORRUPTED:
            prError = PR_DIRECTORY_CORRUPTED_ERROR;
            break;
#endif
#ifdef EDQUOT
        case EDQUOT:
            prError = PR_NO_DEVICE_SPACE_ERROR;
            break;
#endif
        case EEXIST:
            prError = PR_FILE_EXISTS_ERROR;
            break;
        case EFAULT:
            prError = PR_ACCESS_FAULT_ERROR;
            break;
        case EFBIG:
            prError = PR_FILE_TOO_BIG_ERROR;
            break;
        case EHOSTUNREACH:
            prError = PR_HOST_UNREACHABLE_ERROR;
            break;
        case EINPROGRESS:
            prError = PR_IN_PROGRESS_ERROR;
            break;
        case EINTR:
            prError = PR_PENDING_INTERRUPT_ERROR;
            break;
        case EINVAL:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case EIO:
            prError = PR_IO_ERROR;
            break;
        case EISCONN:
            prError = PR_IS_CONNECTED_ERROR;
            break;
        case EISDIR:
            prError = PR_IS_DIRECTORY_ERROR;
            break;
        case ELOOP:
            prError = PR_LOOP_ERROR;
            break;
        case EMFILE:
            prError = PR_PROC_DESC_TABLE_FULL_ERROR;
            break;
        case EMLINK:
            prError = PR_MAX_DIRECTORY_ENTRIES_ERROR;
            break;
        case EMSGSIZE:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
#ifdef EMULTIHOP
        case EMULTIHOP:
            prError = PR_REMOTE_FILE_ERROR;
            break;
#endif
        case ENAMETOOLONG:
            prError = PR_NAME_TOO_LONG_ERROR;
            break;
        case ENETUNREACH:
            prError = PR_NETWORK_UNREACHABLE_ERROR;
            break;
        case ENFILE:
            prError = PR_SYS_DESC_TABLE_FULL_ERROR;
            break;
        /*
         * On SCO OpenServer 5, ENOBUFS is defined as ENOSR.
         */
#if defined(ENOBUFS) && (ENOBUFS != ENOSR)
        case ENOBUFS:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
#endif
        case ENODEV:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        case ENOENT:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        case ENOLCK:
            prError = PR_FILE_IS_LOCKED_ERROR;
            break;
#ifdef ENOLINK 
        case ENOLINK:
            prError = PR_REMOTE_FILE_ERROR;
            break;
#endif
        case ENOMEM:
            prError = PR_OUT_OF_MEMORY_ERROR;
            break;
        case ENOPROTOOPT:
            prError = PR_INVALID_ARGUMENT_ERROR;
            break;
        case ENOSPC:
            prError = PR_NO_DEVICE_SPACE_ERROR;
            break;
#ifdef ENOSR
        case ENOSR:
            prError = PR_INSUFFICIENT_RESOURCES_ERROR;
            break;
#endif
        case ENOTCONN:
            prError = PR_NOT_CONNECTED_ERROR;
            break;
        case ENOTDIR:
            prError = PR_NOT_DIRECTORY_ERROR;
            break;
        case ENOTSOCK:
            prError = PR_NOT_SOCKET_ERROR;
            break;
        case ENXIO:
            prError = PR_FILE_NOT_FOUND_ERROR;
            break;
        case EOPNOTSUPP:
            prError = PR_NOT_TCP_SOCKET_ERROR;
            break;
#ifdef EOVERFLOW
        case EOVERFLOW:
            prError = PR_BUFFER_OVERFLOW_ERROR;
            break;
#endif
        case EPERM:
            prError = PR_NO_ACCESS_RIGHTS_ERROR;
            break;
        case EPIPE:
            prError = PR_CONNECT_RESET_ERROR;
            break;
#ifdef EPROTO
        case EPROTO:
            prError = PR_IO_ERROR;
            break;
#endif
        case EPROTONOSUPPORT:
            prError = PR_PROTOCOL_NOT_SUPPORTED_ERROR;
            break;
        case EPROTOTYPE:
            prError = PR_ADDRESS_NOT_SUPPORTED_ERROR;
            break;
        case ERANGE:
            prError = PR_INVALID_METHOD_ERROR;
            break;
        case EROFS:
            prError = PR_READ_ONLY_FILESYSTEM_ERROR;
            break;
        case ESPIPE:
            prError = PR_INVALID_METHOD_ERROR;
            break;
        case ETIMEDOUT:
            prError = PR_IO_TIMEOUT_ERROR;
            break;
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
            prError = PR_WOULD_BLOCK_ERROR;
            break;
#endif
        case EXDEV:
            prError = PR_NOT_SAME_DEVICE_ERROR;
            break;
        default:
            prError = PR_UNKNOWN_ERROR;
            break;
    }
    PR_SetError(prError, err);
}
