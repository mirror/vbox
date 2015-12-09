/* $Id$ */
/** @file
 * DevIchHda - VBox ICH Intel HD Audio Controller.
 *
 * Implemented against the specifications found in "High Definition Audio
 * Specification", Revision 1.0a June 17, 2010, and  "Intel I/O Controller
 * HUB 6 (ICH6) Family, Datasheet", document number 301473-002.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/version.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/list.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"

#include "AudioMixBuffer.h"
#include "AudioMixer.h"
#include "DevIchHdaCodec.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
//#define HDA_AS_PCI_EXPRESS
#define VBOX_WITH_INTEL_HDA

#if (defined(DEBUG) && defined(DEBUG_andy))
/* Enables experimental support for separate mic-in handling.
   Do not enable this yet for regular builds, as this needs more testing first! */
# define VBOX_WITH_HDA_MIC_IN
#endif

#if defined(VBOX_WITH_HP_HDA)
/* HP Pavilion dv4t-1300 */
# define HDA_PCI_VENDOR_ID 0x103c
# define HDA_PCI_DEVICE_ID 0x30f7
#elif defined(VBOX_WITH_INTEL_HDA)
/* Intel HDA controller */
# define HDA_PCI_VENDOR_ID 0x8086
# define HDA_PCI_DEVICE_ID 0x2668
#elif defined(VBOX_WITH_NVIDIA_HDA)
/* nVidia HDA controller */
# define HDA_PCI_VENDOR_ID 0x10de
# define HDA_PCI_DEVICE_ID 0x0ac0
#else
# error "Please specify your HDA device vendor/device IDs"
#endif

/** @todo r=bird: Looking at what the linux driver (accidentally?) does when
 * updating CORBWP, I belive that the ICH6 datahsheet is wrong and that CORBRP
 * is read only except for bit 15 like the HDA spec states.
 *
 * Btw. the CORBRPRST implementation is incomplete according to both docs (sw
 * writes 1, hw sets it to 1 (after completion), sw reads 1, sw writes 0). */
#define BIRD_THINKS_CORBRP_IS_MOSTLY_RO

#define HDA_NREGS           114
#define HDA_NREGS_SAVED     112

/**
 *  NB: Register values stored in memory (au32Regs[]) are indexed through
 *  the HDA_RMX_xxx macros (also HDA_MEM_IND_NAME()). On the other hand, the
 *  register descriptors in g_aHdaRegMap[] are indexed through the
 *  HDA_REG_xxx macros (also HDA_REG_IND_NAME()).
 *
 *  The au32Regs[] layout is kept unchanged for saved state
 *  compatibility. */

/* Registers */
#define HDA_REG_IND_NAME(x)                 HDA_REG_##x
#define HDA_MEM_IND_NAME(x)                 HDA_RMX_##x
#define HDA_REG_FIELD_MASK(reg, x)          HDA_##reg##_##x##_MASK
#define HDA_REG_FIELD_FLAG_MASK(reg, x)     RT_BIT(HDA_##reg##_##x##_SHIFT)
#define HDA_REG_FIELD_SHIFT(reg, x)         HDA_##reg##_##x##_SHIFT
#define HDA_REG_IND(pThis, x)               ((pThis)->au32Regs[g_aHdaRegMap[x].mem_idx])
#define HDA_REG(pThis, x)                   (HDA_REG_IND((pThis), HDA_REG_IND_NAME(x)))
#define HDA_REG_FLAG_VALUE(pThis, reg, val) (HDA_REG((pThis),reg) & (((HDA_REG_FIELD_FLAG_MASK(reg, val)))))


#define HDA_REG_GCAP                0 /* range 0x00-0x01*/
#define HDA_RMX_GCAP                0
/* GCAP HDASpec 3.3.2 This macro encodes the following information about HDA in a compact manner:
 * oss (15:12) - number of output streams supported
 * iss (11:8)  - number of input streams supported
 * bss (7:3)   - number of bidirectional streams supported
 * bds (2:1)   - number of serial data out signals supported
 * b64sup (0)  - 64 bit addressing supported.
 */
#define HDA_MAKE_GCAP(oss, iss, bss, bds, b64sup) \
    (  (((oss) & 0xF)  << 12)   \
     | (((iss) & 0xF)  << 8)    \
     | (((bss) & 0x1F) << 3)    \
     | (((bds) & 0x3)  << 2)    \
     | ((b64sup) & 1))

#define HDA_REG_VMIN                1 /* 0x02 */
#define HDA_RMX_VMIN                1

#define HDA_REG_VMAJ                2 /* 0x03 */
#define HDA_RMX_VMAJ                2

#define HDA_REG_OUTPAY              3 /* 0x04-0x05 */
#define HDA_RMX_OUTPAY              3

#define HDA_REG_INPAY               4 /* 0x06-0x07 */
#define HDA_RMX_INPAY               4

#define HDA_REG_GCTL                5 /* 0x08-0x0B */
#define HDA_RMX_GCTL                5
#define HDA_GCTL_RST_SHIFT          0
#define HDA_GCTL_FSH_SHIFT          1
#define HDA_GCTL_UR_SHIFT           8

#define HDA_REG_WAKEEN              6 /* 0x0C */
#define HDA_RMX_WAKEEN              6

#define HDA_REG_STATESTS            7 /* 0x0E */
#define HDA_RMX_STATESTS            7
#define HDA_STATES_SCSF             0x7

#define HDA_REG_GSTS                8 /* 0x10-0x11*/
#define HDA_RMX_GSTS                8
#define HDA_GSTS_FSH_SHIFT          1

#define HDA_REG_OUTSTRMPAY          9  /* 0x18 */
#define HDA_RMX_OUTSTRMPAY          112

#define HDA_REG_INSTRMPAY           10 /* 0x1a */
#define HDA_RMX_INSTRMPAY           113

#define HDA_REG_INTCTL              11 /* 0x20 */
#define HDA_RMX_INTCTL              9
#define HDA_INTCTL_GIE_SHIFT        31
#define HDA_INTCTL_CIE_SHIFT        30
#define HDA_INTCTL_S0_SHIFT         0
#define HDA_INTCTL_S1_SHIFT         1
#define HDA_INTCTL_S2_SHIFT         2
#define HDA_INTCTL_S3_SHIFT         3
#define HDA_INTCTL_S4_SHIFT         4
#define HDA_INTCTL_S5_SHIFT         5
#define HDA_INTCTL_S6_SHIFT         6
#define HDA_INTCTL_S7_SHIFT         7
#define INTCTL_SX(pThis, X)         (HDA_REG_FLAG_VALUE((pThis), INTCTL, S##X))

#define HDA_REG_INTSTS              12 /* 0x24 */
#define HDA_RMX_INTSTS              10
#define HDA_INTSTS_GIS_SHIFT        31
#define HDA_INTSTS_CIS_SHIFT        30
#define HDA_INTSTS_S0_SHIFT         0
#define HDA_INTSTS_S1_SHIFT         1
#define HDA_INTSTS_S2_SHIFT         2
#define HDA_INTSTS_S3_SHIFT         3
#define HDA_INTSTS_S4_SHIFT         4
#define HDA_INTSTS_S5_SHIFT         5
#define HDA_INTSTS_S6_SHIFT         6
#define HDA_INTSTS_S7_SHIFT         7
#define HDA_INTSTS_S_MASK(num)      RT_BIT(HDA_REG_FIELD_SHIFT(S##num))

#define HDA_REG_WALCLK              13 /* 0x24 */
#define HDA_RMX_WALCLK              /* Not defined! */

/* Note: The HDA specification defines a SSYNC register at offset 0x38. The
 * ICH6/ICH9 datahseet defines SSYNC at offset 0x34. The Linux HDA driver matches
 * the datasheet.
 */
#define HDA_REG_SSYNC               14 /* 0x34 */
#define HDA_RMX_SSYNC               12

#define HDA_REG_CORBLBASE           15 /* 0x40 */
#define HDA_RMX_CORBLBASE           13

#define HDA_REG_CORBUBASE           16 /* 0x44 */
#define HDA_RMX_CORBUBASE           14

#define HDA_REG_CORBWP              17 /* 0x48 */
#define HDA_RMX_CORBWP              15

#define HDA_REG_CORBRP              18 /* 0x4A */
#define HDA_RMX_CORBRP              16
#define HDA_CORBRP_RST_SHIFT        15
#define HDA_CORBRP_WP_SHIFT         0
#define HDA_CORBRP_WP_MASK          0xFF

#define HDA_REG_CORBCTL             19 /* 0x4C */
#define HDA_RMX_CORBCTL             17
#define HDA_CORBCTL_DMA_SHIFT       1
#define HDA_CORBCTL_CMEIE_SHIFT     0

#define HDA_REG_CORBSTS             20 /* 0x4D */
#define HDA_RMX_CORBSTS             18
#define HDA_CORBSTS_CMEI_SHIFT      0

#define HDA_REG_CORBSIZE            21 /* 0x4E */
#define HDA_RMX_CORBSIZE            19
#define HDA_CORBSIZE_SZ_CAP         0xF0
#define HDA_CORBSIZE_SZ             0x3
/* till ich 10 sizes of CORB and RIRB are hardcoded to 256 in real hw */

#define HDA_REG_RIRBLBASE           22 /* 0x50 */
#define HDA_RMX_RIRBLBASE           20

#define HDA_REG_RIRBUBASE           23 /* 0x54 */
#define HDA_RMX_RIRBUBASE           21

#define HDA_REG_RIRBWP              24 /* 0x58 */
#define HDA_RMX_RIRBWP              22
#define HDA_RIRBWP_RST_SHIFT        15
#define HDA_RIRBWP_WP_MASK          0xFF

#define HDA_REG_RINTCNT             25 /* 0x5A */
#define HDA_RMX_RINTCNT             23
#define RINTCNT_N(pThis)            (HDA_REG(pThis, RINTCNT) & 0xff)

#define HDA_REG_RIRBCTL             26 /* 0x5C */
#define HDA_RMX_RIRBCTL             24
#define HDA_RIRBCTL_RIC_SHIFT       0
#define HDA_RIRBCTL_DMA_SHIFT       1
#define HDA_ROI_DMA_SHIFT           2

#define HDA_REG_RIRBSTS             27 /* 0x5D */
#define HDA_RMX_RIRBSTS             25
#define HDA_RIRBSTS_RINTFL_SHIFT    0
#define HDA_RIRBSTS_RIRBOIS_SHIFT   2

#define HDA_REG_RIRBSIZE            28 /* 0x5E */
#define HDA_RMX_RIRBSIZE            26
#define HDA_RIRBSIZE_SZ_CAP         0xF0
#define HDA_RIRBSIZE_SZ             0x3

#define RIRBSIZE_SZ(pThis)          (HDA_REG(pThis, HDA_REG_RIRBSIZE) & HDA_RIRBSIZE_SZ)
#define RIRBSIZE_SZ_CAP(pThis)      (HDA_REG(pThis, HDA_REG_RIRBSIZE) & HDA_RIRBSIZE_SZ_CAP)


#define HDA_REG_IC                  29 /* 0x60 */
#define HDA_RMX_IC                  27

#define HDA_REG_IR                  30 /* 0x64 */
#define HDA_RMX_IR                  28

#define HDA_REG_IRS                 31 /* 0x68 */
#define HDA_RMX_IRS                 29
#define HDA_IRS_ICB_SHIFT           0
#define HDA_IRS_IRV_SHIFT           1

#define HDA_REG_DPLBASE             32 /* 0x70 */
#define HDA_RMX_DPLBASE             30
#define DPLBASE(pThis)              (HDA_REG((pThis), DPLBASE))

#define HDA_REG_DPUBASE             33 /* 0x74 */
#define HDA_RMX_DPUBASE             31
#define DPUBASE(pThis)              (HDA_REG((pThis), DPUBASE))

#define DPBASE_ADDR_MASK            (~(uint64_t)0x7f)

#define HDA_STREAM_REG_DEF(name, num)           (HDA_REG_SD##num##name)
#define HDA_STREAM_RMX_DEF(name, num)           (HDA_RMX_SD##num##name)
/* Note: sdnum here _MUST_ be stream reg number [0,7]. */
#define HDA_STREAM_REG(pThis, name, sdnum)      (HDA_REG_IND((pThis), HDA_REG_SD0##name + (sdnum) * 10))

#define HDA_SD_NUM_FROM_REG(pThis, func, reg)   ((reg - HDA_STREAM_REG_DEF(func, 0)) / 10)

#define HDA_REG_SD0CTL              34 /* 0x80 */
#define HDA_REG_SD1CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 10) /* 0xA0 */
#define HDA_REG_SD2CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 20) /* 0xC0 */
#define HDA_REG_SD3CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 30) /* 0xE0 */
#define HDA_REG_SD4CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 40) /* 0x100 */
#define HDA_REG_SD5CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 50) /* 0x120 */
#define HDA_REG_SD6CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 60) /* 0x140 */
#define HDA_REG_SD7CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 70) /* 0x160 */
#define HDA_RMX_SD0CTL              32
#define HDA_RMX_SD1CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 10)
#define HDA_RMX_SD2CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 20)
#define HDA_RMX_SD3CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 30)
#define HDA_RMX_SD4CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 40)
#define HDA_RMX_SD5CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 50)
#define HDA_RMX_SD6CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 60)
#define HDA_RMX_SD7CTL              (HDA_STREAM_RMX_DEF(CTL, 0) + 70)

#define SD(func, num)               SD##num##func

#define HDA_SDCTL(pThis, num)       HDA_REG((pThis), SD(CTL, num))
#define HDA_SDCTL_NUM(pThis, num)   ((HDA_SDCTL((pThis), num) & HDA_REG_FIELD_MASK(SDCTL,NUM)) >> HDA_REG_FIELD_SHIFT(SDCTL, NUM))
#define HDA_SDCTL_NUM_MASK          0xF
#define HDA_SDCTL_NUM_SHIFT         20
#define HDA_SDCTL_DIR_SHIFT         19
#define HDA_SDCTL_TP_SHIFT          18
#define HDA_SDCTL_STRIPE_MASK       0x3
#define HDA_SDCTL_STRIPE_SHIFT      16
#define HDA_SDCTL_DEIE_SHIFT        4
#define HDA_SDCTL_FEIE_SHIFT        3
#define HDA_SDCTL_ICE_SHIFT         2
#define HDA_SDCTL_RUN_SHIFT         1
#define HDA_SDCTL_SRST_SHIFT        0

#define HDA_REG_SD0STS              35 /* 0x83 */
#define HDA_REG_SD1STS              (HDA_STREAM_REG_DEF(STS, 0) + 10) /* 0xA3 */
#define HDA_REG_SD2STS              (HDA_STREAM_REG_DEF(STS, 0) + 20) /* 0xC3 */
#define HDA_REG_SD3STS              (HDA_STREAM_REG_DEF(STS, 0) + 30) /* 0xE3 */
#define HDA_REG_SD4STS              (HDA_STREAM_REG_DEF(STS, 0) + 40) /* 0x103 */
#define HDA_REG_SD5STS              (HDA_STREAM_REG_DEF(STS, 0) + 50) /* 0x123 */
#define HDA_REG_SD6STS              (HDA_STREAM_REG_DEF(STS, 0) + 60) /* 0x143 */
#define HDA_REG_SD7STS              (HDA_STREAM_REG_DEF(STS, 0) + 70) /* 0x163 */
#define HDA_RMX_SD0STS              33
#define HDA_RMX_SD1STS              (HDA_STREAM_RMX_DEF(STS, 0) + 10)
#define HDA_RMX_SD2STS              (HDA_STREAM_RMX_DEF(STS, 0) + 20)
#define HDA_RMX_SD3STS              (HDA_STREAM_RMX_DEF(STS, 0) + 30)
#define HDA_RMX_SD4STS              (HDA_STREAM_RMX_DEF(STS, 0) + 40)
#define HDA_RMX_SD5STS              (HDA_STREAM_RMX_DEF(STS, 0) + 50)
#define HDA_RMX_SD6STS              (HDA_STREAM_RMX_DEF(STS, 0) + 60)
#define HDA_RMX_SD7STS              (HDA_STREAM_RMX_DEF(STS, 0) + 70)

#define SDSTS(pThis, num)           HDA_REG((pThis), SD(STS, num))
#define HDA_SDSTS_FIFORDY_SHIFT     5
#define HDA_SDSTS_DE_SHIFT          4
#define HDA_SDSTS_FE_SHIFT          3
#define HDA_SDSTS_BCIS_SHIFT        2

#define HDA_REG_SD0LPIB             36 /* 0x84 */
#define HDA_REG_SD1LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 10) /* 0xA4 */
#define HDA_REG_SD2LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 20) /* 0xC4 */
#define HDA_REG_SD3LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 30) /* 0xE4 */
#define HDA_REG_SD4LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 40) /* 0x104 */
#define HDA_REG_SD5LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 50) /* 0x124 */
#define HDA_REG_SD6LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 60) /* 0x144 */
#define HDA_REG_SD7LPIB             (HDA_STREAM_REG_DEF(LPIB, 0) + 70) /* 0x164 */
#define HDA_RMX_SD0LPIB             34
#define HDA_RMX_SD1LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 10)
#define HDA_RMX_SD2LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 20)
#define HDA_RMX_SD3LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 30)
#define HDA_RMX_SD4LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 40)
#define HDA_RMX_SD5LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 50)
#define HDA_RMX_SD6LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 60)
#define HDA_RMX_SD7LPIB             (HDA_STREAM_RMX_DEF(LPIB, 0) + 70)

#define HDA_REG_SD0CBL              37 /* 0x88 */
#define HDA_REG_SD1CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 10) /* 0xA8 */
#define HDA_REG_SD2CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 20) /* 0xC8 */
#define HDA_REG_SD3CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 30) /* 0xE8 */
#define HDA_REG_SD4CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 40) /* 0x108 */
#define HDA_REG_SD5CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 50) /* 0x128 */
#define HDA_REG_SD6CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 60) /* 0x148 */
#define HDA_REG_SD7CBL              (HDA_STREAM_REG_DEF(CBL, 0) + 70) /* 0x168 */
#define HDA_RMX_SD0CBL              35
#define HDA_RMX_SD1CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 10)
#define HDA_RMX_SD2CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 20)
#define HDA_RMX_SD3CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 30)
#define HDA_RMX_SD4CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 40)
#define HDA_RMX_SD5CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 50)
#define HDA_RMX_SD6CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 60)
#define HDA_RMX_SD7CBL              (HDA_STREAM_RMX_DEF(CBL, 0) + 70)

#define HDA_REG_SD0LVI              38 /* 0x8C */
#define HDA_REG_SD1LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 10) /* 0xAC */
#define HDA_REG_SD2LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 20) /* 0xCC */
#define HDA_REG_SD3LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 30) /* 0xEC */
#define HDA_REG_SD4LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 40) /* 0x10C */
#define HDA_REG_SD5LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 50) /* 0x12C */
#define HDA_REG_SD6LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 60) /* 0x14C */
#define HDA_REG_SD7LVI              (HDA_STREAM_REG_DEF(LVI, 0) + 70) /* 0x16C */
#define HDA_RMX_SD0LVI              36
#define HDA_RMX_SD1LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 10)
#define HDA_RMX_SD2LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 20)
#define HDA_RMX_SD3LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 30)
#define HDA_RMX_SD4LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 40)
#define HDA_RMX_SD5LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 50)
#define HDA_RMX_SD6LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 60)
#define HDA_RMX_SD7LVI              (HDA_STREAM_RMX_DEF(LVI, 0) + 70)

#define HDA_REG_SD0FIFOW            39 /* 0x8E */
#define HDA_REG_SD1FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 10) /* 0xAE */
#define HDA_REG_SD2FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 20) /* 0xCE */
#define HDA_REG_SD3FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 30) /* 0xEE */
#define HDA_REG_SD4FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 40) /* 0x10E */
#define HDA_REG_SD5FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 50) /* 0x12E */
#define HDA_REG_SD6FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 60) /* 0x14E */
#define HDA_REG_SD7FIFOW            (HDA_STREAM_REG_DEF(FIFOW, 0) + 70) /* 0x16E */
#define HDA_RMX_SD0FIFOW            37
#define HDA_RMX_SD1FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 10)
#define HDA_RMX_SD2FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 20)
#define HDA_RMX_SD3FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 30)
#define HDA_RMX_SD4FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 40)
#define HDA_RMX_SD5FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 50)
#define HDA_RMX_SD6FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 60)
#define HDA_RMX_SD7FIFOW            (HDA_STREAM_RMX_DEF(FIFOW, 0) + 70)

/*
 * ICH6 datasheet defined limits for FIFOW values (18.2.38).
 */
#define HDA_SDFIFOW_8B              0x2
#define HDA_SDFIFOW_16B             0x3
#define HDA_SDFIFOW_32B             0x4

