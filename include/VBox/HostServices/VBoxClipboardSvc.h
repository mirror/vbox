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

#ifndef VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h
#define VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/VMMDevCoreTypes.h>
#include <VBox/VBoxGuestCoreTypes.h>
#include <VBox/hgcmsvc.h>

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
#include <VBox/GuestHost/SharedClipboard-uri.h>
#endif

/*
 * The mode of operations.
 */
#define VBOX_SHARED_CLIPBOARD_MODE_OFF           0
#define VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST 1
#define VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST 2
#define VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL 3

/*
 * The service functions which are callable by host.
 */
/** Sets the current Shared Clipboard operation mode. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE           1
/** Run headless on the host, i.e. do not touch the host clipboard. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_SET_HEADLESS       2
/** Reports cancellation of the current operation to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_CANCEL             3
/** Reports an error to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_ERROR              4
/** Reports that a new clipboard area has been registered. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_AREA_REGISTER      5
/** Reports that a clipboard area has been unregistered. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_AREA_UNREGISTER    6
/** Reports that a client (host / guest) has attached to a clipboard area. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_AREA_ATTACH        7
/** Reports that a client (host / guest) has detached from a clipboard area. */
#define VBOX_SHARED_CLIPBOARD_HOST_FN_AREA_DETACH        8

/**
 * The host messages for the guest.
 */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT                   1
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA              2
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS         3

/** Initiates a new transfer (read / write) on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START           50
/** Reads the root list header from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_HDR_READ       51
/** Writes the root list header to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_HDR_WRITE      52
/** Reads a root list entry from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_ENTRY_READ     53
/** Writes a root list entry to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_ENTRY_WRITE    54
/** Open an URI list on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN                55
/** Closes a formerly opened URI list on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE               56
/** Reads a list header from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ            57
/** Writes a list header to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE           58
/** Reads a list entry from the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ          59
/** Writes a list entry to the guest. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE         60
/** Open an URI object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN                 61
/** Closes a formerly opened URI object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE                62
/** Reads from an object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ                 63
/** Writes to an object on the guest side. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE                64
/** Indicates that the host has canceled a transfer. */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL                   65
/** Indicates that the an unrecoverable error on the host occurred . */
#define VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR                    66

/*
 * The service functions which are called by guest.
 */
/** Calls the host and waits (blocking) for an host event VBOX_SHARED_CLIPBOARD_HOST_MSG_*.
 *  Note: This is the old message which still is being used for the non-URI Shared Clipboard transfers,
 *        to not break compatibility with older Guest Additions / VBox versions. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG_OLD  1
/** Sends a list of available formats to the host. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_REPORT_FORMATS    2
/** Reads data in specified format from the host. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA         3
/** Writes data in requested format to the host. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA        4

/** Peeks at the next message, returning immediately.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_NOWAIT        5
/** Peeks at the next message, waiting for one to arrive.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT          6
/** Gets the next message, returning immediately.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET                7
/** Sends a transfer status to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS                 8
/** Replies to a function from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY                  9
/** Reports the available root entries of a transfer.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_READ     10
/** Reports the available root entries of a transfer.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_WRITE    11
/** Reports the available root entries of a transfer.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_READ   12
/** Reports the available root entries of a transfer.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_WRITE  13
/** Opens / gets a list handle from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN              14
/** Closes a list handle from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE             15
/** Reads a list header from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ          16
/** Writes a list header to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE         17
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ        18
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE       19
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN               20
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE              21
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ               22
/**  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_WRITE              23
/** Reports cancellation of the current operation to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL                 24
/** Reports an error to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR                  25

/** The maximum default chunk size for a single data transfer. */
#define VBOX_SHARED_CLIPBOARD_MAX_CHUNK_SIZE                  _64K

/** Maximum number of concurrent Shared Clipboard transfers a VM can have. */
#define VBOX_SHARED_CLIPBOARD_MAX_TRANSFERS                   UINT16_MAX
/** Maximum number of concurrent events a transfer can have. */
#define VBOX_SHARED_CLIPBOARD_MAX_EVENTS                      UINT16_MAX

/** Creates a context ID out of a transfer ID. */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(uTransfer, uEvent) \
    RT_MAKE_U32(uEvent, uTransfer)
