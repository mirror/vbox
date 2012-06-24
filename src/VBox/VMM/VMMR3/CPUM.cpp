/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_cpum CPUM - CPU Monitor / Manager
 *
 * The CPU Monitor / Manager keeps track of all the CPU registers. It is
 * also responsible for lazy FPU handling and some of the context loading
 * in raw mode.
 *
 * There are three CPU contexts, the most important one is the guest one (GC).
 * When running in raw-mode (RC) there is a special hyper context for the VMM
 * part that floats around inside the guest address space. When running in
 * raw-mode, CPUM also maintains a host context for saving and restoring
 * registers across world switches. This latter is done in cooperation with the
 * world switcher (@see pg_vmm).
 *
 * @see grp_cpum
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/cpumdis.h>
#include <VBox/vmm/cpumctx-v1_6.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/ssm.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include "internal/pgm.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if 0 /* later when actual changes have been made */
/** The current saved state version. */
#define CPUM_SAVED_STATE_VERSION                14
#else
# define CPUM_SAVED_STATE_VERSION               CPUM_SAVED_STATE_VERSION_MEM
#endif
/** The current saved state version before using SSMR3PutStruct. */
#define CPUM_SAVED_STATE_VERSION_MEM            13
/** The saved state version before introducing the MSR size field. */
#define CPUM_SAVED_STATE_VERSION_NO_MSR_SIZE    12
/** The saved state version of 3.2, 3.1 and 3.3 trunk before the hidden
 * selector register change (CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID). */
#define CPUM_SAVED_STATE_VERSION_VER3_2         11
/** The saved state version of 3.0 and 3.1 trunk before the teleportation
 * changes. */
#define CPUM_SAVED_STATE_VERSION_VER3_0         10
/** The saved state version for the 2.1 trunk before the MSR changes. */
#define CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR   9
/** The saved state version of 2.0, used for backwards compatibility. */
#define CPUM_SAVED_STATE_VERSION_VER2_0         8
/** The saved state version of 1.6, used for backwards compatibility. */
#define CPUM_SAVED_STATE_VERSION_VER1_6         6

#define CPUM_WITH_CHANGED_CPUMCTX

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * What kind of cpu info dump to perform.
 */
typedef enum CPUMDUMPTYPE
{
    CPUMDUMPTYPE_TERSE,
    CPUMDUMPTYPE_DEFAULT,
    CPUMDUMPTYPE_VERBOSE
} CPUMDUMPTYPE;
/** Pointer to a cpu info dump type. */
typedef CPUMDUMPTYPE *PCPUMDUMPTYPE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static CPUMCPUVENDOR cpumR3DetectVendor(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX);
static int cpumR3CpuIdInit(PVM pVM);
static DECLCALLBACK(int)  cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass);
static DECLCALLBACK(int)  cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  cpumR3LoadPrep(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(int)  cpumR3LoadDone(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Saved state field descriptors for CPUMCTX. */
static const SSMFIELD g_aCpumCtxFields[] =
{
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FCW),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FSW),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FTW),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FOP),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FPUIP),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.CS),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.Rsrvd1),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.FPUDP),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.DS),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.Rsrvd2),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.MXCSR),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.MXCSR_MASK),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[0]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[1]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[2]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[3]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[4]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[5]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[6]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aRegs[7]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[0]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[1]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[2]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[3]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[4]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[5]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[6]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[7]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[8]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[9]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[10]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[11]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[12]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[13]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[14]),
    SSMFIELD_ENTRY(         CPUMCTX, fpu.aXMM[15]),
    SSMFIELD_ENTRY_IGNORE(  CPUMCTX, fpu.au32RsrvdRest),
    SSMFIELD_ENTRY(         CPUMCTX, rdi),
    SSMFIELD_ENTRY(         CPUMCTX, rsi),
    SSMFIELD_ENTRY(         CPUMCTX, rbp),
    SSMFIELD_ENTRY(         CPUMCTX, rax),
    SSMFIELD_ENTRY(         CPUMCTX, rbx),
    SSMFIELD_ENTRY(         CPUMCTX, rdx),
    SSMFIELD_ENTRY(         CPUMCTX, rcx),
    SSMFIELD_ENTRY(         CPUMCTX, rsp),
    SSMFIELD_ENTRY_OLD(              lss_esp, sizeof(uint32_t)),
    SSMFIELD_ENTRY(         CPUMCTX, ss.Sel),
    SSMFIELD_ENTRY_OLD(              ssPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, gs.Sel),
    SSMFIELD_ENTRY_OLD(              gsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, fs.Sel),
    SSMFIELD_ENTRY_OLD(              fsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, es.Sel),
    SSMFIELD_ENTRY_OLD(              esPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, ds.Sel),
    SSMFIELD_ENTRY_OLD(              dsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, cs.Sel),
    SSMFIELD_ENTRY_OLD(              csPadding, sizeof(uint16_t)*3),
    SSMFIELD_ENTRY(         CPUMCTX, rflags),
    SSMFIELD_ENTRY(         CPUMCTX, rip),
    SSMFIELD_ENTRY(         CPUMCTX, r8),
    SSMFIELD_ENTRY(         CPUMCTX, r9),
    SSMFIELD_ENTRY(         CPUMCTX, r10),
    SSMFIELD_ENTRY(         CPUMCTX, r11),
    SSMFIELD_ENTRY(         CPUMCTX, r12),
    SSMFIELD_ENTRY(         CPUMCTX, r13),
    SSMFIELD_ENTRY(         CPUMCTX, r14),
    SSMFIELD_ENTRY(         CPUMCTX, r15),
    SSMFIELD_ENTRY(         CPUMCTX, es.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, es.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, es.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, cs.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, cs.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, cs.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, ss.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, ss.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, ss.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, ds.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, ds.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, ds.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, fs.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, fs.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, fs.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, gs.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, gs.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, gs.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, cr0),
    SSMFIELD_ENTRY(         CPUMCTX, cr2),
    SSMFIELD_ENTRY(         CPUMCTX, cr3),
    SSMFIELD_ENTRY(         CPUMCTX, cr4),
    SSMFIELD_ENTRY(         CPUMCTX, dr[0]),
    SSMFIELD_ENTRY(         CPUMCTX, dr[1]),
    SSMFIELD_ENTRY(         CPUMCTX, dr[2]),
    SSMFIELD_ENTRY(         CPUMCTX, dr[3]),
    SSMFIELD_ENTRY_OLD(              dr[4], sizeof(uint64_t)),
    SSMFIELD_ENTRY_OLD(              dr[5], sizeof(uint64_t)),
    SSMFIELD_ENTRY(         CPUMCTX, dr[6]),
    SSMFIELD_ENTRY(         CPUMCTX, dr[7]),
    SSMFIELD_ENTRY(         CPUMCTX, gdtr.cbGdt),
    SSMFIELD_ENTRY(         CPUMCTX, gdtr.pGdt),
    SSMFIELD_ENTRY_OLD(              gdtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, idtr.cbIdt),
    SSMFIELD_ENTRY(         CPUMCTX, idtr.pIdt),
    SSMFIELD_ENTRY_OLD(              idtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, ldtr.Sel),
    SSMFIELD_ENTRY_OLD(              ldtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, tr.Sel),
    SSMFIELD_ENTRY_OLD(              trPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(         CPUMCTX, SysEnter.cs),
    SSMFIELD_ENTRY(         CPUMCTX, SysEnter.eip),
    SSMFIELD_ENTRY(         CPUMCTX, SysEnter.esp),
    SSMFIELD_ENTRY(         CPUMCTX, msrEFER),
    SSMFIELD_ENTRY(         CPUMCTX, msrSTAR),
    SSMFIELD_ENTRY(         CPUMCTX, msrPAT),
    SSMFIELD_ENTRY(         CPUMCTX, msrLSTAR),
    SSMFIELD_ENTRY(         CPUMCTX, msrCSTAR),
    SSMFIELD_ENTRY(         CPUMCTX, msrSFMASK),
    SSMFIELD_ENTRY(         CPUMCTX, msrKERNELGSBASE),
    SSMFIELD_ENTRY(         CPUMCTX, ldtr.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, ldtr.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, ldtr.Attr),
    SSMFIELD_ENTRY(         CPUMCTX, tr.u64Base),
    SSMFIELD_ENTRY(         CPUMCTX, tr.u32Limit),
    SSMFIELD_ENTRY(         CPUMCTX, tr.Attr),
    SSMFIELD_ENTRY_TERM()
};

/** Saved state field descriptors for CPUMCTX_VER1_6. */
static const SSMFIELD g_aCpumCtxFieldsV16[] =
{
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FCW),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FSW),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FTW),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FOP),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FPUIP),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.CS),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.Rsrvd1),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.FPUDP),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.DS),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.Rsrvd2),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.MXCSR),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.MXCSR_MASK),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[0]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[1]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[2]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[3]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[4]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[5]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[6]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aRegs[7]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[0]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[1]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[2]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[3]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[4]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[5]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[6]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[7]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[8]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[9]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[10]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[11]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[12]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[13]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[14]),
    SSMFIELD_ENTRY(             CPUMCTX, fpu.aXMM[15]),
    SSMFIELD_ENTRY_IGNORE(      CPUMCTX, fpu.au32RsrvdRest),
    SSMFIELD_ENTRY(             CPUMCTX, rdi),
    SSMFIELD_ENTRY(             CPUMCTX, rsi),
    SSMFIELD_ENTRY(             CPUMCTX, rbp),
    SSMFIELD_ENTRY(             CPUMCTX, rax),
    SSMFIELD_ENTRY(             CPUMCTX, rbx),
    SSMFIELD_ENTRY(             CPUMCTX, rdx),
    SSMFIELD_ENTRY(             CPUMCTX, rcx),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, rsp),
    SSMFIELD_ENTRY(             CPUMCTX, ss.Sel),
    SSMFIELD_ENTRY_OLD(                  ssPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY_OLD(         CPUMCTX, sizeof(uint64_t) /*rsp_notused*/),
    SSMFIELD_ENTRY(             CPUMCTX, gs.Sel),
    SSMFIELD_ENTRY_OLD(                  gsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, fs.Sel),
    SSMFIELD_ENTRY_OLD(                  fsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, es.Sel),
    SSMFIELD_ENTRY_OLD(                  esPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, ds.Sel),
    SSMFIELD_ENTRY_OLD(                  dsPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, cs.Sel),
    SSMFIELD_ENTRY_OLD(                  csPadding, sizeof(uint16_t)*3),
    SSMFIELD_ENTRY(             CPUMCTX, rflags),
    SSMFIELD_ENTRY(             CPUMCTX, rip),
    SSMFIELD_ENTRY(             CPUMCTX, r8),
    SSMFIELD_ENTRY(             CPUMCTX, r9),
    SSMFIELD_ENTRY(             CPUMCTX, r10),
    SSMFIELD_ENTRY(             CPUMCTX, r11),
    SSMFIELD_ENTRY(             CPUMCTX, r12),
    SSMFIELD_ENTRY(             CPUMCTX, r13),
    SSMFIELD_ENTRY(             CPUMCTX, r14),
    SSMFIELD_ENTRY(             CPUMCTX, r15),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, es.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, es.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, es.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, cs.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, cs.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, cs.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, ss.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, ss.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, ss.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, ds.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, ds.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, ds.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, fs.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, fs.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, fs.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, gs.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, gs.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, gs.Attr),
    SSMFIELD_ENTRY(             CPUMCTX, cr0),
    SSMFIELD_ENTRY(             CPUMCTX, cr2),
    SSMFIELD_ENTRY(             CPUMCTX, cr3),
    SSMFIELD_ENTRY(             CPUMCTX, cr4),
    SSMFIELD_ENTRY_OLD(                  cr8, sizeof(uint64_t)),
    SSMFIELD_ENTRY(             CPUMCTX, dr[0]),
    SSMFIELD_ENTRY(             CPUMCTX, dr[1]),
    SSMFIELD_ENTRY(             CPUMCTX, dr[2]),
    SSMFIELD_ENTRY(             CPUMCTX, dr[3]),
    SSMFIELD_ENTRY_OLD(                  dr[4], sizeof(uint64_t)),
    SSMFIELD_ENTRY_OLD(                  dr[5], sizeof(uint64_t)),
    SSMFIELD_ENTRY(             CPUMCTX, dr[6]),
    SSMFIELD_ENTRY(             CPUMCTX, dr[7]),
    SSMFIELD_ENTRY(             CPUMCTX, gdtr.cbGdt),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, gdtr.pGdt),
    SSMFIELD_ENTRY_OLD(                  gdtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY_OLD(                  gdtrPadding64, sizeof(uint64_t)),
    SSMFIELD_ENTRY(             CPUMCTX, idtr.cbIdt),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, idtr.pIdt),
    SSMFIELD_ENTRY_OLD(                  idtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY_OLD(                  idtrPadding64, sizeof(uint64_t)),
    SSMFIELD_ENTRY(             CPUMCTX, ldtr.Sel),
    SSMFIELD_ENTRY_OLD(                  ldtrPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, tr.Sel),
    SSMFIELD_ENTRY_OLD(                  trPadding, sizeof(uint16_t)),
    SSMFIELD_ENTRY(             CPUMCTX, SysEnter.cs),
    SSMFIELD_ENTRY(             CPUMCTX, SysEnter.eip),
    SSMFIELD_ENTRY(             CPUMCTX, SysEnter.esp),
    SSMFIELD_ENTRY(             CPUMCTX, msrEFER),
    SSMFIELD_ENTRY(             CPUMCTX, msrSTAR),
    SSMFIELD_ENTRY(             CPUMCTX, msrPAT),
    SSMFIELD_ENTRY(             CPUMCTX, msrLSTAR),
    SSMFIELD_ENTRY(             CPUMCTX, msrCSTAR),
    SSMFIELD_ENTRY(             CPUMCTX, msrSFMASK),
    SSMFIELD_ENTRY_OLD(                  msrFSBASE, sizeof(uint64_t)),
    SSMFIELD_ENTRY_OLD(                  msrGSBASE, sizeof(uint64_t)),
    SSMFIELD_ENTRY(             CPUMCTX, msrKERNELGSBASE),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, ldtr.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, ldtr.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, ldtr.Attr),
    SSMFIELD_ENTRY_U32_ZX_U64(  CPUMCTX, tr.u64Base),
    SSMFIELD_ENTRY(             CPUMCTX, tr.u32Limit),
    SSMFIELD_ENTRY(             CPUMCTX, tr.Attr),
    SSMFIELD_ENTRY_OLD(                  padding, sizeof(uint32_t)*2),
    SSMFIELD_ENTRY_TERM()
};


