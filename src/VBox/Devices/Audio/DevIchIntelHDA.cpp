/* $Id$ */
/** @file
 * DevIchIntelHD - VBox ICH Intel HD Audio Controller.
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
#include <VBox/version.h>

#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>

#include "VBoxDD.h"

extern "C" {
#include "audio.h"
}
#include "DevCodec.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
//#define HDA_AS_PCI_EXPRESS
#define VBOX_WITH_INTEL_HDA

#if defined(VBOX_WITH_HP_HDA)
/* HP Pavilion dv4t-1300 */
# define HDA_PCI_VENDOR_ID 0x103c
# define HDA_PCI_DEICE_ID 0x30f7
#elif defined(VBOX_WITH_INTEL_HDA)
/* Intel HDA controller */
# define HDA_PCI_VENDOR_ID 0x8086
# define HDA_PCI_DEICE_ID 0x2668
#elif defined(VBOX_WITH_NVIDIA_HDA)
/* nVidia HDA controller */
# define HDA_PCI_VENDOR_ID 0x10de
# define HDA_PCI_DEICE_ID 0x0ac0
#else
# error "Please specify your HDA device vendor/device IDs"
#endif

#define HDA_NREGS 112
/* Registers */
#define HDA_REG_IND_NAME(x)                 ICH6_HDA_REG_##x
#define HDA_REG_FIELD_NAME(reg, x)          ICH6_HDA_##reg##_##x
#define HDA_REG_FIELD_MASK(reg, x)          ICH6_HDA_##reg##_##x##_MASK
#define HDA_REG_FIELD_FLAG_MASK(reg, x)     RT_BIT(ICH6_HDA_##reg##_##x##_SHIFT)
#define HDA_REG_FIELD_SHIFT(reg, x)         ICH6_HDA_##reg##_##x##_SHIFT
#define HDA_REG_IND(pThis, x)               ((pThis)->au32Regs[(x)])
#define HDA_REG(pThis, x)                   (HDA_REG_IND((pThis), HDA_REG_IND_NAME(x)))
#define HDA_REG_VALUE(pThis, reg, val)      (HDA_REG((pThis),reg) & (((HDA_REG_FIELD_MASK(reg, val))) << (HDA_REG_FIELD_SHIFT(reg, val))))
#define HDA_REG_FLAG_VALUE(pThis, reg, val) (HDA_REG((pThis),reg) & (((HDA_REG_FIELD_FLAG_MASK(reg, val)))))
#define HDA_REG_SVALUE(pThis, reg, val)     (HDA_REG_VALUE(pThis, reg, val) >> (HDA_REG_FIELD_SHIFT(reg, val)))

#define ICH6_HDA_REG_GCAP 0 /* range 0x00-0x01*/
#define GCAP(pThis) (HDA_REG((pThis), GCAP))
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
#define ICH6_HDA_REG_VMIN           1 /* range 0x02 */
#define VMIN(pThis)                 (HDA_REG((pThis), VMIN))

#define ICH6_HDA_REG_VMAJ           2 /* range 0x03 */
#define VMAJ(pThis)                 (HDA_REG((pThis), VMAJ))

#define ICH6_HDA_REG_OUTPAY         3 /* range 0x04-0x05 */
#define OUTPAY(pThis)               (HDA_REG((pThis), OUTPAY))

#define ICH6_HDA_REG_INPAY          4 /* range 0x06-0x07 */
#define INPAY(pThis)                (HDA_REG((pThis), INPAY))

#define ICH6_HDA_REG_GCTL           (5)
#define ICH6_HDA_GCTL_RST_SHIFT     (0)
#define ICH6_HDA_GCTL_FSH_SHIFT     (1)
#define ICH6_HDA_GCTL_UR_SHIFT      (8)
#define GCTL(pThis)                 (HDA_REG((pThis), GCTL))

#define ICH6_HDA_REG_WAKEEN         6 /* 0x0C */
#define WAKEEN(pThis)               (HDA_REG((pThis), WAKEEN))

#define ICH6_HDA_REG_STATESTS       7 /* range 0x0E */
#define STATESTS(pThis)             (HDA_REG((pThis), STATESTS))
#define ICH6_HDA_STATES_SCSF        0x7

#define ICH6_HDA_REG_GSTS           8 /* range 0x10-0x11*/
#define ICH6_HDA_GSTS_FSH_SHIFT     (1)
#define GSTS(pThis)                 (HDA_REG(pThis, GSTS))

#define ICH6_HDA_REG_INTCTL         9 /* 0x20 */
#define ICH6_HDA_INTCTL_GIE_SHIFT   31
#define ICH6_HDA_INTCTL_CIE_SHIFT   30
#define ICH6_HDA_INTCTL_S0_SHIFT    (0)
#define ICH6_HDA_INTCTL_S1_SHIFT    (1)
#define ICH6_HDA_INTCTL_S2_SHIFT    (2)
#define ICH6_HDA_INTCTL_S3_SHIFT    (3)
#define ICH6_HDA_INTCTL_S4_SHIFT    (4)
#define ICH6_HDA_INTCTL_S5_SHIFT    (5)
#define ICH6_HDA_INTCTL_S6_SHIFT    (6)
#define ICH6_HDA_INTCTL_S7_SHIFT    (7)
#define INTCTL(pThis)               (HDA_REG((pThis), INTCTL))
#define INTCTL_GIE(pThis)           (HDA_REG_FLAG_VALUE(pThis, INTCTL, GIE))
#define INTCTL_CIE(pThis)           (HDA_REG_FLAG_VALUE(pThis, INTCTL, CIE))
#define INTCTL_SX(pThis, X)         (HDA_REG_FLAG_VALUE((pThis), INTCTL, S##X))
#define INTCTL_SALL(pThis)          (INTCTL((pThis)) & 0xFF)

/* Note: The HDA specification defines a SSYNC register at offset 0x38. The
 * ICH6/ICH9 datahseet defines SSYNC at offset 0x34. The Linux HDA driver matches
 * the datasheet.
 */
#define ICH6_HDA_REG_SSYNC          12 /* 0x34 */
#define SSYNC(pThis)                (HDA_REG((pThis), SSYNC))

#define ICH6_HDA_REG_INTSTS         10 /* 0x24 */
#define ICH6_HDA_INTSTS_GIS_SHIFT   (31)
#define ICH6_HDA_INTSTS_CIS_SHIFT   (30)
#define ICH6_HDA_INTSTS_S0_SHIFT    (0)
#define ICH6_HDA_INTSTS_S1_SHIFT    (1)
#define ICH6_HDA_INTSTS_S2_SHIFT    (2)
#define ICH6_HDA_INTSTS_S3_SHIFT    (3)
#define ICH6_HDA_INTSTS_S4_SHIFT    (4)
#define ICH6_HDA_INTSTS_S5_SHIFT    (5)
#define ICH6_HDA_INTSTS_S6_SHIFT    (6)
#define ICH6_HDA_INTSTS_S7_SHIFT    (7)
#define ICH6_HDA_INTSTS_S_MASK(num) RT_BIT(HDA_REG_FIELD_SHIFT(S##num))
#define INTSTS(pThis)               (HDA_REG((pThis), INTSTS))
#define INTSTS_GIS(pThis)           (HDA_REG_FLAG_VALUE((pThis), INTSTS, GIS)
#define INTSTS_CIS(pThis)           (HDA_REG_FLAG_VALUE((pThis), INTSTS, CIS)
#define INTSTS_SX(pThis, X)         (HDA_REG_FLAG_VALUE(pThis), INTSTS, S##X)
#define INTSTS_SANY(pThis)          (INTSTS((pThis)) & 0xFF)

#define ICH6_HDA_REG_CORBLBASE      13 /* 0x40 */
#define CORBLBASE(pThis)            (HDA_REG((pThis), CORBLBASE))
#define ICH6_HDA_REG_CORBUBASE      14 /* 0x44 */
#define CORBUBASE(pThis)            (HDA_REG((pThis), CORBUBASE))
#define ICH6_HDA_REG_CORBWP         15 /* 48 */
#define ICH6_HDA_REG_CORBRP         16 /* 4A */
#define ICH6_HDA_CORBRP_RST_SHIFT   15
#define ICH6_HDA_CORBRP_WP_SHIFT    0
#define ICH6_HDA_CORBRP_WP_MASK     0xFF

#define CORBRP(pThis)               (HDA_REG(pThis, CORBRP))
#define CORBWP(pThis)               (HDA_REG(pThis, CORBWP))

#define ICH6_HDA_REG_CORBCTL        17 /* 0x4C */
#define ICH6_HDA_CORBCTL_DMA_SHIFT  (1)
#define ICH6_HDA_CORBCTL_CMEIE_SHIFT (0)

#define CORBCTL(pThis)              (HDA_REG(pThis, CORBCTL))


#define ICH6_HDA_REG_CORBSTS        18 /* 0x4D */
#define CORBSTS(pThis)              (HDA_REG(pThis, CORBSTS))
#define ICH6_HDA_CORBSTS_CMEI_SHIFT (0)

#define ICH6_HDA_REG_CORBSIZE       19 /* 0x4E */
#define ICH6_HDA_CORBSIZE_SZ_CAP    0xF0
#define ICH6_HDA_CORBSIZE_SZ        0x3
#define CORBSIZE_SZ(pThis)          (HDA_REG(pThis, ICH6_HDA_REG_CORBSIZE) & ICH6_HDA_CORBSIZE_SZ)
#define CORBSIZE_SZ_CAP(pThis)      (HDA_REG(pThis, ICH6_HDA_REG_CORBSIZE) & ICH6_HDA_CORBSIZE_SZ_CAP)
/* till ich 10 sizes of CORB and RIRB are hardcoded to 256 in real hw */

