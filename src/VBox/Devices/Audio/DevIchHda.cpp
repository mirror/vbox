/* $Id$ */
/** @file
 * DevIchHda - VBox ICH Intel HD Audio Controller.
 *
 * Implemented against the specifications found in "High Definition Audio
 * Specification", Revision 1.0a June 17, 2010, and  "Intel I/O Controller
 * HUB 6 (ICH6) Family, Datasheet", document number 301473-002.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/vmm/pdmdev.h>
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
#include <VBox/vmm/pdmaudioifs.h>
#endif
#include <VBox/version.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>
#ifdef IN_RING3
# include <iprt/uuid.h>
# include <iprt/string.h>
# include <iprt/mem.h>
#endif

#include "VBoxDD.h"

#ifndef VBOX_WITH_PDM_AUDIO_DRIVER
extern "C" {
#include "audio.h"
}
#endif
#include "DevIchHdaCodec.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
//#define HDA_AS_PCI_EXPRESS
#define VBOX_WITH_INTEL_HDA

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
#define DPBASE_ENABLED              1
#define DPBASE_ADDR_MASK            (~(uint64_t)0x7f)

#define HDA_STREAM_REG_DEF(name, num)           (HDA_REG_SD##num##name)
#define HDA_STREAM_RMX_DEF(name, num)           (HDA_RMX_SD##num##name)
/* Note: sdnum here _MUST_ be stream reg number [0,7] */
#define HDA_STREAM_REG(pThis, name, sdnum)     (HDA_REG_IND((pThis), HDA_REG_SD0##name + (sdnum) * 10))

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
#define SDCTL(pThis, num)           HDA_REG((pThis), SD(CTL, num))
#define SDCTL_NUM(pThis, num)       ((SDCTL((pThis), num) & HDA_REG_FIELD_MASK(SDCTL,NUM)) >> HDA_REG_FIELD_SHIFT(SDCTL, NUM))
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
 * ICH6 datasheet defined limits for FIFOW values (18.2.38)
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
#define HDA_SDONFIFO_16B            0x0F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_32B            0x1F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_64B            0x3F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_128B           0x7F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_192B           0xBF /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_256B           0xFF /* 20-, 24-bit Output Streams */
#define HDA_SDINFIFO_120B           0x77 /* 8-, 16-, 20-, 24-, 32-bit Input Streams */
#define HDA_SDINFIFO_160B           0x9F /* 20-, 24-bit Input Streams Streams */
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


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct HDABDLEDESC
{
    uint64_t    u64BdleCviAddr;
    uint32_t    u32BdleMaxCvi;
    uint32_t    u32BdleCvi;
    uint32_t    u32BdleCviLen;
    uint32_t    u32BdleCviPos;
    bool        fBdleCviIoc;
    uint32_t    cbUnderFifoW;
    uint8_t     au8HdaBuffer[HDA_SDONFIFO_256B + 1];
} HDABDLEDESC, *PHDABDLEDESC;

typedef struct HDASTREAMTRANSFERDESC
{
    uint64_t u64BaseDMA;
    uint32_t u32Ctl;
    uint32_t *pu32Sts;
    uint8_t  u8Strm;
    uint32_t *pu32Lpib;
    uint32_t u32Cbl;
    uint32_t u32Fifos;
} HDASTREAMTRANSFERDESC, *PHDASTREAMTRANSFERDESC;

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

    uint32_t                           u32Padding;

    /** Pointer to the connector of the attached audio driver. */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pDrv[2];
#else
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pDrv;
#endif
    /** Pointer to the attached audio driver. */
    R3PTRTYPE(PPDMIBASE)               pDrvBase;
    /** The base interface for LUN\#0. */
    PDMIBASE                           IBase;
    RTGCPHYS                           MMIOBaseAddr;
    uint32_t                           au32Regs[HDA_NREGS];
    HDABDLEDESC                        StInBdle;
    HDABDLEDESC                        StOutBdle;
    HDABDLEDESC                        StMicBdle;
    uint64_t                           u64CORBBase;
    uint64_t                           u64RIRBBase;
    uint64_t                           u64DPBase;
    /** pointer to CORB buf */
    R3PTRTYPE(uint32_t *)              pu32CorbBuf;
    /** size in bytes of CORB buf */
    uint32_t                           cbCorbBuf;
    uint32_t                           u32Padding2;
    /** pointer on RIRB buf */
    R3PTRTYPE(uint64_t *)              pu64RirbBuf;
    /** size in bytes of RIRB buf */
    uint32_t                           cbRirbBuf;
    /** indicates if HDA in reset. */
    bool                               fInReset;
    /** Interrupt on completion */
    bool                               fCviIoc;
    /** Flag whether the R0 part is enabled. */
    bool                               fR0Enabled;
    /** Flag whether the RC part is enabled. */
    bool                               fRCEnabled;
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    /** The HDA codec state. */
    R3PTRTYPE(PHDACODEC)               pCodec[2];
#else
    R3PTRTYPE(PHDACODEC)               pCodec;
#endif
    uint64_t                           u64BaseTS;
    /** 1.2.3.4.5.6.7. - someone please tell me what I'm counting! - .8.9.10... */
    uint8_t                            u8Counter;
    uint8_t                            au8Padding[7];
} HDASTATE;
/** Pointer to the ICH Intel HD Audio Controller state. */
typedef HDASTATE *PHDASTATE;

#define ISD0FMT_TO_AUDIO_SELECTOR(pThis) \
    ( AUDIO_FORMAT_SELECTOR((pThis)->pCodec, In, SDFMT_BASE_RATE(pThis, 0), SDFMT_MULT(pThis, 0), SDFMT_DIV(pThis, 0)) )