/** Gets the transfer ID out of a context ID. */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(uContextID) \
    RT_HI_U16(uContextID)
/** Gets the event ID out of a context ID. */
#define VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(uContextID) \
    RT_LO_U16(uContextID)

/*
 * HGCM parameter structures.
 */
#pragma pack(1)
typedef struct _VBoxClipboardGetHostMsgOld
{
    VBGLIOCHGCMCALL hdr;

    /* VBOX_SHARED_CLIPBOARD_HOST_MSG_* */
    HGCMFunctionParameter msg;     /* OUT uint32_t */

    /* VBOX_SHARED_CLIPBOARD_FMT_*, depends on the 'msg'. */
    HGCMFunctionParameter formats; /* OUT uint32_t */
} VBoxClipboardGetHostMsgOld;

#define VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG_OLD 2

typedef struct _VBoxClipboardReportFormatsMsg
{
    VBGLIOCHGCMCALL hdr;

    /* VBOX_SHARED_CLIPBOARD_FMT_* */
    HGCMFunctionParameter formats; /* OUT uint32_t */
} VBoxClipboardReportFormatsMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_REPORT_FORMATS 1

typedef struct _VBoxClipboardReadDataMsg
{
    VBGLIOCHGCMCALL hdr;

    /* Requested format. */
    HGCMFunctionParameter format; /* IN uint32_t */

    /* The data buffer. */
    HGCMFunctionParameter ptr;    /* IN linear pointer. */

    /* Size of returned data, if > ptr->cb, then no data was
     * actually transferred and the guest must repeat the call.
     */
    HGCMFunctionParameter size;   /* OUT uint32_t */

} VBoxClipboardReadDataMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA 3

typedef struct _VBoxClipboardWriteDataMsg
{
    VBGLIOCHGCMCALL hdr;

    /* Returned format as requested in the VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message. */
    HGCMFunctionParameter format; /* IN uint32_t */

    /* Data.  */
    HGCMFunctionParameter ptr;    /* IN linear pointer. */
} VBoxClipboardWriteDataMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA 2

typedef struct _VBoxClipboardTransferReport
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint32_t, out: Status to report. */
    HGCMFunctionParameter uStatus;
} VBoxClipboardTransferReport;

#define VBOX_SHARED_CLIPBOARD_CPARMS_TRANSFER_REPORT 2

/**
 * Asks the host for the next command to process, along
 * with the needed amount of parameters and an optional blocking
 * flag.
 *
 * Used by: VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG
 *
 */
typedef struct _VBoxClipboardGetHostMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Message ID. */
    HGCMFunctionParameter uMsg;
    /** uint32_t, out: Number of parameters the message needs. */
    HGCMFunctionParameter cParms;
    /** uint32_t, in: Whether or not to block (wait) for a  new message to arrive. */
    HGCMFunctionParameter fBlock;
} VBoxClipboardPeekMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG 3

/** @todo might be necessary for future. */
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_NONE          0
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_RETURN_ONE    RT_BIT(0)
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_RESTART       RT_BIT(1)

#define VBOX_SHAREDCLIPBOARD_LISTHDR_FLAG_NONE       0

/** No additional information provided. */
#define VBOX_SHAREDCLIPBOARD_INFO_FLAG_NONE          0
/** Get object information of type SHAREDCLIPBOARDFSOBJINFO. */
#define VBOX_SHAREDCLIPBOARD_INFO_FLAG_FSOBJINFO     RT_BIT(0)

/**
 * Transfert status message.
 */
typedef struct _VBoxClipboardStatusMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Transfer status of type SHAREDCLIPBOARDURITRANSFERSTATUS. */
    HGCMFunctionParameter uStatus;
    /** uint32_t, in: Size of payload of this status, based on the status type. */
    HGCMFunctionParameter cbPayload;
    /** pointer, in: Optional payload of this status, based on the status type. */
    HGCMFunctionParameter pvPayload;
} VBoxClipboardStatusMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_STATUS 4

#define VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_INVALID           0
#define VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_OPEN         1
#define VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_CLOSE        2
#define VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_OBJ_OPEN          3
#define VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_OBJ_CLOSE         4

/**
 * Generic reply message.
 */
typedef struct _VBoxClipboardReplyMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint32_t, out: Message type of type VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_XXX. */
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
} VBoxClipboardReplyMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_REPLY_MIN 5

