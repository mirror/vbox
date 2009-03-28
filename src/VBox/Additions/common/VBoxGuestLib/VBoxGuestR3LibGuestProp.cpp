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
#include <iprt/autores.h>
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
    /** @todo add a magic and validate the handle. */
    /** The buffer containing the raw enumeration data */
    char *pchBuf;
    /** The end of the buffer */
    char *pchBufEnd;
    /** Pointer to the next entry to enumerate inside the buffer */
    char *pchNext;
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
        Msg.hdr.cParms = 3;
        VbglHGCMParmPtrSetString(&Msg.name,  pszName);
        VbglHGCMParmPtrSetString(&Msg.value, pszValue);
        VbglHGCMParmPtrSetString(&Msg.flags, pszFlags);
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
        VbglHGCMParmPtrSetString(&Msg.name, pszName);
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
        VbglHGCMParmPtrSetString(&Msg.name, pszName);
        VbglHGCMParmPtrSetString(&Msg.value, pszValue);
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
        VbglHGCMParmPtrSetString(&Msg.name, pszName);
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
 * @param   u32ClientId     The client id returned by VbglR3GuestPropConnect().
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
    /*
     * Create the GET_PROP message and call the host.
     */
    GetProperty Msg;

    Msg.hdr.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.u32ClientID = u32ClientId;
    Msg.hdr.u32Function = GET_PROP;
    Msg.hdr.cParms = 4;
    VbglHGCMParmPtrSetString(&Msg.name, pszName);
    VbglHGCMParmPtrSet(&Msg.buffer, pvBuf, cbBuf);
    VbglHGCMParmUInt64Set(&Msg.timestamp, 0);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;

    /*
     * The cbBufActual parameter is also returned on overflow so the call can
     * adjust his/her buffer.
     */
    if (    rc == VERR_BUFFER_OVERFLOW
        ||  pcbBufActual != NULL)
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.size, pcbBufActual);
        AssertRCReturn(rc2, RT_FAILURE(rc) ? rc : rc2);
    }
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Buffer layout: Value\0Flags\0.
     *
     * If the caller cares about any of these strings, make sure things are
     * propertly terminated (paranoia).
     */
    if (    RT_SUCCESS(rc)
        &&  (ppszValue != NULL || ppszFlags != NULL))
    {
        /* Validate / skip 'Name'. */
        char *pszFlags = (char *)memchr(pvBuf, '\0', cbBuf) + 1;
        AssertPtrReturn(pszFlags, VERR_TOO_MUCH_DATA);
        if (ppszValue)
            *ppszValue = (char *)pvBuf;

        if (ppszFlags)
        {
            /* Validate 'Flags'. */
            void *pvEos = memchr(pszFlags, '\0', cbBuf - (pszFlags - (char *)pvBuf));
            AssertPtrReturn(pvEos, VERR_TOO_MUCH_DATA);
            *ppszFlags = pszFlags;
        }
    }

    /* And the timestamp, if requested. */
    if (pu64Timestamp != NULL)
    {
        rc = VbglHGCMParmUInt64Get(&Msg.timestamp, pu64Timestamp);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
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
 * @param   u32ClientId     The client id returned by VbglR3GuestPropConnect().
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
 * @param   u32ClientId     The client id returned by VbglR3GuestPropConnect().
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
    char *pszBuf = NULL;
    int rc = VbglR3GuestPropReadValueAlloc(u32ClientId, pszName, &pszBuf);
    if (RT_SUCCESS(rc))
    {
        size_t cchValueActual = strlen(pszBuf) + 1;
        if (cchValueActual <= cchValue)
            memcpy(pszValue, pszBuf, cchValueActual);
        else
        {
            if (pcchValueActual != NULL)
                *pcchValueActual = (uint32_t)cchValueActual;
            rc = VERR_BUFFER_OVERFLOW;
        }
    }
    VbglR3GuestPropReadValueFree(pszBuf);
    return rc;
}


