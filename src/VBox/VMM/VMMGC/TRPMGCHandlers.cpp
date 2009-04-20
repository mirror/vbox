/* $Id$ */
/** @file
 * TRPM - Guest Context Trap Handlers, CPP part
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
#define LOG_GROUP LOG_GROUP_TRPM
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/dbgf.h>
#include <VBox/em.h>
#include <VBox/csam.h>
#include <VBox/patm.h>
#include <VBox/mm.h>
#include <VBox/cpum.h>
#include "TRPMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/param.h>

#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/x86.h>
#include <VBox/log.h>
#include <VBox/tm.h>
#include <iprt/asm.h>
#include <iprt/assert.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* still here. MODR/M byte parsing */
#define X86_OPCODE_MODRM_MOD_MASK       0xc0
#define X86_OPCODE_MODRM_REG_MASK       0x38
#define X86_OPCODE_MODRM_RM_MASK        0x07

/** @todo fix/remove/permanent-enable this when DIS/PATM handles invalid lock sequences. */
#define DTRACE_EXPERIMENT


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to a readonly hypervisor trap record. */
typedef const struct TRPMGCHYPER *PCTRPMGCHYPER;

/**
 * A hypervisor trap record.
 * This contains information about a handler for a instruction range.
 *
 * @remark This must match what TRPM_HANDLER outputs.
 */
typedef struct TRPMGCHYPER
{
    /** The start address. */
    uintptr_t uStartEIP;
    /** The end address. (exclusive)
     * If NULL the it's only for the instruction at pvStartEIP. */
    uintptr_t uEndEIP;
    /**
     * The handler.
     *
     * @returns VBox status code
     *          VINF_SUCCESS means we've handled the trap.
     *          Any other error code means returning to the host context.
     * @param   pVM             The VM handle.
     * @param   pRegFrame       The register frame.
     * @param   uUser           The user argument.
     */
    DECLRCCALLBACKMEMBER(int, pfnHandler, (PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser));
    /** Whatever the handler desires to put here. */
    uintptr_t uUser;
} TRPMGCHYPER;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
__BEGIN_DECLS
/** Defined in VMMGC0.asm or VMMGC99.asm.
 * @{ */
extern const TRPMGCHYPER g_aTrap0bHandlers[1];
extern const TRPMGCHYPER g_aTrap0bHandlersEnd[1];
extern const TRPMGCHYPER g_aTrap0dHandlers[1];
extern const TRPMGCHYPER g_aTrap0dHandlersEnd[1];
extern const TRPMGCHYPER g_aTrap0eHandlers[1];
extern const TRPMGCHYPER g_aTrap0eHandlersEnd[1];
/** @} */
__END_DECLS


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS /* addressed from asm (not called so no DECLASM). */
DECLCALLBACK(int) trpmGCTrapInGeneric(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser);
__END_DECLS



/**
 * Exits the trap, called when exiting a trap handler.
 *
 * Will reset the trap if it's not a guest trap or the trap
 * is already handled. Will process resume guest FFs.
 *
 * @returns rc, can be adjusted if its VINF_SUCCESS or something really bad
 *          happened.
 * @param   pVM         VM handle.
 * @param   rc          The VBox status code to return.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 */
static int trpmGCExitTrap(PVM pVM, int rc, PCPUMCTXCORE pRegFrame)
{
    PVMCPU   pVCpu = VMMGetCpu0(pVM);
    uint32_t uOldActiveVector = pVCpu->trpm.s.uActiveVector;
    NOREF(uOldActiveVector);

    /* Reset trap? */
    if (    rc != VINF_EM_RAW_GUEST_TRAP
        &&  rc != VINF_EM_RAW_RING_SWITCH_INT)
        pVCpu->trpm.s.uActiveVector = ~0;

#ifdef VBOX_HIGH_RES_TIMERS_HACK
    /*
     * We should poll the timers occationally.
     * We must *NOT* do this too frequently as it adds a significant overhead
     * and it'll kill us if the trap load is high. (See #1354.)
     * (The heuristic is not very intelligent, we should really check trap
     * frequency etc. here, but alas, we lack any such information atm.)
     */
    static unsigned s_iTimerPoll = 0;
    if (rc == VINF_SUCCESS)
    {
        if (!(++s_iTimerPoll & 0xf))
        {
            uint64_t cTicks = TMTimerPoll(pVM); NOREF(cTicks);
            Log2(("TMTimerPoll at %08RX32 returned %RX64 (VM_FF_TIMER=%d)\n", pRegFrame->eip, cTicks, VM_FF_ISPENDING(pVM, VM_FF_TIMER)));
        }
    }
    else
        s_iTimerPoll = 0;
#endif

    /* Clear pending inhibit interrupt state if required. (necessary for dispatching interrupts later on) */
    if (VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS))
    {
        Log2(("VM_FF_INHIBIT_INTERRUPTS at %08RX32 successor %RGv\n", pRegFrame->eip, EMGetInhibitInterruptsPC(pVM, pVCpu)));
        if (pRegFrame->eip != EMGetInhibitInterruptsPC(pVM, pVCpu))
        {
            /** @note we intentionally don't clear VM_FF_INHIBIT_INTERRUPTS here if the eip is the same as the inhibited instr address.
             *  Before we are able to execute this instruction in raw mode (iret to guest code) an external interrupt might
             *  force a world switch again. Possibly allowing a guest interrupt to be dispatched in the process. This could
             *  break the guest. Sounds very unlikely, but such timing sensitive problem are not as rare as you might think.
             */
            VM_FF_CLEAR(pVM, VM_FF_INHIBIT_INTERRUPTS);
        }
    }

    /*
     * Pending resume-guest-FF?
     * Or pending (A)PIC interrupt? Windows XP will crash if we delay APIC interrupts.
     */
    if (    rc == VINF_SUCCESS
        &&  VM_FF_ISPENDING(pVM, VM_FF_TO_R3 | VM_FF_TIMER | VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC | VM_FF_REQUEST
                               | VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL | VM_FF_PGM_NO_MEMORY))
    {
        /* The out of memory condition naturally outrang the others. */
        if (RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)))
            rc = VINF_EM_NO_MEMORY;
        /* Pending Ring-3 action. */
        else if (VM_FF_ISPENDING(pVM, VM_FF_TO_R3))
        {
            VM_FF_CLEAR(pVM, VM_FF_TO_R3);
            rc = VINF_EM_RAW_TO_R3;
        }
        /* Pending timer action. */
        else if (VM_FF_ISPENDING(pVM, VM_FF_TIMER))
            rc = VINF_EM_RAW_TIMER_PENDING;
        /* Pending interrupt: dispatch it. */
        else if (    VM_FF_ISPENDING(pVM, VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC)
                 && !VM_FF_ISSET(pVM, VM_FF_INHIBIT_INTERRUPTS)
                 &&  PATMAreInterruptsEnabledByCtxCore(pVM, pRegFrame)
           )
        {
            uint8_t u8Interrupt;
            rc = PDMGetInterrupt(pVM, &u8Interrupt);
            Log(("trpmGCExitTrap: u8Interrupt=%d (%#x) rc=%Rrc\n", u8Interrupt, u8Interrupt, rc));
            AssertFatalMsgRC(rc, ("PDMGetInterrupt failed with %Rrc\n", rc));
            rc = TRPMForwardTrap(pVCpu, pRegFrame, (uint32_t)u8Interrupt, 0, TRPM_TRAP_NO_ERRORCODE, TRPM_HARDWARE_INT, uOldActiveVector);
            /* can't return if successful */
            Assert(rc != VINF_SUCCESS);

            /* Stop the profile counter that was started in TRPMGCHandlersA.asm */
            Assert(uOldActiveVector <= 16);
            STAM_PROFILE_ADV_STOP(&pVM->trpm.s.aStatGCTraps[uOldActiveVector], a);

            /* Assert the trap and go to the recompiler to dispatch it. */
            TRPMAssertTrap(pVCpu, u8Interrupt, TRPM_HARDWARE_INT);

            STAM_PROFILE_ADV_START(&pVM->trpm.s.aStatGCTraps[uOldActiveVector], a);
            rc = VINF_EM_RAW_INTERRUPT_PENDING;
        }
        /*
         * Try sync CR3?
         */
        else if (VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL))
