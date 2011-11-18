/* $Id$ */
/** @file
 * DevIchIntelHD - VBox ICH Intel HD Audio Controller.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

PDMBOTHCBDECL(int) hdaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) hdaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb);
static DECLCALLBACK(void)  hdaReset (PPDMDEVINS pDevIns);

#define HDA_NREGS 112
/* Registers */
#define HDA_REG_IND_NAME(x) ICH6_HDA_REG_##x
#define HDA_REG_FIELD_NAME(reg, x) ICH6_HDA_##reg##_##x
#define HDA_REG_FIELD_MASK(reg, x) ICH6_HDA_##reg##_##x##_MASK
#define HDA_REG_FIELD_FLAG_MASK(reg, x) RT_BIT(ICH6_HDA_##reg##_##x##_SHIFT)
#define HDA_REG_FIELD_SHIFT(reg, x) ICH6_HDA_##reg##_##x##_SHIFT
#define HDA_REG_IND(pState, x) ((pState)->au32Regs[(x)])
#define HDA_REG(pState, x) (HDA_REG_IND((pState), HDA_REG_IND_NAME(x)))
#define HDA_REG_VALUE(pState, reg, val) (HDA_REG((pState),reg) & (((HDA_REG_FIELD_MASK(reg, val))) << (HDA_REG_FIELD_SHIFT(reg, val))))
#define HDA_REG_FLAG_VALUE(pState, reg, val) (HDA_REG((pState),reg) & (((HDA_REG_FIELD_FLAG_MASK(reg, val)))))
#define HDA_REG_SVALUE(pState, reg, val) (HDA_REG_VALUE(pState, reg, val) >> (HDA_REG_FIELD_SHIFT(reg, val)))

#define ICH6_HDA_REG_GCAP 0 /* range 0x00-0x01*/
#define GCAP(pState) (HDA_REG((pState), GCAP))
/* GCAP HDASpec 3.3.2 This macro compact following information about HDA
 * oss (15:12) - number of output streams supported
 * iss (11:8) - number of input streams supported
 * bss (7:3) - number of bidirection streams suppoted
 * bds (2:1) - number of serial data out signals supported
 * b64sup (0) - 64 bit addressing supported.
 */
#define HDA_MAKE_GCAP(oss, iss, bss, bds, b64sup) \
    (  (((oss) & 0xF) << 12)    \
     | (((iss) & 0xF) << 8)     \
     | (((bss) & 0x1F) << 3)    \
     | (((bds) & 0x3) << 2)     \
     | ((b64sup) & 1))
#define ICH6_HDA_REG_VMIN 1 /* range 0x02 */
#define VMIN(pState) (HDA_REG((pState), VMIN))

#define ICH6_HDA_REG_VMAJ 2 /* range 0x03 */
#define VMAJ(pState) (HDA_REG((pState), VMAJ))

#define ICH6_HDA_REG_OUTPAY 3 /* range 0x04-0x05 */
#define OUTPAY(pState) (HDA_REG((pState), OUTPAY))

#define ICH6_HDA_REG_INPAY 4 /* range 0x06-0x07 */
#define INPAY(pState) (HDA_REG((pState), INPAY))

#define ICH6_HDA_REG_GCTL (5)
#define ICH6_HDA_GCTL_RST_SHIFT (0)
#define ICH6_HDA_GCTL_FSH_SHIFT (1)
#define ICH6_HDA_GCTL_UR_SHIFT (8)
#define GCTL(pState) (HDA_REG((pState), GCTL))

#define ICH6_HDA_REG_WAKEEN 6 /* 0x0C */
#define WAKEEN(pState) (HDA_REG((pState), WAKEEN))

#define ICH6_HDA_REG_STATESTS 7 /* range 0x0E */
#define STATESTS(pState) (HDA_REG((pState), STATESTS))
#define ICH6_HDA_STATES_SCSF 0x7

#define ICH6_HDA_REG_GSTS 8 /* range 0x10-0x11*/
#define ICH6_HDA_GSTS_FSH_SHIFT (1)
#define GSTS(pState) (HDA_REG(pState, GSTS))

#define ICH6_HDA_REG_INTCTL 9 /* 0x20 */
#define ICH6_HDA_INTCTL_GIE_SHIFT 31
#define ICH6_HDA_INTCTL_CIE_SHIFT 30
#define ICH6_HDA_INTCTL_S0_SHIFT (0)
#define ICH6_HDA_INTCTL_S1_SHIFT (1)
#define ICH6_HDA_INTCTL_S2_SHIFT (2)
#define ICH6_HDA_INTCTL_S3_SHIFT (3)
#define ICH6_HDA_INTCTL_S4_SHIFT (4)
#define ICH6_HDA_INTCTL_S5_SHIFT (5)
#define ICH6_HDA_INTCTL_S6_SHIFT (6)
#define ICH6_HDA_INTCTL_S7_SHIFT (7)
#define INTCTL(pState) (HDA_REG((pState), INTCTL))
#define INTCTL_GIE(pState) (HDA_REG_FLAG_VALUE(pState, INTCTL, GIE))
#define INTCTL_CIE(pState) (HDA_REG_FLAG_VALUE(pState, INTCTL, CIE))
#define INTCTL_SX(pState, X) (HDA_REG_FLAG_VALUE((pState), INTCTL, S##X))
#define INTCTL_SALL(pState) (INTCTL((pState)) & 0xFF)

/* Note: The HDA specification defines a SSYNC register at offset 0x38. The
 * ICH6/ICH9 datahseet defines SSYNC at offset 0x34. The Linux HDA driver matches
 * the datasheet.
 */
#define ICH6_HDA_REG_SSYNC  12 /* 0x34 */
#define SSYNC(pState) (HDA_REG((pState), SSYNC))

#define ICH6_HDA_REG_INTSTS 10 /* 0x24 */
#define ICH6_HDA_INTSTS_GIS_SHIFT (31)
#define ICH6_HDA_INTSTS_CIS_SHIFT (30)
#define ICH6_HDA_INTSTS_S0_SHIFT (0)
#define ICH6_HDA_INTSTS_S1_SHIFT (1)
#define ICH6_HDA_INTSTS_S2_SHIFT (2)
#define ICH6_HDA_INTSTS_S3_SHIFT (3)
#define ICH6_HDA_INTSTS_S4_SHIFT (4)
#define ICH6_HDA_INTSTS_S5_SHIFT (5)
#define ICH6_HDA_INTSTS_S6_SHIFT (6)
#define ICH6_HDA_INTSTS_S7_SHIFT (7)
#define ICH6_HDA_INTSTS_S_MASK(num) RT_BIT(HDA_REG_FIELD_SHIFT(S##num))
#define INTSTS(pState) (HDA_REG((pState), INTSTS))
#define INTSTS_GIS(pState) (HDA_REG_FLAG_VALUE((pState), INTSTS, GIS)
#define INTSTS_CIS(pState) (HDA_REG_FLAG_VALUE((pState), INTSTS, CIS)
#define INTSTS_SX(pState, X) (HDA_REG_FLAG_VALUE(pState), INTSTS, S##X)
#define INTSTS_SANY(pState) (INTSTS((pState)) & 0xFF)

#define ICH6_HDA_REG_CORBLBASE  13 /* 0x40 */
#define CORBLBASE(pState) (HDA_REG((pState), CORBLBASE))
#define ICH6_HDA_REG_CORBUBASE  14 /* 0x44 */
#define CORBUBASE(pState) (HDA_REG((pState), CORBUBASE))
#define ICH6_HDA_REG_CORBWP  15 /* 48 */
#define ICH6_HDA_REG_CORBRP  16 /* 4A */
#define ICH6_HDA_CORBRP_RST_SHIFT  15
#define ICH6_HDA_CORBRP_WP_SHIFT  0
#define ICH6_HDA_CORBRP_WP_MASK   0xFF

#define CORBRP(pState) (HDA_REG(pState, CORBRP))
#define CORBWP(pState) (HDA_REG(pState, CORBWP))

#define ICH6_HDA_REG_CORBCTL  17 /* 0x4C */
#define ICH6_HDA_CORBCTL_DMA_SHIFT (1)
#define ICH6_HDA_CORBCTL_CMEIE_SHIFT (0)

#define CORBCTL(pState) (HDA_REG(pState, CORBCTL))


#define ICH6_HDA_REG_CORBSTS  18 /* 0x4D */
#define CORBSTS(pState) (HDA_REG(pState, CORBSTS))
#define ICH6_HDA_CORBSTS_CMEI_SHIFT  (0)

#define ICH6_HDA_REG_CORBSIZE  19 /* 0x4E */
#define ICH6_HDA_CORBSIZE_SZ_CAP 0xF0
#define ICH6_HDA_CORBSIZE_SZ 0x3
#define CORBSIZE_SZ(pState) (HDA_REG(pState, ICH6_HDA_REG_CORBSIZE) & ICH6_HDA_CORBSIZE_SZ)
#define CORBSIZE_SZ_CAP(pState) (HDA_REG(pState, ICH6_HDA_REG_CORBSIZE) & ICH6_HDA_CORBSIZE_SZ_CAP)
/* till ich 10 sizes of CORB and RIRB are hardcoded to 256 in real hw */

#define ICH6_HDA_REG_RIRLBASE  20 /* 0x50 */
#define RIRLBASE(pState) (HDA_REG((pState), RIRLBASE))

#define ICH6_HDA_REG_RIRUBASE  21 /* 0x54 */
#define RIRUBASE(pState) (HDA_REG((pState), RIRUBASE))

#define ICH6_HDA_REG_RIRBWP  22 /* 0x58 */
#define ICH6_HDA_RIRBWP_RST_SHIFT  (15)
#define ICH6_HDA_RIRBWP_WP_MASK   0xFF
#define RIRBWP(pState) (HDA_REG(pState, RIRBWP))

#define ICH6_HDA_REG_RINTCNT  23 /* 0x5A */
#define RINTCNT(pState) (HDA_REG((pState), RINTCNT))
#define RINTCNT_N(pState) (RINTCNT((pState)) & 0xff)

#define ICH6_HDA_REG_RIRBCTL  24 /* 0x5C */
#define ICH6_HDA_RIRBCTL_RIC_SHIFT    (0)
#define ICH6_HDA_RIRBCTL_DMA_SHIFT    (1)
#define ICH6_HDA_ROI_DMA_SHIFT        (2)
#define RIRBCTL(pState)                 (HDA_REG((pState), RIRBCTL))
#define RIRBCTL_RIRB_RIC(pState)        (HDA_REG_FLAG_VALUE(pState, RIRBCTL, RIC))
#define RIRBCTL_RIRB_DMA(pState)        (HDA_REG_FLAG_VALUE((pState), RIRBCTL, DMA)
#define RIRBCTL_ROI(pState)             (HDA_REG_FLAG_VALUE((pState), RIRBCTL, ROI))

#define ICH6_HDA_REG_RIRBSTS  25 /* 0x5D */
#define ICH6_HDA_RIRBSTS_RINTFL_SHIFT (0)
#define ICH6_HDA_RIRBSTS_RIRBOIS_SHIFT (2)
#define RIRBSTS(pState)         (HDA_REG(pState, RIRBSTS))
#define RIRBSTS_RINTFL(pState)  (HDA_REG_FLAG_VALUE(pState, RIRBSTS, RINTFL))
#define RIRBSTS_RIRBOIS(pState) (HDA_REG_FLAG_VALUE(pState, RIRBSTS, RIRBOIS))

#define ICH6_HDA_REG_RIRBSIZE  26 /* 0x5E */
#define ICH6_HDA_RIRBSIZE_SZ_CAP 0xF0
#define ICH6_HDA_RIRBSIZE_SZ 0x3

#define RIRBSIZE_SZ(pState)     (HDA_REG(pState, ICH6_HDA_REG_RIRBSIZE) & ICH6_HDA_RIRBSIZE_SZ)
#define RIRBSIZE_SZ_CAP(pState) (HDA_REG(pState, ICH6_HDA_REG_RIRBSIZE) & ICH6_HDA_RIRBSIZE_SZ_CAP)


#define ICH6_HDA_REG_IC   27 /* 0x60 */
#define IC(pState) (HDA_REG(pState, IC))
#define ICH6_HDA_REG_IR   28 /* 0x64 */
#define IR(pState) (HDA_REG(pState, IR))
#define ICH6_HDA_REG_IRS  29 /* 0x68 */
#define ICH6_HDA_IRS_ICB_SHIFT   (0)
#define ICH6_HDA_IRS_IRV_SHIFT   (1)
#define IRS(pState)     (HDA_REG(pState, IRS))
#define IRS_ICB(pState) (HDA_REG_FLAG_VALUE(pState, IRS, ICB))
#define IRS_IRV(pState) (HDA_REG_FLAG_VALUE(pState, IRS, IRV))

