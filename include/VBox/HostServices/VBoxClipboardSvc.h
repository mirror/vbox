/** @file
 *
 * Shared Clipboard:
 * Common header for host service and guest clients.
 */

/*
 * Copyright (C) 2006-2007 InnoTek Systemberatung GmbH
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

#ifndef __VBOXCLIPBOARDSVC__H
#define __VBOXCLIPBOARDSVC__H

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>

#define VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE   1

#define VBOX_SHARED_CLIPBOARD_MODE_OFF           0
#define VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST 1
#define VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST 2
#define VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL 3

#define VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT 0
#define VBOX_SHARED_CLIPBOARD_FMT_BITMAP      1

#define VBOX_SHARED_CLIPBOARD_FN_QUERY_MODE        1
#define VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_CANCEL 2
#define VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_QUERY  3
#define VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_READ   4
#define VBOX_SHARED_CLIPBOARD_FN_SEND_DATA         5

#pragma pack (1)
typedef struct _VBoxClipboardQueryMode
{
    VBoxGuestHGCMCallInfo hdr;

    HGCMFunctionParameter mode; /* OUT uint32_t */

} VBoxClipboardQueryMode;

typedef struct _VBoxClipboardHostEventCancel
{
    VBoxGuestHGCMCallInfo hdr;
} VBoxClipboardHostEventCancel;

typedef struct _VBoxClipboardHostEventQuery
{
    VBoxGuestHGCMCallInfo hdr;
    
    HGCMFunctionParameter format; /* OUT uint32_t */
    
    HGCMFunctionParameter size;   /* OUT uint32_t */
} VBoxClipboardHostEventQuery;

typedef struct _VBoxClipboardHostEventRead
{
    VBoxGuestHGCMCallInfo hdr;
    
    HGCMFunctionParameter ptr;    /* OUT linear pointer. */
} VBoxClipboardHostEventRead;

typedef struct _VBoxClipboardSendData
{
    VBoxGuestHGCMCallInfo hdr;
    
    HGCMFunctionParameter format; /* IN uint32_t */
    
    HGCMFunctionParameter ptr;    /* IN linear pointer. */
} VBoxClipboardSendData;
#pragma pack ()

#endif /* __VBOXCLIPBOARDSVC__H */
