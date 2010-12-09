/* $Id$ */
/** @file
 * VBoxManage - Implementation of the debugvm command.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <iprt/ctype.h>
#include <VBox/err.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/log.h>

#include "VBoxManage.h"


/**
 * Handles the inject sub-command.
 * @returns Suitable exit code.
 * @param   a                   The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_InjectNMI(HandlerArg *a, IMachineDebugger *pDebugger)
{
    if (a->argc != 2)
        return errorSyntax(USAGE_DEBUGVM, "The inject sub-command does not take any arguments");
    CHECK_ERROR2_RET(pDebugger, InjectNMI(), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the inject sub-command.
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_DumpVMCore(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * Parse arguments.
     */
    const char                 *pszFilename  = NULL;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING }
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, 0 /*fFlags*/);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'f':
                if (pszFilename)
                    return errorSyntax(USAGE_DEBUGVM, "The --filename option has already been given");
                pszFilename = ValueUnion.psz;
                break;
            default:
                return errorGetOpt(USAGE_DEBUGVM, rc, &ValueUnion);
        }
    }

    if (!pszFilename)
        return errorSyntax(USAGE_DEBUGVM, "The --filename option is required");

    /*
     * Make the filename absolute before handing it on to the API.
     */
    char szAbsFilename[RTPATH_MAX];
    rc = RTPathAbs(pszFilename, szAbsFilename, sizeof(szAbsFilename));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs failed on '%s': %Rrc", pszFilename, rc);

    com::Bstr bstrFilename(szAbsFilename);
    CHECK_ERROR2_RET(pDebugger, DumpGuestCore(bstrFilename.raw()), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}

int handleDebugVM(HandlerArg *pArgs)
{
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;

    /*
     * The first argument is the VM name or UUID.  Open a session to it.
     */
    if (pArgs->argc < 2)
        return errorSyntax(USAGE_DEBUGVM, "Too few parameters");
    ComPtr<IMachine> ptrMachine;
    CHECK_ERROR2_RET(pArgs->virtualBox, FindMachine(com::Bstr(pArgs->argv[0]).raw(), ptrMachine.asOutParam()), RTEXITCODE_FAILURE);
    CHECK_ERROR2_RET(ptrMachine, LockMachine(pArgs->session, LockType_Shared), RTEXITCODE_FAILURE);

    /*
     * Get the associated console and machine debugger.
     */
    HRESULT rc;
    ComPtr<IConsole> ptrConsole;
    CHECK_ERROR(pArgs->session, COMGETTER(Console)(ptrConsole.asOutParam()));
    if (SUCCEEDED(rc))
    {
        ComPtr<IMachineDebugger> ptrDebugger;
        CHECK_ERROR(ptrConsole, COMGETTER(Debugger)(ptrDebugger.asOutParam()));
        if (SUCCEEDED(rc))
        {
            /*
             * String switch on the sub-command.
             */
            const char *pszSubCmd = pArgs->argv[1];
            if (!strcmp(pszSubCmd, "injectnmi"))
                rcExit = handleDebugVM_InjectNMI(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "dumpguestcore"))
                rcExit = handleDebugVM_DumpVMCore(pArgs, ptrDebugger);
            else
                errorSyntax(USAGE_DEBUGVM, "Invalid parameter '%s'", pArgs->argv[1]);
        }
    }

    pArgs->session->UnlockMachine();

    return rcExit;
}