#define ICH6_HDA_REG_DPLBASE  30 /* 0x70 */
#define DPLBASE(pState) (HDA_REG((pState), DPLBASE))
#define ICH6_HDA_REG_DPUBASE  31 /* 0x74 */
#define DPUBASE(pState) (HDA_REG((pState), DPUBASE))
#define DPBASE_ENABLED          1
#define DPBASE_ADDR_MASK        (~0x7f)

#define HDA_STREAM_REG_DEF(name, num) (ICH6_HDA_REG_SD##num##name)
#define HDA_STREAM_REG(pState, name, num) (HDA_REG((pState), N_(HDA_STREAM_REG_DEF(name, num))))
/* Note: sdnum here _MUST_ be stream reg number [0,7] */
#define HDA_STREAM_REG2(pState, name, sdnum) (HDA_REG_IND((pState), ICH6_HDA_REG_SD0##name + (sdnum) * 10))

#define ICH6_HDA_REG_SD0CTL   32 /* 0x80 */
#define ICH6_HDA_REG_SD1CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 10) /* 0xA0 */
#define ICH6_HDA_REG_SD2CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 20) /* 0xC0 */
#define ICH6_HDA_REG_SD3CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 30) /* 0xE0 */
#define ICH6_HDA_REG_SD4CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 40) /* 0x100 */
#define ICH6_HDA_REG_SD5CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 50) /* 0x120 */
#define ICH6_HDA_REG_SD6CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 60) /* 0x140 */
#define ICH6_HDA_REG_SD7CTL   (HDA_STREAM_REG_DEF(CTL, 0) + 70) /* 0x160 */

#define SD(func, num) SD##num##func
#define SDCTL(pState, num) HDA_REG((pState), SD(CTL, num))
#define SDCTL_NUM(pState, num) ((SDCTL((pState), num) & HDA_REG_FIELD_MASK(SDCTL,NUM)) >> HDA_REG_FIELD_SHIFT(SDCTL, NUM))
#define ICH6_HDA_SDCTL_NUM_MASK   (0xF)
#define ICH6_HDA_SDCTL_NUM_SHIFT  (20)
#define ICH6_HDA_SDCTL_DIR_SHIFT  (19)
#define ICH6_HDA_SDCTL_TP_SHIFT   (18)
#define ICH6_HDA_SDCTL_STRIPE_MASK  (0x3)
#define ICH6_HDA_SDCTL_STRIPE_SHIFT (16)
#define ICH6_HDA_SDCTL_DEIE_SHIFT (4)
#define ICH6_HDA_SDCTL_FEIE_SHIFT (3)
#define ICH6_HDA_SDCTL_ICE_SHIFT  (2)
#define ICH6_HDA_SDCTL_RUN_SHIFT  (1)
#define ICH6_HDA_SDCTL_SRST_SHIFT (0)

#define ICH6_HDA_REG_SD0STS   33 /* 0x83 */
#define ICH6_HDA_REG_SD1STS   (HDA_STREAM_REG_DEF(STS, 0) + 10) /* 0xA3 */
#define ICH6_HDA_REG_SD2STS   (HDA_STREAM_REG_DEF(STS, 0) + 20) /* 0xC3 */
#define ICH6_HDA_REG_SD3STS   (HDA_STREAM_REG_DEF(STS, 0) + 30) /* 0xE3 */
#define ICH6_HDA_REG_SD4STS   (HDA_STREAM_REG_DEF(STS, 0) + 40) /* 0x103 */
#define ICH6_HDA_REG_SD5STS   (HDA_STREAM_REG_DEF(STS, 0) + 50) /* 0x123 */
#define ICH6_HDA_REG_SD6STS   (HDA_STREAM_REG_DEF(STS, 0) + 60) /* 0x143 */
#define ICH6_HDA_REG_SD7STS   (HDA_STREAM_REG_DEF(STS, 0) + 70) /* 0x163 */

#define SDSTS(pState, num) HDA_REG((pState), SD(STS, num))
#define ICH6_HDA_SDSTS_FIFORDY_SHIFT (5)
#define ICH6_HDA_SDSTS_DE_SHIFT (4)
#define ICH6_HDA_SDSTS_FE_SHIFT (3)
#define ICH6_HDA_SDSTS_BCIS_SHIFT  (2)

#define ICH6_HDA_REG_SD0LPIB   34 /* 0x84 */
#define ICH6_HDA_REG_SD1LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 10) /* 0xA4 */
#define ICH6_HDA_REG_SD2LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 20) /* 0xC4 */
#define ICH6_HDA_REG_SD3LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 30) /* 0xE4 */
#define ICH6_HDA_REG_SD4LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 40) /* 0x104 */
#define ICH6_HDA_REG_SD5LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 50) /* 0x124 */
#define ICH6_HDA_REG_SD6LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 60) /* 0x144 */
#define ICH6_HDA_REG_SD7LPIB   (HDA_STREAM_REG_DEF(LPIB, 0) + 70) /* 0x164 */

#define SDLPIB(pState, num) HDA_REG((pState), SD(LPIB, num))

#define ICH6_HDA_REG_SD0CBL   35 /* 0x88 */
#define ICH6_HDA_REG_SD1CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 10) /* 0xA8 */
#define ICH6_HDA_REG_SD2CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 20) /* 0xC8 */
#define ICH6_HDA_REG_SD3CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 30) /* 0xE8 */
#define ICH6_HDA_REG_SD4CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 40) /* 0x108 */
#define ICH6_HDA_REG_SD5CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 50) /* 0x128 */
#define ICH6_HDA_REG_SD6CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 60) /* 0x148 */
#define ICH6_HDA_REG_SD7CBL   (HDA_STREAM_REG_DEF(CBL, 0) + 70) /* 0x168 */

#define SDLCBL(pState, num) HDA_REG((pState), SD(CBL, num))

#define ICH6_HDA_REG_SD0LVI   36 /* 0x8C */
#define ICH6_HDA_REG_SD1LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 10) /* 0xAC */
#define ICH6_HDA_REG_SD2LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 20) /* 0xCC */
#define ICH6_HDA_REG_SD3LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 30) /* 0xEC */
#define ICH6_HDA_REG_SD4LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 40) /* 0x10C */
#define ICH6_HDA_REG_SD5LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 50) /* 0x12C */
#define ICH6_HDA_REG_SD6LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 60) /* 0x14C */
#define ICH6_HDA_REG_SD7LVI   (HDA_STREAM_REG_DEF(LVI, 0) + 70) /* 0x16C */

#define SDLVI(pState, num) HDA_REG((pState), SD(LVI, num))

#define ICH6_HDA_REG_SD0FIFOW   37 /* 0x8E */
#define ICH6_HDA_REG_SD1FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 10) /* 0xAE */
#define ICH6_HDA_REG_SD2FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 20) /* 0xCE */
#define ICH6_HDA_REG_SD3FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 30) /* 0xEE */
#define ICH6_HDA_REG_SD4FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 40) /* 0x10E */
#define ICH6_HDA_REG_SD5FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 50) /* 0x12E */
#define ICH6_HDA_REG_SD6FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 60) /* 0x14E */
#define ICH6_HDA_REG_SD7FIFOW   (HDA_STREAM_REG_DEF(FIFOW, 0) + 70) /* 0x16E */

/*
 * ICH6 datasheet defined limits for FIFOW values (18.2.38)
 */
#define HDA_SDFIFOW_8B       (0x2)
#define HDA_SDFIFOW_16B      (0x3)
#define HDA_SDFIFOW_32B      (0x4)
#define SDFIFOW(pState, num) HDA_REG((pState), SD(FIFOW, num))

#define ICH6_HDA_REG_SD0FIFOS   38 /* 0x90 */
#define ICH6_HDA_REG_SD1FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 10) /* 0xB0 */
#define ICH6_HDA_REG_SD2FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 20) /* 0xD0 */
#define ICH6_HDA_REG_SD3FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 30) /* 0xF0 */
#define ICH6_HDA_REG_SD4FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 40) /* 0x110 */
#define ICH6_HDA_REG_SD5FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 50) /* 0x130 */
#define ICH6_HDA_REG_SD6FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 60) /* 0x150 */
#define ICH6_HDA_REG_SD7FIFOS   (HDA_STREAM_REG_DEF(FIFOS, 0) + 70) /* 0x170 */

/*
 * ICH6 datasheet defines limits for FIFOS registers (18.2.39)
 * formula: size - 1
 * Other values not listed are not supported.
 */
#define HDA_SDONFIFO_16B  (0xF) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_32B  (0x1F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_64B  (0x3F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_128B (0x7F) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_192B (0xBF) /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDONFIFO_256B (0xFF) /* 20-, 24-bit Output Streams */
#define HDA_SDINFIFO_120B (0x77) /* 8-, 16-, 20-, 24-, 32-bit Input Streams */
#define HDA_SDINFIFO_160B (0x9F) /* 20-, 24-bit Input Streams Streams */
#define SDFIFOS(pState, num) HDA_REG((pState), SD(FIFOS, num))

#define ICH6_HDA_REG_SD0FMT     39 /* 0x92 */
#define ICH6_HDA_REG_SD1FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 10) /* 0xB2 */
#define ICH6_HDA_REG_SD2FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 20) /* 0xD2 */
#define ICH6_HDA_REG_SD3FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 30) /* 0xF2 */
#define ICH6_HDA_REG_SD4FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 40) /* 0x112 */
#define ICH6_HDA_REG_SD5FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 50) /* 0x132 */
#define ICH6_HDA_REG_SD6FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 60) /* 0x152 */
#define ICH6_HDA_REG_SD7FMT     (HDA_STREAM_REG_DEF(FMT, 0) + 70) /* 0x172 */

#define SDFMT(pState, num)      (HDA_REG((pState), SD(FMT, num)))
#define ICH6_HDA_SDFMT_BASE_RATE_SHIFT (14)
#define ICH6_HDA_SDFMT_MULT_SHIFT (11)
#define ICH6_HDA_SDFMT_MULT_MASK (0x7)
#define ICH6_HDA_SDFMT_DIV_SHIFT (8)
#define ICH6_HDA_SDFMT_DIV_MASK (0x7)
#define ICH6_HDA_SDFMT_BITS_SHIFT (4)
#define ICH6_HDA_SDFMT_BITS_MASK (0x7)
#define SDFMT_BASE_RATE(pState, num) ((SDFMT(pState, num) & HDA_REG_FIELD_FLAG_MASK(SDFMT, BASE_RATE)) >> HDA_REG_FIELD_SHIFT(SDFMT, BASE_RATE))
#define SDFMT_MULT(pState, num) ((SDFMT((pState), num) & HDA_REG_FIELD_MASK(SDFMT,MULT)) >> HDA_REG_FIELD_SHIFT(SDFMT, MULT))
#define SDFMT_DIV(pState, num) ((SDFMT((pState), num) & HDA_REG_FIELD_MASK(SDFMT,DIV)) >> HDA_REG_FIELD_SHIFT(SDFMT, DIV))

#define ICH6_HDA_REG_SD0BDPL     40 /* 0x98 */
#define ICH6_HDA_REG_SD1BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 10) /* 0xB8 */
#define ICH6_HDA_REG_SD2BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 20) /* 0xD8 */
#define ICH6_HDA_REG_SD3BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 30) /* 0xF8 */
#define ICH6_HDA_REG_SD4BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 40) /* 0x118 */
#define ICH6_HDA_REG_SD5BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 50) /* 0x138 */
#define ICH6_HDA_REG_SD6BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 60) /* 0x158 */
#define ICH6_HDA_REG_SD7BDPL     (HDA_STREAM_REG_DEF(BDPL, 0) + 70) /* 0x178 */

#define SDBDPL(pState, num) HDA_REG((pState), SD(BDPL, num))

#define ICH6_HDA_REG_SD0BDPU     41 /* 0x9C */
#define ICH6_HDA_REG_SD1BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 10) /* 0xBC */
#define ICH6_HDA_REG_SD2BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 20) /* 0xDC */
#define ICH6_HDA_REG_SD3BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 30) /* 0xFC */
#define ICH6_HDA_REG_SD4BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 40) /* 0x11C */
#define ICH6_HDA_REG_SD5BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 50) /* 0x13C */
#define ICH6_HDA_REG_SD6BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 60) /* 0x15C */
#define ICH6_HDA_REG_SD7BDPU     (HDA_STREAM_REG_DEF(BDPU, 0) + 70) /* 0x17C */

