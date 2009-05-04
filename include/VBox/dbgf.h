/** @file
 * DBGF - Debugger Facility.
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

#ifndef ___VBox_dbgf_h
#define ___VBox_dbgf_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm.h>
#include <VBox/log.h>                   /* LOG_ENABLED */
#include <VBox/dbgfsel.h>

#include <iprt/stdarg.h>

__BEGIN_DECLS


/** @defgroup grp_dbgf     The Debugger Facility API
 * @{
 */

#if defined(IN_RC)|| defined(IN_RING0)
/** @addgroup grp_dbgf_rz  The RZ DBGF API
 * @ingroup grp_dbgf
 * @{
 */
VMMRCDECL(int) DBGFRZTrap01Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6);
VMMRCDECL(int) DBGFRZTrap03Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame);
/** @} */
#endif



/**
 * Mixed address.
 */
typedef struct DBGFADDRESS
{
    /** The flat address. */
    RTGCUINTPTR FlatPtr;
    /** The selector offset address. */
    RTGCUINTPTR off;
    /** The selector. DBGF_SEL_FLAT is a legal value. */
    RTSEL       Sel;
    /** Flags describing further details about the address. */
    uint16_t    fFlags;
} DBGFADDRESS;
/** Pointer to a mixed address. */
typedef DBGFADDRESS *PDBGFADDRESS;
/** Pointer to a const mixed address. */
typedef const DBGFADDRESS *PCDBGFADDRESS;

/** @name DBGFADDRESS Flags.
 * @{ */
/** A 16:16 far address. */
#define DBGFADDRESS_FLAGS_FAR16         0
/** A 16:32 far address. */
#define DBGFADDRESS_FLAGS_FAR32         1
/** A 16:64 far address. */
#define DBGFADDRESS_FLAGS_FAR64         2
/** A flat address. */
#define DBGFADDRESS_FLAGS_FLAT          3
/** A physical address. */
#define DBGFADDRESS_FLAGS_PHYS          4
/** The address type mask. */
#define DBGFADDRESS_FLAGS_TYPE_MASK     7

/** Set if the address is valid. */
#define DBGFADDRESS_FLAGS_VALID         RT_BIT(3)

/** The address is within the hypervisor memoary area (HMA).
 * If not set, the address can be assumed to be a guest address. */
#define DBGFADDRESS_FLAGS_HMA           RT_BIT(4)

/** Checks if the mixed address is flat or not. */
#define DBGFADDRESS_IS_FLAT(pAddress)    ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FLAT )
/** Checks if the mixed address is flat or not. */
#define DBGFADDRESS_IS_PHYS(pAddress)    ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_PHYS )
/** Checks if the mixed address is far 16:16 or not. */
#define DBGFADDRESS_IS_FAR16(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR16 )
/** Checks if the mixed address is far 16:32 or not. */
#define DBGFADDRESS_IS_FAR32(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR32 )
/** Checks if the mixed address is far 16:64 or not. */
#define DBGFADDRESS_IS_FAR64(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR64 )
/** Checks if the mixed address is valid. */
#define DBGFADDRESS_IS_VALID(pAddress)   ( !!((pAddress)->fFlags & DBGFADDRESS_FLAGS_VALID) )
/** Checks if the address is flagged as within the HMA. */
#define DBGFADDRESS_IS_HMA(pAddress)     ( !!((pAddress)->fFlags & DBGFADDRESS_FLAGS_HMA) )
/** @} */

