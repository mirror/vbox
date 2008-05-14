/* $Id$ */
/** @file
 * DBGF - Internal header file.
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

#ifndef ___DBGFInternal_h
#define ___DBGFInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/string.h>
#include <iprt/avl.h>
#include <VBox/dbgf.h>


#if !defined(IN_DBGF_R3) && !defined(IN_DBGF_R0) && !defined(IN_DBGF_GC)
# error "Not in DBGF! This is an internal header!"
#endif


/** @defgroup grp_dbgf_int   Internals
 * @ingroup grp_dbgf
 * @internal
 * @{
 */


/** VMM Debugger Command. */
typedef enum DBGFCMD
{
    /** No command.
     * This is assigned to the field by the emulation thread after
     * a command has been completed. */
    DBGFCMD_NO_COMMAND = 0,
    /** Halt the VM. */
    DBGFCMD_HALT,
    /** Resume execution. */
    DBGFCMD_GO,
    /** Single step execution - stepping into calls. */
    DBGFCMD_SINGLE_STEP,
    /** Set a breakpoint. */
    DBGFCMD_BREAKPOINT_SET,
    /** Set a access breakpoint. */
    DBGFCMD_BREAKPOINT_SET_ACCESS,
    /** Set a REM breakpoint. */
    DBGFCMD_BREAKPOINT_SET_REM,
    /** Clear a breakpoint. */
    DBGFCMD_BREAKPOINT_CLEAR,
    /** Enable a breakpoint. */
    DBGFCMD_BREAKPOINT_ENABLE,
    /** Disable a breakpoint. */
    DBGFCMD_BREAKPOINT_DISABLE,
    /** List breakpoints. */
    DBGFCMD_BREAKPOINT_LIST,

    /** Detaches the debugger.
     * Disabling all breakpoints, watch points and the like. */
    DBGFCMD_DETACH_DEBUGGER = 0x7fffffff

} DBGFCMD;

/**
 * VMM Debugger Command.
 */
typedef union DBGFCMDDATA
{
    uint32_t    uDummy;

} DBGFCMDDATA;
/** Pointer to DBGF Command Data. */
typedef DBGFCMDDATA *PDBGFCMDDATA;

/**
 * Info type.
 */
typedef enum DBGFINFOTYPE
{
    /** Invalid. */
    DBGFINFOTYPE_INVALID = 0,
    /** Device owner. */
    DBGFINFOTYPE_DEV,
    /** Driver owner. */
    DBGFINFOTYPE_DRV,
    /** Internal owner. */
    DBGFINFOTYPE_INT,
    /** External owner. */
    DBGFINFOTYPE_EXT
} DBGFINFOTYPE;


/** Pointer to info structure. */
typedef struct DBGFINFO *PDBGFINFO;

/**
 * Info structure.
 */
typedef struct DBGFINFO
{
    /** The flags. */
    uint32_t        fFlags;
    /** Owner type. */
    DBGFINFOTYPE    enmType;
    /** Per type data. */
    union
    {
        /** DBGFINFOTYPE_DEV */
        struct
        {
            /** Device info handler function. */
            PFNDBGFHANDLERDEV   pfnHandler;
            /** The device instance. */
            PPDMDEVINS          pDevIns;
        } Dev;

        /** DBGFINFOTYPE_DRV */
        struct
        {
            /** Driver info handler function. */
            PFNDBGFHANDLERDRV   pfnHandler;
            /** The driver instance. */
            PPDMDRVINS          pDrvIns;
        } Drv;

        /** DBGFINFOTYPE_INT */
        struct
        {
            /** Internal info handler function. */
            PFNDBGFHANDLERINT   pfnHandler;
        } Int;

        /** DBGFINFOTYPE_EXT */
        struct
        {
            /** External info handler function. */
            PFNDBGFHANDLEREXT   pfnHandler;
            /** The user argument. */
            void               *pvUser;
        } Ext;
    } u;

    /** Pointer to the description. */
    const char     *pszDesc;
    /** Pointer to the next info structure. */
    PDBGFINFO       pNext;
    /** The identifier name length. */
    size_t          cchName;
    /** The identifier name. (Extends 'beyond' the struct as usual.) */
    char            szName[1];
} DBGFINFO;


/**
 * Guest OS digger instance.
 */
