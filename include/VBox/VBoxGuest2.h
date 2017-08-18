/** @file
 * VBoxGuest - VirtualBox Guest Additions Driver Interface, Mixed Up Mess.
 * (ADD,DEV)
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

#ifndef ___VBox_VBoxGuest2_h
#define ___VBox_VBoxGuest2_h

#include <iprt/assert.h>

#ifdef VBOX_WITH_HGCM
# include <VBox/VMMDev2.h>

/** @addtogroup grp_vmmdev
 * @{ */

/**
 * HGCM connect info structure.
 *
 * This is used by VBOXGUEST_IOCTL_HGCM_CONNECT and in VbglR0.
 *
 * @ingroup grp_vboxguest
 */
# pragma pack(1) /* explicit packing for good measure. */
typedef struct VBoxGuestHGCMConnectInfo
{
    int32_t result;           /**< OUT */
    HGCMServiceLocation Loc;  /**< IN */
    uint32_t u32ClientID;     /**< OUT */
} VBoxGuestHGCMConnectInfo;
AssertCompileSize(VBoxGuestHGCMConnectInfo, 4+4+128+4);
# pragma pack()


/**
 * HGCM connect info structure.
 *
 * This is used by VBOXGUEST_IOCTL_HGCM_DISCONNECT and in VbglR0.
 *
 * @ingroup grp_vboxguest
 */
typedef struct VBoxGuestHGCMDisconnectInfo
{
    int32_t result;           /**< OUT */
    uint32_t u32ClientID;     /**< IN */
} VBoxGuestHGCMDisconnectInfo;
AssertCompileSize(VBoxGuestHGCMDisconnectInfo, 8);

/**
 * HGCM call info structure.
 *
 * This is used by VBOXGUEST_IOCTL_HGCM_CALL.
 *
 * @ingroup grp_vboxguest
 */
typedef struct VBoxGuestHGCMCallInfo
{
    int32_t result;           /**< OUT Host HGCM return code.*/
    uint32_t u32ClientID;     /**< IN  The id of the caller. */
    uint32_t u32Function;     /**< IN  Function number. */
    uint32_t cParms;          /**< IN  How many parms. */
    /* Parameters follow in form HGCMFunctionParameter aParms[cParms] */
} VBoxGuestHGCMCallInfo;
AssertCompileSize(VBoxGuestHGCMCallInfo, 16);

/**
 * Initialize a HGCM header (VBoxGuestHGCMCallInfo).
 *
 * @param   a_pHdr          The header to initalize.
 * @param   a_idClient      The client connection ID to call thru.
 * @param   a_idFunction    The function we're calling
 * @param   a_cParameters   Number of parameters.
 */
# define VBGL_HGCM_HDR_INIT(a_pHdr, a_idClient, a_idFunction, a_cParameters) \
    do { \
        (a_pHdr)->result         = VERR_INTERNAL_ERROR; \
        (a_pHdr)->u32ClientID    = (a_idClient); \
        (a_pHdr)->u32Function    = (a_idFunction); \
        (a_pHdr)->cParms         = (a_cParameters); \
    } while (0)


/**
 * HGCM call info structure.
 *
 * This is used by VBOXGUEST_IOCTL_HGCM_CALL_TIMED.
 *
 * @ingroup grp_vboxguest
 */
# pragma pack(1) /* explicit packing for good measure. */
typedef struct VBoxGuestHGCMCallInfoTimed
{
    uint32_t u32Timeout;         /**< IN  How long to wait for completion before cancelling the call. */
    uint32_t fInterruptible;     /**< IN  Is this request interruptible? */
    VBoxGuestHGCMCallInfo info;  /**< IN/OUT The rest of the call information.  Placed after the timeout
                                  * so that the parameters follow as they would for a normal call. */
    /* Parameters follow in form HGCMFunctionParameter aParms[cParms] */
} VBoxGuestHGCMCallInfoTimed;
AssertCompileSize(VBoxGuestHGCMCallInfoTimed, 8+16);
# pragma pack()

/**
 * Initialize a HGCM header (VBoxGuestHGCMCallInfo), with timeout (interruptible).
 *
 * @param   a_pHdr          The header to initalize.
 * @param   a_idClient      The client connection ID to call thru.
 * @param   a_idFunction    The function we're calling
 * @param   a_cParameters   Number of parameters.
 * @param   a_cMsTimeout    The timeout in milliseconds.
 */
# define VBGL_HGCM_HDR_INIT_TIMED(a_pHdr, a_idClient, a_idFunction, a_cParameters, a_cMsTimeout) \
    do { \
        (a_pHdr)->u32Timeout        = (a_cMsTimeout); \
        (a_pHdr)->fInterruptible    = true; \
        (a_pHdr)->info.result       = VERR_INTERNAL_ERROR; \
        (a_pHdr)->info.u32ClientID  = (a_idClient); \
        (a_pHdr)->info.u32Function  = (a_idFunction); \
        (a_pHdr)->info.cParms       = (a_cParameters); \
    } while (0)

/** @} */

#endif /* VBOX_WITH_HGCM */

#endif

