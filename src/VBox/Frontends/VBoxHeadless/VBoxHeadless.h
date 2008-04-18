/** @file
 *
 * VBox frontends: VRDP (headless RDP server):
 * Header file with registration call for ffmpeg framebuffer
 */

/*
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

#ifndef __H_VBOXVRDP
#define __H_VBOXVRDP

#include <VBox/com/VirtualBox.h>

#include <iprt/uuid.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include <iprt/runtime.h>
#include <iprt/critsect.h>

/**
 * Callback function to register an ffmpeg framebuffer.
 *
 * @returns COM status code.
 * @param   width        Framebuffer width.
 * @param   height       Framebuffer height.
 * @param   bitrate      Bitrate of mpeg file to be created.
 * @param   pixelFormat  Framebuffer pixel format
 * @param   filename     Name of mpeg file to be created
 * @retval  retVal       The new framebuffer
 */
typedef DECLCALLBACK(HRESULT) FNREGISTERFFMPEGFB(ULONG width,
                                     ULONG height, ULONG bitrate,
                                     com::Bstr filename,
                                     IFramebuffer **retVal);
typedef FNREGISTERFFMPEGFB *PFNREGISTERFFMPEGFB;

#endif // __H_VBOXVRDP