/**
 * Initializes the CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int) CPUMR3Init(PVM pVM)
{
    LogFlow(("CPUMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, cpum.s, 32);
    AssertCompile(sizeof(pVM->cpum.s) <= sizeof(pVM->cpum.padding));
    AssertCompileSizeAlignment(CPUMCTX, 64);
    AssertCompileSizeAlignment(CPUMCTXMSRS, 64);
    AssertCompileSizeAlignment(CPUMHOSTCTX, 64);
    AssertCompileMemberAlignment(VM, cpum, 64);
    AssertCompileMemberAlignment(VM, aCpus, 64);
    AssertCompileMemberAlignment(VMCPU, cpum.s, 64);
    AssertCompileMemberSizeAlignment(VM, aCpus[0].cpum.s, 64);

    /* Calculate the offset from CPUM to CPUMCPU for the first CPU. */
    pVM->cpum.s.offCPUMCPU0 = RT_OFFSETOF(VM, aCpus[0].cpum) - RT_OFFSETOF(VM, cpum);
    Assert((uintptr_t)&pVM->cpum + pVM->cpum.s.offCPUMCPU0 == (uintptr_t)&pVM->aCpus[0].cpum);

    /* Calculate the offset from CPUMCPU to CPUM. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        /*
         * Setup any fixed pointers and offsets.
         */
        pVCpu->cpum.s.pHyperCoreR3 = CPUMCTX2CORE(&pVCpu->cpum.s.Hyper);
        pVCpu->cpum.s.pHyperCoreR0 = VM_R0_ADDR(pVM, CPUMCTX2CORE(&pVCpu->cpum.s.Hyper));

        pVCpu->cpum.s.offCPUM      = RT_OFFSETOF(VM, aCpus[i].cpum) - RT_OFFSETOF(VM, cpum);
        Assert((uintptr_t)&pVCpu->cpum - pVCpu->cpum.s.offCPUM == (uintptr_t)&pVM->cpum);
    }

    /*
     * Check that the CPU supports the minimum features we require.
     */
    if (!ASMHasCpuId())
    {
        Log(("The CPU doesn't support CPUID!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    ASMCpuId_ECX_EDX(1, &pVM->cpum.s.CPUFeatures.ecx, &pVM->cpum.s.CPUFeatures.edx);
    ASMCpuId_ECX_EDX(0x80000001, &pVM->cpum.s.CPUFeaturesExt.ecx, &pVM->cpum.s.CPUFeaturesExt.edx);

    /* Setup the CR4 AND and OR masks used in the switcher */
    /* Depends on the presence of FXSAVE(SSE) support on the host CPU */
    if (!pVM->cpum.s.CPUFeatures.edx.u1FXSR)
    {
        Log(("The CPU doesn't support FXSAVE/FXRSTOR!\n"));
        /* No FXSAVE implies no SSE */
        pVM->cpum.s.CR4.AndMask = X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = 0;
    }
    else
    {
        pVM->cpum.s.CR4.AndMask = X86_CR4_OSXMMEEXCPT | X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = X86_CR4_OSFSXR;
    }

    if (!pVM->cpum.s.CPUFeatures.edx.u1MMX)
    {
        Log(("The CPU doesn't support MMX!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    if (!pVM->cpum.s.CPUFeatures.edx.u1TSC)
    {
        Log(("The CPU doesn't support TSC!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    /* Bogus on AMD? */
    if (!pVM->cpum.s.CPUFeatures.edx.u1SEP)
        Log(("The CPU doesn't support SYSENTER/SYSEXIT!\n"));

    /*
     * Detect the host CPU vendor.
     * (The guest CPU vendor is re-detected later on.)
     */
    uint32_t uEAX, uEBX, uECX, uEDX;
    ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
    pVM->cpum.s.enmHostCpuVendor = cpumR3DetectVendor(uEAX, uEBX, uECX, uEDX);
    pVM->cpum.s.enmGuestCpuVendor = pVM->cpum.s.enmHostCpuVendor;

    /*
     * Setup hypervisor startup values.
     */

    /*
     * Register saved state data item.
     */
    int rc = SSMR3RegisterInternal(pVM, "cpum", 1, CPUM_SAVED_STATE_VERSION, sizeof(CPUM),
                                   NULL, cpumR3LiveExec, NULL,
                                   NULL, cpumR3SaveExec, NULL,
                                   cpumR3LoadPrep, cpumR3LoadExec, cpumR3LoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register info handlers and registers with the debugger facility.
     */
    DBGFR3InfoRegisterInternal(pVM, "cpum",             "Displays the all the cpu states.",         &cpumR3InfoAll);
    DBGFR3InfoRegisterInternal(pVM, "cpumguest",        "Displays the guest cpu state.",            &cpumR3InfoGuest);
    DBGFR3InfoRegisterInternal(pVM, "cpumhyper",        "Displays the hypervisor cpu state.",       &cpumR3InfoHyper);
    DBGFR3InfoRegisterInternal(pVM, "cpumhost",         "Displays the host cpu state.",             &cpumR3InfoHost);
    DBGFR3InfoRegisterInternal(pVM, "cpuid",            "Displays the guest cpuid leaves.",         &cpumR3CpuIdInfo);
    DBGFR3InfoRegisterInternal(pVM, "cpumguestinstr",   "Displays the current guest instruction.",  &cpumR3InfoGuestInstr);

    rc = cpumR3DbgInit(pVM);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the Guest CPUID state.
     */
    rc = cpumR3CpuIdInit(pVM);
    if (RT_FAILURE(rc))
        return rc;
    CPUMR3Reset(pVM);
    return VINF_SUCCESS;
}


/**
 * Detect the CPU vendor give n the
 *
 * @returns The vendor.
 * @param   uEAX                EAX from CPUID(0).
 * @param   uEBX                EBX from CPUID(0).
 * @param   uECX                ECX from CPUID(0).
 * @param   uEDX                EDX from CPUID(0).
 */
static CPUMCPUVENDOR cpumR3DetectVendor(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX)
{
    if (    uEAX >= 1
        &&  uEBX == X86_CPUID_VENDOR_AMD_EBX
        &&  uECX == X86_CPUID_VENDOR_AMD_ECX
        &&  uEDX == X86_CPUID_VENDOR_AMD_EDX)
        return CPUMCPUVENDOR_AMD;

    if (    uEAX >= 1
        &&  uEBX == X86_CPUID_VENDOR_INTEL_EBX
        &&  uECX == X86_CPUID_VENDOR_INTEL_ECX
        &&  uEDX == X86_CPUID_VENDOR_INTEL_EDX)
        return CPUMCPUVENDOR_INTEL;

    /** @todo detect the other buggers... */
    return CPUMCPUVENDOR_UNKNOWN;
}


/**
 * Fetches overrides for a CPUID leaf.
 *
 * @returns VBox status code.
 * @param   pLeaf               The leaf to load the overrides into.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 * @param   iLeaf               The CPUID leaf number.
 */
static int cpumR3CpuIdFetchLeafOverride(PCPUMCPUID pLeaf, PCFGMNODE pCfgNode, uint32_t iLeaf)
{
    PCFGMNODE pLeafNode = CFGMR3GetChildF(pCfgNode, "%RX32", iLeaf);
    if (pLeafNode)
    {
        uint32_t u32;
        int rc = CFGMR3QueryU32(pLeafNode, "eax", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->eax = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ebx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ebx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ecx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ecx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "edx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->edx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

    }
    return VINF_SUCCESS;
}


/**
 * Load the overrides for a set of CPUID leaves.
 *
 * @returns VBox status code.
 * @param   paLeaves            The leaf array.
 * @param   cLeaves             The number of leaves.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 */
static int cpumR3CpuIdInitLoadOverrideSet(uint32_t uStart, PCPUMCPUID paLeaves, uint32_t cLeaves, PCFGMNODE pCfgNode)
{
    for (uint32_t i = 0; i < cLeaves; i++)
    {
        int rc = cpumR3CpuIdFetchLeafOverride(&paLeaves[i], pCfgNode, uStart + i);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

/**
 * Init a set of host CPUID leaves.
 *
 * @returns VBox status code.
 * @param   paLeaves            The leaf array.
 * @param   cLeaves             The number of leaves.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The /CPUM/HostCPUID/ node.
 */
static int cpumR3CpuIdInitHostSet(uint32_t uStart, PCPUMCPUID paLeaves, uint32_t cLeaves, PCFGMNODE pCfgNode)
{
    /* Using the ECX variant for all of them can't hurt... */
    for (uint32_t i = 0; i < cLeaves; i++)
        ASMCpuId_Idx_ECX(uStart + i, 0, &paLeaves[i].eax, &paLeaves[i].ebx, &paLeaves[i].ecx, &paLeaves[i].edx);

    /* Load CPUID leaf override; we currently don't care if the user
       specifies features the host CPU doesn't support. */
    return cpumR3CpuIdInitLoadOverrideSet(uStart, paLeaves, cLeaves, pCfgNode);
}


/**
 * Initializes the emulated CPU's cpuid information.
 *
 * @returns VBox status code.
 * @param   pVM          Pointer to the VM.
 */
static int cpumR3CpuIdInit(PVM pVM)
{
    PCPUM       pCPUM    = &pVM->cpum.s;
    PCFGMNODE   pCpumCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM");
    uint32_t    i;
    int         rc;

#define PORTABLE_CLEAR_BITS_WHEN(Lvl, LeafSuffReg, FeatNm, fMask, uValue) \
    if (pCPUM->u8PortableCpuIdLevel >= (Lvl) && (pCPUM->aGuestCpuId##LeafSuffReg & (fMask)) == (uValue) ) \
    { \
        LogRel(("PortableCpuId: " #LeafSuffReg "[" #FeatNm "]: %#x -> 0\n", pCPUM->aGuestCpuId##LeafSuffReg & (fMask))); \
        pCPUM->aGuestCpuId##LeafSuffReg &= ~(uint32_t)(fMask); \
    }
#define PORTABLE_DISABLE_FEATURE_BIT(Lvl, LeafSuffReg, FeatNm, fBitMask) \
    if (pCPUM->u8PortableCpuIdLevel >= (Lvl) && (pCPUM->aGuestCpuId##LeafSuffReg & (fBitMask)) ) \
    { \
        LogRel(("PortableCpuId: " #LeafSuffReg "[" #FeatNm "]: 1 -> 0\n")); \
        pCPUM->aGuestCpuId##LeafSuffReg &= ~(uint32_t)(fBitMask); \
    }

    /*
     * Read the configuration.
     */
    /** @cfgm{CPUM/SyntheticCpu, boolean, false}
     * Enables the Synthetic CPU.  The Vendor ID and Processor Name are
     * completely overridden by VirtualBox custom strings.  Some
     * CPUID information is withheld, like the cache info. */
    rc = CFGMR3QueryBoolDef(pCpumCfg, "SyntheticCpu",  &pCPUM->fSyntheticCpu, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{CPUM/PortableCpuIdLevel, 8-bit, 0, 3, 0}
     * When non-zero CPUID features that could cause portability issues will be
     * stripped.  The higher the value the more features gets stripped.  Higher
     * values should only be used when older CPUs are involved since it may
     * harm performance and maybe also cause problems with specific guests. */
    rc = CFGMR3QueryU8Def(pCpumCfg, "PortableCpuIdLevel", &pCPUM->u8PortableCpuIdLevel, 0);
    AssertRCReturn(rc, rc);

    AssertLogRelReturn(!pCPUM->fSyntheticCpu || !pCPUM->u8PortableCpuIdLevel, VERR_CPUM_INCOMPATIBLE_CONFIG);

    /*
     * Get the host CPUID leaves and redetect the guest CPU vendor (could've
     * been overridden).
     */
    /** @cfgm{CPUM/HostCPUID/[000000xx|800000xx|c000000x]/[eax|ebx|ecx|edx],32-bit}
     * Overrides the host CPUID leaf values used for calculating the guest CPUID
     * leaves.  This can be used to preserve the CPUID values when moving a VM
     * to a different machine.  Another use is restricting (or extending) the
     * feature set exposed to the guest. */
    PCFGMNODE pHostOverrideCfg = CFGMR3GetChild(pCpumCfg, "HostCPUID");
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0x00000000), &pCPUM->aGuestCpuIdStd[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdStd),     pHostOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0x80000000), &pCPUM->aGuestCpuIdExt[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdExt),     pHostOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0xc0000000), &pCPUM->aGuestCpuIdCentaur[0], RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur), pHostOverrideCfg);
    AssertRCReturn(rc, rc);

    pCPUM->enmGuestCpuVendor = cpumR3DetectVendor(pCPUM->aGuestCpuIdStd[0].eax, pCPUM->aGuestCpuIdStd[0].ebx,
                                                  pCPUM->aGuestCpuIdStd[0].ecx, pCPUM->aGuestCpuIdStd[0].edx);

    /*
     * Determine the default leaf.
     *
     * Intel returns values of the highest standard function, while AMD
     * returns zeros. VIA on the other hand seems to returning nothing or
     * perhaps some random garbage, we don't try to duplicate this behavior.
     */
    ASMCpuId(pCPUM->aGuestCpuIdStd[0].eax + 10, /** @todo r=bird: Use the host value here in case of overrides and more than 10 leaves being stripped already. */
             &pCPUM->GuestCpuIdDef.eax, &pCPUM->GuestCpuIdDef.ebx,
             &pCPUM->GuestCpuIdDef.ecx, &pCPUM->GuestCpuIdDef.edx);


    /* Cpuid 1 & 0x80000001:
     * Only report features we can support.
     *
     * Note! When enabling new features the Synthetic CPU and Portable CPUID
     *       options may require adjusting (i.e. stripping what was enabled).
     */
    pCPUM->aGuestCpuIdStd[1].edx &= X86_CPUID_FEATURE_EDX_FPU
                                  | X86_CPUID_FEATURE_EDX_VME
                                  | X86_CPUID_FEATURE_EDX_DE
                                  | X86_CPUID_FEATURE_EDX_PSE
                                  | X86_CPUID_FEATURE_EDX_TSC
                                  | X86_CPUID_FEATURE_EDX_MSR
                                  //| X86_CPUID_FEATURE_EDX_PAE   - set later if configured.
                                  | X86_CPUID_FEATURE_EDX_MCE
                                  | X86_CPUID_FEATURE_EDX_CX8
                                  //| X86_CPUID_FEATURE_EDX_APIC  - set by the APIC device if present.
                                  /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see @bugref{1757}) */
                                  //| X86_CPUID_FEATURE_EDX_SEP
                                  | X86_CPUID_FEATURE_EDX_MTRR
                                  | X86_CPUID_FEATURE_EDX_PGE
                                  | X86_CPUID_FEATURE_EDX_MCA
                                  | X86_CPUID_FEATURE_EDX_CMOV
                                  | X86_CPUID_FEATURE_EDX_PAT
                                  | X86_CPUID_FEATURE_EDX_PSE36
                                  //| X86_CPUID_FEATURE_EDX_PSN   - no serial number.
                                  | X86_CPUID_FEATURE_EDX_CLFSH
                                  //| X86_CPUID_FEATURE_EDX_DS    - no debug store.
                                  //| X86_CPUID_FEATURE_EDX_ACPI  - not virtualized yet.
                                  | X86_CPUID_FEATURE_EDX_MMX
                                  | X86_CPUID_FEATURE_EDX_FXSR
                                  | X86_CPUID_FEATURE_EDX_SSE
                                  | X86_CPUID_FEATURE_EDX_SSE2
                                  //| X86_CPUID_FEATURE_EDX_SS    - no self snoop.
                                  //| X86_CPUID_FEATURE_EDX_HTT   - no hyperthreading.
                                  //| X86_CPUID_FEATURE_EDX_TM    - no thermal monitor.
                                  //| X86_CPUID_FEATURE_EDX_PBE   - no pending break enabled.
                                  | 0;
    pCPUM->aGuestCpuIdStd[1].ecx &= 0
                                  | X86_CPUID_FEATURE_ECX_SSE3
                                  /* Can't properly emulate monitor & mwait with guest SMP; force the guest to use hlt for idling VCPUs. */
                                  | ((pVM->cCpus == 1) ? X86_CPUID_FEATURE_ECX_MONITOR : 0)
                                  //| X86_CPUID_FEATURE_ECX_CPLDS - no CPL qualified debug store.
                                  //| X86_CPUID_FEATURE_ECX_VMX   - not virtualized.
                                  //| X86_CPUID_FEATURE_ECX_EST   - no extended speed step.
                                  //| X86_CPUID_FEATURE_ECX_TM2   - no thermal monitor 2.
                                    | X86_CPUID_FEATURE_ECX_SSSE3
                                  //| X86_CPUID_FEATURE_ECX_CNTXID - no L1 context id (MSR++).
                                  //| X86_CPUID_FEATURE_ECX_CX16  - no cmpxchg16b
                                  /* ECX Bit 14 - xTPR Update Control. Processor supports changing IA32_MISC_ENABLES[bit 23]. */
                                  //| X86_CPUID_FEATURE_ECX_TPRUPDATE
                                  /* ECX Bit 21 - x2APIC support - not yet. */
                                  // | X86_CPUID_FEATURE_ECX_X2APIC
                                  /* ECX Bit 23 - POPCNT instruction. */
                                  //| X86_CPUID_FEATURE_ECX_POPCNT
                                  | 0;
    if (pCPUM->u8PortableCpuIdLevel > 0)
    {
        PORTABLE_CLEAR_BITS_WHEN(1, Std[1].eax, ProcessorType, (UINT32_C(3) << 12), (UINT32_C(2) << 12));
        PORTABLE_DISABLE_FEATURE_BIT(1, Std[1].ecx, SSSE3, X86_CPUID_FEATURE_ECX_SSSE3);
        PORTABLE_DISABLE_FEATURE_BIT(1, Std[1].ecx, SSE3,  X86_CPUID_FEATURE_ECX_SSE3);
        PORTABLE_DISABLE_FEATURE_BIT(2, Std[1].edx, SSE2,  X86_CPUID_FEATURE_EDX_SSE2);
        PORTABLE_DISABLE_FEATURE_BIT(3, Std[1].edx, SSE,   X86_CPUID_FEATURE_EDX_SSE);
        PORTABLE_DISABLE_FEATURE_BIT(3, Std[1].edx, CLFSH, X86_CPUID_FEATURE_EDX_CLFSH);
        PORTABLE_DISABLE_FEATURE_BIT(3, Std[1].edx, CMOV,  X86_CPUID_FEATURE_EDX_CMOV);

        Assert(!(pCPUM->aGuestCpuIdStd[1].edx & (  X86_CPUID_FEATURE_EDX_SEP
                                                 | X86_CPUID_FEATURE_EDX_PSN
                                                 | X86_CPUID_FEATURE_EDX_DS
                                                 | X86_CPUID_FEATURE_EDX_ACPI
                                                 | X86_CPUID_FEATURE_EDX_SS
                                                 | X86_CPUID_FEATURE_EDX_TM
                                                 | X86_CPUID_FEATURE_EDX_PBE
                                                 )));
        Assert(!(pCPUM->aGuestCpuIdStd[1].ecx & (  X86_CPUID_FEATURE_ECX_PCLMUL
                                                 | X86_CPUID_FEATURE_ECX_DTES64
                                                 | X86_CPUID_FEATURE_ECX_CPLDS
                                                 | X86_CPUID_FEATURE_ECX_VMX
                                                 | X86_CPUID_FEATURE_ECX_SMX
                                                 | X86_CPUID_FEATURE_ECX_EST
                                                 | X86_CPUID_FEATURE_ECX_TM2
                                                 | X86_CPUID_FEATURE_ECX_CNTXID
                                                 | X86_CPUID_FEATURE_ECX_FMA
                                                 | X86_CPUID_FEATURE_ECX_CX16
                                                 | X86_CPUID_FEATURE_ECX_TPRUPDATE
                                                 | X86_CPUID_FEATURE_ECX_PDCM
                                                 | X86_CPUID_FEATURE_ECX_DCA
                                                 | X86_CPUID_FEATURE_ECX_MOVBE
                                                 | X86_CPUID_FEATURE_ECX_AES
                                                 | X86_CPUID_FEATURE_ECX_POPCNT
                                                 | X86_CPUID_FEATURE_ECX_XSAVE
                                                 | X86_CPUID_FEATURE_ECX_OSXSAVE
                                                 | X86_CPUID_FEATURE_ECX_AVX
                                                 )));
    }

    /* Cpuid 0x80000001:
     * Only report features we can support.
     *
     * Note! When enabling new features the Synthetic CPU and Portable CPUID
     *       options may require adjusting (i.e. stripping what was enabled).
     *
     * ASSUMES that this is ALWAYS the AMD defined feature set if present.
     */
    pCPUM->aGuestCpuIdExt[1].edx &= X86_CPUID_AMD_FEATURE_EDX_FPU
                                  | X86_CPUID_AMD_FEATURE_EDX_VME
                                  | X86_CPUID_AMD_FEATURE_EDX_DE
                                  | X86_CPUID_AMD_FEATURE_EDX_PSE
                                  | X86_CPUID_AMD_FEATURE_EDX_TSC
                                  | X86_CPUID_AMD_FEATURE_EDX_MSR //?? this means AMD MSRs..
                                  //| X86_CPUID_AMD_FEATURE_EDX_PAE    - not implemented yet.
                                  //| X86_CPUID_AMD_FEATURE_EDX_MCE    - not virtualized yet.
                                  | X86_CPUID_AMD_FEATURE_EDX_CX8
                                  //| X86_CPUID_AMD_FEATURE_EDX_APIC   - set by the APIC device if present.
                                  /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see @bugref{1757}) */
                                  //| X86_CPUID_AMD_FEATURE_EDX_SEP
                                  | X86_CPUID_AMD_FEATURE_EDX_MTRR
                                  | X86_CPUID_AMD_FEATURE_EDX_PGE
                                  | X86_CPUID_AMD_FEATURE_EDX_MCA
                                  | X86_CPUID_AMD_FEATURE_EDX_CMOV
                                  | X86_CPUID_AMD_FEATURE_EDX_PAT
                                  | X86_CPUID_AMD_FEATURE_EDX_PSE36
                                  //| X86_CPUID_AMD_FEATURE_EDX_NX     - not virtualized, requires PAE.
                                  //| X86_CPUID_AMD_FEATURE_EDX_AXMMX
                                  | X86_CPUID_AMD_FEATURE_EDX_MMX
                                  | X86_CPUID_AMD_FEATURE_EDX_FXSR
                                  | X86_CPUID_AMD_FEATURE_EDX_FFXSR
                                  //| X86_CPUID_AMD_FEATURE_EDX_PAGE1GB
                                  //| X86_CPUID_AMD_FEATURE_EDX_RDTSCP - AMD only; turned on when necessary
                                  //| X86_CPUID_AMD_FEATURE_EDX_LONG_MODE - turned on when necessary
                                  | X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX
                                  | X86_CPUID_AMD_FEATURE_EDX_3DNOW
                                  | 0;
    pCPUM->aGuestCpuIdExt[1].ecx &= 0
                                  //| X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF
                                  //| X86_CPUID_AMD_FEATURE_ECX_CMPL
                                  //| X86_CPUID_AMD_FEATURE_ECX_SVM    - not virtualized.
                                  //| X86_CPUID_AMD_FEATURE_ECX_EXT_APIC
                                  /* Note: This could prevent teleporting from AMD to Intel CPUs! */
                                  | X86_CPUID_AMD_FEATURE_ECX_CR8L         /* expose lock mov cr0 = mov cr8 hack for guests that can use this feature to access the TPR. */
                                  //| X86_CPUID_AMD_FEATURE_ECX_ABM
                                  //| X86_CPUID_AMD_FEATURE_ECX_SSE4A
                                  //| X86_CPUID_AMD_FEATURE_ECX_MISALNSSE
                                  //| X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF
                                  //| X86_CPUID_AMD_FEATURE_ECX_OSVW
                                  //| X86_CPUID_AMD_FEATURE_ECX_IBS
                                  //| X86_CPUID_AMD_FEATURE_ECX_SSE5
                                  //| X86_CPUID_AMD_FEATURE_ECX_SKINIT
                                  //| X86_CPUID_AMD_FEATURE_ECX_WDT
                                  | 0;
    if (pCPUM->u8PortableCpuIdLevel > 0)
    {
        PORTABLE_DISABLE_FEATURE_BIT(1, Ext[1].ecx, CR8L,       X86_CPUID_AMD_FEATURE_ECX_CR8L);
        PORTABLE_DISABLE_FEATURE_BIT(1, Ext[1].edx, 3DNOW,      X86_CPUID_AMD_FEATURE_EDX_3DNOW);
        PORTABLE_DISABLE_FEATURE_BIT(1, Ext[1].edx, 3DNOW_EX,   X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
        PORTABLE_DISABLE_FEATURE_BIT(1, Ext[1].edx, FFXSR,      X86_CPUID_AMD_FEATURE_EDX_FFXSR);
        PORTABLE_DISABLE_FEATURE_BIT(1, Ext[1].edx, RDTSCP,     X86_CPUID_AMD_FEATURE_EDX_RDTSCP);
        PORTABLE_DISABLE_FEATURE_BIT(2, Ext[1].ecx, LAHF_SAHF,  X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF);
        PORTABLE_DISABLE_FEATURE_BIT(3, Ext[1].ecx, CMOV,       X86_CPUID_AMD_FEATURE_EDX_CMOV);

        Assert(!(pCPUM->aGuestCpuIdExt[1].ecx & (  X86_CPUID_AMD_FEATURE_ECX_CMPL
                                                 | X86_CPUID_AMD_FEATURE_ECX_SVM
                                                 | X86_CPUID_AMD_FEATURE_ECX_EXT_APIC
                                                 | X86_CPUID_AMD_FEATURE_ECX_CR8L
                                                 | X86_CPUID_AMD_FEATURE_ECX_ABM
                                                 | X86_CPUID_AMD_FEATURE_ECX_SSE4A
                                                 | X86_CPUID_AMD_FEATURE_ECX_MISALNSSE
                                                 | X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF
                                                 | X86_CPUID_AMD_FEATURE_ECX_OSVW
                                                 | X86_CPUID_AMD_FEATURE_ECX_IBS
                                                 | X86_CPUID_AMD_FEATURE_ECX_SSE5
                                                 | X86_CPUID_AMD_FEATURE_ECX_SKINIT
                                                 | X86_CPUID_AMD_FEATURE_ECX_WDT
                                                 | UINT32_C(0xffffc000)
                                                 )));
        Assert(!(pCPUM->aGuestCpuIdExt[1].edx & (  RT_BIT(10)
                                                 | X86_CPUID_AMD_FEATURE_EDX_SEP
                                                 | RT_BIT(18)
                                                 | RT_BIT(19)
                                                 | RT_BIT(21)
                                                 | X86_CPUID_AMD_FEATURE_EDX_AXMMX
                                                 | X86_CPUID_AMD_FEATURE_EDX_PAGE1GB
                                                 | RT_BIT(28)
                                                 )));
    }

    /*
     * Apply the Synthetic CPU modifications. (TODO: move this up)
     */
    if (pCPUM->fSyntheticCpu)
    {
        static const char s_szVendor[13]    = "VirtualBox  ";
        static const char s_szProcessor[48] = "VirtualBox SPARCx86 Processor v1000            "; /* includes null terminator */

        pCPUM->enmGuestCpuVendor = CPUMCPUVENDOR_SYNTHETIC;

        /* Limit the nr of standard leaves; 5 for monitor/mwait */
        pCPUM->aGuestCpuIdStd[0].eax = RT_MIN(pCPUM->aGuestCpuIdStd[0].eax, 5);

        /* 0: Vendor */
        pCPUM->aGuestCpuIdStd[0].ebx = pCPUM->aGuestCpuIdExt[0].ebx = ((uint32_t *)s_szVendor)[0];
        pCPUM->aGuestCpuIdStd[0].ecx = pCPUM->aGuestCpuIdExt[0].ecx = ((uint32_t *)s_szVendor)[2];
        pCPUM->aGuestCpuIdStd[0].edx = pCPUM->aGuestCpuIdExt[0].edx = ((uint32_t *)s_szVendor)[1];

        /* 1.eax: Version information.  family : model : stepping */
        pCPUM->aGuestCpuIdStd[1].eax = (0xf << 8) + (0x1 << 4) + 1;

        /* Leaves 2 - 4 are Intel only - zero them out */
        memset(&pCPUM->aGuestCpuIdStd[2], 0, sizeof(pCPUM->aGuestCpuIdStd[2]));
        memset(&pCPUM->aGuestCpuIdStd[3], 0, sizeof(pCPUM->aGuestCpuIdStd[3]));
        memset(&pCPUM->aGuestCpuIdStd[4], 0, sizeof(pCPUM->aGuestCpuIdStd[4]));

        /* Leaf 5 = monitor/mwait */

        /* Limit the nr of extended leaves: 0x80000008 to include the max virtual and physical address size (64 bits guests). */
        pCPUM->aGuestCpuIdExt[0].eax = RT_MIN(pCPUM->aGuestCpuIdExt[0].eax, 0x80000008);
        /* AMD only - set to zero. */
        pCPUM->aGuestCpuIdExt[0].ebx = pCPUM->aGuestCpuIdExt[0].ecx = pCPUM->aGuestCpuIdExt[0].edx = 0;

        /* 0x800000001: AMD only; shared feature bits are set dynamically. */
        memset(&pCPUM->aGuestCpuIdExt[1], 0, sizeof(pCPUM->aGuestCpuIdExt[1]));

        /* 0x800000002-4: Processor Name String Identifier. */
        pCPUM->aGuestCpuIdExt[2].eax = ((uint32_t *)s_szProcessor)[0];
        pCPUM->aGuestCpuIdExt[2].ebx = ((uint32_t *)s_szProcessor)[1];
        pCPUM->aGuestCpuIdExt[2].ecx = ((uint32_t *)s_szProcessor)[2];
        pCPUM->aGuestCpuIdExt[2].edx = ((uint32_t *)s_szProcessor)[3];
        pCPUM->aGuestCpuIdExt[3].eax = ((uint32_t *)s_szProcessor)[4];
        pCPUM->aGuestCpuIdExt[3].ebx = ((uint32_t *)s_szProcessor)[5];
        pCPUM->aGuestCpuIdExt[3].ecx = ((uint32_t *)s_szProcessor)[6];
        pCPUM->aGuestCpuIdExt[3].edx = ((uint32_t *)s_szProcessor)[7];
        pCPUM->aGuestCpuIdExt[4].eax = ((uint32_t *)s_szProcessor)[8];
        pCPUM->aGuestCpuIdExt[4].ebx = ((uint32_t *)s_szProcessor)[9];
        pCPUM->aGuestCpuIdExt[4].ecx = ((uint32_t *)s_szProcessor)[10];
        pCPUM->aGuestCpuIdExt[4].edx = ((uint32_t *)s_szProcessor)[11];

        /* 0x800000005-7 - reserved -> zero */
        memset(&pCPUM->aGuestCpuIdExt[5], 0, sizeof(pCPUM->aGuestCpuIdExt[5]));
        memset(&pCPUM->aGuestCpuIdExt[6], 0, sizeof(pCPUM->aGuestCpuIdExt[6]));
        memset(&pCPUM->aGuestCpuIdExt[7], 0, sizeof(pCPUM->aGuestCpuIdExt[7]));

        /* 0x800000008: only the max virtual and physical address size. */
        pCPUM->aGuestCpuIdExt[8].ecx = pCPUM->aGuestCpuIdExt[8].ebx = pCPUM->aGuestCpuIdExt[8].edx = 0;  /* reserved */
    }

    /*
     * Hide HTT, multicode, SMP, whatever.
     * (APIC-ID := 0 and #LogCpus := 0)
     */
    pCPUM->aGuestCpuIdStd[1].ebx &= 0x0000ffff;
#ifdef VBOX_WITH_MULTI_CORE
    if (    pCPUM->enmGuestCpuVendor != CPUMCPUVENDOR_SYNTHETIC
        &&  pVM->cCpus > 1)
    {
        /* If CPUID Fn0000_0001_EDX[HTT] = 1 then LogicalProcessorCount is the number of threads per CPU core times the number of CPU cores per processor */
        pCPUM->aGuestCpuIdStd[1].ebx |= (pVM->cCpus << 16);
        pCPUM->aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_HTT;  /* necessary for hyper-threading *or* multi-core CPUs */
    }
#endif

    /* Cpuid 2:
     * Intel: Cache and TLB information
     * AMD:   Reserved
     * Safe to expose; restrict the number of calls to 1 for the portable case.
     */
    if (    pCPUM->u8PortableCpuIdLevel > 0
        &&  pCPUM->aGuestCpuIdStd[0].eax >= 2
        && (pCPUM->aGuestCpuIdStd[2].eax & 0xff) > 1)
    {
        LogRel(("PortableCpuId: Std[2].al: %d -> 1\n", pCPUM->aGuestCpuIdStd[2].eax & 0xff));
        pCPUM->aGuestCpuIdStd[2].eax &= UINT32_C(0xfffffffe);
    }

    /* Cpuid 3:
     * Intel: EAX, EBX - reserved (transmeta uses these)
     *        ECX, EDX - Processor Serial Number if available, otherwise reserved
     * AMD:   Reserved
     * Safe to expose
     */
    if (!(pCPUM->aGuestCpuIdStd[1].edx & X86_CPUID_FEATURE_EDX_PSN))
    {
        pCPUM->aGuestCpuIdStd[3].ecx = pCPUM->aGuestCpuIdStd[3].edx = 0;
        if (pCPUM->u8PortableCpuIdLevel > 0)
            pCPUM->aGuestCpuIdStd[3].eax = pCPUM->aGuestCpuIdStd[3].ebx = 0;
    }

    /* Cpuid 4:
     * Intel: Deterministic Cache Parameters Leaf
     *        Note: Depends on the ECX input! -> Feeling rather lazy now, so we just return 0
     * AMD:   Reserved
     * Safe to expose, except for EAX:
     *      Bits 25-14: Maximum number of addressable IDs for logical processors sharing this cache (see note)**
     *      Bits 31-26: Maximum number of processor cores in this physical package**
     * Note: These SMP values are constant regardless of ECX
     */
    pCPUM->aGuestCpuIdStd[4].ecx = pCPUM->aGuestCpuIdStd[4].edx = 0;
    pCPUM->aGuestCpuIdStd[4].eax = pCPUM->aGuestCpuIdStd[4].ebx = 0;
#ifdef VBOX_WITH_MULTI_CORE
    if (    pVM->cCpus > 1
        &&  pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        AssertReturn(pVM->cCpus <= 64, VERR_TOO_MANY_CPUS);
        /* One logical processor with possibly multiple cores. */
        /* See  http://www.intel.com/Assets/PDF/appnote/241618.pdf p. 29 */
        pCPUM->aGuestCpuIdStd[4].eax |= ((pVM->cCpus - 1) << 26);   /* 6 bits only -> 64 cores! */
    }
#endif

    /* Cpuid 5:     Monitor/mwait Leaf
     * Intel: ECX, EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     * AMD:   EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     *        ECX - extensions (ignored for now)
     * Safe to expose
     */
    if (!(pCPUM->aGuestCpuIdStd[1].ecx & X86_CPUID_FEATURE_ECX_MONITOR))
        pCPUM->aGuestCpuIdStd[5].eax = pCPUM->aGuestCpuIdStd[5].ebx = 0;

    pCPUM->aGuestCpuIdStd[5].ecx = pCPUM->aGuestCpuIdStd[5].edx = 0;
    /** @cfgm{/CPUM/MWaitExtensions, boolean, false}
     * Expose MWAIT extended features to the guest.  For now we expose
     * just MWAIT break on interrupt feature (bit 1).
     */
    bool fMWaitExtensions;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "MWaitExtensions", &fMWaitExtensions, false); AssertRCReturn(rc, rc);
    if (fMWaitExtensions)
    {
        pCPUM->aGuestCpuIdStd[5].ecx = X86_CPUID_MWAIT_ECX_EXT | X86_CPUID_MWAIT_ECX_BREAKIRQIF0;
        /* @todo: for now we just expose host's MWAIT C-states, although conceptually
           it shall be part of our power management virtualization model */
#if 0
        /* MWAIT sub C-states */
        pCPUM->aGuestCpuIdStd[5].edx =
                (0 << 0)  /* 0 in C0 */ |
                (2 << 4)  /* 2 in C1 */ |
                (2 << 8)  /* 2 in C2 */ |
                (2 << 12) /* 2 in C3 */ |
                (0 << 16) /* 0 in C4 */
                ;
#endif
    }
    else
        pCPUM->aGuestCpuIdStd[5].ecx = pCPUM->aGuestCpuIdStd[5].edx = 0;

    /* Cpuid 0x800000005 & 0x800000006 contain information about L1, L2 & L3 cache and TLB identifiers.
     * Safe to pass on to the guest.
     *
     * Intel: 0x800000005 reserved
     *        0x800000006 L2 cache information
     * AMD:   0x800000005 L1 cache information
     *        0x800000006 L2/L3 cache information
     */

    /* Cpuid 0x800000007:
     * AMD:               EAX, EBX, ECX - reserved
     *                    EDX: Advanced Power Management Information
     * Intel:             Reserved
     */
    if (pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000007))
    {
        Assert(pVM->cpum.s.enmGuestCpuVendor != CPUMCPUVENDOR_INVALID);

        pCPUM->aGuestCpuIdExt[7].eax = pCPUM->aGuestCpuIdExt[7].ebx = pCPUM->aGuestCpuIdExt[7].ecx = 0;

        if (pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Only expose the TSC invariant capability bit to the guest. */
            pCPUM->aGuestCpuIdExt[7].edx    &= 0
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TS
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_FID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_VID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TTP
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TM
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_STC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_MC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_HWPSTATE
#if 0 /* We don't expose X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR, because newer
       * Linux kernels blindly assume that the AMD performance counters work
       * if this is set for 64 bits guests. (Can't really find a CPUID feature
       * bit for them though.) */
                                            | X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR
#endif
                                            | 0;
        }
        else
            pCPUM->aGuestCpuIdExt[7].edx    = 0;
    }

    /* Cpuid 0x800000008:
     * AMD:               EBX, EDX - reserved
     *                    EAX: Virtual/Physical/Guest address Size
     *                    ECX: Number of cores + APICIdCoreIdSize
     * Intel:             EAX: Virtual/Physical address Size
     *                    EBX, ECX, EDX - reserved
     */
    if (pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000008))
    {
        /* Only expose the virtual and physical address sizes to the guest. */
        pCPUM->aGuestCpuIdExt[8].eax &= UINT32_C(0x0000ffff);
        pCPUM->aGuestCpuIdExt[8].ebx = pCPUM->aGuestCpuIdExt[8].edx = 0;  /* reserved */
        /* Set APICIdCoreIdSize to zero (use legacy method to determine the number of cores per cpu)
         * NC (0-7) Number of cores; 0 equals 1 core */
        pCPUM->aGuestCpuIdExt[8].ecx = 0;
#ifdef VBOX_WITH_MULTI_CORE
        if (    pVM->cCpus > 1
            &&  pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Legacy method to determine the number of cores. */
            pCPUM->aGuestCpuIdExt[1].ecx |= X86_CPUID_AMD_FEATURE_ECX_CMPL;
            pCPUM->aGuestCpuIdExt[8].ecx |= (pVM->cCpus - 1); /* NC: Number of CPU cores - 1; 8 bits */
        }
#endif
    }

    /** @cfgm{/CPUM/NT4LeafLimit, boolean, false}
     * Limit the number of standard CPUID leaves to 0..3 to prevent NT4 from
     * bugchecking with MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED (0x3e).
     * This option corresponds somewhat to IA32_MISC_ENABLES.BOOT_NT4[bit 22].
     */
    bool fNt4LeafLimit;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "NT4LeafLimit", &fNt4LeafLimit, false); AssertRCReturn(rc, rc);
    if (fNt4LeafLimit)
        pCPUM->aGuestCpuIdStd[0].eax = 3; /** @todo r=bird: shouldn't we check if pCPUM->aGuestCpuIdStd[0].eax > 3 before setting it 3 here? */

    /*
     * Limit it the number of entries and fill the remaining with the defaults.
     *
     * The limits are masking off stuff about power saving and similar, this
     * is perhaps a bit crudely done as there is probably some relatively harmless
     * info too in these leaves (like words about having a constant TSC).
     */
    if (pCPUM->aGuestCpuIdStd[0].eax > 5)
        pCPUM->aGuestCpuIdStd[0].eax = 5;
    for (i = pCPUM->aGuestCpuIdStd[0].eax + 1; i < RT_ELEMENTS(pCPUM->aGuestCpuIdStd); i++)
        pCPUM->aGuestCpuIdStd[i] = pCPUM->GuestCpuIdDef;

    if (pCPUM->aGuestCpuIdExt[0].eax > UINT32_C(0x80000008))
        pCPUM->aGuestCpuIdExt[0].eax = UINT32_C(0x80000008);
    for (i = pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000000)
           ? pCPUM->aGuestCpuIdExt[0].eax - UINT32_C(0x80000000) + 1
           : 0;
         i < RT_ELEMENTS(pCPUM->aGuestCpuIdExt);
         i++)
        pCPUM->aGuestCpuIdExt[i] = pCPUM->GuestCpuIdDef;

    /*
     * Centaur stuff (VIA).
     *
     * The important part here (we think) is to make sure the 0xc0000000
     * function returns 0xc0000001. As for the features, we don't currently
     * let on about any of those... 0xc0000002 seems to be some
     * temperature/hz/++ stuff, include it as well (static).
     */
    if (    pCPUM->aGuestCpuIdCentaur[0].eax >= UINT32_C(0xc0000000)
        &&  pCPUM->aGuestCpuIdCentaur[0].eax <= UINT32_C(0xc0000004))
    {
        pCPUM->aGuestCpuIdCentaur[0].eax = RT_MIN(pCPUM->aGuestCpuIdCentaur[0].eax, UINT32_C(0xc0000002));
        pCPUM->aGuestCpuIdCentaur[1].edx = 0; /* all features hidden */
        for (i = pCPUM->aGuestCpuIdCentaur[0].eax - UINT32_C(0xc0000000);
             i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur);
             i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;
    }
    else
        for (i = 0; i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur); i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;

    /*
     * Hypervisor identification.
     *
     * We only return minimal information, primarily ensuring that the
     * 0x40000000 function returns 0x40000001 and identifying ourselves.
     * Currently we do not support any hypervisor-specific interface.
     */
    pCPUM->aGuestCpuIdHyper[0].eax = UINT32_C(0x40000001);
    pCPUM->aGuestCpuIdHyper[0].ebx = pCPUM->aGuestCpuIdHyper[0].ecx
                                   = pCPUM->aGuestCpuIdHyper[0].edx = 0x786f4256;   /* 'VBox' */
    pCPUM->aGuestCpuIdHyper[1].eax = 0x656e6f6e;                            /* 'none' */
    pCPUM->aGuestCpuIdHyper[1].ebx = pCPUM->aGuestCpuIdHyper[1].ecx
                                   = pCPUM->aGuestCpuIdHyper[1].edx = 0;    /* Reserved */

    /*
     * Load CPUID overrides from configuration.
     * Note: Kind of redundant now, but allows unchanged overrides
     */
    /** @cfgm{CPUM/CPUID/[000000xx|800000xx|c000000x]/[eax|ebx|ecx|edx],32-bit}
     * Overrides the CPUID leaf values. */
    PCFGMNODE pOverrideCfg = CFGMR3GetChild(pCpumCfg, "CPUID");
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x00000000), &pCPUM->aGuestCpuIdStd[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdStd),     pOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x80000000), &pCPUM->aGuestCpuIdExt[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdExt),     pOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0xc0000000), &pCPUM->aGuestCpuIdCentaur[0], RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur), pOverrideCfg);
    AssertRCReturn(rc, rc);

    /*
     * Check if PAE was explicitely enabled by the user.
     */
    bool fEnable;
    rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "EnablePAE", &fEnable, false);      AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);

    /*
     * We don't normally enable NX for raw-mode, so give the user a chance to
     * force it on.
     */
    rc = CFGMR3QueryBoolDef(pCpumCfg, "EnableNX", &fEnable, false);                 AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NXE);

    /*
     * We don't enable the Hypervisor Present bit by default, but it may
     * be needed by some guests.
     */
    rc = CFGMR3QueryBoolDef(pCpumCfg, "EnableHVP", &fEnable, false);                AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);
    /*
     * Log the cpuid and we're good.
     */
    bool fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);
    RTCPUSET OnlineSet;
    LogRel(("Logical host processors: %u present, %u max, %u online, online mask: %016RX64\n",
            (unsigned)RTMpGetPresentCount(), (unsigned)RTMpGetCount(), (unsigned)RTMpGetOnlineCount(),
            RTCpuSetToU64(RTMpGetOnlineSet(&OnlineSet)) ));
    LogRel(("************************* CPUID dump ************************\n"));
    DBGFR3Info(pVM, "cpuid", "verbose", DBGFR3InfoLogRelHlp());
    LogRel(("\n"));
    DBGFR3InfoLog(pVM, "cpuid", "verbose"); /* macro */
    RTLogRelSetBuffering(fOldBuffered);
    LogRel(("******************** End of CPUID dump **********************\n"));

#undef PORTABLE_DISABLE_FEATURE_BIT
#undef PORTABLE_CLEAR_BITS_WHEN

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The CPUM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
VMMR3DECL(void) CPUMR3Relocate(PVM pVM)
{
    LogFlow(("CPUMR3Relocate\n"));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        /*
         * Switcher pointers.
         */
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->cpum.s.pHyperCoreRC = MMHyperCCToRC(pVM, pVCpu->cpum.s.pHyperCoreR3);
        Assert(pVCpu->cpum.s.pHyperCoreRC != NIL_RTRCPTR);

    }
}


