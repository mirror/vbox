/* $Id$ */
/** @file
 * DevTpm - Trusted Platform Module emulation.
 *
 * This emulation is based on the spec available under (as of 2021-08-02):
 *     https://trustedcomputinggroup.org/wp-content/uploads/PC-Client-Specific-Platform-TPM-Profile-for-TPM-2p0-v1p05p_r14_pub.pdf
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEFAULT /** @todo DEV_TPM */
#include <VBox/vmm/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** The TPM saved state version. */
#define TPM_SAVED_STATE_VERSION                         1

/** Default vendor ID. */
#define TPM_VID_DEFAULT                                 0x1014
/** Default device ID. */
#define TPM_DID_DEFAULT                                 0x0001
/** Default revision ID. */
#define TPM_RID_DEFAULT                                 0x01

/** The TPM MMIO base default as defined in chapter 5.2. */
#define TPM_MMIO_BASE_DEFAULT                           0xfed40000
/** The size of the TPM MMIO area. */
#define TPM_MMIO_SIZE                                   0x5000

/** Number of localities as mandated by the TPM spec. */
#define TPM_LOCALITY_COUNT                              5
/** Size of each locality in the TPM MMIO area (chapter 6.5.2).*/
#define TPM_LOCALITY_MMIO_SIZE                          0x1000

/** @name TPM locality register related defines.
 * @{ */
/** Ownership management for a particular locality. */
#define TPM_LOCALITY_REG_ACCESS                         0x00
/** INdicates whether a dynamic OS has been established on this platform before.. */
# define TPM_LOCALITY_REG_ESTABLISHMENT                 RT_BIT(0)
/** On reads indicates whether the locality requests use of the TPM (1) or not or is already active locality (0),
 * writing a 1 requests the locality to be granted getting the active locality.. */
# define TPM_LOCALITY_REG_REQUEST_USE                   RT_BIT(1)
/** Indicates whether another locality is requesting usage of the TPM. */
# define TPM_LOCALITY_REG_PENDING_REQUEST               RT_BIT(2)
/** Writing a 1 forces the TPM to give control to the locality if it has a higher priority. */
# define TPM_LOCALITY_REG_ACCESS_SEIZE                  RT_BIT(3)
/** On reads indicates whether this locality has been seized by a higher locality (1) or not (0), writing a 1 clears this bit. */
# define TPM_LOCALITY_REG_ACCESS_BEEN_SEIZED            RT_BIT(4)
/** On reads indicates whether this locality is active (1) or not (0), writing a 1 relinquishes control for this locality. */
# define TPM_LOCALITY_REG_ACCESS_ACTIVE                 RT_BIT(5)
/** Set bit indicates whether all other bits in this register have valid data. */
# define TPM_LOCALITY_REG_ACCESS_VALID                  RT_BIT(7)
/** Writable mask. */
# define TPM_LOCALITY_REG_ACCESS_WR_MASK                0x3a

/** Interrupt enable register. */
#define TPM_LOCALITY_REG_INT_ENABLE                     0x08
/** Data available interrupt enable bit. */
# define TPM_LOCALITY_REG_INT_ENABLE_DATA_AVAIL         RT_BIT_32(0)
/** Status valid interrupt enable bit. */
# define TPM_LOCALITY_REG_INT_ENABLE_STS_VALID          RT_BIT_32(1)
/** Locality change interrupt enable bit. */
# define TPM_LOCALITY_REG_INT_ENABLE_LOCALITY_CHANGE    RT_BIT_32(2)
/** Interrupt polarity configuration. */
# define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_MASK      0x18
# define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_SHIFT     3
# define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_SET(a)    ((a) << TPM_LOCALITY_REG_INT_POLARITY_SHIFT)
# define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_GET(a)    (((a) & TPM_LOCALITY_REG_INT_POLARITY_MASK) >> TPM_LOCALITY_REG_INT_POLARITY_SHIFT)
/** High level interrupt trigger. */
#  define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_HIGH     0
/** Low level interrupt trigger. */
#  define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_LOW      1
/** Rising edge interrupt trigger. */
#  define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_RISING   2
/** Falling edge interrupt trigger. */
#  define TPM_LOCALITY_REG_INT_ENABLE_POLARITY_FALLING  3
/** Command ready enable bit. */
# define TPM_LOCALITY_REG_INT_ENABLE_CMD_RDY            RT_BIT_32(7)
/** Global interrupt enable/disable bit. */
# define TPM_LOCALITY_REG_INT_ENABLE_GLOBAL             RT_BIT_32(31)