#define SDBDPU(pState, num) HDA_REG((pState), SD(BDPU, num))


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
    /** Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** Pointer to the connector of the attached audio driver. */
    PPDMIAUDIOCONNECTOR     pDrv;
    /** Pointer to the attached audio driver. */
    PPDMIBASE               pDrvBase;
    /** The base interface for LUN\#0. */
    PDMIBASE                IBase;
    RTGCPHYS    addrMMReg;
    uint32_t     au32Regs[HDA_NREGS];
    HDABDLEDESC  stInBdle;
    HDABDLEDESC  stOutBdle;
    HDABDLEDESC  stMicBdle;
    /* Interrupt on completion */
    bool        fCviIoc;
    uint64_t    u64CORBBase;
    uint64_t    u64RIRBBase;
    uint64_t    u64DPBase;
    /* pointer on CORB buf */
    uint32_t    *pu32CorbBuf;
    /* size in bytes of CORB buf */
    uint32_t    cbCorbBuf;
    /* pointer on RIRB buf */
    uint64_t    *pu64RirbBuf;
    /* size in bytes of RIRB buf */
    uint32_t    cbRirbBuf;
    /* indicates if HDA in reset. */
    bool        fInReset;
    CODECState  Codec;
    uint8_t     u8Counter;
    uint64_t    u64BaseTS;
} INTELHDLinkState, *PINTELHDLinkState;

#define ICH6_HDASTATE_2_DEVINS(pINTELHD)   ((pINTELHD)->pDevIns)
#define PCIDEV_2_ICH6_HDASTATE(pPciDev) ((PCIINTELHDLinkState *)(pPciDev))

#define ISD0FMT_TO_AUDIO_SELECTOR(pState) (AUDIO_FORMAT_SELECTOR(&(pState)->Codec, In,     \
                SDFMT_BASE_RATE(pState, 0), SDFMT_MULT(pState, 0), SDFMT_DIV(pState, 0)))
#define OSD0FMT_TO_AUDIO_SELECTOR(pState) (AUDIO_FORMAT_SELECTOR(&(pState)->Codec, Out,     \
                SDFMT_BASE_RATE(pState, 4), SDFMT_MULT(pState, 4), SDFMT_DIV(pState, 4)))




typedef struct PCIINTELHDLinkState
{
    PCIDevice dev;
    INTELHDLinkState hda;
} PCIINTELHDLinkState;