#define HDA_REG_SD0FIFOS            40 /* 0x90 */
#define HDA_REG_SD1FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 10) /* 0xB0 */
#define HDA_REG_SD2FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 20) /* 0xD0 */
#define HDA_REG_SD3FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 30) /* 0xF0 */
#define HDA_REG_SD4FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 40) /* 0x110 */
#define HDA_REG_SD5FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 50) /* 0x130 */
#define HDA_REG_SD6FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 60) /* 0x150 */
#define HDA_REG_SD7FIFOS            (HDA_STREAM_REG_DEF(FIFOS, 0) + 70) /* 0x170 */
#define HDA_RMX_SD0FIFOS            38
#define HDA_RMX_SD1FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 10)
#define HDA_RMX_SD2FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 20)
#define HDA_RMX_SD3FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 30)
#define HDA_RMX_SD4FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 40)
#define HDA_RMX_SD5FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 50)
#define HDA_RMX_SD6FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 60)
#define HDA_RMX_SD7FIFOS            (HDA_STREAM_RMX_DEF(FIFOS, 0) + 70)

/*
 * ICH6 datasheet defines limits for FIFOS registers (18.2.39)
 * formula: size - 1
 * Other values not listed are not supported.
 */
#define HDA_SDINFIFO_120B           0x77 /* 8-, 16-, 20-, 24-, 32-bit Input Streams */
#define HDA_SDINFIFO_160B           0x9F /* 20-, 24-bit Input Streams Streams */

#define HDA_SDONFIFO_16B            0x0F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_32B            0x1F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_64B            0x3F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_128B           0x7F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_192B           0xBF /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_256B           0xFF /* 20-, 24-bit Output Streams */
#define SDFIFOS(pThis, num)         HDA_REG((pThis), SD(FIFOS, num))

#define HDA_REG_SD0FMT              41 /* 0x92 */
#define HDA_REG_SD1FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 10) /* 0xB2 */
#define HDA_REG_SD2FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 20) /* 0xD2 */
#define HDA_REG_SD3FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 30) /* 0xF2 */
#define HDA_REG_SD4FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 40) /* 0x112 */
#define HDA_REG_SD5FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 50) /* 0x132 */
#define HDA_REG_SD6FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 60) /* 0x152 */
#define HDA_REG_SD7FMT              (HDA_STREAM_REG_DEF(FMT, 0) + 70) /* 0x172 */
#define HDA_RMX_SD0FMT              39
#define HDA_RMX_SD1FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 10)
#define HDA_RMX_SD2FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 20)
#define HDA_RMX_SD3FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 30)
#define HDA_RMX_SD4FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 40)
#define HDA_RMX_SD5FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 50)
#define HDA_RMX_SD6FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 60)
#define HDA_RMX_SD7FMT              (HDA_STREAM_RMX_DEF(FMT, 0) + 70)

#define SDFMT(pThis, num)           (HDA_REG((pThis), SD(FMT, num)))
#define HDA_SDFMT_BASE_RATE_SHIFT   14
#define HDA_SDFMT_MULT_SHIFT        11
#define HDA_SDFMT_MULT_MASK         0x7
#define HDA_SDFMT_DIV_SHIFT         8
#define HDA_SDFMT_DIV_MASK          0x7
#define HDA_SDFMT_BITS_SHIFT        4
#define HDA_SDFMT_BITS_MASK         0x7
#define SDFMT_BASE_RATE(pThis, num) ((SDFMT(pThis, num) & HDA_REG_FIELD_FLAG_MASK(SDFMT, BASE_RATE)) >> HDA_REG_FIELD_SHIFT(SDFMT, BASE_RATE))
#define SDFMT_MULT(pThis, num)      ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,MULT)) >> HDA_REG_FIELD_SHIFT(SDFMT, MULT))
#define SDFMT_DIV(pThis, num)       ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,DIV)) >> HDA_REG_FIELD_SHIFT(SDFMT, DIV))

#define HDA_REG_SD0BDPL             42 /* 0x98 */
#define HDA_REG_SD1BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 10) /* 0xB8 */
#define HDA_REG_SD2BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 20) /* 0xD8 */
#define HDA_REG_SD3BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 30) /* 0xF8 */
#define HDA_REG_SD4BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 40) /* 0x118 */
#define HDA_REG_SD5BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 50) /* 0x138 */
#define HDA_REG_SD6BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 60) /* 0x158 */
#define HDA_REG_SD7BDPL             (HDA_STREAM_REG_DEF(BDPL, 0) + 70) /* 0x178 */
#define HDA_RMX_SD0BDPL             40
#define HDA_RMX_SD1BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 10)
#define HDA_RMX_SD2BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 20)
#define HDA_RMX_SD3BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 30)
#define HDA_RMX_SD4BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 40)
#define HDA_RMX_SD5BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 50)
#define HDA_RMX_SD6BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 60)
#define HDA_RMX_SD7BDPL             (HDA_STREAM_RMX_DEF(BDPL, 0) + 70)

#define HDA_REG_SD0BDPU             43 /* 0x9C */
#define HDA_REG_SD1BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 10) /* 0xBC */
#define HDA_REG_SD2BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 20) /* 0xDC */
#define HDA_REG_SD3BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 30) /* 0xFC */
#define HDA_REG_SD4BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 40) /* 0x11C */
#define HDA_REG_SD5BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 50) /* 0x13C */
#define HDA_REG_SD6BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 60) /* 0x15C */
#define HDA_REG_SD7BDPU             (HDA_STREAM_REG_DEF(BDPU, 0) + 70) /* 0x17C */
#define HDA_RMX_SD0BDPU             41
#define HDA_RMX_SD1BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 10)
#define HDA_RMX_SD2BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 20)
#define HDA_RMX_SD3BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 30)
#define HDA_RMX_SD4BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 40)
#define HDA_RMX_SD5BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 50)
#define HDA_RMX_SD6BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 60)
#define HDA_RMX_SD7BDPU             (HDA_STREAM_RMX_DEF(BDPU, 0) + 70)

#define HDA_CODEC_CAD_SHIFT         28
/* Encodes the (required) LUN into a codec command. */
#define HDA_CODEC_CMD(cmd, lun)     ((cmd) | (lun << HDA_CODEC_CAD_SHIFT))



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal state of a Buffer Descriptor List Entry (BDLE),
 * needed to keep track of the data needed for the actual device
 * emulation.
 */
typedef struct HDABDLESTATE
{
    /** Own index within the BDL (Buffer Descriptor List). */
    uint32_t     u32BDLIndex;
    /** Number of bytes below the stream's FIFO watermark (SDFIFOW).
     *  Used to check if we need fill up the FIFO again. */
    uint32_t     cbBelowFIFOW;
    /** The buffer descriptor's internal DMA buffer. */
    uint8_t      au8FIFO[HDA_SDONFIFO_256B + 1];
    /** Current offset in DMA buffer (in bytes).*/
    uint32_t     u32BufOff;
    uint32_t     Padding;
} HDABDLESTATE, *PHDABDLESTATE;

/**
 * Buffer Descriptor List Entry (BDLE) (3.6.3).
 *
 * Contains only register values which do *not* change until a
 * stream reset occurs.
 */
typedef struct HDABDLE
{
    /** Starting address of the actual buffer. Must be 128-bit aligned. */
    uint64_t     u64BufAdr;
    /** Size of the actual buffer (in bytes). */
    uint32_t     u32BufSize;
    /** Interrupt on completion; the controller will generate
     *  an interrupt when the last byte of the buffer has been
     *  fetched by the DMA engine. */
    bool         fIntOnCompletion;
    /** Internal state of this BDLE.
     *  Not part of the actual BDLE registers. */
    HDABDLESTATE State;
} HDABDLE, *PHDABDLE;

/**
 * Internal state of a HDA stream.
 */
typedef struct HDASTREAMSTATE
{
    /** Number of BDLEs (Buffer Descriptor List Entry).
     *  Should be SDnLVI + 1 usually. */
    uint16_t            cBDLE;
    /** Current BDLE to use. Wraps around to 0 if
     *  maximum (cBDLE) is reached. */
    uint16_t            uCurBDLE;
    uint32_t            Padding;
    /** Array of BDLEs. */
    R3PTRTYPE(PHDABDLE) paBDLE;
} HDASTREAMSTATE, *PHDASTREAMSTATE;

/**
 * Structure for keeping a HDA stream state.
 *
 * Contains only register values which do *not* change until a
 * stream reset occurs.
 */
typedef struct HDASTREAM
{
    /** Stream number (SDn). */
    uint8_t        u8Strm;
    uint8_t        Padding0[7];
    /** DMA base address (SDnBDPU - SDnBDPL). */
    uint64_t       u64BaseDMA;
    /** Cyclic Buffer Length (SDnCBL).
     *  Represents the size of the ring buffer. */
    uint32_t       u32CBL;
    /** Format (SDnFMT). */
    uint16_t       u16FMT;
    /** FIFO Size (FIFOS).
     *  Maximum number of bytes that may have been DMA'd into
     *  memory but not yet transmitted on the link.
     *
     *  Must be a power of two. */
    uint16_t       u16FIFOS;
    /** Last Valid Index (SDnLVI). */
    uint16_t       u16LVI;
    uint16_t       Padding1[3];
    /** Internal state of this stream. */
    HDASTREAMSTATE State;
} HDASTREAM, *PHDASTREAM;

typedef struct HDAINPUTSTREAM
{
    /** PCM line input stream. */
    R3PTRTYPE(PPDMAUDIOGSTSTRMIN)      pStrmIn;
    /** Mixer handle for line input stream. */
    R3PTRTYPE(PAUDMIXSTREAM)           phStrmIn;
} HDAINPUTSTREAM, *PHDAINPUTSTREAM;

typedef struct HDAOUTPUTSTREAM
{
    /** PCM output stream. */
    R3PTRTYPE(PPDMAUDIOGSTSTRMOUT)     pStrmOut;
    /** Mixer handle for line output stream. */
    R3PTRTYPE(PAUDMIXSTREAM)           phStrmOut;
} HDAOUTPUTSTREAM, *PHDAOUTPUTSTREAM;

/**
 * Struct for maintaining a host backend driver.
 * This driver must be associated to one, and only one,
 * HDA codec. The HDA controller does the actual multiplexing
 * of HDA codec data to various host backend drivers then.
 *
 * This HDA device uses a timer in order to synchronize all
 * read/write accesses across all attached LUNs / backends.
 */
typedef struct HDADRIVER
{
    union
    {
        /** Node for storing this driver in our device driver
         *  list of HDASTATE. */
        RTLISTNODE                     Node;
        struct
        {
            R3PTRTYPE(void *)          dummy1;
            R3PTRTYPE(void *)          dummy2;
        } dummy;
    };

    /** Pointer to HDA controller (state). */
    R3PTRTYPE(PHDASTATE)               pHDAState;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                   Flags;
    uint8_t                            u32Padding0[3];
    /** LUN to which this driver has been assigned. */
    uint8_t                            uLUN;
    /** Audio connector interface to the underlying
     *  host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pConnector;
    /** Stream for line input. */
    HDAINPUTSTREAM                     LineIn;
    /** Stream for mic input. */
    HDAINPUTSTREAM                     MicIn;
    /** Stream for output. */
    HDAOUTPUTSTREAM                    Out;
} HDADRIVER;

/**
 * ICH Intel HD Audio Controller state.
 */
typedef struct HDASTATE
{
    /** The PCI device structure. */
    PCIDevice                          PciDev;
    /** R3 Pointer to the device instance. */
    PPDMDEVINSR3                       pDevInsR3;
    /** R0 Pointer to the device instance. */
    PPDMDEVINSR0                       pDevInsR0;
    /** R0 Pointer to the device instance. */
    PPDMDEVINSRC                       pDevInsRC;
    /** Padding for alignment. */
    uint32_t                           u32Padding;
    /** Pointer to the attached audio driver. */
    R3PTRTYPE(PPDMIBASE)               pDrvBase;
    /** The base interface for LUN\#0. */
    PDMIBASE                           IBase;
    RTGCPHYS                           MMIOBaseAddr;
    /** The HDA's register set. */
    uint32_t                           au32Regs[HDA_NREGS];
    /** Stream state for line-in. */
    HDASTREAM                          StrmStLineIn;
    /** Stream state for microphone-in. */
    HDASTREAM                          StrmStMicIn;
    /** Stream state for output. */
    HDASTREAM                          StrmStOut;
    /** CORB buffer base address. */
    uint64_t                           u64CORBBase;
    /** RIRB buffer base address. */
    uint64_t                           u64RIRBBase;
    /** DMA base address.
     *  Made out of DPLBASE + DPUBASE (3.3.32 + 3.3.33). */
    uint64_t                           u64DPBase;
    /** DMA position buffer enable bit. */
    bool                               fDMAPosition;
    /** Padding for alignment. */
    uint8_t                            u32Padding0[7];
    /** Pointer to CORB buffer. */
    R3PTRTYPE(uint32_t *)              pu32CorbBuf;
    /** Size in bytes of CORB buffer. */
    uint32_t                           cbCorbBuf;
    /** Padding for alignment. */
    uint32_t                           u32Padding1;
    /** Pointer to RIRB buffer. */
    R3PTRTYPE(uint64_t *)              pu64RirbBuf;
    /** Size in bytes of RIRB buffer. */
    uint32_t                           cbRirbBuf;
    /** Indicates if HDA is in reset. */
    bool                               fInReset;
    /** Flag whether the R0 part is enabled. */
    bool                               fR0Enabled;
    /** Flag whether the RC part is enabled. */
    bool                               fRCEnabled;
#ifndef VBOX_WITH_AUDIO_CALLBACKS
    /** The emulation timer for handling the attached
     *  LUN drivers. */
    PTMTIMERR3                         pTimer;
    /** Timer ticks for handling the LUN drivers. */
    uint64_t                           uTimerTicks;
    /** Timestamp (delta) since last timer call. */
    uint64_t                           uTimerTS;
#endif
#ifdef VBOX_WITH_STATISTICS
# ifndef VBOX_WITH_AUDIO_CALLBACKS
    STAMPROFILE                        StatTimer;
# endif
    STAMCOUNTER                        StatBytesRead;
    STAMCOUNTER                        StatBytesWritten;
#endif
    /** Pointer to HDA codec to use. */
    R3PTRTYPE(PHDACODEC)               pCodec;
    union
    {
        /** List of associated LUN drivers. */
        RTLISTANCHOR                   lstDrv;
        /** Padding for alignment. */
        struct
        {
            R3PTRTYPE(void *)          dummy1;
            R3PTRTYPE(void *)          dummy2;
        } dummy;
    };
    /** The device' software mixer. */
    R3PTRTYPE(PAUDIOMIXER)             pMixer;
    /** Audio sink for PCM output. */
    R3PTRTYPE(PAUDMIXSINK)             pSinkOutput;
    /** Audio mixer sink for line input. */
    R3PTRTYPE(PAUDMIXSINK)             pSinkLineIn;
    /** Audio mixer sink for microphone input. */
    R3PTRTYPE(PAUDMIXSINK)             pSinkMicIn;
    uint64_t                           u64BaseTS;
    /** Response Interrupt Count (RINTCNT). */
    uint8_t                            u8RespIntCnt;
    /** Padding for alignment. */
    uint8_t                            au8Padding2[7];
} HDASTATE;
/** Pointer to the ICH Intel HD Audio Controller state. */
typedef HDASTATE *PHDASTATE;

#ifdef VBOX_WITH_AUDIO_CALLBACKS
typedef struct HDACALLBACKCTX
{
    PHDASTATE  pThis;
    PHDADRIVER pDriver;
} HDACALLBACKCTX, *PHDACALLBACKCTX;
#endif

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE
static FNPDMDEVRESET hdaReset;

/*
 * Stubs.
 */
static int hdaRegReadUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);

/*
 * Global register set read/write functions.
 */
