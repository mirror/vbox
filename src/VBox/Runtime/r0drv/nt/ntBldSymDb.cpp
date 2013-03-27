/* $Id$ */
/** @file
 * IPRT - RTDirCreateUniqueNumbered, generic implementation.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <Windows.h>
#include <Dbghelp.h>

#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/err.h>


static BOOL CALLBACK enumTypesCallback(PSYMBOL_INFO pSymInfo, ULONG cbSymbol, PVOID pvUser)
{
    RTPrintf("pSymInfo=%p cbSymbol=%#x SizeOfStruct=%#x\n", pSymInfo, cbSymbol, pSymInfo->SizeOfStruct);
    RTPrintf("  TypeIndex=%#x Reserved[0]=%#llx Reserved[1]=%#llx info=%#x\n",
             pSymInfo->TypeIndex, pSymInfo->Reserved[0], pSymInfo->Reserved[1], pSymInfo->Index);
    RTPrintf("  Size=%#x ModBase=%#llx Flags=%#x Value=%#llx Address=%#llx\n",
             pSymInfo->Size, pSymInfo->ModBase, pSymInfo->Flags, pSymInfo->Value, pSymInfo->Address);
    RTPrintf("  Register=%#x Scope=%#x Tag=%#x NameLen=%#x MaxNameLen=%#x Name=%s\n",
             pSymInfo->Register, pSymInfo->Scope, pSymInfo->Tag, pSymInfo->NameLen, pSymInfo->MaxNameLen, pSymInfo->Name);
    return TRUE;
}


static RTEXITCODE processOnePdb(const char *pszPdb)
{
    /*
     * We need the size later on, so get that now and present proper IPRT error
     * info if the file is missing or inaccessible.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTPathQueryInfo(pszPdb, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathQueryInfo fail on '%s': %Rrc\n", pszPdb, rc);

    /*
     * Create a fake handle and open the PDB.
     */
    static uintptr_t s_iHandle = 0;
    HANDLE hFake = (HANDLE)++s_iHandle;
    if (!SymInitialize(hFake, NULL, FALSE))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "SymInitialied failed: %u\n", GetLastError());

    RTEXITCODE rcExit;
    uint64_t uModAddr = UINT64_C(0x1000000);
    uModAddr = SymLoadModuleEx(hFake, NULL /*hFile*/, pszPdb, NULL /*pszModuleName*/,
                               uModAddr, ObjInfo.cbObject, NULL /*pData*/, 0 /*fFlags*/);
    if (uModAddr != 0)
    {
        RTPrintf("debug: uModAddr=%#llx\n", uModAddr);

        /*
         * Enumerate types.
         */
        if (SymEnumTypes(hFake, uModAddr, enumTypesCallback, NULL))
        {
            rcExit = RTEXITCODE_SUCCESS;
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "SymEnumTypes failed: %u\n", GetLastError());
    }
    if (!SymCleanup(hFake))
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "SymCleanup failed: %u\n", GetLastError());
    return rcExit;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse options.
     */


    /*
     * Do job.
     */
    RTEXITCODE rcExit = processOnePdb(argv[argc - 1]);

    return rcExit;
}
