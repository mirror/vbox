/* $Id$ */
/** @file
 *
 * Testcase for the guest property service.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <VBox/HostServices/GuestPropertySvc.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>

using namespace guestProp;

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable);

/** Simple call handle structure for the guest call completion callback */
struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code */
    int32_t rc;
};

/** Call completion callback for guest calls. */
static void callComplete(VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    callHandle->rc = rc;
}

/**
 * Initialise the HGCM service table as much as we need to start the
 * service
 * @param  pTable the table to initialise
 */
void initTable(VBOXHGCMSVCFNTABLE *pTable, VBOXHGCMSVCHELPERS *pHelpers)
{
    pTable->cbSize = sizeof (VBOXHGCMSVCFNTABLE);
    pTable->u32Version = VBOX_HGCM_SVC_VERSION;
    pHelpers->pfnCallComplete = callComplete;
    pTable->pHelpers = pHelpers;
}

/**
 * A list of valid flag strings for testConvertFlags.  The flag conversion
 * functions should accept these and convert them from string to a flag type
 * and back without errors.
 */
struct flagStrings
{
    /** Flag string in a format the functions should recognise */
    const char *pcszIn;
    /** How the functions should output the string again */
    const char *pcszOut;
}
validFlagStrings[] =
{
    { "  ", "" },
    { "transient, ", "TRANSIENT" },
    { "  rdOnLyHOST, transIENT  ,     READONLY    ", "TRANSIENT, READONLY" },
    { " rdonlyguest", "RDONLYGUEST" },
    { "rdonlyhost     ", "RDONLYHOST" }
};

/**
 * A list of invalid flag strings for testConvertFlags.  The flag conversion
 * functions should reject these.
 */
const char *invalidFlagStrings[] =
{
    "RDONLYHOST,,",
    "  TRANSIENT READONLY"
};

/**
 * Test the flag conversion functions.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testConvertFlags()
{
    int rc = VINF_SUCCESS;
    RTPrintf("tstGuestPropSvc: Testing conversion of valid flags strings.\n");
    for (unsigned i = 0; i < RT_ELEMENTS(validFlagStrings) && RT_SUCCESS(rc); ++i)
    {
        char szFlagBuffer[MAX_FLAGS_LEN * 2];
        uint32_t fFlags;
        rc = validateFlags(validFlagStrings[i].pcszIn, &fFlags);
        if (RT_FAILURE(rc))
            RTPrintf("tstGuestPropSvc: FAILURE - Failed to validate flag string '%s'.\n", validFlagStrings[i].pcszIn);
        if (RT_SUCCESS(rc))
        {
            rc = writeFlags(fFlags, szFlagBuffer);
            if (RT_FAILURE(rc))
                RTPrintf("tstGuestPropSvc: FAILURE - Failed to convert flag string '%s' back to a string.\n",
                            validFlagStrings[i].pcszIn);
        }
        if (RT_SUCCESS(rc) && (strlen(szFlagBuffer) > MAX_FLAGS_LEN - 1))
        {
            RTPrintf("tstGuestPropSvc: FAILURE - String '%s' converts back to a flag string which is too long.\n",
                        validFlagStrings[i].pcszIn);
            rc = VERR_TOO_MUCH_DATA;
        }
        if (RT_SUCCESS(rc) && (strcmp(szFlagBuffer, validFlagStrings[i].pcszOut) != 0))
        {
            RTPrintf("tstGuestPropSvc: FAILURE - String '%s' converts back to '%s' instead of to '%s'\n",
                        validFlagStrings[i].pcszIn, szFlagBuffer,
                        validFlagStrings[i].pcszOut);
            rc = VERR_PARSE_ERROR;
        }
    }
    if (RT_SUCCESS(rc))
    {
        RTPrintf("Testing rejection of invalid flags strings.\n");
        for (unsigned i = 0; i < RT_ELEMENTS(invalidFlagStrings) && RT_SUCCESS(rc); ++i)
        {
            uint32_t fFlags;
            /* This is required to fail. */
            if (RT_SUCCESS(validateFlags(invalidFlagStrings[i], &fFlags)))
            {
                RTPrintf("String '%s' was incorrectly accepted as a valid flag string.\n",
                         invalidFlagStrings[i]);
                rc = VERR_PARSE_ERROR;
            }
        }
    }
    if (RT_SUCCESS(rc))
    {
        char szFlagBuffer[MAX_FLAGS_LEN * 2];
        uint32_t u32BadFlags = ALLFLAGS << 1;
        RTPrintf("Testing rejection of an invalid flags field.\n");
        /* This is required to fail. */
        if (RT_SUCCESS(writeFlags(u32BadFlags, szFlagBuffer)))
        {
            RTPrintf("Flags 0x%x were incorrectly written out as '%.*s'\n",
                     u32BadFlags, MAX_FLAGS_LEN, szFlagBuffer);
            rc = VERR_PARSE_ERROR;
        }
    }
    return rc;
}