#define ICH6_HDA_REG_RIRLBASE       20 /* 0x50 */
#define RIRLBASE(pThis)             (HDA_REG((pThis), RIRLBASE))

#define ICH6_HDA_REG_RIRUBASE       21 /* 0x54 */
#define RIRUBASE(pThis)             (HDA_REG((pThis), RIRUBASE))

#define ICH6_HDA_REG_RIRBWP         22 /* 0x58 */
#define ICH6_HDA_RIRBWP_RST_SHIFT   (15)
#define ICH6_HDA_RIRBWP_WP_MASK     0xFF
#define RIRBWP(pThis)              (HDA_REG(pThis, RIRBWP))

#define ICH6_HDA_REG_RINTCNT        23 /* 0x5A */
#define RINTCNT(pThis)              (HDA_REG((pThis), RINTCNT))
#define RINTCNT_N(pThis)            (RINTCNT((pThis)) & 0xff)

#define ICH6_HDA_REG_RIRBCTL        24 /* 0x5C */
#define ICH6_HDA_RIRBCTL_RIC_SHIFT  (0)
#define ICH6_HDA_RIRBCTL_DMA_SHIFT  (1)
#define ICH6_HDA_ROI_DMA_SHIFT      (2)
#define RIRBCTL(pThis)              (HDA_REG((pThis), RIRBCTL))
#define RIRBCTL_RIRB_RIC(pThis)     (HDA_REG_FLAG_VALUE(pThis, RIRBCTL, RIC))
#define RIRBCTL_RIRB_DMA(pThis)     (HDA_REG_FLAG_VALUE((pThis), RIRBCTL, DMA)
#define RIRBCTL_ROI(pThis)          (HDA_REG_FLAG_VALUE((pThis), RIRBCTL, ROI))

#define ICH6_HDA_REG_RIRBSTS        25 /* 0x5D */
#define ICH6_HDA_RIRBSTS_RINTFL_SHIFT (0)
#define ICH6_HDA_RIRBSTS_RIRBOIS_SHIFT (2)
#define RIRBSTS(pThis)              (HDA_REG(pThis, RIRBSTS))
#define RIRBSTS_RINTFL(pThis)       (HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RINTFL))
#define RIRBSTS_RIRBOIS(pThis)      (HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RIRBOIS))

#define ICH6_HDA_REG_RIRBSIZE       26 /* 0x5E */
#define ICH6_HDA_RIRBSIZE_SZ_CAP    0xF0
#define ICH6_HDA_RIRBSIZE_SZ        0x3

#define RIRBSIZE_SZ(pThis)          (HDA_REG(pThis, ICH6_HDA_REG_RIRBSIZE) & ICH6_HDA_RIRBSIZE_SZ)
#define RIRBSIZE_SZ_CAP(pThis)      (HDA_REG(pThis, ICH6_HDA_REG_RIRBSIZE) & ICH6_HDA_RIRBSIZE_SZ_CAP)


#define ICH6_HDA_REG_IC             27 /* 0x60 */
#define IC(pThis)                   (HDA_REG(pThis, IC))
#define ICH6_HDA_REG_IR             28 /* 0x64 */
#define IR(pThis)                   (HDA_REG(pThis, IR))
#define ICH6_HDA_REG_IRS            29 /* 0x68 */
#define ICH6_HDA_IRS_ICB_SHIFT      (0)
#define ICH6_HDA_IRS_IRV_SHIFT      (1)
#define IRS(pThis)                  (HDA_REG(pThis, IRS))
#define IRS_ICB(pThis)              (HDA_REG_FLAG_VALUE(pThis, IRS, ICB))
#define IRS_IRV(pThis)              (HDA_REG_FLAG_VALUE(pThis, IRS, IRV))

#define ICH6_HDA_REG_DPLBASE        30 /* 0x70 */
#define DPLBASE(pThis)              (HDA_REG((pThis), DPLBASE))
#define ICH6_HDA_REG_DPUBASE        31 /* 0x74 */
#define DPUBASE(pThis)              (HDA_REG((pThis), DPUBASE))
#define DPBASE_ENABLED              1
#define DPBASE_ADDR_MASK            (~(uint64_t)0x7f)

#define HDA_STREAM_REG_DEF(name, num)           (ICH6_HDA_REG_SD##num##name)
#define HDA_STREAM_REG(pThis, name, num)        (HDA_REG((pThis), N_(HDA_STREAM_REG_DEF(name, num))))
/* Note: sdnum here _MUST_ be stream reg number [0,7] */
#define HDA_STREAM_REG2(pThis, name, sdnum)     (HDA_REG_IND((pThis), ICH6_HDA_REG_SD0##name + (sdnum) * 10))

#define ICH6_HDA_REG_SD0CTL         32 /* 0x80 */
#define ICH6_HDA_REG_SD1CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 10) /* 0xA0 */
#define ICH6_HDA_REG_SD2CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 20) /* 0xC0 */
#define ICH6_HDA_REG_SD3CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 30) /* 0xE0 */
#define ICH6_HDA_REG_SD4CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 40) /* 0x100 */
#define ICH6_HDA_REG_SD5CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 50) /* 0x120 */
#define ICH6_HDA_REG_SD6CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 60) /* 0x140 */
#define ICH6_HDA_REG_SD7CTL         (HDA_STREAM_REG_DEF(CTL, 0) + 70) /* 0x160 */

#define SD(func, num)               SD##num##func
#define SDCTL(pThis, num)           HDA_REG((pThis), SD(CTL, num))
#define SDCTL_NUM(pThis, num)       ((SDCTL((pThis), num) & HDA_REG_FIELD_MASK(SDCTL,NUM)) >> HDA_REG_FIELD_SHIFT(SDCTL, NUM))
#define ICH6_HDA_SDCTL_NUM_MASK     (0xF)
#define ICH6_HDA_SDCTL_NUM_SHIFT    (20)
#define ICH6_HDA_SDCTL_DIR_SHIFT    (19)
#define ICH6_HDA_SDCTL_TP_SHIFT     (18)
#define ICH6_HDA_SDCTL_STRIPE_MASK  (0x3)
#define ICH6_HDA_SDCTL_STRIPE_SHIFT (16)
#define ICH6_HDA_SDCTL_DEIE_SHIFT   (4)
#define ICH6_HDA_SDCTL_FEIE_SHIFT   (3)
#define ICH6_HDA_SDCTL_ICE_SHIFT    (2)
#define ICH6_HDA_SDCTL_RUN_SHIFT    (1)
#define ICH6_HDA_SDCTL_SRST_SHIFT   (0)

#define ICH6_HDA_REG_SD0STS         33 /* 0x83 */
#define ICH6_HDA_REG_SD1STS         (HDA_STREAM_REG_DEF(STS, 0) + 10) /* 0xA3 */
#define ICH6_HDA_REG_SD2STS         (HDA_STREAM_REG_DEF(STS, 0) + 20) /* 0xC3 */
#define ICH6_HDA_REG_SD3STS         (HDA_STREAM_REG_DEF(STS, 0) + 30) /* 0xE3 */
#define ICH6_HDA_REG_SD4STS         (HDA_STREAM_REG_DEF(STS, 0) + 40) /* 0x103 */
#define ICH6_HDA_REG_SD5STS         (HDA_STREAM_REG_DEF(STS, 0) + 50) /* 0x123 */
#define ICH6_HDA_REG_SD6STS         (HDA_STREAM_REG_DEF(STS, 0) + 60) /* 0x143 */
#define ICH6_HDA_REG_SD7STS         (HDA_STREAM_REG_DEF(STS, 0) + 70) /* 0x163 */

#define SDSTS(pThis, num)           HDA_REG((pThis), SD(STS, num))
#define ICH6_HDA_SDSTS_FIFORDY_SHIFT (5)
#define ICH6_HDA_SDSTS_DE_SHIFT     (4)
#define ICH6_HDA_SDSTS_FE_SHIFT     (3)
#define ICH6_HDA_SDSTS_BCIS_SHIFT   (2)

#define ICH6_HDA_REG_SD0LPIB        34 /* 0x84 */
#define ICH6_HDA_REG_SD1LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 10) /* 0xA4 */
#define ICH6_HDA_REG_SD2LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 20) /* 0xC4 */
#define ICH6_HDA_REG_SD3LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 30) /* 0xE4 */
#define ICH6_HDA_REG_SD4LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 40) /* 0x104 */
#define ICH6_HDA_REG_SD5LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 50) /* 0x124 */
#define ICH6_HDA_REG_SD6LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 60) /* 0x144 */
#define ICH6_HDA_REG_SD7LPIB        (HDA_STREAM_REG_DEF(LPIB, 0) + 70) /* 0x164 */

#define SDLPIB(pThis, num)          HDA_REG((pThis), SD(LPIB, num))

#define ICH6_HDA_REG_SD0CBL         35 /* 0x88 */
#define ICH6_HDA_REG_SD1CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 10) /* 0xA8 */
#define ICH6_HDA_REG_SD2CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 20) /* 0xC8 */
#define ICH6_HDA_REG_SD3CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 30) /* 0xE8 */
#define ICH6_HDA_REG_SD4CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 40) /* 0x108 */
#define ICH6_HDA_REG_SD5CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 50) /* 0x128 */
#define ICH6_HDA_REG_SD6CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 60) /* 0x148 */
#define ICH6_HDA_REG_SD7CBL         (HDA_STREAM_REG_DEF(CBL, 0) + 70) /* 0x168 */

#define SDLCBL(pThis, num)          HDA_REG((pThis), SD(CBL, num))

#define ICH6_HDA_REG_SD0LVI         36 /* 0x8C */
#define ICH6_HDA_REG_SD1LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 10) /* 0xAC */
#define ICH6_HDA_REG_SD2LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 20) /* 0xCC */
#define ICH6_HDA_REG_SD3LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 30) /* 0xEC */
#define ICH6_HDA_REG_SD4LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 40) /* 0x10C */
#define ICH6_HDA_REG_SD5LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 50) /* 0x12C */
#define ICH6_HDA_REG_SD6LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 60) /* 0x14C */
#define ICH6_HDA_REG_SD7LVI         (HDA_STREAM_REG_DEF(LVI, 0) + 70) /* 0x16C */

