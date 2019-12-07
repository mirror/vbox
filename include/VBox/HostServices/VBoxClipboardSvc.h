/** @file
 * Shared Clipboard - Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
 * Protocol handling and notes:
 *     All client/server components should be backwards compatible.
 *
 ******************************************************************************
 *
 * Protocol changelog:
 *
 *     VBox < 6.1, deprecated:
 *         | First, initial implementation since feature was developed.
 *           Has no protocol handshake or support for feature exchange,
 *           the client's waiting message also acted as retrieving the
 *           parameters from the host (always and only *exactly* two
 *           parameters). Does not have the ability to control / handle parallel
 *           transfers.
 *
 *     VBox >= 6.1:
 *         + Adds host/guest feature flags and context IDs for parallel,
 *           asynchronous transfers.
 *         | Keeps backwards-compatbility with by translating messages to
 *           the older protocol (< 6.1), to not break compatibility with older
 *           Guest Additions.
 */

#ifndef VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h
#define VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VMMDevCoreTypes.h>
#include <VBox/VBoxGuestCoreTypes.h>
#include <VBox/hgcmsvc.h>

#include <VBox/GuestHost/SharedClipboard.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
#include <VBox/GuestHost/SharedClipboard-transfers.h>
#endif


/** @name VBOX_SHCL_MODE_XXX - The Shared Clipboard modes of operation.
 * @{
 */
/** Shared Clipboard is disabled completely. */
#define VBOX_SHCL_MODE_OFF           0
/** Only transfers from host to the guest are possible. */
#define VBOX_SHCL_MODE_HOST_TO_GUEST 1
/** Only transfers from guest to the host are possible. */
#define VBOX_SHCL_MODE_GUEST_TO_HOST 2
/** Bidirectional transfers between guest and host are possible. */
#define VBOX_SHCL_MODE_BIDIRECTIONAL 3
/** @}  */

/** @name VBOX_SHCL_TRANSFER_MODE_XXX - The Shared Clipboard file transfer mode (bit field).
 * @{
 */
/** Shared Clipboard file transfers are disabled. */
#define VBOX_SHCL_TRANSFER_MODE_DISABLED     UINT32_C(0)
/** Shared Clipboard file transfers are enabled. */
#define VBOX_SHCL_TRANSFER_MODE_ENABLED      RT_BIT(0)
/** Shared Clipboard file transfer mode valid mask. */
#define VBOX_SHCL_TRANSFER_MODE_VALID_MASK   UINT32_C(0x1)
/** @} */

/** @name VBOX_SHCL_HOST_FN_XXX - The service functions which are callable by host.
 * @note These are not sacred and can be modified at will as long as all host
 *       clients are updated accordingly (probably just Main).
 * @{
 */
/** Sets the current Shared Clipboard operation mode. */
#define VBOX_SHCL_HOST_FN_SET_MODE           1
/** Sets the current Shared Clipboard (file) transfers mode.
 *  Operates on the VBOX_SHCL_TRANSFERS_XXX defines.
 * @since   6.1  */
#define VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE  2
/** Run headless on the host, i.e. do not touch the host clipboard. */
#define VBOX_SHCL_HOST_FN_SET_HEADLESS       3
/** Reports cancellation of the current operation to the guest.
 * @since   6.1 - still a todo  */
#define VBOX_SHCL_HOST_FN_CANCEL             4
/** Reports an error to the guest.
 * @since   6.1 - still a todo  */
#define VBOX_SHCL_HOST_FN_ERROR              5
/** Reports that a new clipboard area has been registered.
 * @since   6.1 - not implemented */
#define VBOX_SHCL_HOST_FN_AREA_REGISTER      6
/** Reports that a clipboard area has been unregistered.
 * @since   6.1 - not implemented  */
#define VBOX_SHCL_HOST_FN_AREA_UNREGISTER    7
/** Reports that a client (host / guest) has attached to a clipboard area.
 * @since   6.1 - not implemented  */
#define VBOX_SHCL_HOST_FN_AREA_ATTACH        8
/** Reports that a client (host / guest) has detached from a clipboard area.
 * @since   6.1 - not implemented */
#define VBOX_SHCL_HOST_FN_AREA_DETACH        9
/** @} */