#define OSD0FMT_TO_AUDIO_SELECTOR(pThis) \
    ( AUDIO_FORMAT_SELECTOR((pThis)->pCodec, Out, SDFMT_BASE_RATE(pThis, 4), SDFMT_MULT(pThis, 4), SDFMT_DIV(pThis, 4)) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE
static FNPDMDEVRESET hdaReset;

static int hdaRegReadUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteCORBWP(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegWriteRIRBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadIRS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);

static int hdaRegWriteSDSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDLVI(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFIFOW(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDBDPL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSDBDPU(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteBase(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadU32(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU24(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU24(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU16(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU16(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadU8(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU8(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);

#ifdef IN_RING3
DECLINLINE(void) hdaInitTransferDescriptor(PHDASTATE pThis, PHDABDLEDESC pBdle, uint8_t u8Strm,
                                           PHDASTREAMTRANSFERDESC pStreamDesc);
static void hdaFetchBdle(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc);
#ifdef LOG_ENABLED
static void dump_bd(PHDASTATE pThis, PHDABDLEDESC pBdle, uint64_t u64BaseDMA);
#endif
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

/* see 302349 p 6.2*/
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
} g_aHdaRegMap[HDA_NREGS] =

/* Turn a short register name into an memory index and a stringized name. */
#define RA(abbrev)  HDA_MEM_IND_NAME(abbrev), #abbrev
/* Same as above for an input stream ('I' prefixed). */
#define IA(abbrev)  HDA_MEM_IND_NAME(abbrev), "I"#abbrev
/* Same as above for an output stream ('O' prefixed). */
#define OA(abbrev)  HDA_MEM_IND_NAME(abbrev), "O"#abbrev
/* Same as above for a register *not* stored in memory. */
#define UA(abbrev)  0, #abbrev

{
    /* offset  size     read mask   write mask         read callback         write callback         abbrev     */
    /*-------  -------  ----------  ----------  -----------------------  ------------------------ ----------   */
    { 0x00000, 0x00002, 0x0000FFFB, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , RA(GCAP)      }, /* Global Capabilities */
    { 0x00002, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , RA(VMIN)      }, /* Minor Version */
    { 0x00003, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , RA(VMAJ)      }, /* Major Version */
    { 0x00004, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , RA(OUTPAY)    }, /* Output Payload Capabilities */
    { 0x00006, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , RA(INPAY)     }, /* Input Payload Capabilities */
    { 0x00008, 0x00004, 0x00000103, 0x00000103, hdaRegReadU32          , hdaRegWriteGCTL       , RA(GCTL)      }, /* Global Control */
    { 0x0000c, 0x00002, 0x00007FFF, 0x00007FFF, hdaRegReadU16          , hdaRegWriteU16        , RA(WAKEEN)    }, /* Wake Enable */
    { 0x0000e, 0x00002, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteSTATESTS   , RA(STATESTS)  }, /* State Change Status */
    { 0x00010, 0x00002, 0xFFFFFFFF, 0x00000000, hdaRegReadUnimpl       , hdaRegWriteUnimpl     , RA(GSTS)      }, /* Global Status */
    { 0x00018, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , RA(OUTSTRMPAY)}, /* Output Stream Payload Capability */
    { 0x0001A, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimpl     , RA(INSTRMPAY) }, /* Input Stream Payload Capability */
    { 0x00020, 0x00004, 0xC00000FF, 0xC00000FF, hdaRegReadU32          , hdaRegWriteU32        , RA(INTCTL)    }, /* Interrupt Control */
    { 0x00024, 0x00004, 0xC00000FF, 0x00000000, hdaRegReadINTSTS       , hdaRegWriteUnimpl     , RA(INTSTS)    }, /* Interrupt Status */
    { 0x00030, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadWALCLK       , hdaRegWriteUnimpl     , UA(WALCLK)    }, /* Wall Clock Counter */
    /// @todo r=michaln: Doesn't the SSYNC register need to actually stop the stream(s)?
    { 0x00034, 0x00004, 0x000000FF, 0x000000FF, hdaRegReadU32          , hdaRegWriteU32        , RA(SSYNC)     }, /* Stream Synchronization */
    { 0x00040, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase       , RA(CORBLBASE) }, /* CORB Lower Base Address */
    { 0x00044, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , RA(CORBUBASE) }, /* CORB Upper Base Address */
    { 0x00048, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteCORBWP     , RA(CORBWP)    }, /* CORB Write Pointer */
    { 0x0004A, 0x00002, 0x000080FF, 0x000080FF, hdaRegReadU16          , hdaRegWriteCORBRP     , RA(CORBRP)    }, /* CORB Read Pointer */
    { 0x0004C, 0x00001, 0x00000003, 0x00000003, hdaRegReadU8           , hdaRegWriteCORBCTL    , RA(CORBCTL)   }, /* CORB Control */
    { 0x0004D, 0x00001, 0x00000001, 0x00000001, hdaRegReadU8           , hdaRegWriteCORBSTS    , RA(CORBSTS)   }, /* CORB Status */
    { 0x0004E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , RA(CORBSIZE)  }, /* CORB Size */
    { 0x00050, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase       , RA(RIRBLBASE) }, /* RIRB Lower Base Address */
    { 0x00054, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , RA(RIRBUBASE) }, /* RIRB Upper Base Address */
    { 0x00058, 0x00002, 0x000000FF, 0x00008000, hdaRegReadU8           , hdaRegWriteRIRBWP     , RA(RIRBWP)    }, /* RIRB Write Pointer */
    { 0x0005A, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteU16        , RA(RINTCNT)   }, /* Response Interrupt Count */
    { 0x0005C, 0x00001, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteU8         , RA(RIRBCTL)   }, /* RIRB Control */
    { 0x0005D, 0x00001, 0x00000005, 0x00000005, hdaRegReadU8           , hdaRegWriteRIRBSTS    , RA(RIRBSTS)   }, /* RIRB Status */
    { 0x0005E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimpl     , RA(RIRBSIZE)  }, /* RIRB Size */
    { 0x00060, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , RA(IC)        }, /* Immediate Command */
    { 0x00064, 0x00004, 0x00000000, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteUnimpl     , RA(IR)        }, /* Immediate Response */
    { 0x00068, 0x00002, 0x00000002, 0x00000002, hdaRegReadIRS          , hdaRegWriteIRS        , RA(IRS)       }, /* Immediate Command Status */
    { 0x00070, 0x00004, 0xFFFFFFFF, 0xFFFFFF81, hdaRegReadU32          , hdaRegWriteBase       , RA(DPLBASE)   }, /* MA Position Lower Base */
    { 0x00074, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase       , RA(DPUBASE)   }, /* DMA Position Upper Base */

    { 0x00080, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , IA(SD0CTL)    }, /* Input Stream Descriptor 0 (ICD0) Control */
    { 0x00083, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , IA(SD0STS)    }, /* ISD0 Status */
    { 0x00084, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , IA(SD0LPIB)   }, /* ISD0 Link Position In Buffer */
    { 0x00088, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , IA(SD0CBL)    }, /* ISD0 Cyclic Buffer Length */
    { 0x0008C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , IA(SD0LVI)    }, /* ISD0 Last Valid Index */
    { 0x0008E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , IA(SD0FIFOW)  }, /* ISD0 FIFO Watermark */
    { 0x00090, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16        , IA(SD0FIFOS)  }, /* ISD0 FIFO Size */
    { 0x00092, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , IA(SD0FMT)    }, /* ISD0 Format */
    { 0x00098, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , IA(SD0BDPL)   }, /* ISD0 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x0009C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , IA(SD0BDPU)   }, /* ISD0 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x000A0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , IA(SD1CTL)    }, /* Input Stream Descriptor 1 (ISD1) Control */
    { 0x000A3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , IA(SD1STS)    }, /* ISD1 Status */
    { 0x000A4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , IA(SD1LPIB)   }, /* ISD1 Link Position In Buffer */
    { 0x000A8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , IA(SD1CBL)    }, /* ISD1 Cyclic Buffer Length */
    { 0x000AC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , IA(SD1LVI)    }, /* ISD1 Last Valid Index */
    { 0x000AE, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , IA(SD1FIFOW)  }, /* ISD1 FIFO Watermark */
    { 0x000B0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16        , IA(SD1FIFOS)  }, /* ISD1 FIFO Size */
    { 0x000B2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , IA(SD1FMT)    }, /* ISD1 Format */
    { 0x000B8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , IA(SD1BDPL)   }, /* ISD1 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x000BC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , IA(SD1BDPU)   }, /* ISD1 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x000C0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , IA(SD2CTL)    }, /* Input Stream Descriptor 2 (ISD2) Control */
    { 0x000C3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , IA(SD2STS)    }, /* ISD2 Status */
    { 0x000C4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , IA(SD2LPIB)   }, /* ISD2 Link Position In Buffer */
    { 0x000C8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , IA(SD2CBL)    }, /* ISD2 Cyclic Buffer Length */
    { 0x000CC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , IA(SD2LVI)    }, /* ISD2 Last Valid Index */
    { 0x000CE, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , IA(SD2FIFOW)  }, /* ISD2 FIFO Watermark */
    { 0x000D0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16        , IA(SD2FIFOS)  }, /* ISD2 FIFO Size */
    { 0x000D2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , IA(SD2FMT)    }, /* ISD2 Format */
    { 0x000D8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , IA(SD2BDPL)   }, /* ISD2 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x000DC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , IA(SD2BDPU)   }, /* ISD2 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x000E0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , IA(SD3CTL)    }, /* Input Stream Descriptor 3 (ISD3) Control */
    { 0x000E3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , IA(SD3STS)    }, /* ISD3 Status */
    { 0x000E4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , IA(SD3LPIB)   }, /* ISD3 Link Position In Buffer */
    { 0x000E8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , IA(SD3CBL)    }, /* ISD3 Cyclic Buffer Length */
    { 0x000EC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , IA(SD3LVI)    }, /* ISD3 Last Valid Index */
    { 0x000EE, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , IA(SD3FIFOW)  }, /* ISD3 FIFO Watermark */
    { 0x000F0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16        , IA(SD3FIFOS)  }, /* ISD3 FIFO Size */
    { 0x000F2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , IA(SD3FMT)    }, /* ISD3 Format */
    { 0x000F8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , IA(SD3BDPL)   }, /* ISD3 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x000FC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , IA(SD3BDPU)   }, /* ISD3 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x00100, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , OA(SD4CTL)    }, /* Output Stream Descriptor 0 (OSD0) Control */
    { 0x00103, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , OA(SD4STS)    }, /* OSD0 Status */
    { 0x00104, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , OA(SD4LPIB)   }, /* OSD0 Link Position In Buffer */
    { 0x00108, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , OA(SD4CBL)    }, /* OSD0 Cyclic Buffer Length */
    { 0x0010C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , OA(SD4LVI)    }, /* OSD0 Last Valid Index */
    { 0x0010E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , OA(SD4FIFOW)  }, /* OSD0 FIFO Watermark */
    { 0x00110, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS    , OA(SD4FIFOS)  }, /* OSD0 FIFO Size */
    { 0x00112, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , OA(SD4FMT)    }, /* OSD0 Format */
    { 0x00118, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , OA(SD4BDPL)   }, /* OSD0 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x0011C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , OA(SD4BDPU)   }, /* OSD0 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x00120, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , OA(SD5CTL)    }, /* Output Stream Descriptor 0 (OSD1) Control */
    { 0x00123, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , OA(SD5STS)    }, /* OSD1 Status */
    { 0x00124, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , OA(SD5LPIB)   }, /* OSD1 Link Position In Buffer */
    { 0x00128, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , OA(SD5CBL)    }, /* OSD1 Cyclic Buffer Length */
    { 0x0012C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , OA(SD5LVI)    }, /* OSD1 Last Valid Index */
    { 0x0012E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , OA(SD5FIFOW)  }, /* OSD1 FIFO Watermark */
    { 0x00130, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS    , OA(SD5FIFOS)  }, /* OSD1 FIFO Size */
    { 0x00132, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , OA(SD5FMT)    }, /* OSD1 Format */
    { 0x00138, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , OA(SD5BDPL)   }, /* OSD1 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x0013C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , OA(SD5BDPU)   }, /* OSD1 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x00140, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , OA(SD6CTL)    }, /* Output Stream Descriptor 0 (OSD2) Control */
    { 0x00143, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , OA(SD6STS)    }, /* OSD2 Status */
    { 0x00144, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , OA(SD6LPIB)   }, /* OSD2 Link Position In Buffer */
    { 0x00148, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , OA(SD6CBL)    }, /* OSD2 Cyclic Buffer Length */
    { 0x0014C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , OA(SD6LVI)    }, /* OSD2 Last Valid Index */
    { 0x0014E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , OA(SD6FIFOW)  }, /* OSD2 FIFO Watermark */
    { 0x00150, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS    , OA(SD6FIFOS)  }, /* OSD2 FIFO Size */
    { 0x00152, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , OA(SD6FMT)    }, /* OSD2 Format */
    { 0x00158, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , OA(SD6BDPL)   }, /* OSD2 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x0015C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , OA(SD6BDPU)   }, /* OSD2 Buffer Descriptor List Pointer-Upper Base Address */

    { 0x00160, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL      , OA(SD7CTL)    }, /* Output Stream Descriptor 0 (OSD3) Control */
    { 0x00163, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS      , OA(SD7STS)    }, /* OSD3 Status */
    { 0x00164, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32        , OA(SD7LPIB)   }, /* OSD3 Link Position In Buffer */
    { 0x00168, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32        , OA(SD7CBL)    }, /* OSD3 Cyclic Buffer Length */
    { 0x0016C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI      , OA(SD7LVI)    }, /* OSD3 Last Valid Index */
    { 0x0016E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW    , OA(SD7FIFOW)  }, /* OSD3 FIFO Watermark */
    { 0x00170, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS    , OA(SD7FIFOS)  }, /* OSD3 FIFO Size */
    { 0x00172, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT      , OA(SD7FMT)    }, /* OSD3 Format */
    { 0x00178, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL     , OA(SD7BDPL)   }, /* OSD3 Buffer Descriptor List Pointer-Lower Base Address */
    { 0x0017C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU     , OA(SD7BDPU)   }, /* OSD3 Buffer Descriptor List Pointer-Upper Base Address */
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
/** HDABDLEDESC field descriptors the v3+ saved state. */
static SSMFIELD const g_aHdaBDLEDescFields[] =
{
    SSMFIELD_ENTRY(     HDABDLEDESC, u64BdleCviAddr),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleMaxCvi),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCvi),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCviLen),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCviPos),
    SSMFIELD_ENTRY(     HDABDLEDESC, fBdleCviIoc),
    SSMFIELD_ENTRY(     HDABDLEDESC, cbUnderFifoW),
    SSMFIELD_ENTRY(     HDABDLEDESC, au8HdaBuffer),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLEDESC field descriptors the v1 and v2 saved state. */
static SSMFIELD const g_aHdaBDLEDescFieldsOld[] =
{
    SSMFIELD_ENTRY(     HDABDLEDESC, u64BdleCviAddr),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleMaxCvi),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCvi),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCviLen),
    SSMFIELD_ENTRY(     HDABDLEDESC, u32BdleCviPos),
    SSMFIELD_ENTRY(     HDABDLEDESC, fBdleCviIoc),
    SSMFIELD_ENTRY_PAD_HC_AUTO(3, 3),
    SSMFIELD_ENTRY(     HDABDLEDESC, cbUnderFifoW),
    SSMFIELD_ENTRY(     HDABDLEDESC, au8HdaBuffer),
    SSMFIELD_ENTRY_TERM()
};
#endif

/**
 * 32-bit size indexed masks, i.e. g_afMasks[2 bytes] = 0xffff.
 */
static uint32_t const   g_afMasks[5] =
{
    UINT32_C(0), UINT32_C(0x000000ff), UINT32_C(0x0000ffff), UINT32_C(0x00ffffff), UINT32_C(0xffffffff)
};

#ifdef IN_RING3
DECLINLINE(void) hdaUpdatePosBuf(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    if (pThis->u64DPBase & DPBASE_ENABLED)
        PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns),
                              (pThis->u64DPBase & DPBASE_ADDR_MASK) + pStreamDesc->u8Strm * 8,
                              pStreamDesc->pu32Lpib, sizeof(uint32_t));
}
#endif

DECLINLINE(uint32_t) hdaFifoWToSz(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc)
{
#if 0
    switch(HDA_STREAM_REG(pThis, FIFOW, pStreamDesc->u8Strm))
    {
        case HDA_SDFIFOW_8B: return 8;
        case HDA_SDFIFOW_16B: return 16;
        case HDA_SDFIFOW_32B: return 32;
        default:
            AssertMsgFailed(("hda: unsupported value (%x) in SDFIFOW(,%d)\n", HDA_REG_IND(pThis, pStreamDesc->u8Strm), pStreamDesc->u8Strm));
    }
#endif
    return 0;
}

static int hdaProcessInterrupt(PHDASTATE pThis)
{
#define IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, num) \
        (   INTCTL_SX((pThis), num) \
         && (SDSTS(pThis, num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))
    bool fIrq = false;
    if (   HDA_REG_FLAG_VALUE(pThis, INTCTL, CIE)
       && (   HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RINTFL)
           || HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RIRBOIS)
           || (HDA_REG(pThis, STATESTS) & HDA_REG(pThis, WAKEEN))))
        fIrq = true;

    if (   IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 0)
        || IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 4))
        fIrq = true;

    if (HDA_REG_FLAG_VALUE(pThis, INTCTL, GIE))
    {
        Log(("hda: irq %s\n", fIrq ? "asserted" : "deasserted"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0 , fIrq);
    }
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
            Log(("hda: corb%02x: ", i));
            uint8_t j = 0;
            do
            {
                const char *prefix;
                if ((i + j) == HDA_REG(pThis, CORBRP);
                    prefix = "[R]";
                else if ((i + j) == HDA_REG(pThis, CORBWP);
                    prefix = "[W]";
                else
                    prefix = "   "; /* three spaces */
                Log(("%s%08x", prefix, pThis->pu32CorbBuf[i + j]));
                j++;
            } while (j < 8);
            Log(("\n"));
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
            Log(("hda: rirb%02x: ", i));
            uint8_t j = 0;
            do {
                const char *prefix;
                if ((i + j) == HDA_REG(pThis, RIRBWP))
                    prefix = "[W]";
                else
                    prefix = "   ";
                Log((" %s%016lx", prefix, pThis->pu64RirbBuf[i + j]));
            } while (++j < 8);
            Log(("\n"));
            i += 8;
        } while (i != 0);
#endif
    }
    return rc;
}

static int hdaCORBCmdProcess(PHDASTATE pThis)
{
    int rc;
    uint8_t corbRp;
    uint8_t corbWp;
    uint8_t rirbWp;

    PFNHDACODECVERBPROCESSOR pfn = (PFNHDACODECVERBPROCESSOR)NULL;

    rc = hdaCmdSync(pThis, true);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    corbRp = HDA_REG(pThis, CORBRP);
    corbWp = HDA_REG(pThis, CORBWP);
    rirbWp = HDA_REG(pThis, RIRBWP);
    Assert((corbWp != corbRp));
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP),
         HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));
    while (corbRp != corbWp)
    {
        uint32_t cmd;
        uint64_t resp;
        pfn = NULL;
        corbRp++;
        cmd = pThis->pu32CorbBuf[corbRp];
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        for (uint32_t lun = 0; lun < 1; lun++)
            rc = pThis->pCodec[lun]->pfnLookup(pThis->pCodec[lun], cmd, &pfn);
#else
        rc = pThis->pCodec->pfnLookup(pThis->pCodec, cmd, &pfn);
#endif
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Assert(pfn);
        (rirbWp)++;

        if (RT_LIKELY(pfn))
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        {
            for (uint32_t lun = 0; lun < 1; lun++)
                rc = pfn(pThis->pCodec[lun], cmd, &resp);
        }
#else
            rc = pfn(pThis->pCodec, cmd, &resp);
#endif
        else
            rc = VERR_INVALID_FUNCTION;

        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Log(("hda: verb:%08x->%016lx\n", cmd, resp));
        if (   (resp & CODEC_RESPONSE_UNSOLICITED)
            && !HDA_REG_FLAG_VALUE(pThis, GCTL, UR))
        {
            Log(("hda: unexpected unsolicited response.\n"));
            HDA_REG(pThis, CORBRP) = corbRp;
            return rc;
        }
        pThis->pu64RirbBuf[rirbWp] = resp;
        pThis->u8Counter++;
        if (pThis->u8Counter == RINTCNT_N(pThis))
            break;
    }
    HDA_REG(pThis, CORBRP) = corbRp;
    HDA_REG(pThis, RIRBWP) = rirbWp;
    rc = hdaCmdSync(pThis, false);
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP),
         HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));
    if (HDA_REG_FLAG_VALUE(pThis, RIRBCTL, RIC))
    {
        HDA_REG(pThis, RIRBSTS) |= HDA_REG_FIELD_FLAG_MASK(RIRBSTS,RINTFL);
        pThis->u8Counter = 0;
        rc = hdaProcessInterrupt(pThis);
    }
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    return rc;
}
#endif

