/** @file
 * PDM - Pluggable Device Manager, raw PCI Devices. (VMM)
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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

#ifndef ___VBox_rawpci_h
#define ___VBox_rawpci_h

#include <iprt/types.h>


RT_C_DECLS_BEGIN

/** Parameters buffer for PCIRAWR0_DO_GET_REGION_INFO call */
typedef struct
{
    /* in */
    int32_t  iRegion;
    /* out */
    RTHCPHYS RegionStart;
    uint64_t u64RegionSize;
    bool     fPresent;
    bool     fMmio;
} PCIRAWREQGETREGIONINFO;

/** Parameters buffer for PCIRAWR0_DO_MAP_REGION call. */
typedef struct
{
    /* in */
    RTHCPHYS             StartAddress;
    uint64_t             iRegionSize;
    uint32_t             fFlags;
    /* out */
    RTR3PTR              pvAddressR3;
    RTR0PTR              pvAddressR0;
} PCIRAWREQMAPREGION;

/** Parameters buffer for PCIRAWR0_DO_UNMAP_REGION call. */
typedef struct
{
    /* in */
    RTR3PTR              pvAddressR3;
    RTR0PTR              pvAddressR0;
} PCIRAWREQUNMAPREGION;

/** Parameters buffer for PCIRAWR0_DO_PIO_WRITE call. */
typedef struct
{
    /* in */
    uint16_t             iPort;
    uint16_t             cb;
    uint32_t             iValue;
} PCIRAWREQPIOWRITE;

/** Parameters buffer for PCIRAWR0_DO_PIO_READ call. */
typedef struct
{
    /* in */
    uint16_t             iPort;
    uint16_t             cb;
    /* out */
    uint32_t             iValue;
} PCIRAWREQPIOREAD;

/** Memory operand. */
typedef struct
{
    union
    {
        uint8_t          u8;
        uint16_t         u16;
        uint32_t         u32;
        uint64_t         u64;
    } u;
    uint8_t cb;
} PCIRAWMEMLOC;

/** Parameters buffer for PCIRAWR0_DO_MMIO_WRITE call. */
typedef struct
{
    /* in */
    RTGCPHYS             Address;
    PCIRAWMEMLOC         Value;
} PCIRAWREQMMIOWRITE;

/** Parameters buffer for PCIRAWR0_DO_MMIO_READ call. */
typedef struct
{
    /* in */
    RTGCPHYS             Address;
    /* inout (Value.cb is in) */
    PCIRAWMEMLOC         Value;
} PCIRAWREQMMIOREAD;

/* Parameters buffer for PCIRAWR0_DO_PCICFG_WRITE call. */
typedef struct
{
    /* in */
    uint32_t             iOffset;
    PCIRAWMEMLOC         Value;
} PCIRAWREQPCICFGWRITE;

/** Parameters buffer for PCIRAWR0_DO_PCICFG_READ call. */
typedef struct
{
    /* in */
    uint32_t             iOffset;
    /* inout (Value.cb is in) */
    PCIRAWMEMLOC         Value;
} PCIRAWREQPCICFGREAD;

/**
 * Request buffer use for communication with the driver.
 */
typedef struct PCIRAWSENDREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** Alternative to passing the taking the session from the VM handle.
     *  Either use this member or use the VM handle, don't do both.
     */
    PSUPDRVSESSION  pSession;
    /** Request type. */
    int32_t         iRequest;
    /** Host device request targetted to. */
    uint32_t        TargetDevice;
    /** Call parameters. */
    union
    {
        PCIRAWREQGETREGIONINFO aGetRegionInfo;
        PCIRAWREQMAPREGION     aMapRegion;
        PCIRAWREQUNMAPREGION   aUnmapRegion;
        PCIRAWREQPIOWRITE      aPioWrite;
        PCIRAWREQPIOREAD       aPioRead;
        PCIRAWREQMMIOWRITE     aMmioWrite;
        PCIRAWREQMMIOREAD      aMmioRead;
        PCIRAWREQPCICFGWRITE   aPciCfgWrite;
        PCIRAWREQPCICFGREAD    aPciCfgRead;
    } u;
} PCIRAWSENDREQ;
typedef PCIRAWSENDREQ *PPCIRAWSENDREQ;


/**
 * Operations performed by the driver.
 */
typedef enum PCIRAWR0OPERATION
{
    /* Get PCI region info. */
    PCIRAWR0_DO_GET_REGION_INFO,
    /* Map PCI region into VM address space. */
    PCIRAWR0_DO_MAP_REGION,
    /* Unmap PCI region from VM address space. */
    PCIRAWR0_DO_UNMAP_REGION,
    /* Perform PIO write. */
    PCIRAWR0_DO_PIO_WRITE,
    /* Perform PIO read. */
    PCIRAWR0_DO_PIO_READ,
    /* Perform MMIO write. */
    PCIRAWR0_DO_MMIO_WRITE,
    /* Perform MMIO read. */
    PCIRAWR0_DO_MMIO_READ,
    /* Perform PCI config write. */
    PCIRAWR0_DO_PCICFG_WRITE,
    /* Perform PCI config read. */
    PCIRAWR0_DO_PCICFG_READ,
    /** The usual 32-bit type blow up. */
    PCIRAWR0_DO_32BIT_HACK = 0x7fffffff
} PCIRAWR0OPERATION;

RT_C_DECLS_END

#endif
