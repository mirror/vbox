/* $Id$ */
/** @file
 * DevHDA - VBox Intel HD Audio Controller.
 *
 * Implemented against the specifications found in "High Definition Audio
 * Specification", Revision 1.0a June 17, 2010, and  "Intel I/O Controller
 * HUB 6 (ICH6) Family, Datasheet", document number 301473-002.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#include <iprt/file.h>
#include <iprt/list.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/semaphore.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"

#include "AudioMixBuffer.h"
#include "AudioMixer.h"
#include "HDACodec.h"
#include "DevHDACommon.h"
#include "DrvAudio.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
//#define HDA_AS_PCI_EXPRESS
#define VBOX_WITH_INTEL_HDA

/*
 * HDA_DEBUG_DUMP_PCM_DATA enables dumping the raw PCM data
 * to a file on the host. Be sure to adjust HDA_DEBUG_DUMP_PCM_DATA_PATH
 * to your needs before using this!
 */
//#define HDA_DEBUG_DUMP_PCM_DATA
#ifdef HDA_DEBUG_DUMP_PCM_DATA
# ifdef RT_OS_WINDOWS
#  define HDA_DEBUG_DUMP_PCM_DATA_PATH "c:\\temp\\"
# else
#  define HDA_DEBUG_DUMP_PCM_DATA_PATH "/tmp/"
# endif
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

/* Make sure that interleaving streams support is enabled if the 5.1 surround code is being used. */
#if defined (VBOX_WITH_AUDIO_HDA_51_SURROUND) && !defined(VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT)
# define VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT
#endif

/** Default timer frequency (in Hz). */
#define HDA_TIMER_HZ            100

/**
 * At the moment we support 4 input + 4 output streams max, which is 8 in total.
 * Bidirectional streams are currently *not* supported.
 *
 * Note: When changing any of those values, be prepared for some saved state
 *       fixups / trouble!
 */
#define HDA_MAX_SDI             4
#define HDA_MAX_SDO             4
#define HDA_MAX_STREAMS         (HDA_MAX_SDI + HDA_MAX_SDO)
AssertCompile(HDA_MAX_SDI <= HDA_MAX_SDO);

/** Number of general registers. */
#define HDA_NUM_GENERAL_REGS    34
/** Number of total registers in the HDA's register map. */
#define HDA_NUM_REGS            (HDA_NUM_GENERAL_REGS + (HDA_MAX_STREAMS * 10 /* Each stream descriptor has 10 registers */))
/** Total number of stream tags (channels). Index 0 is reserved / invalid. */
#define HDA_MAX_TAGS            16

/**
 * NB: Register values stored in memory (au32Regs[]) are indexed through
 * the HDA_RMX_xxx macros (also HDA_MEM_IND_NAME()). On the other hand, the
 * register descriptors in g_aHdaRegMap[] are indexed through the
 * HDA_REG_xxx macros (also HDA_REG_IND_NAME()).
 *
 * The au32Regs[] layout is kept unchanged for saved state
 * compatibility.
 */

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
 * bds (2:1)   - number of serial data out (SDO) signals supported
 * b64sup (0)  - 64 bit addressing supported.
 */
#define HDA_MAKE_GCAP(oss, iss, bss, bds, b64sup) \
    (  (((oss)   & 0xF)  << 12) \
     | (((iss)   & 0xF)  << 8)  \
     | (((bss)   & 0x1F) << 3)  \
     | (((bds)   & 0x3)  << 1)  \
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
#define HDA_STATESTS_SCSF_MASK      0x7 /* State Change Status Flags (6.2.8). */

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
/* Bits 0-29 correspond to streams 0-29. */
#define HDA_INTCTL_GIE_MASK         RT_BIT(31) /* Global Interrupt Enable (3.3.14). */

#define HDA_REG_INTSTS              12 /* 0x24 */
#define HDA_RMX_INTSTS              10
#define HDA_INTSTS_GIS_SHIFT        31
#define HDA_INTSTS_CIS_SHIFT        30
/* Bits 0-29 correspond to streams 0-29. */

#define HDA_REG_WALCLK              13 /* 0x30 */
#define HDA_RMX_WALCLK              /* Not defined! */

/* Note: The HDA specification defines a SSYNC register at offset 0x38. The
 * ICH6/ICH9 datahseet defines SSYNC at offset 0x34. The Linux HDA driver matches
 * the datasheet.
 */
#define HDA_REG_SSYNC               14 /* 0x38 */
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

/** @todo Condense marcos! */

#define HDA_REG_SD0CTL              HDA_NUM_GENERAL_REGS /* 0x80 */
#define HDA_REG_SD1CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 10)  /* 0xA0 */
#define HDA_REG_SD2CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 20)  /* 0xC0 */
#define HDA_REG_SD3CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 30)  /* 0xE0 */
#define HDA_REG_SD4CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 40)  /* 0x100 */
#define HDA_REG_SD5CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 50)  /* 0x120 */
#define HDA_REG_SD6CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 60)  /* 0x140 */
#define HDA_REG_SD7CTL              (HDA_STREAM_REG_DEF(CTL, 0) + 70)  /* 0x160 */
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
#define HDA_SDSTS_FIFORDY           RT_BIT(5)   /* FIFO Ready */
#define HDA_SDSTS_DESE              RT_BIT(4)   /* Descriptor Error */
#define HDA_SDSTS_FIFOE             RT_BIT(3)   /* FIFO Error */
#define HDA_SDSTS_BCIS              RT_BIT(2)   /* Buffer Completion Interrupt Status */

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
/** Maximum FIFO size (in bytes). */
#define HDA_FIFO_MAX                256

#define HDA_SDIFIFO_120B            0x77 /* 8-, 16-, 20-, 24-, 32-bit Input Streams */
#define HDA_SDIFIFO_160B            0x9F /* 20-, 24-bit Input Streams Streams */

#define HDA_SDOFIFO_16B             0x0F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDOFIFO_32B             0x1F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDOFIFO_64B             0x3F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDOFIFO_128B            0x7F /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDOFIFO_192B            0xBF /* 8-, 16-, 20-, 24-, 32-bit Output Streams */
#define HDA_SDOFIFO_256B            0xFF /* 20-, 24-bit Output Streams */
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

#define SDFMT(pThis, num)               (HDA_REG((pThis), SD(FMT, num)))
#define HDA_SDFMT_BASE_RATE(pThis, num) ((SDFMT(pThis, num)   & HDA_REG_FIELD_FLAG_MASK(SDFMT, BASE_RATE)) >> HDA_REG_FIELD_SHIFT(SDFMT, BASE_RATE))
#define HDA_SDFMT_MULT(pThis, num)      ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,MULT)) >> HDA_REG_FIELD_SHIFT(SDFMT, MULT))
#define HDA_SDFMT_DIV(pThis, num)       ((SDFMT((pThis), num) & HDA_REG_FIELD_MASK(SDFMT,DIV)) >> HDA_REG_FIELD_SHIFT(SDFMT, DIV))

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

#define HDA_BDLE_FLAG_IOC           RT_BIT(0) /* Interrupt on completion (IOC). */

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
    /** Current offset in DMA buffer (in bytes).*/
    uint32_t     u32BufOff;
    uint32_t     Padding;
} HDABDLESTATE, *PHDABDLESTATE;

/**
 * BDL description structure.
 * Do not touch this, as this must match to the HDA specs.
 */
typedef struct HDABDLEDESC
{
    /** Starting address of the actual buffer. Must be 128-bit aligned. */
    uint64_t     u64BufAdr;
    /** Size of the actual buffer (in bytes). */
    uint32_t     u32BufSize;
    /** Bit 0: Interrupt on completion; the controller will generate
     *  an interrupt when the last byte of the buffer has been
     *  fetched by the DMA engine.
     *
     *  Rest is reserved for further use and must be 0. */
    uint32_t     fFlags;
} HDABDLEDESC, *PHDABDLEDESC;
AssertCompileSize(HDABDLEDESC, 16); /* Always 16 byte. Also must be aligned on 128-byte boundary. */

/**
 * Buffer Descriptor List Entry (BDLE) (3.6.3).
 */
typedef struct HDABDLE
{
    /** The actual BDL description. */
    HDABDLEDESC  Desc;
    /** Internal state of this BDLE.
     *  Not part of the actual BDLE registers. */
    HDABDLESTATE State;
} HDABDLE, *PHDABDLE;

/**
 * Structure for keeping an audio stream data mapping.
 */
typedef struct HDASTREAMMAPPING
{
    /** The stream's layout. */
    PDMAUDIOSTREAMLAYOUT              enmLayout;
    /** Number of audio channels in this stream. */
    uint8_t                           cChannels;
    /** Array of audio channels. */
    R3PTRTYPE(PPDMAUDIOSTREAMCHANNEL) paChannels;
    /** Circular buffer holding for holding audio data for this mapping. */
    R3PTRTYPE(PRTCIRCBUF)             pCircBuf;
} HDASTREAMMAPPING, *PHDASTREAMMAPPING;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Structure keeping the HDA stream's state for asynchronous I/O.
 */
typedef struct HDASTREAMSTATEAIO
{
    /** Thread handle for the actual I/O thread. */
    RTTHREAD              Thread;
    /** Event for letting the thread know there is some data to process. */
    RTSEMEVENT            Event;
    /** Critical section for synchronizing access. */
    RTCRITSECT            CritSect;
    /** Started indicator. */
    volatile bool         fStarted;
    /** Shutdown indicator. */
    volatile bool         fShutdown;
    /** Whether the thread should do any data processing or not. */
    volatile bool         fEnabled;
    uint32_t              Padding1;
} HDASTREAMSTATEAIO, *PHDASTREAMSTATEAIO;
#endif

/**
 * Internal state of a HDA stream.
 */
typedef struct HDASTREAMSTATE
{
    /** Current BDLE to use. Wraps around to 0 if
     *  maximum (cBDLE) is reached. */
    uint16_t                uCurBDLE;
    /** Flag indicating whether this stream currently is
     *  in reset mode and therefore not acccessible by the guest. */
    volatile bool           fInReset;
    /** Unused, padding. */
    uint32_t                Padding0;
    /** Critical section to serialize access. */
    RTCRITSECT              CritSect;
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    /** Asynchronous I/O state members. */
    HDASTREAMSTATEAIO       AIO;
#endif
    /** This stream's data mapping. */
    HDASTREAMMAPPING        Mapping;
    /** Current BDLE (Buffer Descriptor List Entry). */
    HDABDLE                 BDLE;
    /** Circular buffer (FIFO) for holding DMA'ed data. */
    R3PTRTYPE(PRTCIRCBUF)   pCircBuf;
} HDASTREAMSTATE, *PHDASTREAMSTATE;

/**
 * Structure defining an HDA mixer sink.
 * Its purpose is to know which audio mixer sink is bound to
 * which SDn (SDI/SDO) device stream.
 *
 * This is needed in order to handle interleaved streams
 * (that is, multiple channels in one stream) or non-interleaved
 * streams (each channel has a dedicated stream).
 *
 * This is only known to the actual device emulation level.
 */
typedef struct HDAMIXERSINK
{
    /** SDn ID this sink is assigned to. 0 if not assigned. */
    uint8_t                uSD;
    /** Channel ID of SDn ID. Only valid if SDn ID is valid. */
    uint8_t                uChannel;
    uint8_t                Padding[3];
    /** Pointer to the actual audio mixer sink. */
    R3PTRTYPE(PAUDMIXSINK) pMixSink;
} HDAMIXERSINK, *PHDAMIXERSINK;

/**
 * Structure for keeping a HDA stream (SDI / SDO).
 *
 * Note: This HDA stream has nothing to do with a regular audio stream handled
 * by the audio connector or the audio mixer. This HDA stream is a serial data in/out
 * stream (SDI/SDO) defined in hardware and can contain multiple audio streams
 * in one single SDI/SDO (interleaving streams).
 *
 * How a specific SDI/SDO is mapped to our internal audio streams relies on the
 * stream channel mappings.
 *
 * Contains only register values which do *not* change until a
 * stream reset occurs.
 */
typedef struct HDASTREAM
{
    /** Stream descriptor number (SDn). */
    uint8_t                  u8SD;
    uint8_t                  Padding0[7];
    /** DMA base address (SDnBDPU - SDnBDPL). */
    uint64_t                 u64BDLBase;
    /** Cyclic Buffer Length (SDnCBL).
     *  Represents the size of the ring buffer. */
    uint32_t                 u32CBL;
    /** Format (SDnFMT). */
    uint16_t                 u16FMT;
    /** FIFO Size (FIFOS).
     *  Maximum number of bytes that may have been DMA'd into
     *  memory but not yet transmitted on the link. */
    uint16_t                 u16FIFOS;
    /** FIFO Watermark. */
    uint16_t                 u16FIFOW;
    /** Last Valid Index (SDnLVI). */
    uint16_t                 u16LVI;
    uint16_t                 Padding1[2];
    /** Pointer to HDA sink this stream is attached to. */
    R3PTRTYPE(PHDAMIXERSINK) pMixSink;
    /** Internal state of this stream. */
    HDASTREAMSTATE           State;
} HDASTREAM, *PHDASTREAM;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Structure for keeping a HDA stream thread context.
 */
typedef struct HDASTREAMTHREADCTX
{
    PHDASTATE  pThis;
    PHDASTREAM pStream;
} HDASTREAMTHREADCTX, *PHDASTREAMTHREADCTX;
#endif

/**
 * Structure for mapping a stream tag to an HDA stream.
 */
typedef struct HDATAG
{
    /** Own stream tag. */
    uint8_t               uTag;
    uint8_t               Padding[7];
    /** Pointer to associated stream. */
    R3PTRTYPE(PHDASTREAM) pStrm;
} HDATAG, *PHDATAG;

/**
 * Structure defining a (host backend) driver stream.
 * Each driver has its own instances of audio mixer streams, which then
 * can go into the same (or even different) audio mixer sinks.
 */
typedef struct HDADRIVERSTREAM
{
    union
    {
        /** Desired playback destination (for an output stream). */
        PDMAUDIOPLAYBACKDEST           Dest;
        /** Desired recording source (for an input stream). */
        PDMAUDIORECSOURCE              Source;
    } DestSource;
    uint8_t                            Padding1[4];
    /** Associated mixer handle. */
    R3PTRTYPE(PAUDMIXSTREAM)           pMixStrm;
} HDADRIVERSTREAM, *PHDADRIVERSTREAM;

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
    /** Node for storing this driver in our device driver list of HDASTATE. */
    RTLISTNODER3                       Node;
    /** Pointer to HDA controller (state). */
    R3PTRTYPE(PHDASTATE)               pHDAState;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                   fFlags;
    uint8_t                            u32Padding0[2];
    /** LUN to which this driver has been assigned. */
    uint8_t                            uLUN;
    /** Whether this driver is in an attached state or not. */
    bool                               fAttached;
    /** Pointer to attached driver base interface. */
    R3PTRTYPE(PPDMIBASE)               pDrvBase;
    /** Audio connector interface to the underlying host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pConnector;
    /** Mixer stream for line input. */
    HDADRIVERSTREAM                     LineIn;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    /** Mixer stream for mic input. */
    HDADRIVERSTREAM                     MicIn;
#endif
    /** Mixer stream for front output. */
    HDADRIVERSTREAM                     Front;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** Mixer stream for center/LFE output. */
    HDADRIVERSTREAM                     CenterLFE;
    /** Mixer stream for rear output. */
    HDADRIVERSTREAM                     Rear;
#endif
} HDADRIVER;

/**
 * ICH Intel HD Audio Controller state.
 */
typedef struct HDASTATE
{
    /** The PCI device structure. */
    PDMPCIDEV                          PciDev;
    /** R3 Pointer to the device instance. */
    PPDMDEVINSR3                       pDevInsR3;
    /** R0 Pointer to the device instance. */
    PPDMDEVINSR0                       pDevInsR0;
    /** R0 Pointer to the device instance. */
    PPDMDEVINSRC                       pDevInsRC;
    /** Padding for alignment. */
    uint32_t                           u32Padding;
    /** The base interface for LUN\#0. */
    PDMIBASE                           IBase;
    RTGCPHYS                           MMIOBaseAddr;
    /** The HDA's register set. */
    uint32_t                           au32Regs[HDA_NUM_REGS];
    /** Internal stream states. */
    HDASTREAM                          aStreams[HDA_MAX_STREAMS];
    /** Mapping table between stream tags and stream states. */
    HDATAG                             aTags[HDA_MAX_TAGS];
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
    uint8_t                            u8Padding0[7];
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
    /** Indicates if HDA controller is in reset mode. */
    bool                               fInReset;
    /** Flag whether the R0 part is enabled. */
    bool                               fR0Enabled;
    /** Flag whether the RC part is enabled. */
    bool                               fRCEnabled;
    /** Number of active (running) SDn streams. */
    uint8_t                            cStreamsActive;
#ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    /** The timer for pumping data thru the attached LUN drivers. */
    PTMTIMERR3                         pTimer;
    /** Flag indicating whether the timer is active or not. */
    bool                               fTimerActive;
    uint8_t                            u8Padding1[7];
    /** Timer ticks per Hz. */
    uint64_t                           cTimerTicks;
    /** Timestamp of the last timer callback (hdaTimer).
     * Used to calculate the time actually elapsed between two timer callbacks. */
    uint64_t                           uTimerTS;
    uint64_t                           uTimerMS;
#endif
#ifdef VBOX_WITH_STATISTICS
# ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    STAMPROFILE                        StatTimer;
# endif
    STAMPROFILE                        StatIn;
    STAMPROFILE                        StatOut;
    STAMCOUNTER                        StatBytesRead;
    STAMCOUNTER                        StatBytesWritten;
#endif
    /** Pointer to HDA codec to use. */
    R3PTRTYPE(PHDACODEC)               pCodec;
    /** List of associated LUN drivers (HDADRIVER). */
    RTLISTANCHORR3                     lstDrv;
    /** The device' software mixer. */
    R3PTRTYPE(PAUDIOMIXER)             pMixer;
    /** HDA sink for (front) output. */
    HDAMIXERSINK                       SinkFront;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** HDA sink for center / LFE output. */
    HDAMIXERSINK                       SinkCenterLFE;
    /** HDA sink for rear output. */
    HDAMIXERSINK                       SinkRear;
#endif
    /** HDA mixer sink for line input. */
    HDAMIXERSINK                       SinkLineIn;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    /** Audio mixer sink for microphone input. */
    HDAMIXERSINK                       SinkMicIn;
#endif
    uint64_t                           u64BaseTS;
    /** Response Interrupt Count (RINTCNT). */
    uint8_t                            u8RespIntCnt;
    /** Padding for alignment. */
    uint8_t                            au8Padding2[7];
} HDASTATE;
/** Pointer to the ICH Intel HD Audio Controller state. */
typedef HDASTATE *PHDASTATE;

#ifdef VBOX_WITH_AUDIO_HDA_CALLBACKS
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
#ifdef IN_RING3
static FNPDMDEVRESET hdaReset;
#endif

/** @name Register read/write stubs.
 * @{
 */
static int hdaRegReadUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t pu32Value);
/** @} */

/** @name Global register set read/write functions.
 * @{
 */
