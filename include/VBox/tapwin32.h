/** @file
 * VirtualBox - TAP for Windows definitions.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_tapwin32_h
#define ___VBox_tapwin32_h

#define TAP_DRIVER_MAJOR_VERSION    8
#define TAP_DRIVER_MINOR_VERSION    2

//=============
// TAP IOCTLs
//=============

#pragma pack(4)
typedef struct
{
    ULONG major;
    ULONG minor;
    ULONG debug;
} TAP_VERSION;

typedef struct
{
    LONG fConnect;
} TAP_MEDIASTATUS;

typedef struct
{
    ULONG   cb;
    PVOID   pPacket;
} TAP_SCATTER_GATHER_ITEM;

/** Arbitrary maximum for sanity checks. */
#define TAP_SCATTER_GATHER_MAX_PACKETS          64

typedef struct
{
    ULONG                   cPackets;
    TAP_SCATTER_GATHER_ITEM aPacket[1];
} TAP_SCATTER_GATHER_LIST, *PTAP_SCATTER_GATHER_LIST;

typedef struct
{
    ULONG                   cPackets;
    TAP_SCATTER_GATHER_ITEM aPacket[TAP_SCATTER_GATHER_MAX_PACKETS];
} TAP_SCATTER_GATHER_LIST_MAX, *PTAP_SCATTER_GATHER_LIST_MAX;

#pragma pack()

#define TAP_CONTROL_CODE(request,method) \
  CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_GET_MAC               TAP_CONTROL_CODE (1,  METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION           TAP_CONTROL_CODE (2,  METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU               TAP_CONTROL_CODE (3,  METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO              TAP_CONTROL_CODE (4,  METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE (5,  METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS      TAP_CONTROL_CODE (6,  METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ      TAP_CONTROL_CODE (7,  METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE          TAP_CONTROL_CODE (8,  METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT   TAP_CONTROL_CODE (9,  METHOD_BUFFERED)
/* Starting with version 8.2 */
#define TAP_IOCTL_TRANSFER_ETHPACKETS   TAP_CONTROL_CODE (10, METHOD_BUFFERED)


#endif
