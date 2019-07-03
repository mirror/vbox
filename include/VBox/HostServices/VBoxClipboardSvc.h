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

#define VBOX_SHARED_CLIPBOARD_GUEST_FN_TRANSFER_REPORT   5
/** New message for getting the next message from the host (see VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG_OLD).
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG      6
/** Opens / gets a list handle from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN         7
/** Closes a list handle from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE        8
/** Reads a list header from the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ     9
/** Writes a list header to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE    10
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ   11
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE  12
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN          13
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE         14
/** New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ          15
/**  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_WRITE         16
/** Reports cancellation of the current operation to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL            17
/** Reports an error to the host.
 *  New since URI handling was implemented. */
#define VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR             18

/** The maximum default chunk size for a single data transfer. */
#define VBOX_SHARED_CLIPBOARD_MAX_CHUNK_SIZE             _64K

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

    /** Message ID. */
    HGCMFunctionParameter uMsg;      /* OUT uint32_t */
    /** Number of parameters the message needs. */
    HGCMFunctionParameter cParms;    /* OUT uint32_t */
    /** Whether or not to block (wait) for a
     *  new message to arrive. */
    HGCMFunctionParameter fBlock;    /* OUT uint32_t */
} VBoxClipboardGetHostMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG 3

/** @todo might be necessary for future. */
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_NONE          0
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_RETURN_ONE    RT_BIT(0)
#define VBOX_SHAREDCLIPBOARD_LIST_FLAG_RESTART       RT_BIT(1)

#define VBOX_SHAREDCLIPBOARD_INFO_FLAG_NONE         0
/** Get object information of type SHAREDCLIPBOARDFSOBJINFO. */
#define VBOX_SHAREDCLIPBOARD_INFO_FLAG_FSOBJINFO     RT_BIT(0)

/**
 * Opens a list.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN
 */
typedef struct _VBoxClipboardListOpenMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, out: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in/out: Listing flags (see VBOX_SHAREDCLIPBOARD_LIST_FLAG_XXX). */
    HGCMFunctionParameter fList;
    /** uint32_t, in/out: Feature flags (see VBOX_SHAREDCLIPBOARD_FEATURE_FLAG_XXX). */
    HGCMFunctionParameter fFeatures;
    /** uint32_t, in: Size (in bytes) of the filter string. */
    HGCMFunctionParameter cbFilter;
    /** pointer, in: Filter string. */
    HGCMFunctionParameter pvFilter;
} VBoxClipboardListOpenMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN 5

/**
 * Closes a list.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN
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

/**
 * Reads / writes a list header.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE
 */
typedef struct _VBoxClipboardListHdrMsg
{
    VBGLIOCHGCMCALL hdr;

    /** uint32_t, in/out: Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** value64, out:  Number of total objects to transfer. */
    HGCMFunctionParameter cTotalObjects;
    /** value64, out:  Number of total bytes to transfer. */
    HGCMFunctionParameter cbTotalSize;
    /** Compression type. */
    HGCMFunctionParameter enmCompression;
    /** Checksum type used for data transfer. */
    HGCMFunctionParameter enmChecksumType;
} VBoxClipboardListHdrReadMsg, VBoxClipboardListHdrWriteMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ  6
#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_WRITE 6

/**
 * Reads / writes a list entry.
 *
 * Used by:
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ
 * VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE
 */
typedef struct _VBoxClipboardListEntryMsg
{
    VBGLIOCHGCMCALL hdr;

    /** Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;
    /** uint64_t, in: List handle. */
    HGCMFunctionParameter uHandle;
    /** uint32_t, in/out: VBOX_SHAREDCLIPBOARD_INFO_FLAG_XXX. */
    HGCMFunctionParameter fInfo;
    /** uint32_t, in/out: Bytes to be used for information/How many bytes were used.  */
    HGCMFunctionParameter cbInfo;
    /** pointer, in/out: Information to be set/get (SHAREDCLIPBOARDFSOBJINFO only currently).
     * Do not forget to set the SHAREDCLIPBOARDFSOBJINFO::Attr::enmAdditional for Get operation as well.  */
    HGCMFunctionParameter pvInfo;
} VBoxClipboardListEntryReadMsg, VBoxClipboardListEntryWriteMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ  5
#define VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_WRITE 5

typedef struct _VBoxClipboardObjOpenMsg
{
   VBGLIOCHGCMCALL hdr;

    /** Absoulte path of object to open/create. */
    HGCMFunctionParameter cbPath;
    /** Absoulte path of object to open/create. */
    HGCMFunctionParameter szPath;
    /** Points to SHAREDCLIPBOARDCREATEPARMS buffer. */
    HGCMFunctionParameter parms;

} VBoxClipboardObjOpenMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_OPEN 2

typedef struct _VBoxClipboardObjCloseMsg
{
   VBGLIOCHGCMCALL hdr;

    /** value64, in: SHAREDCLIPBOARDOBJHANDLE of object to close. */
    HGCMFunctionParameter uHandle;
} VBoxClipboardObjCloseMsg;

#define VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_CLOSE 1

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

    /** Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;     /* OUT uint32_t */
    /** value64, in: SHAREDCLIPBOARDOBJHANDLE of object to write to. */
    HGCMFunctionParameter uHandle;
    /** Size (in bytes) of current data chunk. */
    HGCMFunctionParameter cbData;       /* OUT uint32_t */
    /** Current data chunk. */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    /** Size (in bytes) of current data chunk checksum. */
    HGCMFunctionParameter cbChecksum;   /* OUT uint32_t */
    /** Checksum of data block, based on the checksum
     *  type in the data header. Optional. */
    HGCMFunctionParameter pvChecksum;   /* OUT ptr */
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxSvcClipboardURIGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTHDR pListHdr);
int VBoxSvcClipboardURISetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTHDR pListHdr);
int VBoxSvcClipboardURIGetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTENTRY pListEntry);
int VBoxSvcClipboardURISetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDLISTENTRY pListEntry);
int VBoxSvcClipboardURIGetFileData(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDFILEDATA pFileData);
int VBoxSvcClipboardURISetFileData(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDFILEDATA pFileData);
#endif

#endif /* !VBOX_INCLUDED_HostServices_VBoxClipboardSvc_h */

