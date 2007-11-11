/* $Id$ */
/** @file
 *
 * VBoxREM Win64 DLL Wrapper.
 */
/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_vboxrem_amd64      VBoxREM Hacks on AMD64
 *
 * There are problems with building BoxREM both on WIN64 and 64-bit linux.
 *
 * On linux binutils refuses to link shared objects without -fPIC compiled code
 * (bitches about some fixup types). But when trying to build with -fPIC dyngen
 * doesn't like the code anymore. Sweet. The current solution is to build the
 * VBoxREM code as a relocatable module and use our ELF loader to load it.
 *
 * On WIN64 we're not aware of any GCC port which can emit code using the MSC
 * calling convention. So, we're in for some real fun here. The choice is between
 * porting GCC to AMD64 WIN64 and comming up with some kind of wrapper around
 * either the win32 build or the 64-bit linux build.
 *
 *  -# Porting GCC will be a lot of work. For one thing the calling convention differs
 *     and messing with such stuff can easily create ugly bugs. We would also have to
 *     do some binutils changes, but I think those are rather small compared to GCC.
 *     (That said, the MSC calling convention is far simpler than the linux one, it
 *     reminds me of _Optlink which we have working already.)
 *  -# Wrapping win32 code will work, but addresses outside the first 4GB are
 *     inaccessible and we will have to create 32-64 thunks for all imported functions.
 *     (To switch between 32-bit and 64-bit is load the right CS using far jmps (32->64)
 *     or far returns (both).)
 *  -# Wrapping 64-bit linux code might be the easier solution. The requirements here
 *     are:
 *       - Remove all CRT references we possibly, either by using intrinsics or using
 *         IPRT. Part of IPRT will be linked into VBoxREM2.rel, this will be yet another
 *         IPRT mode which I've dubbed 'no-crt'. The no-crt mode provide basic non-system
 *         dependent stuff.
 *       - Compile and link it into a relocatable object (include the gcc intrinsics
 *         in libgcc). Call this VBoxREM2.rel.
 *       - Write a wrapper dll, VBoxREM.dll, for which during REMR3Init() will load
 *         VBoxREM2.rel (using IPRT) and generate calling convention wrappers
 *         for all IPRT functions and VBoxVMM functions that it uses. All exports
 *         will be wrapped vice versa.
 *       - For building on windows hosts, we will use a mingw32 hosted cross compiler.
 *         and add a 'no-crt' mode to IPRT where it provides the necessary CRT headers
 *         and function implementations.
 *
 * The 3rd solution will be tried out first since it requires the least effort and
 * will let us make use of the full 64-bit register set.
 *
 *
 *
 * @section sec_vboxrem_amd64_compare   Comparing the GCC and MSC calling conventions
 *
 * GCC expects the following (cut & past from page 20 in the ABI draft 0.96):
 *
 * %rax     temporary register; with variable arguments passes information about the
 *          number of SSE registers used; 1st return register.
 *          [Not preserved]
 * %rbx     callee-saved register; optionally used as base pointer.
 *          [Preserved]
 * %rcx     used to pass 4th integer argument to functions.
 *          [Not preserved]
 * %rdx     used to pass 3rd argument to functions; 2nd return register
 *          [Not preserved]
 * %rsp     stack pointer
 *          [Preserved]
 * %rbp     callee-saved register; optionally used as frame pointer
 *          [Preserved]
 * %rsi     used to pass 2nd argument to functions
 *          [Not preserved]
 * %rdi     used to pass 1st argument to functions
 *          [Not preserved]
 * %r8      used to pass 5th argument to functions
 *          [Not preserved]
 * %r9      used to pass 6th argument to functions
 *          [Not preserved]
 * %r10     temporary register, used for passing a function’s static chain pointer
 *          [Not preserved]
 * %r11     temporary register
 *          [Not preserved]
 * %r12-r15 callee-saved registers
 *          [Preserved]
 * %xmm0-%xmm1  used to pass and return floating point arguments
 *          [Not preserved]
 * %xmm2-%xmm7  used to pass floating point arguments
 *          [Not preserved]
 * %xmm8-%xmm15 temporary registers
 *          [Not preserved]
 * %mmx0-%mmx7  temporary registers
 *          [Not preserved]
 * %st0     temporary register; used to return long double arguments
 *          [Not preserved]
 * %st1     temporary registers; used to return long double arguments
 *          [Not preserved]
 * %st2-%st7 temporary registers
 *          [Not preserved]
 * %fs      Reserved for system use (as thread specific data register)
 *          [Not preserved]
 *
 * Direction flag is preserved as cleared.
 * The stack must be aligned on a 16-byte boundrary before the 'call/jmp' instruction.
 *
 *
 *
 * MSC expects the following:
 * rax      return value, not preserved.
 * rbx      preserved.
 * rcx      1st argument, integer, not preserved.
 * rdx      2nd argument, integer, not preserved.
 * rbp      preserved.
 * rsp      preserved.
 * rsi      preserved.
 * rdi      preserved.
 * r8       3rd argument, integer, not preserved.
 * r9       4th argument, integer, not preserved.
 * r10      scratch register, not preserved.
 * r11      scratch register, not preserved.
 * r12-r15  preserved.
 * xmm0     1st argument, fp, return value, not preserved.
 * xmm1     2st argument, fp, not preserved.
 * xmm2     3st argument, fp, not preserved.
 * xmm3     4st argument, fp, not preserved.
 * xmm4-xmm5    scratch, not preserved.
 * xmm6-xmm15   preserved.
 *
 * Dunno what the direction flag is...
 * The stack must be aligned on a 16-byte boundrary before the 'call/jmp' instruction.
 *
 *
 * Thus, When GCC code is calling MSC code we don't really have to preserve
 * anything. But but MSC code is calling GCC code, we'll have to save esi and edi.
 *
 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def USE_REM_STUBS
 * Define USE_REM_STUBS to stub the entire REM stuff. This is useful during
 * early porting (before we start running stuff).
 */
#if defined(__DOXYGEN__)
# define USE_REM_STUBS
#endif

/** @def USE_REM_CALLING_CONVENTION_GLUE
 * Define USE_REM_CALLING_CONVENTION_GLUE for platforms where it's necessary to
 * use calling convention wrappers.
 */
#if (defined(RT_ARCH_AMD64) && defined(RT_OS_WINDOWS)) || defined(__DOXYGEN__)
# define USE_REM_CALLING_CONVENTION_GLUE
#endif

/** @def USE_REM_IMPORT_JUMP_GLUE
 * Define USE_REM_IMPORT_JUMP_GLUE for platforms where we need to
 * emit some jump glue to deal with big addresses.
 */
#if (defined(RT_ARCH_AMD64) && !defined(USE_REM_CALLING_CONVENTION_GLUE) && !defined(RT_OS_DARWIN)) || defined(__DOXYGEN__)
# define USE_REM_IMPORT_JUMP_GLUE
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_REM
#include <VBox/rem.h>
#include <VBox/vmm.h>
#include <VBox/dbgf.h>
#include <VBox/dbg.h>
#include <VBox/csam.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/ssm.h>
#include <VBox/hwaccm.h>
#include <VBox/patm.h>
#include <VBox/pdm.h>
#include <VBox/pgm.h>
#include <VBox/iom.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/dis.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Parameter descriptor.
 */
typedef struct REMPARMDESC
{
    /** Parameter flags (REMPARMDESC_FLAGS_*). */
    uint8_t     fFlags;
    /** The parameter size if REMPARMDESC_FLAGS_SIZE is set. */
    uint8_t     cb;
    /** Pointer to additional data.
     * For REMPARMDESC_FLAGS_PFN this is a PREMFNDESC. */
    void       *pvExtra;

} REMPARMDESC, *PREMPARMDESC;
/** Pointer to a constant parameter descriptor. */
typedef const REMPARMDESC *PCREMPARMDESC;

/** @name Parameter descriptor flags.
 * @{ */
/** The parameter type is a kind of integer which could fit in a register. This includes pointers. */
#define REMPARMDESC_FLAGS_INT           0
/** The parameter is a GC pointer. */
#define REMPARMDESC_FLAGS_GCPTR         1
/** The parameter is a GC physical address. */
#define REMPARMDESC_FLAGS_GCPHYS        2
/** The parameter is a HC physical address. */
#define REMPARMDESC_FLAGS_HCPHYS        3
/** The parameter type is a kind of floating point. */
#define REMPARMDESC_FLAGS_FLOAT         4
/** The parameter value is a struct. This type takes a size. */
#define REMPARMDESC_FLAGS_STRUCT        5
/** The parameter is an elipsis. */
#define REMPARMDESC_FLAGS_ELLIPSIS      6
/** The parameter is a va_list. */
#define REMPARMDESC_FLAGS_VALIST        7
/** The parameter is a function pointer. pvExtra is a PREMFNDESC. */
#define REMPARMDESC_FLAGS_PFN           8
/** The parameter type mask. */
#define REMPARMDESC_FLAGS_TYPE_MASK     15
/** The parameter size field is valid. */
#define REMPARMDESC_FLAGS_SIZE          RT_BIT(7)
/** @} */

/**
 * Function descriptor.
 */
typedef struct REMFNDESC
{
    /** The function name. */
    const char         *pszName;
    /** Exports: Pointer to the function pointer.
     * Imports: Pointer to the function. */
    void               *pv;
    /** Array of parameter descriptors. */
    PCREMPARMDESC       paParams;
    /** The number of parameter descriptors pointed to by paParams. */
    uint8_t             cParams;
    /** Function flags (REMFNDESC_FLAGS_*). */
    uint8_t             fFlags;
    /** The size of the return value. */
    uint8_t             cbReturn;
    /** Pointer to the wrapper code for imports. */
    void               *pvWrapper;
} REMFNDESC, *PREMFNDESC;
/** Pointer to a constant function descriptor. */
typedef const REMFNDESC *PCREMFNDESC;

/** @name Function descriptor flags.
 * @{ */
/** The return type is void. */
#define REMFNDESC_FLAGS_RET_VOID        0
/** The return type is a kind of integer passed in rax/eax. This includes pointers. */
#define REMFNDESC_FLAGS_RET_INT         1
/** The return type is a kind of floating point. */
#define REMFNDESC_FLAGS_RET_FLOAT       2
/** The return value is a struct. This type take a size. */
#define REMFNDESC_FLAGS_RET_STRUCT      3
/** The return type mask. */
#define REMFNDESC_FLAGS_RET_TYPE_MASK   7
/** The argument list contains one or more va_list arguments (i.e. problems). */
#define REMFNDESC_FLAGS_VALIST          RT_BIT(6)
/** The function has an ellipsis (i.e. a problem). */
#define REMFNDESC_FLAGS_ELLIPSIS        RT_BIT(7)
/** @} */

/**
 * Chunk of read-write-executable memory.
 */
typedef struct REMEXECMEM
{
    /** The number of bytes left. */
    struct REMEXECMEM  *pNext;
    /** The size of this chunk. */
    uint32_t            cb;
    /** The offset of the next code block. */
    uint32_t            off;
#if ARCH_BITS == 32
    uint32_t            padding;
#endif
} REMEXECMEM, *PREMEXECMEM;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifndef USE_REM_STUBS
/** Loader handle of the REM object/DLL. */
static RTLDRMOD g_ModREM2;
/** Pointer to the memory containing the loaded REM2 object/DLL. */
static void    *g_pvREM2;