static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegReadLPIB(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteCORBWP(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteRIRBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadIRS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteBase(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);

/*
 * {IOB}SDn read/write functions.
 */
static int hdaRegWriteSDCBL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDLVI(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFIFOW(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDBDPL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDBDPU(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);

/*
 * Generic register read/write functions.
 */
static int hdaRegReadU32(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU24(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU24(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU16(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU16(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU8(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU8(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);

static void hdaStreamDestroy(PHDASTREAM pStrmSt);

static int hdaTransfer(PHDASTATE pThis, ENMSOUNDSOURCE enmSrc, uint32_t cbMax, uint32_t *pcbProcessed);

#ifdef IN_RING3
static int hdaBDLEFetch(PHDASTATE pThis, PHDABDLE pBDLE, uint64_t u64BaseDMA, uint16_t u16Entry);
static void hdaStreamUpdateLPIB(PHDASTATE pThis, PHDASTREAM pStrmSt, uint32_t u32LPIB);
# ifdef LOG_ENABLED
static void hdaBDLEDumpAll(PHDASTATE pThis, uint64_t u64BaseDMA, uint16_t u16LVI);
# endif
static void hdaBDLEReset(PHDABDLE pBDLE);
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/** Offset of the SD0 register map. */
#define HDA_REG_DESC_SD0_BASE 0x80

/** Turn a short global register name into an memory index and a stringized name. */
#define HDA_REG_IDX(abbrev)         HDA_MEM_IND_NAME(abbrev), #abbrev

/** Turns a short stream register name into an memory index and a stringized name. */
#define HDA_REG_IDX_STRM(reg, suff) HDA_MEM_IND_NAME(reg ## suff), #reg #suff

/** Same as above for a register *not* stored in memory. */
#define HDA_REG_IDX_LOCAL(abbrev)   0, #abbrev

/** Emits a single audio stream register set (e.g. OSD0) at a specified offset. */
#define HDA_REG_MAP_STRM(offset, name) \
    /* offset        size     read mask   write mask  read callback   write callback     index + abbrev                  description */ \
    /* -------       -------  ----------  ----------  --------------  -----------------  ------------------------------  ----------- */ \
    /* Offset 0x80 (SD0) */ \
    { offset,        0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24 , hdaRegWriteSDCTL , HDA_REG_IDX_STRM(name, CTL)  , #name " Stream Descriptor Control" }, \
    /* Offset 0x83 (SD0) */ \
    { offset + 0x3,  0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8  , hdaRegWriteSDSTS , HDA_REG_IDX_STRM(name, STS)  , #name " Status" }, \
    /* Offset 0x84 (SD0) */ \
    { offset + 0x4,  0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadLPIB, hdaRegWriteU32   , HDA_REG_IDX_STRM(name, LPIB) , #name " Link Position In Buffer" }, \
    /* Offset 0x88 (SD0) */ \
    { offset + 0x8,  0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32, hdaRegWriteU32    , HDA_REG_IDX_STRM(name, CBL)  , #name " Cyclic Buffer Length" }, \
    /* Offset 0x8C (SD0) */ \
    { offset + 0xC,  0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16, hdaRegWriteSDLVI  , HDA_REG_IDX_STRM(name, LVI)  , #name " Last Valid Index" }, \
    /* Reserved: FIFO Watermark. ** @todo Document this! */ \
    { offset + 0xE,  0x00002, 0x00000007, 0x00000007, hdaRegReadU16, hdaRegWriteSDFIFOW, HDA_REG_IDX_STRM(name, FIFOW), #name " FIFO Watermark" }, \
    /* Offset 0x90 (SD0) */ \
    { offset + 0x10, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16, hdaRegWriteSDFIFOS, HDA_REG_IDX_STRM(name, FIFOS), #name " FIFO Size" }, \
    /* Offset 0x92 (SD0) */ \
    { offset + 0x12, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16, hdaRegWriteSDFMT  , HDA_REG_IDX_STRM(name, FMT)  , #name " Format" }, \
    /* Reserved: 0x94 - 0x98. */ \
    /* Offset 0x98 (SD0) */ \
    { offset + 0x18, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32, hdaRegWriteSDBDPL , HDA_REG_IDX_STRM(name, BDPL) , #name " Buffer Descriptor List Pointer-Lower Base Address" }, \
    /* Offset 0x9C (SD0) */ \
    { offset + 0x1C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32, hdaRegWriteSDBDPU , HDA_REG_IDX_STRM(name, BDPU) , #name " Buffer Descriptor List Pointer-Upper Base Address" }

/** Defines a single audio stream register set (e.g. OSD0). */
#define HDA_REG_MAP_DEF_STREAM(index, name) \
    HDA_REG_MAP_STRM(HDA_REG_DESC_SD0_BASE + (index * 32 /* 0x20 */), name)

/* See 302349 p 6.2. */
static const struct HDAREGDESC
{
    /** Register offset in the register space. */
    uint32_t    offset;
    /** Size in bytes. Registers of size > 4 are in fact tables. */
    uint32_t    size;
    /** Readable bits. */
    uint32_t    readable;
    /** Writable bits. */
    uint32_t    writable;
    /** Read callback. */
    int       (*pfnRead)(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
    /** Write callback. */
    int       (*pfnWrite)(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
    /** Index into the register storage array. */
    uint32_t    mem_idx;
    /** Abbreviated name. */
    const char *abbrev;
    /** Descripton. */
    const char *desc;
} g_aHdaRegMap[HDA_NREGS] =

{
    /* offset  size     read mask   write mask  read callback            write callback         index + abbrev   */
    /*-------  -------  ----------  ----------  -----------------------  ---------------------- ---------------- */
    { 0x00000, 0x00002, 0x0000FFFB, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , HDA_REG_IDX(GCAP)         }, /* Global Capabilities */
    { 0x00002, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , HDA_REG_IDX(VMIN)         }, /* Minor Version */
    { 0x00003, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , HDA_REG_IDX(VMAJ)         }, /* Major Version */
    { 0x00004, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , HDA_REG_IDX(OUTPAY)       }, /* Output Payload Capabilities */
    { 0x00006, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , HDA_REG_IDX(INPAY)        }, /* Input Payload Capabilities */
    { 0x00008, 0x00004, 0x00000103, 0x00000103, hdaRegReadU32          , hdaRegWriteGCTL       , HDA_REG_IDX(GCTL)         }, /* Global Control */
    { 0x0000c, 0x00002, 0x00007FFF, 0x00007FFF, hdaRegReadU16          , hdaRegWriteU16        , HDA_REG_IDX(WAKEEN)       }, /* Wake Enable */
    { 0x0000e, 0x00002, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteSTATESTS   , HDA_REG_IDX(STATESTS)     }, /* State Change Status */
    { 0x00010, 0x00002, 0xFFFFFFFF, 0x00000000, hdaRegReadUnimpl       , hdaRegWriteUnimpl     , HDA_REG_IDX(GSTS)         }, /* Global Status */
    { 0x00018, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , HDA_REG_IDX(OUTSTRMPAY)   }, /* Output Stream Payload Capability */
    { 0x0001A, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , HDA_REG_IDX(INSTRMPAY)    }, /* Input Stream Payload Capability */
    { 0x00020, 0x00004, 0xC00000FF, 0xC00000FF, hdaRegReadU32          , hdaRegWriteU32        , HDA_REG_IDX(INTCTL)       }, /* Interrupt Control */
    { 0x00024, 0x00004, 0xC00000FF, 0x00000000, hdaRegReadINTSTS       , hdaRegWriteUnimpl     , HDA_REG_IDX(INTSTS)       }, /* Interrupt Status */
    { 0x00030, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadWALCLK       , hdaRegWriteUnimpl     , HDA_REG_IDX_LOCAL(WALCLK) }, /* Wall Clock Counter */
    { 0x00034, 0x00004, 0x000000FF, 0x000000FF, hdaRegReadU32          , hdaRegWriteU32        , HDA_REG_IDX(SSYNC)        }, /* Stream Synchronization */
    { 0x00040, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(CORBLBASE)    }, /* CORB Lower Base Address */
    { 0x00044, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(CORBUBASE)    }, /* CORB Upper Base Address */
    { 0x00048, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteCORBWP     , HDA_REG_IDX(CORBWP)       }, /* CORB Write Pointer */
    { 0x0004A, 0x00002, 0x000080FF, 0x000080FF, hdaRegReadU16          , hdaRegWriteCORBRP     , HDA_REG_IDX(CORBRP)       }, /* CORB Read Pointer */
    { 0x0004C, 0x00001, 0x00000003, 0x00000003, hdaRegReadU8           , hdaRegWriteCORBCTL    , HDA_REG_IDX(CORBCTL)      }, /* CORB Control */
    { 0x0004D, 0x00001, 0x00000001, 0x00000001, hdaRegReadU8           , hdaRegWriteCORBSTS    , HDA_REG_IDX(CORBSTS)      }, /* CORB Status */
    { 0x0004E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , HDA_REG_IDX(CORBSIZE)     }, /* CORB Size */
    { 0x00050, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(RIRBLBASE)    }, /* RIRB Lower Base Address */
    { 0x00054, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(RIRBUBASE)    }, /* RIRB Upper Base Address */
    { 0x00058, 0x00002, 0x000000FF, 0x00008000, hdaRegReadU8           , hdaRegWriteRIRBWP     , HDA_REG_IDX(RIRBWP)       }, /* RIRB Write Pointer */
    { 0x0005A, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteU16        , HDA_REG_IDX(RINTCNT)      }, /* Response Interrupt Count */
    { 0x0005C, 0x00001, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteU8         , HDA_REG_IDX(RIRBCTL)      }, /* RIRB Control */
    { 0x0005D, 0x00001, 0x00000005, 0x00000005, hdaRegReadU8           , hdaRegWriteRIRBSTS    , HDA_REG_IDX(RIRBSTS)      }, /* RIRB Status */
    { 0x0005E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , HDA_REG_IDX(RIRBSIZE)     }, /* RIRB Size */
    { 0x00060, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , HDA_REG_IDX(IC)           }, /* Immediate Command */
    { 0x00064, 0x00004, 0x00000000, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteUnimpl     , HDA_REG_IDX(IR)           }, /* Immediate Response */
    { 0x00068, 0x00002, 0x00000002, 0x00000002, hdaRegReadIRS          , hdaRegWriteIRS        , HDA_REG_IDX(IRS)          }, /* Immediate Command Status */
    { 0x00070, 0x00004, 0xFFFFFFFF, 0xFFFFFF81, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(DPLBASE)      }, /* DMA Position Lower Base */
    { 0x00074, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , HDA_REG_IDX(DPUBASE)      }, /* DMA Position Upper Base */
    /* 4 Input Stream Descriptors (ISD). */
    HDA_REG_MAP_DEF_STREAM(0, SD0),
    HDA_REG_MAP_DEF_STREAM(1, SD1),
    HDA_REG_MAP_DEF_STREAM(2, SD2),
    HDA_REG_MAP_DEF_STREAM(3, SD3),
    /* 4 Output Stream Descriptors (OSD). */
    HDA_REG_MAP_DEF_STREAM(4, SD4),
    HDA_REG_MAP_DEF_STREAM(5, SD5),
    HDA_REG_MAP_DEF_STREAM(6, SD6),
    HDA_REG_MAP_DEF_STREAM(7, SD7)
};

/**
 * HDA register aliases (HDA spec 3.3.45).
 * @remarks Sorted by offReg.
 */
static const struct
{
    /** The alias register offset. */
    uint32_t    offReg;
    /** The register index. */
    int         idxAlias;
} g_aHdaRegAliases[] =
{
    { 0x2084, HDA_REG_SD0LPIB },
    { 0x20a4, HDA_REG_SD1LPIB },
    { 0x20c4, HDA_REG_SD2LPIB },
    { 0x20e4, HDA_REG_SD3LPIB },
    { 0x2104, HDA_REG_SD4LPIB },
    { 0x2124, HDA_REG_SD5LPIB },
    { 0x2144, HDA_REG_SD6LPIB },
    { 0x2164, HDA_REG_SD7LPIB },
};

#ifdef IN_RING3
/** HDABDLESTATE field descriptors for the v5+ saved state. */
static SSMFIELD const g_aSSMBDLEStateFields5[] =
{
    SSMFIELD_ENTRY(HDABDLESTATE, u32BDLIndex),
    SSMFIELD_ENTRY(HDABDLESTATE, cbBelowFIFOW),
    SSMFIELD_ENTRY(HDABDLESTATE, au8FIFO),
    SSMFIELD_ENTRY(HDABDLESTATE, u32BufOff),
    SSMFIELD_ENTRY_TERM()
};

/** HDASTREAMSTATE field descriptors for the v5+ saved state. */
static SSMFIELD const g_aSSMStreamStateFields5[] =
{
    SSMFIELD_ENTRY       (HDASTREAMSTATE, cBDLE),
    SSMFIELD_ENTRY       (HDASTREAMSTATE, uCurBDLE),
    SSMFIELD_ENTRY_IGNORE(HDASTREAMSTATE, paBDLE),
    SSMFIELD_ENTRY_TERM()
};
#endif

/**
 * 32-bit size indexed masks, i.e. g_afMasks[2 bytes] = 0xffff.
 */
static uint32_t const g_afMasks[5] =
{
    UINT32_C(0), UINT32_C(0x000000ff), UINT32_C(0x0000ffff), UINT32_C(0x00ffffff), UINT32_C(0xffffffff)
};

#ifdef IN_RING3
DECLINLINE(void) hdaStreamUpdateLPIB(PHDASTATE pThis, PHDASTREAM pStrmSt, uint32_t u32LPIB)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStrmSt);

    Assert(u32LPIB <= pStrmSt->u32CBL);

    LogFlowFunc(("uStrm=%RU8, LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",
                 pStrmSt->u8Strm, u32LPIB, pThis->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm) = u32LPIB;

    /* Do we need to tell the current DMA position? */
    if (pThis->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns),
                                        (pThis->u64DPBase & DPBASE_ADDR_MASK) + (pStrmSt->u8Strm * 8),
                                        (void *)&u32LPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }
}
#endif

/**
 * Retrieves the number of bytes of a FIFOS register.
 *
 * @return Number of bytes of a given FIFOS register.
 */
DECLINLINE(uint16_t) hdaSDFIFOSToBytes(uint32_t u32RegFIFOS)
{
    uint16_t cb;
    switch (u32RegFIFOS)
    {
        /* Input */
        case HDA_SDINFIFO_120B: cb = 120; break;
        case HDA_SDINFIFO_160B: cb = 160; break;

        /* Output */
        case HDA_SDONFIFO_16B:  cb = 16;  break;
        case HDA_SDONFIFO_32B:  cb = 32;  break;
        case HDA_SDONFIFO_64B:  cb = 64;  break;
        case HDA_SDONFIFO_128B: cb = 128; break;
        case HDA_SDONFIFO_192B: cb = 192; break;
        case HDA_SDONFIFO_256B: cb = 256; break;
        default:
        {
            cb = 0;
            AssertMsgFailed(("Wrong FIFO value\n"));
            break;
        }
    }

    return cb;
}

/**
 * Retrieves the number of bytes of a FIFOW register.
 *
 * @return Number of bytes of a given FIFOW register.
 */
DECLINLINE(uint8_t) hdaSDFIFOWToBytes(uint32_t u32RegFIFOW)
{
    uint32_t cb;
    switch (u32RegFIFOW)
    {
        case HDA_SDFIFOW_8B:  cb = 8;  break;
        case HDA_SDFIFOW_16B: cb = 16; break;
        case HDA_SDFIFOW_32B: cb = 32; break;
        default:              cb = 0;  break;
    }

#ifdef RT_STRICT
    Assert(RT_IS_POWER_OF_TWO(cb));
#endif
    return cb;
}

#ifdef IN_RING3
/**
 * Returns the current BDLE to use for a stream.
 *
 * @return BDLE to use, NULL if none found.
 */
DECLINLINE(PHDABDLE) hdaStreamGetCurrentBDLE(PHDASTATE pThis, PHDASTREAM pStrmSt)
{
    AssertPtrReturn(pThis,   NULL);
    AssertPtrReturn(pStrmSt, NULL);

    Assert(pStrmSt->State.paBDLE);
    Assert(pStrmSt->State.uCurBDLE < pStrmSt->State.cBDLE);

    PHDABDLE pBDLE = &pStrmSt->State.paBDLE[pStrmSt->State.uCurBDLE];
    return pBDLE;
}

/**
 * Returns the next BDLE to use for a stream.
 *
 * @return BDLE to use next, NULL if none found.
 */
DECLINLINE(PHDABDLE) hdaStreamGetNextBDLE(PHDASTATE pThis, PHDASTREAM pStrmSt)
{
    AssertPtrReturn(pThis,   NULL);
    AssertPtrReturn(pStrmSt, NULL);

    NOREF(pThis);

    Assert(pStrmSt->State.paBDLE);
    Assert(pStrmSt->State.uCurBDLE < pStrmSt->State.cBDLE);

#ifdef DEBUG
    uint32_t uOldBDLE = pStrmSt->State.uCurBDLE;
#endif

    /*
     * Switch to the next BDLE entry and do a wrap around
     * if we reached the end of the Buffer Descriptor List (BDL).
     */
    pStrmSt->State.uCurBDLE++;
    if (pStrmSt->State.uCurBDLE == pStrmSt->State.cBDLE)
    {
        pStrmSt->State.uCurBDLE = 0;

        hdaStreamUpdateLPIB(pThis, pStrmSt, 0);
    }

    Assert(pStrmSt->State.uCurBDLE < pStrmSt->State.cBDLE);

    PHDABDLE pBDLE = &pStrmSt->State.paBDLE[pStrmSt->State.uCurBDLE];
    AssertPtr(pBDLE);

    hdaBDLEReset(pBDLE);

#ifdef DEBUG
    LogFlowFunc(("uOldBDLE=%RU16, uCurBDLE=%RU16, cBDLE=%RU32, %R[bdle]\n",
                 uOldBDLE, pStrmSt->State.uCurBDLE, pStrmSt->State.cBDLE, pBDLE));
#endif
    return pBDLE;
}
#endif

/**
 * Retrieves the minimum number of bytes accumulated/free in the
 * FIFO before the controller will start a fetch/eviction of data.
 *
 * Uses SDFIFOW (FIFO Watermark Register).
 *
 * @return Number of bytes accumulated/free in the FIFO.
 */
DECLINLINE(uint8_t) hdaStreamGetFIFOW(PHDASTATE pThis, PHDASTREAM pStrmSt)
{
    AssertPtrReturn(pThis, 0);
    AssertPtrReturn(pStrmSt, 0);

#ifdef VBOX_HDA_WITH_FIFO
    return hdaSDFIFOWToBytes(HDA_STREAM_REG(pThis, FIFOW, pStrmSt->u8Strm));
#else
    return 0;
#endif
}

static int hdaProcessInterrupt(PHDASTATE pThis)
{
#define IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, num) \
        (   INTCTL_SX((pThis), num) \
         && (SDSTS(pThis, num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))

    bool fIrq = false;

    if (/* Controller Interrupt Enable (CIE). */
          HDA_REG_FLAG_VALUE(pThis, INTCTL, CIE)
       && (   HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RINTFL)
           || HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RIRBOIS)
           || (HDA_REG(pThis, STATESTS) & HDA_REG(pThis, WAKEEN))))
        fIrq = true;

    /** @todo Don't hardcode stream numbers here. */
    if (   IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 0)
        || IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 4))
    {
#ifdef IN_RING3
        LogFunc(("BCIS\n"));
#endif
        fIrq = true;
    }

    if (HDA_REG_FLAG_VALUE(pThis, INTCTL, GIE))
    {
        LogFunc(("%s\n", fIrq ? "Asserted" : "Deasserted"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0 , fIrq);
    }

#undef IS_INTERRUPT_OCCURED_AND_ENABLED

    return VINF_SUCCESS;
}

/**
 * Looks up a register at the exact offset given by @a offReg.
 *
 * @returns Register index on success, -1 if not found.
 * @param   pThis               The HDA device state.
 * @param   offReg              The register offset.
 */
static int hdaRegLookup(PHDASTATE pThis, uint32_t offReg)
{
    /*
     * Aliases.
     */
    if (offReg >= g_aHdaRegAliases[0].offReg)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegAliases); i++)
            if (offReg == g_aHdaRegAliases[i].offReg)
                return g_aHdaRegAliases[i].idxAlias;
        Assert(g_aHdaRegMap[RT_ELEMENTS(g_aHdaRegMap) - 1].offset < offReg);
        return -1;
    }

    /*
     * Binary search the
     */
    int idxEnd  = RT_ELEMENTS(g_aHdaRegMap);
    int idxLow  = 0;
    for (;;)
    {
        int idxMiddle = idxLow + (idxEnd - idxLow) / 2;
        if (offReg < g_aHdaRegMap[idxMiddle].offset)
        {
            if (idxLow == idxMiddle)
                break;
            idxEnd = idxMiddle;
        }
        else if (offReg > g_aHdaRegMap[idxMiddle].offset)
        {
            idxLow = idxMiddle + 1;
            if (idxLow >= idxEnd)
                break;
        }
        else
            return idxMiddle;
    }

#ifdef RT_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
        Assert(g_aHdaRegMap[i].offset != offReg);
#endif
    return -1;
}

/**
 * Looks up a register covering the offset given by @a offReg.
 *
 * @returns Register index on success, -1 if not found.
 * @param   pThis               The HDA device state.
 * @param   offReg              The register offset.
 */
static int hdaRegLookupWithin(PHDASTATE pThis, uint32_t offReg)
{
    /*
     * Aliases.
     */
    if (offReg >= g_aHdaRegAliases[0].offReg)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegAliases); i++)
        {
            uint32_t off = offReg - g_aHdaRegAliases[i].offReg;
            if (off < 4 && off < g_aHdaRegMap[g_aHdaRegAliases[i].idxAlias].size)
                return g_aHdaRegAliases[i].idxAlias;
        }
        Assert(g_aHdaRegMap[RT_ELEMENTS(g_aHdaRegMap) - 1].offset < offReg);
        return -1;
    }

    /*
     * Binary search the register map.
     */
    int idxEnd  = RT_ELEMENTS(g_aHdaRegMap);
    int idxLow  = 0;
    for (;;)
    {
        int idxMiddle = idxLow + (idxEnd - idxLow) / 2;
        if (offReg < g_aHdaRegMap[idxMiddle].offset)
        {
            if (idxLow == idxMiddle)
                break;
            idxEnd = idxMiddle;
        }
        else if (offReg >= g_aHdaRegMap[idxMiddle].offset + g_aHdaRegMap[idxMiddle].size)
        {
            idxLow = idxMiddle + 1;
            if (idxLow >= idxEnd)
                break;
        }
        else
            return idxMiddle;
    }

#ifdef RT_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
        Assert(offReg - g_aHdaRegMap[i].offset >= g_aHdaRegMap[i].size);
#endif
    return -1;
}

#ifdef IN_RING3
static int hdaCmdSync(PHDASTATE pThis, bool fLocal)
{
    int rc = VINF_SUCCESS;
    if (fLocal)
    {
        Assert((HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA)));
        rc = PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pThis->u64CORBBase, pThis->pu32CorbBuf, pThis->cbCorbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
#ifdef DEBUG_CMD_BUFFER
        uint8_t i = 0;
        do
        {
            LogFunc(("CORB%02x: ", i));
            uint8_t j = 0;
            do
            {
                const char *pszPrefix;
                if ((i + j) == HDA_REG(pThis, CORBRP));
                    pszPrefix = "[R]";
                else if ((i + j) == HDA_REG(pThis, CORBWP));
                    pszPrefix = "[W]";
                else
                    pszPrefix = "   "; /* three spaces */
                LogFunc(("%s%08x", pszPrefix, pThis->pu32CorbBuf[i + j]));
                j++;
            } while (j < 8);
            LogFunc(("\n"));
            i += 8;
        } while(i != 0);
#endif
    }
    else
    {
        Assert((HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA)));
        rc = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), pThis->u64RIRBBase, pThis->pu64RirbBuf, pThis->cbRirbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
#ifdef DEBUG_CMD_BUFFER
        uint8_t i = 0;
        do {
            LogFunc(("RIRB%02x: ", i));
            uint8_t j = 0;
            do {
                const char *prefix;
                if ((i + j) == HDA_REG(pThis, RIRBWP))
                    prefix = "[W]";
                else
                    prefix = "   ";
                LogFunc((" %s%016lx", prefix, pThis->pu64RirbBuf[i + j]));
            } while (++j < 8);
            LogFunc(("\n"));
            i += 8;
        } while (i != 0);
#endif
    }
    return rc;
}

static int hdaCORBCmdProcess(PHDASTATE pThis)
{
    PFNHDACODECVERBPROCESSOR pfn = (PFNHDACODECVERBPROCESSOR)NULL;

    int rc = hdaCmdSync(pThis, true);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    uint8_t corbRp = HDA_REG(pThis, CORBRP);
    uint8_t corbWp = HDA_REG(pThis, CORBWP);
    uint8_t rirbWp = HDA_REG(pThis, RIRBWP);

    Assert((corbWp != corbRp));
    LogFlowFunc(("CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP), HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));

    while (corbRp != corbWp)
    {
        uint32_t cmd;
        uint64_t resp;
        pfn = NULL;
        corbRp++;
        cmd = pThis->pu32CorbBuf[corbRp];

        rc = pThis->pCodec->pfnLookup(pThis->pCodec, HDA_CODEC_CMD(cmd, 0 /* Codec index */), &pfn);
        if (RT_SUCCESS(rc))
        {
            AssertPtr(pfn);
            rc = pfn(pThis->pCodec, HDA_CODEC_CMD(cmd, 0 /* LUN */), &resp);
        }

        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        (rirbWp)++;

        LogFunc(("verb:%08x->%016lx\n", cmd, resp));
        if (   (resp & CODEC_RESPONSE_UNSOLICITED)
            && !HDA_REG_FLAG_VALUE(pThis, GCTL, UR))
        {
            LogFunc(("unexpected unsolicited response.\n"));
            HDA_REG(pThis, CORBRP) = corbRp;
            return rc;
        }

        pThis->pu64RirbBuf[rirbWp] = resp;

        pThis->u8RespIntCnt++;
        if (pThis->u8RespIntCnt == RINTCNT_N(pThis))
            break;
    }
    HDA_REG(pThis, CORBRP) = corbRp;
    HDA_REG(pThis, RIRBWP) = rirbWp;
    rc = hdaCmdSync(pThis, false);
    LogFunc(("CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP),
         HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));
    if (HDA_REG_FLAG_VALUE(pThis, RIRBCTL, RIC))
    {
        HDA_REG(pThis, RIRBSTS) |= HDA_REG_FIELD_FLAG_MASK(RIRBSTS,RINTFL);

        pThis->u8RespIntCnt = 0;
        rc = hdaProcessInterrupt(pThis);
    }
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    return rc;
}

static int hdaStreamInit(PHDASTATE pThis, PHDASTREAM pStrmSt, uint8_t u8Strm)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pStrmSt, VERR_INVALID_POINTER);

    pStrmSt->u8Strm     = u8Strm;
    pStrmSt->u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, u8Strm),
                                      HDA_STREAM_REG(pThis, BDPU, u8Strm));
    pStrmSt->u16LVI     = HDA_STREAM_REG(pThis, LVI, u8Strm);
    pStrmSt->u32CBL     = HDA_STREAM_REG(pThis, CBL, u8Strm);
    pStrmSt->u16FIFOS   = hdaSDFIFOSToBytes(HDA_STREAM_REG(pThis, FIFOS, u8Strm));

    hdaStreamDestroy(pStrmSt);

    int rc = VINF_SUCCESS;

    if (pStrmSt->u16LVI) /* Any BDLEs to fetch? */
    {
        uint32_t cbBDLE = 0;

        pStrmSt->State.cBDLE  = pStrmSt->u16LVI + 1; /* See 18.2.37: If LVI is n, then there are n + 1 entries. */
        pStrmSt->State.paBDLE = (PHDABDLE)RTMemAllocZ(sizeof(HDABDLE) * pStrmSt->State.cBDLE);
        if (pStrmSt->State.paBDLE)
        {
            for (uint16_t i = 0; i < pStrmSt->State.cBDLE; i++)
            {
                rc = hdaBDLEFetch(pThis, &pStrmSt->State.paBDLE[i], pStrmSt->u64BaseDMA, i);
                if (RT_FAILURE(rc))
                    break;

                cbBDLE += pStrmSt->State.paBDLE[i].u32BufSize;
            }

#ifdef DEBUG
            hdaBDLEDumpAll(pThis, pStrmSt->u64BaseDMA, pStrmSt->State.cBDLE);
#endif
            if (RT_SUCCESS(rc))
            {
                if (pStrmSt->u32CBL != cbBDLE)
                    LogRel(("HDA: Warning: CBL (%RU32) does not match BDL entries (%RU32); expect sound hickups\n",
                            pStrmSt->u32CBL, cbBDLE));

                HDA_STREAM_REG(pThis, STS, pStrmSt->u8Strm) |= HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    LogFunc(("[SD%RU8]: DMA=0x%x, LVI=%RU16, CBL=%RU32, FIFOS=%RU16\n",
             u8Strm, pStrmSt->u64BaseDMA, pStrmSt->u16LVI, pStrmSt->u32CBL, pStrmSt->u16FIFOS));

    return rc;
}

static void hdaStreamDestroy(PHDASTREAM pStrmSt)
{
    AssertPtrReturnVoid(pStrmSt);

    if (pStrmSt->State.paBDLE)
    {
        Assert(pStrmSt->State.cBDLE);
        RTMemFree(pStrmSt->State.paBDLE);
        pStrmSt->State.paBDLE = NULL;
    }

    pStrmSt->State.cBDLE = 0;
}
#endif

static void hdaStreamReset(PHDASTATE pThis, PHDASTREAM pStrmSt, uint8_t u8Strm)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStrmSt);
    AssertReturnVoid(u8Strm <= 7); /** @todo Use a define for MAX_STREAMS! */

    /*
     * Initialize stream state.
     */
    RT_BZERO(pStrmSt, sizeof(HDASTREAM));

    /*
     * Initialize registers.
     */
    HDA_STREAM_REG(pThis, STS,   u8Strm) = 0;
    /* According to the ICH6 datasheet, 0x40000 is the default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRST bit. */
    HDA_STREAM_REG(pThis, CTL,   u8Strm) = 0x40000 | (HDA_STREAM_REG(pThis, CTL, u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    /* ICH6 defines default values (0x77 for input and 0xBF for output descriptors) of FIFO size. 18.2.39. */
    HDA_STREAM_REG(pThis, FIFOS, u8Strm) = u8Strm < 4 ? HDA_SDINFIFO_120B : HDA_SDONFIFO_192B;
    /* See 18.2.38: Always defaults to 0x4 (32 bytes). */
    HDA_STREAM_REG(pThis, FIFOW, u8Strm) = HDA_SDFIFOW_32B;
    HDA_STREAM_REG(pThis, LPIB,  u8Strm) = 0;
    HDA_STREAM_REG(pThis, CBL,   u8Strm) = 0;
    HDA_STREAM_REG(pThis, LVI,   u8Strm) = 0;
    HDA_STREAM_REG(pThis, FMT,   u8Strm) = 0;
    HDA_STREAM_REG(pThis, BDPU,  u8Strm) = 0;
    HDA_STREAM_REG(pThis, BDPL,  u8Strm) = 0;

    LogFunc(("[SD%RU8] Reset\n", u8Strm));
}

/* Register access handlers. */

static int hdaRegReadUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    *pu32Value = 0;
    return VINF_SUCCESS;
}

static int hdaRegWriteUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return VINF_SUCCESS;
}

/* U8 */
static int hdaRegReadU8(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xffffff00) == 0);
    return hdaRegReadU32(pThis, iReg, pu32Value);
}

static int hdaRegWriteU8(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xffffff00) == 0);
    return hdaRegWriteU32(pThis, iReg, u32Value);
}

/* U16 */
static int hdaRegReadU16(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xffff0000) == 0);
    return hdaRegReadU32(pThis, iReg, pu32Value);
}

static int hdaRegWriteU16(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xffff0000) == 0);
    return hdaRegWriteU32(pThis, iReg, u32Value);
}