/**
 * Raw API for enumerating guest properties which match a given pattern.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success and pcBuf points to a packed array
 *          of the form <name>, <value>, <timestamp string>, <flags>,
 *          terminated by four empty strings.  pcbBufActual will contain the
 *          total size of the array.
 * @retval  VERR_BUFFER_OVERFLOW if the buffer provided was too small.  In
 *          this case pcbBufActual will contain the size of the buffer needed.
 * @returns IPRT error code in other cases, and pchBufActual is undefined.
 *
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
                                       const char *pszzPatterns,
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
    size_t cchPatterns = 0;
    for (size_t cchCurrent = strlen(pszzPatterns); cchCurrent != 0;
         cchCurrent = strlen(pszzPatterns + cchPatterns))
        cchPatterns += cchCurrent + 1;
    /* ...including the terminator. */
    ++cchPatterns;
    VbglHGCMParmPtrSet(&Msg.patterns, (char *)pszzPatterns, (uint32_t)cchPatterns);
    VbglHGCMParmPtrSet(&Msg.strings, pcBuf, cbBuf);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.result;
    if (   pcbBufActual
        && (    RT_SUCCESS(rc)
            ||  rc == VERR_BUFFER_OVERFLOW))
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
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *ppHandle points to a handle for continuing
 *          the enumeration and *ppszName, *ppszValue, *pu64Timestamp and
 *          *ppszFlags are set.
 * @retval  VERR_NOT_FOUND if no matching properties were found.  In this case
 *          the return parameters are not initialised.
 * @retval  VERR_TOO_MUCH_DATA if it was not possible to determine the amount
 *          of local space needed to store all the enumeration data.  This is
 *          due to a race between allocating space and the host adding new
 *          data, so retrying may help here.  Other parameters are left
 *          uninitialised
 *
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 * @param   papszPatterns   The patterns against which the properties are
 *                          matched.  Pass NULL if everything should be matched.
 * @param   cPatterns       The number of patterns in @a papszPatterns.  0 means
 *                          match everything.
 * @param   ppHandle        where the handle for continued enumeration is stored
 *                          on success.  This must be freed with
 *                          VbglR3GuestPropEnumFree when it is no longer needed.
 * @param   ppszName        Where to store the first property name on success.
 *                          Should not be freed. Optional.
 * @param   ppszValue       Where to store the first property value on success.
 *                          Should not be freed. Optional.
 * @param   ppszValue       Where to store the first timestamp value on success.
 *                          Optional.
 * @param   ppszFlags       Where to store the first flags value on success.
 *                          Should not be freed. Optional.
 */
VBGLR3DECL(int) VbglR3GuestPropEnum(uint32_t u32ClientId,
                                    char const * const *papszPatterns,
                                    uint32_t cPatterns,
                                    PVBGLR3GUESTPROPENUM *ppHandle,
                                    char const **ppszName,
                                    char const **ppszValue,
                                    uint64_t *pu64Timestamp,
                                    char const **ppszFlags)
{
    /* Create the handle. */
    RTMemAutoPtr<VBGLR3GUESTPROPENUM, VbglR3GuestPropEnumFree> Handle;
    Handle = (PVBGLR3GUESTPROPENUM)RTMemAllocZ(sizeof(VBGLR3GUESTPROPENUM));
    if (!Handle)
        return VERR_NO_MEMORY;

    /* Get the length of the pattern string, including the final terminator. */
    size_t cchPatterns = 1;
    for (uint32_t i = 0; i < cPatterns; ++i)
        cchPatterns += strlen(papszPatterns[i]) + 1;

    /* Pack the pattern array */
    RTMemAutoPtr<char> Patterns;
    Patterns = (char *)RTMemAlloc(cchPatterns);
    size_t off = 0;
    for (uint32_t i = 0; i < cPatterns; ++i)
    {
        size_t cb = strlen(papszPatterns[i]) + 1;
        memcpy(&Patterns[off], papszPatterns[i], cb);
        off += cb;
    }
    Patterns[off] = '\0';

    /* Randomly chosen initial size for the buffer to hold the enumeration
     * information. */
    uint32_t cchBuf = 4096;
    RTMemAutoPtr<char> Buf;

    /* In reading the guest property data we are racing against the host
     * adding more of it, so loop a few times and retry on overflow. */
    int rc = VINF_SUCCESS;
    for (int i = 0; i < 10; ++i)
    {
        if (!Buf.realloc(cchBuf))
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        rc = VbglR3GuestPropEnumRaw(u32ClientId, Patterns.get(),
                                    Buf.get(), cchBuf, &cchBuf);
        if (rc != VERR_BUFFER_OVERFLOW)
            break;
        cchBuf += 4096;  /* Just to increase our chances */
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Transfer ownership of the buffer to the handle structure and
         * call VbglR3GuestPropEnumNext to retriev the first entry.
         */
        Handle->pchNext = Handle->pchBuf = Buf.release();
        Handle->pchBufEnd = Handle->pchBuf + cchBuf;

        const char *pszNameTmp;
        if (!ppszName)
            ppszName = &pszNameTmp;
        rc = VbglR3GuestPropEnumNext(Handle.get(), ppszName, ppszValue,
                                     pu64Timestamp, ppszFlags);
        if (RT_SUCCESS(rc) && *ppszName != NULL)
            *ppHandle = Handle.release();
        else if (RT_SUCCESS(rc))
            rc = VERR_NOT_FOUND; /* No matching properties found. */
    }
    else if (rc == VERR_BUFFER_OVERFLOW)
        rc = VERR_TOO_MUCH_DATA;
    return rc;
}


