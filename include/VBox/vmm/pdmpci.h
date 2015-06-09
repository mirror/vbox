/** @file
 * PDM - Pluggable Device Manager, raw PCI Devices. (VMM)
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
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

#ifndef ___VBox_vmm_pdmpci_h
#define ___VBox_vmm_pdmpci_h

#include <VBox/types.h>
#include <VBox/rawpci.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_pcidev    The raw PCI Devices API
 * @ingroup grp_pdm
 * @{
 */

typedef struct PDMIPCIRAW *PPDMIPCIRAW;
typedef struct PDMIPCIRAW
{
    /**
     * Notify virtual device that interrupt has arrived.
     * For this callback to be called, interface have to be
     * registered with PDMIPCIRAWUP::pfnRegisterInterruptListener.
     *
     * @note   no level parameter, as we can only support flip-flop.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   iGuestIrq           Guest interrupt number, passed earlier when registering listener.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnInterruptRequest ,(PPDMIPCIRAW pInterface, int32_t iGuestIrq));
} PDMIPCIRAW;

typedef struct PDMIPCIRAWUP *PPDMIPCIRAWUP;
typedef struct PDMIPCIRAWUP
{
    /**
     * Host PCI MMIO access function.
     */

    /**
     * Request driver info about PCI region on host PCI device.
     *
     * @returns true, if region is present, and out parameters are correct
     * @param   pInterface          Pointer to this interface structure.
     * @param   iRegion             Region number.
     * @param   pRegStart           Where to store region base address (guest).
     * @param   piRegSize           Where to store region size.
     *
     * @param   fMmio               If region is MMIO or IO.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnGetRegionInfo, (PPDMIPCIRAWUP pInterface,
                                                  int32_t              iRegion,
                                                  RTGCPHYS             *pRegStart,
                                                  uint64_t             *piRegSize,
                                                  uint32_t             *pfFlags
                                                  ));

    /**
     * Request driver to map part of host device's MMIO region to the VM process and maybe kernel.
     * Shall only be issued within earlier obtained with pfnGetRegionInfo()
     * host physical address ranges for the device BARs. Even if failed, device still may function
     * using pfnMmio* and pfnPio* operations, just much slower.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iRegion             Number of the region.
     * @param   StartAddress        Host physical address of start.
     * @param   iRegionSize         Size of the region.
     * @param   fFlags              Flags, currently lowest significant bit set if R0 mapping requested too
     * @param   ppvAddressR3        Where to store mapped region address for R3 (can be 0, if cannot map into userland)
     * @param   ppvAddressR0        Where to store mapped region address for R0 (can be 0, if cannot map into kernel)
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMapRegion, (PPDMIPCIRAWUP pInterface,
                                             int32_t              iRegion,
                                             RTHCPHYS             StartAddress,
                                             uint64_t             iRegionSize,
                                             uint32_t             fFlags,
                                             RTR3PTR              *ppvAddressR3,
                                             RTR0PTR              *ppvAddressR0
                                             ));

    /**
     * Request driver to unmap part of host device's MMIO region to the VM process.
     * Shall only be issued with pointer earlier obtained with pfnMapRegion().
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure
     * @param   iRegion             Number of the region.
     * @param   StartAddress        Host physical address of start.
     * @param   iRegionSize         Size of the region.
     * @param   pvAddressR3         R3 address of mapped region.
     * @param   pvAddressR0         R0 address of mapped region.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnmapRegion, (PPDMIPCIRAWUP pInterface,
                                               int                  iRegion,
                                               RTHCPHYS             StartAddress,
                                               uint64_t             iRegionSize,
                                               RTR3PTR              pvAddressR3,
                                               RTR0PTR              pvAddressR0
                                               ));

    /**
     * Request port IO write.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iPort               IO port.
     * @param   iValue              Value to write.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPioWrite, (PPDMIPCIRAWUP pInterface,
                                            uint16_t             iPort,
                                            uint32_t             iValue,
                                            unsigned             cb
                                            ));

    /**
     * Request port IO read.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iPort               IO port.
     * @param   piValue             Place to store read value.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */

    DECLR3CALLBACKMEMBER(int, pfnPioRead, (PPDMIPCIRAWUP pInterface,
                                           uint16_t             iPort,
                                           uint32_t             *piValue,
                                           unsigned             cb
                                           ));


    /**
     * Request MMIO write. This callback is only called if driver wants to receive MMIO via
     * pu32Flags argument of pfnPciDeviceConstructStart().
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   Address             Guest physical address.
     * @param   pValue              Address of value to write.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMmioWrite, (PPDMIPCIRAWUP pInterface,
                                             RTR0PTR              Address,
                                             void const          *pValue,
                                             unsigned             cb
                                             ));

    /**
     * Request MMIO read.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   Address             Guest physical address.
     * @param   pValue              Place to store read value.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */

    DECLR3CALLBACKMEMBER(int, pfnMmioRead, (PPDMIPCIRAWUP pInterface,
                                            RTR0PTR              Address,
                                            void                 *pValue,
                                            unsigned             cb
                                            ));

    /**
     * Host PCI config space accessors.
     */
    /**
     * Request driver to write value to host device's PCI config space.
     * Host specific way (PIO or MCFG) is used to perform actual operation.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iOffset             Offset in PCI config space.
     * @param   iValue              Value to write.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPciCfgWrite, (PPDMIPCIRAWUP pInterface,
                                               uint32_t             iOffset,
                                               void*                pValue,
                                               unsigned             cb
                                               ));
     /**
     * Request driver to read value from host device's PCI config space.
     * Host specific way (PIO or MCFG) is used to perform actual operation.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iOffset             Offset in PCI config space.
     * @param   pValue              Where to store read value.
     * @param   cb                  Access width.
     *
     * @thread  EMT thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPciCfgRead, (PPDMIPCIRAWUP pInterface,
                                              uint32_t             iOffset,
                                              void                *pValue,
                                              unsigned             cb                                              ));

    /**
     * Request to enable interrupt notifications. Please note that this is purely
     * R3 interface, so it's up to implementor to perform necessary machinery
     * for communications with host OS kernel driver. Typical implementation will start
     * userland thread waiting on shared semaphore (such as using SUPSEMEVENT),
     * notified by the kernel interrupt handler, and then will call
     * upper port pfnInterruptRequest() based on data provided by the driver.
     * This apporach is taken, as calling VBox code from an asyncronous R0
     * interrupt handler when VMM may not be even running doesn't look
     * like a good idea.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   pListener           Pointer to the listener object.
     * @param   iGuestIrq           Guest IRQ to be passed to pfnInterruptRequest().
     *
     * @thread  Any thread, pfnInterruptRequest() will be usually invoked on a dedicated thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnEnableInterruptNotifications, (PPDMIPCIRAWUP pInterface, int32_t iGuestIrq
                                                                ));

    /**
     * Request to disable interrupt notifications.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisableInterruptNotifications, (PPDMIPCIRAWUP pInterface
                                                                 ));

    /**
     * Notification APIs.
     */

    /**
     * Notify driver when raw PCI device construction starts. Have to be the first operation
     * as initializes internal state and opens host device driver.
     *
     * @returns status code
     * @param   pInterface          Pointer to this interface structure.
     * @param   iHostAddress        Host PCI address of device attached.
     * @param   iGuestAddress       Guest PCI address of device attached.
     * @param   szDeviceName        Human readable device name.
     * @param   fDeviceFlags        Flags for the host device.
     * @param   pu32Flags           Flags for virtual device, from the upper driver.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPciDeviceConstructStart, (PPDMIPCIRAWUP  pInterface,
                                                           uint32_t             iHostAddress,
                                                           uint32_t             iGuestAddress,
                                                           const char*          szDeviceName,
                                                           uint32_t             fDeviceFlags,
                                                           uint32_t             *pu32Flags));

    /**
     * Notify driver when raw PCI device construction completes, so that it may
     * perform further actions depending on success or failure of this operation.
     * Standard action is to raise global IHostPciDevicePlugEvent.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   rc                  Result code of the operation.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnPciDeviceConstructComplete, (PPDMIPCIRAWUP pInterface,
                                                               int                  rc));

    /**
     * Notify driver on finalization of raw PCI device.
     *
     * @param   pInterface          Pointer to this interface structure.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPciDeviceDestruct, (PPDMIPCIRAWUP pInterface,
                                                     uint32_t             fFlags));

    /**
     * Notify driver on guest power state change.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   aState              New power state.
     * @param   pu64Param           State-specific in/out parameter. For now only used during power-on to provide VM caps.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPciDevicePowerStateChange, (PPDMIPCIRAWUP pInterface,
                                                             PCIRAWPOWERSTATE     aState,
                                                             uint64_t             *pu64Param));

     /**
     * Notify driver about runtime error.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   fFatal              If error is fatal.
     * @param   szErrorId           Error ID.
     * @param   szMessage           Error message.
     *
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReportRuntimeError, (PPDMIPCIRAWUP pInterface,
                                                      uint8_t              fFatal,
                                                      const char*          szErrorId,
                                                      const char*          szMessage));
} PDMIPCIRAWUP;

/**
 * Init R0 PCI module.
 */
PCIRAWR0DECL(int)  PciRawR0Init(void);
/**
 * Process request (in R0).
 */
PCIRAWR0DECL(int)  PciRawR0ProcessReq(PSUPDRVSESSION pSession, PVM pVM, PPCIRAWSENDREQ pReq);
/**
 * Terminate R0 PCI module.
 */
PCIRAWR0DECL(void) PciRawR0Term(void);

/**
 * Per-VM R0 module init.
 */
PCIRAWR0DECL(int)  PciRawR0InitVM(PVM pVM);

/**
 * Per-VM R0 module termination routine.
 */
PCIRAWR0DECL(void)  PciRawR0TermVM(PVM pVM);

/**
 * Flags returned by pfnPciDeviceConstructStart(), to notify device
 * how it shall handle device IO traffic.
 */
typedef enum PCIRAWDEVICEFLAGS
{
    /** Intercept port IO (R3 PIO always go to the driver). */
    PCIRAWRFLAG_CAPTURE_PIO   =  (1 << 0),
    /** Intercept MMIO. */
    PCIRAWRFLAG_CAPTURE_MMIO  =  (1 << 1),
    /** Allow bus mastering by physical device (requires IOMMU). */
    PCIRAWRFLAG_ALLOW_BM      =  (1 << 2),
    /** Allow R3 MMIO mapping. */
    PCIRAWRFLAG_ALLOW_R3MAP   =  (1 << 3),

    /** The usual 32-bit type blow up. */
    PCIRAWRFLAG_32BIT_HACK = 0x7fffffff
} PCIRAWDEVICEFLAGS;

#define PDMIPCIRAWUP_IID       "06daa17f-097b-4ebe-a626-15f467b1de12"
#define PDMIPCIRAW_IID         "68c6e4c4-4223-47e0-9134-e3c297992543"

/** @} */

RT_C_DECLS_END

#endif