DECLCALLBACK(int) hdaRegReadUnimplemented(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteUnimplemented(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadGCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteGCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadSTATESTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteSTATESTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadGCAP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegReadINTSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegReadWALCLK(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteINTSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegWriteCORBWP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegWriteCORBRP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteCORBCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteCORBSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteRIRBWP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegWriteRIRBSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteIRS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegReadIRS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteSDCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegReadSDCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);

DECLCALLBACK(int) hdaRegWriteSDSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDLVI(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDFIFOW(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDFIFOS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDFMT(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDBDPL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteSDBDPU(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegWriteBase(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
DECLCALLBACK(int) hdaRegReadU32(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteU32(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadU24(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteU24(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadU16(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteU16(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);
DECLCALLBACK(int) hdaRegReadU8(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
DECLCALLBACK(int) hdaRegWriteU8(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t pu32Value);

static inline void hdaInitTransferDescriptor(PINTELHDLinkState pState, PHDABDLEDESC pBdle, uint8_t u8Strm, PHDASTREAMTRANSFERDESC pStreamDesc);
static int hdaMMIORegLookup(INTELHDLinkState* pState, uint32_t u32Offset);
static void hdaFetchBdle(INTELHDLinkState *pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc);
#ifdef LOG_ENABLED
static void dump_bd(INTELHDLinkState *pState, PHDABDLEDESC pBdle, uint64_t u64BaseDMA);
#endif


/* see 302349 p 6.2*/
const static struct stIchIntelHDRegMap
{
    /** Register offset in the register space. */
    uint32_t   offset;
    /** Size in bytes. Registers of size > 4 are in fact tables. */
    uint32_t   size;
    /** Readable bits. */
    uint32_t readable;
    /** Writable bits. */
    uint32_t writable;
    /** Read callback. */
    int       (*pfnRead)(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value);
    /** Write callback. */
    int       (*pfnWrite)(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value);
    /** Abbreviated name. */
    const char *abbrev;
    /** Full name. */
    const char *name;
} s_ichIntelHDRegMap[HDA_NREGS] =
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

static void inline hdaUpdatePosBuf(INTELHDLinkState *pState, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    if (pState->u64DPBase & DPBASE_ENABLED)
        PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pState),
                       (pState->u64DPBase & DPBASE_ADDR_MASK) + pStreamDesc->u8Strm*8, pStreamDesc->pu32Lpib, sizeof(uint32_t));
}
static uint32_t inline hdaFifoWToSz(INTELHDLinkState *pState, PHDASTREAMTRANSFERDESC pStreamDesc)
{
#if 0
    switch(HDA_STREAM_REG2(pState, FIFOW, pStreamDesc->u8Strm))
    {
        case HDA_SDFIFOW_8B: return 8;
        case HDA_SDFIFOW_16B: return 16;
        case HDA_SDFIFOW_32B: return 32;
        default:
            AssertMsgFailed(("hda: unsupported value (%x) in SDFIFOW(,%d)\n", HDA_REG_IND(pState, pStreamDesc->u8Strm), pStreamDesc->u8Strm));
    }
#endif
    return 0;
}

static int hdaProcessInterrupt(INTELHDLinkState* pState)
{
#define IS_INTERRUPT_OCCURED_AND_ENABLED(pState, num)                       \
        (   INTCTL_SX((pState), num)                                        \
         && (SDSTS(pState, num) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))
    bool fIrq = false;
    if (   INTCTL_CIE(pState)
       && (   RIRBSTS_RINTFL(pState)
           || RIRBSTS_RIRBOIS(pState)
           || (STATESTS(pState) & WAKEEN(pState))))
        fIrq = true;

    if (   IS_INTERRUPT_OCCURED_AND_ENABLED(pState, 0)
        || IS_INTERRUPT_OCCURED_AND_ENABLED(pState, 4))
        fIrq = true;

    if (INTCTL_GIE(pState))
    {
        Log(("hda: irq %s\n", fIrq ? "asserted" : "deasserted"));
        PDMDevHlpPCISetIrq(ICH6_HDASTATE_2_DEVINS(pState), 0 , fIrq);
    }
    return VINF_SUCCESS;
}

static int hdaMMIORegLookup(INTELHDLinkState* pState, uint32_t u32Offset)
{
    int idxMiddle;
    int idxHigh = RT_ELEMENTS(s_ichIntelHDRegMap);
    int idxLow = 0;
    /* Aliases HDA spec 3.3.45 */
    switch(u32Offset)
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
    while (1)
    {
#ifdef DEBUG_vvl
            Assert((   idxHigh >= 0
                    && idxLow >= 0));
#endif
            if (   idxHigh < idxLow
                || idxHigh < 0)
                break;
            idxMiddle = idxLow + (idxHigh - idxLow)/2;
            if (u32Offset < s_ichIntelHDRegMap[idxMiddle].offset)
            {
                idxHigh = idxMiddle - 1;
                continue;
            }
            if (u32Offset >= s_ichIntelHDRegMap[idxMiddle].offset + s_ichIntelHDRegMap[idxMiddle].size)
            {
                idxLow = idxMiddle + 1;
                continue;
            }
            if (   u32Offset >= s_ichIntelHDRegMap[idxMiddle].offset
                && u32Offset < s_ichIntelHDRegMap[idxMiddle].offset + s_ichIntelHDRegMap[idxMiddle].size)
                return idxMiddle;
    }
    return -1;
}

static int hdaCmdSync(INTELHDLinkState *pState, bool fLocal)
{
    int rc = VINF_SUCCESS;
    if (fLocal)
    {
        Assert((HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA)));
        rc = PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pState), pState->u64CORBBase, pState->pu32CorbBuf, pState->cbCorbBuf);
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
                if ((i + j) == CORBRP(pState))
                    prefix = "[R]";
                else if ((i + j) == CORBWP(pState))
                    prefix = "[W]";
                else
                    prefix = "   "; /* three spaces */
                Log(("%s%08x", prefix, pState->pu32CorbBuf[i + j]));
                j++;
            } while (j < 8);
            Log(("\n"));
            i += 8;
        } while(i != 0);
#endif
    }
    else
    {
        Assert((HDA_REG_FLAG_VALUE(pState, RIRBCTL, DMA)));
        rc = PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pState), pState->u64RIRBBase, pState->pu64RirbBuf, pState->cbRirbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
#ifdef DEBUG_CMD_BUFFER
        uint8_t i = 0;
        do {
            Log(("hda: rirb%02x: ", i));
            uint8_t j = 0;
            do {
                const char *prefix;
                if ((i + j) == RIRBWP(pState))
                    prefix = "[W]";
                else
                    prefix = "   ";
                Log((" %s%016lx", prefix, pState->pu64RirbBuf[i + j]));
            } while (++j < 8);
            Log(("\n"));
            i += 8;
        } while (i != 0);
#endif
    }
    return rc;
}

static int hdaCORBCmdProcess(INTELHDLinkState *pState)
{
    int rc;
    uint8_t corbRp;
    uint8_t corbWp;
    uint8_t rirbWp;

    PFNCODECVERBPROCESSOR pfn = (PFNCODECVERBPROCESSOR)NULL;

    rc = hdaCmdSync(pState, true);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    corbRp = CORBRP(pState);
    corbWp = CORBWP(pState);
    rirbWp = RIRBWP(pState);
    Assert((corbWp != corbRp));
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", CORBRP(pState), CORBWP(pState), RIRBWP(pState)));
    while (corbRp != corbWp)
    {
        uint32_t cmd;
        uint64_t resp;
        corbRp++;
        cmd = pState->pu32CorbBuf[corbRp];
        rc = (pState)->Codec.pfnLookup(&pState->Codec, cmd, &pfn);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Assert(pfn);
        (rirbWp)++;
        rc = pfn(&pState->Codec, cmd, &resp);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        Log(("hda: verb:%08x->%016lx\n", cmd, resp));
        if (   (resp & CODEC_RESPONSE_UNSOLICITED)
            && !HDA_REG_FLAG_VALUE(pState, GCTL, UR))
        {
            Log(("hda: unexpected unsolicited response.\n"));
            pState->au32Regs[ICH6_HDA_REG_CORBRP] = corbRp;
            return rc;
        }
        pState->pu64RirbBuf[rirbWp] = resp;
        pState->u8Counter++;
        if (pState->u8Counter == RINTCNT_N(pState))
            break;
    }
    pState->au32Regs[ICH6_HDA_REG_CORBRP] = corbRp;
    pState->au32Regs[ICH6_HDA_REG_RIRBWP] = rirbWp;
    rc = hdaCmdSync(pState, false);
    Log(("hda: CORB(RP:%x, WP:%x) RIRBWP:%x\n", CORBRP(pState), CORBWP(pState), RIRBWP(pState)));
    if (RIRBCTL_RIRB_RIC(pState))
    {
        RIRBSTS((pState)) |= HDA_REG_FIELD_FLAG_MASK(RIRBSTS,RINTFL);
        pState->u8Counter = 0;
        rc = hdaProcessInterrupt(pState);
    }
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    return rc;
}

static void hdaStreamReset(INTELHDLinkState *pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc, uint8_t u8Strm)
{
    Log(("hda: reset of stream (%d) started\n", u8Strm));
    Assert((   pState
            && pBdle
            && pStreamDesc
            && u8Strm <= 7));
    memset(pBdle, 0, sizeof(HDABDLEDESC));
    *pStreamDesc->pu32Lpib = 0;
    *pStreamDesc->pu32Sts = 0;
    /* According to ICH6 datasheet, 0x40000 is default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRCT bit */
    HDA_STREAM_REG2(pState, CTL, u8Strm) = 0x40000 | (HDA_STREAM_REG2(pState, CTL, u8Strm) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    /* ICH6 defines default values (0x77 for input and 0xBF for output descriptors) of FIFO size. 18.2.39 */
    HDA_STREAM_REG2(pState, FIFOS, u8Strm) =  u8Strm < 4 ? HDA_SDINFIFO_120B : HDA_SDONFIFO_192B;
    HDA_STREAM_REG2(pState, FIFOW, u8Strm) = u8Strm < 4 ? HDA_SDFIFOW_8B : HDA_SDFIFOW_32B;
    HDA_STREAM_REG2(pState, CBL, u8Strm) = 0;
    HDA_STREAM_REG2(pState, LVI, u8Strm) = 0;
    HDA_STREAM_REG2(pState, FMT, u8Strm) = 0;
    HDA_STREAM_REG2(pState, BDPU, u8Strm) = 0;
    HDA_STREAM_REG2(pState, BDPL, u8Strm) = 0;
    Log(("hda: reset of stream (%d) finished\n", u8Strm));
}


DECLCALLBACK(int)hdaRegReadUnimplemented(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    *pu32Value = 0;
    return VINF_SUCCESS;
}
DECLCALLBACK(int)hdaRegWriteUnimplemented(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    return VINF_SUCCESS;
}
/* U8 */
DECLCALLBACK(int)hdaRegReadU8(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    Assert(((pState->au32Regs[index] & s_ichIntelHDRegMap[index].readable) & 0xffffff00) == 0);
    return hdaRegReadU32(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteU8(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    Assert(((u32Value & 0xffffff00) == 0));
    return hdaRegWriteU32(pState, offset, index, u32Value);
}
/* U16 */
DECLCALLBACK(int)hdaRegReadU16(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    Assert(((pState->au32Regs[index] & s_ichIntelHDRegMap[index].readable) & 0xffff0000) == 0);
    return hdaRegReadU32(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteU16(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    Assert(((u32Value & 0xffff0000) == 0));
    return hdaRegWriteU32(pState, offset, index, u32Value);
}

/* U24 */
DECLCALLBACK(int)hdaRegReadU24(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    Assert(((pState->au32Regs[index] & s_ichIntelHDRegMap[index].readable) & 0xff000000) == 0);
    return hdaRegReadU32(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteU24(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    Assert(((u32Value & 0xff000000) == 0));
    return hdaRegWriteU32(pState, offset, index, u32Value);
}
/* U32 */
DECLCALLBACK(int)hdaRegReadU32(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    *pu32Value = pState->au32Regs[index] & s_ichIntelHDRegMap[index].readable;
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteU32(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    pState->au32Regs[index]  = (u32Value & s_ichIntelHDRegMap[index].writable)
                                  | (pState->au32Regs[index] & ~s_ichIntelHDRegMap[index].writable);
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegReadGCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    return hdaRegReadU32(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteGCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, RST))
    {
        /* exit reset state */
        GCTL(pState) |= HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pState->fInReset = false;
    }
    else
    {
        /* enter reset state*/
        if (   HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA)
            || HDA_REG_FLAG_VALUE(pState, RIRBCTL, DMA))
        {
            Log(("hda: HDA enters in reset with DMA(RIRB:%s, CORB:%s)\n",
                HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA) ? "on" : "off",
                HDA_REG_FLAG_VALUE(pState, RIRBCTL, DMA) ? "on" : "off"));
        }
        hdaReset(ICH6_HDASTATE_2_DEVINS(pState));
        GCTL(pState) &= ~HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pState->fInReset = true;
    }
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, FSH))
    {
        /* Flush: GSTS:1 set,  see 6.2.6*/
        GSTS(pState) |= HDA_REG_FIELD_FLAG_MASK(GSTS, FSH); /* set the flush state */
        /* DPLBASE and DPUBASE, should be initialized with initial value (see 6.2.6)*/
    }
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteSTATESTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    uint32_t v = pState->au32Regs[index];
    uint32_t nv = u32Value & ICH6_HDA_STATES_SCSF;
    pState->au32Regs[index] &= ~(v & nv); /* write of 1 clears corresponding bit */
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegReadINTSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    uint32_t v = 0;
    if (   RIRBSTS_RIRBOIS(pState)
        || RIRBSTS_RINTFL(pState)
        || HDA_REG_FLAG_VALUE(pState, CORBSTS, CMEI)
        || STATESTS(pState))
        v |= RT_BIT(30);
#define HDA_IS_STREAM_EVENT(pState, stream)             \
       (   (SDSTS((pState),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, DE))  \
        || (SDSTS((pState),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, FE))  \
        || (SDSTS((pState),stream) & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS)))
#define MARK_STREAM(pState, stream, v) do {(v) |= HDA_IS_STREAM_EVENT((pState),stream) ? RT_BIT((stream)) : 0;}while(0)
    MARK_STREAM(pState, 0, v);
    MARK_STREAM(pState, 1, v);
    MARK_STREAM(pState, 2, v);
    MARK_STREAM(pState, 3, v);
    MARK_STREAM(pState, 4, v);
    MARK_STREAM(pState, 5, v);
    MARK_STREAM(pState, 6, v);
    MARK_STREAM(pState, 7, v);
    v |= v ? RT_BIT(31) : 0;
    *pu32Value = v;
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegReadWALCLK(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    /* HDA spec (1a): 3.3.16 WALCLK counter ticks with 24Mhz bitclock rate. */
    *pu32Value = (uint32_t)ASMMultU64ByU32DivByU32(PDMDevHlpTMTimeVirtGetNano(ICH6_HDASTATE_2_DEVINS(pState)) - pState->u64BaseTS, 24, 1000);
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegReadGCAP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    return hdaRegReadU16(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteCORBRP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(CORBRP, RST))
        CORBRP(pState) = 0;
    else
        return hdaRegWriteU8(pState, offset, index, u32Value);
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteCORBCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = hdaRegWriteU8(pState, offset, index, u32Value);
    AssertRC(rc);
    if (   CORBWP(pState) != CORBRP(pState)
        && HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA) != 0)
        return hdaCORBCmdProcess(pState);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteCORBSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    uint32_t v = CORBSTS(pState);
    CORBSTS(pState) &= ~(v & u32Value);
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteCORBWP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc;
    rc = hdaRegWriteU16(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    if (CORBWP(pState) == CORBRP(pState))
        return VINF_SUCCESS;
    if (!HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA))
        return VINF_SUCCESS;
    rc = hdaCORBCmdProcess(pState);
    return rc;
}

DECLCALLBACK(int)hdaRegReadSDCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    return hdaRegReadU24(pState, offset, index, pu32Value);
}

DECLCALLBACK(int)hdaRegWriteSDCTL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    bool fRun = RT_BOOL((u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)));
    bool fInRun = RT_BOOL((HDA_REG_IND(pState, index) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)));
    bool fReset = RT_BOOL((u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST)));
    bool fInReset = RT_BOOL((HDA_REG_IND(pState, index) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST)));
    int rc = VINF_SUCCESS;
    if (fInReset)
    {
        /* Assert!!! Guest is resetting HDA's stream, we're expecting guest will mark stream as exit
         * from reset
         */
        Assert((!fReset));
        Log(("hda: guest initiate exit of stream reset.\n"));
        goto done;
    }
    else if (fReset)
    {
        /*
         * Assert!!! ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset.
         */
        uint8_t u8Strm = 0;
        PHDABDLEDESC pBdle = NULL;
        HDASTREAMTRANSFERDESC stStreamDesc;
        Assert((!fInRun && !fRun));
        switch (index)
        {
            case ICH6_HDA_REG_SD0CTL:
                u8Strm = 0;
                pBdle = &pState->stInBdle;
            break;
            case ICH6_HDA_REG_SD4CTL:
                u8Strm = 4;
                pBdle = &pState->stOutBdle;
            break;
            default:
                Log(("hda: changing SRST bit on non-attached stream\n"));
                goto done;
        }
        Log(("hda: guest initiate enter to stream reset.\n"));
        hdaInitTransferDescriptor(pState, pBdle, u8Strm, &stStreamDesc);
        hdaStreamReset(pState, pBdle, &stStreamDesc, u8Strm);
        goto done;
    }

    /* we enter here to change DMA states only */
    if (   (fInRun && !fRun)
        || (fRun && !fInRun))
    {
        Assert((!fReset && !fInReset));
        switch (index)
        {
            case ICH6_HDA_REG_SD0CTL:
                AUD_set_active_in(pState->Codec.SwVoiceIn, fRun);
            break;
            case ICH6_HDA_REG_SD4CTL:
                AUD_set_active_out(pState->Codec.SwVoiceOut, fRun);
            break;
            default:
                Log(("hda: changing RUN bit on non-attached stream\n"));
                goto done;
        }
    }

    done:
    rc = hdaRegWriteU24(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteSDSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    uint32_t v = HDA_REG_IND(pState, index);
    v &= ~(u32Value & v);
    HDA_REG_IND(pState, index) = v;
    hdaProcessInterrupt(pState);
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteSDLVI(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteSDFIFOW(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    switch (u32Value)
    {
        case HDA_SDFIFOW_8B:
        case HDA_SDFIFOW_16B:
        case HDA_SDFIFOW_32B:
            return hdaRegWriteU16(pState, offset, index, u32Value);
        default:
            Log(("hda: Attempt to store unsupported value(%x) in SDFIFOW\n", u32Value));
            return hdaRegWriteU16(pState, offset, index, HDA_SDFIFOW_32B);
    }
    return VINF_SUCCESS;
}
/*
 * Note this method could be called for changing value on Output Streams only (ICH6 datacheet 18.2.39)
 *
 */
DECLCALLBACK(int)hdaRegWriteSDFIFOS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    switch (index)
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
                    return hdaRegWriteU16(pState, offset, index, u32Value);

                case HDA_SDONFIFO_256B:
                    Log(("hda: 256 bit is unsupported, HDA is switched into 192B mode\n"));
                default:
                    return hdaRegWriteU16(pState, offset, index, HDA_SDONFIFO_192B);
            }
            return VINF_SUCCESS;
        default:
            AssertMsgFailed(("Something wierd happens with register lookup routine"));
    }
    return VINF_SUCCESS;
}

static void inline hdaSdFmtToAudSettings(uint32_t u32SdFmt, audsettings_t *pAudSetting)
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
            Log(("hda: %s requested 8 bit\n", __FUNCTION__));
            pAudSetting->fmt = AUD_FMT_S8;
        break;
        case 1:
            Log(("hda: %s requested 16 bit\n", __FUNCTION__));
            pAudSetting->fmt = AUD_FMT_S16;
        break;
        case 2:
            Log(("hda: %s requested 20 bit\n", __FUNCTION__));
        break;
        case 3:
            Log(("hda: %s requested 24 bit\n", __FUNCTION__));
        break;
        case 4:
            Log(("hda: %s requested 32 bit\n", __FUNCTION__));
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

DECLCALLBACK(int)hdaRegWriteSDFMT(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
#ifdef VBOX_WITH_HDA_CODEC_EMU
    /* @todo here some more investigations are required. */
    int rc = 0;
    audsettings_t as;
    /* no reason to reopen voice with same settings */
    if (u32Value == HDA_REG_IND(pState, index))
        return VINF_SUCCESS;
    hdaSdFmtToAudSettings(u32Value, &as);
    switch (index)
    {
        case ICH6_HDA_REG_SD0FMT:
            rc = codecOpenVoice(&pState->Codec, PI_INDEX, &as);
            break;
        case ICH6_HDA_REG_SD4FMT:
            rc = codecOpenVoice(&pState->Codec, PO_INDEX, &as);
            break;
        default:
            Log(("HDA: attempt to change format on %d\n", index));
            rc = 0;
    }
    return hdaRegWriteU16(pState, offset, index, u32Value);
#else
    return hdaRegWriteU16(pState, offset, index, u32Value);
#endif
}

DECLCALLBACK(int)hdaRegWriteSDBDPL(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteSDBDPU(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, VINF_SUCCESS);
    return rc;
}

DECLCALLBACK(int)hdaRegReadIRS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t *pu32Value)
{
    int rc = VINF_SUCCESS;
    /* regarding 3.4.3 we should mark IRS as busy in case CORB is active */
    if (   CORBWP(pState) != CORBRP(pState)
        || HDA_REG_FLAG_VALUE(pState, CORBCTL, DMA))
        IRS(pState) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */

    rc = hdaRegReadU32(pState, offset, index, pu32Value);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteIRS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;
    uint64_t resp;
    PFNCODECVERBPROCESSOR pfn = (PFNCODECVERBPROCESSOR)NULL;
    /*
     * if guest set ICB bit of IRS register HDA should process verb in IC register and
     * writes response in IR register and set IRV (valid in case of success) bit of IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, ICB)
        && !IRS_ICB(pState))
    {
        uint32_t cmd = IC(pState);
        if (CORBWP(pState) != CORBRP(pState))
        {
            /*
             * 3.4.3 defines behaviour of immediate Command status register.
             */
            LogRel(("hda: guest has tried process immediate verb (%x) with active CORB\n", cmd));
            return rc;
        }
        IRS(pState) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */
        Log(("hda: IC:%x\n", cmd));
        rc = pState->Codec.pfnLookup(&pState->Codec, cmd, &pfn);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        rc = pfn(&pState->Codec, cmd, &resp);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
        IR(pState) = (uint32_t)resp;
        Log(("hda: IR:%x\n", IR(pState)));
        IRS(pState) = HDA_REG_FIELD_FLAG_MASK(IRS, IRV);  /* result is ready  */
        IRS(pState) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, ICB); /* busy is clear */
        return rc;
    }
    /*
     * when guest's read the response it should clean the IRV bit of the IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, IRV)
        && IRS_IRV(pState))
        IRS(pState) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, IRV);
    return rc;
}

DECLCALLBACK(int)hdaRegWriteRIRBWP(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    if (u32Value & HDA_REG_FIELD_FLAG_MASK(RIRBWP, RST))
    {
        RIRBWP(pState) = 0;
    }
    /*The rest of bits are O, see 6.2.22 */
    return VINF_SUCCESS;
}

