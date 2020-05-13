/* $Id$ */
/** @file
 * IPRT - Base64, MIME content transfer encoding, internal header.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The line length used for encoding. */
#define RTBASE64_LINE_LEN   64

/** @name Special g_au8rtBase64CharToVal values
 * @{ */
#define BASE64_SPACE        0xc0
#define BASE64_PAD          0xe0
#define BASE64_NULL         0xfe
#define BASE64_INVALID      0xff
/** @} */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern DECLHIDDEN(const uint8_t)    g_au8rtBase64CharToVal[256];
extern DECLHIDDEN(const char)       g_szrtBase64ValToChar[64+1];
extern DECLHIDDEN(const size_t)     g_acchrtBase64EolStyles[RTBASE64_FLAGS_EOL_STYLE_MASK + 1];
extern DECLHIDDEN(const char)       g_aachrtBase64EolStyles[RTBASE64_FLAGS_EOL_STYLE_MASK + 1][2];


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef RT_STRICT
DECLHIDDEN(void) rtBase64Sanity(void);
#endif


/**
 * Recalcs 6-bit to 8-bit and adjust for padding.
 */
DECLINLINE(ssize_t) rtBase64DecodedSizeRecalc(uint32_t c6Bits, unsigned cbPad)
{
   size_t cb;
   if (c6Bits * 3 / 3 == c6Bits)
   {
       if ((c6Bits * 3 % 4) != 0)
           return -1;
       cb = c6Bits * 3 / 4;
   }
   else
   {
       if ((c6Bits * (uint64_t)3 % 4) != 0)
           return -1;
       cb = c6Bits * (uint64_t)3 / 4;
   }

   if (cb < cbPad)
       return -1;
   cb -= cbPad;
   return cb;
}

