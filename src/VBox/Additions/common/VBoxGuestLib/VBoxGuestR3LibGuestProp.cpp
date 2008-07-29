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
#include <iprt/autores>
#include <iprt/stdarg.h>
#include <VBox/log.h>
#include <VBox/HostServices/GuestPropertySvc.h>

#include "VBGLR3Internal.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** 
 * Structure containing information needed to enumerate through guest
 * properties.
 */
struct VBGLR3GUESTPROPENUM
{
    /** The buffer containing the raw enumeration data */
    char *pchBuf;
    /** The size of the buffer */
    uint32_t cchBuf;
    /** Index into the buffer pointing to the next entry to enumerate */
    uint32_t iBuf;
};

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
 * @param   pvBuf           A scratch buffer to store the data retrieved into.
 *                          The returned data is only valid for it's lifetime.
 *                          @a ppszValue will point to the start of this buffer.
 * @param   cbBuf           The size of @a pcBuf
 * @param   pszValue        Where to store the pointer to the value retrieved.
 *                          Optional.
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
    if (RT_SUCCESS(rc) && (ppszValue != NULL))
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
    uint32_t cchBuf = MAX_VALUE_LEN;
    RTMemAutoPtr<char> pcBuf;
    int rc = VERR_BUFFER_OVERFLOW;
    for (unsigned i = 0; i < 10 && rc == VERR_BUFFER_OVERFLOW; ++i)
    {
        /* We leave a bit of space here in case the maximum value is raised. */
        cchBuf += 1024;
        if (!pcBuf.realloc<RTMemRealloc>(cchBuf))
            rc = VERR_NO_MEMORY;
        if (RT_SUCCESS(rc))
            rc = VbglR3GuestPropRead(u32ClientId, pszName, pcBuf.get(), cchBuf,
                                     NULL, NULL, NULL, &cchBuf);
        else
            rc = VERR_NO_MEMORY;
    }
    if (RT_SUCCESS(rc))
        *ppszValue = pcBuf.release();
    else if (rc == VERR_BUFFER_OVERFLOW)
        /* VERR_BUFFER_OVERFLOW has a different meaning here as a
         * return code, but we need to report the race. */
        rc = VERR_TOO_MUCH_DATA;

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


/**
 * Raw API for enumerating guest properties which match a given pattern.
 * 
 * @returns VINF_SUCCESS on success and pcBuf points to a packed array
 *          of the form <name>, <value>, <timestamp string>, <flags>,
 *          terminated by four empty strings.  pcbBufActual will contain the
 *          total size of the array.
 * @returns VERR_BUFFER_OVERFLOW if the buffer provided was too small.  In
 *          this case pcbBufActual will contain the size of the buffer needed.
 * @returns IPRT error code in other cases, and pchBufActual is undefined.
 * @param   u32ClientId   The client ID returned by VbglR3GuestPropConnect
 * @param   paszPatterns  A packed array of zero terminated strings, terminated
 *                        by an empty string.
 * @param   pcBuf         The buffer to store the results to.
 * @param   cbBuf         The size of the buffer
 * @param   pcbBufActual  Where to store the size of the returned data on
 *                        success or the buffer size needed if @a pcBuf is too
 *                        small.
 */
VBGLR3DECL(int) VbglR3GuestPropEnumRaw(uint32_t u32ClientId,
                                       const char *paszPatterns,
                                       char *pcBuf,
                                       uint32_t cbBuf,
                                       uint32_t *pcbBufActual)
{
    EnumProperties Msg;

    Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = ENUM_PROPS;
    Msg.hdr.cParms = 3;
    /* Get the length of the patterns array... */
    uint32_t cchPatterns = 0;
    for (uint32_t cchCurrent = strlen(paszPatterns); cchCurrent != 0;
         cchCurrent = strlen(paszPatterns + cchPatterns))
        cchPatterns += cchCurrent + 1;
    /* ...including the terminator. */
    ++cchPatterns;
    VbglHGCMParmPtrSet(&Msg.patterns, const_cast<char *>(paszPatterns),
                       cchPatterns);
    VbglHGCMParmPtrSet(&Msg.strings, pcBuf, cbBuf);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    if (   (RT_SUCCESS(rc) || (VERR_BUFFER_OVERFLOW == rc))
        && (pcbBufActual != NULL)
       )
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.size, pcbBufActual);
        if (!RT_SUCCESS(rc2))
            rc = rc2;
    }
    return rc;
}


/**
 * Start enumerating guest properties which match a given pattern.
 * This function creates a handle which can be used to continue enumerating.
 * @returns VINF_SUCCESS on success, ppHandle points to a handle for continuing
 *          the enumeration and ppszName, ppszValue, pu64Timestamp and
 *          ppszFlags are set.
 * @returns VERR_NOT_FOUND if no matching properties were found.  In this case
 *          the return parameters are not initialised.
 * @returns VERR_TOO_MUCH_DATA if it was not possible to determine the amount
 *          of local space needed to store all the enumeration data.  This is
 *          due to a race between allocating space and the host adding new
 *          data, so retrying may help here.  Other parameters are left
 *          uninitialised
 * @returns IPRT status value otherwise.  Other parameters are left
 *          uninitialised.
 * @param   ppaszPatterns the patterns against which the properties are
 *                        matched.  Pass NULL if everything should be matched.
 * @param   cPatterns     the number of patterns in @a ppaszPatterns.  0 means
 *                        match everything.
 * @param   ppHandle      where the handle for continued enumeration is stored
 *                        on success.  This must be freed with
 *                        VbglR3GuestPropEnumFree when it is no longer needed.
 * @param   ppszName      Where to store the first property name on success.
 *                        Should not be freed.
 * @param   ppszValue     Where to store the first property value on success.
 *                        Should not be freed.
 * @param   ppszValue     Where to store the first timestamp value on success.
 * @param   ppszFlags     Where to store the first flags value on success.
 *                        Should not be freed.
 */