/** Configured interrupt vector register. */
#define TPM_LOCALITY_REG_INT_VEC                        0x0c

/** Interrupt status register. */
#define TPM_LOCALITY_REG_INT_STS                        0x10
/** Data available interrupt occured bit, writing a 1 clears the bit. */
# define TPM_LOCALITY_REG_INT_STS_DATA_AVAIL            RT_BIT_32(0)
/** Status valid interrupt occured bit, writing a 1 clears the bit. */
# define TPM_LOCALITY_REG_INT_STS_STS_VALID             RT_BIT_32(1)
/** Locality change interrupt occured bit, writing a 1 clears the bit. */
# define TPM_LOCALITY_REG_INT_STS_LOCALITY_CHANGE       RT_BIT_32(2)
/** Command ready occured bit, writing a 1 clears the bit. */
# define TPM_LOCALITY_REG_INT_STS_CMD_RDY               RT_BIT_32(7)
/** Writable mask. */
# define TPM_LOCALITY_REG_INT_STS_WR_MASK               UINT32_C(0x87)

/** Interfacce capabilities register. */
#define TPM_LOCALITY_REG_IF_CAP                         0x14
/** Flag whether the TPM supports the data avilable interrupt. */
# define TPM_LOCALITY_REG_IF_CAP_INT_DATA_AVAIL         RT_BIT(0)
/** Flag whether the TPM supports the status valid interrupt. */
# define TPM_LOCALITY_REG_IF_CAP_INT_STS_VALID          RT_BIT(1)
/** Flag whether the TPM supports the data avilable interrupt. */
# define TPM_LOCALITY_REG_IF_CAP_INT_LOCALITY_CHANGE    RT_BIT(2)
/** Flag whether the TPM supports high level interrupts. */
# define TPM_LOCALITY_REG_IF_CAP_INT_LVL_HIGH           RT_BIT(3)
/** Flag whether the TPM supports low level interrupts. */
# define TPM_LOCALITY_REG_IF_CAP_INT_LVL_LOW            RT_BIT(4)
/** Flag whether the TPM supports rising edge interrupts. */
# define TPM_LOCALITY_REG_IF_CAP_INT_RISING_EDGE        RT_BIT(5)
/** Flag whether the TPM supports falling edge interrupts. */
# define TPM_LOCALITY_REG_IF_CAP_INT_FALLING_EDGE       RT_BIT(6)
/** Flag whether the TPM supports the command ready interrupt. */
# define TPM_LOCALITY_REG_IF_CAP_INT_CMD_RDY            RT_BIT(7)
/** Flag whether the busrt count field is static or dynamic. */
# define TPM_LOCALITY_REG_IF_CAP_BURST_CNT_STATIC       RT_BIT(8)
/** Maximum transfer size support. */
# define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_MASK      0x600
# define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_SHIFT     9
# define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_SET(a)    ((a) << TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_SHIFT)
/** Only legacy transfers supported. */
#  define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_LEGACY   0x0
/** 8B maximum transfer size. */
#  define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_8B       0x1
/** 32B maximum transfer size. */
#  define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_32B      0x2
/** 64B maximum transfer size. */
#  define TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_64B      0x3
/** Interface version. */
# define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_MASK        UINT32_C(0x70000000)
# define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_SHIFT       28
# define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_SET(a)      ((a) << TPM_LOCALITY_REG_IF_CAP_IF_VERSION_SHIFT)
/** Interface 1.21 or ealier. */
#  define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_IF_1_21    0
/** Interface 1.3. */
#  define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_IF_1_3     2
/** Interface 1.3 for TPM 2.0. */
#  define TPM_LOCALITY_REG_IF_CAP_IF_VERSION_IF_1_3_TPM2    3

