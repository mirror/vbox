/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Breakpoint Management.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_dbgf_bp            DBGF - The Debugger Facility, Breakpoint Management
 *
 * The debugger facilities breakpoint managers purpose is to efficiently manage
 * large amounts of breakpoints for various use cases like dtrace like operations
 * or execution flow tracing for instance. Especially execution flow tracing can
 * require thousands of breakpoints which need to be managed efficiently to not slow
 * down guest operation too much. Before the rewrite starting end of 2020, DBGF could
 * only handle 32 breakpoints (+ 4 hardware assisted breakpoints). The new
 * manager is supposed to be able to handle up to one million breakpoints.
 *
 * @see grp_dbgf
 *
 *
 * @section sec_dbgf_bp_owner   Breakpoint owners
 *
 * A single breakpoint owner has a mandatory ring-3 callback and an optional ring-0
 * callback assigned which is called whenever a breakpoint with the owner assigned is hit.
 * The common part of the owner is managed by a single table mapped into both ring-0
 * and ring-3 and the handle being the index into the table. This allows resolving
 * the handle to the internal structure efficiently. Searching for a free entry is
 * done using a bitmap indicating free and occupied entries. For the optional
 * ring-0 owner part there is a separate ring-0 only table for security reasons.
 *
 * The callback of the owner can be used to gather and log guest state information
 * and decide whether to continue guest execution or stop and drop into the debugger.
 * Breakpoints which don't have an owner assigned will always drop the VM right into
 * the debugger.
 *
 *
 * @section sec_dbgf_bp_bps     Breakpoints
 *
 * Breakpoints are referenced by an opaque handle which acts as an index into a global table
 * mapped into ring-3 and ring-0. Each entry contains the necessary state to manage the breakpoint
 * like trigger conditions, type, owner, etc. If an owner is given an optional opaque user argument
 * can be supplied which is passed in the respective owner callback. For owners with ring-0 callbacks
 * a dedicated ring-0 table is held saving possible ring-0 user arguments.
 *
 * To keep memory consumption under control and still support large amounts of
 * breakpoints the table is split into fixed sized chunks and the chunk index and index
 * into the chunk can be derived from the handle with only a few logical operations.
 *
 *
 * @section sec_dbgf_bp_resolv  Resolving breakpoint addresses
 *
 * Whenever a \#BP(0) event is triggered DBGF needs to decide whether the event originated
 * from within the guest or whether a DBGF breakpoint caused it. This has to happen as fast
 * as possible. The following scheme is employed to achieve this:
 *
 * @verbatim
 *                       7   6   5   4   3   2   1   0
 *                     +---+---+---+---+---+---+---+---+
 *                     |   |   |   |   |   |   |   |   | BP address
 *                     +---+---+---+---+---+---+---+---+
 *                      \_____________________/ \_____/
 *                                 |               |
 *                                 |               +---------------+
 *                                 |                               |
 *    BP table                     |                               v
 * +------------+                  |                         +-----------+
 * |   hBp 0    |                  |                    X <- | 0 | xxxxx |
 * |   hBp 1    | <----------------+------------------------ | 1 | hBp 1 |
 * |            |                  |                    +--- | 2 | idxL2 |
 * |   hBp <m>  | <---+            v                    |    |...|  ...  |
 * |            |     |      +-----------+              |    |...|  ...  |
 * |            |     |      |           |              |    |...|  ...  |
 * |   hBp <n>  | <-+ +----- | +> leaf   |              |    |     .     |
 * |            |   |        | |         |              |    |     .     |
 * |            |   |        | + root +  | <------------+    |     .     |
 * |            |   |        |        |  |                   +-----------+
 * |            |   +------- |   leaf<+  |                     L1: 65536
 * |     .      |            |     .     |
 * |     .      |            |     .     |
 * |     .      |            |     .     |
 * +------------+            +-----------+
 *                            L2 idx AVL
 * @endverbatim
 *
 *     -# Take the lowest 16 bits of the breakpoint address and use it as an direct index
 *        into the L1 table. The L1 table is contiguous and consists of 4 byte entries
 *        resulting in 256KiB of memory used. The topmost 4 bits indicate how to proceed
 *        and the meaning of the remaining 28bits depends on the topmost 4 bits:
 *            - A 0 type entry means no breakpoint is registered with the matching lowest 16bits,
 *              so forward the event to the guest.
 *            - A 1 in the topmost 4 bits means that the remaining 28bits directly denote a breakpoint
 *              handle which can be resolved by extracting the chunk index and index into the chunk
 *              of the global breakpoint table. If the address matches the breakpoint is processed
 *              according to the configuration. Otherwise the breakpoint is again forwarded to the guest.
 *            - A 2 in the topmost 4 bits means that there are multiple breakpoints registered
 *              matching the lowest 16bits and the search must continue in the L2 table with the
 *              remaining 28bits acting as an index into the L2 table indicating the search root.
 *     -# The L2 table consists of multiple index based AVL trees, there is one for each reference
 *        from the L1 table. The key for the table are the upper 6 bytes of the breakpoint address
 *        used for searching. This tree is traversed until either a matching address is found and
 *        the breakpoint is being processed or again forwardedto the guest if it isn't successful.
 *        Each entry in the L2 table is 16 bytes big and densly packed to avoid excessive memory usage.
 *
 *
 * @section sec_dbgf_bp_note    Random thoughts and notes for the implementation
 *
 * - The assumption for this approach is that the lowest 16bits of the breakpoint address are
 *   hopefully the ones being the most varying ones across breakpoints so the traversal
 *   can skip the L2 table in most of the cases. Even if the L2 table must be taken the
 *   individual trees should be quite shallow resulting in low overhead when walking it
 *   (though only real world testing can assert this assumption).
 * - Index based tables and trees are used instead of pointers because the tables
 *   are always mapped into ring-0 and ring-3 with different base addresses.
 * - Efficent breakpoint allocation is done by having a global bitmap indicating free
 *   and occupied breakpoint entries. Same applies for the L2 AVL table.
 * - Special care must be taken when modifying the L1 and L2 tables as other EMTs
 *   might still access it (want to try a lockless approach first using
 *   atomic updates, have to resort to locking if that turns out to be too difficult).
 * - Each BP entry is supposed to be 64 byte big and each chunk should contain 65536
 *   breakpoints which results in 4MiB for each chunk plus the allocation bitmap.
 * - ring-0 has to take special care when traversing the L2 AVL tree to not run into cycles
 *   and do strict bounds checking before accessing anything. The L1 and L2 table
 *   are written to from ring-3 only. Same goes for the breakpoint table with the
 *   exception being the opaque user argument for ring-0 which is stored in ring-0 only
 *   memory.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/hm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
