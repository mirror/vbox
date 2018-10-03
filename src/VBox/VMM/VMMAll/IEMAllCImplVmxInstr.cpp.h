/* $Id$ */
/** @file
 * IEM - VT-x instruction implementation.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/** @todo NSTVMX: The following VM-exit intercepts are pending:
 *  VMX_EXIT_XCPT_OR_NMI
 *  VMX_EXIT_EXT_INT
 *  VMX_EXIT_TRIPLE_FAULT
 *  VMX_EXIT_INIT_SIGNAL
 *  VMX_EXIT_SIPI
 *  VMX_EXIT_IO_SMI
 *  VMX_EXIT_SMI
 *  VMX_EXIT_INT_WINDOW
 *  VMX_EXIT_NMI_WINDOW
 *  VMX_EXIT_TASK_SWITCH
 *  VMX_EXIT_GETSEC
 *  VMX_EXIT_INVD
 *  VMX_EXIT_RSM
 *  VMX_EXIT_MOV_CRX
 *  VMX_EXIT_MOV_DRX
 *  VMX_EXIT_IO_INSTR
 *  VMX_EXIT_MWAIT
 *  VMX_EXIT_MTF
 *  VMX_EXIT_MONITOR (APIC access VM-exit caused by MONITOR pending)
 *  VMX_EXIT_PAUSE
 *  VMX_EXIT_ERR_MACHINE_CHECK
 *  VMX_EXIT_TPR_BELOW_THRESHOLD
 *  VMX_EXIT_APIC_ACCESS
 *  VMX_EXIT_VIRTUALIZED_EOI
 *  VMX_EXIT_EPT_VIOLATION
 *  VMX_EXIT_EPT_MISCONFIG
 *  VMX_EXIT_INVEPT
 *  VMX_EXIT_PREEMPT_TIMER
 *  VMX_EXIT_INVVPID
 *  VMX_EXIT_WBINVD
 *  VMX_EXIT_XSETBV
 *  VMX_EXIT_APIC_WRITE
 *  VMX_EXIT_RDRAND
 *  VMX_EXIT_VMFUNC
 *  VMX_EXIT_ENCLS
 *  VMX_EXIT_RDSEED
 *  VMX_EXIT_PML_FULL
 *  VMX_EXIT_XSAVES
 *  VMX_EXIT_XRSTORS
 */

/**
 * Map of VMCS field encodings to their virtual-VMCS structure offsets.
 *
 * The first array dimension is VMCS field encoding of Width OR'ed with Type and the
 * second dimension is the Index, see VMXVMCSFIELDENC.
 */
uint16_t const g_aoffVmcsMap[16][VMX_V_VMCS_MAX_INDEX + 1] =
{
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u16Vpid),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u16PostIntNotifyVector),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u16EptpIndex),
        /*  3-10 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 11-18 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 19-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*   0-7 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, GuestEs),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, GuestCs),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, GuestSs),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, GuestDs),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, GuestFs),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, GuestGs),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, GuestLdtr),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, GuestTr),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u16GuestIntStatus),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u16PmlIndex),
        /* 10-17 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, HostEs),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, HostCs),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, HostSs),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, HostDs),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, HostFs),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, HostGs),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, HostTr),
        /*  7-14 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 15-22 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 23-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64AddrIoBitmapA),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64AddrIoBitmapB),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64AddrMsrBitmap),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64AddrExitMsrStore),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64AddrExitMsrLoad),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64AddrEntryMsrLoad),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64ExecVmcsPtr),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64AddrPml),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64TscOffset),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64AddrVirtApic),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64AddrApicAccess),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64AddrPostedIntDesc),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u64VmFuncCtls),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u64EptpPtr),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap0),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap1),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap2),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap3),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u64AddrEptpList),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmreadBitmap),
        /*    20 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmwriteBitmap),
        /*    21 */ RT_OFFSETOF(VMXVVMCS, u64AddrXcptVeInfo),
        /*    22 */ RT_OFFSETOF(VMXVVMCS, u64AddrXssBitmap),
        /*    23 */ RT_OFFSETOF(VMXVVMCS, u64AddrEnclsBitmap),
        /*    24 */ UINT16_MAX,
        /*    25 */ RT_OFFSETOF(VMXVVMCS, u64TscMultiplier)
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64RoGuestPhysAddr),
        /*   1-8 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  9-16 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 17-24 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*    25 */ UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64VmcsLinkPtr),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64GuestDebugCtlMsr),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64GuestPatMsr),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64GuestEferMsr),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64GuestPerfGlobalCtlMsr),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte0),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte1),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte2),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte3),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64GuestBndcfgsMsr),
        /* 10-17 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64HostPatMsr),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64HostEferMsr),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64HostPerfGlobalCtlMsr),
        /*  3-10 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 11-18 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 19-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32PinCtls),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32ProcCtls),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32XcptBitmap),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32XcptPFMask),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32XcptPFMatch),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32Cr3TargetCount),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32ExitCtls),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32ExitMsrStoreCount),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u32ExitMsrLoadCount),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u32EntryCtls),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u32EntryMsrLoadCount),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u32EntryIntInfo),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u32EntryXcptErrCode),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u32EntryInstrLen),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u32TprThreshold),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u32ProcCtls2),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u32PleGap),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u32PleWindow),
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32RoVmInstrError),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32RoExitReason),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32RoExitIntInfo),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32RoExitErrCode),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringInfo),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringErrCode),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32RoExitInstrLen),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32RoExitInstrInfo),
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsLimit),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32GuestCsLimit),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32GuestSsLimit),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32GuestDsLimit),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsLimit),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32GuestFsLimit),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32GuestGsLimit),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32GuestLdtrLimit),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u32GuestTrLimit),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u32GuestGdtrLimit),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u32GuestIdtrLimit),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsAttr),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u32GuestCsAttr),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u32GuestSsAttr),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u32GuestDsAttr),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u32GuestFsAttr),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u32GuestGsAttr),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u32GuestLdtrAttr),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u32GuestTrAttr),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u32GuestIntrState),
        /*    20 */ RT_OFFSETOF(VMXVVMCS, u32GuestActivityState),
        /*    21 */ RT_OFFSETOF(VMXVVMCS, u32GuestSmBase),
        /*    22 */ RT_OFFSETOF(VMXVVMCS, u32GuestSysenterCS),
        /*    23 */ RT_OFFSETOF(VMXVVMCS, u32PreemptTimer),
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32HostSysenterCs),
        /*   1-8 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  9-16 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 17-24 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*    25 */ UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64Cr0Mask),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64Cr4Mask),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64Cr0ReadShadow),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64Cr4ReadShadow),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target0),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target1),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target2),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target3),
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64RoExitQual),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64RoIoRcx),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64RoIoRsi),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64RoIoRdi),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64RoIoRip),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64RoGuestLinearAddr),
        /*  6-13 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 14-21 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 22-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr0),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr3),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr4),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64GuestEsBase),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64GuestCsBase),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestSsBase),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64GuestDsBase),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64GuestFsBase),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64GuestGsBase),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64GuestLdtrBase),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64GuestTrBase),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64GuestGdtrBase),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u64GuestIdtrBase),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u64GuestDr7),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u64GuestRsp),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u64GuestRip),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u64GuestRFlags),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u64GuestPendingDbgXcpt),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u64GuestSysenterEsp),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u64GuestSysenterEip),
        /* 20-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64HostCr0),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64HostCr3),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64HostCr4),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64HostFsBase),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64HostGsBase),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64HostTrBase),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64HostGdtrBase),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64HostIdtrBase),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64HostSysenterEsp),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64HostSysenterEip),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64HostRsp),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64HostRip),
        /* 12-19 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 20-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    }
};


/**
 * Gets the ModR/M, SIB and displacement byte(s) from decoded opcodes given their
 * relative offsets.
 */
# ifdef IEM_WITH_CODE_TLB
#  define IEM_MODRM_GET_U8(a_pVCpu, a_bModRm, a_offModRm)         do { } while (0)
#  define IEM_SIB_GET_U8(a_pVCpu, a_bSib, a_offSib)               do { } while (0)
#  define IEM_DISP_GET_U16(a_pVCpu, a_u16Disp, a_offDisp)         do { } while (0)
#  define IEM_DISP_GET_S8_SX_U16(a_pVCpu, a_u16Disp, a_offDisp)   do { } while (0)
#  define IEM_DISP_GET_U32(a_pVCpu, a_u32Disp, a_offDisp)         do { } while (0)
#  define IEM_DISP_GET_S8_SX_U32(a_pVCpu, a_u32Disp, a_offDisp)   do { } while (0)
#  define IEM_DISP_GET_S32_SX_U64(a_pVCpu, a_u64Disp, a_offDisp)  do { } while (0)
#  define IEM_DISP_GET_S8_SX_U64(a_pVCpu, a_u64Disp, a_offDisp)   do { } while (0)
#  error "Implement me: Getting ModR/M, SIB, displacement needs to work even when instruction crosses a page boundary."
# else  /* !IEM_WITH_CODE_TLB */
#  define IEM_MODRM_GET_U8(a_pVCpu, a_bModRm, a_offModRm) \
    do \
    { \
        Assert((a_offModRm) < (a_pVCpu)->iem.s.cbOpcode); \
        (a_bModRm) = (a_pVCpu)->iem.s.abOpcode[(a_offModRm)]; \
    } while (0)

#  define IEM_SIB_GET_U8(a_pVCpu, a_bSib, a_offSib)      IEM_MODRM_GET_U8(a_pVCpu, a_bSib, a_offSib)

#  define IEM_DISP_GET_U16(a_pVCpu, a_u16Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmpLo = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmpHi = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        (a_u16Disp) = RT_MAKE_U16(bTmpLo, bTmpHi); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U16(a_pVCpu, a_u16Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u16Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_U32(a_pVCpu, a_u32Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 3 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmp0 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmp1 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        uint8_t const bTmp2 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 2]; \
        uint8_t const bTmp3 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 3]; \
        (a_u32Disp) = RT_MAKE_U32_FROM_U8(bTmp0, bTmp1, bTmp2, bTmp3); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U32(a_pVCpu, a_u32Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u32Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U64(a_pVCpu, a_u64Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u64Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_S32_SX_U64(a_pVCpu, a_u64Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 3 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmp0 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmp1 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        uint8_t const bTmp2 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 2]; \
        uint8_t const bTmp3 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 3]; \
        (a_u64Disp) = (int32_t)RT_MAKE_U32_FROM_U8(bTmp0, bTmp1, bTmp2, bTmp3); \
    } while (0)
# endif /* !IEM_WITH_CODE_TLB */

/** Gets the guest-physical address of the shadows VMCS for the given VCPU. */
#define IEM_VMX_GET_SHADOW_VMCS(a_pVCpu)            ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysShadowVmcs)

/** Whether a shadow VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_SHADOW_VMCS(a_pVCpu)            RT_BOOL(IEM_VMX_GET_SHADOW_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the VMXON region pointer. */
#define IEM_VMX_GET_VMXON_PTR(a_pVCpu)              ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)

/** Gets the guest-physical address of the current VMCS for the given VCPU. */
#define IEM_VMX_GET_CURRENT_VMCS(a_pVCpu)           ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs)

/** Whether a current VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_CURRENT_VMCS(a_pVCpu)           RT_BOOL(IEM_VMX_GET_CURRENT_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Assigns the guest-physical address of the current VMCS for the given VCPU. */
#define IEM_VMX_SET_CURRENT_VMCS(a_pVCpu, a_GCPhysVmcs) \
    do \
    { \
        Assert((a_GCPhysVmcs) != NIL_RTGCPHYS); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = (a_GCPhysVmcs); \
    } while (0)

/** Clears any current VMCS for the given VCPU. */
#define IEM_VMX_CLEAR_CURRENT_VMCS(a_pVCpu) \
    do \
    { \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = NIL_RTGCPHYS; \
    } while (0)

/** Check for VMX instructions requiring to be in VMX operation.
 * @note Any changes here, check if IEMOP_HLP_IN_VMX_OPERATION needs udpating. */
#define IEM_VMX_IN_VMX_OPERATION(a_pVCpu, a_szInstr, a_InsDiagPrefix) \
    do \
    { \
        if (IEM_VMX_IS_ROOT_MODE(a_pVCpu)) \
        { /* likely */ } \
        else \
        { \
            Log((a_szInstr ": Not in VMX operation (root mode) -> #UD\n")); \
            (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = a_InsDiagPrefix##_VmxRoot; \
            return iemRaiseUndefinedOpcode(a_pVCpu); \
        } \
    } while (0)

/** Marks a VM-entry failure with a diagnostic reason, logs and returns. */
#define IEM_VMX_VMENTRY_FAILED_RET(a_pVCpu, a_pszInstr, a_pszFailure, a_VmxDiag) \
    do \
    { \
        Log(("%s: VM-entry failed! enmDiag=%u (%s) -> %s\n", (a_pszInstr), (a_VmxDiag), \
            HMVmxGetDiagDesc(a_VmxDiag), (a_pszFailure))); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = (a_VmxDiag); \
        return VERR_VMX_VMENTRY_FAILED; \
    } while (0)

/** Marks a VM-exit failure with a diagnostic reason, logs and returns. */
#define IEM_VMX_VMEXIT_FAILED_RET(a_pVCpu, a_uExitReason, a_pszFailure, a_VmxDiag) \
    do \
    { \
        Log(("VM-exit failed! uExitReason=%u enmDiag=%u (%s) -> %s\n", (a_uExitReason), (a_VmxDiag), \
            HMVmxGetDiagDesc(a_VmxDiag), (a_pszFailure))); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = (a_VmxDiag); \
        return VERR_VMX_VMEXIT_FAILED; \
    } while (0)



/**
 * Returns whether the given VMCS field is valid and supported by our emulation.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u64FieldEnc     The VMCS field encoding.
 *
 * @remarks This takes into account the CPU features exposed to the guest.
 */
IEM_STATIC bool iemVmxIsVmcsFieldValid(PVMCPU pVCpu, uint64_t u64FieldEnc)
{
    uint32_t const uFieldEncHi = RT_HI_U32(u64FieldEnc);
    uint32_t const uFieldEncLo = RT_LO_U32(u64FieldEnc);
    if (!uFieldEncHi)
    { /* likely */ }
    else
        return false;

    PCCPUMFEATURES pFeat = IEM_GET_GUEST_CPU_FEATURES(pVCpu);
    switch (uFieldEncLo)
    {
        /*
         * 16-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS16_VPID:                             return pFeat->fVmxVpid;
        case VMX_VMCS16_POSTED_INT_NOTIFY_VECTOR:         return pFeat->fVmxPostedInt;
        case VMX_VMCS16_EPTP_INDEX:                       return pFeat->fVmxEptXcptVe;

        /* Guest-state fields. */
        case VMX_VMCS16_GUEST_ES_SEL:
        case VMX_VMCS16_GUEST_CS_SEL:
        case VMX_VMCS16_GUEST_SS_SEL:
        case VMX_VMCS16_GUEST_DS_SEL:
        case VMX_VMCS16_GUEST_FS_SEL:
        case VMX_VMCS16_GUEST_GS_SEL:
        case VMX_VMCS16_GUEST_LDTR_SEL:
        case VMX_VMCS16_GUEST_TR_SEL:
        case VMX_VMCS16_GUEST_INTR_STATUS:                return pFeat->fVmxVirtIntDelivery;
        case VMX_VMCS16_GUEST_PML_INDEX:                  return pFeat->fVmxPml;

        /* Host-state fields. */
        case VMX_VMCS16_HOST_ES_SEL:
        case VMX_VMCS16_HOST_CS_SEL:
        case VMX_VMCS16_HOST_SS_SEL:
        case VMX_VMCS16_HOST_DS_SEL:
        case VMX_VMCS16_HOST_FS_SEL:
        case VMX_VMCS16_HOST_GS_SEL:
        case VMX_VMCS16_HOST_TR_SEL:                      return true;

        /*
         * 64-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS64_CTRL_IO_BITMAP_A_FULL:
        case VMX_VMCS64_CTRL_IO_BITMAP_A_HIGH:
        case VMX_VMCS64_CTRL_IO_BITMAP_B_FULL:
        case VMX_VMCS64_CTRL_IO_BITMAP_B_HIGH:            return pFeat->fVmxUseIoBitmaps;
        case VMX_VMCS64_CTRL_MSR_BITMAP_FULL:
        case VMX_VMCS64_CTRL_MSR_BITMAP_HIGH:             return pFeat->fVmxUseMsrBitmaps;
        case VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_STORE_HIGH:
        case VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_LOAD_HIGH:
        case VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_HIGH:
        case VMX_VMCS64_CTRL_EXEC_VMCS_PTR_FULL:
        case VMX_VMCS64_CTRL_EXEC_VMCS_PTR_HIGH:          return true;
        case VMX_VMCS64_CTRL_EXEC_PML_ADDR_FULL:
        case VMX_VMCS64_CTRL_EXEC_PML_ADDR_HIGH:          return pFeat->fVmxPml;
        case VMX_VMCS64_CTRL_TSC_OFFSET_FULL:
        case VMX_VMCS64_CTRL_TSC_OFFSET_HIGH:             return true;
        case VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_FULL:
        case VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_HIGH:     return pFeat->fVmxUseTprShadow;
        case VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL:
        case VMX_VMCS64_CTRL_APIC_ACCESSADDR_HIGH:        return pFeat->fVmxVirtApicAccess;
        case VMX_VMCS64_CTRL_POSTED_INTR_DESC_FULL:
        case VMX_VMCS64_CTRL_POSTED_INTR_DESC_HIGH:       return pFeat->fVmxPostedInt;
        case VMX_VMCS64_CTRL_VMFUNC_CTRLS_FULL:
        case VMX_VMCS64_CTRL_VMFUNC_CTRLS_HIGH:           return pFeat->fVmxVmFunc;
        case VMX_VMCS64_CTRL_EPTP_FULL:
        case VMX_VMCS64_CTRL_EPTP_HIGH:                   return pFeat->fVmxEpt;
        case VMX_VMCS64_CTRL_EOI_BITMAP_0_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_0_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_1_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_1_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_2_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_2_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_3_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_3_HIGH:           return pFeat->fVmxVirtIntDelivery;
        case VMX_VMCS64_CTRL_EPTP_LIST_FULL:
        case VMX_VMCS64_CTRL_EPTP_LIST_HIGH:
        {
            uint64_t const uVmFuncMsr = CPUMGetGuestIa32VmxVmFunc(pVCpu);
            return RT_BOOL(RT_BF_GET(uVmFuncMsr, VMX_BF_VMFUNC_EPTP_SWITCHING));
        }
        case VMX_VMCS64_CTRL_VMREAD_BITMAP_FULL:
        case VMX_VMCS64_CTRL_VMREAD_BITMAP_HIGH:
        case VMX_VMCS64_CTRL_VMWRITE_BITMAP_FULL:
        case VMX_VMCS64_CTRL_VMWRITE_BITMAP_HIGH:         return pFeat->fVmxVmcsShadowing;
        case VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_FULL:
        case VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_HIGH:     return pFeat->fVmxEptXcptVe;
        case VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_FULL:
        case VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_HIGH:     return pFeat->fVmxXsavesXrstors;
        case VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_FULL:
        case VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_HIGH:   return false;
        case VMX_VMCS64_CTRL_TSC_MULTIPLIER_FULL:
        case VMX_VMCS64_CTRL_TSC_MULTIPLIER_HIGH:         return pFeat->fVmxUseTscScaling;

        /* Read-only data fields. */
        case VMX_VMCS64_RO_GUEST_PHYS_ADDR_FULL:
        case VMX_VMCS64_RO_GUEST_PHYS_ADDR_HIGH:          return pFeat->fVmxEpt;

        /* Guest-state fields. */
        case VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL:
        case VMX_VMCS64_GUEST_VMCS_LINK_PTR_HIGH:
        case VMX_VMCS64_GUEST_DEBUGCTL_FULL:
        case VMX_VMCS64_GUEST_DEBUGCTL_HIGH:              return true;
        case VMX_VMCS64_GUEST_PAT_FULL:
        case VMX_VMCS64_GUEST_PAT_HIGH:                   return pFeat->fVmxEntryLoadPatMsr || pFeat->fVmxExitSavePatMsr;
        case VMX_VMCS64_GUEST_EFER_FULL:
        case VMX_VMCS64_GUEST_EFER_HIGH:                  return pFeat->fVmxEntryLoadEferMsr || pFeat->fVmxExitSaveEferMsr;
        case VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL:
        case VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_HIGH:      return false;
        case VMX_VMCS64_GUEST_PDPTE0_FULL:
        case VMX_VMCS64_GUEST_PDPTE0_HIGH:
        case VMX_VMCS64_GUEST_PDPTE1_FULL:
        case VMX_VMCS64_GUEST_PDPTE1_HIGH:
        case VMX_VMCS64_GUEST_PDPTE2_FULL:
        case VMX_VMCS64_GUEST_PDPTE2_HIGH:
        case VMX_VMCS64_GUEST_PDPTE3_FULL:
        case VMX_VMCS64_GUEST_PDPTE3_HIGH:                return pFeat->fVmxEpt;
        case VMX_VMCS64_GUEST_BNDCFGS_FULL:
        case VMX_VMCS64_GUEST_BNDCFGS_HIGH:               return false;

        /* Host-state fields. */
        case VMX_VMCS64_HOST_PAT_FULL:
        case VMX_VMCS64_HOST_PAT_HIGH:                    return pFeat->fVmxExitLoadPatMsr;
        case VMX_VMCS64_HOST_EFER_FULL:
        case VMX_VMCS64_HOST_EFER_HIGH:                   return pFeat->fVmxExitLoadEferMsr;
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL:
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_HIGH:       return false;

        /*
         * 32-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS32_CTRL_PIN_EXEC:
        case VMX_VMCS32_CTRL_PROC_EXEC:
        case VMX_VMCS32_CTRL_EXCEPTION_BITMAP:
        case VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK:
        case VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH:
        case VMX_VMCS32_CTRL_CR3_TARGET_COUNT:
        case VMX_VMCS32_CTRL_EXIT:
        case VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT:
        case VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT:
        case VMX_VMCS32_CTRL_ENTRY:
        case VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT:
        case VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO:
        case VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE:
        case VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH:          return true;
        case VMX_VMCS32_CTRL_TPR_THRESHOLD:               return pFeat->fVmxUseTprShadow;
        case VMX_VMCS32_CTRL_PROC_EXEC2:                  return pFeat->fVmxSecondaryExecCtls;
        case VMX_VMCS32_CTRL_PLE_GAP:
        case VMX_VMCS32_CTRL_PLE_WINDOW:                  return pFeat->fVmxPauseLoopExit;

        /* Read-only data fields. */
        case VMX_VMCS32_RO_VM_INSTR_ERROR:
        case VMX_VMCS32_RO_EXIT_REASON:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_ERROR_CODE:
        case VMX_VMCS32_RO_IDT_VECTORING_INFO:
        case VMX_VMCS32_RO_IDT_VECTORING_ERROR_CODE:
        case VMX_VMCS32_RO_EXIT_INSTR_LENGTH:
        case VMX_VMCS32_RO_EXIT_INSTR_INFO:               return true;

        /* Guest-state fields. */
        case VMX_VMCS32_GUEST_ES_LIMIT:
        case VMX_VMCS32_GUEST_CS_LIMIT:
        case VMX_VMCS32_GUEST_SS_LIMIT:
        case VMX_VMCS32_GUEST_DS_LIMIT:
        case VMX_VMCS32_GUEST_FS_LIMIT:
        case VMX_VMCS32_GUEST_GS_LIMIT:
        case VMX_VMCS32_GUEST_LDTR_LIMIT:
        case VMX_VMCS32_GUEST_TR_LIMIT:
        case VMX_VMCS32_GUEST_GDTR_LIMIT:
        case VMX_VMCS32_GUEST_IDTR_LIMIT:
        case VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_INT_STATE:
        case VMX_VMCS32_GUEST_ACTIVITY_STATE:
        case VMX_VMCS32_GUEST_SMBASE:
        case VMX_VMCS32_GUEST_SYSENTER_CS:                return true;
        case VMX_VMCS32_PREEMPT_TIMER_VALUE:              return pFeat->fVmxPreemptTimer;

        /* Host-state fields. */
        case VMX_VMCS32_HOST_SYSENTER_CS:                 return true;

        /*
         * Natural-width fields.
         */
        /* Control fields. */
        case VMX_VMCS_CTRL_CR0_MASK:
        case VMX_VMCS_CTRL_CR4_MASK:
        case VMX_VMCS_CTRL_CR0_READ_SHADOW:
        case VMX_VMCS_CTRL_CR4_READ_SHADOW:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL0:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL1:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL2:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL3:               return true;

        /* Read-only data fields. */
        case VMX_VMCS_RO_EXIT_QUALIFICATION:
        case VMX_VMCS_RO_IO_RCX:
        case VMX_VMCS_RO_IO_RSX:
        case VMX_VMCS_RO_IO_RDI:
        case VMX_VMCS_RO_IO_RIP:
        case VMX_VMCS_RO_EXIT_GUEST_LINEAR_ADDR:          return true;

        /* Guest-state fields. */
        case VMX_VMCS_GUEST_CR0:
        case VMX_VMCS_GUEST_CR3:
        case VMX_VMCS_GUEST_CR4:
        case VMX_VMCS_GUEST_ES_BASE:
        case VMX_VMCS_GUEST_CS_BASE:
        case VMX_VMCS_GUEST_SS_BASE:
        case VMX_VMCS_GUEST_DS_BASE:
        case VMX_VMCS_GUEST_FS_BASE:
        case VMX_VMCS_GUEST_GS_BASE:
        case VMX_VMCS_GUEST_LDTR_BASE:
        case VMX_VMCS_GUEST_TR_BASE:
        case VMX_VMCS_GUEST_GDTR_BASE:
        case VMX_VMCS_GUEST_IDTR_BASE:
        case VMX_VMCS_GUEST_DR7:
        case VMX_VMCS_GUEST_RSP:
        case VMX_VMCS_GUEST_RIP:
        case VMX_VMCS_GUEST_RFLAGS:
        case VMX_VMCS_GUEST_PENDING_DEBUG_XCPTS:
        case VMX_VMCS_GUEST_SYSENTER_ESP:
        case VMX_VMCS_GUEST_SYSENTER_EIP:                 return true;

        /* Host-state fields. */
        case VMX_VMCS_HOST_CR0:
        case VMX_VMCS_HOST_CR3:
        case VMX_VMCS_HOST_CR4:
        case VMX_VMCS_HOST_FS_BASE:
        case VMX_VMCS_HOST_GS_BASE:
        case VMX_VMCS_HOST_TR_BASE:
        case VMX_VMCS_HOST_GDTR_BASE:
        case VMX_VMCS_HOST_IDTR_BASE:
        case VMX_VMCS_HOST_SYSENTER_ESP:
        case VMX_VMCS_HOST_SYSENTER_EIP:
        case VMX_VMCS_HOST_RSP:
        case VMX_VMCS_HOST_RIP:                           return true;
    }

    return false;
}


/**
 * Gets a host selector from the VMCS.
 *
 * @param   pVmcs       Pointer to the virtual VMCS.
 * @param   iSelReg     The index of the segment register (X86_SREG_XXX).
 */
DECLINLINE(RTSEL) iemVmxVmcsGetHostSelReg(PCVMXVVMCS pVmcs, uint8_t iSegReg)
{
    Assert(iSegReg < X86_SREG_COUNT);
    RTSEL HostSel;
    uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_16BIT;
    uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
    uint8_t  const  uWidthType = (uWidth << 2) | uType;
    uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS16_GUEST_ES_SEL, VMX_BF_VMCS_ENC_INDEX);
    Assert(uIndex <= VMX_V_VMCS_MAX_INDEX);
    uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
    uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
    uint8_t  const *pbField    = pbVmcs + offField;
    HostSel = *(uint16_t *)pbField;
    return HostSel;
}


