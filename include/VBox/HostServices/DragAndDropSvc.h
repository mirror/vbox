/** @file
 * Drag and Drop service - Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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

#define hasDnDCopyAction(a)   ((a) && DND_COPY_ACTION)
#define hasDnDMoveAction(a)   ((a) && DND_MOVE_ACTION)
#define hasDnDLinkAction(a)   ((a) && DND_LINK_ACTION)

#define isDnDIgnoreAction(a)  ((a) == DND_IGNORE_ACTION)
#define isDnDCopyAction(a)    ((a) == DND_COPY_ACTION)
#define isDnDMoveAction(a)    ((a) == DND_MOVE_ACTION)
#define isDnDLinkAction(a)    ((a) == DND_LINK_ACTION)

/* Everything defined in this file lives in this namespace. */
namespace DragAndDropSvc {

/******************************************************************************
* Typedefs, constants and inlines                                             *
******************************************************************************/

/**
 * The service functions which are callable by host.
 */
enum eHostFn
{
    HOST_DND_SET_MODE                  = 100,

    /* H->G */
    HOST_DND_HG_EVT_ENTER              = 200,
    HOST_DND_HG_EVT_MOVE,
    HOST_DND_HG_EVT_LEAVE,
    HOST_DND_HG_EVT_DROPPED,
    HOST_DND_HG_EVT_CANCEL,
    HOST_DND_HG_SND_DATA,
    HOST_DND_HG_SND_MORE_DATA,
    HOST_DND_HG_SND_DIR,
    HOST_DND_HG_SND_FILE,

    /* G->H */
    HOST_DND_GH_REQ_PENDING            = 300,
    HOST_DND_GH_EVT_DROPPED
};

/**
 * The service functions which are called by guest.
 */
enum eGuestFn
{
    /**
     * Guest waits for a new message the host wants to process on the guest side.
     * This is a blocking call and can be deferred.
     */
    GUEST_DND_GET_NEXT_HOST_MSG        = 300,

    /* H->G */
    GUEST_DND_HG_ACK_OP                = 400,
    GUEST_DND_HG_REQ_DATA,
    GUEST_DND_HG_EVT_PROGRESS,

    /* G->H */
    GUEST_DND_GH_ACK_PENDING           = 500,
    GUEST_DND_GH_SND_DATA,
    GUEST_DND_GH_EVT_ERROR
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
    HGCMFunctionParameter cData;        /* OUT uint32_t */
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
    HGCMFunctionParameter cData;        /* OUT uint32_t */
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
    HGCMFunctionParameter cName;        /* OUT uint32_t */
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
    HGCMFunctionParameter cName;        /* OUT uint32_t */
    HGCMFunctionParameter pvData;       /* OUT ptr */
    HGCMFunctionParameter cData;        /* OUT uint32_t */
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
    HGCMFunctionParameter pData;        /* OUT ptr */
    HGCMFunctionParameter uSize;        /* OUT uint32_t */
} VBOXDNDGHSENDDATAMSG;

typedef struct VBOXDNDGHEVTERRORMSG
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * GH Cancel Data event.
     *
     * Used by:
     * GUEST_DND_GH_EVT_CANCEL
     */
    HGCMFunctionParameter uRC;          /* OUT uint32_t */
} VBOXDNDGHEVTERRORMSG;

#pragma pack()

/*
 * Callback handler
 */
enum
{
    CB_MAGIC_DND_HG_ACK_OP       = 0xe2100b93,
    CB_MAGIC_DND_HG_REQ_DATA     = 0x5cb3faf9,
    CB_MAGIC_DND_HG_EVT_PROGRESS = 0x8c8a6956,
    CB_MAGIC_DND_GH_ACK_PENDING  = 0xbe975a14,
    CB_MAGIC_DND_GH_SND_DATA     = 0x4eb61bff,
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
} VBOXDNDCBHGEVTPROGRESSDATA;
typedef VBOXDNDCBHGEVTPROGRESSDATA *PVBOXDNDCBHGEVTPROGRESSDATA ;

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
    uint32_t  cbAllSize;
} VBOXDNDCBSNDDATADATA;
typedef VBOXDNDCBSNDDATADATA *PVBOXDNDCBSNDDATADATA;

typedef struct VBOXDNDCBEVTERRORDATA
{
    /** Callback data header. */
    VBOXDNDCBHEADERDATA hdr;
    int32_t  rc;
} VBOXDNDCBEVTERRORDATA;
typedef VBOXDNDCBEVTERRORDATA *PVBOXDNDCBEVTERRORDATA;


} /* namespace DragAndDropSvc */

#endif  /* !___VBox_HostService_DragAndDropSvc_h */