RT_C_DECLS_END



/**
 * Initialize the breakpoint mangement.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
DECLHIDDEN(int) dbgfR3BpInit(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Terminates the breakpoint mangement.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
DECLHIDDEN(int) dbgfR3BpTerm(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Creates a new breakpoint owner returning a handle which can be used when setting breakpoints.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pfnBpHit            The R3 callback which is called when a breakpoint with the owner handle is hit.
 * @param   phBpOwner           Where to store the owner handle on success.
 */
VMMR3DECL(int) DBGFR3BpOwnerCreate(PUVM pUVM, PFNDBGFBPHIT pfnBpHit, PDBGFBPOWNER phBpOwner)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pfnBpHit, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBpOwner, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Destroys the owner identified by the given handle.
 *
 * @returns VBox status code.
 * @retval  VERR_DBGF_OWNER_BUSY if there are still breakpoints set with the given owner handle.
 * @param   pUVM                The user mode VM handle.
 * @param   hBpOwner            The breakpoint owner handle to destroy.
 */
VMMR3DECL(int) DBGFR3BpOwnerDestroy(PUVM pUVM, DBGFBPOWNER hBpOwner)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBpOwner != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets a breakpoint (int 3 based).
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   idSrcCpu    The ID of the virtual CPU used for the
 *                      breakpoint address resolution.
 * @param   pAddress    The address of the breakpoint.
 * @param   iHitTrigger The hit count at which the breakpoint start triggering.
 *                      Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable The hit count which disables the breakpoint.
 *                      Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp        Where to store the breakpoint handle on success.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetInt3(PUVM pUVM, VMCPUID idSrcCpu, PCDBGFADDRESS pAddress,
                               uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetInt3Ex(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, idSrcCpu, pAddress,
                             iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets a breakpoint (int 3 based) - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   idSrcCpu        The ID of the virtual CPU used for the
 *                          breakpoint address resolution.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle on success.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetInt3Ex(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                 VMCPUID idSrcCpu, PCDBGFADDRESS pAddress,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pUVM, pAddress), VERR_INVALID_PARAMETER);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);

    RT_NOREF(idSrcCpu);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets a register breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType           The access type (one of the X86_DR7_RW_* defines).
 * @param   cb              The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                          Must be 1 if fType is X86_DR7_RW_EO.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetReg(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger,
                              uint64_t iHitDisable, uint8_t fType, uint8_t cb, PDBGFBP phBp)
{
    return DBGFR3BpSetRegEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, pAddress,
                            iHitTrigger, iHitDisable, fType, cb, phBp);
}