VMMR3DECL(int)          DBGFR3AddrFromSelOff(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromFlat(PVM pVM, PDBGFADDRESS pAddress, RTGCUINTPTR FlatPtr);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromPhys(PVM pVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr);
VMMR3DECL(bool)         DBGFR3AddrIsValid(PVM pVM, PCDBGFADDRESS pAddress);
VMMR3DECL(int)          DBGFR3AddrToPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTGCPHYS pGCPhys);
VMMR3DECL(int)          DBGFR3AddrToHostPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys);
VMMR3DECL(int)          DBGFR3AddrToVolatileR3Ptr(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrAdd(PDBGFADDRESS pAddress, RTGCUINTPTR uAddend);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrSub(PDBGFADDRESS pAddress, RTGCUINTPTR uSubtrahend);




/**
 * VMM Debug Event Type.
 */
typedef enum DBGFEVENTTYPE
{
    /** Halt completed.
     * This notifies that a halt command have been successfully completed.
     */
    DBGFEVENT_HALT_DONE = 0,
    /** Detach completed.
     * This notifies that the detach command have been successfully completed.
     */
    DBGFEVENT_DETACH_DONE,
    /** The command from the debugger is not recognized.
     * This means internal error or half implemented features.
     */
    DBGFEVENT_INVALID_COMMAND,


    /** Fatal error.
     * This notifies a fatal error in the VMM and that the debugger get's a
     * chance to first hand information about the the problem.
     */
    DBGFEVENT_FATAL_ERROR = 100,
    /** Breakpoint Hit.
     * This notifies that a breakpoint installed by the debugger was hit. The
     * identifier of the breakpoint can be found in the DBGFEVENT::u::Bp::iBp member.
     */
    DBGFEVENT_BREAKPOINT,
    /** Breakpoint Hit in the Hypervisor.
     * This notifies that a breakpoint installed by the debugger was hit. The
     * identifier of the breakpoint can be found in the DBGFEVENT::u::Bp::iBp member.
     */
    DBGFEVENT_BREAKPOINT_HYPER,
    /** Assertion in the Hypervisor (breakpoint instruction).
     * This notifies that a breakpoint instruction was hit in the hypervisor context.
     */
    DBGFEVENT_ASSERTION_HYPER,
    /** Single Stepped.
     * This notifies that a single step operation was completed.
     */
    DBGFEVENT_STEPPED,
    /** Single Stepped.
     * This notifies that a hypervisor single step operation was completed.
     */
    DBGFEVENT_STEPPED_HYPER,
    /** The developer have used the DBGFSTOP macro or the PDMDeviceDBGFSTOP function
     * to bring up the debugger at a specific place.
     */
    DBGFEVENT_DEV_STOP,
    /** The VM is terminating.
     * When this notification is received, the debugger thread should detach ASAP.
     */
    DBGFEVENT_TERMINATING,

    /** The usual 32-bit hack. */
    DBGFEVENT_32BIT_HACK = 0x7fffffff
} DBGFEVENTTYPE;


/**
 * The context of an event.
 */
typedef enum DBGFEVENTCTX
{
    /** The usual invalid entry. */
    DBGFEVENTCTX_INVALID = 0,
    /** Raw mode. */
    DBGFEVENTCTX_RAW,
    /** Recompiled mode. */
    DBGFEVENTCTX_REM,
    /** VMX / AVT mode. */
    DBGFEVENTCTX_HWACCL,
    /** Hypervisor context. */
    DBGFEVENTCTX_HYPER,
    /** Other mode */
    DBGFEVENTCTX_OTHER,

    /** The usual 32-bit hack */
    DBGFEVENTCTX_32BIT_HACK = 0x7fffffff
} DBGFEVENTCTX;

/**
 * VMM Debug Event.
 */
typedef struct DBGFEVENT
{
    /** Type. */
    DBGFEVENTTYPE   enmType;
    /** Context */
    DBGFEVENTCTX    enmCtx;
    /** Type specific data. */
    union
    {
        /** Fatal error details. */
        struct
        {
            /** The GC return code. */
            int                     rc;
        } FatalError;

        /** Source location. */
        struct
        {
            /** File name. */
            R3PTRTYPE(const char *) pszFile;
            /** Function name. */
            R3PTRTYPE(const char *) pszFunction;
            /** Message. */
            R3PTRTYPE(const char *) pszMessage;
            /** Line number. */
            unsigned                uLine;
        } Src;

        /** Assertion messages. */
        struct
        {
            /** The first message. */
            R3PTRTYPE(const char *) pszMsg1;
            /** The second message. */
            R3PTRTYPE(const char *) pszMsg2;
        } Assert;

        /** Breakpoint. */
        struct DBGFEVENTBP
        {
            /** The identifier of the breakpoint which was hit. */
            RTUINT                  iBp;
        } Bp;
        /** Padding for ensuring that the structure is 8 byte aligned. */
        uint64_t        au64Padding[4];
    } u;
} DBGFEVENT;
/** Pointer to VMM Debug Event. */
typedef DBGFEVENT *PDBGFEVENT;
/** Pointer to const VMM Debug Event. */
typedef const DBGFEVENT *PCDBGFEVENT;


/** @def DBGFSTOP
 * Stops the debugger raising a DBGFEVENT_DEVELOPER_STOP event.
 *
 * @returns VBox status code which must be propagated up to EM if not VINF_SUCCESS.
 * @param   pVM     VM Handle.
 */
#ifdef VBOX_STRICT
# define DBGFSTOP(pVM)  DBGFR3EventSrc(pVM, DBGFEVENT_DEV_STOP, __FILE__, __LINE__, __PRETTY_FUNCTION__, NULL)
#else
# define DBGFSTOP(pVM)  VINF_SUCCESS
#endif

VMMR3DECL(int)  DBGFR3Init(PVM pVM);
VMMR3DECL(int)  DBGFR3Term(PVM pVM);
VMMR3DECL(void) DBGFR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(int)  DBGFR3VMMForcedAction(PVM pVM);
VMMR3DECL(int)  DBGFR3Event(PVM pVM, DBGFEVENTTYPE enmEvent);
VMMR3DECL(int)  DBGFR3EventSrc(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, ...);
VMMR3DECL(int)  DBGFR3EventSrcV(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, va_list args);
VMMR3DECL(int)  DBGFR3EventAssertion(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszMsg1, const char *pszMsg2);
VMMR3DECL(int)  DBGFR3EventBreakpoint(PVM pVM, DBGFEVENTTYPE enmEvent);
VMMR3DECL(int)  DBGFR3Attach(PVM pVM);
VMMR3DECL(int)  DBGFR3Detach(PVM pVM);
VMMR3DECL(int)  DBGFR3EventWait(PVM pVM, unsigned cMillies, PCDBGFEVENT *ppEvent);
VMMR3DECL(int)  DBGFR3Halt(PVM pVM);
VMMR3DECL(bool) DBGFR3IsHalted(PVM pVM);
VMMR3DECL(bool) DBGFR3CanWait(PVM pVM);
VMMR3DECL(int)  DBGFR3Resume(PVM pVM);
VMMR3DECL(int)  DBGFR3Step(PVM pVM, VMCPUID idCpu);
VMMR3DECL(int)  DBGFR3PrgStep(PVMCPU pVCpu);


/** Breakpoint type. */
typedef enum DBGFBPTYPE
{
    /** Free breakpoint entry. */
    DBGFBPTYPE_FREE = 0,
    /** Debug register. */
    DBGFBPTYPE_REG,
    /** INT 3 instruction. */
    DBGFBPTYPE_INT3,
    /** Recompiler. */
    DBGFBPTYPE_REM,
    /** ensure 32-bit size. */
    DBGFBPTYPE_32BIT_HACK = 0x7fffffff
} DBGFBPTYPE;


/**
 * A Breakpoint.
 */
typedef struct DBGFBP
{
    /** The number of breakpoint hits. */
    uint64_t        cHits;
    /** The hit number which starts to trigger the breakpoint. */
    uint64_t        iHitTrigger;
    /** The hit number which stops triggering the breakpoint (disables it).
     * Use ~(uint64_t)0 if it should never stop. */
    uint64_t        iHitDisable;
    /** The Flat GC address of the breakpoint.
     * (PC register value if REM type?) */
    RTGCUINTPTR     GCPtr;
    /** The breakpoint id. */
    RTUINT          iBp;
    /** The breakpoint status - enabled or disabled. */
    bool            fEnabled;

    /** The breakpoint type. */
    DBGFBPTYPE      enmType;

#if GC_ARCH_BITS == 64
    uint32_t        u32Padding;
#endif

    /** Union of type specific data. */
    union
    {
        /** Debug register data. */
        struct DBGFBPREG
        {
            /** The debug register number. */
            uint8_t     iReg;
            /** The access type (one of the X86_DR7_RW_* value). */
            uint8_t     fType;
            /** The access size. */
            uint8_t     cb;
        } Reg;
        /** Recompiler breakpoint data. */
        struct DBGFBPINT3
        {
            /** The byte value we replaced by the INT 3 instruction. */
            uint8_t     bOrg;
        } Int3;

        /** Recompiler breakpoint data. */
        struct DBGFBPREM
        {
            /** nothing yet */
            uint8_t fDummy;
        } Rem;
        /** Paddind to ensure that the size is identical on win32 and linux. */
        uint64_t    u64Padding;
    } u;
} DBGFBP;

/** Pointer to a breakpoint. */
typedef DBGFBP *PDBGFBP;
/** Pointer to a const breakpoint. */
typedef const DBGFBP *PCDBGFBP;


VMMR3DECL(int)  DBGFR3BpSet(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp);
VMMR3DECL(int)  DBGFR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                               uint8_t fType, uint8_t cb, PRTUINT piBp);
VMMR3DECL(int)  DBGFR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp);
VMMR3DECL(int)  DBGFR3BpClear(PVM pVM, RTUINT iBp);
VMMR3DECL(int)  DBGFR3BpEnable(PVM pVM, RTUINT iBp);
VMMR3DECL(int)  DBGFR3BpDisable(PVM pVM, RTUINT iBp);