/** @name VBOX_SHCL_HOST_MSG_XXX - The host messages for the guest.
 * @{
 */
/** Asks the client to quit / terminate. */
#define VBOX_SHCL_HOST_MSG_QUIT                              1
/** Reads (simple) data from the guest. */
#define VBOX_SHCL_HOST_MSG_READ_DATA                         2
/** Reports available clipboard format from host to the guest.
 *  Formerly known as VBOX_SHCL_HOST_MSG_REPORT_FORMATS. */
#define VBOX_SHCL_HOST_MSG_FORMATS_REPORT                    3

/** Sends a transfer status to the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_STATUS                   50
/** Reads the root list header from the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_READ       51
/** Writes the root list header to the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_WRITE      52
/** Reads a root list entry from the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_READ     53
/** Writes a root list entry to the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_WRITE    54
/** Open a transfer list on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_OPEN                55
/** Closes a formerly opened transfer list on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_CLOSE               56
/** Reads a list header from the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_READ            57
/** Writes a list header to the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_WRITE           58
/** Reads a list entry from the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_READ          59
/** Writes a list entry to the guest.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_WRITE         60
/** Open a transfer object on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_OPEN                 61
/** Closes a formerly opened transfer object on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_CLOSE                62
/** Reads from an object on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_READ                 63
/** Writes to an object on the guest side.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_WRITE                64
/** Indicates that the host has canceled a transfer.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_CANCEL                   65
/** Indicates that the an unrecoverable error on the host occurred.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_HOST_MSG_TRANSFER_ERROR                    66
/** @} */

/** @name VBOX_SHCL_GUEST_FN_XXX - The service functions which are called by guest.
 * @{
 */
/** Calls the host and waits (blocking) for an host event VBOX_SHCL_HOST_MSG_*.
 *  Deprecated, do not use anymore. Kept to not break compatibility with older
 *  Guest Additions / VBox versions. */
#define VBOX_SHCL_GUEST_FN_GET_HOST_MSG_OLD       1
/** Sends a list of available formats to the host.
 *  Formely known as VBOX_SHCL_GUEST_FN_REPORT_FORMATS. */
#define VBOX_SHCL_GUEST_FN_FORMATS_REPORT         2
/** Reads data in specified format from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  For VBox >= 6.1: VINF_BUFFER_OVERLFLOW if not enough buffer space has been given to retrieve the actual data.
 *                           The call then must be repeated with a buffer size returned from the host in cbData.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 */
#define VBOX_SHCL_GUEST_FN_DATA_READ              3
/** Writes data in requested format to the host. */
#define VBOX_SHCL_GUEST_FN_DATA_WRITE             4

/** Does the actual protocol handshake. If this message is not
 *  being sent by the guest, the host handles that particular client
 *  with the legacy protocol (v0).
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_CONNECT                5
/** Report guest side feature flags and retrieve the host ones.
 *
 * The guest replies to the host what features it support in addition.
 * In return the host will return features the host supports.
 *
 * Two 64-bit parameters are passed in from the
 * guest with the guest features (VBOX_SHCL_GF_XXX), the host replies by
 * replacing the parameter values with the host ones (VBOX_SHCL_HF_XXX).
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_REPORT_FEATURES        6
/** Query the host ones feature masks.
 *
 * That way the guest (client) can get hold of the features
 * from the host.  Again, it is prudent to set the 127 bit and observe it being
 * cleared on success, as older hosts might return success without doing
 * anything.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_QUERY_FEATURES         7
/** Peeks at the next message, returning immediately.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT        8
/** Peeks at the next message, waiting for one to arrive.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT          9
/** Gets the next message, returning immediately.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_MSG_GET                10
/** Replies to a function from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_REPLY                  11
/** Gets the root list header from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ     12
/** Sends the root list header to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE    13
/** Gets a root list root entry from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ   14
/** Sends a root list root entry to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE  15
/** Opens / gets a list handle from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_OPEN              16
/** Closes a list handle from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_CLOSE             17
/** Reads a list header from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_HDR_READ          18
/** Writes a list header to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE         19
/** Reads a list entry from the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ        20
/** Sends a list entry to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE       21
/** Opens an object on the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_OBJ_OPEN               22
/** Closes an object on the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_OBJ_CLOSE              23
/** Reads from an object on the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_OBJ_READ               24
/** Writes to an object on the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_OBJ_WRITE              25
/** Reports cancellation of the current operation to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_CANCEL                 26
/** Reports an error to the host.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_CLIENT_ID
 * @retval  VERR_WRONG_PARAMETER_COUNT
 * @retval  VERR_WRONG_PARAMETER_TYPE
 * @since   6.1
 */