#define SDLVI(pThis, num)           HDA_REG((pThis), SD(LVI, num))

#define ICH6_HDA_REG_SD0FIFOW       37 /* 0x8E */
#define ICH6_HDA_REG_SD1FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 10) /* 0xAE */
#define ICH6_HDA_REG_SD2FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 20) /* 0xCE */
#define ICH6_HDA_REG_SD3FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 30) /* 0xEE */
#define ICH6_HDA_REG_SD4FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 40) /* 0x10E */
#define ICH6_HDA_REG_SD5FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 50) /* 0x12E */
#define ICH6_HDA_REG_SD6FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 60) /* 0x14E */
#define ICH6_HDA_REG_SD7FIFOW       (HDA_STREAM_REG_DEF(FIFOW, 0) + 70) /* 0x16E */

/*
 * ICH6 datasheet defined limits for FIFOW values (18.2.38)
 */
#define HDA_SDFIFOW_8B              (0x2)
#define HDA_SDFIFOW_16B             (0x3)
#define HDA_SDFIFOW_32B             (0x4)
#define SDFIFOW(pThis, num)         HDA_REG((pThis), SD(FIFOW, num))

#define ICH6_HDA_REG_SD0FIFOS       38 /* 0x90 */
#define ICH6_HDA_REG_SD1FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 10) /* 0xB0 */
#define ICH6_HDA_REG_SD2FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 20) /* 0xD0 */
#define ICH6_HDA_REG_SD3FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 30) /* 0xF0 */
#define ICH6_HDA_REG_SD4FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 40) /* 0x110 */
#define ICH6_HDA_REG_SD5FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 50) /* 0x130 */
#define ICH6_HDA_REG_SD6FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 60) /* 0x150 */
#define ICH6_HDA_REG_SD7FIFOS       (HDA_STREAM_REG_DEF(FIFOS, 0) + 70) /* 0x170 */

/*
 * ICH6 datasheet defines limits for FIFOS registers (18.2.39)
 * formula: size - 1
 * Other values not listed are not supported.
 */
#define HDA_SDONFIFO_16B            (0x0F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_32B            (0x1F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_64B            (0x3F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_128B           (0x7F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_192B           (0xBF) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_256B           (0xFF) /* 20-, 24-bit Output Streams */
#define HDA_SDINFIFO_120B           (0x77) /* 8-, 16-, 20-, 24-, 32-bit Input Streams */
#define HDA_SDINFIFO_160B           (0x9F) /* 20-, 24-bit Input Streams Streams */
#define SDFIFOS(pThis, num)         HDA_REG((pThis), SD(FIFOS, num))

#define ICH6_HDA_REG_SD0FMT         39 /* 0x92 */
#define ICH6_HDA_REG_SD1FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 10) /* 0xB2 */
#define ICH6_HDA_REG_SD2FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 20) /* 0xD2 */
#define ICH6_HDA_REG_SD3FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 30) /* 0xF2 */
#define ICH6_HDA_REG_SD4FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 40) /* 0x112 */
#define ICH6_HDA_REG_SD5FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 50) /* 0x132 */
#define ICH6_HDA_REG_SD6FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 60) /* 0x152 */
#define ICH6_HDA_REG_SD7FMT         (HDA_STREAM_REG_DEF(FMT, 0) + 70) /* 0x172 */

#define SDFMT(pThis, num)           (HDA_REG((pThis), SD(FMT, num)))
#define ICH6_HDA_SDFMT_BASE_RATE_SHIFT (14)
#define ICH6_HDA_SDFMT_MULT_SHIFT   (11)
#define ICH6_HDA_SDFMT_MULT_MASK    (0x7)
#define ICH6_HDA_SDFMT_DIV_SHIFT    (8)
#define ICH6_HDA_SDFMT_DIV_MASK     (0x7)
#define ICH6_HDA_SDFMT_BITS_SHIFT   (4)
#define ICH6_HDA_SDFMT_BITS_MASK    (0x7)
#define SDFMT_BASE_RATE(pThis, num) ((SDFMT(pThis, num) & HDA_REG_FIELD_FLAG_MASK(SDFMT, BASE_RATE)) >> HDA_REG_FIELD_SHIFT(SDFMT, BASE_RATE))
#define SDFMT_MULT(pThis, num)      ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,MULT)) >> HDA_REG_FIELD_SHIFT(SDFMT, MULT))
#define SDFMT_DIV(pThis, num)       ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,DIV)) >> HDA_REG_FIELD_SHIFT(SDFMT, DIV))

#define ICH6_HDA_REG_SD0BDPL        40 /* 0x98 */
#define ICH6_HDA_REG_SD1BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 10) /* 0xB8 */
#define ICH6_HDA_REG_SD2BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 20) /* 0xD8 */
#define ICH6_HDA_REG_SD3BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 30) /* 0xF8 */
#define ICH6_HDA_REG_SD4BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 40) /* 0x118 */
#define ICH6_HDA_REG_SD5BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 50) /* 0x138 */
#define ICH6_HDA_REG_SD6BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 60) /* 0x158 */
#define ICH6_HDA_REG_SD7BDPL        (HDA_STREAM_REG_DEF(BDPL, 0) + 70) /* 0x178 */

#define SDBDPL(pThis, num)          HDA_REG((pThis), SD(BDPL, num))

#define ICH6_HDA_REG_SD0BDPU        41 /* 0x9C */
#define ICH6_HDA_REG_SD1BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 10) /* 0xBC */
#define ICH6_HDA_REG_SD2BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 20) /* 0xDC */
#define ICH6_HDA_REG_SD3BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 30) /* 0xFC */
#define ICH6_HDA_REG_SD4BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 40) /* 0x11C */
#define ICH6_HDA_REG_SD5BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 50) /* 0x13C */
#define ICH6_HDA_REG_SD6BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 60) /* 0x15C */
#define ICH6_HDA_REG_SD7BDPU        (HDA_STREAM_REG_DEF(BDPU, 0) + 70) /* 0x17C */

#define SDBDPU(pThis, num)          HDA_REG((pThis), SD(BDPU, num))


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

typedef struct INTELHDLinkState
{
    /** The PCI device structure. */
    PCIDevice               PciDev;
    /** Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** Pointer to the connector of the attached audio driver. */
    PPDMIAUDIOCONNECTOR     pDrv;
    /** Pointer to the attached audio driver. */
    PPDMIBASE               pDrvBase;
    /** The base interface for LUN\#0. */
    PDMIBASE                IBase;
    RTGCPHYS                MMIOBaseAddr;
    uint32_t                au32Regs[HDA_NREGS];
    HDABDLEDESC             StInBdle;
    HDABDLEDESC             StOutBdle;
    HDABDLEDESC             StMicBdle;
    /** Interrupt on completion */
    bool                    fCviIoc;
    uint64_t                u64CORBBase;
    uint64_t                u64RIRBBase;
    uint64_t                u64DPBase;
    /** pointer to CORB buf */
    uint32_t               *pu32CorbBuf;
    /** size in bytes of CORB buf */
    uint32_t                cbCorbBuf;
    /** pointer on RIRB buf */
    uint64_t               *pu64RirbBuf;
    /** size in bytes of RIRB buf */
    uint32_t                cbRirbBuf;
    /** indicates if HDA in reset. */
    bool                    fInReset;
    CODECState              Codec;
    /** 1.2.3.4.5.6.7. - someone please tell me what I'm counting! - .8.9.10... */
    uint8_t                 u8Counter;
    uint64_t                u64BaseTS;
} INTELHDLinkState, *PINTELHDLinkState;
/** ICH Intel HD Audio Controller state. */
typedef INTELHDLinkState HDASTATE;
/** Pointer to the ICH Intel HD Audio Controller state. */
typedef HDASTATE *PHDASTATE;

#define ICH6_HDASTATE_2_DEVINS(pINTELHD)   ((pINTELHD)->pDevIns)
#define PCIDEV_2_ICH6_HDASTATE(pPciDev) ((PHDASTATE)(pPciDev))

#define ISD0FMT_TO_AUDIO_SELECTOR(pThis) \
    ( AUDIO_FORMAT_SELECTOR(&(pThis)->Codec, In, SDFMT_BASE_RATE(pThis, 0), SDFMT_MULT(pThis, 0), SDFMT_DIV(pThis, 0)) )
#define OSD0FMT_TO_AUDIO_SELECTOR(pThis) \
    ( AUDIO_FORMAT_SELECTOR(&(pThis)->Codec, Out, SDFMT_BASE_RATE(pThis, 4), SDFMT_MULT(pThis, 4), SDFMT_DIV(pThis, 4)) )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static FNPDMDEVRESET hdaReset;