#if 1
            rc = PGMSyncCR3(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR3(pVCpu), CPUMGetGuestCR4(pVCpu), VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3));
#else
            rc = VINF_PGM_SYNC_CR3;
#endif
        /* Pending request packets might contain actions that need immediate attention, such as pending hardware interrupts. */
        else if (VM_FF_ISPENDING(pVM, VM_FF_REQUEST))
            rc = VINF_EM_PENDING_REQUEST;
    }

    AssertMsg(     rc != VINF_SUCCESS
              ||   (   pRegFrame->eflags.Bits.u1IF
                    && ( pRegFrame->eflags.Bits.u2IOPL < (unsigned)(pRegFrame->ss & X86_SEL_RPL) || pRegFrame->eflags.Bits.u1VM))
              , ("rc=%Rrc\neflags=%RX32 ss=%RTsel IOPL=%d\n", rc, pRegFrame->eflags.u32, pRegFrame->ss, pRegFrame->eflags.Bits.u2IOPL));
    return rc;
}


/**
 * \#DB (Debug event) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap01Handler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    RTGCUINTREG uDr6 = ASMGetAndClearDR6();
    PVM         pVM = TRPM2VM(pTrpmCpu);
    PVMCPU      pVCpu = VMMGetCpu0(pVM);

    LogFlow(("TRPMGC01: cs:eip=%04x:%08x uDr6=%RTreg\n", pRegFrame->cs, pRegFrame->eip, uDr6));

    /*
     * We currently don't make sure of the X86_DR7_GD bit, but
     * there might come a time when we do.
     */
    if ((uDr6 & X86_DR6_BD) == X86_DR6_BD)
    {
        AssertReleaseMsgFailed(("X86_DR6_BD isn't used, but it's set! dr7=%RTreg(%RTreg) dr6=%RTreg\n",
                                ASMGetDR7(), CPUMGetHyperDR7(pVCpu), uDr6));
        return VERR_NOT_IMPLEMENTED;
    }

    AssertReleaseMsg(!(uDr6 & X86_DR6_BT), ("X86_DR6_BT is impossible!\n"));

    /*
     * Now leave the rest to the DBGF.
     */
    int rc = DBGFGCTrap01Handler(pVM, pRegFrame, uDr6);
    if (rc == VINF_EM_RAW_GUEST_TRAP)
        CPUMSetGuestDR6(pVCpu, uDr6);

    rc = trpmGCExitTrap(pVM, rc, pRegFrame);
    Log6(("TRPMGC01: %Rrc (%04x:%08x %RTreg)\n", rc, pRegFrame->cs, pRegFrame->eip, uDr6));
    return rc;
}


/**
 * NMI handler, for when we are using NMIs to debug things.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 * @remark  This is not hooked up unless you're building with VBOX_WITH_NMI defined.
 */
DECLASM(int) TRPMGCTrap02Handler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    LogFlow(("TRPMGCTrap02Handler: cs:eip=%04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
    RTLogComPrintf("TRPMGCTrap02Handler: cs:eip=%04x:%08x\n", pRegFrame->cs, pRegFrame->eip);
    return VERR_TRPM_DONT_PANIC;
}


/**
 * \#BP (Breakpoint) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap03Handler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    LogFlow(("TRPMGC03: %04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
    PVM pVM = TRPM2VM(pTrpmCpu);
    int rc;

    /*
     * Both PATM are using INT3s, let them have a go first.
     */
    if (    (pRegFrame->ss & X86_SEL_RPL) == 1
        &&  !pRegFrame->eflags.Bits.u1VM)
    {
        rc = PATMHandleInt3PatchTrap(pVM, pRegFrame);
        if (rc == VINF_SUCCESS || rc == VINF_EM_RAW_EMULATE_INSTR || rc == VINF_PATM_PATCH_INT3 || rc == VINF_PATM_DUPLICATE_FUNCTION)
        {
            rc = trpmGCExitTrap(pVM, rc, pRegFrame);
            Log6(("TRPMGC03: %Rrc (%04x:%08x) (PATM)\n", rc, pRegFrame->cs, pRegFrame->eip));
            return rc;
        }
    }
    rc = DBGFGCTrap03Handler(pVM, pRegFrame);

    /* anything we should do with this? Schedule it in GC? */
    rc = trpmGCExitTrap(pVM, rc, pRegFrame);
    Log6(("TRPMGC03: %Rrc (%04x:%08x)\n", rc, pRegFrame->cs, pRegFrame->eip));
    return rc;
}


