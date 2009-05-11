/* $Id$ */
/** @file
 * VMM - The Virtual Machine Monitor, Guru Meditation Code.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include <VBox/pdmapi.h>
#include <VBox/trpm.h>
#include <VBox/dbgf.h>
#include "VMMInternal.h"
#include <VBox/vm.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/version.h>
#include <VBox/hwaccm.h>
#include <iprt/assert.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Structure to pass to DBGFR3Info() and for doing all other
 * output during fatal dump.
 */
typedef struct VMMR3FATALDUMPINFOHLP
{
    /** The helper core. */
    DBGFINFOHLP Core;
    /** The release logger instance. */
    PRTLOGGER   pRelLogger;
    /** The saved release logger flags. */
    RTUINT      fRelLoggerFlags;
    /** The logger instance. */
    PRTLOGGER   pLogger;
    /** The saved logger flags. */
    RTUINT      fLoggerFlags;
    /** The saved logger destination flags. */
    RTUINT      fLoggerDestFlags;
    /** Whether to output to stderr or not. */
    bool        fStdErr;
} VMMR3FATALDUMPINFOHLP, *PVMMR3FATALDUMPINFOHLP;
/** Pointer to a VMMR3FATALDUMPINFOHLP structure. */
typedef const VMMR3FATALDUMPINFOHLP *PCVMMR3FATALDUMPINFOHLP;


/**
 * Print formatted string.
 *
 * @param   pHlp        Pointer to this structure.
 * @param   pszFormat   The format string.
 * @param   ...         Arguments.
 */
static DECLCALLBACK(void) vmmR3FatalDumpInfoHlp_pfnPrintf(PCDBGFINFOHLP pHlp, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    pHlp->pfnPrintfV(pHlp, pszFormat, args);
    va_end(args);
}


/**
 * Print formatted string.
 *
 * @param   pHlp        Pointer to this structure.
 * @param   pszFormat   The format string.
 * @param   args        Argument list.
 */
static DECLCALLBACK(void) vmmR3FatalDumpInfoHlp_pfnPrintfV(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args)
{
    PCVMMR3FATALDUMPINFOHLP pMyHlp = (PCVMMR3FATALDUMPINFOHLP)pHlp;

    if (pMyHlp->pRelLogger)
    {
        va_list args2;
        va_copy(args2, args);
        RTLogLoggerV(pMyHlp->pRelLogger, pszFormat, args2);
        va_end(args2);
    }
    if (pMyHlp->pLogger)
    {
        va_list args2;
        va_copy(args2, args);
        RTLogLoggerV(pMyHlp->pLogger, pszFormat, args);
        va_end(args2);
    }
    if (pMyHlp->fStdErr)
    {
        va_list args2;
        va_copy(args2, args);
        RTStrmPrintfV(g_pStdErr, pszFormat, args);
        va_end(args2);
    }
}


/**
 * Initializes the fatal dump output helper.
 *
 * @param   pHlp        The structure to initialize.
 */
static void vmmR3FatalDumpInfoHlpInit(PVMMR3FATALDUMPINFOHLP pHlp)
{
    memset(pHlp, 0, sizeof(*pHlp));

    pHlp->Core.pfnPrintf = vmmR3FatalDumpInfoHlp_pfnPrintf;
    pHlp->Core.pfnPrintfV = vmmR3FatalDumpInfoHlp_pfnPrintfV;

    /*
     * The loggers.
     */
    pHlp->pRelLogger = RTLogRelDefaultInstance();
#ifndef LOG_ENABLED
    if (!pHlp->pRelLogger)
#endif
        pHlp->pLogger = RTLogDefaultInstance();

    if (pHlp->pRelLogger)
    {
        pHlp->fRelLoggerFlags = pHlp->pRelLogger->fFlags;
        pHlp->pRelLogger->fFlags &= ~(RTLOGFLAGS_BUFFERED | RTLOGFLAGS_DISABLED);
    }

    if (pHlp->pLogger)
    {
        pHlp->fLoggerFlags     = pHlp->pLogger->fFlags;
        pHlp->fLoggerDestFlags = pHlp->pLogger->fDestFlags;
        pHlp->pLogger->fFlags     &= ~(RTLOGFLAGS_BUFFERED | RTLOGFLAGS_DISABLED);
#ifndef DEBUG_sandervl
        pHlp->pLogger->fDestFlags |= RTLOGDEST_DEBUGGER;
#endif
    }

    /*
     * Check if we need write to stderr.
     */
#ifdef DEBUG_sandervl
    pHlp->fStdErr = false; /* takes too long to display here */
#else
    pHlp->fStdErr = (!pHlp->pRelLogger || !(pHlp->pRelLogger->fDestFlags & (RTLOGDEST_STDOUT | RTLOGDEST_STDERR)))
                 && (!pHlp->pLogger || !(pHlp->pLogger->fDestFlags & (RTLOGDEST_STDOUT | RTLOGDEST_STDERR)));
#endif
}