/** TPM status register. */
#define TPM_LOCALITY_REG_STS                            0x18
/** Writing a 1 forces the TPM to re-send the response. */
# define TPM_LOCALITY_REG_STS_RESPONSE_RETRY            RT_BIT_32(1)
/** Indicating whether the TPM has finished a self test. */
# define TPM_LOCALITY_REG_STS_SELF_TEST_DONE            RT_BIT_32(2)
/** Flag indicating whether the TPM expects more data for the command. */
# define TPM_LOCALITY_REG_STS_EXPECT                    RT_BIT_32(3)
/** Flag indicating whether the TPM has more response data available. */
# define TPM_LOCALITY_REG_STS_DATA_AVAIL                RT_BIT_32(4)
/** Written by software to cause the TPM to execute a previously transfered command. */
# define TPM_LOCALITY_REG_STS_TPM_GO                    RT_BIT_32(5)
/** On reads indicates whether the TPM is ready to receive a new command (1) or not (0),
 * a write of 1 causes the TPM to transition to this state. */
# define TPM_LOCALITY_REG_STS_CMD_RDY                   RT_BIT_32(6)
/** Indicates whether the Expect and data available bits are valid. */
# define TPM_LOCALITY_REG_STS_VALID                     RT_BIT_32(7)
# define TPM_LOCALITY_REG_STS_BURST_CNT_MASK            UINT32_C(0xffff00)
# define TPM_LOCALITY_REG_STS_BURST_CNT_SHIFT           8
# define TPM_LOCALITY_REG_STS_BURST_CNT_SET(a)          ((a) << TPM_LOCALITY_REG_STS_BURST_CNT_SHIFT)

/** TPM end of HASH operation signal register for locality 4. */
#define TPM_LOCALITY_REG_HASH_END                       0x20
/** Data FIFO read/write register. */
#define TPM_LOCALITY_REG_DATA_FIFO                      0x24
/** TPM start of HASH operation signal register for locality 4. */
#define TPM_LOCALITY_REG_HASH_START                     0x28
/** Extended data FIFO read/write register. */
#define TPM_LOCALITY_REG_XDATA_FIFO                     0x80
/** TPM device and vendor ID. */
#define TPM_LOCALITY_REG_DID_VID                        0xf00
/** TPM revision ID. */
#define TPM_LOCALITY_REG_RID                            0xf04
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Possible locality states
 * (see chapter 5.6.12.1 Figure 3 State Transition Diagram).
 */
typedef enum DEVTPMLOCALITYSTATE
{
    /** Invalid state, do not use. */
    DEVTPMLOCALITYSTATE_INVALID = 0,
    /** Init state. */
    DEVTPMLOCALITYSTATE_INIT,
    /** Ready to accept command data. */
    DEVTPMLOCALITYSTATE_READY,
    /** Command data being transfered. */
    DEVTPMLOCALITYSTATE_CMD_RECEPTION,
    /** Command is being executed by the TPM. */
    DEVTPMLOCALITYSTATE_CMD_EXEC,
    /** Command has completed and data can be read. */
    DEVTPMLOCALITYSTATE_CMD_COMPLETION,
    /** 32bit hack. */
    DEVTPMLOCALITYSTATE_32BIT_HACK = 0x7fffffff
} DEVTPMLOCALITYSTATE;


/**
 * Locality state.
 */
typedef struct DEVTPMLOCALITY
{
    /** The current state of the locality. */
    DEVTPMLOCALITYSTATE             enmState;
    /** Access register state. */
    uint32_t                        uRegAccess;
    /** The interrupt enable register. */
    uint32_t                        uRegIntEn;
    /** The interrupt status register. */
    uint32_t                        uRegIntSts;
} DEVTPMLOCALITY;
typedef DEVTPMLOCALITY *PDEVTPMLOCALITY;
typedef const DEVTPMLOCALITY *PCDEVTPMLOCALITY;


/**
 * Shared TPM device state.
 */
typedef struct DEVTPM
{
    /** The handle of the MMIO region. */
    IOMMMIOHANDLE                   hMmio;
    /** The vendor ID configured. */
    uint16_t                        uVenId;
    /** The device ID configured. */
    uint16_t                        uDevId;
    /** The revision ID configured. */
    uint8_t                         bRevId;
    /** The IRQ value. */
    uint8_t                         uIrq;

    /** Currently selected locality. */
    uint8_t                         bLoc;
    /** States of the implemented localities. */
    DEVTPMLOCALITY                  aLoc[TPM_LOCALITY_COUNT];

} DEVTPM;
/** Pointer to the shared TPM device state. */
typedef DEVTPM *PDEVTPM;

