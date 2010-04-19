/* $Id$ */
/** @file
 * Internal networking - The ring 0 service.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SRV_INTNET
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/pdmnetinline.h>
#include <VBox/sup.h>
#include <VBox/pdm.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/handletable.h>
#include <iprt/net.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def INTNET_WITH_DHCP_SNOOPING
 * Enabled DHCP snooping when in shared-mac-on-the-wire mode. */
#define INTNET_WITH_DHCP_SNOOPING

#if 0
/** Enables the new code - temporarily while doing the rewrite. */
# define WITH_NEW_STUFF
#endif



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifdef WITH_NEW_STUFF
/**
 * MAC address lookup table entry.
 */
typedef struct INTNETMACTABENTRY
{
    /** The MAC address of this entry. */
    RTMAC                   MacAddr;
    /** Is it promiscuous.  */
    bool                    fPromiscuous;
    /** Is it active.
     * We ignore the entry if this is clear and may end up sending packets addressed
     * to this interface onto the trunk.  The reasoning for this is that this could
     * be the interface of a VM that just has been teleported to a different host. */
    bool                    fActive;
    /** Pointer to the network interface. */
    struct INTNETIF        *pIf;
} INTNETMACTABENTRY;
/** Pointer to a MAC address lookup table entry. */
typedef INTNETMACTABENTRY *PINTNETMACTABENTRY;
/** Pointer to a const MAC address lookup table entry. */
typedef INTNETMACTABENTRY const *PCINTNETMACTABENTRY;

/**
 * MAC address lookup table.
 */
typedef struct INTNETMACTAB
{
    /** The spinlock protecting the table, interrupt safe. */
    RTSPINLOCK              hSpinlock;
    /** The current number of entries. */
    uint32_t                cEntries;
    /** The number of entries we've allocated space for. */
    uint32_t                cEntriesAllocated;
    /** Table entries. */
    PCINTNETMACTABENTRY     paEntries;

    /** The host MAC address. */
    RTMAC                   HostMac;
    /** The host promiscous setting. */
    bool                    fHostPromiscuous;
    /** Whether the host is active. */
    bool                    fHostActive;

    /** Whether the wire is promiscuous. */
    bool                    fWirePromiscuous;
    /** Whether the wire is active. */
    bool                    fWireActive;

    /** Pointer to the the trunk interface. */
    struct INTNETTRUNKIF   *pTrunk;
} INTNETMACTAB;
/** Pointer to a MAC address .  */
typedef INTNETMACTAB *PINTNETMACTAB;

/**
 * Destination table.
 */
typedef struct INTNETDSTTAB
{
    /** The trunk destinations. */
    uint32_t                fTrunkDst;
    /** Pointer to the trunk interface (referenced) if fTrunkDst is non-zero. */
    struct INTNETTRUNKIF   *pTrunk;
    /** The number of destination interfaces. */
    uint32_t                cIfs;
    /** The interfaces (referenced).  Variable sized array. */
    struct INTNETIF        *apIfs[1];
} INTNETDSTTAB;
/** Pointer to a destination table. */
typedef INTNETDSTTAB *PINTNETDSTTAB;
#endif /* WITH_NEW_STUFF */


/** Network layer address type. */
typedef enum INTNETADDRTYPE
{
    /** The invalid 0 entry. */
    kIntNetAddrType_Invalid = 0,
    /** IP version 4. */
    kIntNetAddrType_IPv4,
    /** IP version 6. */
    kIntNetAddrType_IPv6,
    /** IPX. */
    kIntNetAddrType_IPX,
    /** The end of the valid values. */
    kIntNetAddrType_End,
    /** The usual 32-bit hack. */
    kIntNetAddrType_32BitHack = 0x7fffffff
} INTNETADDRTYPE;
/** Pointer to a network layer address type. */
typedef INTNETADDRTYPE *PINTNETADDRTYPE;


/**
 * Address and type.
 */
typedef struct INTNETADDR
{
    /** The address type. */
    INTNETADDRTYPE          enmType;
    /** The address. */
    RTNETADDRU              Addr;
} INTNETADDR;
/** Pointer to an address. */
typedef INTNETADDR *PINTNETADDR;
/** Pointer to a const address. */
typedef INTNETADDR const *PCINTNETADDR;


/**
 * Address cache for a specific network layer.
 */
typedef struct INTNETADDRCACHE
{
    /** Pointer to the table of addresses. */
    uint8_t                *pbEntries;
    /** The number of valid address entries. */
    uint8_t                 cEntries;
    /** The number of allocated address entries. */
    uint8_t                 cEntriesAlloc;
    /** The address size. */
    uint8_t                 cbAddress;
    /** The size of an entry. */
    uint8_t                 cbEntry;
} INTNETADDRCACHE;
/** Pointer to an address cache. */
typedef INTNETADDRCACHE *PINTNETADDRCACHE;
/** Pointer to a const address cache. */
typedef INTNETADDRCACHE const *PCINTNETADDRCACHE;


/**
 * A network interface.
 *
 * Unless explicitly stated, all members are protect by the network semaphore.
 */
typedef struct INTNETIF
{
    /** Pointer to the next interface.
     * This is protected by the INTNET::FastMutex. */
    struct INTNETIF        *pNext;
    /** The current MAC address for the interface. (reported or learned)
     * Updated while owning the switch table spinlock.  */
    RTMAC                   Mac;
    /** Set if the INTNET::Mac member is valid. */
    bool                    fMacSet;
    /** Set if the interface is in promiscuous mode.
     * In promiscuous mode the interface will receive all packages except the one it's sending. */
    bool                    fPromiscuous;
    /** Whether the interface is active or not. */
    bool                    fActive;
    /** Whether someone is currently in the destructor. */
    bool volatile           fDestroying;
    /** Number of yields done to try make the interface read pending data.
     * We will stop yielding when this reaches a threshold assuming that the VM is
     * paused or that it simply isn't worth all the delay. It is cleared when a
     * successful send has been done. */
    uint32_t                cYields;
    /** Pointer to the current exchange buffer (ring-0). */
    PINTNETBUF              pIntBuf;
    /** Pointer to ring-3 mapping of the current exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufR3;
    /** Pointer to the default exchange buffer for the interface. */
    PINTNETBUF              pIntBufDefault;
    /** Pointer to ring-3 mapping of the default exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufDefaultR3;
    /** Event semaphore which a receiver thread will sleep on while waiting
     * for data to arrive. */
    RTSEMEVENT volatile     Event;
    /** Number of threads sleeping on the Event semaphore. */
    uint32_t                cSleepers;
    /** The interface handle.
     * When this is INTNET_HANDLE_INVALID a sleeper which is waking up
     * should return with the appropriate error condition. */
    INTNETIFHANDLE volatile hIf;
    /** Pointer to the network this interface is connected to.
     * This is protected by the INTNET::FastMutex. */
    struct INTNETNETWORK   *pNetwork;
    /** The session this interface is associated with. */
    PSUPDRVSESSION          pSession;
    /** The SUPR0 object id. */
    void                   *pvObj;
    /** The network layer address cache. (Indexed by type, 0 entry isn't used.) */
    INTNETADDRCACHE         aAddrCache[kIntNetAddrType_End];
#ifdef WITH_NEW_STUFF
    /** Spinlock protecting the input (producer) side of the receive ring. */
    RTSPINLOCK              hRecvInSpinlock;
    /** Busy count for tracking destination table references and active sends.
     * Incremented while owning the switch table spinlock. */
    uint32_t volatile       cBusy;
    /** Set if a someone is waiting on INTNETNETWORK::hEvtBusyIf for cBusy to
     * reach zero. */
    bool volatile           fBusyZeroWakeup;
    /** The preallocated destination table.
     * This is NULL when it's in use as a precaution against unserialized
     * transmitting.  This is grown when new interfaces are added to the network. */
    PINTNETDSTTAB volatile  pDstTab;
#endif /* WITH_NEW_STUFF */
} INTNETIF;
/** Pointer to an internal network interface. */
typedef INTNETIF *PINTNETIF;


/**
 * A trunk interface.
 */
typedef struct INTNETTRUNKIF
{
    /** The port interface we present to the component. */
    INTNETTRUNKSWPORT       SwitchPort;
    /** The port interface we get from the component. */
    PINTNETTRUNKIFPORT      pIfPort;
#ifdef WITH_NEW_STUFF
    /** @todo This won't quite cut the mustard any longer.  That said, GSO
     *        segmentation needs to be serialized because of the header buffer. */
    RTSEMFASTMUTEX          FastMutex;
#else
    /** The trunk mutex that serializes all calls <b>to</b> the component. */
    RTSEMFASTMUTEX          FastMutex;
#endif
    /** Pointer to the network we're connect to.
     * This may be NULL if we're orphaned? */
    struct INTNETNETWORK   *pNetwork;
#ifdef WITH_NEW_STUFF
    /** The current MAC address for the interface. (reported)
     * Updated while owning the switch table spinlock.
     * @todo rename to Mac  */
    RTMAC                   CachedMac;
#else
    /** The cached MAC address of the interface the trunk is attached to.
     * This is for the situations where we cannot take the out-bound
     * semaphore (the recv case) but need to make frame edits (ARP). */
    RTMAC                   CachedMac;
#endif
#ifndef WITH_NEW_STUFF
    /** Whether to supply physical addresses with the outbound SGs. (reported) */
    bool volatile           fPhysSG;
    /** Set if the 'wire' is in promiscuous mode.
     * The state of the 'host' is queried each time. */
    bool                    fWirePromiscuous;
#else  /* WITH_NEW_STUFF */
    /** Set if the 'wire' is in promiscuous mode. (config) */
    bool                    fWirePromiscuous;
    /** Set if the 'host' is in promiscuous mode. (reported) */
    bool volatile           fHostPromiscuous;
    /** Can pfnXmit cope with disabled preemption for the 'wire'. (reported) */
    bool                    fWireNoPreempt;
    /** Can pfnXmit cope with disabled preemption for the 'host'. (reported) */
    bool                    fHostNoPreempt;
    /** Whether to supply physical addresses with the outbound SGs. (reported) */
    bool                    fPhysSG;
    /** Set if a someone is waiting on INTNETNETWORK::hEvtBusyIf for cBusy to
     * reach zero. */
    bool volatile           fBusyZeroWakeup;
    /** Busy count for tracking destination table references and active sends.
     * Incremented while owning the switch table spinlock. */
    uint32_t volatile       cBusy;
#endif /* WITH_NEW_STUFF */
    /** The GSO capabilities of the wire destination. (reported) */
    uint32_t                fWireGsoCapabilites;
    /** The GSO capabilities of the host destination. (reported)
     * This is as bit map where each bit represents the GSO type with the same
     * number. */
    uint32_t                fHostGsoCapabilites;
    /** Header buffer for when we're carving GSO frames. */
    uint8_t                 abGsoHdrs[256];
#ifdef WITH_NEW_STUFF
    /** @todo what exactly to do about the destination tables here? how many do
     *        we need / want? One? One per CPU? */
#endif
} INTNETTRUNKIF;
/** Pointer to a trunk interface. */
typedef INTNETTRUNKIF *PINTNETTRUNKIF;

/** Converts a pointer to INTNETTRUNKIF::SwitchPort to a PINTNETTRUNKIF. */
#define INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort) ((PINTNETTRUNKIF)(pSwitchPort))


/**
 * Internal representation of a network.
 */
typedef struct INTNETNETWORK
{
    /** The Next network in the chain.
     * This is protected by the INTNET::FastMutex. */
    struct INTNETNETWORK   *pNext;
    /** List of interfaces connected to the network.
     * This is protected by the INTNET::FastMutex. */
    PINTNETIF               pIfs;
    /** Pointer to the trunk interface.
     * Can be NULL if there is no trunk connection. */
    PINTNETTRUNKIF          pTrunkIF;
    /** The network mutex.
     * It protects everything dealing with this network. */
#ifdef WITH_NEW_STUFF
    /** @todo Make this a mutex so we can block on the event semaphore while holding
     *        it. Requires fixing the mutex code on linux...  Or maybe add
     *        another mutex for creation / destruction serilization. */
#endif
    RTSEMFASTMUTEX          FastMutex2;
#ifdef WITH_NEW_STUFF
    /** Wait for an interface to stop being busy so it can be removed or have its
     * destination table replaced.  We have to wait upon this while owning the
     * network mutex. */
    RTSEMEVENT              hEvtBusyIf;
#endif
    /** Pointer to the instance data. */
    struct INTNET          *pIntNet;
    /** The SUPR0 object id. */
    void                   *pvObj;
    /** Pointer to the temporary buffer that is used when snooping fragmented packets.
     * This is allocated after this structure if we're sharing the MAC address with
     * the host. The buffer is INTNETNETWORK_TMP_SIZE big and aligned on a 64-byte boundrary. */
    uint8_t                *pbTmp;
    /** Network creation flags (INTNET_OPEN_FLAGS_*). */
    uint32_t                fFlags;
    /** The number of active interfaces (excluding the trunk). */
    uint32_t                cActiveIFs;
    /** The length of the network name. */
    uint8_t                 cchName;
    /** The network name. */
    char                    szName[INTNET_MAX_NETWORK_NAME];
    /** The trunk type. */
    INTNETTRUNKTYPE         enmTrunkType;
    /** The trunk name. */
    char                    szTrunk[INTNET_MAX_TRUNK_NAME];
} INTNETNETWORK;
/** Pointer to an internal network. */
typedef INTNETNETWORK *PINTNETNETWORK;

/** The size of the buffer INTNETNETWORK::pbTmp points at. */
#define INTNETNETWORK_TMP_SIZE  2048


/**
 * Internal networking instance.
 */
typedef struct INTNET
{
    /** Mutex protecting the creation, opening and destruction of both networks and
     * interfaces.  (This means all operations affecting the pNetworks list.) */
    RTSEMMUTEX              hMtxCreateOpenDestroy;
    /** List of networks. Protected by INTNET::Spinlock. */
    PINTNETNETWORK volatile pNetworks;
    /** Handle table for the interfaces. */
    RTHANDLETABLE           hHtIfs;
} INTNET;



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static PINTNETTRUNKIF intnetR0TrunkIfRetain(PINTNETTRUNKIF pThis);
static void intnetR0TrunkIfRelease(PINTNETTRUNKIF pThis);
static bool intnetR0TrunkIfOutLock(PINTNETTRUNKIF pThis);
static void intnetR0TrunkIfOutUnlock(PINTNETTRUNKIF pThis);


/**
 * Worker for intnetR0SgWritePart that deals with the case where the
 * request doesn't fit into the first segment.
 *
 * @returns true, unless the request or SG invalid.
 * @param   pSG         The SG list to write to.
 * @param   off         Where to start writing (offset into the SG).
 * @param   cb          How much to write.
 * @param   pvBuf       The buffer to containing the bits to write.
 */
static bool intnetR0SgWritePartSlow(PCINTNETSG pSG, uint32_t off, uint32_t cb, void const *pvBuf)
{
    if (RT_UNLIKELY(off + cb > pSG->cbTotal))
        return false;

    /*
     * Skip ahead to the segment where off starts.
     */
    unsigned const cSegs = pSG->cSegsUsed; Assert(cSegs == pSG->cSegsUsed);
    unsigned iSeg = 0;
    while (off > pSG->aSegs[iSeg].cb)
    {
        off -= pSG->aSegs[iSeg++].cb;
        AssertReturn(iSeg < cSegs, false);
    }

    /*
     * Copy the data, hoping that it's all from one segment...
     */
    uint32_t cbCanCopy = pSG->aSegs[iSeg].cb - off;
    if (cbCanCopy >= cb)
        memcpy((uint8_t *)pSG->aSegs[iSeg].pv + off, pvBuf, cb);
    else
    {
        /* copy the portion in the current segment. */
        memcpy((uint8_t *)pSG->aSegs[iSeg].pv + off, pvBuf, cbCanCopy);
        cb -= cbCanCopy;

        /* copy the portions in the other segments. */
        do
        {
            pvBuf = (uint8_t const *)pvBuf + cbCanCopy;
            iSeg++;
            AssertReturn(iSeg < cSegs, false);

            cbCanCopy = RT_MIN(cb, pSG->aSegs[iSeg].cb);
            memcpy(pSG->aSegs[iSeg].pv, pvBuf, cbCanCopy);

            cb -= cbCanCopy;
        } while (cb > 0);
    }

    return true;
}


/**
 * Writes to a part of an SG.
 *
 * @returns true on success, false on failure (out of bounds).
 * @param   pSG         The SG list to write to.
 * @param   off         Where to start writing (offset into the SG).
 * @param   cb          How much to write.
 * @param   pvBuf       The buffer to containing the bits to write.
 */
DECLINLINE(bool) intnetR0SgWritePart(PCINTNETSG pSG, uint32_t off, uint32_t cb, void const *pvBuf)
{
    Assert(off + cb > off);

    /* The optimized case. */
    if (RT_LIKELY(    pSG->cSegsUsed == 1
                  ||  pSG->aSegs[0].cb >= off + cb))
    {
        Assert(pSG->cbTotal == pSG->aSegs[0].cb);
        memcpy((uint8_t *)pSG->aSegs[0].pv + off, pvBuf, cb);
        return true;
    }
    return intnetR0SgWritePartSlow(pSG, off, cb, pvBuf);
}


/**
 * Reads a byte from a SG list.
 *
 * @returns The byte on success. 0xff on failure.
 * @param   pSG         The SG list to read.
 * @param   off         The offset (into the SG) off the byte.
 */
DECLINLINE(uint8_t) intnetR0SgReadByte(PCINTNETSG pSG, uint32_t off)
{
    if (RT_LIKELY(pSG->aSegs[0].cb > off))
        return ((uint8_t const *)pSG->aSegs[0].pv)[off];

    off -= pSG->aSegs[0].cb;
    unsigned const cSegs = pSG->cSegsUsed; Assert(cSegs == pSG->cSegsUsed);
    for (unsigned iSeg = 1; iSeg < cSegs; iSeg++)
    {
        if (pSG->aSegs[iSeg].cb > off)
            return ((uint8_t const *)pSG->aSegs[iSeg].pv)[off];
        off -= pSG->aSegs[iSeg].cb;
    }
    return false;
}


/**
 * Worker for intnetR0SgReadPart that deals with the case where the
 * requested data isn't in the first segment.
 *
 * @returns true, unless the SG is invalid.
 * @param   pSG         The SG list to read.
 * @param   off         Where to start reading (offset into the SG).
 * @param   cb          How much to read.
 * @param   pvBuf       The buffer to read into.
 */
static bool intnetR0SgReadPartSlow(PCINTNETSG pSG, uint32_t off, uint32_t cb, void *pvBuf)
{
    if (RT_UNLIKELY(off + cb > pSG->cbTotal))
        return false;

    /*
     * Skip ahead to the segment where off starts.
     */
    unsigned const cSegs = pSG->cSegsUsed; Assert(cSegs == pSG->cSegsUsed);
    unsigned iSeg = 0;
    while (off > pSG->aSegs[iSeg].cb)
    {
        off -= pSG->aSegs[iSeg++].cb;
        AssertReturn(iSeg < cSegs, false);
    }

    /*
     * Copy the data, hoping that it's all from one segment...
     */
    uint32_t cbCanCopy = pSG->aSegs[iSeg].cb - off;
    if (cbCanCopy >= cb)
        memcpy(pvBuf, (uint8_t const *)pSG->aSegs[iSeg].pv + off, cb);
    else
    {
        /* copy the portion in the current segment. */
        memcpy(pvBuf, (uint8_t const *)pSG->aSegs[iSeg].pv + off, cbCanCopy);
        cb -= cbCanCopy;

        /* copy the portions in the other segments. */
        do
        {
            pvBuf = (uint8_t *)pvBuf + cbCanCopy;
            iSeg++;
            AssertReturn(iSeg < cSegs, false);

            cbCanCopy = RT_MIN(cb, pSG->aSegs[iSeg].cb);
            memcpy(pvBuf, (uint8_t const *)pSG->aSegs[iSeg].pv, cbCanCopy);

            cb -= cbCanCopy;
        } while (cb > 0);
    }

    return true;
}


/**
 * Reads a part of an SG into a buffer.
 *
 * @returns true on success, false on failure (out of bounds).
 * @param   pSG         The SG list to read.
 * @param   off         Where to start reading (offset into the SG).
 * @param   cb          How much to read.
 * @param   pvBuf       The buffer to read into.
 */
DECLINLINE(bool) intnetR0SgReadPart(PCINTNETSG pSG, uint32_t off, uint32_t cb, void *pvBuf)
{
    Assert(off + cb > off);

    /* The optimized case. */
    if (RT_LIKELY(    pSG->cSegsUsed == 1
                  ||  pSG->aSegs[0].cb >= off + cb))
    {
        Assert(pSG->cbTotal == pSG->aSegs[0].cb);
        memcpy(pvBuf, (uint8_t const *)pSG->aSegs[0].pv + off, cb);
        return true;
    }
    return intnetR0SgReadPartSlow(pSG, off, cb, pvBuf);
}


#ifdef WITH_NEW_STUFF

/**
 * Compares two MAC addresses.
 *
 * @returns true if equal, false if not.
 * @param   pDstAddr1           Address 1.
 * @param   pDstAddr2           Address 2.
 */
