/** @file
 *
 * Shared Clipboard
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

#ifndef __VBOXCLIPBOARD__H
#define __VBOXCLIPBOARD__H

#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/log.h>

typedef enum _VBoxClipboardGuestStatus
{
    enmEventIdle,     /* The guest did not call the QueryEvent yet and
                       * therefore does not expect any data.
                       * It is allowed only call the Cancel and QueryEvent
                       * functions for host data.
                       */
    enmEventQueried,  /* The guest has called the QueryEvent function but no event
                       * was pending (fEventPresent == false) and therefore the
                       * request is processed asynchronously.
                       * Guest only allowed to call the Cancel function.
                       * If client in the state and a clipboard event occurs,
                       * the request is processed and client goes to EventReported state.
                       */
    enmEventReported  /* The information about a clipboard event was returned to client,
                       * either immediately or asynchronously.
                       * The client can call the Read or Cancel functions.
                       * Both functions reset the client to EventIdle state.
                       */
} VBoxClipboardGuestStatus;



typedef struct _VBOXCLIPBOARDCLIENTDATA
{
    struct _VBOXCLIPBOARDCLIENTDATA *pNext;
    struct _VBOXCLIPBOARDCLIENTDATA *pPrev;
    
    uint32_t u32ClientID;
    
    bool fEventPresent: 1; /* Event infomation has been latched and is valid. */
    bool fClipboardChanged: 1; /* Clipboard was changed since last latch. */
    bool fAsync: 1; /* Async processing for the QueryEvent is performed. */
    
    VBoxClipboardGuestStatus enmGuestStatus;  /* The guest status, as guest waits for the host events.  */
    
    struct {
        uint32_t u32Format;
        uint32_t u32Size;
        void *pv;
    } event;
    
    struct {
        VBOXHGCMCALLHANDLE callHandle;
        VBOXHGCMSVCPARM *paParms;
    } async;
    
    uint64_t u64LastSentCRC64;
    uint32_t u32LastSentFormat;
    
} VBOXCLIPBOARDCLIENTDATA;

bool vboxClipboardLock (void);
void vboxClipboardUnlock (void);

void vboxClipboardReportEvent (VBOXCLIPBOARDCLIENTDATA *pClient, bool fForce);


int vboxClipboardInit (void);
void vboxClipboardDestroy (void);

int vboxClipboardAddClient (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32ClientID);
void vboxClipboardRemoveClient (VBOXCLIPBOARDCLIENTDATA *pClient);

void vboxClipboardReset (VBOXCLIPBOARDCLIENTDATA *pClient);

void vboxClipboardSet (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format);


#endif /* __VBOXCLIPBOARD__H */