static int hdaRegReadUnimplemented(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteUnimplemented(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
static int hdaRegReadGCAP(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
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
static int hdaRegReadSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);

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

DECLINLINE(void) hdaInitTransferDescriptor(PHDASTATE pThis, PHDABDLEDESC pBdle, uint8_t u8Strm,
                                           PHDASTREAMTRANSFERDESC pStreamDesc);
static int hdaMMIORegLookup(PHDASTATE pThis, uint32_t offReg);
static void hdaFetchBdle(PHDASTATE pThis, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc);
#ifdef LOG_ENABLED
static void dump_bd(PHDASTATE pThis, PHDABDLEDESC pBdle, uint64_t u64BaseDMA);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/* see 302349 p 6.2*/
static const struct
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
    /** Abbreviated name. */
    const char *abbrev;
    /** Full name. */
    const char *name;
} g_aIchIntelHDRegMap[HDA_NREGS] =
{
    /* offset  size     read mask   write mask         read callback         write callback         abbrev      full name                     */
    /*-------  -------  ----------  ----------  -----------------------  ------------------------ ----------    ------------------------------*/
    { 0x00000, 0x00002, 0x0000FFFB, 0x00000000, hdaRegReadGCAP         , hdaRegWriteUnimplemented, "GCAP"      , "Global Capabilities" },
    { 0x00002, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimplemented, "VMIN"      , "Minor Version" },
    { 0x00003, 0x00001, 0x000000FF, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimplemented, "VMAJ"      , "Major Version" },
    { 0x00004, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimplemented, "OUTPAY"    , "Output Payload Capabilities" },
    { 0x00006, 0x00002, 0x0000FFFF, 0x00000000, hdaRegReadU16          , hdaRegWriteUnimplemented, "INPAY"     , "Input Payload Capabilities" },
    { 0x00008, 0x00004, 0x00000103, 0x00000103, hdaRegReadGCTL         , hdaRegWriteGCTL         , "GCTL"      , "Global Control" },
    { 0x0000c, 0x00002, 0x00007FFF, 0x00007FFF, hdaRegReadU16          , hdaRegWriteU16          , "WAKEEN"    , "Wake Enable" },
    { 0x0000e, 0x00002, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteSTATESTS     , "STATESTS"  , "State Change Status" },
    { 0x00010, 0x00002, 0xFFFFFFFF, 0x00000000, hdaRegReadUnimplemented, hdaRegWriteUnimplemented, "GSTS"      , "Global Status" },
    { 0x00020, 0x00004, 0xC00000FF, 0xC00000FF, hdaRegReadU32          , hdaRegWriteU32          , "INTCTL"    , "Interrupt Control" },
    { 0x00024, 0x00004, 0xC00000FF, 0x00000000, hdaRegReadINTSTS       , hdaRegWriteUnimplemented, "INTSTS"    , "Interrupt Status" },
    { 0x00030, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadWALCLK       , hdaRegWriteUnimplemented, "WALCLK"    , "Wall Clock Counter" },
    /// @todo r=michaln: Doesn't the SSYNC register need to actually stop the stream(s)?
    { 0x00034, 0x00004, 0x000000FF, 0x000000FF, hdaRegReadU32          , hdaRegWriteU32          , "SSYNC"     , "Stream Synchronization" },
    { 0x00040, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase         , "CORBLBASE" , "CORB Lower Base Address" },
    { 0x00044, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase         , "CORBUBASE" , "CORB Upper Base Address" },
    { 0x00048, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteCORBWP       , "CORBWP"    , "CORB Write Pointer" },
    { 0x0004A, 0x00002, 0x000000FF, 0x000080FF, hdaRegReadU8           , hdaRegWriteCORBRP       , "CORBRP"    , "CORB Read Pointer" },
    { 0x0004C, 0x00001, 0x00000003, 0x00000003, hdaRegReadU8           , hdaRegWriteCORBCTL      , "CORBCTL"   , "CORB Control" },
    { 0x0004D, 0x00001, 0x00000001, 0x00000001, hdaRegReadU8           , hdaRegWriteCORBSTS      , "CORBSTS"   , "CORB Status" },
    { 0x0004E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimplemented, "CORBSIZE"  , "CORB Size" },
    { 0x00050, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteBase         , "RIRBLBASE" , "RIRB Lower Base Address" },
    { 0x00054, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase         , "RIRBUBASE" , "RIRB Upper Base Address" },
    { 0x00058, 0x00002, 0x000000FF, 0x00008000, hdaRegReadU8,            hdaRegWriteRIRBWP       , "RIRBWP"    , "RIRB Write Pointer" },
    { 0x0005A, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteU16          , "RINTCNT"   , "Response Interrupt Count" },
    { 0x0005C, 0x00001, 0x00000007, 0x00000007, hdaRegReadU8           , hdaRegWriteU8           , "RIRBCTL"   , "RIRB Control" },
    { 0x0005D, 0x00001, 0x00000005, 0x00000005, hdaRegReadU8           , hdaRegWriteRIRBSTS      , "RIRBSTS"   , "RIRB Status" },
    { 0x0005E, 0x00001, 0x000000F3, 0x00000000, hdaRegReadU8           , hdaRegWriteUnimplemented, "RIRBSIZE"  , "RIRB Size" },
    { 0x00060, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "IC"        , "Immediate Command" },
    { 0x00064, 0x00004, 0x00000000, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteUnimplemented, "IR"        , "Immediate Response" },
    { 0x00068, 0x00004, 0x00000002, 0x00000002, hdaRegReadIRS          , hdaRegWriteIRS          , "IRS"       , "Immediate Command Status" },
    { 0x00070, 0x00004, 0xFFFFFFFF, 0xFFFFFF81, hdaRegReadU32          , hdaRegWriteBase         , "DPLBASE"   , "DMA Position Lower Base" },
    { 0x00074, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteBase         , "DPUBASE"   , "DMA Position Upper Base" },

    { 0x00080, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "ISD0CTL"  , "Input Stream Descriptor 0 (ICD0) Control" },
    { 0x00083, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "ISD0STS"  , "ISD0 Status" },
    { 0x00084, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "ISD0LPIB" , "ISD0 Link Position In Buffer" },
    { 0x00088, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "ISD0CBL"  , "ISD0 Cyclic Buffer Length" },
    { 0x0008C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "ISD0LVI"  , "ISD0 Last Valid Index" },
    { 0x0008E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "ISD0FIFOW", "ISD0 FIFO Watermark" },
    { 0x00090, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16          , "ISD0FIFOS", "ISD0 FIFO Size" },
    { 0x00092, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "ISD0FMT"  , "ISD0 Format" },
    { 0x00098, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "ISD0BDPL" , "ISD0 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x0009C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "ISD0BDPU" , "ISD0 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x000A0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "ISD1CTL"  , "Input Stream Descriptor 1 (ISD1) Control" },
    { 0x000A3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "ISD1STS"  , "ISD1 Status" },
    { 0x000A4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "ISD1LPIB" , "ISD1 Link Position In Buffer" },
    { 0x000A8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "ISD1CBL"  , "ISD1 Cyclic Buffer Length" },
    { 0x000AC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "ISD1LVI"  , "ISD1 Last Valid Index" },
    { 0x000AE, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "ISD1FIFOW", "ISD1 FIFO Watermark" },
    { 0x000B0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16          , "ISD1FIFOS", "ISD1 FIFO Size" },
    { 0x000B2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "ISD1FMT"  , "ISD1 Format" },
    { 0x000B8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "ISD1BDPL" , "ISD1 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x000BC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "ISD1BDPU" , "ISD1 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x000C0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "ISD2CTL"  , "Input Stream Descriptor 2 (ISD2) Control" },
    { 0x000C3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "ISD2STS"  , "ISD2 Status" },
    { 0x000C4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "ISD2LPIB" , "ISD2 Link Position In Buffer" },
    { 0x000C8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "ISD2CBL"  , "ISD2 Cyclic Buffer Length" },
    { 0x000CC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "ISD2LVI"  , "ISD2 Last Valid Index" },
    { 0x000CE, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "ISD2FIFOW", "ISD2 FIFO Watermark" },
    { 0x000D0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16          , "ISD2FIFOS", "ISD2 FIFO Size" },
    { 0x000D2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "ISD2FMT"  , "ISD2 Format" },
    { 0x000D8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "ISD2BDPL" , "ISD2 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x000DC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "ISD2BDPU" , "ISD2 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x000E0, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "ISD3CTL"  , "Input Stream Descriptor 3 (ISD3) Control" },
    { 0x000E3, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "ISD3STS"  , "ISD3 Status" },
    { 0x000E4, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "ISD3LPIB" , "ISD3 Link Position In Buffer" },
    { 0x000E8, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "ISD3CBL"  , "ISD3 Cyclic Buffer Length" },
    { 0x000EC, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "ISD3LVI"  , "ISD3 Last Valid Index" },
    { 0x000EE, 0x00002, 0x00000005, 0x00000005, hdaRegReadU16          , hdaRegWriteU16          , "ISD3FIFOW", "ISD3 FIFO Watermark" },
    { 0x000F0, 0x00002, 0x000000FF, 0x00000000, hdaRegReadU16          , hdaRegWriteU16          , "ISD3FIFOS", "ISD3 FIFO Size" },
    { 0x000F2, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "ISD3FMT"  , "ISD3 Format" },
    { 0x000F8, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "ISD3BDPL" , "ISD3 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x000FC, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "ISD3BDPU" , "ISD3 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x00100, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadSDCTL        , hdaRegWriteSDCTL        , "OSD0CTL"  , "Input Stream Descriptor 0 (OSD0) Control" },
    { 0x00103, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "OSD0STS"  , "OSD0 Status" },
    { 0x00104, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "OSD0LPIB" , "OSD0 Link Position In Buffer" },
    { 0x00108, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "OSD0CBL"  , "OSD0 Cyclic Buffer Length" },
    { 0x0010C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "OSD0LVI"  , "OSD0 Last Valid Index" },
    { 0x0010E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "OSD0FIFOW", "OSD0 FIFO Watermark" },
    { 0x00110, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS      , "OSD0FIFOS", "OSD0 FIFO Size" },
    { 0x00112, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "OSD0FMT"  , "OSD0 Format" },
    { 0x00118, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "OSD0BDPL" , "OSD0 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x0011C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "OSD0BDPU" , "OSD0 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x00120, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "OSD1CTL"  , "Input Stream Descriptor 0 (OSD1) Control" },
    { 0x00123, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "OSD1STS"  , "OSD1 Status" },
    { 0x00124, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "OSD1LPIB" , "OSD1 Link Position In Buffer" },
    { 0x00128, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "OSD1CBL"  , "OSD1 Cyclic Buffer Length" },
    { 0x0012C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "OSD1LVI"  , "OSD1 Last Valid Index" },
    { 0x0012E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "OSD1FIFOW", "OSD1 FIFO Watermark" },
    { 0x00130, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS      , "OSD1FIFOS", "OSD1 FIFO Size" },
    { 0x00132, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "OSD1FMT"  , "OSD1 Format" },
    { 0x00138, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "OSD1BDPL" , "OSD1 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x0013C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "OSD1BDPU" , "OSD1 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x00140, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "OSD2CTL"  , "Input Stream Descriptor 0 (OSD2) Control" },
    { 0x00143, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "OSD2STS"  , "OSD2 Status" },
    { 0x00144, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "OSD2LPIB" , "OSD2 Link Position In Buffer" },
    { 0x00148, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "OSD2CBL"  , "OSD2 Cyclic Buffer Length" },
    { 0x0014C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "OSD2LVI"  , "OSD2 Last Valid Index" },
    { 0x0014E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "OSD2FIFOW", "OSD2 FIFO Watermark" },
    { 0x00150, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS      , "OSD2FIFOS", "OSD2 FIFO Size" },
    { 0x00152, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "OSD2FMT"  , "OSD2 Format" },
    { 0x00158, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "OSD2BDPL" , "OSD2 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x0015C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "OSD2BDPU" , "OSD2 Buffer Descriptor List Pointer-Upper Base Address" },

    { 0x00160, 0x00003, 0x00FF001F, 0x00F0001F, hdaRegReadU24          , hdaRegWriteSDCTL        , "OSD3CTL"  , "Input Stream Descriptor 0 (OSD3) Control" },
    { 0x00163, 0x00001, 0x0000001C, 0x0000003C, hdaRegReadU8           , hdaRegWriteSDSTS        , "OSD3STS"  , "OSD3 Status" },
    { 0x00164, 0x00004, 0xFFFFFFFF, 0x00000000, hdaRegReadU32          , hdaRegWriteU32          , "OSD3LPIB" , "OSD3 Link Position In Buffer" },
    { 0x00168, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteU32          , "OSD3CBL"  , "OSD3 Cyclic Buffer Length" },
    { 0x0016C, 0x00002, 0x0000FFFF, 0x0000FFFF, hdaRegReadU16          , hdaRegWriteSDLVI        , "OSD3LVI"  , "OSD3 Last Valid Index" },
    { 0x0016E, 0x00002, 0x00000007, 0x00000007, hdaRegReadU16          , hdaRegWriteSDFIFOW      , "OSD3FIFOW", "OSD3 FIFO Watermark" },
    { 0x00170, 0x00002, 0x000000FF, 0x000000FF, hdaRegReadU16          , hdaRegWriteSDFIFOS      , "OSD3FIFOS", "OSD3 FIFO Size" },
    { 0x00172, 0x00002, 0x00007F7F, 0x00007F7F, hdaRegReadU16          , hdaRegWriteSDFMT        , "OSD3FMT"  , "OSD3 Format" },
    { 0x00178, 0x00004, 0xFFFFFF80, 0xFFFFFF80, hdaRegReadU32          , hdaRegWriteSDBDPL       , "OSD3BDPL" , "OSD3 Buffer Descriptor List Pointer-Lower Base Address" },
    { 0x0017C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, hdaRegReadU32          , hdaRegWriteSDBDPU       , "OSD3BDPU" , "OSD3 Buffer Descriptor List Pointer-Upper Base Address" },
};

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


DECLINLINE(void) hdaUpdatePosBuf(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    if (pThis->u64DPBase & DPBASE_ENABLED)
        PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pThis),
                           (pThis->u64DPBase & DPBASE_ADDR_MASK) + pStreamDesc->u8Strm * 8,
                           pStreamDesc->pu32Lpib, sizeof(uint32_t));
}
DECLINLINE(uint32_t) hdaFifoWToSz(PHDASTATE pThis, PHDASTREAMTRANSFERDESC pStreamDesc)
{
#if 0
    switch(HDA_STREAM_REG2(pThis, FIFOW, pStreamDesc->u8Strm))
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
    if (   INTCTL_CIE(pThis)
       && (   RIRBSTS_RINTFL(pThis)
           || RIRBSTS_RIRBOIS(pThis)
           || (STATESTS(pThis) & WAKEEN(pThis))))
        fIrq = true;

    if (   IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 0)
        || IS_INTERRUPT_OCCURED_AND_ENABLED(pThis, 4))
        fIrq = true;

    if (INTCTL_GIE(pThis))
    {
        Log(("hda: irq %s\n", fIrq ? "asserted" : "deasserted"));
        PDMDevHlpPCISetIrq(ICH6_HDASTATE_2_DEVINS(pThis), 0 , fIrq);
    }
    return VINF_SUCCESS;
}