DECL_FORCE_INLINE(bool) intnetR0AreMacAddrsEqual(PCRTMAC pDstAddr1, PCRTMAC pDstAddr2)
{
    return pDstAddr1->au16[2] == pDstAddr2->au16[2]
        && pDstAddr1->au16[1] == pDstAddr2->au16[1]
        && pDstAddr1->au16[0] == pDstAddr2->au16[0];
}


/**
 * Switch a MAC address and return a destination table.
 *
 * @returns Destination table or NULL if *ppDstTab is NULL.
 * @param   pTab                The MAC address table to work on.
 * @param   pDstAddr            The destination address of the packet.
 * @param   ppDstTab            Where to get the destination table from.
 * @param   pcBusy              The busy counter to increment if *ppDstTab isn't
 *                              NULL.
 */
static PINTNETDSTTAB intnetR0MacTabSwitch(PINTNETMACTAB pTab, PCRTMAC pDstAddr,
                                          PINTNETDSTTAB volatile *ppDstTab, uint32_t volatile *pcBusy)
{
    /*
     * Grab the spinlock first, then get the destination table and increment
     * the busy counter (to indicate that we're using the dst tab and it cannot
     * be freed yet).
     */
    RTSPINLOCKTMP   Tmp     = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pTab->hSpinlock, &Tmp);
    PINTNETDSTTAB   pDstTab = (PINTNETDSTTAB)ASMAtomicXchgPtr((void * volatile *)ppDstTab, NULL);
    if (pDstTab)
    {
        ASMAtomicIncU32(pcBusy);

        /*
         * Do the switching.
         */
        pDstTab->fTrunkDst = 0;
        pDstTab->pTrunk    = 0;
        pDstTab->cIfs      = 0;

        uint32_t i = pTab->cEntries;
        if (pDstAddr->au8[0] & 1) /* multicast or broadcast address */
        {
            /* Broadcast/multicast - add all active interfaces. */
            while (i-- > 0)
            {
                if (pTab->paEntries[i].fActive)
                {
                    PINTNETIF pIf = pTab->paEntries[i].pIf;
                    pDstTab->apIfs[i] = pIf;
                    ASMAtomicIncU32(&pIf->cBusy);
                    pDstTab->cIfs++;
                }
            }

            if (pTab->fHostActive)
                pDstTab->fTrunkDst |= INTNETTRUNKDIR_HOST;
            if (pTab->fWireActive)
                pDstTab->fTrunkDst |= INTNETTRUNKDIR_WIRE;
        }
        else
        {
            /* Find exactly matching or promiscuous interfaces. */
            uint32_t cExactHits = 0;
            while (i-- > 0)
            {
                if (pTab->paEntries[i].fActive)
                {
                    bool fExact = intnetR0AreMacAddrsEqual(&pTab->paEntries[i].MacAddr, pDstAddr);
                    if (fExact || pTab->paEntries[i].fPromiscuous)
                    {
                        cExactHits += fExact;

                        PINTNETIF pIf = pTab->paEntries[i].pIf;
                        pDstTab->apIfs[i] = pIf;
                        ASMAtomicIncU32(&pIf->cBusy);
                        pDstTab->cIfs++;
                    }
                }
            }

            /* Does it match the host, or is the host promiscuous? */
            if (pTab->fHostActive)
            {
                bool fExact = intnetR0AreMacAddrsEqual(&pTab->HostMac, pDstAddr);
                if (pTab->fHostPromiscuous)
                {
                    cExactHits += fExact;
                    pDstTab->fTrunkDst |= INTNETTRUNKDIR_HOST;
                }
            }

            /* Hit the wire if there are no exact matches or if it's in promiscuous mode. */
            if (pTab->fWireActive && (!cExactHits || pTab->fWirePromiscuous))
                pDstTab->fTrunkDst |= INTNETTRUNKDIR_WIRE;
        }

        /* Grab the trunk if we're sending to it. */
        if (pDstTab->fTrunkDst)
        {
            PINTNETTRUNKIF pTrunkIf = pTab->pTrunk;
            pDstTab->pTrunk = pTrunkIf;
            ASMAtomicIncU32(&pTrunkIf->cBusy);
        }
    }

    RTSpinlockRelease(pTab->hSpinlock, &Tmp);
    return pDstTab;
}


#endif /* WITH_NEW_STUFF */


/**
 * Retain an interface.
 *
 * @returns VBox status code, can assume success in most situations.
 * @param   pIf                 The interface instance.
 * @param   pSession            The current session.
 */
DECLINLINE(int) intnetR0IfRetain(PINTNETIF pIf, PSUPDRVSESSION pSession)
{
    int rc = SUPR0ObjAddRefEx(pIf->pvObj, pSession, true /* fNoBlocking */);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}


/**
 * Release an interface previously retained by intnetR0IfRetain or
 * by handle lookup/freeing.
 *
 * @returns true if destroyed, false if not.
 * @param   pIf                 The interface instance.
 * @param   pSession            The current session.
 */
DECLINLINE(bool) intnetR0IfRelease(PINTNETIF pIf, PSUPDRVSESSION pSession)
{
    int rc = SUPR0ObjRelease(pIf->pvObj, pSession);
    AssertRC(rc);
    return rc == VINF_OBJECT_DESTROYED;
}


/**
 * RTHandleCreateEx callback that retains an object in the
 * handle table before returning it.
 *
 * (Avoids racing the freeing of the handle.)
 *
 * @returns VBox status code.
 * @param   hHandleTable        The handle table (ignored).
 * @param   pvObj               The object (INTNETIF).
 * @param   pvCtx               The context (SUPDRVSESSION).
 * @param   pvUser              The user context (ignored).
 */
static DECLCALLBACK(int) intnetR0IfRetainHandle(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser)
{
    NOREF(pvUser);
    NOREF(hHandleTable);
    PINTNETIF pIf = (PINTNETIF)pvObj;
    if (pIf->hIf != INTNET_HANDLE_INVALID) /* Don't try retain it if called from intnetR0IfDestruct. */
        return intnetR0IfRetain(pIf, (PSUPDRVSESSION)pvCtx);
    return VINF_SUCCESS;
}






/**
 * Checks if the IPv4 address is a broadcast address.
 * @returns true/false.
 * @param   Addr        The address, network endian.
 */
DECLINLINE(bool) intnetR0IPv4AddrIsBroadcast(RTNETADDRIPV4 Addr)
{
    /* Just check for 255.255.255.255 atm. */
    return Addr.u == UINT32_MAX;
}


/**
 * Checks if the IPv4 address is a good interface address.
 * @returns true/false.
 * @param   Addr        The address, network endian.
 */
DECLINLINE(bool) intnetR0IPv4AddrIsGood(RTNETADDRIPV4 Addr)
{
    /* Usual suspects. */
    if (    Addr.u == UINT32_MAX    /* 255.255.255.255 - broadcast. */
        ||  Addr.au8[0] == 0)       /* Current network, can be used as source address. */
        return false;

    /* Unusual suspects. */
    if (RT_UNLIKELY(     Addr.au8[0]         == 127  /* Loopback */
                    ||  (Addr.au8[0] & 0xf0) == 224 /* Multicast */
                    ))
        return false;
    return true;
}


/**
 * Gets the address size of a network layer type.
 *
 * @returns size in bytes.
 * @param   enmType             The type.
 */
DECLINLINE(uint8_t) intnetR0AddrSize(INTNETADDRTYPE enmType)
{
    switch (enmType)
    {
        case kIntNetAddrType_IPv4:  return 4;
        case kIntNetAddrType_IPv6:  return 16;
        case kIntNetAddrType_IPX:   return 4 + 6;
        default:                    AssertFailedReturn(0);
    }
}


/**
 * Compares two address to see if they are equal, assuming naturally align structures.
 *
 * @returns true if equal, false if not.
 * @param   pAddr1          The first address.
 * @param   pAddr2          The second address.
 * @param   cbAddr          The address size.
 */
DECLINLINE(bool) intnetR0AddrUIsEqualEx(PCRTNETADDRU pAddr1, PCRTNETADDRU pAddr2, uint8_t const cbAddr)
{
    switch (cbAddr)
    {
        case 4:  /* IPv4 */
            return pAddr1->au32[0] == pAddr2->au32[0];
        case 16: /* IPv6 */
            return pAddr1->au64[0] == pAddr2->au64[0]
                && pAddr1->au64[1] == pAddr2->au64[1];
        case 10: /* IPX */
            return pAddr1->au64[0] == pAddr2->au64[0]
                && pAddr1->au16[4] == pAddr2->au16[4];
        default:
            AssertFailedReturn(false);
    }
}


/**
 * Worker for intnetR0IfAddrCacheLookup that performs the lookup
 * in the remaining cache entries after the caller has check the
 * most likely ones.
 *
 * @returns -1 if not found, the index of the cache entry if found.
 * @param   pCache      The cache.
 * @param   pAddr       The address.
 * @param   cbAddr      The address size (optimization).
 */
static int intnetR0IfAddrCacheLookupSlow(PCINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr)
{
    unsigned i = pCache->cEntries - 2;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry * i;
    while (i >= 1)
    {
        if (intnetR0AddrUIsEqualEx((PCRTNETADDRU)pbEntry, pAddr, cbAddr))
            return i;
        pbEntry -= pCache->cbEntry;
        i--;
    }

    return -1;
}

/**
 * Lookup an address in a cache without any expectations.
 *
 * @returns -1 if not found, the index of the cache entry if found.
 * @param   pCache          The cache.
 * @param   pAddr           The address.
 * @param   cbAddr          The address size (optimization).
 */
DECLINLINE(int) intnetR0IfAddrCacheLookup(PCINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when there is one cache entry and
     * it doesn't match.
     */
    unsigned i = pCache->cEntries;
    if (    i > 0
        &&  intnetR0AddrUIsEqualEx((PCRTNETADDRU)pCache->pbEntries, pAddr, cbAddr))
        return 0;
    if (i <= 1)
        return -1;

    /*
     * Check the last entry.
     */
    i--;
    if (intnetR0AddrUIsEqualEx((PCRTNETADDRU)(pCache->pbEntries + pCache->cbEntry * i), pAddr, cbAddr))
        return i;
    if (i <= 1)
        return -1;

    return intnetR0IfAddrCacheLookupSlow(pCache, pAddr, cbAddr);
}


/** Same as intnetR0IfAddrCacheLookup except we expect the address to be present already. */
DECLINLINE(int) intnetR0IfAddrCacheLookupLikely(PCINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr)
{
    /** @todo implement this. */
    return intnetR0IfAddrCacheLookup(pCache, pAddr, cbAddr);
}


/**
 * Worker for intnetR0IfAddrCacheLookupUnlikely that performs
 * the lookup in the remaining cache entries after the caller
 * has check the most likely ones.
 *
 * The routine is expecting not to find the address.
 *
 * @returns -1 if not found, the index of the cache entry if found.
 * @param   pCache      The cache.
 * @param   pAddr       The address.
 * @param   cbAddr      The address size (optimization).
 */
static int intnetR0IfAddrCacheInCacheUnlikelySlow(PCINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr)
{
    /*
     * Perform a full table lookup.
     */
    unsigned i = pCache->cEntries - 2;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry * i;
    while (i >= 1)
    {
        if (RT_UNLIKELY(intnetR0AddrUIsEqualEx((PCRTNETADDRU)pbEntry, pAddr, cbAddr)))
            return i;
        pbEntry -= pCache->cbEntry;
        i--;
    }

    return -1;
}


/**
 * Lookup an address in a cache expecting not to find it.
 *
 * @returns -1 if not found, the index of the cache entry if found.
 * @param   pCache          The cache.
 * @param   pAddr           The address.
 * @param   cbAddr          The address size (optimization).
 */
DECLINLINE(int) intnetR0IfAddrCacheLookupUnlikely(PCINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when there is one cache entry and
     * it doesn't match.
     */
    unsigned i = pCache->cEntries;
    if (RT_UNLIKELY(   i > 0
                    && intnetR0AddrUIsEqualEx((PCRTNETADDRU)pCache->pbEntries, pAddr, cbAddr)))
        return 0;
    if (RT_LIKELY(i <= 1))
        return -1;

    /*
     * Then check the last entry and return if there are just two cache entries.
     */
    i--;
    if (RT_UNLIKELY(intnetR0AddrUIsEqualEx((PCRTNETADDRU)(pCache->pbEntries + pCache->cbEntry * i), pAddr, cbAddr)))
        return i;
    if (i <= 1)
        return -1;

    return intnetR0IfAddrCacheInCacheUnlikelySlow(pCache, pAddr, cbAddr);
}


/**
 * Deletes a specific cache entry.
 *
 * Worker for intnetR0NetworkAddrCacheDelete and intnetR0NetworkAddrCacheDeleteMinusIf.
 *
 * @param   pIf             The interface (for logging).
 * @param   pCache          The cache.
 * @param   iEntry          The entry to delete.
 * @param   pszMsg          Log message.
 */
static void intnetR0IfAddrCacheDeleteIt(PINTNETIF pIf, PINTNETADDRCACHE pCache, int iEntry, const char *pszMsg)
{
    AssertReturnVoid(iEntry < pCache->cEntries);
    AssertReturnVoid(iEntry >= 0);
#ifdef LOG_ENABLED
    INTNETADDRTYPE enmAddrType = (INTNETADDRTYPE)(uintptr_t)(pCache - &pIf->aAddrCache[0]);
    PCRTNETADDRU pAddr = (PCRTNETADDRU)(pCache->pbEntries + iEntry * pCache->cbEntry);
    switch (enmAddrType)
    {
        case kIntNetAddrType_IPv4:
            Log(("intnetR0IfAddrCacheDeleteIt: hIf=%#x MAC=%.6Rhxs IPv4 added #%d %d.%d.%d.%d %s\n",
                 pIf->hIf, &pIf->Mac, iEntry, pAddr->au8[0], pAddr->au8[1], pAddr->au8[2], pAddr->au8[3], pszMsg));
            break;
        default:
            Log(("intnetR0IfAddrCacheDeleteIt: hIf=%RX32 MAC=%.6Rhxs type=%d #%d %.*Rhxs %s\n",
                 pIf->hIf, &pIf->Mac, enmAddrType, iEntry, pCache->cbAddress, pAddr, pszMsg));
            break;
    }
#endif

    pCache->cEntries--;
    if (iEntry < pCache->cEntries)
        memmove(pCache->pbEntries +      iEntry  * pCache->cbEntry,
                pCache->pbEntries + (iEntry + 1) * pCache->cbEntry,
                (pCache->cEntries - iEntry)      * pCache->cbEntry);
}


/**
 * Deletes an address from the cache, assuming it isn't actually in the cache.
 *
 * @param   pIf             The interface (for logging).
 * @param   pCache          The cache.
 * @param   pAddr           The address.
 * @param   cbAddr          The address size (optimization).
 */
DECLINLINE(void) intnetR0IfAddrCacheDelete(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr, const char *pszMsg)
{
    int i = intnetR0IfAddrCacheLookup(pCache, pAddr, cbAddr);
    if (RT_UNLIKELY(i >= 0))
        intnetR0IfAddrCacheDeleteIt(pIf, pCache, i, pszMsg);
}


/**
 * Deletes the address from all the interface caches.
 *
 * This is used to remove stale entries that has been reassigned to
 * other machines on the network.
 *
 * @param   pNetwork        The network.
 * @param   pAddr           The address.
 * @param   enmType         The address type.
 * @param   cbAddr          The address size (optimization).
 * @param   pszMsg          Log message.
 */
DECLINLINE(void) intnetR0NetworkAddrCacheDelete(PINTNETNETWORK pNetwork, PCRTNETADDRU pAddr, INTNETADDRTYPE const enmType,
                                                uint8_t const cbAddr, const char *pszMsg)
{
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
    {
        int i = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmType], pAddr, cbAddr);
        if (RT_UNLIKELY(i >= 0))
            intnetR0IfAddrCacheDeleteIt(pIf, &pIf->aAddrCache[enmType], i, pszMsg);
    }
}


/**
 * Deletes the address from all the interface caches except the specified one.
 *
 * This is used to remove stale entries that has been reassigned to
 * other machines on the network.
 *
 * @param   pNetwork        The network.
 * @param   pAddr           The address.
 * @param   enmType         The address type.
 * @param   cbAddr          The address size (optimization).
 * @param   pszMsg          Log message.
 */
DECLINLINE(void) intnetR0NetworkAddrCacheDeleteMinusIf(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, PCRTNETADDRU pAddr,
                                                       INTNETADDRTYPE const enmType, uint8_t const cbAddr, const char *pszMsg)
{
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
        if (pIf != pIfSender)
        {
            int i = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmType], pAddr, cbAddr);
            if (RT_UNLIKELY(i >= 0))
                intnetR0IfAddrCacheDeleteIt(pIf, &pIf->aAddrCache[enmType], i, pszMsg);
        }
}


/**
 * Lookup an address on the network, returning the (first) interface
 * having it in its address cache.
 *
 * @returns Pointer to the interface on success, NULL if not found.
 * @param   pNetwork        The network.
 * @param   pAddr           The address to lookup.
 * @param   enmType         The address type.
 * @param   cbAddr          The size of the address.
 */
DECLINLINE(PINTNETIF) intnetR0NetworkAddrCacheLookupIf(PINTNETNETWORK pNetwork, PCRTNETADDRU pAddr, INTNETADDRTYPE const enmType, uint8_t const cbAddr)
{
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
    {
        int i = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmType], pAddr, cbAddr);
        if (i >= 0)
            return pIf;
    }
    return NULL;
}


/**
 * Adds an address to the cache, the caller is responsible for making sure it'
 * s not already in the cache.
 *
 * @param   pIf         The interface (for logging).
 * @param   pCache      The address cache.
 * @param   pAddr       The address.
 * @param   pszMsg      log message.
 */
static void intnetR0IfAddrCacheAddIt(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, const char *pszMsg)
{
    if (!pCache->cEntriesAlloc)
    {
        /* Allocate the first array */
        pCache->pbEntries = (uint8_t *)RTMemAllocZ(pCache->cbEntry * 16);
        if (!pCache->pbEntries)
            return;
        pCache->cEntriesAlloc = 16;
    }
    else if (pCache->cEntries >= pCache->cEntriesAlloc)
    {
        bool fReplace = true;
        if (pCache->cEntriesAlloc < 64)
        {
            uint8_t cEntriesAlloc = pCache->cEntriesAlloc + 16;
            void *pvNew = RTMemRealloc(pCache->pbEntries, pCache->cbEntry * cEntriesAlloc);
            if (pvNew)
            {
                pCache->pbEntries = (uint8_t *)pvNew;
                pCache->cEntriesAlloc = cEntriesAlloc;
                fReplace = false;
            }
        }
        if (fReplace)
        {
            /* simple FIFO, might consider usage/ageing here... */
            Log(("intnetR0IfAddrCacheAddIt: type=%d replacing %.*Rhxs\n",
                 (int)(uintptr_t)(pCache - &pIf->aAddrCache[0]), pCache->cbAddress, pCache->pbEntries));
            memmove(pCache->pbEntries, pCache->pbEntries + pCache->cbEntry, pCache->cbEntry * (pCache->cEntries - 1));
            pCache->cEntries--;
        }
    }

    /*
     * Add the new entry to the end of the array.
     */
    uint8_t *pbEntry = pCache->pbEntries + pCache->cEntries * pCache->cbEntry;
    memcpy(pbEntry, pAddr, pCache->cbAddress);
    memset(pbEntry + pCache->cbAddress, '\0', pCache->cbEntry - pCache->cbAddress);
#ifdef LOG_ENABLED
    INTNETADDRTYPE enmAddrType = (INTNETADDRTYPE)(uintptr_t)(pCache - &pIf->aAddrCache[0]);
    switch (enmAddrType)
    {
        case kIntNetAddrType_IPv4:
            Log(("intnetR0IfAddrCacheAddIt: hIf=%#x MAC=%.6Rhxs IPv4 added #%d %d.%d.%d.%d %s\n",
                 pIf->hIf, &pIf->Mac, pCache->cEntries, pAddr->au8[0], pAddr->au8[1], pAddr->au8[2], pAddr->au8[3], pszMsg));
            break;
        default:
            Log(("intnetR0IfAddrCacheAddIt: hIf=%#x MAC=%.6Rhxs type=%d added #%d %.*Rhxs %s\n",
                 pIf->hIf, &pIf->Mac, enmAddrType, pCache->cEntries, pCache->cbAddress, pAddr, pszMsg));
            break;
    }
#endif
    pCache->cEntries++;
    Assert(pCache->cEntries <= pCache->cEntriesAlloc);
}


/**
 * A intnetR0IfAddrCacheAdd worker that performs the rest of the lookup.
 *
 * @param   pIf         The interface (for logging).
 * @param   pCache      The address cache.
 * @param   pAddr       The address.
 * @param   cbAddr      The size of the address (optimization).
 * @param   pszMsg      Log message.
 */
static void intnetR0IfAddrCacheAddSlow(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr, const char *pszMsg)
{
    /*
     * Check all but the first and last entries, the caller
     * has already checked those.
     */
    int i = pCache->cEntries - 2;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry;
    while (i >= 1)
    {
        if (RT_LIKELY(intnetR0AddrUIsEqualEx((PCRTNETADDRU)pbEntry, pAddr, cbAddr)))
            return;
        pbEntry += pCache->cbEntry;
        i--;
    }

    /*
     * Not found, add it.
     */
    intnetR0IfAddrCacheAddIt(pIf, pCache, pAddr, pszMsg);
}