/**
 * List of property names for testSetPropsHost.
 */
const char *apcszNameBlock[] =
{
    "test/name/",
    "test name",
    "TEST NAME",
    "/test/name",
    NULL
};

/**
 * List of property values for testSetPropsHost.
 */
const char *apcszValueBlock[] =
{
    "test/value/",
    "test value",
    "TEST VALUE",
    "/test/value",
    NULL
};

/**
 * List of property timestamps for testSetPropsHost.
 */
uint64_t au64TimestampBlock[] =
{
    0, 999, 999999, UINT64_C(999999999999), 0
};

/**
 * List of property flags for testSetPropsHost.
 */
const char *apcszFlagsBlock[] =
{
    "",
    "readonly, transient",
    "RDONLYHOST",
    "RdOnlyGuest",
    NULL
};

/**
 * Test the SET_PROPS_HOST function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testSetPropsHost(VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;
    RTPrintf("Testing the SET_PROPS_HOST call.\n");
    if (!VALID_PTR(ptable->pfnHostCall))
    {
        RTPrintf("Invalid pfnHostCall() pointer\n");
        rc = VERR_INVALID_POINTER;
    }
    if (RT_SUCCESS(rc))
    {
        VBOXHGCMSVCPARM paParms[4];
        paParms[0].setPointer ((void *) apcszNameBlock, 0);
        paParms[1].setPointer ((void *) apcszValueBlock, 0);
        paParms[2].setPointer ((void *) au64TimestampBlock, 0);
        paParms[3].setPointer ((void *) apcszFlagsBlock, 0);
        rc = ptable->pfnHostCall(ptable->pvService, SET_PROPS_HOST, 4,
                                 paParms);
        if (RT_FAILURE(rc))
            RTPrintf("SET_PROPS_HOST call failed with rc=%Rrc\n", rc);
    }
    return rc;
}

/** Result strings for zeroth enumeration test */
static const char *pcchEnumResult0[] =
{
    "test/name/\0test/value/\0""0\0",
    "test name\0test value\0""999\0TRANSIENT, READONLY",
    "TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST",
    "/test/name\0/test/value\0""999999999999\0RDONLYGUEST",
    NULL
};

/** Result string sizes for zeroth enumeration test */
static const uint32_t cchEnumResult0[] =
{
    sizeof("test/name/\0test/value/\0""0\0"),
    sizeof("test name\0test value\0""999\0TRANSIENT, READONLY"),
    sizeof("TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST"),
    sizeof("/test/name\0/test/value\0""999999999999\0RDONLYGUEST"),
    0
};

/**
 * The size of the buffer returned by the zeroth enumeration test -
 * the - 1 at the end is because of the hidden zero terminator
 */