DECLCALLBACK(int)hdaRegWriteBase(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    int rc = hdaRegWriteU32(pState, offset, index, u32Value);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);
    switch(index)
    {
        case ICH6_HDA_REG_CORBLBASE:
            pState->u64CORBBase &= 0xFFFFFFFF00000000ULL;
            pState->u64CORBBase |= pState->au32Regs[index];
        break;
        case ICH6_HDA_REG_CORBUBASE:
            pState->u64CORBBase &= 0x00000000FFFFFFFFULL;
            pState->u64CORBBase |= ((uint64_t)pState->au32Regs[index] << 32);
        break;
        case ICH6_HDA_REG_RIRLBASE:
            pState->u64RIRBBase &= 0xFFFFFFFF00000000ULL;
            pState->u64RIRBBase |= pState->au32Regs[index];
        break;
        case ICH6_HDA_REG_RIRUBASE:
            pState->u64RIRBBase &= 0x00000000FFFFFFFFULL;
            pState->u64RIRBBase |= ((uint64_t)pState->au32Regs[index] << 32);
        break;
        case ICH6_HDA_REG_DPLBASE:
            /* @todo: first bit has special meaning */
            pState->u64DPBase &= 0xFFFFFFFF00000000ULL;
            pState->u64DPBase |= pState->au32Regs[index];
        break;
        case ICH6_HDA_REG_DPUBASE:
            pState->u64DPBase &= 0x00000000FFFFFFFFULL;
            pState->u64DPBase |= ((uint64_t)pState->au32Regs[index] << 32);
        break;
        default:
            AssertMsgFailed(("Invalid index"));
    }
    Log(("hda: CORB base:%llx RIRB base: %llx DP base: %llx\n", pState->u64CORBBase, pState->u64RIRBBase, pState->u64DPBase));
    return rc;
}

DECLCALLBACK(int)hdaRegWriteRIRBSTS(INTELHDLinkState* pState, uint32_t offset, uint32_t index, uint32_t u32Value)
{
    uint8_t v = RIRBSTS(pState);
    RIRBSTS(pState) &= ~(v & u32Value);

    return hdaProcessInterrupt(pState);
}

#ifdef LOG_ENABLED
static void dump_bd(INTELHDLinkState *pState, PHDABDLEDESC pBdle, uint64_t u64BaseDMA)
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
        PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pState), u64BaseDMA + i*16, bdle, 16);
        addr = *(uint64_t *)bdle;
        len = *(uint32_t *)&bdle[8];
        ioc = *(uint32_t *)&bdle[12];
        Log(("hda: %s bdle[%d] a:%llx, len:%d, ioc:%d\n",  (i == pBdle->u32BdleCvi? "[C]": "   "), i, addr, len, ioc & 0x1));
        sum += len;
    }
    Log(("hda: sum: %d\n", sum));
    for (i = 0; i < 8; ++i)
    {
        PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pState), (pState->u64DPBase & DPBASE_ADDR_MASK) + i*8, &counter, sizeof(&counter));
        Log(("hda: %s stream[%d] counter=%x\n", i == SDCTL_NUM(pState, 4) || i == SDCTL_NUM(pState, 0)? "[C]": "   ",
             i , counter));
    }
#endif
}
#endif

static void hdaFetchBdle(INTELHDLinkState *pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    uint8_t  bdle[16];
    Assert((   pStreamDesc->u64BaseDMA
            && pBdle
            && pBdle->u32BdleMaxCvi));
    PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pState), pStreamDesc->u64BaseDMA + pBdle->u32BdleCvi*16, bdle, 16);
    pBdle->u64BdleCviAddr = *(uint64_t *)bdle;
    pBdle->u32BdleCviLen = *(uint32_t *)&bdle[8];
    pBdle->fBdleCviIoc = (*(uint32_t *)&bdle[12]) & 0x1;
#ifdef LOG_ENABLED
    dump_bd(pState, pBdle, pStreamDesc->u64BaseDMA);
#endif
}

static inline uint32_t hdaCalculateTransferBufferLength(PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t u32SoundBackendBufferBytesAvail, uint32_t u32CblLimit)
{
    uint32_t cb2Copy;
    /*
     * Amounts of bytes depends on current position in buffer (u32BdleCviLen-u32BdleCviPos)
     */
    Assert((pBdle->u32BdleCviLen >= pBdle->u32BdleCviPos)); /* sanity */
    cb2Copy = pBdle->u32BdleCviLen - pBdle->u32BdleCviPos;
    /*
     * we may increase the counter in range of [0, FIFOS + 1]
     */
    cb2Copy = RT_MIN(cb2Copy, pStreamDesc->u32Fifos + 1);
    Assert((u32SoundBackendBufferBytesAvail > 0));

    /* sanity check to avoid overriding sound backend buffer */
    cb2Copy = RT_MIN(cb2Copy, u32SoundBackendBufferBytesAvail);
    cb2Copy = RT_MIN(cb2Copy, u32CblLimit);

    if (cb2Copy <= pBdle->cbUnderFifoW)
        return 0;
    cb2Copy -= pBdle->cbUnderFifoW; /* forcely reserve amount of ureported bytes to copy */
    return cb2Copy;
}

static inline void hdaBackendWriteTransferReported(PHDABDLEDESC pBdle, uint32_t cbArranged2Copy, uint32_t cbCopied, uint32_t *pu32DMACursor, uint32_t *pu32BackendBufferCapacity)
{
    Log(("hda:hdaBackendWriteTransferReported: cbArranged2Copy: %d, cbCopied: %d, pu32DMACursor: %d, pu32BackendBufferCapacity:%d\n",
        cbArranged2Copy, cbCopied, pu32DMACursor ? *pu32DMACursor : 0, pu32BackendBufferCapacity ? *pu32BackendBufferCapacity : 0));
    Assert((cbCopied));
    Assert((pu32BackendBufferCapacity && *pu32BackendBufferCapacity));
    /* Assertion!!! It was copied less than cbUnderFifoW
     * Probably we need to move the buffer, but it rather hard to imagine situation
     * why it may happen.
     */
    Assert((cbCopied == pBdle->cbUnderFifoW + cbArranged2Copy)); /* we assume that we write whole buffer including not reported bytes */
    if (   pBdle->cbUnderFifoW
        && pBdle->cbUnderFifoW <= cbCopied)
        Log(("hda:hdaBackendWriteTransferReported: CVI resetting cbUnderFifoW:%d(pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    pBdle->cbUnderFifoW -= RT_MIN(pBdle->cbUnderFifoW, cbCopied);
    Assert((!pBdle->cbUnderFifoW)); /* Assert!!! Assumption failed */

    /* We always increment position on DMA buffer counter because we're always reading to intermediate buffer */
    pBdle->u32BdleCviPos += cbArranged2Copy;

    Assert((pBdle->u32BdleCviLen >= pBdle->u32BdleCviPos && *pu32BackendBufferCapacity >= cbCopied)); /* sanity */
    /* We reports all bytes (including unreported previously) */
    *pu32DMACursor += cbCopied;
    /* reducing backend counter on amount of bytes we copied to backend */
    *pu32BackendBufferCapacity -= cbCopied;
    Log(("hda:hdaBackendWriteTransferReported: CVI(pos:%d, len:%d), pu32DMACursor: %d, pu32BackendBufferCapacity:%d\n",
        pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, *pu32DMACursor, *pu32BackendBufferCapacity));
}

static inline void hdaBackendReadTransferReported(PHDABDLEDESC pBdle, uint32_t cbArranged2Copy, uint32_t cbCopied, uint32_t *pu32DMACursor, uint32_t *pu32BackendBufferCapacity)
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

static inline void hdaBackendTransferUnreported(INTELHDLinkState *pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t cbCopied, uint32_t *pu32BackendBufferCapacity)
{
    Log(("hda:hdaBackendTransferUnreported: CVI (cbUnderFifoW:%d, pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));
    pBdle->u32BdleCviPos += cbCopied;
    pBdle->cbUnderFifoW += cbCopied;
    /* In case of read transaction we're always coping from backend buffer */
    if (pu32BackendBufferCapacity)
        *pu32BackendBufferCapacity -= cbCopied;
    Log(("hda:hdaBackendTransferUnreported: CVI (cbUnderFifoW:%d, pos:%d, len:%d)\n", pBdle->cbUnderFifoW, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));
    Assert((pBdle->cbUnderFifoW <= hdaFifoWToSz(pState, pStreamDesc)));
}
static inline bool hdaIsTransferCountersOverlapped(PINTELHDLinkState pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    bool fOnBufferEdge = (   *pStreamDesc->pu32Lpib == pStreamDesc->u32Cbl
                          || pBdle->u32BdleCviPos == pBdle->u32BdleCviLen);

    Assert((*pStreamDesc->pu32Lpib <= pStreamDesc->u32Cbl));

    if (*pStreamDesc->pu32Lpib == pStreamDesc->u32Cbl)
        *pStreamDesc->pu32Lpib -= pStreamDesc->u32Cbl;
    hdaUpdatePosBuf(pState, pStreamDesc);

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

static inline void hdaStreamCounterUpdate(PINTELHDLinkState pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t cbInc)
{
    /*
     * if we're under FIFO Watermark it's expected that HDA doesn't fetch anything.
     * (ICH6 datasheet 18.2.38)
     */
    if (!pBdle->cbUnderFifoW)
    {
        *pStreamDesc->pu32Lpib += cbInc;

        /*
         * Assert. Overlapping of buffer counter shouldn't happen.
         */
        Assert((*pStreamDesc->pu32Lpib <= pStreamDesc->u32Cbl));

        hdaUpdatePosBuf(pState, pStreamDesc);

    }
}

static inline bool hdaDoNextTransferCycle(PINTELHDLinkState pState, PHDABDLEDESC pBdle, PHDASTREAMTRANSFERDESC pStreamDesc)
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
             * but in general sound quality becomes lesser.
             */
            *pStreamDesc->pu32Sts |= HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS);

            /*
             * we should generate the interrupt if ICE bit of SDCTL register is set.
             */
            if (pStreamDesc->u32Ctl & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE))
                hdaProcessInterrupt(pState);
        }
        fDoNextTransferLoop = false;
    }
    return fDoNextTransferLoop;
}

/*
 * hdaReadAudio - copies samples from Qemu Sound back-end to DMA.
 * Note: this function writes immediately to DMA buffer, but "reports bytes" when all conditions meet (FIFOW)
 */