/**
 * Adds an address to the cache if it's not already there.
 *
 * @param   pIf         The interface (for logging).
 * @param   pCache      The address cache.
 * @param   pAddr       The address.
 * @param   cbAddr      The size of the address (optimization).
 * @param   pszMsg      Log message.
 */
DECLINLINE(void) intnetR0IfAddrCacheAdd(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCRTNETADDRU pAddr, uint8_t const cbAddr, const char *pszMsg)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when the address the first or last cache entry.
     */
    unsigned i = pCache->cEntries;
    if (RT_LIKELY(   i > 0
                  && (   intnetR0AddrUIsEqualEx((PCRTNETADDRU)pCache->pbEntries, pAddr, cbAddr)
                      || (i > 1
                          && intnetR0AddrUIsEqualEx((PCRTNETADDRU)(pCache->pbEntries + pCache->cbEntry * i), pAddr, cbAddr))) ))
        return;
    intnetR0IfAddrCacheAddSlow(pIf, pCache, pAddr, cbAddr, pszMsg);
}


#ifdef INTNET_WITH_DHCP_SNOOPING

/**
 * Snoops IP assignments and releases from the DHCPv4 traffic.
 *
 * The caller is responsible for making sure this traffic between the
 * BOOTPS and BOOTPC ports and validate the IP header. The UDP packet
 * need not be validated beyond the ports.
 *
 * @param   pNetwork        The network this frame was seen on.
 * @param   pIpHdr          Pointer to a valid IP header. This is for pseudo
 *                          header validation, so only the minimum header size
 *                          needs to be available and valid here.
 * @param   pUdpHdr         Pointer to the UDP header in the frame.
 * @param   cbUdpPkt        What's left of the frame when starting at the UDP header.
 * @param   fGso            Set if this is a GSO frame, clear if regular.
 */
static void intnetR0NetworkSnoopDhcp(PINTNETNETWORK pNetwork, PCRTNETIPV4 pIpHdr, PCRTNETUDP pUdpHdr, uint32_t cbUdpPkt)
{
    /*
     * Check if the DHCP message is valid and get the type.
     */
    if (!RTNetIPv4IsUDPValid(pIpHdr, pUdpHdr, pUdpHdr + 1, cbUdpPkt, true /*fCheckSum*/))
    {
        Log6(("Bad UDP packet\n"));
        return;
    }
    PCRTNETBOOTP pDhcp = (PCRTNETBOOTP)(pUdpHdr + 1);
    uint8_t MsgType;
    if (!RTNetIPv4IsDHCPValid(pUdpHdr, pDhcp, cbUdpPkt - sizeof(*pUdpHdr), &MsgType))
    {
        Log6(("Bad DHCP packet\n"));
        return;
    }

#ifdef LOG_ENABLED
    /*
     * Log it.
     */
    const char *pszType = "unknown";
    switch (MsgType)
    {
        case RTNET_DHCP_MT_DISCOVER: pszType = "discover";  break;
        case RTNET_DHCP_MT_OFFER:    pszType = "offer"; break;
        case RTNET_DHCP_MT_REQUEST:  pszType = "request"; break;
        case RTNET_DHCP_MT_DECLINE:  pszType = "decline"; break;
        case RTNET_DHCP_MT_ACK:      pszType = "ack"; break;
        case RTNET_DHCP_MT_NAC:      pszType = "nac"; break;
        case RTNET_DHCP_MT_RELEASE:  pszType = "release"; break;
        case RTNET_DHCP_MT_INFORM:   pszType = "inform"; break;
    }
    Log6(("DHCP msg: %d (%s) client %.6Rhxs ciaddr=%d.%d.%d.%d yiaddr=%d.%d.%d.%d\n", MsgType, pszType, &pDhcp->bp_chaddr,
          pDhcp->bp_ciaddr.au8[0], pDhcp->bp_ciaddr.au8[1], pDhcp->bp_ciaddr.au8[2], pDhcp->bp_ciaddr.au8[3],
          pDhcp->bp_yiaddr.au8[0], pDhcp->bp_yiaddr.au8[1], pDhcp->bp_yiaddr.au8[2], pDhcp->bp_yiaddr.au8[3]));
#endif /* LOG_EANBLED */

    /*
     * Act upon the message.
     */
    switch (MsgType)
    {
#if 0
        case RTNET_DHCP_MT_REQUEST:
            /** @todo Check for valid non-broadcast requests w/ IP for any of the MACs we
             *        know, and add the IP to the cache. */
            break;
#endif


        /*
         * Lookup the interface by its MAC address and insert the IPv4 address into the cache.
         * Delete the old client address first, just in case it changed in a renewal.
         */
        case RTNET_DHCP_MT_ACK:
            if (intnetR0IPv4AddrIsGood(pDhcp->bp_yiaddr))
                for (PINTNETIF pCur = pNetwork->pIfs; pCur; pCur = pCur->pNext)
                    if (    pCur->fMacSet
                        &&  !memcmp(&pCur->Mac, &pDhcp->bp_chaddr, sizeof(RTMAC)))
                    {
                        intnetR0IfAddrCacheDelete(pCur, &pCur->aAddrCache[kIntNetAddrType_IPv4],
                                                  (PCRTNETADDRU)&pDhcp->bp_ciaddr, sizeof(RTNETADDRIPV4), "DHCP_MT_ACK");
                        intnetR0IfAddrCacheAdd(pCur, &pCur->aAddrCache[kIntNetAddrType_IPv4],
                                               (PCRTNETADDRU)&pDhcp->bp_yiaddr, sizeof(RTNETADDRIPV4), "DHCP_MT_ACK");
                        break;
                    }
            break;


        /*
         * Lookup the interface by its MAC address and remove the IPv4 address(es) from the cache.
         */
        case RTNET_DHCP_MT_RELEASE:
        {
            for (PINTNETIF pCur = pNetwork->pIfs; pCur; pCur = pCur->pNext)
                if (    pCur->fMacSet
                    &&  !memcmp(&pCur->Mac, &pDhcp->bp_chaddr, sizeof(RTMAC)))
                {
                    intnetR0IfAddrCacheDelete(pCur, &pCur->aAddrCache[kIntNetAddrType_IPv4],
                                              (PCRTNETADDRU)&pDhcp->bp_ciaddr, sizeof(RTNETADDRIPV4), "DHCP_MT_RELEASE");
                    intnetR0IfAddrCacheDelete(pCur, &pCur->aAddrCache[kIntNetAddrType_IPv4],
                                              (PCRTNETADDRU)&pDhcp->bp_yiaddr, sizeof(RTNETADDRIPV4), "DHCP_MT_RELEASE");
                }
            break;
        }
    }

}


/**
 * Worker for intnetR0TrunkIfSnoopAddr that takes care of what
 * is likely to be a DHCP message.
 *
 * The caller has already check that the UDP source and destination ports
 * are BOOTPS or BOOTPC.
 *
 * @param   pNetwork        The network this frame was seen on.
 * @param   pSG             The gather list for the frame.
 */
static void intnetR0TrunkIfSnoopDhcp(PINTNETNETWORK pNetwork, PCINTNETSG pSG)
{
    /*
     * Get a pointer to a linear copy of the full packet, using the
     * temporary buffer if necessary.
     */
    PCRTNETIPV4 pIpHdr = (PCRTNETIPV4)((PCRTNETETHERHDR)pSG->aSegs[0].pv + 1);
    uint32_t cbPacket = pSG->cbTotal - sizeof(RTNETETHERHDR);
    if (pSG->cSegsUsed > 1)
    {
        cbPacket = RT_MIN(cbPacket, INTNETNETWORK_TMP_SIZE);
        Log6(("intnetR0TrunkIfSnoopDhcp: Copying IPv4/UDP/DHCP pkt %u\n", cbPacket));
        if (!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR), cbPacket, pNetwork->pbTmp))
            return;
        //pSG->fFlags |= INTNETSG_FLAGS_PKT_CP_IN_TMP;
        pIpHdr = (PCRTNETIPV4)pNetwork->pbTmp;
    }

    /*
     * Validate the IP header and find the UDP packet.
     */
    if (!RTNetIPv4IsHdrValid(pIpHdr, cbPacket, pSG->cbTotal - sizeof(RTNETETHERHDR), true /*fChecksum*/))
    {
        Log(("intnetR0TrunkIfSnoopDhcp: bad ip header\n"));
        return;
    }
    uint32_t cbIpHdr = pIpHdr->ip_hl * 4;

    /*
     * Hand it over to the common DHCP snooper.
     */
    intnetR0NetworkSnoopDhcp(pNetwork, pIpHdr, (PCRTNETUDP)((uintptr_t)pIpHdr + cbIpHdr), cbPacket - cbIpHdr);
}

#endif /* INTNET_WITH_DHCP_SNOOPING */


/**
 * Snoops up source addresses from ARP requests and purge these
 * from the address caches.
 *
 * The purpose of this purging is to get rid of stale addresses.
 *
 * @param   pNetwork        The network this frame was seen on.
 * @param   pSG             The gather list for the frame.
 */
static void intnetR0TrunkIfSnoopArp(PINTNETNETWORK pNetwork, PCINTNETSG pSG)
{
    /*
     * Check the minimum size first.
     */
    if (RT_UNLIKELY(pSG->cbTotal < sizeof(RTNETETHERHDR) + sizeof(RTNETARPIPV4)))
        return;

    /*
     * Copy to temporary buffer if necessary.
     */
    uint32_t cbPacket = RT_MIN(pSG->cbTotal, sizeof(RTNETARPIPV4));
    PCRTNETARPIPV4 pArpIPv4 = (PCRTNETARPIPV4)((uintptr_t)pSG->aSegs[0].pv + sizeof(RTNETETHERHDR));
    if (    pSG->cSegsUsed != 1
        &&  pSG->aSegs[0].cb < cbPacket)
    {
        if (        (pSG->fFlags & (INTNETSG_FLAGS_ARP_IPV4 | INTNETSG_FLAGS_PKT_CP_IN_TMP))
                !=  (INTNETSG_FLAGS_ARP_IPV4 | INTNETSG_FLAGS_PKT_CP_IN_TMP)
            && !intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR), cbPacket, pNetwork->pbTmp))
                return;
        pArpIPv4 = (PCRTNETARPIPV4)pNetwork->pbTmp;
    }

    /*
     * Ignore packets which doesn't interest us or we perceive as malformed.
     */
    if (RT_UNLIKELY(    pArpIPv4->Hdr.ar_hlen  != sizeof(RTMAC)
                    ||  pArpIPv4->Hdr.ar_plen  != sizeof(RTNETADDRIPV4)
                    ||  pArpIPv4->Hdr.ar_htype != RT_H2BE_U16(RTNET_ARP_ETHER)
                    ||  pArpIPv4->Hdr.ar_ptype != RT_H2BE_U16(RTNET_ETHERTYPE_IPV4)))
        return;
    uint16_t ar_oper = RT_H2BE_U16(pArpIPv4->Hdr.ar_oper);
    if (RT_UNLIKELY(    ar_oper != RTNET_ARPOP_REQUEST
                    &&  ar_oper != RTNET_ARPOP_REPLY))
    {
        Log6(("ts-ar: op=%#x\n", ar_oper));
        return;
    }

    /*
     * Delete the source address if it's OK.
     */
    if (    !(pArpIPv4->ar_sha.au8[0] & 1)
        &&  (   pArpIPv4->ar_sha.au16[0]
             || pArpIPv4->ar_sha.au16[1]
             || pArpIPv4->ar_sha.au16[2])
        &&  intnetR0IPv4AddrIsGood(pArpIPv4->ar_spa))
    {
        Log6(("ts-ar: %d.%d.%d.%d / %.6Rhxs\n", pArpIPv4->ar_spa.au8[0], pArpIPv4->ar_spa.au8[1],
              pArpIPv4->ar_spa.au8[2], pArpIPv4->ar_spa.au8[3], &pArpIPv4->ar_sha));
        intnetR0NetworkAddrCacheDelete(pNetwork, (PCRTNETADDRU)&pArpIPv4->ar_spa,
                                       kIntNetAddrType_IPv4, sizeof(pArpIPv4->ar_spa), "tif/arp");
    }
}


#ifdef INTNET_WITH_DHCP_SNOOPING
/**
 * Snoop up addresses from ARP and DHCP traffic from frames comming
 * over the trunk connection.
 *
 * The caller is responsible for do some basic filtering before calling
 * this function.
 * For IPv4 this means checking against the minimum DHCPv4 frame size.
 *
 * @param   pNetwork        The network.
 * @param   pSG             The SG list for the frame.
 * @param   EtherType       The Ethertype of the frame.
 */
static void intnetR0TrunkIfSnoopAddr(PINTNETNETWORK pNetwork, PCINTNETSG pSG, uint16_t EtherType)
{
    switch (EtherType)
    {
        case RTNET_ETHERTYPE_IPV4:
        {
            uint32_t    cbIpHdr;
            uint8_t     b;

            Assert(pSG->cbTotal >= sizeof(RTNETETHERHDR) + RTNETIPV4_MIN_LEN + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN);
            if (pSG->aSegs[0].cb >= sizeof(RTNETETHERHDR) + RTNETIPV4_MIN_LEN)
            {
                /* check if the protocol is UDP */
                PCRTNETIPV4 pIpHdr = (PCRTNETIPV4)((uint8_t const *)pSG->aSegs[0].pv + sizeof(RTNETETHERHDR));
                if (pIpHdr->ip_p != RTNETIPV4_PROT_UDP)
                    return;

                /* get the TCP header length */
                cbIpHdr = pIpHdr->ip_hl * 4;
            }
            else
            {
                /* check if the protocol is UDP */
                if (    intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + RT_OFFSETOF(RTNETIPV4, ip_p))
                    !=  RTNETIPV4_PROT_UDP)
                    return;

                /* get the TCP header length */
                b = intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + 0); /* (IPv4 first byte, a bitfield) */
                cbIpHdr = (b & 0x0f) * 4;
            }
            if (cbIpHdr < RTNETIPV4_MIN_LEN)
                return;

            /* compare the ports. */
            if (pSG->aSegs[0].cb >= sizeof(RTNETETHERHDR) + cbIpHdr + RTNETUDP_MIN_LEN)
            {
                PCRTNETUDP pUdpHdr = (PCRTNETUDP)((uint8_t const *)pSG->aSegs[0].pv + sizeof(RTNETETHERHDR) + cbIpHdr);
                if (    (   RT_BE2H_U16(pUdpHdr->uh_sport) != RTNETIPV4_PORT_BOOTPS
                         && RT_BE2H_U16(pUdpHdr->uh_dport) != RTNETIPV4_PORT_BOOTPS)
                    ||  (   RT_BE2H_U16(pUdpHdr->uh_dport) != RTNETIPV4_PORT_BOOTPC
                         && RT_BE2H_U16(pUdpHdr->uh_sport) != RTNETIPV4_PORT_BOOTPC))
                    return;
            }
            else
            {
                /* get the lower byte of the UDP source port number. */
                b = intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + cbIpHdr + RT_OFFSETOF(RTNETUDP, uh_sport) + 1);
                if (    b != RTNETIPV4_PORT_BOOTPS
                    &&  b != RTNETIPV4_PORT_BOOTPC)
                    return;
                uint8_t SrcPort = b;
                b = intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + cbIpHdr + RT_OFFSETOF(RTNETUDP, uh_sport));
                if (b)
                    return;

                /* get the lower byte of the UDP destination port number. */
                b = intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + cbIpHdr + RT_OFFSETOF(RTNETUDP, uh_dport) + 1);
                if (    b != RTNETIPV4_PORT_BOOTPS
                    &&  b != RTNETIPV4_PORT_BOOTPC)
                    return;
                if (b == SrcPort)
                    return;
                b = intnetR0SgReadByte(pSG, sizeof(RTNETETHERHDR) + cbIpHdr + RT_OFFSETOF(RTNETUDP, uh_dport));
                if (b)
                    return;
            }
            intnetR0TrunkIfSnoopDhcp(pNetwork, pSG);
            break;
        }

        case RTNET_ETHERTYPE_IPV6:
        {
            /** @todo IPv6: Check for ICMPv6. It looks like type 133 (Router solicitation) might
             * need to be edited. Check out how NDP works...  */
            break;
        }

        case RTNET_ETHERTYPE_ARP:
            intnetR0TrunkIfSnoopArp(pNetwork, pSG);
            break;
    }
}
#endif /* INTNET_WITH_DHCP_SNOOPING */


/**
 * Deals with an IPv4 packet.
 *
 * This will fish out the source IP address and add it to the cache.
 * Then it will look for DHCPRELEASE requests (?) and anything else
 * that we migh find useful later.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pIpHdr          Pointer to the IPv4 header in the frame.
 * @param   cbPacket        The size of the packet, or more correctly the
 *                          size of the frame without the ethernet header.
 * @param   fGso            Set if this is a GSO frame, clear if regular.
 */
static void intnetR0IfSnoopIPv4SourceAddr(PINTNETIF pIf, PCRTNETIPV4 pIpHdr, uint32_t cbPacket, bool fGso)
{
    /*
     * Check the header size first to prevent access invalid data.
     */
    if (cbPacket < RTNETIPV4_MIN_LEN)
        return;
    uint32_t cbHdr = (uint32_t)pIpHdr->ip_hl * 4;
    if (    cbHdr < RTNETIPV4_MIN_LEN
        ||  cbPacket < cbHdr)
        return;

    /*
     * If the source address is good (not broadcast or my network) and
     * not already in the address cache of the sender, add it. Validate
     * the IP header before adding it.
     */
    bool fValidatedIpHdr = false;
    RTNETADDRU Addr;
    Addr.IPv4 = pIpHdr->ip_src;
    if (    intnetR0IPv4AddrIsGood(Addr.IPv4)
        &&  intnetR0IfAddrCacheLookupLikely(&pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr, sizeof(Addr.IPv4)) < 0)
    {
        if (!RTNetIPv4IsHdrValid(pIpHdr, cbPacket, cbPacket, !fGso /*fChecksum*/))
        {
            Log(("intnetR0IfSnoopIPv4SourceAddr: bad ip header\n"));
            return;
        }
        intnetR0IfAddrCacheAddIt(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr, "if/ipv4");
        fValidatedIpHdr = true;
    }

#ifdef INTNET_WITH_DHCP_SNOOPING
    /*
     * Check for potential DHCP packets.
     */
    if (    pIpHdr->ip_p == RTNETIPV4_PROT_UDP                              /* DHCP is UDP. */
        &&  cbPacket >= cbHdr + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN  /* Min DHCP packet len. */
        &&  !fGso)                                                          /* GSO is not applicable to DHCP traffic. */
    {
        PCRTNETUDP pUdpHdr = (PCRTNETUDP)((uint8_t const *)pIpHdr + cbHdr);
        if (    (   RT_BE2H_U16(pUdpHdr->uh_dport) == RTNETIPV4_PORT_BOOTPS
                 || RT_BE2H_U16(pUdpHdr->uh_sport) == RTNETIPV4_PORT_BOOTPS)
            &&  (   RT_BE2H_U16(pUdpHdr->uh_sport) == RTNETIPV4_PORT_BOOTPC
                 || RT_BE2H_U16(pUdpHdr->uh_dport) == RTNETIPV4_PORT_BOOTPC))
        {
            if (    fValidatedIpHdr
                ||  RTNetIPv4IsHdrValid(pIpHdr, cbPacket, cbPacket, !fGso /*fChecksum*/))
                intnetR0NetworkSnoopDhcp(pIf->pNetwork, pIpHdr, pUdpHdr, cbPacket - cbHdr);
            else
                Log(("intnetR0IfSnoopIPv4SourceAddr: bad ip header (dhcp)\n"));
        }
    }
#endif /* INTNET_WITH_DHCP_SNOOPING */
}


/**
 * Snoop up source addresses from an ARP request or reply.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pHdr            The ARP header.
 * @param   cbPacket        The size of the packet (migth be larger than the ARP
 *                          request 'cause of min ethernet frame size).
 * @param   pfSgFlags       Pointer to the SG flags. This is used to tag the packet so we
 *                          don't have to repeat the frame parsing in intnetR0TrunkIfSend.
 */