/**
 * Get the next guest property.
 *
 * See @a VbglR3GuestPropEnum.
 *
 * @returns VBox status code.
 *
 * @param  pHandle       Handle obtained from @a VbglR3GuestPropEnum.
 * @param  ppszName      Where to store the next property name.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed. Optional.
 * @param  ppszValue     Where to store the next property value.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed. Optional.
 * @param  pu64Timestamp Where to store the next property timestamp.  This
 *                       will be set to zero if there are no more properties
 *                       to enumerate. Optional.
 * @param  ppszFlags     Where to store the next property flags.  This will be
 *                       set to NULL if there are no more properties to
 *                       enumerate.  This pointer should not be freed. Optional.
 *
 * @remarks While all output parameters are optional, you need at least one to
 *          figure out when to stop.
 */
VBGLR3DECL(int) VbglR3GuestPropEnumNext(PVBGLR3GUESTPROPENUM pHandle,
                                        char const **ppszName,
                                        char const **ppszValue,
                                        uint64_t *pu64Timestamp,
                                        char const **ppszFlags)
{
    /*
     * The VBGLR3GUESTPROPENUM structure contains a buffer containing the raw
     * properties data and a pointer into the buffer which tracks how far we
     * have parsed so far.  The buffer contains packed strings in groups of
     * four - name, value, timestamp (as a decimal string) and flags.  It is
     * terminated by four empty strings.  We can rely on this layout unless
     * the caller has been poking about in the structure internals, in which
     * case they must take responsibility for the results.
     *
     * Layout:
     *   Name\0Value\0Timestamp\0Flags\0
     */
    char *pchNext = pHandle->pchNext;       /* The cursor. */
    char *pchEnd  = pHandle->pchBufEnd;     /* End of buffer, for size calculations. */

    char *pszName      = pchNext;
    char *pszValue     = pchNext = (char *)memchr(pchNext, '\0', pchEnd - pchNext) + 1;
    AssertPtrReturn(pchNext, VERR_PARSE_ERROR);  /* 0x1 is also an invalid pointer :) */

    char *pszTimestamp = pchNext = (char *)memchr(pchNext, '\0', pchEnd - pchNext) + 1;
    AssertPtrReturn(pchNext, VERR_PARSE_ERROR);

    char *pszFlags     = pchNext = (char *)memchr(pchNext, '\0', pchEnd - pchNext) + 1;
    AssertPtrReturn(pchNext, VERR_PARSE_ERROR);

    /*
     * Don't move the index pointer if we found the terminating "\0\0\0\0" entry.
     * Don't try convert the timestamp either.
     */
    uint64_t u64Timestamp;
    if (*pszName != '\0')
    {
        pchNext = (char *)memchr(pchNext, '\0', pchEnd - pchNext) + 1;
        AssertPtrReturn(pchNext, VERR_PARSE_ERROR);

        /* Convert the timestamp string into a number. */
        int rc = RTStrToUInt64Full(pszTimestamp, 0, &u64Timestamp);
        AssertRCSuccessReturn(rc, VERR_PARSE_ERROR);

        pHandle->pchNext = pchNext;
        AssertPtr(pchNext);
    }
    else
    {
        u64Timestamp = 0;
        AssertMsgReturn(!*pszValue && !*pszTimestamp && !*pszFlags,
                        ("'%s' '%s' '%s'\n", pszValue, pszTimestamp, pszFlags),
                        VERR_PARSE_ERROR);
    }

    /*
     * Everything is fine, set the return values.
     */
    if (ppszName)
        *ppszName  = *pszName  != '\0' ? pszName  : NULL;
    if (ppszValue)
        *ppszValue = *pszValue != '\0' ? pszValue : NULL;
    if (pu64Timestamp)
        *pu64Timestamp = u64Timestamp;
    if (ppszFlags)
        *ppszFlags = *pszFlags != '\0' ? pszFlags : NULL;
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


/**
 * Deletes a set of keys.
 *
 * The set is specified in the same way as for VbglR3GuestPropEnum.
 *
 * @returns VBox status code. Stops on first failure.
 *          See also VbglR3GuestPropEnum.
 *
 * @param   u32ClientId     The client id returned by VbglR3InfoSvcConnect().
 * @param   papszPatterns   The patterns against which the properties are
 *                          matched.  Pass NULL if everything should be matched.
 * @param   cPatterns       The number of patterns in @a papszPatterns.  0 means
 *                          match everything.
 */
VBGLR3DECL(int) VbglR3GuestPropDelSet(uint32_t u32ClientId,
                                      const char * const *papszPatterns,
                                      uint32_t cPatterns)
{
    PVBGLR3GUESTPROPENUM pHandle;
    char const *pszName, *pszValue, *pszFlags;
    uint64_t pu64Timestamp;
    int rc = VbglR3GuestPropEnum(u32ClientId,
                                 (char **)papszPatterns, /** @todo fix this cast. */
                                 cPatterns,
                                 &pHandle,
                                 &pszName,
                                 &pszValue,
                                 &pu64Timestamp,
                                 &pszFlags);

    while (RT_SUCCESS(rc) && pszName)
    {
        rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, NULL);
        if (!RT_SUCCESS(rc))
            break;

        rc = VbglR3GuestPropEnumNext(pHandle,
                                     &pszName,
                                     &pszValue,
                                     &pu64Timestamp,
                                     &pszFlags);
    }

    VbglR3GuestPropEnumFree(pHandle);
    return rc;
}


