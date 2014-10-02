/** @file
 * Drag and Drop service - Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2011-2014 Oracle Corporation
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
    "application/x-openoffice-embed-source-xml;windows_formatname=\"Star Embed Source (XML)\""  \
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
    HOST_DND_SET_MODE                  = 100,

    /*
     * Host -> Guest messages
     */

    HOST_DND_HG_EVT_ENTER              = 200,
    HOST_DND_HG_EVT_MOVE               = 201,
    HOST_DND_HG_EVT_LEAVE              = 202,
    HOST_DND_HG_EVT_DROPPED            = 203,
    HOST_DND_HG_EVT_CANCEL             = 204,
    /** Gets the actual MIME data, based on
     *  the format(s) specified by HOST_DND_HG_EVT_ENTER. */
    HOST_DND_HG_SND_DATA               = 205,
    /** Sent when the actual buffer for HOST_DND_HG_SND_DATA
     *  was too small, issued by the DnD host service. */
    HOST_DND_HG_SND_MORE_DATA          = 206,
    /** Directory entry to be handled on the guest. */
    HOST_DND_HG_SND_DIR                = 207,
    /** File entry to be handled on the guest. */
    HOST_DND_HG_SND_FILE               = 208,

    /*
     * Guest -> Host messages
     */

    /** The host asks the guest whether a DnD operation
     *  is in progress when the mouse leaves the guest window. */
    HOST_DND_GH_REQ_PENDING            = 600,
    /** The host informs the guest that a DnD drop operation
     *  has been started and that the host wants the data in
     *  a specific MIME type. */
    HOST_DND_GH_EVT_DROPPED,

    HOST_DND_GH_RECV_DIR               = 650,
    HOST_DND_GH_RECV_FILE              = 670
};

/**
 * The service functions which are called by guest.
 * Note: When adding new functions to this table, make sure that the actual ID
 *       does *not* overlap with the eGuestFn enumeration above!
 */
enum eGuestFn
{
    /**
     * Guest waits for a new message the host wants to process
     * on the guest side. This can be a blocking call.
     */
    GUEST_DND_GET_NEXT_HOST_MSG        = 300,

    /* H->G */
    /** The guest acknowledges that the pending DnD data from
     *  the host can be dropped on the currently selected source
     *  on the guest. */
    GUEST_DND_HG_ACK_OP                = 400,
    /** The guest requests the actual DnD data to be sent
     *  from the host. */
    GUEST_DND_HG_REQ_DATA              = 401,
    GUEST_DND_HG_EVT_PROGRESS          = 402,

    /* G->H */
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
    GUEST_DND_GH_EVT_ERROR             = 502,

    GUEST_DND_GH_SND_DIR               = 700,
    GUEST_DND_GH_SND_FILE              = 701
};

/**
 * The possible states for the progress operations.
 */
enum
{
    DND_PROGRESS_RUNNING = 1,
    DND_PROGRESS_COMPLETE,
    DND_PROGRESS_CANCELLED,
    DND_PROGRESS_ERROR
};

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

typedef struct VBOXDNDHGSENDFILEMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG File event.
     *
     * Used by:
     * HOST_DND_HG_SND_FILE
     */
    HGCMFunctionParameter pvName;       /* OUT ptr */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    HGCMFunctionParameter cbData;       /* OUT uint32_t */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
} VBOXDNDHGSENDFILEMSG;

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

typedef struct VBOXDNDNEXTMSGMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The returned command the host wants to
     * run on the guest.
     *
     * Used by:
     * GUEST_DND_GET_NEXT_HOST_MSG
     */
    HGCMFunctionParameter msg;          /* OUT uint32_t */
    /** Number of parameters the message needs. */
    HGCMFunctionParameter num_parms;    /* OUT uint32_t */
    /** Whether or not to block (wait) for a
     *  new message to arrive. */
    HGCMFunctionParameter block;        /* OUT uint32_t */

} VBOXDNDNEXTMSGMSG;

typedef struct VBOXDNDHGACKOPMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG Acknowledge Operation event.
     *
     * Used by:
     * GUEST_DND_HG_ACK_OP
     */
    HGCMFunctionParameter uAction;      /* OUT uint32_t */
} VBOXDNDHGACKOPMSG;

typedef struct VBOXDNDHGREQDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * HG request for data event.
     *
     * Used by:
     * GUEST_DND_HG_REQ_DATA
     */
    HGCMFunctionParameter pFormat;      /* OUT ptr */
} VBOXDNDHGREQDATAMSG;

typedef struct VBOXDNDGHACKPENDINGMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Acknowledge Pending event.
     *
     * Used by:
     * GUEST_DND_GH_ACK_PENDING
     */
    HGCMFunctionParameter uDefAction;   /* OUT uint32_t */
    HGCMFunctionParameter uAllActions;  /* OUT uint32_t */
    HGCMFunctionParameter pFormat;      /* OUT ptr */
} VBOXDNDGHACKPENDINGMSG;

typedef struct VBOXDNDGHSENDDATAMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Send Data event.
     *
     * Used by:
     * GUEST_DND_GH_SND_DATA
     */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    /** Total bytes to send. This can be more than
     *  the data block specified in pvData above, e.g.
     *  when sending over file objects afterwards. */
    HGCMFunctionParameter cbTotalBytes; /* OUT uint32_t */
} VBOXDNDGHSENDDATAMSG;

typedef struct VBOXDNDGHSENDDIRMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Directory event.
     *
     * Used by:
     * GUEST_DND_HG_SND_DIR
     */
    HGCMFunctionParameter pvName;       /* OUT ptr */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
} VBOXDNDGHSENDDIRMSG;

typedef struct VBOXDNDGHSENDFILEMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH File event.
     *
     * Used by:
     * GUEST_DND_HG_SND_FILE
     */
    HGCMFunctionParameter pvName;       /* OUT ptr */
    HGCMFunctionParameter cbName;       /* OUT uint32_t */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    HGCMFunctionParameter cbData;       /* OUT uint32_t */
    HGCMFunctionParameter fMode;        /* OUT uint32_t */
} VBOXDNDGHSENDFILEMSG;

typedef struct VBOXDNDGHEVTERRORMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Error event.
     *
     * Used by:
     * GUEST_DND_GH_EVT_ERROR
     */
    HGCMFunctionParameter uRC;          /* OUT uint32_t */
} VBOXDNDGHEVTERRORMSG;

#pragma pack()

/*
 * Callback data magics.
 */
enum
{
    CB_MAGIC_DND_HG_ACK_OP       = 0xe2100b93,
    CB_MAGIC_DND_HG_REQ_DATA     = 0x5cb3faf9,
    CB_MAGIC_DND_HG_EVT_PROGRESS = 0x8c8a6956,
    CB_MAGIC_DND_GH_ACK_PENDING  = 0xbe975a14,
    CB_MAGIC_DND_GH_SND_DATA     = 0x4eb61bff,
    CB_MAGIC_DND_GH_SND_DIR      = 0x411ca754,
    CB_MAGIC_DND_GH_SND_FILE     = 0x65e35eaf,
    CB_MAGIC_DND_GH_EVT_ERROR    = 0x117a87c4
};

typedef struct VBOXDNDCBHEADERDATA
{
    /** Magic number to identify the structure. */
    uint32_t u32Magic;
    /** Context ID to identify callback data. */
    uint32_t u32ContextID;
} VBOXDNDCBHEADERDATA;
typedef VBOXDNDCBHEADERDATA *PVBOXDNDCBHEADERDATA;

typedef struct VBOXDNDCBHGACKOPDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    uint32_t uAction;
} VBOXDNDCBHGACKOPDATA;
typedef VBOXDNDCBHGACKOPDATA *PVBOXDNDCBHGACKOPDATA;

typedef struct VBOXDNDCBHGREQDATADATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    char *pszFormat;
} VBOXDNDCBHGREQDATADATA;
typedef VBOXDNDCBHGREQDATADATA *PVBOXDNDCBHGREQDATADATA;

typedef struct VBOXDNDCBHGEVTPROGRESSDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    uint32_t uPercentage;
    uint32_t uState;
    int rc;
} VBOXDNDCBHGEVTPROGRESSDATA;
typedef VBOXDNDCBHGEVTPROGRESSDATA *PVBOXDNDCBHGEVTPROGRESSDATA;

typedef struct VBOXDNDCBGHACKPENDINGDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    uint32_t  uDefAction;
    uint32_t  uAllActions;
    char     *pszFormat;
} VBOXDNDCBGHACKPENDINGDATA;
typedef VBOXDNDCBGHACKPENDINGDATA *PVBOXDNDCBGHACKPENDINGDATA;

typedef struct VBOXDNDCBSNDDATADATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    void     *pvData;
    uint32_t  cbData;
    /** Total metadata size (in bytes). This is transmitted
     *  with every message because the size can change. */
    uint32_t  cbTotalSize;
} VBOXDNDCBSNDDATADATA;
typedef VBOXDNDCBSNDDATADATA *PVBOXDNDCBSNDDATADATA;

typedef struct VBOXDNDCBSNDDIRDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    char     *pszPath;
    uint32_t  cbPath;
    uint32_t  fMode;
} VBOXDNDCBSNDDIRDATA;
typedef VBOXDNDCBSNDDIRDATA *PVBOXDNDCBSNDDIRDATA;

typedef struct VBOXDNDCBSNDFILEDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    char     *pszFilePath;
    uint32_t  cbFilePath;
    uint32_t  fMode;
    void     *pvData;
    uint32_t  cbData;
} VBOXDNDCBSNDFILEDATA;
typedef VBOXDNDCBSNDFILEDATA *PVBOXDNDCBSNDFILEDATA;

typedef struct VBOXDNDCBEVTERRORDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    int32_t rc;
} VBOXDNDCBEVTERRORDATA;
typedef VBOXDNDCBEVTERRORDATA *PVBOXDNDCBEVTERRORDATA;

} /* namespace DragAndDropSvc */

#endif  /* !___VBox_HostService_DragAndDropSvc_h */

