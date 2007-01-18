/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - UUID.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/uuid.h>
#include <iprt/stream.h>
#include <iprt/err.h>


int main(int argc, char **argv)
{
    int rc;
    int cErrors = 0;

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

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstUuid: SUCCESS {%s}\n", sz);
    else
        RTPrintf("tstUuid: FAILED - %d errors\n", cErrors);
    return !!cErrors;
}

