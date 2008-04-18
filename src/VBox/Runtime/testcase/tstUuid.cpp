/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - UUID.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/uuid.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/initterm.h>


int main(int argc, char **argv)
{
    int rc;
    int cErrors = 0;

    rc = RTR3Init();
    if (rc)
    {
        RTPrintf("RTR3Init failed: %Vrc\n", rc);
        return 1;
    }

#define CHECK_RC()      \
    do { if (RT_FAILURE(rc)) { RTPrintf("tstUuid(%d): rc=%Vrc!\n", __LINE__, rc); cErrors++; } } while (0)
#define CHECK_EXPR(expr) \
    do { const bool f = !!(expr); if (!f) { RTPrintf("tstUuid(%d): %s!\n", __LINE__, #expr); cErrors++; } } while (0)

    RTUUID UuidNull;
    rc = RTUuidClear(&UuidNull); CHECK_RC();
    CHECK_EXPR(RTUuidIsNull(&UuidNull));
    CHECK_EXPR(RTUuidCompare(&UuidNull, &UuidNull) == 0);

    RTUUID Uuid;
    rc = RTUuidCreate(&Uuid); CHECK_RC();
    CHECK_EXPR(!RTUuidIsNull(&Uuid));
    CHECK_EXPR(RTUuidCompare(&Uuid, &Uuid) == 0);
    CHECK_EXPR(RTUuidCompare(&Uuid, &UuidNull) > 0);
    CHECK_EXPR(RTUuidCompare(&UuidNull, &Uuid) < 0);

    char sz[RTUUID_STR_LENGTH];
    rc = RTUuidToStr(&Uuid, sz, sizeof(sz)); CHECK_RC();
    RTUUID Uuid2;
    rc = RTUuidFromStr(&Uuid2, sz); CHECK_RC();
    CHECK_EXPR(RTUuidCompare(&Uuid, &Uuid2) == 0);

    RTPrintf("tstUuid: Created {%s}\n", sz);

    /*
     * Check the binary representation.
     */
    RTUUID Uuid3;
    Uuid3.au8[0]  = 0x01;
    Uuid3.au8[1]  = 0x23;
    Uuid3.au8[2]  = 0x45;
    Uuid3.au8[3]  = 0x67;
    Uuid3.au8[4]  = 0x89;
    Uuid3.au8[5]  = 0xab;
    Uuid3.au8[6]  = 0xcd;
    Uuid3.au8[7]  = 0x4f;
    Uuid3.au8[8]  = 0x10;
    Uuid3.au8[9]  = 0xb2;
    Uuid3.au8[10] = 0x54;
    Uuid3.au8[11] = 0x76;
    Uuid3.au8[12] = 0x98;
    Uuid3.au8[13] = 0xba;
    Uuid3.au8[14] = 0xdc;
    Uuid3.au8[15] = 0xfe;
    Uuid3.Gen.u16ClockSeq = (Uuid3.Gen.u16ClockSeq & 0x3fff) | 0x8000;
    Uuid3.Gen.u16TimeHiAndVersion = (Uuid3.Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
    const char *pszUuid3 = "67452301-ab89-4fcd-10b2-547698badcfe";
    rc = RTUuidToStr(&Uuid3, sz, sizeof(sz)); CHECK_RC();
    CHECK_EXPR(strcmp(sz, pszUuid3) == 0);
    rc = RTUuidFromStr(&Uuid, pszUuid3); CHECK_RC();
    CHECK_EXPR(RTUuidCompare(&Uuid, &Uuid3) == 0);
    CHECK_EXPR(memcmp(&Uuid3, &Uuid, sizeof(Uuid)) == 0);

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstUuid: SUCCESS {%s}\n", sz);
    else
        RTPrintf("tstUuid: FAILED - %d errors\n", cErrors);
    return !!cErrors;
}