/**
 * Apply late CPUM property changes based on the fHWVirtEx setting
 *
 * @param   pVM                 Pointer to the VM.
 * @param   fHWVirtExEnabled    HWVirtEx enabled/disabled
 */
VMMR3DECL(void) CPUMR3SetHWVirtEx(PVM pVM, bool fHWVirtExEnabled)
{
    /*
     * Workaround for missing cpuid(0) patches when leaf 4 returns GuestCpuIdDef:
     * If we miss to patch a cpuid(0).eax then Linux tries to determine the number
     * of processors from (cpuid(4).eax >> 26) + 1.
     *
     * Note: this code is obsolete, but let's keep it here for reference.
     *       Purpose is valid when we artificially cap the max std id to less than 4.
     */
    if (!fHWVirtExEnabled)
    {
        Assert(   pVM->cpum.s.aGuestCpuIdStd[4].eax == 0
               || pVM->cpum.s.aGuestCpuIdStd[0].eax < 0x4);
        pVM->cpum.s.aGuestCpuIdStd[4].eax = 0;
    }
}

/**
 * Terminates the CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int) CPUMR3Term(PVM pVM)
{
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU   pVCpu = &pVM->aCpus[i];
        PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

        memset(pVCpu->cpum.s.aMagic, 0, sizeof(pVCpu->cpum.s.aMagic));
        pVCpu->cpum.s.uMagic     = 0;
        pCtx->dr[5]              = 0;
    }
#else
    NOREF(pVM);
#endif
    return VINF_SUCCESS;
}


/**
 * Resets a virtual CPU.
 *
 * Used by CPUMR3Reset and CPU hot plugging.
 *
 * @param   pVCpu               Pointer to the VMCPU.
 */