static int hdaRegWriteGCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadLPIB(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteCORBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteRIRBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteSTATESTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteINTCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadIRS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteBase(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
/** @} */

/** @name {IOB}SDn write functions.
 * @{
 */
static int       hdaRegWriteSDCBL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDLVI(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDFIFOW(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDBDPL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int       hdaRegWriteSDBDPU(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
/** @} */

/** @name Generic register read/write functions.
 * @{
 */
static int hdaRegReadU32(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU32(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadU24(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
#ifdef IN_RING3
static int hdaRegWriteU24(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
#endif
static int hdaRegReadU16(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU16(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
static int hdaRegReadU8(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value);
static int hdaRegWriteU8(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value);
/** @} */

/** @name Stream functions.
 * @{
 */
#ifdef IN_RING3
static void          hdaStreamDestroy(PHDASTATE pThis, PHDASTREAM pStream);
static int           hdaStreamDoDMA(PHDASTATE pThis, PHDASTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t cbToProcess, uint32_t *pcbProcessed);
static int           hdaStreamEnable(PHDASTATE pThis, PHDASTREAM pStream, bool fEnable);
static int           hdaStreamUpdate(PHDASTATE pThis, PHDASTREAM pStream);
DECLINLINE(uint32_t) hdaStreamUpdateLPIB(PHDASTATE pThis, PHDASTREAM pStream, uint32_t u32LPIB);
static void          hdaStreamLock(PHDASTREAM pStream);
static void          hdaStreamUnlock(PHDASTREAM pStream);
#endif /* IN_RING3 */
/** @} */

/** @name Async I/O stream functions.
 * @{
 */
#ifdef IN_RING3
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
static DECLCALLBACK(int) hdaStreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser);
static int               hdaStreamAsyncIOCreate(PHDASTATE pThis, PHDASTREAM pStream);
static int               hdaStreamAsyncIODestroy(PHDASTATE pThis, PHDASTREAM pStream);
static int               hdaStreamAsyncIONotify(PHDASTATE pThis, PHDASTREAM pStream);
static void              hdaStreamAsyncIOLock(PHDASTREAM pStream);
static void              hdaStreamAsyncIOUnlock(PHDASTREAM pStream);
static void              hdaStreamAsyncIOEnable(PHDASTREAM pStream, bool fEnable);
# endif
#endif
/** @} */

/** @name Stream mapping functions.
 * @{
 */
#ifdef IN_RING3
static int           hdaStreamMapInit(PHDASTREAMMAPPING pMapping, PPDMAUDIOSTREAMCFG pCfg);
static void          hdaStreamMapDestroy(PHDASTREAMMAPPING pMapping);
static void          hdaStreamMapReset(PHDASTREAMMAPPING pMapping);
#endif /* IN_RING3 */
/** @} */

/** @name HDA device functions.
 * @{
 */
#ifdef IN_RING3
static void          hdaDoTransfers(PHDASTATE pThis);
#endif /* IN_RING3 */
static int           hdaProcessInterrupt(PHDASTATE pThis);
/** @} */

/** @name BDLE (Buffer Descriptor List Entry) functions.
 * @{
 */
#ifdef IN_RING3
static int           hdaBDLEFetch(PHDASTATE pThis, PHDABDLE pBDLE, uint64_t u64BaseDMA, uint16_t u16Entry);
# ifdef LOG_ENABLED
static void          hdaBDLEDumpAll(PHDASTATE pThis, uint64_t u64BaseDMA, uint16_t cBDLE);
# endif
#endif /* IN_RING3 */
/** @} */

/** @name Timer functions.
 * @{
 */
#if !defined(VBOX_WITH_AUDIO_HDA_CALLBACKS) && defined(IN_RING3)
static void          hdaTimerMaybeStart(PHDASTATE pThis);
static void          hdaTimerMaybeStop(PHDASTATE pThis);
static void          hdaTimerMain(PHDASTATE pThis);
#endif
/** @} */


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

/** No register description (RD) flags defined. */
#define HDA_RD_FLAG_NONE           UINT32_C(0)
/** Writes to SD are allowed while RUN bit is set. */
#define HDA_RD_FLAG_SD_WRITE_RUN   RT_BIT(0)

/** Emits a single audio stream register set (e.g. OSD0) at a specified offset. */
#define HDA_REG_MAP_STRM(offset, name) \
    /* offset        size     read mask   write mask  flags                     read callback   write callback     index + abbrev                 description */ \
    /* -------       -------  ----------  ----------  ------------------------- --------------  -----------------  -----------------------------  ----------- */ \
    /* Offset 0x80 (SD0) */ \
    { offset,        0x00003, 0x00FF001F, 0x00F0001F, HDA_RD_FLAG_SD_WRITE_RUN, hdaRegReadU24 , hdaRegWriteSDCTL  , HDA_REG_IDX_STRM(name, CTL)  , #name " Stream Descriptor Control" }, \
    /* Offset 0x83 (SD0) */ \
    { offset + 0x3,  0x00001, 0x0000003C, 0x0000001C, HDA_RD_FLAG_SD_WRITE_RUN, hdaRegReadU8  , hdaRegWriteSDSTS  , HDA_REG_IDX_STRM(name, STS)  , #name " Status" }, \
    /* Offset 0x84 (SD0) */ \
    { offset + 0x4,  0x00004, 0xFFFFFFFF, 0x00000000, HDA_RD_FLAG_NONE,         hdaRegReadLPIB, hdaRegWriteU32    , HDA_REG_IDX_STRM(name, LPIB) , #name " Link Position In Buffer" }, \
    /* Offset 0x88 (SD0) */ \
    { offset + 0x8,  0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE,         hdaRegReadU32 , hdaRegWriteSDCBL  , HDA_REG_IDX_STRM(name, CBL)  , #name " Cyclic Buffer Length" }, \
    /* Offset 0x8C (SD0) */ \
    { offset + 0xC,  0x00002, 0x0000FFFF, 0x0000FFFF, HDA_RD_FLAG_NONE,         hdaRegReadU16 , hdaRegWriteSDLVI  , HDA_REG_IDX_STRM(name, LVI)  , #name " Last Valid Index" }, \
    /* Reserved: FIFO Watermark. ** @todo Document this! */ \
    { offset + 0xE,  0x00002, 0x00000007, 0x00000007, HDA_RD_FLAG_NONE,         hdaRegReadU16 , hdaRegWriteSDFIFOW, HDA_REG_IDX_STRM(name, FIFOW), #name " FIFO Watermark" }, \
    /* Offset 0x90 (SD0) */ \
    { offset + 0x10, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_FLAG_NONE,         hdaRegReadU16 , hdaRegWriteSDFIFOS, HDA_REG_IDX_STRM(name, FIFOS), #name " FIFO Size" }, \
    /* Offset 0x92 (SD0) */ \
    { offset + 0x12, 0x00002, 0x00007F7F, 0x00007F7F, HDA_RD_FLAG_NONE,         hdaRegReadU16 , hdaRegWriteSDFMT  , HDA_REG_IDX_STRM(name, FMT)  , #name " Stream Format" }, \
    /* Reserved: 0x94 - 0x98. */ \
    /* Offset 0x98 (SD0) */ \
    { offset + 0x18, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_FLAG_NONE,         hdaRegReadU32 , hdaRegWriteSDBDPL , HDA_REG_IDX_STRM(name, BDPL) , #name " Buffer Descriptor List Pointer-Lower Base Address" }, \
    /* Offset 0x9C (SD0) */ \
    { offset + 0x1C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE,         hdaRegReadU32 , hdaRegWriteSDBDPU , HDA_REG_IDX_STRM(name, BDPU) , #name " Buffer Descriptor List Pointer-Upper Base Address" }

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
    /** Register descriptor (RD) flags of type HDA_RD_FLAG_.
     *  These are used to specify the handling (read/write)
     *  policy of the register. */
    uint32_t    fFlags;
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
} g_aHdaRegMap[HDA_NUM_REGS] =

{
    /* offset  size     read mask   write mask  flags             read callback     write callback       index + abbrev              */
    /*-------  -------  ----------  ----------  ----------------- ----------------  -------------------     ------------------------ */
    { 0x00000, 0x00002, 0x0000FFFB, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(GCAP)         }, /* Global Capabilities */
    { 0x00002, 0x00001, 0x000000FF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(VMIN)         }, /* Minor Version */
    { 0x00003, 0x00001, 0x000000FF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(VMAJ)         }, /* Major Version */
    { 0x00004, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(OUTPAY)       }, /* Output Payload Capabilities */
    { 0x00006, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(INPAY)        }, /* Input Payload Capabilities */
    { 0x00008, 0x00004, 0x00000103, 0x00000103, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteGCTL    , HDA_REG_IDX(GCTL)         }, /* Global Control */
    { 0x0000c, 0x00002, 0x00007FFF, 0x00007FFF, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(WAKEEN)       }, /* Wake Enable */
    { 0x0000e, 0x00002, 0x00000007, 0x00000007, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteSTATESTS, HDA_REG_IDX(STATESTS)     }, /* State Change Status */
    { 0x00010, 0x00002, 0xFFFFFFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadUnimpl, hdaRegWriteUnimpl  , HDA_REG_IDX(GSTS)         }, /* Global Status */
    { 0x00018, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(OUTSTRMPAY)   }, /* Output Stream Payload Capability */
    { 0x0001A, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(INSTRMPAY)    }, /* Input Stream Payload Capability */
    { 0x00020, 0x00004, 0xC00000FF, 0xC00000FF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteINTCTL  , HDA_REG_IDX(INTCTL)       }, /* Interrupt Control */
    { 0x00024, 0x00004, 0xC00000FF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteUnimpl  , HDA_REG_IDX(INTSTS)       }, /* Interrupt Status */
    { 0x00030, 0x00004, 0xFFFFFFFF, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadWALCLK, hdaRegWriteUnimpl  , HDA_REG_IDX_LOCAL(WALCLK) }, /* Wall Clock Counter */
    { 0x00034, 0x00004, 0x000000FF, 0x000000FF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteU32     , HDA_REG_IDX(SSYNC)        }, /* Stream Synchronization */
    { 0x00040, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(CORBLBASE)    }, /* CORB Lower Base Address */
    { 0x00044, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(CORBUBASE)    }, /* CORB Upper Base Address */
    { 0x00048, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteCORBWP  , HDA_REG_IDX(CORBWP)       }, /* CORB Write Pointer */
    { 0x0004A, 0x00002, 0x000080FF, 0x000080FF, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteCORBRP  , HDA_REG_IDX(CORBRP)       }, /* CORB Read Pointer */
    { 0x0004C, 0x00001, 0x00000003, 0x00000003, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteCORBCTL , HDA_REG_IDX(CORBCTL)      }, /* CORB Control */
    { 0x0004D, 0x00001, 0x00000001, 0x00000001, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteCORBSTS , HDA_REG_IDX(CORBSTS)      }, /* CORB Status */
    { 0x0004E, 0x00001, 0x000000F3, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(CORBSIZE)     }, /* CORB Size */
    { 0x00050, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(RIRBLBASE)    }, /* RIRB Lower Base Address */
    { 0x00054, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(RIRBUBASE)    }, /* RIRB Upper Base Address */
    { 0x00058, 0x00002, 0x000000FF, 0x00008000, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteRIRBWP  , HDA_REG_IDX(RIRBWP)       }, /* RIRB Write Pointer */
    { 0x0005A, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_FLAG_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(RINTCNT)      }, /* Response Interrupt Count */
    { 0x0005C, 0x00001, 0x00000007, 0x00000007, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteU8      , HDA_REG_IDX(RIRBCTL)      }, /* RIRB Control */
    { 0x0005D, 0x00001, 0x00000005, 0x00000005, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteRIRBSTS , HDA_REG_IDX(RIRBSTS)      }, /* RIRB Status */
    { 0x0005E, 0x00001, 0x000000F3, 0x00000000, HDA_RD_FLAG_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(RIRBSIZE)     }, /* RIRB Size */
    { 0x00060, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteU32     , HDA_REG_IDX(IC)           }, /* Immediate Command */
    { 0x00064, 0x00004, 0x00000000, 0xFFFFFFFF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteUnimpl  , HDA_REG_IDX(IR)           }, /* Immediate Response */
    { 0x00068, 0x00002, 0x00000002, 0x00000002, HDA_RD_FLAG_NONE, hdaRegReadIRS   , hdaRegWriteIRS     , HDA_REG_IDX(IRS)          }, /* Immediate Command Status */
    { 0x00070, 0x00004, 0xFFFFFFFF, 0xFFFFFF81, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(DPLBASE)      }, /* DMA Position Lower Base */
    { 0x00074, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_FLAG_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(DPUBASE)      }, /* DMA Position Upper Base */
    /* 4 Serial Data In (SDI). */
    HDA_REG_MAP_DEF_STREAM(0, SD0),
    HDA_REG_MAP_DEF_STREAM(1, SD1),
    HDA_REG_MAP_DEF_STREAM(2, SD2),
    HDA_REG_MAP_DEF_STREAM(3, SD3),
    /* 4 Serial Data Out (SDO). */
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
/** HDABDLEDESC field descriptors for the v6+ saved state. */
static SSMFIELD const g_aSSMBDLEDescFields6[] =
{
    SSMFIELD_ENTRY(HDABDLEDESC, u64BufAdr),
    SSMFIELD_ENTRY(HDABDLEDESC, u32BufSize),
    SSMFIELD_ENTRY(HDABDLEDESC, fFlags),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLESTATE field descriptors for the v6+ saved state. */
static SSMFIELD const g_aSSMBDLEStateFields6[] =
{
    SSMFIELD_ENTRY(HDABDLESTATE, u32BDLIndex),
    SSMFIELD_ENTRY(HDABDLESTATE, cbBelowFIFOW),
    SSMFIELD_ENTRY_OLD(FIFO, 256),
    SSMFIELD_ENTRY(HDABDLESTATE, u32BufOff),
    SSMFIELD_ENTRY_TERM()
};

/** HDASTREAMSTATE field descriptors for the v6+ saved state. */
static SSMFIELD const g_aSSMStreamStateFields6[] =
{
    SSMFIELD_ENTRY_OLD(cBDLE, 2),
    SSMFIELD_ENTRY(HDASTREAMSTATE, uCurBDLE),
    SSMFIELD_ENTRY_OLD(fDoStop, 1),
    SSMFIELD_ENTRY_OLD(fActive, 1),
    SSMFIELD_ENTRY(HDASTREAMSTATE, fInReset),
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

    Assert(RT_IS_POWER_OF_TWO(cb));
    return cb;
}


DECLINLINE(uint32_t) hdaStreamUpdateLPIB(PHDASTATE pThis, PHDASTREAM pStream, uint32_t u32LPIB)
{
    AssertPtrReturn(pThis,   0);
    AssertPtrReturn(pStream, 0);

    AssertMsg(u32LPIB <= pStream->u32CBL,
              ("[SD%RU8] New LPIB (%RU32) exceeds CBL (%RU32)\n", pStream->u8SD, u32LPIB, pStream->u32CBL));

    u32LPIB = RT_MIN(u32LPIB, pStream->u32CBL);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",
                 pStream->u8SD, u32LPIB, pThis->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pThis, LPIB, pStream->u8SD) = u32LPIB;

    /* Do we need to tell the current DMA position? */
    if (pThis->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns),
                                        (pThis->u64DPBase & DPBASE_ADDR_MASK) + (pStream->u8SD * 2 * sizeof(uint32_t)),
                                        (void *)&u32LPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }

    return u32LPIB;
}


/**
 * Locks an HDA stream for serialized access.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to lock.
 */
static void hdaStreamLock(PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);
    int rc2 = RTCritSectEnter(&pStream->State.CritSect);
    AssertRC(rc2);
}


/**
 * Unlocks a formerly locked HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStream             HDA stream to unlock.
 */
static void hdaStreamUnlock(PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);
    int rc2 = RTCritSectLeave(&pStream->State.CritSect);
    AssertRC(rc2);
}


/**
 * Fetches the next BDLE to use for a stream.
 *
 * @return  IPRT status code.
 */
DECLINLINE(int) hdaStreamGetNextBDLE(PHDASTATE pThis, PHDASTREAM pStream, bool *pfWrapAround)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    NOREF(pThis);

    Assert(pStream->State.uCurBDLE < pStream->u16LVI + 1);

    LogFlowFuncEnter();

# ifdef LOG_ENABLED
    uint32_t const uOldBDLE = pStream->State.uCurBDLE;
# endif

    PHDABDLE pBDLE       = &pStream->State.BDLE;
    bool     fWrapAround = false;

    AssertMsg(pBDLE->State.u32BufOff == pBDLE->Desc.u32BufSize, ("BDLE not finished yet: %R[bdle]\n", pBDLE));

    /*
     * Switch to the next BDLE entry and do a wrap around
     * if we reached the end of the Buffer Descriptor List (BDL).
     */
    pStream->State.uCurBDLE++;
    if (pStream->State.uCurBDLE == pStream->u16LVI + 1)
    {
        pStream->State.uCurBDLE = 0;
        fWrapAround = true;
    }

    Assert(pStream->State.uCurBDLE < pStream->u16LVI + 1);

    /* Fetch the next BDLE entry. */
    int rc = hdaBDLEFetch(pThis, pBDLE, pStream->u64BDLBase, pStream->State.uCurBDLE);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("[SD%RU8]: uOldBDLE=%RU16, uCurBDLE=%RU16, LVI=%RU32, rc=%Rrc, %R[bdle]\n",
                     pStream->u8SD, uOldBDLE, pStream->State.uCurBDLE, pStream->u16LVI, rc, pBDLE));

        if (pfWrapAround)
            *pfWrapAround = fWrapAround;
    }

    return rc;
}


/**
 * Returns the HDA stream of specified stream descriptor number.
 *
 * @return  Pointer to HDA stream, or NULL if none found.
 */
DECLINLINE(PHDASTREAM) hdaStreamGetFromSD(PHDASTATE pThis, uint8_t uSD)
{
    AssertPtrReturn(pThis, NULL);
    AssertReturn(uSD <= HDA_MAX_STREAMS, NULL);

    if (uSD >= HDA_MAX_STREAMS)
    {
        AssertMsgFailed(("Invalid / non-handled SD%RU8\n", uSD));
        return NULL;
    }

    return &pThis->aStreams[uSD];
}


/**
 * Returns the HDA stream of specified HDA sink.
 *
 * @return  Pointer to HDA stream, or NULL if none found.
 */
DECLINLINE(PHDASTREAM) hdaSinkGetStream(PHDASTATE pThis, PHDAMIXERSINK pSink)
{
    AssertPtrReturn(pThis, NULL);
    AssertPtrReturn(pSink, NULL);

    /** @todo Do something with the channel mapping here? */
    return hdaStreamGetFromSD(pThis, pSink->uSD);
}


/**
 * Returns the audio direction of a specified stream descriptor.
 *
 * The register layout specifies that input streams (SDI) come first,
 * followed by the output streams (SDO). So every stream ID below HDA_MAX_SDI
 * is an input stream, whereas everything >= HDA_MAX_SDI is an output stream.
 *
 * Note: SDnFMT register does not provide that information, so we have to judge
 *       for ourselves.
 *
 * @return  Audio direction.
 */
DECLINLINE(PDMAUDIODIR) hdaGetDirFromSD(uint8_t uSD)
{
    AssertReturn(uSD <= HDA_MAX_STREAMS, PDMAUDIODIR_UNKNOWN);

    if (uSD < HDA_MAX_SDI)
        return PDMAUDIODIR_IN;

    return PDMAUDIODIR_OUT;
}
#endif /* IN_RING3 */


static void hdaUpdateINTSTS(PHDASTATE pThis)
{
    uint32_t intSts = 0;

    if (/* Response Overrun Interrupt Status (ROIS) */
           HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RIRBOIS)
        /* Response Interrupt */
        || HDA_REG_FLAG_VALUE(pThis, RIRBSTS, RINTFL)
        /* SDIN State Change Status Flags (SCSF) */
        || (HDA_REG(pThis, STATESTS) & HDA_STATESTS_SCSF_MASK))
    {
        intSts |= RT_BIT(30); /* Touch Controller Interrupt Status (CIS). */
    }

#define HDA_MARK_STREAM(x)                                                  \
        if (   (HDA_REG(pThis, INTCTL) & RT_BIT(x))                         \
            && ((SDSTS(pThis, x) & (HDA_SDSTS_DESE | HDA_SDSTS_FIFOE | HDA_SDSTS_BCIS))) \
           )                                                                \
    {                                                                       \
        Log3Func(("[SD%RU8] Marked\n", x));                                 \
        intSts |= RT_BIT(x);                                                \
    }

    HDA_MARK_STREAM(0);
    HDA_MARK_STREAM(1);
    HDA_MARK_STREAM(2);
    HDA_MARK_STREAM(3);
    HDA_MARK_STREAM(4);
    HDA_MARK_STREAM(5);
    HDA_MARK_STREAM(6);
    HDA_MARK_STREAM(7);

#undef HDA_MARK_STREAM

    if (intSts)
        intSts |= RT_BIT(31); /* Touch Global Interrupt Status (GIS). */

    HDA_REG(pThis, INTSTS) = intSts;

    Log3Func(("INTSTS=%x\n", intSts));
}

static int hdaProcessInterrupt(PHDASTATE pThis)
{
    hdaUpdateINTSTS(pThis);

    int iLevel = 0;

    /* Global Interrupt Status (GIS) touched? */
    if (HDA_REG_FLAG_VALUE(pThis, INTSTS, GIS))
        iLevel = 1;

    Log3Func(("INTCTL=%x, INTSTS=%x, Level=%d\n", HDA_REG(pThis, INTCTL), HDA_REG(pThis, INTSTS), iLevel));

    /* Global Interrupt Enable (GIE) set? */
    if (HDA_REG_FLAG_VALUE(pThis, INTCTL, GIE))
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, iLevel);

    return VINF_SUCCESS;
}

/**
 * Looks up a register at the exact offset given by @a offReg.
 *
 * @returns Register index on success, -1 if not found.
 * @param   offReg              The register offset.
 */
static int hdaRegLookup(uint32_t offReg)
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
 * @param   offReg              The register offset.
 */
static int hdaRegLookupWithin(uint32_t offReg)
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
        Assert(pThis->u64CORBBase);
        AssertPtr(pThis->pu32CorbBuf);
        Assert(pThis->cbCorbBuf);

        rc = PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pThis->u64CORBBase, pThis->pu32CorbBuf, pThis->cbCorbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
# ifdef DEBUG_CMD_BUFFER
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
# endif
    }
    else
    {
        Assert((HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA)));
        rc = PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), pThis->u64RIRBBase, pThis->pu64RirbBuf, pThis->cbRirbBuf);
        if (RT_FAILURE(rc))
            AssertRCReturn(rc, rc);
# ifdef DEBUG_CMD_BUFFER
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
# endif
    }
    return rc;
}

static int hdaCORBCmdProcess(PHDASTATE pThis)
{
    int rc = hdaCmdSync(pThis, true);
    if (RT_FAILURE(rc))
        AssertRCReturn(rc, rc);

    uint8_t corbRp = HDA_REG(pThis, CORBRP);
    uint8_t corbWp = HDA_REG(pThis, CORBWP);
    uint8_t rirbWp = HDA_REG(pThis, RIRBWP);

    Assert((corbWp != corbRp));
    Log3Func(("CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP), HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));

    while (corbRp != corbWp)
    {
        uint64_t uResp;
        uint32_t uCmd = pThis->pu32CorbBuf[++corbRp];

        int rc2 = pThis->pCodec->pfnLookup(pThis->pCodec, HDA_CODEC_CMD(uCmd, 0 /* Codec index */), &uResp);
        if (RT_FAILURE(rc2))
            LogFunc(("Codec lookup failed with rc=%Rrc\n", rc2));

        (rirbWp)++;

        if (   (uResp & CODEC_RESPONSE_UNSOLICITED)
            && !HDA_REG_FLAG_VALUE(pThis, GCTL, UR))
        {
            LogFunc(("Unexpected unsolicited response\n"));
            HDA_REG(pThis, CORBRP) = corbRp;
            return rc;
        }

        pThis->pu64RirbBuf[rirbWp] = uResp;

        pThis->u8RespIntCnt++;
        if (pThis->u8RespIntCnt == RINTCNT_N(pThis))
            break;
    }

    HDA_REG(pThis, CORBRP) = corbRp;
    HDA_REG(pThis, RIRBWP) = rirbWp;

    rc = hdaCmdSync(pThis, false);

    Log3Func(("CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP), HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));

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

static int hdaStreamCreate(PHDASTATE pThis, PHDASTREAM pStream, uint8_t uSD)
{
    RT_NOREF(pThis);
    AssertPtrReturn(pStream,             VERR_INVALID_POINTER);
    AssertReturn(uSD <= HDA_MAX_STREAMS, VERR_INVALID_PARAMETER);

    int rc = RTCritSectInit(&pStream->State.CritSect);
    if (RT_SUCCESS(rc))
    {
        pStream->u8SD           = uSD;
        pStream->pMixSink       = NULL;

        pStream->State.fInReset = false;
    }

    if (RT_SUCCESS(rc))
        rc = RTCircBufCreate(&pStream->State.pCircBuf, _4K); /** @todo Make this configurable. */

    LogFlowFunc(("uSD=%RU8\n", uSD));
    return rc;
}

static void hdaStreamDestroy(PHDASTATE pThis, PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pStream);

    LogFlowFunc(("[SD%RU8]: Destroying ...\n", pStream->u8SD));

    hdaStreamMapDestroy(&pStream->State.Mapping);

    int rc2;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    rc2 = hdaStreamAsyncIODestroy(pThis, pStream);
    AssertRC(rc2);
#else
    RT_NOREF(pThis);
#endif

    rc2 = RTCritSectDelete(&pStream->State.CritSect);
    AssertRC(rc2);

    if (pStream->State.pCircBuf)
    {
        RTCircBufDestroy(pStream->State.pCircBuf);
        pStream->State.pCircBuf = NULL;
    }

    LogFlowFuncLeave();
}

static int hdaStreamInit(PHDASTATE pThis, PHDASTREAM pStream, uint8_t u8SD)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    pStream->u8SD       = u8SD;
    pStream->u64BDLBase = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, pStream->u8SD),
                                      HDA_STREAM_REG(pThis, BDPU, pStream->u8SD));
    pStream->u16LVI     = HDA_STREAM_REG(pThis, LVI, pStream->u8SD);
    pStream->u32CBL     = HDA_STREAM_REG(pThis, CBL, pStream->u8SD);
    pStream->u16FIFOS   = HDA_STREAM_REG(pThis, FIFOS, pStream->u8SD) + 1;

    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    hdaStreamMapReset(&pStream->State.Mapping);

    LogFlowFunc(("[SD%RU8]: DMA @ 0x%x (%RU32 bytes), LVI=%RU16, FIFOS=%RU16\n",
                 pStream->u8SD, pStream->u64BDLBase, pStream->u32CBL, pStream->u16LVI, pStream->u16FIFOS));

# ifdef DEBUG
    uint64_t u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, pStream->u8SD),
                                      HDA_STREAM_REG(pThis, BDPU, pStream->u8SD));
    uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, pStream->u8SD);
    uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, pStream->u8SD);

    LogFlowFunc(("\t-> DMA @ 0x%x, LVI=%RU16, CBL=%RU32\n", u64BaseDMA, u16LVI, u32CBL));

    hdaBDLEDumpAll(pThis, u64BaseDMA, u16LVI + 1);
# endif

    return VINF_SUCCESS;
}

/**
 * Resets an HDA stream.
 *
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to reset.
 */
static void hdaStreamReset(PHDASTATE pThis, PHDASTREAM pStream)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStream);

    const uint8_t uSD = pStream->u8SD;

# ifdef VBOX_STRICT
    AssertReleaseMsg(!RT_BOOL(HDA_STREAM_REG(pThis, CTL, uSD) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)),
                     ("[SD%RU8] Cannot reset stream while in running state\n", uSD));
# endif

    LogFunc(("[SD%RU8]: Reset\n", uSD));

    /*
     * First, reset the internal stream state.
     */
    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    if (pStream->State.pCircBuf)
        RTCircBufReset(pStream->State.pCircBuf);

    /*
     * Second, initialize the registers.
     */
    HDA_STREAM_REG(pThis, STS,   uSD) = HDA_SDSTS_FIFORDY;
    /* According to the ICH6 datasheet, 0x40000 is the default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRST bit. */
    HDA_STREAM_REG(pThis, CTL,   uSD) = 0x40000 | (HDA_STREAM_REG(pThis, CTL, uSD) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    /*
     * ICH6 defines default values (120 bytes for input and 192 bytes for output descriptors) of FIFO size. 18.2.39.
     * BUT: Windows guests seem to read the FIFOS but define a DMA region which does not fit to that FIFO size
     *      (e.g. 1792 bytes DMA region vs. 192 bytes FIFOS).
     *      This will lead to crackling and corrupted sound -- so define a 256 bytes FIOS for output streams here per default.
     */
    HDA_STREAM_REG(pThis, FIFOS, uSD) = hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN ? HDA_SDIFIFO_120B : HDA_SDOFIFO_256B;
    /* See 18.2.38: Always defaults to 0x4 (32 bytes). */
    HDA_STREAM_REG(pThis, FIFOW, uSD) = HDA_SDFIFOW_32B;
    HDA_STREAM_REG(pThis, LPIB,  uSD) = 0;
    HDA_STREAM_REG(pThis, CBL,   uSD) = 0;
    HDA_STREAM_REG(pThis, LVI,   uSD) = 0;
    HDA_STREAM_REG(pThis, FMT,   uSD) = HDA_SDFMT_MAKE(HDA_SDFMT_TYPE_PCM, HDA_SDFMT_BASE_44KHZ,
                                                       HDA_SDFMT_MULT_1X, HDA_SDFMT_DIV_1X, HDA_SDFMT_16_BIT,
                                                       HDA_SDFMT_CHAN_STEREO);
    HDA_STREAM_REG(pThis, BDPU,  uSD) = 0;
    HDA_STREAM_REG(pThis, BDPL,  uSD) = 0;

    int rc2 = hdaStreamInit(pThis, pStream, uSD);
    AssertRC(rc2);
}

/**
 * Enables or disables an HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to enable or disable.
 * @param   fEnable             Whether to enable or disble the stream.
 */
static int hdaStreamEnable(PHDASTATE pThis, PHDASTREAM pStream, bool fEnable)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFunc(("[SD%RU8]: fEnable=%RTbool, pMixSink=%p\n", pStream->u8SD, fEnable, pStream->pMixSink));

    int rc = VINF_SUCCESS;

    hdaStreamLock(pStream);

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    hdaStreamAsyncIOLock(pStream);
    hdaStreamAsyncIOEnable(pStream, fEnable);
#endif

    if (pStream->pMixSink) /* Stream attached to a sink? */
    {
        AUDMIXSINKCMD enmCmd = fEnable
                             ? AUDMIXSINKCMD_ENABLE : AUDMIXSINKCMD_DISABLE;

        /* First, enable or disable the stream and the stream's sink, if any. */
        if (pStream->pMixSink->pMixSink)
            rc = AudioMixerSinkCtl(pStream->pMixSink->pMixSink, enmCmd);
    }

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    hdaStreamAsyncIOUnlock(pStream);
#endif

    /* Make sure to leave the lock before (eventually) starting the timer. */
    hdaStreamUnlock(pStream);

#ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    /* Second, see if we need to start or stop the timer. */
    if (!fEnable)
        hdaTimerMaybeStop(pThis);
    else
        hdaTimerMaybeStart(pThis);
#endif

    LogFunc(("[SD%RU8]: cStreamsActive=%RU8, rc=%Rrc\n", pStream->u8SD, pThis->cStreamsActive, rc));
    return rc;
}

# if defined(VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT) || defined(VBOX_WITH_AUDIO_HDA_51_SURROUND)
static int hdaStreamChannelExtract(PPDMAUDIOSTREAMCHANNEL pChan, const void *pvBuf, size_t cbBuf)
{
    AssertPtrReturn(pChan, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,    VERR_INVALID_PARAMETER);

    AssertRelease(pChan->cbOff <= cbBuf);

    const uint8_t *pu8Buf = (const uint8_t *)pvBuf;

    size_t         cbSrc = cbBuf - pChan->cbOff;
    const uint8_t *pvSrc = &pu8Buf[pChan->cbOff];

    size_t         cbDst;
    uint8_t       *pvDst;
    RTCircBufAcquireWriteBlock(pChan->Data.pCircBuf, cbBuf, (void **)&pvDst, &cbDst);

    cbSrc = RT_MIN(cbSrc, cbDst);

    while (cbSrc)
    {
        AssertBreak(cbDst >= cbSrc);

        /* Enough data for at least one next frame? */
        if (cbSrc < pChan->cbFrame)
            break;

        memcpy(pvDst, pvSrc, pChan->cbFrame);

        /* Advance to next channel frame in stream. */
        pvSrc        += pChan->cbStep;
        Assert(cbSrc >= pChan->cbStep);
        cbSrc        -= pChan->cbStep;

        /* Advance destination by one frame. */
        pvDst        += pChan->cbFrame;
        Assert(cbDst >= pChan->cbFrame);
        cbDst        -= pChan->cbFrame;

        /* Adjust offset. */
        pChan->cbOff += pChan->cbFrame;
    }

    RTCircBufReleaseWriteBlock(pChan->Data.pCircBuf, cbDst);

    return VINF_SUCCESS;
}
# endif /* defined(VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT) || defined(VBOX_WITH_AUDIO_HDA_51_SURROUND) */

# if 0 /** @todo hdaStreamChannelAdvance is unused */
static int hdaStreamChannelAdvance(PPDMAUDIOSTREAMCHANNEL pChan, size_t cbAdv)
{
    AssertPtrReturn(pChan, VERR_INVALID_POINTER);

    if (!cbAdv)
        return VINF_SUCCESS;

    return VINF_SUCCESS;
}
# endif

static int hdaStreamChannelDataInit(PPDMAUDIOSTREAMCHANNELDATA pChanData, uint32_t fFlags)
{
    int rc = RTCircBufCreate(&pChanData->pCircBuf, 256); /** @todo Make this configurable? */
    if (RT_SUCCESS(rc))
    {
        pChanData->fFlags = fFlags;
    }

    return rc;
}

/**
 * Frees a stream channel data block again.
 *
 * @param   pChanData           Pointer to channel data to free.
 */
static void hdaStreamChannelDataDestroy(PPDMAUDIOSTREAMCHANNELDATA pChanData)
{
    if (!pChanData)
        return;

    if (pChanData->pCircBuf)
    {
        RTCircBufDestroy(pChanData->pCircBuf);
        pChanData->pCircBuf = NULL;
    }

    pChanData->fFlags = PDMAUDIOSTREAMCHANNELDATA_FLAG_NONE;
}

# if defined(VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT) || defined(VBOX_WITH_AUDIO_HDA_51_SURROUND)

static int hdaStreamChannelAcquireData(PPDMAUDIOSTREAMCHANNELDATA pChanData, void *pvData, size_t *pcbData)
{
    AssertPtrReturn(pChanData, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,    VERR_INVALID_POINTER);
    AssertPtrReturn(pcbData,   VERR_INVALID_POINTER);

    RTCircBufAcquireReadBlock(pChanData->pCircBuf, 256 /** @todo Make this configurarble? */, &pvData, &pChanData->cbAcq);

    *pcbData = pChanData->cbAcq;
    return VINF_SUCCESS;
}

static int hdaStreamChannelReleaseData(PPDMAUDIOSTREAMCHANNELDATA pChanData)
{
    AssertPtrReturn(pChanData, VERR_INVALID_POINTER);
    RTCircBufReleaseReadBlock(pChanData->pCircBuf, pChanData->cbAcq);

    return VINF_SUCCESS;
}

# endif /* defined(VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT) || defined(VBOX_WITH_AUDIO_HDA_51_SURROUND) */

# if 0 /* currently unused */
static int hdaStreamWaitForStateChange(PHDASTREAM pStream, RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFlowFunc(("[SD%RU8]: msTimeout=%RU32\n", pStream->u8SD, msTimeout));
    return RTSemEventWait(pStream->State.hStateChangedEvent, msTimeout);
}
# endif /* currently unused */

#endif /* IN_RING3 */

/* Register access handlers. */

static int hdaRegReadUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg);
    *pu32Value = 0;
    return VINF_SUCCESS;
}