/** The special no current locality selected value. */
#define TPM_NO_LOCALITY_SELECTED    0xff


/**
 * TPM device state for ring-3.
 */
typedef struct DEVTPMR3
{
    uint8_t                         bDummy;
} DEVTPMR3;
/** Pointer to the TPM device state for ring-3. */
typedef DEVTPMR3 *PDEVTPMR3;


/**
 * TPM device state for ring-0.
 */
typedef struct DEVTPMR0
{
    uint8_t                         bDummy;
} DEVTPMR0;
/** Pointer to the TPM device state for ring-0. */
typedef DEVTPMR0 *PDEVTPMR0;


/**
 * TPM device state for raw-mode.
 */
typedef struct DEVTPMRC
{
    uint8_t                         bDummy;
} DEVTPMRC;
/** Pointer to the TPM device state for raw-mode. */
typedef DEVTPMRC *PDEVTPMRC;

/** The TPM device state for the current context. */
typedef CTX_SUFF(DEVTPM) DEVTPMCC;
/** Pointer to the TPM device state for the current context. */
typedef CTX_SUFF(PDEVTPM) PDEVTPMCC;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE



/**
 * Sets the IRQ line of the given device to the given state.
 *
 * @returns nothing.
 * @param   pDevIns             Pointer to the PDM device instance data.
 * @param   pThis               Pointer to the shared TPM device.
 * @param   iLvl                The interrupt level to set.
 */
DECLINLINE(void) tpmIrqReq(PPDMDEVINS pDevIns, PDEVTPM pThis, int iLvl)
{
    PDMDevHlpISASetIrqNoWait(pDevIns, pThis->uIrq, iLvl);
}


/**
 * Updates the IRQ status of the given locality.
 *
 * @returns nothing.
 * @param   pDevIns             Pointer to the PDM device instance data.
 * @param   pThis               Pointer to the shared TPM device.
 * @param   pLoc                The locality state.
 */
PDMBOTHCBDECL(void) tpmLocIrqUpdate(PPDMDEVINS pDevIns, PDEVTPM pThis, PDEVTPMLOCALITY pLoc)
{
    if (pLoc->uRegIntEn & pLoc->uRegIntSts)
        tpmIrqReq(pDevIns, pThis, 1);
    else
        tpmIrqReq(pDevIns, pThis, 0);
}


/**
 * Returns the given locality being accessed from the given TPM MMIO offset.
 *
 * @returns Locality number.
 * @param   off                 The offset into the TPM MMIO region.
 */
DECLINLINE(uint8_t) tpmGetLocalityFromOffset(RTGCPHYS off)
{
    return off / TPM_LOCALITY_MMIO_SIZE;
}


/**
 * Returns the given register of a particular locality being accessed from the given TPM MMIO offset.
 *
 * @returns Register index being accessed.
 * @param   off                 The offset into the TPM MMIO region.
 */
DECLINLINE(uint32_t) tpmGetRegisterFromOffset(RTGCPHYS off)
{
    return off % TPM_LOCALITY_MMIO_SIZE;
}


