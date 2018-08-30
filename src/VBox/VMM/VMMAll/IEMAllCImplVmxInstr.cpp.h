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


/**
 * Implements 'VMCALL'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmcall)
{
    /** @todo NSTVMX: intercept. */

    /* Join forces with vmmcall. */
    return IEM_CIMPL_CALL_1(iemCImpl_Hypercall, OP_VMCALL);
}

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
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
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmExitMsrStore),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmExitMsrLoad),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmEntryMsrLoad),
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
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64GuestPhysAddr),
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
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u32TprTreshold),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u32ProcCtls2),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u32PleGap),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u32PleWindow),
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32RoVmInstrError),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32RoVmExitReason),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32RoVmExitIntInfo),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32RoVmExitErrCode),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringInfo),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringErrCode),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32RoVmExitInstrLen),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32RoVmExitInstrInfo),
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
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64ExitQual),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64IoRcx),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64IoRsi),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64IoRdi),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64IoRip),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestLinearAddr),
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

/** Whether a shadow VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_SHADOW_VMCS(a_pVCpu)            RT_BOOL(IEM_VMX_GET_SHADOW_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the guest-physical address of the shadows VMCS for the given VCPU. */
#define IEM_VMX_GET_SHADOW_VMCS(a_pVCpu)            ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->u64VmcsLinkPtr.u)

/** Whether a current VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_CURRENT_VMCS(a_pVCpu)           RT_BOOL(IEM_VMX_GET_CURRENT_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the guest-physical address of the current VMCS for the given VCPU. */
#define IEM_VMX_GET_CURRENT_VMCS(a_pVCpu)           ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs)

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


/**
 * Returns whether the given VMCS field is valid and supported by our emulation.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFieldEnc       The VMCS field encoding.
 *
 * @remarks This takes into account the CPU features exposed to the guest.
 */
IEM_STATIC bool iemVmxIsVmcsFieldValid(PVMCPU pVCpu, uint32_t uFieldEnc)
{
    PCCPUMFEATURES pFeat = IEM_GET_GUEST_CPU_FEATURES(pVCpu);
    switch (uFieldEnc)
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
 * Gets VM-exit instruction information along with any displacement for an
 * instruction VM-exit.
 *
 * @returns The VM-exit instruction information.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 * @param   InstrId         The VM-exit instruction identity (VMX_INSTR_ID_XXX) if
 *                          any. Pass VMX_INSTR_ID_NONE otherwise.
 * @param   pGCPtrDisp      Where to store the displacement field. Optional, can be
 *                          NULL.
 */
IEM_STATIC uint32_t iemVmxGetExitInstrInfo(PVMCPU pVCpu, uint32_t uExitReason, VMXINSTRID InstrId, PRTGCPTR pGCPtrDisp)
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
         */
        ExitInstrInfo.All.u2Scaling       = 0;
        ExitInstrInfo.All.iReg1           = (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB;
        ExitInstrInfo.All.u3AddrSize      = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand   = 1;
        ExitInstrInfo.All.uOperandSize    = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg         = 0;
        ExitInstrInfo.All.iIdxReg         = 0;
        ExitInstrInfo.All.fIdxRegInvalid  = 1;
        ExitInstrInfo.All.iBaseReg        = 0;
        ExitInstrInfo.All.fBaseRegInvalid = 1;
        ExitInstrInfo.All.iReg2           = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;

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
        uint8_t  iReg2         = 0;
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
            iReg2     = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
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
            iReg2     = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK);
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
            iReg2 = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
        }

        ExitInstrInfo.All.u2Scaling      = uScale;
        ExitInstrInfo.All.iReg1          = 0;   /* Not applicable for memory instructions. */
        ExitInstrInfo.All.u3AddrSize     = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand  = 0;
        ExitInstrInfo.All.uOperandSize   = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg        = pVCpu->iem.s.iEffSeg;
        ExitInstrInfo.All.iIdxReg        = iIdxReg;
        ExitInstrInfo.All.fIdxRegInvalid = !fIdxRegValid;
        ExitInstrInfo.All.iBaseReg       = iBaseReg;
        ExitInstrInfo.All.iIdxReg        = !fBaseRegValid;
        ExitInstrInfo.All.iReg2          = iReg2;
    }

    /*
     * Handle exceptions for certain instructions.
     * (e.g. some instructions convey an instruction identity).
     */
    switch (uExitReason)
    {
        case VMX_EXIT_XDTR_ACCESS:
        {
            Assert(VMX_INSTR_ID_IS_VALID(InstrId));
            ExitInstrInfo.GdtIdt.u2InstrId = VMX_INSTR_ID_GET_ID(InstrId);
            ExitInstrInfo.GdtIdt.u2Undef0  = 0;
            break;
        }

        case VMX_EXIT_TR_ACCESS:
        {
            Assert(VMX_INSTR_ID_IS_VALID(InstrId));
            ExitInstrInfo.LdtTr.u2InstrId = VMX_INSTR_ID_GET_ID(InstrId);
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
 * Implements VMSucceed for VMX instruction success.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) iemVmxVmSucceed(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
}


/**
 * Implements VMFailInvalid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) iemVmxVmFailInvalid(PVMCPU pVCpu)
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
DECLINLINE(void) iemVmxVmFailValid(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
        pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_ZF;
        /** @todo NSTVMX: VMWrite enmInsErr to VM-instruction error field. */
        RT_NOREF(enmInsErr);
    }
}


