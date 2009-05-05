/** @file
 * VirtualBox Status Codes.
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

#ifndef ___VBox_err_h
#define ___VBox_err_h

#include <VBox/cdefs.h>
#include <iprt/err.h>


/** @defgroup grp_err       Error Codes
 * @{
 */

/* SED-START */

/** @name Misc. Status Codes
 * @{
 */
/** Failed to allocate VM memory. */
#define VERR_NO_VM_MEMORY                   (-1000)
/** GC is toasted and the VMM should be terminated at once, but no need to panic about it :-) */
#define VERR_DONT_PANIC                     (-1001)
/** Unsupported CPU. */
#define VERR_UNSUPPORTED_CPU                (-1002)
/** Unsupported CPU mode. */
#define VERR_UNSUPPORTED_CPU_MODE           (-1003)
/** Page not present. */
#define VERR_PAGE_NOT_PRESENT               (-1004)
/** Invalid/Corrupted configuration file. */
#define VERR_CFG_INVALID_FORMAT             (-1005)
/** No configuration value exists. */
#define VERR_CFG_NO_VALUE                   (-1006)
/** Not selector not present. */
#define VERR_SELECTOR_NOT_PRESENT           (-1007)
/** Not code selector. */
#define VERR_NOT_CODE_SELECTOR              (-1008)
/** Not data selector. */
#define VERR_NOT_DATA_SELECTOR              (-1009)
/** Out of selector bounds. */
#define VERR_OUT_OF_SELECTOR_BOUNDS         (-1010)
/** Invalid selector. Usually beyond table limits. */
#define VERR_INVALID_SELECTOR               (-1011)
/** Invalid requested privilegde level. */
#define VERR_INVALID_RPL                    (-1012)
/** PML4 entry not present. */
#define VERR_PAGE_MAP_LEVEL4_NOT_PRESENT    (-1013)
/** Page directory pointer not present. */
#define VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT (-1014)
/** Raw mode doesn't support SMP. */
#define VERR_RAW_MODE_INVALID_SMP           (-1015)
/** Invalid VM handle. */
#define VERR_INVALID_VM_HANDLE              (-1016)
/** Invalid VM handle. */
#define VERR_INVALID_VMCPU_HANDLE           (-1017)
/** Invalid Virtual CPU ID. */
#define VERR_INVALID_CPU_ID                 (-1018)
/** @} */


/** @name Execution Monitor/Manager (EM) Status Codes
 *
 * The order of the status codes between VINF_EM_FIRST and VINF_EM_LAST
 * are of vital importance. The lower the number the higher importance
 * as a scheduling instruction.
 * @{
 */
/** First scheduling related status code. */
#define VINF_EM_FIRST                       1100
/** Indicating that the VM is being terminated and that the the execution
 * shall stop. */
#define VINF_EM_TERMINATE                   1100
/** Hypervisor code was stepped.
 * EM will first send this to the debugger, and if the issue isn't
 * resolved there it will enter guru meditation. */
#define VINF_EM_DBG_HYPER_STEPPED           1101
/** Hit a breakpoint in the hypervisor code,
 * EM will first send this to the debugger, and if the issue isn't
 * resolved there it will enter guru meditation. */
#define VINF_EM_DBG_HYPER_BREAKPOINT        1102
/** Hit a possible assertion in the hypervisor code,
 * EM will first send this to the debugger, and if the issue isn't
 * resolved there it will enter guru meditation. */
#define VINF_EM_DBG_HYPER_ASSERTION         1103
/** Indicating that the VM should be suspended for debugging because
 * the developer wants to inspect the VM state. */
#define VINF_EM_DBG_STOP                    1105
/** Indicating success single stepping and that EM should report that
 * event to the debugger. */
#define VINF_EM_DBG_STEPPED                 1106
/** Indicating that a breakpoint was hit and that EM should notify the debugger
 * and in the event there is no debugger fail fatally. */
#define VINF_EM_DBG_BREAKPOINT              1107
/** Indicating that EM should single step an instruction.
 * The instruction is stepped in the current execution mode (RAW/REM). */
#define VINF_EM_DBG_STEP                    1108
/** Indicating that the VM is being turned off and that the EM should
 * exit to the VM awaiting the destruction request. */
#define VINF_EM_OFF                         1109
/** Indicating that the VM has been reset and that scheduling goes
 * back to startup defaults. */
#define VINF_EM_RESET                       1110
/** Indicating that the VM has been suspended and that the the thread
 * should wait for request telling it what to do next. */
#define VINF_EM_SUSPEND                     1111
/** Indicating that the VM has executed a halt instruction and that
 * the emulation thread should wait for an interrupt before resuming
 * execution. */
#define VINF_EM_HALT                        1112
/** Indicating that the VM has been resumed and that the thread should
 * start executing. */
#define VINF_EM_RESUME                      1113
/** Indicating that we've got an out-of-memory condition and that we need
 * to take the appropriate actions to deal with this.
 * @remarks It might seem odd at first that this has lower priority than VINF_EM_HALT,
 *          VINF_EM_SUSPEND, and VINF_EM_RESUME. The reason is that these events are
 *          vital to correctly operating the VM. Also, they can't normally occur together
 *          with an out-of-memory condition, and even if that should happen the condition
 *          will be rediscovered before executing any more code. */
#define VINF_EM_NO_MEMORY                   1114
/** The fatal variant of VINF_EM_NO_MEMORY. */
#define VERR_EM_NO_MEMORY                   (-1114)
/** Indicating that a rescheduling to recompiled execution.
 * Typically caused by raw-mode executing code which is difficult/slow
 * to virtualize rawly.
 * @remarks Important to have a higher priority (lower number) than the other rescheduling status codes. */
#define VINF_EM_RESCHEDULE_REM              1115
/** Indicating that a rescheduling to vmx-mode execution.
 * Typically caused by REM detecting that hardware-accelerated raw-mode execution is possible. */
#define VINF_EM_RESCHEDULE_HWACC            1116
/** Indicating that a rescheduling to raw-mode execution.
 * Typically caused by REM detecting that raw-mode execution is possible.
 * @remarks Important to have a higher priority (lower number) than VINF_EM_RESCHEDULE. */
#define VINF_EM_RESCHEDULE_RAW              1117
/** Indicating that a rescheduling now is required. Typically caused by
 * interrupts having changed the EIP. */
#define VINF_EM_RESCHEDULE                  1118
/** PARAV call */
#define VINF_EM_RESCHEDULE_PARAV            1119
/** Last scheduling related status code. (inclusive) */
#define VINF_EM_LAST                        1119

/** Reason for leaving GC: Guest trap which couldn't be handled in GC.
 * The trap is generally forwared to the REM and executed there. */
#define VINF_EM_RAW_GUEST_TRAP              1121
/** Reason for leaving GC: Interrupted by external interrupt.
 * The interrupt needed to be handled by the host OS. */
#define VINF_EM_RAW_INTERRUPT               1122
/** Reason for leaving GC: Interrupted by external interrupt while in hypervisor code.
 * The interrupt needed to be handled by the host OS and hypervisor execution must be
 * resumed. VM state is not complete at this point. */
#define VINF_EM_RAW_INTERRUPT_HYPER         1123
/** Reason for leaving GC: A Ring switch was attempted.
 * Normal cause of action is to execute this in REM. */
#define VINF_EM_RAW_RING_SWITCH             1124
/** Reason for leaving GC: A Ring switch was attempted using software interrupt.
 * Normal cause of action is to execute this in REM. */
#define VINF_EM_RAW_RING_SWITCH_INT         1125
/** Reason for leaving GC: A privileged instruction was attempted executed.
 * Normal cause of action is to execute this in REM. */
#define VINF_EM_RAW_EXCEPTION_PRIVILEGED    1126

/** Reason for leaving GC: Emulate instruction. */
#define VINF_EM_RAW_EMULATE_INSTR           1127
/** Reason for leaving GC: Unhandled TSS write.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT 1128
/** Reason for leaving GC: Unhandled LDT write.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT 1129
/** Reason for leaving GC: Unhandled IDT write.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT 1130
/** Reason for leaving GC: Unhandled GDT write.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT 1131
/** Reason for leaving GC: Unhandled Page Directory write.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_PD_FAULT  1132
/** Reason for leaving GC: jump inside generated patch jump.
 * Fatal error. */
#define VERR_EM_RAW_PATCH_CONFLICT          (-1133)
/** Reason for leaving GC: Hlt instruction.
 * Recompiler gets control. */
