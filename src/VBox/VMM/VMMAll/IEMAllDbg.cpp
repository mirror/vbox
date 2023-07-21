/* $Id$ */
/** @file
 * IEM - Debug and Logging.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IEM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/errcore.h>


/*********************************************************************************************************************************
*   Syscalls                                                                                                                     *
*********************************************************************************************************************************/

#ifdef LOG_ENABLED

# undef  LOG_GROUP
# define LOG_GROUP LOG_GROUP_IEM_SYSCALL


static void iemLogSyscallWinVxDCall(PVMCPUCC pVCpu, uint8_t cbInstr)
{
    /*
     * Two double words follow the instruction:
     *      1. Service number.
     *      2. VxD identifier.
     */
    uint16_t       auParams[2] = {0, 0};
    RTGCPTR  const GCPtrParams = pVCpu->cpum.GstCtx.cs.u64Base + pVCpu->cpum.GstCtx.rip + cbInstr;
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, auParams, GCPtrParams, sizeof(auParams));
    if (RT_SUCCESS(rc))
    {
        const char    *pszVxD     = NULL;
        const char    *pszService = NULL;
        uint16_t const idVxD      = auParams[1];
        uint16_t const idService  = auParams[0];
        switch (idVxD)
        {
            case 0x0001:
                switch (idService)
                {
                    case 0x0000: pszService = "get version"; break;
                    case 0x0001: pszService = "get current VM handle"; break;
                    case 0x0002: pszService = "test current VM handle"; break;
                    case 0x0003: pszService = "get system VM handle"; break;
                    case 0x0004: pszService = "test system VM handle"; break;
                    case 0x0005: pszService = "validate VM handle"; break;
                    case 0x0006: pszService = "get VMM reenter count"; break;
                    case 0x0007: pszService = "begin reentrant execution"; break;
                    case 0x0008: pszService = "end reentrant execution"; break;
                    case 0x0009: pszService = "install V86 breakpoint"; break;
                    case 0x000a: pszService = "remove V86 breakpoint"; break;
                    case 0x000b: pszService = "allocate V86 callback"; break;
                    case 0x000c: pszService = "allocation PM callback"; break;
                    case 0x000d: pszService = "call when VM returns"; break;
                    case 0x000e: pszService = "schedule global event"; break;
                    case 0x000f: pszService = "schedule VM event"; break;
                    case 0x0010: pszService = "call global event"; break;
                    case 0x0011: pszService = "call VM event"; break;
                    case 0x0012: pszService = "cancel global event"; break;
                    case 0x0013: pszService = "cancel VM event"; break;
                    case 0x0014: pszService = "call priority VM event"; break;
                    case 0x0015: pszService = "cancel priority VM event"; break;
                    case 0x0016: pszService = "get NMI handler address"; break;
                    case 0x0017: pszService = "set NMI handler address"; break;
                    case 0x0018: pszService = "hook NMI event"; break;
                    case 0x0019: pszService = "call when VM interrupts enabled"; break;
                    case 0x001a: pszService = "enable VM interrupts"; break;
                    case 0x001b: pszService = "disable VM interrupts"; break;
                    case 0x001c: pszService = "map flat"; break;
                    case 0x001d: pszService = "map linear to VM address"; break;
                    case 0x001e: pszService = "adjust execution priority"; break;
                    case 0x001f: pszService = "begin critical section"; break;
                    case 0x0020: pszService = "end critical section"; break;
                    case 0x0021: pszService = "end critical section and suspend"; break;
                    case 0x0022: pszService = "claim critical section"; break;
                    case 0x0023: pszService = "release critical section"; break;
                    case 0x0024: pszService = "call when not critical"; break;
                    case 0x0025: pszService = "create semaphore"; break;
                    case 0x0026: pszService = "destroy semaphore"; break;
                    case 0x0027: pszService = "wait on semaphore"; break;
                    case 0x0028: pszService = "signal semaphore"; break;
                    case 0x0029: pszService = "get critical section status"; break;
                    case 0x002a: pszService = "call when task switched"; break;
                    case 0x002b: pszService = "suspend VM"; break;
                    case 0x002c: pszService = "resume VM"; break;
                    case 0x002d: pszService = "no-fail resume VM"; break;
                    case 0x002e: pszService = "nuke VM"; break;
                    case 0x002f: pszService = "crash current VM"; break;
                    case 0x0030: pszService = "get execution focus"; break;
                    case 0x0031: pszService = "set execution focus"; break;
                    case 0x0032: pszService = "get time slice priority"; break;
                    case 0x0033: pszService = "set time slice priority"; break;
                    case 0x0034: pszService = "get time slice granularity"; break;
                    case 0x0035: pszService = "set time slice granularity"; break;
                    case 0x0036: pszService = "get time slice information"; break;
                    case 0x0037: pszService = "adjust execution time"; break;
                    case 0x0038: pszService = "release time slice"; break;
                    case 0x0039: pszService = "wake up VM"; break;
                    case 0x003a: pszService = "call when idle"; break;
                    case 0x003b: pszService = "get next VM handle"; break;
                    case 0x003c: pszService = "set global timeout"; break;
                    case 0x003d: pszService = "set VM timeout"; break;
                    case 0x003e: pszService = "cancel timeout"; break;
                    case 0x003f: pszService = "get system time"; break;
                    case 0x0040: pszService = "get VM execution time"; break;
                    case 0x0041: pszService = "hook V86 interrupt chain"; break;
                    case 0x0042: pszService = "get V86 interrupt vector"; break;
                    case 0x0043: pszService = "set V86 interrupt vector"; break;
                    case 0x0044: pszService = "get PM interrupt vector"; break;
                    case 0x0045: pszService = "set PM interrupt vector"; break;
                    case 0x0046: pszService = "simulate interrupt"; break;
                    case 0x0047: pszService = "simulate IRET"; break;
                    case 0x0048: pszService = "simulate far call"; break;
                    case 0x0049: pszService = "simulate far jump"; break;
                    case 0x004a: pszService = "simulate far RET"; break;
                    case 0x004b: pszService = "simulate far RET N"; break;
                    case 0x004c: pszService = "build interrupt stack frame"; break;
                    case 0x004d: pszService = "simulate push"; break;
                    case 0x004e: pszService = "simulate pop"; break;
                    case 0x004f: pszService = "HeapAllocate"; break;
                    case 0x0050: pszService = "HeapReAllocate"; break;
                    case 0x0051: pszService = "HeapFree"; break;
                    case 0x0052: pszService = "HeapGetSize"; break;
                    case 0x0053: pszService = "PageAllocate"; break;
                    case 0x0054: pszService = "PageReAllocate"; break;
                    case 0x0055: pszService = "PageFree"; break;
                    case 0x0056: pszService = "PageLock"; break;
                    case 0x0057: pszService = "PageUnLock"; break;
                    case 0x0058: pszService = "PageGetSizeAddr"; break;
                    case 0x0059: pszService = "PageGetAllocInfo"; break;
                    case 0x005a: pszService = "GetFreePageCount"; break;
                    case 0x005b: pszService = "GetSysPageCount"; break;
                    case 0x005c: pszService = "GetVMPgCount"; break;
                    case 0x005d: pszService = "MapIntoV86"; break;
                    case 0x005e: pszService = "PhysIntoV86"; break;
                    case 0x005f: pszService = "TestGlobalV86Mem"; break;
                    case 0x0060: pszService = "ModifyPageBits"; break;
                    case 0x0061: pszService = "copy page table"; break;
                    case 0x0062: pszService = "map linear into V86"; break;
                    case 0x0063: pszService = "linear page lock"; break;
                    case 0x0064: pszService = "linear page unlock"; break;
                    case 0x0065: pszService = "SetResetV86Pageabl"; break;
                    case 0x0066: pszService = "GetV86PageableArray"; break;
                    case 0x0067: pszService = "PageCheckLinRange"; break;
                    case 0x0068: pszService = "page out dirty pages"; break;
                    case 0x0069: pszService = "discard pages"; break;
                    case 0x006a: pszService = "GetNulPageHandle"; break;
                    case 0x006b: pszService = "get first V86 page"; break;
                    case 0x006c: pszService = "map physical address to linear address"; break;
                    case 0x006d: pszService = "GetAppFlatDSAlias"; break;
                    case 0x006e: pszService = "SelectorMapFlat"; break;
                    case 0x006f: pszService = "GetDemandPageInfo"; break;
                    case 0x0070: pszService = "GetSetPageOutCount"; break;
                    case 0x0071: pszService = "hook V86 page"; break;
                    case 0x0072: pszService = "assign device V86 pages"; break;
                    case 0x0073: pszService = "deassign device V86 pages"; break;
                    case 0x0074: pszService = "get array of V86 pages for device"; break;
                    case 0x0075: pszService = "SetNULPageAddr"; break;
                    case 0x0076: pszService = "allocate GDT selector"; break;
                    case 0x0077: pszService = "free GDT selector"; break;
                    case 0x0078: pszService = "allocate LDT selector"; break;
                    case 0x0079: pszService = "free LDT selector"; break;
                    case 0x007a: pszService = "BuildDescriptorDWORDs"; break;
                    case 0x007b: pszService = "get descriptor"; break;
                    case 0x007c: pszService = "set descriptor"; break;
                    case 0x007d: pszService = "toggle HMA"; break;
                    case 0x007e: pszService = "get fault hook addresses"; break;
                    case 0x007f: pszService = "hook V86 fault"; break;
                    case 0x0080: pszService = "hook PM fault"; break;
                    case 0x0081: pszService = "hook VMM fault"; break;
                    case 0x0082: pszService = "begin nested V86 execution"; break;
                    case 0x0083: pszService = "begin nested execution"; break;
                    case 0x0084: pszService = "execute V86-mode interrupt"; break;
                    case 0x0085: pszService = "resume execution"; break;
                    case 0x0086: pszService = "end nested execution"; break;
                    case 0x0087: pszService = "allocate PM application callback area"; break;
                    case 0x0088: pszService = "get current PM application callback area"; break;
                    case 0x0089: pszService = "set V86 execution mode"; break;
                    case 0x008a: pszService = "set PM execution mode"; break;
                    case 0x008b: pszService = "begin using locked PM stack"; break;
                    case 0x008c: pszService = "end using locked PM stack"; break;
                    case 0x008d: pszService = "save client state"; break;
                    case 0x008e: pszService = "restore client state"; break;
                    case 0x008f: pszService = "execute VxD interrupt"; break;
                    case 0x0090: pszService = "hook device service"; break;
                    case 0x0091: pszService = "hook device V86 API"; break;
                    case 0x0092: pszService = "hook device PM API"; break;
                    case 0x0093: pszService = "system control (see also #02657)"; break;
                    case 0x0094: pszService = "simulate I/O"; break;
                    case 0x0095: pszService = "install multiple I/O handlers"; break;
                    case 0x0096: pszService = "install I/O handler"; break;
                    case 0x0097: pszService = "enable global trapping"; break;
                    case 0x0098: pszService = "enable local trapping"; break;
                    case 0x0099: pszService = "disable global trapping"; break;
                    case 0x009a: pszService = "disable local trapping"; break;
                    case 0x009b: pszService = "create list"; break;
                    case 0x009c: pszService = "destroy list"; break;
                    case 0x009d: pszService = "allocate list"; break;
                    case 0x009e: pszService = "attach list"; break;
                    case 0x009f: pszService = "attach list tail"; break;
                    case 0x00a0: pszService = "insert into list"; break;
                    case 0x00a1: pszService = "remove from list"; break;
                    case 0x00a2: pszService = "deallocate list"; break;
                    case 0x00a3: pszService = "get first item in list"; break;
                    case 0x00a4: pszService = "get next item in list"; break;
                    case 0x00a5: pszService = "remove first item in list"; break;
                    case 0x00a6: pszService = "add instance item"; break;
                    case 0x00a7: pszService = "allocate device callback area"; break;
                    case 0x00a8: pszService = "allocate global V86 data area"; break;
                    case 0x00a9: pszService = "allocate temporary V86 data area"; break;
                    case 0x00aa: pszService = "free temporary V86 data area"; break;
                    case 0x00ab: pszService = "get decimal integer from profile"; break;
                    case 0x00ac: pszService = "convert decimal string to integer"; break;
                    case 0x00ad: pszService = "get fixed-point number from profile"; break;
                    case 0x00ae: pszService = "convert fixed-point string"; break;
                    case 0x00af: pszService = "get hex integer from profile"; break;
                    case 0x00b0: pszService = "convert hex string to integer"; break;
                    case 0x00b1: pszService = "get boolean value from profile"; break;
                    case 0x00b2: pszService = "convert boolean string"; break;
                    case 0x00b3: pszService = "get string from profile"; break;
                    case 0x00b4: pszService = "get next string from profile"; break;
                    case 0x00b5: pszService = "get environment string"; break;
                    case 0x00b6: pszService = "get exec path"; break;
                    case 0x00b7: pszService = "get configuration directory"; break;
                    case 0x00b8: pszService = "open file"; break;
                    case 0x00b9: pszService = "get PSP segment"; break;
                    case 0x00ba: pszService = "get DOS vectors"; break;
                    case 0x00bb: pszService = "get machine information"; break;
                    case 0x00bc: pszService = "get/set HMA information"; break;
                    case 0x00bd: pszService = "set system exit code"; break;
                    case 0x00be: pszService = "fatal error handler"; break;
                    case 0x00bf: pszService = "fatal memory error"; break;
                    case 0x00c0: pszService = "update system clock"; break;
                    case 0x00c1: pszService = "test if debugger installed"; break;
                    case 0x00c2: pszService = "output debugger string"; break;
                    case 0x00c3: pszService = "output debugger character"; break;
                    case 0x00c4: pszService = "input debugger character"; break;
                    case 0x00c5: pszService = "debugger convert hex to binary"; break;
                    case 0x00c6: pszService = "debugger convert hex to decimal"; break;
                    case 0x00c7: pszService = "debugger test if valid handle"; break;
                    case 0x00c8: pszService = "validate client pointer"; break;
                    case 0x00c9: pszService = "test reentry"; break;
                    case 0x00ca: pszService = "queue debugger string"; break;
                    case 0x00cb: pszService = "log procedure call"; break;
                    case 0x00cc: pszService = "debugger test current VM"; break;
                    case 0x00cd: pszService = "get PM interrupt type"; break;
                    case 0x00ce: pszService = "set PM interrupt type"; break;
                    case 0x00cf: pszService = "get last updated system time"; break;
                    case 0x00d0: pszService = "get last updated VM execution time"; break;
                    case 0x00d1: pszService = "test if double-byte character-set lead byte"; break;
                    case 0x00d2: pszService = "AddFreePhysPage"; break;
                    case 0x00d3: pszService = "PageResetHandlePAddr"; break;
                    case 0x00d4: pszService = "SetLastV86Page"; break;
                    case 0x00d5: pszService = "GetLastV86Page"; break;
                    case 0x00d6: pszService = "MapFreePhysReg"; break;
                    case 0x00d7: pszService = "UnmapFreePhysReg"; break;
                    case 0x00d8: pszService = "XchgFreePhysReg"; break;
                    case 0x00d9: pszService = "SetFreePhysRegCalBk"; break;
                    case 0x00da: pszService = "get next arena (MCB)"; break;
                    case 0x00db: pszService = "get name of ugly TSR"; break;
                    case 0x00dc: pszService = "get debug options"; break;
                    case 0x00dd: pszService = "set physical HMA alias"; break;
                    case 0x00de: pszService = "GetGlblRng0V86IntBase"; break;
                    case 0x00df: pszService = "add global V86 data area"; break;
                    case 0x00e0: pszService = "get/set detailed VM error"; break;
                    case 0x00e1: pszService = "Is_Debug_Chr"; break;
                    case 0x00e2: pszService = "clear monochrome screen"; break;
                    case 0x00e3: pszService = "output character to mono screen"; break;
                    case 0x00e4: pszService = "output string to mono screen"; break;
                    case 0x00e5: pszService = "set current position on mono screen"; break;
                    case 0x00e6: pszService = "get current position on mono screen"; break;
                    case 0x00e7: pszService = "get character from mono screen"; break;
                    case 0x00e8: pszService = "locate byte in ROM"; break;
                    case 0x00e9: pszService = "hook invalid page fault"; break;
                    case 0x00ea: pszService = "unhook invalid page fault"; break;
                    case 0x00eb: pszService = "set delete on exit file"; break;
                    case 0x00ec: pszService = "close VM"; break;
                    case 0x00ed: pszService = "Enable_Touch_1st_Meg"; break;
                    case 0x00ee: pszService = "Disable_Touch_1st_Meg"; break;
                    case 0x00ef: pszService = "install exception handler"; break;
                    case 0x00f0: pszService = "remove exception handler"; break;
                    case 0x00f1: pszService = "Get_Crit_Status_No_Block"; break;
                    case 0x00f2: pszService = "Schedule_VM_RTI_Event"; break;
                    case 0x00f3: pszService = "Trace_Out_Service"; break;
                    case 0x00f4: pszService = "Debug_Out_Service"; break;
                    case 0x00f5: pszService = "Debug_Flags_Service"; break;
                    case 0x00f6: pszService = "VMM add import module name"; break;
                    case 0x00f7: pszService = "VMM Add DDB"; break;
                    case 0x00f8: pszService = "VMM Remove DDB"; break;
                    case 0x00f9: pszService = "get thread time slice priority"; break;
                    case 0x00fa: pszService = "set thread time slice priority"; break;
                    case 0x00fb: pszService = "schedule thread event"; break;
                    case 0x00fc: pszService = "cancel thread event"; break;
                    case 0x00fd: pszService = "set thread timeout"; break;
                    case 0x00fe: pszService = "set asynchronous timeout"; break;
                    case 0x00ff: pszService = "AllocatreThreadDataSlot"; break;
                    case 0x0100: pszService = "FreeThreadDataSlot"; break;
                    case 0x0101: pszService = "create Mutex"; break;
                    case 0x0102: pszService = "destroy Mutex"; break;
                    case 0x0103: pszService = "get Mutex owner"; break;
                    case 0x0104: pszService = "call when thread switched"; break;
                    case 0x0105: pszService = "create thread"; break;
                    case 0x0106: pszService = "start thread"; break;
                    case 0x0107: pszService = "terminate thread"; break;
                    case 0x0108: pszService = "get current thread handle"; break;
                    case 0x0109: pszService = "test current thread handle"; break;
                    case 0x010a: pszService = "Get_Sys_Thread_Handle"; break;
                    case 0x010b: pszService = "Test_Sys_Thread_Handle"; break;
                    case 0x010c: pszService = "Validate_Thread_Handle"; break;
                    case 0x010d: pszService = "Get_Initial_Thread_Handle"; break;
                    case 0x010e: pszService = "Test_Initial_Thread_Handle"; break;
                    case 0x010f: pszService = "Debug_Test_Valid_Thread_Handle"; break;
                    case 0x0110: pszService = "Debug_Test_Cur_Thread"; break;
                    case 0x0111: pszService = "VMM_GetSystemInitState"; break;
                    case 0x0112: pszService = "Cancel_Call_When_Thread_Switched"; break;
                    case 0x0113: pszService = "Get_Next_Thread_Handle"; break;
                    case 0x0114: pszService = "Adjust_Thread_Exec_Priority"; break;
                    case 0x0115: pszService = "Deallocate_Device_CB_Area"; break;
                    case 0x0116: pszService = "Remove_IO_Handler"; break;
                    case 0x0117: pszService = "Remove_Mult_IO_Handlers"; break;
                    case 0x0118: pszService = "unhook V86 interrupt chain"; break;
                    case 0x0119: pszService = "unhook V86 fault handler"; break;
                    case 0x011a: pszService = "unhook PM fault handler"; break;
                    case 0x011b: pszService = "unhook VMM fault handler"; break;
                    case 0x011c: pszService = "unhook device service"; break;
                    case 0x011d: pszService = "PageReserve"; break;
                    case 0x011e: pszService = "PageCommit"; break;
                    case 0x011f: pszService = "PageDecommit"; break;
                    case 0x0120: pszService = "PagerRegister"; break;
                    case 0x0121: pszService = "PagerQuery"; break;
                    case 0x0122: pszService = "PagerDeregister"; break;
                    case 0x0123: pszService = "ContextCreate"; break;
                    case 0x0124: pszService = "ContextDestroy"; break;
                    case 0x0125: pszService = "PageAttach"; break;
                    case 0x0126: pszService = "PageFlush"; break;
                    case 0x0127: pszService = "SignalID"; break;
                    case 0x0128: pszService = "PageCommitPhys"; break;
                    case 0x0129: pszService = "Register_Win32_Services"; break;
                    case 0x012a: pszService = "Cancel_Call_When_Not_Critical"; break;
                    case 0x012b: pszService = "Cancel_Call_When_Idle"; break;
                    case 0x012c: pszService = "Cancel_Call_When_Task_Switched"; break;
                    case 0x012d: pszService = "Debug_Printf_Service"; break;
                    case 0x012e: pszService = "enter Mutex"; break;
                    case 0x012f: pszService = "leave Mutex"; break;
                    case 0x0130: pszService = "simulate VM I/O"; break;
                    case 0x0131: pszService = "Signal_Semaphore_No_Switch"; break;
                    case 0x0132: pszService = "MMSwitchContext"; break;
                    case 0x0133: pszService = "MMModifyPermissions"; break;
                    case 0x0134: pszService = "MMQuery"; break;
                    case 0x0135: pszService = "EnterMustComplete"; break;
                    case 0x0136: pszService = "LeaveMustComplete"; break;
                    case 0x0137: pszService = "ResumeExecMustComplete"; break;
                    case 0x0138: pszService = "get thread termination status"; break;
                    case 0x0139: pszService = "GetInstanceInfo"; break;
                    case 0x013a: pszService = "ExecIntMustComplete"; break;
                    case 0x013b: pszService = "ExecVxDIntMustComplete"; break;
                    case 0x013c: pszService = "begin V86 serialization"; break;
                    case 0x013d: pszService = "unhook V86 page"; break;
                    case 0x013e: pszService = "VMM_GetVxDLocationList"; break;
                    case 0x013f: pszService = "VMM_GetDDBList get start of VxD chain"; break;
                    case 0x0140: pszService = "unhook NMI event"; break;
                    case 0x0141: pszService = "Get_Instanced_V86_Int_Vector"; break;
                    case 0x0142: pszService = "get or set real DOS PSP"; break;
                    case 0x0143: pszService = "call priority thread event"; break;
                    case 0x0144: pszService = "Get_System_Time_Address"; break;
                    case 0x0145: pszService = "Get_Crit_Status_Thread"; break;
                    case 0x0146: pszService = "Get_DDB"; break;
                    case 0x0147: pszService = "Directed_Sys_Control"; break;
                    case 0x0148: pszService = "RegOpenKey"; break;
                    case 0x0149: pszService = "RegCloseKey"; break;
                    case 0x014a: pszService = "RegCreateKey"; break;
                    case 0x014b: pszService = "RegDeleteKey"; break;
                    case 0x014c: pszService = "RegEnumKey"; break;
                    case 0x014d: pszService = "RegQueryValue"; break;
                    case 0x014e: pszService = "RegSetValue"; break;
                    case 0x014f: pszService = "RegDeleteValue"; break;
                    case 0x0150: pszService = "RegEnumValue"; break;
                    case 0x0151: pszService = "RegQueryValueEx"; break;
                    case 0x0152: pszService = "RegSetValueEx"; break;
                    case 0x0153: pszService = "CallRing3"; break;
                    case 0x0154: pszService = "Exec_PM_Int"; break;
                    case 0x0155: pszService = "RegFlushKey"; break;
                    case 0x0156: pszService = "PageCommitContig"; break;
                    case 0x0157: pszService = "GetCurrentContext"; break;
                    case 0x0158: pszService = "LocalizeSprintf"; break;
                    case 0x0159: pszService = "LocalizeStackSprintf"; break;
                    case 0x015a: pszService = "Call_Restricted_Event"; break;
                    case 0x015b: pszService = "Cancel_Restricted_Event"; break;
                    case 0x015c: pszService = "Register_PEF_Provider"; break;
                    case 0x015d: pszService = "GetPhysPageInfo"; break;
                    case 0x015e: pszService = "RegQueryInfoKey"; break;
                    case 0x015f: pszService = "MemArb_Reserve_Pages"; break;
                    case 0x0160: pszService = "Time_Slice_Sys_VM_Idle"; break;
                    case 0x0161: pszService = "Time_Slice_Sleep"; break;
                    case 0x0162: pszService = "Boost_With_Decay"; break;
                    case 0x0163: pszService = "Set_Inversion_Pri"; break;
                    case 0x0164: pszService = "Reset_Inversion_Pri"; break;
                    case 0x0165: pszService = "Release_Inversion_Pri"; break;
                    case 0x0166: pszService = "Get_Thread_Win32_Pri"; break;
                    case 0x0167: pszService = "Set_Thread_Win32_Pri"; break;
                    case 0x0168: pszService = "Set_Thread_Static_Boost"; break;
                    case 0x0169: pszService = "Set_VM_Static_Boost"; break;
                    case 0x016a: pszService = "Release_Inversion_Pri_ID"; break;
                    case 0x016b: pszService = "Attach_Thread_To_Group"; break;
                    case 0x016c: pszService = "Detach_Thread_From_Group"; break;
                    case 0x016d: pszService = "Set_Group_Static_Boost"; break;
                    case 0x016e: pszService = "GetRegistryPath"; break;
                    case 0x016f: pszService = "GetRegistryKey"; break;
                    case 0x0170: pszService = "CleanupNestedExec"; break;
                    case 0x0171: pszService = "RegRemapPreDefKey"; break;
                    case 0x0172: pszService = "End_V86_Serialization"; break;
                    case 0x0173: pszService = "Assert_Range"; break;
                    case 0x0174: pszService = "Sprintf"; break;
                    case 0x0175: pszService = "PageChangePager"; break;
                    case 0x0176: pszService = "RegCreateDynKey"; break;
                    case 0x0177: pszService = "RegQMulti"; break;
                    case 0x0178: pszService = "Boost_Thread_With_VM"; break;
                    case 0x0179: pszService = "Get_Boot_Flags"; break;
                    case 0x017a: pszService = "Set_Boot_Flags"; break;
                    case 0x017b: pszService = "lstrcpyn"; break;
                    case 0x017c: pszService = "lstrlen"; break;
                    case 0x017d: pszService = "lmemcpy"; break;
                    case 0x017e: pszService = "GetVxDName"; break;
                    case 0x017f: pszService = "Force_Mutexes_Free"; break;
                    case 0x0180: pszService = "Restore_Forced_Mutexes"; break;
                    case 0x0181: pszService = "AddReclaimableItem"; break;
                    case 0x0182: pszService = "SetReclaimableItem"; break;
                    case 0x0183: pszService = "EnumReclaimableItem"; break;
                    case 0x0184: pszService = "Time_Slice_Wake_Sys_VM"; break;
                    case 0x0185: pszService = "VMM_Replace_Global_Environment"; break;
                    case 0x0186: pszService = "Begin_Non_Serial_Nest_V86_Exec"; break;
                    case 0x0187: pszService = "Get_Nest_Exec_Status"; break;
                    case 0x0188: pszService = "Open_Boot_Log"; break;
                    case 0x0189: pszService = "Write_Boot_Log"; break;
                    case 0x018a: pszService = "Close_Boot_Log"; break;
                    case 0x018b: pszService = "EnableDisable_Boot_Log"; break;
                    case 0x018c: pszService = "Call_On_My_Stack"; break;
                    case 0x018d: pszService = "Get_Inst_V86_Int_Vec_Base"; break;
                    case 0x018e: pszService = "lstrcmpi"; break;
                    case 0x018f: pszService = "strupr"; break;
                    case 0x0190: pszService = "Log_Fault_Call_Out"; break;
                    case 0x0191: pszService = "AtEventTime"; break;
                }
                pszVxD = "VMM";
                break;
            case 0x0002: pszVxD = "DEBUG"; break;
            case 0x0003: pszVxD = "VPICD"; break;
            case 0x0004: pszVxD = "VDMAD"; break;
            case 0x0005: pszVxD = "VTD"; break;
            case 0x0006: pszVxD = "V86MMGR"; break;
            case 0x0007: pszVxD = "PageSwap"; break;
            case 0x0009: pszVxD = "REBOOT"; break;
            case 0x000A: pszVxD = "VDD"; break;
            case 0x000B: pszVxD = "VSD"; break;
            case 0x000C: pszVxD = "VMD / VMOUSE"; break;
            case 0x000D: pszVxD = "VKD"; break;
            case 0x000E: pszVxD = "VCD"; break;
            case 0x0010: pszVxD = "BlockDev / IOS"; break;
            case 0x0011: pszVxD = "VMCPD"; break;
            case 0x0012: pszVxD = "EBIOS"; break;
            case 0x0014: pszVxD = "VNETBIOS"; break;
            case 0x0015: pszVxD = "DOSMGR"; break;
            case 0x0017: pszVxD = "SHELL"; break;
            case 0x0018: pszVxD = "VMPoll"; break;
            case 0x001A: pszVxD = "DOSNET"; break;
            case 0x001B: pszVxD = "VFD"; break;
            case 0x001C: pszVxD = "LoadHi"; break;
            case 0x0020: pszVxD = "Int13"; break;
            case 0x0021: pszVxD = "PAGEFILE"; break;
            case 0x0026: pszVxD = "VPOWERD"; break;
            case 0x0027: pszVxD = "VXDLDR"; break;
            case 0x0028: pszVxD = "NDIS"; break;
            case 0x002A: pszVxD = "VWIN32"; break;
            case 0x002B: pszVxD = "VCOMM"; break;
            case 0x002C: pszVxD = "SPOOLER"; break;
            case 0x0032: pszVxD = "VSERVER"; break;
            case 0x0033: pszVxD = "CONFIGMG"; break;
            case 0x0034: pszVxD = "DWCFGMG.SYS"; break;
            case 0x0036: pszVxD = "VFBACKUP"; break;
            case 0x0037: pszVxD = "VMINI / ENABLE"; break;
            case 0x0038: pszVxD = "VCOND"; break;
            case 0x003D: pszVxD = "BIOS"; break;
            case 0x003E: pszVxD = "WSOCK"; break;
            case 0x0040: pszVxD = "IFSMgr"; break;
            case 0x0041: pszVxD = "VCDFSD"; break;
            case 0x0048: pszVxD = "PERF"; break;
            case 0x004A: pszVxD = "MTRR"; break;
            case 0x004B: pszVxD = "NTKERN"; break;
            case 0x011F: pszVxD = "VFLATD"; break;
            case 0x0449: pszVxD = "vjoyd"; break;
            case 0x044A: pszVxD = "mmdevldr"; break;
            case 0x0480: pszVxD = "VNetSup"; break;
            case 0x0481: pszVxD = "VREDIR"; break;
            case 0x0483: pszVxD = "VSHARE"; break;
            case 0x0487: pszVxD = "NWLINK"; break;
            case 0x0488: pszVxD = "VTDI"; break;
            case 0x0489: pszVxD = "VIP"; break;
            case 0x048A: pszVxD = "MSTCP"; break;
            case 0x048B: pszVxD = "VCACHE"; break;
            case 0x048E: pszVxD = "NWREDIR"; break;
            case 0x0491: pszVxD = "FILESEC"; break;
            case 0x0492: pszVxD = "NWSERVER"; break;
            case 0x0493: pszVxD = "MSSP / NWSP"; break;
            case 0x0494: pszVxD = "NSCL"; break;
            case 0x0495: pszVxD = "AFVXD"; break;
            case 0x0496: pszVxD = "NDIS2SUP"; break;
            case 0x0498: pszVxD = "Splitter"; break;
            case 0x0499: pszVxD = "PPPMAC"; break;
            case 0x049A: pszVxD = "VDHCP"; break;
            case 0x049B: pszVxD = "VNBT"; break;
            case 0x049D: pszVxD = "LOGGER"; break;
            case 0x097C: pszVxD = "PCCARD"; break;
            case 0x3098: pszVxD = "VstlthD"; break; /* QEMM */
            case 0x30F6: pszVxD = "WSVV"; break;
            case 0x33FC: pszVxD = "ASPIENUM"; break;
            case 0x357E: pszVxD = "DSOUND"; break;
            case 0x39E6: pszVxD = "A3D"; break;
            case 0x3BFD: pszVxD = "CWCPROXY"; break;
            case 0x3C78: pszVxD = "VGARTD"; break;

            default:
            {
                uint8_t abOpcodes[16] = {0};
                cbInstr = RT_MIN(cbInstr, 16);
                rc = PGMPhysSimpleReadGCPtr(pVCpu, abOpcodes, GCPtrParams - cbInstr, cbInstr);
                Log2(("VxD syscall: VxD=%#x Service=%#x - Unknown at %04x:%08RX64: %.*Rhxs - %.*Rhxs (%Rrc)\n",
                      idVxD, idService, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
                      cbInstr, abOpcodes, sizeof(auParams), auParams, rc));
                return;
            }
        }
        if (pszService)
            Log2(("VxD syscall: VxD=%#04x Service=%#04x - %s: %s\n", idVxD, idService, pszVxD, pszService));
        else
            Log2(("VxD syscall: VxD=%#04x Service=%#04x - %s\n", idVxD, idService, pszVxD));
    }
    else
        Log2(("VxD syscall: unable to read parameters at %RGv: %Rrc\n", GCPtrParams, rc));
}


void iemLogSyscallProtModeInt(PVMCPUCC pVCpu, uint8_t u8Vector, uint8_t cbInstr)
{
    /* DOS & BIOS (V86 mode) */
    if (LogIsEnabled())
    {
    }

    /* Windows 3.x */
    if (LogIs2Enabled())
        switch (u8Vector)
        {
            case 0x20: /* VxD call. */
                iemLogSyscallWinVxDCall(pVCpu, cbInstr);
                break;
        }

    /* Linux */
    if (LogIs3Enabled())
    {

    }
}

#endif /* LOG_ENABLED */