/**
 * Sets a guest segment register in the VMCS.
 *
 * @param   pVmcs       Pointer to the virtual VMCS.
 * @param   iSegReg     The index of the segment register (X86_SREG_XXX).
 * @param   pSelReg     Pointer to the segment register.
 */
IEM_STATIC void iemVmxVmcsSetGuestSegReg(PCVMXVVMCS pVmcs, uint8_t iSegReg, PCCPUMSELREG pSelReg)
{
    Assert(pSelReg);
    Assert(iSegReg < X86_SREG_COUNT);

    /* Selector. */
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_16BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS16_GUEST_ES_SEL, VMX_BF_VMCS_ENC_INDEX);
        Assert(uIndex <= VMX_V_VMCS_MAX_INDEX);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t        *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t        *pbField    = pbVmcs + offField;
        *(uint16_t *)pbField = pSelReg->Sel;
    }

    /* Limit. */
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_LIMIT, VMX_BF_VMCS_ENC_INDEX);
        Assert(uIndex <= VMX_V_VMCS_MAX_INDEX);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t        *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t        *pbField    = pbVmcs + offField;
        *(uint32_t *)pbField = pSelReg->u32Limit;
    }

    /* Base. */
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_NATURAL;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS_GUEST_ES_BASE, VMX_BF_VMCS_ENC_INDEX);
        Assert(uIndex <= VMX_V_VMCS_MAX_INDEX);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        *(uint64_t *)pbField = pSelReg->u64Base;
    }

    /* Attributes. */
    {
        uint32_t const fValidAttrMask = X86DESCATTR_TYPE | X86DESCATTR_DT  | X86DESCATTR_DPL | X86DESCATTR_P
                                      | X86DESCATTR_AVL  | X86DESCATTR_L   | X86DESCATTR_D   | X86DESCATTR_G
                                      | X86DESCATTR_UNUSABLE;
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS, VMX_BF_VMCS_ENC_INDEX);
        Assert(uIndex <= VMX_V_VMCS_MAX_INDEX);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t        *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t        *pbField    = pbVmcs + offField;
        *(uint32_t *)pbField       = pSelReg->Attr.u & fValidAttrMask;
    }
}


/**
 * Gets a guest segment register from the VMCS.
 *
 * @returns VBox status code.
 * @param   pVmcs       Pointer to the virtual VMCS.
 * @param   iSegReg     The index of the segment register (X86_SREG_XXX).
 * @param   pSelReg     Where to store the segment register (only updated when
 *                      VINF_SUCCESS is returned).
 *
 * @remarks Warning! This does not validate the contents of the retreived segment
 *          register.
 */
IEM_STATIC int iemVmxVmcsGetGuestSegReg(PCVMXVVMCS pVmcs, uint8_t iSegReg, PCPUMSELREG pSelReg)
{
    Assert(pSelReg);
    Assert(iSegReg < X86_SREG_COUNT);

    /* Selector. */
    uint16_t u16Sel;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_16BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS16_GUEST_ES_SEL, VMX_BF_VMCS_ENC_INDEX);
        AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u16Sel = *(uint16_t *)pbField;
    }

    /* Limit. */
    uint32_t u32Limit;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_LIMIT, VMX_BF_VMCS_ENC_INDEX);
        AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u32Limit = *(uint32_t *)pbField;
    }

    /* Base. */
    uint64_t u64Base;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_NATURAL;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS_GUEST_ES_BASE, VMX_BF_VMCS_ENC_INDEX);
        AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u64Base = *(uint64_t *)pbField;
        /** @todo NSTVMX: Should we zero out high bits here for 32-bit virtual CPUs? */
    }

    /* Attributes. */
    uint32_t u32Attr;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS, VMX_BF_VMCS_ENC_INDEX);
        AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u32Attr = *(uint32_t *)pbField;
    }

    pSelReg->Sel      = u16Sel;
    pSelReg->ValidSel = u16Sel;
    pSelReg->fFlags   = CPUMSELREG_FLAGS_VALID;
    pSelReg->u32Limit = u32Limit;
    pSelReg->u64Base  = u64Base;
    pSelReg->Attr.u   = u32Attr;
    return VINF_SUCCESS;
}


/**
 * Gets VM-exit instruction information along with any displacement for an
 * instruction VM-exit.
 *
 * @returns The VM-exit instruction information.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 * @param   uInstrId        The VM-exit instruction identity (VMXINSTRID_XXX).
 * @param   pGCPtrDisp      Where to store the displacement field. Optional, can be
 *                          NULL.
 */
IEM_STATIC uint32_t iemVmxGetExitInstrInfo(PVMCPU pVCpu, uint32_t uExitReason, VMXINSTRID uInstrId, PRTGCPTR pGCPtrDisp)
{
    RTGCPTR          GCPtrDisp;
    VMXEXITINSTRINFO ExitInstrInfo;
    ExitInstrInfo.u = 0;

    /*
     * Get and parse the ModR/M byte from our decoded opcodes.
     */
    uint8_t bRm;
    uint8_t const offModRm = pVCpu->iem.s.offModRm;
    IEM_MODRM_GET_U8(pVCpu, bRm, offModRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * ModR/M indicates register addressing.
         *
         * The primary/secondary register operands are reported in the iReg1 or iReg2
         * fields depending on whether it is a read/write form.
         */
        uint8_t idxReg1;
        uint8_t idxReg2;
        if (!VMXINSTRID_IS_MODRM_PRIMARY_OP_W(uInstrId))
        {
            idxReg1 = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
            idxReg2 = (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB;
        }
        else
        {
            idxReg1 = (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB;
            idxReg2 = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
        }
        ExitInstrInfo.All.u2Scaling       = 0;
        ExitInstrInfo.All.iReg1           = idxReg1;
        ExitInstrInfo.All.u3AddrSize      = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand   = 1;
        ExitInstrInfo.All.uOperandSize    = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg         = 0;
        ExitInstrInfo.All.iIdxReg         = 0;
        ExitInstrInfo.All.fIdxRegInvalid  = 1;
        ExitInstrInfo.All.iBaseReg        = 0;
        ExitInstrInfo.All.fBaseRegInvalid = 1;
        ExitInstrInfo.All.iReg2           = idxReg2;

        /* Displacement not applicable for register addressing. */
        GCPtrDisp = 0;
    }
    else
    {
        /*
         * ModR/M indicates memory addressing.
         */
        uint8_t  uScale        = 0;
        bool     fBaseRegValid = false;
        bool     fIdxRegValid  = false;
        uint8_t  iBaseReg      = 0;
        uint8_t  iIdxReg       = 0;
        if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_16BIT)
        {
            /*
             * Parse the ModR/M, displacement for 16-bit addressing mode.
             * See Intel instruction spec. Table 2-1. "16-Bit Addressing Forms with the ModR/M Byte".
             */
            uint16_t u16Disp = 0;
            uint8_t const offDisp = offModRm + sizeof(bRm);
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
            {
                /* Displacement without any registers. */
                IEM_DISP_GET_U16(pVCpu, u16Disp, offDisp);
            }
            else
            {
                /* Register (index and base). */
                switch (bRm & X86_MODRM_RM_MASK)
                {
                    case 0: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; fIdxRegValid = true; iIdxReg = X86_GREG_xSI; break;
                    case 1: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; fIdxRegValid = true; iIdxReg = X86_GREG_xDI; break;
                    case 2: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; fIdxRegValid = true; iIdxReg = X86_GREG_xSI; break;
                    case 3: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; fIdxRegValid = true; iIdxReg = X86_GREG_xDI; break;
                    case 4: fIdxRegValid  = true; iIdxReg  = X86_GREG_xSI; break;
                    case 5: fIdxRegValid  = true; iIdxReg  = X86_GREG_xDI; break;
                    case 6: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; break;
                    case 7: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; break;
                }

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:                                                  break;
                    case 1: IEM_DISP_GET_S8_SX_U16(pVCpu, u16Disp, offDisp); break;
                    case 2: IEM_DISP_GET_U16(pVCpu, u16Disp, offDisp);       break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            Assert(!uScale);                    /* There's no scaling/SIB byte for 16-bit addressing. */
            GCPtrDisp = (int16_t)u16Disp;       /* Sign-extend the displacement. */
        }
        else if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT)
        {
            /*
             * Parse the ModR/M, SIB, displacement for 32-bit addressing mode.
             * See Intel instruction spec. Table 2-2. "32-Bit Addressing Forms with the ModR/M Byte".
             */
            uint32_t u32Disp = 0;
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
            {
                /* Displacement without any registers. */
                uint8_t const offDisp = offModRm + sizeof(bRm);
                IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);
            }
            else
            {
                /* Register (and perhaps scale, index and base). */
                uint8_t offDisp = offModRm + sizeof(bRm);
                iBaseReg = (bRm & X86_MODRM_RM_MASK);
                if (iBaseReg == 4)
                {
                    /* An SIB byte follows the ModR/M byte, parse it. */
                    uint8_t bSib;
                    uint8_t const offSib = offModRm + sizeof(bRm);
                    IEM_SIB_GET_U8(pVCpu, bSib, offSib);

                    /* A displacement may follow SIB, update its offset. */
                    offDisp += sizeof(bSib);

                    /* Get the scale. */
                    uScale = (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* Get the index register. */
                    iIdxReg = (bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK;
                    fIdxRegValid = RT_BOOL(iIdxReg != 4);

                    /* Get the base register. */
                    iBaseReg = bSib & X86_SIB_BASE_MASK;
                    fBaseRegValid = true;
                    if (iBaseReg == 5)
                    {
                        if ((bRm & X86_MODRM_MOD_MASK) == 0)
                        {
                            /* Mod is 0 implies a 32-bit displacement with no base. */
                            fBaseRegValid = false;
                            IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);
                        }
                        else
                        {
                            /* Mod is not 0 implies an 8-bit/32-bit displacement (handled below) with an EBP base. */
                            iBaseReg = X86_GREG_xBP;
                        }
                    }
                }

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0: /* Handled above */                              break;
                    case 1: IEM_DISP_GET_S8_SX_U32(pVCpu, u32Disp, offDisp); break;
                    case 2: IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);       break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            GCPtrDisp = (int32_t)u32Disp;       /* Sign-extend the displacement. */
        }
        else
        {
            Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT);

            /*
             * Parse the ModR/M, SIB, displacement for 64-bit addressing mode.
             * See Intel instruction spec. 2.2 "IA-32e Mode".
             */
            uint64_t u64Disp = 0;
            bool const fRipRelativeAddr = RT_BOOL((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5);
            if (fRipRelativeAddr)
            {
                /*
                 * RIP-relative addressing mode.
                 *
                 * The displacment is 32-bit signed implying an offset range of +/-2G.
                 * See Intel instruction spec. 2.2.1.6 "RIP-Relative Addressing".
                 */
                uint8_t const offDisp = offModRm + sizeof(bRm);
                IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp);
            }
            else
            {
                uint8_t offDisp = offModRm + sizeof(bRm);

                /*
                 * Register (and perhaps scale, index and base).
                 *
                 * REX.B extends the most-significant bit of the base register. However, REX.B
                 * is ignored while determining whether an SIB follows the opcode. Hence, we
                 * shall OR any REX.B bit -after- inspecting for an SIB byte below.
                 *
                 * See Intel instruction spec. Table 2-5. "Special Cases of REX Encodings".
                 */
                iBaseReg = (bRm & X86_MODRM_RM_MASK);
                if (iBaseReg == 4)
                {
                    /* An SIB byte follows the ModR/M byte, parse it. Displacement (if any) follows SIB. */
                    uint8_t bSib;
                    uint8_t const offSib = offModRm + sizeof(bRm);
                    IEM_SIB_GET_U8(pVCpu, bSib, offSib);

                    /* Displacement may follow SIB, update its offset. */
                    offDisp += sizeof(bSib);

                    /* Get the scale. */
                    uScale = (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* Get the index. */
                    iIdxReg = ((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK) | pVCpu->iem.s.uRexIndex;
                    fIdxRegValid = RT_BOOL(iIdxReg != 4);   /* R12 -can- be used as an index register. */

                    /* Get the base. */
                    iBaseReg = (bSib & X86_SIB_BASE_MASK);
                    fBaseRegValid = true;
                    if (iBaseReg == 5)
                    {
                        if ((bRm & X86_MODRM_MOD_MASK) == 0)
                        {
                            /* Mod is 0 implies a signed 32-bit displacement with no base. */
                            IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp);
                        }
                        else
                        {
                            /* Mod is non-zero implies an 8-bit/32-bit displacement (handled below) with RBP or R13 as base. */
                            iBaseReg = pVCpu->iem.s.uRexB ? X86_GREG_x13 : X86_GREG_xBP;
                        }
                    }
                }
                iBaseReg |= pVCpu->iem.s.uRexB;

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0: /* Handled above */                               break;
                    case 1: IEM_DISP_GET_S8_SX_U64(pVCpu, u64Disp, offDisp);  break;
                    case 2: IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp); break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            GCPtrDisp = fRipRelativeAddr ? pVCpu->cpum.GstCtx.rip + u64Disp : u64Disp;
        }

        /*
         * The primary or secondary register operand is reported in iReg2 depending
         * on whether the primary operand is in read/write form.
         */
        uint8_t idxReg2;
        if (!VMXINSTRID_IS_MODRM_PRIMARY_OP_W(uInstrId))
        {
            idxReg2 = bRm & X86_MODRM_RM_MASK;
            if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
                idxReg2 |= pVCpu->iem.s.uRexB;
        }
        else
        {
            idxReg2 = (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK;
            if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
                 idxReg2 |= pVCpu->iem.s.uRexReg;
        }
        ExitInstrInfo.All.u2Scaling      = uScale;
        ExitInstrInfo.All.iReg1          = 0;   /* Not applicable for memory addressing. */
        ExitInstrInfo.All.u3AddrSize     = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand  = 0;
        ExitInstrInfo.All.uOperandSize   = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg        = pVCpu->iem.s.iEffSeg;
        ExitInstrInfo.All.iIdxReg        = iIdxReg;
        ExitInstrInfo.All.fIdxRegInvalid = !fIdxRegValid;
        ExitInstrInfo.All.iBaseReg       = iBaseReg;
        ExitInstrInfo.All.iIdxReg        = !fBaseRegValid;
        ExitInstrInfo.All.iReg2          = idxReg2;
    }

    /*
     * Handle exceptions to the norm for certain instructions.
     * (e.g. some instructions convey an instruction identity in place of iReg2).
     */
    switch (uExitReason)
    {
        case VMX_EXIT_GDTR_IDTR_ACCESS:
        {
            Assert(VMXINSTRID_IS_VALID(uInstrId));
            Assert(VMXINSTRID_GET_ID(uInstrId) == (uInstrId & 0x3));
            ExitInstrInfo.GdtIdt.u2InstrId = VMXINSTRID_GET_ID(uInstrId);
            ExitInstrInfo.GdtIdt.u2Undef0  = 0;
            break;
        }

        case VMX_EXIT_LDTR_TR_ACCESS:
        {
            Assert(VMXINSTRID_IS_VALID(uInstrId));
            Assert(VMXINSTRID_GET_ID(uInstrId) == (uInstrId & 0x3));
            ExitInstrInfo.LdtTr.u2InstrId = VMXINSTRID_GET_ID(uInstrId);
            ExitInstrInfo.LdtTr.u2Undef0 = 0;
            break;
        }

        case VMX_EXIT_RDRAND:
        case VMX_EXIT_RDSEED:
        {
            Assert(ExitInstrInfo.RdrandRdseed.u2OperandSize != 3);
            break;
        }
    }

    /* Update displacement and return the constructed VM-exit instruction information field. */
    if (pGCPtrDisp)
        *pGCPtrDisp = GCPtrDisp;

    return ExitInstrInfo.u;
}


/**
 * Sets the VM-instruction error VMCS field.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM-instruction error.
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetVmInstrErr(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u32RoVmInstrError = enmInsErr;
}


/**
 * Sets the VM-exit qualification VMCS field.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uExitQual   The VM-exit qualification field.
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetExitQual(PVMCPU pVCpu, uint64_t uExitQual)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u64RoExitQual.u = uExitQual;
}


/**
 * Sets the VM-exit guest-linear address VMCS field.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uGuestLinearAddr    The VM-exit guest-linear address field.
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetExitGuestLinearAddr(PVMCPU pVCpu, uint64_t uGuestLinearAddr)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u64RoGuestLinearAddr.u = uGuestLinearAddr;
}


/**
 * Sets the VM-exit guest-physical address VMCS field.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uGuestPhysAddr  The VM-exit guest-physical address field.
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetExitGuestPhysAddr(PVMCPU pVCpu, uint64_t uGuestPhysAddr)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u64RoGuestPhysAddr.u = uGuestPhysAddr;
}


/**
 * Sets the VM-exit instruction length VMCS field.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cbInstr     The VM-exit instruction length (in bytes).
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetExitInstrLen(PVMCPU pVCpu, uint32_t cbInstr)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u32RoExitInstrLen = cbInstr;
}


/**
 * Sets the VM-exit instruction info. VMCS field.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitInstrInfo  The VM-exit instruction info. field.
 */
DECL_FORCE_INLINE(void) iemVmxVmcsSetExitInstrInfo(PVMCPU pVCpu, uint32_t uExitInstrInfo)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVmcs->u32RoExitInstrInfo = uExitInstrInfo;
}


/**
 * Implements VMSucceed for VMX instruction success.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmSucceed(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
}


/**
 * Implements VMFailInvalid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmFailInvalid(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
    pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_CF;
}


/**
 * Implements VMFailValid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECL_FORCE_INLINE(void) iemVmxVmFailValid(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
        pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_ZF;
        iemVmxVmcsSetVmInstrErr(pVCpu, enmInsErr);
    }
}


/**
 * Implements VMFail for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECL_FORCE_INLINE(void) iemVmxVmFail(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
        iemVmxVmFailValid(pVCpu, enmInsErr);
    else
        iemVmxVmFailInvalid(pVCpu);
}


/**
 * Checks if the given auto-load/store MSR area count is valid for the
 * implementation.
 *
 * @returns @c true if it's within the valid limit, @c false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uMsrCount       The MSR area count to check.
 */
DECL_FORCE_INLINE(bool) iemVmxIsAutoMsrCountValid(PVMCPU pVCpu, uint32_t uMsrCount)
{
    uint64_t const u64VmxMiscMsr      = CPUMGetGuestIa32VmxMisc(pVCpu);
    uint32_t const cMaxSupportedMsrs  = VMX_MISC_MAX_MSRS(u64VmxMiscMsr);
    Assert(cMaxSupportedMsrs <= VMX_V_AUTOMSR_AREA_SIZE / sizeof(VMXAUTOMSR));
    if (uMsrCount <= cMaxSupportedMsrs)
        return true;
    return false;
}


/**
 * Flushes the current VMCS contents back to guest memory.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(int) iemVmxCommitCurrentVmcsToMemory(PVMCPU pVCpu)
{
    Assert(IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), IEM_VMX_GET_CURRENT_VMCS(pVCpu),
                                      pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs), sizeof(VMXVVMCS));
    IEM_VMX_CLEAR_CURRENT_VMCS(pVCpu);
    return rc;
}


/**
 * Implements VMSucceed for the VMREAD instruction and increments the guest RIP.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmreadSuccess(PVMCPU pVCpu, uint8_t cbInstr)
{
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
}


/**
 * Gets the instruction diagnostic for segment base checks during VM-entry of a
 * nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegBase(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegBaseCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegBaseDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegBaseEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegBaseFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegBaseGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegBaseSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_1);
    }
}


/**
 * Gets the instruction diagnostic for segment base checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegBaseV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegBaseV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegBaseV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegBaseV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegBaseV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegBaseV86Gs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegBaseV86Ss;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_2);
    }
}


/**
 * Gets the instruction diagnostic for segment limit checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegLimitV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegLimitV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegLimitV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegLimitV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegLimitV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegLimitV86Gs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegLimitV86Ss;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_3);
    }
}


/**
 * Gets the instruction diagnostic for segment attribute checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrV86Gs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrV86Ss;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_4);
    }
}


/**
 * Gets the instruction diagnostic for segment attributes reserved bits failure
 * during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrRsvd(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrRsvdEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_5);
    }
}


/**
 * Gets the instruction diagnostic for segment attributes descriptor-type
 * (code/segment or system) failure during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrDescType(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_6);
    }
}


/**
 * Gets the instruction diagnostic for segment attributes descriptor-type
 * (code/segment or system) failure during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrPresent(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrPresentCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrPresentDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrPresentEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrPresentFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrPresentGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrPresentSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_7);
    }
}


/**
 * Gets the instruction diagnostic for segment attribute granularity failure during
 * VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrGran(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrGranCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrGranDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrGranEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrGranFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrGranGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrGranSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_8);
    }
}

/**
 * Gets the instruction diagnostic for segment attribute DPL/RPL failure during
 * VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrDplRpl(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrDplRplEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_9);
    }
}


/**
 * Gets the instruction diagnostic for segment attribute type accessed failure
 * during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrTypeAcc(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccGs;
        case X86_SREG_SS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccSs;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_10);
    }
}


/**
 * Gets the instruction diagnostic for guest CR3 referenced PDPTE reserved bits
 * failure during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The PDPTE entry index.
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentryPdpteRsvd(unsigned iPdpte)
{
    Assert(iPdpte < X86_PG_PAE_PDPE_ENTRIES);
    switch (iPdpte)
    {
        case 0: return kVmxVDiag_Vmentry_GuestPdpte0Rsvd;
        case 1: return kVmxVDiag_Vmentry_GuestPdpte1Rsvd;
        case 2: return kVmxVDiag_Vmentry_GuestPdpte2Rsvd;
        case 3: return kVmxVDiag_Vmentry_GuestPdpte3Rsvd;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_11);
    }
}


/**
 * Gets the instruction diagnostic for host CR3 referenced PDPTE reserved bits
 * failure during VM-exit of a nested-guest.
 *
 * @param   iSegReg     The PDPTE entry index.
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmexitPdpteRsvd(unsigned iPdpte)
{
    Assert(iPdpte < X86_PG_PAE_PDPE_ENTRIES);
    switch (iPdpte)
    {
        case 0: return kVmxVDiag_Vmexit_HostPdpte0Rsvd;
        case 1: return kVmxVDiag_Vmexit_HostPdpte1Rsvd;
        case 2: return kVmxVDiag_Vmexit_HostPdpte2Rsvd;
        case 3: return kVmxVDiag_Vmexit_HostPdpte3Rsvd;
        IEM_NOT_REACHED_DEFAULT_CASE_RET2(kVmxVDiag_Ipe_12);
    }
}


/**
 * Saves the guest control registers, debug registers and some MSRs are part of
 * VM-exit.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmexitSaveGuestControlRegsMsrs(PVMCPU pVCpu)
{
    /*
     * Saves the guest control registers, debug registers and some MSRs.
     * See Intel spec. 27.3.1 "Saving Control Registers, Debug Registers and MSRs".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);

    /* Save control registers. */
    pVmcs->u64GuestCr0.u = pVCpu->cpum.GstCtx.cr0;
    pVmcs->u64GuestCr3.u = pVCpu->cpum.GstCtx.cr3;
    pVmcs->u64GuestCr4.u = pVCpu->cpum.GstCtx.cr4;

    /* Save SYSENTER CS, ESP, EIP. */
    pVmcs->u32GuestSysenterCS    = pVCpu->cpum.GstCtx.SysEnter.cs;
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        pVmcs->u64GuestSysenterEsp.u = pVCpu->cpum.GstCtx.SysEnter.esp;
        pVmcs->u64GuestSysenterEip.u = pVCpu->cpum.GstCtx.SysEnter.eip;
    }
    else
    {
        pVmcs->u64GuestSysenterEsp.s.Lo = pVCpu->cpum.GstCtx.SysEnter.esp;
        pVmcs->u64GuestSysenterEip.s.Lo = pVCpu->cpum.GstCtx.SysEnter.eip;
    }

    /* Save debug registers (DR7 and IA32_DEBUGCTL MSR). */
    if (pVmcs->u32ExitCtls & VMX_EXIT_CTLS_SAVE_DEBUG)
    {
        pVmcs->u64GuestDr7.u = pVCpu->cpum.GstCtx.dr[7];
        /** @todo NSTVMX: Support IA32_DEBUGCTL MSR */
    }

    /* Save PAT MSR. */
    if (pVmcs->u32ExitCtls & VMX_EXIT_CTLS_SAVE_PAT_MSR)
        pVmcs->u64GuestPatMsr.u = pVCpu->cpum.GstCtx.msrPAT;

    /* Save EFER MSR. */
    if (pVmcs->u32ExitCtls & VMX_EXIT_CTLS_SAVE_EFER_MSR)
        pVmcs->u64GuestEferMsr.u = pVCpu->cpum.GstCtx.msrEFER;

    /* We don't support clearing IA32_BNDCFGS MSR yet. */
    Assert(!(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_CLEAR_BNDCFGS_MSR));

    /* Nothing to do for SMBASE register - We don't support SMM yet. */
}