/**
 * Trap handler for illegal opcode fault (\#UD).
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap06Handler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    LogFlow(("TRPMGC06: %04x:%08x efl=%x\n", pRegFrame->cs, pRegFrame->eip, pRegFrame->eflags.u32));
    PVM     pVM = TRPM2VM(pTrpmCpu);
    PVMCPU  pVCpu = VMMGetCpu0(pVM);
    int     rc;

    if (CPUMGetGuestCPL(pVCpu, pRegFrame) == 0)
    {
        /*
         * Decode the instruction.
         */
        RTGCPTR PC;
        rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &PC);
        if (RT_FAILURE(rc))
        {
            Log(("TRPMGCTrap06Handler: Failed to convert %RTsel:%RX32 (cpl=%d) - rc=%Rrc !!\n", pRegFrame->cs, pRegFrame->eip, pRegFrame->ss & X86_SEL_RPL, rc));
            rc = trpmGCExitTrap(pVM, VINF_EM_RAW_GUEST_TRAP, pRegFrame);
            Log6(("TRPMGC06: %Rrc (%04x:%08x) (SELM)\n", rc, pRegFrame->cs, pRegFrame->eip));
            return rc;
        }

        DISCPUSTATE Cpu;
        uint32_t    cbOp;
        rc = EMInterpretDisasOneEx(pVM, pVCpu, (RTGCUINTPTR)PC, pRegFrame, &Cpu, &cbOp);
        if (RT_FAILURE(rc))
        {
            rc = trpmGCExitTrap(pVM, VINF_EM_RAW_EMULATE_INSTR, pRegFrame);
            Log6(("TRPMGC06: %Rrc (%04x:%08x) (EM)\n", rc, pRegFrame->cs, pRegFrame->eip));
            return rc;
        }

        /*
         * UD2 in a patch?
         */
        if (    Cpu.pCurInstr->opcode == OP_ILLUD2
            &&  PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip))
        {
            rc = PATMGCHandleIllegalInstrTrap(pVM, pRegFrame);
            /** @todo  These tests are completely unnecessary, should just follow the
             *         flow and return at the end of the function. */
            if (    rc == VINF_SUCCESS
                ||  rc == VINF_EM_RAW_EMULATE_INSTR
                ||  rc == VINF_PATM_DUPLICATE_FUNCTION
                ||  rc == VINF_PATM_PENDING_IRQ_AFTER_IRET
                ||  rc == VINF_EM_RESCHEDULE)
            {
                rc = trpmGCExitTrap(pVM, rc, pRegFrame);
                Log6(("TRPMGC06: %Rrc (%04x:%08x) (PATM)\n", rc, pRegFrame->cs, pRegFrame->eip));
                return rc;
            }
        }
        /*
         * Speed up dtrace and don't entrust invalid lock sequences to the recompiler.
         */
        else if (Cpu.prefix & PREFIX_LOCK)
        {
            Log(("TRPMGCTrap06Handler: pc=%08x op=%d\n", pRegFrame->eip, Cpu.pCurInstr->opcode));
#ifdef DTRACE_EXPERIMENT /** @todo fix/remove/permanent-enable this when DIS/PATM handles invalid lock sequences. */
            Assert(!PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip));
            rc = TRPMForwardTrap(pVCpu, pRegFrame, 0x6, 0, TRPM_TRAP_NO_ERRORCODE, TRPM_TRAP, 0x6);
            Assert(rc == VINF_EM_RAW_GUEST_TRAP);
#else
            rc = VINF_EM_RAW_EMULATE_INSTR;
#endif
        }
        /*
         * Handle MONITOR - it causes an #UD exception instead of #GP when not executed in ring 0.
         */
        else if (Cpu.pCurInstr->opcode == OP_MONITOR)
        {
            uint32_t cbIgnored;
            rc = EMInterpretInstructionCPU(pVM, pVCpu, &Cpu, pRegFrame, PC, &cbIgnored);
            if (RT_LIKELY(RT_SUCCESS(rc)))
                pRegFrame->eip += Cpu.opsize;
        }
        /* Never generate a raw trap here; it might be an instruction, that requires emulation. */
        else
            rc = VINF_EM_RAW_EMULATE_INSTR;
    }
    else
    {
        rc = TRPMForwardTrap(pVCpu, pRegFrame, 0x6, 0, TRPM_TRAP_NO_ERRORCODE, TRPM_TRAP, 0x6);
        Assert(rc == VINF_EM_RAW_GUEST_TRAP);
    }

    rc = trpmGCExitTrap(pVM, rc, pRegFrame);
    Log6(("TRPMGC06: %Rrc (%04x:%08x)\n", rc, pRegFrame->cs, pRegFrame->eip));
    return rc;
}


