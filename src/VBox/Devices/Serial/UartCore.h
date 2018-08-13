/* $Id$ */
/** @file
 * UartCore - UART  (16550A up to 16950) emulation.
 *
 * The documentation for this device was taken from the PC16550D spec from TI.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___UartCore_h
#define ___UartCore_h

#include <VBox/types.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmserialifs.h>
#include <VBox/vmm/ssm.h>
#include <iprt/assert.h>

RT_C_DECLS_BEGIN

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** The current serial code saved state version. */
#define UART_SAVED_STATE_VERSION              6
/** Saved state version of the legacy code which got replaced after 5.2. */
#define UART_SAVED_STATE_VERSION_LEGACY_CODE  5
/** Includes some missing bits from the previous saved state. */
#define UART_SAVED_STATE_VERSION_MISSING_BITS 4
/** Saved state version when only the 16450 variant was implemented. */
#define UART_SAVED_STATE_VERSION_16450        3

/** Maximum size of a FIFO. */
#define UART_FIFO_LENGTH_MAX                 128


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** Pointer to the UART core state. */
typedef struct UARTCORE *PUARTCORE;


/**
 * UART core IRQ request callback to let the core instance raise/clear interrupt requests.
 *
 * @returns nothing.
 * @param   pDevIns             The owning device instance.
 * @param   pThis               The UART core instance.
 * @param   iLUN                The LUN associated with the UART core.
 * @param   iLvl                The interrupt level.
 */
typedef DECLCALLBACK(void) FNUARTCOREIRQREQ(PPDMDEVINS pDevIns, PUARTCORE pThis, unsigned iLUN, int iLvl);
/** Pointer to a UART core IRQ request callback. */
typedef FNUARTCOREIRQREQ *PFNUARTCOREIRQREQ;


/**
 * UART type.
 */
typedef enum UARTTYPE
{
    /** Invalid UART type. */
    UARTTYPE_INVALID = 0,
    /** 16450 UART type. */
    UARTTYPE_16450,
    /** 16550A UART type. */
    UARTTYPE_16550A,
    /** 16750 UART type. */
    UARTTYPE_16750,
        /** 32bit hack. */
    UARTTYPE_32BIT_HACK = 0x7fffffff
} UARTTYPE;


/**
 * UART FIFO.
 */
typedef struct UARTFIFO
{
    /** Fifo size configured. */
    uint8_t                         cbMax;
    /** Current amount of bytes used. */
    uint8_t                         cbUsed;
    /** Next index to write to. */
    uint8_t                         offWrite;
    /** Next index to read from. */
    uint8_t                         offRead;
    /** The interrupt trigger level (only used for the receive FIFO). */
    uint8_t                         cbItl;
    /** The data in the FIFO. */
    uint8_t                         abBuf[UART_FIFO_LENGTH_MAX];
    /** Alignment to a 4 byte boundary. */
    uint8_t                         au8Alignment0[3];
} UARTFIFO;
/** Pointer to a FIFO. */
typedef UARTFIFO *PUARTFIFO;


/**
 * UART core device.
 *
 * @implements  PDMIBASE
 * @implements  PDMISERIALPORT
 */
typedef struct UARTCORE
{
    /** Access critical section. */
    PDMCRITSECT                     CritSect;
    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** The LUN on the owning device instance for this core. */
    uint32_t                        iLUN;
        /** LUN\#0: The base interface. */
    PDMIBASE                        IBase;
    /** LUN\#0: The serial port interface. */
    PDMISERIALPORT                  ISerialPort;
    /** Pointer to the attached base driver. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached serial driver. */
    R3PTRTYPE(PPDMISERIALCONNECTOR) pDrvSerial;
    /** Configuration flags. */
    uint32_t                        fFlags;
    /** The selected UART type. */
    UARTTYPE                        enmType;

    /** R3 timer pointer fo the character timeout indication. */
    PTMTIMERR3                      pTimerRcvFifoTimeoutR3;
    /** R3 interrupt request callback of the owning device. */
    R3PTRTYPE(PFNUARTCOREIRQREQ)    pfnUartIrqReqR3;
    /** R0 timer pointer fo the character timeout indication. */
    PTMTIMERR0                      pTimerRcvFifoTimeoutR0;
    /** R0 interrupt request callback of the owning device. */
    R0PTRTYPE(PFNUARTCOREIRQREQ)    pfnUartIrqReqR0;
    /** RC timer pointer fo the character timeout indication. */
    PTMTIMERRC                      pTimerRcvFifoTimeoutRC;
        /** RC interrupt request callback of the owning device. */
    RCPTRTYPE(PFNUARTCOREIRQREQ)    pfnUartIrqReqRC;

    /** The divisor register (DLAB = 1). */
    uint16_t                        uRegDivisor;
    /** The Receiver Buffer Register (RBR, DLAB = 0). */
    uint8_t                         uRegRbr;
    /** The Transmitter Holding Register (THR, DLAB = 0). */
    uint8_t                         uRegThr;
    /** The Interrupt Enable Register (IER, DLAB = 0). */
    uint8_t                         uRegIer;
    /** The Interrupt Identification Register (IIR). */
    uint8_t                         uRegIir;
    /** The FIFO Control Register (FCR). */
    uint8_t                         uRegFcr;
    /** The Line Control Register (LCR). */
    uint8_t                         uRegLcr;
    /** The Modem Control Register (MCR). */
    uint8_t                         uRegMcr;
    /** The Line Status Register (LSR). */
    uint8_t                         uRegLsr;
    /** The Modem Status Register (MSR). */
    uint8_t                         uRegMsr;
    /** The Scratch Register (SCR). */
    uint8_t                         uRegScr;

    /** Flag whether a character timeout interrupt is pending
     * (no symbols were inserted or removed from the receive FIFO
     * during an 4 times the character transmit/receive period and the FIFO
     * is not empty). */
    bool                            fIrqCtiPending;
    /** Flag whether the transmitter holding register went empty since last time the
     * IIR register was read. This gets reset when IIR is read so the guest will get this
     * interrupt ID only once. */
    bool                            fThreEmptyPending;
    /** Alignment. */
    bool                            afAlignment[2];
        /** The transmit FIFO. */
    UARTFIFO                        FifoXmit;
    /** The receive FIFO. */
    UARTFIFO                        FifoRecv;

    /** Time it takes to transmit/receive a single symbol in timer ticks. */
    uint64_t                        cSymbolXferTicks;
    /** Number of bytes available for reading from the layer below. */
    volatile uint32_t               cbAvailRdr;

#if defined(IN_RC) || HC_ARCH_BITS == 32
    uint32_t                        uAlignment;
#endif
} UARTCORE;

