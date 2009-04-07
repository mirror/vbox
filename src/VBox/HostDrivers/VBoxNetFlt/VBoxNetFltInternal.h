/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Internal Header.
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

#ifndef ___VBoxNetFltInternal_h___
#define ___VBoxNetFltInternal_h___

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <iprt/semaphore.h>
#include <iprt/assert.h>


__BEGIN_DECLS

/** Pointer to the globals. */
typedef struct VBOXNETFLTGLOBALS *PVBOXNETFLTGLOBALS;


/**
 * The state of a filter driver instance.
 *
 * The state machine differs a bit between the platforms because of
 * the way we hook into the stack. On some hosts we can dynamically
 * attach when required (on CreateInstance) and on others we will
 * have to connect when the network stack is bound up. These modes
 * are called static and dynamic config and governed at compile time
 * by the VBOXNETFLT_STATIC_CONFIG define.
 *
 * See sec_netflt_msc for more details on locking and synchronization.
 */
typedef enum VBOXNETFTLINSSTATE
{
    /** The usual invalid state. */
    kVBoxNetFltInsState_Invalid = 0,
    /** Initialization.
     * We've reserved the interface name but need to attach to the actual
     * network interface outside the lock to avoid deadlocks.
     * In the dynamic case this happens during a Create(Instance) call.
     * In the static case it happens during driver initialization. */
    kVBoxNetFltInsState_Initializing,
#ifdef VBOXNETFLT_STATIC_CONFIG
    /** Unconnected, not hooked up to a switch (static only).
     * The filter driver instance has been instantiated and hooked up,
     * waiting to be connected to an internal network. */
    kVBoxNetFltInsState_Unconnected,
#endif
    /** Connected to an internal network. */
    kVBoxNetFltInsState_Connected,
    /** Disconnecting from the internal network and possibly the host network interface.
     * Partly for reasons of deadlock avoidance again. */
    kVBoxNetFltInsState_Disconnecting,
    /** The instance has been disconnected from both the host and the internal network. */
    kVBoxNetFltInsState_Destroyed,

    /** The habitual 32-bit enum hack.  */
    kVBoxNetFltInsState_32BitHack = 0x7fffffff
} VBOXNETFTLINSSTATE;


/**
 * The per-instance data of the VBox filter driver.
 *
 * This is data associated with a network interface / NIC / wossname which
 * the filter driver has been or may be attached to. When possible it is
 * attached dynamically, but this may not be possible on all OSes so we have
 * to be flexible about things.
 *
 * A network interface / NIC / wossname can only have one filter driver
 * instance attached to it. So, attempts at connecting an internal network
 * to an interface that's already in use (connected to another internal network)
 * will result in a VERR_SHARING_VIOLATION.
 *
 * Only one internal network can connect to a filter driver instance.
 */