/** Linux object export addresses.
 * These are references from the assembly wrapper code.
 * @{ */
static DECLCALLBACKPTR(int, pfnREMR3Init)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3Term)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3Reset)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3Step)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3BreakpointSet)(PVM, RTGCUINTPTR);
static DECLCALLBACKPTR(int, pfnREMR3BreakpointClear)(PVM, RTGCUINTPTR);
static DECLCALLBACKPTR(int, pfnREMR3EmulateInstruction)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3Run)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3State)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3StateBack)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3StateUpdate)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3A20Set)(PVM, bool);
static DECLCALLBACKPTR(void, pfnREMR3ReplayInvalidatedPages)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3ReplayHandlerNotifications)(PVM pVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyPhysRamRegister)(PVM, RTGCPHYS, RTUINT, void *, unsigned);
static DECLCALLBACKPTR(void, pfnREMR3NotifyPhysRamChunkRegister)(PVM, RTGCPHYS, RTUINT, RTHCUINTPTR, unsigned);
static DECLCALLBACKPTR(void, pfnREMR3NotifyPhysReserve)(PVM, RTGCPHYS, RTUINT);
static DECLCALLBACKPTR(void, pfnREMR3NotifyPhysRomRegister)(PVM, RTGCPHYS, RTUINT, void *, bool);
static DECLCALLBACKPTR(void, pfnREMR3NotifyHandlerPhysicalModify)(PVM, PGMPHYSHANDLERTYPE, RTGCPHYS, RTGCPHYS, RTGCPHYS, bool, bool);
static DECLCALLBACKPTR(void, pfnREMR3NotifyHandlerPhysicalRegister)(PVM, PGMPHYSHANDLERTYPE, RTGCPHYS, RTGCPHYS, bool);
static DECLCALLBACKPTR(void, pfnREMR3NotifyHandlerPhysicalDeregister)(PVM, PGMPHYSHANDLERTYPE, RTGCPHYS, RTGCPHYS, bool, bool);
static DECLCALLBACKPTR(void, pfnREMR3NotifyInterruptSet)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyInterruptClear)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyTimerPending)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyDmaPending)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyQueuePending)(PVM);
static DECLCALLBACKPTR(void, pfnREMR3NotifyFF)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3NotifyCodePageChanged)(PVM, RTGCPTR);
static DECLCALLBACKPTR(void, pfnREMR3NotifyPendingInterrupt)(PVM, uint8_t);
static DECLCALLBACKPTR(uint32_t, pfnREMR3QueryPendingInterrupt)(PVM);
static DECLCALLBACKPTR(int, pfnREMR3DisasEnableStepping)(PVM, bool);
static DECLCALLBACKPTR(bool, pfnREMR3IsPageAccessHandled)(PVM, RTGCPHYS);
/** @} */

/** Export and import parameter descriptors.
 * @{
 */
/* Common args. */
static const REMPARMDESC g_aArgsSIZE_T[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t) }
};
static const REMPARMDESC g_aArgsPTR[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(void *) }
};
static const REMPARMDESC g_aArgsVM[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM) }
};

/* REM args */
static const REMPARMDESC g_aArgsBreakpoint[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCUINTPTR), NULL }
};
static const REMPARMDESC g_aArgsA20Set[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsNotifyPhysRamRegister[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL }
};
static const REMPARMDESC g_aArgsNotifyPhysRamChunkRegister[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTHCUINTPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL }
};
static const REMPARMDESC g_aArgsNotifyPhysReserve[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL }
};
static const REMPARMDESC g_aArgsNotifyPhysRomRegister[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsNotifyHandlerPhysicalModify[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PGMPHYSHANDLERTYPE), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsNotifyHandlerPhysicalRegister[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PGMPHYSHANDLERTYPE), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsNotifyHandlerPhysicalDeregister[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PGMPHYSHANDLERTYPE), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsNotifyCodePageChanged[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCUINTPTR), NULL }
};
static const REMPARMDESC g_aArgsNotifyPendingInterrupt[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL }
};
static const REMPARMDESC g_aArgsDisasEnableStepping[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsIsPageAccessHandled[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL }
};


/* VMM args */
static const REMPARMDESC g_aArgsCPUMGetGuestCpl[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PCPUMCTXCORE), NULL },
};

static const REMPARMDESC g_aArgsCPUMGetGuestCpuId[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL }
};
static const REMPARMDESC g_aArgsCPUMQueryGuestCtxPtr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PCPUMCTX *), NULL }
};
static const REMPARMDESC g_aArgsCSAMR3MonitorPage[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTGCPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(CSAMTAG), NULL }
};

static const REMPARMDESC g_aArgsCSAMR3RecordCallAddress[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTGCPTR), NULL }
};

#if defined(VBOX_WITH_DEBUGGER) && !(defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64)) /* the callbacks are problematic */
static const REMPARMDESC g_aArgsDBGCRegisterCommands[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PCDBGCCMD), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL }
};
#endif
static const REMPARMDESC g_aArgsDBGFR3DisasInstrEx[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTSEL), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTGCPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL }
};
static const REMPARMDESC g_aArgsDBGFR3Info[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PCDBGFINFOHLP), NULL }
};
static const REMPARMDESC g_aArgsDBGFR3SymbolByAddr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCUINTPTR), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCINTPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PDBGFSYMBOL), NULL }
};
static const REMPARMDESC g_aArgsDISInstr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINTPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(char *), NULL }
};
static const REMPARMDESC g_aArgsEMR3FatalError[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(int), NULL }
};
static const REMPARMDESC g_aArgsHWACCMR3CanExecuteGuest[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsIOMIOPortRead[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTIOPORT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsIOMIOPortWrite[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTIOPORT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsIOMMMIORead[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsIOMMMIOWrite[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsMMR3HeapAlloc[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(MMTAG), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsMMR3HeapAllocZ[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(MMTAG), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsPATMIsPatchGCAddr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL }
};
static const REMPARMDESC g_aArgsPATMR3QueryOpcode[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t *), NULL }
};
static const REMPARMDESC g_aArgsPATMR3QueryPatchMem[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL }
};
static const REMPARMDESC g_aArgsPDMApicGetBase[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint64_t *), NULL }
};
static const REMPARMDESC g_aArgsPDMApicGetTPR[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t *), NULL }
};
static const REMPARMDESC g_aArgsPDMApicSetBase[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint64_t), NULL }
};
static const REMPARMDESC g_aArgsPDMApicSetTPR[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL }
};
static const REMPARMDESC g_aArgsPDMGetInterrupt[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t *), NULL }
};
static const REMPARMDESC g_aArgsPDMIsaSetIrq[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL }
};
static const REMPARMDESC g_aArgsPGMGstGetPage[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint64_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTGCPHYS), NULL }
};
static const REMPARMDESC g_aArgsPGMInvalidatePage[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysGCPhys2HCPtr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTHCPTR), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysGCPtr2HCPtrByGstCR3[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTHCPTR), NULL }
};
static const REMPARMDESC g_aArgsPGM3PhysGrowRange[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysIsGCPhysValid[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysRead[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysReadGCPtr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsPGMPhysWrite[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsPGMChangeMode[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint64_t), NULL }
};
static const REMPARMDESC g_aArgsPGMFlushTLB[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(bool), NULL }
};
static const REMPARMDESC g_aArgsPGMR3PhysReadUxx[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL }
};
static const REMPARMDESC g_aArgsPGMR3PhysWriteU8[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL }
};
static const REMPARMDESC g_aArgsPGMR3PhysWriteU16[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint16_t), NULL }
};
static const REMPARMDESC g_aArgsPGMR3PhysWriteU32[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL }
};
static const REMPARMDESC g_aArgsPGMR3PhysWriteU64[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPHYS,     sizeof(RTGCPHYS), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint64_t), NULL }
};
static const REMPARMDESC g_aArgsSSMR3GetGCPtr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTGCPTR), NULL }
};
static const REMPARMDESC g_aArgsSSMR3GetMem[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsSSMR3GetU32[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t *), NULL }
};
static const REMPARMDESC g_aArgsSSMR3GetUInt[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTUINT), NULL }
};
static const REMPARMDESC g_aArgsSSMR3PutGCPtr[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCPTR), NULL }
};
static const REMPARMDESC g_aArgsSSMR3PutMem[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsSSMR3PutU32[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t), NULL },
};
static const REMPARMDESC g_aArgsSSMR3PutUInt[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(RTUINT), NULL },
};

static const REMPARMDESC g_aArgsSSMIntCallback[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE), NULL },
};
static REMFNDESC g_SSMIntCallback =
{
    "SSMIntCallback", NULL, &g_aArgsSSMIntCallback[0], ELEMENTS(g_aArgsSSMIntCallback), REMFNDESC_FLAGS_RET_INT, sizeof(int),  NULL
};

static const REMPARMDESC g_aArgsSSMIntLoadExecCallback[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM),                NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PSSMHANDLE),         NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t),           NULL },
};
static REMFNDESC g_SSMIntLoadExecCallback =
{
    "SSMIntLoadExecCallback", NULL, &g_aArgsSSMIntLoadExecCallback[0], ELEMENTS(g_aArgsSSMIntLoadExecCallback), REMFNDESC_FLAGS_RET_INT, sizeof(int),  NULL
};
static const REMPARMDESC g_aArgsSSMR3RegisterInternal[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM),                NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *),       NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t),           NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint32_t),           NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t),             NULL },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTSAVEPREP),  &g_SSMIntCallback },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTSAVEEXEC),  &g_SSMIntCallback },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTSAVEDONE),  &g_SSMIntCallback },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTLOADPREP),  &g_SSMIntCallback },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTLOADEXEC),  &g_SSMIntLoadExecCallback },
    { REMPARMDESC_FLAGS_PFN,        sizeof(PFNSSMINTLOADDONE),  &g_SSMIntCallback },
};

static const REMPARMDESC g_aArgsSTAMR3Register[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(STAMTYPE), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(STAMVISIBILITY), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(STAMUNIT), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL }
};
static const REMPARMDESC g_aArgsTRPMAssertTrap[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(TRPMEVENT), NULL }
};
static const REMPARMDESC g_aArgsTRPMQueryTrap[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(uint8_t *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(TRPMEVENT *), NULL }
};
static const REMPARMDESC g_aArgsTRPMSetErrorCode[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCUINT), NULL }
};
static const REMPARMDESC g_aArgsTRPMSetFaultAddress[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_GCPTR,      sizeof(RTGCUINT), NULL }
};
static const REMPARMDESC g_aArgsVMR3ReqCall[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVM), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(PVMREQ *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_ELLIPSIS,   0 }
};
static const REMPARMDESC g_aArgsVMR3ReqFree[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PVMREQ), NULL }
};