static int hdaMMIORegLookup(PHDASTATE pThis, uint32_t offReg)
{
    /*
     * Aliases HDA spec 3.3.45
     */
    switch (offReg)
    {
        case 0x2084:
            return HDA_REG_IND_NAME(SD0LPIB);
        case 0x20A4:
            return HDA_REG_IND_NAME(SD1LPIB);
        case 0x20C4:
            return HDA_REG_IND_NAME(SD2LPIB);
        case 0x20E4:
            return HDA_REG_IND_NAME(SD3LPIB);
        case 0x2104:
            return HDA_REG_IND_NAME(SD4LPIB);
        case 0x2124:
            return HDA_REG_IND_NAME(SD5LPIB);
        case 0x2144:
            return HDA_REG_IND_NAME(SD6LPIB);
        case 0x2164:
            return HDA_REG_IND_NAME(SD7LPIB);
    }

    /*
     * Binary search the
     */
    int idxHigh = RT_ELEMENTS(g_aIchIntelHDRegMap);
    int idxLow  = 0;
    for (;;)
    {
#ifdef DEBUG_vvl
            Assert(   idxHigh >= 0
                   && idxLow  >= 0);
#endif
            if (   idxHigh < idxLow
                || idxHigh < 0)
                break;
            int idxMiddle = idxLow + (idxHigh - idxLow) / 2;
            if (offReg < g_aIchIntelHDRegMap[idxMiddle].offset)
                idxHigh = idxMiddle - 1;
            else if (offReg >= g_aIchIntelHDRegMap[idxMiddle].offset + g_aIchIntelHDRegMap[idxMiddle].size)
                idxLow  = idxMiddle + 1;
            else if (   offReg >= g_aIchIntelHDRegMap[idxMiddle].offset
                     && offReg < g_aIchIntelHDRegMap[idxMiddle].offset + g_aIchIntelHDRegMap[idxMiddle].size)
                return idxMiddle;
    }
    return -1;
}

static int hdaCmdSync(PHDASTATE pThis, bool fLocal)
{
    int rc = VINF_SUCCESS;
    if (fLocal)
    {
        Assert((HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA)));
        rc = PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pThis), pThis->u64CORBBase, pThis->pu32CorbBuf, pThis->cbCorbBuf);
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
                if ((i + j) == CORBRP(pThis))
                    prefix = "[R]";
                else if ((i + j) == CORBWP(pThis))
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
        rc = PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pThis), pThis->u64RIRBBase, pThis->pu64RirbBuf, pThis->cbRirbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
#ifdef DEBUG_CMD_BUFFER
        uint8_t i = 0;
        do {
            Log(("hda: rirb%02x: ", i));
            uint8_t j = 0;
            do {
                const char *prefix;
                if ((i + j) == RIRBWP(pThis))
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

    PFNCODECVERBPROCESSOR pfn = (PFNCODECVERBPROCESSOR)NULL;

    rc = hdaCmdSync(pThis, true);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    corbRp = CORBRP(pThis);
    corbWp = CORBWP(pThis);
    rirbWp = RIRBWP(pThis);
    Assert((corbWp != corbRp));
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", CORBRP(pThis), CORBWP(pThis), RIRBWP(pThis)));
    while (corbRp != corbWp)
    {
        uint32_t cmd;
        uint64_t resp;
        pfn = (PFNCODECVERBPROCESSOR)NULL;
        corbRp++;
        cmd = pThis->pu32CorbBuf[corbRp];
        rc = pThis->Codec.pfnLookup(&pThis->Codec, cmd, &pfn);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Assert(pfn);
        (rirbWp)++;

        if (RT_LIKELY(pfn))
            rc = pfn(&pThis->Codec, cmd, &resp);
        else
            rc = VERR_INVALID_FUNCTION;

        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Log(("hda: verb:%08x->%016lx\n", cmd, resp));
        if (   (resp & CODEC_RESPONSE_UNSOLICITED)
            && !HDA_REG_FLAG_VALUE(pThis, GCTL, UR))
        {
            Log(("hda: unexpected unsolicited response.\n"));
            pThis->au32Regs[ICH6_HDA_REG_CORBRP] = corbRp;
            return rc;
        }
        pThis->pu64RirbBuf[rirbWp] = resp;
        pThis->u8Counter++;
        if (pThis->u8Counter == RINTCNT_N(pThis))
            break;
    }
    pThis->au32Regs[ICH6_HDA_REG_CORBRP] = corbRp;
    pThis->au32Regs[ICH6_HDA_REG_RIRBWP] = rirbWp;
    rc = hdaCmdSync(pThis, false);
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", CORBRP(pThis), CORBWP(pThis), RIRBWP(pThis)));
    if (RIRBCTL_RIRB_RIC(pThis))
    {
        RIRBSTS((pThis)) |= HDA_REG_FIELD_FLAG_MASK(RIRBSTS,RINTFL);
        pThis->u8Counter = 0;
        rc = hdaProcessInterrupt(pThis);
    }
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    return rc;
}

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
    HDA_STREAM_REG2(pThis, CTL, u8Strm) = 0x40000 | (HDA_STREAM_REG2(pThis, CTL, u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    /* ICH6 defines default values (0x77 for input and 0xBF for output descriptors) of FIFO size. 18.2.39 */
    HDA_STREAM_REG2(pThis, FIFOS, u8Strm) =  u8Strm < 4 ? HDA_SDINFIFO_120B : HDA_SDONFIFO_192B;
    HDA_STREAM_REG2(pThis, FIFOW, u8Strm) = u8Strm < 4 ? HDA_SDFIFOW_8B : HDA_SDFIFOW_32B;
    HDA_STREAM_REG2(pThis, CBL, u8Strm) = 0;
    HDA_STREAM_REG2(pThis, LVI, u8Strm) = 0;
    HDA_STREAM_REG2(pThis, FMT, u8Strm) = 0;
    HDA_STREAM_REG2(pThis, BDPU, u8Strm) = 0;
    HDA_STREAM_REG2(pThis, BDPL, u8Strm) = 0;
    Log(("hda: reset of stream (%d) finished\n", u8Strm));
}


/* Register access handlers. */

static int hdaRegReadUnimplemented(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    *pu32Value = 0;
    return VINF_SUCCESS;
}

static int hdaRegWriteUnimplemented(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return VINF_SUCCESS;
}

/* U8 */
static int hdaRegReadU8(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[iReg] & g_aIchIntelHDRegMap[iReg].readable) & 0xffffff00) == 0);
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
    Assert(((pThis->au32Regs[iReg] & g_aIchIntelHDRegMap[iReg].readable) & 0xffff0000) == 0);
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
    Assert(((pThis->au32Regs[iReg] & g_aIchIntelHDRegMap[iReg].readable) & 0xff000000) == 0);
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
    *pu32Value = pThis->au32Regs[iReg] & g_aIchIntelHDRegMap[iReg].readable;
    return VINF_SUCCESS;
}