#define VINF_EM_RAW_EMULATE_INSTR_HLT       1134
/** Reason for leaving GC: Ring-3 operation pending. */
#define VINF_EM_RAW_TO_R3                   1135
/** Reason for leaving GC: Timer pending. */
#define VINF_EM_RAW_TIMER_PENDING           1136
/** Reason for leaving GC: Interrupt pending (guest). */
#define VINF_EM_RAW_INTERRUPT_PENDING       1137
/** Reason for leaving GC: Encountered a stale selector. */
#define VINF_EM_RAW_STALE_SELECTOR          1138
/** Reason for leaving GC: The IRET resuming guest code trapped. */
#define VINF_EM_RAW_IRET_TRAP               1139
/** Reason for leaving GC: Emulate (MM)IO intensive code in the recompiler. */
#define VINF_EM_RAW_EMULATE_IO_BLOCK        1140
/** The interpreter was unable to deal with the instruction at hand. */
#define VERR_EM_INTERPRETER                 (-1148)
/** Internal EM error caused by an unknown warning or informational status code. */
#define VERR_EM_INTERNAL_ERROR              (-1149)
/** Pending VM request packet. */
#define VINF_EM_PENDING_REQUEST             (-1150)
/** @} */


/** @name Debugging Facility (DBGF) DBGF Status Codes
 * @{
 */
/** The function called requires the caller to be attached as a
 * debugger to the VM. */
#define VERR_DBGF_NOT_ATTACHED              (-1200)
/** Someone (including the caller) was already attached as
 * debugger to the VM. */
#define VERR_DBGF_ALREADY_ATTACHED          (-1201)
/** Tried to hald a debugger which was already halted.
 * (This is a warning and not an error.) */
#define VWRN_DBGF_ALREADY_HALTED            1202
/** The DBGF has no more free breakpoint slots. */
#define VERR_DBGF_NO_MORE_BP_SLOTS          (-1203)
/** The DBGF couldn't find the specified breakpoint. */
#define VERR_DBGF_BP_NOT_FOUND              (-1204)
/** Attempted to enabled a breakpoint which was already enabled. */
#define VINF_DBGF_BP_ALREADY_ENABLED        1205
/** Attempted to disabled a breakpoint which was already disabled. */
#define VINF_DBGF_BP_ALREADY_DISABLED       1206
/** The breakpoint already exists. */
#define VINF_DBGF_BP_ALREADY_EXIST          1207
/** The byte string was not found. */
#define VERR_DBGF_MEM_NOT_FOUND             (-1208)
/** The OS was not detected. */
#define VERR_DBGF_OS_NOT_DETCTED            (-1209)
/** The OS was not detected. */
#define VINF_DBGF_OS_NOT_DETCTED            1209
/** @} */


/** @name Patch Manager (PATM) Status Codes
 * @{
 */
/** Non fatal Patch Manager analysis phase warning */
#define VWRN_CONTINUE_ANALYSIS              1400
/** Non fatal Patch Manager recompile phase warning (mapped to VWRN_CONTINUE_ANALYSIS). */
#define VWRN_CONTINUE_RECOMPILE             VWRN_CONTINUE_ANALYSIS
/** Continue search (mapped to VWRN_CONTINUE_ANALYSIS). */
#define VWRN_PATM_CONTINUE_SEARCH           VWRN_CONTINUE_ANALYSIS
/** Patch installation refused (patch too complex or unsupported instructions ) */
#define VERR_PATCHING_REFUSED               (-1401)
/** Unable to find patch */
#define VERR_PATCH_NOT_FOUND                (-1402)
/** Patch disabled */
#define VERR_PATCH_DISABLED                 (-1403)
/** Patch enabled */
#define VWRN_PATCH_ENABLED                  1404
/** Patch was already disabled */
#define VERR_PATCH_ALREADY_DISABLED         (-1405)
/** Patch was already enabled */
#define VERR_PATCH_ALREADY_ENABLED          (-1406)
/** Patch was removed. */
#define VWRN_PATCH_REMOVED                  1408

/** Reason for leaving GC: \#GP with EIP pointing to patch code. */
#define VINF_PATM_PATCH_TRAP_GP             1408
/** First leave GC code. */
#define VINF_PATM_LEAVEGC_FIRST             VINF_PATM_PATCH_TRAP_GP
/** Reason for leaving GC: \#PF with EIP pointing to patch code. */
#define VINF_PATM_PATCH_TRAP_PF             1409
/** Reason for leaving GC: int3 with EIP pointing to patch code. */
#define VINF_PATM_PATCH_INT3                1410
/** Reason for leaving GC: \#PF for monitored patch page. */
#define VINF_PATM_CHECK_PATCH_PAGE          1411
/** Reason for leaving GC: duplicate instruction called at current eip. */
#define VINF_PATM_DUPLICATE_FUNCTION        1412
/** Execute one instruction with the recompiler */
#define VINF_PATCH_EMULATE_INSTR            1413
/** Reason for leaving GC: attempt to patch MMIO write. */
#define VINF_PATM_HC_MMIO_PATCH_WRITE       1414
/** Reason for leaving GC: attempt to patch MMIO read. */
#define VINF_PATM_HC_MMIO_PATCH_READ        1415
/** Reason for leaving GC: pending irq after iret that sets IF. */
#define VINF_PATM_PENDING_IRQ_AFTER_IRET    1416
/** Last leave GC code. */
#define VINF_PATM_LEAVEGC_LAST              VINF_PATM_PENDING_IRQ_AFTER_IRET

/** No conflicts to resolve */
#define VERR_PATCH_NO_CONFLICT              (-1425)
/** Detected unsafe code for patching */
#define VERR_PATM_UNSAFE_CODE               (-1426)
/** Terminate search branch */
#define VWRN_PATCH_END_BRANCH                1427
/** Already patched */
#define VERR_PATM_ALREADY_PATCHED           (-1428)
/** Spinlock detection failed. */
#define VINF_PATM_SPINLOCK_FAILED           (1429)
/** Continue execution after patch trap. */
#define VINF_PATCH_CONTINUE                 (1430)

/** @} */


/** @name Code Scanning and Analysis Manager (CSAM) Status Codes
 * @{
 */
/** Trap not handled */
#define VWRN_CSAM_TRAP_NOT_HANDLED          1500
/** Patch installed */
#define VWRN_CSAM_INSTRUCTION_PATCHED       1501
/** Page record not found */
#define VWRN_CSAM_PAGE_NOT_FOUND            1502
/** Reason for leaving GC: CSAM wants perform a task in ring-3. */
#define VINF_CSAM_PENDING_ACTION            1503
/** @} */


/** @name Page Monitor/Manager (PGM) Status Codes
 * @{
 */
/** Attempt to create a GC mapping which conflicts with an existing mapping. */
#define VERR_PGM_MAPPING_CONFLICT           (-1600)
/** The physical handler range has no corresponding RAM range.
 * If this is MMIO, see todo above the return. If not MMIO, then it's
 * someone else's fault... */
#define VERR_PGM_HANDLER_PHYSICAL_NO_RAM_RANGE (-1601)
/** Attempt to register an access handler for a virtual range of which a part
 * was already handled. */
#define VERR_PGM_HANDLER_VIRTUAL_CONFLICT   (-1602)
/** Attempt to register an access handler for a physical range of which a part
 * was already handled. */
#define VERR_PGM_HANDLER_PHYSICAL_CONFLICT  (-1603)
/** Invalid page directory specified to PGM. */
#define VERR_PGM_INVALID_PAGE_DIRECTORY     (-1604)
/** Invalid GC physical address. */
#define VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS (-1605)
/** Invalid GC physical range. Usually used when a specified range crosses
 * a RAM region boundrary. */
#define VERR_PGM_INVALID_GC_PHYSICAL_RANGE  (-1606)
/** Specified access handler was not found. */
#define VERR_PGM_HANDLER_NOT_FOUND          (-1607)
/** Attempt to register a RAM range of which parts are already
 * covered by existing RAM ranges. */
#define VERR_PGM_RAM_CONFLICT               (-1608)
/** Failed to add new mappings because the current mappings are fixed
 * in guest os memory. */
#define VERR_PGM_MAPPINGS_FIXED             (-1609)
/** Failed to fix mappings because of a conflict with the intermediate code. */
#define VERR_PGM_MAPPINGS_FIX_CONFLICT      (-1610)
/** Failed to fix mappings because a mapping rejected the address. */
#define VERR_PGM_MAPPINGS_FIX_REJECTED      (-1611)
/** Failed to fix mappings because the proposed memory area was to small. */
#define VERR_PGM_MAPPINGS_FIX_TOO_SMALL     (-1612)
/** Reason for leaving GC: The urge to syncing CR3. */
#define VINF_PGM_SYNC_CR3                   1613
/** Page not marked for dirty bit tracking */
#define VINF_PGM_NO_DIRTY_BIT_TRACKING      1614
/** Page fault caused by dirty bit tracking; corrected */
#define VINF_PGM_HANDLED_DIRTY_BIT_FAULT    1615
/** Go ahead with the default Read/Write operation.
 * This is returned by a HC physical or virtual handler when it wants the PGMPhys[Read|Write]
 * routine do the reading/writing. */
