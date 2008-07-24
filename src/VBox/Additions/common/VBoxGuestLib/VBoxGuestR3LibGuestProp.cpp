/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions,
 * guest properties.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/stdarg.h>
#include <VBox/log.h>
#include <VBox/HostServices/GuestPropertySvc.h>

#include "VBGLR3Internal.h"

using namespace guestProp;

/**
 * Connects to the guest property service.
 *
 * @returns VBox status code
 * @param   pu32ClientId    Where to put the client id on success. The client id
 *                          must be passed to all the other calls to the service.
 */
VBGLR3DECL(int) VbglR3GuestPropConnect(uint32_t *pu32ClientId)
{
    VBoxGuestHGCMConnectInfo Info;
    Info.result = (uint32_t)VERR_WRONG_ORDER; /** @todo drop the cast when the result type has been fixed! */
    Info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    memset(&Info.Loc.u, 0, sizeof(Info.Loc.u));
    strcpy(Info.Loc.u.host.achName, "VBoxGuestPropSvc");
    Info.u32ClientID = UINT32_MAX;  /* try make valgrid shut up. */

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
    {
        rc = Info.result;
        if (RT_SUCCESS(rc))
            *pu32ClientId = Info.u32ClientID;
    }
    return rc;
}


/**
 * Disconnect from the guest property service.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 */
VBGLR3DECL(int) VbglR3GuestPropDisconnect(uint32_t u32ClientId)
{
    VBoxGuestHGCMDisconnectInfo Info;
    Info.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Info.u32ClientID = u32ClientId;

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));
    if (RT_SUCCESS(rc))
        rc = Info.result;
    return rc;
}


/**
 * Write a property value.
 *
 * @returns VBox status code.
 * @param   u32ClientId     The client id returned by VbglR3InvsSvcConnect().
 * @param   pszName         The property to save to.  Utf8
 * @param   pszValue        The value to store.  Utf8.  If this is NULL then
 *                          the property will be removed.
 * @param   pszFlags        The flags for the property
 */
VBGLR3DECL(int) VbglR3GuestPropWrite(uint32_t u32ClientId, const char *pszName, const char *pszValue, const char *pszFlags)
{
    int rc;

    if (pszValue != NULL)
    {
        SetProperty Msg;

        Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
        Msg.hdr.u32ClientID = u32ClientId;
        Msg.hdr.u32Function = SET_PROP_VALUE;
        Msg.hdr.cParms = 2;
        VbglHGCMParmPtrSet(&Msg.name, const_cast<char *>(pszName), strlen(pszName) + 1);
        VbglHGCMParmPtrSet(&Msg.value, const_cast<char *>(pszValue), strlen(pszValue) + 1);
        VbglHGCMParmPtrSet(&Msg.flags, const_cast<char *>(pszFlags), strlen(pszFlags) + 1);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
            rc = Msg.hdr.result;
    }
    else
    {
        DelProperty Msg;

        Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
        Msg.hdr.u32ClientID = u32ClientId;
        Msg.hdr.u32Function = DEL_PROP;
        Msg.hdr.cParms = 1;
        VbglHGCMParmPtrSet(&Msg.name, const_cast<char *>(pszName), strlen(pszName) + 1);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
            rc = Msg.hdr.result;
    }
    return rc;
}


/**
 * Write a property value.
 *
 * @returns VBox status code.
 *
 * @param   u32ClientId     The client id returned by VbglR3InvsSvcConnect().
 * @param   pszName         The property to save to.  Must be valid UTF-8.
 * @param   pszValue        The value to store.  Must be valid UTF-8.
 *                          If this is NULL then the property will be removed.
 *
 * @note  if the property already exists and pszValue is not NULL then the
 *        property's flags field will be left unchanged
 */
VBGLR3DECL(int) VbglR3GuestPropWriteValue(uint32_t u32ClientId, const char *pszName, const char *pszValue)
{
    int rc;

    if (pszValue != NULL)
    {
        SetPropertyValue Msg;

        Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
        Msg.hdr.u32ClientID = u32ClientId;
        Msg.hdr.u32Function = SET_PROP_VALUE;
        Msg.hdr.cParms = 2;
        VbglHGCMParmPtrSet(&Msg.name, const_cast<char *>(pszName), strlen(pszName) + 1);
        VbglHGCMParmPtrSet(&Msg.value, const_cast<char *>(pszValue), strlen(pszValue) + 1);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
            rc = Msg.hdr.result;
    }
    else
    {
        DelProperty Msg;

        Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
        Msg.hdr.u32ClientID = u32ClientId;
        Msg.hdr.u32Function = DEL_PROP;
        Msg.hdr.cParms = 1;
        VbglHGCMParmPtrSet(&Msg.name, const_cast<char *>(pszName), strlen(pszName) + 1);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
        if (RT_SUCCESS(rc))
            rc = Msg.hdr.result;
    }
    return rc;
}