static void hdaStreamReset(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc, uint8_t u8Strm)
{
    Log(("hda: reset of stream (%d) started\n", u8Strm));
    Assert((   pThis
            && pBdle
            && pStreamDesc
            && u8Strm <= 7));
    memset(pBdle, 0, sizeof(HDABDLEDESC));
    *pStreamDesc->pu32Lpib = 0;
    *pStreamDesc->pu32Sts = 0;
    /* According to the ICH6 datasheet, 0x40000 is the default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRCT bit */
    HDA_STREAM_REG(pThis, CTL, u8Strm) = 0x40000 | (HDA_STREAM_REG(pThis, CTL, u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    /* ICH6 defines default values (0x77 for input and 0xBF for output descriptors) of FIFO size. 18.2.39 */
    HDA_STREAM_REG(pThis, FIFOS, u8Strm) =  u8Strm < 4 ? HDA_SDINFIFO_120B : HDA_SDONFIFO_192B;
    HDA_STREAM_REG(pThis, FIFOW, u8Strm) = u8Strm < 4 ? HDA_SDFIFOW_8B : HDA_SDFIFOW_32B;
    HDA_STREAM_REG(pThis, CBL, u8Strm) = 0;
    HDA_STREAM_REG(pThis, LVI, u8Strm) = 0;
    HDA_STREAM_REG(pThis, FMT, u8Strm) = 0;
    HDA_STREAM_REG(pThis, BDPU, u8Strm) = 0;
    HDA_STREAM_REG(pThis, BDPL, u8Strm) = 0;
    Log(("hda: reset of stream (%d) finished\n", u8Strm));
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
    uint32_t    iRegMem = g_aHdaRegMap[iReg].mem_idx;

    *pu32Value = pThis->au32Regs[iRegMem] & g_aHdaRegMap[iReg].readable;
    return VINF_SUCCESS;
}

static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t    iRegMem = g_aHdaRegMap[iReg].mem_idx;

    pThis->au32Regs[iRegMem]  = (u32Value & g_aHdaRegMap[iReg].writable)
                              | (pThis->au32Regs[iRegMem] & ~g_aHdaRegMap[iReg].writable);
    return VINF_SUCCESS;
}

static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, RST))
    {
        /* exit reset state */
        HDA_REG(pThis, GCTL) |= HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = false;
    }
    else
    {
#ifdef IN_RING3
        /* enter reset state*/
        if (   HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA)
            || HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA))
        {
            Log(("hda: HDA enters in reset with DMA(RIRB:%s, CORB:%s)\n",
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
        /* Flush: GSTS:1 set,  see 6.2.6*/
        HDA_REG(pThis, GSTS) |= HDA_REG_FIELD_FLAG_MASK(GSTS, FSH); /* set the flush state */
        /* DPLBASE and DPUBASE should be initialized with initial value (see 6.2.6)*/
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
        v |= RT_BIT(30);
#define HDA_IS_STREAM_EVENT(pThis, stream)             \
       (   (SDSTS((pThis),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, DE))  \
        || (SDSTS((pThis),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, FE))  \
        || (SDSTS((pThis),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))
#define MARK_STREAM(pThis, stream, v) do { (v) |= HDA_IS_STREAM_EVENT((pThis),stream) ? RT_BIT((stream)) : 0; } while(0)
    MARK_STREAM(pThis, 0, v);
    MARK_STREAM(pThis, 1, v);
    MARK_STREAM(pThis, 2, v);
    MARK_STREAM(pThis, 3, v);
    MARK_STREAM(pThis, 4, v);
    MARK_STREAM(pThis, 5, v);
    MARK_STREAM(pThis, 6, v);
    MARK_STREAM(pThis, 7, v);
    v |= v ? RT_BIT(31) : 0;
    *pu32Value = v;
    return VINF_SUCCESS;
}

static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    /* HDA spec (1a): 3.3.16 WALCLK counter ticks with 24Mhz bitclock rate. */
    *pu32Value = (uint32_t)ASMMultU64ByU32DivByU32(PDMDevHlpTMTimeVirtGetNano(pThis->CTX_SUFF(pDevIns))
                                                   - pThis->u64BaseTS, 24, 1000);
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(CORBRP, RST))
        HDA_REG(pThis, CORBRP) = 0;
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
    if (   HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP)
        && HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) != 0)
        return hdaCORBCmdProcess(pThis);
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

static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    bool fRun     = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fInRun   = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fReset   = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    bool fInReset = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    if (fInReset)
    {
        /*
         * Assert!!! Guest is resetting HDA's stream, we're expecting guest will mark stream as exit
         * from reset
         */
        Assert((!fReset));
        Log(("hda: guest initiated exit of stream reset.\n"));
    }
    else if (fReset)
    {
#ifdef IN_RING3
        /*
         * Assert!!! ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset.
         */
        uint8_t u8Strm = 0;
        PHDABDLEDESC pBdle = NULL;
        HDASTREAMTRANSFERDESC StreamDesc;
        Assert((!fInRun && !fRun));
        switch (iReg)
        {
            case HDA_REG_SD0CTL:
                u8Strm = 0;
                pBdle = &pThis->StInBdle;
                break;
            case HDA_REG_SD4CTL:
                u8Strm = 4;
                pBdle = &pThis->StOutBdle;
                break;
            default:
                Log(("hda: changing SRST bit on non-attached stream\n"));
                return hdaRegWriteU24(pThis, iReg, u32Value);
        }
        Log(("hda: guest initiated enter to stream reset.\n"));
        hdaInitTransferDescriptor(pThis, pBdle, u8Strm, &StreamDesc);
        hdaStreamReset(pThis, pBdle, &StreamDesc, u8Strm);
#else
        return VINF_IOM_R3_MMIO_WRITE;
#endif
    }
    else
    {
#ifdef IN_RING3
        /* we enter here to change DMA states only */
        if (   (fInRun && !fRun)
            || (fRun && !fInRun))
        {
            Assert((!fReset && !fInReset));
            switch (iReg)
            {
                case HDA_REG_SD0CTL:
 #ifdef VBOX_WITH_PDM_AUDIO_DRIVER
                    for (uint32_t lun = 0; lun < 1; lun++)
                        pThis->pDrv[lun]->pfnEnableIn(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceIn, fRun);
 #else
                    AUD_set_active_in(pThis->pCodec->SwVoiceIn, fRun);
 #endif
                    break;
                case HDA_REG_SD4CTL:
 #ifdef VBOX_WITH_PDM_AUDIO_DRIVER
                    for (uint32_t lun = 0; lun < 1; lun++)
                        pThis->pDrv[lun]->pfnEnableOut(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceOut, fRun);
 #else
                    AUD_set_active_out(pThis->pCodec->SwVoiceOut, fRun);
 #endif
                    break;
                default:
                    Log(("hda: changing RUN bit on non-attached stream\n"));
                    break;
            }
        }
#else
        return VINF_IOM_R3_MMIO_WRITE;
#endif
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
            Log(("hda: Attempt to store unsupported value(%x) in SDFIFOW\n", u32Value));
            return hdaRegWriteU16(pThis, iReg, HDA_SDFIFOW_32B);
    }
    return VINF_SUCCESS;
}