VMMR3DECL(void) CPUMR3ResetCpu(PVMCPU pVCpu)
{
    /** @todo anything different for VCPU > 0? */
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    /*
     * Initialize everything to ZERO first.
     */
    uint32_t fUseFlags =  pVCpu->cpum.s.fUseFlags & ~CPUM_USED_FPU_SINCE_REM;
    memset(pCtx, 0, sizeof(*pCtx));
    pVCpu->cpum.s.fUseFlags  = fUseFlags;

    pCtx->cr0                       = X86_CR0_CD | X86_CR0_NW | X86_CR0_ET;  //0x60000010
    pCtx->eip                       = 0x0000fff0;
    pCtx->edx                       = 0x00000600;   /* P6 processor */
    pCtx->eflags.Bits.u1Reserved0   = 1;

    pCtx->cs.Sel                    = 0xf000;
    pCtx->cs.ValidSel               = 0xf000;
    pCtx->cs.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->cs.u64Base                = UINT64_C(0xffff0000);
    pCtx->cs.u32Limit               = 0x0000ffff;
    pCtx->cs.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->cs.Attr.n.u1Present       = 1;
    pCtx->cs.Attr.n.u4Type          = X86_SEL_TYPE_READ | X86_SEL_TYPE_CODE;

    pCtx->ds.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->ds.u32Limit               = 0x0000ffff;
    pCtx->ds.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->ds.Attr.n.u1Present       = 1;
    pCtx->ds.Attr.n.u4Type          = X86_SEL_TYPE_RW;

    pCtx->es.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->es.u32Limit               = 0x0000ffff;
    pCtx->es.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->es.Attr.n.u1Present       = 1;
    pCtx->es.Attr.n.u4Type          = X86_SEL_TYPE_RW;

    pCtx->fs.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->fs.u32Limit               = 0x0000ffff;
    pCtx->fs.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->fs.Attr.n.u1Present       = 1;
    pCtx->fs.Attr.n.u4Type          = X86_SEL_TYPE_RW;

    pCtx->gs.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->gs.u32Limit               = 0x0000ffff;
    pCtx->gs.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->gs.Attr.n.u1Present       = 1;
    pCtx->gs.Attr.n.u4Type          = X86_SEL_TYPE_RW;

    pCtx->ss.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->ss.u32Limit               = 0x0000ffff;
    pCtx->ss.Attr.n.u1Present       = 1;
    pCtx->ss.Attr.n.u1DescType      = 1; /* code/data segment */
    pCtx->ss.Attr.n.u4Type          = X86_SEL_TYPE_RW;

    pCtx->idtr.cbIdt                = 0xffff;
    pCtx->gdtr.cbGdt                = 0xffff;

    pCtx->ldtr.fFlags               = CPUMSELREG_FLAGS_VALID;
    pCtx->ldtr.u32Limit             = 0xffff;
    pCtx->ldtr.Attr.n.u1Present     = 1;
    pCtx->ldtr.Attr.n.u4Type        = X86_SEL_TYPE_SYS_LDT;

    pCtx->tr.fFlags                 = CPUMSELREG_FLAGS_VALID;
    pCtx->tr.u32Limit               = 0xffff;
    pCtx->tr.Attr.n.u1Present       = 1;
    pCtx->tr.Attr.n.u4Type          = X86_SEL_TYPE_SYS_386_TSS_BUSY;    /* Deduction, not properly documented by Intel. */

    pCtx->dr[6]                     = X86_DR6_INIT_VAL;
    pCtx->dr[7]                     = X86_DR7_INIT_VAL;

    pCtx->fpu.FTW                   = 0x00;         /* All empty (abbridged tag reg edition). */
    pCtx->fpu.FCW                   = 0x37f;

    /* Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3A, Table 8-1.
       IA-32 Processor States Following Power-up, Reset, or INIT */
    pCtx->fpu.MXCSR                 = 0x1F80;
    pCtx->fpu.MXCSR_MASK            = 0xffff; /** @todo REM always changed this for us. Should probably check if the HW really
                                                        supports all bits, since a zero value here should be read as 0xffbf. */

    /* Init PAT MSR */
    pCtx->msrPAT                    = UINT64_C(0x0007040600070406); /** @todo correct? */

    /* Reset EFER; see AMD64 Architecture Programmer's Manual Volume 2: Table 14-1. Initial Processor State
    * The Intel docs don't mention it.
    */
    pCtx->msrEFER                   = 0;
}


/**
 * Resets the CPU.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(void) CPUMR3Reset(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        CPUMR3ResetCpu(&pVM->aCpus[i]);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(&pVM->aCpus[i]);

        /* Magic marker for searching in crash dumps. */
        strcpy((char *)pVM->aCpus[i].cpum.s.aMagic, "CPUMCPU Magic");
        pVM->aCpus[i].cpum.s.uMagic     = UINT64_C(0xDEADBEEFDEADBEEF);
        pCtx->dr[5]                     = UINT64_C(0xDEADBEEFDEADBEEF);
#endif
    }
}


/**
 * Called both in pass 0 and the final pass.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   pSSM                The saved state handle.
 */
static void cpumR3SaveCpuId(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save all the CPU ID leaves here so we can check them for compatibility
     * upon loading.
     */
    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3PutMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Save a good portion of the raw CPU IDs as well as they may come in
     * handy when validating features for raw mode.
     */
    CPUMCPUID   aRawStd[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuId(i, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawStd));
    SSMR3PutMem(pSSM, &aRawStd[0], sizeof(aRawStd));

    CPUMCPUID   aRawExt[32];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawExt));
    SSMR3PutMem(pSSM, &aRawExt[0], sizeof(aRawExt));
}


/**
 * Loads the CPU ID leaves saved by pass 0.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM.
 * @param   pSSM                The saved state handle.
 * @param   uVersion            The format version.
 */
static int cpumR3LoadCpuId(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion)
{
    AssertMsgReturn(uVersion >= CPUM_SAVED_STATE_VERSION_VER3_2, ("%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    /*
     * Define a bunch of macros for simplifying the code.
     */
    /* Generic expression + failure message. */
#define CPUID_CHECK_RET(expr, fmt) \
    do { \
        if (!(expr)) \
        { \
            char *pszMsg = RTStrAPrintf2 fmt; /* lack of variadic macros sucks */ \
            if (fStrictCpuIdChecks) \
            { \
                int rcCpuid = SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, "%s", pszMsg); \
                RTStrFree(pszMsg); \
                return rcCpuid; \
            } \
            LogRel(("CPUM: %s\n", pszMsg)); \
            RTStrFree(pszMsg); \
        } \
    } while (0)
#define CPUID_CHECK_WRN(expr, fmt) \
    do { \
        if (!(expr)) \
            LogRel(fmt); \
    } while (0)

    /* For comparing two values and bitch if they differs. */
#define CPUID_CHECK2_RET(what, host, saved) \
    do { \
        if ((host) != (saved)) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#what " mismatch: host=%#x saved=%#x"), (host), (saved)); \
            LogRel(("CPUM: " #what " differs: host=%#x saved=%#x\n", (host), (saved))); \
        } \
    } while (0)
#define CPUID_CHECK2_WRN(what, host, saved) \
    do { \
        if ((host) != (saved)) \
            LogRel(("CPUM: " #what " differs: host=%#x saved=%#x\n", (host), (saved))); \
    } while (0)

    /* For checking raw cpu features (raw mode). */
#define CPUID_RAW_FEATURE_RET(set, reg, bit) \
    do { \
        if ((aHostRaw##set [1].reg & bit) != (aRaw##set [1].reg & bit)) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " mismatch: host=%d saved=%d"), \
                                         !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) ); \
            LogRel(("CPUM: " #bit" differs: host=%d saved=%d\n", \
                    !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) )); \
        } \
    } while (0)
#define CPUID_RAW_FEATURE_WRN(set, reg, bit) \
    do { \
        if ((aHostRaw##set [1].reg & bit) != (aRaw##set [1].reg & bit)) \
            LogRel(("CPUM: " #bit" differs: host=%d saved=%d\n", \
                    !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) )); \
    } while (0)
#define CPUID_RAW_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking guest features. */
#define CPUID_GST_FEATURE_RET(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_FEATURE_WRN(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_FEATURE_EMU(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: Warning - " #bit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking guest features if AMD guest CPU. */
#define CPUID_GST_AMD_FEATURE_RET(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_AMD_FEATURE_WRN(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_AMD_FEATURE_EMU(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
            && !(aGuestOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: Warning - " #bit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_AMD_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking AMD features which have a corresponding bit in the standard
       range.  (Intel defines very few bits in the extended feature sets.) */
#define CPUID_GST_FEATURE2_RET(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
            && !(aGuestOverrideExt[1].reg  & (ExtBit)) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#ExtBit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #ExtBit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_FEATURE2_WRN(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
            && !(aGuestOverrideExt[1].reg  & (ExtBit)) \
           ) \
            LogRel(("CPUM: " #ExtBit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_FEATURE2_EMU(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
            && !(aGuestOverrideExt[1].reg  & (ExtBit)) \
           ) \
            LogRel(("CPUM: Warning - " #ExtBit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_FEATURE2_IGN(reg, ExtBit, StdBit) do { } while (0)

    /*
     * Load them into stack buffers first.
     */
    CPUMCPUID   aGuestCpuIdStd[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd)];
    uint32_t    cGuestCpuIdStd;
    int rc = SSMR3GetU32(pSSM, &cGuestCpuIdStd); AssertRCReturn(rc, rc);
    if (cGuestCpuIdStd > RT_ELEMENTS(aGuestCpuIdStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdStd[0], cGuestCpuIdStd * sizeof(aGuestCpuIdStd[0]));

    CPUMCPUID   aGuestCpuIdExt[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt)];
    uint32_t    cGuestCpuIdExt;
    rc = SSMR3GetU32(pSSM, &cGuestCpuIdExt); AssertRCReturn(rc, rc);
    if (cGuestCpuIdExt > RT_ELEMENTS(aGuestCpuIdExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdExt[0], cGuestCpuIdExt * sizeof(aGuestCpuIdExt[0]));

    CPUMCPUID   aGuestCpuIdCentaur[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur)];
    uint32_t    cGuestCpuIdCentaur;
    rc = SSMR3GetU32(pSSM, &cGuestCpuIdCentaur); AssertRCReturn(rc, rc);
    if (cGuestCpuIdCentaur > RT_ELEMENTS(aGuestCpuIdCentaur))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdCentaur[0], cGuestCpuIdCentaur * sizeof(aGuestCpuIdCentaur[0]));

    CPUMCPUID   GuestCpuIdDef;
    rc = SSMR3GetMem(pSSM, &GuestCpuIdDef, sizeof(GuestCpuIdDef));
    AssertRCReturn(rc, rc);

    CPUMCPUID   aRawStd[16];
    uint32_t    cRawStd;
    rc = SSMR3GetU32(pSSM, &cRawStd); AssertRCReturn(rc, rc);
    if (cRawStd > RT_ELEMENTS(aRawStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aRawStd[0], cRawStd * sizeof(aRawStd[0]));

    CPUMCPUID   aRawExt[32];
    uint32_t    cRawExt;
    rc = SSMR3GetU32(pSSM, &cRawExt); AssertRCReturn(rc, rc);
    if (cRawExt > RT_ELEMENTS(aRawExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    rc = SSMR3GetMem(pSSM, &aRawExt[0], cRawExt * sizeof(aRawExt[0]));
    AssertRCReturn(rc, rc);

    /*
     * Note that we support restoring less than the current amount of standard
     * leaves because we've been allowed more is newer version of VBox.
     *
     * So, pad new entries with the default.
     */
    for (uint32_t i = cGuestCpuIdStd; i < RT_ELEMENTS(aGuestCpuIdStd); i++)
        aGuestCpuIdStd[i] = GuestCpuIdDef;

    for (uint32_t i = cGuestCpuIdExt; i < RT_ELEMENTS(aGuestCpuIdExt); i++)
        aGuestCpuIdExt[i] = GuestCpuIdDef;

    for (uint32_t i = cGuestCpuIdCentaur; i < RT_ELEMENTS(aGuestCpuIdCentaur); i++)
        aGuestCpuIdCentaur[i] = GuestCpuIdDef;

    for (uint32_t i = cRawStd; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuId(i, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);

    for (uint32_t i = cRawExt; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);

    /*
     * Get the raw CPU IDs for the current host.
     */
    CPUMCPUID   aHostRawStd[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawStd); i++)
        ASMCpuId(i, &aHostRawStd[i].eax, &aHostRawStd[i].ebx, &aHostRawStd[i].ecx, &aHostRawStd[i].edx);

    CPUMCPUID   aHostRawExt[32];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aHostRawExt[i].eax, &aHostRawExt[i].ebx, &aHostRawExt[i].ecx, &aHostRawExt[i].edx);

    /*
     * Get the host and guest overrides so we don't reject the state because
     * some feature was enabled thru these interfaces.
     * Note! We currently only need the feature leaves, so skip rest.
     */
    PCFGMNODE   pOverrideCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM/CPUID");
    CPUMCPUID   aGuestOverrideStd[2];
    memcpy(&aGuestOverrideStd[0], &aHostRawStd[0], sizeof(aGuestOverrideStd));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x00000000), &aGuestOverrideStd[0], RT_ELEMENTS(aGuestOverrideStd), pOverrideCfg);

    CPUMCPUID   aGuestOverrideExt[2];
    memcpy(&aGuestOverrideExt[0], &aHostRawExt[0], sizeof(aGuestOverrideExt));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x80000000), &aGuestOverrideExt[0], RT_ELEMENTS(aGuestOverrideExt), pOverrideCfg);

    pOverrideCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM/HostCPUID");
    CPUMCPUID   aHostOverrideStd[2];
    memcpy(&aHostOverrideStd[0], &aHostRawStd[0], sizeof(aHostOverrideStd));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x00000000), &aHostOverrideStd[0], RT_ELEMENTS(aHostOverrideStd), pOverrideCfg);

    CPUMCPUID   aHostOverrideExt[2];
    memcpy(&aHostOverrideExt[0], &aHostRawExt[0], sizeof(aHostOverrideExt));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x80000000), &aHostOverrideExt[0], RT_ELEMENTS(aHostOverrideExt), pOverrideCfg);

    /*
     * This can be skipped.
     */
    bool fStrictCpuIdChecks;
    CFGMR3QueryBoolDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM"), "StrictCpuIdChecks", &fStrictCpuIdChecks, true);



    /*
     * For raw-mode we'll require that the CPUs are very similar since we don't
     * intercept CPUID instructions for user mode applications.
     */
    if (!HWACCMIsEnabled(pVM))
    {
        /* CPUID(0) */
        CPUID_CHECK_RET(   aHostRawStd[0].ebx == aRawStd[0].ebx
                        && aHostRawStd[0].ecx == aRawStd[0].ecx
                        && aHostRawStd[0].edx == aRawStd[0].edx,
                        (N_("CPU vendor mismatch: host='%.4s%.4s%.4s' saved='%.4s%.4s%.4s'"),
                         &aHostRawStd[0].ebx, &aHostRawStd[0].edx, &aHostRawStd[0].ecx,
                         &aRawStd[0].ebx, &aRawStd[0].edx, &aRawStd[0].ecx));
        CPUID_CHECK2_WRN("Std CPUID max leaf",   aHostRawStd[0].eax, aRawStd[0].eax);
        CPUID_CHECK2_WRN("Reserved bits 15:14", (aHostRawExt[1].eax >> 14) & 3, (aRawExt[1].eax >> 14) & 3);
        CPUID_CHECK2_WRN("Reserved bits 31:28",  aHostRawExt[1].eax >> 28,       aRawExt[1].eax >> 28);

        bool const fIntel = ASMIsIntelCpuEx(aRawStd[0].ebx, aRawStd[0].ecx, aRawStd[0].edx);

        /* CPUID(1).eax */
        CPUID_CHECK2_RET("CPU family",          ASMGetCpuFamily(aHostRawStd[1].eax),        ASMGetCpuFamily(aRawStd[1].eax));
        CPUID_CHECK2_RET("CPU model",           ASMGetCpuModel(aHostRawStd[1].eax, fIntel), ASMGetCpuModel(aRawStd[1].eax, fIntel));
        CPUID_CHECK2_WRN("CPU type",            (aHostRawStd[1].eax >> 12) & 3,             (aRawStd[1].eax >> 12) & 3 );

        /* CPUID(1).ebx - completely ignore CPU count and APIC ID. */
        CPUID_CHECK2_RET("CPU brand ID",         aHostRawStd[1].ebx & 0xff,                 aRawStd[1].ebx & 0xff);
        CPUID_CHECK2_WRN("CLFLUSH chunk count", (aHostRawStd[1].ebx >> 8) & 0xff,           (aRawStd[1].ebx >> 8) & 0xff);

        /* CPUID(1).ecx */
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE3);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PCLMUL);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_DTES64);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MONITOR);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CPLDS);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_VMX);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_SMX);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_EST);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_TM2);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSSE3);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_CNTXID);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(11) /*reserved*/ );
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_FMA);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CX16);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_TPRUPDATE);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_PDCM);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(16) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(17) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_DCA);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_1);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_2);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_X2APIC);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MOVBE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_POPCNT);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(24) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AES);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_XSAVE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_OSXSAVE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AVX);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(29) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(30) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_HVP);

        /* CPUID(1).edx */
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FPU);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_VME);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TSC);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MSR);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CX8);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_APIC);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(10) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SEP);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MTRR);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PGE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCA);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CMOV);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAT);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE36);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSN);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CLFSH);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(20) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_DS);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_ACPI);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MMX);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FXSR);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE2);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SS);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_HTT);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_TM);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(30) /*JMPE/IA64*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PBE);

        /* CPUID(2) - config, mostly about caches. ignore. */
        /* CPUID(3) - processor serial number. ignore. */
        /* CPUID(4) - config, cache and topology - takes ECX as input. ignore. */
        /* CPUID(5) - mwait/monitor config. ignore. */
        /* CPUID(6) - power management. ignore. */
        /* CPUID(7) - ???. ignore. */
        /* CPUID(8) - ???. ignore. */
        /* CPUID(9) - DCA. ignore for now. */
        /* CPUID(a) - PeMo info. ignore for now. */
        /* CPUID(b) - topology info - takes ECX as input. ignore. */

        /* CPUID(d) - XCR0 stuff - takes ECX as input. We only warn about the main level (ECX=0) for now. */
        CPUID_CHECK_WRN(   aRawStd[0].eax     <  UINT32_C(0x0000000d)
                        || aHostRawStd[0].eax >= UINT32_C(0x0000000d),
                        ("CPUM: Standard leaf D was present on saved state host, not present on current.\n"));
        if (   aRawStd[0].eax     >= UINT32_C(0x0000000d)
            && aHostRawStd[0].eax >= UINT32_C(0x0000000d))
        {
            CPUID_CHECK2_WRN("Valid low XCR0 bits",             aHostRawStd[0xd].eax, aRawStd[0xd].eax);
            CPUID_CHECK2_WRN("Valid high XCR0 bits",            aHostRawStd[0xd].edx, aRawStd[0xd].edx);
            CPUID_CHECK2_WRN("Current XSAVE/XRSTOR area size",  aHostRawStd[0xd].ebx, aRawStd[0xd].ebx);
            CPUID_CHECK2_WRN("Max XSAVE/XRSTOR area size",      aHostRawStd[0xd].ecx, aRawStd[0xd].ecx);
        }

        /* CPUID(0x80000000) - same as CPUID(0) except for eax.
           Note! Intel have/is marking many of the fields here as reserved. We
                 will verify them as if it's an AMD CPU. */
        CPUID_CHECK_RET(   (aHostRawExt[0].eax >= UINT32_C(0x80000001) && aHostRawExt[0].eax <= UINT32_C(0x8000007f))
                        || !(aRawExt[0].eax    >= UINT32_C(0x80000001) && aRawExt[0].eax     <= UINT32_C(0x8000007f)),
                        (N_("Extended leaves was present on saved state host, but is missing on the current\n")));
        if (aRawExt[0].eax >= UINT32_C(0x80000001) && aRawExt[0].eax     <= UINT32_C(0x8000007f))
        {
            CPUID_CHECK_RET(   aHostRawExt[0].ebx == aRawExt[0].ebx
                            && aHostRawExt[0].ecx == aRawExt[0].ecx
                            && aHostRawExt[0].edx == aRawExt[0].edx,
                            (N_("CPU vendor mismatch: host='%.4s%.4s%.4s' saved='%.4s%.4s%.4s'"),
                             &aHostRawExt[0].ebx, &aHostRawExt[0].edx, &aHostRawExt[0].ecx,
                             &aRawExt[0].ebx,     &aRawExt[0].edx,     &aRawExt[0].ecx));
            CPUID_CHECK2_WRN("Ext CPUID max leaf",   aHostRawExt[0].eax, aRawExt[0].eax);

            /* CPUID(0x80000001).eax - same as CPUID(0).eax. */
            CPUID_CHECK2_RET("CPU family",          ASMGetCpuFamily(aHostRawExt[1].eax),        ASMGetCpuFamily(aRawExt[1].eax));
            CPUID_CHECK2_RET("CPU model",           ASMGetCpuModel(aHostRawExt[1].eax, fIntel), ASMGetCpuModel(aRawExt[1].eax, fIntel));
            CPUID_CHECK2_WRN("CPU type",            (aHostRawExt[1].eax >> 12) & 3, (aRawExt[1].eax >> 12) & 3 );
            CPUID_CHECK2_WRN("Reserved bits 15:14", (aHostRawExt[1].eax >> 14) & 3, (aRawExt[1].eax >> 14) & 3 );
            CPUID_CHECK2_WRN("Reserved bits 31:28",  aHostRawExt[1].eax >> 28, aRawExt[1].eax >> 28);

            /* CPUID(0x80000001).ebx - Brand ID (maybe), just warn if things differs. */
            CPUID_CHECK2_WRN("CPU BrandID",          aHostRawExt[1].ebx & 0xffff, aRawExt[1].ebx & 0xffff);
            CPUID_CHECK2_WRN("Reserved bits 16:27", (aHostRawExt[1].ebx >> 16) & 0xfff, (aRawExt[1].ebx >> 16) & 0xfff);
            CPUID_CHECK2_WRN("PkgType",             (aHostRawExt[1].ebx >> 28) &   0xf, (aRawExt[1].ebx >> 28) &   0xf);

            /* CPUID(0x80000001).ecx */
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CMPL);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SVM);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_EXT_APIC);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CR8L);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_ABM);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE4A);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_OSVW);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_IBS);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE5);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SKINIT);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_WDT);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(14));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(15));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(16));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(17));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(18));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(19));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(20));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(21));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(22));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(23));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(24));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(25));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(26));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(27));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(28));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(29));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(30));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(31));

            /* CPUID(0x80000001).edx */
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FPU);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_VME);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_DE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PSE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_TSC);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MSR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAE);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MCE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_CX8);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_APIC);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(10) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_SEP);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MTRR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PGE);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MCA);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_CMOV);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAT);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PSE36);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(18) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(19) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_NX);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(21) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_AXMMX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MMX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FXSR);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FFXSR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAGE1GB);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_RDTSCP);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(28) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_LONG_MODE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW);

            /** @todo verify the rest as well. */
        }
    }



    /*
     * Verify that we can support the features already exposed to the guest on
     * this host.
     *
     * Most of the features we're emulating requires intercepting instruction
     * and doing it the slow way, so there is no need to warn when they aren't
     * present in the host CPU.  Thus we use IGN instead of EMU on these.
     *
     * Trailing comments:
     *      "EMU"  - Possible to emulate, could be lots of work and very slow.
     *      "EMU?" - Can this be emulated?
     */
    /* CPUID(1).ecx */
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE3);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PCLMUL);  // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_DTES64);  // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_MONITOR);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CPLDS);   // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_VMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_EST);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_TM2);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSSE3);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CNTXID);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(11) /*reserved*/ );
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_FMA);     // -> EMU? what's this?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CX16);    // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_TPRUPDATE);//-> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PDCM);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(16) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(17) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_DCA);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_1);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_2);  // -> EMU
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_X2APIC);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MOVBE);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_POPCNT);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(24) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AES);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_XSAVE);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_OSXSAVE); // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AVX);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(29) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(30) /*reserved*/);
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_HVP);     // Normally not set by host

    /* CPUID(1).edx */
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FPU);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_VME);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DE);      // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TSC);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MSR);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_PAE);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCE);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CX8);     // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_APIC);
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(10) /*reserved*/);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SEP);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MTRR);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PGE);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCA);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CMOV);    // -> EMU
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAT);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE36);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSN);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CLFSH);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(20) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DS);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_ACPI);    // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FXSR);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE2);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SS);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_HTT);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TM);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(30) /*JMPE/IA64*/);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_PBE);     // -> EMU?

    /* CPUID(0x80000000). */
    if (    aGuestCpuIdExt[0].eax >= UINT32_C(0x80000001)
        &&  aGuestCpuIdExt[0].eax <  UINT32_C(0x8000007f))
    {
        /** @todo deal with no 0x80000001 on the host. */
        bool const fHostAmd  = ASMIsAmdCpuEx(aHostRawStd[0].ebx, aHostRawStd[0].ecx, aHostRawStd[0].edx);
        bool const fGuestAmd = ASMIsAmdCpuEx(aGuestCpuIdExt[0].ebx, aGuestCpuIdExt[0].ecx, aGuestCpuIdExt[0].edx);

        /* CPUID(0x80000001).ecx */
        CPUID_GST_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF);   // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CMPL);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SVM);     // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_EXT_APIC);// ???
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CR8L);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_ABM);     // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE4A);   // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);//-> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF);// -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_OSVW);    // -> EMU?
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_IBS);     // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE5);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SKINIT);  // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_WDT);     // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(14));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(15));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(16));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(17));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(18));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(19));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(20));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(21));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(22));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(23));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(24));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(25));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(26));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(27));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(28));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(29));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(30));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(31));

        /* CPUID(0x80000001).edx */
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_FPU,   X86_CPUID_FEATURE_EDX_FPU);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_VME,   X86_CPUID_FEATURE_EDX_VME);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_DE,    X86_CPUID_FEATURE_EDX_DE);      // -> EMU
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PSE,   X86_CPUID_FEATURE_EDX_PSE);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_TSC,   X86_CPUID_FEATURE_EDX_TSC);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_MSR,   X86_CPUID_FEATURE_EDX_MSR);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_PAE,   X86_CPUID_FEATURE_EDX_PAE);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MCE,   X86_CPUID_FEATURE_EDX_MCE);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_CX8,   X86_CPUID_FEATURE_EDX_CX8);     // -> EMU?
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_APIC,  X86_CPUID_FEATURE_EDX_APIC);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(10) /*reserved*/);
        CPUID_GST_FEATURE_IGN(    Ext, edx, X86_CPUID_AMD_FEATURE_EDX_SEP);                                  // Intel: long mode only.
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MTRR,  X86_CPUID_FEATURE_EDX_MTRR);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PGE,   X86_CPUID_FEATURE_EDX_PGE);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MCA,   X86_CPUID_FEATURE_EDX_MCA);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_CMOV,  X86_CPUID_FEATURE_EDX_CMOV);    // -> EMU
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PAT,   X86_CPUID_FEATURE_EDX_PAT);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PSE36, X86_CPUID_FEATURE_EDX_PSE36);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(18) /*reserved*/);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(19) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_AMD_FEATURE_EDX_NX);
        CPUID_GST_FEATURE_WRN(    Ext, edx, RT_BIT_32(21) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_AMD_FEATURE_EDX_AXMMX);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_MMX,   X86_CPUID_FEATURE_EDX_MMX);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_FXSR,  X86_CPUID_FEATURE_EDX_FXSR);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FFXSR);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAGE1GB);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_RDTSCP);
        CPUID_GST_FEATURE_IGN(    Ext, edx, RT_BIT_32(28) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_AMD_FEATURE_EDX_LONG_MODE);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW);
    }

    /*
     * We're good, commit the CPU ID leaves.
     */
    memcpy(&pVM->cpum.s.aGuestCpuIdStd[0],     &aGuestCpuIdStd[0],     sizeof(aGuestCpuIdStd));
    memcpy(&pVM->cpum.s.aGuestCpuIdExt[0],     &aGuestCpuIdExt[0],     sizeof(aGuestCpuIdExt));
    memcpy(&pVM->cpum.s.aGuestCpuIdCentaur[0], &aGuestCpuIdCentaur[0], sizeof(aGuestCpuIdCentaur));
    pVM->cpum.s.GuestCpuIdDef = GuestCpuIdDef;

