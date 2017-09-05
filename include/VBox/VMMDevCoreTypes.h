/** @file
 * Virtual Device for Guest <-> VMM/Host communication, Core Types. (ADD,DEV)
 *
 * These types are needed by several headers VBoxGuestLib.h and are kept
 * separate to avoid having to include the whole VMMDev.h fun.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifndef ___VBox_VMMDevCoreTypes_h
#define ___VBox_VMMDevCoreTypes_h

#include <iprt/assert.h>


/** @addtogroup grp_vmmdev
 * @{
 */


/**
 * Seamless mode.
 *
 * Used by VbglR3SeamlessWaitEvent
 *
 * @ingroup grp_vmmdev_req
 *
 * @todo DARN! DARN! DARN! Who forgot to do the 32-bit hack here???
 *       FIXME! XXX!
 *
 *       We will now have to carefully check how our compilers have treated this
 *       flag. If any are compressing it into a byte type, we'll have to check
 *       how the request memory is initialized. If we are 104% sure it's ok to
 *       expand it, we'll expand it. If not, we must redefine the field to a
 *       uint8_t and a 3 byte padding.
 */
typedef enum
{
    VMMDev_Seamless_Disabled         = 0,     /**< normal mode; entire guest desktop displayed. */
    VMMDev_Seamless_Visible_Region   = 1,     /**< visible region mode; only top-level guest windows displayed. */
    VMMDev_Seamless_Host_Window      = 2      /**< windowed mode; each top-level guest window is represented in a host window. */
} VMMDevSeamlessMode;

/**
 * CPU event types.
 *
 * Used by VbglR3CpuHotplugWaitForEvent
 *
 * @ingroup grp_vmmdev_req
 */
typedef enum
{
    VMMDevCpuEventType_Invalid  = 0,
    VMMDevCpuEventType_None     = 1,
    VMMDevCpuEventType_Plug     = 2,
    VMMDevCpuEventType_Unplug   = 3,
    VMMDevCpuEventType_SizeHack = 0x7fffffff
} VMMDevCpuEventType;

/**
 * HGCM service location types.
 * @ingroup grp_vmmdev_req
 */
typedef enum
{
    VMMDevHGCMLoc_Invalid    = 0,
    VMMDevHGCMLoc_LocalHost  = 1,
    VMMDevHGCMLoc_LocalHost_Existing = 2,
    VMMDevHGCMLoc_SizeHack   = 0x7fffffff
} HGCMServiceLocationType;
AssertCompileSize(HGCMServiceLocationType, 4);

/**
 * HGCM host service location.
 * @ingroup grp_vmmdev_req
 */
typedef struct
{
    char achName[128]; /**< This is really szName. */
} HGCMServiceLocationHost;
AssertCompileSize(HGCMServiceLocationHost, 128);

/**
 * HGCM service location.
 * @ingroup grp_vmmdev_req
 */
typedef struct HGCMSERVICELOCATION
{
    /** Type of the location. */
    HGCMServiceLocationType type;

    union
    {
        HGCMServiceLocationHost host;
    } u;
} HGCMServiceLocation;
AssertCompileSize(HGCMServiceLocation, 128+4);


/** @name Guest capability bits.
 * Used by VMMDevReq_ReportGuestCapabilities and VMMDevReq_SetGuestCapabilities.
 * @{ */
/** The guest supports seamless display rendering. */
#define VMMDEV_GUEST_SUPPORTS_SEAMLESS                      RT_BIT_32(0)
/** The guest supports mapping guest to host windows. */
#define VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING     RT_BIT_32(1)
/** The guest graphical additions are active.
 * Used for fast activation and deactivation of certain graphical operations
 * (e.g. resizing & seamless). The legacy VMMDevReq_ReportGuestCapabilities
 * request sets this automatically, but VMMDevReq_SetGuestCapabilities does
 * not. */
#define VMMDEV_GUEST_SUPPORTS_GRAPHICS                      RT_BIT_32(2)
/** The mask of valid events, for sanity checking. */
#define VMMDEV_GUEST_CAPABILITIES_MASK                      UINT32_C(0x00000007)
/** @} */


/**
 * The guest facility.
 * This needs to be kept in sync with AdditionsFacilityType of the Main API!
 */
typedef enum
{
    VBoxGuestFacilityType_Unknown         = 0,
    VBoxGuestFacilityType_VBoxGuestDriver = 20,
    VBoxGuestFacilityType_AutoLogon       = 90,  /* VBoxGINA / VBoxCredProv / pam_vbox. */
    VBoxGuestFacilityType_VBoxService     = 100,
    VBoxGuestFacilityType_VBoxTrayClient  = 101, /* VBoxTray (Windows), VBoxClient (Linux, Unix). */
    VBoxGuestFacilityType_Seamless        = 1000,
    VBoxGuestFacilityType_Graphics        = 1100,
    VBoxGuestFacilityType_MonitorAttach   = 1101,
    VBoxGuestFacilityType_All             = 0x7ffffffe,
    VBoxGuestFacilityType_SizeHack        = 0x7fffffff
} VBoxGuestFacilityType;
AssertCompileSize(VBoxGuestFacilityType, 4);


/**
 * The current guest status of a facility.
 * This needs to be kept in sync with AdditionsFacilityStatus of the Main API!
 *
 * @remarks r=bird: Pretty please, for future types like this, simply do a
 *          linear allocation without any gaps.  This stuff is impossible work
 *          efficiently with, let alone validate.  Applies to the other facility
 *          enums too.
 */
typedef enum
{
    VBoxGuestFacilityStatus_Inactive    = 0,
    VBoxGuestFacilityStatus_Paused      = 1,
    VBoxGuestFacilityStatus_PreInit     = 20,
    VBoxGuestFacilityStatus_Init        = 30,
    VBoxGuestFacilityStatus_Active      = 50,
    VBoxGuestFacilityStatus_Terminating = 100,
    VBoxGuestFacilityStatus_Terminated  = 101,
    VBoxGuestFacilityStatus_Failed  =     800,
    VBoxGuestFacilityStatus_Unknown     = 999,
    VBoxGuestFacilityStatus_SizeHack    = 0x7fffffff
} VBoxGuestFacilityStatus;
AssertCompileSize(VBoxGuestFacilityStatus, 4);


/**
 * The current status of specific guest user.
 * This needs to be kept in sync with GuestUserState of the Main API!
 */
typedef enum VBoxGuestUserState
{
    VBoxGuestUserState_Unknown            = 0,
    VBoxGuestUserState_LoggedIn           = 1,
    VBoxGuestUserState_LoggedOut          = 2,
    VBoxGuestUserState_Locked             = 3,
    VBoxGuestUserState_Unlocked           = 4,
    VBoxGuestUserState_Disabled           = 5,
    VBoxGuestUserState_Idle               = 6,
    VBoxGuestUserState_InUse              = 7,
    VBoxGuestUserState_Created            = 8,
    VBoxGuestUserState_Deleted            = 9,
    VBoxGuestUserState_SessionChanged     = 10,
    VBoxGuestUserState_CredentialsChanged = 11,
    VBoxGuestUserState_RoleChanged        = 12,
    VBoxGuestUserState_GroupAdded         = 13,
    VBoxGuestUserState_GroupRemoved       = 14,
    VBoxGuestUserState_Elevated           = 15,
    VBoxGuestUserState_SizeHack           = 0x7fffffff
} VBoxGuestUserState;
AssertCompileSize(VBoxGuestUserState, 4);

/* forward declarations: */
struct VMMDevRequestHeader;
struct VMMDevReqMousePointer;
struct VMMDevMemory;

/** @} */

#endif

