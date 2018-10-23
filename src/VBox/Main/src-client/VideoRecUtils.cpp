/* $Id$ */
/** @file
 * Video recording utility code.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VideoRec.h"
#include "VideoRecUtils.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>


/**
 * Convert an image to YUV420p format.
 *
 * @return true on success, false on failure.
 * @param  aDstBuf              The destination image buffer.
 * @param  aDstWidth            Width (in pixel) of destination buffer.
 * @param  aDstHeight           Height (in pixel) of destination buffer.
 * @param  aSrcBuf              The source image buffer.
 * @param  aSrcWidth            Width (in pixel) of source buffer.
 * @param  aSrcHeight           Height (in pixel) of source buffer.
 */
template <class T>
inline bool videoRecColorConvWriteYUV420p(uint8_t *aDstBuf, unsigned aDstWidth, unsigned aDstHeight,
                                          uint8_t *aSrcBuf, unsigned aSrcWidth, unsigned aSrcHeight)
{
    RT_NOREF(aDstWidth, aDstHeight);

    AssertReturn(!(aSrcWidth & 1),  false);
    AssertReturn(!(aSrcHeight & 1), false);

    bool fRc = true;
    T iter1(aSrcWidth, aSrcHeight, aSrcBuf);
    T iter2 = iter1;
    iter2.skip(aSrcWidth);
    unsigned cPixels = aSrcWidth * aSrcHeight;
    unsigned offY = 0;
    unsigned offU = cPixels;
    unsigned offV = cPixels + cPixels / 4;
    unsigned const cyHalf = aSrcHeight / 2;
    unsigned const cxHalf = aSrcWidth  / 2;
    for (unsigned i = 0; i < cyHalf && fRc; ++i)
    {
        for (unsigned j = 0; j < cxHalf; ++j)
        {
            unsigned red, green, blue;
            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            unsigned u = (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            unsigned v = (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter1.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + aSrcWidth] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            fRc = iter2.getRGB(&red, &green, &blue);
            AssertReturn(fRc, false);
            aDstBuf[offY + aSrcWidth + 1] = ((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16;
            u += (((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128) / 4;
            v += (((112 * red - 94 * green -  18 * blue + 128) >> 8) + 128) / 4;

            aDstBuf[offU] = u;
            aDstBuf[offV] = v;
            offY += 2;
            ++offU;
            ++offV;
        }

        iter1.skip(aSrcWidth);
        iter2.skip(aSrcWidth);
        offY += aSrcWidth;
    }

    return true;
}

/**
 * Convert an image to RGB24 format
 * @returns true on success, false on failure
 * @param aWidth    width of image
 * @param aHeight   height of image
 * @param aDestBuf  an allocated memory buffer large enough to hold the
 *                  destination image (i.e. width * height * 12bits)
 * @param aSrcBuf   the source image as an array of bytes
 */
template <class T>
inline bool videoRecColorConvWriteRGB24(unsigned aWidth, unsigned aHeight,
                                        uint8_t *aDestBuf, uint8_t *aSrcBuf)
{
    enum { PIX_SIZE = 3 };
    bool rc = true;
    AssertReturn(0 == (aWidth & 1), false);
    AssertReturn(0 == (aHeight & 1), false);
    T iter(aWidth, aHeight, aSrcBuf);
    unsigned cPixels = aWidth * aHeight;
    for (unsigned i = 0; i < cPixels && rc; ++i)
    {
        unsigned red, green, blue;
        rc = iter.getRGB(&red, &green, &blue);
        if (rc)
        {
            aDestBuf[i * PIX_SIZE    ] = red;
            aDestBuf[i * PIX_SIZE + 1] = green;
            aDestBuf[i * PIX_SIZE + 2] = blue;
        }
    }
    return rc;
}

/**
 * Converts a RGB to YUV buffer.
 *
 * @returns IPRT status code.
 * @param   uPixelFormat        Pixel format to use for conversion.
 * @param   paDst               Pointer to destination buffer.
 * @param   uDstWidth           Width (X, in pixels) of destination buffer.
 * @param   uDstHeight          Height (Y, in pixels) of destination buffer.
 * @param   paSrc               Pointer to source buffer.
 * @param   uSrcWidth           Width (X, in pixels) of source buffer.
 * @param   uSrcHeight          Height (Y, in pixels) of source buffer.
 */
int videoRecRGBToYUV(uint32_t uPixelFormat,
                     uint8_t *paDst, uint32_t uDstWidth, uint32_t uDstHeight,
                     uint8_t *paSrc, uint32_t uSrcWidth, uint32_t uSrcHeight)
{
    switch (uPixelFormat)
    {
        case VIDEORECPIXELFMT_RGB32:
            if (!videoRecColorConvWriteYUV420p<ColorConvBGRA32Iter>(paDst, uDstWidth, uDstHeight,
                                                            paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB24:
            if (!videoRecColorConvWriteYUV420p<ColorConvBGR24Iter>(paDst, uDstWidth, uDstHeight,
                                                           paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        case VIDEORECPIXELFMT_RGB565:
            if (!videoRecColorConvWriteYUV420p<ColorConvBGR565Iter>(paDst, uDstWidth, uDstHeight,
                                                            paSrc, uSrcWidth, uSrcHeight))
                return VERR_INVALID_PARAMETER;
            break;
        default:
            AssertFailed();
            return VERR_NOT_SUPPORTED;
    }
    return VINF_SUCCESS;
}