/**
 * Write a property value where the value is formatted in RTStrPrintfV fashion.
 *
 * @returns The same as VbglR3GuestPropWriteValue with the addition of VERR_NO_STR_MEMORY.
 *
 * @param   u32ClientId     The client ID returned by VbglR3InvsSvcConnect().
 * @param   pszName         The property to save to.  Must be valid UTF-8.
 * @param   pszValueFormat  The value format. This must be valid UTF-8 when fully formatted.
 * @param   va              The format arguments.
 */
VBGLR3DECL(int) VbglR3GuestPropWriteValueV(uint32_t u32ClientId, const char *pszName, const char *pszValueFormat, va_list va)
{
    /*
     * Format the value and pass it on to the setter.
     */
    int rc = VERR_NO_STR_MEMORY;
    char *pszValue;
    if (RTStrAPrintfV(&pszValue, pszValueFormat, va) < 0)
    {
        rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, pszValue);
        RTStrFree(pszValue);
    }
    return rc;
}


/**
 * Write a property value where the value is formatted in RTStrPrintf fashion.
 *
 * @returns The same as VbglR3GuestPropWriteValue with the addition of VERR_NO_STR_MEMORY.
 *
 * @param   u32ClientId     The client ID returned by VbglR3InvsSvcConnect().
 * @param   pszName         The property to save to.  Must be valid UTF-8.
 * @param   pszValueFormat  The value format. This must be valid UTF-8 when fully formatted.
 * @param   ...             The format arguments.
 */
VBGLR3DECL(int) VbglR3GuestPropWriteValueF(uint32_t u32ClientId, const char *pszName, const char *pszValueFormat, ...)
{
    va_list va;
    va_start(va, pszValueFormat);
    int rc = VbglR3GuestPropWriteValueV(u32ClientId, pszName, pszValueFormat, va);
    va_end(va);
    return rc;
}


/**
 * Retrieve a property.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, pszValue, pu64Timestamp and pszFlags
 *          containing valid data.
 * @retval  VERR_BUFFER_OVERFLOW if the scratch buffer @a pcBuf is not large
 *          enough.  In this case the size needed will be placed in
 *          @a pcbBufActual if it is not NULL.
 * @retval  VERR_NOT_FOUND if the key wasn't found.
 *
 * @param   u32ClientId     The client id returned by VbglR3ClipboardConnect().
 * @param   pszName         The value to read.  Utf8
 * @param   pcBuf           A scratch buffer to store the data retrieved into.
 *                          The returned data is only valid for it's lifetime.
 * @param   cbBuf           The size of @a pcBuf
 * @param   pszValue        Where to store the pointer to the value retrieved.
 * @param   pu64Timestamp   Where to store the timestamp.  Optional.
 * @param   pszFlags        Where to store the pointer to the flags.  Optional.
 * @param   pcbBufActual    If @a pcBuf is not large enough, the size needed.
 *                          Optional.
 */
VBGLR3DECL(int) VbglR3GuestPropRead(uint32_t u32ClientId, const char *pszName,
                                    void *pvBuf, uint32_t cbBuf,
                                    char **ppszValue, uint64_t *pu64Timestamp,
                                    char **ppszFlags,
                                    uint32_t *pcbBufActual)
{
    GetProperty Msg;

    Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GET_PROP;
    Msg.hdr.cParms = 4;
    VbglHGCMParmPtrSet(&Msg.name, const_cast<char *>(pszName),
                       strlen(pszName) + 1);
    VbglHGCMParmPtrSet(&Msg.buffer, pvBuf, cbBuf);
    VbglHGCMParmUInt64Set(&Msg.timestamp, 0);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    if ((VERR_BUFFER_OVERFLOW == rc) && (pcbBufActual != NULL))
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.size, pcbBufActual);
        if (!RT_SUCCESS(rc2))
            rc = rc2;
    }
    if (RT_SUCCESS(rc) && (pu64Timestamp != NULL))
        rc = VbglHGCMParmUInt64Get(&Msg.timestamp, pu64Timestamp);
    if (RT_SUCCESS(rc))
        *ppszValue = reinterpret_cast<char *>(pvBuf);
    if (RT_SUCCESS(rc) && (ppszFlags != NULL))
    {
        char *pszEos = reinterpret_cast<char *>(memchr(pvBuf, '\0', cbBuf));
        if (pszEos)
            *ppszFlags = pszEos + 1;
        else
            rc = VERR_TOO_MUCH_DATA;
    }
    return rc;
}