/**
 * @note This method could be called for changing value on Output Streams
 *       only (ICH6 datasheet 18.2.39)
 */
static int hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    switch (iReg)
    {
        /* SDInFIFOS is RO, n=0-3 */
        case HDA_REG_SD0FIFOS:
        case HDA_REG_SD1FIFOS:
        case HDA_REG_SD2FIFOS:
        case HDA_REG_SD3FIFOS:
            Log(("hda: Guest tries change value of FIFO size of Input Stream\n"));
            return VINF_SUCCESS;
        case HDA_REG_SD4FIFOS:
        case HDA_REG_SD5FIFOS:
        case HDA_REG_SD6FIFOS:
        case HDA_REG_SD7FIFOS:
            switch(u32Value)
            {
                case HDA_SDONFIFO_16B:
                case HDA_SDONFIFO_32B:
                case HDA_SDONFIFO_64B:
                case HDA_SDONFIFO_128B:
                case HDA_SDONFIFO_192B:
                    return hdaRegWriteU16(pThis, iReg, u32Value);

                case HDA_SDONFIFO_256B:
                    Log(("hda: 256-bit is unsupported, HDA is switched into 192-bit mode\n"));
                default:
                    return hdaRegWriteU16(pThis, iReg, HDA_SDONFIFO_192B);
            }
            return VINF_SUCCESS;
        default:
            AssertMsgFailed(("Something weird happened with register lookup routine"));
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static void hdaSdFmtToAudSettings(uint32_t u32SdFmt, uint32_t * pFrequency, uint32_t * pChannels, audfmt_e *pFormat, uint32_t *pEndian)
# else
static void hdaSdFmtToAudSettings(uint32_t u32SdFmt, audsettings_t *pAudSetting)
# endif
{
# ifndef VBOX_WITH_PDM_AUDIO_DRIVER
    Assert((pAudSetting));
# endif
# define EXTRACT_VALUE(v, mask, shift) ((v & ((mask) << (shift))) >> (shift))
    uint32_t u32Hz = (u32SdFmt & HDA_SDFMT_BASE_RATE_SHIFT) ? 44100 : 48000;
    uint32_t u32HzMult = 1;
    uint32_t u32HzDiv = 1;
    switch (EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT))
    {
        case 0: u32HzMult = 1; break;
        case 1: u32HzMult = 2; break;
        case 2: u32HzMult = 3; break;
        case 3: u32HzMult = 4; break;
        default:
            Log(("hda: unsupported multiplier %x\n", u32SdFmt));
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
    }
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    *pFrequency = u32Hz * u32HzMult / u32HzDiv;
# else
    pAudSetting->freq = u32Hz * u32HzMult / u32HzDiv;
# endif

    switch (EXTRACT_VALUE(u32SdFmt, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT))
    {
        case 0:
            Log(("hda: %s requested 8-bit\n", __FUNCTION__));
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            *pFormat = AUD_FMT_S8;
# else
            pAudSetting->fmt = AUD_FMT_S8;
# endif
            break;
        case 1:
            Log(("hda: %s requested 16-bit\n", __FUNCTION__));
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            *pFormat = AUD_FMT_S16;
# else
            pAudSetting->fmt = AUD_FMT_S16;
# endif
            break;
        case 2:
            Log(("hda: %s requested 20-bit\n", __FUNCTION__));
            break;
        case 3:
            Log(("hda: %s requested 24-bit\n", __FUNCTION__));
            break;
        case 4:
            Log(("hda: %s requested 32-bit\n", __FUNCTION__));
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            *pFormat = AUD_FMT_S32;
# else
            pAudSetting->fmt = AUD_FMT_S32;
# endif
            break;
        default:
            AssertMsgFailed(("Unsupported"));
    }
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    *pChannels = (u32SdFmt & 0xf) + 1;
    *pFormat = AUD_FMT_S16;
    *pEndian = 0;
# else
    pAudSetting->nchannels = (u32SdFmt & 0xf) + 1;
    pAudSetting->fmt = AUD_FMT_S16;
    pAudSetting->endianness = 0;
# endif
# undef EXTRACT_VALUE
}
#endif

static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
# ifdef VBOX_WITH_HDA_CODEC_EMU
    /** @todo a bit more investigation is required here. */
    int rc = 0;
    /* no reason to reopen voice with same settings */
    if (u32Value == HDA_REG_IND(pThis, iReg))
        return VINF_SUCCESS;
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    uint32_t uFrequency;
    uint32_t cChannels;
    audfmt_e Format;
    uint32_t Endian;
    hdaSdFmtToAudSettings(u32Value, &uFrequency, &cChannels, &Format, &Endian);
# else
    audsettings_t as;
    hdaSdFmtToAudSettings(u32Value, &as);
# endif
    switch (iReg)
    {
        case HDA_REG_SD0FMT:
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            for (uint32_t lun = 0; lun < 1; lun++)
                rc = hdaCodecOpenVoice(pThis->pCodec[lun], PI_INDEX, uFrequency, cChannels, Format, Endian);
# else
            rc = hdaCodecOpenVoice(pThis->pCodec, PI_INDEX, &as);
# endif
            break;
        case HDA_REG_SD4FMT:
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            for (uint32_t lun = 0; lun < 1; lun++)
                rc = hdaCodecOpenVoice(pThis->pCodec[lun], PO_INDEX, uFrequency, cChannels, Format, Endian);
# else
            rc = hdaCodecOpenVoice(pThis->pCodec, PO_INDEX, &as);
# endif
            break;
        default:
            Log(("HDA: attempt to change format on %d\n", iReg));
            rc = 0;
    }
    return hdaRegWriteU16(pThis, iReg, u32Value);
# else
    return hdaRegWriteU16(pThis, iReg, u32Value);
# endif
#else
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
    int                         rc  = VINF_SUCCESS;

    /*
     * if guest set the ICB bit of IRS register, HDA should process the verb in IC register,
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
            LogRel(("hda: guest attempted process immediate verb (%x) with active CORB\n", cmd));
            return rc;
        }
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */
        Log(("hda: IC:%x\n", cmd));
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        for (uint32_t lun = 0; lun < 1; lun++)
            rc = pThis->pCodec[lun]->pfnLookup(pThis->pCodec[lun], cmd, &pfn);
# else
        rc = pThis->pCodec->pfnLookup(pThis->pCodec, cmd, &pfn);
# endif
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        for (uint32_t lun = 0; lun < 1; lun++)
            rc = pfn(pThis->pCodec[lun], cmd, &resp);
#else
        rc = pfn(pThis->pCodec, cmd, &resp);
#endif
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        HDA_REG(pThis, IR) = (uint32_t)resp;
        Log(("hda: IR:%x\n", HDA_REG(pThis, IR)));
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, IRV);  /* result is ready  */
        HDA_REG(pThis, IRS) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, ICB); /* busy is clear */
#else
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
            /** @todo: first bit has special meaning */
            pThis->u64DPBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64DPBase |= pThis->au32Regs[iRegMem];
            break;
        case HDA_REG_DPUBASE:
            pThis->u64DPBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64DPBase |= ((uint64_t)pThis->au32Regs[iRegMem] << 32);
            break;
        default:
            AssertMsgFailed(("Invalid index"));
    }
    Log(("hda: CORB base:%llx RIRB base: %llx DP base: %llx\n", pThis->u64CORBBase, pThis->u64RIRBBase, pThis->u64DPBase));
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
static void dump_bd(PHDASTATE pThis, PHDABDLEDESC pBdle, uint64_t u64BaseDMA)
{
#if 0
    uint64_t addr;
    uint32_t len;
    uint32_t ioc;
    uint8_t  bdle[16];
    uint32_t counter;
    uint32_t i;
    uint32_t sum = 0;
    Assert(pBdle && pBdle->u32BdleMaxCvi);
    for (i = 0; i <= pBdle->u32BdleMaxCvi; ++i)
    {
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BaseDMA + i*16, bdle, 16);
        addr = *(uint64_t *)bdle;
        len = *(uint32_t *)&bdle[8];
        ioc = *(uint32_t *)&bdle[12];
        Log(("hda: %s bdle[%d] a:%llx, len:%d, ioc:%d\n",  (i == pBdle->u32BdleCvi? "[C]": "   "), i, addr, len, ioc & 0x1));
        sum += len;
    }
    Log(("hda: sum: %d\n", sum));
    for (i = 0; i < 8; ++i)
    {
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), (pThis->u64DPBase & DPBASE_ADDR_MASK) + i*8, &counter, sizeof(&counter));
        Log(("hda: %s stream[%d] counter=%x\n", i == SDCTL_NUM(pThis, 4) || i == SDCTL_NUM(pThis, 0)? "[C]": "   ",
             i , counter));
    }
#endif
}
#endif

static void hdaFetchBdle(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    uint8_t  bdle[16];
    Assert((   pStreamDesc->u64BaseDMA
            && pBdle
            && pBdle->u32BdleMaxCvi));
    PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pStreamDesc->u64BaseDMA + pBdle->u32BdleCvi*16, bdle, 16);
    pBdle->u64BdleCviAddr = *(uint64_t *)bdle;
    pBdle->u32BdleCviLen = *(uint32_t *)&bdle[8];
    pBdle->fBdleCviIoc = (*(uint32_t *)&bdle[12]) & 0x1;
#ifdef LOG_ENABLED
    dump_bd(pThis, pBdle, pStreamDesc->u64BaseDMA);
#endif
}

