/* $Id$ */
/** @file
 * VBoxManage - Implementation of the debugvm command.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
 * Handles the getregisters sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_GetRegisters(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * We take a list of register names (case insensitive).  If 'all' is
     * encountered we'll dump all registers.
     */
    ULONG                       idCpu = 0;
    unsigned                    cRegisters = 0;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--cpu", 'c', RTGETOPT_REQ_UINT32 },
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                idCpu = ValueUnion.u32;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!RTStrICmp(ValueUnion.psz, "all"))
                {
                    com::SafeArray<BSTR> aBstrNames;
                    com::SafeArray<BSTR> aBstrValues;
                    CHECK_ERROR2_RET(pDebugger, GetRegisters(idCpu, ComSafeArrayAsOutParam(aBstrNames), ComSafeArrayAsOutParam(aBstrValues)),
                                     RTEXITCODE_FAILURE);
                    Assert(aBstrNames.size() == aBstrValues.size());

                    size_t cchMaxName  = 8;
                    for (size_t i = 0; i < aBstrNames.size(); i++)
                    {
                        size_t cchName = RTUtf16Len(aBstrNames[i]);
                        if (cchName > cchMaxName)
                            cchMaxName = cchName;
                    }

                    for (size_t i = 0; i < aBstrNames.size(); i++)
                        RTPrintf("%-*ls = %ls\n", cchMaxName, aBstrNames[i], aBstrValues[i]);
                }
                else
                {
                    com::Bstr bstrName = ValueUnion.psz;
                    com::Bstr bstrValue;
                    CHECK_ERROR2_RET(pDebugger, GetRegister(idCpu, bstrName.raw(), bstrValue.asOutParam()), RTEXITCODE_FAILURE);
                    RTPrintf("%s = %ls\n", ValueUnion.psz, bstrValue.raw());
                }
                cRegisters++;
                break;

            default:
                return errorGetOpt(USAGE_DEBUGVM, rc, &ValueUnion);
        }
    }

    if (!cRegisters)
        return errorSyntax(USAGE_DEBUGVM, "The getregisters sub-command takes at least one register name");
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the info sub-command.
 *
 * @returns Suitable exit code.
 * @param   a                   The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_Info(HandlerArg *a, IMachineDebugger *pDebugger)
{
    if (a->argc < 3 || a->argc > 4)
        return errorSyntax(USAGE_DEBUGVM, "The inject sub-command takes at one or two arguments");

    com::Bstr bstrName(a->argv[2]);
    com::Bstr bstrArgs(a->argv[3]);
    com::Bstr bstrInfo;
    CHECK_ERROR2_RET(pDebugger, Info(bstrName.raw(), bstrArgs.raw(), bstrInfo.asOutParam()), RTEXITCODE_FAILURE);
    RTPrintf("%ls", bstrInfo.raw());
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the inject sub-command.
 *
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
 * Handles the log sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 * @param   pszSubCmd           The sub command.
 */
static RTEXITCODE handleDebugVM_LogXXXX(HandlerArg *pArgs, IMachineDebugger *pDebugger, const char *pszSubCmd)
{
    /*
     * Parse arguments.
     */
    bool                        fRelease = false;
    com::Utf8Str                strSettings;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;

    /** @todo Put short options into enum / defines! */
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--debug",        'd', RTGETOPT_REQ_NOTHING },
        { "--release",      'r', RTGETOPT_REQ_NOTHING }
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'r':
                fRelease = true;
                break;

            case 'd':
                fRelease = false;
                break;

            /* Because log strings can start with "-" (like "-all+dev_foo")
             * we have to take everything we got as a setting and apply it.
             * IPRT will take care of the validation afterwards. */
            default:
                if (strSettings.length() == 0)
                    strSettings = ValueUnion.psz;
                else
                {
                    strSettings.append(' ');
                    strSettings.append(ValueUnion.psz);
                }
                break;
        }
    }

    if (fRelease)
    {
        com::Utf8Str strTmp(strSettings);
        strSettings = "release: ";
        strSettings.append(strTmp);
    }

    com::Bstr bstrSettings(strSettings);
    if (!strcmp(pszSubCmd, "log"))
        CHECK_ERROR2_RET(pDebugger, ModifyLogGroups(bstrSettings.raw()), RTEXITCODE_FAILURE);
    else if (!strcmp(pszSubCmd, "logdest"))
        CHECK_ERROR2_RET(pDebugger, ModifyLogDestinations(bstrSettings.raw()), RTEXITCODE_FAILURE);
    else if (!strcmp(pszSubCmd, "logflags"))
        CHECK_ERROR2_RET(pDebugger, ModifyLogFlags(bstrSettings.raw()), RTEXITCODE_FAILURE);
    else
        AssertFailedReturn(RTEXITCODE_FAILURE);

    return RTEXITCODE_SUCCESS;
}