/**
 * Retrieve a property value, allocating space for it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *ppszValue containing valid data.
 * @retval  VERR_NOT_FOUND if the key wasn't found.
 * @retval  VERR_TOO_MUCH_DATA if we were unable to determine the right size
 *          to allocate for the buffer.  This can happen as the result of a
 *          race between our allocating space and the host changing the
 *          property value.
 *
 * @param   u32ClientId     The client id returned by VbglR3ClipboardConnect().
 * @param   pszName         The value to read. Must be valid UTF-8.
 * @param   ppszValue       Where to store the pointer to the value returned.
 *                          This is always set to NULL or to the result, even
 *                          on failure.
 */
VBGLR3DECL(int) VbglR3GuestPropReadValueAlloc(uint32_t u32ClientId,
                                              const char *pszName,
                                              char **ppszValue)
{
    /*
     * Quick input validation.
     */
    AssertPtr(ppszValue);
    *ppszValue = NULL;
    AssertPtrReturn(pszName, VERR_INVALID_PARAMETER);

    /*
     * There is a race here between our reading the property size and the
     * host changing the value before we read it.  Try up to ten times and
     * report the problem if that fails.
     */
    char       *pszValue = NULL;
    void       *pvBuf    = NULL;
    uint32_t    cchBuf   = MAX_VALUE_LEN;
    int         rc       = VERR_BUFFER_OVERFLOW;
    for (unsigned i = 0; i < 10 && rc == VERR_BUFFER_OVERFLOW; ++i)
    {
        /* We leave a bit of space here in case the maximum value is raised. */
        cchBuf += 1024;
        void *pvTmpBuf = RTMemRealloc(pvBuf, cchBuf);
        if (pvTmpBuf)
        {
            pvBuf = pvTmpBuf;
            rc = VbglR3GuestPropRead(u32ClientId, pszName, pvBuf, cchBuf,
                                     &pszValue, NULL, NULL, &cchBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    if (RT_SUCCESS(rc))
    {
        Assert(pszValue == (char *)pvBuf);
        *ppszValue = pszValue;
    }
    else
    {
        RTMemFree(pvBuf);
        if (rc == VERR_BUFFER_OVERFLOW)
            /* VERR_BUFFER_OVERFLOW has a different meaning here as a
             * return code, but we need to report the race. */
            rc = VERR_TOO_MUCH_DATA;
    }

    return rc;
}

/**
 * Free the memory used by VbglR3GuestPropReadValueAlloc for returning a
 * value.
 *
 * @param pszValue   the memory to be freed.  NULL pointers will be ignored.
 */
VBGLR3DECL(void) VbglR3GuestPropReadValueFree(char *pszValue)
{
    RTMemFree(pszValue);
}


/**
 * Retrieve a property value, using a user-provided buffer to store it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, pszValue containing valid data.
 * @retval  VERR_BUFFER_OVERFLOW and the size needed in pcchValueActual if the
 *          buffer provided was too small
 * @retval  VERR_NOT_FOUND if the key wasn't found.
 *
 * @note    There is a race here between obtaining the size of the buffer
 *          needed to hold the value and the value being updated.
 *
 * @param   u32ClientId     The client id returned by VbglR3ClipboardConnect().
 * @param   pszName         The value to read.  Utf8
 * @param   pszValue        Where to store the value retrieved.
 * @param   cchValue        The size of the buffer pointed to by @a pszValue
 * @param   pcchValueActual Where to store the size of the buffer needed if
 *                          the buffer supplied is too small.  Optional.
 */
VBGLR3DECL(int) VbglR3GuestPropReadValue(uint32_t u32ClientId, const char *pszName,
                                         char *pszValue, uint32_t cchValue,
                                         uint32_t *pcchValueActual)
{
    char *pcBuf = NULL;
    int rc = VbglR3GuestPropReadValueAlloc(u32ClientId, pszName, &pcBuf);
    if (RT_SUCCESS(rc))
    {
        uint32_t cchValueActual = strlen(pcBuf) + 1;
        if (cchValueActual > cchValue)
        {
            if (pcchValueActual != NULL)
                *pcchValueActual = cchValueActual;
            rc = VERR_BUFFER_OVERFLOW;
        }
        if (RT_SUCCESS(rc))
            strcpy(pszValue, pcBuf);
    }
    VbglR3GuestPropReadValueFree(pcBuf);
    return rc;
}