/* IPRT args */
static const REMPARMDESC g_aArgsAssertMsg1[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL }
};
static const REMPARMDESC g_aArgsAssertMsg2[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_ELLIPSIS,   0 }
};
static const REMPARMDESC g_aArgsRTLogFlags[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTLOGGER), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL }
};
static const REMPARMDESC g_aArgsRTLogFlush[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTLOGGER), NULL }
};
static const REMPARMDESC g_aArgsRTLogLoggerEx[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTLOGGER), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_ELLIPSIS,   0 }
};
static const REMPARMDESC g_aArgsRTLogLoggerExV[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(PRTLOGGER), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_VALIST,     0 }
};
static const REMPARMDESC g_aArgsRTLogPrintf[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_ELLIPSIS,   0 }
};
static const REMPARMDESC g_aArgsRTMemProtect[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(unsigned), NULL }
};
static const REMPARMDESC g_aArgsRTStrPrintf[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_ELLIPSIS,   0 }
};
static const REMPARMDESC g_aArgsRTStrPrintfV[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(char *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const char *), NULL },
    { REMPARMDESC_FLAGS_VALIST,     0 }
};


/* CRT args */
static const REMPARMDESC g_aArgsmemcpy[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(const  void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};
static const REMPARMDESC g_aArgsmemset[] =
{
    { REMPARMDESC_FLAGS_INT,        sizeof(void *), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(int), NULL },
    { REMPARMDESC_FLAGS_INT,        sizeof(size_t), NULL }
};


/** @} */

/**
 * Descriptors for the exported functions.
 */
static const REMFNDESC g_aExports[] =
{  /* pszName,                                  (void *)pv,                                         pParams,                                    cParams,                                            fFlags,                     cb,             pvWrapper. */
    { "REMR3Init",                              (void *)&pfnREMR3Init,                              &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3Term",                              (void *)&pfnREMR3Term,                              &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3Reset",                             (void *)&pfnREMR3Reset,                             &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3Step",                              (void *)&pfnREMR3Step,                              &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3BreakpointSet",                     (void *)&pfnREMR3BreakpointSet,                     &g_aArgsBreakpoint[0],                      ELEMENTS(g_aArgsBreakpoint),                        REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3BreakpointClear",                   (void *)&pfnREMR3BreakpointClear,                   &g_aArgsBreakpoint[0],                      ELEMENTS(g_aArgsBreakpoint),                        REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3EmulateInstruction",                (void *)&pfnREMR3EmulateInstruction,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3Run",                               (void *)&pfnREMR3Run,                               &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3State",                             (void *)&pfnREMR3State,                             &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3StateBack",                         (void *)&pfnREMR3StateBack,                         &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3StateUpdate",                       (void *)&pfnREMR3StateUpdate,                       &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3A20Set",                            (void *)&pfnREMR3A20Set,                            &g_aArgsA20Set[0],                          ELEMENTS(g_aArgsA20Set),                            REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3ReplayInvalidatedPages",            (void *)&pfnREMR3ReplayInvalidatedPages,            &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3ReplayHandlerNotifications",        (void *)&pfnREMR3ReplayHandlerNotifications,        &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyPhysRamRegister",             (void *)&pfnREMR3NotifyPhysRamRegister,             &g_aArgsNotifyPhysRamRegister[0],           ELEMENTS(g_aArgsNotifyPhysRamRegister),             REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyPhysRamChunkRegister",        (void *)&pfnREMR3NotifyPhysRamChunkRegister,        &g_aArgsNotifyPhysRamChunkRegister[0],      ELEMENTS(g_aArgsNotifyPhysRamChunkRegister),        REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyPhysReserve",                 (void *)&pfnREMR3NotifyPhysReserve,                 &g_aArgsNotifyPhysReserve[0],               ELEMENTS(g_aArgsNotifyPhysReserve),                 REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyPhysRomRegister",             (void *)&pfnREMR3NotifyPhysRomRegister,             &g_aArgsNotifyPhysRomRegister[0],           ELEMENTS(g_aArgsNotifyPhysRomRegister),             REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyHandlerPhysicalModify",       (void *)&pfnREMR3NotifyHandlerPhysicalModify,       &g_aArgsNotifyHandlerPhysicalModify[0],     ELEMENTS(g_aArgsNotifyHandlerPhysicalModify),       REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyHandlerPhysicalRegister",     (void *)&pfnREMR3NotifyHandlerPhysicalRegister,     &g_aArgsNotifyHandlerPhysicalRegister[0],   ELEMENTS(g_aArgsNotifyHandlerPhysicalRegister),     REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyHandlerPhysicalDeregister",   (void *)&pfnREMR3NotifyHandlerPhysicalDeregister,   &g_aArgsNotifyHandlerPhysicalDeregister[0], ELEMENTS(g_aArgsNotifyHandlerPhysicalDeregister),   REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyInterruptSet",                (void *)&pfnREMR3NotifyInterruptSet,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyInterruptClear",              (void *)&pfnREMR3NotifyInterruptClear,              &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyTimerPending",                (void *)&pfnREMR3NotifyTimerPending,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyDmaPending",                  (void *)&pfnREMR3NotifyDmaPending,                  &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyQueuePending",                (void *)&pfnREMR3NotifyQueuePending,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyFF",                          (void *)&pfnREMR3NotifyFF,                          &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3NotifyCodePageChanged",             (void *)&pfnREMR3NotifyCodePageChanged,             &g_aArgsNotifyCodePageChanged[0],           ELEMENTS(g_aArgsNotifyCodePageChanged),             REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3NotifyPendingInterrupt",            (void *)&pfnREMR3NotifyPendingInterrupt,            &g_aArgsNotifyPendingInterrupt[0],          ELEMENTS(g_aArgsNotifyPendingInterrupt),            REMFNDESC_FLAGS_RET_VOID,   0,              NULL },
    { "REMR3QueryPendingInterrupt",             (void *)&pfnREMR3QueryPendingInterrupt,             &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t), NULL },
    { "REMR3DisasEnableStepping",               (void *)&pfnREMR3DisasEnableStepping,               &g_aArgsDisasEnableStepping[0],             ELEMENTS(g_aArgsDisasEnableStepping),               REMFNDESC_FLAGS_RET_INT,    sizeof(int),    NULL },
    { "REMR3IsPageAccessHandled",               (void *)&pfnREMR3IsPageAccessHandled,               &g_aArgsIsPageAccessHandled[0],             ELEMENTS(g_aArgsIsPageAccessHandled),               REMFNDESC_FLAGS_RET_INT,    sizeof(bool),   NULL }
};


/**
 * Descriptors for the functions imported from VBoxVMM.
 */