/**
 * Breakpoint enumeration callback function.
 *
 * @returns VBox status code. Any failure will stop the enumeration.
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   pBp         Pointer to the breakpoint information. (readonly)
 */
typedef DECLCALLBACK(int) FNDBGFBPENUM(PVM pVM, void *pvUser, PCDBGFBP pBp);
/** Pointer to a breakpoint enumeration callback function. */
typedef FNDBGFBPENUM *PFNDBGFBPENUM;

VMMR3DECL(int)          DBGFR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser);
VMMDECL(RTGCUINTREG)    DBGFBpGetDR7(PVM pVM);
VMMDECL(RTGCUINTREG)    DBGFBpGetDR0(PVM pVM);
VMMDECL(RTGCUINTREG)    DBGFBpGetDR1(PVM pVM);
VMMDECL(RTGCUINTREG)    DBGFBpGetDR2(PVM pVM);
VMMDECL(RTGCUINTREG)    DBGFBpGetDR3(PVM pVM);
VMMDECL(bool)           DBGFIsStepping(PVMCPU pVCpu);




/** Pointer to a info helper callback structure. */
typedef struct DBGFINFOHLP *PDBGFINFOHLP;
/** Pointer to a const info helper callback structure. */
typedef const struct DBGFINFOHLP *PCDBGFINFOHLP;