static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    pThis->au32Regs[iReg]  = (u32Value & g_aIchIntelHDRegMap[iReg].writable)
                           | (pThis->au32Regs[iReg] & ~g_aIchIntelHDRegMap[iReg].writable);
    return VINF_SUCCESS;
}

static int hdaRegReadGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    return hdaRegReadU32(pThis, iReg, pu32Value);
}

static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, RST))
    {
        /* exit reset state */
        GCTL(pThis) |= HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = false;
    }
    else
    {
        /* enter reset state*/
        if (   HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA)
            || HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA))
        {
            Log(("hda: HDA enters in reset with DMA(RIRB:%s, CORB:%s)\n",
                HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) ? "on" : "off",
                HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA) ? "on" : "off"));
        }
        hdaReset(ICH6_HDASTATE_2_DEVINS(pThis));
        GCTL(pThis) &= ~HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = true;
    }
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, FSH))
    {
        /* Flush: GSTS:1 set,  see 6.2.6*/
        GSTS(pThis) |= HDA_REG_FIELD_FLAG_MASK(GSTS, FSH); /* set the flush state */
        /* DPLBASE and DPUBASE should be initialized with initial value (see 6.2.6)*/
    }
    return VINF_SUCCESS;
}

static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t v = pThis->au32Regs[iReg];
    uint32_t nv = u32Value & ICH6_HDA_STATES_SCSF;
    pThis->au32Regs[iReg] &= ~(v & nv); /* write of 1 clears corresponding bit */
    return VINF_SUCCESS;
}

static int hdaRegReadINTSTS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t v = 0;
    if (   RIRBSTS_RIRBOIS(pThis)
        || RIRBSTS_RINTFL(pThis)
        || HDA_REG_FLAG_VALUE(pThis, CORBSTS, CMEI)
        || STATESTS(pThis))
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
    *pu32Value = (uint32_t)ASMMultU64ByU32DivByU32(PDMDevHlpTMTimeVirtGetNano(ICH6_HDASTATE_2_DEVINS(pThis))
                                                   - pThis->u64BaseTS, 24, 1000);
    return VINF_SUCCESS;
}

static int hdaRegReadGCAP(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    return hdaRegReadU16(pThis, iReg, pu32Value);
}

static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(CORBRP, RST))
        CORBRP(pThis) = 0;
    else
        return hdaRegWriteU8(pThis, iReg, u32Value);
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = hdaRegWriteU8(pThis, iReg, u32Value);
    AssertRC(rc);
    if (   CORBWP(pThis) != CORBRP(pThis)
        && HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) != 0)
        return hdaCORBCmdProcess(pThis);
    return rc;
}

static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t v = CORBSTS(pThis);
    CORBSTS(pThis) &= ~(v & u32Value);
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc;
    rc = hdaRegWriteU16(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    if (CORBWP(pThis) == CORBRP(pThis))
        return VINF_SUCCESS;
    if (!HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA))
        return VINF_SUCCESS;
    rc = hdaCORBCmdProcess(pThis);
    return rc;
}

static int hdaRegReadSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    return hdaRegReadU24(pThis, iReg, pu32Value);
}

static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    bool fRun     = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fInRun   = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fReset   = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    bool fInReset = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    int rc = VINF_SUCCESS;
    if (fInReset)
    {
        /* Assert!!! Guest is resetting HDA's stream, we're expecting guest will mark stream as exit
         * from reset
         */
        Assert((!fReset));
        Log(("hda: guest initiated exit of stream reset.\n"));
        goto l_done;
    }
    else if (fReset)
    {
        /*
         * Assert!!! ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset.
         */
        uint8_t u8Strm = 0;
        PHDABDLEDESC pBdle = NULL;
        HDASTREAMTRANSFERDESC StreamDesc;
        Assert((!fInRun && !fRun));
        switch (iReg)
        {
            case ICH6_HDA_REG_SD0CTL:
                u8Strm = 0;
                pBdle = &pThis->StInBdle;
                break;
            case ICH6_HDA_REG_SD4CTL:
                u8Strm = 4;
                pBdle = &pThis->StOutBdle;
                break;
            default:
                Log(("hda: changing SRST bit on non-attached stream\n"));
                goto l_done;
        }
        Log(("hda: guest initiated enter to stream reset.\n"));
        hdaInitTransferDescriptor(pThis, pBdle, u8Strm, &StreamDesc);
        hdaStreamReset(pThis, pBdle, &StreamDesc, u8Strm);
        goto l_done;
    }

    /* we enter here to change DMA states only */
    if (   (fInRun && !fRun)
        || (fRun && !fInRun))
    {
        Assert((!fReset && !fInReset));
        switch (iReg)
        {
            case ICH6_HDA_REG_SD0CTL:
                AUD_set_active_in(pThis->Codec.SwVoiceIn, fRun);
                break;
            case ICH6_HDA_REG_SD4CTL:
                AUD_set_active_out(pThis->Codec.SwVoiceOut, fRun);
                break;
            default:
                Log(("hda: changing RUN bit on non-attached stream\n"));
                goto l_done;
        }
    }

l_done:
    rc = hdaRegWriteU24(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
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
        case ICH6_HDA_REG_SD0FIFOS:
        case ICH6_HDA_REG_SD1FIFOS:
        case ICH6_HDA_REG_SD2FIFOS:
        case ICH6_HDA_REG_SD3FIFOS:
            Log(("hda: Guest tries change value of FIFO size of Input Stream\n"));
            return VINF_SUCCESS;
        case ICH6_HDA_REG_SD4FIFOS:
        case ICH6_HDA_REG_SD5FIFOS:
        case ICH6_HDA_REG_SD6FIFOS:
        case ICH6_HDA_REG_SD7FIFOS:
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

static void hdaSdFmtToAudSettings(uint32_t u32SdFmt, audsettings_t *pAudSetting)
{
    Assert((pAudSetting));
#define EXTRACT_VALUE(v, mask, shift) ((v & ((mask) << (shift))) >> (shift))
    uint32_t u32Hz = (u32SdFmt & ICH6_HDA_SDFMT_BASE_RATE_SHIFT) ? 44100 : 48000;
    uint32_t u32HzMult = 1;
    uint32_t u32HzDiv = 1;
    switch (EXTRACT_VALUE(u32SdFmt, ICH6_HDA_SDFMT_MULT_MASK, ICH6_HDA_SDFMT_MULT_SHIFT))
    {
        case 0: u32HzMult = 1; break;
        case 1: u32HzMult = 2; break;
        case 2: u32HzMult = 3; break;
        case 3: u32HzMult = 4; break;
        default:
            Log(("hda: unsupported multiplier %x\n", u32SdFmt));
    }
    switch (EXTRACT_VALUE(u32SdFmt, ICH6_HDA_SDFMT_DIV_MASK, ICH6_HDA_SDFMT_DIV_SHIFT))
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
    pAudSetting->freq = u32Hz * u32HzMult / u32HzDiv;

    switch (EXTRACT_VALUE(u32SdFmt, ICH6_HDA_SDFMT_BITS_MASK, ICH6_HDA_SDFMT_BITS_SHIFT))
    {
        case 0:
            Log(("hda: %s requested 8-bit\n", __FUNCTION__));
            pAudSetting->fmt = AUD_FMT_S8;
            break;
        case 1:
            Log(("hda: %s requested 16-bit\n", __FUNCTION__));
            pAudSetting->fmt = AUD_FMT_S16;
            break;
        case 2:
            Log(("hda: %s requested 20-bit\n", __FUNCTION__));
            break;
        case 3:
            Log(("hda: %s requested 24-bit\n", __FUNCTION__));
            break;
        case 4:
            Log(("hda: %s requested 32-bit\n", __FUNCTION__));
            pAudSetting->fmt = AUD_FMT_S32;
            break;
        default:
            AssertMsgFailed(("Unsupported"));
    }
    pAudSetting->nchannels = (u32SdFmt & 0xf) + 1;
    pAudSetting->fmt = AUD_FMT_S16;
    pAudSetting->endianness = 0;
#undef EXTRACT_VALUE
}

static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef VBOX_WITH_HDA_CODEC_EMU
    /* @todo a bit more investigation is required here. */
    int rc = 0;
    audsettings_t as;
    /* no reason to reopen voice with same settings */
    if (u32Value == HDA_REG_IND(pThis, iReg))
        return VINF_SUCCESS;
    hdaSdFmtToAudSettings(u32Value, &as);
    switch (iReg)
    {
        case ICH6_HDA_REG_SD0FMT:
            rc = codecOpenVoice(&pThis->Codec, PI_INDEX, &as);
            break;
        case ICH6_HDA_REG_SD4FMT:
            rc = codecOpenVoice(&pThis->Codec, PO_INDEX, &as);
            break;
        default:
            Log(("HDA: attempt to change format on %d\n", iReg));
            rc = 0;
    }
    return hdaRegWriteU16(pThis, iReg, u32Value);
#else
    return hdaRegWriteU16(pThis, iReg, u32Value);
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
    if (   CORBWP(pThis) != CORBRP(pThis)
        || HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA))
        IRS(pThis) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */

    rc = hdaRegReadU32(pThis, iReg, pu32Value);
    return rc;
}