/* -=-=-=-=-=- MMIO callbacks -=-=-=-=-=- */


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) tpmMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PDEVTPM pThis  = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    RT_NOREF(pvUser);

    Assert(!(off & (cb - 1)));

    LogFlowFunc((": %RGp %#x\n", off, cb));
    VBOXSTRICTRC rc = VINF_SUCCESS;
    uint32_t uReg = tpmGetRegisterFromOffset(off);
    uint8_t bLoc = tpmGetLocalityFromOffset(off);
    PDEVTPMLOCALITY pLoc = &pThis->aLoc[bLoc];
    uint32_t u32;
    switch (uReg)
    {
        case TPM_LOCALITY_REG_ACCESS:
            u32 = 0;
            break;
        case TPM_LOCALITY_REG_INT_ENABLE:
            u32 = pLoc->uRegIntEn;
            break;
        case TPM_LOCALITY_REG_INT_VEC:
            u32 = pThis->uIrq;
            break;
        case TPM_LOCALITY_REG_INT_STS:
            u32 = pLoc->uRegIntSts;
            break;
        case TPM_LOCALITY_REG_IF_CAP:
            u32 =   TPM_LOCALITY_REG_IF_CAP_INT_DATA_AVAIL
                  | TPM_LOCALITY_REG_IF_CAP_INT_STS_VALID
                  | TPM_LOCALITY_REG_IF_CAP_INT_LOCALITY_CHANGE
                  | TPM_LOCALITY_REG_IF_CAP_INT_LVL_LOW
                  | TPM_LOCALITY_REG_IF_CAP_INT_CMD_RDY
                  | TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_SET(TPM_LOCALITY_REG_IF_CAP_DATA_XFER_SZ_64B)
                  | TPM_LOCALITY_REG_IF_CAP_IF_VERSION_SET(TPM_LOCALITY_REG_IF_CAP_IF_VERSION_IF_1_3); /** @todo Make some of them configurable? */
            break;
        case TPM_LOCALITY_REG_STS:
            if (bLoc != pThis->bLoc)
            {
                u32 = UINT32_MAX;
                break;
            }
            /** @todo */
            break;
        case TPM_LOCALITY_REG_DATA_FIFO:
        case TPM_LOCALITY_REG_DATA_XFIFO:
            /** @todo */
            break;
        case TPM_LOCALITY_REG_DID_VID:
            u32 = RT_H2BE_U32(RT_MAKE_U32(pThis->uVenId, pThis->uDevId));
            break;
        case TPM_LOCALITY_REG_RID:
            u32 = pThis->bRevId;
            break;
        default: /* Return ~0. */
            u32 = UINT32_MAX;
            break;
    }

    switch (cb)
    {
        case 1: *(uint8_t *)pv = (uint8_t)u32; break;
        case 2: *(uint16_t *)pv = (uint16_t)u32; break;
        case 4: *(uint32_t *)pv = u32; break;
        default: AssertFailedBreakStmt(rc = VERR_INTERNAL_ERROR);
    }

    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) tpmMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    PDEVTPM pThis  = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    RT_NOREF(pvUser);

    Assert(!(off & (cb - 1)));

    uint32_t u32;
    switch (cb)
    {
        case 1: u32 = *(const uint8_t *)pv; break;
        case 2: u32 = *(const uint16_t *)pv; break;
        case 4: u32 = *(const uint32_t *)pv; break;
        default: AssertFailedReturn(VERR_INTERNAL_ERROR);
    }

    LogFlowFunc((": %RGp %#x\n", off, u32));

    VBOXSTRICTRC rc = VINF_SUCCESS;
    uint32_t uReg = tpmGetRegisterFromOffset(off);
    uint8_t bLoc = tpmGetLocalityFromOffset(off);
    PDEVTPMLOCALITY pLoc = &pThis->aLoc[bLoc];
    switch (uReg)
    {
        case TPM_LOCALITY_REG_ACCESS:
            u32 &= TPM_LOCALITY_REG_ACCESS_WR_MASK;
             /*
              * Chapter 5.6.11, 2 states that writing to this register with more than one
              * bit set to '1' is vendor specific, we decide to ignore such writes to make the logic
              * below simpler.
              */
            if (!RT_IS_POWER_OF_TWO(u32))
                break;
            /** @todo */
            break;
        case TPM_LOCALITY_REG_INT_ENABLE:
            if (bLoc != pThis->bLoc)
                break;
            /** @todo */
            break;
        case TPM_LOCALITY_REG_INT_STS:
            if (bLoc != pThis->bLoc)
                break;
            pLoc->uRegSts &= ~(u32 & TPM_LOCALITY_REG_INT_STS_WR_MASK);
            tpmLocIrqUpdate(pDevIns, pThis, pLoc);
            break;
        case TPM_LOCALITY_REG_STS:
            /*
             * Writes are ignored completely if the locality being accessed is not the
             * current active one or if the value has multiple bits set (not a power of two),
             * see chapter 5.6.12.1.
             */
            if (   bLoc != pThis->bLoc
                || !RT_IS_POWER_OF_TWO(u32))
                break;
            /** @todo */
            break;
        case TPM_LOCALITY_REG_DATA_FIFO:
        case TPM_LOCALITY_REG_DATA_XFIFO:
            if (bLoc != pThis->bLoc)
                break;
            /** @todo */
            break;
        case TPM_LOCALITY_REG_INT_VEC:
        case TPM_LOCALITY_REG_IF_CAP:
        case TPM_LOCALITY_REG_DID_VID:
        case TPM_LOCALITY_REG_RID:
        default: /* Ignore. */
            break;
    }

    return rc;
}