static const uint32_t cchEnumBuffer0 =
sizeof("test/name/\0test/value/\0""0\0\0"
"test name\0test value\0""999\0TRANSIENT, READONLY\0"
"TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST\0"
"/test/name\0/test/value\0""999999999999\0RDONLYGUEST\0\0\0\0\0") - 1;

/** Result strings for first and second enumeration test */
static const char *pcchEnumResult1[] =
{
    "TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST",
    "/test/name\0/test/value\0""999999999999\0RDONLYGUEST",
    NULL
};

/** Result string sizes for first and second enumeration test */
static const uint32_t cchEnumResult1[] =
{
    sizeof("TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST"),
    sizeof("/test/name\0/test/value\0""999999999999\0RDONLYGUEST"),
    0
};

/**
 * The size of the buffer returned by the first enumeration test -
 * the - 1 at the end is because of the hidden zero terminator
 */
static const uint32_t cchEnumBuffer1 =
sizeof("TEST NAME\0TEST VALUE\0""999999\0RDONLYHOST\0"
"/test/name\0/test/value\0""999999999999\0RDONLYGUEST\0\0\0\0\0") - 1;

static const struct enumStringStruct
{
    /** The enumeration pattern to test */
    const char *pcszPatterns;
    /** The size of the pattern string */
    const uint32_t cchPatterns;
    /** The expected enumeration output strings */
    const char **ppcchResult;
    /** The size of the output strings */
    const uint32_t *pcchResult;
    /** The size of the buffer needed for the enumeration */
    const uint32_t cchBuffer;
}
enumStrings[] =
{
    {
        "", sizeof(""),
        pcchEnumResult0,
        cchEnumResult0,
        cchEnumBuffer0
    },
    {
        "/*\0?E*", sizeof("/*\0?E*"),
        pcchEnumResult1,
        cchEnumResult1,
        cchEnumBuffer1
    },
    {
        "/*|?E*", sizeof("/*|?E*"),
        pcchEnumResult1,
        cchEnumResult1,
        cchEnumBuffer1
    }
};

/**
 * Test the ENUM_PROPS_HOST function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testEnumPropsHost(VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;
    RTPrintf("Testing the ENUM_PROPS_HOST call.\n");
    if (!VALID_PTR(ptable->pfnHostCall))
    {
        RTPrintf("Invalid pfnHostCall() pointer\n");
        rc = VERR_INVALID_POINTER;
    }
    for (unsigned i = 0; RT_SUCCESS(rc) && i < RT_ELEMENTS(enumStrings);
         ++i)
    {
        char buffer[2048];
        VBOXHGCMSVCPARM paParms[3];
        paParms[0].setPointer ((void *) enumStrings[i].pcszPatterns,
                               enumStrings[i].cchPatterns);
        paParms[1].setPointer ((void *) buffer,
                               enumStrings[i].cchBuffer - 1);
        AssertBreakStmt(sizeof(buffer) > enumStrings[i].cchBuffer,
                        rc = VERR_INTERNAL_ERROR);
        if (RT_SUCCESS(rc))
        {
            /* This should fail as the buffer is too small. */
            int rc2 = ptable->pfnHostCall(ptable->pvService, ENUM_PROPS_HOST,
                                          3, paParms);
            if (rc2 != VERR_BUFFER_OVERFLOW)
            {
                RTPrintf("ENUM_PROPS_HOST returned %Rrc instead of VERR_BUFFER_OVERFLOW on too small buffer, pattern number %d\n", rc2, i);
                rc = VERR_BUFFER_OVERFLOW;
            }
            else
            {
                uint32_t cchBufferActual;
                rc = paParms[2].getUInt32 (&cchBufferActual);
                if (RT_SUCCESS(rc) && cchBufferActual != enumStrings[i].cchBuffer)
                {
                    RTPrintf("ENUM_PROPS_HOST requested a buffer size of %lu instead of %lu for pattern number %d\n", cchBufferActual, enumStrings[i].cchBuffer, i);
                    rc = VERR_OUT_OF_RANGE;
                }
                else if (RT_FAILURE(rc))
                    RTPrintf("ENUM_PROPS_HOST did not return the required buffer size properly for pattern %d\n", i);
            }
        }
        if (RT_SUCCESS(rc))
        {
            paParms[1].setPointer ((void *) buffer, enumStrings[i].cchBuffer);
            rc = ptable->pfnHostCall(ptable->pvService, ENUM_PROPS_HOST,
                                      3, paParms);
            if (RT_FAILURE(rc))
                RTPrintf("ENUM_PROPS_HOST call failed for pattern %d with rc=%Rrc\n", i, rc);
            else
                /* Look for each of the result strings in the buffer which was returned */
                for (unsigned j = 0; RT_SUCCESS(rc) && enumStrings[i].ppcchResult[j] != NULL;
                     ++j)
                {
                    bool found = false;
                    for (unsigned k = 0; !found && k <   enumStrings[i].cchBuffer
                                                       - enumStrings[i].pcchResult[j];
                         ++k)
                        if (memcmp(buffer + k, enumStrings[i].ppcchResult[j],
                            enumStrings[i].pcchResult[j]) == 0)
                            found = true;
                    if (!found)
                    {
                        RTPrintf("ENUM_PROPS_HOST did not produce the expected output for pattern %d\n",
                                 i);
                        rc = VERR_UNRESOLVED_ERROR;
                    }
                }
        }
    }
    return rc;
}