/* U24 */
static int hdaRegReadU24(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xff000000) == 0);
    return hdaRegReadU32(pThis, iReg, pu32Value);
}

static int hdaRegWriteU24(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xff000000) == 0);
    return hdaRegWriteU32(pThis, iReg, u32Value);
}

/* U32 */
static int hdaRegReadU32(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t iRegMem = g_aHdaRegMap[iReg].mem_idx;

    *pu32Value = pThis->au32Regs[iRegMem] & g_aHdaRegMap[iReg].readable;
    return VINF_SUCCESS;
}

static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t iRegMem = g_aHdaRegMap[iReg].mem_idx;

    pThis->au32Regs[iRegMem]  = (u32Value & g_aHdaRegMap[iReg].writable)
                              | (pThis->au32Regs[iRegMem] & ~g_aHdaRegMap[iReg].writable);
    return VINF_SUCCESS;
}

static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, RST))
    {
        /* Exit reset state. */
        HDA_REG(pThis, GCTL) |= HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = false;
    }
    else
    {
#ifdef IN_RING3
        /* Enter reset state. */
        if (   HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA)
            || HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA))
        {
            LogFunc(("HDA enters in reset with DMA(RIRB:%s, CORB:%s)\n",
                     HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) ? "on" : "off",
                     HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA) ? "on" : "off"));
        }
        hdaReset(pThis->CTX_SUFF(pDevIns));
        HDA_REG(pThis, GCTL) &= ~HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = true;
#else
        return VINF_IOM_R3_MMIO_WRITE;
#endif
    }
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, FSH))
    {
        /* Flush: GSTS:1 set, see 6.2.6. */
        HDA_REG(pThis, GSTS) |= HDA_REG_FIELD_FLAG_MASK(GSTS, FSH); /* Set the flush state. */
        /* DPLBASE and DPUBASE should be initialized with initial value (see 6.2.6). */
    }
    return VINF_SUCCESS;
}

static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t iRegMem = g_aHdaRegMap[iReg].mem_idx;

    uint32_t v = pThis->au32Regs[iRegMem];
    uint32_t nv = u32Value & HDA_STATES_SCSF;
    pThis->au32Regs[iRegMem] &= ~(v & nv); /* write of 1 clears corresponding bit */
    return VINF_SUCCESS;
}

static int hdaRegReadINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t v = 0;
    if (   HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RIRBOIS)
        || HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RINTFL)
        || HDA_REG_FLAG_VALUE(pThis, CORBSTS, CMEI)
        || HDA_REG(pThis, STATESTS))
    {
        v |= RT_BIT(30); /* Touch CIS. */
    }

#define HDA_IS_STREAM_EVENT(pThis, num)                                  \
       (   (SDSTS((pThis), num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, DE))    \
        || (SDSTS((pThis), num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, FE))    \
        || (SDSTS((pThis), num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))

#define HDA_MARK_STREAM(pThis, num, v) \
        do { (v) |= HDA_IS_STREAM_EVENT((pThis), num) ? RT_BIT((num)) : 0; } while(0)

    HDA_MARK_STREAM(pThis, 0, v);
    HDA_MARK_STREAM(pThis, 1, v);
    HDA_MARK_STREAM(pThis, 2, v);
    HDA_MARK_STREAM(pThis, 3, v);
    HDA_MARK_STREAM(pThis, 4, v);
    HDA_MARK_STREAM(pThis, 5, v);
    HDA_MARK_STREAM(pThis, 6, v);
    HDA_MARK_STREAM(pThis, 7, v);

#undef HDA_IS_STREAM_EVENT
#undef HDA_MARK_STREAM

    v |= v ? RT_BIT(31) : 0;

    *pu32Value = v;
    return VINF_SUCCESS;
}

static int hdaRegReadLPIB(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    const uint8_t  u8Strm  = HDA_SD_NUM_FROM_REG(pThis, LPIB, iReg);
          uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, u8Strm);
    const uint32_t u32CBL  = HDA_STREAM_REG(pThis, CBL,  u8Strm);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32, CBL=%RU32\n", u8Strm, u32LPIB, u32CBL));

    *pu32Value = u32LPIB;
    return VINF_SUCCESS;
}

static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    /* HDA spec (1a): 3.3.16 WALCLK counter ticks with 24Mhz bitclock rate. */
    *pu32Value = (uint32_t)ASMMultU64ByU32DivByU32(PDMDevHlpTMTimeVirtGetNano(pThis->CTX_SUFF(pDevIns))
                                                   - pThis->u64BaseTS, 24, 1000);
    LogFlowFunc(("%RU32\n", *pu32Value));
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(CORBRP, RST))
    {
        HDA_REG(pThis, CORBRP) = 0;
    }
#ifndef BIRD_THINKS_CORBRP_IS_MOSTLY_RO
    else
        return hdaRegWriteU8(pThis, iReg, u32Value);
#endif
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    int rc = hdaRegWriteU8(pThis, iReg, u32Value);
    AssertRC(rc);
    if (   HDA_REG(pThis, CORBWP)                  != HDA_REG(pThis, CORBRP)
        && HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) != 0)
    {
        return hdaCORBCmdProcess(pThis);
    }
    return rc;
#else
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t v = HDA_REG(pThis, CORBSTS);
    HDA_REG(pThis, CORBSTS) &= ~(v & u32Value);
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    int rc;
    rc = hdaRegWriteU16(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    if (HDA_REG(pThis, CORBWP) == HDA_REG(pThis, CORBRP))
        return VINF_SUCCESS;
    if (!HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA))
        return VINF_SUCCESS;
    rc = hdaCORBCmdProcess(pThis);
    return rc;
#else
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static int hdaRegWriteSDCBL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    const uint8_t  u8Strm  = HDA_SD_NUM_FROM_REG(pThis, CBL, iReg);
          uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, u8Strm);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32, CBL=%RU32\n", u8Strm, u32LPIB, u32Value));

    return hdaRegWriteU32(pThis, iReg, u32Value);
}

static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    bool fRun      = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fInRun    = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fReset    = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    bool fInReset  = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    uint8_t u8Strm = HDA_SD_NUM_FROM_REG(pThis, CTL, iReg);

    PHDASTREAM pStrmSt;
    switch (u8Strm)
    {
        case 0: /** @todo Use dynamic indices, based on stream assignment. */
        {
            pStrmSt = &pThis->StrmStLineIn;
            break;
        }
# ifdef VBOX_WITH_HDA_MIC_IN
        case 2: /** @todo Use dynamic indices, based on stream assignment. */
        {
            pStrmSt = &pThis->StrmStMicIn;
            break;
        }
# endif
        case 4: /** @todo Use dynamic indices, based on stream assignment. */
        {
            pStrmSt = &pThis->StrmStOut;
            break;
        }

        default:
        {
            LogFunc(("Warning: Changing SDCTL on non-attached stream (iReg=0x%x)\n", iReg));
            return hdaRegWriteU24(pThis, iReg, u32Value); /* Write 3 bytes. */
        }
    }

    LogFunc(("[SD%RU8]: %R[sdctl]\n", u8Strm, u32Value));

    if (fInReset)
    {
        /* Guest is resetting HDA's stream, we're expecting guest will mark stream as exit. */
        Assert(!fReset);
        LogFunc(("Guest initiated exit of stream reset\n"));
    }
    else if (fReset)
    {
#ifdef IN_RING3
        /* ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset. */
        Assert(!fInRun && !fRun);

        LogFunc(("Guest initiated enter to stream reset\n"));
        hdaStreamReset(pThis, pStrmSt, u8Strm);
#else
        return VINF_IOM_R3_MMIO_WRITE;
#endif
    }
    else
    {
#ifdef IN_RING3
        /*
         * We enter here to change DMA states only.
         */
        if (fInRun != fRun)
        {
            Assert(!fReset && !fInReset);

            PHDADRIVER pDrv;
            switch (u8Strm)
            {
                case 0: /** @todo Use a variable here. Later. */
                {
                    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
                        pDrv->pConnector->pfnEnableIn(pDrv->pConnector,
                                                      pDrv->LineIn.pStrmIn, fRun);
                    break;
                }
# ifdef VBOX_WITH_HDA_MIC_IN
                case 2: /** @todo Use a variable here. Later. */
                {
                    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
                        pDrv->pConnector->pfnEnableIn(pDrv->pConnector,
                                                      pDrv->MicIn.pStrmIn, fRun);
                    break;
                }
# endif
                case 4: /** @todo Use a variable here. Later. */
                {
                    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
                        pDrv->pConnector->pfnEnableOut(pDrv->pConnector,
                                                       pDrv->Out.pStrmOut, fRun);
                    break;
                }
                default:
                    AssertMsgFailed(("Changing RUN bit on non-attached stream, register %RU32\n", iReg));
                    break;
            }
        }

        if (pStrmSt)
        {
            int rc2 = hdaStreamInit(pThis, pStrmSt, u8Strm);
            AssertRC(rc2);
        }

#else /* !IN_RING3 */
        return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
    }

    return hdaRegWriteU24(pThis, iReg, u32Value);
}

static int hdaRegWriteSDSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t v = HDA_REG_IND(pThis, iReg);
    v &= ~(u32Value & v);
    HDA_REG_IND(pThis, iReg) = v;
    hdaProcessInterrupt(pThis);
    return VINF_SUCCESS;
}

static int hdaRegWriteSDLVI(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    /* Only can be modified if RUN bit is 0. */
    bool fIsRunning = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    if (fIsRunning)
    {
        AssertMsgFailed(("Cannot write to register when RUN bit is set\n"));
        return VINF_SUCCESS;
    }

    int rc = hdaRegWriteU32(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

static int hdaRegWriteSDFIFOW(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    switch (u32Value)
    {
        case HDA_SDFIFOW_8B:
        case HDA_SDFIFOW_16B:
        case HDA_SDFIFOW_32B:
            return hdaRegWriteU16(pThis, iReg, u32Value);
        default:
            LogFunc(("Attempt to store unsupported value(%x) in SDFIFOW\n", u32Value));
            return hdaRegWriteU16(pThis, iReg, HDA_SDFIFOW_32B);
    }
    return VINF_SUCCESS; /* Never reached. */
}

/**
 * @note This method could be called for changing value on Output Streams
 *       only (ICH6 datasheet 18.2.39).
 */
static int hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    /** @todo Only allow updating FIFOS if RUN bit is 0? */
    uint32_t u32FIFOS = 0;

    switch (iReg)
    {
        /* SDInFIFOS is RO, n=0-3. */
        case HDA_REG_SD0FIFOS:
        case HDA_REG_SD1FIFOS:
        case HDA_REG_SD2FIFOS:
        case HDA_REG_SD3FIFOS:
        {
            LogFunc(("Guest tries to change R/O value of FIFO size of input stream, ignoring\n"));
            break;
        }
        case HDA_REG_SD4FIFOS:
        case HDA_REG_SD5FIFOS:
        case HDA_REG_SD6FIFOS:
        case HDA_REG_SD7FIFOS:
        {
            switch(u32Value)
            {
                case HDA_SDONFIFO_16B:
                case HDA_SDONFIFO_32B:
                case HDA_SDONFIFO_64B:
                case HDA_SDONFIFO_128B:
                case HDA_SDONFIFO_192B:
                    u32FIFOS = u32Value;
                    break;

                case HDA_SDONFIFO_256B: /** @todo r=andy Investigate this. */
                    LogFunc(("256-bit is unsupported, HDA is switched into 192-bit mode\n"));
                    /* Fall through is intentional. */
                default:
                    u32FIFOS = HDA_SDONFIFO_192B;
                    break;
            }

            break;
        }
        default:
        {
            AssertMsgFailed(("Something weird happened with register lookup routine\n"));
            break;
        }
    }

    if (u32FIFOS)
    {
        LogFunc(("[SD%RU8]: Updating FIFOS to %RU32 bytes\n", 0, hdaSDFIFOSToBytes(u32FIFOS)));
        /** @todo Update internal stream state with new FIFOS. */

        return hdaRegWriteU16(pThis, iReg, u32FIFOS);
    }

    return VINF_SUCCESS;
}

#ifdef IN_RING3
static int hdaSdFmtToAudSettings(uint32_t u32SdFmt, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

# define EXTRACT_VALUE(v, mask, shift) ((v & ((mask) << (shift))) >> (shift))

    int rc = VINF_SUCCESS;

    uint32_t u32Hz     = (u32SdFmt & HDA_SDFMT_BASE_RATE_SHIFT) ? 44100 : 48000;
    uint32_t u32HzMult = 1;
    uint32_t u32HzDiv  = 1;

    switch (EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT))
    {
        case 0: u32HzMult = 1; break;
        case 1: u32HzMult = 2; break;
        case 2: u32HzMult = 3; break;
        case 3: u32HzMult = 4; break;
        default:
            LogFunc(("Unsupported multiplier %x\n",
                     EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }
    switch (EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT))
    {
        case 0: u32HzDiv = 1; break;
        case 1: u32HzDiv = 2; break;
        case 2: u32HzDiv = 3; break;
        case 3: u32HzDiv = 4; break;
        case 4: u32HzDiv = 5; break;
        case 5: u32HzDiv = 6; break;
        case 6: u32HzDiv = 7; break;
        case 7: u32HzDiv = 8; break;
        default:
            LogFunc(("Unsupported divisor %x\n",
                     EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    PDMAUDIOFMT enmFmt = AUD_FMT_S16; /* Default to 16-bit signed. */
    switch (EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT))
    {
        case 0:
            LogFunc(("Requested 8-bit\n"));
            enmFmt = AUD_FMT_S8;
            break;
        case 1:
            LogFunc(("Requested 16-bit\n"));
            enmFmt = AUD_FMT_S16;
            break;
        case 2:
            LogFunc(("Requested 20-bit\n"));
            break;
        case 3:
            LogFunc(("Requested 24-bit\n"));
            break;
        case 4:
            LogFunc(("Requested 32-bit\n"));
            enmFmt = AUD_FMT_S32;
            break;
        default:
            AssertMsgFailed(("Unsupported bits shift %x\n",
                             EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        pCfg->uHz           = u32Hz * u32HzMult / u32HzDiv;
        pCfg->cChannels     = (u32SdFmt & 0xf) + 1;
        pCfg->enmFormat     = enmFmt;
        pCfg->enmEndianness = PDMAUDIOHOSTENDIANNESS;
    }

# undef EXTRACT_VALUE

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif

static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
# ifdef VBOX_WITH_HDA_CODEC_EMU
    /* No reason to re-open stream with same settings. */
    if (u32Value == HDA_REG_IND(pThis, iReg))
        return VINF_SUCCESS;

    PDMAUDIOSTREAMCFG as;
    int rc = hdaSdFmtToAudSettings(u32Value, &as);
    if (RT_FAILURE(rc))
        return rc;

    PHDADRIVER pDrv;
    switch (iReg)
    {
        case HDA_REG_SD0FMT:
            RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
                rc = hdaCodecOpenStream(pThis->pCodec, PI_INDEX, &as);
            break;
#  ifdef VBOX_WITH_HDA_MIC_IN
        case HDA_REG_SD2FMT:
            RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
                rc = hdaCodecOpenStream(pThis->pCodec, MC_INDEX, &as);
            break;
#  endif
        default:
            LogFunc(("Warning: Attempt to change format on register %RU32\n", iReg));
            break;
    }

    /** @todo r=andy rc gets lost; needs fixing. */
    return hdaRegWriteU16(pThis, iReg, u32Value);
# else /* !VBOX_WITH_HDA_CODEC_EMU */
    return hdaRegWriteU16(pThis, iReg, u32Value);
# endif
#else /* !IN_RING3 */
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static int hdaRegWriteSDBDPL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

static int hdaRegWriteSDBDPU(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

static int hdaRegReadIRS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    int rc = VINF_SUCCESS;
    /* regarding 3.4.3 we should mark IRS as busy in case CORB is active */
    if (   HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP)
        || HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA))
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */

    rc = hdaRegReadU32(pThis, iReg, pu32Value);
    return rc;
}

static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;

    /*
     * If the guest set the ICB bit of IRS register, HDA should process the verb in IC register,
     * write the response to IR register, and set the IRV (valid in case of success) bit of IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, ICB)
        && !HDA_REG_FLAG_VALUE(pThis, IRS, ICB))
    {
#ifdef IN_RING3
        PFNHDACODECVERBPROCESSOR    pfn = NULL;
        uint64_t                    resp;
        uint32_t cmd = HDA_REG(pThis, IC);
        if (HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP))
        {
            /*
             * 3.4.3 defines behavior of immediate Command status register.
             */
            LogRel(("guest attempted process immediate verb (%x) with active CORB\n", cmd));
            return rc;
        }
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */
        LogFunc(("IC:%x\n", cmd));

        rc = pThis->pCodec->pfnLookup(pThis->pCodec,
                                      HDA_CODEC_CMD(cmd, 0 /* LUN */),
                                      &pfn);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        rc = pfn(pThis->pCodec,
                 HDA_CODEC_CMD(cmd, 0 /* LUN */), &resp);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);

        HDA_REG(pThis, IR) = (uint32_t)resp;
        LogFunc(("IR:%x\n", HDA_REG(pThis, IR)));
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, IRV);  /* result is ready  */
        HDA_REG(pThis, IRS) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, ICB); /* busy is clear */