#ifdef IN_RING3


/* -=-=-=-=-=-=-=-=- Saved State -=-=-=-=-=-=-=-=- */

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) tpmR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PDEVTPM         pThis = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    RT_NOREF(uPass);

    pHlp->pfnSSMPutU8(pSSM, pThis->uIrq);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) tpmR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVTPM         pThis = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMPutU8(pSSM, pThis->uIrq);

    return pHlp->pfnSSMPutU32(pSSM, UINT32_MAX); /* sanity/terminator */
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) tpmR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PDEVTPM         pThis = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    PCPDMDEVHLPR3   pHlp  = pDevIns->pHlpR3;
    uint8_t         bIrq;
    int rc;

    AssertMsgReturn(uVersion >= TPM_SAVED_STATE_VERSION, ("%d\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    pHlp->pfnSSMGetU8(    pSSM, &bIrq);
    if (uPass == SSM_PASS_FINAL)
    {
        /* The marker. */
        uint32_t u32;
        rc = pHlp->pfnSSMGetU32(pSSM, &u32);
        AssertRCReturn(rc, rc);
        AssertMsgReturn(u32 == UINT32_MAX, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    /*
     * Check the config.
     */
    if (pThis->uIrq != bIrq)
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS,
                                       N_("Config mismatch - saved IRQ=%#x; configured IRQ=%#x"),
                                       bIrq, pThis->uIrq);

    return VINF_SUCCESS;
}


/* -=-=-=-=-=-=-=-=- PDMDEVREG -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) tpmR3Reset(PPDMDEVINS pDevIns)
{
    PDEVTPM   pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);

    pThis->bLoc = TPM_NO_LOCALITY_SELECTED;
    for (uint32_t i = 0; i < RT_ELEMENTS(pThis->aLoc); i++)
    {
        PDEVTPMLOCALITY pLoc = &pThis->aLoc[i];

        pLoc->enmState   = DEVTPMLOCALITYSTATE_INIT;
        pLoc->aRegIntEn  = 0;
        pLoc->aRegIntSts = 0;
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) tpmR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PDEVTPM pThis = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);

    /** @todo */
    RT_NOREF(pThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) tpmR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVTPM         pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    //PDEVTPMCC       pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVTPMCC);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    int             rc;

    RT_NOREF(iInstance);

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "Irq"
                                           "|MmioBase"
                                           "|VendorId"
                                           "|DeviceId"
                                           "|RevisionId",
                                           "");

    uint8_t uIrq = 0;
    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "Irq", &uIrq, 10);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"Irq\" value"));

    RTGCPHYS GCPhysMmio;
    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "MmioBase", &GCPhysMmio, TPM_MMIO_BASE_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"MmioBase\" value"));

    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "VendorId", &pThis->uDevId, TPM_VID_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"VendorId\" value"));

    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "DeviceId", &pThis->uDevId, TPM_DID_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"DeviceId\" value"));

    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "RevisionId", &pThis->bRevId, TPM_RID_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to get the \"RevisionId\" value"));

    pThis->uIrq = uIrq;

    /*
     * Register the MMIO range, PDM API requests page aligned
     * addresses and sizes.
     */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmio, TPM_MMIO_SIZE, tpmMmioWrite, tpmMmioRead,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   "TPM MMIO", &pThis->hMmio);
    AssertRCReturn(rc, rc);

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister3(pDevIns, TPM_SAVED_STATE_VERSION, sizeof(*pThis),
                               tpmR3LiveExec, tpmR3SaveExec, tpmR3LoadExec);
    AssertRCReturn(rc, rc);

    tpmR3Reset(pDevIns);
    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) tpmRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVTPM   pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVTPM);
    PDEVTPMCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PDEVTPMCC);

    int rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, tpmMmioWrite, tpmMmioRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceTpm =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "tpm",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_SERIAL,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVTPM),
    /* .cbInstanceCC = */           sizeof(DEVTPMCC),
    /* .cbInstanceRC = */           sizeof(DEVTPMRC),
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Trusted Platform Module",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           tpmR3Construct,
    /* .pfnDestruct = */            tpmR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               tpmR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           tpmRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           tpmRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