static void intnetR0IfSnoopArpAddr(PINTNETIF pIf, PCRTNETARPIPV4 pArpIPv4, uint32_t cbPacket, uint16_t *pfSgFlags)
{
    /*
     * Ignore packets which doesn't interest us or we perceive as malformed.
     */
    if (RT_UNLIKELY(cbPacket < sizeof(RTNETARPIPV4)))
        return;
    if (RT_UNLIKELY(    pArpIPv4->Hdr.ar_hlen  != sizeof(RTMAC)
                    ||  pArpIPv4->Hdr.ar_plen  != sizeof(RTNETADDRIPV4)
                    ||  pArpIPv4->Hdr.ar_htype != RT_H2BE_U16(RTNET_ARP_ETHER)
                    ||  pArpIPv4->Hdr.ar_ptype != RT_H2BE_U16(RTNET_ETHERTYPE_IPV4)))
        return;
    uint16_t ar_oper = RT_H2BE_U16(pArpIPv4->Hdr.ar_oper);
    if (RT_UNLIKELY(    ar_oper != RTNET_ARPOP_REQUEST
                    &&  ar_oper != RTNET_ARPOP_REPLY))
    {
        Log6(("ar_oper=%#x\n", ar_oper));
        return;
    }

    /*
     * Tag the SG as ARP IPv4 for later editing, then check for addresses
     * which can be removed or added to the address cache of the sender.
     */
    *pfSgFlags |= INTNETSG_FLAGS_ARP_IPV4;

    if (    ar_oper == RTNET_ARPOP_REPLY
        &&  !(pArpIPv4->ar_tha.au8[0] & 1)
        &&  (   pArpIPv4->ar_tha.au16[0]
             || pArpIPv4->ar_tha.au16[1]
             || pArpIPv4->ar_tha.au16[2])
        &&  intnetR0IPv4AddrIsGood(pArpIPv4->ar_tpa))
        intnetR0IfAddrCacheDelete(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4],
                                  (PCRTNETADDRU)&pArpIPv4->ar_tpa, sizeof(RTNETADDRIPV4), "if/arp");

    if (    !memcmp(&pArpIPv4->ar_sha, &pIf->Mac, sizeof(RTMAC))
        &&  intnetR0IPv4AddrIsGood(pArpIPv4->ar_spa))
        intnetR0IfAddrCacheAdd(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4],
                               (PCRTNETADDRU)&pArpIPv4->ar_spa, sizeof(RTNETADDRIPV4), "if/arp");
}



/**
 * Checks packets send by a normal interface for new network
 * layer addresses.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pbFrame         The frame.
 * @param   cbFrame         The size of the frame.
 * @param   fGso            Set if this is a GSO frame, clear if regular.
 * @param   pfSgFlags       Pointer to the SG flags. This is used to tag the packet so we
 *                          don't have to repeat the frame parsing in intnetR0TrunkIfSend.
 */
static void intnetR0IfSnoopAddr(PINTNETIF pIf, uint8_t const *pbFrame, uint32_t cbFrame, bool fGso, uint16_t *pfSgFlags)
{
    /*
     * Fish out the ethertype and look for stuff we can handle.
     */
    if (cbFrame <= sizeof(RTNETETHERHDR))
        return;
    cbFrame -= sizeof(RTNETETHERHDR);

    uint16_t EtherType = RT_H2BE_U16(((PCRTNETETHERHDR)pbFrame)->EtherType);
    switch (EtherType)
    {
        case RTNET_ETHERTYPE_IPV4:
            intnetR0IfSnoopIPv4SourceAddr(pIf, (PCRTNETIPV4)((PCRTNETETHERHDR)pbFrame + 1), cbFrame, fGso);
            break;
#if 0 /** @todo IntNet: implement IPv6 for wireless MAC sharing. */
        case RTNET_ETHERTYPE_IPV6:
            /** @todo IPv6: Check for ICMPv6. It looks like type 133 (Router solicitation) might
             * need to be edited. Check out how NDP works...  */
            intnetR0IfSnoopIPv6SourceAddr(pIf, (PCINTNETIPV6)((PCRTNETETHERHDR)pbFrame + 1), cbFrame, fGso, pfSgFlags);
            break;
#endif
#if 0 /** @todo IntNet: implement IPX for wireless MAC sharing? */
        case RTNET_ETHERTYPE_IPX_1:
        case RTNET_ETHERTYPE_IPX_2:
        case RTNET_ETHERTYPE_IPX_3:
            intnetR0IfSnoopIpxSourceAddr(pIf, (PCINTNETIPX)((PCRTNETETHERHDR)pbFrame + 1), cbFrame, pfSgFlags);
            break;
#endif
        case RTNET_ETHERTYPE_ARP:
            intnetR0IfSnoopArpAddr(pIf, (PCRTNETARPIPV4)((PCRTNETETHERHDR)pbFrame + 1), cbFrame, pfSgFlags);
            break;
    }
}


/**
 * Writes a frame packet to the ring buffer.
 *
 * @returns VBox status code.
 * @param   pBuf            The buffer.
 * @param   pRingBuf        The ring buffer to read from.
 * @param   pSG             The gather list.
 * @param   pNewDstMac      Set the destination MAC address to the address if specified.
 */
static int intnetR0RingWriteFrame(PINTNETRINGBUF pRingBuf, PCINTNETSG pSG, PCRTMAC pNewDstMac)
{
    PINTNETHDR  pHdr  = NULL; /* shut up gcc*/
    void       *pvDst = NULL; /* ditto */
    int         rc;
    if (pSG->GsoCtx.u8Type == PDMNETWORKGSOTYPE_INVALID)
        rc = INTNETRingAllocateFrame(pRingBuf, pSG->cbTotal, &pHdr, &pvDst);
    else
        rc = INTNETRingAllocateGsoFrame(pRingBuf, pSG->cbTotal, &pSG->GsoCtx, &pHdr, &pvDst);
    if (RT_SUCCESS(rc))
    {
        INTNETSgRead(pSG, pvDst);
        if (pNewDstMac)
            ((PRTNETETHERHDR)pvDst)->DstMac = *pNewDstMac;

        INTNETRingCommitFrame(pRingBuf, pHdr);
        return VINF_SUCCESS;
    }
    return rc;
}


/**
 * Sends a frame to a specific interface.
 *
 * @param   pIf             The interface.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   pSG             The gather buffer which data is being sent to the interface.
 * @param   pNewDstMac      Set the destination MAC address to the address if specified.
 */
static void intnetR0IfSend(PINTNETIF pIf, PINTNETIF pIfSender, PINTNETSG pSG, PCRTMAC pNewDstMac)
{
//    LogFlow(("intnetR0IfSend: pIf=%p:{.hIf=%RX32}\n", pIf, pIf->hIf));
    int rc = intnetR0RingWriteFrame(&pIf->pIntBuf->Recv, pSG, pNewDstMac);
    if (RT_SUCCESS(rc))
    {
        pIf->cYields = 0;
        RTSemEventSignal(pIf->Event);
        return;
    }

    Log(("intnetR0IfSend: overflow cb=%d hIf=%RX32\n", pSG->cbTotal, pIf->hIf));

#if 0 /* This is bad stuff now as we're blocking while locking down the network.
         we really shouldn't delay the network traffic on the host just because
         some bugger isn't responding. Will have to deal with this in a different
         manner if required. */
    /*
     * Retry a few times, yielding the CPU in between.
     * But don't let a unresponsive VM harm performance, so give up after a couple of tries.
     */
    if (    pIf->fActive
        &&  pIf->cYields < 100)
    {
        unsigned cYields = 10;
#else
    /*
     * Scheduling hack, for unicore machines primarily.
     */
    if (    pIf->fActive
        &&  pIf->cYields < 4 /* just twice */
        &&  pIfSender /* but not if it's from the trunk */)
    {
        unsigned cYields = 2;
#endif
        while (--cYields > 0)
        {
            RTSemEventSignal(pIf->Event);
            RTThreadYield();
            rc = intnetR0RingWriteFrame(&pIf->pIntBuf->Recv, pSG, pNewDstMac);
            if (RT_SUCCESS(rc))
            {
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatYieldsOk);
                RTSemEventSignal(pIf->Event);
                return;
            }
            pIf->cYields++;
        }
        STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatYieldsNok);
    }

    /* ok, the frame is lost. */
    STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatLost);
    RTSemEventSignal(pIf->Event);
}


/**
 * Fallback path that does the GSO segmenting before passing the frame on to the
 * trunk interface.
 *
 * The caller holds the trunk lock.
 *
 * @param   pThis           The trunk.
 * @param   pSG             Pointer to the gather list.
 * @param   fDst            The destination flags.
 */
static int intnetR0TrunkIfSendGsoFallback(PINTNETTRUNKIF pThis, PINTNETSG pSG, uint32_t fDst)
{
    /*
     * Since we're only using this for GSO frame comming from the internal
     * network interfaces and never the trunk, we can assume there is only
     * one segment.  This simplifies the code quite a bit.
     */
    Assert(PDMNetGsoIsValid(&pSG->GsoCtx, sizeof(pSG->GsoCtx), pSG->cbTotal));
    AssertReturn(pSG->cSegsUsed == 1, VERR_INTERNAL_ERROR_4);

    union
    {
        uint8_t     abBuf[sizeof(INTNETSG) + sizeof(INTNETSEG)];
        INTNETSG    SG;
    } u;

    /*
     * Carve out the frame segments with the header and frame in different
     * scatter / gather segments.
     */
    uint32_t const cSegs = PDMNetGsoCalcSegmentCount(&pSG->GsoCtx, pSG->cbTotal);
    for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
    {
        uint32_t cbSegPayload;
        uint32_t offSegPayload = PDMNetGsoCarveSegment(&pSG->GsoCtx, (uint8_t *)pSG->aSegs[0].pv, pSG->cbTotal, iSeg, cSegs,
                                                       pThis->abGsoHdrs, &cbSegPayload);

        INTNETSgInitTempSegs(&u.SG, pSG->GsoCtx.cbHdrs + cbSegPayload, 2, 2);
        u.SG.aSegs[0].Phys = NIL_RTHCPHYS;
        u.SG.aSegs[0].pv   = pThis->abGsoHdrs;
        u.SG.aSegs[0].cb   = pSG->GsoCtx.cbHdrs;
        u.SG.aSegs[1].Phys = NIL_RTHCPHYS;
        u.SG.aSegs[1].pv   = (uint8_t *)pSG->aSegs[0].pv + offSegPayload;
        u.SG.aSegs[1].cb   = (uint32_t)cbSegPayload;

        int rc = pThis->pIfPort->pfnXmit(pThis->pIfPort, &u.SG, fDst);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}


/**
 * Checks if any of the given trunk destinations can handle this kind of GSO SG.
 *
 * @returns true if it can, false if it cannot.
 * @param   pThis               The trunk.
 * @param   pSG                 The scatter / gather buffer.
 * @param   fDst                The desitination mask.
 */
DECLINLINE(bool) intnetR0TrunkIfCanHandleGsoFrame(PINTNETTRUNKIF pThis, PINTNETSG pSG, uint32_t fDst)
{
    uint8_t     u8Type = pSG->GsoCtx.u8Type;
    AssertReturn(u8Type < 32, false);   /* paranoia */
    uint32_t    fMask  = RT_BIT_32(u8Type);

    if (fDst == INTNETTRUNKDIR_HOST)
        return !!(pThis->fHostGsoCapabilites & fMask);
    if (fDst == INTNETTRUNKDIR_WIRE)
        return !!(pThis->fWireGsoCapabilites & fMask);
    Assert(fDst == (INTNETTRUNKDIR_WIRE | INTNETTRUNKDIR_HOST));
    return !!(pThis->fHostGsoCapabilites & pThis->fWireGsoCapabilites & fMask);
}


/**
 * Sends a frame down the trunk.
 *
 * The caller must own the network mutex, might be abandond temporarily.
 * The fTrunkLock parameter indicates whether the trunk lock is held.
 *
 * @param   pThis           The trunk.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pIfSender       The IF sending the frame. Used for MAC address checks in shared MAC mode.
 * @param   fDst            The destination flags.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 */
static void intnetR0TrunkIfSend(PINTNETTRUNKIF pThis, PINTNETNETWORK pNetwork, PINTNETIF pIfSender,
                                uint32_t fDst, PINTNETSG pSG, bool fTrunkLocked)
{
    /*
     * Quick sanity check.
     */
    AssertPtr(pThis);
    AssertPtr(pNetwork);
    AssertPtr(pSG);
    Assert(fDst);
    AssertReturnVoid(pThis->pIfPort);

    /*
     * Edit the frame if we're sharing the MAC address with the host on the wire.
     *
     * If the frame is headed for both the host and the wire, we'll have to send
     * it to the host before making any modifications, and force the OS specific
     * backend to copy it. We do this by marking it as TEMP (which is always the
     * case right now).
     */
    if (    (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
        &&  (fDst & INTNETTRUNKDIR_WIRE))
    {
        /* Dispatch it to the host before making changes. */
        if (fDst & INTNETTRUNKDIR_HOST)
        {
            Assert(pSG->fFlags & INTNETSG_FLAGS_TEMP); /* make sure copy is forced */
            intnetR0TrunkIfSend(pThis, pNetwork, pIfSender, INTNETTRUNKDIR_HOST, pSG, fTrunkLocked);
            fDst &= ~INTNETTRUNKDIR_HOST;
        }

        /* ASSUME frame from INTNETR0IfSend! */
        AssertReturnVoid(pSG->cSegsUsed == 1);
        AssertReturnVoid(pSG->cbTotal >= sizeof(RTNETETHERHDR));
        AssertReturnVoid(fTrunkLocked);
        AssertReturnVoid(pIfSender);
        PRTNETETHERHDR pEthHdr = (PRTNETETHERHDR)pSG->aSegs[0].pv;

        /*
         * Get the host mac address and update the ethernet header.
         *
         * The reason for caching it in the trunk structure is because
         * we cannot take the trunk out-bound semaphore when we make
         * edits in the intnetR0TrunkIfPortRecv path.
         */
        pThis->pIfPort->pfnGetMacAddress(pThis->pIfPort, &pThis->CachedMac);
        if (!memcmp(&pEthHdr->SrcMac, &pIfSender->Mac, sizeof(RTMAC)))
            pEthHdr->SrcMac = pThis->CachedMac;

        /*
         * Deal with tags from the snooping phase.
         */
        if (pSG->fFlags & INTNETSG_FLAGS_ARP_IPV4)
        {
            /*
             * APR IPv4: replace hardware (MAC) addresses because these end up
             *           in ARP caches. So, if we don't the other machiens will
             *           send the packets to the MAC address of the guest
             *           instead of the one of the host, which won't work on
             *           wireless of course...
             */
            PRTNETARPIPV4 pArp = (PRTNETARPIPV4)(pEthHdr + 1);
            if (!memcmp(&pArp->ar_sha, &pIfSender->Mac, sizeof(RTMAC)))
            {
                Log6(("tw: ar_sha %.6Rhxs -> %.6Rhxs\n", &pArp->ar_sha, &pThis->CachedMac));
                pArp->ar_sha = pThis->CachedMac;
            }
            if (!memcmp(&pArp->ar_tha, &pIfSender->Mac, sizeof(RTMAC))) /* just in case... */
            {
                Log6(("tw: ar_tha %.6Rhxs -> %.6Rhxs\n", &pArp->ar_tha, &pThis->CachedMac));
                pArp->ar_tha = pThis->CachedMac;
            }
        }
        //else if (pSG->fFlags & INTNETSG_FLAGS_ICMPV6_NDP)
        //{ /// @todo move the editing into a different function
        //}
    }

    /*
     * Temporarily leave the network lock while transmitting the frame.
     *
     * Note that we're relying on the out-bound lock to serialize threads down
     * in INTNETR0IfSend.  It's theoretically possible for there to be race now
     * because I didn't implement async SG handling yet.  Which is why we
     * currently require the trunk to be locked, well, one of the reasons.
     *
     * Another reason is that the intnetR0NetworkSendUnicast code may have to
     * call into the trunk interface component to do package switching.
     */
    AssertReturnVoid(fTrunkLocked); /* to be removed. */

    int rc;
    if (    fTrunkLocked
        ||  intnetR0TrunkIfRetain(pThis))
    {
        rc = RTSemFastMutexRelease(pNetwork->FastMutex2);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            if (    fTrunkLocked
                ||  intnetR0TrunkIfOutLock(pThis))
            {
                if (   pSG->GsoCtx.u8Type == PDMNETWORKGSOTYPE_INVALID
                    || intnetR0TrunkIfCanHandleGsoFrame(pThis, pSG, fDst) )
                    rc = pThis->pIfPort->pfnXmit(pThis->pIfPort, pSG, fDst);
                else
                    rc = intnetR0TrunkIfSendGsoFallback(pThis, pSG, fDst);

                if (!fTrunkLocked)
                    intnetR0TrunkIfOutUnlock(pThis);
            }
            else
            {
                AssertFailed();
                rc = VERR_SEM_DESTROYED;
            }

            int rc2 = RTSemFastMutexRequest(pNetwork->FastMutex2);
            AssertRC(rc2);
        }

        if (!fTrunkLocked)
            intnetR0TrunkIfRelease(pThis);
    }
    else
    {
        AssertFailed();
        rc = VERR_SEM_DESTROYED;
    }

    /** @todo failure statistics? */
    Log2(("intnetR0TrunkIfSend: %Rrc fDst=%d\n", rc, fDst));
}


/**
 * Edits an ARP packet arriving from the wire via the trunk connection.
 *
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pSG             Pointer to the gather list for the frame.
 *                          The flags and data content may be updated.
 * @param   pEthHdr         Pointer to the ethernet header. This may also be
 *                          updated if it's a unicast...
 */
static void intnetR0NetworkEditArpFromWire(PINTNETNETWORK pNetwork, PINTNETSG pSG, PRTNETETHERHDR pEthHdr)
{
    /*
     * Check the minimum size and get a linear copy of the thing to work on,
     * using the temporary buffer if necessary.
     */
    if (RT_UNLIKELY(pSG->cbTotal < sizeof(RTNETETHERHDR) + sizeof(RTNETARPIPV4)))
        return;
    PRTNETARPIPV4 pArpIPv4 = (PRTNETARPIPV4)((uint8_t *)pSG->aSegs[0].pv + sizeof(RTNETETHERHDR));
    if (    pSG->cSegsUsed != 1
        &&  pSG->aSegs[0].cb < sizeof(RTNETETHERHDR) + sizeof(RTNETARPIPV4))
    {
        Log6(("fw: Copying ARP pkt %u\n", sizeof(RTNETARPIPV4)));
        if (!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR), sizeof(RTNETARPIPV4), pNetwork->pbTmp))
            return;
        pSG->fFlags |= INTNETSG_FLAGS_PKT_CP_IN_TMP;
        pArpIPv4 = (PRTNETARPIPV4)pNetwork->pbTmp;
    }

    /*
     * Ignore packets which doesn't interest us or we perceive as malformed.
     */
    if (RT_UNLIKELY(    pArpIPv4->Hdr.ar_hlen  != sizeof(RTMAC)
                    ||  pArpIPv4->Hdr.ar_plen  != sizeof(RTNETADDRIPV4)
                    ||  pArpIPv4->Hdr.ar_htype != RT_H2BE_U16(RTNET_ARP_ETHER)
                    ||  pArpIPv4->Hdr.ar_ptype != RT_H2BE_U16(RTNET_ETHERTYPE_IPV4)))
        return;
    uint16_t ar_oper = RT_H2BE_U16(pArpIPv4->Hdr.ar_oper);
    if (RT_UNLIKELY(    ar_oper != RTNET_ARPOP_REQUEST
                    &&  ar_oper != RTNET_ARPOP_REPLY))
    {
        Log6(("ar_oper=%#x\n", ar_oper));
        return;
    }

    /* Tag it as ARP IPv4. */
    pSG->fFlags |= INTNETSG_FLAGS_ARP_IPV4;

    /*
     * The thing we're interested in here is a reply to a query made by a guest
     * since we modified the MAC in the initial request the guest made.
     */
    if (    ar_oper == RTNET_ARPOP_REPLY
        &&  !memcmp(&pArpIPv4->ar_tha, &pNetwork->pTrunkIF->CachedMac, sizeof(RTMAC)))
    {
        PINTNETIF pIf = intnetR0NetworkAddrCacheLookupIf(pNetwork, (PCRTNETADDRU)&pArpIPv4->ar_tpa,
                                                         kIntNetAddrType_IPv4, sizeof(pArpIPv4->ar_tpa));
        if (pIf)
        {
            Log6(("fw: ar_tha %.6Rhxs -> %.6Rhxs\n", &pArpIPv4->ar_tha, &pIf->Mac));
            pArpIPv4->ar_tha = pIf->Mac;
            if (!memcmp(&pEthHdr->DstMac, &pNetwork->pTrunkIF->CachedMac, sizeof(RTMAC)))
            {
                Log6(("fw: DstMac %.6Rhxs -> %.6Rhxs\n", &pEthHdr->DstMac, &pIf->Mac));
                pEthHdr->DstMac = pIf->Mac;
                if ((void *)pEthHdr != pSG->aSegs[0].pv)
                    intnetR0SgWritePart(pSG, RT_OFFSETOF(RTNETETHERHDR, DstMac), sizeof(RTMAC), &pIf->Mac);
            }

            /* Write back the packet if we've been making changes to a buffered copy. */
            if (pSG->fFlags & INTNETSG_FLAGS_PKT_CP_IN_TMP)
                intnetR0SgWritePart(pSG, sizeof(RTNETETHERHDR), sizeof(PRTNETARPIPV4), pArpIPv4);
        }
    }
}


/**
 * Detects and edits an DHCP packet arriving from the internal net.
 *
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pSG             Pointer to the gather list for the frame.
 *                          The flags and data content may be updated.
 * @param   pEthHdr         Pointer to the ethernet header. This may also be
 *                          updated if it's a unicast...
 */