static int hdaRegWriteUnimpl(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
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

#ifdef IN_RING3
static int hdaRegWriteU24(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xff000000) == 0);
    return hdaRegWriteU32(pThis, iReg, u32Value);
}
#endif

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
    RT_NOREF_PV(iReg);

    if (u32Value & HDA_REG_FIELD_FLAG_MASK(GCTL, RST))
    {
        /* Set the CRST bit to indicate that we're leaving reset mode. */
        HDA_REG(pThis, GCTL) |= HDA_REG_FIELD_FLAG_MASK(GCTL, RST);

        if (pThis->fInReset)
        {
            LogFunc(("Guest leaving HDA reset\n"));
            pThis->fInReset = false;
        }
    }
    else
    {
#ifdef IN_RING3
        /* Enter reset state. */
        LogFunc(("Guest entering HDA reset with DMA(RIRB:%s, CORB:%s)\n",
                 HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA) ? "on" : "off",
                 HDA_REG_FLAG_VALUE(pThis, RIRBCTL, DMA) ? "on" : "off"));

        /* Clear the CRST bit to indicate that we're in reset state. */
        HDA_REG(pThis, GCTL) &= ~HDA_REG_FIELD_FLAG_MASK(GCTL, RST);
        pThis->fInReset = true;

        hdaReset(pThis->CTX_SUFF(pDevIns));
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
    uint32_t v  = HDA_REG_IND(pThis, iReg);
    uint32_t nv = u32Value & HDA_STATESTS_SCSF_MASK;

    HDA_REG(pThis, STATESTS) &= ~(v & nv); /* Write of 1 clears corresponding bit. */

    return hdaProcessInterrupt(pThis);
}

static int hdaRegWriteINTCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(iReg);

    int rc;

    HDA_REG(pThis, INTCTL) = u32Value;

    /* Global Interrupt Enable (GIE) set? */
    if (u32Value & HDA_INTCTL_GIE_MASK)
    {
        rc = hdaProcessInterrupt(pThis);
    }
    else
    {
        /* Make sure to lower interrupt line, as Global Interrupt Enable (GIE) is disabled. */
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, 0 /* iLevel */);

        rc = VINF_SUCCESS;
    }

    return rc;
}

static int hdaRegReadLPIB(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    const uint8_t  u8Strm  = HDA_SD_NUM_FROM_REG(pThis, LPIB, iReg);
    uint32_t       u32LPIB = HDA_STREAM_REG(pThis, LPIB, u8Strm);
#ifdef LOG_ENABLED
    const uint32_t u32CBL  = HDA_STREAM_REG(pThis, CBL,  u8Strm);
    LogFlowFunc(("[SD%RU8]: LPIB=%RU32, CBL=%RU32\n", u8Strm, u32LPIB, u32CBL));
#endif

    *pu32Value = u32LPIB;
    return VINF_SUCCESS;
}

static int hdaRegReadWALCLK(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF_PV(iReg);

    /* HDA spec (1a): 3.3.16 WALCLK counter ticks with 24Mhz bitclock rate. */
    *pu32Value = (uint32_t)ASMMultU64ByU32DivByU32(PDMDevHlpTMTimeVirtGetNano(pThis->CTX_SUFF(pDevIns))
                                                   - pThis->u64BaseTS, 24, 1000);
    LogFlowFunc(("%RU32\n", *pu32Value));
    return VINF_SUCCESS;
}

static int hdaRegWriteCORBRP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF_PV(iReg);

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
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static int hdaRegWriteCORBSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF_PV(iReg);

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
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

static int hdaRegWriteSDCBL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    if (HDA_REG_IND(pThis, iReg) == u32Value) /* Value already set? */
        return VINF_SUCCESS;

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, HDA_SD_NUM_FROM_REG(pThis, CBL, iReg));
    if (!pStream)
    {
        LogFunc(("[SD%RU8]: Warning: Changing SDCBL on non-attached stream (0x%x)\n",
                 HDA_SD_NUM_FROM_REG(pThis, CTL, iReg), u32Value));
        return hdaRegWriteU32(pThis, iReg, u32Value);
    }

    pStream->u32CBL = u32Value;

    /* Reset BDLE state. */
    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    int rc2 = hdaRegWriteU32(pThis, iReg, u32Value);
    AssertRC(rc2);

    LogFlowFunc(("[SD%RU8]: CBL=%RU32\n", pStream->u8SD, u32Value));

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

static int hdaRegWriteSDCTL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    /*
     * Some guests write too much (that is, 32-bit with the top 8 bit being junk)
     * instead of 24-bit required for SDCTL. So just mask this here to be safe.
     */
    u32Value = (u32Value & 0x00ffffff);

    bool fRun      = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
    bool fInRun    = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));

    bool fReset    = RT_BOOL(u32Value & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));
    bool fInReset  = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST));

    /* Get the stream descriptor. */
    uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, CTL, iReg);

    LogFunc(("[SD%RU8]: fRun=%RTbool, fInRun=%RTbool, fReset=%RTbool, fInReset=%RTbool, %R[sdctl]\n",
             uSD, fRun, fInRun, fReset, fInReset, u32Value));

    if (HDA_REG_IND(pThis, iReg) == u32Value) /* Value already set? */
        return VINF_SUCCESS; /* Always return success to the MMIO handler. */

    /*
     * Extract the stream tag the guest wants to use for this specific
     * stream descriptor (SDn). This only can happen if the stream is in a non-running
     * state, so we're doing the lookup and assignment here.
     *
     * So depending on the guest OS, SD3 can use stream tag 4, for example.
     */
    uint8_t uTag = (u32Value >> HDA_SDCTL_NUM_SHIFT) & HDA_SDCTL_NUM_MASK;
    if (uTag > HDA_MAX_TAGS)
    {
        LogFunc(("[SD%RU8]: Warning: Invalid stream tag %RU8 specified!\n", uSD, uTag));
        return hdaRegWriteU24(pThis, iReg, u32Value);
    }

    PHDATAG pTag = &pThis->aTags[uTag];
    AssertPtr(pTag);

    LogFunc(("[SD%RU8]: Using stream tag=%RU8\n", uSD, uTag));

    /* Assign new values. */
    pTag->uTag  = uTag;
    pTag->pStrm = hdaStreamGetFromSD(pThis, uSD);

    PHDASTREAM pStream = pTag->pStrm;
    AssertPtr(pStream);

    if (fInReset)
    {
        Assert(!fReset);
        Assert(!fInRun && !fRun);

        /* Exit reset state. */
        ASMAtomicXchgBool(&pStream->State.fInReset, false);

        /* Report that we're done resetting this stream by clearing SRST. */
        HDA_STREAM_REG(pThis, CTL, uSD) &= ~HDA_REG_FIELD_FLAG_MASK(SDCTL, SRST);

        LogFunc(("[SD%RU8]: Reset exit\n", uSD));
    }
    else if (fReset)
    {
        /* ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset. */
        Assert(!fInRun && !fRun);

        LogFunc(("[SD%RU8]: Reset enter\n", pStream->u8SD));

        /* Enter reset state. */
        Assert(ASMAtomicReadBool(&pStream->State.fInReset) == false); /* No nested calls. */
        ASMAtomicXchgBool(&pStream->State.fInReset, true);

        hdaStreamLock(pStream);

        hdaStreamReset(pThis, pStream);

        hdaStreamUnlock(pStream);
    }
    else
    {
        /*
         * We enter here to change DMA states only.
         */
        if (fInRun != fRun)
        {
            Assert(!fReset && !fInReset);
            LogFunc(("[SD%RU8]: State changed (fRun=%RTbool)\n", pStream->u8SD, fRun));

            if (fRun)
            {
                /* Make sure to first fetch the current BDLE before enabling the stream below. */
                int rc2 = hdaBDLEFetch(pThis, &pStream->State.BDLE, pStream->u64BDLBase, pStream->State.uCurBDLE);
                AssertRC(rc2);
            }

            hdaStreamEnable(pThis, pStream, fRun /* fEnable */);
        }
    }

    int rc2 = hdaRegWriteU24(pThis, iReg, u32Value);
    AssertRC(rc2);

    /* Make sure to handle interrupts here as well. */
    hdaProcessInterrupt(pThis);

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

static int hdaRegWriteSDSTS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint32_t v = HDA_REG_IND(pThis, iReg);

    /* Clear (zero) FIFOE, DESE and BCIS bits when writing 1 to it (6.2.33). */
    HDA_REG_IND(pThis, iReg) &= ~(u32Value & v);

    LogFunc(("SDSTS 0x%x -> 0x%x\n", v, HDA_REG_IND(pThis, iReg)));

    hdaProcessInterrupt(pThis);
    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
}

static int hdaRegWriteSDLVI(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    if (HDA_REG_IND(pThis, iReg) == u32Value) /* Value already set? */
        return VINF_SUCCESS;

    uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, LVI, iReg);

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, uSD);
    if (!pStream)
    {
        AssertMsgFailed(("[SD%RU8]: Warning: Changing SDLVI on non-attached stream (0x%x)\n", uSD, u32Value));
        return hdaRegWriteU16(pThis, iReg, u32Value);
    }

    /** @todo Validate LVI. */
    pStream->u16LVI = u32Value;
    LogFunc(("[SD%RU8]: Updating LVI to %RU16\n", uSD, pStream->u16LVI));

    /* Reset BDLE state. */
    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    int rc2 = hdaRegWriteU16(pThis, iReg, u32Value);
    AssertRC(rc2);

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

static int hdaRegWriteSDFIFOW(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, FIFOW, iReg);

    if (hdaGetDirFromSD(uSD) != PDMAUDIODIR_IN) /* FIFOW for input streams only. */
    {
        LogRel(("HDA: Warning: Guest tried to write read-only FIFOW to output stream #%RU8, ignoring\n", uSD));
        return VINF_SUCCESS;
    }

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, HDA_SD_NUM_FROM_REG(pThis, FIFOW, iReg));
    if (!pStream)
    {
        AssertMsgFailed(("[SD%RU8]: Warning: Changing FIFOW on non-attached stream (0x%x)\n", uSD, u32Value));
        return hdaRegWriteU16(pThis, iReg, u32Value);
    }

    uint32_t u32FIFOW = 0;

    switch (u32Value)
    {
        case HDA_SDFIFOW_8B:
        case HDA_SDFIFOW_16B:
        case HDA_SDFIFOW_32B:
            u32FIFOW = u32Value;
            break;
        default:
            LogRel(("HDA: Warning: Guest tried write unsupported FIFOW (0x%x) to stream #%RU8, defaulting to 32 bytes\n",
                    u32Value, uSD));
            AssertFailed();
            u32FIFOW = HDA_SDFIFOW_32B;
            break;
    }

    if (u32FIFOW)
    {
        pStream->u16FIFOW = hdaSDFIFOWToBytes(u32FIFOW);
        LogFunc(("[SD%RU8]: Updating FIFOW to %RU32 bytes\n", uSD, pStream->u16FIFOW));

        int rc2 = hdaRegWriteU16(pThis, iReg, u32FIFOW);
        AssertRC(rc2);
    }

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

/**
 * @note This method could be called for changing value on Output Streams only (ICH6 datasheet 18.2.39).
 */
static int hdaRegWriteSDFIFOS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, FIFOS, iReg);

    if (hdaGetDirFromSD(uSD) != PDMAUDIODIR_OUT) /* FIFOS for output streams only. */
    {
        LogRel(("HDA: Warning: Guest tried to write read-only FIFOS to input stream #%RU8, ignoring\n", uSD));
        return VINF_SUCCESS;
    }

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, uSD);
    if (!pStream)
    {
        AssertMsgFailed(("[SD%RU8]: Warning: Changing FIFOS on non-attached stream (0x%x)\n", uSD, u32Value));
        return hdaRegWriteU16(pThis, iReg, u32Value);
    }

    uint32_t u32FIFOS = 0;

    switch(u32Value)
    {
        case HDA_SDOFIFO_16B:
        case HDA_SDOFIFO_32B:
        case HDA_SDOFIFO_64B:
        case HDA_SDOFIFO_128B:
        case HDA_SDOFIFO_192B:
        case HDA_SDOFIFO_256B:
            u32FIFOS = u32Value;
            break;

        default:
            LogRel(("HDA: Warning: Guest tried write unsupported FIFOS (0x%x) to stream #%RU8, defaulting to 192 bytes\n",
                    u32Value, uSD));
            AssertFailed();
            u32FIFOS = HDA_SDOFIFO_192B;
            break;
    }

    if (u32FIFOS)
    {
        pStream->u16FIFOS = u32FIFOS + 1;
        LogFunc(("[SD%RU8]: Updating FIFOS to %RU32 bytes\n", uSD, pStream->u16FIFOS));

        int rc2 = hdaRegWriteU16(pThis, iReg, u32FIFOS);
        AssertRC(rc2);
    }

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