#define VINF_PGM_HANDLER_DO_DEFAULT         1616
/** The paging mode of the host is not supported yet. */
#define VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE (-1617)
/** The physical guest page is a reserved/mmio page and does not have any HC address. */
#define VERR_PGM_PHYS_PAGE_RESERVED         (-1618)
/** No page directory available for the hypervisor. */
#define VERR_PGM_NO_HYPERVISOR_ADDRESS      (-1619)
/** The shadow page pool was flushed.
 * This means that a global CR3 sync was flagged. Anyone receiving this kind of status
 * will have to get down to a SyncCR3 ASAP. See also VINF_PGM_SYNC_CR3. */
#define VERR_PGM_POOL_FLUSHED               (-1620)
/** The shadow page pool was cleared.
 * This is a error code internal to the shadow page pool, it will be
 * converted to a VERR_PGM_POOL_FLUSHED before leaving the pool code. */
#define VERR_PGM_POOL_CLEARED               (-1621)
/** The returned shadow page is cached. */
#define VINF_PGM_CACHED_PAGE                1622
/** Returned by handler registration, modification and deregistration
 * when the shadow PTs could be updated because the guest page
 * aliased or/and mapped by multiple PTs. */
#define VINF_PGM_GCPHYS_ALIASED             1623
/** Reason for leaving GC: Paging mode changed.
 * PGMChangeMode() uses this to force a switch to HC so it can safely
 * deal with a mode switch.
 */
#define VINF_PGM_CHANGE_MODE                1624
/** SyncPage modified the PDE.
 * This is an internal status code used to communicate back to the \#PF handler
 * that the PDE was (probably) marked not-present and it should restart the instruction. */
#define VINF_PGM_SYNCPAGE_MODIFIED_PDE      1625
/** Physical range crosses dynamic ram chunk boundary; translation to HC ptr not safe. */
#define VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY  (-1626)
/** Conflict between the core memory and the intermediate paging context, try again.
 * There are some very special conditions applying to the intermediate paging context
 * (used during the world switches), and some times we continuously run into these
 * when asking the host kernel for memory during VM init. Let us know if you run into
 * this and we'll adjust the code so it tries harder to avoid it.
 */
#define VERR_PGM_INTERMEDIATE_PAGING_CONFLICT   (-1627)
/** The shadow paging mode is not supported yet. */
#define VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE (-1628)
/** The dynamic mapping cache for physical memory failed. */
#define VERR_PGM_DYNMAP_FAILED                  (-1629)
/** The auto usage cache for the dynamic mapping set is full. */
#define VERR_PGM_DYNMAP_FULL_SET                (-1630)
/** The initialization of the dynamic mapping cache failed. */
#define VERR_PGM_DYNMAP_SETUP_ERROR             (-1631)
/** The expanding of the dynamic mapping cache failed. */
#define VERR_PGM_DYNMAP_EXPAND_ERROR            (-1632)
/** The page is unassigned (akin to VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS). */
#define VERR_PGM_PHYS_TLB_UNASSIGNED            (-1633)
/** Catch any access and route it thru PGM. */
#define VERR_PGM_PHYS_TLB_CATCH_ALL             (-1634)
/** Catch write access and route it thru PGM. */
#define VINF_PGM_PHYS_TLB_CATCH_WRITE           1635
/** No CR3 root shadow page table.. */
#define VERR_PGM_NO_CR3_SHADOW_ROOT             (-1636)
/** Trying to free a page with an invalid Page ID. */
#define VERR_PGM_PHYS_INVALID_PAGE_ID           (-1637)
/** PGMPhysWrite/Read hit a handler in Ring-0 or raw-mode context. */
#define VERR_PGM_PHYS_WR_HIT_HANDLER            (-1638)
/** Trying to free a page that isn't RAM. */
#define VERR_PGM_PHYS_NOT_RAM                   (-1639)
/** Not ROM page. */
#define VERR_PGM_PHYS_NOT_ROM                   (-1640)
/** Not MMIO page. */
#define VERR_PGM_PHYS_NOT_MMIO                  (-1641)
/** Not MMIO2 page. */
#define VERR_PGM_PHYS_NOT_MMIO2                 (-1642)
/** Already aliased to a different page. */
#define VERR_PGM_HANDLER_ALREADY_ALIASED        (-1643)
/** Already aliased to the same page. */
#define VINF_PGM_HANDLER_ALREADY_ALIASED        (1643)
/** @} */


/** @name Memory Monitor (MM) Status Codes
 * @{
 */
/** Attempt to register a RAM range of which parts are already
 * covered by existing RAM ranges. */
#define VERR_MM_RAM_CONFLICT                (-1700)
/** Hypervisor memory allocation failed. */
#define VERR_MM_HYPER_NO_MEMORY             (-1701)

/** @} */


/** @name Save State Manager (SSM) Status Codes
 * @{
 */
/** The specified data unit already exist. */
#define VERR_SSM_UNIT_EXISTS                    (-1800)
/** The specified data unit wasn't found. */
#define VERR_SSM_UNIT_NOT_FOUND                 (-1801)
/** The specified data unit wasn't owned by caller. */
#define VERR_SSM_UNIT_NOT_OWNER                 (-1802)
/** General saved state file integrity error. */
#define VERR_SSM_INTEGRITY                      (-1810)
/** The saved state file magic was not recognized. */
#define VERR_SSM_INTEGRITY_MAGIC                (-1811)
/** The saved state file version is not supported. */
#define VERR_SSM_INTEGRITY_VERSION              (-1812)
/** The saved state file size didn't match the one in the header. */
#define VERR_SSM_INTEGRITY_SIZE                 (-1813)
/** The CRC of the saved state file did match. */
#define VERR_SSM_INTEGRITY_CRC                  (-1814)
/** The current virtual machine id didn't match the virtual machine id. */
#define VERR_SMM_INTEGRITY_MACHINE              (-1815)
/** Invalid unit magic (internal data tag). */
#define VERR_SSM_INTEGRITY_UNIT_MAGIC           (-1816)
/** The file contained a data unit which no-one wants. */
#define VERR_SSM_INTEGRITY_UNIT_NOT_FOUND       (-1817)
/** Incorrect type sizes in the header. */
#define VERR_SSM_INTEGRITY_SIZES                (-1818)
/** Incorrect version numbers in the header. */
#define VERR_SSM_INTEGRITY_VBOX_VERSION         (-1819)
/** A data unit in the saved state file was defined but didn't any
 * routine for processing it. */
#define VERR_SSM_NO_LOAD_EXEC                   (-1820)
/** A restore routine attempted to load more data then the unit contained. */
#define VERR_SSM_LOADED_TOO_MUCH                (-1821)
/** Not in the correct state for the attempted operation. */
#define VERR_SSM_INVALID_STATE                  (-1822)

/** Unsupported data unit version.
 * A SSM user returns this if it doesn't know the u32Version. */
#define VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION  (-1823)
/** The format of a data unit has changed.
 * A SSM user returns this if it's not able to read the format for
 * other reasons than u32Version. */
#define VERR_SSM_DATA_UNIT_FORMAT_CHANGED       (-1824)
/** The CPUID instruction returns different information when loading than when saved.
 * Normally caused by hardware changes on the host, but could also be caused by
 * changes in the BIOS setup. */
#define VERR_SSM_LOAD_CPUID_MISMATCH            (-1825)
/** The RAM size differes between the saved state and the VM config. */
#define VERR_SSM_LOAD_MEMORY_SIZE_MISMATCH      (-1826)
/** The state doesn't match the VM configuration in one or another way.
 * (There are certain PCI reconfiguration which the OS could potentially
 * do which can cause this problem. Check this out when it happens.) */
#define VERR_SSM_LOAD_CONFIG_MISMATCH           (-1827)
/** The virtual clock freqency differs too much.
 * The clock source for the virtual time isn't reliable or the code have changed. */
