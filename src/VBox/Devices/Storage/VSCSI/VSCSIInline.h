/* $Id$ */
/** @file
 * Virtual SCSI driver: Inline helpers
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VSCSIInline_h
#define ___VSCSIInline_h

#include <iprt/stdint.h>

DECLINLINE(void) vscsiH2BEU16(uint8_t *pbBuf, uint16_t val)
{
    pbBuf[0] = val >> 8;
    pbBuf[1] = val;
}


DECLINLINE(void) vscsiH2BEU24(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 16;
    pbBuf[1] = val >> 8;
    pbBuf[2] = val;
}


DECLINLINE(void) vscsiH2BEU32(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 24;
    pbBuf[1] = val >> 16;
    pbBuf[2] = val >> 8;
    pbBuf[3] = val;
}

DECLINLINE(void) vscsiH2BEU64(uint8_t *pbBuf, uint64_t val)
{
    pbBuf[0] = val >> 56;
    pbBuf[1] = val >> 48;
    pbBuf[2] = val >> 40;
    pbBuf[3] = val >> 32;
    pbBuf[4] = val >> 24;
    pbBuf[5] = val >> 16;
    pbBuf[6] = val >> 8;
    pbBuf[7] = val;
}

DECLINLINE(uint16_t) vscsiBE2HU16(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 8) | pbBuf[1];
}


DECLINLINE(uint32_t) vscsiBE2HU24(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 16) | (pbBuf[1] << 8) | pbBuf[2];
}


DECLINLINE(uint32_t) vscsiBE2HU32(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 24) | (pbBuf[1] << 16) | (pbBuf[2] << 8) | pbBuf[3];
}

DECLINLINE(uint64_t) vscsiBE2HU64(const uint8_t *pbBuf)
{
    return   ((uint64_t)pbBuf[0] << 56)
           | ((uint64_t)pbBuf[1] << 48)
           | ((uint64_t)pbBuf[2] << 40)
           | ((uint64_t)pbBuf[3] << 32)
           | ((uint64_t)pbBuf[4] << 24)
           | ((uint64_t)pbBuf[5] << 16)
           | ((uint64_t)pbBuf[6] << 8)
           | (uint64_t)pbBuf[7];
}

DECLINLINE(void) vscsiPadStr(int8_t *pbDst, const char *pbSrc, uint32_t cbSize)
{
    for (uint32_t i = 0; i < cbSize; i++)
    {
        if (*pbSrc)
            pbDst[i] = *pbSrc++;
        else
            pbDst[i] = ' ';
    }
}

#endif /* ___VSCSIInline_h */

