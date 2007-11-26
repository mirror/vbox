/** @file
 *
 * VBox frontends: VRDP (headless RDP server):
 * Header file with registration call for ffmpeg framebuffer
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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