#undef CPUID_CHECK_RET
#undef CPUID_CHECK_WRN
#undef CPUID_CHECK2_RET
#undef CPUID_CHECK2_WRN
#undef CPUID_RAW_FEATURE_RET
#undef CPUID_RAW_FEATURE_WRN
#undef CPUID_RAW_FEATURE_IGN
#undef CPUID_GST_FEATURE_RET
#undef CPUID_GST_FEATURE_WRN
#undef CPUID_GST_FEATURE_EMU
#undef CPUID_GST_FEATURE_IGN
#undef CPUID_GST_FEATURE2_RET
#undef CPUID_GST_FEATURE2_WRN
#undef CPUID_GST_FEATURE2_EMU
#undef CPUID_GST_FEATURE2_IGN
#undef CPUID_GST_AMD_FEATURE_RET
#undef CPUID_GST_AMD_FEATURE_WRN
#undef CPUID_GST_AMD_FEATURE_EMU
#undef CPUID_GST_AMD_FEATURE_IGN

    return VINF_SUCCESS;
}


/**
 * Pass 0 live exec callback.
 *
 * @returns VINF_SSM_DONT_CALL_AGAIN.
 * @param   pVM                 Pointer to the VM.
 * @param   pSSM                The saved state handle.
 * @param   uPass               The pass (0).
 */
static DECLCALLBACK(int) cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    AssertReturn(uPass == 0, VERR_SSM_UNEXPECTED_PASS);
    cpumR3SaveCpuId(pVM, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
#ifdef CPUM_WITH_CHANGED_CPUMCTX
        SSMR3PutStructEx(pSSM, &pVCpu->cpum.s.Hyper, sizeof(pVCpu->cpum.s.Hyper), SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED,
                         g_aCpumCtxFields, NULL);
#else
        SSMR3PutStructEx(pSSM, &pVCpu->cpum.s.Hyper, sizeof(pVCpu->cpum.s.Hyper), SSMSTRUCT_FLAGS_MEM_BAND_AID,
                         g_aCpumCtxFields, NULL);
#endif
    }

    SSMR3PutU32(pSSM, pVM->cCpus);
    SSMR3PutU32(pSSM, sizeof(pVM->aCpus[0].cpum.s.GuestMsrs.msr));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

#ifdef CPUM_WITH_CHANGED_CPUMCTX
        SSMR3PutStructEx(pSSM, &pVCpu->cpum.s.Guest, sizeof(pVCpu->cpum.s.Guest), SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED,
                         g_aCpumCtxFields, NULL);
#else
        SSMR3PutStructEx(pSSM, &pVCpu->cpum.s.Guest, sizeof(pVCpu->cpum.s.Guest), SSMSTRUCT_FLAGS_MEM_BAND_AID,
                         g_aCpumCtxFields, NULL);
#endif
        SSMR3PutU32(pSSM, pVCpu->cpum.s.fUseFlags);
        SSMR3PutU32(pSSM, pVCpu->cpum.s.fChanged);
        AssertCompileSizeAlignment(pVM->aCpus[i].cpum.s.GuestMsrs.msr, sizeof(uint64_t));
        SSMR3PutMem(pSSM, &pVCpu->cpum.s.GuestMsrs, sizeof(pVM->aCpus[i].cpum.s.GuestMsrs.msr));
    }

    cpumR3SaveCpuId(pVM, pSSM);
    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMINTLOADPREP
 */
static DECLCALLBACK(int) cpumR3LoadPrep(PVM pVM, PSSMHANDLE pSSM)
{
    NOREF(pSSM);
    pVM->cpum.s.fPendingRestore = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc FNSSMINTLOADEXEC
 */
static DECLCALLBACK(int) cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /*
     * Validate version.
     */
    if (    uVersion != CPUM_SAVED_STATE_VERSION
        &&  uVersion != CPUM_SAVED_STATE_VERSION_MEM
        &&  uVersion != CPUM_SAVED_STATE_VERSION_NO_MSR_SIZE
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER3_2
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER3_0
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER2_0
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER1_6)
    {
        AssertMsgFailed(("cpumR3LoadExec: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (uPass == SSM_PASS_FINAL)
    {
        /*
         * Set the size of RTGCPTR for SSMR3GetGCPtr. (Only necessary for
         * really old SSM file versions.)
         */
        if (uVersion == CPUM_SAVED_STATE_VERSION_VER1_6)
            SSMR3HandleSetGCPtrSize(pSSM, sizeof(RTGCPTR32));
        else if (uVersion <= CPUM_SAVED_STATE_VERSION_VER3_0)
            SSMR3HandleSetGCPtrSize(pSSM, HC_ARCH_BITS == 32 ? sizeof(RTGCPTR32) : sizeof(RTGCPTR));

        PCSSMFIELD  paCpumCtxFields = g_aCpumCtxFields;
        if (uVersion == CPUM_SAVED_STATE_VERSION_VER1_6)
            paCpumCtxFields = g_aCpumCtxFieldsV16;
        uint32_t    fLoad   = 0;
        if (uVersion <= CPUM_SAVED_STATE_VERSION_MEM)
#ifdef CPUM_WITH_CHANGED_CPUMCTX
            fLoad = SSMSTRUCT_FLAGS_MEM_BAND_AID_RELAXED;
#else
            fLoad = SSMSTRUCT_FLAGS_MEM_BAND_AID;
#endif

        /*
         * Restore.
         */
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU   pVCpu = &pVM->aCpus[i];
            uint64_t uCR3  = pVCpu->cpum.s.Hyper.cr3;
            uint64_t uRSP  = pVCpu->cpum.s.Hyper.rsp; /* see VMMR3Relocate(). */
            SSMR3GetStructEx(pSSM, &pVCpu->cpum.s.Hyper, sizeof(pVCpu->cpum.s.Hyper), fLoad, paCpumCtxFields, NULL);
            pVCpu->cpum.s.Hyper.cr3 = uCR3;
            pVCpu->cpum.s.Hyper.rsp = uRSP;
        }

        if (uVersion >= CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR)
        {
            uint32_t cCpus;
            int rc = SSMR3GetU32(pSSM, &cCpus); AssertRCReturn(rc, rc);
            AssertLogRelMsgReturn(cCpus == pVM->cCpus, ("Mismatching CPU counts: saved: %u; configured: %u \n", cCpus, pVM->cCpus),
                                  VERR_SSM_UNEXPECTED_DATA);
        }
        AssertLogRelMsgReturn(   uVersion > CPUM_SAVED_STATE_VERSION_VER2_0
                              || pVM->cCpus == 1,
                              ("cCpus=%u\n", pVM->cCpus),
                              VERR_SSM_UNEXPECTED_DATA);

        uint32_t cbMsrs = 0;
        if (uVersion > CPUM_SAVED_STATE_VERSION_NO_MSR_SIZE)
        {
            int rc = SSMR3GetU32(pSSM, &cbMsrs); AssertRCReturn(rc, rc);
            AssertLogRelMsgReturn(RT_ALIGN(cbMsrs, sizeof(uint64_t)) == cbMsrs, ("Size of MSRs is misaligned: %#x\n", cbMsrs),
                                  VERR_SSM_UNEXPECTED_DATA);
            AssertLogRelMsgReturn(cbMsrs <= sizeof(CPUMCTXMSRS) && cbMsrs > 0,  ("Size of MSRs is out of range: %#x\n", cbMsrs),
                                  VERR_SSM_UNEXPECTED_DATA);
        }

        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            SSMR3GetStructEx(pSSM, &pVM->aCpus[i].cpum.s.Guest, sizeof(pVM->aCpus[i].cpum.s.Guest), fLoad,
                             paCpumCtxFields, NULL);
            SSMR3GetU32(pSSM, &pVM->aCpus[i].cpum.s.fUseFlags);
            SSMR3GetU32(pSSM, &pVM->aCpus[i].cpum.s.fChanged);
            if (uVersion > CPUM_SAVED_STATE_VERSION_NO_MSR_SIZE)
                SSMR3GetMem(pSSM, &pVM->aCpus[i].cpum.s.GuestMsrs.au64[0], cbMsrs);
            else if (uVersion >= CPUM_SAVED_STATE_VERSION_VER3_0)
            {
                SSMR3GetMem(pSSM, &pVM->aCpus[i].cpum.s.GuestMsrs.au64[0], 2 * sizeof(uint64_t)); /* Restore two MSRs. */
                SSMR3Skip(pSSM, 62 * sizeof(uint64_t));
            }
        }

        /* Older states does not set CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID for
           raw-mode guest, so we have to do it ourselves. */
        if (   uVersion <= CPUM_SAVED_STATE_VERSION_VER3_2
            && !HWACCMIsEnabled(pVM))
            for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
                pVM->aCpus[iCpu].cpum.s.fChanged |= CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID;
    }

    pVM->cpum.s.fPendingRestore = false;

    /*
     * Guest CPUIDs.
     */
    if (uVersion > CPUM_SAVED_STATE_VERSION_VER3_0)
        return cpumR3LoadCpuId(pVM, pSSM, uVersion);

    /** @todo Merge the code below into cpumR3LoadCpuId when we've found out what is
     *        actually required. */

    /*
     * Restore the CPUID leaves.
     *
     * Note that we support restoring less than the current amount of standard
     * leaves because we've been allowed more is newer version of VBox.
     */
    uint32_t cElements;
    int rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements > RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], cElements*sizeof(pVM->cpum.s.aGuestCpuIdStd[0]));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3GetMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Check that the basic cpuid id information is unchanged.
     */
    /** @todo we should check the 64 bits capabilities too! */
    uint32_t au32CpuId[8] = {0,0,0,0, 0,0,0,0};
    ASMCpuId(0, &au32CpuId[0], &au32CpuId[1], &au32CpuId[2], &au32CpuId[3]);
    ASMCpuId(1, &au32CpuId[4], &au32CpuId[5], &au32CpuId[6], &au32CpuId[7]);
    uint32_t au32CpuIdSaved[8];
    rc = SSMR3GetMem(pSSM, &au32CpuIdSaved[0], sizeof(au32CpuIdSaved));
    if (RT_SUCCESS(rc))
    {
        /* Ignore CPU stepping. */
        au32CpuId[4]      &=  0xfffffff0;
        au32CpuIdSaved[4] &=  0xfffffff0;

        /* Ignore APIC ID (AMD specs). */
        au32CpuId[5]      &= ~0xff000000;
        au32CpuIdSaved[5] &= ~0xff000000;

        /* Ignore the number of Logical CPUs (AMD specs). */
        au32CpuId[5]      &= ~0x00ff0000;
        au32CpuIdSaved[5] &= ~0x00ff0000;

        /* Ignore some advanced capability bits, that we don't expose to the guest. */
        au32CpuId[6]      &= ~(   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              );
        au32CpuIdSaved[6] &= ~(   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              );

        /* Make sure we don't forget to update the masks when enabling
         * features in the future.
         */
        AssertRelease(!(pVM->cpum.s.aGuestCpuIdStd[1].ecx &
                              (   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              )));
        /* do the compare */
        if (memcmp(au32CpuIdSaved, au32CpuId, sizeof(au32CpuIdSaved)))
        {
            if (SSMR3HandleGetAfter(pSSM) == SSMAFTER_DEBUG_IT)
                LogRel(("cpumR3LoadExec: CpuId mismatch! (ignored due to SSMAFTER_DEBUG_IT)\n"
                        "Saved=%.*Rhxs\n"
                        "Real =%.*Rhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
            else
            {
                LogRel(("cpumR3LoadExec: CpuId mismatch!\n"
                        "Saved=%.*Rhxs\n"
                        "Real =%.*Rhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
                rc = VERR_SSM_LOAD_CPUID_MISMATCH;
            }
        }
    }

    return rc;
}


/**
 * @copydoc FNSSMINTLOADPREP
 */
static DECLCALLBACK(int) cpumR3LoadDone(PVM pVM, PSSMHANDLE pSSM)
{
    if (RT_FAILURE(SSMR3HandleGetStatus(pSSM)))
        return VINF_SUCCESS;

    /* just check this since we can. */ /** @todo Add a SSM unit flag for indicating that it's mandatory during a restore.  */
    if (pVM->cpum.s.fPendingRestore)
    {
        LogRel(("CPUM: Missing state!\n"));
        return VERR_INTERNAL_ERROR_2;
    }

    /* Notify PGM of the NXE states in case they've changed. */
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
        PGMNotifyNxeChanged(&pVM->aCpus[iCpu], !!(pVM->aCpus[iCpu].cpum.s.Guest.msrEFER & MSR_K6_EFER_NXE));
    return VINF_SUCCESS;
}


/**
 * Checks if the CPUM state restore is still pending.
 *
 * @returns true / false.
 * @param   pVM                 Pointer to the VM.
 */
VMMDECL(bool) CPUMR3IsStateRestorePending(PVM pVM)
{
    return pVM->cpum.s.fPendingRestore;
}


/**
 * Formats the EFLAGS value into mnemonics.
 *
 * @param   pszEFlags   Where to write the mnemonics. (Assumes sufficient buffer space.)
 * @param   efl         The EFLAGS value.
 */
static void cpumR3InfoFormatFlags(char *pszEFlags, uint32_t efl)
{
    /*
     * Format the flags.
     */
    static const struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    }   s_aFlags[] =
    {
        { "vip",NULL, X86_EFL_VIP },
        { "vif",NULL, X86_EFL_VIF },
        { "ac", NULL, X86_EFL_AC },
        { "vm", NULL, X86_EFL_VM },
        { "rf", NULL, X86_EFL_RF },
        { "nt", NULL, X86_EFL_NT },
        { "ov", "nv", X86_EFL_OF },
        { "dn", "up", X86_EFL_DF },
        { "ei", "di", X86_EFL_IF },
        { "tf", NULL, X86_EFL_TF },
        { "nt", "pl", X86_EFL_SF },
        { "nz", "zr", X86_EFL_ZF },
        { "ac", "na", X86_EFL_AF },
        { "po", "pe", X86_EFL_PF },
        { "cy", "nc", X86_EFL_CF },
    };
    char *psz = pszEFlags;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFlags); i++)
    {
        const char *pszAdd = s_aFlags[i].fFlag & efl ? s_aFlags[i].pszSet : s_aFlags[i].pszClear;
        if (pszAdd)
        {
            strcpy(psz, pszAdd);
            psz += strlen(pszAdd);
            *psz++ = ' ';
        }
    }
    psz[-1] = '\0';
}


