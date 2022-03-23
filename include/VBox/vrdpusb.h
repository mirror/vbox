/** @file
 * VBox Remote Desktop Protocol - Remote USB backend interface. (VRDP)
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vrdpusb_h
#define VBOX_INCLUDED_vrdpusb_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/types.h>

#ifdef IN_RING0
# error "There are no VRDP APIs available in Ring-0 Host Context!"
#endif
#ifdef IN_RC
# error "There are no VRDP APIs available Guest Context!"
#endif


/** @defgroup grp_vrdpusb   Remote USB over VURP
 * @ingroup grp_vusb
 * @{
 */

#define REMOTE_USB_BACKEND_PREFIX_S   "REMOTEUSB"
#define REMOTE_USB_BACKEND_PREFIX_LEN 9

/* Forward declaration. */
struct _REMOTEUSBDEVICE;
typedef struct _REMOTEUSBDEVICE *PREMOTEUSBDEVICE;

/* Forward declaration. */
struct _REMOTEUSBQURB;
typedef struct _REMOTEUSBQURB *PREMOTEUSBQURB;

/* Forward declaration. Actually a class. */
struct _REMOTEUSBBACKEND;
typedef struct _REMOTEUSBBACKEND *PREMOTEUSBBACKEND;

/**
 * Pointer to this structure is queried from USBREMOTEIF::pfnQueryRemoteUsbBackend.
 */
typedef struct REMOTEUSBCALLBACK
{
    PREMOTEUSBBACKEND pInstance;

    DECLCALLBACKMEMBER(int, pfnOpen,(PREMOTEUSBBACKEND pInstance, const char *pszAddress, size_t cbAddress, PREMOTEUSBDEVICE *ppDevice));
    DECLCALLBACKMEMBER(void, pfnClose,(PREMOTEUSBDEVICE pDevice));
    DECLCALLBACKMEMBER(int, pfnReset,(PREMOTEUSBDEVICE pDevice));
    DECLCALLBACKMEMBER(int, pfnSetConfig,(PREMOTEUSBDEVICE pDevice, uint8_t u8Cfg));
    DECLCALLBACKMEMBER(int, pfnClaimInterface,(PREMOTEUSBDEVICE pDevice, uint8_t u8Ifnum));
    DECLCALLBACKMEMBER(int, pfnReleaseInterface,(PREMOTEUSBDEVICE pDevice, uint8_t u8Ifnum));
    DECLCALLBACKMEMBER(int, pfnInterfaceSetting,(PREMOTEUSBDEVICE pDevice, uint8_t u8Ifnum, uint8_t u8Setting));
    DECLCALLBACKMEMBER(int, pfnQueueURB,(PREMOTEUSBDEVICE pDevice, uint8_t u8Type, uint8_t u8Ep, uint8_t u8Direction, uint32_t u32Len, void *pvData, void *pvURB, PREMOTEUSBQURB *ppRemoteURB));
    DECLCALLBACKMEMBER(int, pfnReapURB,(PREMOTEUSBDEVICE pDevice, uint32_t u32Millies, void **ppvURB, uint32_t *pu32Len, uint32_t *pu32Err));
    DECLCALLBACKMEMBER(int, pfnClearHaltedEP,(PREMOTEUSBDEVICE pDevice, uint8_t u8Ep));
    DECLCALLBACKMEMBER(void, pfnCancelURB,(PREMOTEUSBDEVICE pDevice, PREMOTEUSBQURB pRemoteURB));
    DECLCALLBACKMEMBER(int, pfnWakeup,(PREMOTEUSBDEVICE pDevice));
} REMOTEUSBCALLBACK;
/** Pointer to a remote USB callback table. */
typedef REMOTEUSBCALLBACK *PREMOTEUSBCALLBACK;

/**
 * Remote USB interface for querying the remote USB callback table for particular client.
 * Returned from the *QueryGenericUserObject when passing REMOTEUSBIF_OID as the identifier.
 */
typedef struct REMOTEUSBIF
{
    /** Opqaue user data to pass as the first parameter to the callbacks. */
    void                        *pvUser;

    /**
     * Queries the remote USB interface callback table for a given UUID/client ID pair.
     *
     * @returns Pointer to the remote USB callback table or NULL if the client ID and or UUID is invalid.
     * @param   pvUser          Opaque user data from this interface.
     * @param   pUuid           The device UUID to query the interface for.
     * @param   idClient        The client ID to query the interface for.
     */
    DECLCALLBACKMEMBER(PREMOTEUSBCALLBACK, pfnQueryRemoteUsbBackend, (void *pvUser, PCRTUUID pUuid, uint32_t idClient));
} REMOTEUSBIF;
/** Pointer to a remote USB interface. */
typedef REMOTEUSBIF *PREMOTEUSBIF;

/** The UUID to identify the remote USB interface. */
#define REMOTEUSBIF_OID "87012f58-2ad6-4f89-b7b1-92f72c7ea8cc"

/** @} */

#endif /* !VBOX_INCLUDED_vrdpusb_h */