#define VERR_SSM_VIRTUAL_CLOCK_HZ               (-1828)
/** A timeout occured while waiting for async IDE operations to finish. */
#define VERR_SSM_IDE_ASYNC_TIMEOUT              (-1829)
/** One of the structure magics was wrong. */
#define VERR_SSM_STRUCTURE_MAGIC                (-1830)
/** The data in the saved state doesn't confirm to expectations. */
#define VERR_SSM_UNEXPECTED_DATA                (-1831)
/** Trying to read a 64-bit guest physical address into a 32-bit variable. */
#define VERR_SSM_GCPHYS_OVERFLOW                (-1832)
/** Trying to read a 64-bit guest virtual address into a 32-bit variable. */
#define VERR_SSM_GCPTR_OVERFLOW                 (-1833)
/** @} */


/** @name Virtual Machine (VM) Status Codes
 * @{
 */
/** The specified at reset handler wasn't found. */
#define VERR_VM_ATRESET_NOT_FOUND               (-1900)
/** Invalid VM request type.
 * For the VMR3ReqAlloc() case, the caller just specified an illegal enmType. For
 * all the other occurences it means indicates corruption, broken logic, or stupid
 * interface user. */
#define VERR_VM_REQUEST_INVALID_TYPE            (-1901)
/** Invalid VM request state.
 * The state of the request packet was not the expected and accepted one(s). Either
 * the interface user screwed up, or we've got corruption/broken logic. */
#define VERR_VM_REQUEST_STATE                   (-1902)
/** Invalid VM request packet.
 * One or more of the the VM controlled packet members didn't contain the correct
 * values. Some thing's broken. */
#define VERR_VM_REQUEST_INVALID_PACKAGE         (-1903)
/** The status field has not been updated yet as the request is still
 * pending completion. Someone queried the iStatus field before the request
 * has been fully processed. */
#define VERR_VM_REQUEST_STATUS_STILL_PENDING    (-1904)
/** The request has been freed, don't read the status now.
 * Someone is reading the iStatus field of a freed request packet. */
#define VERR_VM_REQUEST_STATUS_FREED            (-1905)
/** A VM api requiring EMT was called from another thread.
 * Use the VMR3ReqCall() apis to call it! */
#define VERR_VM_THREAD_NOT_EMT                  (-1906)
/** The VM state was invalid for the requested operation.
 * Go check the 'VM Statechart Diagram.gif'. */
#define VERR_VM_INVALID_VM_STATE                (-1907)
/** The support driver is not installed.
 * On linux, open returned ENOENT. */
#define VERR_VM_DRIVER_NOT_INSTALLED            (-1908)
/** The support driver is not accessible.
 * On linux, open returned EPERM. */
#define VERR_VM_DRIVER_NOT_ACCESSIBLE           (-1909)
/** Was not able to load the support driver.
 * On linux, open returned ENODEV. */
#define VERR_VM_DRIVER_LOAD_ERROR               (-1910)
/** Was not able to open the support driver.
 * Generic open error used when none of the other ones fit. */
#define VERR_VM_DRIVER_OPEN_ERROR               (-1911)
/** The installed support driver doesn't match the version of the user. */
#define VERR_VM_DRIVER_VERSION_MISMATCH         (-1912)
/** Saving the VM state is temporarily not allowed. Try again later. */
#define VERR_VM_SAVE_STATE_NOT_ALLOWED          (-1913)
/** @} */


/** @name VBox Remote Desktop Protocol (VRDP) Status Codes
 * @{
 */
/** Successful completion of operation (mapped to generic iprt status code). */
#define VINF_VRDP_SUCCESS                   VINF_SUCCESS
/** VRDP transport operation timed out (mapped to generic iprt status code). */
#define VERR_VRDP_TIMEOUT                   VERR_TIMEOUT

/** Unsupported ISO protocol feature */
#define VERR_VRDP_ISO_UNSUPPORTED           (-2000)
/** Security (en/decryption) engine error */
#define VERR_VRDP_SEC_ENGINE_FAIL           (-2001)
/** VRDP protocol violation */
#define VERR_VRDP_PROTOCOL_ERROR            (-2002)
/** Unsupported VRDP protocol feature */
#define VERR_VRDP_NOT_SUPPORTED             (-2003)
/** VRDP protocol violation, client sends less data than expected */
#define VERR_VRDP_INSUFFICIENT_DATA         (-2004)
/** Internal error, VRDP packet is in wrong operation mode */
#define VERR_VRDP_INVALID_MODE              (-2005)
/** Memory allocation failed */
#define VERR_VRDP_NO_MEMORY                 (-2006)
/** Client has been rejected */
#define VERR_VRDP_ACCESS_DENIED             (-2007)
/** VRPD receives a packet that is not supported */
#define VWRN_VRDP_PDU_NOT_SUPPORTED         2008
/** VRDP script allowed the packet to be processed further */
#define VINF_VRDP_PROCESS_PDU               2009
/** VRDP script has completed its task */
#define VINF_VRDP_OPERATION_COMPLETED       2010
/** VRDP thread has started OK and will run */
#define VINF_VRDP_THREAD_STARTED            2011
/** Framebuffer is resized, terminate send bitmap procedure */
#define VINF_VRDP_RESIZE_REQUESTED          2012
/** Output can be enabled for the client. */
#define VINF_VRDP_OUTPUT_ENABLE             2013
/** @} */


/** @name Configuration Manager (CFGM) Status Codes
 * @{
 */
/** The integer value was too big for the requested representation. */
#define VERR_CFGM_INTEGER_TOO_BIG           (-2100)
/** Child node was not found. */
#define VERR_CFGM_CHILD_NOT_FOUND           (-2101)
/** Path to child node was invalid (i.e. empty). */
#define VERR_CFGM_INVALID_CHILD_PATH        (-2102)
/** Value not found. */
#define VERR_CFGM_VALUE_NOT_FOUND           (-2103)
/** No parent node specified. */
#define VERR_CFGM_NO_PARENT                 (-2104)
/** No node was specified. */
#define VERR_CFGM_NO_NODE                   (-2105)
/** The value is not an integer. */
#define VERR_CFGM_NOT_INTEGER               (-2106)
/** The value is not a zero terminated character string. */
#define VERR_CFGM_NOT_STRING                (-2107)
/** The value is not a byte string. */
#define VERR_CFGM_NOT_BYTES                 (-2108)
/** The specified string / bytes buffer was to small. Specify a larger one and retry. */
#define VERR_CFGM_NOT_ENOUGH_SPACE          (-2109)
/** The path of a new node contained slashs or was empty. */
#define VERR_CFGM_INVALID_NODE_PATH         (-2160)
/** A new node couldn't be inserted because one with the same name exists. */
#define VERR_CFGM_NODE_EXISTS               (-2161)
/** A new leaf couldn't be inserted because one with the same name exists. */
#define VERR_CFGM_LEAF_EXISTS               (-2162)
/** @} */


/** @name Time Manager (TM) Status Codes
 * @{
 */
/** The loaded timer state was incorrect. */
#define VERR_TM_LOAD_STATE                  (-2200)
/** The timer was not in the correct state for the request operation. */
#define VERR_TM_INVALID_STATE               (-2201)
/** The timer was in a unknown state. Corruption or stupid coding error. */
#define VERR_TM_UNKNOWN_STATE               (-2202)
/** The timer was stuck in an unstable state until we grew impatient and returned. */
#define VERR_TM_UNSTABLE_STATE              (-2203)
/** @} */


/** @name Recompiled Execution Manager (REM) Status Codes
 * @{
 */
/** Fatal error in virtual hardware. */
#define VERR_REM_VIRTUAL_HARDWARE_ERROR     (-2300)
/** Fatal error in the recompiler cpu. */
#define VERR_REM_VIRTUAL_CPU_ERROR          (-2301)
/** Recompiler execution was interrupted by forced action. */
#define VINF_REM_INTERRUPED_FF              2302
/** Reason for leaving GC: Must flush pending invlpg operations to REM.
 * Tell REM to flush page invalidations. Will temporary go to REM context
 * from REM and perform the flushes. */
#define VERR_REM_FLUSHED_PAGES_OVERFLOW     (-2303)
/** Too many similar traps. This is a very useful debug only
 * check (we don't do double/tripple faults in REM). */
#define VERR_REM_TOO_MANY_TRAPS             (-2304)
/** The REM is out of breakpoint slots. */
#define VERR_REM_NO_MORE_BP_SLOTS           (-2305)
/** The REM could not find any breakpoint on the specified address. */
#define VERR_REM_BP_NOT_FOUND               (-2306)
/** @} */


/** @name Trap Manager / Monitor (TRPM) Status Codes
 * @{
 */
