/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
 * Display driver entry points.
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBOXIOCTL__H
#define __VBOXIOCTL__H

#include <VBox/VBoxGuest.h>

#ifdef VBOX_WITH_HGSMI
#include <VBox/HGSMI/HGSMI.h>
#endif /* VBOX_WITH_HGSMI */

#define IOCTL_VIDEO_INTERPRET_DISPLAY_MEMORY \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x420, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_QUERY_DISPLAY_INFO \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x421, METHOD_BUFFERED, FILE_ANY_ACCESS)

/** Called by the display driver when it is ready to
 *  switch to VBVA operation mode.
 *  Successful return means that VBVA can be used and
 *  output buffer contains VBVAENABLERESULT data.
 *  An error means that VBVA can not be used
 *  (disabled or not supported by the host).
 */
#define IOCTL_VIDEO_VBVA_ENABLE \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x400, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef VBOX_WITH_HGSMI
#define IOCTL_VIDEO_QUERY_HGSMI_INFO \
    CTL_CODE(FILE_DEVICE_VIDEO, 0x430, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif /* VBOX_WITH_HGSMI */

#pragma pack(1)
/**
 * Data returned by IOCTL_VIDEO_VBVA_ENABLE.
 *
 */
typedef struct _VBVAENABLERESULT
{
    /** Pointer to VBVAMemory part of VMMDev memory region. */
    VBVAMEMORY *pVbvaMemory;

    /** Called to force the host to process VBVA memory,
     *  when there is no more free space in VBVA memory.
     *  Normally this never happens.
     *
     *  The other purpose is to perform a synchronous command.
     *  But the goal is to have no such commands at all.
     */
    DECLR0CALLBACKMEMBER(void, pfnFlush, (void *pvFlush));

    /** Pointer required by the pfnFlush callback. */
    void *pvFlush;

} VBVAENABLERESULT;

/**
 * Data returned by IOCTL_VIDEO_QUERY_DISPLAY_INFO.
 *
 */
typedef struct _QUERYDISPLAYINFORESULT
{
    /* Device index (0 for primary) */
    ULONG iDevice;

    /* Size of the display information area. */
    uint32_t u32DisplayInfoSize;
} QUERYDISPLAYINFORESULT;

#ifdef VBOX_WITH_HGSMI
/**
 * Data returned by IOCTL_VIDEO_QUERY_HGSMI_INFO.
 *
 */
typedef struct _QUERYHGSMIRESULT
{
    /* Device index (0 for primary) */
    ULONG iDevice;

    /* Flags. Currently none are defined and the field must be initialized to 0. */
    ULONG ulFlags;

    /* Describes VRAM chunk for this display device. */
    HGSMIAREA areaDisplay;

    /* Size of the display information area. */
    uint32_t u32DisplayInfoSize;

    /* Minimum size of the VBAV buffer. */
    uint32_t u32MinVBVABufferSize;
} QUERYHGSMIRESULT;
#endif /* VBOX_WITH_HGSMI */
#pragma pack()

#endif /* __VBOXIOCTL__H */
