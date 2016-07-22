/* $Id$ */
/** @file
 * Encodes the screen content in VPX format.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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

struct VIDEORECCONTEXT;
typedef struct VIDEORECCONTEXT *PVIDEORECCONTEXT;

struct VIDEORECSTREAM;
typedef struct VIDEORECSTREAM *PVIDEORECSTREAM;

int  VideoRecContextCreate(PVIDEORECCONTEXT *ppCtx, uint32_t cScreens);
int  VideoRecStrmInit(PVIDEORECCONTEXT pCtx, uint32_t uScreen, const char *pszFile,
                      uint32_t uWidth, uint32_t uHeight, uint32_t uRate, uint32_t uFps,
                      uint32_t uMaxTime, uint32_t uMaxFileSize, const char *pszOptions);
void VideoRecContextClose(PVIDEORECCONTEXT pCtx);
bool VideoRecIsEnabled(PVIDEORECCONTEXT pCtx);
int  VideoRecCopyToIntBuf(PVIDEORECCONTEXT pCtx, uint32_t uScreen,
                          uint32_t x, uint32_t y, uint32_t uPixelFormat, uint32_t uBitsPerPixel,
                          uint32_t uBytesPerLine, uint32_t uGuestWidth, uint32_t uGuestHeight,
                          uint8_t *pu8BufferAddress, uint64_t u64TimeStamp);
bool VideoRecIsReady(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t u64TimeStamp);
bool VideoRecIsFull(PVIDEORECCONTEXT pCtx, uint32_t uScreen, uint64_t u64TimeStamp);

#endif /* !____H_VIDEOREC */

