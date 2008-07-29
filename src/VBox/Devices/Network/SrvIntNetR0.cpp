/* $Id$ */
/** @file
 * Internal networking - The ring 0 service.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
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
 * IPv4 address.
 */
typedef RTUINT32U INTNETADDRIPV4;
/** Pointer to a IPv4 address. */
typedef INTNETADDRIPV4 *PINTNETADDRIPV4;
/** Pointer to a const IPv4 address. */
typedef INTNETADDRIPV4 const *PCINTNETADDRIPV4;

/**
 * IPv6 address.
 */
typedef RTUINT128U INTNETADDRIPV6;
/** Pointer to a IPv4 address. */
typedef INTNETADDRIPV6 *PINTNETADDRIPV6;
/** Pointer to a const IPv4 address. */
typedef INTNETADDRIPV6 const *PCINTNETADDRIPV6;

/**
 * IPX address.
 */
typedef struct INTNETADDRIPX
{
    /** The network ID. */
    uint32_t Network;
    /** The node ID. (Defaults to the MAC address apparently.) */
    PDMMAC Node;
} INTNETADDRIPX;
/** Pointer to an IPX address. */
typedef INTNETADDRIPX *PINTNETADDRIPX;
/** Pointer to a const IPX address. */
typedef INTNETADDRIPX const *PCINTNETADDRIPX;

/**
 * Address union.
 */
typedef union INTNETADDRU
{
    /** 64-bit view. */
    uint64_t au64[2];
    /** 32-bit view. */
    uint32_t au32[4];
    /** 16-bit view. */
    uint16_t au16[8];
    /** 8-bit view. */
    uint8_t  au8[16];
    /** IPv4 view. */
    INTNETADDRIPV4 IPv4;
    /** IPv6 view. */
    INTNETADDRIPV6 IPv6;
    /** IPX view. */
    INTNETADDRIPX Ipx;
} INTNETADDRU;
/** Pointer to an address union. */
typedef INTNETADDRU *PINTNETADDRU;
/** Pointer to a const address union. */
typedef INTNETADDRU const *PCINTNETADDRU;

/**
 * Address and type.
 */
