/* $Id$ */
/** @file
 * VirtualBox Parallel Device Emulation.
 *
 * Contributed by: Alexander Eichner
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* based on DevSerial.cpp */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PARALLEL
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "Builtins.h"

#define PARALLEL_SAVED_STATE_VERSION 1

/* defines for accessing the register bits */
#define LPT_STATUS_BUSY                0x80
#define LPT_STATUS_ACK                 0x40
#define LPT_STATUS_PAPER_OUT           0x20
#define LPT_STATUS_SELECT_IN           0x10
#define LPT_STATUS_ERROR               0x08
#define LPT_STATUS_IRQ                 0x04
#define LPT_STATUS_BIT1                0x02 /* reserved (only for completeness) */
#define LPT_STATUS_EPP_TIMEOUT         0x01

#define LPT_CONTROL_BIT7               0x80 /* reserved (only for completeness) */
#define LPT_CONTROL_BIT6               0x40 /* reserved (only for completeness) */
#define LPT_CONTROL_ENABLE_BIDIRECT    0x20
#define LPT_CONTROL_ENABLE_IRQ_VIA_ACK 0x10
#define LPT_CONTROL_SELECT_PRINTER     0x08
#define LPT_CONTROL_RESET              0x04
#define LPT_CONTROL_AUTO_LINEFEED      0x02
#define LPT_CONTROL_STROBE             0x01

/** mode defines for the extended control register */
#define LPT_ECP_ECR_CHIPMODE_MASK      0xe0
#define LPT_ECP_ECR_CHIPMODE_GET_BITS(reg) ((reg) >> 5)
#define LPT_ECP_ECR_CHIPMODE_SET_BITS(val) ((val) << 5)
#define LPT_ECP_ECR_CHIPMODE_CONFIGURATION 0x07
#define LPT_ECP_ECR_CHIPMODE_FIFO_TEST 0x06
#define LPT_ECP_ECR_CHIPMODE_RESERVED  0x05
#define LPT_ECP_ECR_CHIPMODE_EPP       0x04
#define LPT_ECP_ECR_CHIPMODE_ECP_FIFO  0x03
#define LPT_ECP_ECR_CHIPMODE_PP_FIFO   0x02
#define LPT_ECP_ECR_CHIPMODE_BYTE      0x01
#define LPT_ECP_ECR_CHIPMODE_COMPAT    0x00

/** FIFO status bits in extended control register */
#define LPT_ECP_ECR_FIFO_MASK          0x03
#define LPT_ECP_ECR_FIFO_SOME_DATA     0x00
#define LPT_ECP_ECR_FIFO_FULL          0x02
#define LPT_ECP_ECR_FIFO_EMPTY         0x01

#define LPT_ECP_CONFIGA_FIFO_WITDH_MASK 0x70
#define LPT_ECP_CONFIGA_FIFO_WIDTH_GET_BITS(reg) ((reg) >> 4)
#define LPT_ECP_CONFIGA_FIFO_WIDTH_SET_BITS(val) ((val) << 4)
#define LPT_ECP_CONFIGA_FIFO_WIDTH_16   0x00
#define LPT_ECP_CONFIGA_FIFO_WIDTH_32   0x20
#define LPT_ECP_CONFIGA_FIFO_WIDTH_8    0x10

#define LPT_ECP_FIFO_DEPTH 2