static REMFNDESC g_aVMMImports[] =
{
    { "CPUMAreHiddenSelRegsValid",              (void *)(uintptr_t)&CPUMAreHiddenSelRegsValid,      &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "CPUMGetAndClearChangedFlagsREM",         (void *)(uintptr_t)&CPUMGetAndClearChangedFlagsREM, &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(unsigned),   NULL },
    { "CPUMGetGuestCPL",                        (void *)(uintptr_t)&CPUMGetGuestCPL,                &g_aArgsCPUMGetGuestCpl[0],                 ELEMENTS(g_aArgsCPUMGetGuestCpl),                   REMFNDESC_FLAGS_RET_INT,    sizeof(unsigned),   NULL },
    { "CPUMGetGuestCpuId",                      (void *)(uintptr_t)&CPUMGetGuestCpuId,              &g_aArgsCPUMGetGuestCpuId[0],               ELEMENTS(g_aArgsCPUMGetGuestCpuId),                 REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "CPUMGetGuestEAX",                        (void *)(uintptr_t)&CPUMGetGuestEAX,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestEBP",                        (void *)(uintptr_t)&CPUMGetGuestEBP,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestEBX",                        (void *)(uintptr_t)&CPUMGetGuestEBX,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestECX",                        (void *)(uintptr_t)&CPUMGetGuestECX,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestEDI",                        (void *)(uintptr_t)&CPUMGetGuestEDI,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestEDX",                        (void *)(uintptr_t)&CPUMGetGuestEDX,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestEIP",                        (void *)(uintptr_t)&CPUMGetGuestEIP,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestESI",                        (void *)(uintptr_t)&CPUMGetGuestESI,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMGetGuestESP",                        (void *)(uintptr_t)&CPUMGetGuestESP,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "CPUMQueryGuestCtxPtr",                   (void *)(uintptr_t)&CPUMQueryGuestCtxPtr,           &g_aArgsCPUMQueryGuestCtxPtr[0],            ELEMENTS(g_aArgsCPUMQueryGuestCtxPtr),              REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "CSAMR3MonitorPage",                      (void *)(uintptr_t)&CSAMR3MonitorPage,              &g_aArgsCSAMR3MonitorPage[0],               ELEMENTS(g_aArgsCSAMR3MonitorPage),                 REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "CSAMR3RecordCallAddress",                (void *)(uintptr_t)&CSAMR3RecordCallAddress,        &g_aArgsCSAMR3RecordCallAddress[0],         ELEMENTS(g_aArgsCSAMR3RecordCallAddress),           REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
#if defined(VBOX_WITH_DEBUGGER) && !(defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64)) /* the callbacks are problematic */
    { "DBGCRegisterCommands",                   (void *)(uintptr_t)&DBGCRegisterCommands,           &g_aArgsDBGCRegisterCommands[0],            ELEMENTS(g_aArgsDBGCRegisterCommands),              REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
#endif
    { "DBGFR3DisasInstrEx",                     (void *)(uintptr_t)&DBGFR3DisasInstrEx,             &g_aArgsDBGFR3DisasInstrEx[0],              ELEMENTS(g_aArgsDBGFR3DisasInstrEx),                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "DBGFR3Info",                             (void *)(uintptr_t)&DBGFR3Info,                     &g_aArgsDBGFR3Info[0],                      ELEMENTS(g_aArgsDBGFR3Info),                        REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "DBGFR3InfoLogRelHlp",                    (void *)(uintptr_t)&DBGFR3InfoLogRelHlp,            NULL,                                       0,                                                  REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "DBGFR3SymbolByAddr",                     (void *)(uintptr_t)&DBGFR3SymbolByAddr,             &g_aArgsDBGFR3SymbolByAddr[0],              ELEMENTS(g_aArgsDBGFR3SymbolByAddr),                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "DISInstr",                               (void *)(uintptr_t)&DISInstr,                       &g_aArgsDISInstr[0],                        ELEMENTS(g_aArgsDISInstr),                          REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "EMR3FatalError",                         (void *)(uintptr_t)&EMR3FatalError,                 &g_aArgsEMR3FatalError[0],                  ELEMENTS(g_aArgsEMR3FatalError),                    REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "HWACCMR3CanExecuteGuest",                (void *)(uintptr_t)&HWACCMR3CanExecuteGuest,        &g_aArgsHWACCMR3CanExecuteGuest[0],         ELEMENTS(g_aArgsHWACCMR3CanExecuteGuest),           REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "IOMIOPortRead",                          (void *)(uintptr_t)&IOMIOPortRead,                  &g_aArgsIOMIOPortRead[0],                   ELEMENTS(g_aArgsIOMIOPortRead),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "IOMIOPortWrite",                         (void *)(uintptr_t)&IOMIOPortWrite,                 &g_aArgsIOMIOPortWrite[0],                  ELEMENTS(g_aArgsIOMIOPortWrite),                    REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "IOMMMIORead",                            (void *)(uintptr_t)&IOMMMIORead,                    &g_aArgsIOMMMIORead[0],                     ELEMENTS(g_aArgsIOMMMIORead),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "IOMMMIOWrite",                           (void *)(uintptr_t)&IOMMMIOWrite,                   &g_aArgsIOMMMIOWrite[0],                    ELEMENTS(g_aArgsIOMMMIOWrite),                      REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "MMR3HeapAlloc",                          (void *)(uintptr_t)&MMR3HeapAlloc,                  &g_aArgsMMR3HeapAlloc[0],                   ELEMENTS(g_aArgsMMR3HeapAlloc),                     REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "MMR3HeapAllocZ",                         (void *)(uintptr_t)&MMR3HeapAllocZ,                 &g_aArgsMMR3HeapAllocZ[0],                  ELEMENTS(g_aArgsMMR3HeapAllocZ),                    REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "MMR3PhysGetRamSize",                     (void *)(uintptr_t)&MMR3PhysGetRamSize,             &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint64_t),   NULL },
    { "PATMIsPatchGCAddr",                      (void *)(uintptr_t)&PATMIsPatchGCAddr,              &g_aArgsPATMIsPatchGCAddr[0],               ELEMENTS(g_aArgsPATMIsPatchGCAddr),                 REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "PATMR3QueryOpcode",                      (void *)(uintptr_t)&PATMR3QueryOpcode,              &g_aArgsPATMR3QueryOpcode[0],               ELEMENTS(g_aArgsPATMR3QueryOpcode),                 REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PATMR3QueryPatchMemGC",                  (void *)(uintptr_t)&PATMR3QueryPatchMemGC,          &g_aArgsPATMR3QueryPatchMem[0],             ELEMENTS(g_aArgsPATMR3QueryPatchMem),               REMFNDESC_FLAGS_RET_INT,    sizeof(RTGCPTR),    NULL },
    { "PATMR3QueryPatchMemHC",                  (void *)(uintptr_t)&PATMR3QueryPatchMemHC,          &g_aArgsPATMR3QueryPatchMem[0],             ELEMENTS(g_aArgsPATMR3QueryPatchMem),               REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "PDMApicGetBase",                         (void *)(uintptr_t)&PDMApicGetBase,                 &g_aArgsPDMApicGetBase[0],                  ELEMENTS(g_aArgsPDMApicGetBase),                    REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PDMApicGetTPR",                          (void *)(uintptr_t)&PDMApicGetTPR,                  &g_aArgsPDMApicGetTPR[0],                   ELEMENTS(g_aArgsPDMApicGetTPR),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PDMApicSetBase",                         (void *)(uintptr_t)&PDMApicSetBase,                 &g_aArgsPDMApicSetBase[0],                  ELEMENTS(g_aArgsPDMApicSetBase),                    REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PDMApicSetTPR",                          (void *)(uintptr_t)&PDMApicSetTPR,                  &g_aArgsPDMApicSetTPR[0],                   ELEMENTS(g_aArgsPDMApicSetTPR),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PDMR3DmaRun",                            (void *)(uintptr_t)&PDMR3DmaRun,                    &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "PDMGetInterrupt",                        (void *)(uintptr_t)&PDMGetInterrupt,                &g_aArgsPDMGetInterrupt[0],                 ELEMENTS(g_aArgsPDMGetInterrupt),                   REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PDMIsaSetIrq",                           (void *)(uintptr_t)&PDMIsaSetIrq,                   &g_aArgsPDMIsaSetIrq[0],                    ELEMENTS(g_aArgsPDMIsaSetIrq),                      REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMGstGetPage",                          (void *)(uintptr_t)&PGMGstGetPage,                  &g_aArgsPGMGstGetPage[0],                   ELEMENTS(g_aArgsPGMGstGetPage),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMInvalidatePage",                      (void *)(uintptr_t)&PGMInvalidatePage,              &g_aArgsPGMInvalidatePage[0],               ELEMENTS(g_aArgsPGMInvalidatePage),                 REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysGCPhys2HCPtr",                    (void *)(uintptr_t)&PGMPhysGCPhys2HCPtr,            &g_aArgsPGMPhysGCPhys2HCPtr[0],             ELEMENTS(g_aArgsPGMPhysGCPhys2HCPtr),               REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysGCPtr2HCPtrByGstCR3",             (void *)(uintptr_t)&PGMPhysGCPtr2HCPtrByGstCR3,     &g_aArgsPGMPhysGCPtr2HCPtrByGstCR3[0],      ELEMENTS(g_aArgsPGMPhysGCPtr2HCPtrByGstCR3),        REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGM3PhysGrowRange",                      (void *)(uintptr_t)&PGM3PhysGrowRange,              &g_aArgsPGM3PhysGrowRange[0],               ELEMENTS(g_aArgsPGM3PhysGrowRange),                 REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysIsGCPhysValid",                   (void *)(uintptr_t)&PGMPhysIsGCPhysValid,           &g_aArgsPGMPhysIsGCPhysValid[0],            ELEMENTS(g_aArgsPGMPhysIsGCPhysValid),              REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "PGMPhysIsA20Enabled",                    (void *)(uintptr_t)&PGMPhysIsA20Enabled,            &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "PGMPhysRead",                            (void *)(uintptr_t)&PGMPhysRead,                    &g_aArgsPGMPhysRead[0],                     ELEMENTS(g_aArgsPGMPhysRead),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysReadGCPtr",                       (void *)(uintptr_t)&PGMPhysReadGCPtr,               &g_aArgsPGMPhysReadGCPtr[0],                ELEMENTS(g_aArgsPGMPhysReadGCPtr),                  REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysReadGCPtr",                       (void *)(uintptr_t)&PGMPhysReadGCPtr,               &g_aArgsPGMPhysReadGCPtr[0],                ELEMENTS(g_aArgsPGMPhysReadGCPtr),                  REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMPhysWrite",                           (void *)(uintptr_t)&PGMPhysWrite,                   &g_aArgsPGMPhysWrite[0],                    ELEMENTS(g_aArgsPGMPhysWrite),                      REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "PGMChangeMode",                          (void *)(uintptr_t)&PGMChangeMode,                  &g_aArgsPGMChangeMode[0],                   ELEMENTS(g_aArgsPGMChangeMode),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMFlushTLB",                            (void *)(uintptr_t)&PGMFlushTLB,                    &g_aArgsPGMFlushTLB[0],                     ELEMENTS(g_aArgsPGMFlushTLB),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "PGMR3PhysReadByte",                      (void *)(uintptr_t)&PGMR3PhysReadByte,              &g_aArgsPGMR3PhysReadUxx[0],                ELEMENTS(g_aArgsPGMR3PhysReadUxx),                  REMFNDESC_FLAGS_RET_INT,    sizeof(uint8_t),    NULL },
    { "PGMR3PhysReadDword",                     (void *)(uintptr_t)&PGMR3PhysReadDword,             &g_aArgsPGMR3PhysReadUxx[0],                ELEMENTS(g_aArgsPGMR3PhysReadUxx),                  REMFNDESC_FLAGS_RET_INT,    sizeof(uint32_t),   NULL },
    { "PGMR3PhysReadWord",                      (void *)(uintptr_t)&PGMR3PhysReadWord,              &g_aArgsPGMR3PhysReadUxx[0],                ELEMENTS(g_aArgsPGMR3PhysReadUxx),                  REMFNDESC_FLAGS_RET_INT,    sizeof(uint16_t),   NULL },
    { "PGMR3PhysWriteByte",                     (void *)(uintptr_t)&PGMR3PhysWriteByte,             &g_aArgsPGMR3PhysWriteU8[0],                ELEMENTS(g_aArgsPGMR3PhysWriteU8),                  REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "PGMR3PhysWriteDword",                    (void *)(uintptr_t)&PGMR3PhysWriteDword,            &g_aArgsPGMR3PhysWriteU32[0],               ELEMENTS(g_aArgsPGMR3PhysWriteU32),                 REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "PGMR3PhysWriteWord",                     (void *)(uintptr_t)&PGMR3PhysWriteWord,             &g_aArgsPGMR3PhysWriteU16[0],               ELEMENTS(g_aArgsPGMR3PhysWriteU16),                 REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "SSMR3GetGCPtr",                          (void *)(uintptr_t)&SSMR3GetGCPtr,                  &g_aArgsSSMR3GetGCPtr[0],                   ELEMENTS(g_aArgsSSMR3GetGCPtr),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3GetMem",                            (void *)(uintptr_t)&SSMR3GetMem,                    &g_aArgsSSMR3GetMem[0],                     ELEMENTS(g_aArgsSSMR3GetMem),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3GetU32",                            (void *)(uintptr_t)&SSMR3GetU32,                    &g_aArgsSSMR3GetU32[0],                     ELEMENTS(g_aArgsSSMR3GetU32),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3GetUInt",                           (void *)(uintptr_t)&SSMR3GetUInt,                   &g_aArgsSSMR3GetUInt[0],                    ELEMENTS(g_aArgsSSMR3GetUInt),                      REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3PutGCPtr",                          (void *)(uintptr_t)&SSMR3PutGCPtr,                  &g_aArgsSSMR3PutGCPtr[0],                   ELEMENTS(g_aArgsSSMR3PutGCPtr),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3PutMem",                            (void *)(uintptr_t)&SSMR3PutMem,                    &g_aArgsSSMR3PutMem[0],                     ELEMENTS(g_aArgsSSMR3PutMem),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3PutU32",                            (void *)(uintptr_t)&SSMR3PutU32,                    &g_aArgsSSMR3PutU32[0],                     ELEMENTS(g_aArgsSSMR3PutU32),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3PutUInt",                           (void *)(uintptr_t)&SSMR3PutUInt,                   &g_aArgsSSMR3PutUInt[0],                    ELEMENTS(g_aArgsSSMR3PutUInt),                      REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "SSMR3RegisterInternal",                  (void *)(uintptr_t)&SSMR3RegisterInternal,          &g_aArgsSSMR3RegisterInternal[0],           ELEMENTS(g_aArgsSSMR3RegisterInternal),             REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "STAMR3Register",                         (void *)(uintptr_t)&STAMR3Register,                 &g_aArgsSTAMR3Register[0],                  ELEMENTS(g_aArgsSTAMR3Register),                    REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TMCpuTickGet",                           (void *)(uintptr_t)&TMCpuTickGet,                   &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint64_t),   NULL },
    { "TMCpuTickPause",                         (void *)(uintptr_t)&TMCpuTickPause,                 &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TMCpuTickResume",                        (void *)(uintptr_t)&TMCpuTickResume,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TMTimerPoll",                            (void *)(uintptr_t)&TMTimerPoll,                    &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(uint64_t),   NULL },
    { "TMR3TimerQueuesDo",                      (void *)(uintptr_t)&TMR3TimerQueuesDo,              &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "TMVirtualPause",                         (void *)(uintptr_t)&TMVirtualPause,                 &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TMVirtualResume",                        (void *)(uintptr_t)&TMVirtualResume,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TRPMAssertTrap",                         (void *)(uintptr_t)&TRPMAssertTrap,                 &g_aArgsTRPMAssertTrap[0],                  ELEMENTS(g_aArgsTRPMAssertTrap),                    REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TRPMGetErrorCode",                       (void *)(uintptr_t)&TRPMGetErrorCode,               &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(RTGCUINT),   NULL },
    { "TRPMGetFaultAddress",                    (void *)(uintptr_t)&TRPMGetFaultAddress,            &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(RTGCUINTPTR),NULL },
    { "TRPMQueryTrap",                          (void *)(uintptr_t)&TRPMQueryTrap,                  &g_aArgsTRPMQueryTrap[0],                   ELEMENTS(g_aArgsTRPMQueryTrap),                     REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TRPMResetTrap",                          (void *)(uintptr_t)&TRPMResetTrap,                  &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "TRPMSetErrorCode",                       (void *)(uintptr_t)&TRPMSetErrorCode,               &g_aArgsTRPMSetErrorCode[0],                ELEMENTS(g_aArgsTRPMSetErrorCode),                  REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "TRPMSetFaultAddress",                    (void *)(uintptr_t)&TRPMSetFaultAddress,            &g_aArgsTRPMSetFaultAddress[0],             ELEMENTS(g_aArgsTRPMSetFaultAddress),               REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "VMMR3Lock",                              (void *)(uintptr_t)&VMMR3Lock,                      &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "VMMR3Unlock",                            (void *)(uintptr_t)&VMMR3Unlock,                    &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "VMR3ReqCall",                            (void *)(uintptr_t)&VMR3ReqCall,                    &g_aArgsVMR3ReqCall[0],                     ELEMENTS(g_aArgsVMR3ReqCall),                       REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "VMR3ReqFree",                            (void *)(uintptr_t)&VMR3ReqFree,                    &g_aArgsVMR3ReqFree[0],                     ELEMENTS(g_aArgsVMR3ReqFree),                       REMFNDESC_FLAGS_RET_INT | REMFNDESC_FLAGS_ELLIPSIS, sizeof(int), NULL },
//    { "",                        (void *)(uintptr_t)&,                &g_aArgsVM[0],                              ELEMENTS(g_aArgsVM),                                REMFNDESC_FLAGS_RET_INT,    sizeof(int),   NULL },
};


/**
 * Descriptors for the functions imported from VBoxRT.
 */
static REMFNDESC g_aRTImports[] =
{
    { "AssertMsg1",                             (void *)(uintptr_t)&AssertMsg1,                     &g_aArgsAssertMsg1[0],                      ELEMENTS(g_aArgsAssertMsg1),                        REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "AssertMsg2",                             (void *)(uintptr_t)&AssertMsg2,                     &g_aArgsAssertMsg2[0],                      ELEMENTS(g_aArgsAssertMsg2),                        REMFNDESC_FLAGS_RET_VOID | REMFNDESC_FLAGS_ELLIPSIS, 0, NULL },
    { "RTAssertDoBreakpoint",                   (void *)(uintptr_t)&RTAssertDoBreakpoint,           NULL,                                       0,                                                  REMFNDESC_FLAGS_RET_INT,    sizeof(bool),       NULL },
    { "RTLogDefaultInstance",                   (void *)(uintptr_t)&RTLogDefaultInstance,           NULL,                                       0,                                                  REMFNDESC_FLAGS_RET_INT,    sizeof(PRTLOGGER),  NULL },
    { "RTLogRelDefaultInstance",                (void *)(uintptr_t)&RTLogRelDefaultInstance,        NULL,                                       0,                                                  REMFNDESC_FLAGS_RET_INT,    sizeof(PRTLOGGER),  NULL },
    { "RTLogFlags",                             (void *)(uintptr_t)&RTLogFlags,                     &g_aArgsRTLogFlags[0],                      ELEMENTS(g_aArgsRTLogFlags),                        REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "RTLogFlush",                             (void *)(uintptr_t)&RTLogFlush,                     &g_aArgsRTLogFlush[0],                      ELEMENTS(g_aArgsRTLogFlush),                        REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "RTLogLoggerEx",                          (void *)(uintptr_t)&RTLogLoggerEx,                  &g_aArgsRTLogLoggerEx[0],                   ELEMENTS(g_aArgsRTLogLoggerEx),                     REMFNDESC_FLAGS_RET_VOID | REMFNDESC_FLAGS_ELLIPSIS, 0, NULL },
    { "RTLogLoggerExV",                         (void *)(uintptr_t)&RTLogLoggerExV,                 &g_aArgsRTLogLoggerExV[0],                  ELEMENTS(g_aArgsRTLogLoggerExV),                    REMFNDESC_FLAGS_RET_VOID | REMFNDESC_FLAGS_VALIST, 0, NULL },
    { "RTLogPrintf",                            (void *)(uintptr_t)&RTLogPrintf,                    &g_aArgsRTLogPrintf[0],                     ELEMENTS(g_aArgsRTLogPrintf),                       REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "RTMemAlloc",                             (void *)(uintptr_t)&RTMemAlloc,                     &g_aArgsSIZE_T[0],                          ELEMENTS(g_aArgsSIZE_T),                            REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "RTMemExecAlloc",                         (void *)(uintptr_t)&RTMemExecAlloc,                 &g_aArgsSIZE_T[0],                          ELEMENTS(g_aArgsSIZE_T),                            REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "RTMemExecFree",                          (void *)(uintptr_t)&RTMemExecFree,                  &g_aArgsPTR[0],                             ELEMENTS(g_aArgsPTR),                               REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "RTMemFree",                              (void *)(uintptr_t)&RTMemFree,                      &g_aArgsPTR[0],                             ELEMENTS(g_aArgsPTR),                               REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "RTMemPageAlloc",                         (void *)(uintptr_t)&RTMemPageAlloc,                 &g_aArgsSIZE_T[0],                          ELEMENTS(g_aArgsSIZE_T),                            REMFNDESC_FLAGS_RET_INT,    sizeof(void *),     NULL },
    { "RTMemPageFree",                          (void *)(uintptr_t)&RTMemPageFree,                  &g_aArgsPTR[0],                             ELEMENTS(g_aArgsPTR),                               REMFNDESC_FLAGS_RET_VOID,   0,                  NULL },
    { "RTMemProtect",                           (void *)(uintptr_t)&RTMemProtect,                   &g_aArgsRTMemProtect[0],                    ELEMENTS(g_aArgsRTMemProtect),                      REMFNDESC_FLAGS_RET_INT,    sizeof(int),        NULL },
    { "RTStrPrintf",                            (void *)(uintptr_t)&RTStrPrintf,                    &g_aArgsRTStrPrintf[0],                     ELEMENTS(g_aArgsRTStrPrintf),                       REMFNDESC_FLAGS_RET_INT | REMFNDESC_FLAGS_ELLIPSIS, sizeof(size_t), NULL },
    { "RTStrPrintfV",                           (void *)(uintptr_t)&RTStrPrintfV,                   &g_aArgsRTStrPrintfV[0],                    ELEMENTS(g_aArgsRTStrPrintfV),                      REMFNDESC_FLAGS_RET_INT | REMFNDESC_FLAGS_VALIST, sizeof(size_t), NULL },
    { "RTThreadNativeSelf",                     (void *)(uintptr_t)&RTThreadNativeSelf,             NULL,                                       0,                                                  REMFNDESC_FLAGS_RET_INT, sizeof(RTNATIVETHREAD), NULL },
};


/**
 * Descriptors for the functions imported from VBoxRT.
 */
static REMFNDESC g_aCRTImports[] =
{
    { "memcpy",                                (void *)(uintptr_t)&memcpy,                          &g_aArgsmemcpy[0],                          ELEMENTS(g_aArgsmemcpy),                            REMFNDESC_FLAGS_RET_INT,    sizeof(void *), NULL },
    { "memset",                                (void *)(uintptr_t)&memset,                          &g_aArgsmemset[0],                          ELEMENTS(g_aArgsmemset),                            REMFNDESC_FLAGS_RET_INT,    sizeof(void *), NULL }
/*
floor               floor
memcpy              memcpy
sqrt                sqrt
sqrtf               sqrtf
*/
};


# if defined(USE_REM_CALLING_CONVENTION_GLUE) || defined(USE_REM_IMPORT_JUMP_GLUE)
/** LIFO of read-write-executable memory chunks used for wrappers. */
static PREMEXECMEM g_pExecMemHead;
# endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int remGenerateExportGlue(PRTUINTPTR pValue, PCREMFNDESC pDesc);

# ifdef USE_REM_CALLING_CONVENTION_GLUE
DECLASM(int) WrapGCC2MSC0Int(void);  DECLASM(int) WrapGCC2MSC0Int_EndProc(void);
DECLASM(int) WrapGCC2MSC1Int(void);  DECLASM(int) WrapGCC2MSC1Int_EndProc(void);
DECLASM(int) WrapGCC2MSC2Int(void);  DECLASM(int) WrapGCC2MSC2Int_EndProc(void);
DECLASM(int) WrapGCC2MSC3Int(void);  DECLASM(int) WrapGCC2MSC3Int_EndProc(void);
DECLASM(int) WrapGCC2MSC4Int(void);  DECLASM(int) WrapGCC2MSC4Int_EndProc(void);
DECLASM(int) WrapGCC2MSC5Int(void);  DECLASM(int) WrapGCC2MSC5Int_EndProc(void);
DECLASM(int) WrapGCC2MSC6Int(void);  DECLASM(int) WrapGCC2MSC6Int_EndProc(void);
DECLASM(int) WrapGCC2MSC7Int(void);  DECLASM(int) WrapGCC2MSC7Int_EndProc(void);
DECLASM(int) WrapGCC2MSC8Int(void);  DECLASM(int) WrapGCC2MSC8Int_EndProc(void);
DECLASM(int) WrapGCC2MSC9Int(void);  DECLASM(int) WrapGCC2MSC9Int_EndProc(void);
DECLASM(int) WrapGCC2MSC10Int(void); DECLASM(int) WrapGCC2MSC10Int_EndProc(void);
DECLASM(int) WrapGCC2MSC11Int(void); DECLASM(int) WrapGCC2MSC11Int_EndProc(void);
DECLASM(int) WrapGCC2MSC12Int(void); DECLASM(int) WrapGCC2MSC12Int_EndProc(void);
DECLASM(int) WrapGCC2MSCVariadictInt(void); DECLASM(int) WrapGCC2MSCVariadictInt_EndProc(void);
DECLASM(int) WrapGCC2MSC_SSMR3RegisterInternal(void); DECLASM(int) WrapGCC2MSC_SSMR3RegisterInternal_EndProc(void);

DECLASM(int) WrapMSC2GCC0Int(void);  DECLASM(int) WrapMSC2GCC0Int_EndProc(void);
DECLASM(int) WrapMSC2GCC1Int(void);  DECLASM(int) WrapMSC2GCC1Int_EndProc(void);
DECLASM(int) WrapMSC2GCC2Int(void);  DECLASM(int) WrapMSC2GCC2Int_EndProc(void);
DECLASM(int) WrapMSC2GCC3Int(void);  DECLASM(int) WrapMSC2GCC3Int_EndProc(void);
DECLASM(int) WrapMSC2GCC4Int(void);  DECLASM(int) WrapMSC2GCC4Int_EndProc(void);
DECLASM(int) WrapMSC2GCC5Int(void);  DECLASM(int) WrapMSC2GCC5Int_EndProc(void);
DECLASM(int) WrapMSC2GCC6Int(void);  DECLASM(int) WrapMSC2GCC6Int_EndProc(void);
DECLASM(int) WrapMSC2GCC7Int(void);  DECLASM(int) WrapMSC2GCC7Int_EndProc(void);
DECLASM(int) WrapMSC2GCC8Int(void);  DECLASM(int) WrapMSC2GCC8Int_EndProc(void);
DECLASM(int) WrapMSC2GCC9Int(void);  DECLASM(int) WrapMSC2GCC9Int_EndProc(void);
# endif


# if defined(USE_REM_CALLING_CONVENTION_GLUE) || defined(USE_REM_IMPORT_JUMP_GLUE)
/**
 * Allocates a block of memory for glue code.
 *
 * The returned memory is padded with INT3s.
 *
 * @returns Pointer to the allocated memory.
 * @param   The amount of memory to allocate.
 */
static void *remAllocGlue(size_t cb)
{
    PREMEXECMEM pCur = g_pExecMemHead;
    uint32_t cbAligned = (uint32_t)RT_ALIGN_32(cb, 32);
    while (pCur)
    {
        if (pCur->cb - pCur->off >= cbAligned)
        {
            void *pv = (uint8_t *)pCur + pCur->off;
            pCur->off += cbAligned;
            return memset(pv, 0xcc, cbAligned);
        }
        pCur = pCur->pNext;
    }

    /* add a new chunk */
    AssertReturn(_64K - RT_ALIGN_Z(sizeof(*pCur), 32) > cbAligned, NULL);
    pCur = (PREMEXECMEM)RTMemExecAlloc(_64K);
    AssertReturn(pCur, NULL);
    pCur->cb = _64K;
    pCur->off = RT_ALIGN_32(sizeof(*pCur), 32) + cbAligned;
    pCur->pNext = g_pExecMemHead;
    g_pExecMemHead = pCur;
    return memset((uint8_t *)pCur + RT_ALIGN_Z(sizeof(*pCur), 32), 0xcc, cbAligned);
}
# endif /* USE_REM_CALLING_CONVENTION_GLUE || USE_REM_IMPORT_JUMP_GLUE */


# ifdef USE_REM_CALLING_CONVENTION_GLUE
/**
 * Checks if a function is all straight forward integers.
 *
 * @returns True if it's simple, false if it's bothersome.
 * @param   pDesc       The function descriptor.
 */
static bool remIsFunctionAllInts(PCREMFNDESC pDesc)
{
    if (    (   (pDesc->fFlags & REMFNDESC_FLAGS_RET_TYPE_MASK) != REMFNDESC_FLAGS_RET_INT
             || pDesc->cbReturn > sizeof(uint64_t))
        &&  (pDesc->fFlags & REMFNDESC_FLAGS_RET_TYPE_MASK) != REMFNDESC_FLAGS_RET_VOID)
        return false;
    unsigned i = pDesc->cParams;
    while (i-- > 0)
        switch (pDesc->paParams[i].fFlags & REMPARMDESC_FLAGS_TYPE_MASK)
        {
            case REMPARMDESC_FLAGS_INT:
            case REMPARMDESC_FLAGS_GCPTR:
            case REMPARMDESC_FLAGS_GCPHYS:
            case REMPARMDESC_FLAGS_HCPHYS:
                break;

            default:
                AssertReleaseMsgFailed(("Invalid param flags %#x for #%d of %s!\n", pDesc->paParams[i].fFlags, i, pDesc->pszName));
            case REMPARMDESC_FLAGS_VALIST:
            case REMPARMDESC_FLAGS_ELLIPSIS:
            case REMPARMDESC_FLAGS_FLOAT:
            case REMPARMDESC_FLAGS_STRUCT:
            case REMPARMDESC_FLAGS_PFN:
                return false;
        }
    return true;
}


/**
 * Checks if the function has an ellipsis (...) argument.
 *
 * @returns true if it has an ellipsis, otherwise false.
 * @param   pDesc       The function descriptor.
 */
static bool remHasFunctionEllipsis(PCREMFNDESC pDesc)
{
    unsigned i = pDesc->cParams;
    while (i-- > 0)
        if ((pDesc->paParams[i].fFlags & REMPARMDESC_FLAGS_TYPE_MASK) == REMPARMDESC_FLAGS_ELLIPSIS)
            return true;
    return false;
}


/**
 * Checks if the function uses floating point (FP) arguments or return value.
 *
 * @returns true if it uses floating point, otherwise false.
 * @param   pDesc       The function descriptor.
 */
static bool remIsFunctionUsingFP(PCREMFNDESC pDesc)
{
    if ((pDesc->fFlags & REMFNDESC_FLAGS_RET_TYPE_MASK) == REMFNDESC_FLAGS_RET_FLOAT)
        return true;
    unsigned i = pDesc->cParams;
    while (i-- > 0)
        if ((pDesc->paParams[i].fFlags & REMPARMDESC_FLAGS_TYPE_MASK) == REMPARMDESC_FLAGS_FLOAT)
            return true;
    return false;
}


/** @name The export and import fixups.
 * @{ */
#define REM_FIXUP_32_REAL_STUFF    UINT32_C(0xdeadbeef)
#define REM_FIXUP_64_REAL_STUFF    UINT64_C(0xdeadf00df00ddead)
#define REM_FIXUP_64_DESC          UINT64_C(0xdead00010001dead)
#define REM_FIXUP_64_LOG_ENTRY     UINT64_C(0xdead00020002dead)
#define REM_FIXUP_64_LOG_EXIT      UINT64_C(0xdead00030003dead)
#define REM_FIXUP_64_WRAP_GCC_CB   UINT64_C(0xdead00040004dead)
/** @} */


/**
 * Entry logger function.
 *
 * @param   pDesc       The description.
 */
DECLASM(void) remLogEntry(PCREMFNDESC pDesc)
{
    RTPrintf("calling %s\n", pDesc->pszName);
}


/**
 * Exit logger function.
 *
 * @param   pDesc       The description.
 * @param   pvRet       The return code.
 */
DECLASM(void) remLogExit(PCREMFNDESC pDesc, void *pvRet)
{
    RTPrintf("returning %p from %s\n", pvRet, pDesc->pszName);
}


/**
 * Creates a wrapper for the specified callback function at run time.
 *
 * @param   pDesc       The function descriptor.
 * @param   pValue      Upon entry *pValue contains the address of the function to be wrapped.
 *                      Upon return *pValue contains the address of the wrapper glue function.
 * @param   iParam      The parameter index in the function descriptor (0 based).
 *                      If UINT32_MAX pDesc is the descriptor for *pValue.
 */
DECLASM(void) remWrapGCCCallback(PCREMFNDESC pDesc, PRTUINTPTR pValue, uint32_t iParam)
{
    AssertPtr(pDesc);
    AssertPtr(pValue);

    /*
     * Simple?
     */
    if (!*pValue)
        return;

    /*
     * Locate the right function descriptor.
     */
    if (iParam != UINT32_MAX)
    {
        AssertRelease(iParam < pDesc->cParams);
        pDesc = (PCREMFNDESC)pDesc->paParams[iParam].pvExtra;
        AssertPtr(pDesc);
    }

    /*
     * When we get serious, here is where to insert the hash table lookup.
     */

    /*
     * Create a new glue patch.
     */
#ifdef RT_OS_WINDOWS
    int rc = remGenerateExportGlue(pValue, pDesc);
#else
#error "port me"
#endif
    AssertReleaseRC(rc);

    /*
     * Add it to the hash (later)
     */
}


/**
 * Fixes export glue.
 *
 * @param   pvGlue      The glue code.
 * @param   cb          The size of the glue code.
 * @param   pvExport    The address of the export we're wrapping.
 * @param   pDesc       The export descriptor.
 */
static void remGenerateExportGlueFixup(void *pvGlue, size_t cb, uintptr_t uExport, PCREMFNDESC pDesc)
{
    union
    {
        uint8_t  *pu8;
        int32_t  *pi32;
        uint32_t *pu32;
        uint64_t *pu64;
        void     *pv;
    } u;
    u.pv = pvGlue;

    while (cb >= 4)
    {
        /** @todo add defines for the fixup constants... */
        if (*u.pu32 == REM_FIXUP_32_REAL_STUFF)
        {
            /* 32-bit rel jmp/call to real export. */
            *u.pi32 = uExport - (uintptr_t)(u.pi32 + 1);
            Assert((uintptr_t)(u.pi32 + 1) + *u.pi32 == uExport);
            u.pi32++;
            cb -= 4;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_REAL_STUFF)
        {
            /* 64-bit address to the real export. */
            *u.pu64++ = uExport;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_DESC)
        {
            /* 64-bit address to the descriptor. */
            *u.pu64++ = (uintptr_t)pDesc;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_WRAP_GCC_CB)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remWrapGCCCallback;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_LOG_ENTRY)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remLogEntry;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_LOG_EXIT)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remLogExit;
            cb -= 8;
            continue;
        }

        /* move on. */
        u.pu8++;
        cb--;
    }
}


/**
 * Fixes import glue.
 *
 * @param   pvGlue  The glue code.
 * @param   cb      The size of the glue code.
 * @param   pDesc   The import descriptor.
 */
static void remGenerateImportGlueFixup(void *pvGlue, size_t cb, PCREMFNDESC pDesc)
{
    union
    {
        uint8_t  *pu8;
        int32_t  *pi32;
        uint32_t *pu32;
        uint64_t *pu64;
        void     *pv;
    } u;
    u.pv = pvGlue;

    while (cb >= 4)
    {
        if (*u.pu32 == REM_FIXUP_32_REAL_STUFF)
        {
            /* 32-bit rel jmp/call to real function. */
            *u.pi32 = (uintptr_t)pDesc->pv - (uintptr_t)(u.pi32 + 1);
            Assert((uintptr_t)(u.pi32 + 1) + *u.pi32 == (uintptr_t)pDesc->pv);
            u.pi32++;
            cb -= 4;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_REAL_STUFF)
        {
            /* 64-bit address to the real function. */
            *u.pu64++ = (uintptr_t)pDesc->pv;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_DESC)
        {
            /* 64-bit address to the descriptor. */
            *u.pu64++ = (uintptr_t)pDesc;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_WRAP_GCC_CB)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remWrapGCCCallback;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_LOG_ENTRY)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remLogEntry;
            cb -= 8;
            continue;
        }
        if (cb >= 8 && *u.pu64 == REM_FIXUP_64_LOG_EXIT)
        {
            /* 64-bit address to the entry logger function. */
            *u.pu64++ = (uintptr_t)remLogExit;
            cb -= 8;
            continue;
        }

        /* move on. */
        u.pu8++;
        cb--;
    }
}

