/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager header
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VMMDevState_h__
#define __VMMDevState_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>

#include <VBox/pdm.h>

/** device structure containing all state information */
typedef struct VMMDevState
{
    /** The PCI device structure. */
    PCIDevice dev;

    /** hypervisor address space size */
    uint32_t hypervisorSize;

    /** bit 0: guest capability (1 == wants), bit 1: flag value has changed */
    /** bit 2: host capability (1 == wants), bit 3: flag value has changed */
    uint32_t mouseCapabilities;
    /** absolute mouse position in pixels */
    uint32_t mouseXAbs;
    uint32_t mouseYAbs;

    /** Pointer to device instance. */
    PPDMDEVINS          pDevIns;
    /** VMMDev port base interface. */
    PDMIBASE Base;
    /** VMMDev port interface. */
    PDMIVMMDEVPORT Port;
#ifdef VBOX_HGCM
    /** HGCM port interface. */
    PDMIHGCMPORT HGCMPort;
#endif
    /** Pointer to base interface of the driver. */
    PPDMIBASE pDrvBase;
    /** VMMDev connector interface */
    PPDMIVMMDEVCONNECTOR pDrv;
#ifdef VBOX_HGCM
    /** HGCM connector interface */
    PPDMIHGCMCONNECTOR pHGCMDrv;
#endif
    /** message buffer for backdoor logging. */
    char szMsg[512];
    /** message buffer index. */
    unsigned iMsg;
    /** Base port in the assigned I/O space. */
    RTIOPORT PortBase;

    /** IRQ number assigned to the device */
    uint32_t irq;
    /** Current host side event flags */
    uint32_t u32HostEventFlags;
    /** Mask of events guest is interested in */
    uint32_t u32GuestFilterMask;
    /** Delayed mask of guest events */
    uint32_t u32NewGuestFilterMask;
    /** Flag whether u32NewGuestFilterMask is valid */
    bool fNewGuestFilterMask;

    /** HC pointer to VMMDev RAM area */
    VMMDevMemory *pVMMDevRAMHC;
    /** GC physical address of VMMDev RAM area */
    RTGCPHYS GCPhysVMMDevRAM;

    /** Information reported by guest via VMMDevReportGuestInfo generic request.
     * Until this information is reported the VMMDev refuses any other requests.
     */
    VBoxGuestInfo guestInfo;

    /** "Additions are Ok" indicator, set to true after processing VMMDevReportGuestInfo,
     * if additions version is compatible. This flag is here to avoid repeated comparing
     * of the version in guestInfo.
     */
    uint32_t fu32AdditionsOk;

    /** Video acceleration status set by guest. */
    uint32_t u32VideoAccelEnabled;

    /** resolution change request */
    struct
    {
        uint32_t xres;
        uint32_t yres;
        uint32_t bpp;
    } displayChangeRequest,
      lastReadDisplayChangeRequest;

    /** credentials for guest logon purposes */
    struct
    {
        char szUserName[VMMDEV_CREDENTIALS_STRLEN];
        char szPassword[VMMDEV_CREDENTIALS_STRLEN];
        char szDomain[VMMDEV_CREDENTIALS_STRLEN];
        bool fAllowInteractiveLogon;
    } credentialsLogon;

    /** credentials for verification by guest */
    struct
    {
        char szUserName[VMMDEV_CREDENTIALS_STRLEN];
        char szPassword[VMMDEV_CREDENTIALS_STRLEN];
        char szDomain[VMMDEV_CREDENTIALS_STRLEN];
    } credentialsJudge;

#ifdef TIMESYNC_BACKDOOR
    bool fTimesyncBackdoorLo;
    uint64_t hostTime;
#endif

} VMMDevState;

void VMMDevNotifyGuest (VMMDevState *pVMMDevState, uint32_t u32EventMask);

#endif /* __VMMDevState_h__ */