/**
 * Formats a full register dump.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        The context to format.
 * @param   pCtxCore    The context core to format.
 * @param   pHlp        Output functions.
 * @param   enmType     The dump type.
 * @param   pszPrefix   Register name prefix.
 */
static void cpumR3InfoOne(PVM pVM, PCPUMCTX pCtx, PCCPUMCTXCORE pCtxCore, PCDBGFINFOHLP pHlp, CPUMDUMPTYPE enmType,
                          const char *pszPrefix)
{
    NOREF(pVM);

    /*
     * Format the EFLAGS.
     */
    uint32_t efl = pCtxCore->eflags.u32;
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
    switch (enmType)
    {
        case CPUMDUMPTYPE_TERSE:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x                %seflags=%08x\n",
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pszPrefix, pCtxCore->ss.Sel, pszPrefix, pCtxCore->ds.Sel, pszPrefix, pCtxCore->es.Sel,
                    pszPrefix, pCtxCore->fs.Sel, pszPrefix, pCtxCore->gs.Sel, pszPrefix, efl);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x                %seflags=%08x\n",
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pszPrefix, pCtxCore->ss.Sel, pszPrefix, pCtxCore->ds.Sel, pszPrefix, pCtxCore->es.Sel,
                    pszPrefix, pCtxCore->fs.Sel, pszPrefix, pCtxCore->gs.Sel, pszPrefix, efl);
            break;

        case CPUMDUMPTYPE_DEFAULT:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %str=%04x      %seflags=%08x\n"
                    "%scr0=%08RX64 %scr2=%08RX64 %scr3=%08RX64 %scr4=%08RX64 %sgdtr=%016RX64:%04x %sldtr=%04x\n"
                    ,
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pszPrefix, pCtxCore->ss.Sel, pszPrefix, pCtxCore->ds.Sel, pszPrefix, pCtxCore->es.Sel,
                    pszPrefix, pCtxCore->fs.Sel, pszPrefix, pCtxCore->gs.Sel, pszPrefix, pCtx->tr.Sel, pszPrefix, efl,
                    pszPrefix, pCtx->cr0, pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3, pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->ldtr.Sel);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %str=%04x      %seflags=%08x\n"
                    "%scr0=%08RX64 %scr2=%08RX64 %scr3=%08RX64 %scr4=%08RX64 %sgdtr=%08RX64:%04x %sldtr=%04x\n"
                    ,
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pszPrefix, pCtxCore->ss.Sel, pszPrefix, pCtxCore->ds.Sel, pszPrefix, pCtxCore->es.Sel,
                    pszPrefix, pCtxCore->fs.Sel, pszPrefix, pCtxCore->gs.Sel, pszPrefix, pCtx->tr.Sel, pszPrefix, efl,
                    pszPrefix, pCtx->cr0, pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3, pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->ldtr.Sel);
            break;

        case CPUMDUMPTYPE_VERBOSE:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%scr0=%016RX64 %scr2=%016RX64 %scr3=%016RX64 %scr4=%016RX64\n"
                    "%sdr0=%016RX64 %sdr1=%016RX64 %sdr2=%016RX64 %sdr3=%016RX64\n"
                    "%sdr4=%016RX64 %sdr5=%016RX64 %sdr6=%016RX64 %sdr7=%016RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%016RX64 esp=%016RX64}\n"
                    ,
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pCtx->cs.u64Base, pCtx->cs.u32Limit, pCtx->cs.Attr.u,
                    pszPrefix, pCtxCore->ds.Sel, pCtx->ds.u64Base, pCtx->ds.u32Limit, pCtx->ds.Attr.u,
                    pszPrefix, pCtxCore->es.Sel, pCtx->es.u64Base, pCtx->es.u32Limit, pCtx->es.Attr.u,
                    pszPrefix, pCtxCore->fs.Sel, pCtx->fs.u64Base, pCtx->fs.u32Limit, pCtx->fs.Attr.u,
                    pszPrefix, pCtxCore->gs.Sel, pCtx->gs.u64Base, pCtx->gs.u32Limit, pCtx->gs.Attr.u,
                    pszPrefix, pCtxCore->ss.Sel, pCtx->ss.u64Base, pCtx->ss.u32Limit, pCtx->ss.Attr.u,
                    pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->dr[0],  pszPrefix, pCtx->dr[1], pszPrefix, pCtx->dr[2],  pszPrefix, pCtx->dr[3],
                    pszPrefix, pCtx->dr[4],  pszPrefix, pCtx->dr[5], pszPrefix, pCtx->dr[6],  pszPrefix, pCtx->dr[7],
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                    pszPrefix, pCtx->ldtr.Sel, pCtx->ldtr.u64Base, pCtx->ldtr.u32Limit, pCtx->ldtr.Attr.u,
                    pszPrefix, pCtx->tr.Sel, pCtx->tr.u64Base, pCtx->tr.u32Limit, pCtx->tr.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr0=%08RX64 %sdr1=%08RX64\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x} %sdr2=%08RX64 %sdr3=%08RX64\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x} %sdr4=%08RX64 %sdr5=%08RX64\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr6=%08RX64 %sdr7=%08RX64\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x} %scr0=%08RX64 %scr2=%08RX64\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x} %scr3=%08RX64 %scr4=%08RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                    ,
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, pCtxCore->cs.Sel, pCtx->cs.u64Base, pCtx->cs.u32Limit, pCtx->cs.Attr.u, pszPrefix, pCtx->dr[0],  pszPrefix, pCtx->dr[1],
                    pszPrefix, pCtxCore->ds.Sel, pCtx->ds.u64Base, pCtx->ds.u32Limit, pCtx->ds.Attr.u, pszPrefix, pCtx->dr[2],  pszPrefix, pCtx->dr[3],
                    pszPrefix, pCtxCore->es.Sel, pCtx->es.u64Base, pCtx->es.u32Limit, pCtx->es.Attr.u, pszPrefix, pCtx->dr[4],  pszPrefix, pCtx->dr[5],
                    pszPrefix, pCtxCore->fs.Sel, pCtx->fs.u64Base, pCtx->fs.u32Limit, pCtx->fs.Attr.u, pszPrefix, pCtx->dr[6],  pszPrefix, pCtx->dr[7],
                    pszPrefix, pCtxCore->gs.Sel, pCtx->gs.u64Base, pCtx->gs.u32Limit, pCtx->gs.Attr.u, pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2,
                    pszPrefix, pCtxCore->ss.Sel, pCtx->ss.u64Base, pCtx->ss.u32Limit, pCtx->ss.Attr.u, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                    pszPrefix, pCtx->ldtr.Sel, pCtx->ldtr.u64Base, pCtx->ldtr.u32Limit, pCtx->ldtr.Attr.u,
                    pszPrefix, pCtx->tr.Sel, pCtx->tr.u64Base, pCtx->tr.u32Limit, pCtx->tr.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);

            pHlp->pfnPrintf(pHlp,
                "%sFCW=%04x %sFSW=%04x %sFTW=%04x %sFOP=%04x %sMXCSR=%08x %sMXCSR_MASK=%08x\n"
                "%sFPUIP=%08x %sCS=%04x %sRsrvd1=%04x  %sFPUDP=%08x %sDS=%04x %sRsvrd2=%04x\n"
                ,
                pszPrefix, pCtx->fpu.FCW,   pszPrefix, pCtx->fpu.FSW, pszPrefix, pCtx->fpu.FTW, pszPrefix, pCtx->fpu.FOP,
                pszPrefix, pCtx->fpu.MXCSR, pszPrefix, pCtx->fpu.MXCSR_MASK,
                pszPrefix, pCtx->fpu.FPUIP, pszPrefix, pCtx->fpu.CS,  pszPrefix, pCtx->fpu.Rsrvd1,
                pszPrefix, pCtx->fpu.FPUDP, pszPrefix, pCtx->fpu.DS,  pszPrefix, pCtx->fpu.Rsrvd2
                );
            unsigned iShift = (pCtx->fpu.FSW >> 11) & 7;
            for (unsigned iST = 0; iST < RT_ELEMENTS(pCtx->fpu.aRegs); iST++)
            {
                unsigned iFPR        = (iST + iShift) % RT_ELEMENTS(pCtx->fpu.aRegs);
                unsigned uTag        = pCtx->fpu.FTW & (1 << iFPR) ? 1 : 0;
                char     chSign      = pCtx->fpu.aRegs[0].au16[4] & 0x8000 ? '-' : '+';
                unsigned iInteger    = (unsigned)(pCtx->fpu.aRegs[0].au64[0] >> 63);
                uint64_t u64Fraction = pCtx->fpu.aRegs[0].au64[0] & UINT64_C(0x7fffffffffffffff);
                unsigned uExponent   = pCtx->fpu.aRegs[0].au16[4] & 0x7fff;
                /** @todo This isn't entirenly correct and needs more work! */
                pHlp->pfnPrintf(pHlp,
                                "%sST(%u)=%sFPR%u={%04RX16'%08RX32'%08RX32} t%d %c%u.%022llu ^ %u",
                                pszPrefix, iST, pszPrefix, iFPR,
                                pCtx->fpu.aRegs[0].au16[4], pCtx->fpu.aRegs[0].au32[1], pCtx->fpu.aRegs[0].au32[0],
                                uTag, chSign, iInteger, u64Fraction, uExponent);
                if (pCtx->fpu.aRegs[0].au16[5] || pCtx->fpu.aRegs[0].au16[6] || pCtx->fpu.aRegs[0].au16[7])
                    pHlp->pfnPrintf(pHlp, " res={%04RX16,%04RX16,%04RX16}\n",
                                    pCtx->fpu.aRegs[0].au16[5], pCtx->fpu.aRegs[0].au16[6], pCtx->fpu.aRegs[0].au16[7]);
                else
                    pHlp->pfnPrintf(pHlp, "\n");
            }
            for (unsigned iXMM = 0; iXMM < RT_ELEMENTS(pCtx->fpu.aXMM); iXMM++)
                pHlp->pfnPrintf(pHlp,
                                iXMM & 1
                                ? "%sXMM%u%s=%08RX32'%08RX32'%08RX32'%08RX32\n"
                                : "%sXMM%u%s=%08RX32'%08RX32'%08RX32'%08RX32  ",
                                pszPrefix, iXMM, iXMM < 10 ? " " : "",
                                pCtx->fpu.aXMM[iXMM].au32[3],
                                pCtx->fpu.aXMM[iXMM].au32[2],
                                pCtx->fpu.aXMM[iXMM].au32[1],
                                pCtx->fpu.aXMM[iXMM].au32[0]);
            for (unsigned i = 0; i < RT_ELEMENTS(pCtx->fpu.au32RsrvdRest); i++)
                if (pCtx->fpu.au32RsrvdRest[i])
                    pHlp->pfnPrintf(pHlp, "%sRsrvdRest[i]=%RX32 (offset=%#x)\n",
                                    pszPrefix, i, pCtx->fpu.au32RsrvdRest[i], RT_OFFSETOF(X86FXSTATE, au32RsrvdRest[i]) );

            pHlp->pfnPrintf(pHlp,
                "%sEFER         =%016RX64\n"
                "%sPAT          =%016RX64\n"
                "%sSTAR         =%016RX64\n"
                "%sCSTAR        =%016RX64\n"
                "%sLSTAR        =%016RX64\n"
                "%sSFMASK       =%016RX64\n"
                "%sKERNELGSBASE =%016RX64\n",
                pszPrefix, pCtx->msrEFER,
                pszPrefix, pCtx->msrPAT,
                pszPrefix, pCtx->msrSTAR,
                pszPrefix, pCtx->msrCSTAR,
                pszPrefix, pCtx->msrLSTAR,
                pszPrefix, pCtx->msrSFMASK,
                pszPrefix, pCtx->msrKERNELGSBASE);
            break;
    }
}


/**
 * Display all cpu states and any other cpum info.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    cpumR3InfoGuest(pVM, pHlp, pszArgs);
    cpumR3InfoGuestInstr(pVM, pHlp, pszArgs);
    cpumR3InfoHyper(pVM, pHlp, pszArgs);
    cpumR3InfoHost(pVM, pHlp, pszArgs);
}


/**
 * Parses the info argument.
 *
 * The argument starts with 'verbose', 'terse' or 'default' and then
 * continues with the comment string.
 *
 * @param   pszArgs         The pointer to the argument string.
 * @param   penmType        Where to store the dump type request.
 * @param   ppszComment     Where to store the pointer to the comment string.
 */
static void cpumR3InfoParseArg(const char *pszArgs, CPUMDUMPTYPE *penmType, const char **ppszComment)
{
    if (!pszArgs)
    {
        *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = "";
    }
    else
    {
        if (!strncmp(pszArgs, "verbose", sizeof("verbose") - 1))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_VERBOSE;
        }
        else if (!strncmp(pszArgs, "terse", sizeof("terse") - 1))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_TERSE;
        }
        else if (!strncmp(pszArgs, "default", sizeof("default") - 1))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_DEFAULT;
        }
        else
            *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = RTStrStripL(pszArgs);
    }
}


/**
 * Display the guest cpu state.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);

    /* @todo SMP support! */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    pHlp->pfnPrintf(pHlp, "Guest CPUM (VCPU %d) state: %s\n", pVCpu->idCpu, pszComment);

    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
    cpumR3InfoOne(pVM, pCtx, CPUMCTX2CORE(pCtx), pHlp, enmType, "");
}


/**
 * Display the current guest instruction
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);

    /** @todo SMP support! */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    char szInstruction[256];
    int rc = DBGFR3DisasInstrCurrent(pVCpu, szInstruction, sizeof(szInstruction));
    if (RT_SUCCESS(rc))
        pHlp->pfnPrintf(pHlp, "\nCPUM: %s\n\n", szInstruction);
}


/**
 * Display the hypervisor cpu state.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    /* @todo SMP */
    PVMCPU pVCpu = &pVM->aCpus[0];

    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Hypervisor CPUM state: %s\n", pszComment);
    cpumR3InfoOne(pVM, &pVCpu->cpum.s.Hyper, pVCpu->cpum.s.pHyperCoreR3, pHlp, enmType, ".");
    pHlp->pfnPrintf(pHlp, "CR4OrMask=%#x CR4AndMask=%#x\n", pVM->cpum.s.CR4.OrMask, pVM->cpum.s.CR4.AndMask);
}


/**
 * Display the host cpu state.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Host CPUM state: %s\n", pszComment);

    /*
     * Format the EFLAGS.
     */
    /* @todo SMP */
    PCPUMHOSTCTX pCtx = &pVM->aCpus[0].cpum.s.Host;
#if HC_ARCH_BITS == 32
    uint32_t efl = pCtx->eflags.u32;
#else
    uint64_t efl = pCtx->rflags;
#endif
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
#if HC_ARCH_BITS == 32
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (!(pCtx->efer & MSR_K6_EFER_LMA))
# endif
    {
        pHlp->pfnPrintf(pHlp,
            "eax=xxxxxxxx ebx=%08x ecx=xxxxxxxx edx=xxxxxxxx esi=%08x edi=%08x\n"
            "eip=xxxxxxxx esp=%08x ebp=%08x iopl=%d %31s\n"
            "cs=%04x ds=%04x es=%04x fs=%04x gs=%04x                       eflags=%08x\n"
            "cr0=%08RX64 cr2=xxxxxxxx cr3=%08RX64 cr4=%08RX64 gdtr=%08x:%04x ldtr=%04x\n"
            "dr[0]=%08RX64 dr[1]=%08RX64x dr[2]=%08RX64 dr[3]=%08RX64x dr[6]=%08RX64 dr[7]=%08RX64\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            ,
            /*pCtx->eax,*/ pCtx->ebx, /*pCtx->ecx, pCtx->edx,*/ pCtx->esi, pCtx->edi,
            /*pCtx->eip,*/ pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), szEFlags,
            pCtx->cs, pCtx->ds, pCtx->es, pCtx->fs, pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3, pCtx->cr4,
            pCtx->dr0, pCtx->dr1, pCtx->dr2, pCtx->dr3, pCtx->dr6, pCtx->dr7,
            (uint32_t)pCtx->gdtr.uAddr, pCtx->gdtr.cb, pCtx->ldtr,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);
    }
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    else
# endif
#endif
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    {
        pHlp->pfnPrintf(pHlp,
            "rax=xxxxxxxxxxxxxxxx rbx=%016RX64 rcx=xxxxxxxxxxxxxxxx\n"
            "rdx=xxxxxxxxxxxxxxxx rsi=%016RX64 rdi=%016RX64\n"
            "rip=xxxxxxxxxxxxxxxx rsp=%016RX64 rbp=%016RX64\n"
            " r8=xxxxxxxxxxxxxxxx  r9=xxxxxxxxxxxxxxxx r10=%016RX64\n"
            "r11=%016RX64 r12=%016RX64 r13=%016RX64\n"
            "r14=%016RX64 r15=%016RX64\n"
            "iopl=%d  %31s\n"
            "cs=%04x  ds=%04x  es=%04x  fs=%04x  gs=%04x                   eflags=%08RX64\n"
            "cr0=%016RX64 cr2=xxxxxxxxxxxxxxxx cr3=%016RX64\n"
            "cr4=%016RX64 ldtr=%04x tr=%04x\n"
            "dr[0]=%016RX64 dr[1]=%016RX64 dr[2]=%016RX64\n"
            "dr[3]=%016RX64 dr[6]=%016RX64 dr[7]=%016RX64\n"
            "gdtr=%016RX64:%04x  idtr=%016RX64:%04x\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            "FSbase=%016RX64 GSbase=%016RX64 efer=%08RX64\n"
            ,
            /*pCtx->rax,*/ pCtx->rbx, /*pCtx->rcx,
            pCtx->rdx,*/ pCtx->rsi, pCtx->rdi,
            /*pCtx->rip,*/ pCtx->rsp, pCtx->rbp,
            /*pCtx->r8,  pCtx->r9,*/  pCtx->r10,
            pCtx->r11, pCtx->r12, pCtx->r13,
            pCtx->r14, pCtx->r15,
            X86_EFL_GET_IOPL(efl), szEFlags,
            pCtx->cs, pCtx->ds, pCtx->es, pCtx->fs, pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3,
            pCtx->cr4, pCtx->ldtr, pCtx->tr,
            pCtx->dr0, pCtx->dr1, pCtx->dr2,
            pCtx->dr3, pCtx->dr6, pCtx->dr7,
            pCtx->gdtr.uAddr, pCtx->gdtr.cb, pCtx->idtr.uAddr, pCtx->idtr.cb,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
            pCtx->FSbase, pCtx->GSbase, pCtx->efer);
    }
#endif
}


/**
 * Get L1 cache / TLS associativity.
 */
static const char *getCacheAss(unsigned u, char *pszBuf)
{
    if (u == 0)
        return "res0  ";
    if (u == 1)
        return "direct";
    if (u == 255)
        return "fully";
    if (u >= 256)
        return "???";

    RTStrPrintf(pszBuf, 16, "%d way", u);
    return pszBuf;
}


/**
 * Get L2 cache associativity.
 */
const char *getL2CacheAss(unsigned u)
{
    switch (u)
    {
        case 0:  return "off   ";
        case 1:  return "direct";
        case 2:  return "2 way ";
        case 3:  return "res3  ";
        case 4:  return "4 way ";
        case 5:  return "res5  ";
        case 6:  return "8 way ";
        case 7:  return "res7  ";
        case 8:  return "16 way";
        case 9:  return "res9  ";
        case 10: return "res10 ";
        case 11: return "res11 ";
        case 12: return "res12 ";
        case 13: return "res13 ";
        case 14: return "res14 ";
        case 15: return "fully ";
        default: return "????";
    }
}


