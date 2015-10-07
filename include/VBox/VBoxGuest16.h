/** @file
 * VBoxGuest - VirtualBox Guest Additions Driver Interface, 16-bit (OS/2) header.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___VBox_VBoxGuest16_h
#define ___VBox_VBoxGuest16_h

#define RT_BIT(bit)                             (1UL << (bit))


/** @defgroup grp_vboxguest_16bit   Edition for 16-bit Compilers
 * @ingroup grp_vboxguest
 * @{
 */


#define VBOXGUEST_DEVICE_NAME                   "vboxgst$"

/* aka VBOXGUESTOS2IDCCONNECT */
typedef struct VBGOS2IDC
{
    unsigned long u32Version;
    unsigned long u32Session;
    unsigned long pfnServiceEP;
    short (__cdecl __far *fpfnServiceEP)(unsigned long u32Session, unsigned short iFunction,
                                         void __far *fpvData, unsigned short cbData, unsigned short __far *pcbDataReturned);
    unsigned long fpfnServiceAsmEP;
} VBGOS2IDC;
typedef VBGOS2IDC *PVBGOS2IDC;

#define VBOXGUEST_IOCTL_WAITEVENT               2
#define VBOXGUEST_IOCTL_VMMREQUEST              3
#define VBOXGUEST_IOCTL_CTL_FILTER_MASK         4
#define VBOXGUEST_IOCTL_SET_MOUSE_STATUS        10
#define VBOXGUEST_IOCTL_OS2_IDC_DISCONNECT      48

#define VBOXGUEST_WAITEVENT_OK                  0
#define VBOXGUEST_WAITEVENT_TIMEOUT             1
#define VBOXGUEST_WAITEVENT_INTERRUPTED         2
#define VBOXGUEST_WAITEVENT_ERROR               3

typedef struct _VBoxGuestWaitEventInfo
{
    unsigned long u32TimeoutIn;
    unsigned long u32EventMaskIn;
    unsigned long u32Result;
    unsigned long u32EventFlagsOut;
} VBoxGuestWaitEventInfo;


#define VMMDEV_REQUEST_HEADER_VERSION           (0x10001UL)
typedef struct
{
    unsigned long size;
    unsigned long version;
    unsigned long requestType;
    signed   long rc;
    unsigned long reserved1;
    unsigned long reserved2;
} VMMDevRequestHeader;

#define VMMDevReq_GetMouseStatus                1
#define VMMDevReq_SetMouseStatus                2
#define VMMDevReq_CtlGuestFilterMask            42

#define VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE      RT_BIT(0)
#define VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE     RT_BIT(1)
#define VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR RT_BIT(2)
#define VMMDEV_MOUSE_HOST_CANNOT_HWPOINTER   RT_BIT(3)

typedef struct
{
    VMMDevRequestHeader header;
    unsigned long mouseFeatures;
    unsigned long pointerXPos;
    unsigned long pointerYPos;
} VMMDevReqMouseStatus;

typedef struct VBoxGuestFilterMaskInfo
{
    unsigned long u32OrMask;
    unsigned long u32NotMask;
} VBoxGuestFilterMaskInfo;


/* From VMMDev.h: */
#define VMMDEV_VERSION                          0x00010004UL

#define VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED RT_BIT(0)
#define VMMDEV_EVENT_HGCM                       RT_BIT(1)
#define VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST     RT_BIT(2)
#define VMMDEV_EVENT_JUDGE_CREDENTIALS          RT_BIT(3)
#define VMMDEV_EVENT_RESTORED                   RT_BIT(4)

/** @} */

#endif