static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;
    uint64_t resp;
    PFNCODECVERBPROCESSOR pfn = (PFNCODECVERBPROCESSOR)NULL;
    /*
     * if guest set the ICB bit of IRS register, HDA should process the verb in IC register,
     * write the response to IR register, and set the IRV (valid in case of success) bit of IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, ICB)
        && !IRS_ICB(pThis))
    {
        uint32_t cmd = IC(pThis);
        if (CORBWP(pThis) != CORBRP(pThis))
        {
            /*
             * 3.4.3 defines behavior of immediate Command status register.
             */
            LogRel(("hda: guest attempted process immediate verb (%x) with active CORB\n", cmd));
            return rc;
        }
        IRS(pThis) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */
        Log(("hda: IC:%x\n", cmd));
        rc = pThis->Codec.pfnLookup(&pThis->Codec, cmd, &pfn);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        rc = pfn(&pThis->Codec, cmd, &resp);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        IR(pThis) = (uint32_t)resp;
        Log(("hda: IR:%x\n", IR(pThis)));
        IRS(pThis) = HDA_REG_FIELD_FLAG_MASK(IRS, IRV);  /* result is ready  */
        IRS(pThis) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, ICB); /* busy is clear */
        return rc;
    }
    /*
     * Once the guest read the response, it should clean the IRV bit of the IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, IRV)
        && IRS_IRV(pThis))
        IRS(pThis) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, IRV);
    return rc;
}

static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(RIRBWP, RST))
    {
        RIRBWP(pThis) = 0;
    }
    /* The remaining bits are O, see 6.2.22 */
    return VINF_SUCCESS;
}

static int hdaRegWriteBase(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pThis, iReg, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    switch(iReg)
    {
        case ICH6_HDA_REG_CORBLBASE:
            pThis->u64CORBBase &= 0xFFFFFFFF00000000ULL;
            pThis->u64CORBBase |= pThis->au32Regs[iReg];
            break;
        case ICH6_HDA_REG_CORBUBASE:
            pThis->u64CORBBase &= 0x00000000FFFFFFFFULL;
            pThis->u64CORBBase |= ((uint64_t)pThis->au32Regs[iReg] << 32);
            break;
        case ICH6_HDA_REG_RIRLBASE:
            pThis->u64RIRBBase &= 0xFFFFFFFF00000000ULL;
            pThis->u64RIRBBase |= pThis->au32Regs[iReg];
            break;
        case ICH6_HDA_REG_RIRUBASE:
            pThis->u64RIRBBase &= 0x00000000FFFFFFFFULL;
            pThis->u64RIRBBase |= ((uint64_t)pThis->au32Regs[iReg] << 32);
            break;
        case ICH6_HDA_REG_DPLBASE:
            /* @todo: first bit has special meaning */
            pThis->u64DPBase &= 0xFFFFFFFF00000000ULL;
            pThis->u64DPBase |= pThis->au32Regs[iReg];
            break;
        case ICH6_HDA_REG_DPUBASE:
            pThis->u64DPBase &= 0x00000000FFFFFFFFULL;
            pThis->u64DPBase |= ((uint64_t)pThis->au32Regs[iReg] << 32);
            break;
        default:
            AssertMsgFailed(("Invalid index"));
    }
    Log(("hda: CORB base:%llx RIRB base: %llx DP base: %llx\n", pThis->u64CORBBase, pThis->u64RIRBBase, pThis->u64DPBase));
    return rc;
}

static int hdaRegWriteRIRBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint8_t v = RIRBSTS(pThis);
    RIRBSTS(pThis) &= ~(v & u32Value);

    return hdaProcessInterrupt(pThis);
}

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
        PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pThis), u64BaseDMA + i*16, bdle, 16);
        addr = *(uint64_t *)bdle;
        len = *(uint32_t *)&bdle[8];
        ioc = *(uint32_t *)&bdle[12];
        Log(("hda: %s bdle[%d] a:%llx, len:%d, ioc:%d\n",  (i == pBdle->u32BdleCvi? "[C]": "   "), i, addr, len, ioc & 0x1));
        sum += len;
    }
    Log(("hda: sum: %d\n", sum));
    for (i = 0; i < 8; ++i)
    {
        PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pThis), (pThis->u64DPBase & DPBASE_ADDR_MASK) + i*8, &counter, sizeof(&counter));
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
    PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pThis), pStreamDesc->u64BaseDMA + pBdle->u32BdleCvi*16, bdle, 16);
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
            /*
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
    {
        /* if we enter here we can't report "unreported bits" */
        *fStop = true;
        goto l_done;
    }


    /*
     * read from backend input line to the last unreported position or at the begining.
     */
    cbBackendCopy = AUD_read(pThis->Codec.SwVoiceIn, pBdle->au8HdaBuffer, cb2Copy);
    /*
     * write the HDA DMA buffer
     */
    PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pThis), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer, cbBackendCopy);

    /* Don't see any reason why cb2Copy would differ from cbBackendCopy */
    Assert((cbBackendCopy == cb2Copy && (*pu32Avail) >= cb2Copy)); /* sanity */

    if (pBdle->cbUnderFifoW + cbBackendCopy > hdaFifoWToSz(pThis, 0))
        hdaBackendReadTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransferred, pu32Avail);
    else
    {
        hdaBackendTransferUnreported(pThis, pBdle, pStreamDesc, cbBackendCopy, pu32Avail);
        *fStop = true;
    }
l_done:
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
    {
        *fStop = true;
        goto l_done;
    }

    PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pThis), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer + pBdle->cbUnderFifoW, cb2Copy);
    /*
     * Write to audio backend. we should ensure that we have enough bytes to copy to the backend.
     */
    if (cb2Copy + pBdle->cbUnderFifoW >= hdaFifoWToSz(pThis, pStreamDesc))
    {
        /*
         * Feed the newly fetched samples, including unreported ones, to the backend.
         */
        cbBackendCopy = AUD_write (pThis->Codec.SwVoiceOut, pBdle->au8HdaBuffer, cb2Copy + pBdle->cbUnderFifoW);
        hdaBackendWriteTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransferred, pu32Avail);
    }
    else
    {
        /* Not enough bytes to be processed and reported, we'll try our luck next time around */
        hdaBackendTransferUnreported(pThis, pBdle, pStreamDesc, cb2Copy, NULL);
        *fStop = true;
    }

l_done:
    Assert(cbTransferred <= SDFIFOS(pThis, 4) + 1);
    Log(("hda:wa: CVI(pos:%d, len:%d, cbTransferred:%d)\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, cbTransferred));
    return cbTransferred;
}

/**
 * @interface_method_impl{HDACODEC,pfnReset}
 */
DECLCALLBACK(int) hdaCodecReset(CODECState *pCodecState)
{
    PHDASTATE pThis = (INTELHDLinkState *)pCodecState->pvHDAState;
    return VINF_SUCCESS;
}

DECLINLINE(void) hdaInitTransferDescriptor(PHDASTATE pThis, PHDABDLEDESC pBdle, uint8_t u8Strm,
                                           PHDASTREAMTRANSFERDESC pStreamDesc)
{
    Assert(pThis); Assert(pBdle); Assert(pStreamDesc); Assert(u8Strm <= 7);

    memset(pStreamDesc, 0, sizeof(HDASTREAMTRANSFERDESC));
    pStreamDesc->u8Strm     = u8Strm;
    pStreamDesc->u32Ctl     = HDA_STREAM_REG2(pThis, CTL, u8Strm);
    pStreamDesc->u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG2(pThis, BDPL, u8Strm),
                                          HDA_STREAM_REG2(pThis, BDPU, u8Strm));
    pStreamDesc->pu32Lpib   = &HDA_STREAM_REG2(pThis, LPIB, u8Strm);
    pStreamDesc->pu32Sts    = &HDA_STREAM_REG2(pThis, STS, u8Strm);
    pStreamDesc->u32Cbl     = HDA_STREAM_REG2(pThis, CBL, u8Strm);
    pStreamDesc->u32Fifos   = HDA_STREAM_REG2(pThis, FIFOS, u8Strm);

    pBdle->u32BdleMaxCvi    = HDA_STREAM_REG2(pThis, LVI, u8Strm);

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
static DECLCALLBACK(void) hdaTransfer(CODECState *pCodecState, ENMSOUNDSOURCE src, int avail)
{
    PHDASTATE pThis = (INTELHDLinkState *)pCodecState->pvHDAState;
    uint8_t                 u8Strm = 0;
    PHDABDLEDESC            pBdle = NULL;

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
    uint32_t    offReg = GCPhysAddr - pThis->MMIOBaseAddr;
    int         idxReg = hdaMMIORegLookup(pThis, offReg);
    int         rc;
    Assert(!(offReg & 3)); Assert(cb == 4);

    if (pThis->fInReset && idxReg != ICH6_HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));

    if (idxReg == -1)
        LogRel(("hda: Invalid read access @0x%x(of bytes:%d)\n", offReg, cb));

    if (idxReg != -1)
    {
        rc = g_aIchIntelHDRegMap[idxReg].pfnRead(pThis, idxReg, (uint32_t *)pv);
        Log(("hda: read %s[%x/%x]\n", g_aIchIntelHDRegMap[idxReg].abbrev, *(uint32_t *)pv));
    }
    else
    {
        rc = VINF_IOM_MMIO_UNUSED_FF;
        Log(("hda: hole at %x is accessed for read\n", offReg));
    }
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE, Looks up and calls the appropriate handler.}
 */
