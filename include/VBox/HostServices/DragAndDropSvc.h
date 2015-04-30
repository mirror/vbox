/** @file
 * Drag and Drop service - Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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

#ifndef ___VBox_HostService_DragAndDropSvc_h
#define ___VBox_HostService_DragAndDropSvc_h

#include <VBox/hgcmsvc.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest2.h>

/*
 * The mode of operations.
 */
#define VBOX_DRAG_AND_DROP_MODE_OFF           0
#define VBOX_DRAG_AND_DROP_MODE_HOST_TO_GUEST 1
#define VBOX_DRAG_AND_DROP_MODE_GUEST_TO_HOST 2
#define VBOX_DRAG_AND_DROP_MODE_BIDIRECTIONAL 3

#define DND_IGNORE_ACTION     UINT32_C(0)
#define DND_COPY_ACTION       RT_BIT_32(0)
#define DND_MOVE_ACTION       RT_BIT_32(1)
#define DND_LINK_ACTION       RT_BIT_32(2)

#define hasDnDCopyAction(a)   ((a) & DND_COPY_ACTION)
#define hasDnDMoveAction(a)   ((a) & DND_MOVE_ACTION)
#define hasDnDLinkAction(a)   ((a) & DND_LINK_ACTION)

#define isDnDIgnoreAction(a)  ((a) == DND_IGNORE_ACTION)
#define isDnDCopyAction(a)    ((a) == DND_COPY_ACTION)
#define isDnDMoveAction(a)    ((a) == DND_MOVE_ACTION)
#define isDnDLinkAction(a)    ((a) == DND_LINK_ACTION)

/** @def VBOX_DND_FORMATS_DEFAULT
 * Default drag'n drop formats.
 * Note: If you add new entries here, make sure you test those
 *       with all supported guest OSes!
 */
#define VBOX_DND_FORMATS_DEFAULT                                                                \
    "text/uri-list",                                                                            \
    /* Text. */                                                                                 \
    "text/html",                                                                                \
    "text/plain;charset=utf-8",                                                                 \
    "text/plain;charset=utf-16",                                                                \
    "text/plain",                                                                               \
    "text/richtext",                                                                            \
    "UTF8_STRING",                                                                              \
    "TEXT",                                                                                     \
    "STRING",                                                                                   \
    /* OpenOffice formats. */                                                                   \
    /* See: https://wiki.openoffice.org/wiki/Documentation/DevGuide/OfficeDev/Common_Application_Features#OpenOffice.org_Clipboard_Data_Formats */ \
    "application/x-openoffice-embed-source-xml;windows_formatname=\"Star Embed Source (XML)\"", \
    "application/x-openoffice;windows_formatname=\"Bitmap\""

namespace DragAndDropSvc {

/******************************************************************************
* Typedefs, constants and inlines                                             *
******************************************************************************/

/**
 * The service functions which are callable by host.
 * Note: When adding new functions to this table, make sure that the actual ID
 *       does *not* overlap with the eGuestFn enumeration below!
 */
enum eHostFn
{
    /** The host just has set a new DnD mode. */
    HOST_DND_SET_MODE                  = 100,

    /*
     * Host -> Guest messages
     */

    /** The host entered the VM window for starting an actual
     *  DnD operation. */
    HOST_DND_HG_EVT_ENTER              = 200,
    /** The host's DnD cursor moved within the VM window. */
    HOST_DND_HG_EVT_MOVE               = 201,
    /** The host left the guest VM window. */
    HOST_DND_HG_EVT_LEAVE              = 202,
    /** The host issued a "drop" event, meaning that the host is
     *  ready to transfer data over to the guest. */
    HOST_DND_HG_EVT_DROPPED            = 203,
    /** The host requested to cancel the current DnD operation. */
    HOST_DND_HG_EVT_CANCEL             = 204,
    /** Gets the actual MIME data, based on
     *  the format(s) specified by HOST_DND_HG_EVT_ENTER. If the guest
     *  supplied buffer too small to send the actual data, the host
     *  will send a HOST_DND_HG_SND_MORE_DATA message as follow-up. */
    HOST_DND_HG_SND_DATA               = 205,
    /** Sent when the actual buffer for HOST_DND_HG_SND_DATA
     *  was too small, issued by the DnD host service. */
    HOST_DND_HG_SND_MORE_DATA          = 206,
    /** Directory entry to be sent to the guest. */
    HOST_DND_HG_SND_DIR                = 207,
    /** File data chunk to send to the guest. */
    HOST_DND_HG_SND_FILE_DATA          = 208,
    /** File header to send to the guest.
     *  Note: Only for protocol version 2 and up (>= VBox 5.0). */
    HOST_DND_HG_SND_FILE_HDR           = 209,