/**
 * Trap handler for device not present fault (\#NM).
 *
 * Device not available, FP or (F)WAIT instruction.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap07Handler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    LogFlow(("TRPMGC07: %04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
    PVM     pVM   = TRPM2VM(pTrpmCpu);
    PVMCPU  pVCpu = VMMGetCpu0(pVM);

    int rc = CPUMHandleLazyFPU(pVCpu);
    rc = trpmGCExitTrap(pVM, rc, pRegFrame);
    Log6(("TRPMGC07: %Rrc (%04x:%08x)\n", rc, pRegFrame->cs, pRegFrame->eip));
    return rc;
}


/**
 * \#NP ((segment) Not Present) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap0bHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    LogFlow(("TRPMGC0b: %04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
    PVM pVM = TRPM2VM(pTrpmCpu);

    /*
     * Try to detect instruction by opcode which caused trap.
     * XXX note: this code may cause \#PF (trap e) or \#GP (trap d) while
     * accessing user code. need to handle it somehow in future!
     */
    RTGCPTR GCPtr;
    if (SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &GCPtr) == VINF_SUCCESS)
    {
        uint8_t *pu8Code = (uint8_t *)(uintptr_t)GCPtr;

        /*
         * First skip possible instruction prefixes, such as:
         *      OS, AS
         *      CS:, DS:, ES:, SS:, FS:, GS:
         *      REPE, REPNE
         *
         * note: Currently we supports only up to 4 prefixes per opcode, more
         *       prefixes (normally not used anyway) will cause trap d in guest.
         * note: Instruction length in IA-32 may be up to 15 bytes, we dont
         *       check this issue, its too hard.
         */
        for (unsigned i = 0; i < 4; i++)
        {
            if (    pu8Code[0] != 0xf2     /* REPNE/REPNZ */
                &&  pu8Code[0] != 0xf3     /* REP/REPE/REPZ */
                &&  pu8Code[0] != 0x2e     /* CS: */
                &&  pu8Code[0] != 0x36     /* SS: */
                &&  pu8Code[0] != 0x3e     /* DS: */
                &&  pu8Code[0] != 0x26     /* ES: */
                &&  pu8Code[0] != 0x64     /* FS: */
                &&  pu8Code[0] != 0x65     /* GS: */
                &&  pu8Code[0] != 0x66     /* OS */
                &&  pu8Code[0] != 0x67     /* AS */
               )
                break;
            pu8Code++;
        }

        /*
         * Detect right switch using a callgate.
         *
         * We recognize the following causes for the trap 0b:
         *      CALL FAR, CALL FAR []
         *      JMP FAR, JMP FAR []
         *      IRET (may cause a task switch)
         *
         * Note: we can't detect whether the trap was caused by a call to a
         *       callgate descriptor or it is a real trap 0b due to a bad selector.
         *       In both situations we'll pass execution to our recompiler so we don't
         *       have to worry.
         *       If we wanted to do better detection, we have set GDT entries to callgate
         *       descriptors pointing to our own handlers.
         */
        /** @todo not sure about IRET, may generate Trap 0d (\#GP), NEED TO CHECK! */
        if (    pu8Code[0] == 0x9a       /* CALL FAR */
            ||  (    pu8Code[0] == 0xff  /* CALL FAR [] */
                 && (pu8Code[1] & X86_OPCODE_MODRM_REG_MASK) == 0x18)
            ||  pu8Code[0] == 0xea       /* JMP FAR */
            ||  (    pu8Code[0] == 0xff  /* JMP FAR [] */
                 && (pu8Code[1] & X86_OPCODE_MODRM_REG_MASK) == 0x28)
            ||  pu8Code[0] == 0xcf       /* IRET */
           )
        {
            /*
             * Got potential call to callgate.
             * We simply return execution to the recompiler to do emulation
             * starting from the instruction which caused the trap.
             */
            pTrpmCpu->uActiveVector = ~0;
            Log6(("TRPMGC0b: %Rrc (%04x:%08x) (CG)\n", VINF_EM_RAW_RING_SWITCH, pRegFrame->cs, pRegFrame->eip));
            return VINF_EM_RAW_RING_SWITCH;
        }
    }

    /*
     * Pass trap 0b as is to the recompiler in all other cases.
     */
    Log6(("TRPMGC0b: %Rrc (%04x:%08x)\n", VINF_EM_RAW_GUEST_TRAP, pRegFrame->cs, pRegFrame->eip));
    return VINF_EM_RAW_GUEST_TRAP;
}


/**
 * \#GP (General Protection Fault) handler for Ring-0 privileged instructions.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   pCpu        The opcode info.
 * @param   PC          The program counter corresponding to cs:eip in pRegFrame.
 */