typedef struct INTNETADDR
{
    /** The address type. */
    INTNETADDRTYPE enmType;
    /** The address. */
    INTNETADDRU Addr;
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
    uint8_t *pbEntries;
    /** The number of valid address entries. */
    uint8_t cEntries;
    /** The number of allocated address entries. */
    uint8_t cEntriesAlloc;
    /** The address size. */
    uint8_t cbAddress;
    /** The size of an entry. */
    uint8_t cbEntry;
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
    /** The current MAC address for the interface. */
    PDMMAC                  Mac;
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
     * We will stop yeilding when this reaches a threshold assuming that the VM is paused or
     * that it simply isn't worth all the delay. It is cleared when a successful send has been done.
     */
    uint32_t                cYields;
    /** Pointer to the current exchange buffer (ring-0). */
    PINTNETBUF              pIntBuf;
    /** Pointer to ring-3 mapping of the current exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufR3;
    /** Pointer to the default exchange buffer for the interface. */
    PINTNETBUF              pIntBufDefault;
    /** Pointer to ring-3 mapping of the default exchange buffer. */
    R3PTRTYPE(PINTNETBUF)   pIntBufDefaultR3;
    /** Event semaphore which a receiver thread will sleep on while waiting for data to arrive. */
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
    /** The trunk mutex that serializes all calls <b>to</b> the component. */
    RTSEMFASTMUTEX          FastMutex;
    /** Pointer to the network we're connect to.
     * This may be NULL if we're orphaned? */
    struct INTNETNETWORK   *pNetwork;
    /** Whether to supply physical addresses with the outbound SGs. */
    bool volatile           fPhysSG;
    /** Set if the 'wire' is in promiscuous mode.
     * The state of the 'host' is queried each time. */
    bool                    fPromiscuousWire;
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
    PINTNETIF               pIFs;
    /** Pointer to the trunk interface.
     * Can be NULL if there is no trunk connection. */
    PINTNETTRUNKIF          pTrunkIF;
    /** The network mutex.
     * It protects everything dealing with this network. */
    RTSEMFASTMUTEX          FastMutex;
    /** Pointer to the instance data. */
    struct INTNET          *pIntNet;
    /** The SUPR0 object id. */
    void                   *pvObj;
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


/**
 * Internal networking instance.
 */
typedef struct INTNET
{
    /** Mutex protecting the network creation, opening and destruction.
     * (This means all operations affecting the pNetworks list.) */
    RTSEMFASTMUTEX          FastMutex;
    /** List of networks. Protected by INTNET::Spinlock. */
    PINTNETNETWORK volatile pNetworks;
    /** Handle table for the interfaces. */
    RTHANDLETABLE           hHtIfs;
} INTNET;


/**
 * Ethernet header.
 */
#pragma pack(1)
typedef struct INTNETETHERHDR
{
    PDMMAC      MacDst;
    PDMMAC      MacSrc;
    uint16_t    EtherType;
} INTNETETHERHDR;
#pragma pack()
/** Pointer to an ethernet header. */
typedef INTNETETHERHDR *PINTNETETHERHDR;
/** Pointer to a const ethernet header. */
typedef INTNETETHERHDR const *PCINTNETETHERHDR;

/** @name EtherType
 * @{ */
#define INTNET_ETHERTYPE_IPV4   UINT16_C(0x0800)
#define INTNET_ETHERTYPE_ARP    UINT16_C(0x0806)
#define INTNET_ETHERTYPE_IPV6   UINT16_C(0x86dd)
/** @} */


#pragma pack(1)
typedef struct INTNETIPV4
{
#ifdef RT_BIG_ENDIAN
    unsigned int    ip_v : 4;
    unsigned int    ip_hl : 4;
    unsigned int    ip_tos : 8;
    unsigned int    ip_len : 16;
#else
    unsigned int    ip_hl : 4;
    unsigned int    ip_v : 4;
    unsigned int    ip_tos : 8;
    unsigned int    ip_len : 16;
#endif
    uint16_t        ip_id;
    uint16_t        ip_off;
    uint8_t         ip_ttl;
    uint8_t         ip_p;
    uint16_t        ip_sum;
    uint32_t        ip_src;
    uint32_t        ip_dst;
    /* more */
    uint32_t        ip_options[1];
} INTNETIPV4;
typedef INTNETIPV4 *PINTNETIPV4;
typedef INTNETIPV4 const *PCINTNETIPV4;


typedef struct INTNETUDPV4
{
    uint16_t    uh_sport;
    uint16_t    uh_dport;
    uint16_t    uh_ulen;
    uint16_t    uh_sum;
} INTNETUDPV4;
typedef INTNETUDPV4 *PINTNETUDPV4;
typedef INTNETUDPV4 const *PCINTNETUDPV4;


typedef struct INTNETDHCP
{
    uint8_t     Op;
    uint8_t     HType;
    uint8_t     HLen;
    uint8_t     Hops;
    uint32_t    XID;
    uint16_t    Secs;
    uint16_t    Flags;
    uint32_t    CIAddr;
    uint32_t    YIAddr;
    uint32_t    SIAddr;
    uint32_t    GIAddr;
    uint8_t     CHAddr[16];
    uint8_t     SName[64];
    uint8_t     File[128];
    uint8_t     abMagic[4];
    uint8_t     DhcpOpt;
    uint8_t     DhcpLen; /* 1 */
    uint8_t     DhcpReq;
    uint8_t     abOptions[57];
} INTNETDHCP;
typedef INTNETDHCP *PINTNETDHCP;
typedef INTNETDHCP const *PCINTNETDHCP;

#pragma pack(0)

/** IPv4: UDP */
#define INTNETIPV4_PROT_UDP         7


/** ARP hardware type - ethernet. */
#define INTNET_ARP_ETHER            UINT16_C(1)

/** @name ARP operations
 * @{ */
#define INTNET_ARPOP_REQUEST        UINT16_C(1) /**< Request hardward address given a protocol address (ARP). */
#define INTNET_ARPOP_REPLY          UINT16_C(2)
#define INTNET_ARPOP_REVREQUEST     UINT16_C(3) /**< Request protocol address given a hardware address (RARP). */
#define INTNET_ARPOP_REVREPLY       UINT16_C(4)
#define INTNET_ARPOP_INVREQUEST     UINT16_C(8) /**< Inverse ARP.  */
#define INTNET_ARPOP_INVREPLY       UINT16_C(9)
#define INTNET_ARPOP_IS_REQUEST(Op) ((Op) & 1)
#define INTNET_ARPOP_IS_REPLY(Op)   (!INTNET_ARPOP_IS_REQUEST(Op))
/** @} */

#pragma pack(1)
/**
 * Ethernet ARP header.
 */
typedef struct INTNETARPHDR
{
    /** The hardware type. */
    uint16_t    ar_htype;
    /** The protocol type (ethertype). */
    uint16_t    ar_ptype;
    /** The hardware address length. */
    uint8_t     ar_hlen;
    /** The protocol address length. */
    uint8_t     ar_plen;
    /** The operation. */
    uint16_t    ar_oper;
} INTNETARPHDR;
#pragma pack(0)
/** Pointer to an ethernet ARP header. */
typedef INTNETARPHDR *PINTNETARPHDR;
/** Pointer to a const ethernet ARP header. */
typedef INTNETARPHDR const *PCINTNETARPHDR;


#pragma pack(1)
/**
 * Ethernet IPv4 + 6-byte MAC ARP request packet.
 */
typedef struct INTNETARPIPV4
{
    INTNETARPHDR    Hdr;
    /** The sender hardware address. */
    PDMMAC          ar_sha;
    /** The sender protocol address. */
    INTNETADDRIPV4  ar_spa;
    /** The target hardware address. */
    PDMMAC          ar_tha;
    /** The arget protocol address. */
    INTNETADDRIPV4  ar_tpa;
} INTNETARPIPV4;
#pragma pack(0)
/** Pointer to an ethernet IPv4+MAC ARP request packet. */
typedef INTNETARPIPV4 *PINTNETARPIPV4;
/** Pointer to a const ethernet IPv4+MAC ARP request packet. */
typedef INTNETARPIPV4 const *PCINTNETARPIPV4;


#pragma pack(1)
/**
 * Ethernet IPv6 + 6-byte MAC ARP request packet.
 */
typedef struct INTNETARPIPV6
{
    INTNETARPHDR    Hdr;
    /** The sender hardware address. */
    PDMMAC          ar_sha;
    /** The sender protocol address. */
    INTNETADDRIPV6  ar_spa;
    /** The target hardware address. */
    PDMMAC          ar_tha;
    /** The arget protocol address. */
    INTNETADDRIPV6  ar_tpa;
} INTNETARPIPV6;
#pragma pack(0)
/** Pointer to an ethernet IPv6+MAC ARP request packet. */
typedef INTNETARPIPV6 *PINTNETARPIPV6;
/** Pointer to a const ethernet IPv6+MAC ARP request packet. */
typedef INTNETARPIPV6 const *PCINTNETARPIPV6;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static PINTNETTRUNKIF intnetR0TrunkIfRetain(PINTNETTRUNKIF pThis);
static void intnetR0TrunkIfRelease(PINTNETTRUNKIF pThis);
static bool intnetR0TrunkIfOutLock(PINTNETTRUNKIF pThis);
static void intnetR0TrunkIfOutUnlock(PINTNETTRUNKIF pThis);



/**
 * Retain an interface.
 *
 * @returns VBox status code, can assume success in most situations.
 * @param   pIf                 The interface instance.
 * @param   pSession            The current session.
 */
DECLINLINE(int) intnetR0IfRetain(PINTNETIF pIf, PSUPDRVSESSION pSession)
{
    int rc = SUPR0ObjAddRef(pIf->pvObj, pSession);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}


/**
 * Release an interface previously retained by intnetR0IfRetain or
 * by handle lookup/freeing.
 *
 * @returns VBox status code, can assume success in most situations.
 * @param   pIf                 The interface instance.
 * @param   pSession            The current session.
 */
DECLINLINE(void) intnetR0IfRelease(PINTNETIF pIf, PSUPDRVSESSION pSession)
{
    int rc = SUPR0ObjRelease(pIf->pvObj, pSession);
    AssertRC(rc);
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
DECLINLINE(bool) intnetR0AddrUIsEqualEx(PCINTNETADDRU pAddr1, PCINTNETADDRU pAddr2, uint8_t const cbAddr)
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
static int intnetR0IfAddrCacheLookupSlow(PCINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    unsigned i = pCache->cEntries - 2;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry * i;
    while (i >= 1)
    {
        if (intnetR0AddrUIsEqualEx((PCINTNETADDRU)pbEntry, pAddr, cbAddr))
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
DECLINLINE(int) intnetR0IfAddrCacheLookup(PCINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when there is one cache entry and
     * it doesn't match.
     */
    unsigned i = pCache->cEntries;
    if (    i > 0
        &&  intnetR0AddrUIsEqualEx((PCINTNETADDRU)pCache->pbEntries, pAddr, cbAddr))
        return 0;
    if (i <= 1)
        return -1;

    /*
     * Check the last entry.
     */
    i--;
    if (intnetR0AddrUIsEqualEx((PCINTNETADDRU)(pCache->pbEntries + pCache->cbEntry * i), pAddr, cbAddr))
        return i;
    if (i <= 1)
        return -1;

    return intnetR0IfAddrCacheLookupSlow(pCache, pAddr, cbAddr);
}

/** Same as intnetR0IfAddrCacheLookup except we expect the address to be present already. */
DECLINLINE(int) intnetR0IfAddrCacheLookupLikely(PCINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
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
static int intnetR0IfAddrCacheInCacheUnlikelySlow(PCINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    /*
     * Perform a full table lookup.
     */
    unsigned i = pCache->cEntries - 2;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry * i;
    while (i >= 1)
    {
        if (RT_UNLIKELY(intnetR0AddrUIsEqualEx((PCINTNETADDRU)pbEntry, pAddr, cbAddr)))
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
DECLINLINE(int) intnetR0IfAddrCacheLookupUnlikely(PCINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when there is one cache entry and
     * it doesn't match.
     */
    unsigned i = pCache->cEntries;
    if (RT_UNLIKELY(   i > 0
                    && intnetR0AddrUIsEqualEx((PCINTNETADDRU)pCache->pbEntries, pAddr, cbAddr)))
        return 0;
    if (RT_LIKELY(i <= 1))
        return -1;

    /*
     * Then check the last entry and return if there are just two cache entries.
     */
    i--;
    if (RT_UNLIKELY(intnetR0AddrUIsEqualEx((PCINTNETADDRU)(pCache->pbEntries + pCache->cbEntry * i), pAddr, cbAddr)))
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
 */
static void intnetR0IfAddrCacheDeleteIt(PINTNETIF pIf, PINTNETADDRCACHE pCache, int iEntry)
{
    Log(("intnetR0IfAddrCacheDeleteIt: hIf=%RX32 type=%d #%d %.*Rhxs\n", pIf->hIf,
         (int)(intptr_t)(pCache - &pIf->aAddrCache[0]), iEntry, pCache->cbAddress, pCache->pbEntries + iEntry * pCache->cbEntry));
    Assert(iEntry < pCache->cEntries);
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
DECLINLINE(void) intnetR0IfAddrCacheDelete(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    int i = intnetR0IfAddrCacheLookup(pCache, pAddr, cbAddr);
    if (RT_UNLIKELY(i >= 0))
        intnetR0IfAddrCacheDeleteIt(pIf, pCache, i);
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
 */
DECLINLINE(void) intnetR0NetworkAddrCacheDelete(PINTNETNETWORK pNetwork, PCINTNETADDRU pAddr,
                                                INTNETADDRTYPE const enmType, uint8_t const cbAddr)
{
    for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
    {
        int i = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmType], pAddr, cbAddr);
        if (RT_UNLIKELY(i >= 0))
            intnetR0IfAddrCacheDeleteIt(pIf, &pIf->aAddrCache[enmType], i);
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
 */
DECLINLINE(void) intnetR0NetworkAddrCacheDeleteMinusIf(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, PCINTNETADDRU pAddr,
                                                       INTNETADDRTYPE const enmType, uint8_t const cbAddr)
{
    for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
        if (pIf != pIfSender)
        {
            int i = intnetR0IfAddrCacheLookup(&pIf->aAddrCache[enmType], pAddr, cbAddr);
            if (RT_UNLIKELY(i >= 0))
                intnetR0IfAddrCacheDeleteIt(pIf, &pIf->aAddrCache[enmType], i);
        }
}


/**
 * Adds an address to the cache, the caller is responsible for making sure it'
 * s not already in the cache.
 *
 * @param   pIf         The interface (for logging).
 * @param   pCache      The address cache.
 * @param   pAddr       The address.
 */
static void intnetR0IfAddrCacheAddIt(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCINTNETADDRU pAddr)
{
    if (!pCache->cEntriesAlloc)
    {
        /* Allocate the first array */
        pCache->pbEntries = (uint8_t *)RTMemAllocZ(pCache->cbEntry * 16);
        if (!pCache->pbEntries)
            return;
    }
    else
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
            /* simple FIFO, might consider usage/ageing here? */
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
    Log(("intnetR0IfAddrCacheAddIt: type=%d added #%d %.*Rhxs\n",
         (int)(uintptr_t)(pCache - &pIf->aAddrCache[0]), pCache->cEntries, pCache->cbAddress, pAddr));
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
 */
static void intnetR0IfAddrCacheAddSlow(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    /*
     * Check all but the first and last entries, the caller
     * has already checked those.
     */
    unsigned cLeft = pCache->cEntries;
    uint8_t const *pbEntry = pCache->pbEntries + pCache->cbEntry;
    while (--cLeft > 1)
    {
        if (RT_LIKELY(intnetR0AddrUIsEqualEx((PCINTNETADDRU)pbEntry, pAddr, cbAddr)))
        {
            /** @todo usage/ageing? */
            return;
        }
        pbEntry += pCache->cbEntry;
        cLeft--;
    }

    /*
     * Not found, add it.
     */
    intnetR0IfAddrCacheAddIt(pIf, pCache, pAddr);
}


/**
 * Adds an address to the cache if it's not already there.
 *
 * @param   pIf         The interface (for logging).
 * @param   pCache      The address cache.
 * @param   pAddr       The address.
 * @param   cbAddr      The size of the address (optimization).
 */
DECLINLINE(void) intnetR0IfAddrCacheAdd(PINTNETIF pIf, PINTNETADDRCACHE pCache, PCINTNETADDRU pAddr, uint8_t const cbAddr)
{
    Assert(pCache->cbAddress == cbAddr);

    /*
     * The optimized case is when the address the first cache entry.
     */
    unsigned i = pCache->cEntries;
    if (RT_LIKELY(   i > 0
                  && intnetR0AddrUIsEqualEx((PCINTNETADDRU)pCache->pbEntries, pAddr, cbAddr)))
    {
        /** @todo usage/ageing? */
        return;
    }
    if (i <= 1)
        return;

    /*
     * And the case where it's the last entry.
     */
    i--;
    if (RT_LIKELY(intnetR0AddrUIsEqualEx((PCINTNETADDRU)pCache->pbEntries, pAddr, cbAddr)))
    {
        /** @todo usage/ageing? */
        return;
    }
    if (i <= 1)
        return;

    intnetR0IfAddrCacheAddSlow(pIf, pCache, pAddr, cbAddr);
}


/**
 * Deals with an IPv4 packet.
 *
 * This will fish out the source IP address and add it to the cache.
 * Then it will look for DHCPRELEASE requests (?) and anything else
 * that we migh find useful later.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pHdr            Pointer to the IPv4 header in the frame.
 * @param   cbPacket        The size of the packet, or more correctly the
 *                          size of the frame without the ethernet header.
 */
static void intnetR0IfSniffIPv4SourceAddr(PINTNETIF pIf, PCINTNETIPV4 pHdr, uint32_t cbPacket)
{
    /*
     * Check the header size.
     */
    if (cbPacket < RT_UOFFSETOF(INTNETIPV4, ip_options)) /** @todo check minimum size requirements here. */
        return;
    uint32_t cbHdr = (uint32_t)pHdr->ip_hl * 4;
    if (    cbHdr < RT_UOFFSETOF(INTNETIPV4, ip_options)
        ||  cbPacket < cbHdr)
        return;

    /*
     * Ignore 255.255.255.255 (broadcast), 0.0.0.0 (null) and already cached addresses.
     */
    INTNETADDRU Addr;
    Addr.au32[0] = pHdr->ip_src;
    if (Addr.au32[0] == UINT32_C(0xffffffff))
        return;

    int i = -1;
    if (    Addr.au32[0] == 0
        || (i = intnetR0IfAddrCacheLookupLikely(&pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr, sizeof(pHdr->ip_src))) >= 0)
    {
#if 0 /** @todo quick DHCP check? */
        if (pHdr->ip_p != INTNETIPV4_PROT_UDP)
            return;
#endif
        return;
    }

    /*
     * Check the header checksum.
     */
    /** @todo IP checksumming */

    /*
     * Add the source address.
     */
    if (i < 0)
        intnetR0IfAddrCacheAddIt(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr);

    /*
     * Check for DHCP (properly this time).
     */
    /** @todo DHCPRELEASE request? */
}



/**
 * Sniff up source addresses from an ARP request or reply.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pHdr            The ARP header.
 * @param   cbPacket        The size of the packet (migth be larger than the ARP
 *                          request 'cause of min ethernet frame size).
 */
static void intnetR0IfSniffArpSourceAddr(PINTNETIF pIf, PCINTNETARPHDR pHdr, uint32_t cbPacket)
{
    /*
     * Ignore malformed packets and packets which doesn't interrest us.
     */
    if (RT_UNLIKELY(    cbPacket <= sizeof(*pHdr)
                    ||  pHdr->ar_hlen != sizeof(PDMMAC)
                    ||  pHdr->ar_htype != RT_H2BE_U16(INTNET_ARP_ETHER)
                    ||  (   pHdr->ar_oper != RT_H2BE_U16(INTNET_ARPOP_REQUEST)
                         && pHdr->ar_oper != RT_H2BE_U16(INTNET_ARPOP_REPLY)
                         && pHdr->ar_oper != RT_H2BE_U16(INTNET_ARPOP_REVREQUEST)
                         && pHdr->ar_oper != RT_H2BE_U16(INTNET_ARPOP_REVREPLY)
                         /** @todo Read up on inverse ARP. */
                        )
                   )
       )
        return;

    /*
     * Deal with the protocols.
     */
    INTNETADDRU Addr;
    if (pHdr->ar_ptype == RT_H2BE_U16(INTNET_ETHERTYPE_IPV4))
    {
        /*
         * IPv4.
         */
        PCINTNETARPIPV4 pReq = (PCINTNETARPIPV4)pHdr;
        if (RT_UNLIKELY(    pHdr->ar_plen == sizeof(pReq->ar_spa)
                        ||  cbPacket < sizeof(*pReq)
                        ||  pReq->ar_spa.u == 0 /** @todo add an inline function for checking that an IP address is worth caching. */
                        ||  pReq->ar_spa.u == UINT32_C(0xffffffff)))
            return;

        if (INTNET_ARPOP_IS_REPLY(RT_BE2H_U16(pHdr->ar_oper)))
        {
            Addr.IPv4 = pReq->ar_tpa;
            intnetR0IfAddrCacheDelete(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr, sizeof(pReq->ar_tpa));
        }
        Addr.IPv4 = pReq->ar_spa;
        intnetR0IfAddrCacheAdd(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv4], &Addr, sizeof(pReq->ar_spa));
    }
    else if (pHdr->ar_ptype == RT_H2BE_U16(INTNET_ETHERTYPE_IPV6))
    {
        /*
         * IPv6.
         */
        PCINTNETARPIPV6 pReq = (PCINTNETARPIPV6)pHdr;
        if (RT_UNLIKELY(    pHdr->ar_plen == sizeof(pReq->ar_spa)
                        ||  cbPacket < sizeof(*pReq)
                        /** @todo IPv6 ||  pReq->ar_spa.au32[0] == 0 or something
                        ||  pReq->ar_spa.au32[0] == UINT32_C(0xffffffff)*/))
            return;

        if (INTNET_ARPOP_IS_REPLY(RT_BE2H_U16(pHdr->ar_oper)))
        {
            Addr.IPv6 = pReq->ar_tpa;
            intnetR0IfAddrCacheDelete(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv6], &Addr, sizeof(pReq->ar_tpa));
        }
        Addr.IPv6 = pReq->ar_spa;
        intnetR0IfAddrCacheAdd(pIf, &pIf->aAddrCache[kIntNetAddrType_IPv6], &Addr, sizeof(pReq->ar_spa));
    }
    else
        Log(("intnetR0IfSniffArpSourceAddr: unknown ar_ptype=%#x\n", RT_BE2H_U16(pHdr->ar_ptype)));
}



/**
 * Checks packets send by a normal interface for new network
 * layer addresses.
 *
 * @param   pIf             The interface that's sending the frame.
 * @param   pbFrame         The frame.
 * @param   cbFrame         The size of the frame.
 */
static void intnetR0IfSniffSourceAddr(PINTNETIF pIf, uint8_t const *pbFrame, uint32_t cbFrame)
{
    /*
     * Fish out the ethertype and look for stuff we can handle.
     */
    if (cbFrame <= sizeof(INTNETETHERHDR))
        return;
    cbFrame -= sizeof(INTNETETHERHDR);

    uint16_t EtherType = ((PCINTNETETHERHDR)pbFrame)->EtherType;
    if (EtherType == RT_H2BE_U16(INTNET_ETHERTYPE_IPV4))
        intnetR0IfSniffIPv4SourceAddr(pIf, (PCINTNETIPV4)((PCINTNETETHERHDR)pbFrame + 1), cbFrame);
#if 0 /** @todo IntNet: implement IPv6 for wireless MAC sharing. */
    else if (EtherType == RT_H2BE_U16(INTNET_ETHERTYPE_IPV6))
        intnetR0IfSniffIPv6SourceAddr(pIf, (PCINTNETIPV6)((PCINTNETETHERHDR)pbFrame + 1), cbFrame);
#endif
#if 0 /** @todo IntNet: implement IPX for wireless MAC sharing? */
    else if (   EtherType == RT_H2BE_U16(0x8037) //??
             || EtherType == RT_H2BE_U16(0x8137) //??
             || EtherType == RT_H2BE_U16(0x8138) //??
             )
        intnetR0IfSniffIpxSourceAddr(pIf, (PCINTNETIPX)((PCINTNETETHERHDR)pbFrame + 1), cbFrame);
#endif
    else if (EtherType == RT_H2BE_U16(INTNET_ETHERTYPE_ARP))
        intnetR0IfSniffArpSourceAddr(pIf, (PCINTNETARPHDR)((PCINTNETETHERHDR)pbFrame + 1), cbFrame);
}


/**
 * Initializes a scatter / gather buffer from a simple linear buffer.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 *                      (The pvOwnerData, fFlags, cUsers, and cSegsAlloc members are left untouched.)
 * @param   pvFrame     Pointer to the frame
 * @param   cbFrame     The size of the frame.
 */
DECLINLINE(void) intnetR0SgInitTemp(PINTNETSG pSG, void *pvFrame, uint32_t cbFrame)
{
    pSG->pvOwnerData = NULL;
    pSG->pvUserData = NULL;
    pSG->pvUserData2 = NULL;
    pSG->cbTotal = cbFrame;
    pSG->cUsers = 1;
    pSG->fFlags = INTNETSG_FLAGS_TEMP;
    pSG->cSegsAlloc = 1;
    pSG->cSegsUsed = 1;
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;
    pSG->aSegs[0].pv = pvFrame;
    pSG->aSegs[0].cb = cbFrame;
}


/**
 * Initializes a scatter / gather buffer from a internal networking packet.
 *
 * @returns Pointer to the start of the frame.
 * @param   pSG         Pointer to the scatter / gather structure.
 *                      (The pvOwnerData, fFlags, cUsers, and cSegsAlloc members are left untouched.)
 * @param   pHdr        Pointer to the packet header.
 * @param   pBuf        The buffer the header is within. Only used in strict builds.
 */
DECLINLINE(void) intnetR0SgInitFromPkt(PINTNETSG pSG, PCINTNETHDR pPktHdr, PCINTNETBUF pBuf)
{
    pSG->cSegsUsed = 1;
    pSG->cbTotal = pSG->aSegs[0].cb = pPktHdr->cbFrame;
    pSG->aSegs[0].pv = INTNETHdrGetFramePtr(pPktHdr, pBuf);
    pSG->aSegs[0].Phys = NIL_RTHCPHYS;
}


#ifdef IN_INTNET_TESTCASE
/**
 * Reads the next frame in the buffer.
 * The caller is responsible for ensuring that there is a valid frame in the buffer.
 *
 * @returns Size of the frame in bytes.
 * @param   pBuf        The buffer.
 * @param   pRingBuff   The ring buffer to read from.
 * @param   pvFrame     Where to put the frame. The caller is responsible for
 *                      ensuring that there is sufficient space for the frame.
 */
static unsigned intnetR0RingReadFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, void *pvFrame)
{
    Assert(pRingBuf->offRead < pBuf->cbBuf);
    Assert(pRingBuf->offRead >= pRingBuf->offStart);
    Assert(pRingBuf->offRead < pRingBuf->offEnd);
    uint32_t    offRead   = pRingBuf->offRead;
    PINTNETHDR  pHdr      = (PINTNETHDR)((uint8_t *)pBuf + offRead);
    const void *pvFrameIn = INTNETHdrGetFramePtr(pHdr, pBuf);
    unsigned    cb        = pHdr->cbFrame;
    memcpy(pvFrame, pvFrameIn, cb);

    /* skip the frame */
    offRead += pHdr->offFrame + cb;
    offRead = RT_ALIGN_32(offRead, sizeof(INTNETHDR));
    Assert(offRead <= pRingBuf->offEnd && offRead >= pRingBuf->offStart);
    if (offRead >= pRingBuf->offEnd)
        offRead = pRingBuf->offStart;
    ASMAtomicXchgU32(&pRingBuf->offRead, offRead);
    return cb;
}
#endif /* IN_INTNET_TESTCASE */


/**
 * Reads an entire SG into a fittingly size buffer.
 *
 * @param   pSG         The SG list to read.
 * @param   pvBuf       The buffer to read into (at least pSG->cbTotal in size).
 */
DECLINLINE(void) intnetR0SgRead(PCINTNETSG pSG, void *pvBuf)
{
    if (pSG->cSegsUsed == 1)
    {
        Assert(pSG->cbTotal == pSG->aSegs[0].cb);
        memcpy(pvBuf, pSG->aSegs[0].pv, pSG->cbTotal);
    }
    else
    {
        uint8_t       *pbDst = (uint8_t *)pvBuf;
        unsigned const cSegs = pSG->cSegsUsed; Assert(cSegs == pSG->cSegsUsed);
        for (unsigned iSeg = 0; iSeg < cSegs; iSeg++)
        {
            uint32_t cbSeg = pSG->aSegs[iSeg].cb;
            Assert(cbSeg <= pSG->cbTotal && (uintptr_t)(pbDst - (uint8_t *)pvBuf) + cbSeg <= pSG->cbTotal);
            memcpy(pbDst, pSG->aSegs[iSeg].pv, cbSeg);
            pbDst += cbSeg;
        }
    }
}


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   pSG         The gatter list.
 */
static int intnetR0RingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, PCINTNETSG pSG)
{
    /*
     * Validate input.
     */
    AssertPtr(pBuf);
    AssertPtr(pRingBuf);
    AssertPtr(pSG);
    Assert(pSG->cbTotal >= sizeof(PDMMAC) * 2);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));

    const uint32_t cb = RT_ALIGN_32(pSG->cbTotal, sizeof(INTNETHDR));
    if (offRead <= offWrite)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWrite >= cb + sizeof(INTNETHDR))
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = pSG->cbTotal;
            pHdr->offFrame = sizeof(INTNETHDR);

            intnetR0SgRead(pSG, pHdr + 1);

            offWrite += cb + sizeof(INTNETHDR);
            Assert(offWrite <= pRingBuf->offEnd && offWrite >= pRingBuf->offStart);
            if (offWrite >= pRingBuf->offEnd)
                offWrite = pRingBuf->offStart;
            Log2(("WriteFrame: offWrite: %#x -> %#x (1)\n", pRingBuf->offWrite, offWrite));
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            return VINF_SUCCESS;
        }

        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWrite >= sizeof(INTNETHDR), ("offEnd=%x offWrite=%x\n", pRingBuf->offEnd, offWrite));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            PINTNETHDR  pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            void       *pvFrameOut = (PINTNETHDR)((uint8_t *)pBuf + pRingBuf->offStart);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = pSG->cbTotal;
            pHdr->offFrame = (intptr_t)pvFrameOut - (intptr_t)pHdr;

            intnetR0SgRead(pSG, pvFrameOut);

            offWrite = pRingBuf->offStart + cb;
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            Log2(("WriteFrame: offWrite: %#x -> %#x (2)\n", pRingBuf->offWrite, offWrite));
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWrite > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = pSG->cbTotal;
        pHdr->offFrame = sizeof(INTNETHDR);

        intnetR0SgRead(pSG, pHdr + 1);

        offWrite += cb + sizeof(INTNETHDR);
        ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
        Log2(("WriteFrame: offWrite: %#x -> %#x (3)\n", pRingBuf->offWrite, offWrite));
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    /** @todo stats */
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Sends a frame to a specific interface.
 *
 * @param   pIf             The interface.
 * @param   pIfSender       The interface sending the frame. This is NULL if it's the trunk.
 * @param   pSG             The gather buffer which data is being sent to the interface.
 */
static void intnetR0IfSend(PINTNETIF pIf, PINTNETIF pIfSender, PINTNETSG pSG)
{
//    LogFlow(("intnetR0IfSend: pIf=%p:{.hIf=%RX32}\n", pIf, pIf->hIf));
    int rc = intnetR0RingWriteFrame(pIf->pIntBuf, &pIf->pIntBuf->Recv, pSG);
    if (RT_SUCCESS(rc))
    {
        pIf->cYields = 0;
        STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatRecvs);
        STAM_REL_COUNTER_ADD(&pIf->pIntBuf->cbStatRecv, pSG->cbTotal);
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
            rc = intnetR0RingWriteFrame(pIf->pIntBuf, &pIf->pIntBuf->Recv, pSG);
            if (RT_SUCCESS(rc))
            {
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatYieldsOk);
                STAM_REL_COUNTER_INC(&pIf->pIntBuf->cStatRecvs);
                STAM_REL_COUNTER_ADD(&pIf->pIntBuf->cbStatRecv, pSG->cbTotal);
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
 * Sends a frame down the trunk.
 *
 * The caller must own the network mutex, might be abandond temporarily.
 * The fTrunkLock parameter indicates whether the trunk lock is held.
 *
 * @param   pThis           The trunk.
 * @param   pNetwork        The network the frame is being sent to.
 * @param   fDst            The destination flags.
 * @param   pSG             Pointer to the gather list.
 * @param   fTrunkLocked    Whether the caller owns the out-bound trunk lock.
 */
static void intnetR0TrunkIfSend(PINTNETTRUNKIF pThis, PINTNETNETWORK pNetwork, uint32_t fDst, PINTNETSG pSG, bool fTrunkLocked)
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
     * Temporarily leave the network lock while transmitting the frame.
     *
     * Note that we're relying on the out-bound lock to serialize threads down
     * in INTNETR0IfSend. It's theoretically possible for there to be race now
     * because I didn't implement async SG handling yet. Which is why we currently
     * require the trunk to be locked, well, one of the reasons.
     *
     * Another reason is that the intnetR0NetworkSendUnicast code may have to
     * call into the trunk interface component to do package switching.
     */
    AssertReturnVoid(fTrunkLocked); /* to be removed. */

    int rc;
    if (    fTrunkLocked
        ||  intnetR0TrunkIfRetain(pThis))
    {
        rc = RTSemFastMutexRelease(pNetwork->FastMutex);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            if (    fTrunkLocked
                ||  intnetR0TrunkIfOutLock(pThis))
            {
                rc = pThis->pIfPort->pfnXmit(pThis->pIfPort, pSG, fDst);

                if (!fTrunkLocked)
                    intnetR0TrunkIfOutUnlock(pThis);
            }
            else
            {
                AssertFailed();
                rc = VERR_SEM_DESTROYED;
            }

            int rc2 = RTSemFastMutexRequest(pNetwork->FastMutex);
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
 */
static bool intnetR0NetworkSendBroadcast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, PINTNETSG pSG, bool fTrunkLocked)
{
    /*
     * This is a broadcast or multicast address. For the present we treat those
     * two as the same - investigating multicast is left for later.
     *
     * Write the packet to all the interfaces and signal them.
     */
    for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
        if (pIf != pIfSender)
            intnetR0IfSend(pIf, pIfSender, pSG);

    /*
     * Unless the trunk is the origin, broadcast it to both the wire
     * and the host as well.
     */
    PINTNETTRUNKIF pTrunkIf = pNetwork->pTrunkIF;
    if (    pIfSender
        &&  pTrunkIf)
        intnetR0TrunkIfSend(pTrunkIf, pNetwork, INTNETTRUNKDIR_HOST | INTNETTRUNKDIR_WIRE, pSG, fTrunkLocked);
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
static bool intnetR0NetworkSendMulticast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc, PINTNETSG pSG, bool fTrunkLocked, PCINTNETETHERHDR pEthHdr)
{
    /** @todo implement multicast */
    return intnetR0NetworkSendBroadcast(pNetwork, pIfSender, pSG, fTrunkLocked);
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
static bool intnetR0NetworkSendUnicast(PINTNETNETWORK pNetwork, PINTNETIF pIfSender, uint32_t fSrc, PINTNETSG pSG, bool fTrunkLocked, PCINTNETETHERHDR pEthHdr)
{
    /*
     * Only send to the interfaces with matching a MAC address.
     */
    bool fExactIntNetRecipient = false;
    for (PINTNETIF pIf = pNetwork->pIFs; pIf; pIf = pIf->pNext)
    {
        bool fIt = false;
        if (    (   !pIf->fMacSet
                 || (fIt = !memcmp(&pIf->Mac, &pEthHdr->MacDst, sizeof(pIf->Mac))) )
            ||  (   pIf->fPromiscuous
                 && !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC))
                 && pIf != pIfSender /* promiscuous mode: omit the sender */))
        {
            Log2(("Dst=%.6Rhxs => %.6Rhxs\n", &pEthHdr->MacDst, &pIf->Mac));
            fExactIntNetRecipient |= fIt;
            intnetR0IfSend(pIf, pIfSender, pSG);
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
        /* promiscuous checks first as they are cheaper than pfnIsHostMac. */
        if (    pTrunkIf->fPromiscuousWire
            &&  !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC | INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_WIRE | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_WIRE)) )
            fDst |= INTNETTRUNKDIR_WIRE;
        if (    !(pNetwork->fFlags & (INTNET_OPEN_FLAGS_IGNORE_PROMISC | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC | INTNET_OPEN_FLAGS_IGNORE_PROMISC_TRUNK_HOST | INTNET_OPEN_FLAGS_QUIETLY_IGNORE_PROMISC_TRUNK_HOST))
            ||  pTrunkIf->pIfPort->pfnIsPromiscuous(pTrunkIf->pIfPort) )
            fDst |= INTNETTRUNKDIR_HOST;

        if (    fDst != (INTNETTRUNKDIR_HOST | INTNETTRUNKDIR_WIRE)
            &&  !fExactIntNetRecipient  /* if you have duplicate mac addresses, you're screwed. */ )
        {
            if (pTrunkIf->pIfPort->pfnIsHostMac(pTrunkIf->pIfPort, &pEthHdr->MacDst))
                fDst |= INTNETTRUNKDIR_HOST;
            else
                fDst |= INTNETTRUNKDIR_WIRE;
        }

        if (fDst)
            intnetR0TrunkIfSend(pTrunkIf, pNetwork, fDst, pSG, fTrunkLocked);
    }

    /* log it */
    if (    !fExactIntNetRecipient
        &&  !fDst
        &&  (   (pEthHdr->MacDst.au8[0] == 0x08 && pEthHdr->MacDst.au8[1] == 0x00 && pEthHdr->MacDst.au8[2] == 0x27)
             || (pEthHdr->MacSrc.au8[0] == 0x08 && pEthHdr->MacSrc.au8[1] == 0x00 && pEthHdr->MacSrc.au8[2] == 0x27)))
        Log2(("Dst=%.6Rhxs ??\n", &pEthHdr->MacDst));

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
    if (pSG->cbTotal < sizeof(INTNETETHERHDR))
        return fRc;

    /*
     * Send statistics.
     */
    if (pIfSender)
    {
        STAM_REL_COUNTER_INC(&pIfSender->pIntBuf->cStatSends);
        STAM_REL_COUNTER_ADD(&pIfSender->pIntBuf->cbStatSend, pSG->cbTotal);
    }

    /*
     * Get the ethernet header (might theoretically involve multiple segments).
     */
    INTNETETHERHDR EthHdr;
    if (RT_LIKELY(pSG->aSegs[0].cb >= sizeof(EthHdr)))
        EthHdr = *(PCINTNETETHERHDR)pSG->aSegs[0].pv;
    else
    {
        uint8_t *pbDst = (uint8_t *)&EthHdr;
        size_t   cbLeft = sizeof(EthHdr);
        for (unsigned iSeg = 0; cbLeft && iSeg < pSG->cSegsUsed; iSeg++)
        {
            size_t cb = RT_MIN(cbLeft, pSG->aSegs[iSeg].cb);
            memcpy(pbDst, pSG->aSegs[iSeg].pv, cb);
            pbDst += cb;
            cbLeft -= cb;
        }
        AssertReturn(!cbLeft, false);
    }
    if (    (EthHdr.MacDst.au8[0] == 0x08 && EthHdr.MacDst.au8[1] == 0x00 && EthHdr.MacDst.au8[2] == 0x27)
        ||  (EthHdr.MacSrc.au8[0] == 0x08 && EthHdr.MacSrc.au8[1] == 0x00 && EthHdr.MacSrc.au8[2] == 0x27)
        ||  (EthHdr.MacDst.au8[0] == 0x00 && EthHdr.MacDst.au8[1] == 0x16 && EthHdr.MacDst.au8[2] == 0xcb)
        ||  (EthHdr.MacSrc.au8[0] == 0x00 && EthHdr.MacSrc.au8[1] == 0x16 && EthHdr.MacSrc.au8[2] == 0xcb)
        ||  EthHdr.MacDst.au8[0] == 0xff
        ||  EthHdr.MacSrc.au8[0] == 0xff)
        Log2(("D=%.6Rhxs  S=%.6Rhxs  T=%04x f=%x z=%x\n",
              &EthHdr.MacDst, &EthHdr.MacSrc, RT_BE2H_U16(EthHdr.EtherType), fSrc, pSG->cbTotal));

    /*
     * Inspect the header updating the mac address of the sender in the process.
     */
    if (    pIfSender
        &&  memcmp(&EthHdr.MacSrc, &pIfSender->Mac, sizeof(pIfSender->Mac)))
    {
        /** @todo stats */
        Log2(("IF MAC: %.6Rhxs -> %.6Rhxs\n", &pIfSender->Mac, &EthHdr.MacSrc));
        pIfSender->Mac = EthHdr.MacSrc;
        pIfSender->fMacSet = true;
    }

    /*
     * Distribute the frame.
     */
    if (RT_UNLIKELY(EthHdr.MacDst.au8[0] & 1))      /* multicast address */
        fRc = intnetR0NetworkSendMulticast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, &EthHdr);
    else if (   EthHdr.MacDst.au16[0] == 0xffff     /* broadcast address. */
             && EthHdr.MacDst.au16[1] == 0xffff
             && EthHdr.MacDst.au16[2] == 0xffff)
        fRc = intnetR0NetworkSendBroadcast(pNetwork, pIfSender, pSG, fTrunkLocked);
    else
        fRc = intnetR0NetworkSendUnicast(pNetwork, pIfSender, fSrc, pSG, fTrunkLocked, &EthHdr);
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
 * @param   pvFrame     Pointer to the frame. Optional, please don't use.
 * @param   cbFrame     Size of the frame. Optional, please don't use.
 */
INTNETR0DECL(int) INTNETR0IfSend(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, const void *pvFrame, unsigned cbFrame)
{
    Log5(("INTNETR0IfSend: pIntNet=%p hIf=%RX32 pvFrame=%p cbFrame=%u\n", pIntNet, hIf, pvFrame, cbFrame));

    /*
     * Validate input and translate the handle.
     */
    AssertReturn(pIntNet, VERR_INVALID_PARAMETER);
    if (pvFrame && cbFrame)
    {
        AssertReturn(cbFrame < 0x8000, VERR_INVALID_PARAMETER);
        AssertPtrReturn(pvFrame, VERR_INVALID_PARAMETER);
        AssertPtrReturn((uint8_t *)pvFrame + cbFrame - 1, VERR_INVALID_PARAMETER);

        /* This is the better place to crash, probe the buffer. */
        ASMProbeReadBuffer(pvFrame, cbFrame);
    }
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
    int rc = RTSemFastMutexRequest(pNetwork->FastMutex);
    if (RT_FAILURE(rc))
    {
        intnetR0IfRelease(pIf, pSession);
        return rc;
    }
    PINTNETTRUNKIF pTrunkIf = intnetR0TrunkIfRetain(pNetwork->pTrunkIF);
    if (pTrunkIf)
    {
        RTSemFastMutexRelease(pIf->pNetwork->FastMutex);

        if (!intnetR0TrunkIfOutLock(pTrunkIf))
        {
            intnetR0TrunkIfRelease(pTrunkIf);
            intnetR0IfRelease(pIf, pSession);
            return VERR_SEM_DESTROYED;
        }

        rc = RTSemFastMutexRequest(pNetwork->FastMutex);
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
     * Process the argument.
     */
    if (pvFrame && cbFrame)
    {
        intnetR0SgInitTemp(&Sg, (void *)pvFrame, cbFrame);
        intnetR0NetworkSend(pNetwork, pIf, 0, &Sg, !!pTrunkIf);
    }

    /*
     * Process the send buffer.
     */
    while (pIf->pIntBuf->Send.offRead != pIf->pIntBuf->Send.offWrite)
    {
        /* Send the frame if the type is sane. */
        PINTNETHDR pHdr = (PINTNETHDR)((uintptr_t)pIf->pIntBuf + pIf->pIntBuf->Send.offRead);
        if (pHdr->u16Type == INTNETHDR_TYPE_FRAME)
        {
            void *pvCurFrame = INTNETHdrGetFramePtr(pHdr, pIf->pIntBuf);
            if (pvCurFrame)
            {
                if (pNetwork->fFlags & INTNET_OPEN_FLAGS_SHARED_MAC_ON_WIRE)
                    intnetR0IfSniffSourceAddr(pIf, (uint8_t *)pvCurFrame, pHdr->cbFrame);
                intnetR0SgInitTemp(&Sg, pvCurFrame, pHdr->cbFrame);
                intnetR0NetworkSend(pNetwork, pIf, 0, &Sg, !!pTrunkIf);
            }
        }
        /* else: ignore the frame */

        /* Skip to the next frame. */
        INTNETRingSkipFrame(pIf->pIntBuf, &pIf->pIntBuf->Send);
    }

    /*
     * Release the semaphore(s) and release references.
     */
    rc = RTSemFastMutexRelease(pNetwork->FastMutex);
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
    return INTNETR0IfSend(pIntNet, pReq->hIf, pSession, NULL, 0);
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
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (RT_SUCCESS(rc))
    {
        *ppRing3Buf = pIf->pIntBufR3;
        rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
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
    int rc = RTSemFastMutexRequest(pIf->pNetwork->FastMutex);
    if (RT_SUCCESS(rc))
    {
        *ppRing0Buf = pIf->pIntBuf;

        rc = RTSemFastMutexRelease(pIf->pNetwork->FastMutex);
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
        rc = RTSemFastMutexRequest(pNetwork->FastMutex);
        if (RT_SUCCESS(rc))
        {
            if (pIf->fPromiscuous != fPromiscuous)
            {
                Log(("INTNETR0IfSetPromiscuousMode: hIf=%RX32: Changed from %d -> %d\n",
                     hIf, !fPromiscuous, !!fPromiscuous));
                ASMAtomicUoWriteBool(&pIf->fPromiscuous, fPromiscuous);
            }

            rc = RTSemFastMutexRelease(pNetwork->FastMutex);
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
INTNETR0DECL(int) INTNETR0IfSetMacAddress(PINTNET pIntNet, INTNETIFHANDLE hIf, PSUPDRVSESSION pSession, PCPDMMAC pMac)
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
        rc = RTSemFastMutexRequest(pNetwork->FastMutex);
        if (RT_SUCCESS(rc))
        {
            if (memcmp(&pIf->Mac, pMac, sizeof(pIf->Mac)))
            {
                Log(("INTNETR0IfSetMacAddress: hIf=%RX32: Changed from %.6Rhxs -> %.6Rhxs\n",
                     hIf, &pIf->Mac, pMac));
                pIf->Mac = *pMac;
                pIf->fMacSet = true;
            }

            rc = RTSemFastMutexRelease(pNetwork->FastMutex);
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

    int rc = RTSemFastMutexRequest(pNetwork->FastMutex); AssertRC(rc);
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
                int rc2 = RTSemFastMutexRelease(pNetwork->FastMutex); AssertRC(rc2);
                fNetworkLocked = false;

                if (pTrunkIf->pIfPort)
                    pTrunkIf->pIfPort->pfnSetActive(pTrunkIf->pIfPort, fActive);
            }
        }

        if (fNetworkLocked)
            RTSemFastMutexRelease(pNetwork->FastMutex);
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
            intnetR0IfRelease(pIf, pSession);
            if (pIf->hIf != hIf)
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
    PINTNETIF pIf = (PINTNETIF)pvUser1;
    PINTNET   pIntNet = (PINTNET)pvUser2;
    Log(("intnetR0IfDestruct: pvObj=%p pIf=%p pIntNet=%p hIf=%RX32\n", pvObj, pIf, pIntNet, pIf->hIf));

    RTSemFastMutexRequest(pIntNet->FastMutex);

    /*
     * Mark the interface as being destroyed so the waiter
     * can behave appropriately (theoretical case).
     */
    ASMAtomicWriteBool(&pIf->fDestroying, true);

    /*
     * Delete the interface handle so the object no longer can be used.
     * (Can happen if the client didn't close its session.)
     */
    INTNETIFHANDLE hIf = ASMAtomicXchgU32(&pIf->hIf, INTNET_HANDLE_INVALID);
    if (hIf != INTNET_HANDLE_INVALID)
    {
        void *pvObj2 = RTHandleTableFreeWithCtx(pIntNet->hHtIfs, hIf, pIf->pSession);
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

        if (pNetwork->pIFs == pIf)
            pNetwork->pIFs = pIf->pNext;
        else
        {
            PINTNETIF pPrev = pNetwork->pIFs;
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

        /*
         * Release our reference to the network.
         */
        RTSemFastMutexRelease(pIntNet->FastMutex);

        SUPR0ObjRelease(pNetwork->pvObj, pIf->pSession);
        pIf->pNetwork = NULL;
    }
    else
        RTSemFastMutexRelease(pIntNet->FastMutex);

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
        pIf->pIntBufDefault = NULL;
        pIf->pIntBufDefaultR3 = 0;
        pIf->pIntBuf = NULL;
        pIf->pIntBufR3 = 0;
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
 * The call must have opened the network for the new interface
 * and is responsible for closing it on failure. On success
 * it must leave the network opened so the interface destructor
 * can close it.
 *
 * @returns VBox status code.
 * @param   pNetwork    The network.
 * @param   pSession    The session handle.
 * @param   cbSend      The size of the send buffer.
 * @param   cbRecv      The size of the receive buffer.
 * @param   phIf        Where to store the interface handle.
 */
static int intnetR0NetworkCreateIf(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession, unsigned cbSend, unsigned cbRecv, bool *pfCloseNetwork, PINTNETIFHANDLE phIf)
{
    LogFlow(("intnetR0NetworkCreateIf: pNetwork=%p pSession=%p cbSend=%u cbRecv=%u phIf=%p\n",
             pNetwork, pSession, cbSend, cbRecv, phIf));

    /*
     * Assert input.
     */
    AssertPtr(pNetwork);
    AssertPtr(phIf);
    AssertPtr(pfCloseNetwork);
    *pfCloseNetwork = false;

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
    int rc = RTSemEventCreate((PRTSEMEVENT)&pIf->Event);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the default buffer.
         */
        /** @todo adjust with minimums and apply defaults here. */
        cbRecv = RT_ALIGN(RT_MAX(cbRecv, sizeof(INTNETHDR) * 4), sizeof(INTNETHDR));
        cbSend = RT_ALIGN(RT_MAX(cbSend, sizeof(INTNETHDR) * 4), sizeof(INTNETHDR));
        const unsigned cbBuf = RT_ALIGN(sizeof(*pIf->pIntBuf), sizeof(INTNETHDR)) + cbRecv + cbSend;
        rc = SUPR0MemAlloc(pIf->pSession, cbBuf, (PRTR0PTR)&pIf->pIntBufDefault, (PRTR3PTR)&pIf->pIntBufDefaultR3);
        if (RT_SUCCESS(rc))
        {
            ASMMemZero32(pIf->pIntBufDefault, cbBuf); /** @todo I thought I specified these buggers as clearing the memory... */

            pIf->pIntBuf = pIf->pIntBufDefault;
            pIf->pIntBufR3 = pIf->pIntBufDefaultR3;
            pIf->pIntBuf->cbBuf = cbBuf;
            pIf->pIntBuf->cbRecv = cbRecv;
            pIf->pIntBuf->cbSend = cbSend;
            /* receive ring buffer. */
            pIf->pIntBuf->Recv.offStart = RT_ALIGN_32(sizeof(*pIf->pIntBuf), sizeof(INTNETHDR));
            pIf->pIntBuf->Recv.offRead  = pIf->pIntBuf->Recv.offStart;
            pIf->pIntBuf->Recv.offWrite = pIf->pIntBuf->Recv.offStart;
            pIf->pIntBuf->Recv.offEnd   = pIf->pIntBuf->Recv.offStart + cbRecv;
            /* send ring buffer. */
            pIf->pIntBuf->Send.offStart = pIf->pIntBuf->Recv.offEnd;
            pIf->pIntBuf->Send.offRead  = pIf->pIntBuf->Send.offStart;
            pIf->pIntBuf->Send.offWrite = pIf->pIntBuf->Send.offStart;
            pIf->pIntBuf->Send.offEnd   = pIf->pIntBuf->Send.offStart + cbSend;

            /*
             * Link the interface to the network.
             */
            rc = RTSemFastMutexRequest(pNetwork->FastMutex);
            if (RT_SUCCESS(rc))
            {
                pIf->pNext = pNetwork->pIFs;
                pNetwork->pIFs = pIf;
                RTSemFastMutexRelease(pNetwork->FastMutex);

                /*
                 * Register the interface with the session.
                 */
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

                RTSemFastMutexDestroy(pNetwork->FastMutex);
                pNetwork->FastMutex = NIL_RTSEMFASTMUTEX;
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
    *pfCloseNetwork = true;
    return rc;
}





/** @copydoc INTNETTRUNKSWPORT::pfnSetSGPhys */
static DECLCALLBACK(bool) intnetR0TrunkIfPortSetSGPhys(PINTNETTRUNKSWPORT pSwitchPort, bool fEnable)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    AssertMsgFailed(("Not implemented because it wasn't required on Darwin\n"));
    return ASMAtomicXchgBool(&pThis->fPhysSG, fEnable);
}


/** @copydoc INTNETTRUNKSWPORT::pfnRecv */
static DECLCALLBACK(bool) intnetR0TrunkIfPortRecv(PINTNETTRUNKSWPORT pSwitchPort, PINTNETSG pSG, uint32_t fSrc)
{
    PINTNETTRUNKIF pThis = INTNET_SWITCHPORT_2_TRUNKIF(pSwitchPort);
    PINTNETNETWORK pNetwork = pThis->pNetwork;

    /* assert some sanity */
    AssertPtrReturn(pNetwork, false);
    AssertReturn(pNetwork->FastMutex != NIL_RTSEMFASTMUTEX, false);
    AssertPtr(pSG);
    Assert(fSrc);

    /*
     * Lock the network and send the frame to it.
     */
    int rc = RTSemFastMutexRequest(pNetwork->FastMutex);
    AssertRCReturn(rc, false);

    bool fRc;
    if (RT_LIKELY(pNetwork->cActiveIFs > 0))
        fRc = intnetR0NetworkSend(pNetwork, NULL, fSrc, pSG, false /* fTrunkLocked */);
    else
        fRc = false; /* don't drop it */

    rc = RTSemFastMutexRelease(pNetwork->FastMutex);
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
    AssertReturnVoid(pNetwork->FastMutex != NIL_RTSEMFASTMUTEX);
    AssertPtr(pSG);
    Assert(pSG->cUsers > 0);

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
    AssertReturnVoid(pNetwork->FastMutex != NIL_RTSEMFASTMUTEX);
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
    RTSEMFASTMUTEX FastMutex = pThis->FastMutex;
    pThis->FastMutex = NIL_RTSEMFASTMUTEX;
    pThis->pNetwork = NULL;
    RTSemFastMutexRelease(FastMutex);
    RTSemFastMutexDestroy(FastMutex);
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
        case kIntNetTrunkType_NetTap:
            pszName = "VBoxNetTap";
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
    pTrunkIF->SwitchPort.u32Version     = INTNETTRUNKSWPORT_VERSION;
    pTrunkIF->SwitchPort.pfnSetSGPhys   = intnetR0TrunkIfPortSetSGPhys;
    pTrunkIF->SwitchPort.pfnRecv        = intnetR0TrunkIfPortRecv;
    pTrunkIF->SwitchPort.pfnSGRetain    = intnetR0TrunkIfPortSGRetain;
    pTrunkIF->SwitchPort.pfnSGRelease   = intnetR0TrunkIfPortSGRelease;
    pTrunkIF->SwitchPort.u32VersionEnd  = INTNETTRUNKSWPORT_VERSION;
    //pTrunkIF->pIfPort = NULL;
    pTrunkIF->pNetwork = pNetwork;
    //pTrunkIF->fPhysSG = false;
    //pTrunkIF->fPromiscuousWire = false;
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
            rc = pTrunkFactory->pfnCreateAndConnect(pTrunkFactory, pNetwork->szTrunk, &pTrunkIF->SwitchPort, &pTrunkIF->pIfPort);
            pTrunkFactory->pfnRelease(pTrunkFactory);
            if (RT_SUCCESS(rc))
            {
                Assert(pTrunkIF->pIfPort);
                pNetwork->pTrunkIF = pTrunkIF;
                LogFlow(("intnetR0NetworkCreateTrunkIf: VINF_SUCCESS - pszName=%s szTrunk=%s Network=%s\n",
                         rc, pszName, pNetwork->szTrunk, pNetwork->szName));
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
 * Close a network which was opened/created using intnetR0OpenNetwork()/intnetR0CreateNetwork().
 *
 * @param   pNetwork    The network to close.
 * @param   pSession    The session handle.
 */
static int intnetR0NetworkClose(PINTNETNETWORK pNetwork, PSUPDRVSESSION pSession)
{
    LogFlow(("intnetR0NetworkClose: pNetwork=%p pSession=%p\n", pNetwork, pSession));
    AssertPtrReturn(pSession, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pNetwork, VERR_INVALID_PARAMETER);

    int rc = SUPR0ObjRelease(pNetwork->pvObj, pSession);
    LogFlow(("intnetR0NetworkClose: return %Rrc\n", rc));
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
    RTSemFastMutexRequest(pIntNet->FastMutex);

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
     * Because of the undefined order of the per session object dereferencing when closing a session,
     * we have to handle the case where the network is destroyed before the interfaces. We'll
     * deal with this by simply orphaning the interfaces.
     */
    RTSemFastMutexRequest(pNetwork->FastMutex);

    PINTNETIF pCur = pNetwork->pIFs;
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

    RTSemFastMutexRelease(pNetwork->FastMutex);

    /*
     * If there is a trunk, delete it.
     * Note that this may tak a while if we're unlucky...
     */
    if (pTrunkIF)
        intnetR0TrunkIfDestroy(pTrunkIF, pNetwork);

    /*
     * Free resources.
     */
    RTSemFastMutexDestroy(pNetwork->FastMutex);
    pNetwork->FastMutex = NIL_RTSEMFASTMUTEX;
    RTMemFree(pNetwork);

    /* release the create/destroy sem. (can be done before trunk destruction.) */
    RTSemFastMutexRelease(pIntNet->FastMutex);
}


/**
 * Opens an existing network.
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
    uint8_t cchName = strlen(pszNetwork);
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
 * The call must own the INTNET::FastMutex and has already attempted
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
 * @param   ppNetwork       Where to store the network. In the case of failure whatever is returned
 *                          here should be dereferenced outside the INTNET::FastMutex.
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
    PINTNETNETWORK pNew = (PINTNETNETWORK)RTMemAllocZ(sizeof(*pNew));
    if (!pNew)
        return VERR_NO_MEMORY;
    int rc = RTSemFastMutexCreate(&pNew->FastMutex);
    if (RT_SUCCESS(rc))
    {
        //pNew->pIFs = NULL;
        pNew->pIntNet = pIntNet;
        //pNew->cActiveIFs = 0;
        pNew->fFlags = fFlags;
        size_t cchName = strlen(pszNetwork);
        pNew->cchName = cchName;
        Assert(cchName && cchName < sizeof(pNew->szName));  /* caller's responsibility. */
        memcpy(pNew->szName, pszNetwork, cchName);          /* '\0' by alloc. */
        pNew->enmTrunkType = enmTrunkType;
        Assert(strlen(pszTrunk) < sizeof(pNew->szTrunk));   /* caller's responsibility. */
        strcpy(pNew->szTrunk, pszTrunk);

        /*
         * Register the object in the current session and link it into the network list.
         */
        pNew->pvObj = SUPR0ObjRegister(pSession, SUPDRVOBJTYPE_INTERNAL_NETWORK, intnetR0NetworkDestruct, pNew, pIntNet);
        if (pNew->pvObj)
        {
            pNew->pNext = pIntNet->pNetworks;
            pIntNet->pNetworks = pNew;

            /*
             * Check if the current session is actually allowed to create and open
             * the network. It is possible to implement network name based policies
             * and these must be checked now. SUPR0ObjRegister does no such checks.
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

            /*
             * We unlink it here so it cannot be opened when the caller leaves
             * INTNET::FastMutex before dereferencing it.
             */
            Assert(pIntNet->pNetworks == pNew);
            pIntNet->pNetworks = pNew->pNext;
            pNew->pNext = NULL;

            *ppNetwork = pNew;
            LogFlow(("intnetR0CreateNetwork: returns %Rrc\n", rc));
            return rc;
        }
        rc = VERR_NO_MEMORY;

        RTSemFastMutexDestroy(pNew->FastMutex);
        pNew->FastMutex = NIL_RTSEMFASTMUTEX;
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
             pIntNet, pSession, pszNetwork, pszNetwork, pszTrunk, pszTrunk, enmTrunkType, fFlags, cbSend, cbRecv, phIf));

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
            AssertReturn(!*pszTrunk, VERR_INVALID_PARAMETER);
            break;

        case kIntNetTrunkType_NetFlt:
            AssertReturn(pszTrunk, VERR_INVALID_PARAMETER);
            break;

        default:
            return VERR_NOT_IMPLEMENTED;
    }

    AssertMsgReturn(!(fFlags & ~INTNET_OPEN_FLAGS_MASK), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phIf, VERR_INVALID_PARAMETER);

    /*
     * Acquire the mutex to serialize open/create.
     */
    int rc = RTSemFastMutexRequest(pIntNet->FastMutex);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Try open / create the network and create an interface on it for the caller to use.
     *
     * Note that because of the destructors grabbing INTNET::FastMutex and us being required
     * to own this semaphore for the entire network opening / creation and interface creation
     * sequence, intnetR0CreateNetwork will have to defer the network cleanup to us on failure.
     */
    PINTNETNETWORK pNetwork = NULL;
    rc = intnetR0OpenNetwork(pIntNet, pSession, pszNetwork, enmTrunkType, pszTrunk, fFlags, &pNetwork);
    if (RT_SUCCESS(rc) || rc == VERR_NOT_FOUND)
    {
        bool fCloseNetwork = true;
        if (rc == VERR_NOT_FOUND)
            rc = intnetR0CreateNetwork(pIntNet, pSession, pszNetwork, enmTrunkType, pszTrunk, fFlags, &pNetwork);
        if (RT_SUCCESS(rc))
            rc = intnetR0NetworkCreateIf(pNetwork, pSession, cbSend, cbRecv, &fCloseNetwork, phIf);

        RTSemFastMutexRelease(pIntNet->FastMutex);

        if (RT_FAILURE(rc) && pNetwork && fCloseNetwork)
            intnetR0NetworkClose(pNetwork, pSession);
    }
    else
        RTSemFastMutexRelease(pIntNet->FastMutex);

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
    if (pIntNet->FastMutex != NIL_RTSEMFASTMUTEX)
    {
        RTSemFastMutexDestroy(pIntNet->FastMutex);
        pIntNet->FastMutex = NIL_RTSEMFASTMUTEX;
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
        //pIntNet->pNetworks              = NULL;

        rc = RTSemFastMutexCreate(&pIntNet->FastMutex);
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

            RTSemFastMutexDestroy(pIntNet->FastMutex);
        }
        RTMemFree(pIntNet);
    }
    *ppIntNet = NULL;
    LogFlow(("INTNETR0Create: returns %Rrc\n", rc));
    return rc;
}

