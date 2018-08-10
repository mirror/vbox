/* $Id$ */
/** @file
 * HM VMX (VT-x) - All contexts.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#define VMCPU_INCL_CPUM_GST_CTX
#include "HMInternal.h"
#include <VBox/vmm/vm.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#define VMX_INSTR_DIAG_DESC(a_Def, a_Desc)      #a_Def " - " #a_Desc
static const char * const g_apszVmxInstrDiagDesc[kVmxVInstrDiag_Last] =
{
    /* Internal processing errors. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_1               , "Ipe_1"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_2               , "Ipe_2"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_3               , "Ipe_3"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_4               , "Ipe_4"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_5               , "Ipe_5"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_6               , "Ipe_6"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_7               , "Ipe_7"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_8               , "Ipe_8"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_9               , "Ipe_9"        ),
    /* VMXON. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_A20M          , "A20M"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cpl           , "Cpl"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cr0Fixed0     , "Cr0Fixed0"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cr4Fixed0     , "Cr4Fixed0"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Intercept     , "Intercept"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_LongModeCS    , "LongModeCS"   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_MsrFeatCtl    , "MsrFeatCtl"   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrAlign      , "PtrAlign"     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrAbnormal   , "PtrAbnormal"  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrMap        , "PtrMap"       ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrPhysRead   , "PtrPhysRead"  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrWidth      , "PtrWidth"     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_RealOrV86Mode , "RealOrV86Mode"),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Success       , "Success"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_ShadowVmcs    , "ShadowVmcs"   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Vmxe          , "Vmxe"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmcsRevId     , "VmcsRevId"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmxRoot       , "VmxRoot"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmxRootCpl    , "VmxRootCpl"   ),
    /* VMXOFF. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Cpl          , "Cpl"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Intercept    , "Intercept"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_LongModeCS   , "LongModeCS"   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_RealOrV86Mode, "RealOrV86Mode"),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Success      , "Success"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Vmxe         , "Vmxe"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_VmxRoot      , "VmxRoot"      )
    /* kVmxVInstrDiag_Last */
};
#undef VMX_INSTR_DIAG_DESC


/**
 * Gets a copy of the VMX host MSRs that were read by HM during ring-0
 * initialization.
 *
 * @return VBox status code.
 * @param   pVM        The cross context VM structure.
 * @param   pVmxMsrs   Where to store the VMXMSRS struct (only valid when
 *                     VINF_SUCCESS is returned).
 *
 * @remarks Caller needs to take care not to call this function too early. Call
 *          after HM initialization is fully complete.
 */
VMM_INT_DECL(int) HMVmxGetHostMsrs(PVM pVM, PVMXMSRS pVmxMsrs)
{
    AssertPtrReturn(pVM,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pVmxMsrs, VERR_INVALID_PARAMETER);
    if (pVM->hm.s.vmx.fSupported)
    {
        *pVmxMsrs = pVM->hm.s.vmx.Msrs;
        return VINF_SUCCESS;
    }
    return VERR_VMX_NOT_SUPPORTED;
}


/**
 * Gets the specified VMX host MSR that was read by HM during ring-0
 * initialization.
 *
 * @return VBox status code.
 * @param   pVM        The cross context VM structure.
 * @param   idMsr      The MSR.
 * @param   puValue    Where to store the MSR value (only updated when VINF_SUCCESS
 *                     is returned).
 *
 * @remarks Caller needs to take care not to call this function too early. Call
 *          after HM initialization is fully complete.
 */
VMM_INT_DECL(int) HMVmxGetHostMsr(PVM pVM, uint32_t idMsr, uint64_t *puValue)
{
    AssertPtrReturn(pVM,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(puValue, VERR_INVALID_PARAMETER);

    if (!pVM->hm.s.vmx.fSupported)
        return VERR_VMX_NOT_SUPPORTED;

    PCVMXMSRS pVmxMsrs = &pVM->hm.s.vmx.Msrs;
    switch (idMsr)
    {
        case MSR_IA32_FEATURE_CONTROL:         *puValue =  pVmxMsrs->u64FeatCtrl;      break;
        case MSR_IA32_VMX_BASIC:               *puValue =  pVmxMsrs->u64Basic;         break;
        case MSR_IA32_VMX_PINBASED_CTLS:       *puValue =  pVmxMsrs->PinCtls.u;        break;
        case MSR_IA32_VMX_PROCBASED_CTLS:      *puValue =  pVmxMsrs->ProcCtls.u;       break;
        case MSR_IA32_VMX_PROCBASED_CTLS2:     *puValue =  pVmxMsrs->ProcCtls2.u;      break;
        case MSR_IA32_VMX_EXIT_CTLS:           *puValue =  pVmxMsrs->ExitCtls.u;       break;
        case MSR_IA32_VMX_ENTRY_CTLS:          *puValue =  pVmxMsrs->EntryCtls.u;      break;
        case MSR_IA32_VMX_TRUE_PINBASED_CTLS:  *puValue =  pVmxMsrs->TruePinCtls.u;    break;
        case MSR_IA32_VMX_TRUE_PROCBASED_CTLS: *puValue =  pVmxMsrs->TrueProcCtls.u;   break;
        case MSR_IA32_VMX_TRUE_ENTRY_CTLS:     *puValue =  pVmxMsrs->TrueEntryCtls.u;  break;
        case MSR_IA32_VMX_TRUE_EXIT_CTLS:      *puValue =  pVmxMsrs->TrueExitCtls.u;   break;
        case MSR_IA32_VMX_MISC:                *puValue =  pVmxMsrs->u64Misc;          break;
        case MSR_IA32_VMX_CR0_FIXED0:          *puValue =  pVmxMsrs->u64Cr0Fixed0;     break;
        case MSR_IA32_VMX_CR0_FIXED1:          *puValue =  pVmxMsrs->u64Cr0Fixed1;     break;
        case MSR_IA32_VMX_CR4_FIXED0:          *puValue =  pVmxMsrs->u64Cr4Fixed0;     break;
        case MSR_IA32_VMX_CR4_FIXED1:          *puValue =  pVmxMsrs->u64Cr4Fixed1;     break;
        case MSR_IA32_VMX_VMCS_ENUM:           *puValue =  pVmxMsrs->u64VmcsEnum;      break;
        case MSR_IA32_VMX_VMFUNC:              *puValue =  pVmxMsrs->u64VmFunc;        break;
        case MSR_IA32_VMX_EPT_VPID_CAP:        *puValue =  pVmxMsrs->u64EptVpidCaps;   break;
        default:
        {
            AssertMsgFailed(("Invalid MSR %#x\n", idMsr));
            return VERR_NOT_FOUND;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Gets the description of a VMX instruction diagnostic enum member.
 *
 * @returns The descriptive string.
 * @param   enmInstrDiag    The VMX instruction diagnostic.
 */
VMM_INT_DECL(const char *) HMVmxGetInstrDiagDesc(VMXVINSTRDIAG enmInstrDiag)
{
    if (RT_LIKELY((unsigned)enmInstrDiag < RT_ELEMENTS(g_apszVmxInstrDiagDesc)))
        return g_apszVmxInstrDiagDesc[enmInstrDiag];
    return "Unknown/invalid";
}