/** Array of properties for testing SET_PROP_HOST and _GUEST. */
static const struct
{
    /** Property name */
    const char *pcszName;
    /** Property value */
    const char *pcszValue;
    /** Property flags */
    const char *pcszFlags;
    /** Should this be set as the host or the guest? */
    bool isHost;
    /** Should we use SET_PROP or SET_PROP_VALUE? */
    bool useSetProp;
    /** Should this succeed or be rejected with VERR_PERMISSION_DENIED? */
    bool isAllowed;
}
setProperties[] =
{
    { "Red", "Stop!", "transient", false, true, true },
    { "Amber", "Caution!", "", false, false, true },
    { "Green", "Go!", "readonly", true, true, true },
    { "Blue", "What on earth...?", "", true, false, true },
    { "/test/name", "test", "", false, true, false },
    { "TEST NAME", "test", "", true, true, false },
    { "Green", "gone out...", "", false, false, false },
    { "Green", "gone out...", "", true, false, false },
    { NULL, NULL, NULL, false, false, false }
};

/**
 * Test the SET_PROP, SET_PROP_VALUE, SET_PROP_HOST and SET_PROP_VALUE_HOST
 * functions.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testSetProp(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    RTPrintf("Testing the SET_PROP, SET_PROP_VALUE, SET_PROP_HOST and SET_PROP_VALUE_HOST calls.\n");
    for (unsigned i = 0; RT_SUCCESS(rc) && (setProperties[i].pcszName != NULL);
         ++i)
    {
        int command = SET_PROP_VALUE;
        if (setProperties[i].isHost)
        {
            if (setProperties[i].useSetProp)
                command = SET_PROP_HOST;
            else
                command = SET_PROP_VALUE_HOST;
        }
        else if (setProperties[i].useSetProp)
            command = SET_PROP;
        VBOXHGCMSVCPARM paParms[3];
        /* Work around silly constant issues - we ought to allow passing
         * constant strings in the hgcm parameters. */
        char szName[MAX_NAME_LEN];
        char szValue[MAX_VALUE_LEN];
        char szFlags[MAX_FLAGS_LEN];
        RTStrPrintf(szName, sizeof(szName), "%s", setProperties[i].pcszName);
        RTStrPrintf(szValue, sizeof(szValue), "%s", setProperties[i].pcszValue);
        RTStrPrintf(szFlags, sizeof(szFlags), "%s", setProperties[i].pcszFlags);
        paParms[0].setPointer (szName, (uint32_t)strlen(szName) + 1);
        paParms[1].setPointer (szValue, (uint32_t)strlen(szValue) + 1);
        paParms[2].setPointer (szFlags, (uint32_t)strlen(szFlags) + 1);
        if (setProperties[i].isHost)
            callHandle.rc = pTable->pfnHostCall(pTable->pvService, command,
                                                setProperties[i].useSetProp
                                                    ? 3 : 2, paParms);
        else
            pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL, command,
                            setProperties[i].useSetProp ? 3 : 2, paParms);
        if (setProperties[i].isAllowed && RT_FAILURE(callHandle.rc))
        {
            RTPrintf("Setting property '%s' failed with rc=%Rrc.\n",
                     setProperties[i].pcszName, callHandle.rc);
            rc = callHandle.rc;
        }
        else if (   !setProperties[i].isAllowed
                 && (callHandle.rc != VERR_PERMISSION_DENIED)
                )
        {
            RTPrintf("Setting property '%s' returned %Rrc instead of VERR_PERMISSION_DENIED.\n",
                     setProperties[i].pcszName, callHandle.rc);
            rc = VERR_UNRESOLVED_ERROR;
        }
    }
    return rc;
}