/** No active trap. Cannot query or reset a non-existing trap. */
#define VERR_TRPM_NO_ACTIVE_TRAP            (-2400)
/** Active trap. Cannot assert a new trap when when one is already active. */
#define VERR_TRPM_ACTIVE_TRAP               (-2401)
/** Reason for leaving GC: Guest tried to write to our IDT - fatal.
 * The VM will be terminated assuming the worst, i.e. that the
 * guest has read the idtr register. */
#define VERR_TRPM_SHADOW_IDT_WRITE          (-2402)
/** Reason for leaving GC: Fatal trap in hypervisor. */
#define VERR_TRPM_DONT_PANIC                (-2403)
/** Reason for leaving GC: Double Fault. */
#define VERR_TRPM_PANIC                     (-2404)
/** The exception was dispatched for raw-mode execution. */
#define VINF_TRPM_XCPT_DISPATCHED           2405
/** @} */


/** @name Selector Manager / Monitor (SELM) Status Code
 * @{
 */
/** Reason for leaving GC: Guest tried to write to our GDT - fatal.
 * The VM will be terminated assuming the worst, i.e. that the
 * guest has read the gdtr register. */
#define VERR_SELM_SHADOW_GDT_WRITE          (-2500)
/** Reason for leaving GC: Guest tried to write to our LDT - fatal.
 * The VM will be terminated assuming the worst, i.e. that the
 * guest has read the ldtr register. */
#define VERR_SELM_SHADOW_LDT_WRITE          (-2501)
/** Reason for leaving GC: Guest tried to write to our TSS - fatal.
 * The VM will be terminated assuming the worst, i.e. that the
 * guest has read the ltr register. */
#define VERR_SELM_SHADOW_TSS_WRITE          (-2502)
/** Reason for leaving GC: Sync the GDT table to solve a conflict. */
#define VINF_SELM_SYNC_GDT                  2503
/** No valid TSS present. */
#define VERR_SELM_NO_TSS                    (-2504)
/** @} */


/** @name I/O Manager / Monitor (IOM) Status Code
 * @{
 */
/** The specified I/O port range was invalid.
 * It was either empty or it was out of bounds. */
#define VERR_IOM_INVALID_IOPORT_RANGE       (-2600)
/** The specified GC I/O port range didn't have a corresponding HC range.
 * IOMIOPortRegisterHC() must be called before IOMIOPortRegisterGC(). */
#define VERR_IOM_NO_HC_IOPORT_RANGE         (-2601)
/** The specified I/O port range intruded on an existing range. There is
 * a I/O port conflict between two device, or a device tried to register
 * the same range twice. */
#define VERR_IOM_IOPORT_RANGE_CONFLICT      (-2602)
/** The I/O port range specified for removal wasn't found or it wasn't contiguous. */
#define VERR_IOM_IOPORT_RANGE_NOT_FOUND     (-2603)
/** The specified I/O port range was owned by some other device(s). Both registration
 * and deregistration, but in the first case only GC ranges. */
#define VERR_IOM_NOT_IOPORT_RANGE_OWNER     (-2604)

/** The specified MMIO range was invalid.
 * It was either empty or it was out of bounds. */
#define VERR_IOM_INVALID_MMIO_RANGE         (-2605)
/** The specified GC MMIO range didn't have a corresponding HC range.
 * IOMMMIORegisterHC() must be called before IOMMMIORegisterGC(). */
#define VERR_IOM_NO_HC_MMIO_RANGE           (-2606)
/** The specified MMIO range was owned by some other device(s). Both registration
 * and deregistration, but in the first case only GC ranges. */
#define VERR_IOM_NOT_MMIO_RANGE_OWNER       (-2607)
/** The specified MMIO range intruded on an existing range. There is
 * a MMIO conflict between two device, or a device tried to register
 * the same range twice. */
#define VERR_IOM_MMIO_RANGE_CONFLICT        (-2608)
/** The MMIO range specified for removal was not found. */
#define VERR_IOM_MMIO_RANGE_NOT_FOUND       (-2609)
/** The MMIO range specified for removal was invalid. The range didn't match
 * quite match a set of existing ranges. It's not possible to remove parts of
 * a MMIO range, only one or more full ranges. */
#define VERR_IOM_INCOMPLETE_MMIO_RANGE      (-2610)
/** An invalid I/O port size was specified for a read or write operation. */
#define VERR_IOM_INVALID_IOPORT_SIZE        (-2611)
/** The MMIO handler was called for a bogus address! Internal error! */
#define VERR_IOM_MMIO_HANDLER_BOGUS_CALL    (-2612)
/** The MMIO handler experienced a problem with the disassembler. */
#define VERR_IOM_MMIO_HANDLER_DISASM_ERROR  (-2613)
/** The port being read was not present(/unused) and IOM shall return ~0 according to size. */
#define VERR_IOM_IOPORT_UNUSED              (-2614)
/** Unused MMIO register read, fill with 00. */
#define VINF_IOM_MMIO_UNUSED_00             2615
/** Unused MMIO register read, fill with FF. */
#define VINF_IOM_MMIO_UNUSED_FF             2616

/** Reason for leaving GC: I/O port read. */
#define VINF_IOM_HC_IOPORT_READ             2620
/** Reason for leaving GC: I/O port write. */
#define VINF_IOM_HC_IOPORT_WRITE            2621
/** Reason for leaving GC: MMIO write. */
#define VINF_IOM_HC_MMIO_READ               2623
/** Reason for leaving GC: MMIO read. */
#define VINF_IOM_HC_MMIO_WRITE              2624
/** Reason for leaving GC: MMIO read/write. */
#define VINF_IOM_HC_MMIO_READ_WRITE         2625
/** @} */


/** @name Virtual Machine Monitor (VMM) Status Codes
 * @{
 */
/** Reason for leaving GC: Calling host function. */
#define VINF_VMM_CALL_HOST                  2700
/** Reason for leaving R0: Hit a ring-0 assertion on EMT. */
#define VERR_VMM_RING0_ASSERTION            (-2701)
/** @} */


/** @name Pluggable Device and Driver Manager (PDM) Status Codes
 * @{
 */
/** An invalid LUN specification was given. */
#define VERR_PDM_NO_SUCH_LUN                        (-2800)
/** A device encountered an unknown configuration value.
 * This means that the device is potentially misconfigured and the device
 * construction or unit attachment failed because of this. */
#define VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES          (-2801)
/** The above driver doesn't export a interface required by a driver being
 * attached to it. Typical misconfiguration problem. */
#define VERR_PDM_MISSING_INTERFACE_ABOVE            (-2802)
/** The below driver doesn't export a interface required by the drive
 * having attached it. Typical misconfiguration problem. */
#define VERR_PDM_MISSING_INTERFACE_BELOW            (-2803)
/** A device didn't find a required interface with an attached driver.
 * Typical misconfiguration problem. */
#define VERR_PDM_MISSING_INTERFACE                  (-2804)
/** A driver encountered an unknown configuration value.
 * This means that the driver is potentially misconfigured and the driver
 * construction failed because of this. */
#define VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES          (-2805)
/** The PCI bus assigned to a device didn't have room for it.
 * Either too many devices are configured on the same PCI bus, or there are
 * some internal problem where PDM/PCI doesn't free up slots when unplugging devices. */
#define VERR_PDM_TOO_PCI_MANY_DEVICES               (-2806)
/** A queue is out of free items, the queueing operation failed. */
#define VERR_PDM_NO_QUEUE_ITEMS                     (-2807)
/** Not possible to attach further drivers to the driver.
 * A driver which doesn't support attachments (below of course) will
 * return this status code if it found that further drivers were configured
 * to be attached to it. */
#define VERR_PDM_DRVINS_NO_ATTACH                   (-2808)
/** Not possible to attach drivers to the device.
 * A device which doesn't support attachments (below of course) will
 * return this status code if it found that drivers were configured
 * to be attached to it. */
#define VERR_PDM_DEVINS_NO_ATTACH                   (-2809)
/** No attached driver.
 * The PDMDRVHLP::pfnAttach and PDMDEVHLP::pfnDriverAttach will return
 * this error when no driver was configured to be attached. */
#define VERR_PDM_NO_ATTACHED_DRIVER                 (-2810)
/** The media geometry hasn't been set yet, so it cannot be obtained.
 * The caller should then calculate the geometry from the media size. */
#define VERR_PDM_GEOMETRY_NOT_SET                   (-2811)
/** The media translation hasn't been set yet, so it cannot be obtained.
 * The caller should then guess the translation. */
#define VERR_PDM_TRANSLATION_NOT_SET                (-2812)
/** The media is not mounted, operation requires a mounted media. */
#define VERR_PDM_MEDIA_NOT_MOUNTED                  (-2813)
/** Mount failed because a media was already mounted. Unmount the media
 * and retry the mount. */