/**
 * Implements VMFail for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECLINLINE(void) iemVmxVmFail(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        iemVmxVmFailValid(pVCpu, enmInsErr);
        /** @todo Set VM-instruction error field in the current virtual-VMCS.  */
    }
    else
        iemVmxVmFailInvalid(pVCpu);
}


/**
 * Flushes the current VMCS contents back to guest memory.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(int) iemVmxCommitCurrentVmcsToMemory(PVMCPU pVCpu)
{
    Assert(IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), IEM_VMX_GET_CURRENT_VMCS(pVCpu),
                                      pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs), sizeof(VMXVVMCS));
    IEM_VMX_CLEAR_CURRENT_VMCS(pVCpu);
    return rc;
}


/**
 * VMWRITE instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a u64Val.
 *                          Pass UINT8_MAX if it is a register access.
 * @param   enmEffAddrMode  The effective addressing mode (only used with memory
 *                          operand).
 * @param   u64Val          The value to write (or guest linear address to the
 *                          value), @a iEffSeg will indicate if it's a memory
 *                          operand.
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmwrite(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, IEMMODE enmEffAddrMode, uint64_t u64Val,
                                      uint32_t uFieldEnc, PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
        /** @todo NSTVMX: VMCS shadowing intercept (VMREAD/VMWRITE bitmap). */
    }

    /* CPL. */
    if (CPUMGetGuestCPL(pVCpu) > 0)
    {
        Log(("vmwrite: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMCS pointer in root mode. */
    if (    IEM_IS_VMX_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS-link pointer in non-root mode. */
    if (    IEM_IS_VMX_NON_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_SHADOW_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS-link pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_SHADOW_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* If the VMWRITE instruction references memory, access the specified in memory operand. */
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
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_PtrMap;
            return rcStrict;
        }
    }

    /* Supported VMCS field. */
    if (!iemVmxIsVmcsFieldValid(pVCpu, uFieldEnc))
    {
        Log(("vmwrite: VMCS field %#x invalid -> VMFail\n", uFieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_FieldInvalid;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_INVALID_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Read-only VMCS field. */
    bool const fReadOnlyField = HMVmxIsVmcsFieldReadOnly(uFieldEnc);
    if (   fReadOnlyField
        && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmwriteAll)
    {
        Log(("vmwrite: Write to read-only VMCS component -> VMFail\n", uFieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_FieldRo;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_RO_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Setup writing to the current or shadow VMCS.
     */
    uint8_t *pbVmcs;
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs);
    else
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pbVmcs);

    PCVMXVMCSFIELDENC pFieldEnc = (PCVMXVMCSFIELDENC)&uFieldEnc;
    uint8_t  const uWidth     = pFieldEnc->n.u2Width;
    uint8_t  const uType      = pFieldEnc->n.u2Type;
    uint8_t  const uWidthType = (uWidth << 2) | uType;
    uint8_t  const uIndex     = pFieldEnc->n.u8Index;
    AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_2);
    uint16_t const offField  = g_aoffVmcsMap[uWidthType][uIndex];

    /*
     * Write the VMCS component based on the field's effective width.
     *
     * The effective width is 64-bit fields adjusted to 32-bits if the access-type
     * indicates high bits (little endian).
     */
    uint8_t      *pbField = pbVmcs + offField;
    uint8_t const uEffWidth = HMVmxGetVmcsFieldWidthEff(uFieldEnc);
    switch (uEffWidth)
    {
        case VMX_VMCS_ENC_WIDTH_64BIT:
        case VMX_VMCS_ENC_WIDTH_NATURAL: *(uint64_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_32BIT:   *(uint32_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_16BIT:   *(uint16_t *)pbField = u64Val; break;
    }

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmwrite_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMCLEAR instruction execution worker.
 *
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
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (CPUMGetGuestCPL(pVCpu) > 0)
    {
        Log(("vmclear: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmclear: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmclear: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    Assert(!VMX_V_VMCS_PHYSADDR_4G_LIMIT);
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth)
    {
        Log(("vmclear: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmclear: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmclear: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_PtrAbnormal;
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
        Assert(GCPhysVmcs != NIL_RTGCPHYS); /* Paranoia. */
        pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState = fVmcsStateClear;
        iemVmxCommitCurrentVmcsToMemory(pVCpu);
        Assert(!IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    }
    else
    {
        rcStrict = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), GCPtrVmcs + RT_OFFSETOF(VMXVVMCS, fVmcsState),
                                            (const void *)&fVmcsStateClear, sizeof(fVmcsStateClear));
    }

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmclear_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return rcStrict;
}


/**
 * VMPTRST instruction execution worker.
 *
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
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (CPUMGetGuestCPL(pVCpu) > 0)
    {
        Log(("vmptrst: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrst_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Set the VMCS pointer to the location specified by the destination memory operand. */
    AssertCompile(NIL_RTGCPHYS == ~(RTGCPHYS)0U);
    VBOXSTRICTRC rcStrict = iemMemStoreDataU64(pVCpu, iEffSeg, GCPtrVmcs, IEM_VMX_GET_CURRENT_VMCS(pVCpu));
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrst_Success;
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return rcStrict;
    }

    Log(("vmptrst: Failed to store VMCS pointer to memory at destination operand %#Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrst_PtrMap;
    return rcStrict;
}


/**
 * VMPTRLD instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   GCPtrVmcs       The linear address of the current VMCS pointer.
 * @param   pExitInfo       Pointer to the virtual VM-exit information struct.
 *                          Optional, can be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmptrld(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (CPUMGetGuestCPL(pVCpu) > 0)
    {
        Log(("vmptrld: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmptrld: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmptrld: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    Assert(!VMX_V_VMCS_PHYSADDR_4G_LIMIT);
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth)
    {
        Log(("vmptrld: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmptrld: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmptrld: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrAbnormal;
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
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_PtrReadPhys;
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
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_VmcsRevId;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        Log(("vmptrld: Shadow VMCS -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_ShadowVmcs;
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
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmptrld_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMXON instruction execution worker.
 *
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
    if (!IEM_IS_VMX_ROOT_MODE(pVCpu))
    {
        /* CPL. */
        if (pVCpu->iem.s.uCpl > 0)
        {
            Log(("vmxon: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cpl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* A20M (A20 Masked) mode. */
        if (!PGMPhysIsA20Enabled(pVCpu))
        {
            Log(("vmxon: A20M mode -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_A20M;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR0 fixed bits. */
        bool const     fUnrestrictedGuest = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxUnrestrictedGuest;
        uint64_t const uCr0Fixed0         = fUnrestrictedGuest ? VMX_V_CR0_FIXED0_UX : VMX_V_CR0_FIXED0;
        if ((pVCpu->cpum.GstCtx.cr0 & uCr0Fixed0) != uCr0Fixed0)
        {
            Log(("vmxon: CR0 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cr0Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR4 fixed bits. */
        if ((pVCpu->cpum.GstCtx.cr4 & VMX_V_CR4_FIXED0) != VMX_V_CR4_FIXED0)
        {
            Log(("vmxon: CR4 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Cr4Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Feature control MSR's LOCK and VMXON bits. */
        uint64_t const uMsrFeatCtl = CPUMGetGuestIa32FeatureControl(pVCpu);
        if (!(uMsrFeatCtl & (MSR_IA32_FEATURE_CONTROL_LOCK | MSR_IA32_FEATURE_CONTROL_VMXON)))
        {
            Log(("vmxon: Feature control lock bit or VMXON bit cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_MsrFeatCtl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Get the VMXON pointer from the location specified by the source memory operand. */
        RTGCPHYS GCPhysVmxon;
        VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmxon, iEffSeg, GCPtrVmxon);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmxon: Failed to read VMXON region physaddr from %#RGv, rc=%Rrc\n", GCPtrVmxon, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrMap;
            return rcStrict;
        }

        /* VMXON region pointer alignment. */
        if (GCPhysVmxon & X86_PAGE_4K_OFFSET_MASK)
        {
            Log(("vmxon: VMXON region pointer not page-aligned -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrAlign;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* VMXON physical-address width limits. */
        Assert(!VMX_V_VMCS_PHYSADDR_4G_LIMIT);
        if (GCPhysVmxon >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth)
        {
            Log(("vmxon: VMXON region pointer extends beyond physical-address width -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrWidth;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Ensure VMXON region is not MMIO, ROM etc. This is not an Intel requirement but a
           restriction imposed by our implementation. */
        if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmxon))
        {
            Log(("vmxon: VMXON region not normal memory -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrAbnormal;
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
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_PtrReadPhys;
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
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmcsRevId;
                iemVmxVmFailInvalid(pVCpu);
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
                return VINF_SUCCESS;
            }

            /* Shadow VMCS disallowed. */
            Log(("vmxon: Shadow VMCS -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_ShadowVmcs;
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
        /** @todo NSTVMX: clear address-range monitoring. */
        /** @todo NSTVMX: Intel PT. */
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_Success;
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
        return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, true);
# else
        return VINF_SUCCESS;
# endif
    }
    else if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }

    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxon: In VMX root mode: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmxRootCpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMXON when already in VMX root mode. */
    iemVmxVmFail(pVCpu, VMXINSTRERR_VMXON_IN_VMXROOTMODE);
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxon_VmxRoot;
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
#endif
}


/**
 * Implements 'VMXON'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmxon, uint8_t, iEffSeg, RTGCPTR, GCPtrVmxon)
{
    return iemVmxVmxon(pVCpu, cbInstr, iEffSeg, GCPtrVmxon, NULL /* pExitInfo */);
}


/**
 * Implements 'VMXOFF'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmxoff)
{
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF2(pVCpu, cbInstr);
    return VINF_EM_RAW_EMULATE_INSTR;
# else
    IEM_VMX_INSTR_COMMON_CHECKS(pVCpu, "vmxoff", kVmxVInstrDiag_Vmxoff);
    if (!IEM_IS_VMX_ROOT_MODE(pVCpu))
    {
        Log(("vmxoff: Not in VMX root mode -> #GP(0)\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_VmxRoot;
        return iemRaiseUndefinedOpcode(pVCpu);
    }

    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        /** @todo NSTVMX: intercept. */
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxoff: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_Cpl;
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

    /*
     * Record that we're no longer in VMX root operation, block INIT, block and disable A20M.
     */
    pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = false;
    Assert(!pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode);

    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VMXOFF_UNBLOCK_SMI)
    { /** @todo NSTVMX: Unblock SMI. */ }
    /** @todo NSTVMX: Unblock and enable A20M. */
    /** @todo NSTVMX: Clear address-range monitoring. */

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmInstrDiag = kVmxVInstrDiag_Vmxoff_Success;
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
IEM_CIMPL_DEF_2(iemCImpl_vmwrite_reg, uint64_t, u64Val, uint32_t, uFieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, UINT8_MAX /*iEffSeg*/, IEMMODE_64BIT /* N/A */, u64Val, uFieldEnc, NULL /* pExitInfo */);
}


/**
 * Implements 'VMWRITE' memory.
 */
IEM_CIMPL_DEF_4(iemCImpl_vmwrite_mem, uint8_t, iEffSeg, IEMMODE, enmEffAddrMode, RTGCPTR, GCPtrVal, uint32_t, uFieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, iEffSeg, enmEffAddrMode, GCPtrVal, uFieldEnc,  NULL /* pExitInfo */);
}

#endif