/**
 * Info helper callback structure.
 */
typedef struct DBGFINFOHLP
{
    /**
     * Print formatted string.
     *
     * @param   pHlp        Pointer to this structure.
     * @param   pszFormat   The format string.
     * @param   ...         Arguments.
     */
    DECLCALLBACKMEMBER(void, pfnPrintf)(PCDBGFINFOHLP pHlp, const char *pszFormat, ...);

    /**
     * Print formatted string.
     *
     * @param   pHlp        Pointer to this structure.
     * @param   pszFormat   The format string.
     * @param   args        Argument list.
     */
    DECLCALLBACKMEMBER(void, pfnPrintfV)(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args);
} DBGFINFOHLP;


/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERDEV(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERDEV function. */
typedef FNDBGFHANDLERDEV  *PFNDBGFHANDLERDEV;

/**
 * Info handler, driver version.
 *
 * @param   pDrvIns     Driver instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERDRV(PPDMDRVINS pDrvIns, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERDRV function. */
typedef FNDBGFHANDLERDRV  *PFNDBGFHANDLERDRV;

/**
 * Info handler, internal version.
 *
 * @param   pVM         The VM handle.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERINT(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERINT function. */
typedef FNDBGFHANDLERINT  *PFNDBGFHANDLERINT;

/**
 * Info handler, external version.
 *
 * @param   pvUser      User argument.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLEREXT(void *pvUser, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLEREXT function. */
typedef FNDBGFHANDLEREXT  *PFNDBGFHANDLEREXT;


/** @name Flags for the info registration functions.
 * @{ */
/** The handler must run on the EMT. */
#define DBGFINFO_FLAGS_RUN_ON_EMT       RT_BIT(0)
/** @} */

VMMR3DECL(int) DBGFR3InfoRegisterDevice(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler, PPDMDEVINS pDevIns);
VMMR3DECL(int) DBGFR3InfoRegisterDriver(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDRV pfnHandler, PPDMDRVINS pDrvIns);
VMMR3DECL(int) DBGFR3InfoRegisterInternal(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler);
VMMR3DECL(int) DBGFR3InfoRegisterInternalEx(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler, uint32_t fFlags);
VMMR3DECL(int) DBGFR3InfoRegisterExternal(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLEREXT pfnHandler, void *pvUser);
VMMR3DECL(int) DBGFR3InfoDeregisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName);
VMMR3DECL(int) DBGFR3InfoDeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName);
VMMR3DECL(int) DBGFR3InfoDeregisterInternal(PVM pVM, const char *pszName);
VMMR3DECL(int) DBGFR3InfoDeregisterExternal(PVM pVM, const char *pszName);
VMMR3DECL(int) DBGFR3Info(PVM pVM, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp);

/** @def DBGFR3InfoLog
 * Display a piece of info writing to the log if enabled.
 *
 * @param   pVM         VM handle.
 * @param   pszName     The identifier of the info to display.
 * @param   pszArgs     Arguments to the info handler.
 */
#ifdef LOG_ENABLED
#define DBGFR3InfoLog(pVM, pszName, pszArgs) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3Info(pVM, pszName, pszArgs, NULL); \
    } while (0)
#else
#define DBGFR3InfoLog(pVM, pszName, pszArgs) do { } while (0)
#endif

/**
 * Enumeration callback for use with DBGFR3InfoEnum.
 *
 * @returns VBox status code.
 *          A status code indicating failure will end the enumeration
 *          and DBGFR3InfoEnum will return with that status code.
 * @param   pVM         VM handle.
 * @param   pszName     Info identifier name.
 * @param   pszDesc     The description.
 */
typedef DECLCALLBACK(int) FNDBGFINFOENUM(PVM pVM, const char *pszName, const char *pszDesc, void *pvUser);
/** Pointer to a FNDBGFINFOENUM function. */
typedef FNDBGFINFOENUM *PFNDBGFINFOENUM;