    /*
     * Guest -> Host messages
     */

    /** The host asks the guest whether a DnD operation
     *  is in progress when the mouse leaves the guest window. */
    HOST_DND_GH_REQ_PENDING            = 600,
    /** The host informs the guest that a DnD drop operation
     *  has been started and that the host wants the data in
     *  a specific MIME type. */
    HOST_DND_GH_EVT_DROPPED            = 601,
    /** Creates a directory on the guest. */
    HOST_DND_GH_RECV_DIR               = 650,
    /** Retrieves file data from the guest. */
    HOST_DND_GH_RECV_FILE_DATA         = 670,
    /** Retrieves a file header from the guest.
     *  Note: Only for protocol version 2 and up (>= VBox 5.0). */
    HOST_DND_GH_RECV_FILE_HDR          = 671,
    /** Blow the type up to 32-bit. */
    HOST_DND_32BIT_HACK                = 0x7fffffff
};

/**
 * The service functions which are called by guest.
 * Note: When adding new functions to this table, make sure that the actual ID
 *       does *not* overlap with the eHostFn enumeration above!
 */
enum eGuestFn
{
    /* Guest sends a connection request to the HGCM service.
     * Note: New since protocol version 2. */
    GUEST_DND_CONNECT                  = 10,

    /**
     * Guest waits for a new message the host wants to process
     * on the guest side. This can be a blocking call.
     */
    GUEST_DND_GET_NEXT_HOST_MSG        = 300,

    /*
     * Host -> Guest operation messages.
     */

    /** The guest acknowledges that the pending DnD data from
     *  the host can be dropped on the currently selected source
     *  on the guest. */
    GUEST_DND_HG_ACK_OP                = 400,
    /** The guest requests the actual DnD data to be sent
     *  from the host. */
    GUEST_DND_HG_REQ_DATA              = 401,
    /** Reports back the guest's progress on a host -> guest operation. */
    GUEST_DND_HG_EVT_PROGRESS          = 402,

    /*
     * Guest -> Host operation messages.
     */