static int trpmGCTrap0dHandlerRing0(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, RTGCPTR PC)
{
    int     rc;
    PVMCPU  pVCpu = VMMGetCpu0(pVM);

    /*
     * Try handle it here, if not return to HC and emulate/interpret it there.
     */
    switch (pCpu->pCurInstr->opcode)
    {
        case OP_INT3:
            /*
             * Little hack to make the code below not fail
             */
            pCpu->param1.flags  = USE_IMMEDIATE8;
            pCpu->param1.parval = 3;
            /* fallthru */
        case OP_INT:
        {
            Assert(pCpu->param1.flags & USE_IMMEDIATE8);
            Assert(!(PATMIsPatchGCAddr(pVM, (RTRCPTR)PC)));
            if (pCpu->param1.parval == 3)
            {
                /* Int 3 replacement patch? */
                if (PATMHandleInt3PatchTrap(pVM, pRegFrame) == VINF_SUCCESS)
                {
                    AssertFailed();
                    return trpmGCExitTrap(pVM, VINF_SUCCESS, pRegFrame);
                }
            }
            rc = TRPMForwardTrap(pVCpu, pRegFrame, (uint32_t)pCpu->param1.parval, pCpu->opsize, TRPM_TRAP_NO_ERRORCODE, TRPM_SOFTWARE_INT, 0xd);
            if (RT_SUCCESS(rc) && rc != VINF_EM_RAW_GUEST_TRAP)
                return trpmGCExitTrap(pVM, VINF_SUCCESS, pRegFrame);

            pVCpu->trpm.s.uActiveVector = (pVCpu->trpm.s.uActiveErrorCode & X86_TRAP_ERR_SEL_MASK) >> X86_TRAP_ERR_SEL_SHIFT;
            pVCpu->trpm.s.enmActiveType = TRPM_SOFTWARE_INT;
            return trpmGCExitTrap(pVM, VINF_EM_RAW_RING_SWITCH_INT, pRegFrame);
        }

#ifdef PATM_EMULATE_SYSENTER
        case OP_SYSEXIT:
        case OP_SYSRET:
            rc = PATMSysCall(pVM, pRegFrame, pCpu);
            return trpmGCExitTrap(pVM, rc, pRegFrame);
#endif

        case OP_HLT:
            /* If it's in patch code, defer to ring-3. */
            if (PATMIsPatchGCAddr(pVM, (RTRCPTR)PC))
                break;

            pRegFrame->eip += pCpu->opsize;
            return trpmGCExitTrap(pVM, VINF_EM_HALT, pRegFrame);


        /*
         * These instructions are used by PATM and CASM for finding
         * dangerous non-trapping instructions. Thus, since all
         * scanning and patching is done in ring-3 we'll have to
         * return to ring-3 on the first encounter of these instructions.
         */
        case OP_MOV_CR:
        case OP_MOV_DR:
            /* We can safely emulate control/debug register move instructions in patched code. */
            if (    !PATMIsPatchGCAddr(pVM, (RTRCPTR)PC)
                &&  !CSAMIsKnownDangerousInstr(pVM, (RTRCPTR)PC))
                break;
        case OP_INVLPG:
        case OP_LLDT:
        case OP_STI:
        case OP_RDTSC:  /* just in case */
        case OP_RDPMC:
        case OP_CLTS:
        case OP_WBINVD: /* nop */
        case OP_RDMSR:
        case OP_WRMSR:
        {
            uint32_t cbIgnored;
            rc = EMInterpretInstructionCPU(pVM, pVCpu, pCpu, pRegFrame, PC, &cbIgnored);
            if (RT_SUCCESS(rc))
                pRegFrame->eip += pCpu->opsize;
            else if (rc == VERR_EM_INTERPRETER)
                rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
            return trpmGCExitTrap(pVM, rc, pRegFrame);
        }
    }

    return trpmGCExitTrap(pVM, VINF_EM_RAW_EXCEPTION_PRIVILEGED, pRegFrame);
}


/**
 * \#GP (General Protection Fault) handler for Ring-3.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   pCpu        The opcode info.
 * @param   PC          The program counter corresponding to cs:eip in pRegFrame.
 */
static int trpmGCTrap0dHandlerRing3(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu, RTGCPTR PC)
{
    int     rc;
    PVMCPU  pVCpu = VMMGetCpu0(pVM);

    Assert(!pRegFrame->eflags.Bits.u1VM);

    switch (pCpu->pCurInstr->opcode)
    {
        /*
         * INT3 and INT xx are ring-switching.
         * (The shadow IDT will have set the entries to DPL=0, that's why we're here.)
         */
        case OP_INT3:
            /*
             * Little hack to make the code below not fail
             */
            pCpu->param1.flags  = USE_IMMEDIATE8;
            pCpu->param1.parval = 3;
            /* fall thru */
        case OP_INT:
        {
            Assert(pCpu->param1.flags & USE_IMMEDIATE8);
            rc = TRPMForwardTrap(pVCpu, pRegFrame, (uint32_t)pCpu->param1.parval, pCpu->opsize, TRPM_TRAP_NO_ERRORCODE, TRPM_SOFTWARE_INT, 0xd);
            if (RT_SUCCESS(rc) && rc != VINF_EM_RAW_GUEST_TRAP)
                return trpmGCExitTrap(pVM, VINF_SUCCESS, pRegFrame);

            pVCpu->trpm.s.uActiveVector = (pVCpu->trpm.s.uActiveErrorCode & X86_TRAP_ERR_SEL_MASK) >> X86_TRAP_ERR_SEL_SHIFT;
            pVCpu->trpm.s.enmActiveType = TRPM_SOFTWARE_INT;
            return trpmGCExitTrap(pVM, VINF_EM_RAW_RING_SWITCH_INT, pRegFrame);
        }

        /*
         * SYSCALL, SYSENTER, INTO and BOUND are also ring-switchers.
         */
        case OP_SYSCALL:
        case OP_SYSENTER:
#ifdef PATM_EMULATE_SYSENTER
            rc = PATMSysCall(pVM, pRegFrame, pCpu);
            if (rc == VINF_SUCCESS)
                return trpmGCExitTrap(pVM, VINF_SUCCESS, pRegFrame);
            /* else no break; */
#endif
        case OP_BOUND:
        case OP_INTO:
            pVCpu->trpm.s.uActiveVector = ~0;
            return trpmGCExitTrap(pVM, VINF_EM_RAW_RING_SWITCH, pRegFrame);

        /*
         * Handle virtualized TSC & PMC reads, just in case.
         */
        case OP_RDTSC:
        case OP_RDPMC:
        {
            uint32_t cbIgnored;
            rc = EMInterpretInstructionCPU(pVM, pVCpu, pCpu, pRegFrame, PC, &cbIgnored);
            if (RT_SUCCESS(rc))
                pRegFrame->eip += pCpu->opsize;
            else if (rc == VERR_EM_INTERPRETER)
                rc = VINF_EM_RAW_EXCEPTION_PRIVILEGED;
            return trpmGCExitTrap(pVM, rc, pRegFrame);
        }

        /*
         * STI and CLI are I/O privileged, i.e. if IOPL
         */
        case OP_STI:
        case OP_CLI:
        {
            uint32_t efl = CPUMRawGetEFlags(pVCpu, pRegFrame);
            if (X86_EFL_GET_IOPL(efl) >= (unsigned)(pRegFrame->ss & X86_SEL_RPL))
            {
                LogFlow(("trpmGCTrap0dHandlerRing3: CLI/STI -> REM\n"));
                return trpmGCExitTrap(pVM, VINF_EM_RESCHEDULE_REM, pRegFrame);
            }
            LogFlow(("trpmGCTrap0dHandlerRing3: CLI/STI -> #GP(0)\n"));
            break;
        }
    }

    /*
     * A genuine guest fault.
     */
    return trpmGCExitTrap(pVM, VINF_EM_RAW_GUEST_TRAP, pRegFrame);
}


