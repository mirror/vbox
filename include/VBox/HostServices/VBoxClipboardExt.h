/** @file
 * Shared Clipboard - Common header for the service extension.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VBOX_INCLUDED_HostServices_VBoxClipboardExt_h
#define VBOX_INCLUDED_HostServices_VBoxClipboardExt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <VBox/GuestHost/SharedClipboard-transfers.h>
#endif

#define VBOX_CLIPBOARD_EXT_FN_SET_CALLBACK         (0)
#define VBOX_CLIPBOARD_EXT_FN_FORMAT_ANNOUNCE      (1)
#define VBOX_CLIPBOARD_EXT_FN_DATA_READ            (2)
#define VBOX_CLIPBOARD_EXT_FN_DATA_WRITE           (3)
/** Registers a new clipboard area.
 *  Uses the SHCLEXTAREAPARMS struct. */
#define VBOX_CLIPBOARD_EXT_FN_AREA_REGISTER        (4)
/** Unregisters an existing clipboard area.
 *  Uses the SHCLEXTAREAPARMS struct. */
#define VBOX_CLIPBOARD_EXT_FN_AREA_UNREGISTER      (5)
/** Attaches to an existing clipboard area.
 *  Uses the SHCLEXTAREAPARMS struct. */
#define VBOX_CLIPBOARD_EXT_FN_AREA_ATTACH          (6)
/** Detaches from an existing clipboard area.
 *  Uses the SHCLEXTAREAPARMS struct. */
#define VBOX_CLIPBOARD_EXT_FN_AREA_DETACH          (7)

typedef DECLCALLBACKTYPE(int, FNVRDPCLIPBOARDEXTCALLBACK,(uint32_t u32Function, uint32_t u32Format, void *pvData, uint32_t cbData));
typedef FNVRDPCLIPBOARDEXTCALLBACK *PFNVRDPCLIPBOARDEXTCALLBACK;

typedef struct _SHCLEXTPARMS
{
    uint32_t                        uFormat;
    union
    {
        void                       *pvData;
        PFNVRDPCLIPBOARDEXTCALLBACK pfnCallback;
    } u;
    uint32_t   cbData;
} SHCLEXTPARMS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
typedef uint32_t SHCLEXTAREAREGISTETRFLAGS;
/** No clipboard register area flags specified. */
#define SHCLEXTAREA_REGISTER_FLAGS_NONE        0

typedef uint32_t SHCLEXTAREAATTACHFLAGS;
/** No clipboard attach area flags specified. */
#define SHCLEXTAREA_ATTACH_FLAGS_NONE          0

/**
 * Structure for keeping clipboard area callback parameters.
 */
typedef struct _SHCLEXTAREAPARMS
{
    /** The clipboard area's ID the callback is for. */
    SHCLAREAID uID;
    union
    {
        struct
        {
            void                              *pvData;
            uint32_t                           cbData;
            /** Area register flags; not used yet and must be set to 0. */
            SHCLEXTAREAREGISTETRFLAGS fFlags;
        } fn_register;
        struct
        {
            /** Area attach flags; not used yet and must be set to 0. */
            SHCLEXTAREAATTACHFLAGS    fFlags;
        } fn_attach;
    } u;
} SHCLEXTAREAPARMS, *PSHCLEXTAREAPARMS;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

#endif /* !VBOX_INCLUDED_HostServices_VBoxClipboardExt_h */