VMMR3DECL(int)              DBGFR3InfoEnum(PVM pVM, PFNDBGFINFOENUM pfnCallback, void *pvUser);
VMMR3DECL(PCDBGFINFOHLP)    DBGFR3InfoLogHlp(void);
VMMR3DECL(PCDBGFINFOHLP)    DBGFR3InfoLogRelHlp(void);



VMMR3DECL(int) DBGFR3LogModifyGroups(PVM pVM, const char *pszGroupSettings);
VMMR3DECL(int) DBGFR3LogModifyFlags(PVM pVM, const char *pszFlagSettings);
VMMR3DECL(int) DBGFR3LogModifyDestinations(PVM pVM, const char *pszDestSettings);



/** Max length (including '\\0') of a symbol name. */
#define DBGF_SYMBOL_NAME_LENGTH   512

/**
 * Debug symbol.
 */
typedef struct DBGFSYMBOL
{
    /** Symbol value (address). */
    RTGCUINTPTR         Value;
    /** Symbol size. */
    uint32_t            cb;
    /** Symbol Flags. (reserved). */
    uint32_t            fFlags;
    /** Symbol name. */
    char                szName[DBGF_SYMBOL_NAME_LENGTH];
} DBGFSYMBOL;
/** Pointer to debug symbol. */
typedef DBGFSYMBOL *PDBGFSYMBOL;
/** Pointer to const debug symbol. */
typedef const DBGFSYMBOL *PCDBGFSYMBOL;

/**
 * Debug line number information.
 */
typedef struct DBGFLINE
{
    /** Address. */
    RTGCUINTPTR         Address;
    /** Line number. */
    uint32_t            uLineNo;
    /** Filename. */
    char                szFilename[260];
} DBGFLINE;
/** Pointer to debug line number. */
typedef DBGFLINE *PDBGFLINE;
/** Pointer to const debug line number. */
typedef const DBGFLINE *PCDBGFLINE;

VMMR3DECL(int)          DBGFR3ModuleLoad(PVM pVM, const char *pszFilename, RTGCUINTPTR AddressDelta, const char *pszName, RTGCUINTPTR ModuleAddress, unsigned cbImage);
VMMR3DECL(void)         DBGFR3ModuleRelocate(PVM pVM, RTGCUINTPTR OldImageBase, RTGCUINTPTR NewImageBase, RTGCUINTPTR cbImage,
                                             const char *pszFilename, const char *pszName);
VMMR3DECL(int)          DBGFR3SymbolAdd(PVM pVM, RTGCUINTPTR ModuleAddress, RTGCUINTPTR SymbolAddress, RTUINT cbSymbol, const char *pszSymbol);
VMMR3DECL(int)          DBGFR3SymbolByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFSYMBOL pSymbol);
VMMR3DECL(int)          DBGFR3SymbolByName(PVM pVM, const char *pszSymbol, PDBGFSYMBOL pSymbol);
VMMR3DECL(PDBGFSYMBOL)  DBGFR3SymbolByAddrAlloc(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement);
VMMR3DECL(PDBGFSYMBOL)  DBGFR3SymbolByNameAlloc(PVM pVM, const char *pszSymbol);
VMMR3DECL(void)         DBGFR3SymbolFree(PDBGFSYMBOL pSymbol);
VMMR3DECL(int)          DBGFR3LineByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFLINE pLine);
VMMR3DECL(PDBGFLINE)    DBGFR3LineByAddrAlloc(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement);
VMMR3DECL(void)         DBGFR3LineFree(PDBGFLINE pLine);


/**
 * Return type.
 */
typedef enum DBGFRETRUNTYPE
{
    /** The usual invalid 0 value. */
    DBGFRETURNTYPE_INVALID = 0,
    /** Near 16-bit return. */
    DBGFRETURNTYPE_NEAR16,
    /** Near 32-bit return. */
    DBGFRETURNTYPE_NEAR32,
    /** Near 64-bit return. */
    DBGFRETURNTYPE_NEAR64,
    /** Far 16:16 return. */
    DBGFRETURNTYPE_FAR16,
    /** Far 16:32 return. */
    DBGFRETURNTYPE_FAR32,
    /** Far 16:64 return. */
    DBGFRETURNTYPE_FAR64,
    /** 16-bit iret return (e.g. real or 286 protect mode). */
    DBGFRETURNTYPE_IRET16,
    /** 32-bit iret return. */
    DBGFRETURNTYPE_IRET32,
    /** 32-bit iret return. */
    DBGFRETURNTYPE_IRET32_PRIV,
    /** 32-bit iret return to V86 mode. */
    DBGFRETURNTYPE_IRET32_V86,
    /** @todo 64-bit iret return. */
    DBGFRETURNTYPE_IRET64,
    /** The end of the valid return types. */
    DBGFRETURNTYPE_END,
    /** The usual 32-bit blowup. */
    DBGFRETURNTYPE_32BIT_HACK = 0x7fffffff
} DBGFRETURNTYPE;