    /**
     * The guests acknowledges that it currently has a drag'n drop
     * operation in progress on the guest, which eventually could be
     * dragged over to the host.
     */
    GUEST_DND_GH_ACK_PENDING           = 500,
    /**
     * Sends data of the requested format to the host. There can
     * be more than one message if the actual data does not fit
     * into one.
     */
    GUEST_DND_GH_SND_DATA              = 501,
    /** Reports an error back to the host. */
    GUEST_DND_GH_EVT_ERROR             = 502,
    /** Guest sends a directory entry to the host. */
    GUEST_DND_GH_SND_DIR               = 700,
    /** Guest sends file data to the host. */
    /*  Note: On protocol version 1 this also contains the file name
     *        and other attributes. */
    GUEST_DND_GH_SND_FILE_DATA         = 701,
    /** Guest sends a file header to the host, marking the
     *  beginning of a (new) file transfer.
     *  Note: Available since protocol version 2 (VBox 5.0). */
    GUEST_DND_GH_SND_FILE_HDR          = 702,
    /** Blow the type up to 32-bit. */
    GUEST_DND_32BIT_HACK               = 0x7fffffff
};

/**
 * DnD operation progress states.
 */
typedef enum DNDPROGRESS
{
    DND_PROGRESS_UNKNOWN               = 0,
    DND_PROGRESS_RUNNING               = 1,
    DND_PROGRESS_COMPLETE,
    DND_PROGRESS_CANCELLED,
    DND_PROGRESS_ERROR,
    /** Blow the type up to 32-bit. */
    DND_PROGRESS_32BIT_HACK            = 0x7fffffff
} DNDPROGRESS, *PDNDPROGRESS;

#pragma pack (1)

/*
 * Host events
 */

typedef struct VBOXDNDHGACTIONMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Action event.
     *
     * Used by:
     * HOST_DND_HG_EVT_ENTER
     * HOST_DND_HG_EVT_MOVE
     * HOST_DND_HG_EVT_DROPPED
     */
    HGCMFunctionParameter uScreenId;    /* OUT uint32_t */
    HGCMFunctionParameter uX;           /* OUT uint32_t */
    HGCMFunctionParameter uY;           /* OUT uint32_t */
    HGCMFunctionParameter uDefAction;   /* OUT uint32_t */
    HGCMFunctionParameter uAllActions;  /* OUT uint32_t */
    HGCMFunctionParameter pvFormats;    /* OUT ptr */
    HGCMFunctionParameter cFormats;     /* OUT uint32_t */
} VBOXDNDHGACTIONMSG;

typedef struct VBOXDNDHGLEAVEMSG
{
    VBoxGuestHGCMCallInfo hdr;
    /**
     * HG Leave event.
     *
     * Used by:
     * HOST_DND_HG_EVT_LEAVE
     */
} VBOXDNDHGLEAVEMSG;

typedef struct VBOXDNDHGCANCELMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Cancel return event.
     *
     * Used by:
     * HOST_DND_HG_EVT_CANCEL
     */
} VBOXDNDHGCANCELMSG;

typedef struct VBOXDNDHGSENDDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Send Data event.
     *
     * Used by:
     * HOST_DND_HG_SND_DATA
     */
    HGCMFunctionParameter uScreenId;    /* OUT uint32_t */
    HGCMFunctionParameter pvFormat;     /* OUT ptr */
    HGCMFunctionParameter cFormat;      /* OUT uint32_t */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    HGCMFunctionParameter cbData;       /* OUT uint32_t */
} VBOXDNDHGSENDDATAMSG;

typedef struct VBOXDNDHGSENDMOREDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Send More Data event.
     *
     * Used by:
     * HOST_DND_HG_SND_MORE_DATA
     */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    HGCMFunctionParameter cbData;       /* OUT uint32_t */
} VBOXDNDHGSENDMOREDATAMSG;

typedef struct VBOXDNDHGSENDDIRMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Directory event.
     *
     * Used by:
     * HOST_DND_HG_SND_DIR
     */
    HGCMFunctionParameter pvName;       /* OUT ptr */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
} VBOXDNDHGSENDDIRMSG;

/**
 * File header event.
 * Note: Only for protocol version 2 and up.
 *
 * Used by:
 * HOST_DND_HG_SND_FILE_HDR
 * HOST_DND_GH_SND_FILE_HDR
 */
typedef struct VBOXDNDHGSENDFILEHDRMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /** Context ID. Unused at the moment. */
    HGCMFunctionParameter uContext;     /* OUT uint32_t */
    /** File path. */
    HGCMFunctionParameter pvName;       /* OUT ptr */
    /** Size (in bytes) of file path. */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    /** Optional flags; unused at the moment. */
    HGCMFunctionParameter uFlags;       /* OUT uint32_t */
    /** File creation mode. */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
    /** Total size (in bytes). */
    HGCMFunctionParameter cbTotal;      /* OUT uint64_t */

} VBOXDNDHGSENDFILEHDRMSG;

/**
 * HG: File data (chunk) event.
 *
 * Used by:
 * HOST_DND_HG_SND_FILE
 */
typedef struct VBOXDNDHGSENDFILEDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    union
    {
        /* Note: Protocol v1 sends the file name + file mode
         *       every time a file data chunk is being sent. */
        struct
        {
            HGCMFunctionParameter pvName;       /* OUT ptr */
            HGCMFunctionParameter cbName;       /* OUT uint32_t */
            HGCMFunctionParameter pvData;       /* OUT ptr */
            HGCMFunctionParameter cbData;       /* OUT uint32_t */
            HGCMFunctionParameter fMode;        /* OUT uint32_t */
        } v1;
        struct
        {
            /** Note: pvName is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
            /** Note: cbName is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
            /** Context ID. Unused at the moment. */
            HGCMFunctionParameter uContext;     /* OUT uint32_t */
            HGCMFunctionParameter pvData;       /* OUT ptr */
            HGCMFunctionParameter cbData;       /* OUT uint32_t */
            /** Note: fMode is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
        } v2;
    } u;

} VBOXDNDHGSENDFILEDATAMSG;

typedef struct VBOXDNDGHREQPENDINGMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Request Pending event.
     *
     * Used by:
     * HOST_DND_GH_REQ_PENDING
     */
    HGCMFunctionParameter uScreenId;    /* OUT uint32_t */
} VBOXDNDGHREQPENDINGMSG;

typedef struct VBOXDNDGHDROPPEDMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Dropped event.
     *
     * Used by:
     * HOST_DND_GH_EVT_DROPPED
     */
    HGCMFunctionParameter pvFormat;     /* OUT ptr */
    HGCMFunctionParameter cFormat;      /* OUT uint32_t */
    HGCMFunctionParameter uAction;      /* OUT uint32_t */
} VBOXDNDGHDROPPEDMSG;

/*
 * Guest events
 */

/**
 * The returned command the host wants to
 * run on the guest.
 *
 * Used by:
 * GUEST_DND_GET_NEXT_HOST_MSG
 */
typedef struct VBOXDNDNEXTMSGMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter msg;          /* OUT uint32_t */
    /** Number of parameters the message needs. */
    HGCMFunctionParameter num_parms;    /* OUT uint32_t */
    /** Whether or not to block (wait) for a
     *  new message to arrive. */
    HGCMFunctionParameter block;        /* OUT uint32_t */

} VBOXDNDNEXTMSGMSG;

/**
 * Connection request. Used to tell the DnD protocol
 * version to the (host) service.
 *
 * Used by:
 * GUEST_DND_CONNECT
 */
typedef struct VBOXDNDCONNECTPMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /** Protocol version to use. */
    HGCMFunctionParameter uProtocol;     /* OUT uint32_t */
    /** Connection flags. Optional. */
    HGCMFunctionParameter uFlags;        /* OUT uint32_t */

} VBOXDNDCONNECTPMSG;

/**
 * HG Acknowledge Operation event.
 *
 * Used by:
 * GUEST_DND_HG_ACK_OP
 */
typedef struct VBOXDNDHGACKOPMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter uAction;      /* OUT uint32_t */
} VBOXDNDHGACKOPMSG;

/**
 * HG request for data event.
 *
 * Used by:
 * GUEST_DND_HG_REQ_DATA
 */
typedef struct VBOXDNDHGREQDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter pFormat;      /* OUT ptr */
} VBOXDNDHGREQDATAMSG;

typedef struct VBOXDNDHGEVTPROGRESSMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter uStatus;
    HGCMFunctionParameter uPercent;
    HGCMFunctionParameter rc;
} VBOXDNDHGEVTPROGRESSMSG;

/**
 * GH Acknowledge Pending event.
 *
 * Used by:
 * GUEST_DND_GH_ACK_PENDING
 */
typedef struct VBOXDNDGHACKPENDINGMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter uDefAction;   /* OUT uint32_t */
    HGCMFunctionParameter uAllActions;  /* OUT uint32_t */
    HGCMFunctionParameter pFormat;      /* OUT ptr */
} VBOXDNDGHACKPENDINGMSG;

