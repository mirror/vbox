/* $Id$ */
/** @file
 * VMMDev - Guest <-> VMM/Host communication device, internal header.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VMMDev_VMMDevState_h
#define ___VMMDev_VMMDevState_h

#include <VBox/VMMDev.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmifs.h>

#define VMMDEV_WITH_ALT_TIMESYNC

typedef struct DISPLAYCHANGEINFO
{
    uint32_t xres;
    uint32_t yres;
    uint32_t bpp;
    uint32_t display;
    uint32_t xOrigin;
    uint32_t yOrigin;
    bool fEnabled;
    bool fChangeOrigin;
} DISPLAYCHANGEINFO;

typedef struct DISPLAYCHANGEREQUEST
{
    bool fPending;
    bool afAlignment[3];
    DISPLAYCHANGEINFO displayChangeRequest;
    DISPLAYCHANGEINFO lastReadDisplayChangeRequest;
} DISPLAYCHANGEREQUEST;

typedef struct DISPLAYCHANGEDATA
{
    /* Which monitor is being reported to the guest. */
    int iCurrentMonitor;

    /** true if the guest responded to VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST at least once */
    bool fGuestSentChangeEventAck;
    bool afAlignment[3];

    DISPLAYCHANGEREQUEST aRequests[64]; /// @todo maxMonitors
} DISPLAYCHANGEDATA;


/**
 * Credentials for automatic guest logon and host configured logon (?).
 *
 * This is not stored in the same block as the instance data in order to make it
 * harder to access.
 */
typedef struct VMMDEVCREDS
{
    /** credentials for guest logon purposes */
    struct
    {
        char szUserName[VMMDEV_CREDENTIALS_SZ_SIZE];
        char szPassword[VMMDEV_CREDENTIALS_SZ_SIZE];
        char szDomain[VMMDEV_CREDENTIALS_SZ_SIZE];
        bool fAllowInteractiveLogon;
    } Logon;

    /** credentials for verification by guest */
    struct
    {
        char szUserName[VMMDEV_CREDENTIALS_SZ_SIZE];
        char szPassword[VMMDEV_CREDENTIALS_SZ_SIZE];
        char szDomain[VMMDEV_CREDENTIALS_SZ_SIZE];
    } Judge;
} VMMDEVCREDS;


/**
 * Facility status entry.
 */
typedef struct VMMDEVFACILITYSTATUSENTRY
{
    /** The facility, see VBoxGuestFacilityType. */
    uint32_t    uFacility;
    /** The status, see VBoxGuestFacilityStatus. */
    uint16_t    uStatus;
    /** Whether this entry is fixed and cannot be reused when inactive. */
    bool        fFixed;
    /** Explicit alignment padding / reserved for future use. MBZ. */
    bool        fPadding;
    /** The facility flags (yet to be defined). */
    uint32_t    fFlags;
    /** Explicit alignment padding / reserved for future use. MBZ. */
    uint32_t    uPadding;
    /** Last update timestamp. */
    RTTIMESPEC  TimeSpecTS;
} VMMDEVFACILITYSTATUSENTRY;
/** Pointer to a facility status entry. */
typedef VMMDEVFACILITYSTATUSENTRY *PVMMDEVFACILITYSTATUSENTRY;