#define VERR_PDM_MEDIA_MOUNTED                      (-2814)
/** The media is locked and cannot be unmounted. */
#define VERR_PDM_MEDIA_LOCKED                       (-2815)
/** No 'Type' attribute in the DrvBlock configuration.
 * Misconfiguration. */
#define VERR_PDM_BLOCK_NO_TYPE                      (-2816)
/** The 'Type' attribute in the DrvBlock configuration had an unknown value.
 * Misconfiguration. */
#define VERR_PDM_BLOCK_UNKNOWN_TYPE                 (-2817)
/** The 'Translation' attribute in the DrvBlock configuration had an unknown value.
 * Misconfiguration. */
#define VERR_PDM_BLOCK_UNKNOWN_TRANSLATION          (-2818)
/** The block driver type wasn't supported.
 * Misconfiguration of the kind you get when attaching a floppy to an IDE controller. */
#define VERR_PDM_UNSUPPORTED_BLOCK_TYPE             (-2819)
/** A attach or prepare mount call failed because the driver already
 * had a driver attached. */
#define VERR_PDM_DRIVER_ALREADY_ATTACHED            (-2820)
/** An attempt on deattaching a driver without anyone actually being attached, or
 * performing any other operation on an attached driver. */
#define VERR_PDM_NO_DRIVER_ATTACHED                 (-2821)
/** The attached driver configuration is missing the 'Driver' attribute. */
#define VERR_PDM_CFG_MISSING_DRIVER_NAME            (-2822)
/** The configured driver wasn't found.
 * Either the necessary driver modules wasn't loaded, the name was
 * misspelled, or it was a misconfiguration. */
#define VERR_PDM_DRIVER_NOT_FOUND                   (-2823)
/** The Ring-3 module was already loaded. */
#define VINF_PDM_ALREADY_LOADED                     (2824)
/** The name of the module clashed with an existing module. */
#define VERR_PDM_MODULE_NAME_CLASH                  (-2825)
/** Couldn't find any export for registration of drivers/devices. */
#define VERR_PDM_NO_REGISTRATION_EXPORT             (-2826)
/** A module name is too long. */
#define VERR_PDM_MODULE_NAME_TOO_LONG               (-2827)
/** Driver name clash. Another driver with the same name as the
 * one begin registred exists. */
#define VERR_PDM_DRIVER_NAME_CLASH                  (-2828)
/** The version of the driver registration structure is unknown
 * to this VBox version. Either mixing incompatible versions or
 * the structure isn't correctly initialized. */
#define VERR_PDM_UNKNOWN_DRVREG_VERSION             (-2829)
/** Invalid entry in the driver registration structure. */
#define VERR_PDM_INVALID_DRIVER_REGISTRATION        (-2830)
/** Invalid host bit mask. */
#define VERR_PDM_INVALID_DRIVER_HOST_BITS           (-2831)
/** Not possible to detach a driver because the above driver/device
 * doesn't support it. The above entity doesn't implement the pfnDetach call. */
#define VERR_PDM_DRIVER_DETACH_NOT_POSSIBLE         (-2832)
/** No PCI Bus is available to register the device with. This is usually a
 * misconfiguration or in rare cases a buggy pci device. */
#define VERR_PDM_NO_PCI_BUS                         (-2833)
/** The device is not a registered PCI device and thus cannot
 * perform any PCI operations. The device forgot to register it self. */
#define VERR_PDM_NOT_PCI_DEVICE                     (-2834)

/** The version of the device registration structure is unknown
 * to this VBox version. Either mixing incompatible versions or
 * the structure isn't correctly initialized. */
#define VERR_PDM_UNKNOWN_DEVREG_VERSION             (-2835)
/** Invalid entry in the device registration structure. */
#define VERR_PDM_INVALID_DEVICE_REGISTRATION        (-2836)
/** Invalid host bit mask. */
#define VERR_PDM_INVALID_DEVICE_GUEST_BITS          (-2837)
/** The guest bit mask didn't match the guest being loaded. */
#define VERR_PDM_INVALID_DEVICE_HOST_BITS           (-2838)
/** Device name clash. Another device with the same name as the
 * one begin registred exists. */
#define VERR_PDM_DEVICE_NAME_CLASH                  (-2839)
/** The device wasn't found. There was no registered device
 * by that name. */
#define VERR_PDM_DEVICE_NOT_FOUND                   (-2840)
/** The device instance was not found. */
#define VERR_PDM_DEVICE_INSTANCE_NOT_FOUND          (-2841)
/** The device instance have no base interface. */
#define VERR_PDM_DEVICE_INSTANCE_NO_IBASE           (-2842)
/** The device instance have no such logical unit. */
#define VERR_PDM_DEVICE_INSTANCE_LUN_NOT_FOUND      (-2843)
/** The driver instance could not be found. */
#define VERR_PDM_DRIVER_INSTANCE_NOT_FOUND          (-2844)
/** Logical Unit was not found. */
#define VERR_PDM_LUN_NOT_FOUND                      (-2845)
/** The Logical Unit was found, but it had no driver attached to it. */
#define VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN          (-2846)
/** The Logical Unit was found, but it had no driver attached to it. */
#define VINF_PDM_NO_DRIVER_ATTACHED_TO_LUN          2846
/** No PIC device instance is registered with the current VM and thus
 * the PIC operation cannot be performed. */
#define VERR_PDM_NO_PIC_INSTANCE                    (-2847)
/** No APIC device instance is registered with the current VM and thus
 * the APIC operation cannot be performed. */
#define VERR_PDM_NO_APIC_INSTANCE                   (-2848)
/** No DMAC device instance is registered with the current VM and thus
 * the DMA operation cannot be performed. */
#define VERR_PDM_NO_DMAC_INSTANCE                   (-2849)
/** No RTC device instance is registered with the current VM and thus
 * the RTC or CMOS operation cannot be performed. */
#define VERR_PDM_NO_RTC_INSTANCE                    (-2850)
/** Unable to open the host interface due to a sharing violation . */
#define VERR_PDM_HIF_SHARING_VIOLATION              (-2851)
/** Unable to open the host interface. */
#define VERR_PDM_HIF_OPEN_FAILED                    (-2852)
/** The device doesn't support runtime driver attaching.
 * The PDMDEVREG::pfnAttach callback function is NULL. */
#define VERR_PDM_DEVICE_NO_RT_ATTACH                (-2853)
/** The device doesn't support runtime driver detaching.
 * The PDMDEVREG::pfnDetach callback function is NULL. */
#define VERR_PDM_DEVICE_NO_RT_DETACH                (-2854)
/** Invalid host interface version. */
#define VERR_PDM_HIF_INVALID_VERSION                (-2855)

/** The version of the USB device registration structure is unknown
 * to this VBox version. Either mixing incompatible versions or
 * the structure isn't correctly initialized. */
#define VERR_PDM_UNKNOWN_USBREG_VERSION             (-2856)
/** Invalid entry in the device registration structure. */
#define VERR_PDM_INVALID_USB_REGISTRATION           (-2857)
/** Driver name clash. Another driver with the same name as the
 * one begin registred exists. */
#define VERR_PDM_USB_NAME_CLASH                     (-2858)
/** The USB hub is already registered. */
#define VERR_PDM_USB_HUB_EXISTS                     (-2859)
/** Couldn't find any USB hubs to attach the device to. */
#define VERR_PDM_NO_USB_HUBS                        (-2860)
/** Couldn't find any free USB ports to attach the device to. */
#define VERR_PDM_NO_USB_PORTS                       (-2861)
/** Couldn't find the USB Proxy device. Using OSE? */
#define VERR_PDM_NO_USBPROXY                        (-2862)
/** The async completion template is still used. */
#define VERR_PDM_ASYNC_TEMPLATE_BUSY                (-2863)
/** The async completion task is already suspended. */
#define VERR_PDM_ASYNC_COMPLETION_ALREADY_SUSPENDED (-2864)
/** The async completion task is not suspended. */
#define VERR_PDM_ASYNC_COMPLETION_NOT_SUSPENDED     (-2865)
/** The driver properties were invalid, and as a consequence construction
 * failed. Caused my unusable media or similar problems. */
#define VERR_PDM_DRIVER_INVALID_PROPERTIES          (-2866)
/** @} */


/** @name Host-Guest Communication Manager (HGCM) Status Codes
 * @{
 */