/**
 * Deletes the fatal dump output helper.
 *
 * @param   pHlp        The structure to delete.
 */
static void vmmR3FatalDumpInfoHlpDelete(PVMMR3FATALDUMPINFOHLP pHlp)
{
    if (pHlp->pRelLogger)
    {
        RTLogFlush(pHlp->pRelLogger);
        pHlp->pRelLogger->fFlags = pHlp->fRelLoggerFlags;
    }

    if (pHlp->pLogger)
    {
        RTLogFlush(pHlp->pLogger);
        pHlp->pLogger->fFlags     = pHlp->fLoggerFlags;
        pHlp->pLogger->fDestFlags = pHlp->fLoggerDestFlags;
    }
}


/**
 * Dumps the VM state on a fatal error.
 *
 * @param   pVM         VM Handle.
 * @param   pVCpu       VMCPU Handle.
 * @param   rcErr       VBox status code.
 */
VMMR3DECL(void) VMMR3FatalDump(PVM pVM, PVMCPU pVCpu, int rcErr)
{
    /*
     * Create our output helper and sync it with the log settings.
     * This helper will be used for all the output.
     */
    VMMR3FATALDUMPINFOHLP   Hlp;
    PCDBGFINFOHLP           pHlp = &Hlp.Core;
    vmmR3FatalDumpInfoHlpInit(&Hlp);

    /*
     * Header.
     */
    pHlp->pfnPrintf(pHlp,
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                    "!!\n"
                    "!!                 Guru Meditation %d (%Rrc)\n"
                    "!!\n",
                    rcErr, rcErr);

    /*
     * Continue according to context.
     */
    bool fDoneHyper = false;
    switch (rcErr)
    {
        /*
         * Hypervisor errors.
         */
        case VERR_VMM_RING0_ASSERTION:
        case VINF_EM_DBG_HYPER_ASSERTION:
        {
            const char *pszMsg1 = VMMR3GetRZAssertMsg1(pVM);
            while (pszMsg1 && *pszMsg1 == '\n')
                pszMsg1++;
            const char *pszMsg2 = VMMR3GetRZAssertMsg2(pVM);
            while (pszMsg2 && *pszMsg2 == '\n')
                pszMsg2++;
            pHlp->pfnPrintf(pHlp,
                            "%s"
                            "%s",
                            pszMsg1,
                            pszMsg2);
            if (    !pszMsg2
                ||  !*pszMsg2
                ||  strchr(pszMsg2, '\0')[-1] != '\n')
                pHlp->pfnPrintf(pHlp, "\n");
            pHlp->pfnPrintf(pHlp, "!!\n");
            /* fall thru */
        }
        case VERR_TRPM_DONT_PANIC:
        case VERR_TRPM_PANIC:
        case VINF_EM_RAW_STALE_SELECTOR:
        case VINF_EM_RAW_IRET_TRAP:
        case VINF_EM_DBG_HYPER_BREAKPOINT:
        case VINF_EM_DBG_HYPER_STEPPED:
        {
            /*
             * Active trap? This is only of partial interest when in hardware
             * assisted virtualization mode, thus the different messages.
             */
            uint32_t        uEIP       = CPUMGetHyperEIP(pVCpu);
            TRPMEVENT       enmType;
            uint8_t         u8TrapNo   =       0xce;
            RTGCUINT        uErrorCode = 0xdeadface;
            RTGCUINTPTR     uCR2       = 0xdeadface;
            int rc2 = TRPMQueryTrapAll(pVCpu, &u8TrapNo, &enmType, &uErrorCode, &uCR2);
            if (!HWACCMR3IsActive(pVM))
            {
                if (RT_SUCCESS(rc2))
                    pHlp->pfnPrintf(pHlp,
                                    "!! TRAP=%02x ERRCD=%RGv CR2=%RGv EIP=%RX32 Type=%d\n",
                                    u8TrapNo, uErrorCode, uCR2, uEIP, enmType);
                else
                    pHlp->pfnPrintf(pHlp,
                                    "!! EIP=%RX32 NOTRAP\n",
                                    uEIP);
            }
            else if (RT_SUCCESS(rc2))
                pHlp->pfnPrintf(pHlp,
                                "!! ACTIVE TRAP=%02x ERRCD=%RGv CR2=%RGv PC=%RGr Type=%d (Guest!)\n",
                                u8TrapNo, uErrorCode, uCR2, CPUMGetGuestRIP(pVCpu), enmType);

            /*
             * The hypervisor dump is not relevant when we're in VT-x/AMD-V mode.
             */
            if (HWACCMR3IsActive(pVM))
            {
                pHlp->pfnPrintf(pHlp, "\n");
#if 0
                /* Callstack. */
                PCDBGFSTACKFRAME pFirstFrame;
                DBGFADDRESS eip, ebp, esp;

                eip.fFlags   = DBGFADDRESS_FLAGS_RING0 | DBGFADDRESS_FLAGS_VALID;
#if HC_ARCH_BITS == 64
                eip.FlatPtr = eip.off = pVCpu->vmm.s.CallHostR0JmpBuf.rip;
#else
                eip.FlatPtr = eip.off = pVCpu->vmm.s.CallHostR0JmpBuf.eip;
#endif
                ebp.fFlags   = DBGFADDRESS_FLAGS_RING0 | DBGFADDRESS_FLAGS_VALID;
                ebp.FlatPtr  = ebp.off = pVCpu->vmm.s.CallHostR0JmpBuf.SavedEbp;
                esp.fFlags   = DBGFADDRESS_FLAGS_RING0 | DBGFADDRESS_FLAGS_VALID;
                esp.FlatPtr  = esp.off = pVCpu->vmm.s.CallHostR0JmpBuf.SavedEsp;

                rc2 = DBGFR3StackWalkBeginEx(pVM, pVCpu->idCpu, DBGFCODETYPE_RING0, &ebp, &esp, &eip,
                                             DBGFRETURNTYPE_INVALID, &pFirstFrame);
                if (RT_SUCCESS(rc2))
                {
                    pHlp->pfnPrintf(pHlp,
                                    "!!\n"
                                    "!! Call Stack:\n"
                                    "!!\n"
                                    "EBP      Ret EBP  Ret CS:EIP    Arg0     Arg1     Arg2     Arg3     CS:EIP        Symbol [line]\n");
                    for (PCDBGFSTACKFRAME pFrame = pFirstFrame;
                         pFrame;
                         pFrame = DBGFR3StackWalkNext(pFrame))
                    {
                        pHlp->pfnPrintf(pHlp,
                                        "%08RX32 %08RX32 %04RX32:%08RX32 %08RX32 %08RX32 %08RX32 %08RX32",
                                        (uint32_t)pFrame->AddrFrame.off,
                                        (uint32_t)pFrame->AddrReturnFrame.off,
                                        (uint32_t)pFrame->AddrReturnPC.Sel,
                                        (uint32_t)pFrame->AddrReturnPC.off,
                                        pFrame->Args.au32[0],
                                        pFrame->Args.au32[1],
                                        pFrame->Args.au32[2],
                                        pFrame->Args.au32[3]);
                        pHlp->pfnPrintf(pHlp, " %RTsel:%08RGv", pFrame->AddrPC.Sel, pFrame->AddrPC.off);
                        if (pFrame->pSymPC)
                        {
                            RTGCINTPTR offDisp = pFrame->AddrPC.FlatPtr - pFrame->pSymPC->Value;
                            if (offDisp > 0)
                                pHlp->pfnPrintf(pHlp, " %s+%llx", pFrame->pSymPC->szName, (int64_t)offDisp);
                            else if (offDisp < 0)
                                pHlp->pfnPrintf(pHlp, " %s-%llx", pFrame->pSymPC->szName, -(int64_t)offDisp);
                            else
                                pHlp->pfnPrintf(pHlp, " %s", pFrame->pSymPC->szName);
                        }
                        if (pFrame->pLinePC)
                            pHlp->pfnPrintf(pHlp, " [%s @ 0i%d]", pFrame->pLinePC->szFilename, pFrame->pLinePC->uLineNo);
                        pHlp->pfnPrintf(pHlp, "\n");
                    }
                    DBGFR3StackWalkEnd(pFirstFrame);
                }
#endif
            }
            else
            {
                /*
                 * Try figure out where eip is.
                 */
                /* core code? */
                if (uEIP - (RTGCUINTPTR)pVM->vmm.s.pvCoreCodeRC < pVM->vmm.s.cbCoreCode)
                    pHlp->pfnPrintf(pHlp,
                                "!! EIP is in CoreCode, offset %#x\n",
                                uEIP - (RTGCUINTPTR)pVM->vmm.s.pvCoreCodeRC);
                else
                {   /* ask PDM */  /** @todo ask DBGFR3Sym later? */
                    char        szModName[64];
                    RTRCPTR     RCPtrMod;
                    char        szNearSym1[260];
                    RTRCPTR     RCPtrNearSym1;
                    char        szNearSym2[260];
                    RTRCPTR     RCPtrNearSym2;
                    int rc = PDMR3LdrQueryRCModFromPC(pVM, uEIP,
                                                      &szModName[0],  sizeof(szModName),  &RCPtrMod,
                                                      &szNearSym1[0], sizeof(szNearSym1), &RCPtrNearSym1,
                                                      &szNearSym2[0], sizeof(szNearSym2), &RCPtrNearSym2);
                    if (RT_SUCCESS(rc))
                        pHlp->pfnPrintf(pHlp,
                                        "!! EIP in %s (%RRv) at rva %x near symbols:\n"
                                        "!!    %RRv rva %RRv off %08x  %s\n"
                                        "!!    %RRv rva %RRv off -%08x %s\n",
                                        szModName,  RCPtrMod, (unsigned)(uEIP - RCPtrMod),
                                        RCPtrNearSym1, RCPtrNearSym1 - RCPtrMod, (unsigned)(uEIP - RCPtrNearSym1), szNearSym1,
                                        RCPtrNearSym2, RCPtrNearSym2 - RCPtrMod, (unsigned)(RCPtrNearSym2 - uEIP), szNearSym2);
                    else
                        pHlp->pfnPrintf(pHlp,
                                        "!! EIP is not in any code known to VMM!\n");
                }

                /* Disassemble the instruction. */
                char szInstr[256];
                rc2 = DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, 0, 0, DBGF_DISAS_FLAGS_CURRENT_HYPER, &szInstr[0], sizeof(szInstr), NULL);
                if (RT_SUCCESS(rc2))
                    pHlp->pfnPrintf(pHlp,
                                    "!! %s\n", szInstr);

                /* Dump the hypervisor cpu state. */
                pHlp->pfnPrintf(pHlp,
                                "!!\n"
                                "!!\n"
                                "!!\n");
                rc2 = DBGFR3Info(pVM, "cpumhyper", "verbose", pHlp);
                fDoneHyper = true;

                /* Callstack. */
                PCDBGFSTACKFRAME pFirstFrame;
                rc2 = DBGFR3StackWalkBegin(pVM, pVCpu->idCpu, DBGFCODETYPE_HYPER, &pFirstFrame);
                if (RT_SUCCESS(rc2))
                {
                    pHlp->pfnPrintf(pHlp,
                                    "!!\n"
                                    "!! Call Stack:\n"
                                    "!!\n"
                                    "EBP      Ret EBP  Ret CS:EIP    Arg0     Arg1     Arg2     Arg3     CS:EIP        Symbol [line]\n");
                    for (PCDBGFSTACKFRAME pFrame = pFirstFrame;
                         pFrame;
                         pFrame = DBGFR3StackWalkNext(pFrame))
                    {
                        pHlp->pfnPrintf(pHlp,
                                        "%08RX32 %08RX32 %04RX32:%08RX32 %08RX32 %08RX32 %08RX32 %08RX32",
                                        (uint32_t)pFrame->AddrFrame.off,
                                        (uint32_t)pFrame->AddrReturnFrame.off,
                                        (uint32_t)pFrame->AddrReturnPC.Sel,
                                        (uint32_t)pFrame->AddrReturnPC.off,
                                        pFrame->Args.au32[0],
                                        pFrame->Args.au32[1],
                                        pFrame->Args.au32[2],
                                        pFrame->Args.au32[3]);
                        pHlp->pfnPrintf(pHlp, " %RTsel:%08RGv", pFrame->AddrPC.Sel, pFrame->AddrPC.off);
                        if (pFrame->pSymPC)
                        {
                            RTGCINTPTR offDisp = pFrame->AddrPC.FlatPtr - pFrame->pSymPC->Value;
                            if (offDisp > 0)
                                pHlp->pfnPrintf(pHlp, " %s+%llx", pFrame->pSymPC->szName, (int64_t)offDisp);
                            else if (offDisp < 0)
                                pHlp->pfnPrintf(pHlp, " %s-%llx", pFrame->pSymPC->szName, -(int64_t)offDisp);
                            else
                                pHlp->pfnPrintf(pHlp, " %s", pFrame->pSymPC->szName);
                        }
                        if (pFrame->pLinePC)
                            pHlp->pfnPrintf(pHlp, " [%s @ 0i%d]", pFrame->pLinePC->szFilename, pFrame->pLinePC->uLineNo);
                        pHlp->pfnPrintf(pHlp, "\n");
                    }
                    DBGFR3StackWalkEnd(pFirstFrame);
                }

                /* raw stack */
                pHlp->pfnPrintf(pHlp,
                                "!!\n"
                                "!! Raw stack (mind the direction). pbEMTStackRC=%RRv pbEMTStackBottomRC=%RRv\n"
                                "!!\n"
                                "%.*Rhxd\n",
                                pVCpu->vmm.s.pbEMTStackRC, pVCpu->vmm.s.pbEMTStackBottomRC,
                                VMM_STACK_SIZE, pVCpu->vmm.s.pbEMTStackR3);
            } /* !HWACCMR3IsActive */
            break;
        }

        default:
        {
            break;
        }

    } /* switch (rcErr) */


    /*
     * Generic info dumper loop.
     */
    static struct
    {
        const char *pszInfo;
        const char *pszArgs;
    } const     aInfo[] =
    {
        { "mappings",       NULL },
        { "hma",            NULL },
        { "cpumguest",      "verbose" },
        { "cpumguestinstr", "verbose" },
        { "cpumhyper",      "verbose" },
        { "cpumhost",       "verbose" },
        { "mode",           "all" },
        { "cpuid",          "verbose" },
        { "gdt",            NULL },
        { "ldt",            NULL },
        //{ "tss",            NULL },
        { "ioport",         NULL },
        { "mmio",           NULL },
        { "phys",           NULL },
        //{ "pgmpd",          NULL }, - doesn't always work at init time...
        { "timers",         NULL },
        { "activetimers",   NULL },
        { "handlers",       "phys virt hyper stats" },
        { "cfgm",           NULL },
    };
    for (unsigned i = 0; i < RT_ELEMENTS(aInfo); i++)
    {
        if (fDoneHyper && !strcmp(aInfo[i].pszInfo, "cpumhyper"))
            continue;
        pHlp->pfnPrintf(pHlp,
                        "!!\n"
                        "!! {%s, %s}\n"
                        "!!\n",
                        aInfo[i].pszInfo, aInfo[i].pszArgs);
        DBGFR3Info(pVM, aInfo[i].pszInfo, aInfo[i].pszArgs, pHlp);
    }

    /* done */
    pHlp->pfnPrintf(pHlp,
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");


    /*
     * Delete the output instance (flushing and restoring of flags).
     */
    vmmR3FatalDumpInfoHlpDelete(&Hlp);
}