PDMBOTHCBDECL(int) hdaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PHDASTATE   pThis  = PDMINS_2_DATA(pDevIns, PHDASTATE);
    uint32_t    offReg = GCPhysAddr - pThis->MMIOBaseAddr;
    int         idxReg = hdaMMIORegLookup(pThis, offReg);
    int         rc;
    Assert(!(offReg & 3)); Assert(cb == 4);

    if (pThis->fInReset && idxReg != ICH6_HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));

    if (idxReg != -1)
    {
#ifdef LOG_ENABLED
        uint32_t const u32CurValue = pThis->au32Regs[idxReg];
#endif
        rc = g_aIchIntelHDRegMap[idxReg].pfnWrite(pThis, idxReg, *(uint32_t const *)pv);
        Log(("hda: write %s:(%x) %x => %x\n", g_aIchIntelHDRegMap[idxReg].abbrev, *(uint32_t const *)pv,
             u32CurValue, pThis->au32Regs[idxReg]));
    }
    else
    {
        LogRel(("hda: Invalid write access @0x%x\n", offReg));
        rc = VINF_SUCCESS;
    }

    Log(("hda: hole at %x is accessed for write\n", offReg));
    return rc;
}


/* PCI callback. */

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) hdaPciIoRegionMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb,
                                           PCIADDRESSSPACE enmType)
{
    int         rc;
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    RTIOPORT    Port = (RTIOPORT)GCPhysAddress;
    PHDASTATE pThis = PCIDEV_2_ICH6_HDASTATE(pPciDev);

    /* 18.2 of the ICH6 datasheet defines the valid access widths as byte, word, and double word */
    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    rc = PDMDevHlpMMIORegister(pPciDev->pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_READ_MISSING,
                               hdaMMIOWrite, hdaMMIORead, "ICH6_HDA");

    if (RT_FAILURE(rc))
        return rc;

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
    codecSaveState(&pThis->Codec, pSSM);

    /* Save MMIO registers */
    AssertCompile(RT_ELEMENTS(pThis->au32Regs) == 112);
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
    int rc = codecLoadState(&pThis->Codec, pSSM, uVersion);
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
            AssertCompile(RT_ELEMENTS(pThis->au32Regs) == 112);
            break;

        case HDA_SSM_VERSION:
            rc = SSMR3GetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
            AssertLogRelMsgReturn(cRegs == RT_ELEMENTS(pThis->au32Regs),
                                  ("cRegs is %d, expected %d\n", cRegs, RT_ELEMENTS(pThis->au32Regs)),
                                  VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
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
    {
        RT_ZERO(pThis->au32Regs);
        SSMR3GetMem(pSSM, pThis->au32Regs, sizeof(uint32_t) * cRegs);
    }

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
    AUD_set_active_in(pThis->Codec.SwVoiceIn, SDCTL(pThis, 0) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    AUD_set_active_out(pThis->Codec.SwVoiceOut, SDCTL(pThis, 4) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));

    pThis->u64CORBBase = RT_MAKE_U64(CORBLBASE(pThis), CORBUBASE(pThis));
    pThis->u64RIRBBase = RT_MAKE_U64(RIRLBASE(pThis), RIRUBASE(pThis));
    pThis->u64DPBase   = RT_MAKE_U64(DPLBASE(pThis), DPUBASE(pThis));
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
                       (sdCtl & HDA_REG_FIELD_MASK(SDCTL, NUM)) >> ICH6_HDA_SDCTL_NUM_SHIFT,
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, DIR)),
                       RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, TP)),
                       (sdCtl & HDA_REG_FIELD_MASK(SDCTL, STRIPE)) >> ICH6_HDA_SDCTL_STRIPE_SHIFT,
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
        if (!RTStrICmp(g_aIchIntelHDRegMap[iReg].abbrev, pszArgs))
            return iReg;
    return -1;
}


static void hdaDbgPrintRegister(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iHdaIndex)
{
    Assert(   pThis
           && iHdaIndex >= 0
           && iHdaIndex < HDA_NREGS);
    pHlp->pfnPrintf(pHlp, "hda: %s: 0x%x\n", g_aIchIntelHDRegMap[iHdaIndex].abbrev, pThis->au32Regs[iHdaIndex]);
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
    pHlp->pfnPrintf(pHlp, "SD%dCTL: %R[sdctl]\n", iHdaStrmIndex, HDA_STREAM_REG2(pThis, CTL, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dCTS: %R[sdsts]\n", iHdaStrmIndex, HDA_STREAM_REG2(pThis, STS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOS: %R[sdfifos]\n", iHdaStrmIndex, HDA_STREAM_REG2(pThis, FIFOS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOW: %R[sdfifow]\n", iHdaStrmIndex, HDA_STREAM_REG2(pThis, FIFOW, iHdaStrmIndex));
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
    if (pThis->Codec.pfnCodecDbgListNodes)
        pThis->Codec.pfnCodecDbgListNodes(&pThis->Codec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaInfoCodecSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    if (pThis->Codec.pfnCodecDbgSelector)
        pThis->Codec.pfnCodecDbgSelector(&pThis->Codec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
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
    GCAP(pThis) = HDA_MAKE_GCAP(4,4,0,0,1); /* see 6.2.1 */
    VMIN(pThis) = 0x00;       /* see 6.2.2 */
    VMAJ(pThis) = 0x01;       /* see 6.2.3 */
    VMAJ(pThis) = 0x01;       /* see 6.2.3 */
    OUTPAY(pThis) = 0x003C;   /* see 6.2.4 */
    INPAY(pThis)  = 0x001D;   /* see 6.2.5 */
    pThis->au32Regs[ICH6_HDA_REG_CORBSIZE] = 0x42; /* see 6.2.1 */
    pThis->au32Regs[ICH6_HDA_REG_RIRBSIZE] = 0x42; /* see 6.2.1 */
    CORBRP(pThis) = 0x0;
    RIRBWP(pThis) = 0x0;

    Log(("hda: inter HDA reset.\n"));
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

    HDABDLEDESC stEmptyBdle;
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
            memset(&stEmptyBdle, 0, sizeof(HDABDLEDESC));
            pBdle = &stEmptyBdle;
        }
        hdaInitTransferDescriptor(pThis, pBdle, u8Strm, &StreamDesc);
        /* hdaStreamReset prevents changing the SRST bit, so we force it to zero here. */
        HDA_STREAM_REG2(pThis, CTL, u8Strm) = 0;
        hdaStreamReset(pThis, pBdle, &StreamDesc, u8Strm);
    }

    /* emulation of codec "wake up" (HDA spec 5.5.1 and 6.5)*/
    STATESTS(pThis) = 0x1;

    Log(("hda: reset finished\n"));
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) hdaDestruct(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    int rc = codecDestruct(&pThis->Codec);
    AssertRC(rc);

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
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_ ("Invalid configuration for the Intel HDA device"));

    // ** @todo r=michaln: This device may need R0/RC enabling, especially if guests
    // poll some register(pThis).

    /*
     * Initialize data (most of it anyway).
     */
    pThis->pDevIns                  = pDevIns;
    /* IBase */
    pThis->IBase.pfnQueryInterface  = hdaQueryInterface;

    /* PCI Device */
    PCIDevSetVendorId           (&pThis->PciDev, HDA_PCI_VENDOR_ID); /* nVidia */
    PCIDevSetDeviceId           (&pThis->PciDev, HDA_PCI_DEICE_ID); /* HDA */

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
        LogRel(("Chipset cannot do MSI: %Rrc\n", rc));
        PCIDevSetCapabilityList(&pThis->PciDev, 0x50);
    }
#endif

    rc = PDMDevHlpSSMRegister(pDevIns, HDA_SSM_VERSION, sizeof(*pThis), hdaSaveExec, hdaLoadExec);
    if (RT_FAILURE(rc))
        return rc;

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

    pThis->Codec.pvHDAState = pThis;
    rc = codecConstruct(pDevIns, &pThis->Codec, pCfgHandle);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
       verb F20 should provide device/codec recognition. */
    Assert(pThis->Codec.u16VendorId);
    Assert(pThis->Codec.u16DeviceId);
    PCIDevSetSubSystemVendorId(&pThis->PciDev, pThis->Codec.u16VendorId); /* 2c ro - intel.) */
    PCIDevSetSubSystemId(      &pThis->PciDev, pThis->Codec.u16DeviceId); /* 2e ro. */

    hdaReset(pDevIns);
    pThis->Codec.id = 0;
    pThis->Codec.pfnTransfer = hdaTransfer;
    pThis->Codec.pfnReset = hdaCodecReset;

    /*
     * 18.2.6,7 defines that values of this registers might be cleared on power on/reset
     * hdaReset shouldn't affects these registers.
     */
    WAKEEN(pThis) = 0x0;
    STATESTS(pThis) = 0x0;

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
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Intel HD Audio Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
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
    /* pfnIOCtl */
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
