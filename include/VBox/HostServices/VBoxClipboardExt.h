/** @file
 *
 * Shared Clipboard:
 * Common header for the service extension.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef __VBOXCLIPBOARDEXT__H
#define __VBOXCLIPBOARDEXT__H

#include <VBox/types.h>

#define VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK    (0)
#define VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE (1)
#define VBOX_CLIPBOARD_EXT_FN_DATA_READ       (2)
#define VBOX_CLIPBOARD_EXT_FN_DATA_WRITE      (3)

typedef DECLCALLBACK(int) VRDPCLIPBOARDEXTCALLBACK (uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData);
typedef VRDPCLIPBOARDEXTCALLBACK *PFNVRDPCLIPBOARDEXTCALLBACK;

typedef struct _VBOXCLIPBOARDEXTPARMS
{
    uint32_t   u32Format;
    void      *pvData;
    uint32_t   cbData;
} VBOXCLIPBOARDEXTPARMS;

#endif /* __VBOXCLIPBOARDEXT__H */