/** Array of properties for testing DEL_PROP_HOST and _GUEST. */
static const struct
{
    /** Property name */
    const char *pcszName;
    /** Should this be set as the host or the guest? */
    bool isHost;
    /** Should this succeed or be rejected with VERR_PERMISSION_DENIED? */
    bool isAllowed;
}
delProperties[] =
{
    { "Red", false, true },
    { "Amber", true, true },
    { "Red2", false, true },
    { "Amber2", true, true },
    { "Green", false, false },
    { "Green", true, false },
    { "/test/name", false, false },
    { "TEST NAME", true, false },
    { NULL, false, false }
};

/**
 * Test the DEL_PROP, and DEL_PROP_HOST functions.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testDelProp(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    RTPrintf("Testing the DEL_PROP and DEL_PROP_HOST calls.\n");
    for (unsigned i = 0; RT_SUCCESS(rc) && (delProperties[i].pcszName != NULL);
         ++i)
    {
        int command = DEL_PROP;
        if (delProperties[i].isHost)
            command = DEL_PROP_HOST;
        VBOXHGCMSVCPARM paParms[1];
        /* Work around silly constant issues - we ought to allow passing
         * constant strings in the hgcm parameters. */
        char szName[MAX_NAME_LEN];
        RTStrPrintf(szName, sizeof(szName), "%s", delProperties[i].pcszName);
        paParms[0].setPointer (szName, (uint32_t)strlen(szName) + 1);
        if (delProperties[i].isHost)
            callHandle.rc = pTable->pfnHostCall(pTable->pvService, command,
                                                1, paParms);
        else
            pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL, command,
                            1, paParms);
        if (delProperties[i].isAllowed && RT_FAILURE(callHandle.rc))
        {
            RTPrintf("Deleting property '%s' failed with rc=%Rrc.\n",
                     delProperties[i].pcszName, callHandle.rc);
            rc = callHandle.rc;
        }
        else if (   !delProperties[i].isAllowed
                 && (callHandle.rc != VERR_PERMISSION_DENIED)
                )
        {
            RTPrintf("Deleting property '%s' returned %Rrc instead of VERR_PERMISSION_DENIED.\n",
                     delProperties[i].pcszName, callHandle.rc);
            rc = VERR_UNRESOLVED_ERROR;
        }
    }
    return rc;
}