typedef struct ParallelState
{
    /** Access critical section. */
    PDMCRITSECT                         CritSect;

    /** Pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS)             pDevInsHC;
    /** Pointer to the device instance. */
    GCPTRTYPE(PPDMDEVINS)               pDevInsGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS != 64
    RTGCPTR                             Alignment0;
#endif
    /** The base interface. */
    R3PTRTYPE(PDMIBASE)                 IBase;
    /** The host device port interface. */
    R3PTRTYPE(PDMIHOSTPARALLELPORT)     IHostParallelPort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)                pDrvBase;
    /** Pointer to the attached host device. */
    R3PTRTYPE(PPDMIHOSTPARALLELCONNECTOR) pDrvHostParallelConnector;

    uint8_t                             reg_data;
    uint8_t                             reg_status;
    uint8_t                             reg_control;
    uint8_t                             reg_epp_addr;
    uint8_t                             reg_epp_data;
    uint8_t                             reg_ecp_ecr;
    uint8_t                             reg_ecp_base_plus_400h; /* has different meanings */
    uint8_t                             reg_ecp_config_b;

    /** The ECP FIFO implementation*/
    uint8_t                             ecp_fifo[LPT_ECP_FIFO_DEPTH];
    int                                 act_fifo_pos_write;
    int                                 act_fifo_pos_read;

    int                                 irq;
    uint8_t                             epp_timeout;

    bool                                fGCEnabled;
    bool                                fR0Enabled;
    bool                                afAlignment[6];

    RTSEMEVENT                          ReceiveSem;
    uint32_t                            base;

} DEVPARALLELSTATE, *PDEVPARALLELSTATE;
typedef DEVPARALLELSTATE ParallelState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define PDMIHOSTPARALLELPORT_2_PARALLELSTATE(pInstance)   ( (ParallelState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ParallelState, IHostParallelPort)) )
#define PDMIHOSTDEVICEPORT_2_PARALLELSTATE(pInstance)   ( (ParallelState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ParallelState, IHostDevicePort)) )
#define PDMIBASE_2_PARALLELSTATE(pInstance)       ( (ParallelState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ParallelState, IBase)) )