/**
 * Figures the size of the return state on the stack.
 *
 * @returns number of bytes. 0 if invalid parameter.
 * @param   enmRetType  The type of return.
 */
DECLINLINE(unsigned) DBGFReturnTypeSize(DBGFRETURNTYPE enmRetType)
{
    switch (enmRetType)
    {
        case DBGFRETURNTYPE_NEAR16:         return 2;
        case DBGFRETURNTYPE_NEAR32:         return 4;
        case DBGFRETURNTYPE_NEAR64:         return 8;
        case DBGFRETURNTYPE_FAR16:          return 4;
        case DBGFRETURNTYPE_FAR32:          return 4;
        case DBGFRETURNTYPE_FAR64:          return 8;
        case DBGFRETURNTYPE_IRET16:         return 6;
        case DBGFRETURNTYPE_IRET32:         return 4*3;
        case DBGFRETURNTYPE_IRET32_PRIV:    return 4*5;
        case DBGFRETURNTYPE_IRET32_V86:     return 4*9;
        case DBGFRETURNTYPE_IRET64:
        default:
            return 0;
    }
}


/** Pointer to stack frame info. */
typedef struct DBGFSTACKFRAME *PDBGFSTACKFRAME;
/** Pointer to const stack frame info. */
typedef struct DBGFSTACKFRAME const  *PCDBGFSTACKFRAME;
/**
 * Info about a stack frame.
 */
typedef struct DBGFSTACKFRAME
{
    /** Frame number. */
    RTUINT          iFrame;
    /** Frame flags. */
    RTUINT          fFlags;
    /** The frame address.
     * The off member is [e|r]bp and the Sel member is ss. */
    DBGFADDRESS     AddrFrame;
    /** The stack address of the frame.
     * The off member is [e|r]sp and the Sel member is ss. */
    DBGFADDRESS     AddrStack;
    /** The program counter (PC) address of the frame.
     * The off member is [e|r]ip and the Sel member is cs. */
    DBGFADDRESS     AddrPC;
    /** Pointer to the symbol nearest the program counter (PC). NULL if not found. */
    PDBGFSYMBOL     pSymPC;
    /** Pointer to the linnumber nearest the program counter (PC). NULL if not found. */
    PDBGFLINE       pLinePC;

    /** The return frame address.
     * The off member is [e|r]bp and the Sel member is ss. */
    DBGFADDRESS     AddrReturnFrame;
    /** The return stack address.
     * The off member is [e|r]sp and the Sel member is ss. */
    DBGFADDRESS     AddrReturnStack;
    /** The way this frame returns to the next one. */
    DBGFRETURNTYPE  enmReturnType;

    /** The program counter (PC) address which the frame returns to.
     * The off member is [e|r]ip and the Sel member is cs. */
    DBGFADDRESS     AddrReturnPC;
    /** Pointer to the symbol nearest the return PC. NULL if not found. */
    PDBGFSYMBOL     pSymReturnPC;
    /** Pointer to the linnumber nearest the return PC. NULL if not found. */
    PDBGFLINE       pLineReturnPC;

    /** 32-bytes of stack arguments. */
    union
    {
        /** 64-bit view */
        uint64_t    au64[4];
        /** 32-bit view */
        uint32_t    au32[8];
        /** 16-bit view */
        uint16_t    au16[16];
        /** 8-bit view */
        uint8_t     au8[32];
    } Args;

    /** Pointer to the next frame.
     * Might not be used in some cases, so consider it internal. */
    PCDBGFSTACKFRAME pNextInternal;
    /** Pointer to the first frame.
     * Might not be used in some cases, so consider it internal. */
    PCDBGFSTACKFRAME pFirstInternal;
} DBGFSTACKFRAME;

/** @name DBGFSTACKFRAME Flags.
 * @{ */
/** Set if the content of the frame is filled in by DBGFR3StackWalk() and can be used
 * to construct the next frame. */
#define DBGFSTACKFRAME_FLAGS_ALL_VALID  RT_BIT(0)
/** This is the last stack frame we can read.
 * This flag is not set if the walk stop because of max dept or recursion. */
#define DBGFSTACKFRAME_FLAGS_LAST       RT_BIT(1)
/** This is the last record because we detected a loop. */
#define DBGFSTACKFRAME_FLAGS_LOOP       RT_BIT(2)
/** This is the last record because we reached the maximum depth. */
#define DBGFSTACKFRAME_FLAGS_MAX_DEPTH  RT_BIT(3)
/** @} */