/**
 * Emulates RDTSC for the \#GP handler.
 *
 * @returns VINF_SUCCESS or VINF_EM_RAW_EMULATE_INSTR.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pRegFrame   Pointer to the registre frame for the trap.
 *                      This will be updated on successful return.
 */
DECLINLINE(int) trpmGCTrap0dHandlerRdTsc(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    STAM_COUNTER_INC(&pVM->trpm.s.StatTrap0dRdTsc);

    if (CPUMGetGuestCR4(pVCpu) & X86_CR4_TSD)
        return trpmGCExitTrap(pVM, VINF_EM_RAW_EMULATE_INSTR, pRegFrame); /* will trap (optimize later). */

    uint64_t uTicks = TMCpuTickGet(pVM);
    pRegFrame->eax = uTicks;
    pRegFrame->edx = uTicks >> 32;
    pRegFrame->eip += 2;
    return trpmGCExitTrap(pVM, VINF_SUCCESS, pRegFrame);
}


/**
 * \#GP (General Protection Fault) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 */
static int trpmGCTrap0dHandler(PVM pVM, PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    LogFlow(("trpmGCTrap0dHandler: cs:eip=%RTsel:%08RX32 uErr=%RGv\n", pRegFrame->ss, pRegFrame->eip, pTrpmCpu->uActiveErrorCode));

    /*
     * Convert and validate CS.
     */
    STAM_PROFILE_START(&pVM->trpm.s.StatTrap0dDisasm, a);
    RTGCPTR PC;
    uint32_t cBits;
    int rc = SELMValidateAndConvertCSAddrGCTrap(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs,
                                                (RTGCPTR)pRegFrame->eip, &PC, &cBits);
    if (RT_FAILURE(rc))
    {
        Log(("trpmGCTrap0dHandler: Failed to convert %RTsel:%RX32 (cpl=%d) - rc=%Rrc !!\n",
             pRegFrame->cs, pRegFrame->eip, pRegFrame->ss & X86_SEL_RPL, rc));
        STAM_PROFILE_STOP(&pVM->trpm.s.StatTrap0dDisasm, a);
        return trpmGCExitTrap(pVM, VINF_EM_RAW_EMULATE_INSTR, pRegFrame);
    }

    /*
     * Optimize RDTSC traps.
     * Some guests (like Solaris) are using RDTSC all over the place and
     * will end up trapping a *lot* because of that.
     */
    if (   !pRegFrame->eflags.Bits.u1VM
        && ((uint8_t *)PC)[0] == 0x0f
        && ((uint8_t *)PC)[1] == 0x31)
    {
        STAM_PROFILE_STOP(&pVM->trpm.s.StatTrap0dDisasm, a);
        return trpmGCTrap0dHandlerRdTsc(pVM, pRegFrame);
    }

    /*
     * Disassemble the instruction.
     */
    DISCPUSTATE Cpu;
    uint32_t    cbOp;
    rc = DISCoreOneEx((RTGCUINTPTR)PC, cBits == 32 ? CPUMODE_32BIT : cBits == 16 ? CPUMODE_16BIT : CPUMODE_64BIT,
                      NULL, NULL, &Cpu, &cbOp);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("DISCoreOneEx failed to PC=%RGv rc=%Rrc\n", PC, rc));
        STAM_PROFILE_STOP(&pVM->trpm.s.StatTrap0dDisasm, a);
        return trpmGCExitTrap(pVM, VINF_EM_RAW_EMULATE_INSTR, pRegFrame);
    }
    STAM_PROFILE_STOP(&pVM->trpm.s.StatTrap0dDisasm, a);

    /*
     * Deal with I/O port access.
     */
    if (    pVCpu->trpm.s.uActiveErrorCode == 0
        &&  (Cpu.pCurInstr->optype & OPTYPE_PORTIO))
    {
        rc = EMInterpretPortIO(pVM, pVCpu, pRegFrame, &Cpu, cbOp);
        return trpmGCExitTrap(pVM, rc, pRegFrame);
    }

    /*
     * Deal with Ring-0 (privileged instructions)
     */
    if (    (pRegFrame->ss & X86_SEL_RPL) <= 1
        &&  !pRegFrame->eflags.Bits.u1VM)
        return trpmGCTrap0dHandlerRing0(pVM, pRegFrame, &Cpu, PC);

    /*
     * Deal with Ring-3 GPs.
     */
    if (!pRegFrame->eflags.Bits.u1VM)
        return trpmGCTrap0dHandlerRing3(pVM, pRegFrame, &Cpu, PC);

    /*
     * Deal with v86 code.
     *
     * We always set IOPL to zero which makes e.g. pushf fault in V86
     * mode. The guest might use IOPL=3 and therefore not expect a #GP.
     * Simply fall back to the recompiler to emulate this instruction if
     * that's the case. To get the correct we must use CPUMRawGetEFlags.
     */
    X86EFLAGS eflags;
    eflags.u32 = CPUMRawGetEFlags(pVCpu, pRegFrame); /* Get the correct value. */
    Log3(("TRPM #GP V86: cs:eip=%04x:%08x IOPL=%d efl=%08x\n", pRegFrame->cs, pRegFrame->eip, eflags.Bits.u2IOPL, eflags.u));
    if (eflags.Bits.u2IOPL != 3)
    {
        Assert(eflags.Bits.u2IOPL == 0);

        int rc = TRPMForwardTrap(pVCpu, pRegFrame, 0xD, 0, TRPM_TRAP_HAS_ERRORCODE, TRPM_TRAP, 0xd);
        Assert(rc == VINF_EM_RAW_GUEST_TRAP);
        return trpmGCExitTrap(pVM, rc, pRegFrame);
    }
    return trpmGCExitTrap(pVM, VINF_EM_RAW_EMULATE_INSTR, pRegFrame);
}