/** device structure containing all state information */
typedef struct VMMDevState
{
    /** The PCI device structure. */
    PCIDevice           PciDev;
    /** The critical section for this device.
     * @remarks We use this rather than the default one, it's simpler with all
     *          the driver interfaces where we have to waste time digging out the
     *          PDMDEVINS structure. */
    PDMCRITSECT         CritSect;

    /** hypervisor address space size */
    uint32_t hypervisorSize;

    /** mouse capabilities of host and guest */
    uint32_t mouseCapabilities;
    /** absolute mouse position in pixels */
    int32_t mouseXAbs;
    int32_t mouseYAbs;
    /** Does the guest currently want the host pointer to be shown? */
    uint32_t fHostCursorRequested;

    /** Alignment padding. */
    uint32_t u32Alignment0;

    /** Pointer to device instance. */
    PPDMDEVINSR3 pDevIns;
    /** LUN\#0 + Status: VMMDev port base interface. */
    PDMIBASE IBase;
    /** LUN\#0: VMMDev port interface. */
    PDMIVMMDEVPORT IPort;
#ifdef VBOX_WITH_HGCM
    /** LUN\#0: HGCM port interface. */
    PDMIHGCMPORT IHGCMPort;
#endif
    /** Pointer to base interface of the driver. */
    R3PTRTYPE(PPDMIBASE) pDrvBase;
    /** VMMDev connector interface */
    R3PTRTYPE(PPDMIVMMDEVCONNECTOR) pDrv;
#ifdef VBOX_WITH_HGCM
    /** HGCM connector interface */
    R3PTRTYPE(PPDMIHGCMCONNECTOR) pHGCMDrv;
#endif
    /** Alignment padding. */
    RTR3PTR PtrR3Alignment1;
    /** message buffer for backdoor logging. */
    char szMsg[512];
    /** message buffer index. */
    uint32_t iMsg;
    /** Alignment padding. */
    uint32_t u32Alignment2;

    /** IRQ number assigned to the device */
    uint32_t irq;
    /** Current host side event flags */
    uint32_t u32HostEventFlags;
    /** Mask of events guest is interested in.
     * @note The HGCM events are enabled automatically by the VMMDev device when
     *       guest issues HGCM commands. */
    uint32_t u32GuestFilterMask;
    /** Delayed mask of guest events */
    uint32_t u32NewGuestFilterMask;
    /** Flag whether u32NewGuestFilterMask is valid */
    bool fNewGuestFilterMask;
    /** Alignment padding. */
    bool afAlignment3[3];

    /** GC physical address of VMMDev RAM area */
    RTGCPHYS32 GCPhysVMMDevRAM;
    /** R3 pointer to VMMDev RAM area */
    R3PTRTYPE(VMMDevMemory *) pVMMDevRAMR3;

    /** R3 pointer to VMMDev Heap RAM area
     */
    R3PTRTYPE(VMMDevMemory *) pVMMDevHeapR3;
    /** GC physical address of VMMDev Heap RAM area */
    RTGCPHYS32 GCPhysVMMDevHeap;

    /** Information reported by guest via VMMDevReportGuestInfo generic request.
     * Until this information is reported the VMMDev refuses any other requests.
     */
    VBoxGuestInfo guestInfo;
    /** Information report \#2, chewed a litte. */
    struct
    {
        uint32_t uFullVersion; /**< non-zero if info is present. */
        uint32_t uRevision;
        uint32_t fFeatures;
        char     szName[128];
    } guestInfo2;

    /** Array of guest facility statuses. */
    VMMDEVFACILITYSTATUSENTRY   aFacilityStatuses[32];
    /** The number of valid entries in the facility status array. */
    uint32_t                    cFacilityStatuses;

    /** Information reported by guest via VMMDevReportGuestCapabilities. */
    uint32_t      guestCaps;

    /** "Additions are Ok" indicator, set to true after processing VMMDevReportGuestInfo,
     * if additions version is compatible. This flag is here to avoid repeated comparing
     * of the version in guestInfo.
     */
    uint32_t fu32AdditionsOk;

    /** Video acceleration status set by guest. */
    uint32_t u32VideoAccelEnabled;

    DISPLAYCHANGEDATA displayChangeData;

    /** Pointer to the credentials. */
    R3PTRTYPE(VMMDEVCREDS *) pCredentials;

    bool afAlignment4[HC_ARCH_BITS == 32 ? 3 : 7];

    /* memory balloon change request */
    uint32_t    cMbMemoryBalloon;
    /** The last balloon size queried by the guest additions.  */
    uint32_t    cMbMemoryBalloonLast;

    /* guest ram size */
    uint64_t    cbGuestRAM;

    /* unique session id; the id will be different after each start, reset or restore of the VM. */
    uint64_t    idSession;

    /* statistics interval change request */
    uint32_t    u32StatIntervalSize, u32LastStatIntervalSize;

    /* seamless mode change request */
    bool fLastSeamlessEnabled, fSeamlessEnabled;
    bool afAlignment5[1];

    bool fVRDPEnabled;
    uint32_t uVRDPExperienceLevel;

#ifdef VMMDEV_WITH_ALT_TIMESYNC
    uint64_t hostTime;
    bool fTimesyncBackdoorLo;
    bool afAlignment6[3];
#endif
    /** Set if GetHostTime should fail.
     * Loaded from the GetHostTimeDisabled configuration value. */
    bool fGetHostTimeDisabled;

    /** Set if backdoor logging should be disabled (output will be ignored then) */
    bool fBackdoorLogDisabled;

    /** Don't clear credentials */
    bool fKeepCredentials;

    /** Heap enabled. */
    bool fHeapEnabled;

    /** Guest Core Dumping enabled. */
    bool fGuestCoreDumpEnabled;

    /** Guest Core Dump location. */
    char szGuestCoreDumpDir[RTPATH_MAX];

    /** Number of additional cores to keep around.   */
    uint32_t cGuestCoreDumps;

    bool afAlignment7[1];

#ifdef VBOX_WITH_HGCM
    /** List of pending HGCM requests, used for saving the HGCM state. */
    R3PTRTYPE(PVBOXHGCMCMD) pHGCMCmdList;
    /** Critical section to protect the list. */
    RTCRITSECT critsectHGCMCmdList;
    /** Whether the HGCM events are already automatically enabled. */
    uint32_t u32HGCMEnabled;
    /** Alignment padding. */
    uint32_t u32Alignment7;
#endif /* VBOX_WITH_HGCM */

    /** Status LUN: Shared folders LED */
    struct
    {
        /** The LED. */
        PDMLED                              Led;
        /** The LED ports. */
        PDMILEDPORTS                        ILeds;
        /** Partner of ILeds. */
        R3PTRTYPE(PPDMILEDCONNECTORS)       pLedsConnector;
    } SharedFolders;

    /** FLag whether CPU hotplug events are monitored */
    bool                fCpuHotPlugEventsEnabled;
    /** Alignment padding. */
    bool                afPadding8[3];
    /** CPU hotplug event */
    VMMDevCpuEventType  enmCpuHotPlugEvent;
    /** Core id of the CPU to change */
    uint32_t            idCpuCore;
    /** Package id of the CPU to change */
    uint32_t            idCpuPackage;

    uint32_t            StatMemBalloonChunks;

    /** Set if RC/R0 is enabled. */
    bool                fRZEnabled;
    /** Set if testing is enabled. */
    bool                fTestingEnabled;
    /** Alignment padding. */
    bool                afPadding9[HC_ARCH_BITS == 32 ? 2 : 6];
#ifndef VBOX_WITHOUT_TESTING_FEATURES
    /** The high timestamp value. */
    uint32_t            u32TestingHighTimestamp;
    /** The current testing command (VMMDEV_TESTING_CMD_XXX). */
    uint32_t            u32TestingCmd;
    /** The testing data offset (command specific). */
    uint32_t            offTestingData;
    /** For buffering the what comes in over the testing data port. */
    union
    {
        char            padding[1024];

        /** VMMDEV_TESTING_CMD_INIT, VMMDEV_TESTING_CMD_SUB_NEW,
         *  VMMDEV_TESTING_CMD_FAILED. */
        struct
        {
            char        sz[1024];
        } String, Init, SubNew, Failed;

        /** VMMDEV_TESTING_CMD_TERM, VMMDEV_TESTING_CMD_SUB_DONE. */
        struct
        {
            uint32_t    c;
        } Error, Term, SubDone;

        /** VMMDEV_TESTING_CMD_VALUE. */
        struct
        {
            RTUINT64U   u64Value;
            uint32_t    u32Unit;
            char        szName[1024 - 8 - 4];
        } Value;
    } TestingData;
#endif /* !VBOX_WITHOUT_TESTING_FEATURES */
} VMMDevState;
typedef VMMDevState VMMDEV;
/** Pointer to the VMM device state. */
typedef VMMDEV *PVMMDEV;
AssertCompileMemberAlignment(VMMDEV, CritSect, 8);
AssertCompileMemberAlignment(VMMDEV, cbGuestRAM, 8);
AssertCompileMemberAlignment(VMMDEV, enmCpuHotPlugEvent, 4);
AssertCompileMemberAlignment(VMMDEV, aFacilityStatuses, 8);
#ifndef VBOX_WITHOUT_TESTING_FEATURES
AssertCompileMemberAlignment(VMMDEV, TestingData.Value.u64Value, 8);
#endif


void VMMDevNotifyGuest(VMMDEV *pVMMDevState, uint32_t u32EventMask);
void VMMDevCtlSetGuestFilterMask(VMMDEV *pVMMDevState, uint32_t u32OrMask, uint32_t u32NotMask);

#endif /* !___VMMDev_VMMDevState_h */

