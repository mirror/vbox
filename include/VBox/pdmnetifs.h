/** @file
 * PDM - Pluggable Device Manager, Network Interfaces. (VMM)
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_pdmnetifs_h
#define ___VBox_pdmnetifs_h

#include <VBox/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_ifs_net       PDM Network Interfaces
 * @ingroup grp_pdm_interfaces
 * @{
 */

/** Pointer to a network port interface */
typedef struct PDMINETWORKPORT *PPDMINETWORKPORT;
/**
 * Network port interface (down).
 * Pair with PDMINETWORKCONNECTOR.
 */
typedef struct PDMINETWORKPORT
{
    /**
     * Wait until there is space for receiving data. We do not care how much space is available
     * because pfnReceive() will re-check and notify the guest if necessary.
     *
     * This function must be called before the pfnRecieve() method is called.
     *
     * @returns VBox status code. VINF_SUCCESS means there is at least one receive descriptor available.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cMillies        Number of milliseconds to wait. 0 means return immediately.
     */
    DECLR3CALLBACKMEMBER(int, pfnWaitReceiveAvail,(PPDMINETWORKPORT pInterface, RTMSINTERVAL cMillies));

    /**
     * Receive data from the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           The available data.
     * @param   cb              Number of bytes available in the buffer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnReceive,(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb));

} PDMINETWORKPORT;
/** PDMINETWORKPORT inteface ID. */
#define PDMINETWORKPORT_IID                     "eb66670b-7998-4470-8e72-886e30f6a9c3"


/**
 * Network link state.
 */
typedef enum PDMNETWORKLINKSTATE
{
    /** Invalid state. */
    PDMNETWORKLINKSTATE_INVALID = 0,
    /** The link is up. */
    PDMNETWORKLINKSTATE_UP,
    /** The link is down. */
    PDMNETWORKLINKSTATE_DOWN,
    /** The link is temporarily down while resuming. */
    PDMNETWORKLINKSTATE_DOWN_RESUME
} PDMNETWORKLINKSTATE;


/** Pointer to a network connector interface */
typedef struct PDMINETWORKCONNECTOR *PPDMINETWORKCONNECTOR;
/**
 * Network connector interface (up).
 * Pair with PDMINETWORKPORT.
 */
typedef struct PDMINETWORKCONNECTOR
{
    /**
     * Send data to the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Data to send.
     * @param   cb              Number of bytes to send.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSend,(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb));

    /**
     * Set promiscuous mode.
     *
     * This is called when the promiscuous mode is set. This means that there doesn't have
     * to be a mode change when it's called.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnSetPromiscuousMode,(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous));

    /**
     * Notification on link status changes.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmLinkState    The new link state.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyLinkChanged,(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState));

    /** @todo Add a callback that informs the driver chain about MAC address changes if we ever implement that.  */

} PDMINETWORKCONNECTOR;
/** PDMINETWORKCONNECTOR interface ID. */
#define PDMINETWORKCONNECTOR_IID                "b4b6f850-50d0-4ddf-9efa-daee80194dca"


/** Pointer to a network config port interface */
typedef struct PDMINETWORKCONFIG *PPDMINETWORKCONFIG;
/**
 * Network config port interface (main).
 * No interface pair.
 */
typedef struct PDMINETWORKCONFIG
{
    /**
     * Gets the current Media Access Control (MAC) address.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pMac            Where to store the MAC address.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnGetMac,(PPDMINETWORKCONFIG pInterface, PRTMAC pMac));

    /**
     * Gets the new link state.
     *
     * @returns The current link state.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(PDMNETWORKLINKSTATE, pfnGetLinkState,(PPDMINETWORKCONFIG pInterface));

    /**
     * Sets the new link state.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmState        The new link state
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSetLinkState,(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState));

} PDMINETWORKCONFIG;
/** PDMINETWORKCONFIG interface ID. */
#define PDMINETWORKCONFIG_IID                   "d6d909e8-716d-415d-b109-534e4478ff4e"

/** @} */

RT_C_DECLS_END

#endif