# endif /* USE_REM_CALLING_CONVENTION_GLUE */


/**
 * Generate wrapper glue code for an export.
 *
 * This is only used on win64 when loading a 64-bit linux module. So, on other
 * platforms it will not do anything.
 *
 * @returns VBox status code.
 * @param   pValue      IN: Where to get the address of the function to wrap.
 *                      OUT: Where to store the glue address.
 * @param   pDesc       The export descriptor.
 */
static int remGenerateExportGlue(PRTUINTPTR pValue, PCREMFNDESC pDesc)
{
# ifdef USE_REM_CALLING_CONVENTION_GLUE
    uintptr_t *ppfn = (uintptr_t *)pDesc->pv;

    uintptr_t pfn = 0; /* a little hack for the callback glue */
    if (!ppfn)
        ppfn = &pfn;

    if (!*ppfn)
    {
        if (remIsFunctionAllInts(pDesc))
        {
            static const struct { void *pvStart, *pvEnd; } s_aTemplates[] =
            {
                { (void *)&WrapMSC2GCC0Int,  (void *)&WrapMSC2GCC0Int_EndProc },
                { (void *)&WrapMSC2GCC1Int,  (void *)&WrapMSC2GCC1Int_EndProc },
                { (void *)&WrapMSC2GCC2Int,  (void *)&WrapMSC2GCC2Int_EndProc },
                { (void *)&WrapMSC2GCC3Int,  (void *)&WrapMSC2GCC3Int_EndProc },
                { (void *)&WrapMSC2GCC4Int,  (void *)&WrapMSC2GCC4Int_EndProc },
                { (void *)&WrapMSC2GCC5Int,  (void *)&WrapMSC2GCC5Int_EndProc },
                { (void *)&WrapMSC2GCC6Int,  (void *)&WrapMSC2GCC6Int_EndProc },
                { (void *)&WrapMSC2GCC7Int,  (void *)&WrapMSC2GCC7Int_EndProc },
                { (void *)&WrapMSC2GCC8Int,  (void *)&WrapMSC2GCC8Int_EndProc },
                { (void *)&WrapMSC2GCC9Int,  (void *)&WrapMSC2GCC9Int_EndProc },
            };
            const unsigned i = pDesc->cParams;
            AssertReleaseMsg(i < ELEMENTS(s_aTemplates), ("%d (%s)\n", i, pDesc->pszName));

            /* duplicate the patch. */
            const size_t cb = (uintptr_t)s_aTemplates[i].pvEnd - (uintptr_t)s_aTemplates[i].pvStart;
            uint8_t *pb = (uint8_t *)remAllocGlue(cb);
            AssertReturn(pb, VERR_NO_MEMORY);
            memcpy(pb, s_aTemplates[i].pvStart, cb);

            /* fix it up. */
            remGenerateExportGlueFixup(pb, cb, *pValue, pDesc);
            *ppfn = (uintptr_t)pb;
        }
        else
        {
            /* custom hacks - it's simpler to make assembly templates than writing a more generic code generator... */
            static const struct { const char *pszName; PFNRT pvStart, pvEnd; } s_aTemplates[] =
            {
                { "somefunction",  (PFNRT)&WrapMSC2GCC9Int,  (PFNRT)&WrapMSC2GCC9Int_EndProc },
            };
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(s_aTemplates); i++)
                if (!strcmp(pDesc->pszName, s_aTemplates[i].pszName))
                    break;
            AssertReleaseMsgReturn(i < RT_ELEMENTS(s_aTemplates), ("Not implemented! %s\n", pDesc->pszName), VERR_NOT_IMPLEMENTED);

            /* duplicate the patch. */
            const size_t cb = (uintptr_t)s_aTemplates[i].pvEnd - (uintptr_t)s_aTemplates[i].pvStart;
            uint8_t *pb = (uint8_t *)remAllocGlue(cb);
            AssertReturn(pb, VERR_NO_MEMORY);
            memcpy(pb, s_aTemplates[i].pvStart, cb);

            /* fix it up. */
            remGenerateExportGlueFixup(pb, cb, *pValue, pDesc);
            *ppfn = (uintptr_t)pb;
        }
    }
    *pValue = *ppfn;
    return VINF_SUCCESS;