static void intnetR0NetworkEditDhcpFromIntNet(PINTNETNETWORK pNetwork, PINTNETSG pSG, PRTNETETHERHDR pEthHdr)
{
    /*
     * Check the minimum size and get a linear copy of the thing to work on,
     * using the temporary buffer if necessary.
     */
    if (RT_UNLIKELY(pSG->cbTotal < sizeof(RTNETETHERHDR) + RTNETIPV4_MIN_LEN + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN))
        return;
    /*
     * Get a pointer to a linear copy of the full packet, using the
     * temporary buffer if necessary.
     */
    PCRTNETIPV4 pIpHdr = (PCRTNETIPV4)((PCRTNETETHERHDR)pSG->aSegs[0].pv + 1);
    uint32_t cbPacket = pSG->cbTotal - sizeof(RTNETETHERHDR);
    if (pSG->cSegsUsed > 1)
    {
        cbPacket = RT_MIN(cbPacket, INTNETNETWORK_TMP_SIZE);
        Log6(("intnetR0NetworkEditDhcpFromIntNet: Copying IPv4/UDP/DHCP pkt %u\n", cbPacket));
        if (!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR), cbPacket, pNetwork->pbTmp))
            return;
        //pSG->fFlags |= INTNETSG_FLAGS_PKT_CP_IN_TMP;
        pIpHdr = (PCRTNETIPV4)pNetwork->pbTmp;
    }

    /*
     * Validate the IP header and find the UDP packet.
     */
    if (!RTNetIPv4IsHdrValid(pIpHdr, cbPacket, pSG->cbTotal - sizeof(RTNETETHERHDR), true /*fCheckSum*/))
    {
        Log6(("intnetR0NetworkEditDhcpFromIntNet: bad ip header\n"));
        return;
    }
    size_t cbIpHdr = pIpHdr->ip_hl * 4;
    if (    pIpHdr->ip_p != RTNETIPV4_PROT_UDP                               /* DHCP is UDP. */
        ||  cbPacket < cbIpHdr + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN) /* Min DHCP packet len */
        return;

    size_t cbUdpPkt = cbPacket - cbIpHdr;
    PCRTNETUDP pUdpHdr = (PCRTNETUDP)((uintptr_t)pIpHdr + cbIpHdr);
    /* We are only interested in DHCP packets coming from client to server. */
    if (    RT_BE2H_U16(pUdpHdr->uh_dport) != RTNETIPV4_PORT_BOOTPS
         || RT_BE2H_U16(pUdpHdr->uh_sport) != RTNETIPV4_PORT_BOOTPC)
        return;

    /*
     * Check if the DHCP message is valid and get the type.
     */
    if (!RTNetIPv4IsUDPValid(pIpHdr, pUdpHdr, pUdpHdr + 1, cbUdpPkt, true /*fCheckSum*/))
    {
        Log6(("intnetR0NetworkEditDhcpFromIntNet: Bad UDP packet\n"));
        return;
    }
    PCRTNETBOOTP pDhcp = (PCRTNETBOOTP)(pUdpHdr + 1);
    uint8_t MsgType;
    if (!RTNetIPv4IsDHCPValid(pUdpHdr, pDhcp, cbUdpPkt - sizeof(*pUdpHdr), &MsgType))
    {
        Log6(("intnetR0NetworkEditDhcpFromIntNet: Bad DHCP packet\n"));
        return;
    }

    switch (MsgType)
    {
        case RTNET_DHCP_MT_DISCOVER:
        case RTNET_DHCP_MT_REQUEST:
            Log6(("intnetR0NetworkEditDhcpFromIntNet: Setting broadcast flag in DHCP %#x, previously %x\n", MsgType, pDhcp->bp_flags));
            if (!(pDhcp->bp_flags & RT_H2BE_U16_C(RTNET_DHCP_FLAG_BROADCAST)))
            {
                /* Patch flags */
                uint16_t uFlags = pDhcp->bp_flags | RT_H2BE_U16_C(RTNET_DHCP_FLAG_BROADCAST);
                intnetR0SgWritePart(pSG, (uintptr_t)&pDhcp->bp_flags - (uintptr_t)pIpHdr + sizeof(RTNETETHERHDR), sizeof(uFlags), &uFlags);
                /* Patch UDP checksum */
                uint32_t uChecksum = (uint32_t)~pUdpHdr->uh_sum + RT_H2BE_U16_C(RTNET_DHCP_FLAG_BROADCAST);
                while (uChecksum >> 16)
                    uChecksum = (uChecksum >> 16) + (uChecksum & 0xFFFF);
                uChecksum = ~uChecksum;
                intnetR0SgWritePart(pSG, (uintptr_t)&pUdpHdr->uh_sum - (uintptr_t)pIpHdr + sizeof(RTNETETHERHDR), sizeof(pUdpHdr->uh_sum), &uChecksum);
            }
            break;
    }
}


/**
 * Sends a broadcast frame.
 *
 * The caller must own the network mutex, might be abandond temporarily.
 * When pIfSender is not NULL, the caller must also own the trunk semaphore.
 *
 * @returns true if it's addressed to someone on the network, otherwise false.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   fSrc            The source flags. This 0 if it's not from the trunk.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 * @param   pEthHdr         Pointer to the ethernet header.
 */
static bool intnetR0NetworkSendBroadcast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc,
                                         PINTNETSG pSG, bool fTrunkLocked, PRTNETETHERHDR pEthHdr)
{
    /*
     * Check for ARP packets from the wire since we'll have to make
     * modification to them if we're sharing the MAC address with the host.
     */
    if (    (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
        &&  (fSrc & INTNETTRUNKDIR_WIRE)
        &&  RT_BE2H_U16(pEthHdr->EtherType) == RTNET_ETHERTYPE_ARP)
        intnetR0NetworkEditArpFromWire(pNetwork, pSG, pEthHdr);

     /*
     * Check for DHCP packets from the internal net since we'll have to set
     * broadcast flag in DHCP requests if we're sharing the MAC address with
     * the host.  GSO is not applicable to DHCP traffic.
     */
    if (    (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
        &&  !fSrc
        &&  RT_BE2H_U16(pEthHdr->EtherType) == RTNET_ETHERTYPE_IPV4
        &&  pSG->GsoCtx.u8Type == PDMNETWORKGSOTYPE_INVALID)
        intnetR0NetworkEditDhcpFromIntNet(pNetwork, pSG, pEthHdr);

    /*
     * This is a broadcast or multicast address. For the present we treat those
     * two as the same - investigating multicast is left for later.
     *
     * Write the packet to all the interfaces and signal them.
     */
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
        if (pIf != pIfSender)
            intnetR0IfSend(pIf, pIfSender, pSG, NULL);

    /*
     * Unless the trunk is the origin, broadcast it to both the wire
     * and the host as well.
     */
    PINTNETTRUNKIF pTrunkIf = pNetwork->pTrunkIF;
    if (    pIfSender
        &&  pTrunkIf)
        intnetR0TrunkIfSend(pTrunkIf, pNetwork, pIfSender, INTNETTRUNKDIR_HOST | INTNETTRUNKDIR_WIRE, pSG, fTrunkLocked);

    /*
     * Snoop address info from packet orginating from the trunk connection.
     */
    else if (   (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
             && !pIfSender)
    {
#ifdef INTNET_WITH_DHCP_SNOOPING
        uint16_t EtherType = RT_BE2H_U16(pEthHdr->EtherType);
        if (    (   EtherType == RTNET_ETHERTYPE_IPV4       /* for DHCP */
                 && pSG->cbTotal >= sizeof(RTNETETHERHDR) + RTNETIPV4_MIN_LEN + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN
                 && pSG->GsoCtx.u8Type == PDMNETWORKGSOTYPE_INVALID )
            ||  (pSG->fFlags & (INTNETSG_FLAGS_ARP_IPV4)) )
            intnetR0TrunkIfSnoopAddr(pNetwork, pSG, EtherType);
#else
       if (pSG->fFlags & (INTNETSG_FLAGS_ARP_IPV4))
           intnetR0TrunkIfSnoopArp(pNetwork, pSG);
#endif
    }

    return false; /* broadcast frames are never dropped */
}


/**
 * Sends a multicast frame.
 *
 * The caller must own the network mutex, might be abandond temporarily.
 *
 * @returns true if it's addressed to someone on the network, otherwise false.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   fSrc            The source flags. This 0 if it's not from the trunk.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 * @param   pEthHdr         Pointer to the ethernet header.
 */
static bool intnetR0NetworkSendMulticast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc, PINTNETSG pSG, bool fTrunkLocked, PRTNETETHERHDR pEthHdr)
{
    /** @todo implement multicast */
    return intnetR0NetworkSendBroadcast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, pEthHdr);
}


/**
 * Sends a unicast frame using the network layer address instead
 * of the link layer one.
 *
 * This function is only used for frames comming from the write (trunk).
 *
 * The caller must own the network mutex, might be abandond temporarily.
 *
 * @returns true if it's addressed to someone on the network, otherwise false.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 * @param   pEthHdr         Pointer to the ethernet header.
 */
static bool intnetR0NetworkSendUnicastWithSharedMac(PINTNETNETWORK pNetwork, PINTNETSG pSG, bool fTrunkLocked, PRTNETETHERHDR pEthHdr)
{
    /*
     * Extract the network address from the packet.
     */
    RTNETADDRU      Addr;
    INTNETADDRTYPE  enmAddrType;
    uint8_t         cbAddr;
    switch (RT_BE2H_U16(pEthHdr->EtherType))
    {
        case RTNET_ETHERTYPE_IPV4:
            if (RT_UNLIKELY(!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR) + RT_OFFSETOF(RTNETIPV4, ip_dst), sizeof(Addr.IPv4), &Addr)))
            {
                Log(("intnetshareduni: failed to read ip_dst! cbTotal=%#x\n", pSG->cbTotal));
                return false;
            }
            enmAddrType = kIntNetAddrType_IPv4;
            cbAddr = sizeof(Addr.IPv4);
            Log6(("intnetshareduni: IPv4 %d.%d.%d.%d\n", Addr.au8[0], Addr.au8[1], Addr.au8[2], Addr.au8[3]));
            break;

#if 0 /** @todo IntNet: implement IPv6 for wireless MAC sharing. */
        case RTNET_ETHERTYPE_IPV6
            if (RT_UNLIKELY(!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR) + RT_OFFSETOF(RTNETIPV6, ip6_dst), sizeof(Addr.IPv6), &Addr)))
            {
                Log(("intnetshareduni: failed to read ip6_dst! cbTotal=%#x\n", pSG->cbTotal));
                return false;
            }
            enmAddrType = kIntNetAddrType_IPv6;
            cbAddr = sizeof(Addr.IPv6);
            break;
#endif
#if 0 /** @todo IntNet: implement IPX for wireless MAC sharing? */
        case RTNET_ETHERTYPE_IPX_1:
        case RTNET_ETHERTYPE_IPX_2:
        case RTNET_ETHERTYPE_IPX_3:
            if (RT_UNLIKELY(!intnetR0SgReadPart(pSG, sizeof(RTNETETHERHDR) + RT_OFFSETOF(RTNETIPX, ipx_dstnet), sizeof(Addr.IPX), &Addr)))
            {
                Log(("intnetshareduni: failed to read ipx_dstnet! cbTotal=%#x\n", pSG->cbTotal));
                return false;
            }
            enmAddrType = kIntNetAddrType_IPX;
            cbAddr = sizeof(Addr.IPX);
            break;
#endif

        /*
         * Treat ARP as broadcast (it shouldn't end up here normally,
         * so it goes last in the switch).
         */
        case RTNET_ETHERTYPE_ARP:
            Log6(("intnetshareduni: ARP\n"));
            /** @todo revisit this broadcasting of unicast ARP frames! */
            return intnetR0NetworkSendBroadcast(pNetwork, NULL, INTNETTRUNKDIR_WIRE, pSG, fTrunkLocked, pEthHdr);

        /*
         * Unknown packets are sent do all interfaces that are in promiscuous mode.
         */
        default:
        {
            Log6(("intnetshareduni: unknown ethertype=%#x\n", RT_BE2H_U16(pEthHdr->EtherType)));
            if (!(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC)))
            {
                for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
                    if (pIf->fPromiscuous)
                    {
                        Log2(("Dst=%.6Rhxs => %.6Rhxs\n", &pEthHdr->DstMac, &pIf->Mac));
                        intnetR0IfSend(pIf, NULL, pSG, NULL);
                    }
            }
            return false;
        }
    }

    /*
     * Send it to interfaces with matching network addresses.
     */
    bool fExactIntNetRecipient = false;
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
    {
        bool fIt = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmAddrType], &Addr, cbAddr) >= 0;
        if (    fIt
            ||  (   pIf->fPromiscuous
                 && !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC))))
        {
            Log2(("Dst=%.6Rhxs => %.6Rhxs\n", &pEthHdr->DstMac, &pIf->Mac));
            fExactIntNetRecipient |= fIt;
            intnetR0IfSend(pIf, NULL, pSG, fIt ? &pIf->Mac : NULL);
        }
    }

#ifdef INTNET_WITH_DHCP_SNOOPING
    /*
     * Perform DHCP snooping. GSO is not applicable to DHCP traffic
     */
    if (    enmAddrType == kIntNetAddrType_IPv4
        &&  pSG->cbTotal >= sizeof(RTNETETHERHDR) + RTNETIPV4_MIN_LEN + RTNETUDP_MIN_LEN + RTNETBOOTP_DHCP_MIN_LEN
        &&  pSG->GsoCtx.u8Type == PDMNETWORKGSOTYPE_INVALID)
        intnetR0TrunkIfSnoopAddr(pNetwork, pSG, RT_BE2H_U16(pEthHdr->EtherType));
#endif /* INTNET_WITH_DHCP_SNOOPING */

    return fExactIntNetRecipient;
}


/**
 * Sends a unicast frame.
 *
 * The caller must own the network mutex, might be abandond temporarily.
 *
 * @returns true if it's addressed to someone on the network, otherwise false.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   fSrc            The source flags. This 0 if it's not from the trunk.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 * @param   pEthHdr         Pointer to the ethernet header.
 */
static bool intnetR0NetworkSendUnicast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc, PINTNETSG pSG, bool fTrunkLocked, PCRTNETETHERHDR pEthHdr)
{
    /*
     * Only send to the interfaces with matching a MAC address.
     */
    bool fExactIntNetRecipient = false;
    for (PINTNETIF pIf = pNetwork->pIfs; pIf; pIf = pIf->pNext)
    {
        bool fIt = false;
        if (    (   !pIf->fMacSet
                 || (fIt = !memcmp(&pIf->Mac, &pEthHdr->DstMac, sizeof(pIf->Mac))) )
            ||  (   pIf->fPromiscuous
                 && !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC))
                 && pIf != pIfSender /* promiscuous mode: omit the sender */))
        {
            Log2(("Dst=%.6Rhxs => %.6Rhxs\n", &pEthHdr->DstMac, &pIf->Mac));
            fExactIntNetRecipient |= fIt;
            intnetR0IfSend(pIf, pIfSender, pSG, NULL);
        }
    }

    /*
     * Send it to the trunk?
     * If we didn't find the recipient on the internal network the
     * frame will hit the wire.
     */
    uint32_t fDst = 0;
    PINTNETTRUNKIF pTrunkIf = pNetwork->pTrunkIF;
    if (    pIfSender
        &&  pTrunkIf
        &&  pTrunkIf->pIfPort)
    {
        Assert(!fSrc);

        /* promiscuous checks first as they are cheaper than pfnIsHostMac. */
        if (    pTrunkIf->fWirePromiscuous
            &&  !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC | INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_WIRE | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_WIRE)) )
            fDst |= INTNETTRUNKDIR_WIRE;
        if (    !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC | INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_HOST | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_HOST))
            &&  pTrunkIf->pIfPort->pfnIsPromiscuous(pTrunkIf->pIfPort) )
            fDst |= INTNETTRUNKDIR_HOST;

        if (    fDst != (INTNETTRUNKDIR_HOST | INTNETTRUNKDIR_WIRE)
            &&  !fExactIntNetRecipient  /* if you have duplicate mac addresses, you're screwed. */ )
        {
            if (pTrunkIf->pIfPort->pfnIsHostMac(pTrunkIf->pIfPort, &pEthHdr->DstMac))
                fDst |= INTNETTRUNKDIR_HOST;
            else
                fDst |= INTNETTRUNKDIR_WIRE;
        }

        if (fDst)
            intnetR0TrunkIfSend(pTrunkIf, pNetwork, pIfSender, fDst, pSG, fTrunkLocked);
    }

    /* log it */
    if (    !fExactIntNetRecipient
        &&  !fDst
        &&  (   (pEthHdr->DstMac.au8[0] == 0x08 && pEthHdr->DstMac.au8[1] == 0x00 && pEthHdr->DstMac.au8[2] == 0x27)
             || (pEthHdr->SrcMac.au8[0] == 0x08 && pEthHdr->SrcMac.au8[1] == 0x00 && pEthHdr->SrcMac.au8[2] == 0x27)))
        Log2(("Dst=%.6Rhxs ??\n", &pEthHdr->DstMac));

    return fExactIntNetRecipient;
}


/**
 * Sends a frame.
 *
 * This function will distribute the frame to the interfaces it is addressed to.
 * It will also update the MAC address of the sender.
 *
 * The caller must own the network mutex.
 *
 * @returns true if it's addressed to someone on the network, otherwise false.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   fSrc            The source flags. This 0 if it's not from the trunk.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 */
static bool intnetR0NetworkSend(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc, PINTNETSG pSG, bool fTrunkLocked)
{
    bool fRc = false;

    /*
     * Assert reality.
     */
    AssertPtr(pNetwork);
    AssertPtrNull(pIfSender);
    Assert(pIfSender ? fSrc == 0 : fSrc != 0);
    Assert(!pIfSender || pNetwork == pIfSender->pNetwork);
    AssertPtr(pSG);
    Assert(pSG->cSegsUsed >= 1);
    Assert(pSG->cSegsUsed <= pSG->cSegsAlloc);
    if (pSG->cbTotal < sizeof(RTNETETHERHDR))
        return fRc;

    /*
     * Get the ethernet header (might theoretically involve multiple segments).
     */
    RTNETETHERHDR EthHdr;
    if (pSG->aSegs[0].cb >= sizeof(EthHdr))
        EthHdr = *(PCRTNETETHERHDR)pSG->aSegs[0].pv;
    else if (!intnetR0SgReadPart(pSG, 0, sizeof(EthHdr), &EthHdr))
        return false;
    if (    (EthHdr.DstMac.au8[0] == 0x08 && EthHdr.DstMac.au8[1] == 0x00 && EthHdr.DstMac.au8[2] == 0x27)
        ||  (EthHdr.SrcMac.au8[0] == 0x08 && EthHdr.SrcMac.au8[1] == 0x00 && EthHdr.SrcMac.au8[2] == 0x27)
        ||  (EthHdr.DstMac.au8[0] == 0x00 && EthHdr.DstMac.au8[1] == 0x16 && EthHdr.DstMac.au8[2] == 0xcb)
        ||  (EthHdr.SrcMac.au8[0] == 0x00 && EthHdr.SrcMac.au8[1] == 0x16 && EthHdr.SrcMac.au8[2] == 0xcb)
        ||  EthHdr.DstMac.au8[0] == 0xff
        ||  EthHdr.SrcMac.au8[0] == 0xff)
        Log2(("D=%.6Rhxs  S=%.6Rhxs  T=%04x f=%x z=%x\n",
              &EthHdr.DstMac, &EthHdr.SrcMac, RT_BE2H_U16(EthHdr.EtherType), fSrc, pSG->cbTotal));

    /*
     * Inspect the header updating the mac address of the sender in the process.
     */
    if (    pIfSender
        &&  memcmp(&EthHdr.SrcMac, &pIfSender->Mac, sizeof(pIfSender->Mac)))
    {
        /** @todo stats */
        Log2(("IF MAC: %.6Rhxs -> %.6Rhxs\n", &pIfSender->Mac, &EthHdr.SrcMac));
        pIfSender->Mac = EthHdr.SrcMac;
        pIfSender->fMacSet = true;
    }

    /*
     * Distribute the frame.
     */
    if (   EthHdr.DstMac.au16[0] == 0xffff              /* broadcast address. */
        && EthHdr.DstMac.au16[1] == 0xffff
        && EthHdr.DstMac.au16[2] == 0xffff)
        fRc = intnetR0NetworkSendBroadcast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, &EthHdr);
    else if (RT_UNLIKELY(EthHdr.DstMac.au8[0] & 1))     /* multicast address */
        fRc = intnetR0NetworkSendMulticast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, &EthHdr);
    else if (   !(pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
             || !(fSrc & INTNETTRUNKDIR_WIRE))
        fRc = intnetR0NetworkSendUnicast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, &EthHdr);
    else
        fRc = intnetR0NetworkSendUnicastWithSharedMac(pNetwork, pSG, fTrunkLocked, &EthHdr);
    return fRc;
}


/**
 * Sends one or more frames.
 *
 * The function will first the frame which is passed as the optional
 * arguments pvFrame and cbFrame. These are optional since it also
 * possible to chain together one or more frames in the send buffer
 * which the function will process after considering it's arguments.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIf         The interface handle.
 * @param   pSession    The caller's session.
 */