/**
 * Sets a register breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   pAddress        The address of the breakpoint.
 * @param   iHitTrigger     The hit count at which the breakpoint start triggering.
 *                          Use 0 (or 1) if it's gonna trigger at once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   fType           The access type (one of the X86_DR7_RW_* defines).
 * @param   cb              The access size - 1,2,4 or 8 (the latter is AMD64 long mode only.
 *                          Must be 1 if fType is X86_DR7_RW_EO.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetRegEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                                uint8_t fType, uint8_t cb, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(DBGFR3AddrIsValid(pUVM, pAddress), VERR_INVALID_PARAMETER);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0 && cb <= 8 && RT_IS_POWER_OF_TWO(cb), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    switch (fType)
    {
        case X86_DR7_RW_EO:
            if (cb == 1)
                break;
            AssertMsgFailedReturn(("fType=%#x cb=%d != 1\n", fType, cb), VERR_INVALID_PARAMETER);
        case X86_DR7_RW_IO:
        case X86_DR7_RW_RW:
        case X86_DR7_RW_WO:
            break;
        default:
            AssertMsgFailedReturn(("fType=%#x\n", fType), VERR_INVALID_PARAMETER);
    }

    return VERR_NOT_IMPLEMENTED;
}


/**
 * This is only kept for now to not mess with the debugger implementation at this point,
 * recompiler breakpoints are not supported anymore (IEM has some API but it isn't implemented
 * and should probably be merged with the DBGF breakpoints).
 */
VMMR3DECL(int) DBGFR3BpSetREM(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger,
                              uint64_t iHitDisable, PDBGFBP phBp)
{
    RT_NOREF(pUVM, pAddress, iHitTrigger, iHitDisable, phBp);
    return VERR_NOT_SUPPORTED;
}


/**
 * Sets an I/O port breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   uPort           The first I/O port.
 * @param   cPorts          The number of I/O ports, see DBGFBPIOACCESS_XXX.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetPortIo(PUVM pUVM, RTIOPORT uPort, RTIOPORT cPorts, uint32_t fAccess,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetPortIoEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, uPort, cPorts,
                               fAccess, iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets an I/O port breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   uPort           The first I/O port.
 * @param   cPorts          The number of I/O ports, see DBGFBPIOACCESS_XXX.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetPortIoEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                   RTIOPORT uPort, RTIOPORT cPorts, uint32_t fAccess,
                                   uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAccess & ~DBGFBPIOACCESS_VALID_MASK_PORT_IO), VERR_INVALID_FLAGS);
    AssertReturn(fAccess, VERR_INVALID_FLAGS);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    AssertReturn(cPorts > 0, VERR_OUT_OF_RANGE);
    AssertReturn((RTIOPORT)(uPort + cPorts) < uPort, VERR_OUT_OF_RANGE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets a memory mapped I/O breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   GCPhys          The first MMIO address.
 * @param   cb              The size of the MMIO range to break on.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetMmio(PUVM pUVM, RTGCPHYS GCPhys, uint32_t cb, uint32_t fAccess,
                               uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    return DBGFR3BpSetMmioEx(pUVM, NIL_DBGFBPOWNER, NULL /*pvUser*/, GCPhys, cb, fAccess,
                             iHitTrigger, iHitDisable, phBp);
}