static uint32_t hdaReadAudio(INTELHDLinkState *pState, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t *pu32Avail, bool *fStop, uint32_t u32CblLimit)
{
    PHDABDLEDESC pBdle = &pState->stInBdle;
    uint32_t cbTransfered = 0;
    uint32_t cb2Copy = 0;
    uint32_t cbBackendCopy = 0;

    Log(("hda:ra: CVI(pos:%d, len:%d)\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    cb2Copy = hdaCalculateTransferBufferLength(pBdle, pStreamDesc, *pu32Avail, u32CblLimit);
    if (!cb2Copy)
    {
        /* if we enter here we can't report "unreported bits" */
        *fStop = true;
        goto done;
    }


    /*
     * read from backend input line to last ureported position or at the begining.
     */
    cbBackendCopy = AUD_read (pState->Codec.SwVoiceIn, pBdle->au8HdaBuffer, cb2Copy);
    /*
     * write on the HDA DMA
     */
    PDMDevHlpPhysWrite(ICH6_HDASTATE_2_DEVINS(pState), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer, cbBackendCopy);

    /* Don't see reasons why cb2Copy could differ from cbBackendCopy */
    Assert((cbBackendCopy == cb2Copy && (*pu32Avail) >= cb2Copy)); /* sanity */

    if (pBdle->cbUnderFifoW + cbBackendCopy > hdaFifoWToSz(pState, 0))
        hdaBackendReadTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransfered, pu32Avail);
    else
    {
        hdaBackendTransferUnreported(pState, pBdle, pStreamDesc, cbBackendCopy, pu32Avail);
        *fStop = true;
    }
    done:
    Assert((cbTransfered <= (SDFIFOS(pState, 0) + 1)));
    Log(("hda:ra: CVI(pos:%d, len:%d) cbTransfered: %d\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, cbTransfered));
    return cbTransfered;
}

static uint32_t hdaWriteAudio(INTELHDLinkState *pState, PHDASTREAMTRANSFERDESC pStreamDesc, uint32_t *pu32Avail, bool *fStop, uint32_t u32CblLimit)
{
    PHDABDLEDESC pBdle = &pState->stOutBdle;
    uint32_t cbTransfered = 0;
    uint32_t cb2Copy = 0; /* local byte counter (on local buffer) */
    uint32_t cbBackendCopy = 0; /* local byte counter, how many bytes copied to backend */

    Log(("hda:wa: CVI(cvi:%d, pos:%d, len:%d)\n", pBdle->u32BdleCvi, pBdle->u32BdleCviPos, pBdle->u32BdleCviLen));

    cb2Copy = hdaCalculateTransferBufferLength(pBdle, pStreamDesc, *pu32Avail, u32CblLimit);

    /*
     * Copy from DMA to the corresponding hdaBuffer (if there exists some bytes from the previous not reported transfer we write to ''pBdle->cbUnderFifoW'' offset)
     */
    if (!cb2Copy)
    {
        *fStop = true;
        goto done;
    }

    PDMDevHlpPhysRead(ICH6_HDASTATE_2_DEVINS(pState), pBdle->u64BdleCviAddr + pBdle->u32BdleCviPos, pBdle->au8HdaBuffer + pBdle->cbUnderFifoW, cb2Copy);
    /*
     * Write to audio backend. we should be sure whether we have enought bytes to copy to Audio backend.
     */
    if (cb2Copy + pBdle->cbUnderFifoW >= hdaFifoWToSz(pState, pStreamDesc))
    {
        /*
         * We feed backend with new portion of fetched samples including not reported.
         */
        cbBackendCopy = AUD_write (pState->Codec.SwVoiceOut, pBdle->au8HdaBuffer, cb2Copy + pBdle->cbUnderFifoW);
        hdaBackendWriteTransferReported(pBdle, cb2Copy, cbBackendCopy, &cbTransfered, pu32Avail);
    }
    else
    {
        /* Not enough bytes to be processed and reported, check luck on next enterence */
        hdaBackendTransferUnreported(pState, pBdle, pStreamDesc, cb2Copy, NULL);
        *fStop = true;
    }

    done:
    Assert((cbTransfered <= (SDFIFOS(pState, 4) + 1)));
    Log(("hda:wa: CVI(pos:%d, len:%d, cbTransfered:%d)\n", pBdle->u32BdleCviPos, pBdle->u32BdleCviLen, cbTransfered));
    return cbTransfered;
}

DECLCALLBACK(int) hdaCodecReset(CODECState *pCodecState)
{
    INTELHDLinkState *pState = (INTELHDLinkState *)pCodecState->pHDAState;
    return VINF_SUCCESS;
}

static inline void hdaInitTransferDescriptor(PINTELHDLinkState pState, PHDABDLEDESC pBdle, uint8_t u8Strm, PHDASTREAMTRANSFERDESC pStreamDesc)
{
    Assert((   pState
            && pBdle
            && pStreamDesc
            && u8Strm <= 7));
    memset(pStreamDesc, 0, sizeof(HDASTREAMTRANSFERDESC));
    pStreamDesc->u8Strm = u8Strm;
    pStreamDesc->u32Ctl = HDA_STREAM_REG2(pState, CTL, u8Strm);
    pStreamDesc->u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG2(pState, BDPL, u8Strm),
                                          HDA_STREAM_REG2(pState, BDPU, u8Strm));
    pStreamDesc->pu32Lpib = &HDA_STREAM_REG2(pState, LPIB, u8Strm);
    pStreamDesc->pu32Sts = &HDA_STREAM_REG2(pState, STS, u8Strm);
    pStreamDesc->u32Cbl = HDA_STREAM_REG2(pState, CBL, u8Strm);
    pStreamDesc->u32Fifos = HDA_STREAM_REG2(pState, FIFOS, u8Strm);

    pBdle->u32BdleMaxCvi = HDA_STREAM_REG2(pState, LVI, u8Strm);
#ifdef LOG_ENABLED
    if (   pBdle
        && pBdle->u32BdleMaxCvi)
    {
        Log(("Initialization of transfer descriptor:\n"));
        dump_bd(pState, pBdle, pStreamDesc->u64BaseDMA);
    }
#endif
}

DECLCALLBACK(void) hdaTransfer(CODECState *pCodecState, ENMSOUNDSOURCE src, int avail)
{
    bool fStop = false;
    uint8_t u8Strm = 0;
    PHDABDLEDESC pBdle = NULL;
    INTELHDLinkState *pState = (INTELHDLinkState *)pCodecState->pHDAState;
    HDASTREAMTRANSFERDESC stStreamDesc;
    uint32_t nBytes;
    switch (src)
    {
        case PO_INDEX:
        {
            u8Strm = 4;
            pBdle = &pState->stOutBdle;
            break;
        }
        case PI_INDEX:
        {
            u8Strm = 0;
            pBdle = &pState->stInBdle;
            break;
        }
        default:
            return;
    }
    hdaInitTransferDescriptor(pState, pBdle, u8Strm, &stStreamDesc);
    while( avail && !fStop)
    {
        Assert (   (stStreamDesc.u32Ctl & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN))
                && avail
                && stStreamDesc.u64BaseDMA);

        /* Fetch the Buffer Descriptor Entry (BDE). */

        if (hdaIsTransferCountersOverlapped(pState, pBdle, &stStreamDesc))
            hdaFetchBdle(pState, pBdle, &stStreamDesc);
        *stStreamDesc.pu32Sts |= HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);
        Assert((avail >= 0 && (stStreamDesc.u32Cbl >= (*stStreamDesc.pu32Lpib)))); /* sanity */
        uint32_t u32CblLimit = stStreamDesc.u32Cbl - (*stStreamDesc.pu32Lpib);
        Assert((u32CblLimit > hdaFifoWToSz(pState, &stStreamDesc)));
        Log(("hda: CBL=%d, LPIB=%d\n", stStreamDesc.u32Cbl, *stStreamDesc.pu32Lpib));
        switch (src)
        {
            case PO_INDEX:
                nBytes = hdaWriteAudio(pState, &stStreamDesc, (uint32_t *)&avail, &fStop, u32CblLimit);
                break;
            case PI_INDEX:
                nBytes = hdaReadAudio(pState, &stStreamDesc, (uint32_t *)&avail, &fStop, u32CblLimit);
                break;
            default:
                nBytes = 0;
                fStop  = true;
                AssertMsgFailed(("Unsupported"));
        }
        Assert(nBytes <= (stStreamDesc.u32Fifos + 1));
        *stStreamDesc.pu32Sts &= ~HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY);

        /* Process end of buffer condition. */
        hdaStreamCounterUpdate(pState, pBdle, &stStreamDesc, nBytes);
        fStop = !fStop ? !hdaDoNextTransferCycle(pState, pBdle, &stStreamDesc) : fStop;
    }
}

/**
 * Handle register read operation.
 *
 * Looks up and calls appropriate handler.
 *
 * @note: while implementation was detected so called "forgotten" or "hole" registers
 * which description is missed in RPM, datasheet or spec.
 *
 * @returns VBox status code.
 *
 * @param   pState      The device state structure.
 * @param   uOffset     Register offset in memory-mapped frame.
 * @param   pv          Where to fetch the value.
 * @param   cb          Number of bytes to write.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) hdaMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    int rc = VINF_SUCCESS;
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    uint32_t offReg  = GCPhysAddr - pThis->hda.addrMMReg;
    int idxReg = hdaMMIORegLookup(&pThis->hda, offReg);
    if (pThis->hda.fInReset && idxReg != ICH6_HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));

    if (idxReg == -1)
        LogRel(("hda: Invalid read access @0x%x(of bytes:%d)\n", offReg, cb));

    if (idxReg != -1)
    {
        /** @todo r=bird: Accesses crossing register boundraries aren't handled
         *        right from what I can tell?  If they are, please explain
         *        what the rules are. */
        uint32_t mask = 0;
        uint32_t shift = (s_ichIntelHDRegMap[idxReg].offset - offReg) % sizeof(uint32_t) * 8;
        uint32_t u32Value = 0;
        switch(cb)
        {
            case 1: mask = 0x000000ff; break;
            case 2: mask = 0x0000ffff; break;
            case 4:
            /* 18.2 of ICH6 datasheet defines wideness of the accesses byte, word and double word */
            case 8:
                mask = 0xffffffff;
                cb = 4;
                break;
        }
#if 0
        /* cross register access. Mac guest hit this assert doing assumption 4 byte access to 3 byte registers e.g. {I,O}SDnCTL
         */
        //Assert((cb <= s_ichIntelHDRegMap[idxReg].size - (offReg - s_ichIntelHDRegMap[idxReg].offset)));
        if (cb > s_ichIntelHDRegMap[idxReg].size - (offReg - s_ichIntelHDRegMap[idxReg].offset))
        {
            int off = cb - (s_ichIntelHDRegMap[idxReg].size - (offReg - s_ichIntelHDRegMap[idxReg].offset));
            rc = hdaMMIORead(pDevIns, pvUser, GCPhysAddr + cb - off, (char *)pv + cb - off, off);
            if (RT_FAILURE(rc))
                AssertRCReturn (rc, rc);
        }
        //Assert(((offReg - s_ichIntelHDRegMap[idxReg].offset) == 0));
#endif
        mask <<= shift;
        rc = s_ichIntelHDRegMap[idxReg].pfnRead(&pThis->hda, offReg, idxReg, &u32Value);
        *(uint32_t *)pv |= (u32Value & mask);
        Log(("hda: read %s[%x/%x]\n", s_ichIntelHDRegMap[idxReg].abbrev, u32Value, *(uint32_t *)pv));
        return rc;
    }
    *(uint32_t *)pv = 0xFF;
    Log(("hda: hole at %x is accessed for read\n", offReg));
    return rc;
}

/**
 * Handle register write operation.
 *
 * Looks up and calls appropriate handler.
 *
 * @returns VBox status code.
 *
 * @param   pState      The device state structure.
 * @param   uOffset     Register offset in memory-mapped frame.
 * @param   pv          Where to fetch the value.
 * @param   cb          Number of bytes to write.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) hdaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PCIINTELHDLinkState    *pThis     = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    uint32_t                offReg    = GCPhysAddr - pThis->hda.addrMMReg;
    int                     idxReg    = hdaMMIORegLookup(&pThis->hda, offReg);
    int                     rc        = VINF_SUCCESS;

    if (pThis->hda.fInReset && idxReg != ICH6_HDA_REG_GCTL)
        Log(("hda: access to registers except GCTL is blocked while reset\n"));

    if (   idxReg == -1
        || cb > 4)
        LogRel(("hda: Invalid write access @0x%x(of bytes:%d)\n", offReg, cb));

    if (idxReg != -1)
    {
        /** @todo r=bird: This looks like code for handling unalinged register
         * accesses.  If it isn't then, add a comment explaing what you're
         * trying to do here.  OTOH, if it is then it has the following
         * issues:
         *      -# You're calculating the wrong new value for the register.
         *      -# You're not handling cross register accesses.  Imagine a
         *       4-byte write starting at CORBCTL, or a 8-byte write.
         *
         * PS! consider dropping the 'offset' argument to pfnWrite/pfnRead as
         * nobody seems to be using it and it just add complexity when reading
         * the code.
         *
         */
        uint32_t u32CurValue = pThis->hda.au32Regs[idxReg];
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
                /* 18.2 of ICH6 datasheet defines wideness of the accesses byte, word and double word */
                u32NewValue = *(uint32_t const *)pv;
                mask = 0xffffffff;
                cb = 4;
                break;
            default:
                AssertFailedReturn(VERR_INTERNAL_ERROR_4); /* shall not happen. */
        }
        /* cross register access, see corresponding comment in hdaMMIORead */
#if 0
        if (cb > s_ichIntelHDRegMap[idxReg].size - (offReg - s_ichIntelHDRegMap[idxReg].offset))
        {
            int off = cb - (s_ichIntelHDRegMap[idxReg].size - (offReg - s_ichIntelHDRegMap[idxReg].offset));
            rc = hdaMMIOWrite(pDevIns, pvUser, GCPhysAddr + cb - off, (char *)pv + cb - off, off);
            if (RT_FAILURE(rc))
                AssertRCReturn (rc, rc);
        }
#endif
        uint32_t shift = (s_ichIntelHDRegMap[idxReg].offset - offReg) % sizeof(uint32_t) * 8;
        mask <<= shift;
        u32NewValue <<= shift;
        u32NewValue &= mask;
        u32NewValue |= (u32CurValue & ~mask);

        rc = s_ichIntelHDRegMap[idxReg].pfnWrite(&pThis->hda, offReg, idxReg, u32NewValue);
        Log(("hda: write %s:(%x) %x => %x\n", s_ichIntelHDRegMap[idxReg].abbrev, u32NewValue,
             u32CurValue, pThis->hda.au32Regs[idxReg]));
        return rc;
    }

    Log(("hda: hole at %x is accessed for write\n", offReg));
    return rc;
}