#define VBOX_SHCL_GUEST_FN_ERROR                  27
/** @} */


/** The maximum default chunk size for a single data transfer.
 * @note r=bird: Nobody actually uses this.  Name is misleading.  */
#define VBOX_SHCL_DEFAULT_MAX_CHUNK_SIZE          _64K

/** @name VBOX_SHCL_GF_XXX - Guest features.
 * @sa VBOX_SHCL_GUEST_FN_REPORT_FEATURES
 * @{ */
/** No flags set. */
#define VBOX_SHCL_GF_NONE                         0
/** Guest can handle context IDs (uint64_t, in paParam[0]).
 *  This is true for Guest Additions < 6.1. */
#define VBOX_SHCL_GF_0_CONTEXT_ID                 RT_BIT_64(0)
/** Bit that must be set in the 2nd parameter, will be cleared if the host reponds
 * correctly (old hosts might not). */
#define VBOX_SHCL_GF_1_MUST_BE_ONE                RT_BIT_64(63)
/** @} */

/** @name VBOX_GUESTCTRL_HF_XXX - Host features.
 * @sa VBOX_SHCL_GUEST_FN_REPORT_FEATURES
 * @{ */
/** No flags set. */
#define VBOX_SHCL_HF_NONE                         0
/** @} */

/*
 * HGCM parameter structures.
 */
/** @todo r=bird: These structures are mostly pointless. */
#pragma pack(1)
/**
 * Waits (blocking) for a new host message to arrive.
 * Deprecated; do not use anymore.
 * Kept for maintaining compatibility with older Guest Additions.
 */
typedef struct _VBoxShClGetHostMsgOld
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Host message type. */
    HGCMFunctionParameter msg;
    /** uint32_t, out: VBOX_SHCL_FMT_*, depends on the 'msg'.
     *  r=andy This actual can have *different* meanings, depending on the host message type. */
    HGCMFunctionParameter formats; /* OUT uint32_t */
} VBoxShClGetHostMsgOld;

#define VBOX_SHCL_CPARMS_GET_HOST_MSG_OLD 2

/**
 * Message for doing the protocol negotiation between the host
 * and the guest. Not available on older (VBox <= 6.0) hosts.
 *
 * This message acts as a beacon between the old protocol (VBox <= 6.0) and
 * the new protocol (>= VBox 6.1). Newer features are getting introduced soley via
 * the guest / host feature flags then.
 */
typedef struct _VBoxShClConnect
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Maximum chunk size for data transfers. */
    HGCMFunctionParameter cbChunkSize;
    /** uint32_t, in/out: Compression type. Currently unused. */
    HGCMFunctionParameter enmCompression;
    /** uint32_t, in/out: Checksum type used for data transfer. Currently unused. */
    HGCMFunctionParameter enmChecksumType;
} VBoxShClConnect;

#define VBOX_SHCL_CPARMS_CONNECT 3

/**
 * Reports available formats.
 */
typedef struct _VBoxShClFormatsMsg
{
    VBGLIOCHGCMCALL hdr;

    union
    {
        struct
        {
            /** uint32_t, out:  VBOX_SHCL_FMT_*. */
            HGCMFunctionParameter uFormats;
        } v0;

        struct
        {
            /** uint64_t, out: Context ID. */
            HGCMFunctionParameter uContext;
            /** uint32_t, out: VBOX_SHCL_FMT_*. */
            HGCMFunctionParameter uFormats;
            /** uint32_t, out: Format flags. */
            HGCMFunctionParameter fFlags;
        } v1;
    } u;
} VBoxShClFormatsMsg;

/**
 * Requests to read clipboard data.
 */
typedef struct _VBoxShClReadDataReqMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, out: Request flags; currently unused and must be set to 0. */
    HGCMFunctionParameter fFlags;
    /** uint32_t, out: Requested format to read data in. */
    HGCMFunctionParameter uFormat;
    /** uint32_t, out: Maximum size (in bytes) to read. */
    HGCMFunctionParameter cbSize;
} VBoxShClReadDataReqMsg;

#define VBOX_SHCL_CPARMS_READ_DATA_REQ 4

/**
 * Reads clipboard data.
 */
typedef struct _VBoxShClReadDataMsg
{
    VBGLIOCHGCMCALL hdr;

    union
    {
        struct
        {
            /** uint32_t, out: Requested format. */
            HGCMFunctionParameter format; /* IN uint32_t */
            /** ptr, out: The data buffer. */
            HGCMFunctionParameter ptr;    /* IN linear pointer. */
            /** uint32_t, out: Size of returned data, if > ptr->cb, then no data was
             *  actually transferred and the guest must repeat the call.
             */
            HGCMFunctionParameter size;   /* OUT uint32_t */
        } v0;
        struct
        {
            /** uint64_t, out: Context ID. */
            HGCMFunctionParameter uContext;
            /** uint32_t, out: Read flags; currently unused and must be set to 0. */
            HGCMFunctionParameter fFlags;
            /** uint32_t, out: Requested format. */
            HGCMFunctionParameter uFormat;
            /** uint32_t, in/out:
             *  On input:  How much data to read max.
             *  On output: Size of returned data, if > ptr->cb, then no data was
             *  actually transferred and the guest must repeat the call.
             */
            HGCMFunctionParameter cbData;
            /** ptr, out: The data buffer. */
            HGCMFunctionParameter pvData;
        } v1;
    } u;
} VBoxShClReadDataMsg;

#define VBOX_SHCL_CPARMS_READ_DATA 5

/**
 * Writes clipboard data.
 */
typedef struct _VBoxShClWriteDataMsg
{
    VBGLIOCHGCMCALL hdr;

    union
    {
        struct
        {
            /** Returned format as requested in the VBOX_SHCL_HOST_MSG_READ_DATA message. */
            HGCMFunctionParameter format; /* IN uint32_t */
            /** Data.  */
            HGCMFunctionParameter ptr;    /* IN linear pointer. */
        } v0;
        struct
        {
            /** uint64_t, out: Context ID. */
            HGCMFunctionParameter uContext;
            /** uint32_t, out: Write flags; currently unused and must be set to 0. */
            HGCMFunctionParameter fFlags;
            /** uint32_t, out: Requested format to read data in. */
            HGCMFunctionParameter uFormat;
            /** uint32_t, out: Size of data (in bytes). */
            HGCMFunctionParameter cbData;
            /** ptr, out: Actual data. */
            HGCMFunctionParameter pvData;
        } v1;
    } u;
} VBoxShClWriteDataMsg;

#define VBOX_SHCL_CPARMS_WRITE_DATA 5

/**
 * Reports a transfer status.
 */
typedef struct _VBoxShClTransferStatusMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, out: Direction of transfer; of type SHCLTRANSFERDIR_. */
    HGCMFunctionParameter enmDir;
    /** uint32_t, out: Status to report; of type SHCLTRANSFERSTATUS_. */
    HGCMFunctionParameter enmStatus;
    /** uint32_t, out: Result code to report. Optional. */
    HGCMFunctionParameter rc;
    /** uint32_t, out: Reporting flags. Currently unused and must be 0. */
    HGCMFunctionParameter fFlags;
} VBoxShClTransferStatusMsg;

#define VBOX_SHCL_CPARMS_TRANSFER_STATUS 5

/**
 * Asks the host for the next command to process, along
 * with the needed amount of parameters and an optional blocking
 * flag.
 *
 * Used by: VBOX_SHCL_GUEST_FN_GET_HOST_MSG
 *
 */
typedef struct _VBoxShClGetHostMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Message ID. */
    HGCMFunctionParameter uMsg;
    /** uint32_t, out: Number of parameters the message needs. */
    HGCMFunctionParameter cParms;
    /** uint32_t, in: Whether or not to block (wait) for a  new message to arrive. */
    HGCMFunctionParameter fBlock;
} VBoxShClPeekMsg;