# else  /* !USE_REM_CALLING_CONVENTION_GLUE */
    return VINF_SUCCESS;
# endif /* !USE_REM_CALLING_CONVENTION_GLUE */
}


/**
 * Generate wrapper glue code for an import.
 *
 * This is only used on win64 when loading a 64-bit linux module. So, on other
 * platforms it will simply return the address of the imported function
 * without generating any glue code.
 *
 * @returns VBox status code.
 * @param   pValue      Where to store the glue address.
 * @param   pDesc       The export descriptor.
 */
static int remGenerateImportGlue(PRTUINTPTR pValue, PREMFNDESC pDesc)
{
# if defined(USE_REM_CALLING_CONVENTION_GLUE) || defined(USE_REM_IMPORT_JUMP_GLUE)
    if (!pDesc->pvWrapper)
    {
#  ifdef USE_REM_CALLING_CONVENTION_GLUE
        if (remIsFunctionAllInts(pDesc))
        {
            static const struct { void *pvStart, *pvEnd; } s_aTemplates[] =
            {
                { (void *)&WrapGCC2MSC0Int,  (void *)&WrapGCC2MSC0Int_EndProc },
                { (void *)&WrapGCC2MSC1Int,  (void *)&WrapGCC2MSC1Int_EndProc },
                { (void *)&WrapGCC2MSC2Int,  (void *)&WrapGCC2MSC2Int_EndProc },
                { (void *)&WrapGCC2MSC3Int,  (void *)&WrapGCC2MSC3Int_EndProc },
                { (void *)&WrapGCC2MSC4Int,  (void *)&WrapGCC2MSC4Int_EndProc },
                { (void *)&WrapGCC2MSC5Int,  (void *)&WrapGCC2MSC5Int_EndProc },
                { (void *)&WrapGCC2MSC6Int,  (void *)&WrapGCC2MSC6Int_EndProc },
                { (void *)&WrapGCC2MSC7Int,  (void *)&WrapGCC2MSC7Int_EndProc },
                { (void *)&WrapGCC2MSC8Int,  (void *)&WrapGCC2MSC8Int_EndProc },
                { (void *)&WrapGCC2MSC9Int,  (void *)&WrapGCC2MSC9Int_EndProc },
                { (void *)&WrapGCC2MSC10Int, (void *)&WrapGCC2MSC10Int_EndProc },
                { (void *)&WrapGCC2MSC11Int, (void *)&WrapGCC2MSC11Int_EndProc },
                { (void *)&WrapGCC2MSC12Int, (void *)&WrapGCC2MSC12Int_EndProc }
            };
            const unsigned i = pDesc->cParams;
            AssertReleaseMsg(i < ELEMENTS(s_aTemplates), ("%d (%s)\n", i, pDesc->pszName));

            /* duplicate the patch. */
            const size_t cb = (uintptr_t)s_aTemplates[i].pvEnd - (uintptr_t)s_aTemplates[i].pvStart;
            pDesc->pvWrapper = remAllocGlue(cb);
            AssertReturn(pDesc->pvWrapper, VERR_NO_MEMORY);
            memcpy(pDesc->pvWrapper, s_aTemplates[i].pvStart, cb);

            /* fix it up. */
            remGenerateImportGlueFixup((uint8_t *)pDesc->pvWrapper, cb, pDesc);
        }
        else if (   remHasFunctionEllipsis(pDesc)
                 && !remIsFunctionUsingFP(pDesc))
        {
            /* duplicate the patch. */
            const size_t cb = (uintptr_t)&WrapGCC2MSCVariadictInt_EndProc - (uintptr_t)&WrapGCC2MSCVariadictInt;
            pDesc->pvWrapper = remAllocGlue(cb);
            AssertReturn(pDesc->pvWrapper, VERR_NO_MEMORY);
            memcpy(pDesc->pvWrapper, (void *)&WrapGCC2MSCVariadictInt, cb);

            /* fix it up. */
            remGenerateImportGlueFixup((uint8_t *)pDesc->pvWrapper, cb, pDesc);
        }
        else
        {
            /* custom hacks - it's simpler to make assembly templates than writing a more generic code generator... */
            static const struct { const char *pszName; PFNRT pvStart, pvEnd; } s_aTemplates[] =
            {
                { "SSMR3RegisterInternal",  (PFNRT)&WrapGCC2MSC_SSMR3RegisterInternal,  (PFNRT)&WrapGCC2MSC_SSMR3RegisterInternal_EndProc },
            };
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(s_aTemplates); i++)
                if (!strcmp(pDesc->pszName, s_aTemplates[i].pszName))
                    break;
            AssertReleaseMsgReturn(i < RT_ELEMENTS(s_aTemplates), ("Not implemented! %s\n", pDesc->pszName), VERR_NOT_IMPLEMENTED);

            /* duplicate the patch. */
            const size_t cb = (uintptr_t)s_aTemplates[i].pvEnd - (uintptr_t)s_aTemplates[i].pvStart;
            pDesc->pvWrapper = remAllocGlue(cb);
            AssertReturn(pDesc->pvWrapper, VERR_NO_MEMORY);
            memcpy(pDesc->pvWrapper, s_aTemplates[i].pvStart, cb);

            /* fix it up. */
            remGenerateImportGlueFixup((uint8_t *)pDesc->pvWrapper, cb, pDesc);
        }
#  else  /* !USE_REM_CALLING_CONVENTION_GLUE */

        /*
         * Generate a jump patch.
         */
        uint8_t *pb;
#   ifdef RT_ARCH_AMD64
        pDesc->pvWrapper = pb = (uint8_t *)remAllocGlue(32);
        AssertReturn(pDesc->pvWrapper, VERR_NO_MEMORY);
        /**pb++ = 0xcc;*/
        *pb++ = 0xff;
        *pb++ = 0x24;
        *pb++ = 0x25;
        *(uint32_t *)pb = (uintptr_t)pb + 5;
        pb += 5;
        *(uint64_t *)pb = (uint64_t)pDesc->pv;
#   else
        pDesc->pvWrapper = pb = (uint8_t *)remAllocGlue(8);
        AssertReturn(pDesc->pvWrapper, VERR_NO_MEMORY);
        *pb++ = 0xea;
        *(uint32_t *)pb = (uint32_t)pDesc->pv;
#   endif
#  endif /* !USE_REM_CALLING_CONVENTION_GLUE */
    }
    *pValue = (uintptr_t)pDesc->pvWrapper;