/**
 * Display the guest CpuId leaves.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     "terse", "default" or "verbose".
 */
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Parse the argument.
     */
    unsigned iVerbosity = 1;
    if (pszArgs)
    {
        pszArgs = RTStrStripL(pszArgs);
        if (!strcmp(pszArgs, "terse"))
            iVerbosity--;
        else if (!strcmp(pszArgs, "verbose"))
            iVerbosity++;
    }

    /*
     * Start cracking.
     */
    CPUMCPUID   Host;
    CPUMCPUID   Guest;
    unsigned    cStdMax = pVM->cpum.s.aGuestCpuIdStd[0].eax;

    pHlp->pfnPrintf(pHlp,
                    "         RAW Standard CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[i];
        ASMCpuId_Idx_ECX(i, 0, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cStdMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * If verbose, decode it.
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[0];
        pHlp->pfnPrintf(pHlp,
                        "Name:                            %.04s%.04s%.04s\n"
                        "Supports:                        0-%x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    /*
     * Get Features.
     */
    bool const fIntel = ASMIsIntelCpuEx(pVM->cpum.s.aGuestCpuIdStd[0].ebx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].ecx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].edx);
    if (cStdMax >= 1 && iVerbosity)
    {
        static const char * const s_apszTypes[4] = { "primary", "overdrive", "MP", "reserved" };

        Guest = pVM->cpum.s.aGuestCpuIdStd[1];
        uint32_t uEAX = Guest.eax;

        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffective: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffective: %d\n"
                        "Stepping:                        %d\n"
                        "Type:                            %d (%s)\n"
                        "APIC ID:                         %#04x\n"
                        "Logical CPUs:                    %d\n"
                        "CLFLUSH Size:                    %d\n"
                        "Brand ID:                        %#04x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        (uEAX >> 12) & 3, s_apszTypes[(uEAX >> 12) & 3],
                        (Guest.ebx >> 24) & 0xff,
                        (Guest.ebx >> 16) & 0xff,
                        (Guest.ebx >>  8) & 0xff,
                        (Guest.ebx >>  0) & 0xff);
        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SEP");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " PSN");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " CLFSH");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " 20");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " DS");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ACPI");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " SSE");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " SSE2");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " SS");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " HTT");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " TM");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " 30");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " PBE");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " SSE3");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " PCLMUL");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DTES64");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " MONITOR");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " DS-CPL");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " VMX");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " SMX");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " EST");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " TM2");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " SSSE3");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " CNXT-ID");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " 11");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " FMA");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " CX16");
            if (uECX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " TPRUPDATE");
            if (uECX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " PDCM");
            if (uECX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " 16");
            if (uECX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PCID");
            if (uECX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " DCA");
            if (uECX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " SSE4.1");
            if (uECX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " SSE4.2");
            if (uECX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " X2APIC");
            if (uECX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " MOVBE");
            if (uECX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " POPCNT");
            if (uECX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " TSCDEADL");
            if (uECX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " AES");
            if (uECX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " XSAVE");
            if (uECX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " OSXSAVE");
            if (uECX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " AVX");
            if (uECX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " 29");
            if (uECX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " 30");
            if (uECX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " 31");
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(1, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            X86CPUIDFEATEDX EdxHost  = *(PX86CPUIDFEATEDX)&Host.edx;
            X86CPUIDFEATECX EcxHost  = *(PX86CPUIDFEATECX)&Host.ecx;
            X86CPUIDFEATEDX EdxGuest = *(PX86CPUIDFEATEDX)&Guest.edx;
            X86CPUIDFEATECX EcxGuest = *(PX86CPUIDFEATECX)&Guest.ecx;

            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  EdxGuest.u1FPU,        EdxHost.u1FPU);
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  EdxGuest.u1VME,        EdxHost.u1VME);
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  EdxGuest.u1DE,         EdxHost.u1DE);
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  EdxGuest.u1PSE,        EdxHost.u1PSE);
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  EdxGuest.u1TSC,        EdxHost.u1TSC);
            pHlp->pfnPrintf(pHlp, "MSR - Model Specific Registers         = %d (%d)\n",  EdxGuest.u1MSR,        EdxHost.u1MSR);
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  EdxGuest.u1PAE,        EdxHost.u1PAE);
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  EdxGuest.u1MCE,        EdxHost.u1MCE);
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  EdxGuest.u1CX8,        EdxHost.u1CX8);
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  EdxGuest.u1APIC,       EdxHost.u1APIC);
            pHlp->pfnPrintf(pHlp, "10 - Reserved                          = %d (%d)\n",  EdxGuest.u1Reserved1,  EdxHost.u1Reserved1);
            pHlp->pfnPrintf(pHlp, "SEP - SYSENTER and SYSEXIT             = %d (%d)\n",  EdxGuest.u1SEP,        EdxHost.u1SEP);
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  EdxGuest.u1MTRR,       EdxHost.u1MTRR);
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  EdxGuest.u1PGE,        EdxHost.u1PGE);
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  EdxGuest.u1MCA,        EdxHost.u1MCA);
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  EdxGuest.u1CMOV,       EdxHost.u1CMOV);
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  EdxGuest.u1PAT,        EdxHost.u1PAT);
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  EdxGuest.u1PSE36,      EdxHost.u1PSE36);
            pHlp->pfnPrintf(pHlp, "PSN - Processor Serial Number          = %d (%d)\n",  EdxGuest.u1PSN,        EdxHost.u1PSN);
            pHlp->pfnPrintf(pHlp, "CLFSH - CLFLUSH Instruction.           = %d (%d)\n",  EdxGuest.u1CLFSH,      EdxHost.u1CLFSH);
            pHlp->pfnPrintf(pHlp, "20 - Reserved                          = %d (%d)\n",  EdxGuest.u1Reserved2,  EdxHost.u1Reserved2);
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  EdxGuest.u1DS,         EdxHost.u1DS);
            pHlp->pfnPrintf(pHlp, "ACPI - Thermal Mon. & Soft. Clock Ctrl.= %d (%d)\n",  EdxGuest.u1ACPI,       EdxHost.u1ACPI);
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  EdxGuest.u1MMX,        EdxHost.u1MMX);
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  EdxGuest.u1FXSR,       EdxHost.u1FXSR);
            pHlp->pfnPrintf(pHlp, "SSE - SSE Support                      = %d (%d)\n",  EdxGuest.u1SSE,        EdxHost.u1SSE);
            pHlp->pfnPrintf(pHlp, "SSE2 - SSE2 Support                    = %d (%d)\n",  EdxGuest.u1SSE2,       EdxHost.u1SSE2);
            pHlp->pfnPrintf(pHlp, "SS - Self Snoop                        = %d (%d)\n",  EdxGuest.u1SS,         EdxHost.u1SS);
            pHlp->pfnPrintf(pHlp, "HTT - Hyper-Threading Technology       = %d (%d)\n",  EdxGuest.u1HTT,        EdxHost.u1HTT);
            pHlp->pfnPrintf(pHlp, "TM - Thermal Monitor                   = %d (%d)\n",  EdxGuest.u1TM,         EdxHost.u1TM);
            pHlp->pfnPrintf(pHlp, "30 - Reserved                          = %d (%d)\n",  EdxGuest.u1Reserved3,  EdxHost.u1Reserved3);
            pHlp->pfnPrintf(pHlp, "PBE - Pending Break Enable             = %d (%d)\n",  EdxGuest.u1PBE,        EdxHost.u1PBE);

            pHlp->pfnPrintf(pHlp, "Supports SSE3                          = %d (%d)\n",  EcxGuest.u1SSE3,       EcxHost.u1SSE3);
            pHlp->pfnPrintf(pHlp, "PCLMULQDQ                              = %d (%d)\n",  EcxGuest.u1PCLMULQDQ,  EcxHost.u1PCLMULQDQ);
            pHlp->pfnPrintf(pHlp, "DS Area 64-bit layout                  = %d (%d)\n",  EcxGuest.u1DTE64,      EcxHost.u1DTE64);
            pHlp->pfnPrintf(pHlp, "Supports MONITOR/MWAIT                 = %d (%d)\n",  EcxGuest.u1Monitor,    EcxHost.u1Monitor);
            pHlp->pfnPrintf(pHlp, "CPL-DS - CPL Qualified Debug Store     = %d (%d)\n",  EcxGuest.u1CPLDS,      EcxHost.u1CPLDS);
            pHlp->pfnPrintf(pHlp, "VMX - Virtual Machine Technology       = %d (%d)\n",  EcxGuest.u1VMX,        EcxHost.u1VMX);
            pHlp->pfnPrintf(pHlp, "SMX - Safer Mode Extensions            = %d (%d)\n",  EcxGuest.u1SMX,        EcxHost.u1SMX);
            pHlp->pfnPrintf(pHlp, "Enhanced SpeedStep Technology          = %d (%d)\n",  EcxGuest.u1EST,        EcxHost.u1EST);
            pHlp->pfnPrintf(pHlp, "Terminal Monitor 2                     = %d (%d)\n",  EcxGuest.u1TM2,        EcxHost.u1TM2);
            pHlp->pfnPrintf(pHlp, "Supplemental SSE3 instructions         = %d (%d)\n",  EcxGuest.u1SSSE3,      EcxHost.u1SSSE3);
            pHlp->pfnPrintf(pHlp, "L1 Context ID                          = %d (%d)\n",  EcxGuest.u1CNTXID,     EcxHost.u1CNTXID);
            pHlp->pfnPrintf(pHlp, "11 - Reserved                          = %d (%d)\n",  EcxGuest.u1Reserved1,  EcxHost.u1Reserved1);
            pHlp->pfnPrintf(pHlp, "FMA extensions using YMM state         = %d (%d)\n",  EcxGuest.u1FMA,        EcxHost.u1FMA);
            pHlp->pfnPrintf(pHlp, "CMPXCHG16B instruction                 = %d (%d)\n",  EcxGuest.u1CX16,       EcxHost.u1CX16);
            pHlp->pfnPrintf(pHlp, "xTPR Update Control                    = %d (%d)\n",  EcxGuest.u1TPRUpdate,  EcxHost.u1TPRUpdate);
            pHlp->pfnPrintf(pHlp, "Perf/Debug Capability MSR              = %d (%d)\n",  EcxGuest.u1PDCM,       EcxHost.u1PDCM);
            pHlp->pfnPrintf(pHlp, "16 - Reserved                          = %d (%d)\n",  EcxGuest.u1Reserved2,  EcxHost.u1Reserved2);
            pHlp->pfnPrintf(pHlp, "PCID - Process-context identifiers     = %d (%d)\n",  EcxGuest.u1PCID,       EcxHost.u1PCID);
            pHlp->pfnPrintf(pHlp, "DCA - Direct Cache Access              = %d (%d)\n",  EcxGuest.u1DCA,        EcxHost.u1DCA);
            pHlp->pfnPrintf(pHlp, "SSE4.1 instruction extensions          = %d (%d)\n",  EcxGuest.u1SSE4_1,     EcxHost.u1SSE4_1);
            pHlp->pfnPrintf(pHlp, "SSE4.2 instruction extensions          = %d (%d)\n",  EcxGuest.u1SSE4_2,     EcxHost.u1SSE4_2);
            pHlp->pfnPrintf(pHlp, "Supports the x2APIC extensions         = %d (%d)\n",  EcxGuest.u1x2APIC,     EcxHost.u1x2APIC);
            pHlp->pfnPrintf(pHlp, "MOVBE instruction                      = %d (%d)\n",  EcxGuest.u1MOVBE,      EcxHost.u1MOVBE);
            pHlp->pfnPrintf(pHlp, "POPCNT instruction                     = %d (%d)\n",  EcxGuest.u1POPCNT,     EcxHost.u1POPCNT);
            pHlp->pfnPrintf(pHlp, "TSC-Deadline LAPIC timer mode          = %d (%d)\n",  EcxGuest.u1TSCDEADLINE,EcxHost.u1TSCDEADLINE);
            pHlp->pfnPrintf(pHlp, "AESNI instruction extensions           = %d (%d)\n",  EcxGuest.u1AES,        EcxHost.u1AES);
            pHlp->pfnPrintf(pHlp, "XSAVE/XRSTOR extended state feature    = %d (%d)\n",  EcxGuest.u1XSAVE,      EcxHost.u1XSAVE);
            pHlp->pfnPrintf(pHlp, "Supports OSXSAVE                       = %d (%d)\n",  EcxGuest.u1OSXSAVE,    EcxHost.u1OSXSAVE);
            pHlp->pfnPrintf(pHlp, "AVX instruction extensions             = %d (%d)\n",  EcxGuest.u1AVX,        EcxHost.u1AVX);
            pHlp->pfnPrintf(pHlp, "29/30 - Reserved                       = %#x (%#x)\n",EcxGuest.u2Reserved3,  EcxHost.u2Reserved3);
            pHlp->pfnPrintf(pHlp, "Hypervisor Present (we're a guest)     = %d (%d)\n",  EcxGuest.u1HVP,        EcxHost.u1HVP);
        }
    }
    if (cStdMax >= 2 && iVerbosity)
    {
        /** @todo */
    }

    /*
     * Extended.
     * Implemented after AMD specs.
     */
    unsigned    cExtMax = pVM->cpum.s.aGuestCpuIdExt[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Extended CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[i];
        ASMCpuId(0x80000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0x80000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cExtMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[0];
        pHlp->pfnPrintf(pHlp,
                        "Ext Name:                        %.4s%.4s%.4s\n"
                        "Ext Supports:                    0x80000000-%#010x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    if (iVerbosity && cExtMax >= 1)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[1];
        uint32_t uEAX = Guest.eax;
        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffective: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffective: %d\n"
                        "Stepping:                        %d\n"
                        "Brand ID:                        %#05x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        Guest.ebx & 0xfff);

        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SCR");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " 18");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " 19");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " NX");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " 21");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ExtMMX");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " FastFXSR");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " Page1GB");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " RDTSCP");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " 28");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " LongMode");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " Ext3DNow");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " 3DNow");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " LAHF/SAHF");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " CMPL");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " SVM");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " ExtAPIC");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " CR8L");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " ABM");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " SSE4A");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MISALNSSE");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " 3DNOWPRF");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " OSVW");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " IBS");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SSE5");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " SKINIT");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " WDT");
            for (unsigned iBit = 5; iBit < 32; iBit++)
                if (uECX & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(0x80000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            uint32_t uEdxGst = Guest.edx;
            uint32_t uEdxHst = Host.edx;
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "MSR - K86 Model Specific Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "10 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SEP - SYSCALL and SYSRET               = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  !!(uEdxGst & RT_BIT(14)),  !!(uEdxHst & RT_BIT(14)));
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  !!(uEdxGst & RT_BIT(15)),  !!(uEdxHst & RT_BIT(15)));
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  !!(uEdxGst & RT_BIT(16)),  !!(uEdxHst & RT_BIT(16)));
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  !!(uEdxGst & RT_BIT(17)),  !!(uEdxHst & RT_BIT(17)));
            pHlp->pfnPrintf(pHlp, "18 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(18)),  !!(uEdxHst & RT_BIT(18)));
            pHlp->pfnPrintf(pHlp, "19 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(19)),  !!(uEdxHst & RT_BIT(19)));
            pHlp->pfnPrintf(pHlp, "NX - No-Execute Page Protection        = %d (%d)\n",  !!(uEdxGst & RT_BIT(20)),  !!(uEdxHst & RT_BIT(20)));
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  !!(uEdxGst & RT_BIT(21)),  !!(uEdxHst & RT_BIT(21)));
            pHlp->pfnPrintf(pHlp, "AXMMX - AMD Extensions to MMX Instr.   = %d (%d)\n",  !!(uEdxGst & RT_BIT(22)),  !!(uEdxHst & RT_BIT(22)));
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  !!(uEdxGst & RT_BIT(23)),  !!(uEdxHst & RT_BIT(23)));
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  !!(uEdxGst & RT_BIT(24)),  !!(uEdxHst & RT_BIT(24)));
            pHlp->pfnPrintf(pHlp, "25 - AMD fast FXSAVE and FXRSTOR Instr.= %d (%d)\n",  !!(uEdxGst & RT_BIT(25)),  !!(uEdxHst & RT_BIT(25)));
            pHlp->pfnPrintf(pHlp, "26 - 1 GB large page support           = %d (%d)\n",  !!(uEdxGst & RT_BIT(26)),  !!(uEdxHst & RT_BIT(26)));
            pHlp->pfnPrintf(pHlp, "27 - RDTSCP instruction                = %d (%d)\n",  !!(uEdxGst & RT_BIT(27)),  !!(uEdxHst & RT_BIT(27)));
            pHlp->pfnPrintf(pHlp, "28 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(28)),  !!(uEdxHst & RT_BIT(28)));
            pHlp->pfnPrintf(pHlp, "29 - AMD Long Mode                     = %d (%d)\n",  !!(uEdxGst & RT_BIT(29)),  !!(uEdxHst & RT_BIT(29)));
            pHlp->pfnPrintf(pHlp, "30 - AMD Extensions to 3DNow           = %d (%d)\n",  !!(uEdxGst & RT_BIT(30)),  !!(uEdxHst & RT_BIT(30)));
            pHlp->pfnPrintf(pHlp, "31 - AMD 3DNow                         = %d (%d)\n",  !!(uEdxGst & RT_BIT(31)),  !!(uEdxHst & RT_BIT(31)));

            uint32_t uEcxGst = Guest.ecx;
            uint32_t uEcxHst = Host.ecx;
            pHlp->pfnPrintf(pHlp, "LahfSahf - LAHF/SAHF in 64-bit mode    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 0)),  !!(uEcxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "CmpLegacy - Core MP legacy mode (depr) = %d (%d)\n",  !!(uEcxGst & RT_BIT( 1)),  !!(uEcxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "SVM - AMD VM Extensions                = %d (%d)\n",  !!(uEcxGst & RT_BIT( 2)),  !!(uEcxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "APIC registers starting at 0x400       = %d (%d)\n",  !!(uEcxGst & RT_BIT( 3)),  !!(uEcxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "AltMovCR8 - LOCK MOV CR0 means MOV CR8 = %d (%d)\n",  !!(uEcxGst & RT_BIT( 4)),  !!(uEcxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "Advanced bit manipulation              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 5)),  !!(uEcxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "SSE4A instruction support              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 6)),  !!(uEcxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "Misaligned SSE mode                    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 7)),  !!(uEcxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "PREFETCH and PREFETCHW instruction     = %d (%d)\n",  !!(uEcxGst & RT_BIT( 8)),  !!(uEcxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "OS visible workaround                  = %d (%d)\n",  !!(uEcxGst & RT_BIT( 9)),  !!(uEcxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "Instruction based sampling             = %d (%d)\n",  !!(uEcxGst & RT_BIT(10)),  !!(uEcxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SSE5 support                           = %d (%d)\n",  !!(uEcxGst & RT_BIT(11)),  !!(uEcxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "SKINIT, STGI, and DEV support          = %d (%d)\n",  !!(uEcxGst & RT_BIT(12)),  !!(uEcxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "Watchdog timer support.                = %d (%d)\n",  !!(uEcxGst & RT_BIT(13)),  !!(uEcxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "31:14 - Reserved                       = %#x (%#x)\n",   uEcxGst >> 14,          uEcxHst >> 14);
        }
    }

    if (iVerbosity && cExtMax >= 2)
    {
        char szString[4*4*3+1] = {0};
        uint32_t *pu32 = (uint32_t *)szString;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].eax;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ebx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ecx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].edx;
        if (cExtMax >= 3)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].edx;
        }
        if (cExtMax >= 4)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].edx;
        }
        pHlp->pfnPrintf(pHlp, "Full Name:                       %s\n", szString);
    }

    if (iVerbosity && cExtMax >= 5)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[5].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[5].ebx;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[5].ecx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[5].edx;
        char sz1[32];
        char sz2[32];

        pHlp->pfnPrintf(pHlp,
                        "TLB 2/4M Instr/Uni:              %s %3d entries\n"
                        "TLB 2/4M Data:                   %s %3d entries\n",
                        getCacheAss((uEAX >>  8) & 0xff, sz1), (uEAX >>  0) & 0xff,
                        getCacheAss((uEAX >> 24) & 0xff, sz2), (uEAX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "TLB 4K Instr/Uni:                %s %3d entries\n"
                        "TLB 4K Data:                     %s %3d entries\n",
                        getCacheAss((uEBX >>  8) & 0xff, sz1), (uEBX >>  0) & 0xff,
                        getCacheAss((uEBX >> 24) & 0xff, sz2), (uEBX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp, "L1 Instr Cache Line Size:        %d bytes\n"
                        "L1 Instr Cache Lines Per Tag:    %d\n"
                        "L1 Instr Cache Associativity:    %s\n"
                        "L1 Instr Cache Size:             %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xff,
                        getCacheAss((uEDX >> 16) & 0xff, sz1),
                        (uEDX >> 24) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "L1 Data Cache Line Size:         %d bytes\n"
                        "L1 Data Cache Lines Per Tag:     %d\n"
                        "L1 Data Cache Associativity:     %s\n"
                        "L1 Data Cache Size:              %d KB\n",
                        (uECX >> 0) & 0xff,
                        (uECX >> 8) & 0xff,
                        getCacheAss((uECX >> 16) & 0xff, sz1),
                        (uECX >> 24) & 0xff);
    }

    if (iVerbosity && cExtMax >= 6)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[6].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[6].ebx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[6].edx;

        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 2/4M Instr/Uni:           %s %4d entries\n"
                        "L2 TLB 2/4M Data:                %s %4d entries\n",
                        getL2CacheAss((uEAX >> 12) & 0xf),  (uEAX >>  0) & 0xfff,
                        getL2CacheAss((uEAX >> 28) & 0xf),  (uEAX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 4K Instr/Uni:             %s %4d entries\n"
                        "L2 TLB 4K Data:                  %s %4d entries\n",
                        getL2CacheAss((uEBX >> 12) & 0xf),  (uEBX >>  0) & 0xfff,
                        getL2CacheAss((uEBX >> 28) & 0xf),  (uEBX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 Cache Line Size:              %d bytes\n"
                        "L2 Cache Lines Per Tag:          %d\n"
                        "L2 Cache Associativity:          %s\n"
                        "L2 Cache Size:                   %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xf,
                        getL2CacheAss((uEDX >> 12) & 0xf),
                        (uEDX >> 16) & 0xffff);
    }

    if (iVerbosity && cExtMax >= 7)
    {
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[7].edx;

        pHlp->pfnPrintf(pHlp, "APM Features:                   ");
        if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " TS");
        if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " FID");
        if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " VID");
        if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " TTP");
        if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TM");
        if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " STC");
        for (unsigned iBit = 6; iBit < 32; iBit++)
            if (uEDX & RT_BIT(iBit))
                pHlp->pfnPrintf(pHlp, " %d", iBit);
        pHlp->pfnPrintf(pHlp, "\n");
    }

    if (iVerbosity && cExtMax >= 8)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[8].eax;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[8].ecx;

        pHlp->pfnPrintf(pHlp,
                        "Physical Address Width:          %d bits\n"
                        "Virtual Address Width:           %d bits\n"
                        "Guest Physical Address Width:    %d bits\n",
                        (uEAX >> 0) & 0xff,
                        (uEAX >> 8) & 0xff,
                        (uEAX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "Physical Core Count:             %d\n",
                        (uECX >> 0) & 0xff);
    }


    /*
     * Centaur.
     */
    unsigned cCentaurMax = pVM->cpum.s.aGuestCpuIdCentaur[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Centaur CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[i];
        ASMCpuId(0xc0000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0xc0000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cCentaurMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[0];
        pHlp->pfnPrintf(pHlp,
                        "Centaur Supports:                0xc0000000-%#010x\n",
                        Guest.eax);
    }

    if (iVerbosity && cCentaurMax >= 1)
    {
        ASMCpuId(0xc0000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);
        uint32_t uEdxGst = pVM->cpum.s.aGuestCpuIdExt[1].edx;
        uint32_t uEdxHst = Host.edx;

        if (iVerbosity == 1)
        {
            pHlp->pfnPrintf(pHlp, "Centaur Features EDX:           ");
            if (uEdxGst & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " AIS");
            if (uEdxGst & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " AIS-E");
            if (uEdxGst & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " RNG");
            if (uEdxGst & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " RNG-E");
            if (uEdxGst & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " LH");
            if (uEdxGst & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " FEMMS");
            if (uEdxGst & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " ACE");
            if (uEdxGst & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " ACE-E");
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            if (uEdxGst & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " ACE2");
            if (uEdxGst & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " ACE2-E");
            if (uEdxGst & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " PHE");
            if (uEdxGst & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " PHE-E");
            if (uEdxGst & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " PMM");
            if (uEdxGst & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PMM-E");
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if (uEdxGst & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "AIS - Alternate Instruction Set        = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "AIS-E - AIS enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "RNG - Random Number Generator          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "RNG-E - RNG enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "LH - LongHaul MSR 0000_110Ah           = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "FEMMS - FEMMS                          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "ACE - Advanced Cryptography Engine     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "ACE-E - ACE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            pHlp->pfnPrintf(pHlp, "ACE2 - Advanced Cryptography Engine 2  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "ACE2-E - ACE enabled                   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "PHE - Hash Engine                      = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "PHE-E - PHE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "PMM - Montgomery Multiplier            = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PMM-E - PMM enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if ((uEdxGst | uEdxHst) & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, "Bit %d                                 = %d (%d)\n",  !!(uEdxGst & RT_BIT(iBit)),  !!(uEdxHst & RT_BIT(iBit)));
            pHlp->pfnPrintf(pHlp, "\n");
        }
    }
}


/**
 * Structure used when disassembling and instructions in DBGF.
 * This is used so the reader function can get the stuff it needs.
 */
typedef struct CPUMDISASSTATE
{
    /** Pointer to the CPU structure. */
    PDISCPUSTATE    pCpu;
    /** Pointer to the VM. */
    PVM             pVM;
    /** Pointer to the VMCPU. */
    PVMCPU          pVCpu;
    /** Pointer to the first byte in the segment. */
    RTGCUINTPTR     GCPtrSegBase;
    /** Pointer to the byte after the end of the segment. (might have wrapped!) */
    RTGCUINTPTR     GCPtrSegEnd;
    /** The size of the segment minus 1. */
    RTGCUINTPTR     cbSegLimit;
    /** Pointer to the current page - R3 Ptr. */
    void const     *pvPageR3;
    /** Pointer to the current page - GC Ptr. */
    RTGCPTR         pvPageGC;
    /** The lock information that PGMPhysReleasePageMappingLock needs. */
    PGMPAGEMAPLOCK  PageMapLock;
    /** Whether the PageMapLock is valid or not. */
    bool            fLocked;
    /** 64 bits mode or not. */
    bool            f64Bits;
} CPUMDISASSTATE, *PCPUMDISASSTATE;


/**
 * @callback_method_impl{FNDISREADBYTES}
 */
static DECLCALLBACK(int) cpumR3DisasInstrRead(PDISCPUSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
    PCPUMDISASSTATE pState = (PCPUMDISASSTATE)pDis->pvUser;
    for (;;)
    {
        RTGCUINTPTR GCPtr = pDis->uInstrAddr + offInstr + pState->GCPtrSegBase;

        /*
         * Need to update the page translation?
         */
        if (   !pState->pvPageR3
            || (GCPtr >> PAGE_SHIFT) != (pState->pvPageGC >> PAGE_SHIFT))
        {
            int rc = VINF_SUCCESS;

            /* translate the address */
            pState->pvPageGC = GCPtr & PAGE_BASE_GC_MASK;
            if (    MMHyperIsInsideArea(pState->pVM, pState->pvPageGC)
                &&  !HWACCMIsEnabled(pState->pVM))
            {
                pState->pvPageR3 = MMHyperRCToR3(pState->pVM, (RTRCPTR)pState->pvPageGC);
                if (!pState->pvPageR3)
                    rc = VERR_INVALID_POINTER;
            }
            else
            {
                /* Release mapping lock previously acquired. */
                if (pState->fLocked)
                    PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);
                rc = PGMPhysGCPtr2CCPtrReadOnly(pState->pVCpu, pState->pvPageGC, &pState->pvPageR3, &pState->PageMapLock);
                pState->fLocked = RT_SUCCESS_NP(rc);
            }
            if (RT_FAILURE(rc))
            {
                pState->pvPageR3 = NULL;
                return rc;
            }
        }

        /*
         * Check the segment limit.
         */
        if (!pState->f64Bits && pDis->uInstrAddr + offInstr > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /*
         * Calc how much we can read.
         */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        if (!pState->f64Bits)
        {
            RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
            if (cb > cbSeg && cbSeg)
                cb = cbSeg;
        }
        if (cb > cbMaxRead)
            cb = cbMaxRead;

        /*
         * Read and advance or exit.
         */
        memcpy(&pDis->abInstr[offInstr], (uint8_t *)pState->pvPageR3 + (GCPtr & PAGE_OFFSET_MASK), cb);
        offInstr  += (uint8_t)cb;
        if (cb >= cbMinRead)
        {
            pDis->cbCachedInstr = offInstr;
            return VINF_SUCCESS;
        }
        cbMinRead -= (uint8_t)cb;
        cbMaxRead -= (uint8_t)cb;
    }
}


/**
 * Disassemble an instruction and return the information in the provided structure.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 * @param   GCPtrPC     Program counter (relative to CS) to disassemble from.
 * @param   pCpu        Disassembly state.
 * @param   pszPrefix   String prefix for logging (debug only).
 *
 */
VMMR3DECL(int) CPUMR3DisasmInstrCPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTGCPTR GCPtrPC, PDISCPUSTATE pCpu, const char *pszPrefix)
{
    CPUMDISASSTATE  State;
    int             rc;

    const PGMMODE enmMode = PGMGetGuestMode(pVCpu);
    State.pCpu            = pCpu;
    State.pvPageGC        = 0;
    State.pvPageR3        = NULL;
    State.pVM             = pVM;
    State.pVCpu           = pVCpu;
    State.fLocked         = false;
    State.f64Bits         = false;

    /*
     * Get selector information.
     */
    DISCPUMODE enmDisCpuMode;
    if (    (pCtx->cr0 & X86_CR0_PE)
        &&   pCtx->eflags.Bits.u1VM == 0)
    {
        if (CPUMAreHiddenSelRegsValid(pVCpu))
        {
            State.f64Bits         = enmMode >= PGMMODE_AMD64 && pCtx->cs.Attr.n.u1Long;
            State.GCPtrSegBase    = pCtx->cs.u64Base;
            State.GCPtrSegEnd     = pCtx->cs.u32Limit + 1 + (RTGCUINTPTR)pCtx->cs.u64Base;
            State.cbSegLimit      = pCtx->cs.u32Limit;
            enmDisCpuMode         = (State.f64Bits)
                                    ? DISCPUMODE_64BIT
                                    : pCtx->cs.Attr.n.u1DefBig
                                    ? DISCPUMODE_32BIT
                                    : DISCPUMODE_16BIT;
        }
        else
        {
            DBGFSELINFO SelInfo;

            rc = SELMR3GetShadowSelectorInfo(pVM, pCtx->cs.Sel, &SelInfo);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("SELMR3GetShadowSelectorInfo failed for %04X:%RGv rc=%d\n", pCtx->cs.Sel, GCPtrPC, rc));
                return rc;
            }

            /*
             * Validate the selector.
             */
            rc = DBGFR3SelInfoValidateCS(&SelInfo, pCtx->ss.Sel);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("SELMSelInfoValidateCS failed for %04X:%RGv rc=%d\n", pCtx->cs.Sel, GCPtrPC, rc));
                return rc;
            }
            State.GCPtrSegBase    = SelInfo.GCPtrBase;
            State.GCPtrSegEnd     = SelInfo.cbLimit + 1 + (RTGCUINTPTR)SelInfo.GCPtrBase;
            State.cbSegLimit      = SelInfo.cbLimit;
            enmDisCpuMode         = SelInfo.u.Raw.Gen.u1DefBig ? DISCPUMODE_32BIT : DISCPUMODE_16BIT;
        }
    }
    else
    {
        /* real or V86 mode */
        enmDisCpuMode         = DISCPUMODE_16BIT;
        State.GCPtrSegBase    = pCtx->cs.Sel * 16;
        State.GCPtrSegEnd     = 0xFFFFFFFF;
        State.cbSegLimit      = 0xFFFFFFFF;
    }

    /*
     * Disassemble the instruction.
     */
    uint32_t cbInstr;