#define VBOX_SHCL_CPARMS_GET_HOST_MSG 3

/** No listing flags specified. */
#define VBOX_SHCL_LIST_FLAG_NONE          0
/** Only returns one entry per read. */
#define VBOX_SHCL_LIST_FLAG_RETURN_ONE    RT_BIT(0)
/** Restarts reading a list from the beginning. */
#define VBOX_SHCL_LIST_FLAG_RESTART       RT_BIT(1)

#define VBOX_SHCL_LISTHDR_FLAG_NONE       0

/** No additional information provided. */
#define VBOX_SHCL_INFO_FLAG_NONE          0
/** Get object information of type SHCLFSOBJINFO. */
#define VBOX_SHCL_INFO_FLAG_FSOBJINFO     RT_BIT(0)

/**
 * Status messag for lists and objects.
 */
typedef struct _VBoxShClStatusMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Transfer status of type SHCLTRANSFERSTATUS. */
    HGCMFunctionParameter uStatus;
    /** uint32_t, in: Size of payload of this status, based on the status type. */
    HGCMFunctionParameter cbPayload;
    /** pointer, in: Optional payload of this status, based on the status type. */
    HGCMFunctionParameter pvPayload;
} VBoxShClStatusMsg;

#define VBOX_SHCL_CPARMS_STATUS 4

/** Invalid message type, do not use. */
#define VBOX_SHCL_REPLYMSGTYPE_INVALID           0
/** Replies a transfer status. */
#define VBOX_SHCL_REPLYMSGTYPE_TRANSFER_STATUS   1
/** Replies a list open status. */
#define VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN         2
/** Replies a list close status. */
#define VBOX_SHCL_REPLYMSGTYPE_LIST_CLOSE        3
/** Replies an object open status. */
#define VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN          4
/** Replies an object close status. */
#define VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE         5

/**
 * Generic reply message.
 */
typedef struct _VBoxShClReplyMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, out: Message type of type VBOX_SHCL_REPLYMSGTYPE_XXX. */
    HGCMFunctionParameter enmType;
    /** uint32_t, out: IPRT result of overall operation. */
    HGCMFunctionParameter rc;
    /** uint32_t, out: Size of optional payload of this reply, based on the message type. */
    HGCMFunctionParameter cbPayload;
    /** pointer, out: Optional payload of this reply, based on the message type. */
    HGCMFunctionParameter pvPayload;
    union
    {
        struct
        {
            HGCMFunctionParameter enmStatus;
        } TransferStatus;
        struct
        {
            HGCMFunctionParameter uHandle;
        } ListOpen;
        struct
        {
            HGCMFunctionParameter uHandle;
        } ObjOpen;
        struct
        {
            HGCMFunctionParameter uHandle;
        } ObjClose;
    } u;
} VBoxShClReplyMsg;

/** Minimum parameters (HGCM function parameters minus the union) a reply message must have. */
#define VBOX_SHCL_CPARMS_REPLY_MIN 5

/**
 * Structure for keeping root list message parameters.
 */
typedef struct _VBoxShClRootListParms
{
    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Roots listing flags; unused at the moment. */
    HGCMFunctionParameter fRoots;
} VBoxShClRootListParms;

/**
 * Requests to read the root list header.
 */
typedef struct _VBoxShClRootListReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClRootListParms ReqParms;
} VBoxShClRootListReadReqMsg;

#define VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ_REQ 2

/**
 * Reads / Writes a root list header.
 */
typedef struct _VBoxShClRootListHdrMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClRootListParms ReqParms;
    /** uint64_t, in/out: Number of total root list entries. */
    HGCMFunctionParameter cRoots;
} VBoxShClRootListHdrMsg;

#define VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ  3
#define VBOX_SHCL_CPARMS_ROOT_LIST_HDR_WRITE 3

/**
 * Structure for keeping list entry message parameters.
 */
typedef struct _VBoxShClRootListEntryParms
{
    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: VBOX_SHCL_INFO_FLAG_XXX. */
    HGCMFunctionParameter fInfo;
    /** uint32_t, in: Index of root list entry to get (zero-based). */
    HGCMFunctionParameter uIndex;
} VBoxShClRootListEntryParms;