VBGLR3DECL(int) VbglR3GuestPropEnum(uint32_t u32ClientId,
                                    char **ppaszPatterns,
                                    int cPatterns,
                                    PVBGLR3GUESTPROPENUM *ppHandle,
                                    char **ppszName,
                                    char **ppszValue,
                                    uint64_t *pu64Timestamp,
                                    char **ppszFlags)
{
    int rc = VINF_SUCCESS;
    RTMemAutoPtr<VBGLR3GUESTPROPENUM, VbglR3GuestPropEnumFree> pHandle;
    pHandle = reinterpret_cast<PVBGLR3GUESTPROPENUM>(
                                      RTMemAllocZ(sizeof(VBGLR3GUESTPROPENUM))
                                                    );
    if (NULL == pHandle.get())
        rc = VERR_NO_MEMORY;

    /* Get the length of the pattern string, including the final terminator. */
    uint32_t cchPatterns = 1;
    for (int i = 0; i < cPatterns; ++i)
        cchPatterns += strlen(ppaszPatterns[i]) + 1;
    /* Pack the pattern array */
    RTMemAutoPtr<char> pPatterns;
    pPatterns = reinterpret_cast<char *>(RTMemAlloc(cchPatterns));
    char *pPatternsRaw = pPatterns.get();
    for (int i = 0; i < cPatterns; ++i)
        pPatternsRaw = strcpy(pPatternsRaw, ppaszPatterns[i]) + strlen(ppaszPatterns[i]) + 1;
    *pPatternsRaw = 0;

    /* Randomly chosen initial size for the buffer to hold the enumeration
     * information. */
    uint32_t cchBuf = 4096;
    RTMemAutoPtr<char> pchBuf;
    /* In reading the guest property data we are racing against the host
     * adding more of it, so loop a few times and retry on overflow. */
    bool finish = false;
    if (RT_SUCCESS(rc))
        for (int i = 0; (i < 10) && !finish; ++i)
        {
            if (!pchBuf.realloc<RTMemRealloc>(cchBuf))
                rc = VERR_NO_MEMORY;
            if (RT_SUCCESS(rc) || (VERR_BUFFER_OVERFLOW == rc))
                rc = VbglR3GuestPropEnumRaw(u32ClientId, pPatterns.get(),
                                            pchBuf.get(), cchBuf, &cchBuf);
            if (rc != VERR_BUFFER_OVERFLOW)
                finish = true;
            else
                cchBuf += 4096;  /* Just to increase our chances */
        }
    if (VERR_BUFFER_OVERFLOW == rc)
        rc = VERR_TOO_MUCH_DATA;
    if (RT_SUCCESS(rc))
    {
        /* Transfer ownership of the buffer to the handle structure. */
        pHandle->pchBuf = pchBuf.release();
        pHandle->cchBuf = cchBuf;
    }
    if (RT_SUCCESS(rc))
        rc = VbglR3GuestPropEnumNext(pHandle.get(), ppszName, ppszValue,
                                     pu64Timestamp, ppszFlags);
    if (RT_SUCCESS(rc) && (NULL == ppszName))
        /* No matching properties found */
        rc = VERR_NOT_FOUND;
    /* And transfer ownership of the handle to the caller. */
    if (RT_SUCCESS(rc))
        *ppHandle = pHandle.release();
    return rc;
}


/**
 * Get the next guest property.  See @a VbglR3GuestPropEnum.
 * @param  pHandle       handle obtained from @a VbglR3GuestPropEnum.
 * @param  ppszName      where to store the next property name.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed.
 * @param  ppszValue     where to store the next property value.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed.
 * @param  pu64Timestamp where to store the next property timestamp.  This
 *                       will be set to zero if there are no more properties
 *                       to enumerate.
 * @param  ppszFlags     where to store the next property flags.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed.
 */
VBGLR3DECL(int) VbglR3GuestPropEnumNext(PVBGLR3GUESTPROPENUM pHandle,
                                        char **ppszName,
                                        char **ppszValue,
                                        uint64_t *pu64Timestamp,
                                        char **ppszFlags)
{
    uint32_t iBuf = pHandle->iBuf;
    char *pszName = pHandle->pchBuf + iBuf;
    /** @todo replace these with safe strlen's and return an error if needed. */
    iBuf += strlen(pszName) + 1;
    char *pszValue = pHandle->pchBuf + iBuf;
    iBuf += strlen(pszValue) + 1;
    char *pszTimestamp = pHandle->pchBuf + iBuf;
    iBuf += strlen(pszTimestamp) + 1;
    uint64_t u64Timestamp = RTStrToUInt64(pszTimestamp);
    char *pszFlags = pHandle->pchBuf + iBuf;
    iBuf += strlen(pszFlags) + 1;
    /* Otherwise we just stay at the end of the list. */
    if ((iBuf != pHandle->iBuf + 4) && (iBuf < pHandle->cchBuf) /* sanity */)
        pHandle->iBuf = iBuf;
    *ppszName = *pszName != 0 ? pszName : NULL;
    *ppszValue = pszValue != 0 ? pszValue : NULL;
    *pu64Timestamp = u64Timestamp;
    *ppszFlags = pszFlags != 0 ? pszFlags : NULL;
    return VINF_SUCCESS;
}


/**
 * Free an enumeration handle returned by @a VbglR3GuestPropEnum.
 * @param pHandle the handle to free
 */
VBGLR3DECL(void) VbglR3GuestPropEnumFree(PVBGLR3GUESTPROPENUM pHandle)
{
    RTMemFree(pHandle->pchBuf);
    RTMemFree(pHandle);
}
