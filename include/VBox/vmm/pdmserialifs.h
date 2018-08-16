/** @file
 * PDM - Pluggable Device Manager, Serial port related interfaces.
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

#ifndef ___VBox_vmm_pdmserialifs_h
#define ___VBox_vmm_pdmserialifs_h

#include <VBox/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_ifs_serial       PDM Serial Port Interfaces
 * @ingroup grp_pdm_interfaces
 * @{
 */


/** @name Bit mask definitions for status line type.
 * @{ */
#define PDMISERIALPORT_STS_LINE_DCD   RT_BIT(0)
#define PDMISERIALPORT_STS_LINE_RI    RT_BIT(1)
#define PDMISERIALPORT_STS_LINE_DSR   RT_BIT(2)
#define PDMISERIALPORT_STS_LINE_CTS   RT_BIT(3)
/** @} */

/** Pointer to a serial port interface. */
typedef struct PDMISERIALPORT *PPDMISERIALPORT;
/**
 * Serial port interface (down).
 */
typedef struct PDMISERIALPORT
{
    /**
     * Notifies the upper device/driver that data is available for reading.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   cbAvail             The amount of data available to be written.
     */
    DECLR3CALLBACKMEMBER(int, pfnDataAvailRdrNotify, (PPDMISERIALPORT pInterface, size_t cbAvail));

    /**
     * Notifies the upper device/driver that all data was sent.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnDataSentNotify, (PPDMISERIALPORT pInterface));

    /**
     * Try to read data from the device/driver above for writing.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf               Where to store the read data.
     * @param   cbRead              How much to read.
     * @param   pcbRead             Where to store the amount of data actually read on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadWr, (PPDMISERIALPORT pInterface, void *pvBuf, size_t cbRead, size_t *pcbRead));

    /**
     * Notify the device/driver when the status lines changed.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fNewStatusLines New state of the status line pins.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyStsLinesChanged, (PPDMISERIALPORT pInterface, uint32_t fNewStatusLines));

    /**
     * Notify the device/driver that a break condition occurred.
     *
     * @returns VBox statsus code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyBrk, (PPDMISERIALPORT pInterface));

} PDMISERIALPORT;
/** PDMISERIALPORT interface ID. */
#define PDMISERIALPORT_IID                       "44540323-06ca-44c1-8eb2-f5a387704dbd"


/**
 * Supported parity modes.
 */
typedef enum PDMSERIALPARITY
{
    /** Invalid parity setting. */
    PDMSERIALPARITY_INVALID = 0,
    /** No parity. */
    PDMSERIALPARITY_NONE,
    /** Even parity. */
    PDMSERIALPARITY_EVEN,
    /** Odd parity. */
    PDMSERIALPARITY_ODD,
    /** Mark parity. */
    PDMSERIALPARITY_MARK,
    /** Space parity. */
    PDMSERIALPARITY_SPACE,
    /** 32bit hack. */
    PDMSERIALPARITY_32BIT_HACK = 0x7fffffff
} PDMSERIALPARITY;


/**
 * Supported number of stop bits.
 */
typedef enum PDMSERIALSTOPBITS
{
    /** Invalid stop bits setting. */
    PDMSERIALSTOPBITS_INVALID = 0,
    /** One stop bit is used. */
    PDMSERIALSTOPBITS_ONE,
    /** 1.5 stop bits are used. */
    PDMSERIALSTOPBITS_ONEPOINTFIVE,
    /** 2 stop bits are used. */
    PDMSERIALSTOPBITS_TWO,
    /** 32bit hack. */
    PDMSERIALSTOPBITS_32BIT_HACK = 0x7fffffff
} PDMSERIALSTOPBITS;


/** Pointer to a serial interface. */
typedef struct PDMISERIALCONNECTOR *PPDMISERIALCONNECTOR;
/**
 * Serial interface (up).
 * Pairs with PDMISERIALPORT.
 */
typedef struct PDMISERIALCONNECTOR
{
    /**
     * Notifies the lower layer that data is available for writing.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnDataAvailWrNotify, (PPDMISERIALCONNECTOR pInterface));

    /**
     * Try to read data from the underyling driver.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf               Where to store the read data.
     * @param   cbRead              How much to read.
     * @param   pcbRead             Where to store the amount of data actually read on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadRdr, (PPDMISERIALCONNECTOR pInterface, void *pvBuf, size_t cbRead, size_t *pcbRead));

    /**
     * Change device parameters.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   uBps            Speed of the serial connection. (bits per second)
     * @param   enmParity       Parity method.
     * @param   cDataBits       Number of data bits.
     * @param   enmStopBits     Number of stop bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnChgParams, (PPDMISERIALCONNECTOR pInterface, uint32_t uBps,
                                             PDMSERIALPARITY enmParity, unsigned cDataBits,
                                             PDMSERIALSTOPBITS enmStopBits));

    /**
     * Set the state of the modem lines.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   fRts                Set to true to make the Request to Send line active otherwise to 0.
     * @param   fDtr                Set to true to make the Data Terminal Ready line active otherwise 0.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnChgModemLines, (PPDMISERIALCONNECTOR pInterface, bool fRts, bool fDtr));

    /**
     * Changes the TD line into the requested break condition.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   fBrk                Set to true to let the device send a break false to put into normal operation.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnChgBrk, (PPDMISERIALCONNECTOR pInterface, bool fBrk));

    /**
     * Queries the current state of the status lines.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pfStsLines          Where to store the status line states on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStsLines, (PPDMISERIALCONNECTOR pInterface, uint32_t *pfStsLines));

} PDMISERIALCONNECTOR;
/** PDMIMEDIA interface ID. */
#define PDMISERIALCONNECTOR_IID                  "2f16fda0-4980-4ec8-969c-18c1d10b7b95"

/** @} */

RT_C_DECLS_END

#endif