/**
 * Request to read a list root entry.
 */
typedef struct _VBoxShClRootListEntryReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in: Request parameters. */
    VBoxShClRootListEntryParms Parms;
} VBoxShClRootListEntryReadReqMsg;

#define VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ_REQ 3

/**
 * Reads / Writes a root list entry.
 */
typedef struct _VBoxShClRootListEntryMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in/out: Request parameters. */
    VBoxShClRootListEntryParms Parms;
    /** pointer, in/out: Entry name. */
    HGCMFunctionParameter      szName;
    /** uint32_t, out: Bytes to be used for information/How many bytes were used.  */
    HGCMFunctionParameter      cbInfo;
    /** pointer, in/out: Information to be set/get (SHCLFSOBJINFO only currently).
     *  Do not forget to set the SHCLFSOBJINFO::Attr::enmAdditional for Get operation as well.  */
    HGCMFunctionParameter      pvInfo;
} VBoxShClRootListEntryMsg;

#define VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ  6
#define VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_WRITE 6

/**
 * Opens a list.
 */
typedef struct _VBoxShClListOpenMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Listing flags (see VBOX_SHCL_LIST_FLAG_XXX). */
    HGCMFunctionParameter fList;
    /** uint32_t, in: Size (in bytes) of the filter string. */
    HGCMFunctionParameter cbFilter;
    /** pointer, in: Filter string. */
    HGCMFunctionParameter pvFilter;
    /** uint32_t, in: Size (in bytes) of the listing path. */
    HGCMFunctionParameter cbPath;
    /** pointer, in: Listing poth. If empty or NULL the listing's root path will be opened. */
    HGCMFunctionParameter pvPath;
    /** uint64_t, out: List handle. */
    HGCMFunctionParameter uHandle;
} VBoxShClListOpenMsg;

#define VBOX_SHCL_CPARMS_LIST_OPEN 7

/**
 * Closes a list.
 */
typedef struct _VBoxShClListCloseMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in/out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
} VBoxShClListCloseMsg;

#define VBOX_SHCL_CPARMS_LIST_CLOSE 2

typedef struct _VBoxShClListHdrReqParms
{
    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: Flags of type VBOX_SHCL_LISTHDR_FLAG_XXX. */
    HGCMFunctionParameter fFlags;
} VBoxShClListHdrReqParms;

/**
 * Request to read a list header.
 */
typedef struct _VBoxShClListHdrReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClListHdrReqParms ReqParms;
} VBoxShClListHdrReadReqMsg;

#define VBOX_SHCL_CPARMS_LIST_HDR_READ_REQ 3

/**
 * Reads / Writes a list header.
 */
typedef struct _VBoxShClListHdrMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClListHdrReqParms ReqParms;
    /** uint32_t, in/out: Feature flags (see VBOX_SHCL_FEATURE_FLAG_XXX). */
    HGCMFunctionParameter   fFeatures;
    /** uint64_t, in/out:  Number of total objects to transfer. */
    HGCMFunctionParameter   cTotalObjects;
    /** uint64_t, in/out:  Number of total bytes to transfer. */
    HGCMFunctionParameter   cbTotalSize;
} VBoxShClListHdrMsg;

#define VBOX_SHCL_CPARMS_LIST_HDR 6

typedef struct _VBoxShClListEntryReqParms
{
    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: VBOX_SHCL_INFO_FLAG_XXX. */
    HGCMFunctionParameter fInfo;
} VBoxShClListEntryReqParms;

/**
 * Request to read a list entry.
 */
typedef struct _VBoxShClListEntryReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClListEntryReqParms ReqParms;
} VBoxShClListEntryReadReqMsg;

#define VBOX_SHCL_CPARMS_LIST_ENTRY_READ 3

/**
 * Reads / Writes a list entry.
 */
typedef struct _VBoxShClListEntryMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in/out: Request parameters. */
    VBoxShClListEntryReqParms ReqParms;
    /** pointer, in/out: Entry name. */
    HGCMFunctionParameter          szName;
    /** uint32_t, out: Bytes to be used for information/How many bytes were used.  */
    HGCMFunctionParameter          cbInfo;
    /** pointer, in/out: Information to be set/get (SHCLFSOBJINFO only currently).
     *  Do not forget to set the SHCLFSOBJINFO::Attr::enmAdditional for Get operation as well.  */
    HGCMFunctionParameter          pvInfo;
} VBoxShClListEntryMsg;