/**
 * Saves the guest force-flags in prepartion of entering the nested-guest.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmentrySaveForceFlags(PVMCPU pVCpu)
{
    /* We shouldn't be called multiple times during VM-entry. */
    Assert(pVCpu->cpum.GstCtx.hwvirt.fLocalForcedActions == 0);

    /* MTF should not be set outside VMX non-root mode. */
    Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_MTF));

    /*
     * Preserve the required force-flags.
     *
     * We cache and clear force-flags that would affect the execution of the
     * nested-guest. Cached flags are then restored while returning to the guest
     * if necessary.
     *
     *   - VMCPU_FF_INHIBIT_INTERRUPTS need not be cached as it only affects
     *     interrupts until the completion of the current VMLAUNCH/VMRESUME
     *     instruction. Interrupt inhibition for any nested-guest instruction
     *     will be set later while loading the guest-interruptibility state.
     *
     *   - VMCPU_FF_BLOCK_NMIS needs to be cached as VM-exits caused before
     *     successful VM-entry needs to continue blocking NMIs if it was in effect
     *     during VM-entry.
     *
     *   - MTF need not be preserved as it's used only in VMX non-root mode and
     *     is supplied on VM-entry through the VM-execution controls.
     *
     * The remaining FFs (e.g. timers, APIC updates) must stay in place so that
     * we will be able to generate interrupts that may cause VM-exits for
     * the nested-guest.
     */
    pVCpu->cpum.GstCtx.hwvirt.fLocalForcedActions = pVCpu->fLocalForcedActions & VMCPU_FF_BLOCK_NMIS;

    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS | VMCPU_FF_BLOCK_NMIS))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS | VMCPU_FF_BLOCK_NMIS);
}


/**
 * Restores the guest force-flags in prepartion of exiting the nested-guest.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmexitRestoreForceFlags(PVMCPU pVCpu)
{
    if (pVCpu->cpum.GstCtx.hwvirt.fLocalForcedActions)
    {
        VMCPU_FF_SET(pVCpu, pVCpu->cpum.GstCtx.hwvirt.fLocalForcedActions);
        pVCpu->cpum.GstCtx.hwvirt.fLocalForcedActions = 0;
    }
}


/**
 * Perform a VMX transition updated PGM, IEM and CPUM.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
IEM_STATIC int iemVmxWorldSwitch(PVMCPU pVCpu)
{
    /*
     * Inform PGM about paging mode changes.
     * We include X86_CR0_PE because PGM doesn't handle paged-real mode yet,
     * see comment in iemMemPageTranslateAndCheckAccess().
     */
    int rc = PGMChangeMode(pVCpu, pVCpu->cpum.GstCtx.cr0 | X86_CR0_PE, pVCpu->cpum.GstCtx.cr4, pVCpu->cpum.GstCtx.msrEFER);
# ifdef IN_RING3
    Assert(rc != VINF_PGM_CHANGE_MODE);
# endif
    AssertRCReturn(rc, rc);

    /* Inform CPUM (recompiler), can later be removed. */
    CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_ALL);

    /*
     * Flush the TLB with new CR3. This is required in case the PGM mode change
     * above doesn't actually change anything.
     */
    if (rc == VINF_SUCCESS)
    {
        rc = PGMFlushTLB(pVCpu, pVCpu->cpum.GstCtx.cr3, true);
        AssertRCReturn(rc, rc);
    }

    /* Re-initialize IEM cache/state after the drastic mode switch. */
    iemReInitExec(pVCpu);
    return rc;
}


/**
 * Saves guest segment registers, GDTR, IDTR, LDTR, TR as part of VM-exit.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmexitSaveGuestSegRegs(PVMCPU pVCpu)
{
    /*
     * Save guest segment registers, GDTR, IDTR, LDTR, TR.
     * See Intel spec 27.3.2 "Saving Segment Registers and Descriptor-Table Registers".
     */
    /* CS, SS, ES, DS, FS, GS. */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    for (unsigned iSegReg = 0; iSegReg < X86_SREG_COUNT; iSegReg++)
    {
        PCCPUMSELREG pSelReg = &pVCpu->cpum.GstCtx.aSRegs[iSegReg];
        if (!pSelReg->Attr.n.u1Unusable)
            iemVmxVmcsSetGuestSegReg(pVmcs, iSegReg, pSelReg);
        else
        {
            /*
             * For unusable segments the attributes are undefined except for CS and SS.
             * For the rest we don't bother preserving anything but the unusable bit.
             */
            switch (iSegReg)
            {
                case X86_SREG_CS:
                    pVmcs->GuestCs          = pSelReg->Sel;
                    pVmcs->u64GuestCsBase.u = pSelReg->u64Base;
                    pVmcs->u32GuestCsLimit  = pSelReg->u32Limit;
                    pVmcs->u32GuestCsAttr   = pSelReg->Attr.u & (  X86DESCATTR_L | X86DESCATTR_D | X86DESCATTR_G
                                                                 | X86DESCATTR_UNUSABLE);
                    break;

                case X86_SREG_SS:
                    pVmcs->GuestSs        = pSelReg->Sel;
                    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
                        pVmcs->u64GuestSsBase.u &= UINT32_C(0xffffffff);
                    pVmcs->u32GuestSsAttr = pSelReg->Attr.u & (X86DESCATTR_DPL | X86DESCATTR_UNUSABLE);
                    break;

                case X86_SREG_DS:
                    pVmcs->GuestDs        = pSelReg->Sel;
                    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
                        pVmcs->u64GuestDsBase.u &= UINT32_C(0xffffffff);
                    pVmcs->u32GuestDsAttr = X86DESCATTR_UNUSABLE;
                    break;

                case X86_SREG_ES:
                    pVmcs->GuestEs        = pSelReg->Sel;
                    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
                        pVmcs->u64GuestEsBase.u &= UINT32_C(0xffffffff);
                    pVmcs->u32GuestEsAttr = X86DESCATTR_UNUSABLE;
                    break;

                case X86_SREG_FS:
                    pVmcs->GuestFs          = pSelReg->Sel;
                    pVmcs->u64GuestFsBase.u = pSelReg->u64Base;
                    pVmcs->u32GuestFsAttr   = X86DESCATTR_UNUSABLE;
                    break;

                case X86_SREG_GS:
                    pVmcs->GuestGs          = pSelReg->Sel;
                    pVmcs->u64GuestGsBase.u = pSelReg->u64Base;
                    pVmcs->u32GuestGsAttr   = X86DESCATTR_UNUSABLE;
                    break;
            }
        }
    }

    /* Segment attribute bits 31:7 and 11:8 MBZ. */
    uint32_t const fValidAttrMask = X86DESCATTR_TYPE | X86DESCATTR_DT  | X86DESCATTR_DPL | X86DESCATTR_P
                                  | X86DESCATTR_AVL  | X86DESCATTR_L   | X86DESCATTR_D   | X86DESCATTR_G | X86DESCATTR_UNUSABLE;
    /* LDTR. */
    {
        PCCPUMSELREG pSelReg = &pVCpu->cpum.GstCtx.ldtr;
        pVmcs->GuestLdtr          = pSelReg->Sel;
        pVmcs->u64GuestLdtrBase.u = pSelReg->u64Base;
        Assert(X86_IS_CANONICAL(pSelReg->u64Base));
        pVmcs->u32GuestLdtrLimit  = pSelReg->u32Limit;
        pVmcs->u32GuestLdtrAttr   = pSelReg->Attr.u & fValidAttrMask;
    }

    /* TR. */
    {
        PCCPUMSELREG pSelReg = &pVCpu->cpum.GstCtx.tr;
        pVmcs->GuestTr          = pSelReg->Sel;
        pVmcs->u64GuestTrBase.u = pSelReg->u64Base;
        pVmcs->u32GuestTrLimit  = pSelReg->u32Limit;
        pVmcs->u32GuestTrAttr   = pSelReg->Attr.u & fValidAttrMask;
    }

    /* GDTR. */
    pVmcs->u64GuestGdtrBase.u = pVCpu->cpum.GstCtx.gdtr.pGdt;
    pVmcs->u32GuestGdtrLimit  = pVCpu->cpum.GstCtx.gdtr.cbGdt;

    /* IDTR. */
    pVmcs->u64GuestIdtrBase.u = pVCpu->cpum.GstCtx.idtr.pIdt;
    pVmcs->u32GuestIdtrLimit  = pVCpu->cpum.GstCtx.idtr.cbIdt;
}


/**
 * Saves guest non-register state as part of VM-exit.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 */
IEM_STATIC void iemVmxVmexitSaveGuestNonRegState(PVMCPU pVCpu, uint32_t uExitReason)
{
    /*
     * Save guest non-register state.
     * See Intel spec. 27.3.4 "Saving Non-Register State".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);

    /* Activity-state: VM-exits occur before changing the activity state, nothing further to do */

    /* Interruptibility-state. */
    pVmcs->u32GuestIntrState = 0;
    if (pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI)
    { /** @todo NSTVMX: Virtual-NMI blocking. */ }
    else if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        pVmcs->u32GuestIntrState |= VMX_VMCS_GUEST_INT_STATE_BLOCK_NMI;

    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && pVCpu->cpum.GstCtx.rip == EMGetInhibitInterruptsPC(pVCpu))
    {
        /** @todo NSTVMX: We can't distinguish between blocking-by-MovSS and blocking-by-STI
         *        currently. */
        pVmcs->u32GuestIntrState |= VMX_VMCS_GUEST_INT_STATE_BLOCK_STI;
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
    }
    /* Nothing to do for SMI/enclave. We don't support enclaves or SMM yet. */

    /* Pending debug exceptions. */
    if (    uExitReason != VMX_EXIT_INIT_SIGNAL
        &&  uExitReason != VMX_EXIT_SMI
        &&  uExitReason != VMX_EXIT_ERR_MACHINE_CHECK
        && !HMVmxIsTrapLikeVmexit(uExitReason))
    {
        /** @todo NSTVMX: also must exclude VM-exits caused by debug exceptions when
         *        block-by-MovSS is in effect. */
        pVmcs->u64GuestPendingDbgXcpt.u = 0;
    }

    /** @todo NSTVMX: Save VMX preemption timer value. */

    /* PDPTEs. */
    /* We don't support EPT yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT));
    pVmcs->u64GuestPdpte0.u = 0;
    pVmcs->u64GuestPdpte1.u = 0;
    pVmcs->u64GuestPdpte2.u = 0;
    pVmcs->u64GuestPdpte3.u = 0;
}


/**
 * Saves the guest-state as part of VM-exit.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 */
IEM_STATIC void iemVmxVmexitSaveGuestState(PVMCPU pVCpu, uint32_t uExitReason)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pVmcs);

    iemVmxVmexitSaveGuestControlRegsMsrs(pVCpu);
    iemVmxVmexitSaveGuestSegRegs(pVCpu);

    /*
     * Save guest RIP, RSP and RFLAGS.
     * See Intel spec. 27.3.3 "Saving RIP, RSP and RFLAGS".
     */
    /* We don't support enclave mode yet. */
    pVmcs->u64GuestRip.u    = pVCpu->cpum.GstCtx.rip;
    pVmcs->u64GuestRsp.u    = pVCpu->cpum.GstCtx.rsp;
    pVmcs->u64GuestRFlags.u = pVCpu->cpum.GstCtx.rflags.u;  /** @todo NSTVMX: Check RFLAGS.RF handling. */

    iemVmxVmexitSaveGuestNonRegState(pVCpu, uExitReason);
}


/**
 * Saves the guest MSRs into the VM-exit auto-store MSRs area as part of VM-exit.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason (for diagnostic purposes).
 */
IEM_STATIC int iemVmxVmexitSaveGuestAutoMsrs(PVMCPU pVCpu, uint32_t uExitReason)
{
    /*
     * Save guest MSRs.
     * See Intel spec. 27.4 "Saving MSRs".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VMX-abort";

    /*
     * The VM-exit MSR-store area address need not be a valid guest-physical address if the
     * VM-exit MSR-store count is 0. If this is the case, bail early without reading it.
     * See Intel spec. 24.7.2 "VM-Exit Controls for MSRs".
     */
    uint32_t const cMsrs = pVmcs->u32ExitMsrStoreCount;
    if (!cMsrs)
        return VINF_SUCCESS;

    /*
     * Verify the MSR auto-store count. Physical CPUs can behave unpredictably if the count
     * is exceeded including possibly raising #MC exceptions during VMX transition. Our
     * implementation causes a VMX-abort followed by a triple-fault.
     */
    bool const fIsMsrCountValid = iemVmxIsAutoMsrCountValid(pVCpu, cMsrs);
    if (fIsMsrCountValid)
    { /* likely */ }
    else
        IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrStoreCount);

    PVMXAUTOMSR pMsr = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea);
    Assert(pMsr);
    for (uint32_t idxMsr = 0; idxMsr < cMsrs; idxMsr++, pMsr++)
    {
        if (   !pMsr->u32Reserved
            &&  pMsr->u32Msr != MSR_IA32_SMBASE
            &&  pMsr->u32Msr >> 8 != MSR_IA32_X2APIC_START >> 8)
        {
            VBOXSTRICTRC rcStrict = CPUMQueryGuestMsr(pVCpu, pMsr->u32Msr, &pMsr->u64Value);
            if (rcStrict == VINF_SUCCESS)
                continue;

            /*
             * If we're in ring-0, we cannot handle returns to ring-3 at this point and continue VM-exit.
             * If any guest hypervisor loads MSRs that require ring-3 handling, we cause a VMX-abort
             * recording the MSR index in the auxiliary info. field and indicated further by our
             * own, specific diagnostic code. Later, we can try implement handling of the MSR in ring-0
             * if possible, or come up with a better, generic solution.
             */
            pVCpu->cpum.GstCtx.hwvirt.vmx.uAbortAux = pMsr->u32Msr;
            VMXVDIAG const enmDiag = rcStrict == VINF_CPUM_R3_MSR_READ
                                   ? kVmxVDiag_Vmexit_MsrStoreRing3
                                   : kVmxVDiag_Vmexit_MsrStore;
            IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, enmDiag);
        }
        else
        {
            pVCpu->cpum.GstCtx.hwvirt.vmx.uAbortAux = pMsr->u32Msr;
            IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrStoreRsvd);
        }
    }

    RTGCPHYS const GCPhysAutoMsrArea = pVmcs->u64AddrExitMsrStore.u;
    int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), GCPhysAutoMsrArea,
                                      pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea), VMX_V_AUTOMSR_AREA_SIZE);
    if (RT_SUCCESS(rc))
    { /* likely */ }
    else
    {
        AssertMsgFailed(("VM-exit: Failed to write MSR auto-store area at %#RGp, rc=%Rrc\n", GCPhysAutoMsrArea, rc));
        IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrStorePtrWritePhys);
    }

    NOREF(uExitReason);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Performs a VMX abort (due to an fatal error during VM-exit).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmAbort    The VMX abort reason.
 */
IEM_STATIC VBOXSTRICTRC iemVmxAbort(PVMCPU pVCpu, VMXABORT enmAbort)
{
    /*
     * Perform the VMX abort.
     * See Intel spec. 27.7 "VMX Aborts".
     */
    LogFunc(("enmAbort=%u (%s) -> RESET\n", enmAbort, HMVmxGetAbortDesc(enmAbort)));

    /* We don't support SMX yet. */
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmAbort = enmAbort;
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        RTGCPHYS const GCPhysVmcs  = IEM_VMX_GET_CURRENT_VMCS(pVCpu);
        uint32_t const offVmxAbort = RT_OFFSETOF(VMXVVMCS, u32VmxAbortId);
        PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), GCPhysVmcs + offVmxAbort, &enmAbort, sizeof(enmAbort));
    }

    return VINF_EM_TRIPLE_FAULT;
}


/**
 * Loads host control registers, debug registers and MSRs as part of VM-exit.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmexitLoadHostControlRegsMsrs(PVMCPU pVCpu)
{
    /*
     * Load host control registers, debug registers and MSRs.
     * See Intel spec. 27.5.1 "Loading Host Control Registers, Debug Registers, MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fHostInLongMode = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);

    /* CR0. */
    {
        /* Bits 63:32, 28:19, 17, 15:6, ET, CD, NW and CR0 MB1 bits are not modified. */
        uint64_t const uCr0Fixed0  = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
        uint64_t const fCr0IgnMask = UINT64_C(0xffffffff1ff8ffc0) | X86_CR0_ET | X86_CR0_CD | X86_CR0_NW | uCr0Fixed0;
        uint64_t const uHostCr0    = pVmcs->u64HostCr0.u;
        uint64_t const uGuestCr0   = pVCpu->cpum.GstCtx.cr0;
        uint64_t const uValidCr0   = (uHostCr0 & ~fCr0IgnMask) | (uGuestCr0 & fCr0IgnMask);
        CPUMSetGuestCR0(pVCpu, uValidCr0);
    }

    /* CR4. */
    {
        /* CR4 MB1 bits are not modified. */
        uint64_t const fCr4IgnMask = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
        uint64_t const uHostCr4    = pVmcs->u64HostCr4.u;
        uint64_t const uGuestCr4   = pVCpu->cpum.GstCtx.cr4;
        uint64_t       uValidCr4   = (uHostCr4 & ~fCr4IgnMask) | (uGuestCr4 & fCr4IgnMask);
        if (fHostInLongMode)
            uValidCr4 |= X86_CR4_PAE;
        else
            uValidCr4 &= ~X86_CR4_PCIDE;
        CPUMSetGuestCR4(pVCpu, uValidCr4);
    }

    /* CR3 (host value validated while checking host-state during VM-entry). */
    pVCpu->cpum.GstCtx.cr3 = pVmcs->u64HostCr3.u;

    /* DR7. */
    pVCpu->cpum.GstCtx.dr[7] = X86_DR7_INIT_VAL;

    /** @todo NSTVMX: Support IA32_DEBUGCTL MSR */

    /* Save SYSENTER CS, ESP, EIP (host value validated while checking host-state during VM-entry). */
    pVCpu->cpum.GstCtx.SysEnter.eip = pVmcs->u64HostSysenterEip.u;
    pVCpu->cpum.GstCtx.SysEnter.esp = pVmcs->u64HostSysenterEsp.u;
    pVCpu->cpum.GstCtx.SysEnter.cs  = pVmcs->u32HostSysenterCs;

    /* FS, GS bases are loaded later while we load host segment registers. */

    /* EFER MSR (host value validated while checking host-state during VM-entry). */
    if (pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_EFER_MSR)
        pVCpu->cpum.GstCtx.msrEFER = pVmcs->u64HostEferMsr.u;
    else if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        if (fHostInLongMode)
            pVCpu->cpum.GstCtx.msrEFER |=  (MSR_K6_EFER_LMA | MSR_K6_EFER_LME);
        else
            pVCpu->cpum.GstCtx.msrEFER &= ~(MSR_K6_EFER_LMA | MSR_K6_EFER_LME);
    }

    /* We don't support IA32_PERF_GLOBAL_CTRL MSR yet. */

    /* PAT MSR (host value is validated while checking host-state during VM-entry). */
    if (pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_PAT_MSR)
        pVCpu->cpum.GstCtx.msrPAT = pVmcs->u64HostPatMsr.u;

    /* We don't support IA32_BNDCFGS MSR yet. */
}