VMMR3DECL(int)              DBGFR3StackWalkBeginGuest(PVM pVM, VMCPUID idCpu, PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(int)              DBGFR3StackWalkBeginHyper(PVM pVM, VMCPUID idCpu, PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(int)              DBGFR3StackWalkBeginGuestEx(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddrFrame,
                                                        PCDBGFADDRESS pAddrStack,PCDBGFADDRESS pAddrPC,
                                                        DBGFRETURNTYPE enmReturnType, PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(int)              DBGFR3StackWalkBeginHyperEx(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddrFrame,
                                                        PCDBGFADDRESS pAddrStack,PCDBGFADDRESS pAddrPC,
                                                        DBGFRETURNTYPE enmReturnType, PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(PCDBGFSTACKFRAME) DBGFR3StackWalkNext(PCDBGFSTACKFRAME pCurrent);
VMMR3DECL(void)             DBGFR3StackWalkEnd(PCDBGFSTACKFRAME pFirstFrame);




/** Flags to pass to DBGFR3DisasInstrEx().
 * @{ */
/** Disassemble the current guest instruction, with annotations. */
#define DBGF_DISAS_FLAGS_CURRENT_GUEST      RT_BIT(0)
/** Disassemble the current hypervisor instruction, with annotations. */
#define DBGF_DISAS_FLAGS_CURRENT_HYPER      RT_BIT(1)
/** No annotations for current context. */
#define DBGF_DISAS_FLAGS_NO_ANNOTATION      RT_BIT(2)
/** No symbol lookup. */
#define DBGF_DISAS_FLAGS_NO_SYMBOLS         RT_BIT(3)
/** No instruction bytes. */
#define DBGF_DISAS_FLAGS_NO_BYTES           RT_BIT(4)
/** No address in the output. */
#define DBGF_DISAS_FLAGS_NO_ADDRESS         RT_BIT(5)
/** @} */

/** Special flat selector. */
#define DBGF_SEL_FLAT                       1

VMMR3DECL(int) DBGFR3DisasInstrEx(PVM pVM, VMCPUID idCpu, RTSEL Sel, RTGCPTR GCPtr, unsigned fFlags, char *pszOutput, uint32_t cchOutput, uint32_t *pcbInstr);
VMMR3DECL(int) DBGFR3DisasInstrCurrent(PVM pVM, char *pszOutput, uint32_t cbOutput);
VMMR3DECL(int) DBGFR3DisasInstrCurrentLogInternal(PVM pVM, const char *pszPrefix);

/** @def DBGFR3DisasInstrCurrentLog
 * Disassembles the current guest context instruction and writes it to the log.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 */
/** @todo SMP */
#ifdef LOG_ENABLED
# define DBGFR3DisasInstrCurrentLog(pVM, pszPrefix) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3DisasInstrCurrentLogInternal(pVM, pszPrefix); \
    } while (0)
#else
# define DBGFR3DisasInstrCurrentLog(pVM, pszPrefix) do { } while (0)
#endif

VMMR3DECL(int) DBGFR3DisasInstrLogInternal(PVM pVM, RTSEL Sel, RTGCPTR GCPtr);

/** @def DBGFR3DisasInstrLog
 * Disassembles the specified guest context instruction and writes it to the log.
 * Addresses will be attempted resolved to symbols.
 * @thread Any EMT.
 */
#ifdef LOG_ENABLED
# define DBGFR3DisasInstrLog(pVM, Sel, GCPtr) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3DisasInstrLogInternal(pVM, Sel, GCPtr); \
    } while (0)
#else
# define DBGFR3DisasInstrLog(pVM, Sel, GCPtr) do { } while (0)
#endif


VMMR3DECL(int) DBGFR3MemScan(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress);
VMMR3DECL(int) DBGFR3MemRead(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead);
VMMR3DECL(int) DBGFR3MemReadString(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, char *pszBuf, size_t cbBuf);
VMMR3DECL(int) DBGFR3MemWrite(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, void const *pvBuf, size_t cbRead);


/** @name DBGFR3SelQueryInfo flags.
 * @{ */
/** Get the info from the guest descriptor table. */
#define DBGFSELQI_FLAGS_DT_GUEST        UINT32_C(0)
/** Get the info from the shadow descriptor table.
 * Only works in raw-mode.  */
#define DBGFSELQI_FLAGS_DT_SHADOW       UINT32_C(1)
/** @} */
VMMR3DECL(int) DBGFR3SelQueryInfo(PVM pVM, VMCPUID idCpu, RTSEL Sel, uint32_t fFlags, PDBGFSELINFO pSelInfo);


/**
 * Guest OS digger interface identifier.
 *
 * This is for use together with PDBGFR3QueryInterface and is used to
 * obtain access to optional interfaces.
 */
typedef enum DBGFOSINTERFACE
{
    /** The usual invalid entry. */
    DBGFOSINTERFACE_INVALID = 0,
    /** Process info. */
    DBGFOSINTERFACE_PROCESS,
    /** Thread info. */
    DBGFOSINTERFACE_THREAD,
    /** The end of the valid entries. */
    DBGFOSINTERFACE_END,
    /** The usual 32-bit type blowup. */
    DBGFOSINTERFACE_32BIT_HACK = 0x7fffffff
} DBGFOSINTERFACE;
/** Pointer to a Guest OS digger interface identifier. */
typedef DBGFOSINTERFACE *PDBGFOSINTERFACE;
/** Pointer to a const Guest OS digger interface identifier. */
typedef DBGFOSINTERFACE const *PCDBGFOSINTERFACE;


/**
 * Guest OS Digger Registration Record.
 *
 * This is used with the DBGFR3OSRegister() API.
 */
typedef struct DBGFOSREG
{
    /** Magic value (DBGFOSREG_MAGIC). */
    uint32_t u32Magic;
    /** Flags. Reserved. */
    uint32_t fFlags;
    /** The size of the instance data. */
    uint32_t cbData;
    /** Operative System name. */
    char szName[24];

    /**
     * Constructs the instance.
     *
     * @returns VBox status code.
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnConstruct)(PVM pVM, void *pvData);

    /**
     * Destroys the instance.
     *
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(void, pfnDestruct)(PVM pVM, void *pvData);

    /**
     * Probes the guest memory for OS finger prints.
     *
     * No setup or so is performed, it will be followed by a call to pfnInit
     * or pfnRefresh that should take care of that.
     *
     * @returns true if is an OS handled by this module, otherwise false.
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(bool, pfnProbe)(PVM pVM, void *pvData);

    /**
     * Initializes a fresly detected guest, loading symbols and such useful stuff.
     *
     * This is called after pfnProbe.
     *
     * @returns VBox status code.
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(PVM pVM, void *pvData);

    /**
     * Refreshes symbols and stuff following a redetection of the same OS.
     *
     * This is called after pfnProbe.
     *
     * @returns VBox status code.
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnRefresh)(PVM pVM, void *pvData);

    /**
     * Terminates an OS when a new (or none) OS has been detected,
     * and before destruction.
     *
     * This is called after pfnProbe and if needed before pfnDestruct.
     *
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(PVM pVM, void *pvData);

    /**
     * Queries the version of the running OS.
     *
     * This is only called after pfnInit().
     *
     * @returns VBox status code.
     * @param   pVM         Pointer to the shared VM structure.
     * @param   pvData      Pointer to the instance data.
     * @param   pszVersion  Where to store the version string.
     * @param   cchVersion  The size of the version string buffer.
     */
    DECLCALLBACKMEMBER(int, pfnQueryVersion)(PVM pVM, void *pvData, char *pszVersion, size_t cchVersion);

    /**
     * Queries the pointer to a interface.
     *
     * This is called after pfnProbe.
     *
     * @returns Pointer to the interface if available, NULL if not available.
     * @param   pVM     Pointer to the shared VM structure.
     * @param   pvData  Pointer to the instance data.
     * @param   enmIf   The interface identifier.
     */
    DECLCALLBACKMEMBER(void *, pfnQueryInterface)(PVM pVM, void *pvData, DBGFOSINTERFACE enmIf);

    /** Trailing magic (DBGFOSREG_MAGIC). */
    uint32_t u32EndMagic;
} DBGFOSREG;
/** Pointer to a Guest OS digger registration record. */
typedef DBGFOSREG *PDBGFOSREG;
/** Pointer to a const Guest OS digger registration record. */
typedef DBGFOSREG const *PCDBGFOSREG;

/** Magic value for DBGFOSREG::u32Magic and DBGFOSREG::u32EndMagic. (Hitomi Kanehara) */
#define DBGFOSREG_MAGIC     0x19830808

VMMR3DECL(int)      DBGFR3OSRegister(PVM pVM, PCDBGFOSREG pReg);
VMMR3DECL(int)      DBGFR3OSDeregister(PVM pVM, PCDBGFOSREG pReg);
VMMR3DECL(int)      DBGFR3OSDetect(PVM pVM, char *pszName, size_t cchName);
VMMR3DECL(int)      DBGFR3OSQueryNameAndVersion(PVM pVM, char *pszName, size_t cchName, char *pszVersion, size_t cchVersion);
VMMR3DECL(void *)   DBGFR3OSQueryInterface(PVM pVM, DBGFOSINTERFACE enmIf);

/** @} */


__END_DECLS

#endif