# else  /* !USE_REM_CALLING_CONVENTION_GLUE */
    *pValue = (uintptr_t)pDesc->pv;
# endif /* !USE_REM_CALLING_CONVENTION_GLUE */
    return VINF_SUCCESS;
}


/**
 * Resolve an external symbol during RTLdrGetBits().
 *
 * @returns iprt status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) remGetImport(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser)
{
    unsigned i;
    for (i = 0; i < ELEMENTS(g_aVMMImports); i++)
        if (!strcmp(g_aVMMImports[i].pszName, pszSymbol))
            return remGenerateImportGlue(pValue, &g_aVMMImports[i]);
    for (i = 0; i < ELEMENTS(g_aRTImports); i++)
        if (!strcmp(g_aRTImports[i].pszName, pszSymbol))
            return remGenerateImportGlue(pValue, &g_aRTImports[i]);
    for (i = 0; i < ELEMENTS(g_aCRTImports); i++)
        if (!strcmp(g_aCRTImports[i].pszName, pszSymbol))
            return remGenerateImportGlue(pValue, &g_aCRTImports[i]);
    LogRel(("Missing REM Import: %s\n", pszSymbol));
#if 1
    *pValue = 0;
    AssertMsgFailed(("%s.%s\n", pszModule, pszSymbol));
    return VERR_SYMBOL_NOT_FOUND;
#else
    return remGenerateImportGlue(pValue, &g_aCRTImports[0]);
#endif
}

/**
 * Loads the linux object, resolves all imports and exports.
 *
 * @returns VBox status code.
 */