/** Array of properties for testing GET_PROP_HOST. */
static const struct
{
    /** Property name */
    const char *pcszName;
    /** What value/flags pattern do we expect back? */
    const char *pcchValue;
    /** What size should the value/flags array be? */
    uint32_t cchValue;
    /** Should this proeprty exist? */
    bool exists;
    /** Do we expect a particular timestamp? */
    bool hasTimestamp;
    /** What timestamp if any do ex expect? */
    uint64_t u64Timestamp;
}
getProperties[] =
{
    { "test/name/", "test/value/\0", sizeof("test/value/\0"), true, true, 0 },
    { "test name", "test value\0TRANSIENT, READONLY",
      sizeof("test value\0TRANSIENT, READONLY"), true, true, 999 },
    { "TEST NAME", "TEST VALUE\0RDONLYHOST", sizeof("TEST VALUE\0RDONLYHOST"),
      true, true, 999999 },
    { "/test/name", "/test/value\0RDONLYGUEST",
      sizeof("/test/value\0RDONLYGUEST"), true, true, UINT64_C(999999999999) },
    { "Green", "Go!\0READONLY", sizeof("Go!\0READONLY"), true, false, 0 },
    { "Blue", "What on earth...?\0", sizeof("What on earth...?\0"), true,
      false, 0 },
    { "Red", "", 0, false, false, 0 },
    { NULL, NULL, 0, false, false, 0 }
};

/**
 * Test the GET_PROP_HOST function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testGetProp(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS, rc2 = VINF_SUCCESS;
    RTPrintf("Testing the GET_PROP_HOST call.\n");
    for (unsigned i = 0; RT_SUCCESS(rc) && (getProperties[i].pcszName != NULL);
         ++i)
    {
        VBOXHGCMSVCPARM paParms[4];
        /* Work around silly constant issues - we ought to allow passing
         * constant strings in the hgcm parameters. */
        char szName[MAX_NAME_LEN] = "";
        char szBuffer[MAX_VALUE_LEN + MAX_FLAGS_LEN];
        AssertBreakStmt(sizeof(szBuffer) >= getProperties[i].cchValue,
                        rc = VERR_INTERNAL_ERROR);
        RTStrPrintf(szName, sizeof(szName), "%s", getProperties[i].pcszName);
        paParms[0].setPointer (szName, (uint32_t)strlen(szName) + 1);
        paParms[1].setPointer (szBuffer, sizeof(szBuffer));
        rc2 = pTable->pfnHostCall(pTable->pvService, GET_PROP_HOST, 4,
                                  paParms);
        if (getProperties[i].exists && RT_FAILURE(rc2))
        {
            RTPrintf("Getting property '%s' failed with rc=%Rrc.\n",
                     getProperties[i].pcszName, rc2);
            rc = rc2;
        }
        else if (!getProperties[i].exists && (rc2 != VERR_NOT_FOUND))
        {
            RTPrintf("Getting property '%s' returned %Rrc instead of VERR_NOT_FOUND.\n",
                     getProperties[i].pcszName, rc2);
            rc = VERR_UNRESOLVED_ERROR;
        }
        if (RT_SUCCESS(rc) && getProperties[i].exists)
        {
            uint32_t u32ValueLen;
            rc = paParms[3].getUInt32 (&u32ValueLen);
            if (RT_FAILURE(rc))
                RTPrintf("Failed to get the size of the output buffer for property '%s'\n",
                         getProperties[i].pcszName);
            if (   RT_SUCCESS(rc)
                && (memcmp(szBuffer, getProperties[i].pcchValue,
                           getProperties[i].cchValue) != 0)
               )
            {
                RTPrintf("Unexpected result '%.*s' for property '%s', expected '%.*s'.\n",
                         u32ValueLen, szBuffer, getProperties[i].pcszName,
                         getProperties[i].cchValue, getProperties[i].pcchValue);
                rc = VERR_UNRESOLVED_ERROR;
            }
            if (RT_SUCCESS(rc) && getProperties[i].hasTimestamp)
            {
                uint64_t u64Timestamp;
                rc = paParms[2].getUInt64 (&u64Timestamp);
                if (RT_FAILURE(rc))
                    RTPrintf("Failed to get the timestamp for property '%s'\n",
                             getProperties[i].pcszName);
                if (   RT_SUCCESS(rc)
                    && (u64Timestamp != getProperties[i].u64Timestamp)
                   )
                {
                    RTPrintf("Bad timestamp %llu for property '%s', expected %llu.\n",
                             u64Timestamp, getProperties[i].pcszName,
                             getProperties[i].u64Timestamp);
                    rc = VERR_UNRESOLVED_ERROR;
                }
            }
        }
    }
    return rc;
}