__BEGIN_DECLS
PDMBOTHCBDECL(int) parallelIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) parallelIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
#if 0
PDMBOTHCBDECL(int) parallelIOPortReadECP(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) parallelIOPortWriteECP(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
#endif
__END_DECLS

#ifdef IN_RING3
static void parallel_set_irq(ParallelState *s)
{
    if (s->reg_control & LPT_CONTROL_ENABLE_IRQ_VIA_ACK)
    {
        Log(("parallel_update_irq %d 1\n", s->irq));
        PDMDevHlpISASetIrqNoWait(CTXSUFF(s->pDevIns), s->irq, 1);
    }
}

static void parallel_clear_irq(ParallelState *s)
{
    Log(("parallel_update_irq %d 0\n", s->irq));
    PDMDevHlpISASetIrqNoWait(CTXSUFF(s->pDevIns), s->irq, 0);
}
#endif

static int parallel_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    ParallelState *s = (ParallelState *)opaque;
    unsigned char ch;

    addr &= 7;
    LogFlow(("parallel: write addr=0x%02x val=0x%02x\n", addr, val));
    ch = val;

    switch(addr) {
    default:
    case 0:
#ifndef IN_RING3
        NOREF(ch);
        return VINF_IOM_HC_IOPORT_WRITE;
#else
        s->reg_data = ch;
        if (RT_LIKELY(s->pDrvHostParallelConnector))
        {
            Log(("parallel_io_port_write: write 0x%X\n", ch));
            size_t cbWrite = 1;
            int rc = s->pDrvHostParallelConnector->pfnWrite(s->pDrvHostParallelConnector, &ch, &cbWrite);
            AssertRC(rc);
        }
#endif
        break;
    case 1:
        break;
    case 2:
        /* Set the reserved bits to one */
        ch |= (LPT_CONTROL_BIT6 | LPT_CONTROL_BIT7);
        if (ch != s->reg_control) {
#ifndef IN_RING3
            NOREF(ch);
            return VINF_IOM_HC_IOPORT_WRITE;
#else
            int rc = s->pDrvHostParallelConnector->pfnWriteControl(s->pDrvHostParallelConnector, ch);
            AssertRC(rc);
            s->reg_control = val;
#endif
        }
        break;
    case 3:
        s->reg_epp_addr = val;
        break;
    case 4:
        s->reg_epp_data = val;
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    return VINF_SUCCESS;
}

static uint32_t parallel_ioport_read(void *opaque, uint32_t addr, int *pRC)
{
    ParallelState *s = (ParallelState *)opaque;
    uint32_t ret = ~0U;

    *pRC = VINF_SUCCESS;

    addr &= 7;
    switch(addr) {
    default:
    case 0:
        if (!(s->reg_control & LPT_CONTROL_ENABLE_BIDIRECT))
            ret = s->reg_data;
        else
        {
#ifndef IN_RING3
            *pRC = VINF_IOM_HC_IOPORT_READ;
#else
            if (RT_LIKELY(s->pDrvHostParallelConnector))
            {
                size_t cbRead;
                int rc = s->pDrvHostParallelConnector->pfnRead(s->pDrvHostParallelConnector, &s->reg_data, &cbRead);
                Log(("parallel_io_port_read: read 0x%X\n", s->reg_data));
                AssertRC(rc);
            }
            ret = s->reg_data;
#endif
        }
        break;
    case 1:
#ifndef IN_RING3
        *pRC = VINF_IOM_HC_IOPORT_READ;
#else
        if (RT_LIKELY(s->pDrvHostParallelConnector))
        {
            int rc = s->pDrvHostParallelConnector->pfnReadStatus(s->pDrvHostParallelConnector, &s->reg_status);
            AssertRC(rc);
        }
        ret = s->reg_status;
        parallel_clear_irq(s);
#endif
        break;
    case 2:
        ret = s->reg_control;
        break;
    case 3:
        ret = s->reg_epp_addr;
        break;
    case 4:
        ret = s->reg_epp_data;
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    LogFlow(("parallel: read addr=0x%02x val=0x%02x\n", addr, ret));
    return ret;
}

#if 0
static int parallel_ioport_write_ecp(void *opaque, uint32_t addr, uint32_t val)
{
    ParallelState *s = (ParallelState *)opaque;
    unsigned char ch;

    addr &= 7;
    LogFlow(("parallel: write ecp addr=0x%02x val=0x%02x\n", addr, val));
    ch = val;
    switch(addr) {
    default:
    case 0:
        if (LPT_ECP_ECR_CHIPMODE_GET_BITS(s->reg_ecp_ecr) == LPT_ECP_ECR_CHIPMODE_FIFO_TEST) {
            s->ecp_fifo[s->act_fifo_pos_write] = ch;
            s->act_fifo_pos_write++;
            if (s->act_fifo_pos_write < LPT_ECP_FIFO_DEPTH) {
                /* FIFO has some data (clear both FIFO bits) */
                s->reg_ecp_ecr &= ~(LPT_ECP_ECR_FIFO_EMPTY | LPT_ECP_ECR_FIFO_FULL);
            } else {
                /* FIFO is full */
                /* Clear FIFO empty bit */
                s->reg_ecp_ecr &= ~LPT_ECP_ECR_FIFO_EMPTY;
                /* Set FIFO full bit */
                s->reg_ecp_ecr |= LPT_ECP_ECR_FIFO_FULL;
                s->act_fifo_pos_write = 0;
            }
        } else {
            s->reg_ecp_base_plus_400h = ch;
        }
        break;
    case 1:
        s->reg_ecp_config_b = ch;
        break;
    case 2:
        /* If we change the mode clear FIFO */
        if ((ch & LPT_ECP_ECR_CHIPMODE_MASK) != (s->reg_ecp_ecr & LPT_ECP_ECR_CHIPMODE_MASK)) {
            /* reset the fifo */
            s->act_fifo_pos_write = 0;
            s->act_fifo_pos_read = 0;
            /* Set FIFO empty bit */
            s->reg_ecp_ecr |= LPT_ECP_ECR_FIFO_EMPTY;
            /* Clear FIFO full bit */
            s->reg_ecp_ecr &= ~LPT_ECP_ECR_FIFO_FULL;
        }
        /* Set new mode */
        s->reg_ecp_ecr |= LPT_ECP_ECR_CHIPMODE_SET_BITS(LPT_ECP_ECR_CHIPMODE_GET_BITS(ch));
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    return VINF_SUCCESS;
}

static uint32_t parallel_ioport_read_ecp(void *opaque, uint32_t addr, int *pRC)
{
    ParallelState *s = (ParallelState *)opaque;
    uint32_t ret = ~0U;

    *pRC = VINF_SUCCESS;

    addr &= 7;
    switch(addr) {
    default:
    case 0:
        if (LPT_ECP_ECR_CHIPMODE_GET_BITS(s->reg_ecp_ecr) == LPT_ECP_ECR_CHIPMODE_FIFO_TEST) {
            ret = s->ecp_fifo[s->act_fifo_pos_read];
            s->act_fifo_pos_read++;
            if (s->act_fifo_pos_read == LPT_ECP_FIFO_DEPTH)
                s->act_fifo_pos_read = 0; /* end of FIFO, start at beginning */
            if (s->act_fifo_pos_read == s->act_fifo_pos_write) {
                /* FIFO is empty */
                /* Set FIFO empty bit */
                s->reg_ecp_ecr |= LPT_ECP_ECR_FIFO_EMPTY;
                /* Clear FIFO full bit */
                s->reg_ecp_ecr &= ~LPT_ECP_ECR_FIFO_FULL;
            } else {
                /* FIFO has some data (clear all FIFO bits) */
                s->reg_ecp_ecr &= ~(LPT_ECP_ECR_FIFO_EMPTY | LPT_ECP_ECR_FIFO_FULL);
            }
        } else {
            ret = s->reg_ecp_base_plus_400h;
        }
        break;
    case 1:
        ret = s->reg_ecp_config_b;
        break;
    case 2:
        ret = s->reg_ecp_ecr;
        break;
    case 3:
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    }
    LogFlow(("parallel: read ecp addr=0x%02x val=0x%02x\n", addr, ret));
    return ret;
}
#endif

#ifdef IN_RING3
static DECLCALLBACK(int) parallelNotifyInterrupt(PPDMIHOSTPARALLELPORT pInterface)
{
    ParallelState *pData = PDMIHOSTPARALLELPORT_2_PARALLELSTATE(pInterface);

    PDMCritSectEnter(&pData->CritSect, VINF_SUCCESS);
    parallel_set_irq(pData);
    PDMCritSectLeave(&pData->CritSect);

    return VINF_SUCCESS;
}
#endif /* IN_RING3 */

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortWrite(PPDMDEVINS pDevIns, void *pvUser,
                                       RTIOPORT Port, uint32_t u32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (rc == VINF_SUCCESS)
        {
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, u32));
            rc = parallel_ioport_write (pData, Port, u32);
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));

    return rc;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortRead(PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (rc == VINF_SUCCESS)
        {
            *pu32 = parallel_ioport_read (pData, Port, &rc);
            Log2(("%s: port %#06x val %#04x\n", __FUNCTION__, Port, *pu32));
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        rc = VERR_IOM_IOPORT_UNUSED;

    return rc;
}

#if 0
/**
 * Port I/O Handler for OUT operations on ECP registers.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortWriteECP(PPDMDEVINS pDevIns, void *pvUser,
                                          RTIOPORT Port, uint32_t u32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_WRITE);
        if (rc == VINF_SUCCESS)
        {
            Log2(("%s: ecp port %#06x val %#04x\n", __FUNCTION__, Port, u32));
            rc = parallel_ioport_write_ecp (pData, Port, u32);
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));

    return rc;
}

/**
 * Port I/O Handler for IN operations on ECP registers.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) parallelIOPortReadECP(PPDMDEVINS pDevIns, void *pvUser,
                                         RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    int          rc = VINF_SUCCESS;

    if (cb == 1)
    {
        rc = PDMCritSectEnter(&pData->CritSect, VINF_IOM_HC_IOPORT_READ);
        if (rc == VINF_SUCCESS)
        {
            *pu32 = parallel_ioport_read_ecp (pData, Port, &rc);
            Log2(("%s: ecp port %#06x val %#04x\n", __FUNCTION__, Port, *pu32));
            PDMCritSectLeave(&pData->CritSect);
        }
    }
    else
        rc = VERR_IOM_IOPORT_UNUSED;

    return rc;
}
#endif

#ifdef IN_RING3
/**
 * Saves a state of the serial port device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) parallelSaveExec(PPDMDEVINS pDevIns,
                                          PSSMHANDLE pSSMHandle)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    SSMR3PutU8(pSSMHandle, pData->reg_data);
    SSMR3PutU8(pSSMHandle, pData->reg_status);
    SSMR3PutU8(pSSMHandle, pData->reg_control);
    SSMR3PutS32(pSSMHandle, pData->irq);
    SSMR3PutU32(pSSMHandle, pData->base);

    return SSMR3PutU32(pSSMHandle, ~0); /* sanity/terminator */
}

/**
 * Loads a saved serial port device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) parallelLoadExec(PPDMDEVINS pDevIns,
                                          PSSMHANDLE pSSMHandle,
                                          uint32_t u32Version)
{
    int          rc;
    uint32_t     u32;
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    if (u32Version != PARALLEL_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    SSMR3GetU8(pSSMHandle, &pData->reg_data);
    SSMR3GetU8(pSSMHandle, &pData->reg_status);
    SSMR3GetU8(pSSMHandle, &pData->reg_control);
    SSMR3GetS32(pSSMHandle, &pData->irq);
    SSMR3GetU32(pSSMHandle, &pData->base);

    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (VBOX_FAILURE(rc))
        return rc;

    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x expected ~0\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    return VINF_SUCCESS;
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) parallelRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);
    pData->pDevInsGC     += offDelta;
}

/** @copyfrom PIBASE::pfnqueryInterface */
static DECLCALLBACK(void *) parallelQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    ParallelState *pData = PDMIBASE_2_PARALLELSTATE(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        case PDMINTERFACE_HOST_PARALLEL_PORT:
            return &pData->IHostParallelPort;
        default:
            return NULL;
    }
}

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) parallelDestruct(PPDMDEVINS pDevIns)
{
    ParallelState *pData = PDMINS2DATA(pDevIns, ParallelState *);

    PDMR3CritSectDelete(&pData->CritSect);

    return VINF_SUCCESS;
}


