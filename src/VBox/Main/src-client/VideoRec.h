/* $Id$ */
/** @file
 * Encodes the screen content in VPX format.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIDEOREC
#define ____H_VIDEOREC

#include <iprt/critsect.h>

#include "EbmlWriter.h"

struct VIDEORECCONTEXT;
typedef struct VIDEORECCONTEXT *PVIDEORECCONTEXT;

int VideoRecContextCreate(PVIDEORECCONTEXT *ppVideoRecContext);
int VideoRecContextInit(PVIDEORECCONTEXT pVideoRecContext, com::Bstr mFileName,
                        uint32_t uWidth, uint32_t uHeight, uint32_t uRate);
void VideoRecContextClose(PVIDEORECCONTEXT pVideoRecContext);
bool VideoRecIsEnabled(PVIDEORECCONTEXT pVideoRecContext);
int VideoRecCopyToIntBuffer(PVIDEORECCONTEXT pVideoRecContext, uint32_t x,
                            uint32_t y, uint32_t uPixelFormat, uint32_t uBitsPerPixel,
                            uint32_t uBytesPerLine, uint32_t uGuestHeight, uint32_t uGuestWidth,
                            uint8_t *pu8BufferAddress);
int VideoRecDoRGBToYUV(PVIDEORECCONTEXT pVideoRecContext, uint32_t u32PixelFormat);
int VideoRecEncodeAndWrite(PVIDEORECCONTEXT pVideoRecContext,
                           uint32_t uSourceWidth, uint32_t uSourceHeight);

#endif /* !____H_VIDEOREC */