/**
 * Loads host segment registers, GDTR, IDTR, LDTR and TR as part of VM-exit.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmexitLoadHostSegRegs(PVMCPU pVCpu)
{
    /*
     * Load host segment registers, GDTR, IDTR, LDTR and TR.
     * See Intel spec. 27.5.2 "Loading Host Segment and Descriptor-Table Registers".
     *
     * Warning! Be careful to not touch fields that are reserved by VT-x,
     * e.g. segment limit high bits stored in segment attributes (in bits 11:8).
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fHostInLongMode = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);

    /* CS, SS, ES, DS, FS, GS. */
    for (unsigned iSegReg = 0; iSegReg < X86_SREG_COUNT; iSegReg++)
    {
        RTSEL const HostSel  = iemVmxVmcsGetHostSelReg(pVmcs, iSegReg);
        bool const  fUnusable = RT_BOOL(HostSel == 0);

        /* Selector. */
        pVCpu->cpum.GstCtx.aSRegs[iSegReg].Sel      = HostSel;
        pVCpu->cpum.GstCtx.aSRegs[iSegReg].ValidSel = HostSel;
        pVCpu->cpum.GstCtx.aSRegs[iSegReg].fFlags   = CPUMSELREG_FLAGS_VALID;

        /* Limit. */
        pVCpu->cpum.GstCtx.aSRegs[iSegReg].u32Limit = 0xffffffff;

        /* Base and Attributes. */
        switch (iSegReg)
        {
            case X86_SREG_CS:
            {
                pVCpu->cpum.GstCtx.cs.u64Base = 0;
                pVCpu->cpum.GstCtx.cs.Attr.n.u4Type        = X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_ACCESSED;
                pVCpu->cpum.GstCtx.ss.Attr.n.u1DescType    = 1;
                pVCpu->cpum.GstCtx.cs.Attr.n.u2Dpl         = 0;
                pVCpu->cpum.GstCtx.cs.Attr.n.u1Present     = 1;
                pVCpu->cpum.GstCtx.cs.Attr.n.u1Long        = fHostInLongMode;
                pVCpu->cpum.GstCtx.cs.Attr.n.u1DefBig      = !fHostInLongMode;
                pVCpu->cpum.GstCtx.cs.Attr.n.u1Granularity = 1;
                Assert(!pVCpu->cpum.GstCtx.cs.Attr.n.u1Unusable);
                Assert(!fUnusable);
                break;
            }

            case X86_SREG_SS:
            case X86_SREG_ES:
            case X86_SREG_DS:
            {
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].u64Base = 0;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u4Type        = X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u1DescType    = 1;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u2Dpl         = 0;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u1Present     = 1;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u1DefBig      = 1;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u1Granularity = 1;
                pVCpu->cpum.GstCtx.aSRegs[iSegReg].Attr.n.u1Unusable    = fUnusable;
                break;
            }

            case X86_SREG_FS:
            {
                Assert(X86_IS_CANONICAL(pVmcs->u64HostFsBase.u));
                pVCpu->cpum.GstCtx.fs.u64Base = !fUnusable ? pVmcs->u64HostFsBase.u : 0;
                pVCpu->cpum.GstCtx.fs.Attr.n.u4Type        = X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED;
                pVCpu->cpum.GstCtx.fs.Attr.n.u1DescType    = 1;
                pVCpu->cpum.GstCtx.fs.Attr.n.u2Dpl         = 0;
                pVCpu->cpum.GstCtx.fs.Attr.n.u1Present     = 1;
                pVCpu->cpum.GstCtx.fs.Attr.n.u1DefBig      = 1;
                pVCpu->cpum.GstCtx.fs.Attr.n.u1Granularity = 1;
                pVCpu->cpum.GstCtx.fs.Attr.n.u1Unusable    = fUnusable;
                break;
            }

            case X86_SREG_GS:
            {
                Assert(X86_IS_CANONICAL(pVmcs->u64HostGsBase.u));
                pVCpu->cpum.GstCtx.gs.u64Base = !fUnusable ? pVmcs->u64HostGsBase.u : 0;
                pVCpu->cpum.GstCtx.gs.Attr.n.u4Type        = X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED;
                pVCpu->cpum.GstCtx.gs.Attr.n.u1DescType    = 1;
                pVCpu->cpum.GstCtx.gs.Attr.n.u2Dpl         = 0;
                pVCpu->cpum.GstCtx.gs.Attr.n.u1Present     = 1;
                pVCpu->cpum.GstCtx.gs.Attr.n.u1DefBig      = 1;
                pVCpu->cpum.GstCtx.gs.Attr.n.u1Granularity = 1;
                pVCpu->cpum.GstCtx.gs.Attr.n.u1Unusable    = fUnusable;
                break;
            }
        }
    }

    /* TR. */
    Assert(X86_IS_CANONICAL(pVmcs->u64HostTrBase.u));
    Assert(!pVCpu->cpum.GstCtx.tr.Attr.n.u1Unusable);
    pVCpu->cpum.GstCtx.tr.Sel                  = pVmcs->HostTr;
    pVCpu->cpum.GstCtx.tr.ValidSel             = pVmcs->HostTr;
    pVCpu->cpum.GstCtx.tr.fFlags               = CPUMSELREG_FLAGS_VALID;
    pVCpu->cpum.GstCtx.tr.u32Limit             = X86_SEL_TYPE_SYS_386_TSS_LIMIT_MIN;
    pVCpu->cpum.GstCtx.tr.u64Base              = pVmcs->u64HostTrBase.u;
    pVCpu->cpum.GstCtx.tr.Attr.n.u4Type        = X86_SEL_TYPE_SYS_386_TSS_BUSY;
    pVCpu->cpum.GstCtx.tr.Attr.n.u1DescType    = 0;
    pVCpu->cpum.GstCtx.tr.Attr.n.u2Dpl         = 0;
    pVCpu->cpum.GstCtx.tr.Attr.n.u1Present     = 1;
    pVCpu->cpum.GstCtx.tr.Attr.n.u1DefBig      = 0;
    pVCpu->cpum.GstCtx.tr.Attr.n.u1Granularity = 0;

    /* LDTR. */
    pVCpu->cpum.GstCtx.ldtr.Sel               = 0;
    pVCpu->cpum.GstCtx.ldtr.ValidSel          = 0;
    pVCpu->cpum.GstCtx.ldtr.fFlags            = CPUMSELREG_FLAGS_VALID;
    pVCpu->cpum.GstCtx.ldtr.u32Limit          = 0;
    pVCpu->cpum.GstCtx.ldtr.u64Base           = 0;
    pVCpu->cpum.GstCtx.ldtr.Attr.n.u1Unusable = 1;

    /* GDTR. */
    Assert(X86_IS_CANONICAL(pVmcs->u64HostGdtrBase.u));
    pVCpu->cpum.GstCtx.gdtr.pGdt  = pVmcs->u64HostGdtrBase.u;
    pVCpu->cpum.GstCtx.gdtr.cbGdt = 0xfff;

    /* IDTR.*/
    Assert(X86_IS_CANONICAL(pVmcs->u64HostIdtrBase.u));
    pVCpu->cpum.GstCtx.idtr.pIdt  = pVmcs->u64HostIdtrBase.u;
    pVCpu->cpum.GstCtx.idtr.cbIdt = 0xfff;
}


/**
 * Checks host PDPTes as part of VM-exit.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason (for logging purposes).
 */
IEM_STATIC int iemVmxVmexitCheckHostPdptes(PVMCPU pVCpu, uint32_t uExitReason)
{
    /*
     * Check host PDPTEs.
     * See Intel spec. 27.5.4 "Checking and Loading Host Page-Directory-Pointer-Table Entries".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VMX-abort";
    bool const fHostInLongMode   = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);

    if (   (pVCpu->cpum.GstCtx.cr4 & X86_CR4_PAE)
        && !fHostInLongMode)
    {
        uint64_t const uHostCr3 = pVCpu->cpum.GstCtx.cr3 & X86_CR3_PAE_PAGE_MASK;
        X86PDPE aPdptes[X86_PG_PAE_PDPE_ENTRIES];
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), (void *)&aPdptes[0], uHostCr3, sizeof(aPdptes));
        if (RT_SUCCESS(rc))
        {
            for (unsigned iPdpte = 0; iPdpte < RT_ELEMENTS(aPdptes); iPdpte++)
            {
                if (   !(aPdptes[iPdpte].u & X86_PDPE_P)
                    || !(aPdptes[iPdpte].u & X86_PDPE_PAE_MBZ_MASK))
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmexitPdpteRsvd(iPdpte);
                    IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, enmDiag);
                }
            }
        }
        else
            IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_HostPdpteCr3ReadPhys);
    }

    NOREF(pszFailure);
    NOREF(uExitReason);
    return VINF_SUCCESS;
}


/**
 * Loads the host MSRs from the VM-exit auto-load MSRs area as part of VM-exit.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pszInstr    The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmexitLoadHostAutoMsrs(PVMCPU pVCpu, uint32_t uExitReason)
{
    /*
     * Load host MSRs.
     * See Intel spec. 27.6 "Loading MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VMX-abort";

    /*
     * The VM-exit MSR-load area address need not be a valid guest-physical address if the
     * VM-exit MSR load count is 0. If this is the case, bail early without reading it.
     * See Intel spec. 24.7.2 "VM-Exit Controls for MSRs".
     */
    uint32_t const cMsrs = pVmcs->u32ExitMsrLoadCount;
    if (!cMsrs)
        return VINF_SUCCESS;

    /*
     * Verify the MSR auto-load count. Physical CPUs can behave unpredictably if the count
     * is exceeded including possibly raising #MC exceptions during VMX transition. Our
     * implementation causes a VMX-abort followed by a triple-fault.
     */
    bool const fIsMsrCountValid = iemVmxIsAutoMsrCountValid(pVCpu, cMsrs);
    if (fIsMsrCountValid)
    { /* likely */ }
    else
        IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrLoadCount);

    RTGCPHYS const GCPhysAutoMsrArea = pVmcs->u64AddrExitMsrLoad.u;
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), (void *)&pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea),
                                     GCPhysAutoMsrArea, VMX_V_AUTOMSR_AREA_SIZE);
    if (RT_SUCCESS(rc))
    {
        PCVMXAUTOMSR pMsr = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea);
        Assert(pMsr);
        for (uint32_t idxMsr = 0; idxMsr < cMsrs; idxMsr++, pMsr++)
        {
            if (   !pMsr->u32Reserved
                &&  pMsr->u32Msr != MSR_K8_FS_BASE
                &&  pMsr->u32Msr != MSR_K8_GS_BASE
                &&  pMsr->u32Msr != MSR_K6_EFER
                &&  pMsr->u32Msr != MSR_IA32_SMM_MONITOR_CTL
                &&  pMsr->u32Msr >> 8 != MSR_IA32_X2APIC_START >> 8)
            {
                VBOXSTRICTRC rcStrict = CPUMSetGuestMsr(pVCpu, pMsr->u32Msr, pMsr->u64Value);
                if (rcStrict == VINF_SUCCESS)
                    continue;

                /*
                 * If we're in ring-0, we cannot handle returns to ring-3 at this point and continue VM-exit.
                 * If any guest hypervisor loads MSRs that require ring-3 handling, we cause a VMX-abort
                 * recording the MSR index in the auxiliary info. field and indicated further by our
                 * own, specific diagnostic code. Later, we can try implement handling of the MSR in ring-0
                 * if possible, or come up with a better, generic solution.
                 */
                pVCpu->cpum.GstCtx.hwvirt.vmx.uAbortAux = pMsr->u32Msr;
                VMXVDIAG const enmDiag = rcStrict == VINF_CPUM_R3_MSR_WRITE
                                       ? kVmxVDiag_Vmexit_MsrLoadRing3
                                       : kVmxVDiag_Vmexit_MsrLoad;
                IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, enmDiag);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrLoadRsvd);
        }
    }
    else
    {
        AssertMsgFailed(("VM-exit: Failed to read MSR auto-load area at %#RGp, rc=%Rrc\n", GCPhysAutoMsrArea, rc));
        IEM_VMX_VMEXIT_FAILED_RET(pVCpu, uExitReason, pszFailure, kVmxVDiag_Vmexit_MsrLoadPtrReadPhys);
    }

    NOREF(uExitReason);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Loads the host state as part of VM-exit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason (for logging purposes).
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmexitLoadHostState(PVMCPU pVCpu, uint32_t uExitReason)
{
    /*
     * Load host state.
     * See Intel spec. 27.5 "Loading Host State".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fHostInLongMode = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);

    /* We cannot return from a long-mode guest to a host that is not in long mode. */
    if (    CPUMIsGuestInLongMode(pVCpu)
        && !fHostInLongMode)
    {
        Log(("VM-exit from long-mode guest to host not in long-mode -> VMX-Abort\n"));
        return iemVmxAbort(pVCpu, VMXABORT_HOST_NOT_IN_LONG_MODE);
    }

    iemVmxVmexitLoadHostControlRegsMsrs(pVCpu);
    iemVmxVmexitLoadHostSegRegs(pVCpu);

    /*
     * Load host RIP, RSP and RFLAGS.
     * See Intel spec. 27.5.3 "Loading Host RIP, RSP and RFLAGS"
     */
    pVCpu->cpum.GstCtx.rip      = pVmcs->u64HostRip.u;
    pVCpu->cpum.GstCtx.rsp      = pVmcs->u64HostRsp.u;
    pVCpu->cpum.GstCtx.rflags.u = X86_EFL_1;

    /* Update non-register state. */
    iemVmxVmexitRestoreForceFlags(pVCpu);

    /* Clear address range monitoring. */
    EMMonitorWaitClear(pVCpu);

    /* Perform the VMX transition (PGM updates). */
    VBOXSTRICTRC rcStrict = iemVmxWorldSwitch(pVCpu);
    if (rcStrict == VINF_SUCCESS)
    {
        /* Check host PDPTEs (only when we've fully switched page tables_. */
        /** @todo r=ramshankar: I don't know if PGM does this for us already or not... */
        int rc = iemVmxVmexitCheckHostPdptes(pVCpu, uExitReason);
        if (RT_FAILURE(rc))
        {
            Log(("VM-exit failed while restoring host PDPTEs -> VMX-Abort\n"));
            return iemVmxAbort(pVCpu, VMXBOART_HOST_PDPTE);
        }
    }
    else if (RT_SUCCESS(rcStrict))
    {
        Log3(("VM-exit: iemVmxWorldSwitch returns %Rrc (uExitReason=%u) -> Setting passup status\n", VBOXSTRICTRC_VAL(rcStrict),
              uExitReason));
        rcStrict = iemSetPassUpStatus(pVCpu, rcStrict);
    }
    else
    {
        Log3(("VM-exit: iemVmxWorldSwitch failed! rc=%Rrc (uExitReason=%u)\n", VBOXSTRICTRC_VAL(rcStrict), uExitReason));
        return VBOXSTRICTRC_VAL(rcStrict);
    }

    Assert(rcStrict == VINF_SUCCESS);

    /* Load MSRs from the VM-exit auto-load MSR area. */
    int rc = iemVmxVmexitLoadHostAutoMsrs(pVCpu, uExitReason);
    if (RT_FAILURE(rc))
    {
        Log(("VM-exit failed while loading host MSRs -> VMX-Abort\n"));
        return iemVmxAbort(pVCpu, VMXABORT_LOAD_HOST_MSR);
    }

    return rcStrict;
}


/**
 * VMX VM-exit handler.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmexit(PVMCPU pVCpu, uint32_t uExitReason)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pVmcs);

    pVmcs->u32RoExitReason = uExitReason;

    /** @todo NSTVMX: IEMGetCurrentXcpt will be VM-exit interruption info. */
    /** @todo NSTVMX: The source event should be recorded in IDT-vectoring info
     *        during injection. */

    /*
     * Save the guest state back into the VMCS.
     * We only need to save the state when the VM-entry was successful.
     */
    bool const fVmentryFailed = VMX_EXIT_REASON_HAS_ENTRY_FAILED(uExitReason);
    if (!fVmentryFailed)
    {
        iemVmxVmexitSaveGuestState(pVCpu, uExitReason);
        int rc = iemVmxVmexitSaveGuestAutoMsrs(pVCpu, uExitReason);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
            return iemVmxAbort(pVCpu, VMXABORT_SAVE_GUEST_MSRS);
    }

    /*
     * The high bits of the VM-exit reason are only relevant when the VM-exit occurs in
     * enclave mode/SMM which we don't support yet. If we ever add support for it, we can
     * pass just the lower bits, till then an assert should suffice.
     */
    Assert(!RT_HI_U16(uExitReason));

    VBOXSTRICTRC rcStrict = iemVmxVmexitLoadHostState(pVCpu, uExitReason);
    if (RT_FAILURE(rcStrict))
        LogFunc(("Loading host-state failed. uExitReason=%u rc=%Rrc\n", uExitReason, VBOXSTRICTRC_VAL(rcStrict)));

    /* We're no longer in nested-guest execution mode. */
    pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode = false;

    return rcStrict;
}


/**
 * VMX VM-exit handler for VM-exits due to instruction execution.
 *
 * This is intended for instructions where the caller provides all the relevant
 * VM-exit information.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pExitInfo       Pointer to the VM-exit instruction information struct.
 */
DECLINLINE(VBOXSTRICTRC) iemVmxVmexitInstrWithInfo(PVMCPU pVCpu, PCVMXVEXITINFO pExitInfo)
{
    /*
     * For instructions where any of the following fields are not applicable:
     *   - VM-exit instruction info. is undefined.
     *   - VM-exit qualification must be cleared.
     *   - VM-exit guest-linear address is undefined.
     *   - VM-exit guest-physical address is undefined.
     *
     * The VM-exit instruction length is mandatory for all VM-exits that are caused by
     * instruction execution.
     *
     * In our implementation, all undefined fields are generally cleared (caller's
     * responsibility).
     *
     * See Intel spec. 27.2.1 "Basic VM-Exit Information".
     * See Intel spec. 27.2.4 "Information for VM Exits Due to Instruction Execution".
     */
    Assert(pExitInfo);
    AssertMsg(pExitInfo->uReason <= VMX_EXIT_MAX, ("uReason=%u\n", pExitInfo->uReason));
    AssertMsg(pExitInfo->cbInstr >= 1 && pExitInfo->cbInstr <= 15,
              ("uReason=%u cbInstr=%u\n", pExitInfo->uReason, pExitInfo->cbInstr));

    /* Update all the relevant fields from the VM-exit instruction information struct. */
    iemVmxVmcsSetExitInstrInfo(pVCpu, pExitInfo->InstrInfo.u);
    iemVmxVmcsSetExitQual(pVCpu, pExitInfo->u64Qual);
    iemVmxVmcsSetExitGuestLinearAddr(pVCpu, pExitInfo->u64GuestLinearAddr);
    iemVmxVmcsSetExitGuestPhysAddr(pVCpu, pExitInfo->u64GuestPhysAddr);
    iemVmxVmcsSetExitInstrLen(pVCpu, pExitInfo->cbInstr);

    /* Perform the VM-exit. */
    return iemVmxVmexit(pVCpu, pExitInfo->uReason);
}


/**
 * VMX VM-exit handler for VM-exits due to instruction execution.
 *
 * This is intended for instructions that only provide the VM-exit instruction
 * length.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 * @param   cbInstr         The instruction length (in bytes).
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmexitInstr(PVMCPU pVCpu, uint32_t uExitReason, uint8_t cbInstr)
{
    VMXVEXITINFO ExitInfo;
    RT_ZERO(ExitInfo);
    ExitInfo.uReason = uExitReason;
    ExitInfo.cbInstr = cbInstr;

#ifdef VBOX_STRICT
    /* To prevent us from shooting ourselves in the foot. Maybe remove later. */
    switch (uExitReason)
    {
        case VMX_EXIT_INVEPT:
        case VMX_EXIT_INVPCID:
        case VMX_EXIT_LDTR_TR_ACCESS:
        case VMX_EXIT_GDTR_IDTR_ACCESS:
        case VMX_EXIT_VMCLEAR:
        case VMX_EXIT_VMPTRLD:
        case VMX_EXIT_VMPTRST:
        case VMX_EXIT_VMREAD:
        case VMX_EXIT_VMWRITE:
        case VMX_EXIT_VMXON:
        case VMX_EXIT_XRSTORS:
        case VMX_EXIT_XSAVES:
        case VMX_EXIT_RDRAND:
        case VMX_EXIT_RDSEED:
        case VMX_EXIT_IO_INSTR:
            AssertMsgFailedReturn(("Use iemVmxVmexitInstrNeedsInfo for uExitReason=%u\n", uExitReason), VERR_IEM_IPE_5);
            break;
    }
#endif

    return iemVmxVmexitInstrWithInfo(pVCpu, &ExitInfo);
}