INTNETR0DECL(int) INTNETR0IfSend(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession)
{
    Log5(("INTNETR0IfSend: pIntNet=%p hIf=%RX32\n", pIntNet, hIf));

    /*
     * Validate input and translate the handle.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    /*
     * Lock the network. If there is a trunk retain it and grab its
     * out-bound lock (this requires leaving the network lock first).
     * Grabbing the out-bound lock here simplifies things quite a bit
     * later on, so while this is excessive and a bit expensive it's
     * not worth caring about right now.
     */
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    int rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
    if (RT_FAILURE(rc))
    {
        intnetR0IfRelease(pIf, pSession);
        return rc;
    }
    PINTNETTRUNKIF pTrunkIf = intnetR0TrunkIfRetain(pNetwork->pTrunkIF);
    if (pTrunkIf)
    {
        RTSemFastMutexRelease(pIf->pNetwork->FastMutex2);

        if (!intnetR0TrunkIfOutLock(pTrunkIf))
        {
            intnetR0TrunkIfRelease(pTrunkIf);
            intnetR0IfRelease(pIf, pSession);
            return VERR_SEM_DESTROYED;
        }

        rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
        if (RT_FAILURE(rc))
        {
            intnetR0TrunkIfOutUnlock(pTrunkIf);
            intnetR0TrunkIfRelease(pTrunkIf);
            intnetR0IfRelease(pIf, pSession);
            return rc;
        }
    }

    INTNETSG Sg; /** @todo this will have to be changed if we're going to use async sending
                  * with buffer sharing for some OS or service. Darwin copies everything so
                  * I won't bother allocating and managing SGs rigth now. Sorry. */

    /*
     * Process the send buffer.
     */
    PINTNETHDR pHdr;
    while ((pHdr = INTNETRingGetNextFrameToRead(&pIf->pIntBuf->Send)) != NULL)
    {
        uint16_t const u16Type = pHdr->u16Type;
        if (u16Type == INTNETHDR_TYPE_FRAME)
        {
            /* Send regular frame. */
            void *pvCurFrame = INTNETHdrGetFramePtr(pHdr, pIf->pIntBuf);
            INTNETSgInitTemp(&Sg, pvCurFrame, pHdr->cbFrame);
            if (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
                intnetR0IfSnoopAddr(pIf, (uint8_t *)pvCurFrame, pHdr->cbFrame, false /*fGso*/, (uint16_t *)&Sg.fFlags);
            intnetR0NetworkSend(pNetwork, pIf, 0, &Sg, !!pTrunkIf);
        }
        else if (u16Type == INTNETHDR_TYPE_GSO)
        {
            /* Send GSO frame if sane*/
            PPDMNETWORKGSO  pGso       = INTNETHdrGetGsoContext(pHdr, pIf->pIntBuf);
            uint32_t        cbFrame    = pHdr->cbFrame - sizeof(*pGso);
            if (RT_LIKELY(PDMNetGsoIsValid(pGso, pHdr->cbFrame, cbFrame)))
            {
                void       *pvCurFrame = pGso + 1;
                INTNETSgInitTempGso(&Sg, pvCurFrame, cbFrame, pGso);
                if (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
                    intnetR0IfSnoopAddr(pIf, (uint8_t *)pvCurFrame, cbFrame, true /*fGso*/, (uint16_t *)&Sg.fFlags);
                intnetR0NetworkSend(pNetwork, pIf, 0, &Sg, !!pTrunkIf);
            }
            else
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatBadFrames); /* ignore */
        }
        /* Unless it's a padding frame, we're getting babble from the producer. */
        else if (u16Type != INTNETHDR_TYPE_PADDING)
            STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatBadFrames); /* ignore */

        /* Skip to the next frame. */
        INTNETRingSkipFrame(&pIf->pIntBuf->Send);
    }

    /*
     * Release the semaphore(s) and release references.
     */
    rc = RTSemFastMutexRelease(pNetwork->FastMutex2);
    if (pTrunkIf)
    {
        intnetR0TrunkIfOutUnlock(pTrunkIf);
        intnetR0TrunkIfRelease(pTrunkIf);
    }

    intnetR0IfRelease(pIf, pSession);
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfSend.
 *
 * @returns see INTNETR0IfSend.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSendReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFSENDREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSend(pIntNet, pReq->hIf, pSession);
}


/**
 * Maps the default buffer into ring 3.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance data.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   ppRing3Buf      Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing3Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, R3PTRTYPE(PINTNETBUF) *ppRing3Buf)
{
    LogFlow(("INTNETR0IfGetRing3Buffer: pIntNet=%p hIf=%RX32 ppRing3Buf=%p\n", pIntNet, hIf, ppRing3Buf));

    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppRing3Buf, VERR_INVALID_PARAMETER);
    *ppRing3Buf = 0;
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    /*
     * ASSUMES that only the process that created an interface can use it.
     * ASSUMES that we created the ring-3 mapping when selecting or
     * allocating the buffer.
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex2);
    if (RT_SUCCESS(rc))
    {
        *ppRing3Buf = pIf->pIntBufR3;
        rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex2);
    }

    intnetR0IfRelease(pIf, pSession);
    LogFlow(("INTNETR0IfGetRing3Buffer: returns %Rrc *ppRing3Buf=%p\n", rc, *ppRing3Buf));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfGetRing3Buffer.
 *
 * @returns see INTNETR0IfGetRing3Buffer.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfGetRing3BufferReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFGETRING3BUFFERREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfGetRing3Buffer(pIntNet, pReq->hIf, pSession, &pReq->pRing3Buf);
}


/**
 * Gets the ring-0 address of the current buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance data.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   ppRing0Buf      Where to store the address of the ring-3 mapping.
 */
INTNETR0DECL(int) INTNETR0IfGetRing0Buffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PINTNETBUF *ppRing0Buf)
{
    LogFlow(("INTNETR0IfGetRing0Buffer: pIntNet=%p hIf=%RX32 ppRing0Buf=%p\n", pIntNet, hIf, ppRing0Buf));

    /*
     * Validate input.
     */
    AssertPtrReturn(ppRing0Buf, VERR_INVALID_PARAMETER);
    *ppRing0Buf = NULL;
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    /*
     * Grab the lock and get the data.
     * ASSUMES that the handle isn't closed while we're here.
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex2);
    if (RT_SUCCESS(rc))
    {
        *ppRing0Buf = pIf->pIntBuf;

        rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex2);
    }
    intnetR0IfRelease(pIf, pSession);
    LogFlow(("INTNETR0IfGetRing0Buffer: returns %Rrc *ppRing0Buf=%p\n", rc, *ppRing0Buf));
    return rc;
}


#if 0
/**
 * Gets the physical addresses of the default interface buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance data.
 * @param   hIF         The interface handle.
 * @param   paPages     Where to store the addresses. (The reserved fields will be set to zero.)
 * @param   cPages
 */
INTNETR0DECL(int) INTNETR0IfGetPhysBuffer(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPPAGE paPages, unsigned cPages)
{
    /*
     * Validate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    AssertPtrReturn(paPages, VERR_INVALID_PARAMETER);
    AssertPtrReturn((uint8_t *)&paPages[cPages] - 1, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    /*
     * Grab the lock and get the data.
     * ASSUMES that the handle isn't closed while we're here.
     */
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (RT_SUCCESS(rc))
    {
        /** @todo make a SUPR0 api for obtaining the array. SUPR0/IPRT is keeping track of everything, there
         * is no need for any extra bookkeeping here.. */

        rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
    }
    intnetR0IfRelease(pIf, pSession);
    return VERR_NOT_IMPLEMENTED;
}
#endif


/**
 * Sets the promiscuous mode property of an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   fPromiscuous    Set if the interface should be in promiscuous mode, clear if not.
 */
INTNETR0DECL(int) INTNETR0IfSetPromiscuousMode(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, bool fPromiscuous)
{
    LogFlow(("INTNETR0IfSetPromiscuousMode: pIntNet=%p hIf=%RX32 fPromiscuous=%d\n", pIntNet, hIf, fPromiscuous));

    /*
     * Validate & translate input.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
    {
        Log(("INTNETR0IfSetPromiscuousMode: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Grab the network semaphore and make the change.
     */
    int rc;
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    if (pNetwork)
    {
        rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
        if (RT_SUCCESS(rc))
        {
            if (pIf->fPromiscuous != fPromiscuous)
            {
                Log(("INTNETR0IfSetPromiscuousMode: hIf=%RX32: Changed from %d -> %d\n",
                     hIf, !fPromiscuous, !!fPromiscuous));
                ASMAtomicUoWriteBool(&pIf->fPromiscuous, fPromiscuous);
            }

            rc = RTSemFastMutexRelease(pNetwork->FastMutex2);
        }
    }
    else
        rc = VERR_WRONG_ORDER;

    intnetR0IfRelease(pIf, pSession);
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfSetPromiscuousMode.
 *
 * @returns see INTNETR0IfSetPromiscuousMode.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSetPromiscuousModeReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFSETPROMISCUOUSMODEREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSetPromiscuousMode(pIntNet, pReq->hIf, pSession, pReq->fPromiscuous);
}


/**
 * Sets the MAC address of an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   pMAC            The new MAC address.
 */
INTNETR0DECL(int) INTNETR0IfSetMacAddress(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PCRTMAC pMac)
{
    LogFlow(("INTNETR0IfSetMacAddress: pIntNet=%p hIf=%RX32 pMac=%p:{%.6Rhxs}\n", pIntNet, hIf, pMac, pMac));

    /*
     * Validate & translate input.
     */
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pMac, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
    {
        Log(("INTNETR0IfSetMacAddress: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Grab the network semaphore and make the change.
     */
    int rc;
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    if (pNetwork)
    {
        rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
        if (RT_SUCCESS(rc))
        {
            if (memcmp(&pIf->Mac, pMac, sizeof(pIf->Mac)))
            {
                Log(("INTNETR0IfSetMacAddress: hIf=%RX32: Changed from %.6Rhxs -> %.6Rhxs\n",
                     hIf, &pIf->Mac, pMac));
                pIf->Mac = *pMac;
                pIf->fMacSet = true;
            }

            rc = RTSemFastMutexRelease(pNetwork->FastMutex2);
        }
    }
    else
        rc = VERR_WRONG_ORDER;

    intnetR0IfRelease(pIf, pSession);
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfSetMacAddress.
 *
 * @returns see INTNETR0IfSetMacAddress.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSetMacAddressReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFSETMACADDRESSREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSetMacAddress(pIntNet, pReq->hIf, pSession, &pReq->Mac);
}


/**
 * Worker for intnetR0IfSetActive.
 *
 * This function will update the active interface count on the network and
 * activate or deactivate the trunk connection if necessary. Note that in
 * order to do this it is necessary to abandond the network semaphore.
 *
 * @returns VBox status code.
 * @param   pNetwork        The network.
 * @param   fIf             The interface.
 * @param   fActive         What to do.
 */
static int intnetR0NetworkSetIfActive(PINTNETNETWORK pNetwork, PINTNETIF pIf, bool fActive)
{
    /* quick santiy check */
    AssertPtr(pNetwork);
    AssertPtr(pIf);

    /*
     * If we've got a trunk, lock it now in case we need to call out, and
     * then lock the network.
     */
    PINTNETTRUNKIF pTrunkIf = pNetwork->pTrunkIF;
    if (pTrunkIf && !intnetR0TrunkIfOutLock(pTrunkIf))
        return VERR_SEM_DESTROYED;

    int rc = RTSemFastMutexRequest(pNetwork->FastMutex2); AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        bool fNetworkLocked = true;

        /*
         * Make the change if necessary.
         */
        if (pIf->fActive != fActive)
        {
            pIf->fActive = fActive;

            uint32_t const cActiveIFs = pNetwork->cActiveIFs;
            Assert((int32_t)cActiveIFs + (fActive ? 1 : -1) >= 0);
            pNetwork->cActiveIFs += fActive ? 1 : -1;

            if (    pTrunkIf
                &&  (   !pNetwork->cActiveIFs
                     || !cActiveIFs))
            {
                /*
                 * We'll have to change the trunk status, so, leave
                 * the network semaphore so we don't create any deadlocks.
                 */
                int rc2 = RTSemFastMutexRelease(pNetwork->FastMutex2); AssertRC(rc2);
                fNetworkLocked = false;

                if (pTrunkIf->pIfPort)
                    pTrunkIf->pIfPort->pfnSetActive(pTrunkIf->pIfPort, fActive);
            }
        }

        if (fNetworkLocked)
            RTSemFastMutexRelease(pNetwork->FastMutex2);
    }
    if (pTrunkIf)
        intnetR0TrunkIfOutUnlock(pTrunkIf);
    return rc;
}


/**
 * Activates or deactivates a interface.
 *
 * This is used to enable and disable the trunk connection on demans as well as
 * know when not to expect an interface to want to receive packets.
 *
 * @returns VBox status code.
 * @param   pIf         The interface.
 * @param   fActive     What to do.
 */
static int intnetR0IfSetActive(PINTNETIF pIf, bool fActive)
{
    /* quick sanity check */
    AssertPtrReturn(pIf, VERR_INVALID_POINTER);

    /*
     * Hand it to the network since it might involve the trunk
     * and things are tricky there wrt to locking order.
     */
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    if (!pNetwork)
        return VERR_WRONG_ORDER;
    return intnetR0NetworkSetIfActive(pNetwork, pIf, fActive);
}


/**
 * Sets the active property of an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   fActive         The new state.
 */
INTNETR0DECL(int) INTNETR0IfSetActive(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, bool fActive)
{
    LogFlow(("INTNETR0IfSetActive: pIntNet=%p hIf=%RX32 fActive=%RTbool\n", pIntNet, hIf, fActive));

    /*
     * Validate & translate input.
     */
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
    {
        Log(("INTNETR0IfSetActive: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Hand it to the network since it might involve the trunk
     * and things are tricky there wrt to locking order.
     */
    int rc;
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    if (pNetwork)
        rc = intnetR0NetworkSetIfActive(pNetwork, pIf, fActive);
    else
        rc = VERR_WRONG_ORDER;

    intnetR0IfRelease(pIf, pSession);
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfSetActive.
 *
 * @returns see INTNETR0IfSetActive.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfSetActiveReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFSETACTIVEREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfSetActive(pIntNet, pReq->hIf, pSession, pReq->fActive);
}


/**
 * Wait for the interface to get signaled.
 * The interface will be signaled when is put into the receive buffer.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance handle.
 * @param   hIf             The interface handle.
 * @param   pSession        The caller's session.
 * @param   cMillies        Number of milliseconds to wait. RT_INDEFINITE_WAIT should be
 *                          used if indefinite wait is desired.
 */
INTNETR0DECL(int) INTNETR0IfWait(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, uint32_t cMillies)
{
    Log4(("INTNETR0IfWait: pIntNet=%p hIf=%RX32 cMillies=%u\n", pIntNet, hIf, cMillies));

    /*
     * Get and validate essential handles.
     */
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableLookupWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
    {
        Log(("INTNETR0IfWait: returns VERR_INVALID_HANDLE\n"));
        return VERR_INVALID_HANDLE;
    }
    const INTNETIFHANDLE    hIfSelf = pIf->hIf;
    const RTSEMEVENT        Event = pIf->Event;
    if (    hIfSelf != hIf              /* paranoia */
        &&  Event != NIL_RTSEMEVENT)
    {
        Log(("INTNETR0IfWait: returns VERR_SEM_DESTROYED\n"));
        return VERR_SEM_DESTROYED;
    }

    /*
     * It is tempting to check if there is data to be read here,
     * but the problem with such an approach is that it will cause
     * one unnecessary supervisor->user->supervisor trip. There is
     * already a slight risk for such, so no need to increase it.
     */

    /*
     * Increment the number of waiters before starting the wait.
     * Upon wakeup we must assert reality, checking that we're not
     * already destroyed or in the process of being destroyed. This
     * code must be aligned with the waiting code in intnetR0IfDestruct.
     */
    ASMAtomicIncU32(&pIf->cSleepers);
    int rc = RTSemEventWaitNoResume(Event, cMillies);
    if (pIf->Event == Event)
    {
        ASMAtomicDecU32(&pIf->cSleepers);
        if (!pIf->fDestroying)
        {
            if (intnetR0IfRelease(pIf, pSession))
                rc = VERR_SEM_DESTROYED;
        }
        else
            rc = VERR_SEM_DESTROYED;
    }
    else
        rc = VERR_SEM_DESTROYED;
    Log4(("INTNETR0IfWait: returns %Rrc\n", rc));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfWait.
 *
 * @returns see INTNETR0IfWait.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfWaitReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFWAITREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfWait(pIntNet, pReq->hIf, pSession, pReq->cMillies);
}


/**
 * Close an interface.
 *
 * @returns VBox status code.
 * @param   pIntNet     The instance handle.
 * @param   hIf         The interface handle.
 * @param   pSession        The caller's session.
 */
INTNETR0DECL(int) INTNETR0IfClose(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession)
{
    LogFlow(("INTNETR0IfClose: pIntNet=%p hIf=%RX32\n", pIntNet, hIf));

    /*
     * Validate and free the handle.
     */
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);
    PINTNETIF pIf = (PINTNETIF)RTHandleTableFreeWithCtx(pIntNet->hHtIfs, hIf, pSession);
    if (!pIf)
        return VERR_INVALID_HANDLE;

    /* mark the handle as freed so intnetR0IfDestruct won't free it again. */
    ASMAtomicWriteU32(&pIf->hIf, INTNET_HANDLE_INVALID);


    /*
     * Release the references to the interface object (handle + free lookup).
     * But signal the event semaphore first so any waiter holding a reference
     * will wake up too (he'll see hIf == invalid and return correctly).
     */
    RTSemEventSignal(pIf->Event);

    void *pvObj = pIf->pvObj;
    intnetR0IfRelease(pIf, pSession); /* (RTHandleTableFreeWithCtx) */

    int rc = SUPR0ObjRelease(pvObj, pSession);
    LogFlow(("INTNETR0IfClose: returns %Rrc\n", rc));
    return rc;
}


/**
 * VMMR0 request wrapper for INTNETR0IfCloseReq.
 *
 * @returns see INTNETR0IfClose.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0IfCloseReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETIFCLOSEREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0IfClose(pIntNet, pReq->hIf, pSession);
}


/**
 * Interface destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     Pointer to the interface.
 * @param   pvUser2     Pointer to the INTNET instance data.
 */
static DECLCALLBACK(void) intnetR0IfDestruct(void *pvObj, void *pvUser1, void *pvUser2)
{
    PINTNETIF pIf     = (PINTNETIF)pvUser1;
    PINTNET   pIntNet = (PINTNET)pvUser2;
    Log(("intnetR0IfDestruct: pvObj=%p pIf=%p pIntNet=%p hIf=%RX32\n", pvObj, pIf, pIntNet, pIf->hIf));

    /*
     * We grab the INTNET create/open/destroy semaphore to make sure nobody is
     * adding or removing interface while we're in here.  For paranoid reasons
     * we also mark the interface as destroyed here so any waiting threads can
     * take the appropriate actions (theoretical case).
     */
    RTSemMutexRequest(pIntNet->hMtxCreateOpenDestroy, RT_INDEFINITE_WAIT);
    ASMAtomicWriteBool(&pIf->fDestroying, true);

    /*
     * Delete the interface handle so the object no longer can be used.
     * (Can happen if the client didn't close its session.)
     */
    INTNETIFHANDLE hIf = ASMAtomicXchgU32(&pIf->hIf, INTNET_HANDLE_INVALID);
    if (hIf != INTNET_HANDLE_INVALID)
    {
        void *pvObj2 = RTHandleTableFreeWithCtx(pIntNet->hHtIfs, hIf, pIf->pSession); NOREF(pvObj2);
        AssertMsg(pvObj2 == pIf, ("%p, %p, hIf=%RX32 pSession=%p\n", pvObj2, pIf, hIf, pIf->pSession));
    }

    /*
     * If we've got a network deactivate and unlink ourselves from it.
     * Because of cleanup order we might be an orphan now.
     */
    PINTNETNETWORK pNetwork = pIf->pNetwork;
    if (pNetwork)
    {
        intnetR0IfSetActive(pIf, false);

        /* unlink */
        if (pNetwork->pIfs == pIf)
            pNetwork->pIfs = pIf->pNext;
        else
        {
            PINTNETIF pPrev = pNetwork->pIfs;
            while (pPrev)
            {
                if (pPrev->pNext == pIf)
                {
                    pPrev->pNext = pIf->pNext;
                    break;
                }
                pPrev = pPrev->pNext;
            }
            Assert(pPrev);
        }
        pIf->pNext = NULL;

        /* Release our reference to the network. */
        SUPR0ObjRelease(pNetwork->pvObj, pIf->pSession);
        pIf->pNetwork = NULL;
    }

    RTSemMutexRelease(pIntNet->hMtxCreateOpenDestroy);

    /*
     * Wakeup anyone waiting on this interface.
     *
     * We *must* make sure they have woken up properly and realized
     * that the interface is no longer valid.
     */
    if (pIf->Event != NIL_RTSEMEVENT)
    {
        RTSEMEVENT Event = pIf->Event;
        unsigned cMaxWait = 0x1000;
        while (pIf->cSleepers && cMaxWait-- > 0)
        {
            RTSemEventSignal(Event);
            RTThreadYield();
        }
        if (pIf->cSleepers)
        {
            RTThreadSleep(1);

            cMaxWait = pIf->cSleepers;
            while (pIf->cSleepers && cMaxWait-- > 0)
            {
                RTSemEventSignal(Event);
                RTThreadSleep(10);
            }
        }

        RTSemEventDestroy(Event);
        pIf->Event = NIL_RTSEMEVENT;
    }

    /*
     * Unmap user buffer.
     */
    if (pIf->pIntBuf != pIf->pIntBufDefault)
    {
        /** @todo user buffer */
    }

    /*
     * Unmap and Free the default buffer.
     */
    if (pIf->pIntBufDefault)
    {
        SUPR0MemFree(pIf->pSession, (RTHCUINTPTR)pIf->pIntBufDefault);
        pIf->pIntBufDefault     = NULL;
        pIf->pIntBufDefaultR3   = 0;
        pIf->pIntBuf            = NULL;
        pIf->pIntBufR3          = 0;
    }

    /*
     * The interface.
     */
    pIf->pvObj = NULL;
    RTMemFree(pIf);
}


/**
 * Creates a new network interface.
 *
 * The call must have opened the network for the new interface and is
 * responsible for closing it on failure.  On success it must leave the network
 * opened so the interface destructor can close it.
 *
 * @returns VBox status code.
 * @param   pNetwork    The network.
 * @param   pSession    The session handle.
 * @param   cbSend      The size of the send buffer.
 * @param   cbRecv      The size of the receive buffer.
 * @param   phIf        Where to store the interface handle.
 */
static int intnetR0NetworkCreateIf(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession, unsigned cbSend, unsigned cbRecv,
                                   PINTNETIFHANDLE phIf)
{
    LogFlow(("intnetR0NetworkCreateIf: pNetwork=%p pSession=%p cbSend=%u cbRecv=%u phIf=%p\n",
             pNetwork, pSession, cbSend, cbRecv, phIf));

    /*
     * Assert input.
     */
    AssertPtr(pNetwork);
    AssertPtr(phIf);

    /*
     * Allocate and initialize the interface structure.
     */
    PINTNETIF pIf = (PINTNETIF)RTMemAllocZ(sizeof(*pIf));
    if (!pIf)
        return VERR_NO_MEMORY;
    //pIf->pNext = NULL;
    memset(&pIf->Mac, 0xff, sizeof(pIf->Mac)); /* broadcast */
    //pIf->fMacSet = false;
    //pIf->fPromiscuous = false;
    //pIf->fActive = false;
    //pIf->fDestroying = false;
    //pIf->pIntBuf = 0;
    //pIf->pIntBufR3 = NIL_RTR3PTR;
    //pIf->pIntBufDefault = 0;
    //pIf->pIntBufDefaultR3 = NIL_RTR3PTR;
    //pIf->cYields = 0;
    pIf->Event = NIL_RTSEMEVENT;
    //pIf->cSleepers = 0;
    pIf->hIf = INTNET_HANDLE_INVALID;
    pIf->pNetwork = pNetwork;
    pIf->pSession = pSession;
    //pIf->pvObj = NULL;
    //pIf->aAddrCache[kIntNetAddrType_Invalid] = {0};
    //pIf->aAddrCache[kIntNetAddrType_IPv4].pbEntries = NULL;
    //pIf->aAddrCache[kIntNetAddrType_IPv4].cEntries = 0;
    //pIf->aAddrCache[kIntNetAddrType_IPv4].cEntriesAlloc = 0;
    pIf->aAddrCache[kIntNetAddrType_IPv4].cbAddress = intnetR0AddrSize(kIntNetAddrType_IPv4);
    pIf->aAddrCache[kIntNetAddrType_IPv4].cbEntry   = intnetR0AddrSize(kIntNetAddrType_IPv4);
    //pIf->aAddrCache[kIntNetAddrType_IPv6].pbEntries = NULL;
    //pIf->aAddrCache[kIntNetAddrType_IPv6].cEntries = 0;
    //pIf->aAddrCache[kIntNetAddrType_IPv6].cEntriesAlloc = 0;
    pIf->aAddrCache[kIntNetAddrType_IPv6].cbAddress = intnetR0AddrSize(kIntNetAddrType_IPv6);
    pIf->aAddrCache[kIntNetAddrType_IPv6].cbEntry   = intnetR0AddrSize(kIntNetAddrType_IPv6);
    //pIf->aAddrCache[kIntNetAddrType_IPX].pbEntries = NULL;
    //pIf->aAddrCache[kIntNetAddrType_IPX].cEntries = 0;
    //pIf->aAddrCache[kIntNetAddrType_IPX].cEntriesAlloc = 0;
    pIf->aAddrCache[kIntNetAddrType_IPX].cbAddress = intnetR0AddrSize(kIntNetAddrType_IPX);
    pIf->aAddrCache[kIntNetAddrType_IPX].cbEntry   = RT_ALIGN_32(intnetR0AddrSize(kIntNetAddrType_IPv4), 16);
    int rc = RTSemEventCreate((PRTSEMEVENT)&pIf->Event);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the default buffer.
         */
        /** @todo adjust with minimums and apply defaults here. */
        cbRecv = RT_ALIGN(RT_MAX(cbRecv, sizeof(INTNETHDR) * 4), INTNETRINGBUF_ALIGNMENT);
        cbSend = RT_ALIGN(RT_MAX(cbSend, sizeof(INTNETHDR) * 4), INTNETRINGBUF_ALIGNMENT);
        const unsigned cbBuf = RT_ALIGN(sizeof(*pIf->pIntBuf), INTNETRINGBUF_ALIGNMENT) + cbRecv + cbSend;
        rc = SUPR0MemAlloc(pIf->pSession, cbBuf, (PRTR0PTR)&pIf->pIntBufDefault, (PRTR3PTR)&pIf->pIntBufDefaultR3);
        if (RT_SUCCESS(rc))
        {
            ASMMemZero32(pIf->pIntBufDefault, cbBuf); /** @todo I thought I specified these buggers as clearing the memory... */

            pIf->pIntBuf   = pIf->pIntBufDefault;
            pIf->pIntBufR3 = pIf->pIntBufDefaultR3;
            INTNETBufInit(pIf->pIntBuf, cbBuf, cbRecv, cbSend);

            /*
             * Link the interface to the network.
             */
            rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
            if (RT_SUCCESS(rc))
            {
                pIf->pNext = pNetwork->pIfs;
                pNetwork->pIfs = pIf;
                RTSemFastMutexRelease(pNetwork->FastMutex2);

                /*
                 * Register the interface with the session, adding another
                 * reference to the network to ease the cleanup duties of the caller.
                 */
                rc = SUPR0ObjAddRef(pNetwork->pvObj, pSession);
                if (RT_SUCCESS(rc))
                {
                    pIf->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_INTERNAL_NETWORK_INTERFACE, intnetR0IfDestruct, pIf, pNetwork->pIntNet);
                    if (pIf->pvObj)
                    {
                        rc = RTHandleTableAllocWithCtx(pNetwork->pIntNet->hHtIfs, pIf, pSession, (uint32_t *)&pIf->hIf);
                        if (RT_SUCCESS(rc))
                        {
                            *phIf = pIf->hIf;
                            Log(("intnetR0NetworkCreateIf: returns VINF_SUCCESS *phIf=%RX32 cbSend=%u cbRecv=%u cbBuf=%u\n",
                                 *phIf, pIf->pIntBufDefault->cbSend, pIf->pIntBufDefault->cbRecv, pIf->pIntBufDefault->cbBuf));
                            return VINF_SUCCESS;
                        }

                        SUPR0ObjRelease(pIf->pvObj, pSession);
                        LogFlow(("intnetR0NetworkCreateIf: returns %Rrc\n", rc));
                        return rc;
                    }

                    SUPR0ObjRelease(pNetwork->pvObj, pSession);
                }

                RTSemFastMutexDestroy(pNetwork->FastMutex2);
                pNetwork->FastMutex2 = NIL_RTSEMFASTMUTEX;
            }

            SUPR0MemFree(pIf->pSession, (RTHCUINTPTR)pIf->pIntBufDefault);
            pIf->pIntBufDefault = NULL;
            pIf->pIntBuf = NULL;
        }

        RTSemEventDestroy(pIf->Event);
        pIf->Event = NIL_RTSEMEVENT;
    }
    RTMemFree(pIf);
    LogFlow(("intnetR0NetworkCreateIf: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc INTNETTRUNKSWPORT::pfnSetSGPhys */
static DECLCALLBACK(bool) intnetR0TrunkIfPortSetSGPhys(PINTNETTRUNKSWPORT pSwitchPort, bool fEnable)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    AssertMsgFailed(("Not implemented because it wasn't required on Darwin\n"));
    return ASMAtomicXchgBool(&pThis->fPhysSG, fEnable);
}


/** @copydoc INTNETTRUNKSWPORT::pfnReportGsoCapabilities */
static DECLCALLBACK(void) intnetR0TrunkIfPortReportGsoCapabilities(PINTNETTRUNKSWPORT pSwitchPort,
                                                                  uint32_t fGsoCapabilities, uint32_t fDst)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);

    for (unsigned iBit = PDMNETWORKGSOTYPE_END; iBit < 32; iBit++)
        Assert(!(fGsoCapabilities & RT_BIT_32(iBit)));
    Assert(!(fDst & ~INTNETTRUNKDIR_VALID_MASK));
    Assert(fDst);

    if (fDst & INTNETTRUNKDIR_HOST)
        pThis->fHostGsoCapabilites = fGsoCapabilities;

    if (fDst & INTNETTRUNKDIR_WIRE)
        pThis->fWireGsoCapabilites = fGsoCapabilities;
}


/** @copydoc INTNETTRUNKSWPORT::pfnPreRecv */
static DECLCALLBACK(INTNETSWDECISION) intnetR0TrunkIfPortPreRecv(PINTNETTRUNKSWPORT pSwitchPort,
                                                                 void const *pvSrc, size_t cbSrc, uint32_t fSrc)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    PINTNETNETWORK pNetwork = pThis->pNetwork;

    /* assert some sanity */
    AssertPtrReturn(pNetwork, INTNETSWDECISION_TRUNK);
    AssertReturn(pNetwork->FastMutex2 != NIL_RTSEMFASTMUTEX, INTNETSWDECISION_TRUNK);
    AssertPtr(pvSrc);
    AssertPtr(cbSrc >= 6);
    Assert(fSrc);

    /** @todo implement the switch table. */

    return INTNETSWDECISION_BROADCAST;
}


/** @copydoc INTNETTRUNKSWPORT::pfnRecv */
static DECLCALLBACK(bool) intnetR0TrunkIfPortRecv(PINTNETTRUNKSWPORT pSwitchPort, PINTNETSG pSG, uint32_t fSrc)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    PINTNETNETWORK pNetwork = pThis->pNetwork;

    /* assert some sanity */
    AssertPtrReturn(pNetwork, false);
    AssertReturn(pNetwork->FastMutex2 != NIL_RTSEMFASTMUTEX, false);
    AssertPtr(pSG);
    Assert(fSrc);

    /*
     * Lock the network and send the frame to it.
     */
    int rc = RTSemFastMutexRequest(pNetwork->FastMutex2);
    AssertRCReturn(rc, false);

    bool fRc;
    if (RT_LIKELY(pNetwork->cActiveIFs > 0))
        fRc = intnetR0NetworkSend(pNetwork, NULL, fSrc, pSG, false /* fTrunkLocked */);
    else
        fRc = false; /* don't drop it */

    rc = RTSemFastMutexRelease(pNetwork->FastMutex2);
    AssertRC(rc);

    return fRc;
}


/** @copydoc INTNETTRUNKSWPORT::pfnSGRetain */
static DECLCALLBACK(void) intnetR0TrunkIfPortSGRetain(PINTNETTRUNKSWPORT pSwitchPort, PINTNETSG pSG)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    PINTNETNETWORK pNetwork = pThis->pNetwork;

    /* assert some sanity */
    AssertPtrReturnVoid(pNetwork);
    AssertReturnVoid(pNetwork->FastMutex2 != NIL_RTSEMFASTMUTEX);
    AssertPtr(pSG);
    Assert(pSG->cUsers > 0 && pSG->cUsers < 256);

    /* do it. */
    ++pSG->cUsers;
}


