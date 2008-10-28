/* $Id$ */
/** @file
 *
 * Testcase for the guest property service.  For now, this only tests
 * flag conversion.
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

/**
 * A list of valid flag strings.  The flag conversion functions should accept
 * these and convert them from string to a flag type and back without errors.
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
    { "", "" },
    { "transient, ", "TRANSIENT" },
    { "  rdOnLyHOST, transIENT  ,     READONLY    ", "TRANSIENT, READONLY" }
};

int testConvertFlags()
{
    int rc = VINF_SUCCESS;
    RTPrintf("tstGuestPropSvc: Testing conversion of valid flags strings.\n");
    for (unsigned i = 0; i < RT_ELEMENTS(validFlagStrings) && RT_SUCCESS(rc); ++i)
    {
        char szFlagBuffer[MAX_FLAGS_LEN];
        uint32_t fFlags;
        rc = validateFlags(validFlagStrings[i].pcszIn, &fFlags);
        if (RT_FAILURE(rc))
            RTPrintf("tstGuestPropSvc: FAILURE - Failed to validate flag string %s.\n", validFlagStrings[i].pcszIn);
        if (RT_SUCCESS(rc))
        {
            rc = writeFlags(fFlags, szFlagBuffer);
            if (RT_FAILURE(rc))
                RTPrintf("tstGuestPropSvc: FAILURE - Failed to convert flag string %s back to a string.\n",
                            validFlagStrings[i].pcszIn);
        }
        if (RT_SUCCESS(rc) && (strlen(szFlagBuffer) > MAX_FLAGS_LEN - 1))
        {
            RTPrintf("tstGuestPropSvc: FAILURE - String %s converts back to a flag string which is too long.\n",
                        validFlagStrings[i].pcszIn);
            rc = VERR_TOO_MUCH_DATA;
        }
        if (RT_SUCCESS(rc) && (strcmp(szFlagBuffer, validFlagStrings[i].pcszOut) != 0))
        {
            RTPrintf("tstGuestPropSvc: FAILURE - String %s converts back to %s instead of to %s\n",
                        validFlagStrings[i].pcszIn, szFlagBuffer,
                        validFlagStrings[i].pcszOut);
            rc = VERR_PARSE_ERROR;
        }
    }
    return rc;
}

int main(int argc, char **argv)
{
    RTR3Init();
    if (RT_FAILURE(testConvertFlags()))
        return 1;
    RTPrintf("tstGuestPropSvc: SUCCEEDED.\n");
    return 0;
}