#ifdef IN_RING3
static int hdaSDFMTToStrmCfg(uint32_t u32SDFMT, PPDMAUDIOSTREAMCFG pStrmCfg)
{
    AssertPtrReturn(pStrmCfg, VERR_INVALID_POINTER);

# define EXTRACT_VALUE(v, mask, shift) ((v & ((mask) << (shift))) >> (shift))

    int rc = VINF_SUCCESS;

    uint32_t u32Hz     = EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_BASE_RATE_MASK, HDA_SDFMT_BASE_RATE_SHIFT)
                       ? 44100 : 48000;
    uint32_t u32HzMult = 1;
    uint32_t u32HzDiv  = 1;

    switch (EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT))
    {
        case 0: u32HzMult = 1; break;
        case 1: u32HzMult = 2; break;
        case 2: u32HzMult = 3; break;
        case 3: u32HzMult = 4; break;
        default:
            LogFunc(("Unsupported multiplier %x\n",
                     EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }
    switch (EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT))
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
                     EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    uint8_t cBits = 0;
    switch (EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT))
    {
        case 0:
            cBits = 8;
            break;
        case 1:
            cBits = 16;
            break;
        case 4:
            cBits = 32;
            break;
        default:
            AssertMsgFailed(("Unsupported bits per sample %x\n",
                             EXTRACT_VALUE(u32SDFMT, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        RT_ZERO(pStrmCfg->Props);

        pStrmCfg->Props.uHz       = u32Hz * u32HzMult / u32HzDiv;
        pStrmCfg->Props.cChannels = (u32SDFMT & 0xf) + 1;
        pStrmCfg->Props.cBits     = cBits;
        pStrmCfg->Props.fSigned   = true;
        pStrmCfg->Props.cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pStrmCfg->Props.cBits, pStrmCfg->Props.cChannels);
    }

# undef EXTRACT_VALUE
    return rc;
}

static int hdaAddStreamOut(PHDASTATE pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    AssertReturn(pCfg->enmDir == PDMAUDIODIR_OUT, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Stream=%s\n", pCfg->szName));

    int rc = VINF_SUCCESS;

    bool fUseFront = true; /* Always use front out by default. */
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    bool fUseRear;
    bool fUseCenter;
    bool fUseLFE;

    fUseRear = fUseCenter = fUseLFE = false;

    /*
     * Use commonly used setups for speaker configurations.
     */

    /** @todo Make the following configurable through mixer API and/or CFGM? */
    switch (pCfg->cChannels)
    {
        case 3:  /* 2.1: Front (Stereo) + LFE. */
        {
            fUseLFE   = true;
            break;
        }

        case 4:  /* Quadrophonic: Front (Stereo) + Rear (Stereo). */
        {
            fUseRear  = true;
            break;
        }

        case 5:  /* 4.1: Front (Stereo) + Rear (Stereo) + LFE. */
        {
            fUseRear  = true;
            fUseLFE   = true;
            break;
        }

        case 6:  /* 5.1: Front (Stereo) + Rear (Stereo) + Center/LFE. */
        {
            fUseRear   = true;
            fUseCenter = true;
            fUseLFE    = true;
            break;
        }

        default: /* Unknown; fall back to 2 front channels (stereo). */
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }
#else /* !VBOX_WITH_AUDIO_HDA_51_SURROUND */
    /* Only support mono or stereo channels. */
    if (   pCfg->Props.cChannels != 1 /* Mono */
        && pCfg->Props.cChannels != 2 /* Stereo */)
    {
        rc = VERR_NOT_SUPPORTED;
    }
#endif

    if (rc == VERR_NOT_SUPPORTED)
    {
        LogRel(("HDA: Unsupported channel count (%RU8), falling back to stereo channels\n", pCfg->Props.cChannels));
        pCfg->Props.cChannels = 2;

        rc = VINF_SUCCESS;
    }

    do
    {
        if (RT_FAILURE(rc))
            break;

        if (fUseFront)
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Front");
            pCfg->DestSource.Dest = PDMAUDIOPLAYBACKDEST_FRONT;
            pCfg->Props.cChannels = 2;

            rc = hdaCodecRemoveStream(pThis->pCodec,  PDMAUDIOMIXERCTL_FRONT);
            if (RT_SUCCESS(rc))
                rc = hdaCodecAddStream(pThis->pCodec, PDMAUDIOMIXERCTL_FRONT, pCfg);
        }

#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        if (   RT_SUCCESS(rc)
            && (fUseCenter || fUseLFE))
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Center/LFE");
            pCfg->DestSource.Dest = PDMAUDIOPLAYBACKDEST_CENTER_LFE;
            pCfg->Props.cChannels = (fUseCenter && fUseLFE) ? 2 : 1;

            rc = hdaCodecRemoveStream(pThis->pCodec,  PDMAUDIOMIXERCTL_CENTER_LFE);
            if (RT_SUCCESS(rc))
                rc = hdaCodecAddStream(pThis->pCodec, PDMAUDIOMIXERCTL_CENTER_LFE, pCfg);
        }

        if (   RT_SUCCESS(rc)
            && fUseRear)
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Rear");
            pCfg->DestSource.Dest = PDMAUDIOPLAYBACKDEST_REAR;
            pCfg->Props.cChannels = 2;

            rc = hdaCodecRemoveStream(pThis->pCodec,  PDMAUDIOMIXERCTL_REAR);
            if (RT_SUCCESS(rc))
                rc = hdaCodecAddStream(pThis->pCodec, PDMAUDIOMIXERCTL_REAR, pCfg);
        }
#endif /* VBOX_WITH_AUDIO_HDA_51_SURROUND */

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int hdaAddStreamIn(PHDASTATE pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    AssertReturn(pCfg->enmDir == PDMAUDIODIR_IN, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Stream=%s, Source=%ld\n", pCfg->szName, pCfg->DestSource.Source));

    int rc;

    switch (pCfg->DestSource.Source)
    {
        case PDMAUDIORECSOURCE_LINE:
        {
            rc = hdaCodecRemoveStream(pThis->pCodec,  PDMAUDIOMIXERCTL_LINE_IN);
            if (RT_SUCCESS(rc))
                rc = hdaCodecAddStream(pThis->pCodec, PDMAUDIOMIXERCTL_LINE_IN, pCfg);
            break;
        }
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        case PDMAUDIORECSOURCE_MIC:
        {
            rc = hdaCodecRemoveStream(pThis->pCodec,  PDMAUDIOMIXERCTL_MIC_IN);
            if (RT_SUCCESS(rc))
                rc = hdaCodecAddStream(pThis->pCodec, PDMAUDIOMIXERCTL_MIC_IN, pCfg);
            break;
        }
#endif
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* IN_RING3 */

static int hdaRegWriteSDFMT(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    PDMAUDIOSTREAMCFG strmCfg;
    RT_ZERO(strmCfg);

    int rc = hdaSDFMTToStrmCfg(u32Value, &strmCfg);
    if (RT_FAILURE(rc))
        return VINF_SUCCESS; /* Always return success to the MMIO handler. */

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, HDA_SD_NUM_FROM_REG(pThis, FMT, iReg));
    if (!pStream)
    {
        LogFunc(("[SD%RU8]: Warning: Changing SDFMT on non-attached stream (0x%x)\n",
                 HDA_SD_NUM_FROM_REG(pThis, FMT, iReg), u32Value));
        return hdaRegWriteU16(pThis, iReg, u32Value);
    }

    LogFunc(("[SD%RU8]: Hz=%RU32, Channels=%RU8, cBits=%RU8\n",
             pStream->u8SD, strmCfg.Props.uHz, strmCfg.Props.cChannels, strmCfg.Props.cBits));

    /* Set audio direction. */
    strmCfg.enmDir = hdaGetDirFromSD(pStream->u8SD);
    switch (strmCfg.enmDir)
    {
        case PDMAUDIODIR_IN:
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
#  error "Implement me!"
# else
            strmCfg.DestSource.Source = PDMAUDIORECSOURCE_LINE;
            RTStrCopy(strmCfg.szName, sizeof(strmCfg.szName), "Line In");
# endif
            break;

        case PDMAUDIODIR_OUT:
            /* Destination(s) will be set in hdaAddStreamOut(),
             * based on the channels / stream layout. */
            break;

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    if (RT_SUCCESS(rc))
    {
        rc = hdaStreamAsyncIOCreate(pThis, pStream);
        AssertRC(rc);
    }
#endif

    /*
     * Initialize the stream mapping in any case, regardless if
     * we support surround audio or not. This is needed to handle
     * the supported channels within a single audio stream, e.g. mono/stereo.
     *
     * In other words, the stream mapping *always* knowns the real
     * number of channels in a single audio stream.
     */
    if (RT_SUCCESS(rc))
    {
        rc = hdaStreamMapInit(&pStream->State.Mapping, &strmCfg);
        AssertRC(rc);
    }

    if (RT_SUCCESS(rc))
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            int rc2;
            switch (strmCfg.enmDir)
            {
                case PDMAUDIODIR_OUT:
                    rc2 = hdaAddStreamOut(pThis, &strmCfg);
                    break;

                case PDMAUDIODIR_IN:
                    rc2 = hdaAddStreamIn(pThis, &strmCfg);
                    break;

                default:
                    rc2 = VERR_NOT_SUPPORTED;
                    AssertFailed();
                    break;
            }

            if (   RT_FAILURE(rc2)
                && (pDrv->fFlags & PDMAUDIODRVFLAGS_PRIMARY)) /* We only care about primary drivers here, the rest may fail. */
            {
                if (RT_SUCCESS(rc))
                    rc = rc2;
                /* Keep going. */
            }
        }

        /* If (re-)opening the stream by the codec above failed, don't write the new
         * format to the register so that the guest is aware it didn't work. */
        if (RT_SUCCESS(rc))
        {
            rc = hdaRegWriteU16(pThis, iReg, u32Value);
            AssertRC(rc);
        }
        else
            LogFunc(("[SD%RU8]: (Re-)Opening stream failed with rc=%Rrc\n", pStream->u8SD, rc));
    }

    return VINF_SUCCESS; /* Never return failure. */
#else /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

/* Note: Will be called for both, BDPL and BDPU, registers. */
DECLINLINE(int) hdaRegWriteSDBDPX(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value, uint8_t uSD)
{
#ifdef IN_RING3
    if (HDA_REG_IND(pThis, iReg) == u32Value) /* Value already set? */
        return VINF_SUCCESS;

    int rc2 = hdaRegWriteU32(pThis, iReg, u32Value);
    AssertRC(rc2);

    PHDASTREAM pStream = hdaStreamGetFromSD(pThis, uSD);
    if (!pStream)
        return VINF_SUCCESS;

    /* Update BDL base. */
    pStream->u64BDLBase = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, uSD),
                                      HDA_STREAM_REG(pThis, BDPU, uSD));
    AssertMsg(pStream->u64BDLBase, ("BDL base invalid\n"));

    /* Reset BDLE state. */
    RT_ZERO(pStream->State.BDLE);
    pStream->State.uCurBDLE = 0;

    LogFlowFunc(("[SD%RU8]: BDLBase=0x%x\n", pStream->u8SD, pStream->u64BDLBase));

    return VINF_SUCCESS; /* Always return success to the MMIO handler. */
#else  /* !IN_RING3 */
    RT_NOREF_PV(pThis); RT_NOREF_PV(iReg); RT_NOREF_PV(u32Value); RT_NOREF_PV(uSD);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* IN_RING3 */
}

static int hdaRegWriteSDBDPL(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return hdaRegWriteSDBDPX(pThis, iReg, u32Value, HDA_SD_NUM_FROM_REG(pThis, BDPL, iReg));
}

static int hdaRegWriteSDBDPU(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return hdaRegWriteSDBDPX(pThis, iReg, u32Value, HDA_SD_NUM_FROM_REG(pThis, BDPU, iReg));
}

static int hdaRegReadIRS(PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    /* regarding 3.4.3 we should mark IRS as busy in case CORB is active */
    if (   HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP)
        || HDA_REG_FLAG_VALUE(pThis, CORBCTL, DMA))
    {
        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */
    }

    return hdaRegReadU32(pThis, iReg, pu32Value);
}

static int hdaRegWriteIRS(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF_PV(iReg);

    /*
     * If the guest set the ICB bit of IRS register, HDA should process the verb in IC register,
     * write the response to IR register, and set the IRV (valid in case of success) bit of IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, ICB)
        && !HDA_REG_FLAG_VALUE(pThis, IRS, ICB))
    {
#ifdef IN_RING3
        uint32_t uCmd = HDA_REG(pThis, IC);

        if (HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP))
        {
            /*
             * 3.4.3: Defines behavior of immediate Command status register.
             */
            LogRel(("HDA: Guest attempted process immediate verb (%x) with active CORB\n", uCmd));
            return VINF_SUCCESS;
        }

        HDA_REG(pThis, IRS) = HDA_REG_FIELD_FLAG_MASK(IRS, ICB);  /* busy */

        uint64_t uResp;
        int rc2 = pThis->pCodec->pfnLookup(pThis->pCodec,
                                           HDA_CODEC_CMD(uCmd, 0 /* LUN */), &uResp);
        if (RT_FAILURE(rc2))
            LogFunc(("Codec lookup failed with rc2=%Rrc\n", rc2));

        HDA_REG(pThis, IR)   = (uint32_t)uResp; /** @todo r=andy Do we need a 64-bit response? */
        HDA_REG(pThis, IRS)  = HDA_REG_FIELD_FLAG_MASK(IRS, IRV);  /* result is ready  */
        HDA_REG(pThis, IRS) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, ICB); /* busy is clear */
        return VINF_SUCCESS;
#else  /* !IN_RING3 */
        return VINF_IOM_R3_MMIO_WRITE;
#endif /* !IN_RING3 */
    }

    /*
     * Once the guest read the response, it should clean the IRV bit of the IRS register.
     */
    if (   u32Value & HDA_REG_FIELD_FLAG_MASK(IRS, IRV)
        && HDA_REG_FLAG_VALUE(pThis, IRS, IRV))
        HDA_REG(pThis, IRS) &= ~HDA_REG_FIELD_FLAG_MASK(IRS, IRV);
    return VINF_SUCCESS;
}

static int hdaRegWriteRIRBWP(PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF_PV(iReg);

    if (u32Value & HDA_REG_FIELD_FLAG_MASK(RIRBWP, RST))
        HDA_REG(pThis, RIRBWP) = 0;

    /* The remaining bits are O, see 6.2.22. */
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
            pThis->fDMAPosition = RT_BOOL(pThis->u64DPBase & RT_BIT_64(0));
            LogRel2(("HDA: %s DMA position buffer\n", pThis->fDMAPosition ? "Enabled" : "Disabled"));
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
    RT_NOREF_PV(iReg);

    uint8_t v = HDA_REG(pThis, RIRBSTS);
    HDA_REG(pThis, RIRBSTS) &= ~(v & u32Value);

    return hdaProcessInterrupt(pThis);
}

#ifdef IN_RING3
#ifdef LOG_ENABLED
static void hdaBDLEDumpAll(PHDASTATE pThis, uint64_t u64BDLBase, uint16_t cBDLE)
{
    LogFlowFunc(("BDLEs @ 0x%x (%RU16):\n", u64BDLBase, cBDLE));
    if (!u64BDLBase)
        return;

    uint32_t cbBDLE = 0;
    for (uint16_t i = 0; i < cBDLE; i++)
    {
        HDABDLEDESC bd;
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BDLBase + i * sizeof(HDABDLEDESC), &bd, sizeof(bd));

        LogFunc(("\t#%03d BDLE(adr:0x%llx, size:%RU32, ioc:%RTbool)\n",
                 i, bd.u64BufAdr, bd.u32BufSize, bd.fFlags & HDA_BDLE_FLAG_IOC));

        cbBDLE += bd.u32BufSize;
    }

    LogFlowFunc(("Total: %RU32 bytes\n", cbBDLE));

    if (!pThis->u64DPBase) /* No DMA base given? Bail out. */
        return;

    LogFlowFunc(("DMA counters:\n"));

    for (int i = 0; i < cBDLE; i++)
    {
        uint32_t uDMACnt;
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), (pThis->u64DPBase & DPBASE_ADDR_MASK) + (i * 2 * sizeof(uint32_t)),
                          &uDMACnt, sizeof(uDMACnt));

        LogFlowFunc(("\t#%03d DMA @ 0x%x\n", i , uDMACnt));
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

    if (!u64BaseDMA)
    {
        LogRel2(("HDA: Unable to fetch BDLE #%RU16 - no base DMA address set (yet)\n", u16Entry));
        return VERR_NOT_FOUND;
    }
    /** @todo Compare u16Entry with LVI. */

    int rc = PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BaseDMA + u16Entry * sizeof(HDABDLEDESC),
                               &pBDLE->Desc, sizeof(pBDLE->Desc));
    if (RT_FAILURE(rc))
        return rc;

    /* Set internal state. */
    pBDLE->State.u32BufOff   = 0;
    pBDLE->State.u32BDLIndex = u16Entry;

    return VINF_SUCCESS;
}

/**
 * Returns the number of outstanding stream data bytes which need to be processed
 * by the DMA engine assigned to this stream.
 *
 * @return Number of bytes for the DMA engine to process.
 */
DECLINLINE(uint32_t) hdaStreamGetTransferSize(PHDASTATE pThis, PHDASTREAM pStream, uint32_t cbMax)
{
    AssertPtrReturn(pThis,   0);
    AssertPtrReturn(pStream, 0);
    AssertReturn   (cbMax,   0);

    PHDABDLE pBDLE = &pStream->State.BDLE;

    uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, pStream->u8SD);
    Assert(u32LPIB <= pStream->u32CBL);

    /* Do we have space left in the CBL at all? */
    uint32_t cbData = pStream->u32CBL - u32LPIB;

    /* Limit to the available free space of the current BDLE. */
    cbData = RT_MIN(cbData, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

    /* Make sure we only transfer as many bytes as requested. */
    cbData = RT_MIN(cbData, cbMax);

    if (pBDLE->State.cbBelowFIFOW)
    {
        /* Are we not going to reach (or exceed) the FIFO watermark yet with the data to copy?
         * No need to read data from DMA then. */
        if (cbData > pBDLE->State.cbBelowFIFOW)
        {
            /* Subtract the amount of bytes that still would fit in the stream's FIFO
             * and therefore do not need to be processed by DMA. */
            cbData -= pBDLE->State.cbBelowFIFOW;
        }
    }

    AssertMsg((cbData % 2 == 0), ("Transfer size invalid: %RU32\n", cbData));

    Log3Func(("[SD%RU8]: CBL=%RU32, LPIB=%RU32, FIFOS=%RU16, cbData=%RU32, %R[bdle]\n", pStream->u8SD,
              pStream->u32CBL, HDA_STREAM_REG(pThis, LPIB, pStream->u8SD), pStream->u16FIFOS,
              cbData, pBDLE));

    return cbData;
}

DECLINLINE(void) hdaBDLEUpdate(PHDABDLE pBDLE, uint32_t cbData, uint32_t cbProcessed)
{
    AssertPtrReturnVoid(pBDLE);

    if (!cbData || !cbProcessed)
        return;

    Assert(pBDLE->Desc.u32BufSize >= cbProcessed);

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
                     pBDLE->State.cbBelowFIFOW, pBDLE->State.u32BufOff, pBDLE->Desc.u32BufSize));
    }
#endif

    pBDLE->State.cbBelowFIFOW -= RT_MIN(pBDLE->State.cbBelowFIFOW, cbProcessed);
    Assert(pBDLE->State.cbBelowFIFOW == 0);

    /* We always increment the position of DMA buffer counter because we're always reading
     * into an intermediate buffer. */
    Assert(pBDLE->Desc.u32BufSize >= (pBDLE->State.u32BufOff + cbProcessed));
    pBDLE->State.u32BufOff += cbProcessed;

    LogFlowFunc(("cbData=%RU32, cbProcessed=%RU32, %R[bdle]\n", cbData, cbProcessed, pBDLE));
}

#ifdef IN_RING3
/**
 * Initializes a stream mapping structure according to the given stream configuration.
 *
 * @return  IPRT status code.
 * @param   pMapping            Pointer to mapping to initialize.
 * @param   pCfg                Pointer to stream configuration to use.
 */
static int hdaStreamMapInit(PHDASTREAMMAPPING pMapping, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMapping, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,     VERR_INVALID_POINTER);

    AssertReturn(pCfg->Props.cChannels, VERR_INVALID_PARAMETER);

    hdaStreamMapReset(pMapping);

    pMapping->paChannels = (PPDMAUDIOSTREAMCHANNEL)RTMemAlloc(sizeof(PDMAUDIOSTREAMCHANNEL) * pCfg->Props.cChannels);
    if (!pMapping->paChannels)
        return VERR_NO_MEMORY;

    if (!DrvAudioHlpStreamCfgIsValid(pCfg))
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;

    Assert(RT_IS_POWER_OF_TWO(pCfg->Props.cBits));

    /** @todo We assume all channels in a stream have the same format. */
    PPDMAUDIOSTREAMCHANNEL pChan = pMapping->paChannels;
    for (uint8_t i = 0; i < pCfg->Props.cChannels; i++)
    {
        pChan->uChannel = i;
        pChan->cbStep   = (pCfg->Props.cBits / 2);
        pChan->cbFrame  = pChan->cbStep * pCfg->Props.cChannels;
        pChan->cbFirst  = i * pChan->cbStep;
        pChan->cbOff    = pChan->cbFirst;

        int rc2 = hdaStreamChannelDataInit(&pChan->Data, PDMAUDIOSTREAMCHANNELDATA_FLAG_NONE);
        if (RT_SUCCESS(rc))
            rc = rc2;

        if (RT_FAILURE(rc))
            break;

        pChan++;
    }

    if (   RT_SUCCESS(rc)
        /* Create circular buffer if not created yet. */
        && !pMapping->pCircBuf)
    {
        rc = RTCircBufCreate(&pMapping->pCircBuf, _4K); /** @todo Make size configurable? */
    }

    if (RT_SUCCESS(rc))
    {
        pMapping->cChannels = pCfg->Props.cChannels;
#ifdef VBOX_WITH_HDA_AUDIO_INTERLEAVING_STREAMS_SUPPORT
        pMapping->enmLayout = PDMAUDIOSTREAMLAYOUT_INTERLEAVED;
#else
        pMapping->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
#endif
    }

    return rc;
}

/**
 * Destroys a given stream mapping.
 *
 * @param   pMapping            Pointer to mapping to destroy.
 */
static void hdaStreamMapDestroy(PHDASTREAMMAPPING pMapping)
{
    hdaStreamMapReset(pMapping);

    if (pMapping->pCircBuf)
    {
        RTCircBufDestroy(pMapping->pCircBuf);
        pMapping->pCircBuf = NULL;
    }
}

/**
 * Resets a given stream mapping.
 *
 * @param   pMapping            Pointer to mapping to reset.
 */
static void hdaStreamMapReset(PHDASTREAMMAPPING pMapping)
{
    AssertPtrReturnVoid(pMapping);

    pMapping->enmLayout = PDMAUDIOSTREAMLAYOUT_UNKNOWN;

    if (pMapping->cChannels)
    {
        for (uint8_t i = 0; i < pMapping->cChannels; i++)
            hdaStreamChannelDataDestroy(&pMapping->paChannels[i].Data);

        AssertPtr(pMapping->paChannels);
        RTMemFree(pMapping->paChannels);
        pMapping->paChannels = NULL;

        pMapping->cChannels = 0;
    }
}
#endif /* IN_RING3 */

DECLINLINE(bool) hdaStreamNeedsNextBDLE(PHDASTATE pThis, PHDASTREAM pStream)
{
    AssertPtrReturn(pThis,   false);
    AssertPtrReturn(pStream, false);

    PHDABDLE pBDLE   = &pStream->State.BDLE;
    uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, pStream->u8SD);

    /* Did we reach the CBL (Cyclic Buffer List) limit? */
    bool fCBLLimitReached = u32LPIB >= pStream->u32CBL;

    /* Do we need to use the next BDLE entry? Either because we reached
     * the CBL limit or our internal DMA buffer is full. */
    bool fNeedsNextBDLE   = (   fCBLLimitReached
                             || (pBDLE->State.u32BufOff >= pBDLE->Desc.u32BufSize));

    Assert(u32LPIB                <= pStream->u32CBL);
    Assert(pBDLE->State.u32BufOff <= pBDLE->Desc.u32BufSize);

    LogFlowFunc(("[SD%RU8]: LPIB=%RU32, CBL=%RU32, fCBLLimitReached=%RTbool, fNeedsNextBDLE=%RTbool, %R[bdle]\n",
                 pStream->u8SD, u32LPIB, pStream->u32CBL, fCBLLimitReached, fNeedsNextBDLE, pBDLE));

    return fNeedsNextBDLE;
}