#else /* !IN_RING3 */
        rc = VINF_IOM_R3_MMIO_WRITE;
#endif
        return rc;
    }
    /*
     * Once the guest read the response, it should clean the IRV bit of the IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, IRV)
        && HDA_REG_FLAG_VALUE(pThis, IRS, IRV))
        HDA_REG(pThis, IRS) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, IRV);
    return rc;
}

static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(RIRBWP, RST))
    {
        HDA_REG(pThis, RIRBWP) = 0;
    }
    /* The remaining bits are O, see 6.2.22 */
    return VINF_SUCCESS;
}

static int hdaRegWriteBase(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t iRegMem = g_aHdaRegMap[iReg].mem_idx;
    int rc = hdaRegWriteU32(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    switch(iReg)
    {
        case HDA_REG_CORBLBASE:
            pThis->u64CORBBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64CORBBase |= pThis->au32Regs[iRegMem];
            break;
        case HDA_REG_CORBUBASE:
            pThis->u64CORBBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64CORBBase |= ((uint64_t)pThis->au32Regs[iRegMem] << 32);
            break;
        case HDA_REG_RIRBLBASE:
            pThis->u64RIRBBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64RIRBBase |= pThis->au32Regs[iRegMem];
            break;
        case HDA_REG_RIRBUBASE:
            pThis->u64RIRBBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64RIRBBase |= ((uint64_t)pThis->au32Regs[iRegMem] << 32);
            break;
        case HDA_REG_DPLBASE:
        {
            pThis->u64DPBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64DPBase |= pThis->au32Regs[iRegMem];

            /* Also make sure to handle the DMA position enable bit. */
            bool fEnabled = pThis->au32Regs[iRegMem] & RT_BIT_32(0);
            if (pThis->fDMAPosition != fEnabled)
            {
                LogRel(("HDA: %s DMA position buffer\n", fEnabled ? "Enabled" : "Disabled"));
                pThis->fDMAPosition = fEnabled;
            }
            break;
        }
        case HDA_REG_DPUBASE:
            pThis->u64DPBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64DPBase |= ((uint64_t)pThis->au32Regs[iRegMem] << 32);
            break;
        default:
            AssertMsgFailed(("Invalid index\n"));
            break;
    }

    LogFunc(("CORB base:%llx RIRB base: %llx DP base: %llx\n",
             pThis->u64CORBBase, pThis->u64RIRBBase, pThis->u64DPBase));
    return rc;
}

static int hdaRegWriteRIRBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint8_t v = HDA_REG(pThis, RIRBSTS);
    HDA_REG(pThis, RIRBSTS) &= ~(v & u32Value);

    return hdaProcessInterrupt(pThis);
}

#ifdef IN_RING3
#ifdef LOG_ENABLED
static void hdaBDLEDumpAll(PHDASTATE pThis, uint64_t u64BaseDMA, uint16_t cBDLE)
{
    uint32_t cbBDLE = 0;

    for (uint16_t i = 0; i < cBDLE; i++)
    {
        uint8_t bdle[16]; /** @todo Use a define. */
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BaseDMA + i * 16, bdle, 16); /** @todo Use a define. */

        uint64_t addr = *(uint64_t *)bdle;
        uint32_t len  = *(uint32_t *)&bdle[8];
        uint32_t ioc  = *(uint32_t *)&bdle[12];

        LogFlowFunc(("#%03d BDLE(adr:0x%llx, size:%RU32, ioc:%RTbool)\n",
                     i, addr, len, RT_BOOL(ioc & 0x1)));

        cbBDLE += len;
    }

    LogFlowFunc(("Total: %RU32 bytes\n", cbBDLE));

    if (!pThis->u64DPBase) /* No DMA base given? Bail out. */
        return;

    for (int i = 0; i < 8; i++) /** @todo Use a define for MAX_STREAMS! */
    {
        uint32_t uDMACnt;
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), (pThis->u64DPBase & DPBASE_ADDR_MASK) + i * 8, /** @todo Use a define. */
                          &uDMACnt, sizeof(&uDMACnt));

        LogFlowFunc(("%s #%02d STREAM(0x%x)\n",
                     i == HDA_SDCTL_NUM(pThis, 4) || i == HDA_SDCTL_NUM(pThis, 0) ? "*" : " ", i , uDMACnt));
    }
}
#endif

/**
 * Fetches a Bundle Descriptor List Entry (BDLE) from the DMA engine.
 *
 * @param   pThis                   Pointer to HDA state.
 * @param   pBDLE                   Where to store the fetched result.
 * @param   u64BaseDMA              Address base of DMA engine to use.
 * @param   u16Entry                BDLE entry to fetch.
 */
static int hdaBDLEFetch(PHDASTATE pThis, PHDABDLE pBDLE, uint64_t u64BaseDMA, uint16_t u16Entry)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pBDLE,   VERR_INVALID_POINTER);
    AssertReturn(u64BaseDMA, VERR_INVALID_PARAMETER);
    /** @todo Compare u16Entry with LVI. */

    uint8_t uBundleEntry[16]; /** @todo Define a BDLE length. */
    int rc = PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BaseDMA + u16Entry * 16, /** @todo Define a BDLE length. */
                               uBundleEntry, RT_ELEMENTS(uBundleEntry));
    if (RT_FAILURE(rc))
        return rc;

    pBDLE->State.u32BDLIndex = u16Entry;
    pBDLE->u64BufAdr         = *(uint64_t *) uBundleEntry;
    pBDLE->u32BufSize        = *(uint32_t *)&uBundleEntry[8];
    if (pBDLE->u32BufSize < sizeof(uint16_t)) /* Must be at least one word. */
        return VERR_INVALID_STATE;

    pBDLE->fIntOnCompletion  = (*(uint32_t *)&uBundleEntry[12]) & 0x1;

    return VINF_SUCCESS;
}

static void hdaBDLEReset(PHDABDLE pBDLE)
{
    AssertPtrReturnVoid(pBDLE);

    pBDLE->State.u32BufOff    = 0;
    pBDLE->State.cbBelowFIFOW = 0;
}

/**
 * Returns the number of outstanding stream data bytes which need to be processed
 * by the DMA engine assigned to this stream.
 *
 * @return Number of bytes for the DMA engine to process.
 */
DECLINLINE(uint32_t) hdaStreamGetTransferSize(PHDASTATE pThis, PHDASTREAM pStrmSt, uint32_t cbMax)
{
    AssertPtrReturn(pThis, 0);
    AssertPtrReturn(pStrmSt, 0);

    if (!cbMax)
        return 0;

    PHDABDLE pBDLE = hdaStreamGetCurrentBDLE(pThis, pStrmSt);

    uint32_t cbFree = pStrmSt->u32CBL - HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm);
    if (cbFree)
    {
        /* Limit to the available free space of the current BDLE. */
        cbFree = RT_MIN(cbFree, pBDLE->u32BufSize - pBDLE->State.u32BufOff);

        /* Make sure we only copy as much as the stream's FIFO can hold (SDFIFOS, 18.2.39). */
        cbFree = RT_MIN(cbFree, pStrmSt->u16FIFOS);

        /* Make sure we only transfer as many bytes as requested. */
        cbFree = RT_MIN(cbFree, cbMax);

        if (pBDLE->State.cbBelowFIFOW)
        {
            /* Are we not going to reach (or exceed) the FIFO watermark yet with the data to copy?
             * No need to read data from DMA then. */
            if (cbFree > pBDLE->State.cbBelowFIFOW)
            {
                /* Subtract the amount of bytes that still would fit in the stream's FIFO
                 * and therefore do not need to be processed by DMA. */
                cbFree -= pBDLE->State.cbBelowFIFOW;
            }
        }
    }

    LogFlowFunc(("[SD%RU8]: CBL=%RU32, LPIB=%RU32, cbFree=%RU32, %R[bdle]\n", pStrmSt->u8Strm,
                 pStrmSt->u32CBL, HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm), cbFree, pBDLE));
    return cbFree;
}

DECLINLINE(void) hdaBDLEUpdate(PHDABDLE pBDLE, uint32_t cbData, uint32_t cbProcessed)
{
    AssertPtrReturnVoid(pBDLE);

    if (!cbData || !cbProcessed)
        return;

    /* Fewer than cbBelowFIFOW bytes were copied.
     * Probably we need to move the buffer, but it is rather hard to imagine a situation
     * where it might happen. */
    AssertMsg((cbProcessed == pBDLE->State.cbBelowFIFOW + cbData), /* we assume that we write the entire buffer including unreported bytes */
              ("cbProcessed=%RU32 != pBDLE->State.cbBelowFIFOW=%RU32 + cbData=%RU32\n",
              cbProcessed, pBDLE->State.cbBelowFIFOW, cbData));

#if 0
    if (   pBDLE->State.cbBelowFIFOW
        && pBDLE->State.cbBelowFIFOW <= cbWritten)
    {
        LogFlowFunc(("BDLE(cbUnderFifoW:%RU32, off:%RU32, size:%RU32)\n",
                     pBDLE->State.cbBelowFIFOW, pBDLE->State.u32BufOff, pBDLE->u32BufSize));
    }
#endif

    pBDLE->State.cbBelowFIFOW -= RT_MIN(pBDLE->State.cbBelowFIFOW, cbProcessed);
    Assert(pBDLE->State.cbBelowFIFOW == 0);

    /* We always increment the position of DMA buffer counter because we're always reading
     * into an intermediate buffer. */
    pBDLE->State.u32BufOff += cbData;
    Assert(pBDLE->State.u32BufOff <= pBDLE->u32BufSize);

    LogFlowFunc(("cbData=%RU32, cbProcessed=%RU32, %R[bdle]\n", cbData, cbProcessed, pBDLE));
}

DECLINLINE(bool) hdaStreamNeedsNextBDLE(PHDASTATE pThis, PHDASTREAM pStrmSt)
{
    AssertPtrReturn(pThis,   false);
    AssertPtrReturn(pStrmSt, false);

    PHDABDLE pBDLE   = hdaStreamGetCurrentBDLE(pThis, pStrmSt);
    uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm);

    /* Did we reach the CBL (Cyclic Buffer List) limit? */
    bool fCBLLimitReached = u32LPIB >= pStrmSt->u32CBL;

    /* Do we need to use the next BDLE entry? Either because we reached
     * the CBL limit or our internal DMA buffer is full. */
    bool fNeedsNextBDLE   = (   fCBLLimitReached
                             || (   pBDLE->State.u32BufOff
                                 && pBDLE->State.u32BufOff >= pBDLE->u32BufSize)
                            );

    Assert(u32LPIB                <= pStrmSt->u32CBL);
    Assert(pBDLE->State.u32BufOff <= pBDLE->u32BufSize);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32, CBL=%RU32, fCBLLimitReached=%RTbool, fNeedsNextBDLE=%RTbool, %R[bdle]\n",
                 pStrmSt->u8Strm, u32LPIB, pStrmSt->u32CBL, fCBLLimitReached, fNeedsNextBDLE, pBDLE));

    if (fCBLLimitReached)
    {
        /* Reset LPIB register. */
        u32LPIB -= RT_MIN(u32LPIB, pStrmSt->u32CBL);
        hdaStreamUpdateLPIB(pThis, pStrmSt, u32LPIB);
    }

    if (fNeedsNextBDLE)
    {
        /* Reset current BDLE. */
        hdaBDLEReset(pBDLE);
    }

    return fNeedsNextBDLE;
}

DECLINLINE(void) hdaStreamTransferUpdate(PHDASTATE pThis, PHDASTREAM pStrmSt, uint32_t cbInc)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStrmSt);

    LogFlowFunc(("[SD%RU8]: cbInc=%RU32\n", pStrmSt->u8Strm, cbInc));

    Assert(cbInc <= pStrmSt->u16FIFOS);

    PHDABDLE pBDLE = hdaStreamGetCurrentBDLE(pThis, pStrmSt);

    /*
     * If we're below the FIFO watermark (SDFIFOW), it's expected that HDA
     * doesn't fetch anything via DMA, so just update LPIB.
     * (ICH6 datasheet 18.2.38).
     */
    if (pBDLE->State.cbBelowFIFOW == 0) /* Did we hit (or exceed) the watermark? */
    {
        const uint32_t u32LPIB = RT_MIN(HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm) + cbInc,
                                        pStrmSt->u32CBL);

        LogFlowFunc(("[SD%RU8]: LPIB: %RU32 -> %RU32, CBL=%RU32\n",
                     pStrmSt->u8Strm,
                     HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm), HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm) + cbInc,
                     pStrmSt->u32CBL));

        hdaStreamUpdateLPIB(pThis, pStrmSt, u32LPIB);
    }
}

static bool hdaStreamTransferIsComplete(PHDASTATE pThis, PHDASTREAM pStrmSt)
{
    AssertPtrReturn(pThis,   true);
    AssertPtrReturn(pStrmSt, true);

    bool fIsComplete = false;

    PHDABDLE       pBDLE   = hdaStreamGetCurrentBDLE(pThis, pStrmSt);
    const uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, pStrmSt->u8Strm);

    if (   pBDLE->State.u32BufOff >= pBDLE->u32BufSize
        || u32LPIB                >= pStrmSt->u32CBL)
    {
        Assert(pBDLE->State.u32BufOff <= pBDLE->u32BufSize);
        Assert(u32LPIB                <= pStrmSt->u32CBL);

        if (/* IOC (Interrupt On Completion) bit set? */
               pBDLE->fIntOnCompletion
            /* All data put into the DMA FIFO? */
            && pBDLE->State.cbBelowFIFOW == 0
           )
        {
            /**
             * Set the BCIS (Buffer Completion Interrupt Status) flag as the
             * last byte of data for the current descriptor has been fetched
             * from memory and put into the DMA FIFO.
             *
             ** @todo More carefully investigate BCIS flag.
             *
             * Speech synthesis works fine on Mac Guest if this bit isn't set
             * but in general sound quality gets worse.
             */
            HDA_STREAM_REG(pThis, STS, pStrmSt->u8Strm) |= HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS);

            /*
             * If the ICE (IOCE, "Interrupt On Completion Enable") bit of the SDCTL register is set
             * we need to generate an interrupt.
             */
            if (HDA_STREAM_REG(pThis, CTL, pStrmSt->u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE))
                hdaProcessInterrupt(pThis);
        }

        fIsComplete = true;
    }

    LogFlowFunc(("[SD%RU8]: u32LPIB=%RU32, CBL=%RU32, %R[bdle] => %s\n",
                 pStrmSt->u8Strm, u32LPIB, pStrmSt->u32CBL, pBDLE, fIsComplete ? "COMPLETE" : "INCOMPLETE"));

    return fIsComplete;
}

/**
 * hdaReadAudio - copies samples from audio backend to DMA.
 * Note: This function writes to the DMA buffer immediately,
 *       but "reports bytes" when all conditions are met (FIFOW).
 */
static int hdaReadAudio(PHDASTATE pThis, PHDASTREAM pStrmSt, PAUDMIXSINK pSink, uint32_t cbMax, uint32_t *pcbRead)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStrmSt, VERR_INVALID_POINTER);
    AssertPtrReturn(pSink,   VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    PHDABDLE pBDLE = hdaStreamGetCurrentBDLE(pThis, pStrmSt);

    int rc;
    uint32_t cbRead = 0;
    uint32_t cbBuf  = hdaStreamGetTransferSize(pThis, pStrmSt, cbMax);

    LogFlowFunc(("cbBuf=%RU32, %R[bdle]\n", cbBuf, pBDLE));

    if (!cbBuf)
    {
        /* Nothing to write, bail out. */
        rc = VINF_EOF;
    }
    else
    {
        rc = AudioMixerProcessSinkIn(pSink, AUDMIXOP_BLEND, pBDLE->State.au8FIFO, cbBuf, &cbRead);
        if (RT_SUCCESS(rc))
        {
            Assert(cbRead);
            Assert(cbRead == cbBuf);
            Assert(cbRead <= pBDLE->u32BufSize - pBDLE->State.u32BufOff);

            /*
             * Write to the BDLE's DMA buffer.
             */
            rc = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns),
                                       pBDLE->u64BufAdr + pBDLE->State.u32BufOff,
                                       pBDLE->State.au8FIFO, cbRead);
            AssertRC(rc);

            if (pBDLE->State.cbBelowFIFOW + cbRead > hdaStreamGetFIFOW(pThis, pStrmSt))
            {
                pBDLE->State.u32BufOff    += cbRead;
                pBDLE->State.cbBelowFIFOW  = 0;
                //hdaBackendReadTransferReported(pBDLE, cbDMAData, cbRead, &cbRead, pcbAvail);
            }
            else
            {
                pBDLE->State.u32BufOff    += cbRead;
                pBDLE->State.cbBelowFIFOW += cbRead;
                Assert(pBDLE->State.cbBelowFIFOW <= hdaStreamGetFIFOW(pThis, pStrmSt));
                //hdaBackendTransferUnreported(pThis, pBDLE, pStreamDesc, cbRead, pcbAvail);

                rc = VERR_NO_DATA;
            }
        }
    }

    Assert(cbRead <= pStrmSt->u16FIFOS);

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbRead;
    }

    LogFunc(("Returning cbRead=%RU32, rc=%Rrc\n", cbRead, rc));
    return rc;
}

static int hdaWriteAudio(PHDASTATE pThis, PHDASTREAM pStrmSt, uint32_t cbMax, uint32_t *pcbWritten)
{
    AssertPtrReturn(pThis,      VERR_INVALID_POINTER);
    AssertPtrReturn(pStrmSt,    VERR_INVALID_POINTER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    PHDABDLE pBDLE = hdaStreamGetCurrentBDLE(pThis, pStrmSt);
    int rc;

    uint32_t cbWritten = 0;
    uint32_t cbData    = hdaStreamGetTransferSize(pThis, pStrmSt, cbMax);

    LogFlowFunc(("cbData=%RU32, %R[bdle]\n", cbData, pBDLE));

    /*
     * Copy from DMA to the corresponding stream buffer (if there are any bytes from the
     * previous unreported transfer we write at offset 'pBDLE->State.cbUnderFifoW').
     */
    if (!cbData)
    {
        rc = VINF_EOF;
    }
    else
    {
        /*
         * Read from the current BDLE's DMA buffer.
         */
        rc = PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns),
                               pBDLE->u64BufAdr + pBDLE->State.u32BufOff,
                               pBDLE->State.au8FIFO + pBDLE->State.cbBelowFIFOW, cbData);
        AssertRC(rc);

#ifdef VBOX_WITH_STATISTICS
        STAM_COUNTER_ADD(&pThis->StatBytesRead, cbData);
#endif
        /*
         * Write to audio backend. We should ensure that we have enough bytes to copy to the backend.
         */
        uint32_t cbToWrite = cbData + pBDLE->State.cbBelowFIFOW;
        if (cbToWrite >= hdaStreamGetFIFOW(pThis, pStrmSt))
        {
            uint32_t cbWrittenToStream;
            int rc2;

            PHDADRIVER pDrv;
            RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
            {
                if (pDrv->pConnector->pfnIsActiveOut(pDrv->pConnector, pDrv->Out.pStrmOut))
                {
                    rc2 = pDrv->pConnector->pfnWrite(pDrv->pConnector, pDrv->Out.pStrmOut,
                                                     pBDLE->State.au8FIFO, cbToWrite, &cbWrittenToStream);
                    if (RT_SUCCESS(rc2))
                    {
                        if (cbWrittenToStream < cbToWrite) /* Lagging behind? */
                            LogFlowFunc(("\tLUN#%RU8: Warning: Only written %RU32 / %RU32 bytes, expect lags\n",
                                         pDrv->uLUN, cbWrittenToStream, cbToWrite));
                    }
                }
                else /* Stream disabled, not fatal. */
                {
                    cbWrittenToStream = 0;
                    rc2 = VERR_NOT_AVAILABLE;
                    /* Keep going. */
                }

                LogFlowFunc(("\tLUN#%RU8: cbToWrite=%RU32, cbWrittenToStream=%RU32, rc=%Rrc\n",
                             pDrv->uLUN, cbToWrite, cbWrittenToStream, rc2));
            }

            /* Always report all data as being written;
             * backends who were not able to catch up have to deal with it themselves. */
            cbWritten = cbToWrite;

            hdaBDLEUpdate(pBDLE, cbData, cbWritten);
        }
        else
        {
            pBDLE->State.u32BufOff += cbWritten;
            pBDLE->State.cbBelowFIFOW += cbWritten;
            Assert(pBDLE->State.cbBelowFIFOW <= hdaStreamGetFIFOW(pThis, pStrmSt));

            /* Not enough bytes to be processed and reported, we'll try our luck next time around. */
            //hdaBackendTransferUnreported(pThis, pBDLE, pStreamDesc, cbAvail, NULL);
            rc = VINF_EOF;
        }
    }

    Assert(cbWritten <= pStrmSt->u16FIFOS);

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    LogFunc(("Returning cbWritten=%RU32, rc=%Rrc\n", cbWritten, rc));
    return rc;
}