/**
 * Handles the inject sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_DumpVMCore(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * Parse arguments.
     */
    const char                 *pszFilename = NULL;
    const char                 *pszCompression = NULL;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--filename",     'f', RTGETOPT_REQ_STRING },
        { "--compression",  'c', RTGETOPT_REQ_STRING }
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, 0 /*fFlags*/);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                if (pszCompression)
                    return errorSyntax(USAGE_DEBUGVM, "The --compression option has already been given");
                pszCompression = ValueUnion.psz;
                break;
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
    com::Bstr bstrCompression(pszCompression);
    CHECK_ERROR2_RET(pDebugger, DumpGuestCore(bstrFilename.raw(), bstrCompression.raw()), RTEXITCODE_FAILURE);
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the os sub-command.
 *
 * @returns Suitable exit code.
 * @param   a                   The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_OSDetect(HandlerArg *a, IMachineDebugger *pDebugger)
{
    if (a->argc != 2)
        return errorSyntax(USAGE_DEBUGVM, "The osdetect sub-command does not take any arguments");

    com::Bstr bstrName;
    CHECK_ERROR2_RET(pDebugger, DetectOS(bstrName.asOutParam()), RTEXITCODE_FAILURE);
    RTPrintf("Detected: %ls\n", bstrName.raw());
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the os sub-command.
 *
 * @returns Suitable exit code.
 * @param   a                   The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_OSInfo(HandlerArg *a, IMachineDebugger *pDebugger)
{
    if (a->argc != 2)
        return errorSyntax(USAGE_DEBUGVM, "The osinfo sub-command does not take any arguments");

    com::Bstr bstrName;
    CHECK_ERROR2_RET(pDebugger, COMGETTER(OSName)(bstrName.asOutParam()), RTEXITCODE_FAILURE);
    com::Bstr bstrVersion;
    CHECK_ERROR2_RET(pDebugger, COMGETTER(OSVersion)(bstrVersion.asOutParam()), RTEXITCODE_FAILURE);
    RTPrintf("Name:    %ls\n", bstrName.raw());
    RTPrintf("Version: %ls\n", bstrVersion.raw());
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the setregisters sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_SetRegisters(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * We take a list of register assignments, that is register=value.
     */
    ULONG                       idCpu = 0;
    com::SafeArray<IN_BSTR>     aBstrNames;
    com::SafeArray<IN_BSTR>     aBstrValues;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--cpu", 'c', RTGETOPT_REQ_UINT32 },
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                idCpu = ValueUnion.u32;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                const char *pszEqual = strchr(ValueUnion.psz, '=');
                if (!pszEqual)
                    return errorSyntax(USAGE_DEBUGVM, "setregisters expects input on the form 'register=value' got '%s'", ValueUnion.psz);
                try
                {
                    com::Bstr bstrName(ValueUnion.psz, pszEqual - ValueUnion.psz);
                    com::Bstr bstrValue(pszEqual + 1);
                    if (   !aBstrNames.push_back(bstrName.raw())
                        || !aBstrValues.push_back(bstrValue.raw()))
                        throw std::bad_alloc();
                }
                catch (std::bad_alloc)
                {
                    RTMsgError("Out of memory\n");
                    return RTEXITCODE_FAILURE;
                }
                break;
            }

            default:
                return errorGetOpt(USAGE_DEBUGVM, rc, &ValueUnion);
        }
    }

    if (!aBstrNames.size())
        return errorSyntax(USAGE_DEBUGVM, "The setregisters sub-command takes at least one register name");

    /*
     * If it is only one register, use the single register method just so
     * we expose it and can test it from the command line.
     */
    if (aBstrNames.size() == 1)
    {
        CHECK_ERROR2_RET(pDebugger, SetRegister(idCpu, aBstrNames[0], aBstrValues[0]), RTEXITCODE_FAILURE);
        RTPrintf("Successfully set %ls\n", aBstrNames[0]);
    }
    else
    {
        CHECK_ERROR2_RET(pDebugger, SetRegisters(idCpu, ComSafeArrayAsInParam(aBstrNames), ComSafeArrayAsInParam(aBstrValues)), RTEXITCODE_FAILURE);
        RTPrintf("Successfully set %u registers\n", aBstrNames.size());
    }

    return RTEXITCODE_SUCCESS;
}

/** @name debugvm show flags
 * @{ */