/**
 * VMX VM-exit handler for VM-exits due to instruction execution.
 *
 * This is intended for instructions that have a ModR/M byte and update the VM-exit
 * instruction information and VM-exit qualification fields.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 * @param   uInstrid        The instruction identity (VMXINSTRID_XXX).
 * @param   cbInstr         The instruction length (in bytes).
 *
 * @remarks Do not use this for INS/OUTS instruction.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmexitInstrNeedsInfo(PVMCPU pVCpu, uint32_t uExitReason, VMXINSTRID uInstrId, uint8_t cbInstr)
{
    VMXVEXITINFO ExitInfo;
    RT_ZERO(ExitInfo);
    ExitInfo.uReason = uExitReason;
    ExitInfo.cbInstr = cbInstr;

    /*
     * Update the VM-exit qualification field with displacement bytes.
     * See Intel spec. 27.2.1 "Basic VM-Exit Information".
     */
    switch (uExitReason)
    {
        case VMX_EXIT_INVEPT:
        case VMX_EXIT_INVPCID:
        case VMX_EXIT_LDTR_TR_ACCESS:
        case VMX_EXIT_GDTR_IDTR_ACCESS:
        case VMX_EXIT_VMCLEAR:
        case VMX_EXIT_VMPTRLD:
        case VMX_EXIT_VMPTRST:
        case VMX_EXIT_VMREAD:
        case VMX_EXIT_VMWRITE:
        case VMX_EXIT_VMXON:
        case VMX_EXIT_XRSTORS:
        case VMX_EXIT_XSAVES:
        case VMX_EXIT_RDRAND:
        case VMX_EXIT_RDSEED:
        {
            /* Construct the VM-exit instruction information. */
            RTGCPTR GCPtrDisp;
            uint32_t const uInstrInfo = iemVmxGetExitInstrInfo(pVCpu, uExitReason, uInstrId, &GCPtrDisp);

            /* Update the VM-exit instruction information. */
            ExitInfo.InstrInfo.u = uInstrInfo;

            /* Update the VM-exit qualification. */
            ExitInfo.u64Qual = GCPtrDisp;
            break;
        }

        default:
            AssertMsgFailedReturn(("Use instruction-specific handler\n"), VERR_IEM_IPE_5);
            break;
    }

    return iemVmxVmexitInstrWithInfo(pVCpu, &ExitInfo);
}


/**
 * VMX VM-exit handler for VM-exits due to INVLPG.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   GCPtrPage       The guest-linear address of the page being invalidated.
 * @param   cbInstr         The instruction length (in bytes).
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmexitInstrInvlpg(PVMCPU pVCpu, RTGCPTR GCPtrPage, uint8_t cbInstr)
{
    VMXVEXITINFO ExitInfo;
    RT_ZERO(ExitInfo);
    ExitInfo.uReason = VMX_EXIT_INVLPG;
    ExitInfo.cbInstr = cbInstr;
    ExitInfo.u64Qual = GCPtrPage;
    Assert(IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode || !RT_HI_U32(ExitInfo.u64Qual));

    return iemVmxVmexitInstrWithInfo(pVCpu, &ExitInfo);
}


/**
 * Checks guest control registers, debug registers and MSRs as part of VM-entry.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pszInstr    The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestControlRegsMsrs(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Guest Control Registers, Debug Registers, and MSRs.
     * See Intel spec. 26.3.1.1 "Checks on Guest Control Registers, Debug Registers, and MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure  = "VM-exit";
    bool const fUnrestrictedGuest = RT_BOOL(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST);

    /* CR0 reserved bits. */
    {
        /* CR0 MB1 bits. */
        uint64_t u64Cr0Fixed0 = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
        Assert(!(u64Cr0Fixed0 & (X86_CR0_NW | X86_CR0_CD)));
        if (fUnrestrictedGuest)
            u64Cr0Fixed0 &= ~(X86_CR0_PE | X86_CR0_PG);
        if ((pVmcs->u64GuestCr0.u & u64Cr0Fixed0) != u64Cr0Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0Fixed0);

        /* CR0 MBZ bits. */
        uint64_t const u64Cr0Fixed1 = CPUMGetGuestIa32VmxCr0Fixed1(pVCpu);
        if (pVmcs->u64GuestCr0.u & ~u64Cr0Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0Fixed1);

        /* Without unrestricted guest support, VT-x supports does not support unpaged protected mode. */
        if (   !fUnrestrictedGuest
            &&  (pVmcs->u64GuestCr0.u & X86_CR0_PG)
            && !(pVmcs->u64GuestCr0.u & X86_CR0_PE))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0PgPe);
    }

    /* CR4 reserved bits. */
    {
        /* CR4 MB1 bits. */
        uint64_t const u64Cr4Fixed0 = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
        if ((pVmcs->u64GuestCr4.u & u64Cr4Fixed0) != u64Cr4Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr4Fixed0);

        /* CR4 MBZ bits. */
        uint64_t const u64Cr4Fixed1 = CPUMGetGuestIa32VmxCr4Fixed1(pVCpu);
        if (pVmcs->u64GuestCr4.u & ~u64Cr4Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr4Fixed1);
    }

    /* DEBUGCTL MSR. */
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_DEBUG)
        && (pVmcs->u64GuestDebugCtlMsr.u & ~MSR_IA32_DEBUGCTL_VALID_MASK_INTEL))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestDebugCtl);

    /* 64-bit CPU checks. */
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        if (fGstInLongMode)
        {
            /* PAE must be set. */
            if (   (pVmcs->u64GuestCr0.u & X86_CR0_PG)
                && (pVmcs->u64GuestCr0.u & X86_CR4_PAE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPae);
        }
        else
        {
            /* PCIDE should not be set. */
            if (!(pVmcs->u64GuestCr4.u & X86_CR4_PCIDE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPcide);
        }

        /* CR3. */
        if (!(pVmcs->u64GuestCr3.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr3);

        /* DR7. */
        if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_DEBUG)
            && (pVmcs->u64GuestDr7.u & X86_DR7_MBZ_MASK))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestDr7);

        /* SYSENTER ESP and SYSENTER EIP. */
        if (   X86_IS_CANONICAL(pVmcs->u64GuestSysenterEsp.u)
            && X86_IS_CANONICAL(pVmcs->u64GuestSysenterEip.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSysenterEspEip);
    }

    /* We don't support IA32_PERF_GLOBAL_CTRL MSR yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PERF_MSR));

    /* PAT MSR. */
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PAT_MSR)
        && !CPUMIsPatMsrValid(pVmcs->u64GuestPatMsr.u))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPatMsr);

    /* EFER MSR. */
    uint64_t const uValidEferMask = CPUMGetGuestEferMsrValidMask(pVCpu->CTX_SUFF(pVM));
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_EFER_MSR)
        && (pVmcs->u64GuestEferMsr.u & ~uValidEferMask))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestEferMsrRsvd);

    bool const fGstLma = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LMA);
    bool const fGstLme = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LME);
    if (   fGstInLongMode == fGstLma
        && (   !(pVmcs->u64GuestCr0.u & X86_CR0_PG)
            || fGstLma == fGstLme))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestEferMsr);

    /* We don't support IA32_BNDCFGS MSR yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_BNDCFGS_MSR));

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest segment registers, LDTR and TR as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestSegRegs(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Segment registers.
     * See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure  = "VM-exit";
    bool const fGstInV86Mode      = RT_BOOL(pVmcs->u64GuestRFlags.u & X86_EFL_VM);
    bool const fUnrestrictedGuest = RT_BOOL(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST);
    bool const fGstInLongMode     = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);

    /* Selectors. */
    if (   !fGstInV86Mode
        && !fUnrestrictedGuest
        && (pVmcs->GuestSs & X86_SEL_RPL) != (pVmcs->GuestCs & X86_SEL_RPL))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelCsSsRpl);

    for (unsigned iSegReg = 0; iSegReg < X86_SREG_COUNT; iSegReg++)
    {
        CPUMSELREG SelReg;
        int rc = iemVmxVmcsGetGuestSegReg(pVmcs, iSegReg, &SelReg);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        { /* likely */ }
        else
            return rc;

        /*
         * Virtual-8086 mode checks.
         */
        if (fGstInV86Mode)
        {
            /* Base address. */
            if (SelReg.u64Base == (uint64_t)SelReg.Sel << 4)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBaseV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Limit. */
            if (SelReg.u32Limit == 0xffff)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegLimitV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Attribute. */
            if (SelReg.Attr.u == 0xf3)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* We're done; move to checking the next segment. */
            continue;
        }

        /* Checks done by 64-bit CPUs. */
        if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
        {
            /* Base address. */
            if (   iSegReg == X86_SREG_FS
                || iSegReg == X86_SREG_GS)
            {
                if (X86_IS_CANONICAL(SelReg.u64Base))
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBase(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }
            }
            else if (iSegReg == X86_SREG_CS)
            {
                if (!RT_HI_U32(SelReg.u64Base))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseCs);
            }
            else
            {
                if (   SelReg.Attr.n.u1Unusable
                    || !RT_HI_U32(SelReg.u64Base))
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBase(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }
            }
        }

        /*
         * Checks outside Virtual-8086 mode.
         */
        uint8_t const uSegType     =  SelReg.Attr.n.u4Type;
        uint8_t const fCodeDataSeg =  SelReg.Attr.n.u1DescType;
        uint8_t const fUsable      = !SelReg.Attr.n.u1Unusable;
        uint8_t const uDpl         =  SelReg.Attr.n.u2Dpl;
        uint8_t const fPresent     =  SelReg.Attr.n.u1Present;
        uint8_t const uGranularity =  SelReg.Attr.n.u1Granularity;
        uint8_t const uDefBig      =  SelReg.Attr.n.u1DefBig;
        uint8_t const fSegLong     =  SelReg.Attr.n.u1Long;

        /* Code or usable segment. */
        if (   iSegReg == X86_SREG_CS
            || fUsable)
        {
            /* Reserved bits (bits 31:17 and bits 11:8). */
            if (!(SelReg.Attr.u & 0xfffe0f00))
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrRsvd(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Descriptor type. */
            if (fCodeDataSeg)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrDescType(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Present. */
            if (fPresent)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrPresent(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Granularity. */
            if (   ((SelReg.u32Limit & 0x00000fff) == 0x00000fff || !uGranularity)
                && ((SelReg.u32Limit & 0xfff00000) == 0x00000000 ||  uGranularity))
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrGran(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }
        }

        if (iSegReg == X86_SREG_CS)
        {
            /* Segment Type and DPL. */
            if (   uSegType == (X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED)
                && fUnrestrictedGuest)
            {
                if (uDpl == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplZero);
            }
            else if (   uSegType == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
                     || uSegType == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_ACCESSED))
            {
                X86DESCATTR AttrSs; AttrSs.u = pVmcs->u32GuestSsAttr;
                if (uDpl == AttrSs.n.u2Dpl)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplEqSs);
            }
            else if ((uSegType & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF | X86_SEL_TYPE_ACCESSED))
                              == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF | X86_SEL_TYPE_ACCESSED))
            {
                X86DESCATTR AttrSs; AttrSs.u = pVmcs->u32GuestSsAttr;
                if (uDpl <= AttrSs.n.u2Dpl)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplLtSs);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsType);

            /* Def/Big. */
            if (   fGstInLongMode
                && fSegLong)
            {
                if (uDefBig == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDefBig);
            }
        }
        else if (iSegReg == X86_SREG_SS)
        {
            /* Segment Type. */
            if (   !fUsable
                || uSegType == (X86_SEL_TYPE_RW   | X86_SEL_TYPE_ACCESSED)
                || uSegType == (X86_SEL_TYPE_DOWN | X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsType);

            /* DPL. */
            if (fUnrestrictedGuest)
            {
                if (uDpl == (SelReg.Sel & X86_SEL_RPL))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsDplEqRpl);
            }
            X86DESCATTR AttrCs; AttrCs.u = pVmcs->u32GuestCsAttr;
            if (   AttrCs.n.u4Type == (X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED)
                || (pVmcs->u64GuestCr0.u & X86_CR0_PE))
            {
                if (uDpl == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsDplZero);
            }
        }
        else
        {
            /* DS, ES, FS, GS. */
            if (fUsable)
            {
                /* Segment type. */
                if (uSegType & X86_SEL_TYPE_ACCESSED)
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrTypeAcc(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }

                if (   !(uSegType & X86_SEL_TYPE_CODE)
                    ||  (uSegType & X86_SEL_TYPE_READ))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsTypeRead);

                /* DPL. */
                if (   !fUnrestrictedGuest
                    && uSegType <= (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_ACCESSED))
                {
                    if (uDpl >= (SelReg.Sel & X86_SEL_RPL))
                    { /* likely */ }
                    else
                    {
                        VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrDplRpl(iSegReg);
                        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                    }
                }
            }
        }
    }

    /*
     * LDTR.
     */
    {
        CPUMSELREG Ldtr;
        Ldtr.Sel      = pVmcs->GuestLdtr;
        Ldtr.u32Limit = pVmcs->u32GuestLdtrLimit;
        Ldtr.u64Base  = pVmcs->u64GuestLdtrBase.u;
        Ldtr.Attr.u   = pVmcs->u32GuestLdtrLimit;

        if (!Ldtr.Attr.n.u1Unusable)
        {
            /* Selector. */
            if (!(Ldtr.Sel & X86_SEL_LDT))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelLdtr);

            /* Base. */
            if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
            {
                if (X86_IS_CANONICAL(Ldtr.u64Base))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseLdtr);
            }

            /* Attributes. */
            /* Reserved bits (bits 31:17 and bits 11:8). */
            if (!(Ldtr.Attr.u & 0xfffe0f00))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrRsvd);

            if (Ldtr.Attr.n.u4Type == X86_SEL_TYPE_SYS_LDT)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrType);

            if (!Ldtr.Attr.n.u1DescType)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrDescType);

            if (Ldtr.Attr.n.u1Present)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrPresent);

            if (   ((Ldtr.u32Limit & 0x00000fff) == 0x00000fff || !Ldtr.Attr.n.u1Granularity)
                && ((Ldtr.u32Limit & 0xfff00000) == 0x00000000 ||  Ldtr.Attr.n.u1Granularity))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrGran);
        }
    }

    /*
     * TR.
     */
    {
        CPUMSELREG Tr;
        Tr.Sel      = pVmcs->GuestTr;
        Tr.u32Limit = pVmcs->u32GuestTrLimit;
        Tr.u64Base  = pVmcs->u64GuestTrBase.u;
        Tr.Attr.u   = pVmcs->u32GuestTrLimit;

        /* Selector. */
        if (!(Tr.Sel & X86_SEL_LDT))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelTr);

        /* Base. */
        if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
        {
            if (X86_IS_CANONICAL(Tr.u64Base))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseTr);
        }

        /* Attributes. */
        /* Reserved bits (bits 31:17 and bits 11:8). */
        if (!(Tr.Attr.u & 0xfffe0f00))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrRsvd);

        if (!Tr.Attr.n.u1Unusable)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrUnusable);

        if (   Tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY
            || (   !fGstInLongMode
                && Tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrType);

        if (!Tr.Attr.n.u1DescType)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrDescType);

        if (Tr.Attr.n.u1Present)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrPresent);

        if (   ((Tr.u32Limit & 0x00000fff) == 0x00000fff || !Tr.Attr.n.u1Granularity)
            && ((Tr.u32Limit & 0xfff00000) == 0x00000000 ||  Tr.Attr.n.u1Granularity))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrGran);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest GDTR and IDTR as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestGdtrIdtr(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * GDTR and IDTR.
     * See Intel spec. 26.3.1.3 "Checks on Guest Descriptor-Table Registers".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* Base. */
        if (X86_IS_CANONICAL(pVmcs->u64GuestGdtrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestGdtrBase);

        if (X86_IS_CANONICAL(pVmcs->u64GuestIdtrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIdtrBase);
    }

    /* Limit. */
    if (!RT_HI_U16(pVmcs->u32GuestGdtrLimit))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestGdtrLimit);

    if (!RT_HI_U16(pVmcs->u32GuestIdtrLimit))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIdtrLimit);

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest RIP and RFLAGS as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestRipRFlags(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * RIP and RFLAGS.
     * See Intel spec. 26.3.1.4 "Checks on Guest RIP and RFLAGS".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";
    bool const fGstInLongMode    = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);

    /* RIP. */
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        X86DESCATTR AttrCs; AttrCs.u = pVmcs->u32GuestCsAttr;
        if (   !fGstInLongMode
            || !AttrCs.n.u1Long)
        {
            if (!RT_HI_U32(pVmcs->u64GuestRip.u))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRipRsvd);
        }

        if (   fGstInLongMode
            && AttrCs.n.u1Long)
        {
            Assert(IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxLinearAddrWidth == 48);   /* Canonical. */
            if (   IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxLinearAddrWidth < 64
                && X86_IS_CANONICAL(pVmcs->u64GuestRip.u))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRip);
        }
    }

    /* RFLAGS (bits 63:22 (or 31:22), bits 15, 5, 3 are reserved, bit 1 MB1). */
    uint64_t const uGuestRFlags = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode ? pVmcs->u64GuestRFlags.u
                                : pVmcs->u64GuestRFlags.s.Lo;
    if (   !(uGuestRFlags & ~(X86_EFL_LIVE_MASK | X86_EFL_RA1_MASK))
        &&  (uGuestRFlags & X86_EFL_RA1_MASK) == X86_EFL_RA1_MASK)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsRsvd);

    if (   fGstInLongMode
        || !(pVmcs->u64GuestCr0.u & X86_CR0_PE))
    {
        if (!(uGuestRFlags & X86_EFL_VM))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsVm);
    }

    if (   VMX_ENTRY_INT_INFO_IS_VALID(pVmcs->u32EntryIntInfo)
        && VMX_ENTRY_INT_INFO_TYPE(pVmcs->u32EntryIntInfo) == VMX_ENTRY_INT_INFO_TYPE_EXT_INT)
    {
        if (uGuestRFlags & X86_EFL_IF)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsIf);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest non-register state as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestNonRegState(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * Guest non-register state.
     * See Intel spec. 26.3.1.5 "Checks on Guest Non-Register State".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";

    /*
     * Activity state.
     */
    uint64_t const u64GuestVmxMiscMsr = CPUMGetGuestIa32VmxMisc(pVCpu);
    uint32_t const fActivityStateMask = RT_BF_GET(u64GuestVmxMiscMsr, VMX_BF_MISC_ACTIVITY_STATES);
    if (!(pVmcs->u32GuestActivityState & fActivityStateMask))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateRsvd);

    X86DESCATTR AttrSs; AttrSs.u = pVmcs->u32GuestSsAttr;
    if (   !AttrSs.n.u2Dpl
        || pVmcs->u32GuestActivityState != VMX_VMCS_GUEST_ACTIVITY_HLT)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateSsDpl);

    if (   pVmcs->u32GuestIntrState == VMX_VMCS_GUEST_INT_STATE_BLOCK_STI
        || pVmcs->u32GuestIntrState == VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS)
    {
        if (pVmcs->u32GuestActivityState == VMX_VMCS_GUEST_ACTIVITY_ACTIVE)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateStiMovSs);
    }

    if (VMX_ENTRY_INT_INFO_IS_VALID(pVmcs->u32EntryIntInfo))
    {
        uint8_t const uIntType = VMX_ENTRY_INT_INFO_TYPE(pVmcs->u32EntryIntInfo);
        uint8_t const uVector  = VMX_ENTRY_INT_INFO_VECTOR(pVmcs->u32EntryIntInfo);
        AssertCompile(VMX_V_GUEST_ACTIVITY_STATE_MASK == (VMX_VMCS_GUEST_ACTIVITY_HLT | VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN));
        switch (pVmcs->u32GuestActivityState)
        {
            case VMX_VMCS_GUEST_ACTIVITY_HLT:
            {
                if (   uIntType == VMX_ENTRY_INT_INFO_TYPE_EXT_INT
                    || uIntType == VMX_ENTRY_INT_INFO_TYPE_NMI
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                        && (   uVector == X86_XCPT_DB
                            || uVector == X86_XCPT_MC))
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT
                        && uVector  == VMX_ENTRY_INT_INFO_VECTOR_MTF))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateHlt);
                break;
            }

            case VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN:
            {
                if (   uIntType == VMX_ENTRY_INT_INFO_TYPE_NMI
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                        && uVector == X86_XCPT_MC))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateShutdown);
                break;
            }

            case VMX_VMCS_GUEST_ACTIVITY_ACTIVE:
            default:
                break;
        }
    }

    /*
     * Interruptibility state.
     */
    if (!(pVmcs->u32GuestIntrState & ~VMX_VMCS_GUEST_INT_STATE_MASK))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateRsvd);

    if ((pVmcs->u32GuestIntrState & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS | VMX_VMCS_GUEST_INT_STATE_BLOCK_STI))
                                 != (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS | VMX_VMCS_GUEST_INT_STATE_BLOCK_STI))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateStiMovSs);

    if (    (pVmcs->u64GuestRFlags.u & X86_EFL_IF)
        || !(pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_BLOCK_STI))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateRFlagsSti);

    if (VMX_ENTRY_INT_INFO_IS_VALID(pVmcs->u32EntryIntInfo))
    {
        uint8_t const uIntType = VMX_ENTRY_INT_INFO_TYPE(pVmcs->u32EntryIntInfo);
        if (uIntType == VMX_ENTRY_INT_INFO_TYPE_EXT_INT)
        {
            if (!(pVmcs->u32GuestIntrState & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS | VMX_VMCS_GUEST_INT_STATE_BLOCK_STI)))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateExtInt);
        }
        else if (uIntType == VMX_ENTRY_INT_INFO_TYPE_NMI)
        {
            if (!(pVmcs->u32GuestIntrState & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS | VMX_VMCS_GUEST_INT_STATE_BLOCK_STI)))
            { /* likely */ }
            else
            {
                /*
                 * We don't support injecting NMIs when blocking-by-STI would be in effect.
                 * We update the VM-exit qualification only when blocking-by-STI is set
                 * without blocking-by-MovSS being set. Although in practise it  does not
                 * make much difference since the order of checks are implementation defined.
                 */
                if (!(pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS))
                    iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_NMI_INJECT);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateNmi);
            }

            if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI)
                || !(pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_BLOCK_NMI))
            { /* likely */ }
            else
               IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateVirtNmi);
        }
    }

    /* We don't support SMM yet. So blocking-by-SMIs must not be set. */
    if (!(pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_BLOCK_SMI))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateSmi);

    /* We don't support SGX yet. So enclave-interruption must not be set. */
    if (!(pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_ENCLAVE))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIntStateEnclave);

    /*
     * Pending debug exceptions.
     */
    uint64_t const uPendingDbgXcpt = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode
                                   ? pVmcs->u64GuestPendingDbgXcpt.u
                                   : pVmcs->u64GuestPendingDbgXcpt.s.Lo;
    if (!(uPendingDbgXcpt & ~VMX_VMCS_GUEST_PENDING_DEBUG_VALID_MASK))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPndDbgXcptRsvd);

    if (   (pVmcs->u32GuestIntrState & (VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS | VMX_VMCS_GUEST_INT_STATE_BLOCK_STI))
        || pVmcs->u32GuestActivityState == VMX_VMCS_GUEST_ACTIVITY_HLT)
    {
        if (   (pVmcs->u64GuestRFlags.u & X86_EFL_TF)
            && !(pVmcs->u64GuestDebugCtlMsr.u & MSR_IA32_DEBUGCTL_BTF)
            && !(uPendingDbgXcpt & VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BS))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPndDbgXcptBsTf);

        if (   (   !(pVmcs->u64GuestRFlags.u & X86_EFL_TF)
                ||  (pVmcs->u64GuestDebugCtlMsr.u & MSR_IA32_DEBUGCTL_BTF))
            && (uPendingDbgXcpt & VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BS))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPndDbgXcptBsNoTf);
    }

    /* We don't support RTM (Real-time Transactional Memory) yet. */
    if (uPendingDbgXcpt & VMX_VMCS_GUEST_PENDING_DEBUG_RTM)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPndDbgXcptRtm);

    /*
     * VMCS link pointer.
     */
    if (pVmcs->u64VmcsLinkPtr.u != UINT64_C(0xffffffffffffffff))
    {
        RTGCPHYS const GCPhysShadowVmcs = pVmcs->u64VmcsLinkPtr.u;
        /* We don't support SMM yet (so VMCS link pointer cannot be the current VMCS). */
        if (GCPhysShadowVmcs != IEM_VMX_GET_CURRENT_VMCS(pVCpu))
        { /* likely */ }
        else
        {
            iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmcsLinkPtrCurVmcs);
        }

        /* Validate the address. */
        if (   (GCPhysShadowVmcs & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysShadowVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysShadowVmcs))
        {
            iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVmcsLinkPtr);
        }

        /* Read the VMCS-link pointer from guest memory. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs),
                                         GCPhysShadowVmcs, VMX_V_VMCS_SIZE);
        if (RT_FAILURE(rc))
        {
            iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmcsLinkPtrReadPhys);
        }

        /* Verify the VMCS revision specified by the guest matches what we reported to the guest. */
        if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs)->u32VmcsRevId.n.u31RevisionId == VMX_V_VMCS_REVISION_ID)
        { /* likely */ }
        else
        {
            iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmcsLinkPtrRevId);
        }

        /* Verify the shadow bit is set if VMCS shadowing is enabled . */
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VMCS_SHADOWING)
            || pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs)->u32VmcsRevId.n.fIsShadowVmcs)
        { /* likely */ }
        else
        {
            iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmcsLinkPtrShadow);
        }

        /* Finally update our cache of the guest physical address of the shadow VMCS. */
        pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysShadowVmcs = GCPhysShadowVmcs;
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks if the PDPTEs referenced by the nested-guest CR3 are valid as part of
 * VM-entry.
 *
 * @returns @c true if all PDPTEs are valid, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pszInstr    The VMX instruction name (for logging purposes).
 * @param   pVmcs       Pointer to the virtual VMCS.
 */