#define VBOX_SHCL_CPARMS_LIST_ENTRY 6

/**
 * Opens a Shared Clipboard object.
 */
typedef struct _VBoxShClObjOpenMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in/out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in/out: Object handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in/out: Size (in bytes) of absoulte path of object to open/create. */
    HGCMFunctionParameter cbPath;
    /** pointer, in/out: Absoulte path of object to open/create. */
    HGCMFunctionParameter szPath;
    /** uint32_t in/out: Open / Create flags of type SHCL_OBJ_CF_. */
    HGCMFunctionParameter fCreate;
} VBoxShClObjOpenMsg;

#define VBOX_SHCL_CPARMS_OBJ_OPEN 5

/**
 * Closes a Shared Clipboard object.
 */
typedef struct _VBoxShClObjCloseMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in/out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: SHCLOBJHANDLE of object to close. */
    HGCMFunctionParameter uHandle;
} VBoxShClObjCloseMsg;

#define VBOX_SHCL_CPARMS_OBJ_CLOSE 2

/**
 * Structure for keeping read parameters of a Shared Clipboard object.
 */
typedef struct _VBoxShClObjReadReqParms
{
    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: SHCLOBJHANDLE of object to write to. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: How many bytes to read. */
    HGCMFunctionParameter cbToRead;
    /** uint32_t, in: Read flags. Currently unused and must be 0. */
    HGCMFunctionParameter fRead;
} VBoxShClObjReadReqParms;

/**
 * Reads from a Shared Clipboard object.
 */
typedef struct _VBoxShClObjReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxShClObjReadReqParms ReqParms;
} VBoxShClObjReadReqMsg;

#define VBOX_SHCL_CPARMS_OBJ_READ_REQ 4

/**
 * Reads / writes data of / to an object.
 *
 * Used by:
 * VBOX_SHCL_FN_OBJ_READ
 * VBOX_SHCL_FN_OBJ_WRITE
 */
typedef struct _VBoxShClObjReadWriteMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in/out: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in/out: SHCLOBJHANDLE of object to write to. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in/out: Size (in bytes) of current data chunk. */
    HGCMFunctionParameter cbData;
    /** pointer, in/out: Current data chunk. */
    HGCMFunctionParameter pvData;
    /** uint32_t, in/out: Size (in bytes) of current data chunk checksum. */
    HGCMFunctionParameter cbChecksum;
    /** pointer, in/out: Checksum of data block, based on the checksum
     *  type in the data header. Optional. */
    HGCMFunctionParameter pvChecksum;
} VBoxShClObjReadWriteMsg;

#define VBOX_SHCL_CPARMS_OBJ_READ  6
#define VBOX_SHCL_CPARMS_OBJ_WRITE 6

/**
 * Sends an error event.
 *
 * Used by:
 * VBOX_SHCL_FN_WRITE_ERROR
 */
typedef struct _VBoxShClErrorMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint64_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: The error code (IPRT-style). */
    HGCMFunctionParameter rc;
} VBoxShClWriteErrorMsg;

#define VBOX_SHCL_CPARMS_ERROR 2

#pragma pack()

#if 0 /** @todo r=bird: Wrong file + unused. Remove or put where they belong! */
/**
 * Structure for keeping a Shared Clipboard file data chunk.
 *
 * @returns VBox status code.
 */
typedef struct _SHCLFILEDATA
{
    /** Current file data chunk. */
    void    *pvData;
    /** Size (in bytes) of current data chunk. */
    uint32_t cbData;
    /** Checksum for current file data chunk. */
    void    *pvChecksum;
    /** Size (in bytes) of current data chunk. */
    uint32_t cbChecksum;
} SHCLFILEDATA, *PSHCLFILEDATA;

/**
 * Structure for keeping Shared Clipboard error data.
 */
typedef struct _SHCLERRORDATA
{
    int32_t rc;
} SHCLERRORDATA, *PSHCLERRORDATA;
#endif

#endif /* !VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h */