/**
 * Callback function for mapping a PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device.
 *                          Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region.
 *                          If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative
 *                          to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) hdaMap(PPCIDEVICE pPciDev, int iRegion,
                                RTGCPHYS GCPhysAddress, uint32_t cb,
                                PCIADDRESSSPACE enmType)
{
    int         rc;
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    RTIOPORT    Port = (RTIOPORT)GCPhysAddress;
    PCIINTELHDLinkState *pThis = PCIDEV_2_ICH6_HDASTATE(pPciDev);

    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    rc = PDMDevHlpMMIORegister(pPciDev->pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                               IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                               hdaMMIOWrite, hdaMMIORead, "ICH6_HDA");

    if (RT_FAILURE(rc))
        return rc;

    pThis->hda.addrMMReg = GCPhysAddress;
    return VINF_SUCCESS;
}

/**
 * Saves a state of the HDA device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM  The handle to save the state to.
 */
static DECLCALLBACK(int) hdaSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    /* Save Codec nodes states */
    codecSaveState(&pThis->hda.Codec, pSSM);

    /* Save MMIO registers */
    AssertCompile(RT_ELEMENTS(pThis->hda.au32Regs) == 112);
    SSMR3PutU32(pSSM, RT_ELEMENTS(pThis->hda.au32Regs));
    SSMR3PutMem(pSSM, pThis->hda.au32Regs, sizeof(pThis->hda.au32Regs));

    /* Save HDA dma counters */
    SSMR3PutStructEx(pSSM, &pThis->hda.stOutBdle, sizeof(pThis->hda.stOutBdle), 0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    SSMR3PutStructEx(pSSM, &pThis->hda.stMicBdle, sizeof(pThis->hda.stMicBdle), 0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    SSMR3PutStructEx(pSSM, &pThis->hda.stInBdle,  sizeof(pThis->hda.stInBdle),  0 /*fFlags*/, g_aHdaBDLEDescFields, NULL);
    return VINF_SUCCESS;
}

/**
 * Loads a saved HDA device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) hdaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Load Codec nodes states.
     */
    int rc = codecLoadState(&pThis->hda.Codec, pSSM, uVersion);
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
               registers due to some code cleanups.  This only affects trunk
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
            AssertCompile(RT_ELEMENTS(pThis->hda.au32Regs) == 112);
            break;

        case HDA_SSM_VERSION:
            rc = SSMR3GetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
            AssertLogRelMsgReturn(cRegs == RT_ELEMENTS(pThis->hda.au32Regs),
                                  ("cRegs is %d, expected %d\n", cRegs, RT_ELEMENTS(pThis->hda.au32Regs)),
                                  VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
            break;

        default:
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (cRegs >= RT_ELEMENTS(pThis->hda.au32Regs))
    {
        SSMR3GetMem(pSSM, pThis->hda.au32Regs, sizeof(pThis->hda.au32Regs));
        SSMR3Skip(pSSM, sizeof(uint32_t) * (cRegs - RT_ELEMENTS(pThis->hda.au32Regs)));
    }
    else
    {
        RT_ZERO(pThis->hda.au32Regs);
        SSMR3GetMem(pSSM, pThis->hda.au32Regs, sizeof(uint32_t) * cRegs);
    }

    /*
     * Load HDA dma counters.
     */
    uint32_t   fFlags   = uVersion <= HDA_SSM_VERSION_2 ? SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED : 0;
    PCSSMFIELD paFields = uVersion <= HDA_SSM_VERSION_2 ? g_aHdaBDLEDescFieldsOld              : g_aHdaBDLEDescFields;
    SSMR3GetStructEx(pSSM, &pThis->hda.stOutBdle, sizeof(pThis->hda.stOutBdle), fFlags, paFields, NULL);
    SSMR3GetStructEx(pSSM, &pThis->hda.stMicBdle, sizeof(pThis->hda.stMicBdle), fFlags, paFields, NULL);
    rc = SSMR3GetStructEx(pSSM, &pThis->hda.stInBdle, sizeof(pThis->hda.stInBdle), fFlags, paFields, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Update stuff after the state changes.
     */
    AUD_set_active_in(pThis->hda.Codec.SwVoiceIn, SDCTL(&pThis->hda, 0) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    AUD_set_active_out(pThis->hda.Codec.SwVoiceOut, SDCTL(&pThis->hda, 4) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));

    pThis->hda.u64CORBBase = RT_MAKE_U64(CORBLBASE(&pThis->hda), CORBUBASE(&pThis->hda));
    pThis->hda.u64RIRBBase = RT_MAKE_U64(RIRLBASE(&pThis->hda), RIRUBASE(&pThis->hda));
    pThis->hda.u64DPBase   = RT_MAKE_U64(DPLBASE(&pThis->hda), DPUBASE(&pThis->hda));
    return VINF_SUCCESS;
}

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
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    GCAP(&pThis->hda) = HDA_MAKE_GCAP(4,4,0,0,1); /* see 6.2.1 */
    VMIN(&pThis->hda) = 0x00;       /* see 6.2.2 */
    VMAJ(&pThis->hda) = 0x01;       /* see 6.2.3 */
    VMAJ(&pThis->hda) = 0x01;       /* see 6.2.3 */
    OUTPAY(&pThis->hda) = 0x003C;   /* see 6.2.4 */
    INPAY(&pThis->hda)  = 0x001D;   /* see 6.2.5 */
    pThis->hda.au32Regs[ICH6_HDA_REG_CORBSIZE] = 0x42; /* see 6.2.1 */
    pThis->hda.au32Regs[ICH6_HDA_REG_RIRBSIZE] = 0x42; /* see 6.2.1 */
    CORBRP(&pThis->hda) = 0x0;
    RIRBWP(&pThis->hda) = 0x0;

    Log(("hda: inter HDA reset.\n"));
    pThis->hda.cbCorbBuf = 256 * sizeof(uint32_t);

    if (pThis->hda.pu32CorbBuf)
        memset(pThis->hda.pu32CorbBuf, 0, pThis->hda.cbCorbBuf);
    else
        pThis->hda.pu32CorbBuf = (uint32_t *)RTMemAllocZ(pThis->hda.cbCorbBuf);

    pThis->hda.cbRirbBuf = 256 * sizeof(uint64_t);
    if (pThis->hda.pu64RirbBuf)
        memset(pThis->hda.pu64RirbBuf, 0, pThis->hda.cbRirbBuf);
    else
        pThis->hda.pu64RirbBuf = (uint64_t *)RTMemAllocZ(pThis->hda.cbRirbBuf);

    pThis->hda.u64BaseTS = PDMDevHlpTMTimeVirtGetNano(pDevIns);

    HDABDLEDESC stEmptyBdle;
    for(uint8_t u8Strm = 0; u8Strm < 8; ++u8Strm)
    {
        HDASTREAMTRANSFERDESC stStreamDesc;
        PHDABDLEDESC pBdle = NULL;
        if (u8Strm == 0)
            pBdle = &pThis->hda.stInBdle;
        else if(u8Strm == 4)
            pBdle = &pThis->hda.stOutBdle;
        else
        {
            memset(&stEmptyBdle, 0, sizeof(HDABDLEDESC));
            pBdle = &stEmptyBdle;
        }
        hdaInitTransferDescriptor(&pThis->hda, pBdle, u8Strm, &stStreamDesc);
        /* hdaStreamReset prevents changing SRST bit, so we zerro it here forcely. */
        HDA_STREAM_REG2(&pThis->hda, CTL, u8Strm) = 0;
        hdaStreamReset(&pThis->hda, pBdle, &stStreamDesc, u8Strm);
    }

    /* emulateion of codec "wake up" HDA spec (5.5.1 and 6.5)*/
    STATESTS(&pThis->hda) = 0x1;

    Log(("hda: reset finished\n"));
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) hdaQueryInterface (struct PDMIBASE *pInterface,
                                                   const char *pszIID)
{
    PCIINTELHDLinkState *pThis = RT_FROM_MEMBER(pInterface, PCIINTELHDLinkState, hda.IBase);
    Assert(&pThis->hda.IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->hda.IBase);
    return NULL;
}

static inline int hdaLookUpRegisterByName(INTELHDLinkState *pState, const char *pszArgs)
{
    int iReg = 0;
    for (; iReg < HDA_NREGS; ++iReg)
        if (!RTStrICmp(s_ichIntelHDRegMap[iReg].abbrev, pszArgs))
            return iReg;
    return -1;
}
static inline void hdaDbgPrintRegister(INTELHDLinkState *pState, PCDBGFINFOHLP pHlp, int iHdaIndex)
{
    Assert(   pState
           && iHdaIndex >= 0
           && iHdaIndex < HDA_NREGS);
    pHlp->pfnPrintf(pHlp, "hda: %s: 0x%x\n", s_ichIntelHDRegMap[iHdaIndex].abbrev, pState->au32Regs[iHdaIndex]);
}
static DECLCALLBACK(void) hdaDbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    INTELHDLinkState *hda = &pThis->hda;
    int iHdaRegisterIndex = hdaLookUpRegisterByName(hda, pszArgs);
    if (iHdaRegisterIndex != -1)
        hdaDbgPrintRegister(hda, pHlp, iHdaRegisterIndex);
    else
        for(iHdaRegisterIndex = 0; (unsigned int)iHdaRegisterIndex < HDA_NREGS; ++iHdaRegisterIndex)
            hdaDbgPrintRegister(hda, pHlp, iHdaRegisterIndex);
}

static inline void hdaDbgPrintStream(INTELHDLinkState *pState, PCDBGFINFOHLP pHlp, int iHdaStrmIndex)
{
    Assert(   pState
           && iHdaStrmIndex >= 0
           && iHdaStrmIndex < 7);
    pHlp->pfnPrintf(pHlp, "Dump of %d Hda Stream:\n", iHdaStrmIndex);
    pHlp->pfnPrintf(pHlp, "SD%dCTL: %R[sdctl]\n", iHdaStrmIndex, HDA_STREAM_REG2(pState, CTL, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dCTS: %R[sdsts]\n", iHdaStrmIndex, HDA_STREAM_REG2(pState, STS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOS: %R[sdfifos]\n", iHdaStrmIndex, HDA_STREAM_REG2(pState, FIFOS, iHdaStrmIndex));
    pHlp->pfnPrintf(pHlp, "SD%dFIFOW: %R[sdfifow]\n", iHdaStrmIndex, HDA_STREAM_REG2(pState, FIFOW, iHdaStrmIndex));
}

static inline int hdaLookUpStreamIndex(INTELHDLinkState *pState, const char *pszArgs)
{
    /* todo: add args parsing */
    return -1;
}
static DECLCALLBACK(void) hdaDbgStreamInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    INTELHDLinkState *hda = &pThis->hda;
    int iHdaStrmIndex = hdaLookUpStreamIndex(hda, pszArgs);
    if (iHdaStrmIndex != -1)
        hdaDbgPrintStream(hda, pHlp, iHdaStrmIndex);
    else
        for(iHdaStrmIndex = 0; iHdaStrmIndex < 7; ++iHdaStrmIndex)
            hdaDbgPrintStream(hda, pHlp, iHdaStrmIndex);
}

/* Codec debugger interface */
static DECLCALLBACK(void) hdaCodecDbgNodes(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    INTELHDLinkState *hda = &pThis->hda;
    if (hda->Codec.pfnCodecDbgListNodes)
        hda->Codec.pfnCodecDbgListNodes(&hda->Codec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
}

static DECLCALLBACK(void) hdaCodecDbgSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    INTELHDLinkState *hda = &pThis->hda;
    if (hda->Codec.pfnCodecDbgSelector)
        hda->Codec.pfnCodecDbgSelector(&hda->Codec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback.\n");
}

//#define HDA_AS_PCI_EXPRESS
/* Misc routines */
static inline bool printHdaIsValid(const char *pszType, const char *pszExpectedFlag)
{
    return (RTStrCmp(pszType, pszExpectedFlag) == 0);
}
static const char *printHdaYesNo(bool fFlag)
{
    return fFlag ? "yes" : "no";
}
static DECLCALLBACK(size_t)
printHdaStrmCtl(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t sdCtl = (uint32_t)(uintptr_t)pvValue;
    size_t cb = 0;
    if (!printHdaIsValid(pszType, "sdctl"))
        return cb;
    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "SDCTL(raw: %#0x, strm:0x%x, dir:%s, tp:%s strip:%x, deie:%s, ioce:%s, run:%s, srst:%s)",
                      sdCtl,
                      ((sdCtl & HDA_REG_FIELD_MASK(SDCTL, NUM)) >> ICH6_HDA_SDCTL_NUM_SHIFT),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, DIR))),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, TP))),
                      ((sdCtl & HDA_REG_FIELD_MASK(SDCTL, STRIPE)) >> ICH6_HDA_SDCTL_STRIPE_SHIFT),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, DEIE))),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, ICE))),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN))),
                      printHdaYesNo(RT_BOOL(sdCtl & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST))));
    return cb;
}