static int remLoadLinuxObj(void)
{
    size_t  offFilename;
    char    szPath[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szPath, sizeof(szPath) - 32);
    AssertRCReturn(rc, rc);
    offFilename = strlen(szPath);

    /*
     * Load the VBoxREM2.rel object/DLL.
     */
    strcpy(&szPath[offFilename], "/VBoxREM2.rel");
    rc = RTLdrOpen(szPath, &g_ModREM2);
    if (VBOX_SUCCESS(rc))
    {
        g_pvREM2 = RTMemExecAlloc(RTLdrSize(g_ModREM2));
        if (g_pvREM2)
        {
#ifdef DEBUG /* How to load the VBoxREM2.rel symbols into the GNU debugger. */
            RTPrintf("VBoxREMWrapper: (gdb) add-symbol-file %s 0x%p\n", szPath, g_pvREM2);
#endif
            LogRel(("REM: Loading %s at 0x%p (%d bytes)\n"
                    "REM: (gdb) add-symbol-file %s 0x%p\n",
                    szPath, g_pvREM2, RTLdrSize(g_ModREM2), szPath, g_pvREM2));
            rc = RTLdrGetBits(g_ModREM2, g_pvREM2, (RTUINTPTR)g_pvREM2, remGetImport, NULL);
            if (VBOX_SUCCESS(rc))
            {
                /*
                 * Resolve exports.
                 */
                unsigned i;
                for (i = 0; i < ELEMENTS(g_aExports); i++)
                {
                    RTUINTPTR Value;
                    rc = RTLdrGetSymbolEx(g_ModREM2, g_pvREM2, (RTUINTPTR)g_pvREM2, g_aExports[i].pszName, &Value);
                    AssertMsgRC(rc, ("%s rc=%Vrc\n", g_aExports[i].pszName, rc));
                    if (VBOX_FAILURE(rc))
                        break;
                    rc = remGenerateExportGlue(&Value, &g_aExports[i]);
                    if (VBOX_FAILURE(rc))
                        break;
                    *(void **)g_aExports[i].pv = (void *)(uintptr_t)Value;
                }
                return rc;
            }
            RTMemExecFree(g_pvREM2);
        }
        RTLdrClose(g_ModREM2);
        g_ModREM2 = NIL_RTLDRMOD;
    }
    LogRel(("REM: failed loading '%s', rc=%Vrc\n", szPath, rc));
    return rc;
}


/**
 * Unloads the linux object, freeing up all resources (dlls and
 * import glue) we allocated during remLoadLinuxObj().
 */
static void remUnloadLinuxObj(void)
{
    unsigned i;

    /* close modules. */
    RTLdrClose(g_ModREM2);
    g_ModREM2 = NIL_RTLDRMOD;
    RTMemExecFree(g_pvREM2);
    g_pvREM2 = NULL;

    /* clear the pointers. */
    for (i = 0; i < ELEMENTS(g_aExports); i++)
        *(void **)g_aExports[i].pv = NULL;
# if defined(USE_REM_CALLING_CONVENTION_GLUE) || defined(USE_REM_IMPORT_JUMP_GLUE)
    for (i = 0; i < ELEMENTS(g_aVMMImports); i++)
        g_aVMMImports[i].pvWrapper = NULL;
    for (i = 0; i < ELEMENTS(g_aRTImports); i++)
        g_aRTImports[i].pvWrapper = NULL;
    for (i = 0; i < ELEMENTS(g_aCRTImports); i++)
        g_aCRTImports[i].pvWrapper = NULL;

    /* free wrapper memory. */
    while (g_pExecMemHead)
    {
        PREMEXECMEM pCur = g_pExecMemHead;
        g_pExecMemHead = pCur->pNext;
        memset(pCur, 0xcc, pCur->cb);
        RTMemExecFree(pCur);
    }
# endif
}
#endif


REMR3DECL(int) REMR3Init(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VINF_SUCCESS;
#else
    if (!pfnREMR3Init)
    {
        int rc = remLoadLinuxObj();
        if (VBOX_FAILURE(rc))
            return rc;
    }
    return pfnREMR3Init(pVM);
#endif
}

REMR3DECL(int) REMR3Term(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VINF_SUCCESS;
#else
    int rc;
    Assert(VALID_PTR(pfnREMR3Term));
    rc = pfnREMR3Term(pVM);
    remUnloadLinuxObj();
    return rc;
#endif
}

REMR3DECL(void) REMR3Reset(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3Reset));
    pfnREMR3Reset(pVM);
#endif
}

REMR3DECL(int) REMR3Step(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3Step));
    return pfnREMR3Step(pVM);
#endif
}

REMR3DECL(int) REMR3BreakpointSet(PVM pVM, RTGCUINTPTR Address)
{
#ifdef USE_REM_STUBS
    return VERR_REM_NO_MORE_BP_SLOTS;
#else
    Assert(VALID_PTR(pfnREMR3BreakpointSet));
    return pfnREMR3BreakpointSet(pVM, Address);
#endif
}

REMR3DECL(int) REMR3BreakpointClear(PVM pVM, RTGCUINTPTR Address)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3BreakpointClear));
    return pfnREMR3BreakpointClear(pVM, Address);
#endif
}

REMR3DECL(int) REMR3EmulateInstruction(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3EmulateInstruction));
    return pfnREMR3EmulateInstruction(pVM);
#endif
}

REMR3DECL(int) REMR3Run(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3Run));
    return pfnREMR3Run(pVM);
#endif
}

REMR3DECL(int) REMR3State(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3State));
    return pfnREMR3State(pVM);
#endif
}

REMR3DECL(int) REMR3StateBack(PVM pVM)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3StateBack));
    return pfnREMR3StateBack(pVM);
#endif
}

REMR3DECL(void) REMR3StateUpdate(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3StateUpdate));
    pfnREMR3StateUpdate(pVM);
#endif
}

REMR3DECL(void) REMR3A20Set(PVM pVM, bool fEnable)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3A20Set));
    pfnREMR3A20Set(pVM, fEnable);
#endif
}

REMR3DECL(void) REMR3ReplayInvalidatedPages(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3ReplayInvalidatedPages));
    pfnREMR3ReplayInvalidatedPages(pVM);
#endif
}

REMR3DECL(void) REMR3ReplayHandlerNotifications(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3ReplayHandlerNotifications));
    pfnREMR3ReplayHandlerNotifications(pVM);
#endif
}

REMR3DECL(int) REMR3NotifyCodePageChanged(PVM pVM, RTGCPTR pvCodePage)
{
#ifdef USE_REM_STUBS
    return VINF_SUCCESS;
#else
    Assert(VALID_PTR(pfnREMR3NotifyCodePageChanged));
    return pfnREMR3NotifyCodePageChanged(pVM, pvCodePage);
#endif
}

REMR3DECL(void) REMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, void *pvRam, unsigned fFlags)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyPhysRamRegister));
    pfnREMR3NotifyPhysRamRegister(pVM, GCPhys, cb, pvRam, fFlags);
#endif
}

REMR3DECL(void) REMR3NotifyPhysRamChunkRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, RTHCUINTPTR pvRam, unsigned fFlags)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyPhysRamChunkRegister));
    pfnREMR3NotifyPhysRamChunkRegister(pVM, GCPhys, cb, pvRam, fFlags);
#endif
}

REMR3DECL(void) REMR3NotifyPhysRomRegister(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, void *pvCopy, bool fShadow)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyPhysRomRegister));
    pfnREMR3NotifyPhysRomRegister(pVM, GCPhys, cb, pvCopy, fShadow);
#endif
}

REMR3DECL(void) REMR3NotifyPhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cb)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyPhysReserve));
    pfnREMR3NotifyPhysReserve(pVM, GCPhys, cb);
#endif
}

REMR3DECL(void) REMR3NotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyHandlerPhysicalRegister));
    pfnREMR3NotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, cb, fHasHCHandler);
#endif
}

REMR3DECL(void) REMR3NotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyHandlerPhysicalDeregister));
    pfnREMR3NotifyHandlerPhysicalDeregister(pVM, enmType, GCPhys, cb, fHasHCHandler, fRestoreAsRAM);
#endif
}

REMR3DECL(void) REMR3NotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fHasHCHandler, bool fRestoreAsRAM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyHandlerPhysicalModify));
    pfnREMR3NotifyHandlerPhysicalModify(pVM, enmType, GCPhysOld, GCPhysNew, cb, fHasHCHandler, fRestoreAsRAM);
#endif
}

REMDECL(bool) REMR3IsPageAccessHandled(PVM pVM, RTGCPHYS GCPhys)
{
#ifdef USE_REM_STUBS
    return false;
#else
    Assert(VALID_PTR(pfnREMR3IsPageAccessHandled));
    return pfnREMR3IsPageAccessHandled(pVM, GCPhys);
#endif
}

REMR3DECL(int) REMR3DisasEnableStepping(PVM pVM, bool fEnable)
{
#ifdef USE_REM_STUBS
    return VERR_NOT_IMPLEMENTED;
#else
    Assert(VALID_PTR(pfnREMR3DisasEnableStepping));
    return pfnREMR3DisasEnableStepping(pVM, fEnable);
#endif
}

REMR3DECL(void) REMR3NotifyPendingInterrupt(PVM pVM, uint8_t u8Interrupt)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyPendingInterrupt));
    pfnREMR3NotifyPendingInterrupt(pVM, u8Interrupt);
#endif
}

REMR3DECL(uint32_t) REMR3QueryPendingInterrupt(PVM pVM)
{
#ifdef USE_REM_STUBS
    return REM_NO_PENDING_IRQ;
#else
    Assert(VALID_PTR(pfnREMR3QueryPendingInterrupt));
    return pfnREMR3QueryPendingInterrupt(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyInterruptSet(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyInterruptSet));
    pfnREMR3NotifyInterruptSet(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyInterruptClear(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyInterruptClear));
    pfnREMR3NotifyInterruptClear(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyTimerPending(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyTimerPending));
    pfnREMR3NotifyTimerPending(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyDmaPending(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyDmaPending));
    pfnREMR3NotifyDmaPending(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyQueuePending(PVM pVM)
{
#ifndef USE_REM_STUBS
    Assert(VALID_PTR(pfnREMR3NotifyQueuePending));
    pfnREMR3NotifyQueuePending(pVM);
#endif
}

REMR3DECL(void) REMR3NotifyFF(PVM pVM)
{
#ifndef USE_REM_STUBS
    /* the timer can call this early on, so don't be picky. */
    if (pfnREMR3NotifyFF)
        pfnREMR3NotifyFF(pVM);
#endif
}