/** Array of properties for testing GET_PROP_HOST. */
static const struct
{
    /** Buffer returned */
    const char *pchBuffer;
    /** What size should the buffer be? */
    uint32_t cchBuffer;
}
getNotifications[] =
{
    { "Red\0Stop!\0TRANSIENT", sizeof("Red\0Stop!\0TRANSIENT") },
    { "Amber\0Caution!\0", sizeof("Amber\0Caution!\0") },
    { "Green\0Go!\0READONLY", sizeof("Green\0Go!\0READONLY") },
    { "Blue\0What on earth...?\0", sizeof("Blue\0What on earth...?\0") },
    { "Red\0\0", sizeof("Red\0\0") },
    { "Amber\0\0", sizeof("Amber\0\0") },
    { NULL, 0 }
};

/**
 * Test the GET_NOTIFICATION function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testGetNotification(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    char chBuffer[MAX_NAME_LEN + MAX_VALUE_LEN + MAX_FLAGS_LEN];
    static char szPattern[] = "";

    RTPrintf("Testing the GET_NOTIFICATION call.\n");
    uint64_t u64Timestamp;
    uint32_t u32Size = 0;
    VBOXHGCMSVCPARM paParms[4];

    /* Test "buffer too small" */
    u64Timestamp = 1;
    paParms[0].setPointer ((void *) szPattern, sizeof(szPattern));
    paParms[1].setUInt64 (u64Timestamp);
    paParms[2].setPointer ((void *) chBuffer, getNotifications[0].cchBuffer - 1);
    pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL,
                    GET_NOTIFICATION, 4, paParms);
    if (   callHandle.rc != VERR_BUFFER_OVERFLOW
        || RT_FAILURE(paParms[3].getUInt32 (&u32Size))
        || u32Size != getNotifications[0].cchBuffer
       )
    {
        RTPrintf("Getting notification for property '%s' with a too small buffer did not fail correctly.\n",
                 getNotifications[0].pchBuffer);
        rc = VERR_UNRESOLVED_ERROR;
    }

    /* Test successful notification queries.  Start with an unknown timestamp
     * to get the oldest available notification. */
    u64Timestamp = 1;
    for (unsigned i = 0; RT_SUCCESS(rc) && (getNotifications[i].pchBuffer != NULL);
         ++i)
    {
        paParms[0].setPointer ((void *) szPattern, sizeof(szPattern));
        paParms[1].setUInt64 (u64Timestamp);
        paParms[2].setPointer ((void *) chBuffer, sizeof(chBuffer));
        pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL,
                        GET_NOTIFICATION, 4, paParms);
        if (   RT_FAILURE(callHandle.rc)
            || (i == 0 && callHandle.rc != VWRN_NOT_FOUND)
            || RT_FAILURE(paParms[1].getUInt64 (&u64Timestamp))
            || RT_FAILURE(paParms[3].getUInt32 (&u32Size))
            || u32Size != getNotifications[i].cchBuffer
            || memcmp(chBuffer, getNotifications[i].pchBuffer, u32Size) != 0
           )
        {
            RTPrintf("Failed to get notification for property '%s'.\n",
                     getNotifications[i].pchBuffer);
            rc = VERR_UNRESOLVED_ERROR;
        }
    }
    return rc;
}

/** Paramters for the asynchronous guest notification call */
struct asyncNotification_
{
    /** Call parameters */
    VBOXHGCMSVCPARM aParms[4];
    /** Result buffer */
    char chBuffer[MAX_NAME_LEN + MAX_VALUE_LEN + MAX_FLAGS_LEN];
    /** Return value */
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle;
} asyncNotification;