typedef struct _VBoxClipboardRootListParms
{
    /** uint32_t, in: Context ID. s*/
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Roots listing flags; unused at the moment. */
    HGCMFunctionParameter fRoots;
} VBoxClipboardRootListParms;

/**
 * Requests to read the root list.
 */
typedef struct _VBoxClipboardRootListReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardRootListParms ReqParms;
} VBoxClipboardRootListReadReqMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_HDR_READ 2

/**
 * Reads / Writes a root list header.
 */
typedef struct _VBoxClipboardRootListHdrMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardRootListParms ReqParms;
    /** uint64_t, in/out: Number of total root list entries. */
    HGCMFunctionParameter      cRoots;
    /** uint32_t, in/out: Compression type. */
    HGCMFunctionParameter      enmCompression;
    /** uint32_t, in/out: Checksum type used for data transfer. */
    HGCMFunctionParameter      enmChecksumType;
} VBoxClipboardRootListHdrMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_HDR 5

typedef struct _VBoxClipboardRootListEntryParms
{
    /** uint32_t, in: Context ID. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: VBOX_SHAREDCLIPBOARD_INFO_FLAG_XXX. */
    HGCMFunctionParameter fInfo;
    /** uint32_t, in: Index of root list entry to get (zero-based). */
    HGCMFunctionParameter uIndex;
} VBoxClipboardRootListEntryParms;

/**
 * Request to read a list root entry.
 */
typedef struct _VBoxClipboardRootListEntryReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in: Request parameters. */
    VBoxClipboardRootListEntryParms Parms;
} VBoxClipboardRootListEntryReadReqMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_ENTRY_READ_REQ 3

/**
 * Reads / Writes a root list entry.
 */
typedef struct _VBoxClipboardRootListEntryMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in/out: Request parameters. */
    VBoxClipboardRootListEntryParms Parms;
    /** pointer, in/out: Entry name. */
    HGCMFunctionParameter           szName;
    /** uint32_t, out: Bytes to be used for information/How many bytes were used.  */
    HGCMFunctionParameter           cbInfo;
    /** pointer, in/out: Information to be set/get (SHAREDCLIPBOARDFSOBJINFO only currently).
     *  Do not forget to set the SHAREDCLIPBOARDFSOBJINFO::Attr::enmAdditional for Get operation as well.  */
    HGCMFunctionParameter           pvInfo;
} VBoxClipboardRootListEntryMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_ROOT_LIST_ENTRY 6

/**
 * Opens a list.
 */
typedef struct _VBoxClipboardListOpenMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: Listing flags (see VBOX_SHAREDCLIPBOARD_LIST_FLAG_XXX). */
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
} VBoxClipboardListOpenMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN 7

/**
 * Closes a list.
 */
typedef struct _VBoxClipboardListCloseMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
} VBoxClipboardListCloseMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE 2

typedef struct _VBoxClipboardListHdrReqParms
{
    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: Flags of type VBOX_SHAREDCLIPBOARD_LISTHDR_FLAG_XXX. */
    HGCMFunctionParameter fFlags;
} VBoxClipboardListHdrReqParms;

/**
 * Request to read a list header.
 */
typedef struct _VBoxClipboardListHdrReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardListHdrReqParms ReqParms;
} VBoxClipboardListHdrReadReqMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ_REQ 3

/**
 * Reads / Writes a list header.
 */
typedef struct _VBoxClipboardListHdrMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardListHdrReqParms ReqParms;
    /** uint32_t, in/out: Feature flags (see VBOX_SHAREDCLIPBOARD_FEATURE_FLAG_XXX). */
    HGCMFunctionParameter        fFeatures;
    /** uint64_t, in/out:  Number of total objects to transfer. */
    HGCMFunctionParameter        cTotalObjects;
    /** uint64_t, in/out:  Number of total bytes to transfer. */
    HGCMFunctionParameter        cbTotalSize;
    /** uint32_t, in/out: Compression type. */
    HGCMFunctionParameter        enmCompression;
    /** uint32_t, in/out: Checksum type used for data transfer. */
    HGCMFunctionParameter        enmChecksumType;
} VBoxClipboardListHdrMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR 8