/**
 * @interface_method_impl{HDACODEC,pfnReset}
 */
static DECLCALLBACK(int) hdaCodecReset(PHDACODEC pCodec)
{
    PHDASTATE pThis = pCodec->pHDAState;
    NOREF(pThis);
    return VINF_SUCCESS;
}


static DECLCALLBACK(void) hdaCloseIn(PHDASTATE pThis, PDMAUDIORECSOURCE enmRecSource)
{
    NOREF(pThis);
    NOREF(enmRecSource);
    LogFlowFuncEnter();
}

static DECLCALLBACK(void) hdaCloseOut(PHDASTATE pThis)
{
    NOREF(pThis);
    LogFlowFuncEnter();
}

static DECLCALLBACK(int) hdaOpenIn(PHDASTATE pThis,
                                   const char *pszName, PDMAUDIORECSOURCE enmRecSource,
                                   PPDMAUDIOSTREAMCFG pCfg)
{
    PAUDMIXSINK pSink;

    switch (enmRecSource)
    {
# ifdef VBOX_WITH_HDA_MIC_IN
        case PDMAUDIORECSOURCE_MIC:
            pSink = pThis->pSinkMicIn;
            break;
# endif
        case PDMAUDIORECSOURCE_LINE_IN:
            pSink = pThis->pSinkLineIn;
            break;
        default:
            AssertMsgFailed(("Audio source %ld not supported\n", enmRecSource));
            return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;
    char *pszDesc;

    PHDADRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
    {
        if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] %s", pDrv->uLUN, pszName) <= 0)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = pDrv->pConnector->pfnCreateIn(pDrv->pConnector, pszDesc, enmRecSource, pCfg, &pDrv->LineIn.pStrmIn);
        LogFlowFunc(("LUN#%RU8: Created input \"%s\", with rc=%Rrc\n", pDrv->uLUN, pszDesc, rc));
        if (rc == VINF_SUCCESS) /* Note: Could return VWRN_ALREADY_EXISTS. */
        {
            AudioMixerRemoveStream(pSink, pDrv->LineIn.phStrmIn);
            rc = AudioMixerAddStreamIn(pSink,
                                       pDrv->pConnector, pDrv->LineIn.pStrmIn,
                                       0 /* uFlags */, &pDrv->LineIn.phStrmIn);
        }

        RTStrFree(pszDesc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) hdaOpenOut(PHDASTATE pThis,
                                    const char *pszName, PPDMAUDIOSTREAMCFG pCfg)
{
    int rc = VINF_SUCCESS;
    char *pszDesc;

    PHDADRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
    {
        if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] %s (%RU32Hz, %RU8 %s)",
                         pDrv->uLUN, pszName, pCfg->uHz, pCfg->cChannels, pCfg->cChannels > 1 ? "Channels" : "Channel") <= 0)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        rc = pDrv->pConnector->pfnCreateOut(pDrv->pConnector, pszDesc, pCfg, &pDrv->Out.pStrmOut);
        LogFlowFunc(("LUN#%RU8: Created output \"%s\", with rc=%Rrc\n", pDrv->uLUN, pszDesc, rc));
        if (rc == VINF_SUCCESS) /* Note: Could return VWRN_ALREADY_EXISTS. */
        {
            AudioMixerRemoveStream(pThis->pSinkOutput, pDrv->Out.phStrmOut);
            rc = AudioMixerAddStreamOut(pThis->pSinkOutput,
                                        pDrv->pConnector, pDrv->Out.pStrmOut,
                                        0 /* uFlags */, &pDrv->Out.phStrmOut);
        }

        RTStrFree(pszDesc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) hdaSetVolume(PHDASTATE pThis, ENMSOUNDSOURCE enmSource,
                                      bool fMute, uint8_t uVolLeft, uint8_t uVolRight)
{
    int             rc = VINF_SUCCESS;
    PDMAUDIOVOLUME  vol = { fMute, uVolLeft, uVolRight };
    PAUDMIXSINK     pSink;

    /* Convert the audio source to corresponding sink. */
    switch (enmSource)
    {
        case PO_INDEX:
            pSink = pThis->pSinkOutput;
            break;
        case PI_INDEX:
            pSink = pThis->pSinkLineIn;
            break;
        case MC_INDEX:
            pSink = pThis->pSinkMicIn;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
            break;
    }

    /* Set the volume. Codec already converted it to the correct range. */
    AudioMixerSetSinkVolume(pSink, &vol);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifndef VBOX_WITH_AUDIO_CALLBACKS

static DECLCALLBACK(void) hdaTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    AssertPtrReturnVoid(pThis);

    STAM_PROFILE_START(&pThis->StatTimer, a);

    int rc = VINF_SUCCESS;

    uint32_t cbInMax  = 0;
    uint32_t cbOutMin = UINT32_MAX;

    PHDADRIVER pDrv;

    uint32_t cbIn, cbOut, cSamplesLive;

    uint64_t uTicksNow     = PDMDevHlpTMTimeVirtGet(pDevIns);
    uint64_t uTicksElapsed = uTicksNow  - pThis->uTimerTS;
    uint64_t uTicksPerSec  = PDMDevHlpTMTimeVirtGetFreq(pDevIns);

    pThis->uTimerTS = uTicksNow;

    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
    {
        cbIn = cbOut = 0;
        rc = pDrv->pConnector->pfnQueryStatus(pDrv->pConnector,
                                              &cbIn, &cbOut, NULL /* cSamplesLive */);
        if (RT_SUCCESS(rc))
            rc = pDrv->pConnector->pfnPlayOut(pDrv->pConnector, NULL /* cSamplesPlayed */);

        uint32_t cSamplesMin  = (int)((2 * uTicksElapsed * pDrv->Out.pStrmOut->Props.uHz + uTicksPerSec) / uTicksPerSec / 2);
        uint32_t cbSamplesMin = AUDIOMIXBUF_S2B(&pDrv->Out.pStrmOut->MixBuf, cSamplesMin);

        LogFlowFunc(("LUN#%RU8: rc=%Rrc, cbOut=%RU32, cSamplesMin=%RU32, cbSamplesMin=%RU32\n",
                     pDrv->uLUN, rc, cbOut, cSamplesMin, cbSamplesMin));

        if (   RT_FAILURE(rc)
            && cbSamplesMin > cbOut)
        {
            LogFlowFunc(("LUN#%RU8: Adj: %RU32 -> %RU32\n", pDrv->uLUN, cbOut, cbSamplesMin));
            cbOut = cbSamplesMin;
        }

        cbOutMin = RT_MIN(cbOutMin, cbOut);
        cbInMax  = RT_MAX(cbInMax, cbIn);
    }

#ifdef DEBUG_TIMER
    LogFlowFunc(("cbInMax=%RU32, cbOutMin=%RU32\n", cbInMax, cbOutMin));
#endif

    if (cbOutMin == UINT32_MAX)
        cbOutMin = 0;

    /*
     * Playback.
     */
    if (cbOutMin)
    {
        Assert(cbOutMin != UINT32_MAX);
        hdaTransfer(pThis, PO_INDEX, cbOutMin /* cbMax */, NULL /* pcbProcessed */); /** @todo Add rc! */
    }

    /*
     * Recording.
     */
    if (cbInMax)
        hdaTransfer(pThis, PI_INDEX, cbInMax /* cbMax */, NULL /* pcbProcessed */); /** @todo Add rc! */

    TMTimerSet(pThis->pTimer, TMTimerGet(pThis->pTimer) + pThis->uTimerTicks);

    STAM_PROFILE_STOP(&pThis->StatTimer, a);
}

#else /* VBOX_WITH_AUDIO_CALLBACKS */

static DECLCALLBACK(int) hdaCallbackInput(PDMAUDIOCALLBACKTYPE enmType, void *pvCtx, size_t cbCtx, void *pvUser, size_t cbUser)
{
    Assert(enmType == PDMAUDIOCALLBACKTYPE_INPUT);
    AssertPtrReturn(pvCtx,  VERR_INVALID_POINTER);
    AssertReturn(cbCtx,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertReturn(cbUser,    VERR_INVALID_PARAMETER);

    PHDACALLBACKCTX pCtx = (PHDACALLBACKCTX)pvCtx;
    AssertReturn(cbCtx == sizeof(HDACALLBACKCTX), VERR_INVALID_PARAMETER);

    PPDMAUDIOCALLBACKDATAIN pData = (PPDMAUDIOCALLBACKDATAIN)pvUser;
    AssertReturn(cbUser == sizeof(PDMAUDIOCALLBACKDATAIN), VERR_INVALID_PARAMETER);

    return hdaTransfer(pCtx->pThis, PI_INDEX, UINT32_MAX, &pData->cbOutRead);
}

static DECLCALLBACK(int) hdaCallbackOutput(PDMAUDIOCALLBACKTYPE enmType, void *pvCtx, size_t cbCtx, void *pvUser, size_t cbUser)
{
    Assert(enmType == PDMAUDIOCALLBACKTYPE_OUTPUT);
    AssertPtrReturn(pvCtx,  VERR_INVALID_POINTER);
    AssertReturn(cbCtx,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertReturn(cbUser,    VERR_INVALID_PARAMETER);

    PHDACALLBACKCTX pCtx = (PHDACALLBACKCTX)pvCtx;
    AssertReturn(cbCtx == sizeof(HDACALLBACKCTX), VERR_INVALID_PARAMETER);

    PPDMAUDIOCALLBACKDATAOUT pData = (PPDMAUDIOCALLBACKDATAOUT)pvUser;
    AssertReturn(cbUser == sizeof(PDMAUDIOCALLBACKDATAOUT), VERR_INVALID_PARAMETER);

    PHDASTATE pThis = pCtx->pThis;

    int rc = hdaTransfer(pCtx->pThis, PO_INDEX, UINT32_MAX, &pData->cbOutWritten);
    if (   RT_SUCCESS(rc)
        && pData->cbOutWritten)
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            uint32_t cSamplesPlayed;
            int rc2 = pDrv->pConnector->pfnPlayOut(pDrv->pConnector, &cSamplesPlayed);
            LogFlowFunc(("LUN#%RU8: cSamplesPlayed=%RU32, rc=%Rrc\n", pDrv->uLUN, cSamplesPlayed, rc2));
        }
    }
}
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

static int hdaTransfer(PHDASTATE pThis, ENMSOUNDSOURCE enmSrc, uint32_t cbMax, uint32_t *pcbProcessed)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    /* pcbProcessed is optional. */

    LogFlowFunc(("enmSrc=%RU32, cbMax=%RU32\n", enmSrc, cbMax));

    PHDASTREAM pStrmSt;
    switch (enmSrc)
    {
        case PI_INDEX:
        {
            pStrmSt = &pThis->StrmStLineIn;
            break;
        }

#ifdef VBOX_WITH_HDA_MIC_IN
        case MC_INDEX:
        {
            pStrmSt = &pThis->StrmStMicIn;
            break;
        }
#endif
        case PO_INDEX:
        {
            pStrmSt = &pThis->StrmStOut;
            break;
        }

        default:
        {
            AssertMsgFailed(("Unknown source index %ld\n", enmSrc));
            return VERR_NOT_SUPPORTED;
        }
    }

    if (pStrmSt->State.cBDLE == 0) /* No buffers available? */
    {
        LogFlowFunc(("[SD%RU8] No buffers available\n", pStrmSt->u8Strm));

        if (pcbProcessed)
            *pcbProcessed = 0;
        return VINF_SUCCESS;
    }
    AssertPtr(pStrmSt->State.paBDLE);

    /* Is this stream running? */
    const bool fIsRunning = RT_BOOL(HDA_STREAM_REG(pThis, CTL, pStrmSt->u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    if (!fIsRunning)
    {
        LogFlowFunc(("[SD%RU8]: Stream not running\n", pStrmSt->u8Strm));

        if (pcbProcessed)
            *pcbProcessed = 0;
        return VINF_SUCCESS;
    }

    Assert(pStrmSt->u8Strm <= 7); /** @todo Use a define for MAX_STREAMS! */
    Assert(pStrmSt->u64BaseDMA);
    Assert(pStrmSt->u32CBL);

    int      rc               = VINF_SUCCESS;
    uint32_t cbToProcess      = cbMax;
    uint32_t cbProcessedTotal = 0;
    bool     fIsComplete      = false;

    while (cbToProcess)
    {
        /* Do we need to fetch the next Buffer Descriptor Entry (BDLE)? */
        if (hdaStreamNeedsNextBDLE(pThis, pStrmSt))
            hdaStreamGetNextBDLE(pThis, pStrmSt);

        /* Set the FIFORDY bit on the stream while doing the transfer. */
        HDA_STREAM_REG(pThis, STS, pStrmSt->u8Strm) |= HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);

        uint32_t cbProcessed;
        switch (enmSrc)
        {
            case PI_INDEX:
                rc = hdaReadAudio(pThis, pStrmSt, pThis->pSinkLineIn, cbToProcess, &cbProcessed);
                break;
            case PO_INDEX:
                rc = hdaWriteAudio(pThis, pStrmSt, cbToProcess, &cbProcessed);
                break;
#ifdef VBOX_WITH_HDA_MIC_IN
            case MC_INDEX:
                rc = hdaReadAudio(pThis, pStrmSt, pThis->pSinkMicIn, cbToProcess, &cbProcessed);
                break;
#endif
            default:
                AssertMsgFailed(("Unsupported source index %ld\n", enmSrc));
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        /* Remove the FIFORDY bit again. */
        HDA_STREAM_REG(pThis, STS, pStrmSt->u8Strm) &= ~HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);

        if (RT_FAILURE(rc))
            break;

        hdaStreamTransferUpdate(pThis, pStrmSt, cbProcessed);

        cbToProcess      -= RT_MIN(cbToProcess,  cbProcessed);
        cbProcessedTotal += cbProcessed;

        LogFlowFunc(("cbProcessed=%RU32, cbToProcess=%RU32, cbProcessedTotal=%RU32, rc=%Rrc\n",
                     cbProcessed, cbToProcess, cbProcessedTotal, rc));

        if (rc == VINF_EOF)
            fIsComplete = true;

        if (!fIsComplete)
            fIsComplete = hdaStreamTransferIsComplete(pThis, pStrmSt);

        if (fIsComplete)
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbProcessed)
            *pcbProcessed = cbProcessedTotal;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* IN_RING3 */

/* MMIO callbacks */

/**
 * @callback_method_impl{FNIOMMMIOREAD, Looks up and calls the appropriate handler.}
 *
 * @note During implementation, we discovered so-called "forgotten" or "hole"
 *       registers whose description is not listed in the RPM, datasheet, or
 *       spec.
 */
PDMBOTHCBDECL(int) hdaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PHDASTATE   pThis  = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int         rc;

    /*
     * Look up and log.
     */
    uint32_t        offReg = GCPhysAddr - pThis->MMIOBaseAddr;
    int             idxRegDsc = hdaRegLookup(pThis, offReg);    /* Register descriptor index. */
#ifdef LOG_ENABLED
    unsigned const  cbLog     = cb;
    uint32_t        offRegLog = offReg;
#endif

    LogFunc(("offReg=%#x cb=%#x\n", offReg, cb));
    Assert(cb == 4); Assert((offReg & 3) == 0);

    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
        LogFunc(("\tAccess to registers except GCTL is blocked while reset\n"));

    if (idxRegDsc == -1)
        LogRel(("HDA: Invalid read access @0x%x (bytes=%d)\n", offReg, cb));

    if (idxRegDsc != -1)
    {
        /* ASSUMES gapless DWORD at end of map. */
        if (g_aHdaRegMap[idxRegDsc].size == 4)
        {
            /*
             * Straight forward DWORD access.
             */
            rc = g_aHdaRegMap[idxRegDsc].pfnRead(pThis, idxRegDsc, (uint32_t *)pv);
            LogFunc(("\tRead %s => %x (%Rrc)\n", g_aHdaRegMap[idxRegDsc].abbrev, *(uint32_t *)pv, rc));
        }
        else
        {
            /*
             * Multi register read (unless there are trailing gaps).
             * ASSUMES that only DWORD reads have sideeffects.
             */
            uint32_t u32Value = 0;
            unsigned cbLeft   = 4;
            do
            {
                uint32_t const  cbReg        = g_aHdaRegMap[idxRegDsc].size;
                uint32_t        u32Tmp       = 0;

                rc = g_aHdaRegMap[idxRegDsc].pfnRead(pThis, idxRegDsc, &u32Tmp);
                LogFunc(("\tRead %s[%db] => %x (%Rrc)*\n", g_aHdaRegMap[idxRegDsc].abbrev, cbReg, u32Tmp, rc));
                if (rc != VINF_SUCCESS)
                    break;
                u32Value |= (u32Tmp & g_afMasks[cbReg]) << ((4 - cbLeft) * 8);

                cbLeft -= cbReg;
                offReg += cbReg;
                idxRegDsc++;
            } while (cbLeft > 0 && g_aHdaRegMap[idxRegDsc].offset == offReg);

            if (rc == VINF_SUCCESS)
                *(uint32_t *)pv = u32Value;
            else
                Assert(!IOM_SUCCESS(rc));
        }
    }
    else
    {
        rc = VINF_IOM_MMIO_UNUSED_FF;
        LogFunc(("\tHole at %x is accessed for read\n", offReg));
    }

    /*
     * Log the outcome.
     */
#ifdef LOG_ENABLED
    if (cbLog == 4)
        LogFunc(("\tReturning @%#05x -> %#010x %Rrc\n", offRegLog, *(uint32_t *)pv, rc));
    else if (cbLog == 2)
        LogFunc(("\tReturning @%#05x -> %#06x %Rrc\n", offRegLog, *(uint16_t *)pv, rc));
    else if (cbLog == 1)
        LogFunc(("\tReturning @%#05x -> %#04x %Rrc\n", offRegLog, *(uint8_t *)pv, rc));
#endif
    return rc;
}


DECLINLINE(int) hdaWriteReg(PHDASTATE pThis, int idxRegDsc, uint32_t u32Value, char const *pszLog)
{
    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
        LogFunc(("access to registers except GCTL is blocked while reset\n"));  /** @todo where is this enforced? */

    uint32_t idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
#ifdef LOG_ENABLED
    uint32_t const u32CurValue = pThis->au32Regs[idxRegMem];
#endif
    int rc = g_aHdaRegMap[idxRegDsc].pfnWrite(pThis, idxRegDsc, u32Value);
    LogFunc(("write %#x -> %s[%db]; %x => %x%s\n", u32Value, g_aHdaRegMap[idxRegDsc].abbrev,
             g_aHdaRegMap[idxRegDsc].size, u32CurValue, pThis->au32Regs[idxRegMem], pszLog));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE, Looks up and calls the appropriate handler.}
 */
PDMBOTHCBDECL(int) hdaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PHDASTATE               pThis  = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int                     rc;

    /*
     * The behavior of accesses that aren't aligned on natural boundraries is
     * undefined. Just reject them outright.
     */
    /** @todo IOM could check this, it could also split the 8 byte accesses for us. */
    Assert(cb == 1 || cb == 2 || cb == 4 || cb == 8);
    if (GCPhysAddr & (cb - 1))
        return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "misaligned write access: GCPhysAddr=%RGp cb=%u\n", GCPhysAddr, cb);

    /*
     * Look up and log the access.
     */
    uint32_t    offReg = GCPhysAddr - pThis->MMIOBaseAddr;
    int         idxRegDsc = hdaRegLookup(pThis, offReg);
    uint32_t    idxRegMem = idxRegDsc != -1 ? g_aHdaRegMap[idxRegDsc].mem_idx : UINT32_MAX;
    uint64_t    u64Value;
    if (cb == 4)        u64Value = *(uint32_t const *)pv;
    else if (cb == 2)   u64Value = *(uint16_t const *)pv;
    else if (cb == 1)   u64Value = *(uint8_t const *)pv;
    else if (cb == 8)   u64Value = *(uint64_t const *)pv;
    else
    {
        u64Value = 0;   /* shut up gcc. */
        AssertReleaseMsgFailed(("%u\n", cb));
    }