IEM_STATIC int iemVmxVmentryCheckGuestPdptesForCr3(PVMCPU pVCpu, const char *pszInstr, PVMXVVMCS pVmcs)
{
    /*
     * Check PDPTEs.
     * See Intel spec. 4.4.1 "PDPTE Registers".
     */
    uint64_t const uGuestCr3 = pVmcs->u64GuestCr3.u & X86_CR3_PAE_PAGE_MASK;
    const char *const pszFailure = "VM-exit";

    X86PDPE aPdptes[X86_PG_PAE_PDPE_ENTRIES];
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), (void *)&aPdptes[0], uGuestCr3, sizeof(aPdptes));
    if (RT_SUCCESS(rc))
    {
        for (unsigned iPdpte = 0; iPdpte < RT_ELEMENTS(aPdptes); iPdpte++)
        {
            if (   !(aPdptes[iPdpte].u & X86_PDPE_P)
                || !(aPdptes[iPdpte].u & X86_PDPE_PAE_MBZ_MASK))
            { /* likely */ }
            else
            {
                iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_PDPTE);
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentryPdpteRsvd(iPdpte);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }
        }
    }
    else
    {
        iemVmxVmcsSetExitQual(pVCpu, VMX_ENTRY_FAIL_QUAL_PDPTE);
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPdpteCr3ReadPhys);
    }

    NOREF(pszFailure);
    return rc;
}


/**
 * Checks guest PDPTEs as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestPdptes(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Guest PDPTEs.
     * See Intel spec. 26.3.1.5 "Checks on Guest Page-Directory-Pointer-Table Entries".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);

    /* Check PDPTes if the VM-entry is to a guest using PAE paging. */
    int rc;
    if (   !fGstInLongMode
        && (pVmcs->u64GuestCr4.u & X86_CR4_PAE)
        && (pVmcs->u64GuestCr0.u & X86_CR0_PG))
    {
        /*
         * We don't support nested-paging for nested-guests yet.
         *
         * Without nested-paging for nested-guests, PDPTEs in the VMCS are not used,
         * rather we need to check the PDPTEs referenced by the guest CR3.
         */
        rc = iemVmxVmentryCheckGuestPdptesForCr3(pVCpu, pszInstr, pVmcs);
    }
    else
        rc = VINF_SUCCESS;
    return rc;
}


/**
 * Checks guest-state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestState(PVMCPU pVCpu, const char *pszInstr)
{
    int rc = iemVmxVmentryCheckGuestControlRegsMsrs(pVCpu, pszInstr);
    if (RT_SUCCESS(rc))
    {
        rc = iemVmxVmentryCheckGuestSegRegs(pVCpu, pszInstr);
        if (RT_SUCCESS(rc))
        {
            rc = iemVmxVmentryCheckGuestGdtrIdtr(pVCpu, pszInstr);
            if (RT_SUCCESS(rc))
            {
                rc = iemVmxVmentryCheckGuestRipRFlags(pVCpu, pszInstr);
                if (RT_SUCCESS(rc))
                {
                    rc = iemVmxVmentryCheckGuestNonRegState(pVCpu, pszInstr);
                    if (RT_SUCCESS(rc))
                        return iemVmxVmentryCheckGuestPdptes(pVCpu, pszInstr);
                }
            }
        }
    }
    return rc;
}


/**
 * Checks host-state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckHostState(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Host Control Registers and MSRs.
     * See Intel spec. 26.2.2 "Checks on Host Control Registers and MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* CR0 reserved bits. */
    {
        /* CR0 MB1 bits. */
        uint64_t const u64Cr0Fixed0 = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
        if ((pVmcs->u64HostCr0.u & u64Cr0Fixed0) != u64Cr0Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr0Fixed0);

        /* CR0 MBZ bits. */
        uint64_t const u64Cr0Fixed1 = CPUMGetGuestIa32VmxCr0Fixed1(pVCpu);
        if (pVmcs->u64HostCr0.u & ~u64Cr0Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr0Fixed1);
    }

    /* CR4 reserved bits. */
    {
        /* CR4 MB1 bits. */
        uint64_t const u64Cr4Fixed0 = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
        if ((pVmcs->u64HostCr4.u & u64Cr4Fixed0) != u64Cr4Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Fixed0);

        /* CR4 MBZ bits. */
        uint64_t const u64Cr4Fixed1 = CPUMGetGuestIa32VmxCr4Fixed1(pVCpu);
        if (pVmcs->u64HostCr4.u & ~u64Cr4Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Fixed1);
    }

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* CR3 reserved bits. */
        if (!(pVmcs->u64HostCr3.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr3);

        /* SYSENTER ESP and SYSENTER EIP. */
        if (   X86_IS_CANONICAL(pVmcs->u64HostSysenterEsp.u)
            && X86_IS_CANONICAL(pVmcs->u64HostSysenterEip.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSysenterEspEip);
    }

    /* We don't support IA32_PERF_GLOBAL_CTRL MSR yet. */
    Assert(!(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_PERF_MSR));

    /* PAT MSR. */
    if (   !(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_PAT_MSR)
        ||  CPUMIsPatMsrValid(pVmcs->u64HostPatMsr.u))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostPatMsr);

    /* EFER MSR. */
    uint64_t const uValidEferMask = CPUMGetGuestEferMsrValidMask(pVCpu->CTX_SUFF(pVM));
    if (   !(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_EFER_MSR)
        || !(pVmcs->u64HostEferMsr.u & ~uValidEferMask))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostEferMsrRsvd);

    bool const fHostInLongMode = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);
    bool const fHostLma        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LMA);
    bool const fHostLme        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LME);
    if (   fHostInLongMode == fHostLma
        && fHostInLongMode == fHostLme)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostEferMsr);

    /*
     * Host Segment and Descriptor-Table Registers.
     * See Intel spec. 26.2.3 "Checks on Host Segment and Descriptor-Table Registers".
     */
    /* Selector RPL and TI. */
    if (   !(pVmcs->HostCs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostSs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostDs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostEs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostFs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostGs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostTr & (X86_SEL_RPL | X86_SEL_LDT)))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSel);

    /* CS and TR selectors cannot be 0. */
    if (   pVmcs->HostCs
        && pVmcs->HostTr)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCsTr);

    /* SS cannot be 0 if 32-bit host. */
    if (   fHostInLongMode
        || pVmcs->HostSs)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSs);

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* FS, GS, GDTR, IDTR, TR base address. */
        if (   X86_IS_CANONICAL(pVmcs->u64HostFsBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostFsBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostGdtrBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostIdtrBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostTrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSegBase);
    }

    /*
     * Host address-space size for 64-bit CPUs.
     * See Intel spec. 26.2.4 "Checks Related to Address-Space Size".
     */
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        bool const fCpuInLongMode = CPUMIsGuestInLongMode(pVCpu);

        /* Logical processor in IA-32e mode. */
        if (fCpuInLongMode)
        {
            if (fHostInLongMode)
            {
                /* PAE must be set. */
                if (pVmcs->u64HostCr4.u & X86_CR4_PAE)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Pae);

                /* RIP must be canonical. */
                if (X86_IS_CANONICAL(pVmcs->u64HostRip.u))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostRip);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostLongMode);
        }
        else
        {
            /* Logical processor is outside IA-32e mode. */
            if (   !fGstInLongMode
                && !fHostInLongMode)
            {
                /* PCIDE should not be set. */
                if (!(pVmcs->u64HostCr4.u & X86_CR4_PCIDE))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Pcide);

                /* The high 32-bits of RIP MBZ. */
                if (!pVmcs->u64HostRip.s.Hi)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostRipRsvd);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostGuestLongMode);
        }
    }
    else
    {
        /* Host address-space size for 32-bit CPUs. */
        if (   !fGstInLongMode
            && !fHostInLongMode)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostGuestLongModeNoCpu);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-entry controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.3 "VM-Entry Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckEntryCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* VM-entry controls. */
    VMXCTLSMSR EntryCtls;
    EntryCtls.u = CPUMGetGuestIa32VmxEntryCtls(pVCpu);
    if (~pVmcs->u32EntryCtls & EntryCtls.n.disallowed0)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryCtlsDisallowed0);

    if (pVmcs->u32EntryCtls & ~EntryCtls.n.allowed1)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryCtlsAllowed1);

    /* Event injection. */
    uint32_t const uIntInfo = pVmcs->u32EntryIntInfo;
    if (RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_VALID))
    {
        /* Type and vector. */
        uint8_t const uType   = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_TYPE);
        uint8_t const uVector = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_VECTOR);
        uint8_t const uRsvd   = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_RSVD_12_30);
        if (   !uRsvd
            && HMVmxIsEntryIntInfoTypeValid(IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxMonitorTrapFlag, uType)
            && HMVmxIsEntryIntInfoVectorValid(uVector, uType))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoTypeVecRsvd);

        /* Exception error code. */
        if (RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_ERR_CODE_VALID))
        {
            /* Delivery possible only in Unrestricted-guest mode when CR0.PE is set. */
            if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)
                ||  (pVmcs->u64GuestCr0.s.Lo & X86_CR0_PE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoErrCodePe);

            /* Exceptions that provide an error code. */
            if (   uType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                && (   uVector == X86_XCPT_DF
                    || uVector == X86_XCPT_TS
                    || uVector == X86_XCPT_NP
                    || uVector == X86_XCPT_SS
                    || uVector == X86_XCPT_GP
                    || uVector == X86_XCPT_PF
                    || uVector == X86_XCPT_AC))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoErrCodeVec);

            /* Exception error-code reserved bits. */
            if (!(pVmcs->u32EntryXcptErrCode & ~VMX_ENTRY_INT_XCPT_ERR_CODE_VALID_MASK))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryXcptErrCodeRsvd);

            /* Injecting a software interrupt, software exception or privileged software exception. */
            if (   uType == VMX_ENTRY_INT_INFO_TYPE_SW_INT
                || uType == VMX_ENTRY_INT_INFO_TYPE_SW_XCPT
                || uType == VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT)
            {
                /* Instruction length must be in the range 0-15. */
                if (pVmcs->u32EntryInstrLen <= VMX_ENTRY_INSTR_LEN_MAX)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryInstrLen);

                /* Instruction length of 0 is allowed only when its CPU feature is present. */
                if (   pVmcs->u32EntryInstrLen == 0
                    && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxEntryInjectSoftInt)
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryInstrLenZero);
            }
        }
    }

    /* VM-entry MSR-load count and VM-entry MSR-load area address. */
    if (pVmcs->u32EntryMsrLoadCount)
    {
        if (   (pVmcs->u64AddrEntryMsrLoad.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrEntryMsrLoad.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrEntryMsrLoad.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrEntryMsrLoad);
    }

    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_ENTRY_TO_SMM));           /* We don't support SMM yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_DEACTIVATE_DUAL_MON));    /* We don't support dual-monitor treatment yet. */

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-exit controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.2 "VM-Exit Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckExitCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* VM-exit controls. */
    VMXCTLSMSR ExitCtls;
    ExitCtls.u = CPUMGetGuestIa32VmxExitCtls(pVCpu);
    if (~pVmcs->u32ExitCtls & ExitCtls.n.disallowed0)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ExitCtlsDisallowed0);

    if (pVmcs->u32ExitCtls & ~ExitCtls.n.allowed1)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ExitCtlsAllowed1);

    /* Save preemption timer without activating it. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_PREEMPT_TIMER)
        && (pVmcs->u32ProcCtls & VMX_EXIT_CTLS_SAVE_PREEMPT_TIMER))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_SavePreemptTimer);

    /* VM-exit MSR-store count and VM-exit MSR-store area address. */
    if (pVmcs->u32ExitMsrStoreCount)
    {
        if (   (pVmcs->u64AddrExitMsrStore.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrExitMsrStore.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrExitMsrStore.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrExitMsrStore);
    }

    /* VM-exit MSR-load count and VM-exit MSR-load area address. */
    if (pVmcs->u32ExitMsrLoadCount)
    {
        if (   (pVmcs->u64AddrExitMsrLoad.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrExitMsrLoad.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrExitMsrLoad.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrExitMsrLoad);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-execution controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.1 "VM-Execution Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 *
 * @remarks This may update secondary-processor based VM-execution control fields
 *          in the current VMCS if necessary.
 */
IEM_STATIC int iemVmxVmentryCheckExecCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* Pin-based VM-execution controls. */
    {
        VMXCTLSMSR PinCtls;
        PinCtls.u = CPUMGetGuestIa32VmxPinbasedCtls(pVCpu);
        if (~pVmcs->u32PinCtls & PinCtls.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_PinCtlsDisallowed0);

        if (pVmcs->u32PinCtls & ~PinCtls.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_PinCtlsAllowed1);
    }

    /* Processor-based VM-execution controls. */
    {
        VMXCTLSMSR ProcCtls;
        ProcCtls.u = CPUMGetGuestIa32VmxProcbasedCtls(pVCpu);
        if (~pVmcs->u32ProcCtls & ProcCtls.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtlsDisallowed0);

        if (pVmcs->u32ProcCtls & ~ProcCtls.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtlsAllowed1);
    }

    /* Secondary processor-based VM-execution controls. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
    {
        VMXCTLSMSR ProcCtls2;
        ProcCtls2.u = CPUMGetGuestIa32VmxProcbasedCtls2(pVCpu);
        if (~pVmcs->u32ProcCtls2 & ProcCtls2.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtls2Disallowed0);

        if (pVmcs->u32ProcCtls2 & ~ProcCtls2.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtls2Allowed1);
    }
    else
        Assert(!pVmcs->u32ProcCtls2);

    /* CR3-target count. */
    if (pVmcs->u32Cr3TargetCount <= VMX_V_CR3_TARGET_COUNT)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_Cr3TargetCount);

    /* IO bitmaps physical addresses. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_IO_BITMAPS)
    {
        if (   (pVmcs->u64AddrIoBitmapA.u & X86_PAGE_4K_OFFSET_MASK)
            || (pVmcs->u64AddrIoBitmapA.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrIoBitmapA.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrIoBitmapA);

        if (   (pVmcs->u64AddrIoBitmapB.u & X86_PAGE_4K_OFFSET_MASK)
            || (pVmcs->u64AddrIoBitmapB.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrIoBitmapB.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrIoBitmapB);
    }

    /* MSR bitmap physical address. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_MSR_BITMAPS)
    {
        RTGCPHYS const GCPhysMsrBitmap = pVmcs->u64AddrMsrBitmap.u;
        if (   (GCPhysMsrBitmap & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysMsrBitmap >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysMsrBitmap))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrMsrBitmap);

        /* Read the MSR bitmap. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvMsrBitmap));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvMsrBitmap),
                                         GCPhysMsrBitmap, VMX_V_MSR_BITMAP_SIZE);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_MsrBitmapPtrReadPhys);
    }

    /* TPR shadow related controls. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_TPR_SHADOW)
    {
        /* Virtual-APIC page physical address. */
        RTGCPHYS GCPhysVirtApic = pVmcs->u64AddrVirtApic.u;
        if (   (GCPhysVirtApic & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysVirtApic >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVirtApic))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVirtApicPage);

        /* Read the Virtual-APIC page. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage),
                                         GCPhysVirtApic, VMX_V_VIRT_APIC_PAGES);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtApicPagePtrReadPhys);

        /* TPR threshold without virtual-interrupt delivery. */
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
            &&  (pVmcs->u32TprThreshold & ~VMX_TPR_THRESHOLD_MASK))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_TprThresholdRsvd);

        /* TPR threshold and VTPR. */
        uint8_t const *pbVirtApic = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage);
        uint8_t const  u8VTpr     = *(pbVirtApic + XAPIC_OFF_TPR);
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
            && RT_BF_GET(pVmcs->u32TprThreshold, VMX_BF_TPR_THRESHOLD_TPR) > ((u8VTpr >> 4) & UINT32_C(0xf)) /* Bits 4:7 */)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_TprThresholdVTpr);
    }
    else
    {
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_APIC_REG_VIRT)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY))
        { /* likely */ }
        else
        {
            if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicTprShadow);
            if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_APIC_REG_VIRT)
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ApicRegVirt);
            Assert(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtIntDelivery);
        }
    }

    /* NMI exiting and virtual-NMIs. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_NMI_EXIT)
        &&  (pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtNmi);

    /* Virtual-NMIs and NMI-window exiting. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI)
        && (pVmcs->u32ProcCtls & VMX_PROC_CTLS_NMI_WINDOW_EXIT))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_NmiWindowExit);

    /* Virtualize APIC accesses. */
    if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS)
    {
        /* APIC-access physical address. */
        RTGCPHYS GCPhysApicAccess = pVmcs->u64AddrApicAccess.u;
        if (   (GCPhysApicAccess & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysApicAccess >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysApicAccess))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrApicAccess);
    }

    /* Virtualize-x2APIC mode is mutually exclusive with virtualize-APIC accesses. */
    if (   (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
        && (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicVirtApic);

    /* Virtual-interrupt delivery requires external interrupt exiting. */
    if (   (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
        && !(pVmcs->u32PinCtls & VMX_PIN_CTLS_EXT_INT_EXIT))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicVirtApic);

    /* VPID. */
    if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VPID)
        || pVmcs->u16Vpid != 0)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_Vpid);

    Assert(!(pVmcs->u32PinCtls & VMX_PIN_CTLS_POSTED_INT));             /* We don't support posted interrupts yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT));                /* We don't support EPT yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_PML));                /* We don't support PML yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)); /* We don't support Unrestricted-guests yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VMFUNC));             /* We don't support VM functions yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT_VE));             /* We don't support EPT-violation #VE yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_PAUSE_LOOP_EXIT));    /* We don't support Pause-loop exiting yet. */

    /* VMCS shadowing. */
    if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VMCS_SHADOWING)
    {
        /* VMREAD-bitmap physical address. */
        RTGCPHYS GCPhysVmreadBitmap = pVmcs->u64AddrVmreadBitmap.u;
        if (   ( GCPhysVmreadBitmap & X86_PAGE_4K_OFFSET_MASK)
            || ( GCPhysVmreadBitmap >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmreadBitmap))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVmreadBitmap);

        /* VMWRITE-bitmap physical address. */
        RTGCPHYS GCPhysVmwriteBitmap = pVmcs->u64AddrVmreadBitmap.u;
        if (   ( GCPhysVmwriteBitmap & X86_PAGE_4K_OFFSET_MASK)
            || ( GCPhysVmwriteBitmap >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmwriteBitmap))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVmwriteBitmap);

        /* Read the VMREAD-bitmap. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap),
                                         GCPhysVmreadBitmap, VMX_V_VMREAD_VMWRITE_BITMAP_SIZE);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmreadBitmapPtrReadPhys);

        /* Read the VMWRITE-bitmap. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmwriteBitmap));
        rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmwriteBitmap),
                                     GCPhysVmwriteBitmap, VMX_V_VMREAD_VMWRITE_BITMAP_SIZE);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmwriteBitmapPtrReadPhys);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Loads the guest control registers, debug register and some MSRs as part of
 * VM-entry.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmentryLoadGuestControlRegsMsrs(PVMCPU pVCpu)
{
    /*
     * Load guest control registers, debug registers and MSRs.
     * See Intel spec. 26.3.2.1 "Loading Guest Control Registers, Debug Registers and MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    uint64_t const uGstCr0 = (pVmcs->u64GuestCr0.u   & ~VMX_ENTRY_CR0_IGNORE_MASK)
                           | (pVCpu->cpum.GstCtx.cr0 &  VMX_ENTRY_CR0_IGNORE_MASK);
    CPUMSetGuestCR0(pVCpu, uGstCr0);
    CPUMSetGuestCR4(pVCpu, pVmcs->u64GuestCr4.u);
    pVCpu->cpum.GstCtx.cr3 = pVmcs->u64GuestCr3.u;

    if (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_DEBUG)
        pVCpu->cpum.GstCtx.dr[7] = (pVmcs->u64GuestDr7.u & ~VMX_ENTRY_DR7_MBZ_MASK) | VMX_ENTRY_DR7_MB1_MASK;

    pVCpu->cpum.GstCtx.SysEnter.eip = pVmcs->u64GuestSysenterEip.s.Lo;
    pVCpu->cpum.GstCtx.SysEnter.esp = pVmcs->u64GuestSysenterEsp.s.Lo;
    pVCpu->cpum.GstCtx.SysEnter.cs  = pVmcs->u32GuestSysenterCS;

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* FS base and GS base are loaded  while loading the rest of the guest segment registers. */

        /* EFER MSR. */
        if (!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_EFER_MSR))
        {
            bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
            bool const fGstPaging     = RT_BOOL(uGstCr0 & X86_CR0_PG);
            uint64_t const uHostEfer  = pVCpu->cpum.GstCtx.msrEFER;
            if (fGstInLongMode)
            {
                /* If the nested-guest is in long mode, LMA and LME are both set. */
                Assert(fGstPaging);
                pVCpu->cpum.GstCtx.msrEFER = uHostEfer | (MSR_K6_EFER_LMA | MSR_K6_EFER_LME);
            }
            else
            {
                /*
                 * If the nested-guest is outside long mode:
                 *   - With paging:    LMA is cleared, LME is cleared.
                 *   - Without paging: LMA is cleared, LME is left unmodified.
                 */
                uint64_t const fLmaLmeMask = MSR_K6_EFER_LMA | (fGstPaging ? MSR_K6_EFER_LME : 0);
                pVCpu->cpum.GstCtx.msrEFER = uHostEfer & ~fLmaLmeMask;
            }
        }
        /* else: see below. */
    }

    /* PAT MSR. */
    if (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PAT_MSR)
        pVCpu->cpum.GstCtx.msrPAT = pVmcs->u64GuestPatMsr.u;

    /* EFER MSR. */
    if (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_EFER_MSR)
        pVCpu->cpum.GstCtx.msrEFER = pVmcs->u64GuestEferMsr.u;

    /* We don't support IA32_PERF_GLOBAL_CTRL MSR yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PERF_MSR));

    /* We don't support IA32_BNDCFGS MSR yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_BNDCFGS_MSR));

    /* Nothing to do for SMBASE register - We don't support SMM yet. */
}