DECLINLINE(void) hdaStreamTransferUpdate(PHDASTATE pThis, PHDASTREAM pStream, uint32_t cbInc)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pStream);

    if (!cbInc) /* Nothing to do? Bail out early. */
        return;

    /*
     * If we're below the FIFO watermark (SDFIFOW), it's expected that HDA
     * doesn't fetch anything via DMA, so just update LPIB.
     * (ICH6 datasheet 18.2.38).
     */
    PHDABDLE pBDLE = &pStream->State.BDLE;
    if (pBDLE->State.cbBelowFIFOW == 0) /* Did we hit (or exceed) the watermark? */
    {
        uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, pStream->u8SD) + cbInc;

        Log3Func(("[SD%RU8]: LPIB: %RU32 -> %RU32, CBL=%RU32\n",
                  pStream->u8SD, HDA_STREAM_REG(pThis, LPIB, pStream->u8SD), u32LPIB, pStream->u32CBL));

        hdaStreamUpdateLPIB(pThis, pStream, u32LPIB);
    }
}

static bool hdaBDLEIsComplete(PHDABDLE pBDLE, bool *pfInterrupt)
{
    AssertPtrReturn(pBDLE, true);

    bool fInterrupt  = false;
    bool fIsComplete = false;

    /* Check if the current BDLE entry is complete (full). */
    if (pBDLE->State.u32BufOff >= pBDLE->Desc.u32BufSize)
    {
        Assert(pBDLE->State.u32BufOff <= pBDLE->Desc.u32BufSize);

        if (/* IOC (Interrupt On Completion) bit set? */
               pBDLE->Desc.fFlags & HDA_BDLE_FLAG_IOC
            /* All data put into the DMA FIFO? */
            && pBDLE->State.cbBelowFIFOW == 0
           )
        {
            fInterrupt = true;
        }

        fIsComplete = true;
    }

    if (pfInterrupt)
        *pfInterrupt = fInterrupt;

    LogFlowFunc(("%R[bdle] fIsComplete=%RTbool, fInterrupt=%RTbool\n", pBDLE, fIsComplete, fInterrupt));

    return fIsComplete;
}

/**
 * Retrieves a corresponding sink for a given mixer control.
 * Returns NULL if no sink is found.
 *
 * @return  PHDAMIXERSINK
 * @param   pThis               HDA state.
 * @param   enmMixerCtl         Mixer control to get the corresponding sink for.
 */
static PHDAMIXERSINK hdaMixerControlToSink(PHDASTATE pThis, PDMAUDIOMIXERCTL enmMixerCtl)
{
    PHDAMIXERSINK pSink;

    switch (enmMixerCtl)
    {
        case PDMAUDIOMIXERCTL_VOLUME_MASTER:
            /* Fall through is intentional. */
        case PDMAUDIOMIXERCTL_FRONT:
            pSink = &pThis->SinkFront;
            break;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        case PDMAUDIOMIXERCTL_CENTER_LFE:
            pSink = &pThis->SinkCenterLFE;
            break;
        case PDMAUDIOMIXERCTL_REAR:
            pSink = &pThis->SinkRear;
            break;
#endif
        case PDMAUDIOMIXERCTL_LINE_IN:
            pSink = &pThis->SinkLineIn;
            break;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        case PDMAUDIOMIXERCTL_MIC_IN:
            pSink = &pThis->SinkMicIn;
            break;
#endif
        default:
            pSink = NULL;
            AssertMsgFailed(("Unhandled mixer control\n"));
            break;
    }

    return pSink;
}