/**
 * Wait for notification of changes to a guest property.  If this is called in
 * a loop, the timestamp of the last notification seen can be passed as a
 * parameter to be sure that no notifications are missed.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, @a ppszName, @a ppszValue,
 *          @a pu64Timestamp and @a ppszFlags containing valid data.
 * @retval  VINF_NOT_FOUND if no previous notification could be found with the
 *          timestamp supplied.  This will normally mean that a large number
 *          of notifications occurred in between.
 * @retval  VERR_BUFFER_OVERFLOW if the scratch buffer @a pvBuf is not large
 *          enough.  In this case the size needed will be placed in
 *          @a pcbBufActual if it is not NULL.
 * @retval  VERR_TIMEOUT if a timeout occurred before a notification was seen.
 *
 * @param   u32ClientId     The client id returned by VbglR3GuestPropConnect().
 * @param   pszPatterns     The patterns that the property names must matchfor
 *                          the change to be reported.
 * @param   pvBuf           A scratch buffer to store the data retrieved into.
 *                          The returned data is only valid for it's lifetime.
 *                          @a ppszValue will point to the start of this buffer.
 * @param   cbBuf           The size of @a pvBuf
 * @param   u64Timestamp    The timestamp of the last event seen.  Pass zero
 *                          to wait for the next event.
 * @param   u32Timeout      Timeout in milliseconds.  Use RT_INDEFINITE_WAIT
 *                          to wait indefinitely.
 * @param   ppszName        Where to store the pointer to the name retrieved.
 *                          Optional.
 * @param   ppszValue       Where to store the pointer to the value retrieved.
 *                          Optional.
 * @param   pu64Timestamp   Where to store the timestamp.  Optional.
 * @param   ppszFlags       Where to store the pointer to the flags.  Optional.
 * @param   pcbBufActual    If @a pcBuf is not large enough, the size needed.
 *                          Optional.
 */