/**
 * \#GP (General Protection Fault) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap0dHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    PVM pVM = TRPM2VM(pTrpmCpu);
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    LogFlow(("TRPMGC0d: %04x:%08x err=%x\n", pRegFrame->cs, pRegFrame->eip, (uint32_t)pVCpu->trpm.s.uActiveErrorCode));

    int rc = trpmGCTrap0dHandler(pVM, pTrpmCpu, pRegFrame);
    switch (rc)
    {
        case VINF_EM_RAW_GUEST_TRAP:
        case VINF_EM_RAW_EXCEPTION_PRIVILEGED:
            if (PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip))
                rc = VINF_PATM_PATCH_TRAP_GP;
            break;

        case VINF_EM_RAW_INTERRUPT_PENDING:
            Assert(TRPMHasTrap(pVCpu));
            /* no break; */
        case VINF_PGM_SYNC_CR3: /** @todo Check this with Sander. */
        case VINF_EM_RAW_EMULATE_INSTR:
        case VINF_IOM_HC_IOPORT_READ:
        case VINF_IOM_HC_IOPORT_WRITE:
        case VINF_IOM_HC_MMIO_WRITE:
        case VINF_IOM_HC_MMIO_READ:
        case VINF_IOM_HC_MMIO_READ_WRITE:
        case VINF_PATM_PATCH_INT3:
        case VINF_EM_NO_MEMORY:
        case VINF_EM_RAW_TO_R3:
        case VINF_EM_RAW_TIMER_PENDING:
        case VINF_EM_PENDING_REQUEST:
        case VINF_EM_HALT:
        case VINF_SUCCESS:
            break;

        default:
            AssertMsg(PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip) == false, ("return code %d\n", rc));
            break;
        }
    Log6(("TRPMGC0d: %Rrc (%04x:%08x)\n", rc, pRegFrame->cs, pRegFrame->eip));
    return rc;
}


/**
 * \#PF (Page Fault) handler.
 *
 * Calls PGM which does the actual handling.
 *
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCTrap0eHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    PVM     pVM = TRPM2VM(pTrpmCpu);
    PVMCPU  pVCpu = VMMGetCpu0(pVM);

    LogFlow(("TRPMGC0e: %04x:%08x err=%x cr2=%08x\n", pRegFrame->cs, pRegFrame->eip, (uint32_t)pVCpu->trpm.s.uActiveErrorCode, (uint32_t)pVCpu->trpm.s.uActiveCR2));

    /*
     * This is all PGM stuff.
     */
    int rc = PGMTrap0eHandler(pVCpu, pVCpu->trpm.s.uActiveErrorCode, pRegFrame, (RTGCPTR)pVCpu->trpm.s.uActiveCR2);
    switch (rc)
    {
        case VINF_EM_RAW_EMULATE_INSTR:
        case VINF_EM_RAW_EMULATE_INSTR_PD_FAULT:
        case VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT:
        case VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT:
        case VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT:
        case VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT:
            if (PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip))
                rc = VINF_PATCH_EMULATE_INSTR;
            break;

        case VINF_EM_RAW_GUEST_TRAP:
            if (PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip))
                return VINF_PATM_PATCH_TRAP_PF;

            rc = TRPMForwardTrap(pVCpu, pRegFrame, 0xE, 0, TRPM_TRAP_HAS_ERRORCODE, TRPM_TRAP, 0xe);
            Assert(rc == VINF_EM_RAW_GUEST_TRAP);
            break;

        case VINF_EM_RAW_INTERRUPT_PENDING:
            Assert(TRPMHasTrap(pVCpu));
            /* no break; */
        case VINF_IOM_HC_MMIO_READ:
        case VINF_IOM_HC_MMIO_WRITE:
        case VINF_IOM_HC_MMIO_READ_WRITE:
        case VINF_PATM_HC_MMIO_PATCH_READ:
        case VINF_PATM_HC_MMIO_PATCH_WRITE:
        case VINF_SUCCESS:
        case VINF_EM_RAW_TO_R3:
        case VINF_EM_PENDING_REQUEST:
        case VINF_EM_RAW_TIMER_PENDING:
        case VINF_EM_NO_MEMORY:
        case VINF_CSAM_PENDING_ACTION:
        case VINF_PGM_SYNC_CR3: /** @todo Check this with Sander. */
            break;

        default:
            AssertMsg(PATMIsPatchGCAddr(pVM, (RTRCPTR)pRegFrame->eip) == false, ("Patch address for return code %d. eip=%08x\n", rc, pRegFrame->eip));
            break;
    }
    rc = trpmGCExitTrap(pVM, rc, pRegFrame);
    Log6(("TRPMGC0e: %Rrc (%04x:%08x)\n", rc, pRegFrame->cs, pRegFrame->eip));
    return rc;
}


/**
 * Scans for the EIP in the specified array of trap handlers.
 *
 * If we don't fine the EIP, we'll panic.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   paHandlers  The array of trap handler records.
 * @param   pEndRecord  The end record (exclusive).
 */
static int trpmGCHyperGeneric(PVM pVM, PCPUMCTXCORE pRegFrame, PCTRPMGCHYPER paHandlers, PCTRPMGCHYPER pEndRecord)
{
    uintptr_t uEip  = (uintptr_t)pRegFrame->eip;
    Assert(paHandlers <= pEndRecord);

    Log(("trpmGCHyperGeneric: uEip=%x %p-%p\n", uEip, paHandlers, pEndRecord));

#if 0 /// @todo later
    /*
     * Start by doing a kind of binary search.
     */
    unsigned iStart = 0;
    unsigned iEnd   = pEndRecord - paHandlers;
    unsigned i      = iEnd / 2;
#endif

    /*
     * Do a linear search now (in case the array wasn't properly sorted).
     */
    for (PCTRPMGCHYPER pCur = paHandlers; pCur < pEndRecord; pCur++)
    {
        if (    pCur->uStartEIP <= uEip
            &&  (pCur->uEndEIP ? pCur->uEndEIP > uEip : pCur->uStartEIP == uEip))
            return pCur->pfnHandler(pVM, pRegFrame, pCur->uUser);
    }

    return VERR_TRPM_DONT_PANIC;
}


