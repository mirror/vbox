/* $Id$ */
/** @file
 * Shared Clipboard host service test case.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "../VBoxClipboard.h"

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <VBox/vmm/ssm.h>

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/test.h>

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable);

static int setupTable(VBOXHGCMSVCFNTABLE *pTable)
{
    pTable->cbSize = sizeof(*pTable);
    pTable->u32Version = VBOX_HGCM_SVC_VERSION;
    return VBoxHGCMSvcLoad(pTable);
}

static struct
{
    uint32_t cParms;
    uint32_t type1;
    uint32_t val1;
    int rcExp;
} s_testHostFnSetMode[] =
{
    { 0, VBOX_HGCM_SVC_PARM_32BIT, 99, VERR_INVALID_PARAMETER },
    { 2, VBOX_HGCM_SVC_PARM_32BIT, 99, VERR_INVALID_PARAMETER },
    { 1, VBOX_HGCM_SVC_PARM_64BIT, 99, VERR_INVALID_PARAMETER },
    { 1, VBOX_HGCM_SVC_PARM_32BIT, 99, VINF_SUCCESS }
};

static void testHostCall(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE table;

    RTTestSub(hTest, "Testing HOST_FN_SET_MODE");
    for (unsigned i = 0; i < RT_ELEMENTS(s_testHostFnSetMode); ++i)
    {
        struct VBOXHGCMSVCPARM parms[2];

        int rc = setupTable(&table);
        RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("i=%u, rc=%Rrc\n", i, rc));
        parms[0].type = s_testHostFnSetMode[i].type1;
        parms[0].u.uint32 = s_testHostFnSetMode[i].val1;
        rc = table.pfnHostCall(NULL, VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE,
                               s_testHostFnSetMode[i].cParms, parms);
        RTTESTI_CHECK_MSG(rc == s_testHostFnSetMode[i].rcExp,
                          ("i=%u, rc=%Rrc\n", i, rc));
    }
}


int main(int argc, char *argv[])
{
    /*
     * Init the runtime, test and say hello.
     */
    const char *pcszExecName;
    NOREF(argc);
    pcszExecName = strrchr(argv[0], '/');
    pcszExecName = pcszExecName ? pcszExecName + 1 : argv[0];
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate(pcszExecName, &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Run the tests.
     */
    testHostCall(hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

int vboxClipboardInit() { return VINF_SUCCESS; }
void vboxClipboardDestroy() { AssertFailed(); }
void vboxClipboardDisconnect(_VBOXCLIPBOARDCLIENTDATA*) { AssertFailed(); }
int vboxClipboardConnect(_VBOXCLIPBOARDCLIENTDATA*)
{ AssertFailed(); return VERR_WRONG_ORDER; }
void vboxClipboardFormatAnnounce(_VBOXCLIPBOARDCLIENTDATA*, unsigned int)
{ AssertFailed(); }
int vboxClipboardReadData(_VBOXCLIPBOARDCLIENTDATA*, unsigned int, void*, unsigned int, unsigned int*)
{ AssertFailed(); return VERR_WRONG_ORDER; }
void vboxClipboardWriteData(_VBOXCLIPBOARDCLIENTDATA*, void*, unsigned int, unsigned int) { AssertFailed(); }
VMMR3DECL(int) SSMR3PutU32(PSSMHANDLE pSSM, uint32_t u32)
{ AssertFailed(); return VERR_WRONG_ORDER; }
VMMR3DECL(int) SSMR3PutStructEx(PSSMHANDLE pSSM, const void *pvStruct, size_t cbStruct, uint32_t fFlags, PCSSMFIELD paFields, void *pvUser)
{ AssertFailed(); return VERR_WRONG_ORDER; }
VMMR3DECL(int) SSMR3GetU32(PSSMHANDLE pSSM, uint32_t *pu32)
{ AssertFailed(); return VERR_WRONG_ORDER; }
VMMR3DECL(uint32_t)     SSMR3HandleHostBits(PSSMHANDLE pSSM)
{ AssertFailed(); return 0; }
VMMR3DECL(int) SSMR3GetStructEx(PSSMHANDLE pSSM, void *pvStruct, size_t cbStruct, uint32_t fFlags, PCSSMFIELD paFields, void *pvUser)
{ AssertFailed(); return VERR_WRONG_ORDER; }
int vboxClipboardSync(_VBOXCLIPBOARDCLIENTDATA*)
{ AssertFailed(); return VERR_WRONG_ORDER; }
