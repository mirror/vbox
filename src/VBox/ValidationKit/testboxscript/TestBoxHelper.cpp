/* $Id$ */
/** @file
 * VirtualBox Validation Kit - Testbox C Helper Utility.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/buildconfig.h>
#include <iprt/env.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/mp.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/system.h>

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/x86.h>
# include <iprt/asm-amd64-x86.h>
#endif

#ifdef RT_OS_DARWIN
# include <sys/types.h>
# include <sys/sysctl.h>
#endif


/**
 * Generates a kind of report of the hardware, software and whatever else we
 * think might be useful to know about the testbox.
 */
static RTEXITCODE handlerReport(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    /*
     * For now, a simple CPUID dump.  Need to figure out how to share code
     * like this with other bits, putting it in IPRT.
     */
    RTPrintf("CPUID Dump\n"
             "Leaf      eax      ebx      ecx      edx\n"
             "---------------------------------------------\n");
    static uint32_t const s_auRanges[] =
    {
        UINT32_C(0x00000000),
        UINT32_C(0x80000000),
        UINT32_C(0x80860000),
        UINT32_C(0xc0000000),
        UINT32_C(0x40000000),
    };
    for (uint32_t iRange = 0; iRange < RT_ELEMENTS(s_auRanges); iRange++)
    {
        uint32_t const uFirst = s_auRanges[iRange];

        uint32_t uEax, uEbx, uEcx, uEdx;
        ASMCpuIdExSlow(uFirst, 0, 0, 0, &uEax, &uEbx, &uEcx, &uEdx);
        if (uEax >= uFirst && uEax < uFirst + 100)
        {
            uint32_t const cLeafs = RT_MIN(uEax - uFirst + 1, 32);
            for (uint32_t iLeaf = 0; iLeaf < cLeafs; iLeaf++)
            {
                uint32_t uLeaf = uFirst + iLeaf;
                ASMCpuIdExSlow(uLeaf, 0, 0, 0, &uEax, &uEbx, &uEcx, &uEdx);

                /* Clear APIC IDs to avoid submitting new reports all the time. */
                if (uLeaf == 1)
                    uEbx &= UINT32_C(0x00ffffff);
                if (uLeaf == 0xb)
                    uEdx  = 0;
                if (uLeaf == 0x8000001e)
                    uEax  = 0;

                /* Clear some other node/cpu/core/thread ids. */
                if (uLeaf == 0x8000001e)
                {
                    uEbx &= UINT32_C(0xffffff00);
                    uEcx &= UINT32_C(0xffffff00);
                }

                RTPrintf("%08x: %08x %08x %08x %08x\n", uLeaf, uEax, uEbx, uEcx, uEdx);
            }
        }
    }
    RTPrintf("\n");

    /*
     * DMI info.
     */
    RTPrintf("DMI Info\n"
             "--------\n");
    static const struct { const char *pszName; RTSYSDMISTR enmDmiStr; } s_aDmiStrings[] =
    {
        { "Product Name",           RTSYSDMISTR_PRODUCT_NAME },
        { "Product version",        RTSYSDMISTR_PRODUCT_VERSION },
        { "Product UUID",           RTSYSDMISTR_PRODUCT_UUID },
        { "Product Serial",         RTSYSDMISTR_PRODUCT_SERIAL },
        { "System Manufacturer",    RTSYSDMISTR_MANUFACTURER },
    };
    for (uint32_t iDmiString = 0; iDmiString < RT_ELEMENTS(s_aDmiStrings); iDmiString++)
    {
        char szTmp[4096];
        RT_ZERO(szTmp);
        int rc = RTSystemQueryDmiString(s_aDmiStrings[iDmiString].enmDmiStr, szTmp, sizeof(szTmp) - 1);
        if (RT_SUCCESS(rc))
            RTPrintf("%25s: %s\n", s_aDmiStrings[iDmiString].pszName, RTStrStrip(szTmp));
        else
            RTPrintf("%25s: %s [rc=%Rrc]\n", s_aDmiStrings[iDmiString].pszName, RTStrStrip(szTmp), rc);
    }
    RTPrintf("\n");

#else
#endif

    /*
     * Dump the environment.
     */
    RTPrintf("Environment\n"
             "-----------\n");
    RTENV hEnv;
    int rc = RTEnvClone(&hEnv, RTENV_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        uint32_t cVars = RTEnvCountEx(hEnv);
        for (uint32_t iVar = 0; iVar < cVars; iVar++)
        {
            char szVar[1024];
            char szValue[16384];
            rc = RTEnvGetByIndexEx(hEnv, iVar, szVar, sizeof(szVar), szValue, sizeof(szValue));

            /* zap the value of variables that are subject to change. */
            if (   (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW)
                && (   !strcmp(szVar, "TESTBOX_SCRIPT_REV")
                    || !strcmp(szVar, "TESTBOX_ID")
                    || !strcmp(szVar, "TESTBOX_SCRATCH_SIZE")
                    || !strcmp(szVar, "TESTBOX_TIMEOUT")
                    || !strcmp(szVar, "TESTBOX_TIMEOUT_ABS")
                    || !strcmp(szVar, "TESTBOX_TEST_SET_ID")
                   )
               )
               strcpy(szValue, "<volatile>");

            if (RT_SUCCESS(rc))
                RTPrintf("%25s=%s\n", szVar, szValue);
            else if (rc == VERR_BUFFER_OVERFLOW)
                RTPrintf("%25s=%s [VERR_BUFFER_OVERFLOW]\n", szVar, szValue);
            else
                RTPrintf("rc=%Rrc\n", rc);
        }
        RTEnvDestroy(hEnv);
    }

    /** @todo enumerate volumes and whatnot.  */

    int cch = RTPrintf("\n");
    return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/** Print the total memory size in bytes. */
static RTEXITCODE handlerMemSize(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);

    uint64_t cb;
    int rc = RTSystemQueryTotalRam(&cb);
    if (RT_SUCCESS(rc))
    {
        int cch = RTPrintf("%llu\n", cb);
        return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    }
    RTPrintf("%Rrc\n", rc);
    return RTEXITCODE_FAILURE;
}