/**
 * Set up the test for the asynchronous GET_NOTIFICATION function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int setupAsyncNotification(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;

    RTPrintf("Testing the asynchronous GET_NOTIFICATION call with no notifications are available.\n");
    uint64_t u64Timestamp = 0;
    uint32_t u32Size = 0;
    static char szPattern[] = "";

    asyncNotification.aParms[0].setPointer ((void *) szPattern, sizeof(szPattern));
    asyncNotification.aParms[1].setUInt64 (u64Timestamp);
    asyncNotification.aParms[2].setPointer ((void *) asyncNotification.chBuffer,
                                            sizeof(asyncNotification.chBuffer));
    asyncNotification.callHandle.rc = VINF_HGCM_ASYNC_EXECUTE;
    pTable->pfnCall(pTable->pvService, &asyncNotification.callHandle, 0, NULL,
                    GET_NOTIFICATION, 4, asyncNotification.aParms);
    if (RT_FAILURE(asyncNotification.callHandle.rc))
    {
        RTPrintf("GET_NOTIFICATION call failed, rc=%Rrc.\n", asyncNotification.callHandle.rc);
        rc = VERR_UNRESOLVED_ERROR;
    }
    else if (asyncNotification.callHandle.rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        RTPrintf("GET_NOTIFICATION call completed when no new notifications should be available.\n");
        rc = VERR_UNRESOLVED_ERROR;
    }
    return rc;
}

/**
 * Test the asynchronous GET_NOTIFICATION function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testAsyncNotification(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;
    uint64_t u64Timestamp;
    uint32_t u32Size;
    if (   asyncNotification.callHandle.rc != VINF_SUCCESS
        || RT_FAILURE(asyncNotification.aParms[1].getUInt64 (&u64Timestamp))
        || RT_FAILURE(asyncNotification.aParms[3].getUInt32 (&u32Size))
        || u32Size != getNotifications[0].cchBuffer
        || memcmp(asyncNotification.chBuffer, getNotifications[0].pchBuffer, u32Size) != 0
       )
    {
        RTPrintf("Asynchronous GET_NOTIFICATION call did not complete as expected, rc=%Rrc\n",
                 asyncNotification.callHandle.rc);
        rc = VERR_UNRESOLVED_ERROR;
    }
    return rc;
}

int main(int argc, char **argv)
{
    VBOXHGCMSVCFNTABLE svcTable;
    VBOXHGCMSVCHELPERS svcHelpers;

    initTable(&svcTable, &svcHelpers);
    RTR3Init();
    if (RT_FAILURE(testConvertFlags()))
        return 1;
    /* The function is inside the service, not HGCM. */
    if (RT_FAILURE(VBoxHGCMSvcLoad(&svcTable)))
    {
        RTPrintf("Failed to start HGCM service.\n");
        return 1;
    }
    if (RT_FAILURE(testSetPropsHost(&svcTable)))
        return 1;
    if (RT_FAILURE(testEnumPropsHost(&svcTable)))
        return 1;
    /* Set up the asynchronous notification test */
    if (RT_FAILURE(setupAsyncNotification(&svcTable)))
        return 1;
    if (RT_FAILURE(testSetProp(&svcTable)))
        return 1;
    RTPrintf("Checking the data returned by the asynchronous notification call.\n");
    /* Our previous notification call should have completed by now. */
    if (RT_FAILURE(testAsyncNotification(&svcTable)))
        return 1;
    if (RT_FAILURE(testDelProp(&svcTable)))
        return 1;
    if (RT_FAILURE(testGetProp(&svcTable)))
        return 1;
    if (RT_FAILURE(testGetNotification(&svcTable)))
        return 1;
    RTPrintf("tstGuestPropSvc: SUCCEEDED.\n");
    return 0;
}