/** @copydoc INTNETTRUNKSWPORT::pfnSGRelease */
static DECLCALLBACK(void) intnetR0TrunkIfPortSGRelease(PINTNETTRUNKSWPORT pSwitchPort, PINTNETSG pSG)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    PINTNETNETWORK pNetwork = pThis->pNetwork;

    /* assert some sanity */
    AssertPtrReturnVoid(pNetwork);
    AssertReturnVoid(pNetwork->FastMutex2 != NIL_RTSEMFASTMUTEX);
    AssertPtr(pSG);
    Assert(pSG->cUsers > 0);

    /*
     * Free it?
     */
    if (!--pSG->cUsers)
    {
        /** @todo later */
    }
}


/**
 * Retain the trunk interface.
 *
 * @returns pThis if retained.
 *
 * @param   pThis       The trunk.
 *
 * @remarks Any locks.
 */
static PINTNETTRUNKIF intnetR0TrunkIfRetain(PINTNETTRUNKIF pThis)
{
    if (pThis && pThis->pIfPort)
    {
        pThis->pIfPort->pfnRetain(pThis->pIfPort);
        return pThis;
    }
    return NULL;
}


/**
 * Release the trunk interface.
 *
 * @param   pThis       The trunk.
 */
static void intnetR0TrunkIfRelease(PINTNETTRUNKIF pThis)
{
    if (pThis && pThis->pIfPort)
        pThis->pIfPort->pfnRelease(pThis->pIfPort);
}


/**
 * Takes the out-bound trunk lock.
 *
 * This will ensure that pIfPort is valid.
 *
 * @returns success indicator.
 * @param   pThis       The trunk.
 *
 * @remarks No locks other than the create/destroy one.
 */
static bool intnetR0TrunkIfOutLock(PINTNETTRUNKIF pThis)
{
    AssertPtrReturn(pThis, false);
    int rc = RTSemFastMutexRequest(pThis->FastMutex);
    if (RT_SUCCESS(rc))
    {
        if (RT_LIKELY(pThis->pIfPort))
            return true;
        RTSemFastMutexRelease(pThis->FastMutex);
    }
    else
        AssertMsg(rc == VERR_SEM_DESTROYED, ("%Rrc\n", rc));
    return false;
}


/**
 * Releases the out-bound trunk lock.
 *
 * @param   pThis       The trunk.
 */
static void intnetR0TrunkIfOutUnlock(PINTNETTRUNKIF pThis)
{
    if (pThis)
    {
        int rc = RTSemFastMutexRelease(pThis->FastMutex);
        AssertRC(rc);
    }
}


/**
 * Activates the trunk interface.
 *
 * @param   pThis       The trunk.
 * @param   fActive     What to do with it.
 *
 * @remarks Caller may only own the create/destroy lock.
 */
static void intnetR0TrunkIfActivate(PINTNETTRUNKIF pThis, bool fActive)
{
    if (intnetR0TrunkIfOutLock(pThis))
    {
        pThis->pIfPort->pfnSetActive(pThis->pIfPort, fActive);
        intnetR0TrunkIfOutUnlock(pThis);
    }
}


/**
 * Shutdown the trunk interface.
 *
 * @param   pThis       The trunk.
 * @param   pNetworks   The network.
 *
 * @remarks The caller must *NOT* hold the network lock. The global
 *          create/destroy lock is fine though.
 */
static void intnetR0TrunkIfDestroy(PINTNETTRUNKIF pThis, PINTNETNETWORK pNetwork)
{
    /* assert sanity */
    if (!pThis)
        return;
    AssertPtr(pThis);
    Assert(pThis->pNetwork == pNetwork);
    AssertPtrNull(pThis->pIfPort);

    /*
     * The interface has already been deactivated, we just to wait for
     * it to become idle before we can disconnect and release it.
     */
    PINTNETTRUNKIFPORT pIfPort = pThis->pIfPort;
    if (pIfPort)
    {
        intnetR0TrunkIfOutLock(pThis);

        /* unset it */
        pThis->pIfPort = NULL;

        /* wait in portions so we can complain ever now an then. */
        uint64_t StartTS = RTTimeSystemNanoTS();
        int rc = pIfPort->pfnWaitForIdle(pIfPort, 10*1000);
        if (RT_FAILURE(rc))
        {
            LogRel(("intnet: '%s' did't become idle in %RU64 ns (%Rrc).\n",
                    pNetwork->szName, RTTimeSystemNanoTS() - StartTS, rc));
            Assert(rc == VERR_TIMEOUT);
            while (     RT_FAILURE(rc)
                   &&   RTTimeSystemNanoTS() - StartTS < UINT64_C(30000000000)) /* 30 sec */
                rc = pIfPort->pfnWaitForIdle(pIfPort, 10*1000);
            if (rc == VERR_TIMEOUT)
            {
                LogRel(("intnet: '%s' did't become idle in %RU64 ns (%Rrc).\n",
                        pNetwork->szName, RTTimeSystemNanoTS() - StartTS, rc));
                while (     rc == VERR_TIMEOUT
                       &&   RTTimeSystemNanoTS() - StartTS < UINT64_C(360000000000)) /* 360 sec */
                    rc = pIfPort->pfnWaitForIdle(pIfPort, 30*1000);
                if (RT_FAILURE(rc))
                {
                    LogRel(("intnet: '%s' did't become idle in %RU64 ns (%Rrc), giving up.\n",
                            pNetwork->szName, RTTimeSystemNanoTS() - StartTS, rc));
                    AssertRC(rc);
                }
            }
        }

        /* disconnect & release it. */
        pIfPort->pfnDisconnectAndRelease(pIfPort);
    }

    /*
     * Free up the resources.
     */
    RTSEMFASTMUTEX hFastMutex = pThis->FastMutex;
    pThis->FastMutex = NIL_RTSEMMUTEX;
    pThis->pNetwork = NULL;
    RTSemFastMutexRelease(hFastMutex);
    RTSemFastMutexDestroy(hFastMutex);
    RTMemFree(pThis);
}


/**
 * Creates the trunk connection (if any).
 *
 * @returns VBox status code.
 *
 * @param   pNetwork    The newly created network.
 * @param   pSession    The session handle.
 */
static int intnetR0NetworkCreateTrunkIf(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession)
{
    const char *pszName;
    switch (pNetwork->enmTrunkType)
    {
        /*
         * The 'None' case, simple.
         */
        case kIntNetTrunkType_None:
        case kIntNetTrunkType_WhateverNone:
            return VINF_SUCCESS;

        /* Can't happen, but makes GCC happy. */
        default:
            return VERR_NOT_IMPLEMENTED;

        /*
         * Translate enum to component factory name.
         */
        case kIntNetTrunkType_NetFlt:
            pszName = "VBoxNetFlt";
            break;
        case kIntNetTrunkType_NetAdp:
#if defined(RT_OS_DARWIN) && !defined(VBOXNETADP_DO_NOT_USE_NETFLT)
            pszName = "VBoxNetFlt";
#else /* VBOXNETADP_DO_NOT_USE_NETFLT */
            pszName = "VBoxNetAdp";
#endif /* VBOXNETADP_DO_NOT_USE_NETFLT */
            break;
        case kIntNetTrunkType_SrvNat:
            pszName = "VBoxSrvNat";
            break;
    }

    /*
     * Allocate the trunk interface.
     */
    PINTNETTRUNKIF pTrunkIF = (PINTNETTRUNKIF)RTMemAllocZ(sizeof(*pTrunkIF));
    if (!pTrunkIF)
        return VERR_NO_MEMORY;
    pTrunkIF->SwitchPort.u32Version                 = INTNETTRUNKSWPORT_VERSION;
    pTrunkIF->SwitchPort.pfnPreRecv                 = intnetR0TrunkIfPortPreRecv;
    pTrunkIF->SwitchPort.pfnRecv                    = intnetR0TrunkIfPortRecv;
    pTrunkIF->SwitchPort.pfnSGRetain                = intnetR0TrunkIfPortSGRetain;
    pTrunkIF->SwitchPort.pfnSGRelease               = intnetR0TrunkIfPortSGRelease;
    pTrunkIF->SwitchPort.pfnSetSGPhys               = intnetR0TrunkIfPortSetSGPhys;
    pTrunkIF->SwitchPort.pfnReportGsoCapabilities   = intnetR0TrunkIfPortReportGsoCapabilities;
    pTrunkIF->SwitchPort.u32VersionEnd              = INTNETTRUNKSWPORT_VERSION;
    //pTrunkIF->pIfPort = NULL;
    pTrunkIF->pNetwork = pNetwork;
    pTrunkIF->CachedMac.au8[0] = 0xfe;
    pTrunkIF->CachedMac.au8[1] = 0xff;
    pTrunkIF->CachedMac.au8[2] = 0xff;
    pTrunkIF->CachedMac.au8[3] = 0xff;
    pTrunkIF->CachedMac.au8[4] = 0xff;
    pTrunkIF->CachedMac.au8[5] = 0xff;
    //pTrunkIF->fPhysSG = false;
    //pTrunkIF->fWirePromiscuous = false;
    //pTrunkIF->fGroksGso = false;  /** @todo query GSO support after connecting. */
    int rc = RTSemFastMutexCreate(&pTrunkIF->FastMutex);
    if (RT_SUCCESS(rc))
    {
#ifdef IN_RING0 /* (testcase is ring-3) */
        /*
         * Query the factory we want, then use it create and connect the trunk.
         */
        PINTNETTRUNKFACTORY pTrunkFactory = NULL;
        rc = SUPR0ComponentQueryFactory(pSession, pszName, INTNETTRUNKFACTORY_UUID_STR, (void **)&pTrunkFactory);
        if (RT_SUCCESS(rc))
        {
            rc = pTrunkFactory->pfnCreateAndConnect(pTrunkFactory, pNetwork->szTrunk, &pTrunkIF->SwitchPort,
                                                    pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE
                                                    ? INTNETTRUNKFACTORY_FLAG_NO_PROMISC : 0,
                                                    &pTrunkIF->pIfPort);
            pTrunkFactory->pfnRelease(pTrunkFactory);
            if (RT_SUCCESS(rc))
            {
                Assert(pTrunkIF->pIfPort);
                pNetwork->pTrunkIF = pTrunkIF;
                Log(("intnetR0NetworkCreateTrunkIf: VINF_SUCCESS - pszName=%s szTrunk=%s%s Network=%s\n",
                     pszName, pNetwork->szTrunk, pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE ? " shared-mac" : "", pNetwork->szName));
                return VINF_SUCCESS;
            }
        }
#endif /* IN_RING0 */
        RTSemFastMutexDestroy(pTrunkIF->FastMutex);
    }
    RTMemFree(pTrunkIF);
    LogFlow(("intnetR0NetworkCreateTrunkIf: %Rrc - pszName=%s szTrunk=%s Network=%s\n",
             rc, pszName, pNetwork->szTrunk, pNetwork->szName));
    return rc;
}