/**
 * GH Send Data event.
 *
 * Used by:
 * GUEST_DND_GH_SND_DATA
 */
typedef struct VBOXDNDGHSENDDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter pvData;       /* OUT ptr */
    /** Total bytes to send. This can be more than
     *  the data block specified in pvData above, e.g.
     *  when sending over file objects afterwards. */
    HGCMFunctionParameter cbTotalBytes; /* OUT uint32_t */
} VBOXDNDGHSENDDATAMSG;

/**
 * GH Directory event.
 *
 * Used by:
 * GUEST_DND_GH_SND_DIR
 */
typedef struct VBOXDNDGHSENDDIRMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter pvName;       /* OUT ptr */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
} VBOXDNDGHSENDDIRMSG;

/**
 * GH File header event.
 * Note: Only for protocol version 2 and up.
 *
 * Used by:
 * HOST_DND_GH_SND_FILE_HDR
 */
typedef struct VBOXDNDHGSENDFILEHDRMSG VBOXDNDGHSENDFILEHDRMSG;

/**
 * GH File data event.
 *
 * Used by:
 * GUEST_DND_HG_SND_FILE_DATA
 */
typedef struct VBOXDNDGHSENDFILEDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    union
    {
        /* Note: Protocol v1 sends the file name + file mode
         *       every time a file data chunk is being sent. */
        struct
        {
            HGCMFunctionParameter pvName;   /* OUT ptr */
            HGCMFunctionParameter cbName;   /* OUT uint32_t */
            HGCMFunctionParameter fMode;    /* OUT uint32_t */
            HGCMFunctionParameter pvData;   /* OUT ptr */
            HGCMFunctionParameter cbData;   /* OUT uint32_t */
        } v1;
        struct
        {
            /** Note: pvName is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
            /** Note: cbName is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
            /** Context ID. Unused at the moment. */
            HGCMFunctionParameter uContext; /* OUT uint32_t */
            HGCMFunctionParameter pvData;   /* OUT ptr */
            HGCMFunctionParameter cbData;   /* OUT uint32_t */
            /** Note: fMode is now part of the VBOXDNDHGSENDFILEHDRMSG message. */
        } v2;
    } u;

} VBOXDNDGHSENDFILEDATAMSG;

/**
 * GH Error event.
 *
 * Used by:
 * GUEST_DND_GH_EVT_ERROR
 */
typedef struct VBOXDNDGHEVTERRORMSG
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter rc;               /* OUT uint32_t */
} VBOXDNDGHEVTERRORMSG;

#pragma pack()

/*
 * Callback data magics.
 */
enum
{
    CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG      = 0x19820126,
    CB_MAGIC_DND_HG_GET_NEXT_HOST_MSG_DATA = 0x19850630,
    CB_MAGIC_DND_HG_ACK_OP                 = 0xe2100b93,
    CB_MAGIC_DND_HG_REQ_DATA               = 0x5cb3faf9,
    CB_MAGIC_DND_HG_EVT_PROGRESS           = 0x8c8a6956,
    CB_MAGIC_DND_GH_ACK_PENDING            = 0xbe975a14,
    CB_MAGIC_DND_GH_SND_DATA               = 0x4eb61bff,
    CB_MAGIC_DND_GH_SND_DIR                = 0x411ca754,
    CB_MAGIC_DND_GH_SND_FILE_HDR           = 0x65e35eaf,
    CB_MAGIC_DND_GH_SND_FILE_DATA          = 0x19840804,
    CB_MAGIC_DND_GH_EVT_ERROR              = 0x117a87c4
};

typedef struct VBOXDNDCBHEADERDATA
{
    /** Magic number to identify the structure. */
    uint32_t                    u32Magic;
    /** Context ID to identify callback data. */
    uint32_t                    u32ContextID;
} VBOXDNDCBHEADERDATA, *PVBOXDNDCBHEADERDATA;

