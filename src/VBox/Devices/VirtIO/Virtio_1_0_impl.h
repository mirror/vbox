/* $Id$ $Revision$ $Date$ $Author$ */
/** @file
 * Virtio_1_0_impl.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h
#define VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/**
* This macro returns true if physical address and access length are within the mapped capability struct.
*
* Actual Parameters:
*     @oaram    pPhysCapStruct - [input]  Pointer to MMIO mapped capability struct
*     @param    pCfgCap        - [input]  Pointer to capability in PCI configuration area
*     @param fMatched       - [output] True if GCPhysAddr is within the physically mapped capability.
*
* Implied parameters:
*     @param    GCPhysAddr     - [input, implied] Physical address accessed (via MMIO callback)
*     @param    cb             - [input, implied] Number of bytes to access
*
*/
#define MATCH_VIRTIO_CAP_STRUCT(pGcPhysCapData, pCfgCap, fMatched) \
        bool fMatched = false; \
        if (pGcPhysCapData && pCfgCap && GCPhysAddr >= (RTGCPHYS)pGcPhysCapData \
            && GCPhysAddr < ((RTGCPHYS)pGcPhysCapData + ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
            && cb <= ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
                fMatched = true;

/**
 * This macro resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if it is a 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * This is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @param   uOffset  - Implied parameter: Offset into VIRTIO_PCI_COMMON_CFG_T
 * @param   cb       - Implied parameter: Number of bytes to access
 * @result           - true or false
 */
#define COMMON_CFG(member) \
        (RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member) == 64 \
         && (   uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
             || uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) + sizeof(uint32_t)) \
         && cb == sizeof(uint32_t)) \
     || (uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
           && cb == RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member))

#define LOG_ACCESSOR(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, pv, cb, uIntraOff, fWrite, false, 0);

#define LOG_INDEXED_ACCESSOR(member, idx) \
        virtioLogMappedIoValue(__FUNCTION__, #member, pv, cb, uIntraOff, fWrite, true, idx);

#define ACCESSOR(member) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pVirtio->member) + uOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uOffset), cb); \
        LOG_ACCESSOR(member); \
    }

#define ACCESSOR_WITH_IDX(member, idx) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)(pVirtio->member + idx)) + uOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)(pVirtio->member + idx)) + uOffset), cb); \
        LOG_INDEXED_ACCESSOR(member, idx); \
    }

#define ACCESSOR_READONLY(member) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uOffset), cb); \
            LOG_ACCESSOR(member); \
        } \
    }

#define ACCESSOR_READONLY_WITH_IDX(member, idx) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
        else \
        { \
            memcpy((char *)pv, ((char *)(pVirtio->member + idx)) + uOffset, cb); \
            LOG_INDEXED_ACCESSOR(member, idx); \
        } \
    }

#ifdef VBOX_DEVICE_STRUCT_TESTCASE
#  define virtioDumpState(x, s)  do {} while (0)
#else
#  ifdef DEBUG
        static void virtioDumpState(PVIRTIOSTATE pVirtio, const char *pcszCaller)
        {
            RT_NOREF2(pVirtio, pcszCaller);
            /* PK TODO, dump state features, selector, status, ISR, queue info (iterate),
                        descriptors, avail, used, size, indices, address
                        each by variable name on new line, indented slightly */
        }
#  endif
#endif

DECLINLINE(void) virtioLogDeviceStatus( uint8_t status)
{
    if (status == 0)
        Log(("RESET"));
    else
    {
        int primed = 0;
        if (status & VIRTIO_STATUS_ACKNOWLEDGE)
            Log(("ACKNOWLEDGE",   primed++));
        if (status & VIRTIO_STATUS_DRIVER)
            Log(("%sDRIVER",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DRIVER_OK)
            Log(("%sDRIVER_OK",   primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FEATURES_OK)
            Log(("%sFEATURES_OK", primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FAILED)
            Log(("%sFAILED",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DEVICE_NEEDS_RESET)
            Log(("%sACKNOWLEDGE", primed++ ? " | " : ""));
    }
}

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h */