/**
 * Object destructor callback.
 * This is called for reference counted objectes when the count reaches 0.
 *
 * @param   pvObj       The object pointer.
 * @param   pvUser1     Pointer to the network.
 * @param   pvUser2     Pointer to the INTNET instance data.
 */
static DECLCALLBACK(void) intnetR0NetworkDestruct(void *pvObj, void *pvUser1, void *pvUser2)
{
    PINTNETNETWORK  pNetwork = (PINTNETNETWORK)pvUser1;
    PINTNET         pIntNet = (PINTNET)pvUser2;
    Log(("intnetR0NetworkDestruct: pvObj=%p pNetwork=%p pIntNet=%p %s\n", pvObj, pNetwork, pIntNet, pNetwork->szName));
    Assert(pNetwork->pIntNet == pIntNet);

    /* take the create/destroy sem. */
    RTSemMutexRequest(pIntNet->hMtxCreateOpenDestroy, RT_INDEFINITE_WAIT);

    /*
     * Deactivate the trunk connection first (if any).
     */
    if (pNetwork->pTrunkIF)
        intnetR0TrunkIfActivate(pNetwork->pTrunkIF, false /* fActive */);

    /*
     * Unlink the network.
     * Note that it needn't be in the list if we failed during creation.
     */
    PINTNETNETWORK pPrev = pIntNet->pNetworks;
    if (pPrev == pNetwork)
        pIntNet->pNetworks = pNetwork->pNext;
    else
    {
        for (; pPrev; pPrev = pPrev->pNext)
            if (pPrev->pNext == pNetwork)
            {
                pPrev->pNext = pNetwork->pNext;
                break;
            }
    }
    pNetwork->pNext = NULL;
    pNetwork->pvObj = NULL;

    /*
     * Because of the undefined order of the per session object dereferencing
     * when closing a session, we have to handle the case where the network is
     * destroyed before the interfaces.
     * We deal with this simply by orphaning the interfaces.
     */
    RTSemFastMutexRequest(pNetwork->FastMutex2);

    PINTNETIF pCur = pNetwork->pIfs;
    while (pCur)
    {
        PINTNETIF pNext = pCur->pNext;
        pCur->pNext    = NULL;
        pCur->pNetwork = NULL;
        pCur = pNext;
    }

    /* Grab and zap the trunk pointer before leaving the mutex. */
    PINTNETTRUNKIF pTrunkIF = pNetwork->pTrunkIF;
    pNetwork->pTrunkIF = NULL;

    RTSemFastMutexRelease(pNetwork->FastMutex2);

    /*
     * If there is a trunk, delete it.
     * Note that this may tak a while if we're unlucky...
     */
    if (pTrunkIF)
        intnetR0TrunkIfDestroy(pTrunkIF, pNetwork);

    /*
     * Free resources.
     */
    RTSemFastMutexDestroy(pNetwork->FastMutex2);
    pNetwork->FastMutex2 = NIL_RTSEMFASTMUTEX;
    RTMemFree(pNetwork);

    /* release the create/destroy sem. (can be done before trunk destruction.) */
    RTSemMutexRelease(pIntNet->hMtxCreateOpenDestroy);
}


/**
 * Opens an existing network.
 *
 * The call must own the INTNET::hMtxCreateOpenDestroy.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance data.
 * @param   pSession        The current session.
 * @param   pszNetwork      The network name. This has a valid length.
 * @param   enmTrunkType    The trunk type.
 * @param   pszTrunk        The trunk name. Its meaning is specfic to the type.
 * @param   fFlags          Flags, see INTNET_OPEN_FLAGS_*.
 * @param   ppNetwork       Where to store the pointer to the network on success.
 */
static int intnetR0OpenNetwork(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, INTNETTRUNKTYPE enmTrunkType,
                               const char *pszTrunk, uint32_t fFlags, PINTNETNETWORK *ppNetwork)
{
    LogFlow(("intnetR0OpenNetwork: pIntNet=%p pSession=%p pszNetwork=%p:{%s} enmTrunkType=%d pszTrunk=%p:{%s} fFlags=%#x ppNetwork=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, enmTrunkType, pszTrunk, pszTrunk, fFlags, ppNetwork));

    /* just pro forma validation, the caller is internal. */
    AssertPtr(pIntNet);
    AssertPtr(pSession);
    AssertPtr(pszNetwork);
    Assert(enmTrunkType > kIntNetTrunkType_Invalid && enmTrunkType < kIntNetTrunkType_End);
    AssertPtr(pszTrunk);
    Assert(!(fFlags & ~(INTNET_OPEN_FLAGS_MASK)));
    AssertPtr(ppNetwork);
    *ppNetwork = NULL;

    /*
     * Search networks by name.
     */
    PINTNETNETWORK pCur;
    uint8_t cchName = (uint8_t)strlen(pszNetwork);
    Assert(cchName && cchName < sizeof(pCur->szName)); /* caller ensures this */

    pCur = pIntNet->pNetworks;
    while (pCur)
    {
        if (    pCur->cchName == cchName
            &&  !memcmp(pCur->szName, pszNetwork, cchName))
        {
            /*
             * Found the network, now check that we have the same ideas
             * about the trunk setup and security.
             */
            int rc;
            if (    enmTrunkType == kIntNetTrunkType_WhateverNone
                ||  (   pCur->enmTrunkType == enmTrunkType
                     && !strcmp(pCur->szTrunk, pszTrunk)))
            {
                if (!((pCur->fFlags ^ fFlags) & INTNET_OPEN_FLAGS_COMPATIBILITY_XOR_MASK))
                {

                    /*
                     * Increment the reference and check that the session
                     * can access this network.
                     */
                    rc = SUPR0ObjAddRef(pCur->pvObj, pSession);
                    if (RT_SUCCESS(rc))
                    {
                        if (!(pCur->fFlags & INTNET_OPEN_FLAGS_PUBLIC))
                            rc = SUPR0ObjVerifyAccess(pCur->pvObj, pSession, pCur->szName);
                        if (RT_SUCCESS(rc))
                        {
                            pCur->fFlags |= fFlags & INTNET_OPEN_FLAGS_SECURITY_OR_MASK;

                            *ppNetwork = pCur;
                        }
                        else
                            SUPR0ObjRelease(pCur->pvObj, pSession);
                    }
                    else if (rc == VERR_WRONG_ORDER)
                        rc = VERR_NOT_FOUND; /* destruction race, pretend the other isn't there. */
                }
                else
                    rc = VERR_INTNET_INCOMPATIBLE_FLAGS;
            }
            else
                rc = VERR_INTNET_INCOMPATIBLE_TRUNK;

            LogFlow(("intnetR0OpenNetwork: returns %Rrc *ppNetwork=%p\n", rc, *ppNetwork));
            return rc;
        }

        pCur = pCur->pNext;
    }

    LogFlow(("intnetR0OpenNetwork: returns VERR_NOT_FOUND\n"));
    return VERR_NOT_FOUND;
}


/**
 * Creates a new network.
 *
 * The call must own the INTNET::hMtxCreateOpenDestroy and has already attempted
 * opening the network and found it to be non-existing.
 *
 * @returns VBox status code.
 * @param   pIntNet         The instance data.
 * @param   pSession        The session handle.
 * @param   pszNetwork      The name of the network. This must be at least one character long and no longer
 *                          than the INTNETNETWORK::szName.
 * @param   enmTrunkType    The trunk type.
 * @param   pszTrunk        The trunk name. Its meaning is specfic to the type.
 * @param   fFlags          Flags, see INTNET_OPEN_FLAGS_*.
 * @param   ppNetwork       Where to store the network. In the case of failure
 *                          whatever is returned here should be dereferenced
 *                          outside the INTNET::hMtxCreateOpenDestroy.
 */
static int intnetR0CreateNetwork(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork, INTNETTRUNKTYPE enmTrunkType,
                                 const char *pszTrunk, uint32_t fFlags, PINTNETNETWORK *ppNetwork)
{
    LogFlow(("intnetR0CreateNetwork: pIntNet=%p pSession=%p pszNetwork=%p:{%s} enmTrunkType=%d pszTrunk=%p:{%s} fFlags=%#x ppNetwork=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, enmTrunkType, pszTrunk, pszTrunk, fFlags, ppNetwork));

    /* just pro forma validation, the caller is internal. */
    AssertPtr(pIntNet);
    AssertPtr(pSession);
    AssertPtr(pszNetwork);
    Assert(enmTrunkType > kIntNetTrunkType_Invalid && enmTrunkType < kIntNetTrunkType_End);
    AssertPtr(pszTrunk);
    Assert(!(fFlags & ~INTNET_OPEN_FLAGS_MASK));
    AssertPtr(ppNetwork);
    *ppNetwork = NULL;

    /*
     * Allocate and initialize.
     */
    size_t cb = sizeof(INTNETNETWORK);
    if (fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
        cb += INTNETNETWORK_TMP_SIZE + 64;
    PINTNETNETWORK pNew = (PINTNETNETWORK)RTMemAllocZ(cb);
    if (!pNew)
        return VERR_NO_MEMORY;
    int rc = RTSemFastMutexCreate(&pNew->FastMutex2);
    if (RT_SUCCESS(rc))
    {
        //pNew->pIfs        = NULL;
        pNew->pIntNet       = pIntNet;
        //pNew->cActiveIFs  = 0;
        pNew->fFlags        = fFlags;
        size_t cchName      = strlen(pszNetwork);
        pNew->cchName       = (uint8_t)cchName;
        Assert(cchName && cchName < sizeof(pNew->szName));  /* caller's responsibility. */
        memcpy(pNew->szName, pszNetwork, cchName);          /* '\0' by alloc. */
        pNew->enmTrunkType  = enmTrunkType;
        Assert(strlen(pszTrunk) < sizeof(pNew->szTrunk));   /* caller's responsibility. */
        strcpy(pNew->szTrunk, pszTrunk);
        if (fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
            pNew->pbTmp     = RT_ALIGN_PT(pNew + 1, 64, uint8_t *);
        //else
        //    pNew->pbTmp   = NULL;

        /*
         * Register the object in the current session and link it into the network list.
         */
        pNew->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_INTERNAL_NETWORK, intnetR0NetworkDestruct, pNew, pIntNet);
        if (pNew->pvObj)
        {
            pNew->pNext = pIntNet->pNetworks;
            pIntNet->pNetworks = pNew;

            /*
             * Check if the current session is actually allowed to create and
             * open the network.  It is possible to implement network name
             * based policies and these must be checked now.  SUPR0ObjRegister
             * does no such checks.
             */
            rc = SUPR0ObjVerifyAccess(pNew->pvObj, pSession, pNew->szName);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Connect the trunk.
                 */
                rc = intnetR0NetworkCreateTrunkIf(pNew, pSession);
                if (RT_SUCCESS(rc))
                {
                    *ppNetwork = pNew;
                    LogFlow(("intnetR0CreateNetwork: returns VINF_SUCCESS *ppNetwork=%p\n", pNew));
                    return VINF_SUCCESS;
                }
            }

            SUPR0ObjRelease(pNew->pvObj, pSession);
            LogFlow(("intnetR0CreateNetwork: returns %Rrc\n", rc));
            return rc;
        }
        rc = VERR_NO_MEMORY;

        RTSemFastMutexDestroy(pNew->FastMutex2);
        pNew->FastMutex2 = NIL_RTSEMFASTMUTEX;
    }
    RTMemFree(pNew);
    LogFlow(("intnetR0CreateNetwork: returns %Rrc\n", rc));
    return rc;
}


/**
 * Opens a network interface and connects it to the specified network.
 *
 * @returns VBox status code.
 * @param   pIntNet         The internal network instance.
 * @param   pSession        The session handle.
 * @param   pszNetwork      The network name.
 * @param   enmTrunkType    The trunk type.
 * @param   pszTrunk        The trunk name. Its meaning is specfic to the type.
 * @param   fFlags          Flags, see INTNET_OPEN_FLAGS_*.
 * @param   fRestrictAccess Whether new participants should be subjected to access check or not.
 * @param   cbSend          The send buffer size.
 * @param   cbRecv          The receive buffer size.
 * @param   phIf            Where to store the handle to the network interface.
 */
INTNETR0DECL(int) INTNETR0Open(PINTNET pIntNet, PSUPDRVSESSION pSession, const char *pszNetwork,
                               INTNETTRUNKTYPE enmTrunkType, const char *pszTrunk, uint32_t fFlags,
                               unsigned cbSend, unsigned cbRecv, PINTNETIFHANDLE phIf)
{
    LogFlow(("INTNETR0Open: pIntNet=%p pSession=%p pszNetwork=%p:{%s} enmTrunkType=%d pszTrunk=%p:{%s} fFlags=%#x cbSend=%u cbRecv=%u phIf=%p\n",
             pIntNet, pSession, pszNetwork, pszNetwork, enmTrunkType, pszTrunk, pszTrunk, fFlags, cbSend, cbRecv, phIf));

    /*
     * Validate input.
     */
    AssertPtrReturn(pIntNet, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszNetwork, VERR_INVALID_PARAMETER);
    const char *pszNetworkEnd = (const char *)memchr(pszNetwork, '\0', INTNET_MAX_NETWORK_NAME);
    AssertReturn(pszNetworkEnd, VERR_INVALID_PARAMETER);
    size_t cchNetwork = pszNetworkEnd - pszNetwork;
    AssertReturn(cchNetwork, VERR_INVALID_PARAMETER);

    if (pszTrunk)
    {
        AssertPtrReturn(pszTrunk, VERR_INVALID_PARAMETER);
        const char *pszTrunkEnd = (const char *)memchr(pszTrunk, '\0', INTNET_MAX_TRUNK_NAME);
        AssertReturn(pszTrunkEnd, VERR_INVALID_PARAMETER);
    }
    else
        pszTrunk = "";

    AssertMsgReturn(enmTrunkType > kIntNetTrunkType_Invalid && enmTrunkType < kIntNetTrunkType_End,
                    ("%d\n", enmTrunkType), VERR_INVALID_PARAMETER);
    switch (enmTrunkType)
    {
        case kIntNetTrunkType_None:
        case kIntNetTrunkType_WhateverNone:
            if (*pszTrunk)
                return VERR_INVALID_PARAMETER;
            break;

        case kIntNetTrunkType_NetFlt:
        case kIntNetTrunkType_NetAdp:
            if (!*pszTrunk)
                return VERR_INVALID_PARAMETER;
            break;

        default:
            return VERR_NOT_IMPLEMENTED;
    }

    AssertMsgReturn(!(fFlags & ~INTNET_OPEN_FLAGS_MASK), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phIf, VERR_INVALID_PARAMETER);

    /*
     * Acquire the mutex to serialize open/create/close.
     */
    int rc = RTSemMutexRequest(pIntNet->hMtxCreateOpenDestroy, RT_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Try open / create the network and create an interface on it for the
     * caller to use.
     */
    PINTNETNETWORK pNetwork = NULL;
    rc = intnetR0OpenNetwork(pIntNet, pSession, pszNetwork, enmTrunkType, pszTrunk, fFlags, &pNetwork);
    if (RT_SUCCESS(rc))
    {
        rc = intnetR0NetworkCreateIf(pNetwork, pSession, cbSend, cbRecv, phIf);
        if (RT_SUCCESS(rc))
            rc = VINF_ALREADY_INITIALIZED;
        SUPR0ObjRelease(pNetwork->pvObj, pSession);
    }
    else if (rc == VERR_NOT_FOUND)
    {
        rc = intnetR0CreateNetwork(pIntNet, pSession, pszNetwork, enmTrunkType, pszTrunk, fFlags, &pNetwork);
        if (RT_SUCCESS(rc))
        {
            rc = intnetR0NetworkCreateIf(pNetwork, pSession, cbSend, cbRecv, phIf);
            SUPR0ObjRelease(pNetwork->pvObj, pSession);
        }
    }

    RTSemMutexRelease(pIntNet->hMtxCreateOpenDestroy);
    LogFlow(("INTNETR0Open: return %Rrc *phIf=%RX32\n", rc, *phIf));
    return rc;
}


/**
 * VMMR0 request wrapper for GMMR0MapUnmapChunk.
 *
 * @returns see GMMR0MapUnmapChunk.
 * @param   pIntNet         The internal networking instance.
 * @param   pSession        The caller's session.
 * @param   pReq            The request packet.
 */
INTNETR0DECL(int) INTNETR0OpenReq(PINTNET pIntNet, PSUPDRVSESSION pSession, PINTNETOPENREQ pReq)
{
    if (RT_UNLIKELY(pReq->Hdr.cbReq != sizeof(*pReq)))
        return VERR_INVALID_PARAMETER;
    return INTNETR0Open(pIntNet, pSession, &pReq->szNetwork[0], pReq->enmTrunkType, pReq->szTrunk,
                        pReq->fFlags, pReq->cbSend, pReq->cbRecv, &pReq->hIf);
}


/**
 * Destroys an instance of the Ring-0 internal networking service.
 *
 * @param   pIntNet     Pointer to the instance data.
 */
INTNETR0DECL(void) INTNETR0Destroy(PINTNET pIntNet)
{
    LogFlow(("INTNETR0Destroy: pIntNet=%p\n", pIntNet));

    /*
     * Allow NULL pointers.
     */
    if (!pIntNet)
        return;
    AssertPtrReturnVoid(pIntNet);

    /*
     * There is not supposed to be any networks hanging around at this time.
     */
    Assert(pIntNet->pNetworks == NULL);
    if (pIntNet->hMtxCreateOpenDestroy != NIL_RTSEMMUTEX)
    {
        RTSemMutexDestroy(pIntNet->hMtxCreateOpenDestroy);
        pIntNet->hMtxCreateOpenDestroy = NIL_RTSEMMUTEX;
    }
    if (pIntNet->hHtIfs != NIL_RTHANDLETABLE)
    {
        /** @todo does it make sense to have a deleter here? */
        RTHandleTableDestroy(pIntNet->hHtIfs, NULL, NULL);
        pIntNet->hHtIfs = NIL_RTHANDLETABLE;
    }

    RTMemFree(pIntNet);
}


/**
 * Create an instance of the Ring-0 internal networking service.
 *
 * @returns VBox status code.
 * @param   ppIntNet    Where to store the instance pointer.
 */
INTNETR0DECL(int) INTNETR0Create(PINTNET *ppIntNet)
{
    LogFlow(("INTNETR0Create: ppIntNet=%p\n", ppIntNet));
    int rc = VERR_NO_MEMORY;
    PINTNET pIntNet = (PINTNET)RTMemAllocZ(sizeof(*pIntNet));
    if (pIntNet)
    {
        //pIntNet->pNetworks = NULL;

        rc = RTSemMutexCreate(&pIntNet->hMtxCreateOpenDestroy);
        if (RT_SUCCESS(rc))
        {
            rc = RTHandleTableCreateEx(&pIntNet->hHtIfs, RTHANDLETABLE_FLAGS_LOCKED | RTHANDLETABLE_FLAGS_CONTEXT,
                                       UINT32_C(0x8ffe0000), 4096, intnetR0IfRetainHandle, NULL);
            if (RT_SUCCESS(rc))
            {
                *ppIntNet = pIntNet;
                LogFlow(("INTNETR0Create: returns VINF_SUCCESS *ppIntNet=%p\n", pIntNet));
                return VINF_SUCCESS;
            }

            RTSemMutexDestroy(pIntNet->hMtxCreateOpenDestroy);
        }
        RTMemFree(pIntNet);
    }
    *ppIntNet = NULL;
    LogFlow(("INTNETR0Create: returns %Rrc\n", rc));
    return rc;
}

