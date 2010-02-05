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


/**
 * PDM scatter/gather buffer.
 *
 * @todo Promote this to VBox/types.h, VBox/pdmcommon.h or some such place.
 */
typedef struct PDMSCATTERGATHER
{
    /** Flags. */
    size_t          fFlags;
    /** The number of bytes used.
     * This is cleared on alloc and set by the user. */
    size_t          cbUsed;
    /** The number of bytes available.
     * This is set on alloc and not changed by the user. */
    size_t          cbAvailable;
    /** Private data member for the allocator side. */
    void           *pvAllocator;
    /** Private data member for the user side. */
    void           *pvUser;
    /** The number of segments
     * This is set on alloc and not changed by the user. */
    size_t          cSegs;
    /** Variable sized array of segments. */
    PDMDATASEG      aSegs[1];
} PDMSCATTERGATHER;
/** Pointer to a PDM scatter/gather buffer. */
typedef PDMSCATTERGATHER *PPDMSCATTERGATHER;
/** Pointer to a PDM scatter/gather buffer pointer. */
typedef PPDMSCATTERGATHER *PPPDMSCATTERGATHER;


/** @name PDMSCATTERGATHER::fFlags
 * @{  */
/** Magic value. */
#define PDMSCATTERGATHER_FLAGS_MAGIC        UINT32_C(0xb1b10000)
/** Magic mask. */
#define PDMSCATTERGATHER_FLAGS_MAGIC_MASK   UINT32_C(0xffff0000)
/** Owned by owner number 1. */
#define PDMSCATTERGATHER_FLAGS_OWNER_1      UINT32_C(0x00000001)
/** Owned by owner number 2. */
#define PDMSCATTERGATHER_FLAGS_OWNER_2      UINT32_C(0x00000002)
/** Owned by owner number 3. */
#define PDMSCATTERGATHER_FLAGS_OWNER_3      UINT32_C(0x00000002)
/** Owner mask. */
#define PDMSCATTERGATHER_FLAGS_OWNER_MASK   UINT32_C(0x00000003)
/** @} */


/**
 * Sets the owner of a scatter/gather buffer.
 *
 * @param   pSgBuf              .
 * @param   uNewOwner           The new owner.
 */
DECLINLINE(void) PDMScatterGatherSetOwner(PPDMSCATTERGATHER pSgBuf, uint32_t uNewOwner)
{
    pSgBuf->fFlags = (pSgBuf->fFlags & ~PDMSCATTERGATHER_FLAGS_OWNER_MASK) | uNewOwner;
}



/** Pointer to a network port interface */
typedef struct PDMINETWORKDOWN *PPDMINETWORKDOWN;
/**
 * Network port interface (down).
 * Pair with PDMINETWORKUP.
 */
typedef struct PDMINETWORKDOWN
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
     *
     * @thread  Non-EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnWaitReceiveAvail,(PPDMINETWORKDOWN pInterface, RTMSINTERVAL cMillies));

    /**
     * Receive data from the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           The available data.
     * @param   cb              Number of bytes available in the buffer.
     *
     * @thread  Non-EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnReceive,(PPDMINETWORKDOWN pInterface, const void *pvBuf, size_t cb));


    /**
     * Called when there is a buffered of the required size available.
     *
     * When a PDMINETWORKUP::pfnAllocBuf call fails with VERR_TRY_AGAIN, the
     * driver will notify the device/driver up stream when a large enough buffer
     * becomes available via this method.
     *
     * @param   pInterface      Pointer to this interface.
     * @param   pSgBuf          Scatter/gather buffer of the size previously
     *                          requested.  Pass to PDMINETWORKUP::pfnSendBuf or
     *                          PDMINETWORKUP::pfnFreeBuf.
     *
     * @thread  Non-EMT.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyBufAvailable,(PPDMINETWORKDOWN pInterface));

} PDMINETWORKDOWN;
/** PDMINETWORKDOWN inteface ID. */
#define PDMINETWORKDOWN_IID                     "eb66670b-7998-4470-8e72-886e30f6a9c3"


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
typedef struct PDMINETWORKUP *PPDMINETWORKUP;
/**
 * Network connector interface (up).
 * Pair with PDMINETWORKDOWN.
 */
typedef struct PDMINETWORKUP
{
    /**
     * Get a send buffer for passing to pfnSendBuf.
     *
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_TRY_AGAIN if temporarily out of buffer space.  After this
     *          happens, the driver will call PDMINETWORKDOWN::pfnNotifyBufAvailable
     *          when this is a buffer of the required size available.
     * @retval  VERR_NO_MEMORY if really out of buffer space.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cbMin           The minimum buffer size.
     * @param   ppSgBuf         Where to return the buffer.  The buffer will be
     *                          owned by the caller, designation owner number 1.
     *
     * @thread  Any, but normally EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnAllocBuf,(PPDMINETWORKUP pInterface, size_t cbMin, PPPDMSCATTERGATHER ppSgBuf));

    /**
     * Frees an unused buffer.
     *
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_TRY_AGAIN if temporarily out of buffer space.  After this
     *          happens, the driver will call PDMINETWORKDOWN::pfnNotifyBufAvailable
     *          when this is a buffer of the required size available.
     * @retval  VERR_NO_MEMORY if really out of buffer space.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pSgBuf          A buffer from PDMINETWORKUP::pfnAllocBuf or
     *                          PDMINETWORKDOWN::pfnNotifyBufAvailable.  The buffer
     *                          ownership shall be 1.
     *
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(int, pfnFreeBuf,(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf));

    /**
     * Send data to the network.
     *
     * @retval  VINF_SUCCESS on success.
     * @retval  VERR_NET_NO_NETWORK if the NIC is not connected to a network.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pSgBuf          The buffer containing the data to send.  The buffer
     *                          ownership shall be 1.  Upon sucessfull return the
     *                          buffer will be owned by the downstream driver and
     *                          have ownership set to 2.  The caller must not ever
     *                          touch it again.  On failure the buffer remains in
     *                          the callers ownership and it should be handed over
     *                          to PDMINETWORKUP::pfnFreeBuf.
     * @param   fOnWorkerThread Set if we're being called on a work thread.  Clear
     *                          if an EMT.
     *
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendBuf,(PPDMINETWORKUP pInterface, PPDMSCATTERGATHER pSgBuf, bool fOnWorkerThread));

    /**
     * Set promiscuous mode.
     *
     * This is called when the promiscuous mode is set. This means that there doesn't have
     * to be a mode change when it's called.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
     * @thread  EMT ??
     */
    DECLR3CALLBACKMEMBER(void, pfnSetPromiscuousMode,(PPDMINETWORKUP pInterface, bool fPromiscuous));

    /**
     * Notification on link status changes.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmLinkState    The new link state.
     * @thread  EMT ??
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyLinkChanged,(PPDMINETWORKUP pInterface, PDMNETWORKLINKSTATE enmLinkState));

    /** @todo Add a callback that informs the driver chain about MAC address changes if we ever implement that.  */

    /**
     * Send data to the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Data to send.
     * @param   cb              Number of bytes to send.
     * @thread  EMT ??
     * @deprecated
     */
    DECLR3CALLBACKMEMBER(int, pfnSendDeprecated,(PPDMINETWORKUP pInterface, const void *pvBuf, size_t cb));


} PDMINETWORKUP;
/** PDMINETWORKUP interface ID. */
#define PDMINETWORKUP_IID                       "f915243e-801a-4868-8979-b6b8594b09cc"


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