static DECLCALLBACK(size_t)
printHdaStrmFifos(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t sdFifos = (uint32_t)(uintptr_t)pvValue;
    uint32_t u32Bytes = 0;
    size_t cb = 0;
    if (!printHdaIsValid(pszType, "sdfifos"))
        return cb;
    switch(sdFifos)
    {
        case HDA_SDONFIFO_16B: u32Bytes = 16; break;
        case HDA_SDONFIFO_32B: u32Bytes = 32; break;
        case HDA_SDONFIFO_64B: u32Bytes = 64; break;
        case HDA_SDONFIFO_128B: u32Bytes = 128; break;
        case HDA_SDONFIFO_192B: u32Bytes = 192; break;
        case HDA_SDONFIFO_256B: u32Bytes = 256; break;
        case HDA_SDINFIFO_120B: u32Bytes = 120; break;
        case HDA_SDINFIFO_160B: u32Bytes = 160; break;
        default:;
    }
    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "SDFIFOS(raw: %#0x, sdfifos:%d B)",
                      sdFifos,
                      u32Bytes);
    return cb;
}

static DECLCALLBACK(size_t)
printHdaStrmFifow(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t sdFifow = (uint32_t)(uintptr_t)pvValue;
    uint32_t u32Bytes = 0;
    size_t cb = 0;
    if (!printHdaIsValid(pszType, "sdfifow"))
        return cb;
    switch(sdFifow)
    {
        case HDA_SDFIFOW_8B: u32Bytes = 8; break;
        case HDA_SDFIFOW_16B: u32Bytes = 16; break;
        case HDA_SDFIFOW_32B: u32Bytes = 32; break;
    }
    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "SDFIFOW(raw: %#0x, sdfifow:%d B)",
                      sdFifow,
                      u32Bytes);
    return cb;
}

static DECLCALLBACK(size_t)
printHdaStrmSts(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                 const char *pszType, void const *pvValue,
                 int cchWidth, int cchPrecision, unsigned fFlags,
                 void *pvUser)
{
    uint32_t sdSts = (uint32_t)(uintptr_t)pvValue;
    size_t cb = 0;
    if (!printHdaIsValid(pszType, "sdsts"))
        return cb;
    cb += RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                      "SDSTS(raw: %#0x, fifordy:%s, dese:%s, fifoe:%s, bcis:%s)",
                      sdSts,
                      printHdaYesNo(RT_BOOL(sdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, FIFORDY))),
                      printHdaYesNo(RT_BOOL(sdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, DE))),
                      printHdaYesNo(RT_BOOL(sdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, FE))),
                      printHdaYesNo(RT_BOOL(sdSts & HDA_REG_FIELD_FLAG_MASK(SDSTS, BCIS))));
    return cb;
}
/**
 * This routine registers debugger info extensions and custom printf formatters
 */
static inline int hdaInitMisc(PPDMDEVINS pDevIns)
{
    int rc;
    PDMDevHlpDBGFInfoRegister(pDevIns, "hda", "HDA info. (hda [register case-insensitive])", hdaDbgInfo);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdastrm", "HDA stream info. (hdastrm [stream number])", hdaDbgStreamInfo);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcnodes", "HDA codec nodes.", hdaCodecDbgNodes);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcselector", "HDA codec's selector states [node number].", hdaCodecDbgSelector);
    rc = RTStrFormatTypeRegister("sdctl", printHdaStrmCtl, NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdsts", printHdaStrmSts, NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdfifos", printHdaStrmFifos, NULL);
    AssertRC(rc);
    rc = RTStrFormatTypeRegister("sdfifow", printHdaStrmFifow, NULL);
    AssertRC(rc);
#if 0
    rc = RTStrFormatTypeRegister("sdfmt", printHdaStrmFmt, NULL);
    AssertRC(rc);
#endif
    return rc;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hdaConstruct (PPDMDEVINS pDevIns, int iInstance,
                                       PCFGMNODE pCfgHandle)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);
    INTELHDLinkState    *s     = &pThis->hda;
    int               rc;

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validations.
     */
    if (!CFGMR3AreValuesValid (pCfgHandle, "\0"))
        return PDMDEV_SET_ERROR (pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                 N_ ("Invalid configuration for the INTELHD device"));

    // ** @todo r=michaln: This device may need R0/RC enabling, especially if guests
    // poll some register(s).

    /*
     * Initialize data (most of it anyway).
     */
    s->pDevIns                  = pDevIns;
    /* IBase */
    s->IBase.pfnQueryInterface  = hdaQueryInterface;

    /* PCI Device (the assertions will be removed later) */
    PCIDevSetVendorId           (&pThis->dev, HDA_PCI_VENDOR_ID); /* nVidia */
    PCIDevSetDeviceId           (&pThis->dev, HDA_PCI_DEICE_ID); /* HDA */

    PCIDevSetCommand            (&pThis->dev, 0x0000); /* 04 rw,ro - pcicmd. */
    PCIDevSetStatus             (&pThis->dev, VBOX_PCI_STATUS_CAP_LIST); /* 06 rwc?,ro? - pcists. */
    PCIDevSetRevisionId         (&pThis->dev, 0x01);   /* 08 ro - rid. */
    PCIDevSetClassProg          (&pThis->dev, 0x00);   /* 09 ro - pi. */
    PCIDevSetClassSub           (&pThis->dev, 0x03);   /* 0a ro - scc; 03 == HDA. */
    PCIDevSetClassBase          (&pThis->dev, 0x04);   /* 0b ro - bcc; 04 == multimedia. */
    PCIDevSetHeaderType         (&pThis->dev, 0x00);   /* 0e ro - headtyp. */
    PCIDevSetBaseAddress        (&pThis->dev, 0,       /* 10 rw - MMIO */
                                 false /* fIoSpace */, false /* fPrefetchable */, true /* f64Bit */, 0x00000000);
    PCIDevSetInterruptLine      (&pThis->dev, 0x00);   /* 3c rw. */
    PCIDevSetInterruptPin       (&pThis->dev, 0x01);   /* 3d ro - INTA#. */

#if defined(HDA_AS_PCI_EXPRESS)
    PCIDevSetCapabilityList     (&pThis->dev, 0x80);
#elif defined(VBOX_WITH_MSI_DEVICES)
    PCIDevSetCapabilityList     (&pThis->dev, 0x60);
#else
    PCIDevSetCapabilityList     (&pThis->dev, 0x50);   /* ICH6 datasheet 18.1.16 */
#endif

    /// @todo r=michaln: If there are really no PCIDevSetXx for these, the meaning
    // of these values needs to be properly documented!
    /* HDCTL off 0x40 bit 0 selects signaling mode (1-HDA, 0 - Ac97) 18.1.19 */
    PCIDevSetByte(&pThis->dev, 0x40, 0x01);

    /* Power Management */
    PCIDevSetByte(&pThis->dev, 0x50 + 0, VBOX_PCI_CAP_ID_PM);
    PCIDevSetByte(&pThis->dev, 0x50 + 1, 0x0); /* next */
    PCIDevSetWord(&pThis->dev, 0x50 + 2, VBOX_PCI_PM_CAP_DSI | 0x02 /* version, PM1.1 */ );

#ifdef HDA_AS_PCI_EXPRESS
    /* PCI Express */
    PCIDevSetByte  (&pThis->dev, 0x80 + 0, VBOX_PCI_CAP_ID_EXP); /* PCI_Express */
    PCIDevSetByte  (&pThis->dev, 0x80 + 1, 0x60); /* next */
    /* Device flags */
    PCIDevSetWord  (&pThis->dev, 0x80 + 2,
                    /* version */ 0x1     |
                    /* Root Complex Integrated Endpoint */ (VBOX_PCI_EXP_TYPE_ROOT_INT_EP << 4) |
                    /* MSI */ (100) << 9
                    );
    /* Device capabilities */
    PCIDevSetDWord (&pThis->dev, 0x80 + 4, VBOX_PCI_EXP_DEVCAP_FLRESET);
    /* Device control */
    PCIDevSetWord  (&pThis->dev, 0x80 + 8, 0);
    /* Device status */
    PCIDevSetWord  (&pThis->dev, 0x80 + 10, 0);
    /* Link caps */
    PCIDevSetDWord (&pThis->dev, 0x80 + 12, 0);
    /* Link control */
    PCIDevSetWord  (&pThis->dev, 0x80 + 16, 0);
    /* Link status */
    PCIDevSetWord  (&pThis->dev, 0x80 + 18, 0);
    /* Slot capabilities */
    PCIDevSetDWord (&pThis->dev, 0x80 + 20, 0);
    /* Slot control */
    PCIDevSetWord  (&pThis->dev, 0x80 + 24, 0);
    /* Slot status */
    PCIDevSetWord  (&pThis->dev, 0x80 + 26, 0);
    /* Root control */
    PCIDevSetWord  (&pThis->dev, 0x80 + 28, 0);
    /* Root capabilities */
    PCIDevSetWord  (&pThis->dev, 0x80 + 30, 0);
    /* Root status */
    PCIDevSetDWord (&pThis->dev, 0x80 + 32, 0);
    /* Device capabilities 2 */
    PCIDevSetDWord (&pThis->dev, 0x80 + 36, 0);
    /* Device control 2 */
    PCIDevSetQWord (&pThis->dev, 0x80 + 40, 0);
    /* Link control 2 */
    PCIDevSetQWord (&pThis->dev, 0x80 + 48, 0);
    /* Slot control 2 */
    PCIDevSetWord  (&pThis->dev, 0x80 + 56, 0);
#endif

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->dev);
    if (RT_FAILURE (rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister (pDevIns, 0, 0x4000, PCI_ADDRESS_SPACE_MEM,
                                       hdaMap);
    if (RT_FAILURE (rc))
        return rc;

#ifdef VBOX_WITH_MSI_DEVICES
    PDMMSIREG aMsiReg;

    RT_ZERO(aMsiReg);
    aMsiReg.cMsiVectors = 1;
    aMsiReg.iMsiCapOffset = 0x60;
    aMsiReg.iMsiNextOffset = 0x50;
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &aMsiReg);
    if (RT_FAILURE (rc))
    {
        LogRel(("Chipset cannot do MSI: %Rrc\n", rc));
        PCIDevSetCapabilityList     (&pThis->dev, 0x50);
    }
#endif

    rc = PDMDevHlpSSMRegister (pDevIns, HDA_SSM_VERSION, sizeof(*pThis), hdaSaveExec, hdaLoadExec);
    if (RT_FAILURE (rc))
        return rc;

    /*
     * Attach driver.
     */
    rc = PDMDevHlpDriverAttach (pDevIns, 0, &s->IBase,
                                &s->pDrvBase, "Audio Driver Port");
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log (("hda: No attached driver!\n"));
    else if (RT_FAILURE (rc))
    {
        AssertMsgFailed (("Failed to attach INTELHD LUN #0! rc=%Rrc\n", rc));
        return rc;
    }



    pThis->hda.Codec.pHDAState = (void *)&pThis->hda;
    rc = codecConstruct(pDevIns, &pThis->hda.Codec, pCfgHandle);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
       verb F20 should provide device/codec recognition. */
    Assert(pThis->hda.Codec.u16VendorId);
    Assert(pThis->hda.Codec.u16DeviceId);
    PCIDevSetSubSystemVendorId  (&pThis->dev, pThis->hda.Codec.u16VendorId); /* 2c ro - intel.) */
    PCIDevSetSubSystemId        (&pThis->dev, pThis->hda.Codec.u16DeviceId); /* 2e ro. */

    hdaReset (pDevIns);
    pThis->hda.Codec.id = 0;
    pThis->hda.Codec.pfnTransfer = hdaTransfer;
    pThis->hda.Codec.pfnReset = hdaCodecReset;
    /*
     * 18.2.6,7 defines that values of this registers might be cleared on power on/reset
     * hdaReset shouldn't affects these registers.
     */
    WAKEEN(&pThis->hda) = 0x0;
    STATESTS(&pThis->hda) = 0x0;
    hdaInitMisc(pDevIns);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) hdaDestruct (PPDMDEVINS pDevIns)
{
    PCIINTELHDLinkState *pThis = PDMINS_2_DATA(pDevIns, PCIINTELHDLinkState *);

    int rc = codecDestruct(&pThis->hda.Codec);
    AssertRC(rc);
    if (pThis->hda.pu32CorbBuf)
        RTMemFree(pThis->hda.pu32CorbBuf);
    if (pThis->hda.pu64RirbBuf)
        RTMemFree(pThis->hda.pu64RirbBuf);
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
    "ICH IntelHD Audio Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIINTELHDLinkState),
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
