/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
 * Display driver entry points.
 *
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
 *
 */

#ifndef __VBOXIOCTL__H
#define __VBOXIOCTL__H

#include <VBox/VBoxGuest.h>

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
    DECLCALLBACKMEMBER(void, pfnFlush) (void *pvFlush);

    /** Pointer required by the pfnFlush callback. */
    void *pvFlush;

} VBVAENABLERESULT;
#pragma pack()

#endif /* __VBOXIOCTL__H */