/** Requested service does not exist. */
#define VERR_HGCM_SERVICE_NOT_FOUND                 (-2900)
/** Service rejected client connection */
#define VINF_HGCM_CLIENT_REJECTED                   2901
/** Command address is invalid. */
#define VERR_HGCM_INVALID_CMD_ADDRESS               (-2902)
/** Service will execute the command in background. */
#define VINF_HGCM_ASYNC_EXECUTE                     2903
/** HGCM could not perform requested operation because of an internal error. */
#define VERR_HGCM_INTERNAL                          (-2904)
/** Invalid HGCM client id. */
#define VERR_HGCM_INVALID_CLIENT_ID                 (-2905)
/** The HGCM is saving state. */
#define VINF_HGCM_SAVE_STATE                        (2906)
/** Requested service already exists. */
#define VERR_HGCM_SERVICE_EXISTS                    (-2907)

/** @} */


/** @name Network Address Translation Driver (DrvNAT) Status Codes
 * @{
 */
/** Failed to find the DNS configured for this machine. */
#define VINF_NAT_DNS                                3000
/** Failed to convert the specified Guest IP to a binary IP address.
 * Malformed input. */
#define VERR_NAT_REDIR_GUEST_IP                     (-3001)
/** Failed while setting up a redirector rule.
 * There probably is a conflict between the rule and some existing
 * service on the computer. */
#define VERR_NAT_REDIR_SETUP                        (-3002)
/** @} */


/** @name HostIF Driver (DrvTUN) Status Codes
 * @{
 */
/** The Host Interface Networking init program failed. */
#define VERR_HOSTIF_INIT_FAILED                     (-3100)
/** The Host Interface Networking device name is too long. */
#define VERR_HOSTIF_DEVICE_NAME_TOO_LONG            (-3101)
/** The Host Interface Networking name config IOCTL call failed. */
#define VERR_HOSTIF_IOCTL                           (-3102)
/** Failed to make the Host Interface Networking handle non-blocking. */
#define VERR_HOSTIF_BLOCKING                        (-3103)
/** If a Host Interface Networking filehandle was specified it's not allowed to
 * have any init or term programs. */
#define VERR_HOSTIF_FD_AND_INIT_TERM                (-3104)
/** The Host Interface Networking terminate program failed. */
#define VERR_HOSTIF_TERM_FAILED                     (-3105)
/** @} */


/** @name VBox HDD Container (VD) Status Codes
 * @{
 */
/** Invalid image type. */
#define VERR_VD_INVALID_TYPE                        (-3200)
/** Operation can't be done in current HDD container state. */
#define VERR_VD_INVALID_STATE                       (-3201)
/** Configuration value not found. */
#define VERR_VD_VALUE_NOT_FOUND                     (-3202)
/** Virtual HDD is not opened. */
#define VERR_VD_NOT_OPENED                          (-3203)
/** Requested image is not opened. */
#define VERR_VD_IMAGE_NOT_FOUND                     (-3204)
/** Image is read-only. */
#define VERR_VD_IMAGE_READ_ONLY                     (-3205)
/** Geometry hasn't been set. */
#define VERR_VD_GEOMETRY_NOT_SET                    (-3206)
/** No data for this block in image. */
#define VERR_VD_BLOCK_FREE                          (-3207)
/** Differencing and parent images can't be used together due to UUID. */
#define VERR_VD_UUID_MISMATCH                       (-3208)
/** Asynchronous I/O request finished. */
#define VINF_VD_ASYNC_IO_FINISHED                   3209
/** Asynchronous I/O is not finished yet. */
#define VERR_VD_ASYNC_IO_IN_PROGRESS                (-3210)
/** The image is too small or too large for this format. */
#define VERR_VD_INVALID_SIZE                        (-3211)
/** Generic: Invalid image file header. Use this for plugins. */
#define VERR_VD_GEN_INVALID_HEADER                  (-3220)
/** VDI: Invalid image file header. */
#define VERR_VD_VDI_INVALID_HEADER                  (-3230)
/** VDI: Invalid image file header: invalid signature. */
#define VERR_VD_VDI_INVALID_SIGNATURE               (-3231)
/** VDI: Invalid image file header: invalid version. */
#define VERR_VD_VDI_UNSUPPORTED_VERSION             (-3232)
/** Comment string is too long. */
#define VERR_VD_VDI_COMMENT_TOO_LONG                (-3233)
/** VMDK: Invalid image file header. */
#define VERR_VD_VMDK_INVALID_HEADER                 (-3240)
/** VMDK: Invalid image file header: invalid version. */
#define VERR_VD_VMDK_UNSUPPORTED_VERSION            (-3241)
/** VMDK: Image property not found. */
#define VERR_VD_VMDK_VALUE_NOT_FOUND                (-3242)
/** VMDK: Operation can't be done in current image state. */
#define VERR_VD_VMDK_INVALID_STATE                  (-3243)
/** VMDK: Format is invalid/inconsistent. */
#define VERR_VD_VMDK_INVALID_FORMAT                 (-3244)
/** VMDK: Invalid write position. */
#define VERR_VD_VMDK_INVALID_WRITE                  (-3245)
/** iSCSI: Invalid header, i.e. dummy for validity check. */
#define VERR_VD_ISCSI_INVALID_HEADER                (-3250)
/** iSCSI: Configuration value is unknown. This indicates misconfiguration. */
#define VERR_VD_ISCSI_UNKNOWN_CFG_VALUES            (-3251)
/** iSCSI: Interface is unknown. This indicates misconfiguration. */
#define VERR_VD_ISCSI_UNKNOWN_INTERFACE             (-3252)
/** iSCSI: Operation can't be done in current image state. */
#define VERR_VD_ISCSI_INVALID_STATE                 (-3253)
/** iSCSI: Invalid device type (not a disk). */
#define VERR_VD_ISCSI_INVALID_TYPE                  (-3254)
/** VHD: Invalid image file header. */
#define VERR_VD_VHD_INVALID_HEADER                  (-3260)
/** Raw: Invalid image file header. */
#define VERR_VD_RAW_INVALID_HEADER                  (-3270)
/** Raw: Invalid image file type. */
#define VERR_VD_RAW_INVALID_TYPE                    (-3271)
/** @} */


/** @name VBox Guest Library (VBGL) Status Codes
 * @{
 */
/** Library was not initialized. */
#define VERR_VBGL_NOT_INITIALIZED                   (-3300)
/** Virtual address was not allocated by the library. */
#define VERR_VBGL_INVALID_ADDR                      (-3301)
/** IOCtl to VBoxGuest driver failed. */
#define VERR_VBGL_IOCTL_FAILED                      (-3302)
/** @} */


/** @name VBox USB (VUSB) Status Codes
 * @{
 */
/** No available ports on the hub.
 * This error is returned when a device is attempted created and/or attached
 * to a hub which is out of ports. */
#define VERR_VUSB_NO_PORTS                          (-3400)
/** The requested operation cannot be performed on a detached USB device. */
#define VERR_VUSB_DEVICE_NOT_ATTACHED               (-3401)
/** Failed to allocate memory for a URB. */
#define VERR_VUSB_NO_URB_MEMORY                     (-3402)
/** General failure during URB queuing.
 * This will go away when the queueing gets proper status code handling. */
#define VERR_VUSB_FAILED_TO_QUEUE_URB               (-3403)
/** Device creation failed because the USB device name was not found. */
#define VERR_VUSB_DEVICE_NAME_NOT_FOUND             (-3404)
/** Not permitted to open the USB device.
 * The user doesn't have access to the device in the usbfs, check the mount options. */
#define VERR_VUSB_USBFS_PERMISSION                  (-3405)
/** The requested operation cannot be performed because the device
 * is currently being reset. */
#define VERR_VUSB_DEVICE_IS_RESETTING               (-3406)
/** @} */


/** @name VBox VGA Status Codes
 * @{
 */
/** One of the custom modes was incorrect.
 * The format or bit count of the custom mode value is invalid. */
#define VERR_VGA_INVALID_CUSTOM_MODE                (-3500)
/** The display connector is resizing. */
#define VINF_VGA_RESIZE_IN_PROGRESS                 (3501)
/** @} */


/** @name Internal Networking Status Codes
 * @{
 */
/** The networking interface to filter was not found. */
#define VERR_INTNET_FLT_IF_NOT_FOUND                (-3600)
/** The networking interface to filter was busy (used by someone). */
#define VERR_INTNET_FLT_IF_BUSY                     (-3601)
/** Failed to create or connect to a networking interface filter. */
#define VERR_INTNET_FLT_IF_FAILED                   (-3602)
/** The network already exists with a different trunk configuration. */
#define VERR_INTNET_INCOMPATIBLE_TRUNK              (-3603)
/** The network already exists with a different security profile (restricted / public). */
#define VERR_INTNET_INCOMPATIBLE_FLAGS              (-3604)
/** @} */