/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) parallelConstruct(PPDMDEVINS pDevIns,
                                           int iInstance,
                                           PCFGMNODE pCfgHandle)
{
    int            rc;
    ParallelState  *pData = PDMINS2DATA(pDevIns, ParallelState*);
    uint16_t       io_base;
    uint8_t        irq_lvl;

    Assert(iInstance < 4);

    pData->pDevInsHC = pDevIns;
    pData->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0IOBase\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: Unknown config key"));

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &pData->fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"GCEnabled\" value"));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &pData->fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"R0Enabled\" value"));

    /* IBase */
    pData->IBase.pfnQueryInterface = parallelQueryInterface;

    /* IHostParallelPort */
    pData->IHostParallelPort.pfnNotifyInterrupt = parallelNotifyInterrupt;

    rc = RTSemEventCreate(&pData->ReceiveSem);
    AssertRC(rc);

    /*
     * Initialize critical section.
     * This must of course be done before attaching drivers or anything else which can call us back..
     */
    char szName[24];
    RTStrPrintf(szName, sizeof(szName), "Parallel#%d", iInstance);
    rc = PDMDevHlpCritSectInit(pDevIns, &pData->CritSect, szName);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = CFGMR3QueryU8(pCfgHandle, "IRQ", &irq_lvl);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        irq_lvl = 7;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));

    rc = CFGMR3QueryU16(pCfgHandle, "IOBase", &io_base);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        io_base = 0x378;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IOBase\" value"));

    Log(("parallelConstruct instance %d iobase=%04x irq=%d\n", iInstance, io_base, irq_lvl));

    pData->irq = irq_lvl;
    pData->base = io_base;
    
    /* Init parallel state */
    pData->reg_data = 0;
    pData->reg_ecp_ecr = LPT_ECP_ECR_CHIPMODE_COMPAT | LPT_ECP_ECR_FIFO_EMPTY;
    pData->act_fifo_pos_read = 0;
    pData->act_fifo_pos_write = 0;

    rc = PDMDevHlpIOPortRegister(pDevIns, io_base, 8, 0,
                                 parallelIOPortWrite, parallelIOPortRead,
                                 NULL, NULL, "PARALLEL");
    if (VBOX_FAILURE(rc))
        return rc;

