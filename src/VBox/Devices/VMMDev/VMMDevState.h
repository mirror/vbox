/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager header
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VMMDevState_h__
#define __VMMDevState_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>

#include <VBox/pdm.h>

#define TIMESYNC_BACKDOOR

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
    PPDMDEVINS pDevIns;
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
    /** Mask of events guest is interested in. Note that the HGCM events 
     *  are enabled automatically by the VMMDev device when guest issues
     *  HGCM commands.
     */
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

    /** Information reported by guest via VMMDevReportGuestCapabilities
     */
    uint32_t      guestCaps;

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
        uint32_t display;
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

    /* memory balloon change request */
    uint32_t    u32MemoryBalloonSize, u32LastMemoryBalloonSize;

    /* guest ram size */
    uint64_t    u64GuestRAMSize;

    /* statistics interval change request */
    uint32_t    u32StatIntervalSize, u32LastStatIntervalSize;

    /* seamless mode change request */
    bool fLastSeamlessEnabled, fSeamlessEnabled;

    bool fVRDPEnabled;
    uint32_t u32VRDPExperienceLevel;
        
#ifdef TIMESYNC_BACKDOOR
    bool fTimesyncBackdoorLo;
    uint64_t hostTime;
#endif
    /** Set if GetHostTime should fail. 
     * Loaded from the GetHostTimeDisabled configuration value. */
    bool fGetHostTimeDisabled;

    /** Set if backdoor logging should be disabled (output will be ignored then) */
    bool fBackdoorLogDisabled;

#ifdef VBOX_HGCM
    /** List of pending HGCM requests, used for saving the HGCM state. */
    PVBOXHGCMCMD pHGCMCmdList;
    /** Critical section to protect the list. */
    RTCRITSECT critsectHGCMCmdList;
    /** Whether the HGCM events are already automatically enabled. */
    uint32_t u32HGCMEnabled;
#endif /* VBOX_HGCM */

    /* Shared folders LED */
    struct
    {
        /** The LED. */
        PDMLED                              Led;
        /** The LED ports. */
        PDMILEDPORTS                        ILeds;
        /** Partner of ILeds. */
        R3PTRTYPE(PPDMILEDCONNECTORS)       pLedsConnector;
    } SharedFolders;

} VMMDevState;

void VMMDevNotifyGuest (VMMDevState *pVMMDevState, uint32_t u32EventMask);
void VMMDevCtlSetGuestFilterMask (VMMDevState *pVMMDevState,
                                  uint32_t u32OrMask,
                                  uint32_t u32NotMask);

#endif /* __VMMDevState_h__ */