typedef struct _VBoxClipboardListEntryReqParms
{
    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: VBOX_SHAREDCLIPBOARD_INFO_FLAG_XXX. */
    HGCMFunctionParameter fInfo;
} VBoxClipboardListEntryReqParms;

/**
 * Request to read a list entry.
 */
typedef struct _VBoxClipboardListEntryReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardListEntryReqParms ReqParms;
} VBoxClipboardListEntryReadReqMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ 3

/**
 * Reads / Writes a list entry.
 */
typedef struct _VBoxClipboardListEntryMsg
{
    VBGLIOCHGCMCALL hdr;

    /** in/out: Request parameters. */
    VBoxClipboardListEntryReqParms ReqParms;
    /** pointer, in/out: Entry name. */
    HGCMFunctionParameter          szName;
    /** uint32_t, out: Bytes to be used for information/How many bytes were used.  */
    HGCMFunctionParameter          cbInfo;
    /** pointer, in/out: Information to be set/get (SHAREDCLIPBOARDFSOBJINFO only currently).
     *  Do not forget to set the SHAREDCLIPBOARDFSOBJINFO::Attr::enmAdditional for Get operation as well.  */
    HGCMFunctionParameter          pvInfo;
} VBoxClipboardListEntryMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY 6

typedef struct _VBoxClipboardObjOpenMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in/out: Object handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in/out: Size (in bytes) of absoulte path of object to open/create. */
    HGCMFunctionParameter cbPath;
    /** pointer, in/out: Absoulte path of object to open/create. */
    HGCMFunctionParameter szPath;
    /** uint32_t in/out: Open / Create flags of type SHAREDCLIPBOARD_OBJ_CF_. */
    HGCMFunctionParameter fCreate;
    /** pointer, in/out: SHAREDCLIPBOARDFSOBJINFO. */
    HGCMFunctionParameter objInfo;
} VBoxClipboardObjOpenMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_OPEN 4

typedef struct _VBoxClipboardObjCloseMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: SHAREDCLIPBOARDOBJHANDLE of object to close. */
    HGCMFunctionParameter uHandle;
} VBoxClipboardObjCloseMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_CLOSE 2

typedef struct _VBoxClipboardObjReadReqParms
{
    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: SHAREDCLIPBOARDOBJHANDLE of object to write to. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in: How many bytes to read. */
    HGCMFunctionParameter cbToRead;
    /** uint32_t, in: Read flags. Currently unused. */
    HGCMFunctionParameter fRead;
} VBoxClipboardObjReadReqParms;

typedef struct _VBoxClipboardObjReadReqMsg
{
    VBGLIOCHGCMCALL hdr;

    VBoxClipboardObjReadReqParms ReqParms;
} VBoxClipboardObjReadReqMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_READ_REQ 3

/**
 * Reads / writes data of / to an object.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_FN_OBJ_READ
 * VBOX_SHARED_CLIPBOARD_FN_OBJ_WRITE
 */
typedef struct _VBoxClipboardObjReadWriteMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in/out: SHAREDCLIPBOARDOBJHANDLE of object to write to. */
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
} VBoxClipboardObjReadWriteMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_READ  6
#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_WRITE 6

/**
 * Sends an error event.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR
 */
typedef struct _VBoxClipboardErrorMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint32_t, in: The error code (IPRT-style). */
    HGCMFunctionParameter rc;
} VBoxClipboardWriteErrorMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_ERROR 2

#pragma pack()

typedef struct _VBOXCLIPBOARDFILEDATA
{
    /** Current file data chunk. */
    void                       *pvData;
    /** Size (in bytes) of current data chunk. */
    uint32_t                    cbData;
    /** Checksum for current file data chunk. */
    void                       *pvChecksum;
    /** Size (in bytes) of current data chunk. */
    uint32_t                    cbChecksum;
} VBOXCLIPBOARDFILEDATA, *PVBOXCLIPBOARDFILEDATA;

typedef struct _VBOXCLIPBOARDERRORDATA
{
    int32_t                     rc;
} VBOXCLIPBOARDERRORDATA, *PVBOXCLIPBOARDERRORDATA;

bool VBoxSvcClipboardGetHeadless(void);
bool VBoxSvcClipboardLock(void);
void VBoxSvcClipboardUnlock(void);

#endif /* !VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h */