#define DEBUGVM_SHOW_FLAGS_HUMAN_READABLE   UINT32_C(0x00000000)
#define DEBUGVM_SHOW_FLAGS_SH_EXPORT        UINT32_C(0x00000001)
#define DEBUGVM_SHOW_FLAGS_SH_EVAL          UINT32_C(0x00000002)
#define DEBUGVM_SHOW_FLAGS_CMD_SET          UINT32_C(0x00000003)
#define DEBUGVM_SHOW_FLAGS_FMT_MASK         UINT32_C(0x00000003)
/** @} */

/**
 * Prints a variable according to the @a fFlags.
 *
 * @param   pszVar              The variable name.
 * @param   pbstrValue          The variable value.
 * @param   fFlags              The debugvm show flags.
 */
static void handleDebugVM_Show_PrintVar(const char *pszVar, com::Bstr const *pbstrValue, uint32_t fFlags)
{
    switch (fFlags & DEBUGVM_SHOW_FLAGS_FMT_MASK)
    {
        case DEBUGVM_SHOW_FLAGS_HUMAN_READABLE: RTPrintf(" %27s=%ls\n", pszVar, pbstrValue->raw()); break;
        case DEBUGVM_SHOW_FLAGS_SH_EXPORT:      RTPrintf("export %s='%ls'\n", pszVar, pbstrValue->raw()); break;
        case DEBUGVM_SHOW_FLAGS_SH_EVAL:        RTPrintf("%s='%ls'\n", pszVar, pbstrValue->raw()); break;
        case DEBUGVM_SHOW_FLAGS_CMD_SET:        RTPrintf("set %s=%ls\n", pszVar, pbstrValue->raw()); break;
        default: AssertFailed();
    }
}

/**
 * Handles logdbg-settings.
 *
 * @returns Exit code.
 * @param   pDebugger           The debugger interface.
 * @param   fFlags              The debugvm show flags.
 */
static RTEXITCODE handleDebugVM_Show_LogDbgSettings(IMachineDebugger *pDebugger, uint32_t fFlags)
{
    if ((fFlags & DEBUGVM_SHOW_FLAGS_FMT_MASK) == DEBUGVM_SHOW_FLAGS_HUMAN_READABLE)
        RTPrintf("Debug logger settings:\n");

    com::Bstr bstr;
    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogDbgFlags)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_LOG", &bstr, fFlags);

    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogDbgGroups)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_LOG_FLAGS", &bstr, fFlags);

    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogDbgDestinations)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_LOG_DEST", &bstr, fFlags);
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles logrel-settings.
 *
 * @returns Exit code.
 * @param   pDebugger           The debugger interface.
 * @param   fFlags              The debugvm show flags.
 */
static RTEXITCODE handleDebugVM_Show_LogRelSettings(IMachineDebugger *pDebugger, uint32_t fFlags)
{
    if ((fFlags & DEBUGVM_SHOW_FLAGS_FMT_MASK) == DEBUGVM_SHOW_FLAGS_HUMAN_READABLE)
        RTPrintf("Release logger settings:\n");

    com::Bstr bstr;
    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogRelFlags)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_RELEASE_LOG", &bstr, fFlags);

    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogRelGroups)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_RELEASE_LOG_FLAGS", &bstr, fFlags);

    CHECK_ERROR2_RET(pDebugger, COMGETTER(LogRelDestinations)(bstr.asOutParam()), RTEXITCODE_FAILURE);
    handleDebugVM_Show_PrintVar("VBOX_RELEASE_LOG_DEST", &bstr, fFlags);
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the show sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_Show(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * Parse arguments and what to show.  Order dependent.
     */
    uint32_t                    fFlags = DEBUGVM_SHOW_FLAGS_HUMAN_READABLE;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--human-readable", 'H', RTGETOPT_REQ_NOTHING },
        { "--sh-export",      'e', RTGETOPT_REQ_NOTHING },
        { "--sh-eval",        'E', RTGETOPT_REQ_NOTHING },
        { "--cmd-set",        's', RTGETOPT_REQ_NOTHING  },
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, 0 /*fFlags*/);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'H':
                fFlags = (fFlags & ~DEBUGVM_SHOW_FLAGS_FMT_MASK) | DEBUGVM_SHOW_FLAGS_HUMAN_READABLE;
                break;

            case 'e':
                fFlags = (fFlags & ~DEBUGVM_SHOW_FLAGS_FMT_MASK) | DEBUGVM_SHOW_FLAGS_SH_EXPORT;
                break;

            case 'E':
                fFlags = (fFlags & ~DEBUGVM_SHOW_FLAGS_FMT_MASK) | DEBUGVM_SHOW_FLAGS_SH_EVAL;
                break;

            case 's':
                fFlags = (fFlags & ~DEBUGVM_SHOW_FLAGS_FMT_MASK) | DEBUGVM_SHOW_FLAGS_CMD_SET;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                RTEXITCODE rcExit;
                if (!strcmp(ValueUnion.psz, "log-settings"))
                {
                    rcExit = handleDebugVM_Show_LogDbgSettings(pDebugger, fFlags);
                    if (rcExit == RTEXITCODE_SUCCESS)
                        rcExit = handleDebugVM_Show_LogRelSettings(pDebugger, fFlags);
                }
                else if (!strcmp(ValueUnion.psz, "logdbg-settings"))
                    rcExit = handleDebugVM_Show_LogDbgSettings(pDebugger, fFlags);
                else if (!strcmp(ValueUnion.psz, "logrel-settings"))
                    rcExit = handleDebugVM_Show_LogRelSettings(pDebugger, fFlags);
                else
                    rcExit = errorSyntax(USAGE_DEBUGVM, "The show sub-command has no idea what '%s' might be", ValueUnion.psz);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            default:
                return errorGetOpt(USAGE_DEBUGVM, rc, &ValueUnion);
        }
    }
    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the statistics sub-command.
 *
 * @returns Suitable exit code.
 * @param   pArgs               The handler arguments.
 * @param   pDebugger           Pointer to the debugger interface.
 */