typedef enum { HWVIRTTYPE_NONE, HWVIRTTYPE_VTX, HWVIRTTYPE_AMDV } HWVIRTTYPE;
static HWVIRTTYPE isHwVirtSupported(void)
{
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    uint32_t uEax, uEbx, uEcx, uEdx;

    /* VT-x */
    ASMCpuId(0x00000000, &uEax, &uEbx, &uEcx, &uEdx);
    if (ASMIsValidStdRange(uEax))
    {
        ASMCpuId(0x00000001, &uEax, &uEbx, &uEcx, &uEdx);
        if (uEcx & X86_CPUID_FEATURE_ECX_VMX)
            return HWVIRTTYPE_VTX;
    }

    /* AMD-V */
    ASMCpuId(0x80000000, &uEax, &uEbx, &uEcx, &uEdx);
    if (ASMIsValidExtRange(uEax))
    {
        ASMCpuId(0x80000001, &uEax, &uEbx, &uEcx, &uEdx);
        if (uEcx & X86_CPUID_AMD_FEATURE_ECX_SVM)
            return HWVIRTTYPE_AMDV;
    }
#endif

    return HWVIRTTYPE_NONE;
}

/** Print the 'true' if VT-x or AMD-v is supported, 'false' it not. */
static RTEXITCODE handlerCpuHwVirt(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);
    int cch = RTPrintf(isHwVirtSupported() != HWVIRTTYPE_NONE ? "true\n" : "false\n");
    return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/** Print the 'true' if nested paging is supported, 'false' if not and
 * 'dunno' if we cannot tell. */
static RTEXITCODE handlerCpuNestedPaging(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);
    HWVIRTTYPE  enmHwVirt  = isHwVirtSupported();
    int         fSupported = -1;

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    if (enmHwVirt == HWVIRTTYPE_AMDV)
    {
        uint32_t uEax, uEbx, uEcx, uEdx;
        ASMCpuId(0x80000000, &uEax, &uEbx, &uEcx, &uEdx);
        if (ASMIsValidExtRange(uEax) && uEax >= 0x8000000a)
        {
            ASMCpuId(0x8000000a, &uEax, &uEbx, &uEcx, &uEdx);
            if (uEdx & RT_BIT(0) /* AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING */)
                fSupported = 1;
            else
                fSupported = 0;
        }
    }
#endif

    int cch = RTPrintf(fSupported == 1 ? "true\n" : fSupported == 0 ? "false\n" : "dunno\n");
    return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/** Print the 'true' if long mode guests are supported, 'false' if not and
 * 'dunno' if we cannot tell. */
static RTEXITCODE handlerCpuLongMode(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);
    HWVIRTTYPE  enmHwVirt  = isHwVirtSupported();
    int         fSupported = 0;

    if (enmHwVirt != HWVIRTTYPE_NONE)
    {
#if defined(RT_ARCH_AMD64)
        fSupported = 1; /* We're running long mode, so it must be supported. */

#elif defined(RT_ARCH_X86)
# ifdef RT_OS_DARWIN
        /* On darwin, we just ask the kernel via sysctl. Rules are a bit different here. */
        int     f64bitCapable = 0;
        size_t  cbParameter   = sizeof(f64bitCapable);
        int rc = sysctlbyname("hw.cpu64bit_capable", &f64bitCapable, &cbParameter, NULL, NULL);
        if (rc != -1)
            fSupported = f64bitCapable != 0;
        else
# endif
        {
            /* PAE and HwVirt are required */
            uint32_t uEax, uEbx, uEcx, uEdx;
            ASMCpuId(0x00000000, &uEax, &uEbx, &uEcx, &uEdx);
            if (ASMIsValidStdRange(uEax))
            {
                ASMCpuId(0x00000001, &uEax, &uEbx, &uEcx, &uEdx);
                if (uEdx & X86_CPUID_FEATURE_EDX_PAE)
                {
                    /* AMD will usually advertise long mode in 32-bit mode. Intel OTOH,
                       won't necessarily do so. */
                    ASMCpuId(0x80000000, &uEax, &uEbx, &uEcx, &uEdx);
                    if (ASMIsValidExtRange(uEax))
                    {
                        ASMCpuId(0x80000001, &uEax, &uEbx, &uEcx, &uEdx);
                        if (uEdx & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE)
                            fSupported = 1;
                        else if (enmHwVirt != HWVIRTTYPE_AMDV)
                            fSupported = -1;
                    }
                }
            }
        }
#endif
    }

    int cch = RTPrintf(fSupported == 1 ? "true\n" : fSupported == 0 ? "false\n" : "dunno\n");
    return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/** Print the CPU 'revision', if available. */