/**
 * Hypervisor \#NP ((segment) Not Present) handler.
 *
 * Scans for the EIP in the registered trap handlers.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed back to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCHyperTrap0bHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    return trpmGCHyperGeneric(TRPM2VM(pTrpmCpu), pRegFrame, g_aTrap0bHandlers, g_aTrap0bHandlersEnd);
}


/**
 * Hypervisor \#GP (General Protection Fault) handler.
 *
 * Scans for the EIP in the registered trap handlers.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed back to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCHyperTrap0dHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    return trpmGCHyperGeneric(TRPM2VM(pTrpmCpu), pRegFrame, g_aTrap0dHandlers, g_aTrap0dHandlersEnd);
}


/**
 * Hypervisor \#PF (Page Fault) handler.
 *
 * Scans for the EIP in the registered trap handlers.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed back to host context.
 *
 * @param   pTrpmCpu    Pointer to TRPMCPU data (within VM).
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @internal
 */
DECLASM(int) TRPMGCHyperTrap0eHandler(PTRPMCPU pTrpmCpu, PCPUMCTXCORE pRegFrame)
{
    return trpmGCHyperGeneric(TRPM2VM(pTrpmCpu), pRegFrame, g_aTrap0dHandlers, g_aTrap0dHandlersEnd);
}


/**
 * Deal with hypervisor traps occuring when resuming execution on a trap.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   pRegFrame   Register frame.
 * @param   uUser       User arg.
 */
DECLCALLBACK(int) trpmGCTrapInGeneric(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser)
{
    Log(("********************************************************\n"));
    Log(("trpmGCTrapInGeneric: eip=%RX32 uUser=%#x\n", pRegFrame->eip, uUser));
    Log(("********************************************************\n"));

    if (uUser & TRPM_TRAP_IN_HYPER)
    {
        /*
         * Check that there is still some stack left, if not we'll flag
         * a guru meditation (the alternative is a triple fault).
         */
        RTRCUINTPTR cbStackUsed = (RTRCUINTPTR)VMMGetStackRC(pVM) - pRegFrame->esp;
        if (cbStackUsed > VMM_STACK_SIZE - _1K)
        {
            LogRel(("trpmGCTrapInGeneric: ran out of stack: esp=#x cbStackUsed=%#x\n", pRegFrame->esp, cbStackUsed));
            return VERR_TRPM_DONT_PANIC;
        }

        /*
         * Just zero the register containing the selector in question.
         * We'll deal with the actual stale or troublesome selector value in
         * the outermost trap frame.
         */
        switch (uUser & TRPM_TRAP_IN_OP_MASK)
        {
            case TRPM_TRAP_IN_MOV_GS:
                pRegFrame->eax = 0;
                pRegFrame->gs = 0; /* prevent recursive trouble. */
                break;
            case TRPM_TRAP_IN_MOV_FS:
                pRegFrame->eax = 0;
                pRegFrame->fs = 0; /* prevent recursive trouble. */
                return VINF_SUCCESS;

            default:
                AssertMsgFailed(("Invalid uUser=%#x\n", uUser));
                return VERR_INTERNAL_ERROR;
        }
    }
    else
    {
        /*
         * Reconstruct the guest context and switch to the recompiler.
         * We ASSUME we're only at
         */
        CPUMCTXCORE  CtxCore = *pRegFrame;
        uint32_t    *pEsp = (uint32_t *)pRegFrame->esp;
        int          rc;

        switch (uUser)
        {
            /*
             * This will only occur when resuming guest code in a trap handler!
             */
            /* @note ASSUMES esp points to the temporary guest CPUMCTXCORE!!! */
            case TRPM_TRAP_IN_MOV_GS:
            case TRPM_TRAP_IN_MOV_FS:
            case TRPM_TRAP_IN_MOV_ES:
            case TRPM_TRAP_IN_MOV_DS:
            {
                PCPUMCTXCORE pTempGuestCtx = (PCPUMCTXCORE)pEsp;

                /* Just copy the whole thing; several selector registers, eip (etc) and eax are not yet in pRegFrame. */
                CtxCore = *pTempGuestCtx;
                rc = VINF_EM_RAW_STALE_SELECTOR;
                break;
            }

            /*
             * This will only occur when resuming guest code!
             */
            case TRPM_TRAP_IN_IRET:
                CtxCore.eip = *pEsp++;
                CtxCore.cs = (RTSEL)*pEsp++;
                CtxCore.eflags.u32 = *pEsp++;
                CtxCore.esp = *pEsp++;
                CtxCore.ss = (RTSEL)*pEsp++;
                rc = VINF_EM_RAW_IRET_TRAP;
                break;

            /*
             * This will only occur when resuming V86 guest code!
             */
            case TRPM_TRAP_IN_IRET | TRPM_TRAP_IN_V86:
                CtxCore.eip = *pEsp++;
                CtxCore.cs = (RTSEL)*pEsp++;
                CtxCore.eflags.u32 = *pEsp++;
                CtxCore.esp = *pEsp++;
                CtxCore.ss = (RTSEL)*pEsp++;
                CtxCore.es = (RTSEL)*pEsp++;
                CtxCore.ds = (RTSEL)*pEsp++;
                CtxCore.fs = (RTSEL)*pEsp++;
                CtxCore.gs = (RTSEL)*pEsp++;
                rc = VINF_EM_RAW_IRET_TRAP;
                break;

            default:
                AssertMsgFailed(("Invalid uUser=%#x\n", uUser));
                return VERR_INTERNAL_ERROR;
        }


        CPUMSetGuestCtxCore(VMMGetCpu0(pVM), &CtxCore);
        TRPMGCHyperReturnToHost(pVM, rc);
    }

    AssertMsgFailed(("Impossible!\n"));
    return VERR_INTERNAL_ERROR;
}

