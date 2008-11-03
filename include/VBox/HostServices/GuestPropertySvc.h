/** @file
 * Guest property service:
 * Common header for host service and guest clients.
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

#ifndef ___VBox_HostService_GuestPropertyService_h
#define ___VBox_HostService_GuestPropertyService_h

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>
#include <VBox/log.h>
#include <iprt/assert.h>

#include <string.h>

#ifdef RT_OS_WINDOWS
# define strncasecmp strnicmp
#endif

/** Everything defined in this file lives in this namespace. */
namespace guestProp {

/******************************************************************************
* Typedefs, constants and inlines                                             *
******************************************************************************/

/** Maximum length for property names */
enum { MAX_NAME_LEN = 64 };
/** Maximum length for property values */
enum { MAX_VALUE_LEN = 128 };
/** Maximum number of properties per guest */
enum { MAX_PROPS = 256 };
/** Maximum size for enumeration patterns */
enum { MAX_PATTERN_LEN = 1024 };

/**
 * The guest property flag values which are currently accepted.
 */
enum ePropFlags
{
    NILFLAG     = 0,
    TRANSIENT   = RT_BIT(1),
    RDONLYGUEST = RT_BIT(2),
    RDONLYHOST  = RT_BIT(3),
    READONLY    = RDONLYGUEST | RDONLYHOST,
    ALLFLAGS    = TRANSIENT | READONLY
};

/**
 * Get the name of a flag as a string.
 * @returns the name, or NULL if fFlag is invalid.
 * @param   fFlag  the flag.  Must be a value from the ePropFlags enumeration
 *                 list.
 */
DECLINLINE(const char *) flagName(uint32_t fFlag)
{
    switch(fFlag)
    {
        case TRANSIENT:
            return "TRANSIENT";
        case RDONLYGUEST:
            return "RDONLYGUEST";
        case RDONLYHOST:
            return "RDONLYHOST";
        case READONLY:
            return "READONLY";
        default:
            return NULL;
    }
}

/**
 * Get the length of a flag name as returned by flagName.
 * @returns the length, or 0 if fFlag is invalid.
 * @param   fFlag  the flag.  Must be a value from the ePropFlags enumeration
 *                 list.
 */
DECLINLINE(size_t) flagNameLen(uint32_t fFlag)
{
    const char *pcszName = flagName(fFlag);
    return RT_LIKELY(pcszName != NULL) ? strlen(pcszName) : 0;
}

/**
 * Maximum length for the property flags field.  We only ever return one of
 * RDONLYGUEST, RDONLYHOST and RDONLY
 */
enum { MAX_FLAGS_LEN =   sizeof("TRANSIENT, RDONLYGUEST") };

/**
 * Parse a guest properties flags string for flag names and make sure that
 * there is no junk text in the string.
 * @returns  IPRT status code
 * @returns  VERR_INVALID_PARAM if the flag string is not valid
 * @param    pcszFlags  the flag string to parse
 * @param    pfFlags    where to store the parse result.  May not be NULL.
 * @note     This function is also inline because it must be accessible from
 *           several modules and it does not seem reasonable to put it into
 *           its own library.
 */
DECLINLINE(int) validateFlags(const char *pcszFlags, uint32_t *pfFlags)
{
    static uint32_t flagList[] =
    {
        TRANSIENT, READONLY, RDONLYGUEST, RDONLYHOST
    };
    const char *pcszNext = pcszFlags;
    int rc = VINF_SUCCESS;
    uint32_t fFlags = 0;
    AssertLogRelReturn(VALID_PTR(pfFlags), VERR_INVALID_POINTER);
    AssertLogRelReturn(VALID_PTR(pcszFlags), VERR_INVALID_POINTER);
    while (' ' == *pcszNext)
        ++pcszNext;
    while ((*pcszNext != '\0') && RT_SUCCESS(rc))
    {
        unsigned i = 0;
        for (; i < RT_ELEMENTS(flagList); ++i)
            if (strncasecmp(pcszNext, flagName(flagList[i]),
                            flagNameLen(flagList[i])
                           ) == 0
               )
                break;
        if (RT_ELEMENTS(flagList) == i)
             rc = VERR_INVALID_PARAMETER;
        else
        {
            fFlags |= flagList[i];
            pcszNext += flagNameLen(flagList[i]);
            while (' ' == *pcszNext)
                ++pcszNext;
            if (',' == *pcszNext)
                ++pcszNext;
            while (' ' == *pcszNext)
                ++pcszNext;
        }
    }
    if (RT_SUCCESS(rc))
        *pfFlags = fFlags;
    return rc;
}

/**
 * Write out flags to a string.
 * @returns  IPRT status code
 * @param    fFlags    the flags to write out
 * @param    pszFlags  where to write the flags string.  This must point to
 *                     a buffer of size (at least) MAX_FLAGS_LEN.
 */
DECLINLINE(int) writeFlags(uint32_t fFlags, char *pszFlags)
{
    static uint32_t flagList[] =
    {
        TRANSIENT, READONLY, RDONLYGUEST, RDONLYHOST
    };
    char *pszNext = pszFlags;
    int rc = VINF_SUCCESS;
    AssertLogRelReturn(VALID_PTR(pszFlags), VERR_INVALID_POINTER);
    if ((fFlags & ~ALLFLAGS) != NILFLAG)
        rc = VERR_INVALID_PARAMETER;
    if (RT_SUCCESS(rc))
    {
        unsigned i = 0;
        for (; i < RT_ELEMENTS(flagList); ++i)
        {
            if (fFlags & flagList[i])
            {
                strcpy(pszNext, flagName(flagList[i]));
                pszNext += flagNameLen(flagList[i]);
                fFlags &= ~flagList[i];
                if (fFlags != NILFLAG)
                {
                    strcpy(pszNext, ", ");
                    pszNext += 2;
                }
            }
        }
        *pszNext = '\0';
        if (fFlags != NILFLAG)
            rc = VERR_INVALID_PARAMETER;  /* But pszFlags will still be set right. */
    }
    return rc;
}

/*
 * The service functions which are callable by host.
 */
enum eHostFn
{
    /** Pass the address of the cfgm node used by the service as a database. */
    SET_CFGM_NODE = 1,
    /**
     * Get the value attached to a guest property
     * The parameter format matches that of GET_PROP.
     */
    GET_PROP_HOST = 2,
    /**
     * Set the value attached to a guest property
     * The parameter format matches that of SET_PROP.
     */
    SET_PROP_HOST = 3,
    /**
     * Set the value attached to a guest property
     * The parameter format matches that of SET_PROP_VALUE.
     */
    SET_PROP_VALUE_HOST = 4,
    /**
     * Remove a guest property.
     * The parameter format matches that of DEL_PROP.
     */
    DEL_PROP_HOST = 5,
    /**
     * Enumerate guest properties.
     * The parameter format matches that of ENUM_PROPS.
     */
    ENUM_PROPS_HOST = 6
};

/**
 * The service functions which are called by guest.  The numbers may not change,
 * so we hardcode them.
 */
enum eGuestFn
{
    /** Get a guest property */
    GET_PROP = 1,
    /** Set a guest property */
    SET_PROP = 2,
    /** Set just the value of a guest property */
    SET_PROP_VALUE = 3,
    /** Delete a guest property */
    DEL_PROP = 4,
    /** Enumerate guest properties */
    ENUM_PROPS = 5
};

/**
 * Data structure to pass to the service extension callback.  We use this to
 * notify the host of changes to properties.
 */
typedef struct _HOSTCALLBACKDATA
{
    /** Magic number to identify the structure */
    uint32_t u32Magic;
    /** The name of the property that was changed */
    const char *pcszName;
    /** The new property value, or NULL if the property was deleted */
    const char *pcszValue;
    /** The timestamp of the modification */
    uint64_t u64Timestamp;
    /** The flags field of the modified property */
    const char *pcszFlags;
} HOSTCALLBACKDATA, *PHOSTCALLBACKDATA;

enum
{
    /** Magic number for sanity checking the HOSTCALLBACKDATA structure */
    HOSTCALLBACKMAGIC = 0x69c87a78
};

/**
 * HGCM parameter structures.  Packing is explicitly defined as this is a wire format.
 */
#pragma pack (1)
/** The guest is requesting the value of a property */
typedef struct _GetProperty
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The property name (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The returned string data will be placed here.  (OUT pointer)
     * This call returns two null-terminated strings which will be placed one
     * after another: value and flags.
     */
    HGCMFunctionParameter buffer;

    /**
     * The property timestamp.  (OUT uint64_t)
     */

    HGCMFunctionParameter timestamp;

    /**
     * If the buffer provided was large enough this will contain the size of
     * the returned data.  Otherwise it will contain the size of the buffer
     * needed to hold the data and VERR_BUFFER_OVERFLOW will be returned.
     * (OUT uint32_t)
     */
    HGCMFunctionParameter size;
} GetProperty;

/** The guest is requesting to change a property */
typedef struct _SetProperty
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The property name.  (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The value of the property (IN pointer)
     * Criteria as for the name parameter, but with length less than or equal to
     * MAX_VALUE_LEN.  
     */
    HGCMFunctionParameter value;

    /**
     * The property flags (IN pointer)
     * This is a comma-separated list of the format flag=value
     * The length must be less than or equal to MAX_FLAGS_LEN and only
     * known flag names and values will be accepted.
     */
    HGCMFunctionParameter flags;
} SetProperty;

/** The guest is requesting to change the value of a property */
typedef struct _SetPropertyValue
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The property name.  (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;

    /**
     * The value of the property (IN pointer)
     * Criteria as for the name parameter, but with length less than or equal to
     * MAX_VALUE_LEN.  
     */
    HGCMFunctionParameter value;
} SetPropertyValue;

/** The guest is requesting to remove a property */
typedef struct _DelProperty
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The property name.  This must fit to a number of criteria, namely
     *  - Only Utf8 strings are allowed
     *  - Less than or equal to MAX_NAME_LEN bytes in length
     *  - Zero terminated
     */
    HGCMFunctionParameter name;
} DelProperty;

/** The guest is requesting to enumerate properties */
typedef struct _EnumProperties
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * Array of patterns to match the properties against, separated by '|'
     * characters.  For backwards compatibility, '\0' is also accepted
     * as a separater.
     * (IN pointer)
     * If only a single, empty pattern is given then match all.
     */
    HGCMFunctionParameter patterns;
    /**
     * On success, null-separated array of strings in which the properties are
     * returned.  (OUT pointer)
     * The number of strings in the array is always a multiple of four,
     * and in sequences of name, value, timestamp (hexadecimal string) and the
     * flags as a comma-separated list in the format "name=value".  The list
     * is terminated by an empty string after a "flags" entry (or at the
     * start).
     */
    HGCMFunctionParameter strings;
    /**
     * On success, the size of the returned data.  If the buffer provided is
     * too small, the size of buffer needed.  (OUT uint32_t)
     */
    HGCMFunctionParameter size;
} EnumProperties;
#pragma pack ()

} /* namespace guestProp */

#endif  /* ___VBox_HostService_GuestPropertySvc_h defined */