typedef struct DBGFOS
{
    /** Pointer to the registration record. */
    PCDBGFOSREG pReg;
    /** Pointer to the next OS we've registered. */
    struct DBGFOS *pNext;
    /** The instance data (variable size). */
    uint8_t abData[16];
} DBGFOS;
/** Pointer to guest OS digger instance. */
typedef DBGFOS *PDBGFOS;
/** Pointer to const guest OS digger instance. */
typedef DBGFOS const *PCDBGFOS;


/**
 * Converts a DBGF pointer into a VM pointer.
 * @returns Pointer to the VM structure the CPUM is part of.
 * @param   pDBGF   Pointer to DBGF instance data.
 */
#define DBGF2VM(pDBGF)  ( (PVM)((char*)pDBGF - pDBGF->offVM) )


/**
 * DBGF Data (part of VM)
 */
typedef struct DBGF
{
    /** Offset to the VM structure. */
    RTINT                   offVM;

    /** Debugger Attached flag.
     * Set if a debugger is attached, elsewise it's clear.
     */
    volatile bool           fAttached;

    /** Stopped in the Hypervisor.
     * Set if we're stopped on a trace, breakpoint or assertion inside
     * the hypervisor and have to restrict the available operations.
     */
    volatile bool           fStoppedInHyper;

    /**
     * Ping-Pong construct where the Ping side is the VMM and the Pong side
     * the Debugger.
     */
    RTPINGPONG              PingPong;

    /** The Event to the debugger.
     * The VMM will ping the debugger when the event is ready. The event is
     * either a response to a command or to a break/watch point issued
     * previously.
     */
    DBGFEVENT               DbgEvent;

    /** The Command to the VMM.
     * Operated in an atomic fashion since the VMM will poll on this.
     * This means that a the command data must be written before this member
     * is set. The VMM will reset this member to the no-command state
     * when it have processed it.
     */
    volatile DBGFCMD        enmVMMCmd;
    /** The Command data.
     * Not all commands take data. */
    DBGFCMDDATA             VMMCmdData;

    /** List of registered info handlers. */
    R3PTRTYPE(PDBGFINFO)    pInfoFirst;
    /** Critical section protecting the above list. */
    RTCRITSECT              InfoCritSect;

    /** Range tree containing the loaded symbols of the a VM.
     * This tree will never have blind spots. */
    R3PTRTYPE(AVLRGCPTRTREE) SymbolTree;
    /** Symbol name space. */
    R3PTRTYPE(PRTSTRSPACE)  pSymbolSpace;
    /** Indicates whether DBGFSym.cpp is initialized or not.
     * This part is initialized in a lazy manner for performance reasons. */
    bool                    fSymInited;
    /** Alignment padding. */
    RTUINT                  uAlignment0;

    /** The number of hardware breakpoints. */
    RTUINT                  cHwBreakpoints;
    /** The number of active breakpoints. */
    RTUINT                  cBreakpoints;
    /** Array of hardware breakpoints. (0..3) */
    DBGFBP                  aHwBreakpoints[4];
    /** Array of int 3 and REM breakpoints. (4..)
     * @remark This is currently a fixed size array for reasons of simplicity. */
    DBGFBP                  aBreakpoints[32];
    /** Current active breakpoint (id).
     * This is ~0U if not active. It is set when a execution engine
     * encounters a breakpoint and returns VINF_EM_DBG_BREAKPOINT. This is
     * currently not used for REM breakpoints because of the lazy coupling
     * between VBox and REM. */
    RTUINT                  iActiveBp;
    /** Set if we're singlestepping in raw mode.
     * This is checked and cleared in the \#DB handler. */
    bool                    fSingleSteppingRaw;

    /** The current Guest OS digger. */
    R3PTRTYPE(PDBGFOS)      pCurOS;
    /** The head of the Guest OS digger instances. */
    R3PTRTYPE(PDBGFOS)      pOSHead;
} DBGF;
/** Pointer to DBGF Data. */
typedef DBGF *PDBGF;


extern int  dbgfR3InfoInit(PVM pVM);
extern int  dbgfR3InfoTerm(PVM pVM);
extern void dbgfR3OSTerm(PVM pVM);
extern int  dbgfR3SymInit(PVM pVM);
extern int  dbgfR3SymTerm(PVM pVM);
extern int  dbgfR3BpInit(PVM pVM);



#ifdef IN_RING3

#endif

/** @} */

#endif