VBGLR3DECL(int) VbglR3GuestPropWait(uint32_t u32ClientId,
                                    const char *pszPatterns,
                                    void *pvBuf, uint32_t cbBuf,
                                    uint64_t u64Timestamp, uint32_t u32Timeout,
                                    char ** ppszName, char **ppszValue,
                                    uint64_t *pu64Timestamp, char **ppszFlags,
                                    uint32_t *pcbBufActual)
{
    /*
     * Create the GET_NOTIFICATION message and call the host.
     */
    GetNotification Msg;

    Msg.hdr.u32Timeout = u32Timeout;
    Msg.hdr.fInterruptible = true;
    Msg.hdr.info.result = (uint32_t)VERR_WRONG_ORDER;  /** @todo drop the cast when the result type has been fixed! */
    Msg.hdr.info.u32ClientID = u32ClientId;
    Msg.hdr.info.u32Function = GET_NOTIFICATION;
    Msg.hdr.info.cParms = 4;
    VbglHGCMParmPtrSetString(&Msg.patterns, pszPatterns);
    Msg.buffer.SetPtr(pvBuf, cbBuf);
    Msg.timestamp.SetUInt64(u64Timestamp);
    Msg.size.SetUInt32(0);

    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_HGCM_CALL_TIMED(sizeof(Msg)), &Msg, sizeof(Msg));
    if (RT_SUCCESS(rc))
        rc = Msg.hdr.info.result;

    /*
     * The cbBufActual parameter is also returned on overflow so the caller can
     * adjust their buffer.
     */
    if (    rc == VERR_BUFFER_OVERFLOW
        ||  pcbBufActual != NULL)
    {
        int rc2 = Msg.size.GetUInt32(pcbBufActual);
        AssertRCReturn(rc2, RT_FAILURE(rc) ? rc : rc2);
    }
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Buffer layout: Name\0Value\0Flags\0.
     *
     * If the caller cares about any of these strings, make sure things are
     * propertly terminated (paranoia).
     */
    if (    RT_SUCCESS(rc)
        &&  (ppszName != NULL || ppszValue != NULL || ppszFlags != NULL))
    {
        /* Validate / skip 'Name'. */
        char *pszValue = (char *)memchr(pvBuf, '\0', cbBuf) + 1;
        AssertPtrReturn(pszValue, VERR_TOO_MUCH_DATA);
        if (ppszName)
            *ppszName = (char *)pvBuf;

        /* Validate / skip 'Value'. */
        char *pszFlags = (char *)memchr(pszValue, '\0',
                                        cbBuf - (pszValue - (char *)pvBuf)) + 1;
        AssertPtrReturn(pszFlags, VERR_TOO_MUCH_DATA);
        if (ppszValue)
            *ppszValue = pszValue;

        if (ppszFlags)
        {
            /* Validate 'Flags'. */
            void *pvEos = memchr(pszFlags, '\0', cbBuf - (pszFlags - (char *)pvBuf));
            AssertPtrReturn(pvEos, VERR_TOO_MUCH_DATA);
            *ppszFlags = pszFlags;
        }
    }

    /* And the timestamp, if requested. */
    if (pu64Timestamp != NULL)
    {
        rc = Msg.timestamp.GetUInt64(pu64Timestamp);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