static DECLCALLBACK(int) hdaMixerAddStream(PHDASTATE pThis, PHDAMIXERSINK pSink, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    LogFunc(("Sink=%s, Stream=%s\n", pSink->pMixSink->pszName, pCfg->szName));

    if (!DrvAudioHlpStreamCfgIsValid(pCfg))
        return VERR_INVALID_PARAMETER;

    int rc = AudioMixerSinkSetFormat(pSink->pMixSink, &pCfg->Props);
    if (RT_FAILURE(rc))
        return rc;

    PHDADRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
    {
        int rc2 = VINF_SUCCESS;
        PHDADRIVERSTREAM pDrvStream = NULL;

        PPDMAUDIOSTREAMCFG pStreamCfg = DrvAudioHlpStreamCfgDup(pCfg);
        if (!pStreamCfg)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        /* Include the driver's LUN in the stream name for easier identification. */
        RTStrPrintf(pStreamCfg->szName, RT_ELEMENTS(pStreamCfg->szName), "[LUN#%RU8] %s", pDrv->uLUN, pCfg->szName);

        if (pStreamCfg->enmDir == PDMAUDIODIR_IN)
        {
            LogFunc(("enmRecSource=%d\n", pStreamCfg->DestSource.Source));

            switch (pStreamCfg->DestSource.Source)
            {
                case PDMAUDIORECSOURCE_LINE:
                    pDrvStream = &pDrv->LineIn;
                    break;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                case PDMAUDIORECSOURCE_MIC:
                    pDrvStream = &pDrv->MicIn;
                    break;
#endif
                default:
                    rc2 = VERR_NOT_SUPPORTED;
                    break;
            }
        }
        else if (pStreamCfg->enmDir == PDMAUDIODIR_OUT)
        {
            LogFunc(("enmPlaybackDest=%d\n", pStreamCfg->DestSource.Dest));

            switch (pStreamCfg->DestSource.Dest)
            {
                case PDMAUDIOPLAYBACKDEST_FRONT:
                    pDrvStream = &pDrv->Front;
                    break;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
                case PDMAUDIOPLAYBACKDEST_CENTER_LFE:
                    pDrvStream = &pDrv->CenterLFE;
                    break;
                case PDMAUDIOPLAYBACKDEST_REAR:
                    pDrvStream = &pDrv->Rear;
                    break;
#endif
                default:
                    rc2 = VERR_NOT_SUPPORTED;
                    break;
            }
        }
        else
            rc2 = VERR_NOT_SUPPORTED;

        if (RT_SUCCESS(rc2))
        {
            AssertPtr(pDrvStream);

            AudioMixerSinkRemoveStream(pSink->pMixSink, pDrvStream->pMixStrm);

            AudioMixerStreamDestroy(pDrvStream->pMixStrm);
            pDrvStream->pMixStrm = NULL;

            PAUDMIXSTREAM pMixStrm;
            rc2 = AudioMixerSinkCreateStream(pSink->pMixSink, pDrv->pConnector, pStreamCfg, 0 /* fFlags */, &pMixStrm);
            if (RT_SUCCESS(rc2))
            {
                rc2 = AudioMixerSinkAddStream(pSink->pMixSink, pMixStrm);
                LogFlowFunc(("LUN#%RU8: Added \"%s\" to sink, rc=%Rrc\n", pDrv->uLUN, pStreamCfg->szName , rc2));
            }

            if (RT_SUCCESS(rc2))
                pDrvStream->pMixStrm = pMixStrm;

            /* If creating a stream fails, be forgiving and continue -- don't pass rc2 to rc here. */
        }

        if (pStreamCfg)
        {
            RTMemFree(pStreamCfg);
            pStreamCfg = NULL;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds a new audio stream to a specific mixer control.
 * Depending on the mixer control the stream then gets assigned to one of the internal
 * mixer sinks, which in turn then handle the mixing of all connected streams to that sink.
 *
 * @return  IPRT status code.
 * @param   pThis               HDA state.
 * @param   enmMixerCtl         Mixer control to assign new stream to.
 * @param   pCfg                Stream configuration for the new stream.
 */
static DECLCALLBACK(int) hdaMixerAddStream(PHDASTATE pThis, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    int rc;

    PHDAMIXERSINK pSink = hdaMixerControlToSink(pThis, enmMixerCtl);
    if (pSink)
    {
        rc = hdaMixerAddStream(pThis, pSink, pCfg);

        AssertPtr(pSink->pMixSink);
        LogFlowFunc(("Sink=%s, enmMixerCtl=%d\n", pSink->pMixSink->pszName, enmMixerCtl));
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Removes a specified mixer control from the HDA's mixer.
 *
 * @return  IPRT status code.
 * @param   pThis               HDA state.
 * @param   enmMixerCtl         Mixer control to remove.
 *
 * @remarks Can be called as a callback by the HDA codec.
 */
static DECLCALLBACK(int) hdaMixerRemoveStream(PHDASTATE pThis, PDMAUDIOMIXERCTL enmMixerCtl)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    int rc;

    PHDAMIXERSINK pSink = hdaMixerControlToSink(pThis, enmMixerCtl);
    if (pSink)
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            PAUDMIXSTREAM pMixStream = NULL;
            switch (enmMixerCtl)
            {
                /*
                 * Input.
                 */
                case PDMAUDIOMIXERCTL_LINE_IN:
                    pMixStream = pDrv->LineIn.pMixStrm;
                    pDrv->LineIn.pMixStrm = NULL;
                    break;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                case PDMAUDIOMIXERCTL_MIC_IN:
                    pMixStream = pDrv->MicIn.pMixStrm;
                    pDrv->MicIn.pMixStrm = NULL;
                    break;
#endif
                /*
                 * Output.
                 */
                case PDMAUDIOMIXERCTL_FRONT:
                    pMixStream = pDrv->Front.pMixStrm;
                    pDrv->Front.pMixStrm = NULL;
                    break;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
                case PDMAUDIOMIXERCTL_CENTER_LFE:
                    pMixStream = pDrv->CenterLFE.pMixStrm;
                    pDrv->CenterLFE.pMixStrm = NULL;
                    break;
                case PDMAUDIOMIXERCTL_REAR:
                    pMixStream = pDrv->Rear.pMixStrm;
                    pDrv->Rear.pMixStrm = NULL;
                    break;
#endif
                default:
                    AssertMsgFailed(("Mixer control %d not implemented\n", enmMixerCtl));
                    break;
            }

            if (pMixStream)
            {
                AudioMixerSinkRemoveStream(pSink->pMixSink, pMixStream);
                AudioMixerStreamDestroy(pMixStream);

                pMixStream = NULL;
            }
        }

        AudioMixerSinkRemoveAllStreams(pSink->pMixSink);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFunc(("enmMixerCtl=%d, rc=%Rrc\n", enmMixerCtl, rc));
    return rc;
}

/**
 * Sets a SDn stream number and channel to a particular mixer control.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA State.
 * @param   enmMixerCtl         Mixer control to set SD stream number and channel for.
 * @param   uSD                 SD stream number (number + 1) to set. Set to 0 for unassign.
 * @param   uChannel            Channel to set. Only valid if a valid SD stream number is specified.
 *
 * @remarks Can be called as a callback by the HDA codec.
 */
static DECLCALLBACK(int) hdaMixerSetStream(PHDASTATE pThis, PDMAUDIOMIXERCTL enmMixerCtl, uint8_t uSD, uint8_t uChannel)
{
    LogFlowFunc(("enmMixerCtl=%RU32, uSD=%RU8, uChannel=%RU8\n", enmMixerCtl, uSD, uChannel));

    if (uSD == 0) /* Stream number 0 is reserved. */
    {
        LogFlowFunc(("Invalid SDn (%RU8) number for mixer control %d, ignoring\n", uSD, enmMixerCtl));
        return VINF_SUCCESS;
    }
    /* uChannel is optional. */

    /* SDn0 starts as 1. */
    Assert(uSD);
    uSD--;

    int rc;

    PHDAMIXERSINK pSink = hdaMixerControlToSink(pThis, enmMixerCtl);
    if (pSink)
    {
        if (   (uSD < HDA_MAX_SDI)
            && AudioMixerSinkGetDir(pSink->pMixSink) == AUDMIXSINKDIR_OUTPUT)
        {
            uSD += HDA_MAX_SDI;
        }

        LogFlowFunc(("%s: Setting to stream ID=%RU8, channel=%RU8, enmMixerCtl=%RU32\n",
                     pSink->pMixSink->pszName, uSD, uChannel, enmMixerCtl));

        Assert(uSD < HDA_MAX_STREAMS);

        PHDASTREAM pStream = hdaStreamGetFromSD(pThis, uSD);
        if (pStream)
        {
            hdaStreamLock(pStream);

            pSink->uSD        = uSD;
            pSink->uChannel   = uChannel;
            pStream->pMixSink = pSink;

            hdaStreamUnlock(pStream);

            rc = VINF_SUCCESS;
        }
        else
        {
            LogRel(("HDA: Guest wanted to assign invalid stream ID=%RU8 (channel %RU8) to mixer control %RU32, skipping\n",
                    uSD, uChannel, enmMixerCtl));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets the volume of a specified mixer control.
 *
 * @return  IPRT status code.
 * @param   pThis               HDA State.
 * @param   enmMixerCtl         Mixer control to set volume for.
 * @param   pVol                Pointer to volume data to set.
 *
 * @remarks Can be called as a callback by the HDA codec.
 */
static DECLCALLBACK(int) hdaMixerSetVolume(PHDASTATE pThis,
                                           PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOVOLUME pVol)
{
    int rc;

    PHDAMIXERSINK pSink = hdaMixerControlToSink(pThis, enmMixerCtl);
    if (pSink)
    {
        /* Set the volume.
         * We assume that the codec already converted it to the correct range. */
        rc = AudioMixerSinkSetVolume(pSink->pMixSink, pVol);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
/**
 * Starts the internal audio device timer (if not started yet).
 *
 * @param   pThis               HDA state.
 */
static void hdaTimerMaybeStart(PHDASTATE pThis)
{
    LogFlowFuncEnter();

    if (!pThis->pTimer)
        return;

    pThis->cStreamsActive++;

    /* Only start the timer at the first active stream. */
    if (pThis->cStreamsActive == 1)
    {
        LogRel2(("HDA: Starting transfers\n"));

        /* Set timer flag. */
        ASMAtomicXchgBool(&pThis->fTimerActive, true);

        /* Update current time timestamp. */
        pThis->uTimerTS = TMTimerGet(pThis->pTimer);

        /* Start transfers. */
        hdaTimerMain(pThis);
    }
}

/**
 * Stops the internal audio device timer.
 *
 * @param   pThis               HDA state.
 */
static void hdaTimerStop(PHDASTATE pThis)
{
    LogFlowFuncEnter();

    /* Set timer flag. */
    ASMAtomicXchgBool(&pThis->fTimerActive, false);
}

/**
 * Decreases the active HDA streams count by one and
 * then checks if the internal audio device timer can be
 * stopped.
 *
 * @param   pThis               HDA state.
 */
static void hdaTimerMaybeStop(PHDASTATE pThis)
{
    LogFlowFuncEnter();

    if (!pThis->pTimer)
        return;

    if (pThis->cStreamsActive) /* Disable can be called mupltiple times. */
    {
        pThis->cStreamsActive--;

        if (pThis->cStreamsActive == 0)
            hdaTimerStop(pThis);
    }
}

/**
 * Main routine for the device timer.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 */
static void hdaTimerMain(PHDASTATE pThis)
{
    AssertPtrReturnVoid(pThis);

    STAM_PROFILE_START(&pThis->StatTimer, a);

    uint64_t cTicksNow = TMTimerGet(pThis->pTimer);

    /* Update current time timestamp. */
    pThis->uTimerTS = cTicksNow;

    /* Flag indicating whether to kick the timer again for a
     * new data processing round. */
    bool fKickTimer = false;

    hdaDoTransfers(pThis);

    /* Do we need to kick the timer again? */
    if (   AudioMixerSinkIsActive(pThis->SinkFront.pMixSink)
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        || AudioMixerSinkIsActive(pThis->SinkCenterLFE.pMixSink)
        || AudioMixerSinkIsActive(pThis->SinkRear.pMixSink)
#endif
        || AudioMixerSinkIsActive(pThis->SinkLineIn.pMixSink)
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        || AudioMixerSinkIsActive(pThis->SinkMicIn.pMixSink)
#endif
        )
    {
        fKickTimer = true;
    }

    pThis->uTimerMS = RTTimeMilliTS();

    if (   ASMAtomicReadBool(&pThis->fTimerActive)
        || fKickTimer)
    {
        /* Kick the timer again. */
        uint64_t cTicks = pThis->cTimerTicks;
        /** @todo adjust cTicks down by now much cbOutMin represents. */
        TMTimerSet(pThis->pTimer, cTicksNow + cTicks);
    }
    else
        LogRel2(("HDA: Stopping transfers\n"));

    STAM_PROFILE_STOP(&pThis->StatTimer, a);
}

/**
 * Timer callback which handles the audio data transfers on a periodic basis.
 *
 * @param   pDevIns             Device instance.
 * @param   pTimer              Timer which was used when calling this.
 * @param   pvUser              User argument as PHDASTATE.
 */
static DECLCALLBACK(void) hdaTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    RT_NOREF(pDevIns, pTimer);

    PHDASTATE pThis = (PHDASTATE)pvUser;
    Assert(pThis == PDMINS_2_DATA(pDevIns, PHDASTATE));
    AssertPtr(pThis);

    hdaTimerMain(pThis);
}

#else /* VBOX_WITH_AUDIO_HDA_CALLBACKS */

static DECLCALLBACK(int) hdaCallbackInput(PDMAUDIOCBTYPE enmType, void *pvCtx, size_t cbCtx, void *pvUser, size_t cbUser)
{
    Assert(enmType == PDMAUDIOCALLBACKTYPE_INPUT);
    AssertPtrReturn(pvCtx,  VERR_INVALID_POINTER);
    AssertReturn(cbCtx,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertReturn(cbUser,    VERR_INVALID_PARAMETER);

    PHDACALLBACKCTX pCtx = (PHDACALLBACKCTX)pvCtx;
    AssertReturn(cbCtx == sizeof(HDACALLBACKCTX), VERR_INVALID_PARAMETER);

    PPDMAUDIOCBDATA_DATA_INPUT pData = (PPDMAUDIOCBDATA_DATA_INPUT)pvUser;
    AssertReturn(cbUser == sizeof(PDMAUDIOCBDATA_DATA_INPUT), VERR_INVALID_PARAMETER);

    return hdaStreamDoDMA(pCtx->pThis, PI_INDEX, UINT32_MAX, &pData->cbOutRead);
}

static DECLCALLBACK(int) hdaCallbackOutput(PDMAUDIOCBTYPE enmType, void *pvCtx, size_t cbCtx, void *pvUser, size_t cbUser)
{
    Assert(enmType == PDMAUDIOCALLBACKTYPE_OUTPUT);
    AssertPtrReturn(pvCtx,  VERR_INVALID_POINTER);
    AssertReturn(cbCtx,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertReturn(cbUser,    VERR_INVALID_PARAMETER);

    PHDACALLBACKCTX pCtx = (PHDACALLBACKCTX)pvCtx;
    AssertReturn(cbCtx == sizeof(HDACALLBACKCTX), VERR_INVALID_PARAMETER);

    PPDMAUDIOCBDATA_DATA_OUTPUT pData = (PPDMAUDIOCBDATA_DATA_OUTPUT)pvUser;
    AssertReturn(cbUser == sizeof(PDMAUDIOCBDATA_DATA_OUTPUT), VERR_INVALID_PARAMETER);

    PHDASTATE pThis = pCtx->pThis;

    int rc = hdaStreamDoDMA(pCtx->pThis, PO_INDEX, UINT32_MAX, &pData->cbOutWritten);
    if (   RT_SUCCESS(rc)
        && pData->cbOutWritten)
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            uint32_t cSamplesPlayed;
            int rc2 = pDrv->pConnector->pfnPlay(pDrv->pConnector, &cSamplesPlayed);
            LogFlowFunc(("LUN#%RU8: cSamplesPlayed=%RU32, rc=%Rrc\n", pDrv->uLUN, cSamplesPlayed, rc2));
        }
    }
}
#endif /* VBOX_WITH_AUDIO_HDA_CALLBACKS */

/**
 * Main routine to perform the actual audio data transfers from the HDA streams
 * to the backend(s) and vice versa.
 *
 * @param   pThis               HDA state.
 */
static void hdaDoTransfers(PHDASTATE pThis)
{
    PHDASTREAM pStreamLineIn  = hdaSinkGetStream(pThis, &pThis->SinkLineIn);
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    PHDASTREAM pStreamMicIn   = hdaSinkGetStream(pThis, &pThis->SinkMicIn);
#endif
    PHDASTREAM pStreamFront   = hdaSinkGetStream(pThis, &pThis->SinkFront);
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** @todo See note below. */
#endif

    hdaStreamUpdate(pThis, pStreamLineIn);
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    hdaStreamUpdate(pThis, pStreamMicIn);
#endif
    hdaStreamUpdate(pThis, pStreamFront);


#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    int rc2 = AudioMixerSinkUpdate(pThis->SinkCenterLFE.pMixSink);
    AssertRC(rc2);

    rc2 = AudioMixerSinkUpdate(pThis->SinkRear.pMixSink);
    AssertRC(rc2);
    /** @todo Check for stream interleaving and only call hdaStreamDoDMA() if required! */

    /*
     * Only call hdaTransfer if CenterLFE and/or Rear are on different SDs,
     * otherwise we have to use the interleaved streams support for getting the data
     * out of the Front sink (depending on the mapping layout).
     */
#endif
}

#ifdef DEBUG_andy
# define HDA_DEBUG_DMA
#endif

/**
 * Does a single DMA transfer for a specific HDA stream (SDI/SDO).
 * This either can be a read or write operation, depending on the HDA stream.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to do the DMA transfer for.
 * @param   pvBuf               Pointer to buffer data to write data to / read data from.
 * @param   cbBuf               Size of buffer (in bytes).
 * @param   cbToProcess         Size (in bytes) to transfer (read/write).
 * @param   pcbProcessed        Size (in bytes) transferred (read/written). Optional.
 */
static int hdaStreamDoDMA(PHDASTATE pThis, PHDASTREAM pStream, void *pvBuf, uint32_t cbBuf,
                          uint32_t cbToProcess, uint32_t *pcbProcessed)
{
    AssertPtrReturn(pThis,             VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,           VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,             VERR_INVALID_POINTER);
    AssertReturn(cbBuf >= cbToProcess, VERR_INVALID_PARAMETER);
    /* pcbProcessed is optional. */

    if (ASMAtomicReadBool(&pThis->fInReset)) /* HDA controller in reset mode? Bail out. */
    {
        if (pcbProcessed)
            *pcbProcessed = 0;
        return VINF_SUCCESS;
    }

    bool fProceed = true;

    Log3Func(("[SD%RU8] %R[sdsts] cbToProcess=%RU32\n",
              pStream->u8SD, HDA_STREAM_REG(pThis, STS, pStream->u8SD), cbToProcess));

    /* Is the stream not in a running state currently? */
    if (!(HDA_STREAM_REG(pThis, CTL, pStream->u8SD) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN)))
        fProceed = false;
    /* Is the BCIS (Buffer Completion Interrupt Status) flag set? Then we have to wait and skip. */
    else if ((HDA_STREAM_REG(pThis, STS, pStream->u8SD) & HDA_SDSTS_BCIS))
        fProceed = false;

    if (!fProceed)
    {
#ifdef HDA_DEBUG_DMA
        Log3Func(("[SD%RU8] DMA: Skip\n", pStream->u8SD));
#endif
        if (pcbProcessed)
            *pcbProcessed = 0;
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;

#ifdef HDA_DEBUG_DMA
    Log3Func(("[SD%RU8] DMA: Start\n", pStream->u8SD));
#endif

    /* Sanity checks. */
    Assert(pStream->u8SD <= HDA_MAX_STREAMS);
    Assert(pStream->u64BDLBase);
    Assert(pStream->u32CBL);

    /* State sanity checks. */
    Assert(pStream->State.fInReset == false);
    Assert(HDA_STREAM_REG(pThis, LPIB, pStream->u8SD) <= pStream->u32CBL);

    bool fSendInterrupt = false;

    /* Only do one FIFO size at a time. */
    uint32_t cbLeft           = RT_MIN(pStream->u16FIFOS, RT_MIN(cbToProcess, cbBuf));
    uint32_t cbTotal          = 0;
    uint32_t cbChunk          = 0;
    uint32_t cbChunkProcessed = 0;

#ifdef HDA_DEBUG_DMA
    LogFunc(("[SD%RU8] DMA: cbToProcess=%RU32, cbLeft=%RU32\n", pStream->u8SD, cbToProcess, cbLeft));
#endif

    /* Get the maximum number of BDL entries. */
    uint16_t cBDLE = pStream->u16LVI + 1;

    while (   cbLeft
           && cBDLE--)
    {
        cbChunk          = hdaStreamGetTransferSize(pThis, pStream, cbLeft);
        cbChunkProcessed = 0;

        PHDABDLE pBDLE = &pStream->State.BDLE;

        if (cbChunk)
        {
            if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
            {
                PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns),
                                  pBDLE->Desc.u64BufAdr + pBDLE->State.u32BufOff,
                                  (uint8_t *)pvBuf + cbTotal, cbChunk);
            }
            else /* Input (SDI). */
            {
                PDMDevHlpPhysWrite(pThis->CTX_SUFF(pDevIns),
                                   pBDLE->Desc.u64BufAdr + pBDLE->State.u32BufOff,
                                   (uint8_t *)pvBuf + cbTotal, cbChunk);
            }

#ifdef HDA_DEBUG_DUMP_PCM_DATA
            RTFILE fh;
            RTFileOpen(&fh,
                         hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT
                       ? HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaDMARead.pcm" : HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaDMAWrite.pcm",
                       RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
            RTFileWrite(fh, (uint8_t *)pvBuf + cbTotal, cbChunk, NULL);
            RTFileClose(fh);
#endif
        }

        cbChunkProcessed = cbChunk;

        hdaBDLEUpdate(pBDLE, cbChunkProcessed, cbChunkProcessed);

        LogFunc(("[SD%RU8] DMA: Entry %RU32 Pos %RU32/%RU32 Chunk %RU32\n",
                 pStream->u8SD, pBDLE->State.u32BDLIndex, pBDLE->State.u32BufOff, pBDLE->Desc.u32BufSize, cbChunk));

        Assert(cbLeft >= cbChunkProcessed);
        cbLeft  -= cbChunkProcessed;
        cbTotal += cbChunkProcessed;
        Assert(cbTotal <= cbToProcess);

        hdaStreamTransferUpdate(pThis, pStream, cbChunkProcessed);

#ifdef HDA_DEBUG_DMA
        LogFunc(("[SD%RU8] DMA: LPIB %RU32 Pos %RU32 Left %RU32\n",
                 pStream->u8SD, HDA_STREAM_REG(pThis, LPIB, pStream->u8SD), cbTotal, cbLeft));
#endif
        bool fNeedsInterrupt = false;
        bool fBDLEIsComplete = hdaBDLEIsComplete(pBDLE, &fNeedsInterrupt);

        if (fNeedsInterrupt)
            fSendInterrupt = true;

        if (fBDLEIsComplete)
        {
            /* Do we need to fetch the next Buffer Descriptor Entry (BDLE)? */
            if (hdaStreamNeedsNextBDLE(pThis, pStream))
            {
                bool fWrapAround;
                rc = hdaStreamGetNextBDLE(pThis, pStream, &fWrapAround);
                if (   RT_SUCCESS(rc)
                    && fWrapAround)
                {
                    hdaStreamUpdateLPIB(pThis, pStream, 0);
                }
            }
        }

        if (RT_FAILURE(rc))
            break;
    }

    Log3Func(("[SD%RU8]: cbLeft=%RU32, rc=%Rrc\n", pStream->u8SD, cbLeft, rc));

    if (fSendInterrupt)
    {
#ifdef HDA_DEBUG_DMA
        Log3Func(("[SD%RU8] DMA: Interrupt\n", pStream->u8SD));
#endif
        /**
         * Set the BCIS (Buffer Completion Interrupt Status) flag as the
         * last byte of data for the current descriptor has been fetched
         * from memory and put into the DMA FIFO.
         *
         * Speech synthesis works fine on Mac Guest if this bit isn't set
         * but in general sound quality gets worse.
         *
         * This must be set in *any* case.
         */
        HDA_STREAM_REG(pThis, STS, pStream->u8SD) |= HDA_SDSTS_BCIS;
        Log3Func(("[SD%RU8]: BCIS set\n", pStream->u8SD));

        hdaProcessInterrupt(pThis);
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbProcessed)
            *pcbProcessed = cbTotal;
    }

#ifdef HDA_DEBUG_DMA
    Log3Func(("[SD%RU8] DMA: End\n", pStream->u8SD));
#endif

    return rc;
}

/**
 * Writes audio data from a mixer sink into an HDA stream's DMA buffer.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to write to.
 * @param   cbToWrite           Number of bytes to write.
 * @param   pcbWritten          Number of bytes written. Optional.
 */
static int hdaStreamWrite(PHDASTATE pThis, PHDASTREAM pStream, uint32_t cbToWrite, uint32_t *pcbWritten)
{
    RT_NOREF(pThis);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertReturn(cbToWrite,  VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PHDAMIXERSINK pSink = pStream->pMixSink;
    if (!pSink)
    {
        AssertMsgFailed(("[SD%RU8]: Can't write to a stream with no sink attached\n", pStream->u8SD));

        if (pcbWritten)
            *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    void *pvDst;
    size_t cbDst;

    uint32_t cbWritten = 0;

    RTCircBufAcquireWriteBlock(pCircBuf, cbToWrite, &pvDst, &cbDst);

    if (cbDst)
    {
        int rc2 = AudioMixerSinkRead(pSink->pMixSink, AUDMIXOP_COPY, pvDst, (uint32_t)cbDst, &cbWritten);
        AssertRC(rc2);

        Assert(cbDst >= cbWritten);
        Log2Func(("[SD%RU8]: %zu/%zu bytes written\n", pStream->u8SD, cbWritten, cbDst));

#ifdef HDA_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        RTFileOpen(&fh, HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaStreamWrite.pcm",
                   RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, pvDst, cbWritten, NULL);
        RTFileClose(fh);
#endif
    }

    RTCircBufReleaseWriteBlock(pCircBuf, cbWritten);

    if (pcbWritten)
        *pcbWritten = cbWritten;

    return VINF_SUCCESS;
}

/**
 * Reads audio data from an HDA stream's DMA buffer and writes into a specified mixer sink.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to read audio data from.
 * @param   cbToRead            Number of bytes to read.
 * @param   pcbRead             Number of bytes read. Optional.
 */
static int hdaStreamRead(PHDASTATE pThis, PHDASTREAM pStream, uint32_t cbToRead, uint32_t *pcbRead)
{
    RT_NOREF(pThis);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertReturn(cbToRead,   VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PHDAMIXERSINK pSink = pStream->pMixSink;
    if (!pSink)
    {
        AssertMsgFailed(("[SD%RU8]: Can't read from a stream with no sink attached\n", pStream->u8SD));

        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    void *pvSrc;
    size_t cbSrc;

    uint32_t cbRead = 0;

    RTCircBufAcquireReadBlock(pCircBuf, cbToRead, &pvSrc, &cbSrc);

    if (cbSrc)
    {
        Log2Func(("[SD%RU8]: Reading %zu bytes ...\n", pStream->u8SD, cbSrc));

#ifdef HDA_DEBUG_DUMP_PCM_DATA
        RTFILE fh;
        RTFileOpen(&fh, HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaStreamRead.pcm",
                   RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, pvSrc, cbSrc, NULL);
        RTFileClose(fh);
#endif
        int rc2 = AudioMixerSinkWrite(pSink->pMixSink, AUDMIXOP_COPY, pvSrc, (uint32_t)cbSrc, &cbRead);
        AssertRC(rc2);

        Assert(cbSrc >= cbRead);
        Log2Func(("[SD%RU8]: %zu/%zu bytes read\n", pStream->u8SD, cbRead, cbSrc));
    }

    RTCircBufReleaseReadBlock(pCircBuf, cbRead);

    if (pcbRead)
        *pcbRead = cbRead;

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * Asynchronous I/O thread for a HDA stream.
 * This will do the heavy lifting work for us as soon as it's getting notified by another thread.
 *
 * @returns IPRT status code.
 * @param   hThreadSelf         Thread handle.
 * @param   pvUser              User argument. Must be of type PHDASTREAMTHREADCTX.
 */
static DECLCALLBACK(int) hdaStreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PHDASTREAMTHREADCTX pCtx = (PHDASTREAMTHREADCTX)pvUser;
    AssertPtr(pCtx);

    PHDASTATE pThis = pCtx->pThis;
    AssertPtr(pThis);

    PHDASTREAM pStream = pCtx->pStream;
    AssertPtr(pStream);

    PHDASTREAMSTATEAIO pAIO = &pCtx->pStream->State.AIO;

    PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    ASMAtomicXchgBool(&pAIO->fStarted, true);

    RTThreadUserSignal(hThreadSelf);

    LogFunc(("[SD%RU8]: Started\n", pStream->u8SD));

    for (;;)
    {
        int rc2 = RTSemEventWait(pAIO->Event, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc2))
            break;

        if (ASMAtomicReadBool(&pAIO->fShutdown))
            break;

        PHDAMIXERSINK pSink = pStream->pMixSink;

        rc2 = RTCritSectEnter(&pAIO->CritSect);
        if (RT_SUCCESS(rc2))
        {
            if (!pAIO->fEnabled)
            {
                RTCritSectLeave(&pAIO->CritSect);
                continue;
            }

            uint32_t cbToProcess;
            uint32_t cbProcessed = 0;

            if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
            {
                cbToProcess = (uint32_t)RTCircBufUsed(pCircBuf);
                if (cbToProcess)
                    rc2 = hdaStreamRead(pThis, pStream, cbToProcess, &cbProcessed);
            }
            else /* Input (SDI). */
            {
                cbToProcess = (uint32_t)RTCircBufFree(pCircBuf);
                if (cbToProcess)
                    rc2 = hdaStreamWrite(pThis, pStream, cbToProcess, &cbProcessed);
            }

            if (   pSink
                && RT_SUCCESS(rc2))
            {
                rc2 = AudioMixerSinkUpdate(pSink->pMixSink);
            }

            int rc3 = RTCritSectLeave(&pAIO->CritSect);
            AssertRC(rc3);
        }

        AssertRC(rc2);
    }

    LogFunc(("[SD%RU8]: Ended\n", pStream->u8SD));

    ASMAtomicXchgBool(&pAIO->fStarted, false);

    return VINF_SUCCESS;
}

/**
 * Creates the async I/O thread for a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA audio stream to create the async I/O thread for.
 */
static int hdaStreamAsyncIOCreate(PHDASTATE pThis, PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    int rc;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
    {
        pAIO->fShutdown = false;

        rc = RTSemEventCreate(&pAIO->Event);
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pAIO->CritSect);
            if (RT_SUCCESS(rc))
            {
                HDASTREAMTHREADCTX Ctx = { pThis, pStream };

                char szThreadName[64];
                RTStrPrintf2(szThreadName, sizeof(szThreadName), "hdaAIO%RU8", pStream->u8SD);

                rc = RTThreadCreate(&pAIO->Thread, hdaStreamAsyncIOThread, &Ctx,
                                    0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, szThreadName);
                if (RT_SUCCESS(rc))
                    rc = RTThreadUserWait(pAIO->Thread, 10 * 1000 /* 10s timeout */);
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    LogFunc(("[SD%RU8]: Returning %Rrc\n", pStream->u8SD, rc));
    return rc;
}

/**
 * Destroys the async I/O thread of a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA audio stream to destroy the async I/O thread for.
 */
static int hdaStreamAsyncIODestroy(PHDASTATE pThis, PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return VINF_SUCCESS;

    ASMAtomicWriteBool(&pAIO->fShutdown, true);

    int rc = hdaStreamAsyncIONotify(pThis, pStream);
    AssertRC(rc);

    int rcThread;
    rc = RTThreadWait(pAIO->Thread, 30 * 1000 /* 30s timeout */, &rcThread);
    LogFunc(("Async I/O thread ended with %Rrc (%Rrc)\n", rc, rcThread));

    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectDelete(&pAIO->CritSect);
        AssertRC(rc);

        rc = RTSemEventDestroy(pAIO->Event);
        AssertRC(rc);

        pAIO->fStarted  = false;
        pAIO->fShutdown = false;
        pAIO->fEnabled  = false;
    }

    LogFunc(("[SD%RU8]: Returning %Rrc\n", pStream->u8SD, rc));
    return rc;
}

/**
 * Lets the stream's async I/O thread know that there is some data to process.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to notify async I/O thread for.
 */
static int hdaStreamAsyncIONotify(PHDASTATE pThis, PHDASTREAM pStream)
{
    RT_NOREF(pThis);
    return RTSemEventSignal(pStream->State.AIO.Event);
}

/**
 * Locks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStream             HDA stream to lock async I/O thread for.
 */
static void hdaStreamAsyncIOLock(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectEnter(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStream             HDA stream to unlock async I/O thread for.
 */
static void hdaStreamAsyncIOUnlock(PHDASTREAM pStream)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectLeave(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Enables (resumes) or disables (pauses) the async I/O thread.
 *
 * @param   pStream             HDA stream to enable/disable async I/O thread for.
 * @param   fEnable             Whether to enable or disable the I/O thread.
 *
 * @remarks Does not do locking.
 */
static void hdaStreamAsyncIOEnable(PHDASTREAM pStream, bool fEnable)
{
    PHDASTREAMSTATEAIO pAIO = &pStream->State.AIO;
    ASMAtomicXchgBool(&pAIO->fEnabled, fEnable);
}
#endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */

/**
 * Updates a HDA stream according to its usage (input / output).
 *
 * For an SDO (output) stream this means reading DMA data from the device to
 * the connected audio sink(s).
 *
 * For an SDI (input) stream this is reading audio data from the connected
 * audio sink(s) and writing it as DMA data to the device.
 *
 * @returns IPRT status code.
 * @param   pThis               HDA state.
 * @param   pStream             HDA stream to update.
 */
static int hdaStreamUpdate(PHDASTATE pThis, PHDASTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    hdaStreamLock(pStream);

    PHDAMIXERSINK pSink  = pStream->pMixSink;
    AssertPtr(pSink);

    PRTCIRCBUF pCircBuf  = pStream->State.pCircBuf;
    AssertPtr(pCircBuf);

    if (!AudioMixerSinkIsActive(pSink->pMixSink))
    {
        hdaStreamUnlock(pStream);
        return VINF_SUCCESS;
    }

    Log2Func(("[SD%RU8]\n", pStream->u8SD));

    bool fDone = false;
    uint8_t cTransfers = 0;

    while (!fDone)
    {
        int rc2;
        uint32_t cbDMA = 0;

        if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
        {
            STAM_PROFILE_START(&pThis->StatOut, a);

            /*
             * Read from DMA.
             */

            uint8_t abFIFO[HDA_FIFO_MAX + 1];
            size_t  offFIFO = 0;

            /* Do one DMA transfer with FIFOS size at a time. */
            rc2 = hdaStreamDoDMA(pThis, pStream, abFIFO, sizeof(abFIFO), (uint32_t)pStream->u16FIFOS /* cbToProcess */, &cbDMA);
            AssertRC(rc2);

            uint32_t cbDMALeft = cbDMA;

            while (   cbDMALeft
                   && RTCircBufFree(pCircBuf))
            {
                void *pvDst;
                size_t cbDst;

                RTCircBufAcquireWriteBlock(pCircBuf, cbDMALeft, &pvDst, &cbDst);

                if (cbDst)
                {
                    memcpy(pvDst, abFIFO + offFIFO, cbDst);

                    offFIFO += cbDst;
                    Assert(offFIFO <= sizeof(abFIFO));
                }

                RTCircBufReleaseWriteBlock(pCircBuf, cbDst);

                Assert(cbDst <= cbDMALeft);
                cbDMALeft -= (uint32_t)cbDst;
            }

            AssertMsg(cbDMALeft == 0, ("%RU32 bytes of DMA data left, CircBuf=%zu/%zu\n",
                                       cbDMALeft, RTCircBufUsed(pCircBuf), RTCircBufSize(pCircBuf)));

            /*
             * Process backends.
             */

            uint32_t cbUsed = (uint32_t)RTCircBufUsed(pCircBuf);
            if (cbUsed)
            {
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
                /* Let the asynchronous thread know that there is some new data to process. */
                hdaStreamAsyncIONotify(pThis, pStream);
#else
                /* Read audio data from the HDA stream and write to the backends. */
                rc2 = hdaStreamRead(pThis, pStream, cbUsed, NULL /* pcbRead */);
                AssertRC(rc2);
#endif
            }

            /* All DMA transfers done for now? */
            if (   !cbDMA
#ifndef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            /* All data read *and* processed for now? */
                && RTCircBufUsed(pCircBuf) == 0
#endif
               )
            {
                fDone = true;
            }

#ifndef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            rc2 = AudioMixerSinkUpdate(pSink->pMixSink);
            AssertRC(rc2);
#endif
            STAM_PROFILE_STOP(&pThis->StatOut, a);
        }
        else if (hdaGetDirFromSD(pStream->u8SD) == PDMAUDIODIR_IN) /* Input (SDI). */
        {
            STAM_PROFILE_START(&pThis->StatIn, a);

            /*
             * Process backends.
             */

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            /* Let the asynchronous thread know that there is some new data to process. */
            hdaStreamAsyncIONotify(pThis, pStream);
#else
            rc2 = AudioMixerSinkUpdate(pSink->pMixSink);
            AssertRC(rc2);

            /* Write read data from the backend to the HDA stream. */
            rc2 = hdaStreamWrite(pThis, pStream, pStream->u16FIFOS, NULL /* pcbWritten */);
            AssertRC(rc2);
#endif
            /*
             * Write to DMA.
             */

            void *pvSrc;
            size_t cbSrc;

            RTCircBufAcquireReadBlock(pCircBuf, pStream->u16FIFOS, &pvSrc, &cbSrc);

            if (cbSrc)
            {
                /* Do one DMA transfer with FIFOS size at a time. */
                rc2 = hdaStreamDoDMA(pThis, pStream, pvSrc, (uint32_t)cbSrc, (uint32_t)cbSrc /* cbToProcess */, &cbDMA);
                AssertRC(rc2);
            }

            RTCircBufReleaseReadBlock(pCircBuf, cbDMA);

            /* All DMA transfers done for now? */
            if (!cbDMA)
                fDone = true;

            STAM_PROFILE_STOP(&pThis->StatIn, a);
        }
        else
            AssertFailed();

        if (++cTransfers == UINT8_MAX) /* Failsafe counter. */
            fDone = true;

    } /* while !fDone */

#ifdef VBOX_STRICT
    AssertMsg(cTransfers < UINT8_MAX, ("HDA: Update for SD#%RU8 ran for too long\n", pStream->u8SD));
#endif

    Log2Func(("[SD%RU8] End\n", pStream->u8SD));

    hdaStreamUnlock(pStream);

    return VINF_SUCCESS;
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
    RT_NOREF_PV(pvUser);

    /*
     * Look up and log.
     */
    uint32_t        offReg = GCPhysAddr - pThis->MMIOBaseAddr;
    int             idxRegDsc = hdaRegLookup(offReg);    /* Register descriptor index. */
#ifdef LOG_ENABLED
    unsigned const  cbLog     = cb;
    uint32_t        offRegLog = offReg;
#endif

    Log3Func(("offReg=%#x cb=%#x\n", offReg, cb));
    Assert(cb == 4); Assert((offReg & 3) == 0);

    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
        LogFunc(("Access to registers except GCTL is blocked while reset\n"));

    if (idxRegDsc == -1)
        LogRel(("HDA: Invalid read access @0x%x (bytes=%u)\n", offReg, cb));

    if (idxRegDsc != -1)
    {
        /* ASSUMES gapless DWORD at end of map. */
        if (g_aHdaRegMap[idxRegDsc].size == 4)
        {
            /*
             * Straight forward DWORD access.
             */
            rc = g_aHdaRegMap[idxRegDsc].pfnRead(pThis, idxRegDsc, (uint32_t *)pv);
            Log3Func(("\tRead %s => %x (%Rrc)\n", g_aHdaRegMap[idxRegDsc].abbrev, *(uint32_t *)pv, rc));
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
                Log3Func(("\tRead %s[%db] => %x (%Rrc)*\n", g_aHdaRegMap[idxRegDsc].abbrev, cbReg, u32Tmp, rc));
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
        Log3Func(("\tHole at %x is accessed for read\n", offReg));
    }

    /*
     * Log the outcome.
     */
#ifdef LOG_ENABLED
    if (cbLog == 4)
        Log3Func(("\tReturning @%#05x -> %#010x %Rrc\n", offRegLog, *(uint32_t *)pv, rc));
    else if (cbLog == 2)
        Log3Func(("\tReturning @%#05x -> %#06x %Rrc\n", offRegLog, *(uint16_t *)pv, rc));
    else if (cbLog == 1)
        Log3Func(("\tReturning @%#05x -> %#04x %Rrc\n", offRegLog, *(uint8_t *)pv, rc));
#endif
    return rc;
}


DECLINLINE(int) hdaWriteReg(PHDASTATE pThis, int idxRegDsc, uint32_t u32Value, char const *pszLog)
{
    if (pThis->fInReset && idxRegDsc != HDA_REG_GCTL)
    {
        Log(("hdaWriteReg: Warning: Access to %s is blocked while controller is in reset mode\n", g_aHdaRegMap[idxRegDsc].abbrev));
        LogRel2(("HDA: Warning: Access to register %s is blocked while controller is in reset mode\n",
                 g_aHdaRegMap[idxRegDsc].abbrev));
        return VINF_SUCCESS;
    }

    /*
     * Handle RD (register description) flags.
     */

    /* For SDI / SDO: Check if writes to those registers are allowed while SDCTL's RUN bit is set. */
    if (idxRegDsc >= HDA_NUM_GENERAL_REGS)
    {
        const uint32_t uSDCTL = HDA_STREAM_REG(pThis, CTL, HDA_SD_NUM_FROM_REG(pThis, CTL, idxRegDsc));

        /*
         * Some OSes (like Win 10 AU) violate the spec by writing stuff to registers which are not supposed to be be touched
         * while SDCTL's RUN bit is set. So just ignore those values.
         */

            /* Is the RUN bit currently set? */
        if (   RT_BOOL(uSDCTL & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN))
            /* Are writes to the register denied if RUN bit is set? */
            && !(g_aHdaRegMap[idxRegDsc].fFlags & HDA_RD_FLAG_SD_WRITE_RUN))
        {
            Log(("hdaWriteReg: Warning: Access to %s is blocked! %R[sdctl]\n", g_aHdaRegMap[idxRegDsc].abbrev, uSDCTL));
            LogRel2(("HDA: Warning: Access to register %s is blocked while the stream's RUN bit is set\n",
                     g_aHdaRegMap[idxRegDsc].abbrev));
            return VINF_SUCCESS;
        }
    }

#ifdef LOG_ENABLED
    uint32_t const idxRegMem   = g_aHdaRegMap[idxRegDsc].mem_idx;
    uint32_t const u32OldValue = pThis->au32Regs[idxRegMem];
#endif
    int rc = g_aHdaRegMap[idxRegDsc].pfnWrite(pThis, idxRegDsc, u32Value);
    Log3Func(("Written value %#x to %s[%d byte]; %x => %x%s\n", u32Value, g_aHdaRegMap[idxRegDsc].abbrev,
              g_aHdaRegMap[idxRegDsc].size, u32OldValue, pThis->au32Regs[idxRegMem], pszLog));
    RT_NOREF(pszLog);
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE, Looks up and calls the appropriate handler.}
 */
PDMBOTHCBDECL(int) hdaMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PHDASTATE pThis  = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int       rc;
    RT_NOREF_PV(pvUser);

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
    int         idxRegDsc = hdaRegLookup(offReg);
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
        Log3Func(("@%#05x u32=%#010x cb=%d\n", offReg, *(uint32_t const *)pv, cb));
    else if (cb == 4)
        Log3Func(("@%#05x u32=%#010x %s\n", offReg, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 2)
        Log3Func(("@%#05x u16=%#06x (%#010x) %s\n", offReg, *(uint16_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));
    else if (cb == 1)
        Log3Func(("@%#05x u8=%#04x (%#010x) %s\n", offReg, *(uint8_t *)pv, *(uint32_t *)pv, g_aHdaRegMap[idxRegDsc].abbrev));

    if (idxRegDsc >= 0 && g_aHdaRegMap[idxRegDsc].size != cb)
        Log3Func(("\tsize=%RU32 != cb=%u!!\n", g_aHdaRegMap[idxRegDsc].size, cb));
#endif

    /*
     * Try for a direct hit first.
     */
    if (idxRegDsc != -1 && g_aHdaRegMap[idxRegDsc].size == cb)
    {
        rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "");
        Log3Func(("\t%#x -> %#x\n", u32LogOldValue, idxRegMem != UINT32_MAX ? pThis->au32Regs[idxRegMem] : UINT32_MAX));
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
        if (idxRegDsc == -1 && (idxRegDsc = hdaRegLookupWithin(offReg)) != -1)
        {
            uint32_t const cbBefore = offReg - g_aHdaRegMap[idxRegDsc].offset; Assert(cbBefore > 0 && cbBefore < 4);
            offReg    -= cbBefore;
            idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
            u64Value <<= cbBefore * 8;
            u64Value  |= pThis->au32Regs[idxRegMem] & g_afMasks[cbBefore];
            Log3Func(("\tWithin register, supplied %u leading bits: %#llx -> %#llx ...\n",
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
                    Log3Func(("\tSupplying missing bits (%#x): %#llx -> %#llx ...\n",
                              g_afMasks[cbReg] & ~g_afMasks[cb], u64Value & g_afMasks[cb], u64Value));
                }
#ifdef LOG_ENABLED
                uint32_t uLogOldVal = pThis->au32Regs[idxRegMem];
#endif
                rc = hdaWriteReg(pThis, idxRegDsc, u64Value, "*");
                Log3Func(("\t%#x -> %#x\n", uLogOldVal, pThis->au32Regs[idxRegMem]));
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
                idxRegDsc = hdaRegLookup(offReg);
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
static DECLCALLBACK(int)  hdaPciIoRegionMap(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                            RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF(iRegion, enmType);
    PHDASTATE   pThis = RT_FROM_MEMBER(pPciDev, HDASTATE, PciDev);

    /*
     * 18.2 of the ICH6 datasheet defines the valid access widths as byte, word, and double word.
     *
     * Let IOM talk DWORDs when reading, saves a lot of complications. On
     * writing though, we have to do it all ourselves because of sideeffects.
     */
    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    int rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_PASSTHRU,
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
    RT_NOREF(pDevIns);
#ifdef DEBUG
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
#endif
    LogFlowFunc(("[SD%RU8]\n", pStrm->u8SD));

    /* Save stream ID. */
    int rc = SSMR3PutU8(pSSM, pStrm->u8SD);
    AssertRCReturn(rc, rc);
    Assert(pStrm->u8SD <= HDA_MAX_STREAMS);

    rc = SSMR3PutStructEx(pSSM, &pStrm->State, sizeof(HDASTREAMSTATE), 0 /*fFlags*/, g_aSSMStreamStateFields6, NULL);
    AssertRCReturn(rc, rc);

#ifdef DEBUG /* Sanity checks. */
    uint64_t u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, pStrm->u8SD),
                                      HDA_STREAM_REG(pThis, BDPU, pStrm->u8SD));
    uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, pStrm->u8SD);
    uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, pStrm->u8SD);

    hdaBDLEDumpAll(pThis, u64BaseDMA, u16LVI + 1);

    Assert(u64BaseDMA == pStrm->u64BDLBase);
    Assert(u16LVI     == pStrm->u16LVI);
    Assert(u32CBL     == pStrm->u32CBL);
#endif

    rc = SSMR3PutStructEx(pSSM, &pStrm->State.BDLE.Desc, sizeof(HDABDLEDESC),
                          0 /*fFlags*/, g_aSSMBDLEDescFields6, NULL);
    AssertRCReturn(rc, rc);

    rc = SSMR3PutStructEx(pSSM, &pStrm->State.BDLE.State, sizeof(HDABDLESTATE),
                          0 /*fFlags*/, g_aSSMBDLEStateFields6, NULL);
    AssertRCReturn(rc, rc);

#ifdef DEBUG /* Sanity checks. */
    PHDABDLE pBDLE = &pStrm->State.BDLE;
    if (u64BaseDMA)
    {
        Assert(pStrm->State.uCurBDLE <= u16LVI + 1);

        HDABDLE curBDLE;
        rc = hdaBDLEFetch(pThis, &curBDLE, u64BaseDMA, pStrm->State.uCurBDLE);
        AssertRC(rc);

        Assert(curBDLE.Desc.u32BufSize == pBDLE->Desc.u32BufSize);
        Assert(curBDLE.Desc.u64BufAdr  == pBDLE->Desc.u64BufAdr);
        Assert(curBDLE.Desc.fFlags     == pBDLE->Desc.fFlags);
    }
    else
    {
        Assert(pBDLE->Desc.u64BufAdr  == 0);
        Assert(pBDLE->Desc.u32BufSize == 0);
    }
#endif
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
    SSMR3PutU32(pSSM, RT_ELEMENTS(pThis->au32Regs));
    SSMR3PutMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));

    /* Save number of streams. */
    SSMR3PutU32(pSSM, HDA_MAX_STREAMS);

    /* Save stream states. */
    for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
    {
        int rc = hdaSaveStream(pDevIns, pSSM, &pThis->aStreams[i]);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) hdaLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    LogRel2(("hdaLoadExec: uVersion=%RU32, uPass=0x%x\n", uVersion, uPass));

    /*
     * Load Codec nodes states.
     */
    int rc = hdaCodecLoadState(pThis->pCodec, pSSM, uVersion);
    if (RT_FAILURE(rc))
    {
        LogRel(("HDA: Failed loading codec state (version %RU32, pass 0x%x), rc=%Rrc\n", uVersion, uPass, rc));
        return rc;
    }

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
            AssertCompile(RT_ELEMENTS(pThis->au32Regs) >= 112);
            break;

        /* Since version 4 we store the register count to stay flexible. */
        case HDA_SSM_VERSION_4:
        case HDA_SSM_VERSION_5:
        case HDA_SSM_VERSION:
            rc = SSMR3GetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
            if (cRegs != RT_ELEMENTS(pThis->au32Regs))
                LogRel(("HDA: SSM version cRegs is %RU32, expected %RU32\n", cRegs, RT_ELEMENTS(pThis->au32Regs)));
            break;

        default:
            LogRel(("HDA: Unsupported / too new saved state version (%RU32)\n", uVersion));
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
#define HDA_SSM_LOAD_BDLE_STATE_PRE_V5(v, x)                                \
    {                                                                       \
        rc = SSMR3Skip(pSSM, sizeof(uint32_t));        /* Begin marker */   \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3GetU64(pSSM, &x.Desc.u64BufAdr);     /* u64BdleCviAddr */ \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3Skip(pSSM, sizeof(uint32_t));        /* u32BdleMaxCvi */  \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3GetU32(pSSM, &x.State.u32BDLIndex);  /* u32BdleCvi */     \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3GetU32(pSSM, &x.Desc.u32BufSize);    /* u32BdleCviLen */  \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3GetU32(pSSM, &x.State.u32BufOff);    /* u32BdleCviPos */  \
        AssertRCReturn(rc, rc);                                             \
        bool fIOC;                                                          \
        rc = SSMR3GetBool(pSSM, &fIOC);                /* fBdleCviIoc */    \
        AssertRCReturn(rc, rc);                                             \
        x.Desc.fFlags = fIOC ? HDA_BDLE_FLAG_IOC : 0;                       \
        rc = SSMR3GetU32(pSSM, &x.State.cbBelowFIFOW); /* cbUnderFifoW */   \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3Skip(pSSM, sizeof(uint8_t) * 256);   /* FIFO */           \
        AssertRCReturn(rc, rc);                                             \
        rc = SSMR3Skip(pSSM, sizeof(uint32_t));        /* End marker */     \
        AssertRCReturn(rc, rc);                                             \
    }                                                                       \

    /*
     * Load BDLEs (Buffer Descriptor List Entries) and DMA counters.
     */
    switch (uVersion)
    {
        case HDA_SSM_VERSION_1:
        case HDA_SSM_VERSION_2:
        case HDA_SSM_VERSION_3:
        case HDA_SSM_VERSION_4:
        {
            /* Only load the internal states.
             * The rest will be initialized from the saved registers later. */

            /* Note 1: Only the *current* BDLE for a stream was saved! */
            /* Note 2: The stream's saving order is/was fixed, so don't touch! */

            /* Output */
            PHDASTREAM pStream = &pThis->aStreams[4];
            rc = hdaStreamInit(pThis, pStream, 4 /* Stream descriptor, hardcoded */);
            if (RT_FAILURE(rc))
                break;
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, pStream->State.BDLE);
            pStream->State.uCurBDLE = pStream->State.BDLE.State.u32BDLIndex;

            /* Microphone-In */
            pStream = &pThis->aStreams[2];
            rc = hdaStreamInit(pThis, pStream, 2 /* Stream descriptor, hardcoded */);
            if (RT_FAILURE(rc))
                break;
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, pStream->State.BDLE);
            pStream->State.uCurBDLE = pStream->State.BDLE.State.u32BDLIndex;

            /* Line-In */
            pStream = &pThis->aStreams[0];
            rc = hdaStreamInit(pThis, pStream, 0 /* Stream descriptor, hardcoded */);
            if (RT_FAILURE(rc))
                break;
            HDA_SSM_LOAD_BDLE_STATE_PRE_V5(uVersion, pStream->State.BDLE);
            pStream->State.uCurBDLE = pStream->State.BDLE.State.u32BDLIndex;
            break;
        }

        /* Since v5 we support flexible stream and BDLE counts. */
        case HDA_SSM_VERSION_5:
        case HDA_SSM_VERSION:
        {
            uint32_t cStreams;
            rc = SSMR3GetU32(pSSM, &cStreams);
            if (RT_FAILURE(rc))
                break;

            LogRel2(("hdaLoadExec: cStreams=%RU32\n", cStreams));

            /* Load stream states. */
            for (uint32_t i = 0; i < cStreams; i++)
            {
                uint8_t uSD;
                rc = SSMR3GetU8(pSSM, &uSD);
                if (RT_FAILURE(rc))
                    break;

                PHDASTREAM pStrm = hdaStreamGetFromSD(pThis, uSD);
                HDASTREAM  StreamDummy;

                if (!pStrm)
                {
                    RT_ZERO(StreamDummy);
                    pStrm = &StreamDummy;
                    LogRel2(("HDA: Warning: Stream ID=%RU32 not supported, skipping to load ...\n", uSD));
                    break;
                }

                rc = hdaStreamInit(pThis, pStrm, uSD);
                if (RT_FAILURE(rc))
                {
                    LogRel(("HDA: Stream #%RU32: Initialization of stream %RU8 failed, rc=%Rrc\n", i, uSD, rc));
                    break;
                }

                if (uVersion == HDA_SSM_VERSION_5)
                {
                    /* Get the current BDLE entry and skip the rest. */
                    uint16_t cBDLE;

                    rc = SSMR3Skip(pSSM, sizeof(uint32_t)); /* Begin marker */
                    AssertRC(rc);
                    rc = SSMR3GetU16(pSSM, &cBDLE);                 /* cBDLE */
                    AssertRC(rc);
                    rc = SSMR3GetU16(pSSM, &pStrm->State.uCurBDLE); /* uCurBDLE */
                    AssertRC(rc);
                    rc = SSMR3Skip(pSSM, sizeof(uint32_t));         /* End marker */
                    AssertRC(rc);

                    uint32_t u32BDLEIndex;
                    for (uint16_t a = 0; a < cBDLE; a++)
                    {
                        rc = SSMR3Skip(pSSM, sizeof(uint32_t)); /* Begin marker */
                        AssertRC(rc);
                        rc = SSMR3GetU32(pSSM, &u32BDLEIndex);  /* u32BDLIndex */
                        AssertRC(rc);

                        /* Does the current BDLE index match the current BDLE to process? */
                        if (u32BDLEIndex == pStrm->State.uCurBDLE)
                        {
                            rc = SSMR3GetU32(pSSM, &pStrm->State.BDLE.State.cbBelowFIFOW); /* cbBelowFIFOW */
                            AssertRC(rc);
                            rc = SSMR3Skip(pSSM, sizeof(uint8_t) * 256);                   /* FIFO, deprecated */
                            AssertRC(rc);
                            rc = SSMR3GetU32(pSSM, &pStrm->State.BDLE.State.u32BufOff);    /* u32BufOff */
                            AssertRC(rc);
                            rc = SSMR3Skip(pSSM, sizeof(uint32_t));                        /* End marker */
                            AssertRC(rc);
                        }
                        else /* Skip not current BDLEs. */
                        {
                            rc = SSMR3Skip(pSSM,   sizeof(uint32_t)      /* cbBelowFIFOW */
                                                 + sizeof(uint8_t) * 256 /* FIFO, deprecated */
                                                 + sizeof(uint32_t)      /* u32BufOff */
                                                 + sizeof(uint32_t));    /* End marker */
                            AssertRC(rc);
                        }
                    }
                }
                else
                {
                    rc = SSMR3GetStructEx(pSSM, &pStrm->State, sizeof(HDASTREAMSTATE),
                                          0 /* fFlags */, g_aSSMStreamStateFields6, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    rc = SSMR3GetStructEx(pSSM, &pStrm->State.BDLE.Desc, sizeof(HDABDLEDESC),
                                          0 /* fFlags */, g_aSSMBDLEDescFields6, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    rc = SSMR3GetStructEx(pSSM, &pStrm->State.BDLE.State, sizeof(HDABDLESTATE),
                                          0 /* fFlags */, g_aSSMBDLEStateFields6, NULL);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
            break;
        }

        default:
            AssertReleaseFailed(); /* Never reached. */
            return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

#undef HDA_SSM_LOAD_BDLE_STATE_PRE_V5

    if (RT_SUCCESS(rc))
    {
        pThis->u64CORBBase  = RT_MAKE_U64(HDA_REG(pThis, CORBLBASE), HDA_REG(pThis, CORBUBASE));
        pThis->u64RIRBBase  = RT_MAKE_U64(HDA_REG(pThis, RIRBLBASE), HDA_REG(pThis, RIRBUBASE));
        pThis->u64DPBase    = RT_MAKE_U64(HDA_REG(pThis, DPLBASE),   HDA_REG(pThis, DPUBASE));

        /* Also make sure to update the DMA position bit if this was enabled when saving the state. */
        pThis->fDMAPosition = RT_BOOL(pThis->u64DPBase & RT_BIT_64(0));
    }

    if (RT_SUCCESS(rc))
    {
        for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
        {
            PHDASTREAM pStream = hdaStreamGetFromSD(pThis, i);
            if (pStream)
            {
                hdaStreamEnable(pThis, pStream, false /* fEnable */);

                bool fActive = RT_BOOL(HDA_STREAM_REG(pThis, CTL, i) & HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN));
                if (fActive)
                {
                    int rc2 = hdaStreamEnable(pThis, pStream, true /* fEnable */);
                    AssertRC(rc2);
                }
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("HDA: Failed loading device state (version %RU32, pass 0x%x), rc=%Rrc\n", uVersion, uPass, rc));

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
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    PHDABDLE pBDLE = (PHDABDLE)pvValue;
    return RTStrFormat(pfnOutput,  pvArgOutput, NULL, 0,
                       "BDLE(idx:%RU32, off:%RU32, fifow:%RU32, IOC:%RTbool, DMA[%RU32 bytes @ 0x%x])",
                       pBDLE->State.u32BDLIndex, pBDLE->State.u32BufOff, pBDLE->State.cbBelowFIFOW,
                       pBDLE->Desc.fFlags & HDA_BDLE_FLAG_IOC, pBDLE->Desc.u32BufSize, pBDLE->Desc.u64BufAdr);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDCTL(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                           const char *pszType, void const *pvValue,
                                           int cchWidth, int cchPrecision, unsigned fFlags,
                                           void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
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
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSDFIFOS = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOS(raw:%#x, sdfifos:%RU8 B)", uSDFIFOS, uSDFIFOS ? uSDFIFOS + 1 : 0);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaDbgFmtSDFIFOW(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                             const char *pszType, void const *pvValue,
                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                             void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
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
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSdSts = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDSTS(raw:%#0x, fifordy:%RTbool, dese:%RTbool, fifoe:%RTbool, bcis:%RTbool)",
                       uSdSts,
                       RT_BOOL(uSdSts & HDA_SDSTS_FIFORDY),
                       RT_BOOL(uSdSts & HDA_SDSTS_DESE),
                       RT_BOOL(uSdSts & HDA_SDSTS_FIFOE),
                       RT_BOOL(uSdSts & HDA_SDSTS_BCIS));
}

static int hdaDbgLookupRegByName(const char *pszArgs)
{
    int iReg = 0;
    for (; iReg < HDA_NUM_REGS; ++iReg)
        if (!RTStrICmp(g_aHdaRegMap[iReg].abbrev, pszArgs))
            return iReg;
    return -1;
}


static void hdaDbgPrintRegister(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iHdaIndex)
{
    Assert(   pThis
           && iHdaIndex >= 0
           && iHdaIndex < HDA_NUM_REGS);
    pHlp->pfnPrintf(pHlp, "%s: 0x%x\n", g_aHdaRegMap[iHdaIndex].abbrev, pThis->au32Regs[g_aHdaRegMap[iHdaIndex].mem_idx]);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaDbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int iHdaRegisterIndex = hdaDbgLookupRegByName(pszArgs);
    if (iHdaRegisterIndex != -1)
        hdaDbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
    else
    {
        for(iHdaRegisterIndex = 0; (unsigned int)iHdaRegisterIndex < HDA_NUM_REGS; ++iHdaRegisterIndex)
            hdaDbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
    }
}

static void hdaDbgPrintStream(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iIdx)
{
    Assert(   pThis
           && iIdx >= 0
           && iIdx < HDA_MAX_STREAMS);

    const PHDASTREAM pStrm = &pThis->aStreams[iIdx];

    pHlp->pfnPrintf(pHlp, "Stream #%d:\n", iIdx);
    pHlp->pfnPrintf(pHlp, "\tSD%dCTL  : %R[sdctl]\n",   iIdx, HDA_STREAM_REG(pThis, CTL,   iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dCTS  : %R[sdsts]\n",   iIdx, HDA_STREAM_REG(pThis, STS,   iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dFIFOS: %R[sdfifos]\n", iIdx, HDA_STREAM_REG(pThis, FIFOS, iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dFIFOW: %R[sdfifow]\n", iIdx, HDA_STREAM_REG(pThis, FIFOW, iIdx));
    pHlp->pfnPrintf(pHlp, "\tBDLE     : %R[bdle]\n",    &pStrm->State.BDLE);
}

static void hdaDbgPrintBDLE(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iIdx)
{
    Assert(   pThis
           && iIdx >= 0
           && iIdx < HDA_MAX_STREAMS);

    const PHDASTREAM pStrm = &pThis->aStreams[iIdx];
    const PHDABDLE   pBDLE = &pStrm->State.BDLE;

    pHlp->pfnPrintf(pHlp, "Stream #%d BDLE:\n",      iIdx);

    uint64_t u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, iIdx),
                                      HDA_STREAM_REG(pThis, BDPU, iIdx));
    uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, iIdx);
    uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, iIdx);

    if (!u64BaseDMA)
        return;

    pHlp->pfnPrintf(pHlp, "\tCurrent: %R[bdle]\n\n", pBDLE);

    pHlp->pfnPrintf(pHlp, "\tMemory:\n");

    uint32_t cbBDLE = 0;
    for (uint16_t i = 0; i < u16LVI + 1; i++)
    {
        HDABDLEDESC bd;
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), u64BaseDMA + i * sizeof(HDABDLEDESC), &bd, sizeof(bd));

        pHlp->pfnPrintf(pHlp, "\t\t%s #%03d BDLE(adr:0x%llx, size:%RU32, ioc:%RTbool)\n",
                        pBDLE->State.u32BDLIndex == i ? "*" : " ", i, bd.u64BufAdr, bd.u32BufSize, bd.fFlags & HDA_BDLE_FLAG_IOC);

        cbBDLE += bd.u32BufSize;
    }

    pHlp->pfnPrintf(pHlp, "Total: %RU32 bytes\n", cbBDLE);

    if (cbBDLE != u32CBL)
        pHlp->pfnPrintf(pHlp, "Warning: %RU32 bytes does not match CBL (%RU32)!\n", cbBDLE, u32CBL);

    pHlp->pfnPrintf(pHlp, "DMA counters (base @ 0x%llx):\n", u64BaseDMA);
    if (!u64BaseDMA) /* No DMA base given? Bail out. */
    {
        pHlp->pfnPrintf(pHlp, "\tNo counters found\n");
        return;
    }

    for (int i = 0; i < u16LVI + 1; i++)
    {
        uint32_t uDMACnt;
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), (pThis->u64DPBase & DPBASE_ADDR_MASK) + (i * 2 * sizeof(uint32_t)),
                          &uDMACnt, sizeof(uDMACnt));

        pHlp->pfnPrintf(pHlp, "\t#%03d DMA @ 0x%x\n", i , uDMACnt);
    }
}

static int hdaDbgLookupStrmIdx(PHDASTATE pThis, const char *pszArgs)
{
    RT_NOREF(pThis, pszArgs);
    /** @todo Add args parsing. */
    return -1;
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaDbgInfoStream(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE   pThis         = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int         iHdaStreamdex = hdaDbgLookupStrmIdx(pThis, pszArgs);
    if (iHdaStreamdex != -1)
        hdaDbgPrintStream(pThis, pHlp, iHdaStreamdex);
    else
        for(iHdaStreamdex = 0; iHdaStreamdex < HDA_MAX_STREAMS; ++iHdaStreamdex)
            hdaDbgPrintStream(pThis, pHlp, iHdaStreamdex);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaDbgInfoBDLE(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE   pThis         = PDMINS_2_DATA(pDevIns, PHDASTATE);
    int         iHdaStreamdex = hdaDbgLookupStrmIdx(pThis, pszArgs);
    if (iHdaStreamdex != -1)
        hdaDbgPrintBDLE(pThis, pHlp, iHdaStreamdex);
    else
        for(iHdaStreamdex = 0; iHdaStreamdex < HDA_MAX_STREAMS; ++iHdaStreamdex)
            hdaDbgPrintBDLE(pThis, pHlp, iHdaStreamdex);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaDbgInfoCodecNodes(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
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
static DECLCALLBACK(void) hdaDbgInfoCodecSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
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
static DECLCALLBACK(void) hdaDbgInfoMixer(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
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

    LogFlowFuncEnter();

# ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    /*
     * Stop the timer, if any.
     */
    hdaTimerStop(pThis);

    pThis->cStreamsActive = 0;
# endif

    /* See 6.2.1. */
    HDA_REG(pThis, GCAP)     = HDA_MAKE_GCAP(HDA_MAX_SDO /* Ouput streams */,
                                             HDA_MAX_SDI /* Input streams */,
                                             0           /* Bidirectional output streams */,
                                             0           /* Serial data out signals */,
                                             1           /* 64-bit */);
    HDA_REG(pThis, VMIN)     = 0x00;                     /* see 6.2.2 */
    HDA_REG(pThis, VMAJ)     = 0x01;                     /* see 6.2.3 */
    /* Announce the full 60 words output payload. */
    HDA_REG(pThis, OUTPAY)   = 0x003C;                   /* see 6.2.4 */
    /* Announce the full 29 words input payload. */
    HDA_REG(pThis, INPAY)    = 0x001D;                   /* see 6.2.5 */
    HDA_REG(pThis, CORBSIZE) = 0x42;                     /* see 6.2.1 */
    HDA_REG(pThis, RIRBSIZE) = 0x42;                     /* see 6.2.1 */
    HDA_REG(pThis, CORBRP)   = 0x0;
    HDA_REG(pThis, RIRBWP)   = 0x0;

    /*
     * Stop any audio currently playing and/or recording.
     */
    if (pThis->SinkFront.pMixSink)
        AudioMixerSinkReset(pThis->SinkFront.pMixSink);
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    if (pThis->SinkMicIn.pMixSink)
        AudioMixerSinkReset(pThis->SinkMicIn.pMixSink);
# endif
    if (pThis->SinkLineIn.pMixSink)
        AudioMixerSinkReset(pThis->SinkLineIn.pMixSink);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    if (pThis->SinkCenterLFE.pMixSink)
        AudioMixerSinkReset(pThis->SinkCenterLFE.pMixSink);
    if (pThis->SinkRear.pMixSink)
        AudioMixerSinkReset(pThis->SinkRear.pMixSink);
# endif

    /*
     * Reset the codec.
     */
    if (   pThis->pCodec
        && pThis->pCodec->pfnReset)
    {
        pThis->pCodec->pfnReset(pThis->pCodec);
    }

    /*
     * Set some sensible defaults for which HDA sinks
     * are connected to which stream number.
     *
     * We use SD0 for input and SD4 for output by default.
     * These stream numbers can be changed by the guest dynamically lateron.
     */
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    hdaMixerSetStream(pThis, PDMAUDIOMIXERCTL_MIC_IN    , 1 /* SD0 */, 0 /* Channel */);
#endif
    hdaMixerSetStream(pThis, PDMAUDIOMIXERCTL_LINE_IN   , 1 /* SD0 */, 0 /* Channel */);

    hdaMixerSetStream(pThis, PDMAUDIOMIXERCTL_FRONT     , 5 /* SD4 */, 0 /* Channel */);
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    hdaMixerSetStream(pThis, PDMAUDIOMIXERCTL_CENTER_LFE, 5 /* SD4 */, 0 /* Channel */);
    hdaMixerSetStream(pThis, PDMAUDIOMIXERCTL_REAR      , 5 /* SD4 */, 0 /* Channel */);
#endif

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

    for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
    {
        /* Remove the RUN bit from SDnCTL in case the stream was in a running state before. */
        HDA_STREAM_REG(pThis, CTL, i) &= ~HDA_REG_FIELD_FLAG_MASK(SDCTL, RUN);
        hdaStreamReset(pThis, &pThis->aStreams[i]);
    }

    /* Clear stream tags <-> objects mapping table. */
    RT_ZERO(pThis->aTags);

    /* Emulation of codec "wake up" (HDA spec 5.5.1 and 6.5). */
    HDA_REG(pThis, STATESTS) = 0x1;

    LogFlowFuncLeave();
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

    if (pThis->pCodec)
    {
        hdaCodecDestruct(pThis->pCodec);

        RTMemFree(pThis->pCodec);
        pThis->pCodec = NULL;
    }

    RTMemFree(pThis->pu32CorbBuf);
    pThis->pu32CorbBuf = NULL;

    RTMemFree(pThis->pu64RirbBuf);
    pThis->pu64RirbBuf = NULL;

    for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
        hdaStreamDestroy(pThis, &pThis->aStreams[i]);

    return VINF_SUCCESS;
}


/**
 * Attach command, internal version.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor has to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pDrv        Driver to (re-)use for (re-)attaching to.
 *                      If NULL is specified, a new driver will be created and appended
 *                      to the driver list.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static int hdaAttachInternal(PPDMDEVINS pDevIns, PHDADRIVER pDrv, unsigned uLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    /*
     * Attach driver.
     */
    char *pszDesc = NULL;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (HDA) for LUN#%u", uLUN) <= 0)
        AssertReleaseMsgReturn(pszDesc,
                               ("Not enough memory for HDA driver port description of LUN #%u\n", uLUN),
                               VERR_NO_MEMORY);

    PPDMIBASE pDrvBase;
    int rc = PDMDevHlpDriverAttach(pDevIns, uLUN,
                                   &pThis->IBase, &pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        if (pDrv == NULL)
            pDrv = (PHDADRIVER)RTMemAllocZ(sizeof(HDADRIVER));
        if (pDrv)
        {
            pDrv->pDrvBase   = pDrvBase;
            pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL, ("Configuration error: LUN#%u has no host audio interface, rc=%Rrc\n", uLUN, rc));
            pDrv->pHDAState  = pThis;
            pDrv->uLUN       = uLUN;

            /*
             * For now we always set the driver at LUN 0 as our primary
             * host backend. This might change in the future.
             */
            if (pDrv->uLUN == 0)
                pDrv->fFlags |= PDMAUDIODRVFLAGS_PRIMARY;

            LogFunc(("LUN#%u: pCon=%p, drvFlags=0x%x\n", uLUN, pDrv->pConnector, pDrv->fFlags));

            /* Attach to driver list if not attached yet. */
            if (!pDrv->fAttached)
            {
                RTListAppend(&pThis->lstDrv, &pDrv->Node);
                pDrv->fAttached = true;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        LogFunc(("No attached driver for LUN #%u\n", uLUN));

    if (RT_FAILURE(rc))
    {
        /* Only free this string on failure;
         * must remain valid for the live of the driver instance. */
        RTStrFree(pszDesc);
    }

    LogFunc(("uLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}

/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor has to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int) hdaAttach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    return hdaAttachInternal(pDevIns, NULL /* pDrv */, uLUN, fFlags);
}

static DECLCALLBACK(void) hdaDetach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    RT_NOREF(pDevIns, uLUN, fFlags);
    LogFunc(("iLUN=%u, fFlags=0x%x\n", uLUN, fFlags));
}

/**
 * Powers off the device.
 *
 * @param   pDevIns             Device instance to power off.
 */
static DECLCALLBACK(void) hdaPowerOff(PPDMDEVINS pDevIns)
{
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);

    LogRel2(("HDA: Powering off ...\n"));

    /* Ditto goes for the codec, which in turn uses the mixer. */
    hdaCodecPowerOff(pThis->pCodec);

    /**
     * Note: Destroy the mixer while powering off and *not* in hdaDestruct,
     *       giving the mixer the chance to release any references held to
     *       PDM audio streams it maintains.
     */
    if (pThis->pMixer)
    {
        AudioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }
}

/**
 * Re-attaches a new driver to the device's driver chain.
 *
 * @returns VBox status code.
 * @param   pThis       Device instance to re-attach driver to.
 * @param   pDrv        Driver instance used for attaching to.
 *                      If NULL is specified, a new driver will be created and appended
 *                      to the driver list.
 * @param   uLUN        The logical unit which is being re-detached.
 * @param   pszDriver   Driver name.
 */
static int hdaReattach(PHDASTATE pThis, PHDADRIVER pDrv, uint8_t uLUN, const char *pszDriver)
{
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pszDriver, VERR_INVALID_POINTER);

    PVM pVM = PDMDevHlpGetVM(pThis->pDevInsR3);
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    PCFGMNODE pDev0 = CFGMR3GetChild(pRoot, "Devices/hda/0/");

    /* Remove LUN branch. */
    CFGMR3RemoveNode(CFGMR3GetChildF(pDev0, "LUN#%u/", uLUN));

    if (pDrv)
    {
        /* Re-use a driver instance => detach the driver before. */
        int rc = PDMDevHlpDriverDetach(pThis->pDevInsR3, PDMIBASE_2_PDMDRV(pDrv->pDrvBase), 0 /* fFlags */);
        if (RT_FAILURE(rc))
            return rc;
    }

#define RC_CHECK() if (RT_FAILURE(rc)) { AssertReleaseRC(rc); break; }

    int rc = VINF_SUCCESS;
    do
    {
        PCFGMNODE pLunL0;
        rc = CFGMR3InsertNodeF(pDev0, &pLunL0, "LUN#%u/", uLUN);        RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",       "AUDIO");       RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config/",       NULL);         RC_CHECK();

        PCFGMNODE pLunL1, pLunL2;
        rc = CFGMR3InsertNode  (pLunL0, "AttachedDriver/", &pLunL1);    RC_CHECK();
        rc = CFGMR3InsertNode  (pLunL1,  "Config/",        &pLunL2);    RC_CHECK();
        rc = CFGMR3InsertString(pLunL1,  "Driver",          pszDriver); RC_CHECK();

        rc = CFGMR3InsertString(pLunL2, "AudioDriver", pszDriver);      RC_CHECK();

    } while (0);

    if (RT_SUCCESS(rc))
        rc = hdaAttachInternal(pThis->pDevInsR3, pDrv, uLUN, 0 /* fFlags */);

    LogFunc(("pThis=%p, uLUN=%u, pszDriver=%s, rc=%Rrc\n", pThis, uLUN, pszDriver, rc));

#undef RC_CHECK

    return rc;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hdaConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RT_NOREF(iInstance);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PHDASTATE pThis = PDMINS_2_DATA(pDevIns, PHDASTATE);
    Assert(iInstance == 0);

    /*
     * Validations.
     */
    if (!CFGMR3AreValuesValid(pCfg, "R0Enabled\0"
                                    "RCEnabled\0"
                                    "TimerHz\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_ ("Invalid configuration for the Intel HDA device"));

    int rc = CFGMR3QueryBoolDef(pCfg, "RCEnabled", &pThis->fRCEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read RCEnabled as boolean"));
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read R0Enabled as boolean"));
#ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    uint16_t uTimerHz;
    rc = CFGMR3QueryU16Def(pCfg, "TimerHz", &uTimerHz, HDA_TIMER_HZ /* Default value, if not set. */);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read Hertz (Hz) rate as unsigned integer"));
#endif

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

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    LogRel(("HDA: Asynchronous I/O enabled\n"));
#endif

    uint8_t uLUN;
    for (uLUN = 0; uLUN < UINT8_MAX; ++uLUN)
    {
        LogFunc(("Trying to attach driver for LUN #%RU32 ...\n", uLUN));
        rc = hdaAttachInternal(pDevIns, NULL /* pDrv */, uLUN, 0 /* fFlags */);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
                rc = VINF_SUCCESS;
            else if (rc == VERR_AUDIO_BACKEND_INIT_FAILED)
            {
                hdaReattach(pThis, NULL /* pDrv */, uLUN, "NullAudio");
                PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                    N_("Host audio backend initialization has failed. Selecting the NULL audio backend "
                       "with the consequence that no sound is audible"));
                /* Attaching to the NULL audio backend will never fail. */
                rc = VINF_SUCCESS;
            }
            break;
        }
    }

    LogFunc(("cLUNs=%RU8, rc=%Rrc\n", uLUN, rc));

    if (RT_SUCCESS(rc))
    {
        rc = AudioMixerCreate("HDA Mixer", 0 /* uFlags */, &pThis->pMixer);
        if (RT_SUCCESS(rc))
        {
            /*
             * Add mixer output sinks.
             */
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
            rc = AudioMixerCreateSink(pThis->pMixer, "[Playback] Front",
                                      AUDMIXSINKDIR_OUTPUT, &pThis->SinkFront.pMixSink);
            AssertRC(rc);
            rc = AudioMixerCreateSink(pThis->pMixer, "[Playback] Center / Subwoofer",
                                      AUDMIXSINKDIR_OUTPUT, &pThis->SinkCenterLFE.pMixSink);
            AssertRC(rc);
            rc = AudioMixerCreateSink(pThis->pMixer, "[Playback] Rear",
                                      AUDMIXSINKDIR_OUTPUT, &pThis->SinkRear.pMixSink);
            AssertRC(rc);
#else
            rc = AudioMixerCreateSink(pThis->pMixer, "[Playback] PCM Output",
                                      AUDMIXSINKDIR_OUTPUT, &pThis->SinkFront.pMixSink);
            AssertRC(rc);
#endif
            /*
             * Add mixer input sinks.
             */
            rc = AudioMixerCreateSink(pThis->pMixer, "[Recording] Line In",
                                      AUDMIXSINKDIR_INPUT, &pThis->SinkLineIn.pMixSink);
            AssertRC(rc);
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
            rc = AudioMixerCreateSink(pThis->pMixer, "[Recording] Microphone In",
                                      AUDMIXSINKDIR_INPUT, &pThis->SinkMicIn.pMixSink);
            AssertRC(rc);
#endif
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

        /* Set codec callbacks to this controller. */
        pThis->pCodec->pfnCbMixerAddStream    = hdaMixerAddStream;
        pThis->pCodec->pfnCbMixerRemoveStream = hdaMixerRemoveStream;
        pThis->pCodec->pfnCbMixerSetStream    = hdaMixerSetStream;
        pThis->pCodec->pfnCbMixerSetVolume    = hdaMixerSetVolume;

        pThis->pCodec->pHDAState = pThis; /* Assign HDA controller state to codec. */

        /* Construct the codec. */
        rc = hdaCodecConstruct(pDevIns, pThis->pCodec, 0 /* Codec index */, pCfg);
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
        /*
         * Create all hardware streams.
         */
        for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
        {
            rc = hdaStreamCreate(pThis, &pThis->aStreams[i], i /* uSD */);
            AssertRC(rc);
        }

#ifdef VBOX_WITH_AUDIO_HDA_ONETIME_INIT
        /*
         * Initialize the driver chain.
         */
        PHDADRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, HDADRIVER, Node)
        {
            /*
             * Only primary drivers are critical for the VM to run. Everything else
             * might not worth showing an own error message box in the GUI.
             */
            if (!(pDrv->fFlags & PDMAUDIODRVFLAGS_PRIMARY))
                continue;

            PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
            AssertPtr(pCon);

            bool fValidLineIn = AudioMixerStreamIsValid(pDrv->LineIn.pMixStrm);
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
            bool fValidMicIn  = AudioMixerStreamIsValid(pDrv->MicIn.pMixStrm);
# endif
            bool fValidOut    = AudioMixerStreamIsValid(pDrv->Front.pMixStrm);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
            /** @todo Anything to do here? */
# endif

            if (    !fValidLineIn
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                 && !fValidMicIn
# endif
                 && !fValidOut)
            {
                LogRel(("HDA: Falling back to NULL backend (no sound audible)\n"));

                hdaReset(pDevIns);
                hdaReattach(pThis, pDrv, pDrv->uLUN, "NullAudio");

                PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                    N_("No audio devices could be opened. Selecting the NULL audio backend "
                       "with the consequence that no sound is audible"));
            }
            else
            {
                bool fWarn = false;

                PDMAUDIOBACKENDCFG backendCfg;
                int rc2 = pCon->pfnGetConfig(pCon, &backendCfg);
                if (RT_SUCCESS(rc2))
                {
                    if (backendCfg.cMaxStreamsIn)
                    {
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                        /* If the audio backend supports two or more input streams at once,
                         * warn if one of our two inputs (microphone-in and line-in) failed to initialize. */
                        if (backendCfg.cMaxStreamsIn >= 2)
                            fWarn = !fValidLineIn || !fValidMicIn;
                        /* If the audio backend only supports one input stream at once (e.g. pure ALSA, and
                         * *not* ALSA via PulseAudio plugin!), only warn if both of our inputs failed to initialize.
                         * One of the two simply is not in use then. */
                        else if (backendCfg.cMaxStreamsIn == 1)
                            fWarn = !fValidLineIn && !fValidMicIn;
                        /* Don't warn if our backend is not able of supporting any input streams at all. */
# else /* !VBOX_WITH_AUDIO_HDA_MIC_IN */
                        /* We only have line-in as input source. */
                        fWarn = !fValidLineIn;
# endif /* VBOX_WITH_AUDIO_HDA_MIC_IN */
                    }

                    if (   !fWarn
                        && backendCfg.cMaxStreamsOut)
                    {
                        fWarn = !fValidOut;
                    }
                }
                else
                {
                    LogRel(("HDA: Unable to retrieve audio backend configuration for LUN #%RU8, rc=%Rrc\n", pDrv->uLUN, rc2));
                    fWarn = true;
                }

                if (fWarn)
                {
                    char   szMissingStreams[255];
                    size_t len = 0;
                    if (!fValidLineIn)
                    {
                        LogRel(("HDA: WARNING: Unable to open PCM line input for LUN #%RU8!\n", pDrv->uLUN));
                        len = RTStrPrintf(szMissingStreams, sizeof(szMissingStreams), "PCM Input");
                    }
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                    if (!fValidMicIn)
                    {
                        LogRel(("HDA: WARNING: Unable to open PCM microphone input for LUN #%RU8!\n", pDrv->uLUN));
                        len += RTStrPrintf(szMissingStreams + len,
                                           sizeof(szMissingStreams) - len, len ? ", PCM Microphone" : "PCM Microphone");
                    }
# endif /* VBOX_WITH_AUDIO_HDA_MIC_IN */
                    if (!fValidOut)
                    {
                        LogRel(("HDA: WARNING: Unable to open PCM output for LUN #%RU8!\n", pDrv->uLUN));
                        len += RTStrPrintf(szMissingStreams + len,
                                           sizeof(szMissingStreams) - len, len ? ", PCM Output" : "PCM Output");
                    }

                    PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                               N_("Some HDA audio streams (%s) could not be opened. Guest applications generating audio "
                                                  "output or depending on audio input may hang. Make sure your host audio device "
                                                  "is working properly. Check the logfile for error messages of the audio "
                                                  "subsystem"), szMissingStreams);
                }
            }
        }
#endif /* VBOX_WITH_AUDIO_HDA_ONETIME_INIT */
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
        PDMDevHlpDBGFInfoRegister(pDevIns, "hda",         "HDA info. (hda [register case-insensitive])",     hdaDbgInfo);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdabdle",     "HDA stream BDLE info. (hdabdle [stream number])", hdaDbgInfoBDLE);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdastrm",     "HDA stream info. (hdastrm [stream number])",      hdaDbgInfoStream);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdcnodes",    "HDA codec nodes.",                                hdaDbgInfoCodecNodes);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdcselector", "HDA codec's selector states [node number].",      hdaDbgInfoCodecSelector);
        PDMDevHlpDBGFInfoRegister(pDevIns, "hdamixer",    "HDA mixer state.",                                hdaDbgInfoMixer);

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

# ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
    if (RT_SUCCESS(rc))
    {
        /* Create the emulation timer. */
        rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, hdaTimer, pThis,
                                    TMTIMER_FLAGS_NO_CRIT_SECT, "DevHDA", &pThis->pTimer);
        AssertRCReturn(rc, rc);

        if (RT_SUCCESS(rc))
        {
            pThis->cTimerTicks = TMTimerGetFreq(pThis->pTimer) / uTimerHz;
            pThis->uTimerTS    = TMTimerGet(pThis->pTimer);
            LogFunc(("Timer ticks=%RU64 (%RU16 Hz)\n", pThis->cTimerTicks, uTimerHz));
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
            if (pDrv->fFlags != PDMAUDIODRVFLAGS_PRIMARY)
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
#  ifndef VBOX_WITH_AUDIO_HDA_CALLBACKS
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTimer,            STAMTYPE_PROFILE, "/Devices/HDA/Timer",             STAMUNIT_TICKS_PER_CALL, "Profiling hdaTimer.");
#  endif
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIn,               STAMTYPE_PROFILE, "/Devices/HDA/Input",             STAMUNIT_TICKS_PER_CALL, "Profiling input.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatOut,              STAMTYPE_PROFILE, "/Devices/HDA/Output",            STAMUNIT_TICKS_PER_CALL, "Profiling output.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead,        STAMTYPE_COUNTER, "/Devices/HDA/BytesRead"   ,      STAMUNIT_BYTES,          "Bytes read from HDA emulation.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesWritten,     STAMTYPE_COUNTER, "/Devices/HDA/BytesWritten",      STAMUNIT_BYTES,          "Bytes written to HDA emulation.");
    }
# endif

#ifdef HDA_DEBUG_DUMP_PCM_DATA
    RTFileDelete(HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaDMARead.pcm");
    RTFileDelete(HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaDMAWrite.pcm");
    RTFileDelete(HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaStreamRead.pcm");
    RTFileDelete(HDA_DEBUG_DUMP_PCM_DATA_PATH "hdaStreamWrite.pcm");
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceHDA =
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
    hdaAttach,
    /* pfnDetach */
    hdaDetach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    hdaPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
