/* $Id$ */
/** @file
 * VBoxNetAdp - Network Filter Driver (Host), Internal Header.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBoxNetAdpInternal_h___
#define ___VBoxNetAdpInternal_h___

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <iprt/semaphore.h>
#include <iprt/assert.h>


__BEGIN_DECLS

/** Pointer to the globals. */
typedef struct VBOXNETADPGLOBALS *PVBOXNETADPGLOBALS;

#define VBOXNETADP_MAX_INSTANCES   8
#define VBOXNETADP_NAME            "vboxnet"
#define VBOXNETADP_MAX_NAME_LEN    32
#define VBOXNETADP_MTU             1500
#if defined(RT_OS_DARWIN)
# define VBOXNETADP_MAX_FAMILIES   4
# define VBOXNETADP_DETACH_TIMEOUT 500
#endif

#define VBOXNETADP_CTL_DEV_NAME    "vboxnetctl"
#ifdef RT_OS_DARWIN
#define VBOXNETADP_CTL_ADD    _IOR('v', 1, VBOXNETADPREQ)
#define VBOXNETADP_CTL_REMOVE _IOW('v', 2, VBOXNETADPREQ)
#else
#define VBOXNETADP_CTL_ADD    1
#define VBOXNETADP_CTL_REMOVE 2
#endif

typedef struct VBoxNetAdpReq
{
    char szName[VBOXNETADP_MAX_NAME_LEN];
} VBOXNETADPREQ;
typedef VBOXNETADPREQ *PVBOXNETADPREQ;

/**
 * Void entries mark vacant slots in adapter array. Valid entries are busy slots.
 * As soon as slot is being modified its state changes to transitional.
 * An entry in transitional state must only be accessed by the thread that
 * put it to this state.
 */
/**
 * To avoid races on adapter fields we stick to the following rules:
 * - rewrite: Int net port calls are serialized
 * - No modifications are allowed on busy adapters (deactivate first)
 *     Refuse to destroy adapter until it gets to available state
 * - No transfers (thus getting busy) on inactive adapters
 * - Init sequence: void->available->connected->active
     1) Create
     2) Connect
     3) Activate
 * - Destruction sequence: active->connected->available->void
     1) Deactivate
     2) Disconnect
     3) Destroy
*/

enum VBoxNetAdpState
{
    kVBoxNetAdpState_Invalid,
    kVBoxNetAdpState_Transitional,
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
    kVBoxNetAdpState_Available,
    kVBoxNetAdpState_Connected,
#endif /* VBOXANETADP_DO_NOT_USE_NETFLT */
    kVBoxNetAdpState_Active,
    kVBoxNetAdpState_U32Hack = 0xFFFFFFFF
};
typedef enum VBoxNetAdpState VBOXNETADPSTATE;

struct VBoxNetAdapter
{
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
    /** The spinlock protecting the state variables and host interface handle. */
    RTSPINLOCK        hSpinlock;

    /* --- Protected with spinlock. --- */

#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */
    /** Denotes availability of this slot in adapter array. */
    VBOXNETADPSTATE   enmState;
#ifdef VBOXANETADP_DO_NOT_USE_NETFLT

    /* --- Unprotected. Atomic access. --- */

    /** Reference count. */
    uint32_t volatile cRefs;
    /** The busy count.
     * This counts the number of current callers and pending packet. */
    uint32_t volatile cBusy;

    /* --- Unprotected. Do not modify when cBusy > 0. --- */

    /** Our RJ-45 port.
     * This is what the internal network plugs into. */
    INTNETTRUNKIFPORT MyPort;
    /** The RJ-45 port on the INTNET "switch".
     * This is what we're connected to. */
    PINTNETTRUNKSWPORT pSwitchPort;
    /** Pointer to the globals. */
    PVBOXNETADPGLOBALS pGlobals;
    /** The event that is signaled when we go idle and that pfnWaitForIdle blocks on. */
    RTSEMEVENT        hEventIdle;
#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */
    /** Corresponds to the digit at the end of device name. */
    uint8_t           uUnit;

    union
    {
#ifdef VBOXNETADP_OS_SPECFIC
        struct
        {
# if defined(RT_OS_DARWIN)
            /** @name Darwin instance data.
             * @{ */
            /** Event to signal detachment of interface. */
            RTSEMEVENT        hEvtDetached;
            /** Pointer to Darwin interface structure. */
            ifnet_t           pIface;
            /** MAC address. */
            RTMAC             Mac;
            /** Protocol families attached to this adapter. */
            protocol_family_t aAttachedFamilies[VBOXNETADP_MAX_FAMILIES];
# else
# error PORTME
# endif
        } s;
#endif
        /** Padding. */
#if defined(RT_OS_WINDOWS)
# if defined(VBOX_NETFLT_ONDEMAND_BIND)
        uint8_t abPadding[192];
# else
        uint8_t abPadding[1024];
# endif
#elif defined(RT_OS_LINUX)
        uint8_t abPadding[320];
#else
        uint8_t abPadding[64];
#endif
    } u;
    /** The interface name. */
    char szName[VBOXNETADP_MAX_NAME_LEN];
};
typedef struct VBoxNetAdapter VBOXNETADP;
typedef VBOXNETADP *PVBOXNETADP;

#ifdef VBOXANETADP_DO_NOT_USE_NETFLT
/**
 * The global data of the VBox filter driver.
 *
 * This contains the bit required for communicating with support driver, VBoxDrv
 * (start out as SupDrv).
 */