/** @name Support Driver Status Codes
 * @{
 */
/** The component factory was not found. */
#define VERR_SUPDRV_COMPONENT_NOT_FOUND             (-3700)
/** The component factories do not support the requested interface. */
#define VERR_SUPDRV_INTERFACE_NOT_SUPPORTED         (-3701)
/** The service module was not found. */
#define VERR_SUPDRV_SERVICE_NOT_FOUND               (-3702)
/** @} */


/** @name VBox GMM Status Codes
 * @{
 */
/** The GMM is out of pages and needs to be give another chunk of user memory that
 * it can lock down and borrow pages from. */
#define VERR_GMM_SEED_ME                            (-3800)
/** Unable to allocate more pages from the host system. */
#define VERR_GMM_OUT_OF_MEMORY                      (-3801)
/** Hit the global allocation limit.
 * If you know there is still sufficient memory available, try raise the limit. */
#define VERR_GMM_HIT_GLOBAL_LIMIT                   (-3802)
/** Hit the a VM account limit. */
#define VERR_GMM_HIT_VM_ACCOUNT_LIMIT               (-3803)
/** Attempt to free more memory than what was previously allocated. */
#define VERR_GMM_ATTEMPT_TO_FREE_TOO_MUCH           (-3804)
/** Attempted to report too many pages as deflated.  */
#define VERR_GMM_ATTEMPT_TO_DEFLATE_TOO_MUCH        (-3805)
/** The page to be freed or updated was not found. */
#define VERR_GMM_PAGE_NOT_FOUND                     (-3806)
/** The specified shared page was not actually private. */
#define VERR_GMM_PAGE_NOT_PRIVATE                   (-3807)
/** The specified shared page was not actually shared. */
#define VERR_GMM_PAGE_NOT_SHARED                    (-3808)
/** The page to be freed was already freed. */
#define VERR_GMM_PAGE_ALREADY_FREE                  (-3809)
/** The page to be updated or freed was noted owned by the caller. */
#define VERR_GMM_NOT_PAGE_OWNER                     (-3810)
/** The specified chunk was not found. */
#define VERR_GMM_CHUNK_NOT_FOUND                    (-3811)
/** The chunk has already been mapped into the process. */
#define VERR_GMM_CHUNK_ALREADY_MAPPED               (-3812)
/** The chunk to be unmapped isn't actually mapped into the process. */
#define VERR_GMM_CHUNK_NOT_MAPPED                   (-3813)
/** The reservation or reservation update was declined - too many VMs, too
 * little memory, and/or too low GMM configuration. */
#define VERR_GMM_MEMORY_RESERVATION_DECLINED        (-3814)
/** @} */


/** @name VBox GVM Status Codes
 * @{
 */
/** The GVM is out of VM handle space. */
#define VERR_GVM_TOO_MANY_VMS                       (-3900)
/** The EMT was not blocked at the time of the call.  */
#define VINF_GVM_NOT_BLOCKED                        3901
/** The EMT was not busy running guest code at the time of the call. */
#define VINF_GVM_NOT_BUSY_IN_GC                     3902
/** RTThreadYield was called during a GVMMR0SchedPoll call. */
#define VINF_GVM_YIELDED                            3903
/** @} */


/** @name VBox VMX Status Codes
 * @{
 */
/** Invalid VMCS index or write to read-only element. */
#define VERR_VMX_INVALID_VMCS_FIELD                 (-4000)
/** Invalid VMCS pointer. */
#define VERR_VMX_INVALID_VMCS_PTR                   (-4001)
/** Invalid VMXON pointer. */
#define VERR_VMX_INVALID_VMXON_PTR                  (-4002)
/** Generic VMX failure. */
#define VERR_VMX_GENERIC                            (-4003)
/** Invalid CPU mode for VMX execution. */
#define VERR_VMX_UNSUPPORTED_MODE                   (-4004)
/** Unable to start VM execution. */
#define VERR_VMX_UNABLE_TO_START_VM                 (-4005)
/** Unable to resume VM execution. */
#define VERR_VMX_UNABLE_TO_RESUME_VM                (-4006)
/** Unable to switch due to invalid host state. */
#define VERR_VMX_INVALID_HOST_STATE                 (-4007)
/** IA32_FEATURE_CONTROL MSR not setup correcty (turn on VMX in the host system BIOS) */
#define VERR_VMX_ILLEGAL_FEATURE_CONTROL_MSR        (-4008)
/** VMX CPU extension not available */
#define VERR_VMX_NO_VMX                             (-4009)
/** VMXON failed; possibly because it was already run before */
#define VERR_VMX_VMXON_FAILED                       (-4010)
/** CPU was incorrectly left in VMX root mode; incompatible with VirtualBox */
#define VERR_VMX_IN_VMX_ROOT_MODE                   (-4011)
/** Somebody cleared X86_CR4_VMXE in the CR4 register. */
#define VERR_VMX_X86_CR4_VMXE_CLEARED               (-4012)
/** VT-x features locked or unavailable in MSR. */
#define VERR_VMX_MSR_LOCKED_OR_DISABLED             (-4013)
/** Unable to switch due to invalid guest state. */
#define VERR_VMX_INVALID_GUEST_STATE                (-4014)
/** Unexpected VM exit code. */
#define VERR_VMX_UNEXPECTED_EXIT_CODE               (-4015)
/** Unexpected VM exception code. */
#define VERR_VMX_UNEXPECTED_EXCEPTION               (-4016)
/** Unexpected interruption exit code. */
#define VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_CODE  (-4017)
/** Running for too long, return to ring 3. */
#define VINF_VMX_PREEMPT_PENDING                    (4018)
/** @} */


/** @name VBox SVM Status Codes
 * @{
 */
/** Unable to start VM execution. */
#define VERR_SVM_UNABLE_TO_START_VM                 (-4050)
/** SVM bit not set in K6_EFER MSR */
#define VERR_SVM_ILLEGAL_EFER_MSR                   (-4051)
/** SVM CPU extension not available. */
#define VERR_SVM_NO_SVM                             (-4052)
/** SVM CPU extension disabled (by BIOS). */
#define VERR_SVM_DISABLED                           (-4053)
/** Running for too long, return to ring 3. */
#define VINF_SVM_PREEMPT_PENDING                    VINF_VMX_PREEMPT_PENDING
/** @} */


/** @name VBox HWACCM Status Codes
 * @{
 */
/** Unable to start VM execution. */
#define VERR_HWACCM_UNKNOWN_CPU                     (-4100)
/** No CPUID support. */
#define VERR_HWACCM_NO_CPUID                        (-4101)
/** Host is about to go into suspend mode. */
#define VERR_HWACCM_SUSPEND_PENDING                 (-4102)
/** Conflicting CFGM values. */
#define VERR_HWACCM_CONFIG_MISMATCH                 (-4103)
/** @} */


/** @name VBox Disassembler Status Codes
 * @{
 */
/** Invalid opcode byte(s) */
#define VERR_DIS_INVALID_OPCODE                     (-4200)
/** Generic failure during disassembly. */
#define VERR_DIS_GEN_FAILURE                        (-4201)
/** @} */


/** @name VBox Webservice Status Codes
 * @{
 */
/** Authentication failed (ISessionManager::logon()) */
#define VERR_WEB_NOT_AUTHENTICATED                  (-4300)
/** Invalid format of managed object reference */
#define VERR_WEB_INVALID_MANAGED_OBJECT_REFERENCE   (-4301)
/** Invalid session ID in managed object reference */
#define VERR_WEB_INVALID_SESSION_ID                 (-4302)
/** Invalid object ID in managed object reference */
#define VERR_WEB_INVALID_OBJECT_ID                  (-4303)
/** Unsupported interface for managed object reference */
#define VERR_WEB_UNSUPPORTED_INTERFACE              (-4304)
/** @} */


/** @name VBox PARAV Status Codes
 * @{
 */
/** Switch back to host */
#define VINF_PARAV_SWITCH_TO_HOST                     4400

/** @} */

/* SED-END */


/** @def VBOX_SUCCESS
 * Check for success.
 *
 * @returns true if rc indicates success.
 * @returns false if rc indicates failure.
 *
 * @param   rc  The iprt status code to test.
 */
#define VBOX_SUCCESS(rc)    RT_SUCCESS(rc)

/** @def VBOX_FAILURE
 * Check for failure.
 *
 * @returns true if rc indicates failure.
 * @returns false if rc indicates success.
 *
 * @param   rc  The iprt status code to test.
 */
#define VBOX_FAILURE(rc)    RT_FAILURE(rc)

/** @} */


#endif