typedef struct VBOXNETFLTINS
{
    /** Pointer to the next interface in the list. (VBOXNETFLTGLOBAL::pInstanceHead) */
    struct VBOXNETFLTINS *pNext;
    /** Our RJ-45 port.
     * This is what the internal network plugs into. */
    INTNETTRUNKIFPORT MyPort;
    /** The RJ-45 port on the INTNET "switch".
     * This is what we're connected to. */
    PINTNETTRUNKSWPORT pSwitchPort;
    /** Pointer to the globals. */
    PVBOXNETFLTGLOBALS pGlobals;

    /** The spinlock protecting the state variables and host interface handle. */
    RTSPINLOCK hSpinlock;
    /** The current interface state. */
    VBOXNETFTLINSSTATE volatile enmState;
    /** Active / Suspended indicator. */
    bool volatile fActive;
    /** Disconnected from the host network interface. */
    bool volatile fDisconnectedFromHost;
    /** Rediscovery is pending.
     * cBusy will never reach zero during rediscovery, so which
     * takes care of serializing rediscovery and disconnecting. */
    bool volatile fRediscoveryPending;
    /** Whether we should not attempt to set promiscuous mode at all. */
    bool fDisablePromiscuous;
#if (ARCH_BITS == 32) && defined(__GNUC__)
    uint32_t u32Padding;    /**< Alignment padding, will assert in ASMAtomicUoWriteU64 otherwise. */
#endif
    /** The timestamp of the last rediscovery. */
    uint64_t volatile NanoTSLastRediscovery;
    /** Reference count. */
    uint32_t volatile cRefs;
    /** The busy count.
     * This counts the number of current callers and pending packet. */
    uint32_t volatile cBusy;
    /** The event that is signaled when we go idle and that pfnWaitForIdle blocks on. */
    RTSEMEVENT hEventIdle;

    union
    {
#ifdef VBOXNETFLT_OS_SPECFIC
        struct
        {
# if defined(RT_OS_DARWIN)
            /** @name Darwin instance data.
             * @{ */
            /** Pointer to the darwin network interface we're attached to.
             * This is treated as highly volatile and should only be read and retained
             * while owning hSpinlock. Releasing references to this should not be done
             * while owning it though as we might end up destroying it in some paths. */
            ifnet_t volatile pIfNet;
            /** The interface filter handle.
             * Same access rules as with pIfNet. */
            interface_filter_t volatile pIfFilter;
            /** Whether we've need to set promiscuous mode when the interface comes up. */
            bool volatile fNeedSetPromiscuous;
            /** Whether we've successfully put the interface into to promiscuous mode.
             * This is for dealing with the ENETDOWN case. */
            bool volatile fSetPromiscuous;
            /** The MAC address of the interface. */
            RTMAC Mac;
            /** @} */
# elif defined(RT_OS_LINUX)
            /** @name Linux instance data
             * @{ */
            /** Pointer to the device. */
            struct net_device volatile *pDev;
            /** Whether we've successfully put the interface into to promiscuous mode.
             * This is for dealing with the ENETDOWN case. */
            bool volatile fPromiscuousSet;
            /** Whether device exists and physically attached. */
            bool volatile fRegistered;
            /** The MAC address of the interface. */
            RTMAC Mac;
            struct notifier_block Notifier;
            struct packet_type    PacketType;
            struct sk_buff_head   XmitQueue;
            struct work_struct    XmitTask;
            /** @} */
# elif defined(RT_OS_SOLARIS)
            /** @name Solaris instance data.
             * @{ */
            /** Pointer to the bound IPv4 stream. */
            void volatile *pvIp4Stream;
            /** Pointer to the bound IPv6 stream. */
            void volatile *pvIp6Stream;
            /** Pointer to the bound ARP stream. */
            void volatile *pvArpStream;
            /** Pointer to the unbound promiscuous stream. */
            void volatile *pvPromiscStream;
            /** Layered device handle to the interface. */
            ldi_handle_t hIface;
            /** The MAC address of the interface. */
            RTMAC Mac;
            /** Mutex protection used for loopback. */
            RTSEMFASTMUTEX hFastMtx;
            /** @} */
# elif defined(RT_OS_WINDOWS)
            /** @name Windows instance data.
             * @{ */
            /** Filter driver device context. */
            ADAPT IfAdaptor;
            /** Packet worker thread info */
            PACKET_QUEUE_WORKER PacketQueueWorker;
            /** The MAC address of the interface. Caching MAC for performance reasons. */
            RTMAC Mac;
            /** mutex used to synchronize ADAPT init/deinit */
            RTSEMMUTEX hAdaptMutex;
            /** @}  */
# else
#  error "PORTME"
# endif
        } s;
#endif
        /** Padding. */
#if defined(RT_OS_WINDOWS)
# if defined(VBOX_NETFLT_ONDEMAND_BIND)
        uint8_t abPadding[192];
# elif defined(VBOXNETADP)
        uint8_t abPadding[128];
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
    char szName[1];
} VBOXNETFLTINS;
/** Pointer to the instance data of a host network filter driver. */
typedef struct VBOXNETFLTINS *PVBOXNETFLTINS;

AssertCompileMemberAlignment(VBOXNETFLTINS, NanoTSLastRediscovery, 8);
#ifdef VBOXNETFLT_OS_SPECFIC
AssertCompile(RT_SIZEOFMEMB(VBOXNETFLTINS, u.s) <= RT_SIZEOFMEMB(VBOXNETFLTINS, u.abPadding));
#endif


/**
 * The global data of the VBox filter driver.
 *
 * This contains the bit required for communicating with support driver, VBoxDrv
 * (start out as SupDrv).
 */
typedef struct VBOXNETFLTGLOBALS
{
    /** Mutex protecting the list of instances and state changes. */
    RTSEMFASTMUTEX hFastMtx;
    /** Pointer to a list of instance data. */
    PVBOXNETFLTINS pInstanceHead;

    /** The INTNET trunk network interface factory. */
    INTNETTRUNKFACTORY TrunkFactory;
    /** The SUPDRV component factory registration. */
    SUPDRVFACTORY SupDrvFactory;
    /** The number of current factory references. */
    int32_t volatile cFactoryRefs;
    /** Whether the IDC connection is open or not.
     * This is only for cleaning up correctly after the separate IDC init on Windows. */
    bool fIDCOpen;
    /** The SUPDRV IDC handle (opaque struct). */
    SUPDRVIDCHANDLE SupDrvIDC;
} VBOXNETFLTGLOBALS;


DECLHIDDEN(int) vboxNetFltInitGlobalsAndIdc(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetFltInitGlobals(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetFltInitIdc(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetFltTryDeleteIdcAndGlobals(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(void) vboxNetFltDeleteGlobals(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(int) vboxNetFltTryDeleteIdc(PVBOXNETFLTGLOBALS pGlobals);

DECLHIDDEN(bool) vboxNetFltCanUnload(PVBOXNETFLTGLOBALS pGlobals);
DECLHIDDEN(PVBOXNETFLTINS) vboxNetFltFindInstance(PVBOXNETFLTGLOBALS pGlobals, const char *pszName);

DECLHIDDEN(void) vboxNetFltRetain(PVBOXNETFLTINS pThis, bool fBusy);
DECLHIDDEN(void) vboxNetFltRelease(PVBOXNETFLTINS pThis, bool fBusy);

#ifdef VBOXNETFLT_STATIC_CONFIG
DECLHIDDEN(int) vboxNetFltSearchCreateInstance(PVBOXNETFLTGLOBALS pGlobals, const char *pszName, PVBOXNETFLTINS *ppInstance, void * pContext);
#endif



/** @name The OS specific interface.
 * @{ */
/**
 * Try rediscover the host interface.
 *
 * This is called periodically from the transmit path if we're marked as
 * disconnected from the host. There is no chance of a race here.
 *
 * @returns true if the interface was successfully rediscovered and reattach,
 *          otherwise false.
 * @param   pThis           The new instance.
 */
DECLHIDDEN(bool) vboxNetFltOsMaybeRediscovered(PVBOXNETFLTINS pThis);

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
DECLHIDDEN(int) vboxNetFltPortOsXmit(PVBOXNETFLTINS pThis, PINTNETSG pSG, uint32_t fDst);

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
DECLHIDDEN(bool) vboxNetFltPortOsIsPromiscuous(PVBOXNETFLTINS pThis);

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
DECLHIDDEN(void) vboxNetFltPortOsGetMacAddress(PVBOXNETFLTINS pThis, PRTMAC pMac);

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
DECLHIDDEN(bool) vboxNetFltPortOsIsHostMac(PVBOXNETFLTINS pThis, PCRTMAC pMac);

/**
 * This is called when activating or suspending the instance.
 *
 * Use this method to enable and disable promiscuous mode on
 * the interface to prevent unnecessary interrupt load.
 *
 * It is only called when the state changes.
 *
 * @param   pThis           The instance.
 *
 * @remarks Owns the lock for the out-bound trunk port.
 */
DECLHIDDEN(void) vboxNetFltPortOsSetActive(PVBOXNETFLTINS pThis, bool fActive);

/**
 * This is called to when disconnecting from a network.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks May own the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(int) vboxNetFltOsDisconnectIt(PVBOXNETFLTINS pThis);

/**
 * This is called to when connecting to a network.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks Owns the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(int) vboxNetFltOsConnectIt(PVBOXNETFLTINS pThis);

/**
 * Counter part to vboxNetFltOsInitInstance().
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks May own the semaphores for the global list, the network lock and the out-bound trunk port.
 */
DECLHIDDEN(void) vboxNetFltOsDeleteInstance(PVBOXNETFLTINS pThis);

/**
 * This is called to attach to the actual host interface
 * after linking the instance into the list.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 * @param   pvContext       The user supplied context in the static config only.
 *                          NULL in the dynamic config.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vboxNetFltOsInitInstance(PVBOXNETFLTINS pThis, void *pvContext);

/**
 * This is called to perform structure initializations.
 *
 * @return  IPRT status code.
 * @param   pThis           The new instance.
 *
 * @remarks Owns no locks.
 */
DECLHIDDEN(int) vboxNetFltOsPreInitInstance(PVBOXNETFLTINS pThis);
/** @} */


__END_DECLS

#endif