#if 0
    /* register ecp registers */
    rc = PDMDevHlpIOPortRegister(pDevIns, io_base+0x400, 8, 0,
                                 parallelIOPortWriteECP, parallelIOPortReadECP,
                                 NULL, NULL, "PARALLEL ECP");
    if (VBOX_FAILURE(rc))
        return rc;
#endif

    if (pData->fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, io_base, 8, 0, "parallelIOPortWrite",
                                      "parallelIOPortRead", NULL, NULL, "Parallel");
        if (VBOX_FAILURE(rc))
            return rc;

#if 0
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, io_base+0x400, 8, 0, "parallelIOPortWriteECP",
                                      "parallelIOPortReadECP", NULL, NULL, "Parallel Ecp");
        if (VBOX_FAILURE(rc))
            return rc;
#endif
    }

    if (pData->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, io_base, 8, 0, "parallelIOPortWrite",
                                      "parallelIOPortRead", NULL, NULL, "Parallel");
        if (VBOX_FAILURE(rc))
            return rc;

#if 0
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, io_base+0x400, 8, 0, "parallelIOPortWriteECP",
                                      "parallelIOPortReadECP", NULL, NULL, "Parallel Ecp");
        if (VBOX_FAILURE(rc))
            return rc;
#endif
    }

    /* Attach the parallel port driver and get the interfaces. For now no run-time
     * changes are supported. */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pData->IBase, &pData->pDrvBase, "Parallel Host");
    if (VBOX_SUCCESS(rc))
    {
        pData->pDrvHostParallelConnector = (PDMIHOSTPARALLELCONNECTOR *)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, 
                                                                                                           PDMINTERFACE_HOST_PARALLEL_CONNECTOR);
        if (!pData->pDrvHostParallelConnector)
        {
            AssertMsgFailed(("Configuration error: instance %d has no host parallel interface!\n", iInstance));
            return VERR_PDM_MISSING_INTERFACE;
        }
        /** @todo provide read notification interface!!!! */
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pData->pDrvBase = NULL;
        pData->pDrvHostParallelConnector = NULL;
        LogRel(("Parallel%d: no unit\n", iInstance));
    }
    else
    {
        AssertMsgFailed(("Parallel%d: Failed to attach to host driver. rc=%Vrc\n", iInstance, rc));
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Parallel device %d cannot attach to host driver\n"), iInstance);
    }

    /* Set compatibility mode */
    pData->pDrvHostParallelConnector->pfnSetMode(pData->pDrvHostParallelConnector, PDM_PARALLEL_PORT_MODE_COMPAT);
    /* Get status of control register */
    pData->pDrvHostParallelConnector->pfnReadControl(pData->pDrvHostParallelConnector, &pData->reg_control);

    rc = PDMDevHlpSSMRegister(
        pDevIns,                        /* pDevIns */
        pDevIns->pDevReg->szDeviceName, /* pszName */
        iInstance,                      /* u32Instance */
        PARALLEL_SAVED_STATE_VERSION,   /* u32Version */
        sizeof (*pData),                /* cbGuess */
        NULL,                           /* pfnSavePrep */
        parallelSaveExec,               /* pfnSaveExec */
        NULL,                           /* pfnSaveDone */
        NULL,                           /* pfnLoadPrep */
        parallelLoadExec,               /* pfnLoadExec */
        NULL                            /* pfnLoadDone */
        );
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceParallelPort =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "parallel",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Parallel Communication Port",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_PARALLEL,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(ParallelState),
    /* pfnConstruct */
    parallelConstruct,
    /* pfnDestruct */
    parallelDestruct,
    /* pfnRelocate */
    parallelRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL
};
#endif /* IN_RING3 */


#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