static RTEXITCODE handlerCpuRevision(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    uint32_t uEax, uEbx, uEcx, uEdx;
    ASMCpuId(0, &uEax, &uEbx, &uEcx, &uEdx);
    if (ASMIsValidStdRange(uEax) && uEax >= 1)
    {
        uint32_t uEax1 = ASMCpuId_EAX(1);
        uint32_t uVersion = (ASMGetCpuFamily(uEax1) << 24)
                          | (ASMGetCpuModel(uEax1, ASMIsIntelCpuEx(uEbx, uEcx, uEdx)) << 8)
                          | ASMGetCpuStepping(uEax1);
        int cch = RTPrintf("%#x\n", uVersion);
        return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    }
#endif
    return RTEXITCODE_FAILURE;
}


/** Print the CPU name, if available. */
static RTEXITCODE handlerCpuName(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);

    char szTmp[1024];
    int rc = RTMpGetDescription(NIL_RTCPUID, szTmp, sizeof(szTmp));
    if (RT_SUCCESS(rc))
    {
        int cch = RTPrintf("%s\n", RTStrStrip(szTmp));
        return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
    }
    return RTEXITCODE_FAILURE;
}


/** Print the CPU vendor name, 'GenuineIntel' and such. */
static RTEXITCODE handlerCpuVendor(int argc, char **argv)
{
    NOREF(argc); NOREF(argv);

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    uint32_t uEax, uEbx, uEcx, uEdx;
    ASMCpuId(0, &uEax, &uEbx, &uEcx, &uEdx);
    int cch = RTPrintf("%.04s%.04s%.04s\n", &uEbx, &uEdx, &uEcx);
#else
    int cch = RTPrintf("%s\n", RTBldCfgTargetArch());
#endif
    return cch > 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}



int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * The first argument is a command.  Figure out which and call its handler.
     */
    static const struct
    {
        const char     *pszCommand;
        RTEXITCODE    (*pfnHandler)(int argc, char **argv);
        bool            fNoArgs;
    } s_aHandlers[] =
    {
        { "cpuvendor",      handlerCpuVendor,       true },
        { "cpuname",        handlerCpuName,         true },
        { "cpurevision",    handlerCpuRevision,     true },
        { "cpuhwvirt",      handlerCpuHwVirt,       true },
        { "nestedpaging",   handlerCpuNestedPaging, true },
        { "longmode",       handlerCpuLongMode,     true },
        { "memsize",        handlerMemSize,         true },
        { "report",         handlerReport,          true }
    };

    if (argc < 2)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "expected command as the first argument");

    for (unsigned i = 0; i < RT_ELEMENTS(s_aHandlers); i++)
    {
        if (!strcmp(argv[1], s_aHandlers[i].pszCommand))
        {
            if (   s_aHandlers[i].fNoArgs
                && argc != 2)
                return RTMsgErrorExit(RTEXITCODE_SYNTAX, "the command '%s' does not take any arguments", argv[1]);
            return s_aHandlers[i].pfnHandler(argc - 1, argv + 1);
        }
    }

    /*
     * Help or version query?
     */
    for (int i = 1; i < argc; i++)
        if (   !strcmp(argv[i], "--help")
            || !strcmp(argv[i], "-h")
            || !strcmp(argv[i], "-?")
            || !strcmp(argv[i], "help") )
        {
            RTPrintf("usage: %s <cmd> [cmd specific args]\n"
                     "\n"
                     "commands:\n", argv[0]);
            for (unsigned j = 0; j < RT_ELEMENTS(s_aHandlers); j++)
                RTPrintf("    %s\n", s_aHandlers[j].pszCommand);
            return RTEXITCODE_FAILURE;
        }
        else if (   !strcmp(argv[i], "--version")
                 || !strcmp(argv[i], "-V") )
        {
            RTPrintf("%sr%u", RTBldCfgVersion(),  RTBldCfgRevision());
            return argc == 2 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
        }

    /*
     * Syntax error.
     */
    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "unknown command '%s'", argv[1]);
}

