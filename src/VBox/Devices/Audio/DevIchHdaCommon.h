/* $Id$ */
/** @file
 * DevIchHdaCommon.h - Shared defines / functions between controller and codec.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef DEV_HDA_COMMON_H
#define DEV_HDA_COMMON_H

#define HDA_SDFMT_NON_PCM_SHIFT                            15
#define HDA_SDFMT_NON_PCM_MASK                             0x1
#define HDA_SDFMT_BASE_RATE_SHIFT                          14
#define HDA_SDFMT_BASE_RATE_MASK                           0x1
#define HDA_SDFMT_MULT_SHIFT                               11
#define HDA_SDFMT_MULT_MASK                                0x7
#define HDA_SDFMT_DIV_SHIFT                                8
#define HDA_SDFMT_DIV_MASK                                 0x7
#define HDA_SDFMT_BITS_SHIFT                               4
#define HDA_SDFMT_BITS_MASK                                0x7
#define HDA_SDFMT_CHANNELS_MASK                            0xF

#define HDA_SDFMT_TYPE                                     RT_BIT(15)
#define HDA_SDFMT_TYPE_PCM                                 (0)
#define HDA_SDFMT_TYPE_NON_PCM                             (1)

#define HDA_SDFMT_BASE                                     RT_BIT(14)
#define HDA_SDFMT_BASE_48KHZ                               (0)
#define HDA_SDFMT_BASE_44KHZ                               (1)

#define HDA_SDFMT_MULT_1X                                  (0)
#define HDA_SDFMT_MULT_2X                                  (1)
#define HDA_SDFMT_MULT_3X                                  (2)
#define HDA_SDFMT_MULT_4X                                  (3)

#define HDA_SDFMT_DIV_1X                                   (0)
#define HDA_SDFMT_DIV_2X                                   (1)
#define HDA_SDFMT_DIV_3X                                   (2)
#define HDA_SDFMT_DIV_4X                                   (3)
#define HDA_SDFMT_DIV_5X                                   (4)
#define HDA_SDFMT_DIV_6X                                   (5)
#define HDA_SDFMT_DIV_7X                                   (6)
#define HDA_SDFMT_DIV_8X                                   (7)

#define HDA_SDFMT_8_BIT                                    (0)
#define HDA_SDFMT_16_BIT                                   (1)
#define HDA_SDFMT_20_BIT                                   (2)
#define HDA_SDFMT_24_BIT                                   (3)
#define HDA_SDFMT_32_BIT                                   (4)

#define HDA_SDFMT_CHAN_MONO                                (0)
#define HDA_SDFMT_CHAN_STEREO                              (1)

/* Emits a SDnFMT register format. */
/* Also being used in the codec's converter format. */
#define HDA_SDFMT_MAKE(_afNonPCM, _aBaseRate, _aMult, _aDiv, _aBits, _aChan)    \
    (  (((_afNonPCM)  & HDA_SDFMT_NON_PCM_MASK)   << HDA_SDFMT_NON_PCM_SHIFT)   \
     | (((_aBaseRate) & HDA_SDFMT_BASE_RATE_MASK) << HDA_SDFMT_BASE_RATE_SHIFT) \
     | (((_aMult)     & HDA_SDFMT_MULT_MASK)      << HDA_SDFMT_MULT_SHIFT)      \
     | (((_aDiv)      & HDA_SDFMT_DIV_MASK)       << HDA_SDFMT_DIV_SHIFT)       \
     | (((_aBits)     & HDA_SDFMT_BITS_MASK)      << HDA_SDFMT_BITS_SHIFT)      \
     | ( (_aChan)     & HDA_SDFMT_CHANNELS_MASK))

#endif /* DEV_HDA_COMMON_H */