AssertCompileSizeAlignment(UARTCORE, 8);


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/** Flag whether to yield the CPU on an LSR read. */
#define UART_CORE_YIELD_ON_LSR_READ      RT_BIT_32(0)

/**
 * Performs a register write to the given register offset.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   uReg                The register offset (byte offset) to start writing to.
 * @param   u32                 The value to write.
 * @param   cb                  Number of bytes to write.
 */
DECLHIDDEN(int) uartRegWrite(PUARTCORE pThis, uint32_t uReg, uint32_t u32, size_t cb);

/**
 * Performs a register read from the given register offset.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   uReg                The register offset (byte offset) to start reading from.
 * @param   pu32                Where to store the read value.
 * @param   cb                  Number of bytes to read.
 */
DECLHIDDEN(int) uartRegRead(PUARTCORE pThis, uint32_t uReg, uint32_t *pu32, size_t cb);

# ifdef IN_RING3
/**
 * Initializes the given UART core instance using the provided configuration.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance to initialize.
 * @param   pDevInsR3           The R3 device instance pointer.
 * @param   enmType             The type of UART emulated.
 * @param   iLUN                The LUN the UART should look for attached drivers.
 * @param   fFlags              Additional flags controlling device behavior.
 * @param   pfnUartIrqReqR3     Pointer to the R3 interrupt request callback.
 * @param   pfnUartIrqReqR0     Pointer to the R0 interrupt request callback.
 * @param   pfnUartIrqReqRC     Pointer to the RC interrupt request callback.
 */
DECLHIDDEN(int) uartR3Init(PUARTCORE pThis, PPDMDEVINS pDevInsR3, UARTTYPE enmType, unsigned iLUN, uint32_t fFlags,
                           R3PTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqR3, R0PTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqR0,
                           RCPTRTYPE(PFNUARTCOREIRQREQ) pfnUartIrqReqRC);

/**
 * Destroys the given UART core instance freeing all allocated resources.
 *
 * @returns nothing.
 * @param   pThis               The UART core instance.
 */
DECLHIDDEN(void) uartR3Destruct(PUARTCORE pThis);

/**
 * Detaches any attached driver from the given UART core instance.
 *
 * @returns nothing.
 * @param   pThis               The UART core instance.
 */
DECLHIDDEN(void) uartR3Detach(PUARTCORE pThis);

/**
 * Attaches the given UART core instance to the drivers at the given LUN.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   iLUN                The LUN being attached.
 */
DECLHIDDEN(int) uartR3Attach(PUARTCORE pThis, unsigned iLUN);

/**
 * Resets the given UART core instance.
 *
 * @returns nothing.
 * @param   pThis               The UART core instance.
 */
DECLHIDDEN(void) uartR3Reset(PUARTCORE pThis);

/**
 * Relocates an RC pointers of the given UART core instance
 *
 * @returns nothing.
 * @param   pThis               The UART core instance.
 * @param   offDelta            The delta to relocate RC pointers with.
 */
DECLHIDDEN(void) uartR3Relocate(PUARTCORE pThis, RTGCINTPTR offDelta);

/**
 * Saves the UART state to the given SSM handle.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   pSSM                The SSM handle to save to.
 */
DECLHIDDEN(int) uartR3SaveExec(PUARTCORE pThis, PSSMHANDLE pSSM);

/**
 * Loads the UART state from the given SSM handle.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   pSSM                The SSM handle to load from.
 * @param   uVersion            Saved state version.
 * @param   uPass               The SSM pass the call is done in.
 * @param   puIrq               Where to store the IRQ value for legacy
 *                              saved states - optional.
 * @param   pPortBase           Where to store the I/O port base for legacy
 *                              saved states - optional.
 */
DECLHIDDEN(int) uartR3LoadExec(PUARTCORE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass,
                               uint8_t *puIrq, RTIOPORT *pPortBase);

/**
 * Called when loading the state completed, updates the parameters of any driver underneath.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   pSSM                The SSM handle.
 */
DECLHIDDEN(int) uartR3LoadDone(PUARTCORE pThis, PSSMHANDLE pSSM);

# endif

#endif

RT_C_DECLS_END

#endif