/**
 * Sets a memory mapped I/O breakpoint - extended version.
 *
 * @returns VBox status code.
 * @param   pUVM            The user mode VM handle.
 * @param   hOwner          The owner handle, use NIL_DBGFBPOWNER if no special owner attached.
 * @param   pvUser          Opaque user data to pass in the owner callback.
 * @param   GCPhys          The first MMIO address.
 * @param   cb              The size of the MMIO range to break on.
 * @param   fAccess         The access we want to break on.
 * @param   iHitTrigger     The hit count at which the breakpoint start
 *                          triggering. Use 0 (or 1) if it's gonna trigger at
 *                          once.
 * @param   iHitDisable     The hit count which disables the breakpoint.
 *                          Use ~(uint64_t) if it's never gonna be disabled.
 * @param   phBp            Where to store the breakpoint handle.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpSetMmioEx(PUVM pUVM, DBGFBPOWNER hOwner, void *pvUser,
                                 RTGCPHYS GCPhys, uint32_t cb, uint32_t fAccess,
                                 uint64_t iHitTrigger, uint64_t iHitDisable, PDBGFBP phBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hOwner != NIL_DBGFBPOWNER || pvUser != NULL, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAccess & ~DBGFBPIOACCESS_VALID_MASK_MMIO), VERR_INVALID_FLAGS);
    AssertReturn(fAccess, VERR_INVALID_FLAGS);
    AssertReturn(iHitTrigger <= iHitDisable, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phBp, VERR_INVALID_POINTER);
    AssertReturn(cb, VERR_OUT_OF_RANGE);
    AssertReturn(GCPhys + cb < GCPhys, VERR_OUT_OF_RANGE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Clears a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be removed (cleared).
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpClear(PUVM pUVM, DBGFBP hBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Enables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be enabled.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpEnable(PUVM pUVM, DBGFBP hBp)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Disables a breakpoint.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   hBp         The handle of the breakpoint which should be disabled.
 *
 * @thread  Any thread.
 */
VMMR3DECL(int) DBGFR3BpDisable(PUVM pUVM, DBGFBP hBp)
{
    /*
     * Validate the input.
     */
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(hBp != NIL_DBGFBPOWNER, VERR_INVALID_HANDLE);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * EMT worker for DBGFR3BpEnum().
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 *
 * @thread  EMT
 * @internal
 */
static DECLCALLBACK(int) dbgfR3BpEnum(PUVM pUVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * Validate input.
     */
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pfnCallback, VERR_INVALID_POINTER);

    RT_NOREF(pvUser);

    return VERR_NOT_IMPLEMENTED;
}


/**
 * Enumerate the breakpoints.
 *
 * @returns VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pfnCallback The callback function.
 * @param   pvUser      The user argument to pass to the callback.
 *
 * @thread  Any thread but the callback will be called from EMT.
 */
VMMR3DECL(int) DBGFR3BpEnum(PUVM pUVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    /*
     * This must be done on EMT.
     */
    int rc = VMR3ReqPriorityCallWaitU(pUVM, 0 /*idDstCpu*/, (PFNRT)dbgfR3BpEnum, 3, pUVM, pfnCallback, pvUser);
    LogFlow(("DBGFR3BpEnum: returns %Rrc\n", rc));
    return rc;
}