#ifdef LOG_ENABLED
    uint32_t const u32LogOldValue = idxRegDsc >= 0 ? pThis->au32Regs[idxRegMem] : UINT32_MAX;
    if (idxRegDsc == -1)
        LogFunc(("@%#05x u32=%#010x cb=%d\n", offReg, *(uint32_t const *)pv, cb));
    else if (cb == 4)
        LogFunc(("@%#05x u32=%#010x %s\n", offReg, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 2)
        LogFunc(("@%#05x u16=%#06x (%#010x) %s\n", offReg, *(uint16_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 1)
        LogFunc(("@%#05x u8=%#04x (%#010x) %s\n", offReg, *(uint8_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));

    if (idxRegDsc >= 0 && g_aHdaRegMap[idxRegDsc].size != cb)
        LogFunc(("\tsize=%RU32 != cb=%u!!\n", g_aHdaRegMap[idxRegDsc].size, cb));
#endif

    /*
     * Try for a direct hit first.
     */
    if (idxRegDsc != -1 && g_aHdaRegMap[idxRegDsc].size == cb)
    {
        rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "");
#ifdef LOG_ENABLED
        LogFunc(("\t%#x -> %#x\n", u32LogOldValue, idxRegMem != UINT32_MAX ? pThis->au32Regs[idxRegMem] : UINT32_MAX));
#endif
    }
    /*
     * Partial or multiple register access, loop thru the requested memory.
     */
    else
    {
        /*
         * If it's an access beyond the start of the register, shift the input
         * value and fill in missing bits. Natural alignment rules means we
         * will only see 1 or 2 byte accesses of this kind, so no risk of
         * shifting out input values.
         */
        if (idxRegDsc == -1 && (idxRegDsc = hdaRegLookupWithin(pThis, offReg)) != -1)
        {
            uint32_t const cbBefore = offReg - g_aHdaRegMap[idxRegDsc].offset; Assert(cbBefore > 0 && cbBefore < 4);
            offReg    -= cbBefore;
            idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
            u64Value <<= cbBefore * 8;
            u64Value  |= pThis->au32Regs[idxRegMem] & g_afMasks[cbBefore];
            LogFunc(("\tWithin register, supplied %u leading bits: %#llx -> %#llx ...\n",
                     cbBefore * 8, ~g_afMasks[cbBefore] & u64Value, u64Value));
        }

        /* Loop thru the write area, it may cover multiple registers. */
        rc = VINF_SUCCESS;
        for (;;)
        {
            uint32_t cbReg;
            if (idxRegDsc != -1)
            {
                idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
                cbReg = g_aHdaRegMap[idxRegDsc].size;
                if (cb < cbReg)
                {
                    u64Value |= pThis->au32Regs[idxRegMem] & g_afMasks[cbReg] & ~g_afMasks[cb];
                    LogFunc(("\tSupplying missing bits (%#x): %#llx -> %#llx ...\n",
                             g_afMasks[cbReg] & ~g_afMasks[cb], u64Value & g_afMasks[cb], u64Value));
                }
                uint32_t u32LogOldVal = pThis->au32Regs[idxRegMem];
                rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "*");
                LogFunc(("\t%#x -> %#x\n", u32LogOldVal, pThis->au32Regs[idxRegMem]));
            }
            else
            {
                LogRel(("HDA: Invalid write access @0x%x\n", offReg));
                cbReg = 1;
            }
            if (rc != VINF_SUCCESS)
                break;
            if (cbReg >= cb)
                break;

            /* Advance. */
            offReg += cbReg;
            cb     -= cbReg;
            u64Value >>= cbReg * 8;
            if (idxRegDsc == -1)
                idxRegDsc = hdaRegLookup(pThis, offReg);
            else
            {
                idxRegDsc++;
                if (   (unsigned)idxRegDsc >= RT_ELEMENTS(g_aHdaRegMap)
                    || g_aHdaRegMap[idxRegDsc].offset != offReg)
                {
                    idxRegDsc = -1;
                }
            }
        }
    }

    return rc;
}


/* PCI callback. */

#ifdef IN_RING3
/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) hdaPciIoRegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb,
                                           PCIADDRESSSPACE enmType)
{
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    PHDASTATE   pThis = RT_FROM_MEMBER(pPciDev, HDASTATE, PciDev);
    RTIOPORT    Port = (RTIOPORT)GCPhysAddress;
    int         rc;

    /*
     * 18.2 of the ICH6 datasheet defines the valid access widths as byte, word, and double word.
     *
     * Let IOM talk DWORDs when reading, saves a lot of complications. On
     * writing though, we have to do it all ourselves because of sideeffects.
     */
    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                 IOMMMIO_FLAGS_READ_DWORD
                               | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                               hdaMMIOWrite, hdaMMIORead, "HDA");

    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, NIL_RTR0PTR /*pvUser*/,
                                     "hdaMMIOWrite", "hdaMMIORead");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fRCEnabled)
    {
        rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/,
                                     "hdaMMIOWrite", "hdaMMIORead");
        if (RT_FAILURE(rc))
            return rc;
    }

    pThis->MMIOBaseAddr = GCPhysAddress;
    return VINF_SUCCESS;
}


/* Saved state callbacks. */

static int hdaSaveStream(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, PHDASTREAM pStrm)
{
    /* Save stream ID. */
    int rc = SSMR3PutU8(pSSM, pStrm->u8Strm);
    AssertRCReturn(rc, rc);
    Assert(pStrm->u8Strm <= 7); /** @todo Use a define. */

    rc = SSMR3PutStructEx(pSSM, &pStrm->State, sizeof(HDASTREAMSTATE), 0 /*fFlags*/, g_aSSMStreamStateFields5, NULL);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < pStrm->State.cBDLE; i++)
    {
        rc = SSMR3PutStructEx(pSSM, &pStrm->State.paBDLE[i], sizeof(HDABDLE), 0 /*fFlags*/, g_aSSMBDLEStateFields5, NULL);
        AssertRCReturn(rc, rc);
    }

    return rc;
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) hdaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    /* Save Codec nodes states. */
    hdaCodecSaveState(pThis->pCodec, pSSM);

    /* Save MMIO registers. */
    AssertCompile(RT_ELEMENTS(pThis->au32Regs) >= HDA_NREGS_SAVED);
    SSMR3PutU32(pSSM, RT_ELEMENTS(pThis->au32Regs));
    SSMR3PutMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));

    /* Save number of streams. */
    SSMR3PutU32(pSSM, 3);

    /* Save stream states. */
    int rc = hdaSaveStream(pDevIns, pSSM, &pThis->StrmStOut);
    AssertRCReturn(rc, rc);
    rc = hdaSaveStream(pDevIns, pSSM, &pThis->StrmStMicIn);
    AssertRCReturn(rc, rc);
    rc = hdaSaveStream(pDevIns, pSSM, &pThis->StrmStLineIn);
    AssertRCReturn(rc, rc);

    return rc;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) hdaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    LogFlowFunc(("uVersion=%RU32, uPass=%RU32\n", uVersion, uPass));

    /*
     * Load Codec nodes states.
     */
    int rc = hdaCodecLoadState(pThis->pCodec, pSSM, uVersion);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Load MMIO registers.
     */
    uint32_t cRegs;
    switch (uVersion)
    {
        case HDA_SSM_VERSION_1:
            /* Starting with r71199, we would save 112 instead of 113
               registers due to some code cleanups.  This only affected trunk
               builds in the 4.1 development period. */
            cRegs = 113;
            if (SSMR3HandleRevision(pSSM) >= 71199)
            {
                uint32_t uVer = SSMR3HandleVersion(pSSM);
                if (   VBOX_FULL_VERSION_GET_MAJOR(uVer) == 4
                    && VBOX_FULL_VERSION_GET_MINOR(uVer) == 0
                    && VBOX_FULL_VERSION_GET_BUILD(uVer) >= 51)
                    cRegs = 112;
            }
            break;

        case HDA_SSM_VERSION_2:
        case HDA_SSM_VERSION_3:
            cRegs = 112;
            AssertCompile(RT_ELEMENTS(pThis->au32Regs) >= HDA_NREGS_SAVED);
            break;

        /* Since version 4 we store the register count to stay flexible. */
        case HDA_SSM_VERSION_4:
        case HDA_SSM_VERSION:
            rc = SSMR3GetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
            if (cRegs != RT_ELEMENTS(pThis->au32Regs))
                LogRel(("HDA: SSM version cRegs is %RU32, expected %RU32\n", cRegs, RT_ELEMENTS(pThis->au32Regs)));
            break;

        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (cRegs >= RT_ELEMENTS(pThis->au32Regs))
    {
        SSMR3GetMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));
        SSMR3Skip(pSSM, sizeof(uint32_t) * (cRegs - RT_ELEMENTS(pThis->au32Regs)));
    }
    else
        SSMR3GetMem(pSSM, pThis->au32Regs, sizeof(uint32_t) * cRegs);

    /*
     * Note: Saved states < v5 store LVI (u32BdleMaxCvi) for
     *       *every* BDLE state, whereas it only needs to be stored
     *       *once* for every stream. Most of the BDLE state we can
     *       get out of the registers anyway, so just ignore those values.
     *
     *       Also, only the current BDLE was saved, regardless whether
     *       there were more than one (and there are at least two entries,
     *       according to the spec).
     */
#define HDA_SSM_LOAD_BDLE_STATE_PRE_V5(v, x)                        \
    rc = SSMR3Skip(pSSM, sizeof(uint32_t));    /* Begin marker */   \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3Skip(pSSM, sizeof(uint64_t));    /* u64BdleCviAddr */ \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3Skip(pSSM, sizeof(uint32_t));    /* u32BdleMaxCvi */  \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3GetU32(pSSM, &x->u32BDLIndex);   /* u32BdleCvi */     \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3Skip(pSSM, sizeof(uint32_t));    /* u32BdleCviLen */  \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3GetU32(pSSM, &x->u32BufOff);     /* u32BdleCviPos */  \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3Skip(pSSM, sizeof(uint8_t));     /* fBdleCviIoc */    \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3GetU32(pSSM, &x->cbBelowFIFOW);  /* cbUnderFifoW */   \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3GetMem(pSSM, &x->au8FIFO, sizeof(x->au8FIFO));        \
    AssertRCReturn(rc, rc);                                         \
    rc = SSMR3Skip(pSSM, sizeof(uint32_t));    /* End marker */     \
    AssertRCReturn(rc, rc);                                         \

    /*
     * Load BDLEs (Buffer Descriptor List Entries) and DMA counters.
     */
    HDABDLESTATE StateBDLEDummy;

    switch (uVersion)
    {
        case HDA_SSM_VERSION_1:
        case HDA_SSM_VERSION_2:
        case HDA_SSM_VERSION_3:
        case HDA_SSM_VERSION_4:
        {
            /* Only load the internal states.
             * The rest will be initialized from the saved registers later. */

            /* Note: Only the *current* BDLE for a stream was saved! */

            /* Output */
            rc = hdaStreamInit(pThis, &pThis->StrmStOut,    4 /* Stream number, hardcoded */);
            AssertRCBreak(rc);
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, (   pThis->StrmStOut.State.cBDLE
                                                      ? &pThis->StrmStOut.State.paBDLE[0].State : &StateBDLEDummy));
            /* Microphone-In */
            rc = hdaStreamInit(pThis, &pThis->StrmStMicIn,  2 /* Stream number, hardcoded */);
            AssertRCBreak(rc);
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, (   pThis->StrmStMicIn.State.cBDLE
                                                      ? &pThis->StrmStMicIn.State.paBDLE[0].State : &StateBDLEDummy));
            /* Line-In */
            rc = hdaStreamInit(pThis, &pThis->StrmStLineIn, 0 /* Stream number, hardcoded */);
            AssertRCBreak(rc);
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, (  pThis->StrmStLineIn.State.cBDLE
                                                      ? &pThis->StrmStLineIn.State.paBDLE[0].State : &StateBDLEDummy));
            break;
        }

        /* Since v5 we support flexible stream and BDLE counts. */
        case HDA_SSM_VERSION:
        {
            uint32_t cStreams;
            rc = SSMR3GetU32(pSSM, &cStreams);
            AssertRCBreak(rc);

            /* Load stream states. */
            for (uint32_t i = 0; i < cStreams; i++)
            {
                uint8_t uStreamID;
                rc = SSMR3GetU8(pSSM, &uStreamID);
                AssertRCBreak(rc);

                PHDASTREAM pStrm;
                HDASTREAM  StreamDummy;

                switch (uStreamID)
                {
                    case 0: /** @todo Use a define. */
                        pStrm = &pThis->StrmStLineIn;
                        break;

                    case 2: /** @todo Use a define. */
                        pStrm = &pThis->StrmStMicIn;
                        break;

                    case 4: /** @todo Use a define. */
                        pStrm = &pThis->StrmStOut;
                        break;

                    default:
                        pStrm = &StreamDummy;
                        break;
                }

                rc = SSMR3GetStructEx(pSSM, &pStrm->State, sizeof(HDASTREAMSTATE), 0 /* fFlags */, g_aSSMStreamStateFields5, NULL);
                AssertRCBreak(rc);

                rc = hdaStreamInit(pThis, pStrm, uStreamID);
                AssertRCBreak(rc);

                /* Load BDLE states. */
                for (uint32_t a = 0; a < pStrm->State.cBDLE; a++)
                {
                    rc = SSMR3GetStructEx(pSSM, &pStrm->State.paBDLE[a].State, sizeof(HDABDLESTATE),
                                          0 /* fFlags */, g_aSSMBDLEStateFields5, NULL);
                    AssertRCBreak(rc);
                }

                /* Destroy dummy again. */
                if (pStrm == &StreamDummy)
                    hdaStreamDestroy(pStrm);
            }
            break;
        }

        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

#undef HDA_SSM_LOAD_BDLE_STATE_PRE_V5

    if (RT_SUCCESS(rc))
    {
        /*
         * Update stuff after the state changes.
         */
        bool fEnableIn    = RT_BOOL(HDA_SDCTL(pThis, 0) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
#ifdef VBOX_WITH_HDA_MIC_IN
        bool fEnableMicIn = RT_BOOL(HDA_SDCTL(pThis, 2) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
#else
        bool fEnableMicIn = fEnableIn; /* Mic In == Line In */
#endif
        bool fEnableOut   = RT_BOOL(HDA_SDCTL(pThis, 4) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));

        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            rc = pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->LineIn.pStrmIn, fEnableIn);
            if (RT_FAILURE(rc))
                break;
            rc = pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->MicIn.pStrmIn, fEnableMicIn);
            if (RT_FAILURE(rc))
                break;
            rc = pDrv->pConnector->pfnEnableOut(pDrv->pConnector, pDrv->Out.pStrmOut, fEnableOut);
            if (RT_FAILURE(rc))
                break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pThis->u64CORBBase = RT_MAKE_U64(HDA_REG(pThis, CORBLBASE), HDA_REG(pThis, CORBUBASE));
        pThis->u64RIRBBase = RT_MAKE_U64(HDA_REG(pThis, RIRBLBASE), HDA_REG(pThis, RIRBUBASE));
        pThis->u64DPBase   = RT_MAKE_U64(HDA_REG(pThis, DPLBASE), HDA_REG(pThis, DPUBASE));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef DEBUG
/* Debug and log type formatters. */

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtBDLE(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                           const char *pszType, void const *pvValue,
                                           int cchWidth, int cchPrecision, unsigned fFlags,
                                           void *pvUser)
{
    PHDABDLE pBDLE = (PHDABDLE)pvValue;
    return RTStrFormat(pfnOutput,  pvArgOutput, NULL, 0,
                       "BDLE(idx:%RU32, off:%RU32, fifow:%RU32, DMA[%RU32 bytes @ 0x%x])",
                       pBDLE->State.u32BDLIndex, pBDLE->State.u32BufOff, pBDLE->State.cbBelowFIFOW, pBDLE->u32BufSize, pBDLE->u64BufAdr);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDCTL(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                           const char *pszType, void const *pvValue,
                                           int cchWidth, int cchPrecision, unsigned fFlags,
                                           void *pvUser)
{
    uint32_t uSDCTL = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDCTL(raw:%#x, DIR:%s, TP:%RTbool, STRIPE:%x, DEIE:%RTbool, FEIE:%RTbool, IOCE:%RTbool, RUN:%RTbool, RESET:%RTbool)",
                       uSDCTL,
                       (uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, DIR)) ? "OUT" : "IN",
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, TP)),
                       (uSDCTL & HDA_REG_FIELD_MASK(SDCTL, STRIPE)) >> HDA_SDCTL_STRIPE_SHIFT,
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, DEIE)),
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, FEIE)),
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE)),
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)),
                       RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST)));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDFIFOS(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                             const char *pszType, void const *pvValue,
                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                             void *pvUser)
{
    uint32_t uSDFIFOS = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOS(raw:%#x, sdfifos:%RU8 B)", uSDFIFOS, hdaSDFIFOSToBytes(uSDFIFOS));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDFIFOW(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                             const char *pszType, void const *pvValue,
                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                             void *pvUser)
{
    uint32_t uSDFIFOW = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOW(raw: %#0x, sdfifow:%d B)", uSDFIFOW, hdaSDFIFOWToBytes(uSDFIFOW));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDSTS(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                           const char *pszType, void const *pvValue,
                                           int cchWidth, int cchPrecision, unsigned fFlags,
                                           void *pvUser)
{
    uint32_t uSdSts = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDSTS(raw:%#0x, fifordy:%RTbool, dese:%RTbool, fifoe:%RTbool, bcis:%RTbool)",
                       uSdSts,
                       RT_BOOL(uSdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY)),
                       RT_BOOL(uSdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, DE)),
                       RT_BOOL(uSdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, FE)),
                       RT_BOOL(uSdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)));
}

static int hdaLookUpRegisterByName(PHDASTATE pThis, const char *pszArgs)
{
    int iReg = 0;
    for (; iReg < HDA_NREGS; ++iReg)
        if (!RTStrICmp(g_aHdaRegMap[iReg].abbrev, pszArgs))
            return iReg;
    return -1;
}


static void hdaDbgPrintRegister(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iHdaIndex)
{
    Assert(   pThis
           && iHdaIndex >= 0
           && iHdaIndex < HDA_NREGS);
    pHlp->pfnPrintf(pHlp, "%s: 0x%x\n", g_aHdaRegMap[iHdaIndex].abbrev, pThis->au32Regs[g_aHdaRegMap[iHdaIndex].mem_idx]);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int iHdaRegisterIndex = hdaLookUpRegisterByName(pThis, pszArgs);
    if (iHdaRegisterIndex != -1)
        hdaDbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
    else
        for(iHdaRegisterIndex = 0; (unsigned int)iHdaRegisterIndex < HDA_NREGS; ++iHdaRegisterIndex)
            hdaDbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
}

