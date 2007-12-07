/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
 * Display driver entry points.
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBOXIOCTL__H
#define __VBOXIOCTL__H

#include <VBox/VBoxGuest.h>

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
#pragma pack()

#endif /* __VBOXIOCTL__H */