/**
 * Loads the guest segment registers, GDTR, IDTR, LDTR and TR as part of VM-entry.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
IEM_STATIC void iemVmxVmentryLoadGuestSegRegs(PVMCPU pVCpu)
{
    /*
     * Load guest segment registers, GDTR, IDTR, LDTR and TR.
     * See Intel spec. 26.3.2.2 "Loading Guest Segment Registers and Descriptor-Table Registers".
     */
    /* CS, SS, ES, DS, FS, GS. */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    for (unsigned iSegReg = 0; iSegReg < X86_SREG_COUNT; iSegReg++)
    {
        PCPUMSELREG pGstSelReg = &pVCpu->cpum.GstCtx.aSRegs[iSegReg];
        CPUMSELREG  VmcsSelReg;
        int rc = iemVmxVmcsGetGuestSegReg(pVmcs, iSegReg, &VmcsSelReg);
        AssertRC(rc); NOREF(rc);
        if (!(VmcsSelReg.Attr.u & X86DESCATTR_UNUSABLE))
        {
            pGstSelReg->Sel      = VmcsSelReg.Sel;
            pGstSelReg->ValidSel = VmcsSelReg.Sel;
            pGstSelReg->fFlags   = CPUMSELREG_FLAGS_VALID;
            pGstSelReg->u64Base  = VmcsSelReg.u64Base;
            pGstSelReg->u32Limit = VmcsSelReg.u32Limit;
            pGstSelReg->Attr.u   = VmcsSelReg.Attr.u;
        }
        else
        {
            pGstSelReg->Sel      = VmcsSelReg.Sel;
            pGstSelReg->ValidSel = VmcsSelReg.Sel;
            pGstSelReg->fFlags   = CPUMSELREG_FLAGS_VALID;
            switch (iSegReg)
            {
                case X86_SREG_CS:
                    pGstSelReg->u64Base  = VmcsSelReg.u64Base;
                    pGstSelReg->u32Limit = VmcsSelReg.u32Limit;
                    pGstSelReg->Attr.u   = VmcsSelReg.Attr.u;
                    break;

                case X86_SREG_SS:
                    pGstSelReg->u64Base  = VmcsSelReg.u64Base & UINT32_C(0xfffffff0);
                    pGstSelReg->u32Limit = 0;
                    pGstSelReg->Attr.u   = (VmcsSelReg.Attr.u & X86DESCATTR_DPL) | X86DESCATTR_D | X86DESCATTR_UNUSABLE;
                    break;

                case X86_SREG_ES:
                case X86_SREG_DS:
                    pGstSelReg->u64Base  = 0;
                    pGstSelReg->u32Limit = 0;
                    pGstSelReg->Attr.u   = X86DESCATTR_UNUSABLE;
                    break;

                case X86_SREG_FS:
                case X86_SREG_GS:
                    pGstSelReg->u64Base  = VmcsSelReg.u64Base;
                    pGstSelReg->u32Limit = 0;
                    pGstSelReg->Attr.u   = X86DESCATTR_UNUSABLE;
                    break;
            }
            Assert(pGstSelReg->Attr.n.u1Unusable);
        }
    }

    /* LDTR. */
    pVCpu->cpum.GstCtx.ldtr.Sel      = pVmcs->GuestLdtr;
    pVCpu->cpum.GstCtx.ldtr.ValidSel = pVmcs->GuestLdtr;
    pVCpu->cpum.GstCtx.ldtr.fFlags   = CPUMSELREG_FLAGS_VALID;
    if (!(pVmcs->u32GuestLdtrAttr & X86DESCATTR_UNUSABLE))
    {
        pVCpu->cpum.GstCtx.ldtr.u64Base  = pVmcs->u64GuestLdtrBase.u;
        pVCpu->cpum.GstCtx.ldtr.u32Limit = pVmcs->u32GuestLdtrLimit;
        pVCpu->cpum.GstCtx.ldtr.Attr.u   = pVmcs->u32GuestLdtrAttr;
    }
    else
    {
        pVCpu->cpum.GstCtx.ldtr.u64Base  = 0;
        pVCpu->cpum.GstCtx.ldtr.u32Limit = 0;
        pVCpu->cpum.GstCtx.ldtr.Attr.u   = X86DESCATTR_UNUSABLE;
    }

    /* TR. */
    Assert(!(pVmcs->u32GuestTrAttr & X86DESCATTR_UNUSABLE));
    pVCpu->cpum.GstCtx.tr.Sel      = pVmcs->GuestTr;
    pVCpu->cpum.GstCtx.tr.ValidSel = pVmcs->GuestTr;
    pVCpu->cpum.GstCtx.tr.fFlags   = CPUMSELREG_FLAGS_VALID;
    pVCpu->cpum.GstCtx.tr.u64Base  = pVmcs->u64GuestTrBase.u;
    pVCpu->cpum.GstCtx.tr.u32Limit = pVmcs->u32GuestTrLimit;
    pVCpu->cpum.GstCtx.tr.Attr.u   = pVmcs->u32GuestTrAttr;

    /* GDTR. */
    pVCpu->cpum.GstCtx.gdtr.cbGdt = pVmcs->u32GuestGdtrLimit;
    pVCpu->cpum.GstCtx.gdtr.pGdt  = pVmcs->u64GuestGdtrBase.u;

    /* IDTR. */
    pVCpu->cpum.GstCtx.idtr.cbIdt = pVmcs->u32GuestIdtrLimit;
    pVCpu->cpum.GstCtx.idtr.pIdt  = pVmcs->u64GuestIdtrBase.u;
}


/**
 * Loads the guest MSRs from the VM-entry auto-load MSRs as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pszInstr    The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryLoadGuestAutoMsrs(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Load guest MSRs.
     * See Intel spec. 26.4 "Loading MSRs".
     */
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";

    /*
     * The VM-entry MSR-load area address need not be a valid guest-physical address if the
     * VM-entry MSR load count is 0. If this is the case, bail early without reading it.
     * See Intel spec. 24.8.2 "VM-Entry Controls for MSRs".
     */
    uint32_t const cMsrs = pVmcs->u32EntryMsrLoadCount;
    if (!cMsrs)
        return VINF_SUCCESS;

    /*
     * Verify the MSR auto-load count. Physical CPUs can behave unpredictably if the count is
     * exceeded including possibly raising #MC exceptions during VMX transition. Our
     * implementation shall fail VM-entry with an VMX_EXIT_ERR_MSR_LOAD VM-exit.
     */
    bool const fIsMsrCountValid = iemVmxIsAutoMsrCountValid(pVCpu, cMsrs);
    if (fIsMsrCountValid)
    { /* likely */ }
    else
    {
        iemVmxVmcsSetExitQual(pVCpu, VMX_V_AUTOMSR_AREA_SIZE / sizeof(VMXAUTOMSR));
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_MsrLoadCount);
    }

    RTGCPHYS const GCPhysAutoMsrArea = pVmcs->u64AddrEntryMsrLoad.u;
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), (void *)&pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea),
                                     GCPhysAutoMsrArea, VMX_V_AUTOMSR_AREA_SIZE);
    if (RT_SUCCESS(rc))
    {
        PVMXAUTOMSR pMsr = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pAutoMsrArea);
        Assert(pMsr);
        for (uint32_t idxMsr = 0; idxMsr < cMsrs; idxMsr++, pMsr++)
        {
            if (   !pMsr->u32Reserved
                &&  pMsr->u32Msr != MSR_K8_FS_BASE
                &&  pMsr->u32Msr != MSR_K8_GS_BASE
                &&  pMsr->u32Msr != MSR_K6_EFER
                &&  pMsr->u32Msr != MSR_IA32_SMM_MONITOR_CTL
                &&  pMsr->u32Msr >> 8 != MSR_IA32_X2APIC_START >> 8)
            {
                VBOXSTRICTRC rcStrict = CPUMSetGuestMsr(pVCpu, pMsr->u32Msr, pMsr->u64Value);
                if (rcStrict == VINF_SUCCESS)
                    continue;

                /*
                 * If we're in ring-0, we cannot handle returns to ring-3 at this point and continue VM-entry.
                 * If any guest hypervisor loads MSRs that require ring-3 handling, we cause a VM-entry failure
                 * recording the MSR index in the VM-exit qualification (as per the Intel spec.) and indicated
                 * further by our own, specific diagnostic code. Later, we can try implement handling of the
                 * MSR in ring-0 if possible, or come up with a better, generic solution.
                 */
                iemVmxVmcsSetExitQual(pVCpu, idxMsr);
                VMXVDIAG const enmDiag = rcStrict == VINF_CPUM_R3_MSR_WRITE
                                       ? kVmxVDiag_Vmentry_MsrLoadRing3
                                       : kVmxVDiag_Vmentry_MsrLoad;
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }
            else
            {
                iemVmxVmcsSetExitQual(pVCpu, idxMsr);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_MsrLoadRsvd);
            }
        }
    }
    else
    {
        AssertMsgFailed(("%s: Failed to read MSR auto-load area at %#RGp, rc=%Rrc\n", pszInstr, GCPhysAutoMsrArea, rc));
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_MsrLoadPtrReadPhys);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Loads the guest-state non-register state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 *
 * @remarks This must be called only after loading the nested-guest register state
 *          (especially nested-guest RIP).
 */
IEM_STATIC void iemVmxVmentryLoadGuestNonRegState(PVMCPU pVCpu)
{
    /*
     * Load guest non-register state.
     * See Intel spec. 26.6 "Special Features of VM Entry"
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    uint32_t const uEntryIntInfo = pVmcs->u32EntryIntInfo;
    if (VMX_ENTRY_INT_INFO_IS_VALID(uEntryIntInfo))
    {
        /** @todo NSTVMX: Pending debug exceptions. */
        Assert(!(pVmcs->u64GuestPendingDbgXcpt.u));

        if (pVmcs->u32GuestIntrState & VMX_VMCS_GUEST_INT_STATE_BLOCK_NMI)
        {
            /** @todo NSTVMX: Virtual-NMIs doesn't affect NMI blocking in the normal sense.
             *        We probably need a different force flag for virtual-NMI
             *        pending/blocking. */
            Assert(!(pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI));
            VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }
        else
            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS));

        if (pVmcs->u32GuestIntrState & (VMX_VMCS_GUEST_INT_STATE_BLOCK_STI | VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS))
            EMSetInhibitInterruptsPC(pVCpu, pVCpu->cpum.GstCtx.rip);
        else
            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));

        /* SMI blocking is irrelevant. We don't support SMIs yet. */
    }

    /* Loading PDPTEs will be taken care when we switch modes. We don't support EPT yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT));

    /* VPID is irrelevant. We don't support VPID yet. */

    /* Clear address-range monitoring. */
    EMMonitorWaitClear(pVCpu);
}


/**
 * Loads the guest-state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 *
 * @remarks This must be done after all the necessary steps prior to loading of
 *          guest-state (e.g. checking various VMCS state).
 */
IEM_STATIC int iemVmxVmentryLoadGuestState(PVMCPU pVCpu, const char *pszInstr)
{
    iemVmxVmentryLoadGuestControlRegsMsrs(pVCpu);
    iemVmxVmentryLoadGuestSegRegs(pVCpu);

    /*
     * Load guest RIP, RSP and RFLAGS.
     * See Intel spec. 26.3.2.3 "Loading Guest RIP, RSP and RFLAGS".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    pVCpu->cpum.GstCtx.rsp      = pVmcs->u64GuestRsp.u;
    pVCpu->cpum.GstCtx.rip      = pVmcs->u64GuestRip.u;
    pVCpu->cpum.GstCtx.rflags.u = pVmcs->u64GuestRFlags.u;

    iemVmxVmentryLoadGuestNonRegState(pVCpu);

    NOREF(pszInstr);
    return VINF_SUCCESS;
}


/**
 * Performs event injection (if any) as part of VM-entry.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pszInstr    The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryInjectEvent(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Inject events.
     * See Intel spec. 26.5 "Event Injection".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    uint32_t const uEntryIntInfo = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->u32EntryIntInfo;
    if (VMX_ENTRY_INT_INFO_IS_VALID(uEntryIntInfo))
    {
        /*
         * The event that is going to be made pending for injection is not subject to VMX intercepts,
         * thus we flag ignoring of intercepts. However, recursive exceptions if any during delivery
         * of the current event -are- subject to intercepts, hence this flag will be flipped during
         * the actually delivery of this event.
         */
        pVCpu->cpum.GstCtx.hwvirt.vmx.fInterceptEvents = false;

        uint8_t const uType = VMX_ENTRY_INT_INFO_TYPE(uEntryIntInfo);
        if (uType == VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT)
        {
            Assert(VMX_ENTRY_INT_INFO_VECTOR(uEntryIntInfo) == VMX_ENTRY_INT_INFO_VECTOR_MTF);
            VMCPU_FF_SET(pVCpu, VMCPU_FF_MTF);
            return VINF_SUCCESS;
        }

        int rc = HMVmxEntryIntInfoInjectTrpmEvent(pVCpu, uEntryIntInfo, pVmcs->u32EntryXcptErrCode, pVmcs->u32EntryInstrLen,
                                                  pVCpu->cpum.GstCtx.cr2);
        AssertRCReturn(rc, rc);
    }

    NOREF(pszInstr);
    return VINF_SUCCESS;
}


/**
 * VMLAUNCH/VMRESUME instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   uInstrId        The instruction identity (VMXINSTRID_VMLAUNCH or
 *                          VMXINSTRID_VMRESUME).
 * @param   pExitInfo       Pointer to the VM-exit instruction information struct.
 *                          Optional, can  be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmlaunchVmresume(PVMCPU pVCpu, uint8_t cbInstr, VMXINSTRID uInstrId, PCVMXVEXITINFO pExitInfo)
{
    Assert(   uInstrId == VMXINSTRID_VMLAUNCH
           || uInstrId == VMXINSTRID_VMRESUME);
    const char *pszInstr = uInstrId == VMXINSTRID_VMRESUME ? "vmresume" : "vmlaunch";

    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        uint32_t const uExitReason = uInstrId == VMXINSTRID_VMRESUME ? VMX_EXIT_VMRESUME : VMX_EXIT_VMLAUNCH;
        return iemVmxVmexitInstrNeedsInfo(pVCpu, uExitReason, uInstrId, cbInstr);
    }

    Assert(IEM_VMX_IS_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("%s: CPL %u -> #GP(0)\n", pszInstr, pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Current VMCS valid. */
    if (!IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("%s: VMCS pointer %#RGp invalid -> VMFailInvalid\n", pszInstr, IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /** @todo Distinguish block-by-MOV-SS from block-by-STI. Currently we
     *        use block-by-STI here which is not quite correct. */
    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && pVCpu->cpum.GstCtx.rip == EMGetInhibitInterruptsPC(pVCpu))
    {
        Log(("%s: VM entry with events blocked by MOV SS -> VMFail\n", pszInstr));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_BlocKMovSS;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_BLOCK_MOVSS);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    if (uInstrId == VMXINSTRID_VMLAUNCH)
    {
        /* VMLAUNCH with non-clear VMCS. */
        if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState == VMX_V_VMCS_STATE_CLEAR)
        { /* likely */ }
        else
        {
            Log(("vmlaunch: VMLAUNCH with non-clear VMCS -> VMFail\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_VmcsClear;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMLAUNCH_NON_CLEAR_VMCS);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }
    }
    else
    {
        /* VMRESUME with non-launched VMCS. */
        if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState == VMX_V_VMCS_STATE_LAUNCHED)
        { /* likely */ }
        else
        {
            Log(("vmresume: VMRESUME with non-launched VMCS -> VMFail\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_VmcsLaunch;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMRESUME_NON_LAUNCHED_VMCS);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }
    }

    /*
     * Load the current VMCS.
     */
    Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs));
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs),
                                     IEM_VMX_GET_CURRENT_VMCS(pVCpu), VMX_V_VMCS_SIZE);
    if (RT_FAILURE(rc))
    {
        Log(("%s: Failed to read VMCS at %#RGp, rc=%Rrc\n", pszInstr, IEM_VMX_GET_CURRENT_VMCS(pVCpu), rc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_PtrReadPhys;
        return rc;
    }

    /*
     * We are allowed to cache VMCS related data structures (such as I/O bitmaps, MSR bitmaps)
     * while entering VMX non-root mode. We do some of this while checking VM-execution
     * controls. The guest hypervisor should not make assumptions and is cannot expect
     * predictable behavior if changes to these structures are made in guest memory after
     * executing VMX non-root mode. As far as VirtualBox is concerned, the guest cannot modify
     * them anyway as we cache them in host memory. We are trade memory for speed here.
     *
     * See Intel spec. 24.11.4 "Software Access to Related Structures".
     */
    rc = iemVmxVmentryCheckExecCtls(pVCpu, pszInstr);
    if (RT_SUCCESS(rc))
    {
        rc = iemVmxVmentryCheckExitCtls(pVCpu, pszInstr);
        if (RT_SUCCESS(rc))
        {
            rc = iemVmxVmentryCheckEntryCtls(pVCpu, pszInstr);
            if (RT_SUCCESS(rc))
            {
                rc = iemVmxVmentryCheckHostState(pVCpu, pszInstr);
                if (RT_SUCCESS(rc))
                {
                    /* Save the guest force-flags as VM-exits can occur from this point on. */
                    iemVmxVmentrySaveForceFlags(pVCpu);

                    rc = iemVmxVmentryCheckGuestState(pVCpu, pszInstr);
                    if (RT_SUCCESS(rc))
                    {
                        rc = iemVmxVmentryLoadGuestState(pVCpu, pszInstr);
                        if (RT_SUCCESS(rc))
                        {
                            rc = iemVmxVmentryLoadGuestAutoMsrs(pVCpu, pszInstr);
                            if (RT_SUCCESS(rc))
                            {
                                Assert(rc != VINF_CPUM_R3_MSR_WRITE);

                                /* VMLAUNCH instruction must update the VMCS launch state. */
                                if (uInstrId == VMXINSTRID_VMLAUNCH)
                                    pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState = VMX_V_VMCS_STATE_LAUNCHED;

                                /* Perform the VMX transition (PGM updates). */
                                VBOXSTRICTRC rcStrict = iemVmxWorldSwitch(pVCpu);
                                if (rcStrict == VINF_SUCCESS)
                                { /* likely */ }
                                else if (RT_SUCCESS(rcStrict))
                                {
                                    Log3(("%s: iemVmxWorldSwitch returns %Rrc -> Setting passup status\n", pszInstr,
                                          VBOXSTRICTRC_VAL(rcStrict)));
                                    rcStrict = iemSetPassUpStatus(pVCpu, rcStrict);
                                }
                                else
                                {
                                    Log3(("%s: iemVmxWorldSwitch failed! rc=%Rrc\n", pszInstr, VBOXSTRICTRC_VAL(rcStrict)));
                                    return rcStrict;
                                }

                                /* We've now entered nested-guest execution. */
                                pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode = true;

                                /* Now that we've switched page tables, we can inject events if any. */
                                iemVmxVmentryInjectEvent(pVCpu, pszInstr);

                                /** @todo NSTVMX: Setup VMX preemption timer */
                                /** @todo NSTVMX: TPR thresholding. */

                                return VINF_SUCCESS;
                            }
                            return iemVmxVmexit(pVCpu, VMX_EXIT_ERR_MSR_LOAD | VMX_EXIT_REASON_ENTRY_FAILED);
                        }
                    }
                    return iemVmxVmexit(pVCpu, VMX_EXIT_ERR_INVALID_GUEST_STATE | VMX_EXIT_REASON_ENTRY_FAILED);
                }

                iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_HOST_STATE);
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
                return VINF_SUCCESS;
            }
        }
    }

    iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_CTLS);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Checks whether an RDMSR or WRMSR instruction for the given MSR is intercepted
 * (causes a VM-exit)  or not.
 *
 * @returns @c true if the instruction is intercepted, @c false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit exit reason (VMX_EXIT_RDMSR or
 *                          VMX_EXIT_WRMSR).
 * @param   idMsr           The MSR.
 */
IEM_STATIC bool iemVmxIsRdmsrWrmsrInterceptSet(PVMCPU pVCpu, uint32_t uExitReason, uint32_t idMsr)
{
    Assert(IEM_VMX_IS_NON_ROOT_MODE(pVCpu));
    Assert(   uExitReason == VMX_EXIT_RDMSR
           || uExitReason == VMX_EXIT_WRMSR);

    /* Consult the MSR bitmap if the feature is supported. */
    if (IEM_VMX_IS_PROCCTLS_SET(pVCpu, VMX_PROC_CTLS_USE_MSR_BITMAPS))
    {
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvMsrBitmap));
        if (uExitReason == VMX_EXIT_RDMSR)
        {
            VMXMSREXITREAD enmRead;
            int rc = HMVmxGetMsrPermission(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvMsrBitmap), idMsr, &enmRead,
                                           NULL /* penmWrite */);
            AssertRC(rc);
            if (enmRead == VMXMSREXIT_INTERCEPT_READ)
                return true;
        }
        else
        {
            VMXMSREXITWRITE enmWrite;
            int rc = HMVmxGetMsrPermission(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvMsrBitmap), idMsr, NULL /* penmRead */,
                                           &enmWrite);
            AssertRC(rc);
            if (enmWrite == VMXMSREXIT_INTERCEPT_WRITE)
                return true;
        }
        return false;
    }

    /* Without MSR bitmaps, all MSR accesses are intercepted. */
    return true;
}


/**
 * Checks whether a VMREAD or VMWRITE instruction for the given VMCS field is
 * intercepted (causes a VM-exit) or not.
 *
 * @returns @c true if the instruction is intercepted, @c false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   uExitReason     The VM-exit exit reason (VMX_EXIT_VMREAD or
 *                          VMX_EXIT_VMREAD).
 */
IEM_STATIC bool iemVmxIsVmreadVmwriteInterceptSet(PVMCPU pVCpu, uint32_t uExitReason, uint64_t u64FieldEnc)
{
    Assert(IEM_VMX_IS_NON_ROOT_MODE(pVCpu));
    Assert(   uExitReason == VMX_EXIT_VMREAD
           || uExitReason == VMX_EXIT_VMWRITE);

    /* Without VMCS shadowing, all VMREAD and VMWRITE instructions are intercepted. */
    if (!IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmcsShadowing)
        return true;

    /*
     * If any reserved bit in the 64-bit VMCS field encoding is set, the VMREAD/VMWRITE is intercepted.
     * This excludes any reserved bits in the valid parts of the field encoding (i.e. bit 12).
     */
    if (u64FieldEnc & VMX_VMCS_ENC_RSVD_MASK)
        return true;

    /* Finally, consult the VMREAD/VMWRITE bitmap whether to intercept the instruction or not. */
    uint32_t u32FieldEnc = RT_LO_U32(u64FieldEnc);
    Assert(u32FieldEnc >> 3 < VMX_V_VMREAD_VMWRITE_BITMAP_SIZE);
    Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap));
    uint8_t const *pbBitmap = uExitReason == VMX_EXIT_VMREAD
                            ? (uint8_t const *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap)
                            : (uint8_t const *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmwriteBitmap);
    pbBitmap += (u32FieldEnc >> 3);
    if (*pbBitmap & RT_BIT(u32FieldEnc & 7))
        return true;

    return false;
}