static void hdaDbgPrintStream(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iHdaStrmIndex)
{
    Assert(   pThis
           && iHdaStrmIndex >= 0
           && iHdaStrmIndex < 7);
    pHlp->pfnPrintf(pHlp, "Dump of %d HDA Stream:\n", iHdaStrmIndex);
    pHlp->pfnPrintf(pHlp, "SD%dCTL: %R[sdctl]\n", iHdaStrmIndex, HDA_STREAM_REG(pThis, CTL, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dCTS: %R[sdsts]\n", iHdaStrmIndex, HDA_STREAM_REG(pThis, STS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOS: %R[sdfifos]\n", iHdaStrmIndex, HDA_STREAM_REG(pThis, FIFOS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOW: %R[sdfifow]\n", iHdaStrmIndex, HDA_STREAM_REG(pThis, FIFOW, iHdaStrmIndex));
}

static int hdaLookUpStreamIndex(PHDASTATE pThis, const char *pszArgs)
{
    /* todo: add args parsing */
    return -1;
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoStream(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE   pThis         = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int         iHdaStrmIndex = hdaLookUpStreamIndex(pThis, pszArgs);
    if (iHdaStrmIndex != -1)
        hdaDbgPrintStream(pThis, pHlp, iHdaStrmIndex);
    else
        for(iHdaStrmIndex = 0; iHdaStrmIndex < 7; ++iHdaStrmIndex)
            hdaDbgPrintStream(pThis, pHlp, iHdaStrmIndex);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoCodecNodes(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    if (pThis->pCodec->pfnDbgListNodes)
        pThis->pCodec->pfnDbgListNodes(pThis->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback\n");
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoCodecSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    if (pThis->pCodec->pfnDbgSelector)
        pThis->pCodec->pfnDbgSelector(pThis->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback\n");
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoMixer(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    if (pThis->pMixer)
        AudioMixerDebug(pThis->pMixer, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Mixer not available\n");
}
#endif /* DEBUG */

/* PDMIBASE */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) hdaQueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PHDASTATE pThis = RT_FROM_MEMBER(pInterface, HDASTATE, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}


/* PDMDEVREG */

/**
 * Reset notification.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance data.
 *
 * @remark  The original sources didn't install a reset handler, but it seems to
 *          make sense to me so we'll do it.
 */
static DECLCALLBACK(void) hdaReset(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    HDA_REG(pThis, GCAP)     = HDA_MAKE_GCAP(4,4,0,0,1); /* see 6.2.1 */
    HDA_REG(pThis, VMIN)     = 0x00;                     /* see 6.2.2 */
    HDA_REG(pThis, VMAJ)     = 0x01;                     /* see 6.2.3 */
    HDA_REG(pThis, OUTPAY)   = 0x003C;                   /* see 6.2.4 */
    HDA_REG(pThis, INPAY)    = 0x001D;                   /* see 6.2.5 */
    HDA_REG(pThis, CORBSIZE) = 0x42;                     /* see 6.2.1 */
    HDA_REG(pThis, RIRBSIZE) = 0x42;                     /* see 6.2.1 */
    HDA_REG(pThis, CORBRP)   = 0x0;
    HDA_REG(pThis, RIRBWP)   = 0x0;

    LogFunc(("Resetting ...\n"));

    /* Stop any audio currently playing. */
    PHDADRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
    {
        pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->LineIn.pStrmIn, false /* Disable */);
        /* Ignore rc. */
        pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->MicIn.pStrmIn, false /* Disable */);
        /* Ditto. */
        pDrv->pConnector->pfnEnableOut(pDrv->pConnector, pDrv->Out.pStrmOut, false /* Disable */);
        /* Ditto. */
    }

    pThis->cbCorbBuf = 256 * sizeof(uint32_t); /** @todo Use a define here. */

    if (pThis->pu32CorbBuf)
        RT_BZERO(pThis->pu32CorbBuf, pThis->cbCorbBuf);
    else
        pThis->pu32CorbBuf = (uint32_t *)RTMemAllocZ(pThis->cbCorbBuf);

    pThis->cbRirbBuf = 256 * sizeof(uint64_t); /** @todo Use a define here. */
    if (pThis->pu64RirbBuf)
        RT_BZERO(pThis->pu64RirbBuf, pThis->cbRirbBuf);
    else
        pThis->pu64RirbBuf = (uint64_t *)RTMemAllocZ(pThis->cbRirbBuf);

    pThis->u64BaseTS = PDMDevHlpTMTimeVirtGetNano(pDevIns);

    for (uint8_t u8Strm = 0; u8Strm < 8; u8Strm++) /** @todo Use a define here. */
    {
        PHDASTREAM pStrmSt = NULL;
        if (u8Strm == 0)
            pStrmSt = &pThis->StrmStOut;
# ifdef VBOX_WITH_HDA_MIC_IN
        else if (u8Strm == 2)
            pStrmSt = &pThis->StrmStMicIn;
# endif
        else if (u8Strm == 4)
            pStrmSt = &pThis->StrmStLineIn;

        if (pStrmSt)
        {
            /* hdaStreamReset prevents changing the SRST bit, so we force it to zero here. */
            HDA_STREAM_REG(pThis, CTL, u8Strm) = 0;

            hdaStreamReset(pThis, pStrmSt, u8Strm);
        }
    }

    /* Emulation of codec "wake up" (HDA spec 5.5.1 and 6.5). */
    HDA_REG(pThis, STATESTS) = 0x1;

    LogRel(("HDA: Reset\n"));
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) hdaDestruct(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    PHDADRIVER pDrv;
    while (!RTListIsEmpty(&pThis->lstDrv))
    {
        pDrv = RTListGetFirst(&pThis->lstDrv, HDADRIVER, Node);

        RTListNodeRemove(&pDrv->Node);
        RTMemFree(pDrv);
    }

    if (pThis->pMixer)
    {
        AudioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }

    if (pThis->pCodec)
    {
        int rc = hdaCodecDestruct(pThis->pCodec);
        AssertRC(rc);

        RTMemFree(pThis->pCodec);
        pThis->pCodec = NULL;
    }

    RTMemFree(pThis->pu32CorbBuf);
    pThis->pu32CorbBuf = NULL;

    RTMemFree(pThis->pu64RirbBuf);
    pThis->pu64RirbBuf = NULL;

    hdaStreamDestroy(&pThis->StrmStLineIn);
    hdaStreamDestroy(&pThis->StrmStMicIn);
    hdaStreamDestroy(&pThis->StrmStOut);

    return VINF_SUCCESS;
}

/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor have to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int) hdaAttach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("HDA device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /*
     * Attach driver.
     */
    char *pszDesc = NULL;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (HDA) for LUN#%u", uLUN) <= 0)
        AssertMsgReturn(pszDesc,
                        ("Not enough memory for HDA driver port description of LUN #%u\n", uLUN),
                        VERR_NO_MEMORY);

    int rc = PDMDevHlpDriverAttach(pDevIns, uLUN,
                                   &pThis->IBase, &pThis->pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        PHDADRIVER pDrv = (PHDADRIVER)RTMemAllocZ(sizeof(HDADRIVER));
        if (pDrv)
        {
            pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL, ("Configuration error: LUN#%u has no host audio interface, rc=%Rrc\n", uLUN, rc));
            pDrv->pHDAState  = pThis;
            pDrv->uLUN       = uLUN;

            /*
             * For now we always set the driver at LUN 0 as our primary
             * host backend. This might change in the future.
             */
            if (pDrv->uLUN == 0)
                pDrv->Flags |= PDMAUDIODRVFLAG_PRIMARY;

            LogFunc(("LUN#%u: pCon=%p, drvFlags=0x%x\n", uLUN, pDrv->pConnector, pDrv->Flags));

            /* Attach to driver list. */
            RTListAppend(&pThis->lstDrv, &pDrv->Node);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (   rc == VERR_PDM_NO_ATTACHED_DRIVER
             || rc == VERR_PDM_CFG_MISSING_DRIVER_NAME)
    {
        LogFunc(("No attached driver for LUN #%u\n", uLUN));
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Failed to attach HDA LUN #%u (\"%s\"), rc=%Rrc\n",
                        uLUN, pszDesc, rc));

    RTStrFree(pszDesc);

    LogFunc(("uLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}

static DECLCALLBACK(void) hdaDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    NOREF(pDevIns); NOREF(iLUN); NOREF(fFlags);

    LogFlowFuncEnter();
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hdaConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validations.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "R0Enabled\0"
                                          "RCEnabled\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_ ("Invalid configuration for the Intel HDA device"));

    int rc = CFGMR3QueryBoolDef(pCfgHandle, "RCEnabled", &pThis->fRCEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read RCEnabled as boolean"));
    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &pThis->fR0Enabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read R0Enabled as boolean"));

    /*
     * Initialize data (most of it anyway).
     */
    pThis->pDevInsR3                = pDevIns;
    pThis->pDevInsR0                = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC                = PDMDEVINS_2_RCPTR(pDevIns);
    /* IBase */
    pThis->IBase.pfnQueryInterface  = hdaQueryInterface;

    /* PCI Device */
    PCIDevSetVendorId           (&pThis->PciDev, HDA_PCI_VENDOR_ID); /* nVidia */
    PCIDevSetDeviceId           (&pThis->PciDev, HDA_PCI_DEVICE_ID); /* HDA */

    PCIDevSetCommand            (&pThis->PciDev, 0x0000); /* 04 rw,ro - pcicmd. */
    PCIDevSetStatus             (&pThis->PciDev, VBOX_PCI_STATUS_CAP_LIST); /* 06 rwc?,ro? - pcists. */
    PCIDevSetRevisionId         (&pThis->PciDev, 0x01);   /* 08 ro - rid. */
    PCIDevSetClassProg          (&pThis->PciDev, 0x00);   /* 09 ro - pi. */
    PCIDevSetClassSub           (&pThis->PciDev, 0x03);   /* 0a ro - scc; 03 == HDA. */
    PCIDevSetClassBase          (&pThis->PciDev, 0x04);   /* 0b ro - bcc; 04 == multimedia. */
    PCIDevSetHeaderType         (&pThis->PciDev, 0x00);   /* 0e ro - headtyp. */
    PCIDevSetBaseAddress        (&pThis->PciDev, 0,       /* 10 rw - MMIO */
                                 false /* fIoSpace */, false /* fPrefetchable */, true /* f64Bit */, 0x00000000);
    PCIDevSetInterruptLine      (&pThis->PciDev, 0x00);   /* 3c rw. */
    PCIDevSetInterruptPin       (&pThis->PciDev, 0x01);   /* 3d ro - INTA#. */

#if defined(HDA_AS_PCI_EXPRESS)
    PCIDevSetCapabilityList     (&pThis->PciDev, 0x80);
#elif defined(VBOX_WITH_MSI_DEVICES)
    PCIDevSetCapabilityList     (&pThis->PciDev, 0x60);
#else
    PCIDevSetCapabilityList     (&pThis->PciDev, 0x50);   /* ICH6 datasheet 18.1.16 */
#endif

    /// @todo r=michaln: If there are really no PCIDevSetXx for these, the meaning
    /// of these values needs to be properly documented!
    /* HDCTL off 0x40 bit 0 selects signaling mode (1-HDA, 0 - Ac97) 18.1.19 */
    PCIDevSetByte(&pThis->PciDev, 0x40, 0x01);

    /* Power Management */
    PCIDevSetByte(&pThis->PciDev, 0x50 + 0, VBOX_PCI_CAP_ID_PM);
    PCIDevSetByte(&pThis->PciDev, 0x50 + 1, 0x0); /* next */
    PCIDevSetWord(&pThis->PciDev, 0x50 + 2, VBOX_PCI_PM_CAP_DSI | 0x02 /* version, PM1.1 */ );

#ifdef HDA_AS_PCI_EXPRESS
    /* PCI Express */
    PCIDevSetByte(&pThis->PciDev, 0x80 + 0, VBOX_PCI_CAP_ID_EXP); /* PCI_Express */
    PCIDevSetByte(&pThis->PciDev, 0x80 + 1, 0x60); /* next */
    /* Device flags */
    PCIDevSetWord(&pThis->PciDev, 0x80 + 2,
                   /* version */ 0x1     |
                   /* Root Complex Integrated Endpoint */ (VBOX_PCI_EXP_TYPE_ROOT_INT_EP << 4) |
                   /* MSI */ (100) << 9 );
    /* Device capabilities */
    PCIDevSetDWord(&pThis->PciDev, 0x80 + 4, VBOX_PCI_EXP_DEVCAP_FLRESET);
    /* Device control */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 8, 0);
    /* Device status */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 10, 0);
    /* Link caps */
    PCIDevSetDWord(&pThis->PciDev, 0x80 + 12, 0);
    /* Link control */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 16, 0);
    /* Link status */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 18, 0);
    /* Slot capabilities */
    PCIDevSetDWord(&pThis->PciDev, 0x80 + 20, 0);
    /* Slot control */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 24, 0);
    /* Slot status */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 26, 0);
    /* Root control */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 28, 0);
    /* Root capabilities */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 30, 0);
    /* Root status */
    PCIDevSetDWord(&pThis->PciDev, 0x80 + 32, 0);
    /* Device capabilities 2 */
    PCIDevSetDWord(&pThis->PciDev, 0x80 + 36, 0);
    /* Device control 2 */
    PCIDevSetQWord(&pThis->PciDev, 0x80 + 40, 0);
    /* Link control 2 */
    PCIDevSetQWord(&pThis->PciDev, 0x80 + 48, 0);
    /* Slot control 2 */
    PCIDevSetWord( &pThis->PciDev, 0x80 + 56, 0);
#endif

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 0x4000, PCI_ADDRESS_SPACE_MEM, hdaPciIoRegionMap);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_MSI_DEVICES
    PDMMSIREG MsiReg;
    RT_ZERO(MsiReg);
    MsiReg.cMsiVectors    = 1;
    MsiReg.iMsiCapOffset  = 0x60;
    MsiReg.iMsiNextOffset = 0x50;
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
    if (RT_FAILURE(rc))
    {
        /* That's OK, we can work without MSI */
        PCIDevSetCapabilityList(&pThis->PciDev, 0x50);
    }
#endif

    rc = PDMDevHlpSSMRegister(pDevIns, HDA_SSM_VERSION, sizeof(*pThis), hdaSaveExec, hdaLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    RTListInit(&pThis->lstDrv);

    uint8_t uLUN;
    for (uLUN = 0; uLUN < UINT8_MAX; uLUN)
    {
        LogFunc(("Trying to attach driver for LUN #%RU32 ...\n", uLUN));
        rc = hdaAttach(pDevIns, uLUN, PDM_TACH_FLAGS_NOT_HOT_PLUG);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
                rc = VINF_SUCCESS;

            break;
        }

        uLUN++;
    }

    LogFunc(("cLUNs=%RU8, rc=%Rrc\n", uLUN, rc));

    if (RT_SUCCESS(rc))
    {
        rc = AudioMixerCreate("HDA Mixer", 0 /* uFlags */, &pThis->pMixer);
        if (RT_SUCCESS(rc))
        {
            /* Set a default audio format for our mixer. */
            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.uHz           = 44100;
            streamCfg.cChannels     = 2;
            streamCfg.enmFormat     = AUD_FMT_S16;
            streamCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

            rc = AudioMixerSetDeviceFormat(pThis->pMixer, &streamCfg);
            AssertRC(rc);

            /* Add all required audio sinks. */
            rc = AudioMixerAddSink(pThis->pMixer, "[Playback] PCM Output",
                                   AUDMIXSINKDIR_OUTPUT, &pThis->pSinkOutput);
            AssertRC(rc);

            rc = AudioMixerAddSink(pThis->pMixer, "[Recording] Line In",
                                   AUDMIXSINKDIR_INPUT, &pThis->pSinkLineIn);
            AssertRC(rc);

            rc = AudioMixerAddSink(pThis->pMixer, "[Recording] Microphone In",
                                   AUDMIXSINKDIR_INPUT, &pThis->pSinkMicIn);
            AssertRC(rc);

            /* There is no master volume control. Set the master to max. */
            PDMAUDIOVOLUME vol = { false, 255, 255 };
            rc = AudioMixerSetMasterVolume(pThis->pMixer, &vol);
            AssertRC(rc);
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Construct codec. */
        pThis->pCodec = (PHDACODEC)RTMemAllocZ(sizeof(HDACODEC));
        if (!pThis->pCodec)
            return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("Out of memory allocating HDA codec state"));

        /* Audio driver callbacks for multiplexing. */
        pThis->pCodec->pfnCloseIn   = hdaCloseIn;
        pThis->pCodec->pfnCloseOut  = hdaCloseOut;
        pThis->pCodec->pfnOpenIn    = hdaOpenIn;
        pThis->pCodec->pfnOpenOut   = hdaOpenOut;
        pThis->pCodec->pfnReset     = hdaCodecReset;
        pThis->pCodec->pfnSetVolume = hdaSetVolume;

        pThis->pCodec->pHDAState = pThis; /* Assign HDA controller state to codec. */

        /* Construct the codec. */
        rc = hdaCodecConstruct(pDevIns, pThis->pCodec, 0 /* Codec index */, pCfgHandle);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);

        /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
           verb F20 should provide device/codec recognition. */
        Assert(pThis->pCodec->u16VendorId);
        Assert(pThis->pCodec->u16DeviceId);
        PCIDevSetSubSystemVendorId(&pThis->PciDev, pThis->pCodec->u16VendorId); /* 2c ro - intel.) */
        PCIDevSetSubSystemId(      &pThis->PciDev, pThis->pCodec->u16DeviceId); /* 2e ro. */
    }

    if (RT_SUCCESS(rc))
    {
        hdaReset(pDevIns);

        /*
         * 18.2.6,7 defines that values of this registers might be cleared on power on/reset
         * hdaReset shouldn't affects these registers.
         */
        HDA_REG(pThis, WAKEEN)   = 0x0;
        HDA_REG(pThis, STATESTS) = 0x0;

#ifdef DEBUG
        /*
         * Debug and string formatter types.
         */
        PDMDevHlpDBGFInfoRegister(pDevIns, "hda",         "HDA info. (hda [register case-insensitive])",    hdaInfo);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdastrm",     "HDA stream info. (hdastrm [stream number])",     hdaInfoStream);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdcnodes",    "HDA codec nodes.",                               hdaInfoCodecNodes);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdcselector", "HDA codec's selector states [node number].",     hdaInfoCodecSelector);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdamixer",    "HDA mixer state.",                               hdaInfoMixer);

        rc = RTStrFormatTypeRegister("bdle",    hdaDbgFmtBDLE,    NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("sdctl",   hdaDbgFmtSDCTL,   NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("sdsts",   hdaDbgFmtSDSTS,   NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("sdfifos", hdaDbgFmtSDFIFOS, NULL);
        AssertRC(rc);
        rc = RTStrFormatTypeRegister("sdfifow", hdaDbgFmtSDFIFOW, NULL);
        AssertRC(rc);
#endif /* DEBUG */

        /*
         * Some debug assertions.
         */
        for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
        {
            struct HDAREGDESC const *pReg     = &g_aHdaRegMap[i];
            struct HDAREGDESC const *pNextReg = i + 1 < RT_ELEMENTS(g_aHdaRegMap) ?  &g_aHdaRegMap[i + 1] : NULL;

            /* binary search order. */
            AssertReleaseMsg(!pNextReg || pReg->offset + pReg->size <= pNextReg->offset,
                             ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                              i, pReg->offset, pReg->size, i + 1, pNextReg->offset, pNextReg->size));

            /* alignment. */
            AssertReleaseMsg(   pReg->size == 1
                             || (pReg->size == 2 && (pReg->offset & 1) == 0)
                             || (pReg->size == 3 && (pReg->offset & 3) == 0)
                             || (pReg->size == 4 && (pReg->offset & 3) == 0),
                             ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));

            /* registers are packed into dwords - with 3 exceptions with gaps at the end of the dword. */
            AssertRelease(((pReg->offset + pReg->size) & 3) == 0 || pNextReg);
            if (pReg->offset & 3)
            {
                struct HDAREGDESC const *pPrevReg = i > 0 ?  &g_aHdaRegMap[i - 1] : NULL;
                AssertReleaseMsg(pPrevReg, ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
                if (pPrevReg)
                    AssertReleaseMsg(pPrevReg->offset + pPrevReg->size == pReg->offset,
                                     ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                                      i - 1, pPrevReg->offset, pPrevReg->size, i + 1, pReg->offset, pReg->size));
            }
#if 0
            if ((pReg->offset + pReg->size) & 3)
            {
                AssertReleaseMsg(pNextReg, ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
                if (pNextReg)
                    AssertReleaseMsg(pReg->offset + pReg->size == pNextReg->offset,
                                     ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                                      i, pReg->offset, pReg->size, i + 1,  pNextReg->offset, pNextReg->size));
            }
#endif
            /* The final entry is a full DWORD, no gaps! Allows shortcuts. */
            AssertReleaseMsg(pNextReg || ((pReg->offset + pReg->size) & 3) == 0,
                             ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
        }
    }

# ifndef VBOX_WITH_AUDIO_CALLBACKS
    if (RT_SUCCESS(rc))
    {
        /* Start the emulation timer. */
        rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, hdaTimer, pThis,
                                    TMTIMER_FLAGS_NO_CRIT_SECT, "DevIchHda", &pThis->pTimer);
        AssertRCReturn(rc, rc);

        if (RT_SUCCESS(rc))
        {
            /** @todo Investigate why sounds is getting corrupted if the "ticks" value is too
             *        low, e.g. "PDMDevHlpTMTimeVirtGetFreq / 200". */
            pThis->uTimerTicks = PDMDevHlpTMTimeVirtGetFreq(pDevIns) / 500; /** @todo Make this configurable! */
            pThis->uTimerTS    = PDMDevHlpTMTimeVirtGet(pDevIns);
            if (pThis->uTimerTicks < 100)
                pThis->uTimerTicks = 100;
            LogFunc(("Timer ticks=%RU64\n", pThis->uTimerTicks));

            /* Fire off timer. */
            TMTimerSet(pThis->pTimer, TMTimerGet(pThis->pTimer) + pThis->uTimerTicks);
        }
    }
# else
    if (RT_SUCCESS(rc))
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            /* Only register primary driver.
             * The device emulation does the output multiplexing then. */
            if (pDrv->Flags != PDMAUDIODRVFLAG_PRIMARY)
                continue;

            PDMAUDIOCALLBACK AudioCallbacks[2];

            HDACALLBACKCTX Ctx = { pThis, pDrv };

            AudioCallbacks[0].enmType     = PDMAUDIOCALLBACKTYPE_INPUT;
            AudioCallbacks[0].pfnCallback = hdaCallbackInput;
            AudioCallbacks[0].pvCtx       = &Ctx;
            AudioCallbacks[0].cbCtx       = sizeof(HDACALLBACKCTX);

            AudioCallbacks[1].enmType     = PDMAUDIOCALLBACKTYPE_OUTPUT;
            AudioCallbacks[1].pfnCallback = hdaCallbackOutput;
            AudioCallbacks[1].pvCtx       = &Ctx;
            AudioCallbacks[1].cbCtx       = sizeof(HDACALLBACKCTX);

            rc = pDrv->pConnector->pfnRegisterCallbacks(pDrv->pConnector, AudioCallbacks, RT_ELEMENTS(AudioCallbacks));
            if (RT_FAILURE(rc))
                break;
        }
    }
# endif

# ifdef VBOX_WITH_STATISTICS
    if (RT_SUCCESS(rc))
    {
        /*
         * Register statistics.
         */
#  ifndef VBOX_WITH_AUDIO_CALLBACKS
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTimer,            STAMTYPE_PROFILE, "/Devices/HDA/Timer",             STAMUNIT_TICKS_PER_CALL, "Profiling hdaTimer.");
#  endif
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead,        STAMTYPE_COUNTER, "/Devices/HDA/BytesRead"   ,      STAMUNIT_BYTES,          "Bytes read from HDA emulation.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesWritten,     STAMTYPE_COUNTER, "/Devices/HDA/BytesWritten",      STAMUNIT_BYTES,          "Bytes written to HDA emulation.");
    }
# endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceICH6_HDA =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "hda",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel HD Audio Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(HDASTATE),
    /* pfnConstruct */
    hdaConstruct,
    /* pfnDestruct */
    hdaDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    hdaReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