DECLINLINE(uint32_t) hdaCalculateTransferBufferLength(PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc,
                                                      uint32_t u32SoundBackendBufferBytesAvail, uint32_t u32CblLimit)
{
    uint32_t cb2Copy;
    /*
     * Number of bytes depends on the current position in buffer (u32BdleCviLen-u32BdleCviPos)
     */
    Assert((pBdle->u32BdleCviLen >= pBdle->u32BdleCviPos)); /* sanity */
    cb2Copy = pBdle->u32BdleCviLen - pBdle->u32BdleCviPos;
    /*
     * we may increase the counter in range of [0, FIFOS + 1]
     */
    cb2Copy = RT_MIN(cb2Copy, pStreamDesc->u32Fifos + 1);
    Assert((u32SoundBackendBufferBytesAvail > 0));

    /* sanity check to avoid overriding the backend audio buffer */
    cb2Copy = RT_MIN(cb2Copy, u32SoundBackendBufferBytesAvail);
    cb2Copy = RT_MIN(cb2Copy, u32CblLimit);

    if (cb2Copy <= pBdle->cbUnderFifoW)
        return 0;
    cb2Copy -= pBdle->cbUnderFifoW; /* forcibly reserve the amount of unreported bytes to copy */
    return cb2Copy;
}

DECLINLINE(void) hdaBackendWriteTransferReported(PHDABDLEDESC pBdle, uint32_t cbArranged2Copy, uint32_t cbCopied,
                                                 uint32_t *pu32DMACursor, uint32_t *pu32BackendBufferCapacity)
{
    Log(("hda:hdaBackendWriteTransferReported: cbArranged2Copy: %d, cbCopied: %d, pu32DMACursor: %d, pu32BackendBufferCapacity:%d\n",
        cbArranged2Copy, cbCopied, pu32DMACursor ? *pu32DMACursor : 0, pu32BackendBufferCapacity ? *pu32BackendBufferCapacity : 0));
    Assert((cbCopied));
    Assert((pu32BackendBufferCapacity && *pu32BackendBufferCapacity));
    /* Assertion!!! Fewer than cbUnderFifoW bytes were copied.
     * Probably we need to move the buffer, but it is rather hard to imagine a situation
     * where it might happen.
     */
    Assert((cbCopied == pBdle->cbUnderFifoW + cbArranged2Copy)); /* we assume that we write the entire buffer including unreported bytes */
    if (   pBdle->cbUnderFifoW
        && pBdle->cbUnderFifoW <= cbCopied)
        Log(("hda:hdaBackendWriteTransferReported: CVI resetting cbUnderFifoW:%d(pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    pBdle->cbUnderFifoW -= RT_MIN(pBdle->cbUnderFifoW, cbCopied);
    Assert((!pBdle->cbUnderFifoW)); /* Assert!!! Incorrect assumption */

    /* We always increment the position of DMA buffer counter because we're always reading into an intermediate buffer */
    pBdle->u32BdleCviPos += cbArranged2Copy;

    Assert((pBdle->u32BdleCviLen >= pBdle->u32BdleCviPos && *pu32BackendBufferCapacity >= cbCopied)); /* sanity */
    /* We report all bytes (including previously unreported bytes) */
    *pu32DMACursor += cbCopied;
    /* Decrease the backend counter by the number of bytes we copied to the backend */
    *pu32BackendBufferCapacity -= cbCopied;
    Log(("hda:hdaBackendWriteTransferReported: CVI(pos:%d, len:%d), pu32DMACursor: %d, pu32BackendBufferCapacity:%d\n",
        pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, *pu32DMACursor, *pu32BackendBufferCapacity));
}

DECLINLINE(void) hdaBackendReadTransferReported(PHDABDLEDESC pBdle, uint32_t cbArranged2Copy, uint32_t cbCopied,
                                                uint32_t *pu32DMACursor, uint32_t *pu32BackendBufferCapacity)
{
    Assert((cbCopied, cbArranged2Copy));
    *pu32BackendBufferCapacity -= cbCopied;
    pBdle->u32BdleCviPos += cbCopied;
    Log(("hda:hdaBackendReadTransferReported: CVI resetting cbUnderFifoW:%d(pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));
    *pu32DMACursor += cbCopied + pBdle->cbUnderFifoW;
    pBdle->cbUnderFifoW = 0;
    Log(("hda:hdaBackendReadTransferReported: CVI(pos:%d, len:%d), pu32DMACursor: %d, pu32BackendBufferCapacity:%d\n",
        pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, pu32DMACursor ? *pu32DMACursor : 0, pu32BackendBufferCapacity ? *pu32BackendBufferCapacity : 0));
}

DECLINLINE(void) hdaBackendTransferUnreported(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc,
                                              uint32_t cbCopied, uint32_t *pu32BackendBufferCapacity)
{
    Log(("hda:hdaBackendTransferUnreported: CVI (cbUnderFifoW:%d, pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));
    pBdle->u32BdleCviPos += cbCopied;
    pBdle->cbUnderFifoW += cbCopied;
    /* In case of a read transaction we're always copying from the backend buffer */
    if (pu32BackendBufferCapacity)
        *pu32BackendBufferCapacity -= cbCopied;
    Log(("hda:hdaBackendTransferUnreported: CVI (cbUnderFifoW:%d, pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));
    Assert((pBdle->cbUnderFifoW <= hdaFifoWToSz(pThis, pStreamDesc)));
}

DECLINLINE(bool) hdaIsTransferCountersOverlapped(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    bool fOnBufferEdge = (   *pStreamDesc->pu32Lpib == pStreamDesc->u32Cbl
                          || pBdle->u32BdleCviPos == pBdle->u32BdleCviLen);

    Assert((*pStreamDesc->pu32Lpib <= pStreamDesc->u32Cbl));

    if (*pStreamDesc->pu32Lpib == pStreamDesc->u32Cbl)
        *pStreamDesc->pu32Lpib -= pStreamDesc->u32Cbl;
    hdaUpdatePosBuf(pThis, pStreamDesc);

    /* don't touch BdleCvi counter on uninitialized descriptor */
    if (   pBdle->u32BdleCviPos
        && pBdle->u32BdleCviPos == pBdle->u32BdleCviLen)
    {
        pBdle->u32BdleCviPos = 0;
        pBdle->u32BdleCvi++;
        if (pBdle->u32BdleCvi == pBdle->u32BdleMaxCvi + 1)
            pBdle->u32BdleCvi = 0;
    }
    return fOnBufferEdge;
}

DECLINLINE(void) hdaStreamCounterUpdate(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc,
                                        uint32_t cbInc)
{
    /*
     * if we're below the FIFO Watermark, it's expected that HDA doesn't fetch anything.
     * (ICH6 datasheet 18.2.38)
     */
    if (!pBdle->cbUnderFifoW)
    {
        *pStreamDesc->pu32Lpib += cbInc;

        /*
         * Assert. The buffer counters should never overlap.
         */
        Assert((*pStreamDesc->pu32Lpib <= pStreamDesc->u32Cbl));

        hdaUpdatePosBuf(pThis, pStreamDesc);

    }
}

static bool hdaDoNextTransferCycle(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    bool fDoNextTransferLoop = true;
    if (   pBdle->u32BdleCviPos == pBdle->u32BdleCviLen
        || *pStreamDesc->pu32Lpib == pStreamDesc->u32Cbl)
    {
        if (    !pBdle->cbUnderFifoW
             && pBdle->fBdleCviIoc)
        {
            /**
             * @todo - more carefully investigate BCIS flag.
             * Speech synthesis works fine on Mac Guest if this bit isn't set
             * but in general sound quality gets worse.
             */
            *pStreamDesc->pu32Sts |= HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS);

            /*
             * we should generate the interrupt if ICE bit of SDCTL register is set.
             */
            if (pStreamDesc->u32Ctl & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE))
                hdaProcessInterrupt(pThis);
        }
        fDoNextTransferLoop = false;
    }
    return fDoNextTransferLoop;
}

/*
 * hdaReadAudio - copies samples from audio backend to DMA.
 * Note: this function writes to the DMA buffer immediately, but "reports bytes" when all conditions are met (FIFOW)
 */
static uint32_t hdaReadAudio(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t *pu32Avail, bool *fStop, uint32_t u32CblLimit)
{
    PHDABDLEDESC pBdle = &pThis->StInBdle;
    uint32_t cbTransferred = 0;
    uint32_t cb2Copy = 0;
    uint32_t cbBackendCopy = 0;

    Log(("hda:ra: CVI(pos:%d, len:%d)\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    cb2Copy = hdaCalculateTransferBufferLength(pBdle, pStreamDesc, *pu32Avail, u32CblLimit);
    if (!cb2Copy)
        /* if we enter here we can't report "unreported bits" */
        *fStop = true;
    else
    {
        /*
         * read from backend input line to the last unreported position or at the begining.
         */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        //cbBackendCopy = pThis->pDrv[0]->pfnRead(pThis->pDrv[0], pThis->pCodec[0]->SwVoiceIn, pBdle->au8HdaBuffer, cb2Copy);
        //cbBackendCopy = pThis->pDrv[0]->pfnRead(pThis->pDrv[0], pThis->pCodec[1]->SwVoiceIn, pBdle->au8HdaBuffer, cb2Copy);
#else
        cbBackendCopy = AUD_read(pThis->pCodec->SwVoiceIn, pBdle->au8HdaBuffer, cb2Copy);
#endif
        /*
         * write the HDA DMA buffer
         */
        PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer, cbBackendCopy);

        /* Don't see any reason why cb2Copy would differ from cbBackendCopy */
        Assert((cbBackendCopy == cb2Copy && (*pu32Avail) >= cb2Copy)); /* sanity */

        if (pBdle->cbUnderFifoW + cbBackendCopy > hdaFifoWToSz(pThis, 0))
            hdaBackendReadTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransferred, pu32Avail);
        else
        {
            hdaBackendTransferUnreported(pThis, pBdle, pStreamDesc, cbBackendCopy, pu32Avail);
            *fStop = true;
        }
    }

    Assert((cbTransferred <= (SDFIFOS(pThis, 0) + 1)));
    Log(("hda:ra: CVI(pos:%d, len:%d) cbTransferred: %d\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, cbTransferred));
    return cbTransferred;
}

static uint32_t hdaWriteAudio(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t *pu32Avail, bool *fStop, uint32_t u32CblLimit)
{
    PHDABDLEDESC pBdle = &pThis->StOutBdle;
    uint32_t cbTransferred = 0;
    uint32_t cb2Copy = 0; /* local byte counter (on local buffer) */
    uint32_t cbBackendCopy = 0; /* local byte counter, how many bytes copied to backend */

    Log(("hda:wa: CVI(cvi:%d, pos:%d, len:%d)\n", pBdle->u32BdleCvi, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    cb2Copy = hdaCalculateTransferBufferLength(pBdle, pStreamDesc, *pu32Avail, u32CblLimit);

    /*
     * Copy from DMA to the corresponding hdaBuffer (if there are any bytes from the
     * previous unreported transfer we write at offset 'pBdle->cbUnderFifoW').
     */
    if (!cb2Copy)
        *fStop = true;
    else
    {
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer + pBdle->cbUnderFifoW, cb2Copy);
        /*
         * Write to audio backend. we should ensure that we have enough bytes to copy to the backend.
         */
        if (cb2Copy + pBdle->cbUnderFifoW >= hdaFifoWToSz(pThis, pStreamDesc))
        {
            /*
             * Feed the newly fetched samples, including unreported ones, to the backend.
             */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            for (uint32_t lun = 0; lun < 1; lun++)
                cbBackendCopy = pThis->pDrv[lun]->pfnWrite(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceOut, pBdle->au8HdaBuffer, cb2Copy + pBdle->cbUnderFifoW);
            LogFlow(("cbBackendCopy write %d bytes \n", cbBackendCopy));
#else
            cbBackendCopy = AUD_write (pThis->pCodec->SwVoiceOut, pBdle->au8HdaBuffer, cb2Copy + pBdle->cbUnderFifoW);
#endif
            hdaBackendWriteTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransferred, pu32Avail);
        }
        else
        {
            /* Not enough bytes to be processed and reported, we'll try our luck next time around */
            hdaBackendTransferUnreported(pThis, pBdle, pStreamDesc, cb2Copy, NULL);
            *fStop = true;
        }
    }

    Assert(cbTransferred <= SDFIFOS(pThis, 4) + 1);
    Log(("hda:wa: CVI(pos:%d, len:%d, cbTransferred:%d)\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, cbTransferred));
    return cbTransferred;
}

/**
 * @interface_method_impl{HDACODEC,pfnReset}
 */
DECLCALLBACK(int) hdaCodecReset(PHDACODEC pCodec)
{
    PHDASTATE pThis = (PHDASTATE)pCodec->pvHDAState;
    NOREF(pThis);
    return VINF_SUCCESS;
}

DECLINLINE(void) hdaInitTransferDescriptor(PHDASTATE pThis, PHDABDLEDESC pBdle, uint8_t u8Strm,
                                           PHDASTREAMTRANSFERDESC pStreamDesc)
{
    Assert(pThis); Assert(pBdle); Assert(pStreamDesc); Assert(u8Strm <= 7);

    memset(pStreamDesc, 0, sizeof(HDASTREAMTRANSFERDESC));
    pStreamDesc->u8Strm     = u8Strm;
    pStreamDesc->u32Ctl     = HDA_STREAM_REG(pThis, CTL, u8Strm);
    pStreamDesc->u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, u8Strm),
                                          HDA_STREAM_REG(pThis, BDPU, u8Strm));
    pStreamDesc->pu32Lpib   = &HDA_STREAM_REG(pThis, LPIB, u8Strm);
    pStreamDesc->pu32Sts    = &HDA_STREAM_REG(pThis, STS, u8Strm);
    pStreamDesc->u32Cbl     = HDA_STREAM_REG(pThis, CBL, u8Strm);
    pStreamDesc->u32Fifos   = HDA_STREAM_REG(pThis, FIFOS, u8Strm);

    pBdle->u32BdleMaxCvi    = HDA_STREAM_REG(pThis, LVI, u8Strm);

#ifdef LOG_ENABLED
    if (   pBdle
        && pBdle->u32BdleMaxCvi)
    {
        Log(("Initialization of transfer descriptor:\n"));
        dump_bd(pThis, pBdle, pStreamDesc->u64BaseDMA);
    }
#endif
}


/**
 * @interface_method_impl{HDACODEC,pfnTransfer}
 */
static DECLCALLBACK(void) hdaTransfer(PHDACODEC pCodec, ENMSOUNDSOURCE src, int avail)
{
    PHDASTATE       pThis  = (PHDASTATE)pCodec->pvHDAState;
    uint8_t         u8Strm = 0;
    PHDABDLEDESC    pBdle  = NULL;

    switch (src)
    {
        case PO_INDEX:
        {
            u8Strm = 4;
            pBdle = &pThis->StOutBdle;
            break;
        }
        case PI_INDEX:
        {
            u8Strm = 0;
            pBdle = &pThis->StInBdle;
            break;
        }
        default:
            return;
    }

    HDASTREAMTRANSFERDESC StreamDesc;
    hdaInitTransferDescriptor(pThis, pBdle, u8Strm, &StreamDesc);

    bool fStop = false;
    while (avail && !fStop)
    {
        Assert(   (StreamDesc.u32Ctl & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN))
               && avail
               && StreamDesc.u64BaseDMA);

        /* Fetch the Buffer Descriptor Entry (BDE). */

        if (hdaIsTransferCountersOverlapped(pThis, pBdle, &StreamDesc))
            hdaFetchBdle(pThis, pBdle, &StreamDesc);
        *StreamDesc.pu32Sts |= HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);
        Assert((avail >= 0 && (StreamDesc.u32Cbl >= (*StreamDesc.pu32Lpib)))); /* sanity */
        uint32_t u32CblLimit = StreamDesc.u32Cbl - (*StreamDesc.pu32Lpib);
        Assert((u32CblLimit > hdaFifoWToSz(pThis, &StreamDesc)));
        Log(("hda: CBL=%d, LPIB=%d\n", StreamDesc.u32Cbl, *StreamDesc.pu32Lpib));
        uint32_t cb;
        switch (src)
        {
            case PO_INDEX:
                cb = hdaWriteAudio(pThis, &StreamDesc, (uint32_t *)&avail, &fStop, u32CblLimit);
                break;
            case PI_INDEX:
                cb = hdaReadAudio(pThis, &StreamDesc, (uint32_t *)&avail, &fStop, u32CblLimit);
                break;
            default:
                cb = 0;
                fStop = true;
                AssertMsgFailed(("Unsupported"));
        }
        Assert(cb <= StreamDesc.u32Fifos + 1);
        *StreamDesc.pu32Sts &= ~HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);

        /* Process end of buffer condition. */
        hdaStreamCounterUpdate(pThis, pBdle, &StreamDesc, cb);
        fStop = !fStop ? !hdaDoNextTransferCycle(pThis, pBdle, &StreamDesc) : fStop;
    }
}
#endif

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

    Log(("hdaMMIORead: offReg=%#x cb=%#x\n", offReg, cb));
#define NEW_READ_CODE
#ifdef NEW_READ_CODE
    Assert(cb == 4); Assert((offReg & 3) == 0);

    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));

    if (idxRegDsc == -1)
        LogRel(("hda: Invalid read access @0x%x(of bytes:%d)\n", offReg, cb));

    if (idxRegDsc != -1)
    {
        /* ASSUMES gapless DWORD at end of map. */
        if (g_aHdaRegMap[idxRegDsc].size == 4)
        {
            /*
             * Straight forward DWORD access.
             */
            rc = g_aHdaRegMap[idxRegDsc].pfnRead(pThis, idxRegDsc, (uint32_t *)pv);
            Log(("hda: read %s => %x (%Rrc)\n", g_aHdaRegMap[idxRegDsc].abbrev, *(uint32_t *)pv, rc));
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
                Log(("hda: read %s[%db] => %x (%Rrc)*\n", g_aHdaRegMap[idxRegDsc].abbrev, cbReg, u32Tmp, rc));
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
        Log(("hda: hole at %x is accessed for read\n", offReg));
    }
#else
    if (idxRegDsc != -1)
    {
        /** @todo r=bird: Accesses crossing register boundraries aren't handled
         *        right from what I can tell?  If they are, please explain
         *        what the rules are. */
        uint32_t mask = 0;
        uint32_t shift = (g_aHdaRegMap[idxRegDsc].offset - offReg) % sizeof(uint32_t) * 8;
        uint32_t u32Value = 0;
        switch(cb)
        {
            case 1: mask = 0x000000ff; break;
            case 2: mask = 0x0000ffff; break;
            case 4:
            /* 18.2 of the ICH6 datasheet defines the valid access widths as byte, word, and double word */
            case 8:
                mask = 0xffffffff;
                cb = 4;
                break;
        }
#if 0
        /* Cross-register access. Mac guest hits this assert doing assumption 4 byte access to 3 byte registers e.g. {I,O}SDnCTL
         */
        //Assert((cb <= g_aHdaRegMap[idxRegDsc].size - (offReg - g_aHdaRegMap[idxRegDsc].offset)));
        if (cb > g_aHdaRegMap[idxRegDsc].size - (offReg - g_aHdaRegMap[idxRegDsc].offset))
        {
            int off = cb - (g_aHdaRegMap[idxRegDsc].size - (offReg - g_aHdaRegMap[idxRegDsc].offset));
            rc = hdaMMIORead(pDevIns, pvUser, GCPhysAddr + cb - off, (char *)pv + cb - off, off);
            if (RT_FAILURE(rc))
                AssertRCReturn (rc, rc);
        }
        //Assert(((offReg - g_aHdaRegMap[idxRegDsc].offset) == 0));
#endif
        mask <<= shift;
        rc = g_aHdaRegMap[idxRegDsc].pfnRead(pThis, idxRegDsc, &u32Value);
        *(uint32_t *)pv |= (u32Value & mask);
        Log(("hda: read %s[%x/%x]\n", g_aHdaRegMap[idxRegDsc].abbrev, u32Value, *(uint32_t *)pv));
    }
    else
    {
        *(uint32_t *)pv = 0xFF;
        Log(("hda: hole at %x is accessed for read\n", offReg));
        rc = VINF_SUCCESS;
    }
#endif

    /*
     * Log the outcome.
     */
#ifdef LOG_ENABLED
    if (cbLog == 4)
        Log(("hdaMMIORead: @%#05x -> %#010x %Rrc\n", offRegLog, *(uint32_t *)pv, rc));
    else if (cbLog == 2)
        Log(("hdaMMIORead: @%#05x -> %#06x %Rrc\n", offRegLog, *(uint16_t *)pv, rc));
    else if (cbLog == 1)
        Log(("hdaMMIORead: @%#05x -> %#04x %Rrc\n", offRegLog, *(uint8_t *)pv, rc));
#endif
    return rc;
}


DECLINLINE(int) hdaWriteReg(PHDASTATE pThis, int idxRegDsc, uint32_t u32Value, char const *pszLog)
{
    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));  /** @todo where is this enforced? */

    uint32_t idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
#ifdef LOG_ENABLED
    uint32_t const u32CurValue = pThis->au32Regs[idxRegMem];
#endif
    int rc = g_aHdaRegMap[idxRegDsc].pfnWrite(pThis, idxRegDsc, u32Value);
    Log(("hda: write %#x -> %s[%db]; %x => %x%s\n", u32Value, g_aHdaRegMap[idxRegDsc].abbrev,
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
        AssertReleaseMsgFailed(("%d\n", cb));
    }

#ifdef LOG_ENABLED
    uint32_t const u32LogOldValue = idxRegDsc != -1 ? pThis->au32Regs[idxRegMem] : UINT32_MAX;
    uint32_t const offRegLog = offReg;
    int      const idxRegLog = idxRegMem;
    if (idxRegDsc == -1)
        Log(("hdaMMIOWrite: @%#05x u32=%#010x cb=%d\n", offReg, *(uint32_t const *)pv, cb));
    else if (cb == 4)
        Log(("hdaMMIOWrite: @%#05x u32=%#010x %s\n", offReg, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 2)
        Log(("hdaMMIOWrite: @%#05x u16=%#06x (%#010x) %s\n", offReg, *(uint16_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 1)
        Log(("hdaMMIOWrite: @%#05x u8=%#04x (%#010x) %s\n", offReg, *(uint8_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    if (idxRegDsc != -1 && g_aHdaRegMap[idxRegDsc].size != cb)
        Log(("hdaMMIOWrite: size=%d != cb=%d!!\n", g_aHdaRegMap[idxRegDsc].size, cb));
#endif

#define NEW_WRITE_CODE
#ifdef NEW_WRITE_CODE
    /*
     * Try for a direct hit first.
     */
    if (idxRegDsc != -1 && g_aHdaRegMap[idxRegDsc].size == cb)
    {
        rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "");
        Log(("hdaMMIOWrite: @%#05x %#x -> %#x\n", offRegLog, u32LogOldValue,
             idxRegLog != -1 ? pThis->au32Regs[idxRegLog] : UINT32_MAX));
    }
    /*
     * Partial or multiple register access, loop thru the requested memory.
     */
    else
    {
        /* If it's an access beyond the start of the register, shift the input
           value and fill in missing bits. Natural alignment rules means we
           will only see 1 or 2 byte accesses of this kind, so no risk of
           shifting out input values. */
        if (idxRegDsc == -1 && (idxRegDsc = hdaRegLookupWithin(pThis, offReg)) != -1)
        {
            uint32_t const cbBefore = offReg - g_aHdaRegMap[idxRegDsc].offset; Assert(cbBefore > 0 && cbBefore < 4);
            offReg    -= cbBefore;
            idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
            u64Value <<= cbBefore * 8;
            u64Value  |= pThis->au32Regs[idxRegMem] & g_afMasks[cbBefore];
            Log(("hdaMMIOWrite: Within register, supplied %u leading bits: %#llx -> %#llx ...\n",
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
                    Log(("hdaMMIOWrite: Supplying missing bits (%#x): %#llx -> %#llx ...\n",
                         g_afMasks[cbReg] & ~g_afMasks[cb], u64Value & g_afMasks[cb], u64Value));
                }
                uint32_t u32LogOldVal = pThis->au32Regs[idxRegMem];
                rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "*");
                Log(("hdaMMIOWrite: @%#05x %#x -> %#x\n", offRegLog, u32LogOldVal,
                     pThis->au32Regs[idxRegMem]));
            }
            else
            {
                LogRel(("hda: Invalid write access @0x%x!\n", offReg));
                cbReg = 1;
            }
            if (rc != VINF_SUCCESS)
                break;
            if (cbReg >= cb)
                break;

            /* advance */
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
                    idxRegDsc = -1;
            }
        }
    }
#else
    if (idxRegDsc != -1)
    {
        /** @todo r=bird: This looks like code for handling unaligned register
         * accesses.  If it isn't, then add a comment explaining what you're
         * trying to do here.  OTOH, if it is then it has the following
         * issues:
         *      -# You're calculating the wrong new value for the register.
         *      -# You're not handling cross register accesses.  Imagine a
         *       4-byte write starting at CORBCTL, or a 8-byte write.
         *
         * PS! consider dropping the 'offset' argument to pfnWrite/pfnRead as
         * nobody seems to be using it and it just adds complexity when reading
         * the code.
         *
         */
        uint32_t u32CurValue = pThis->au32Regs[idxRegMem];
        uint32_t u32NewValue;
        uint32_t mask;
        switch (cb)
        {
            case 1:
                u32NewValue = *(uint8_t const *)pv;
                mask = 0xff;
                break;
            case 2:
                u32NewValue = *(uint16_t const *)pv;
                mask = 0xffff;
                break;
            case 4:
            case 8:
                /* 18.2 of the ICH6 datasheet defines the valid access widths as byte, word, and double word */
                u32NewValue = *(uint32_t const *)pv;
                mask = 0xffffffff;
                cb = 4;
                break;
            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* shall not happen. */
        }
        /* cross-register access, see corresponding comment in hdaMMIORead */
        uint32_t shift = (g_aHdaRegMap[idxRegDsc].offset - offReg) % sizeof(uint32_t) * 8;
        mask <<= shift;
        u32NewValue <<= shift;
        u32NewValue &= mask;
        u32NewValue |= (u32CurValue & ~mask);

        rc = g_aHdaRegMap[idxRegDsc].pfnWrite(pThis, idxRegDsc, u32NewValue);
        Log(("hda: write %s:(%x) %x => %x\n", g_aHdaRegMap[idxRegDsc].abbrev, u32NewValue,
             u32CurValue, pThis->au32Regs[idxRegMem]));
    }
    else
        rc = VINF_SUCCESS;

    Log(("hdaMMIOWrite: @%#05x %#x -> %#x\n", offRegLog, u32LogOldValue,
         idxRegLog != -1 ? pThis->au32Regs[idxRegLog] : UINT32_MAX));
#endif
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
#ifdef NEW_READ_CODE
                               IOMMMIO_FLAGS_READ_DWORD |
#else
                               IOMMMIO_FLAGS_READ_PASSTHRU |
#endif
                               IOMMMIO_FLAGS_WRITE_PASSTHRU,
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

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) hdaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    /* Save Codec nodes states */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    hdaCodecSaveState(pThis->pCodec[0], pSSM);
#else
    hdaCodecSaveState(pThis->pCodec, pSSM);
#endif

    /* Save MMIO registers */
    AssertCompile(RT_ELEMENTS(pThis->au32Regs) >= HDA_NREGS_SAVED);
    SSMR3PutU32(pSSM, RT_ELEMENTS(pThis->au32Regs));
    SSMR3PutMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));

    /* Save HDA dma counters */
    SSMR3PutStructEx(pSSM, &pThis->StOutBdle, sizeof(pThis->StOutBdle), 0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    SSMR3PutStructEx(pSSM, &pThis->StMicBdle, sizeof(pThis->StMicBdle), 0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    SSMR3PutStructEx(pSSM, &pThis->StInBdle,  sizeof(pThis->StInBdle),  0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) hdaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Load Codec nodes states.
     */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    int rc = hdaCodecLoadState(pThis->pCodec[0], pSSM, uVersion);
#else
    int rc = hdaCodecLoadState(pThis->pCodec, pSSM, uVersion);
#endif
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

        case HDA_SSM_VERSION:
            rc = SSMR3GetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
            if (cRegs != RT_ELEMENTS(pThis->au32Regs))
                LogRel(("hda: cRegs is %d, expected %d\n", cRegs, RT_ELEMENTS(pThis->au32Regs)));
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
     * Load HDA dma counters.
     */
    uint32_t   fFlags   = uVersion <= HDA_SSM_VERSION_2 ? SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED : 0;
    PCSSMFIELD paFields = uVersion <= HDA_SSM_VERSION_2 ? g_aHdaBDLEDescFieldsOld              : g_aHdaBDLEDescFields;
    SSMR3GetStructEx(pSSM, &pThis->StOutBdle, sizeof(pThis->StOutBdle), fFlags, paFields, NULL);
    SSMR3GetStructEx(pSSM, &pThis->StMicBdle, sizeof(pThis->StMicBdle), fFlags, paFields, NULL);
    rc = SSMR3GetStructEx(pSSM, &pThis->StInBdle, sizeof(pThis->StInBdle), fFlags, paFields, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Update stuff after the state changes.
     */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    for (uint32_t lun = 0; lun < 1; lun++)
    {
        pThis->pDrv[lun]->pfnEnableIn(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceIn, SDCTL(pThis, 0) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
        pThis->pDrv[lun]->pfnEnableOut(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceOut, SDCTL(pThis, 4) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    }
#else
    AUD_set_active_in(pThis->pCodec->SwVoiceIn, SDCTL(pThis, 0) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    AUD_set_active_out(pThis->pCodec->SwVoiceOut, SDCTL(pThis, 4) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
#endif

    pThis->u64CORBBase = RT_MAKE_U64(HDA_REG(pThis, CORBLBASE), HDA_REG(pThis, CORBUBASE));
    pThis->u64RIRBBase = RT_MAKE_U64(HDA_REG(pThis, RIRBLBASE), HDA_REG(pThis, RIRBUBASE));
    pThis->u64DPBase   = RT_MAKE_U64(HDA_REG(pThis, DPLBASE), HDA_REG(pThis, DPUBASE));
    return VINF_SUCCESS;
}


/* Debug and log type formatters. */

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t)
hdaFormatStrmCtl(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t sdCtl = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDCTL(raw: %#x, strm:%#x, dir:%RTbool, tp:%RTbool strip:%x, deie:%RTbool, ioce:%RTbool, run:%RTbool, srst:%RTbool)",
                       sdCtl,
                       (sdCtl & HDA_REG_FIELD_MASK(SDCTL, NUM)) >> HDA_SDCTL_NUM_SHIFT,
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, DIR)),
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, TP)),
                       (sdCtl & HDA_REG_FIELD_MASK(SDCTL, STRIPE)) >> HDA_SDCTL_STRIPE_SHIFT,
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, DEIE)),
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE)),
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)),
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST)));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t)
hdaFormatStrmFifos(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                   const char *pszType, void const *pvValue,
                   int cchWidth, int cchPrecision, unsigned fFlags,
                   void *pvUser)
{
    uint32_t uSdFifos = (uint32_t)(uintptr_t)pvValue;
    uint32_t cb;
    switch (uSdFifos)
    {
        case HDA_SDONFIFO_16B:  cb = 16; break;
        case HDA_SDONFIFO_32B:  cb = 32; break;
        case HDA_SDONFIFO_64B:  cb = 64; break;
        case HDA_SDONFIFO_128B: cb = 128; break;
        case HDA_SDONFIFO_192B: cb = 192; break;
        case HDA_SDONFIFO_256B: cb = 256; break;
        case HDA_SDINFIFO_120B: cb = 120; break;
        case HDA_SDINFIFO_160B: cb = 160; break;
        default:                cb = 0; break;
    }
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOS(raw: %#x, sdfifos:%u B)", uSdFifos, cb);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t)
hdaFormatStrmFifow(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                   const char *pszType, void const *pvValue,
                   int cchWidth, int cchPrecision, unsigned fFlags,
                   void *pvUser)
{
    uint32_t uSdFifos = (uint32_t)(uintptr_t)pvValue;
    uint32_t cb;
    switch (uSdFifos)
    {
        case HDA_SDFIFOW_8B:  cb = 8;  break;
        case HDA_SDFIFOW_16B: cb = 16; break;
        case HDA_SDFIFOW_32B: cb = 32; break;
        default:              cb = 0;  break;
    }
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOW(raw: %#0x, sdfifow:%d B)", uSdFifos, cb);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t)
hdaFormatStrmSts(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t uSdSts = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDSTS(raw: %#0x, fifordy:%RTbool, dese:%RTbool, fifoe:%RTbool, bcis:%RTbool)",
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
    pHlp->pfnPrintf(pHlp, "hda: %s: 0x%x\n", g_aHdaRegMap[iHdaIndex].abbrev, pThis->au32Regs[g_aHdaRegMap[iHdaIndex].mem_idx]);
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
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    for (uint32_t lun = 0; lun < 1; lun++)
        if (pThis->pCodec[lun]->pfnCodecDbgListNodes)
            pThis->pCodec[lun]->pfnCodecDbgListNodes(pThis->pCodec[lun], pHlp, pszArgs);
        else
            pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
#else
    if (pThis->pCodec->pfnCodecDbgListNodes)
        pThis->pCodec->pfnCodecDbgListNodes(pThis->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
#endif
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoCodecSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    for (uint32_t lun = 0; lun < 1; lun++)
        if (pThis->pCodec[lun]->pfnCodecDbgSelector)
            pThis->pCodec[lun]->pfnCodecDbgSelector(pThis->pCodec[lun], pHlp, pszArgs);
        else
            pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
#else
    if (pThis->pCodec->pfnCodecDbgSelector)
        pThis->pCodec->pfnCodecDbgSelector(pThis->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
#endif
}


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
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *
 * @remark  The original sources didn't install a reset handler, but it seems to
 *          make sense to me so we'll do it.
 */
static DECLCALLBACK(void)  hdaReset(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    HDA_REG(pThis, GCAP)     = HDA_MAKE_GCAP(4,4,0,0,1); /* see 6.2.1 */
    HDA_REG(pThis, VMIN)     = 0x00;    /* see 6.2.2 */
    HDA_REG(pThis, VMAJ)     = 0x01;    /* see 6.2.3 */
    HDA_REG(pThis, OUTPAY)   = 0x003C;  /* see 6.2.4 */
    HDA_REG(pThis, INPAY)    = 0x001D;  /* see 6.2.5 */
    HDA_REG(pThis, CORBSIZE) = 0x42;    /* see 6.2.1 */
    HDA_REG(pThis, RIRBSIZE) = 0x42;    /* see 6.2.1 */
    HDA_REG(pThis, CORBRP)   = 0x0;
    HDA_REG(pThis, RIRBWP)   = 0x0;

    Log(("hda: inter HDA reset.\n"));

    /* Stop any audio currently playing. */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    for (uint32_t lun = 0; lun < 1; lun++)
    {
        pThis->pDrv[lun]->pfnEnableIn(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceIn, false);
        pThis->pDrv[lun]->pfnEnableOut(pThis->pDrv[lun], pThis->pCodec[lun]->SwVoiceOut, false);
    }
#else
    AUD_set_active_in(pThis->pCodec->SwVoiceIn, false);
    AUD_set_active_out(pThis->pCodec->SwVoiceOut, false);
#endif


    pThis->cbCorbBuf = 256 * sizeof(uint32_t);

    if (pThis->pu32CorbBuf)
        memset(pThis->pu32CorbBuf, 0, pThis->cbCorbBuf);
    else
        pThis->pu32CorbBuf = (uint32_t *)RTMemAllocZ(pThis->cbCorbBuf);

    pThis->cbRirbBuf = 256 * sizeof(uint64_t);
    if (pThis->pu64RirbBuf)
        memset(pThis->pu64RirbBuf, 0, pThis->cbRirbBuf);
    else
        pThis->pu64RirbBuf = (uint64_t *)RTMemAllocZ(pThis->cbRirbBuf);

    pThis->u64BaseTS = PDMDevHlpTMTimeVirtGetNano(pDevIns);

    HDABDLEDESC StEmptyBdle;
    for (uint8_t u8Strm = 0; u8Strm < 8; ++u8Strm)
    {
        HDASTREAMTRANSFERDESC StreamDesc;
        PHDABDLEDESC pBdle = NULL;
        if (u8Strm == 0)
            pBdle = &pThis->StInBdle;
        else if(u8Strm == 4)
            pBdle = &pThis->StOutBdle;
        else
        {
            memset(&StEmptyBdle, 0, sizeof(HDABDLEDESC));
            pBdle = &StEmptyBdle;
        }
        hdaInitTransferDescriptor(pThis, pBdle, u8Strm, &StreamDesc);
        /* hdaStreamReset prevents changing the SRST bit, so we force it to zero here. */
        HDA_STREAM_REG(pThis, CTL, u8Strm) = 0;
        hdaStreamReset(pThis, pBdle, &StreamDesc, u8Strm);
    }

    /* emulation of codec "wake up" (HDA spec 5.5.1 and 6.5)*/
    HDA_REG(pThis, STATESTS) = 0x1;

    Log(("hda: reset finished\n"));
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) hdaDestruct(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    for (uint32_t lun = 0; lun < 1; lun++)
    {
        if (pThis->pCodec[lun])
        {
            int rc = hdaCodecDestruct(pThis->pCodec[lun]);
            AssertRC(rc);
            RTMemFree(pThis->pCodec[lun]);
            pThis->pCodec[lun] = NULL;
        }
    }
#else
    if (pThis->pCodec)
    {
        int rc = hdaCodecDestruct(pThis->pCodec);
        AssertRC(rc);

        RTMemFree(pThis->pCodec);
        pThis->pCodec = NULL;
    }
#endif

    RTMemFree(pThis->pu32CorbBuf);
    pThis->pu32CorbBuf = NULL;

    RTMemFree(pThis->pu64RirbBuf);
    pThis->pu64RirbBuf = NULL;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hdaConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PHDASTATE   pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int         rc;

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validations.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "R0Enabled\0"
                                          "RCEnabled\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_ ("Invalid configuration for the Intel HDA device"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "RCEnabled", &pThis->fRCEnabled, false);
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
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    //for (iter = 0; i < 3; i++)
    {
        /*
        * Attach driver.
        */
        rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Audio Driver Port");
        if (RT_SUCCESS(rc))
        {
            pThis->pDrv[0] = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsgReturn(pThis->pDrv,
                            ("Configuration error: instance %d has no host audio interface!\n", iInstance),
                            VERR_PDM_MISSING_INTERFACE);
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            Log(("ac97: No attached driver!\n"));
        }
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to attach AC97 LUN #0! rc=%Rrc\n", rc));
            return rc;
        }
        rc = PDMDevHlpDriverAttach(pDevIns, 1, &pThis->IBase, &pThis->pDrvBase, "Audio Driver Port");
        if (RT_SUCCESS(rc))
        {
            pThis->pDrv[1] = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsgReturn(pThis->pDrv[1],
                            ("Configuration error: instance %d has no host audio interface!\n", iInstance),
                            VERR_PDM_MISSING_INTERFACE);
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            Log(("ac97: No attached driver! LUN#1\n"));
        }
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to attach AC97 LUN #1! rc=%Rrc\n", rc));
            return rc;
        }
    }
#else
    /*
     * Attach driver.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Audio Driver Port");
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log(("hda: No attached driver!\n"));
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach Intel HDA LUN #0! rc=%Rrc\n", rc));
        return rc;
    }
#endif
    /* Construct codec state. */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    pThis->pCodec[0] = (PHDACODEC)RTMemAllocZ(sizeof(HDACODEC));
    if (!pThis->pCodec[0])
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("HDA: Out of memory allocating codec state"));
    pThis->pCodec[1] = (PHDACODEC)RTMemAllocZ(sizeof(HDACODEC));
    if (!pThis->pCodec[1])
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("HDA: Out of memory allocating codec state"));

    pThis->pCodec[0]->pvHDAState = pThis;
    pThis->pCodec[1]->pvHDAState = pThis;

    pThis->pCodec[0]->pDrv = pThis->pDrv[0];
    //pThis->pCodec[1]->pDrv = pThis->pDrv[1];

    rc = hdaCodecConstruct(pDevIns, pThis->pCodec[0], pCfgHandle);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    rc = hdaCodecConstruct(pDevIns, pThis->pCodec[1], pCfgHandle);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
       verb F20 should provide device/codec recognition. */
    Assert(pThis->pCodec[0]->u16VendorId);
    Assert(pThis->pCodec[0]->u16DeviceId);
    Assert(pThis->pCodec[1]->u16VendorId);
    Assert(pThis->pCodec[1]->u16DeviceId);
    PCIDevSetSubSystemVendorId(&pThis->PciDev, pThis->pCodec[0]->u16VendorId); /* 2c ro - intel.) */
    PCIDevSetSubSystemId(      &pThis->PciDev, pThis->pCodec[0]->u16DeviceId); /* 2e ro. */
    PCIDevSetSubSystemVendorId(&pThis->PciDev, pThis->pCodec[1]->u16VendorId); /* 2c ro - intel.) */
    PCIDevSetSubSystemId(      &pThis->PciDev, pThis->pCodec[1]->u16DeviceId); /* 2e ro. */

    hdaReset(pDevIns);
    pThis->pCodec[0]->id = 0;
    pThis->pCodec[0]->pfnTransfer = hdaTransfer;
    pThis->pCodec[0]->pfnReset = hdaCodecReset;
    pThis->pCodec[1]->id = 0;
    pThis->pCodec[1]->pfnTransfer = hdaTransfer;
    pThis->pCodec[1]->pfnReset = hdaCodecReset;
#else
    pThis->pCodec = (PHDACODEC)RTMemAllocZ(sizeof(HDACODEC));
    if (!pThis->pCodec)
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("HDA: Out of memory allocating codec state"));

    pThis->pCodec->pvHDAState = pThis;
    rc = hdaCodecConstruct(pDevIns, pThis->pCodec, pCfgHandle);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
       verb F20 should provide device/codec recognition. */
    Assert(pThis->pCodec->u16VendorId);
    Assert(pThis->pCodec->u16DeviceId);
    PCIDevSetSubSystemVendorId(&pThis->PciDev, pThis->pCodec->u16VendorId); /* 2c ro - intel.) */
    PCIDevSetSubSystemId(      &pThis->PciDev, pThis->pCodec->u16DeviceId); /* 2e ro. */

    hdaReset(pDevIns);
    pThis->pCodec->id = 0;
    pThis->pCodec->pfnTransfer = hdaTransfer;
    pThis->pCodec->pfnReset = hdaCodecReset;

    /*
     * 18.2.6,7 defines that values of this registers might be cleared on power on/reset
     * hdaReset shouldn't affects these registers.
     */
    HDA_REG(pThis, WAKEEN)   = 0x0;
    HDA_REG(pThis, STATESTS) = 0x0;
#endif
    /*
     * Debug and string formatter types.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "hda",         "HDA info. (hda [register case-insensitive])",    hdaInfo);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdastrm",     "HDA stream info. (hdastrm [stream number])",     hdaInfoStream);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcnodes",    "HDA codec nodes.",                               hdaInfoCodecNodes);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcselector", "HDA codec's selector states [node number].",     hdaInfoCodecSelector);

    rc = RTStrFormatTypeRegister("sdctl",   hdaFormatStrmCtl,   NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdsts",   hdaFormatStrmSts,   NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdfifos", hdaFormatStrmFifos, NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdfifow", hdaFormatStrmFifow, NULL);
    AssertRC(rc);
#if 0
    rc = RTStrFormatTypeRegister("sdfmt", printHdaStrmFmt, NULL);
    AssertRC(rc);
#endif

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

        /* The final entry is a full dword, no gaps! Allows shortcuts. */
        AssertReleaseMsg(pNextReg || ((pReg->offset + pReg->size) & 3) == 0,
                         ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
    }

    return VINF_SUCCESS;
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
    "VBoxDDGC.gc",
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