static RTEXITCODE handleDebugVM_Statistics(HandlerArg *pArgs, IMachineDebugger *pDebugger)
{
    /*
     * Parse arguments.
     */
    bool                        fWithDescriptions   = false;
    const char                 *pszPattern          = NULL; /* all */
    bool                        fReset              = false;

    RTGETOPTSTATE               GetState;
    RTGETOPTUNION               ValueUnion;
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--descriptions", 'd', RTGETOPT_REQ_NOTHING },
        { "--pattern",      'p', RTGETOPT_REQ_STRING  },
        { "--reset",        'r', RTGETOPT_REQ_NOTHING  },
    };
    int rc = RTGetOptInit(&GetState, pArgs->argc, pArgs->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 2, 0 /*fFlags*/);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'd':
                fWithDescriptions = true;
                break;

            case 'p':
                if (pszPattern)
                    return errorSyntax(USAGE_DEBUGVM, "Multiple --pattern options are not permitted");
                pszPattern = ValueUnion.psz;
                break;

            case 'r':
                fReset = true;
                break;

            default:
                return errorGetOpt(USAGE_DEBUGVM, rc, &ValueUnion);
        }
    }

    if (fReset && fWithDescriptions)
        return errorSyntax(USAGE_DEBUGVM, "The --reset and --descriptions options does not mix");

    /*
     * Execute the order.
     */
    com::Bstr bstrPattern(pszPattern);
    if (fReset)
        CHECK_ERROR2_RET(pDebugger, ResetStats(bstrPattern.raw()), RTEXITCODE_FAILURE);
    else
    {
        com::Bstr bstrStats;
        CHECK_ERROR2_RET(pDebugger, GetStats(bstrPattern.raw(), fWithDescriptions, bstrStats.asOutParam()),
                         RTEXITCODE_FAILURE);
        /* if (fFormatted)
         { big mess }
         else
         */
        RTPrintf("%ls\n", bstrStats.raw());
    }

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
            if (!strcmp(pszSubCmd, "dumpguestcore"))
                rcExit = handleDebugVM_DumpVMCore(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "getregisters"))
                rcExit = handleDebugVM_GetRegisters(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "info"))
                rcExit = handleDebugVM_Info(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "injectnmi"))
                rcExit = handleDebugVM_InjectNMI(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "log"))
                rcExit = handleDebugVM_LogXXXX(pArgs, ptrDebugger, pszSubCmd);
            else if (!strcmp(pszSubCmd, "logdest"))
                rcExit = handleDebugVM_LogXXXX(pArgs, ptrDebugger, pszSubCmd);
            else if (!strcmp(pszSubCmd, "logflags"))
                rcExit = handleDebugVM_LogXXXX(pArgs, ptrDebugger, pszSubCmd);
            else if (!strcmp(pszSubCmd, "osdetect"))
                rcExit = handleDebugVM_OSDetect(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "osinfo"))
                rcExit = handleDebugVM_OSInfo(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "setregisters"))
                rcExit = handleDebugVM_SetRegisters(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "show"))
                rcExit = handleDebugVM_Show(pArgs, ptrDebugger);
            else if (!strcmp(pszSubCmd, "statistics"))
                rcExit = handleDebugVM_Statistics(pArgs, ptrDebugger);
            else
                errorSyntax(USAGE_DEBUGVM, "Invalid parameter '%s'", pArgs->argv[1]);
        }
    }

    pArgs->session->UnlockMachine();

    return rcExit;
}