#ifndef LOG_ENABLED
    rc = DISInstrWithReader(GCPtrPC, enmDisCpuMode, cpumR3DisasInstrRead, &State, pCpu, &cbInstr);
    if (RT_SUCCESS(rc))
    {
#else
    char szOutput[160];
    rc = DISInstrToStrWithReader(GCPtrPC, enmDisCpuMode, cpumR3DisasInstrRead, &State,
                                 pCpu, &cbInstr, szOutput, sizeof(szOutput));
    if (RT_SUCCESS(rc))
    {
        /* log it */
        if (pszPrefix)
            Log(("%s-CPU%d: %s", pszPrefix, pVCpu->idCpu, szOutput));
        else
            Log(("%s", szOutput));
#endif
        rc = VINF_SUCCESS;
    }
    else
        Log(("CPUMR3DisasmInstrCPU: DISInstr failed for %04X:%RGv rc=%Rrc\n", pCtx->cs.Sel, GCPtrPC, rc));

    /* Release mapping lock acquired in cpumR3DisasInstrRead. */
    if (State.fLocked)
        PGMPhysReleasePageMappingLock(pVM, &State.PageMapLock);

    return rc;
}



/**
 * API for controlling a few of the CPU features found in CR4.
 *
 * Currently only X86_CR4_TSD is accepted as input.
 *
 * @returns VBox status code.
 *
 * @param   pVM     Pointer to the VM.
 * @param   fOr     The CR4 OR mask.
 * @param   fAnd    The CR4 AND mask.
 */
VMMR3DECL(int) CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd)
{
    AssertMsgReturn(!(fOr & ~(X86_CR4_TSD)), ("%#x\n", fOr), VERR_INVALID_PARAMETER);
    AssertMsgReturn((fAnd & ~(X86_CR4_TSD)) == ~(X86_CR4_TSD), ("%#x\n", fAnd), VERR_INVALID_PARAMETER);

    pVM->cpum.s.CR4.OrMask &= fAnd;
    pVM->cpum.s.CR4.OrMask |= fOr;

    return VINF_SUCCESS;
}


/**
 * Gets a pointer to the array of standard CPUID leaves.
 *
 * CPUMR3GetGuestCpuIdStdMax() give the size of the array.
 *
 * @returns Pointer to the standard CPUID leaves (read-only).
 * @param   pVM         Pointer to the VM.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdStdRCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdStd[0]);
}


/**
 * Gets a pointer to the array of extended CPUID leaves.
 *
 * CPUMGetGuestCpuIdExtMax() give the size of the array.
 *
 * @returns Pointer to the extended CPUID leaves (read-only).
 * @param   pVM         Pointer to the VM.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdExtRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdExt[0]);
}


/**
 * Gets a pointer to the array of centaur CPUID leaves.
 *
 * CPUMGetGuestCpuIdCentaurMax() give the size of the array.
 *
 * @returns Pointer to the centaur CPUID leaves (read-only).
 * @param   pVM         Pointer to the VM.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdCentaurRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdCentaur[0]);
}


/**
 * Gets a pointer to the default CPUID leaf.
 *
 * @returns Pointer to the default CPUID leaf (read-only).
 * @param   pVM         Pointer to the VM.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdDefRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.GuestCpuIdDef);
}


/**
 * Transforms the guest CPU state to raw-ring mode.
 *
 * This function will change the any of the cs and ss register with DPL=0 to DPL=1.
 *
 * @returns VBox status. (recompiler failure)
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
VMMR3DECL(int) CPUMR3RawEnter(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    Assert(!pVCpu->cpum.s.fRawEntered);
    Assert(!pVCpu->cpum.s.fRemEntered);
    if (!pCtxCore)
        pCtxCore = CPUMCTX2CORE(&pVCpu->cpum.s.Guest);

    /*
     * Are we in Ring-0?
     */
    if (    pCtxCore->ss.Sel && (pCtxCore->ss.Sel & X86_SEL_RPL) == 0
        &&  !pCtxCore->eflags.Bits.u1VM)
    {
        /*
         * Enter execution mode.
         */
        PATMRawEnter(pVM, pCtxCore);

        /*
         * Set CPL to Ring-1.
         */
        pCtxCore->ss.Sel |= 1;
        if (pCtxCore->cs.Sel && (pCtxCore->cs.Sel & X86_SEL_RPL) == 0)
            pCtxCore->cs.Sel |= 1;
    }
    else
    {
        AssertMsg((pCtxCore->ss.Sel & X86_SEL_RPL) >= 2 || pCtxCore->eflags.Bits.u1VM,
                  ("ring-1 code not supported\n"));
        /*
         * PATM takes care of IOPL and IF flags for Ring-3 and Ring-2 code as well.
         */
        PATMRawEnter(pVM, pCtxCore);
    }

    /*
     * Invalidate the hidden registers.
     */
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID;

    /*
     * Assert sanity.
     */
    AssertMsg((pCtxCore->eflags.u32 & X86_EFL_IF), ("X86_EFL_IF is clear\n"));
    AssertReleaseMsg(   pCtxCore->eflags.Bits.u2IOPL < (unsigned)(pCtxCore->ss.Sel & X86_SEL_RPL)
                     || pCtxCore->eflags.Bits.u1VM,
                     ("X86_EFL_IOPL=%d CPL=%d\n", pCtxCore->eflags.Bits.u2IOPL, pCtxCore->ss.Sel & X86_SEL_RPL));
    Assert((pVCpu->cpum.s.Guest.cr0 & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) == (X86_CR0_PG | X86_CR0_PE | X86_CR0_WP));

    pCtxCore->eflags.u32        |= X86_EFL_IF; /* paranoia */

    pVCpu->cpum.s.fRawEntered = true;
    return VINF_SUCCESS;
}


/**
 * Transforms the guest CPU state from raw-ring mode to correct values.
 *
 * This function will change any selector registers with DPL=1 to DPL=0.
 *
 * @returns Adjusted rc.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   rc          Raw mode return code
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
VMMR3DECL(int) CPUMR3RawLeave(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, int rc)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Don't leave if we've already left (in GC).
     */
    Assert(pVCpu->cpum.s.fRawEntered);
    Assert(!pVCpu->cpum.s.fRemEntered);
    if (!pVCpu->cpum.s.fRawEntered)
        return rc;
    pVCpu->cpum.s.fRawEntered = false;

    PCPUMCTX pCtx = &pVCpu->cpum.s.Guest;
    if (!pCtxCore)
        pCtxCore = CPUMCTX2CORE(pCtx);
    Assert(pCtxCore->eflags.Bits.u1VM || (pCtxCore->ss.Sel & X86_SEL_RPL));
    AssertMsg(pCtxCore->eflags.Bits.u1VM || pCtxCore->eflags.Bits.u2IOPL < (unsigned)(pCtxCore->ss.Sel & X86_SEL_RPL),
              ("X86_EFL_IOPL=%d CPL=%d\n", pCtxCore->eflags.Bits.u2IOPL, pCtxCore->ss.Sel & X86_SEL_RPL));

    /*
     * Are we executing in raw ring-1?
     */
    if (    (pCtxCore->ss.Sel & X86_SEL_RPL) == 1
        &&  !pCtxCore->eflags.Bits.u1VM)
    {
        /*
         * Leave execution mode.
         */
        PATMRawLeave(pVM, pCtxCore, rc);
        /* Not quite sure if this is really required, but shouldn't harm (too much anyways). */
        /** @todo See what happens if we remove this. */
        if ((pCtxCore->ds.Sel & X86_SEL_RPL) == 1)
            pCtxCore->ds.Sel &= ~X86_SEL_RPL;
        if ((pCtxCore->es.Sel & X86_SEL_RPL) == 1)
            pCtxCore->es.Sel &= ~X86_SEL_RPL;
        if ((pCtxCore->fs.Sel & X86_SEL_RPL) == 1)
            pCtxCore->fs.Sel &= ~X86_SEL_RPL;
        if ((pCtxCore->gs.Sel & X86_SEL_RPL) == 1)
            pCtxCore->gs.Sel &= ~X86_SEL_RPL;

        /*
         * Ring-1 selector => Ring-0.
         */
        pCtxCore->ss.Sel &= ~X86_SEL_RPL;
        if ((pCtxCore->cs.Sel & X86_SEL_RPL) == 1)
            pCtxCore->cs.Sel &= ~X86_SEL_RPL;
    }
    else
    {
        /*
         * PATM is taking care of the IOPL and IF flags for us.
         */
        PATMRawLeave(pVM, pCtxCore, rc);
        if (!pCtxCore->eflags.Bits.u1VM)
        {
            /** @todo See what happens if we remove this. */
            if ((pCtxCore->ds.Sel & X86_SEL_RPL) == 1)
                pCtxCore->ds.Sel &= ~X86_SEL_RPL;
            if ((pCtxCore->es.Sel & X86_SEL_RPL) == 1)
                pCtxCore->es.Sel &= ~X86_SEL_RPL;
            if ((pCtxCore->fs.Sel & X86_SEL_RPL) == 1)
                pCtxCore->fs.Sel &= ~X86_SEL_RPL;
            if ((pCtxCore->gs.Sel & X86_SEL_RPL) == 1)
                pCtxCore->gs.Sel &= ~X86_SEL_RPL;
        }
    }

    return rc;
}


/**
 * Enters REM, gets and resets the changed flags (CPUM_CHANGED_*).
 *
 * Only REM should ever call this function!
 *
 * @returns The changed flags.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   puCpl       Where to return the current privilege level (CPL).
 */
VMMR3DECL(uint32_t) CPUMR3RemEnter(PVMCPU pVCpu, uint32_t *puCpl)
{
    Assert(!pVCpu->cpum.s.fRawEntered);
    Assert(!pVCpu->cpum.s.fRemEntered);

    /*
     * Get the CPL first.
     */
    *puCpl = CPUMGetGuestCPL(pVCpu, CPUMCTX2CORE(&pVCpu->cpum.s.Guest));

    /*
     * Get and reset the flags, leaving CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID set.
     */
    uint32_t fFlags = pVCpu->cpum.s.fChanged;
    pVCpu->cpum.s.fChanged &= CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID; /* leave it set */

    /** @todo change the switcher to use the fChanged flags. */
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_SINCE_REM)
    {
        fFlags |= CPUM_CHANGED_FPU_REM;
        pVCpu->cpum.s.fUseFlags &= ~CPUM_USED_FPU_SINCE_REM;
    }

    pVCpu->cpum.s.fRemEntered = true;
    return fFlags;
}


/**
 * Leaves REM and works the CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID flag.
 *
 * @param   pVCpu               Pointer to the VMCPU.
 * @param   fNoOutOfSyncSels    This is @c false if there are out of sync
 *                              registers.
 */
VMMR3DECL(void) CPUMR3RemLeave(PVMCPU pVCpu, bool fNoOutOfSyncSels)
{
    Assert(!pVCpu->cpum.s.fRawEntered);
    Assert(pVCpu->cpum.s.fRemEntered);

    if (fNoOutOfSyncSels)
        pVCpu->cpum.s.fChanged &= ~CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID;
    else
        pVCpu->cpum.s.fChanged |= ~CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID;

    pVCpu->cpum.s.fRemEntered = false;
}