typedef struct VBOXNETADPGLOBALS
{
    /** Mutex protecting the list of instances and state changes. */
    RTSEMFASTMUTEX hFastMtx;
    /** Array of adapter instances. */
    VBOXNETADP aAdapters[VBOXNETADP_MAX_INSTANCES];

    /** The INTNET trunk network interface factory. */
    INTNETTRUNKFACTORY TrunkFactory;
    /** The SUPDRV component factory registration. */
    SUPDRVFACTORY SupDrvFactory;
    /** The number of current factory references. */
    int32_t volatile cFactoryRefs;
    /** The SUPDRV IDC handle (opaque struct). */
    SUPDRVIDCHANDLE SupDrvIDC;
} VBOXNETADPGLOBALS;


DECLHIDDEN(void) vboxNetAdpComposeMACAddress(PVBOXNETADP pThis, PRTMAC pMac);
DECLHIDDEN(void) vboxNetAdpReceive(PVBOXNETADP pThis, PINTNETSG pSG);
DECLHIDDEN(bool) vboxNetAdpPrepareToReceive(PVBOXNETADP pThis);
DECLHIDDEN(void) vboxNetAdpCancelReceive(PVBOXNETADP pThis);

DECLHIDDEN(int) vboxNetAdpInitGlobals(PVBOXNETADPGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetAdpTryDeleteGlobals(PVBOXNETADPGLOBALS pGlobals);
DECLHIDDEN(bool) vboxNetAdpCanUnload(PVBOXNETADPGLOBALS pGlobals);

DECLHIDDEN(void) vboxNetAdpRetain(PVBOXNETADP pThis);
DECLHIDDEN(void) vboxNetAdpRelease(PVBOXNETADP pThis);
DECLHIDDEN(void) vboxNetAdpBusy(PVBOXNETADP pThis);
DECLHIDDEN(void) vboxNetAdpIdle(PVBOXNETADP pThis);

DECLHIDDEN(int) vboxNetAdpInitGlobalsBase(PVBOXNETADPGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetAdpInitIdc(PVBOXNETADPGLOBALS pGlobals);
DECLHIDDEN(void) vboxNetAdpDeleteGlobalsBase(PVBOXNETADPGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetAdpTryDeleteIdc(PVBOXNETADPGLOBALS pGlobals);
                 


/** @name The OS specific interface.
 * @{ */
/**
 * Transmits a frame.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 * @param   pSG             The (scatter/)gather list.
 * @param   fDst            The destination mask. At least one bit will be set.
 *
 * @remarks Owns the out-bound trunk port semaphore.
 */
DECLHIDDEN(int) vboxNetAdpPortOsXmit(PVBOXNETADP pThis, PINTNETSG pSG, uint32_t fDst);

/**
 * Checks if the interface is in promiscuous mode from the host perspective.
 *
 * If it is, then the internal networking switch will send frames
 * heading for the wire to the host as well.
 *
 * @see INTNETTRUNKIFPORT::pfnIsPromiscuous for more details.
 *
 * @returns true / false accordingly.
 * @param   pThis           The instance.
 *
 * @remarks Owns the network lock and the out-bound trunk port semaphores.
 */
DECLHIDDEN(bool) vboxNetAdpPortOsIsPromiscuous(PVBOXNETADP pThis);

/**
 * Get the MAC address of the interface we're attached to.
 *
 * Used by the internal networking switch for implementing the
 * shared-MAC-on-the-wire mode.
 *
 * @param   pThis           The instance.
 * @param   pMac            Where to store the MAC address.
 *                          If you don't know, set all the bits except the first (the multicast one).
 *
 * @remarks Owns the network lock and the out-bound trunk port semaphores.
 */
DECLHIDDEN(void) vboxNetAdpPortOsGetMacAddress(PVBOXNETADP pThis, PRTMAC pMac);

/**
 * Checks if the specified MAC address is for any of the host interfaces.
 *
 * Used by the internal networking switch to decide the destination(s)
 * of a frame.
 *
 * @returns true / false accordingly.
 * @param   pThis           The instance.
 * @param   pMac            The MAC address.
 *
 * @remarks Owns the network lock and the out-bound trunk port semaphores.
 */
DECLHIDDEN(bool) vboxNetAdpPortOsIsHostMac(PVBOXNETADP pThis, PCRTMAC pMac);

/**
 * This is called to when disconnecting from a network.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks May own the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(int) vboxNetAdpOsDisconnectIt(PVBOXNETADP pThis);

/**
 * This is called to when connecting to a network.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks Owns the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(int) vboxNetAdpOsConnectIt(PVBOXNETADP pThis);

/**
 * This is called to perform OS-specific structure initializations.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vboxNetAdpOsInit(PVBOXNETADP pThis);

/**
 * Counter part to vboxNetAdpOsCreate().
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks May own the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(void) vboxNetAdpOsDestroy(PVBOXNETADP pThis);

/**
 * This is called to attach to the actual host interface
 * after linking the instance into the list.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 * @param   pMac            The MAC address to use for this instance.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vboxNetAdpOsCreate(PVBOXNETADP pThis, PCRTMAC pMac);

/** @} */
#endif /* !VBOXANETADP_DO_NOT_USE_NETFLT */


__END_DECLS

#endif