typedef struct VBOXDNDCBHGGETNEXTHOSTMSG
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    uint32_t                    uMsg;
    uint32_t                    cParms;
} VBOXDNDCBHGGETNEXTHOSTMSG, *PVBOXDNDCBHGGETNEXTHOSTMSG;

typedef struct VBOXDNDCBHGGETNEXTHOSTMSGDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    uint32_t                    uMsg;
    uint32_t                    cParms;
    PVBOXHGCMSVCPARM            paParms;
} VBOXDNDCBHGGETNEXTHOSTMSGDATA, *PVBOXDNDCBHGGETNEXTHOSTMSGDATA;

typedef struct VBOXDNDCBHGACKOPDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    uint32_t                    uAction;
} VBOXDNDCBHGACKOPDATA, *PVBOXDNDCBHGACKOPDATA;

typedef struct VBOXDNDCBHGREQDATADATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    char                       *pszFormat;
    uint32_t                    cbFormat;
} VBOXDNDCBHGREQDATADATA, *PVBOXDNDCBHGREQDATADATA;

typedef struct VBOXDNDCBHGEVTPROGRESSDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    uint32_t                    uPercentage;
    uint32_t                    uStatus;
    uint32_t                    rc;
} VBOXDNDCBHGEVTPROGRESSDATA, *PVBOXDNDCBHGEVTPROGRESSDATA;

typedef struct VBOXDNDCBGHACKPENDINGDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    uint32_t                    uDefAction;
    uint32_t                    uAllActions;
    char                       *pszFormat;
    uint32_t                    cbFormat;
} VBOXDNDCBGHACKPENDINGDATA, *PVBOXDNDCBGHACKPENDINGDATA;

typedef struct VBOXDNDCBSNDDATADATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    void                       *pvData;
    uint32_t                    cbData;
    /** Total metadata size (in bytes). This is transmitted
     *  with every message because the size can change. */
    uint32_t                    cbTotalSize;
} VBOXDNDCBSNDDATADATA, *PVBOXDNDCBSNDDATADATA;

typedef struct VBOXDNDCBSNDDIRDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    char                       *pszPath;
    uint32_t                    cbPath;
    uint32_t                    fMode;
} VBOXDNDCBSNDDIRDATA, *PVBOXDNDCBSNDDIRDATA;

/*  Note: Only for protocol version 2 and up (>= VBox 5.0). */
typedef struct VBOXDNDCBSNDFILEHDRDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    /** File path (name). */
    char                       *pszFilePath;
    /** Size (in bytes) of file path. */
    uint32_t                    cbFilePath;
    /** Total size (in bytes) of this file. */
    uint64_t                    cbSize;
    /** File (creation) mode. */
    uint32_t                    fMode;
    /** Additional flags. Not used at the moment. */
    uint32_t                    fFlags;
} VBOXDNDCBSNDFILEHDRDATA, *PVBOXDNDCBSNDFILEHDRDATA;

typedef struct VBOXDNDCBSNDFILEDATADATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    /** Current file data chunk. */
    void                       *pvData;
    /** Size (in bytes) of current data chunk. */
    uint32_t                    cbData;
    union
    {
        struct
        {
            /** File path (name). */
            char               *pszFilePath;
            /** Size (in bytes) of file path. */
            uint32_t            cbFilePath;
            /** File (creation) mode. */
            uint32_t            fMode;
        } v1;
        /* Note: Protocol version 2 has the file attributes (name, size,
                 mode, ...) in the VBOXDNDCBSNDFILEHDRDATA structure. */
    } u;
} VBOXDNDCBSNDFILEDATADATA, *PVBOXDNDCBSNDFILEDATADATA;

typedef struct VBOXDNDCBEVTERRORDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA         hdr;
    int32_t                     rc;
} VBOXDNDCBEVTERRORDATA, *PVBOXDNDCBEVTERRORDATA;

} /* namespace DragAndDropSvc */

#endif  /* !___VBox_HostService_DragAndDropSvc_h */