/**
 * VMREAD common (memory/register) instruction execution worker
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu64Dst         Where to write the VMCS value (only updated when
 *                          VINF_SUCCESS is returned).
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadCommon(PVMCPU pVCpu, uint8_t cbInstr, uint64_t *pu64Dst, uint64_t u64FieldEnc,
                                           PCVMXVEXITINFO pExitInfo)
{
    /* Nested-guest intercept. */
    if (   IEM_VMX_IS_NON_ROOT_MODE(pVCpu)
        && iemVmxIsVmreadVmwriteInterceptSet(pVCpu, VMX_EXIT_VMREAD, u64FieldEnc))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMREAD, VMXINSTRID_VMREAD, cbInstr);
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmread: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMCS pointer in root mode. */
    if (    IEM_VMX_IS_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("vmread: VMCS pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS-link pointer in non-root mode. */
    if (    IEM_VMX_IS_NON_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_SHADOW_VMCS(pVCpu))
    {
        Log(("vmread: VMCS-link pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_SHADOW_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_LinkPtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Supported VMCS field. */
    if (iemVmxIsVmcsFieldValid(pVCpu, u64FieldEnc))
    {
        Log(("vmread: VMCS field %#RX64 invalid -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_FieldInvalid;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMREAD_INVALID_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Setup reading from the current or shadow VMCS.
     */
    uint8_t *pbVmcs;
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs);
    else
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pbVmcs);

    VMXVMCSFIELDENC FieldEnc;
    FieldEnc.u = RT_LO_U32(u64FieldEnc);
    uint8_t  const uWidth     = FieldEnc.n.u2Width;
    uint8_t  const uType      = FieldEnc.n.u2Type;
    uint8_t  const uWidthType = (uWidth << 2) | uType;
    uint8_t  const uIndex     = FieldEnc.n.u8Index;
    AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_2);
    uint16_t const offField   = g_aoffVmcsMap[uWidthType][uIndex];

    /*
     * Read the VMCS component based on the field's effective width.
     *
     * The effective width is 64-bit fields adjusted to 32-bits if the access-type
     * indicates high bits (little endian).
     *
     * Note! The caller is responsible to trim the result and update registers
     * or memory locations are required. Here we just zero-extend to the largest
     * type (i.e. 64-bits).
     */
    uint8_t      *pbField = pbVmcs + offField;
    uint8_t const uEffWidth = HMVmxGetVmcsFieldWidthEff(FieldEnc.u);
    switch (uEffWidth)
    {
        case VMX_VMCS_ENC_WIDTH_64BIT:
        case VMX_VMCS_ENC_WIDTH_NATURAL: *pu64Dst = *(uint64_t *)pbField; break;
        case VMX_VMCS_ENC_WIDTH_32BIT:   *pu64Dst = *(uint32_t *)pbField; break;
        case VMX_VMCS_ENC_WIDTH_16BIT:   *pu64Dst = *(uint16_t *)pbField; break;
    }
    return VINF_SUCCESS;
}


/**
 * VMREAD (64-bit register) instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu64Dst         Where to store the VMCS field's value.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadReg64(PVMCPU pVCpu, uint8_t cbInstr, uint64_t *pu64Dst, uint64_t u64FieldEnc,
                                          PCVMXVEXITINFO pExitInfo)
{
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, pu64Dst, u64FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        iemVmxVmreadSuccess(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    Log(("vmread/reg: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMREAD (32-bit register) instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu32Dst         Where to store the VMCS field's value.
 * @param   u32FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadReg32(PVMCPU pVCpu, uint8_t cbInstr, uint32_t *pu32Dst, uint64_t u32FieldEnc,
                                          PCVMXVEXITINFO pExitInfo)
{
    uint64_t u64Dst;
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, &u64Dst, u32FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        *pu32Dst = u64Dst;
        iemVmxVmreadSuccess(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    Log(("vmread/reg: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMREAD (memory) instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a u64Val.
 *                          Pass UINT8_MAX if it is a register access.
 * @param   enmEffAddrMode  The effective addressing mode (only used with memory
 *                          operand).
 * @param   GCPtrDst        The guest linear address to store the VMCS field's
 *                          value.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadMem(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, IEMMODE enmEffAddrMode,
                                        RTGCPTR GCPtrDst, uint64_t u64FieldEnc, PCVMXVEXITINFO pExitInfo)
{
    uint64_t u64Dst;
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, &u64Dst, u64FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        /*
         * Write the VMCS field's value to the location specified in guest-memory.
         *
         * The pointer size depends on the address size (address-size prefix allowed).
         * The operand size depends on IA-32e mode (operand-size prefix not allowed).
         */
        static uint64_t const s_auAddrSizeMasks[] = { UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_C(0xffffffffffffffff) };
        Assert(enmEffAddrMode < RT_ELEMENTS(s_auAddrSizeMasks));
        GCPtrDst &= s_auAddrSizeMasks[enmEffAddrMode];

        if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
            rcStrict = iemMemStoreDataU64(pVCpu, iEffSeg, GCPtrDst, u64Dst);
        else
            rcStrict = iemMemStoreDataU32(pVCpu, iEffSeg, GCPtrDst, u64Dst);
        if (rcStrict == VINF_SUCCESS)
        {
            iemVmxVmreadSuccess(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        Log(("vmread/mem: Failed to write to memory operand at %#RGv, rc=%Rrc\n", GCPtrDst, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_PtrMap;
        return rcStrict;
    }

    Log(("vmread/mem: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMWRITE instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a u64Val.
 *                          Pass UINT8_MAX if it is a register access.
 * @param   enmEffAddrMode  The effective addressing mode (only used with memory
 *                          operand).
 * @param   u64Val          The value to write (or guest linear address to the
 *                          value), @a iEffSeg will indicate if it's a memory
 *                          operand.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmwrite(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, IEMMODE enmEffAddrMode, uint64_t u64Val,
                                      uint64_t u64FieldEnc, PCVMXVEXITINFO pExitInfo)
{
    /* Nested-guest intercept. */
    if (   IEM_VMX_IS_NON_ROOT_MODE(pVCpu)
        && iemVmxIsVmreadVmwriteInterceptSet(pVCpu, VMX_EXIT_VMWRITE, u64FieldEnc))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMWRITE, VMXINSTRID_VMWRITE, cbInstr);
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmwrite: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMCS pointer in root mode. */
    if (    IEM_VMX_IS_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS-link pointer in non-root mode. */
    if (    IEM_VMX_IS_NON_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_SHADOW_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS-link pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_SHADOW_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_LinkPtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* If the VMWRITE instruction references memory, access the specified memory operand. */
    bool const fIsRegOperand = iEffSeg == UINT8_MAX;
    if (!fIsRegOperand)
    {
        static uint64_t const s_auAddrSizeMasks[] = { UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_C(0xffffffffffffffff) };
        Assert(enmEffAddrMode < RT_ELEMENTS(s_auAddrSizeMasks));
        RTGCPTR const GCPtrVal = u64Val & s_auAddrSizeMasks[enmEffAddrMode];

        /* Read the value from the specified guest memory location. */
        VBOXSTRICTRC rcStrict;
        if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
            rcStrict = iemMemFetchDataU64(pVCpu, &u64Val, iEffSeg, GCPtrVal);
        else
        {
            uint32_t u32Val;
            rcStrict = iemMemFetchDataU32(pVCpu, &u32Val, iEffSeg, GCPtrVal);
            u64Val = u32Val;
        }
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmwrite: Failed to read value from memory operand at %#RGv, rc=%Rrc\n", GCPtrVal, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_PtrMap;
            return rcStrict;
        }
    }
    else
        Assert(!pExitInfo || pExitInfo->InstrInfo.VmreadVmwrite.fIsRegOperand);

    /* Supported VMCS field. */
    if (!iemVmxIsVmcsFieldValid(pVCpu, u64FieldEnc))
    {
        Log(("vmwrite: VMCS field %#RX64 invalid -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_FieldInvalid;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_INVALID_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Read-only VMCS field. */
    bool const fIsFieldReadOnly = HMVmxIsVmcsFieldReadOnly(u64FieldEnc);
    if (   fIsFieldReadOnly
        && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmwriteAll)
    {
        Log(("vmwrite: Write to read-only VMCS component %#RX64 -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_FieldRo;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_RO_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Setup writing to the current or shadow VMCS.
     */
    uint8_t *pbVmcs;
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs);
    else
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pbVmcs);

    VMXVMCSFIELDENC FieldEnc;
    FieldEnc.u = RT_LO_U32(u64FieldEnc);
    uint8_t  const uWidth     = FieldEnc.n.u2Width;
    uint8_t  const uType      = FieldEnc.n.u2Type;
    uint8_t  const uWidthType = (uWidth << 2) | uType;
    uint8_t  const uIndex     = FieldEnc.n.u8Index;
    AssertReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_2);
    uint16_t const offField   = g_aoffVmcsMap[uWidthType][uIndex];

    /*
     * Write the VMCS component based on the field's effective width.
     *
     * The effective width is 64-bit fields adjusted to 32-bits if the access-type
     * indicates high bits (little endian).
     */
    uint8_t      *pbField = pbVmcs + offField;
    uint8_t const uEffWidth = HMVmxGetVmcsFieldWidthEff(FieldEnc.u);
    switch (uEffWidth)
    {
        case VMX_VMCS_ENC_WIDTH_64BIT:
        case VMX_VMCS_ENC_WIDTH_NATURAL: *(uint64_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_32BIT:   *(uint32_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_16BIT:   *(uint16_t *)pbField = u64Val; break;
    }

    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMCLEAR instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a GCPtrVmcs.
 * @param   GCPtrVmcs       The linear address of the VMCS pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmclear(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMCLEAR, VMXINSTRID_NONE, cbInstr);
    }

    Assert(IEM_VMX_IS_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmclear: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmclear: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmclear: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
    {
        Log(("vmclear: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmclear: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmclear: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrAbnormal;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * VMCLEAR allows committing and clearing any valid VMCS pointer.
     *
     * If the current VMCS is the one being cleared, set its state to 'clear' and commit
     * to guest memory. Otherwise, set the state of the VMCS referenced in guest memory
     * to 'clear'.
     */
    uint8_t const fVmcsStateClear = VMX_V_VMCS_STATE_CLEAR;
    if (IEM_VMX_GET_CURRENT_VMCS(pVCpu) == GCPhysVmcs)
    {
        Assert(GCPhysVmcs != NIL_RTGCPHYS);                     /* Paranoia. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs));
        pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState = fVmcsStateClear;
        iemVmxCommitCurrentVmcsToMemory(pVCpu);
        Assert(!IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    }
    else
    {
        rcStrict = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), GCPtrVmcs + RT_OFFSETOF(VMXVVMCS, fVmcsState),
                                            (const void *)&fVmcsStateClear, sizeof(fVmcsStateClear));
    }

    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return rcStrict;
}


/**
 * VMPTRST instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a GCPtrVmcs.
 * @param   GCPtrVmcs       The linear address of where to store the current VMCS
 *                          pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmptrst(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMPTRST, VMXINSTRID_NONE, cbInstr);
    }

    Assert(IEM_VMX_IS_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmptrst: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrst_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Set the VMCS pointer to the location specified by the destination memory operand. */
    AssertCompile(NIL_RTGCPHYS == ~(RTGCPHYS)0U);
    VBOXSTRICTRC rcStrict = iemMemStoreDataU64(pVCpu, iEffSeg, GCPtrVmcs, IEM_VMX_GET_CURRENT_VMCS(pVCpu));
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return rcStrict;
    }

    Log(("vmptrst: Failed to store VMCS pointer to memory at destination operand %#Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrst_PtrMap;
    return rcStrict;
}


/**
 * VMPTRLD instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   GCPtrVmcs       The linear address of the current VMCS pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmptrld(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
    {
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMPTRLD, VMXINSTRID_NONE, cbInstr);
    }

    Assert(IEM_VMX_IS_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmptrld: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmptrld: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmptrld: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
    {
        Log(("vmptrld: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmptrld: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmptrld: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrAbnormal;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Read the VMCS revision ID from the VMCS. */
    VMXVMCSREVID VmcsRevId;
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), &VmcsRevId, GCPhysVmcs, sizeof(VmcsRevId));
    if (RT_FAILURE(rc))
    {
        Log(("vmptrld: Failed to read VMCS at %#RGp, rc=%Rrc\n", GCPhysVmcs, rc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrReadPhys;
        return rc;
    }

    /* Verify the VMCS revision specified by the guest matches what we reported to the guest,
       also check VMCS shadowing feature. */
    if (   VmcsRevId.n.u31RevisionId != VMX_V_VMCS_REVISION_ID
        || (   VmcsRevId.n.fIsShadowVmcs
            && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmcsShadowing))
    {
        if (VmcsRevId.n.u31RevisionId != VMX_V_VMCS_REVISION_ID)
        {
            Log(("vmptrld: VMCS revision mismatch, expected %#RX32 got %#RX32 -> VMFail()\n", VMX_V_VMCS_REVISION_ID,
                 VmcsRevId.n.u31RevisionId));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_VmcsRevId;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        Log(("vmptrld: Shadow VMCS -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_ShadowVmcs;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * We only maintain only the current VMCS in our virtual CPU context (CPUMCTX). Therefore,
     * VMPTRLD shall always flush any existing current VMCS back to guest memory before loading
     * a new VMCS as current.
     */
    if (IEM_VMX_GET_CURRENT_VMCS(pVCpu) != GCPhysVmcs)
    {
        iemVmxCommitCurrentVmcsToMemory(pVCpu);
        IEM_VMX_SET_CURRENT_VMCS(pVCpu, GCPhysVmcs);
    }

    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMXON instruction execution worker.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a
 *                          GCPtrVmxon.
 * @param   GCPtrVmxon      The linear address of the VMXON pointer.
 * @param   pExitInfo       Pointer to the VM-exit instruction information struct.
 *                          Optional, can  be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmxon(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmxon,
                                    PCVMXVEXITINFO pExitInfo)
{
#if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF5(pVCpu, cbInstr, iEffSeg, GCPtrVmxon, pExitInfo);
    return VINF_EM_RAW_EMULATE_INSTR;
#else
    if (!IEM_VMX_IS_ROOT_MODE(pVCpu))
    {
        /* CPL. */
        if (pVCpu->iem.s.uCpl > 0)
        {
            Log(("vmxon: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cpl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* A20M (A20 Masked) mode. */
        if (!PGMPhysIsA20Enabled(pVCpu))
        {
            Log(("vmxon: A20M mode -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_A20M;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR0. */
        {
            /* CR0 MB1 bits. */
            uint64_t const uCr0Fixed0 = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
            if ((pVCpu->cpum.GstCtx.cr0 & uCr0Fixed0) != uCr0Fixed0)
            {
                Log(("vmxon: CR0 fixed0 bits cleared -> #GP(0)\n"));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr0Fixed0;
                return iemRaiseGeneralProtectionFault0(pVCpu);
            }

            /* CR0 MBZ bits. */
            uint64_t const uCr0Fixed1 = CPUMGetGuestIa32VmxCr0Fixed1(pVCpu);
            if (pVCpu->cpum.GstCtx.cr0 & ~uCr0Fixed1)
            {
                Log(("vmxon: CR0 fixed1 bits set -> #GP(0)\n"));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr0Fixed1;
                return iemRaiseGeneralProtectionFault0(pVCpu);
            }
        }

        /* CR4. */
        {
            /* CR4 MB1 bits. */
            uint64_t const uCr4Fixed0 = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
            if ((pVCpu->cpum.GstCtx.cr4 & uCr4Fixed0) != uCr4Fixed0)
            {
                Log(("vmxon: CR4 fixed0 bits cleared -> #GP(0)\n"));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr4Fixed0;
                return iemRaiseGeneralProtectionFault0(pVCpu);
            }

            /* CR4 MBZ bits. */
            uint64_t const uCr4Fixed1 = CPUMGetGuestIa32VmxCr4Fixed1(pVCpu);
            if (pVCpu->cpum.GstCtx.cr4 & ~uCr4Fixed1)
            {
                Log(("vmxon: CR4 fixed1 bits set -> #GP(0)\n"));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr4Fixed1;
                return iemRaiseGeneralProtectionFault0(pVCpu);
            }
        }

        /* Feature control MSR's LOCK and VMXON bits. */
        uint64_t const uMsrFeatCtl = CPUMGetGuestIa32FeatureControl(pVCpu);
        if (!(uMsrFeatCtl & (MSR_IA32_FEATURE_CONTROL_LOCK | MSR_IA32_FEATURE_CONTROL_VMXON)))
        {
            Log(("vmxon: Feature control lock bit or VMXON bit cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_MsrFeatCtl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Get the VMXON pointer from the location specified by the source memory operand. */
        RTGCPHYS GCPhysVmxon;
        VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmxon, iEffSeg, GCPtrVmxon);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmxon: Failed to read VMXON region physaddr from %#RGv, rc=%Rrc\n", GCPtrVmxon, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrMap;
            return rcStrict;
        }

        /* VMXON region pointer alignment. */
        if (GCPhysVmxon & X86_PAGE_4K_OFFSET_MASK)
        {
            Log(("vmxon: VMXON region pointer not page-aligned -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrAlign;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* VMXON physical-address width limits. */
        if (GCPhysVmxon >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
        {
            Log(("vmxon: VMXON region pointer extends beyond physical-address width -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrWidth;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Ensure VMXON region is not MMIO, ROM etc. This is not an Intel requirement but a
           restriction imposed by our implementation. */
        if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmxon))
        {
            Log(("vmxon: VMXON region not normal memory -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrAbnormal;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Read the VMCS revision ID from the VMXON region. */
        VMXVMCSREVID VmcsRevId;
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), &VmcsRevId, GCPhysVmxon, sizeof(VmcsRevId));
        if (RT_FAILURE(rc))
        {
            Log(("vmxon: Failed to read VMXON region at %#RGp, rc=%Rrc\n", GCPhysVmxon, rc));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrReadPhys;
            return rc;
        }

        /* Verify the VMCS revision specified by the guest matches what we reported to the guest. */
        if (RT_UNLIKELY(VmcsRevId.u != VMX_V_VMCS_REVISION_ID))
        {
            /* Revision ID mismatch. */
            if (!VmcsRevId.n.fIsShadowVmcs)
            {
                Log(("vmxon: VMCS revision mismatch, expected %#RX32 got %#RX32 -> VMFailInvalid\n", VMX_V_VMCS_REVISION_ID,
                     VmcsRevId.n.u31RevisionId));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmcsRevId;
                iemVmxVmFailInvalid(pVCpu);
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
                return VINF_SUCCESS;
            }

            /* Shadow VMCS disallowed. */
            Log(("vmxon: Shadow VMCS -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_ShadowVmcs;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /*
         * Record that we're in VMX operation, block INIT, block and disable A20M.
         */
        pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon    = GCPhysVmxon;
        IEM_VMX_CLEAR_CURRENT_VMCS(pVCpu);
        pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = true;

        /* Clear address-range monitoring. */
        EMMonitorWaitClear(pVCpu);
        /** @todo NSTVMX: Intel PT. */

        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
        return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, true);
# else
        return VINF_SUCCESS;
# endif
    }
    else if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
    {
        /* Nested-guest intercept. */
        if (pExitInfo)
            return iemVmxVmexitInstrWithInfo(pVCpu, pExitInfo);
        return iemVmxVmexitInstrNeedsInfo(pVCpu, VMX_EXIT_VMXON, VMXINSTRID_NONE, cbInstr);
    }

    Assert(IEM_VMX_IS_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxon: In VMX root mode: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmxRootCpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMXON when already in VMX root mode. */
    iemVmxVmFail(pVCpu, VMXINSTRERR_VMXON_IN_VMXROOTMODE);
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmxAlreadyRoot;
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
#endif
}


/**
 * Implements 'VMXOFF'.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmxoff)
{
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF2(pVCpu, cbInstr);
    return VINF_EM_RAW_EMULATE_INSTR;
# else
    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
        return iemVmxVmexitInstr(pVCpu, VMX_EXIT_VMXOFF, cbInstr);

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxoff: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxoff_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Dual monitor treatment of SMIs and SMM. */
    uint64_t const fSmmMonitorCtl = CPUMGetGuestIa32SmmMonitorCtl(pVCpu);
    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VALID)
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMXOFF_DUAL_MON);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Record that we're no longer in VMX root operation, block INIT, block and disable A20M. */
    pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = false;
    Assert(!pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode);

    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VMXOFF_UNBLOCK_SMI)
    { /** @todo NSTVMX: Unblock SMI. */ }

    EMMonitorWaitClear(pVCpu);
    /** @todo NSTVMX: Unblock and enable A20M. */

    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
#  if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
    return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, false);
#  else
    return VINF_SUCCESS;
#  endif
# endif
}


/**
 * Implements 'VMXON'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmxon, uint8_t, iEffSeg, RTGCPTR, GCPtrVmxon)
{
    return iemVmxVmxon(pVCpu, cbInstr, iEffSeg, GCPtrVmxon, NULL /* pExitInfo */);
}


/**
 * Implements 'VMLAUNCH'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmlaunch)
{
    return iemVmxVmlaunchVmresume(pVCpu, cbInstr, VMXINSTRID_VMLAUNCH, NULL /* pExitInfo */);
}


/**
 * Implements 'VMRESUME'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmresume)
{
    return iemVmxVmlaunchVmresume(pVCpu, cbInstr, VMXINSTRID_VMRESUME, NULL /* pExitInfo */);
}


/**
 * Implements 'VMPTRLD'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmptrld, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmptrld(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMPTRST'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmptrst, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmptrst(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMCLEAR'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmclear, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmclear(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMWRITE' register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmwrite_reg, uint64_t, u64Val, uint64_t, u64FieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, UINT8_MAX /* iEffSeg */, IEMMODE_64BIT /* N/A */, u64Val, u64FieldEnc,
                         NULL /* pExitInfo */);
}


/**
 * Implements 'VMWRITE' memory.
 */
IEM_CIMPL_DEF_4(iemCImpl_vmwrite_mem, uint8_t, iEffSeg, IEMMODE, enmEffAddrMode, RTGCPTR, GCPtrVal, uint32_t, u64FieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, iEffSeg, enmEffAddrMode, GCPtrVal, u64FieldEnc,  NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' 64-bit register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmread64_reg, uint64_t *, pu64Dst, uint64_t, u64FieldEnc)
{
    return iemVmxVmreadReg64(pVCpu, cbInstr, pu64Dst, u64FieldEnc, NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' 32-bit register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmread32_reg, uint32_t *, pu32Dst, uint32_t, u32FieldEnc)
{
    return iemVmxVmreadReg32(pVCpu, cbInstr, pu32Dst, u32FieldEnc, NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' memory.
 */
IEM_CIMPL_DEF_4(iemCImpl_vmread_mem, uint8_t, iEffSeg, IEMMODE, enmEffAddrMode, RTGCPTR, GCPtrDst, uint32_t, u64FieldEnc)
{
    return iemVmxVmreadMem(pVCpu, cbInstr, iEffSeg, enmEffAddrMode, GCPtrDst, u64FieldEnc, NULL /* pExitInfo */);
}

#endif  /* VBOX_WITH_NESTED_HWVIRT_VMX */


/**
 * Implements 'VMCALL'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmcall)
{
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
    /* Nested-guest intercept. */
    if (IEM_VMX_IS_NON_ROOT_MODE(pVCpu))
        return iemVmxVmexitInstr(pVCpu, VMX_EXIT_VMCALL, cbInstr);
#endif

    /* Join forces with vmmcall. */
    return IEM_CIMPL_CALL_1(iemCImpl_Hypercall, OP_VMCALL);
}

