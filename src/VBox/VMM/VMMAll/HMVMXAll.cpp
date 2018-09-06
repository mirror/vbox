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
#include <VBox/vmm/pdmapi.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#define VMX_INSTR_DIAG_DESC(a_Def, a_Desc)      #a_Def " - " #a_Desc
static const char * const g_apszVmxInstrDiagDesc[kVmxVInstrDiag_Last] =
{
    /* Internal processing errors. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_1                            , "Ipe_1"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_2                            , "Ipe_2"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_3                            , "Ipe_3"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_4                            , "Ipe_4"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_5                            , "Ipe_5"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_6                            , "Ipe_6"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_7                            , "Ipe_7"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_8                            , "Ipe_8"                   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Ipe_9                            , "Ipe_9"                   ),
    /* VMXON. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_A20M                       , "A20M"                    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cpl                        , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cr0Fixed0                  , "Cr0Fixed0"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Cr4Fixed0                  , "Cr4Fixed0"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Intercept                  , "Intercept"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_LongModeCS                 , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_MsrFeatCtl                 , "MsrFeatCtl"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrAbnormal                , "PtrAbnormal"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrAlign                   , "PtrAlign"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrMap                     , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrPhysRead                , "PtrPhysRead"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_PtrWidth                   , "PtrWidth"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_RealOrV86Mode              , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Success                    , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_ShadowVmcs                 , "ShadowVmcs"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmxAlreadyRoot             , "VmxAlreadyRoot"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_Vmxe                       , "Vmxe"                    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmcsRevId                  , "VmcsRevId"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxon_VmxRootCpl                 , "VmxRootCpl"              ),
    /* VMXOFF. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Cpl                       , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Intercept                 , "Intercept"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_LongModeCS                , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_RealOrV86Mode             , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Success                   , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_Vmxe                      , "Vmxe"                    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmxoff_VmxRoot                   , "VmxRoot"                 ),
    /* VMPTRLD. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_Cpl                      , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_LongModeCS               , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrAbnormal              , "PtrAbnormal"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrAlign                 , "PtrAlign"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrMap                   , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrReadPhys              , "PtrReadPhys"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrVmxon                 , "PtrVmxon"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_PtrWidth                 , "PtrWidth"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_RealOrV86Mode            , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_ShadowVmcs               , "ShadowVmcs"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_Success                  , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_VmcsRevId                , "VmcsRevId"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrld_VmxRoot                  , "VmxRoot"                 ),
    /* VMPTRST. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_Cpl                      , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_LongModeCS               , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_PtrMap                   , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_RealOrV86Mode            , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_Success                  , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmptrst_VmxRoot                  , "VmxRoot"                 ),
    /* VMCLEAR. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_Cpl                      , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_LongModeCS               , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrAbnormal              , "PtrAbnormal"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrAlign                 , "PtrAlign"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrMap                   , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrReadPhys              , "PtrReadPhys"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrVmxon                 , "PtrVmxon"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_PtrWidth                 , "PtrWidth"                ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_RealOrV86Mode            , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_Success                  , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmclear_VmxRoot                  , "VmxRoot"                 ),
    /* VMWRITE. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_Cpl                      , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_FieldInvalid             , "FieldInvalid"            ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_FieldRo                  , "FieldRo"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_LinkPtrInvalid           , "LinkPtrInvalid"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_LongModeCS               , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_PtrInvalid               , "PtrInvalid"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_PtrMap                   , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_RealOrV86Mode            , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_Success                  , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmwrite_VmxRoot                  , "VmxRoot"                 ),
    /* VMREAD. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_Cpl                       , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_FieldInvalid              , "FieldInvalid"            ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_LinkPtrInvalid            , "LinkPtrInvalid"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_LongModeCS                , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_PtrInvalid                , "PtrInvalid"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_PtrMap                    , "PtrMap"                  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_RealOrV86Mode             , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_Success                   , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmread_VmxRoot                   , "VmxRoot"                 ),
    /* VMLAUNCH/VMRESUME. */
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrApicAccess           , "AddrApicAccess"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrEntryMsrLoad         , "AddrEntryMsrLoad"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrExitMsrLoad          , "AddrExitMsrLoad"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrExitMsrStore         , "AddrExitMsrStore"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrIoBitmapA            , "AddrIoBitmapA"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrIoBitmapB            , "AddrIoBitmapB"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrMsrBitmap            , "AddrMsrBitmap"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrVirtApicPage         , "AddrVirtApicPage"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrVmreadBitmap         , "AddrVmreadBitmap"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_AddrVmwriteBitmap        , "AddrVmwriteBitmap"       ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ApicRegVirt              , "ApicRegVirt"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_BlocKMovSS               , "BlockMovSS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_Cpl                      , "Cpl"                     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_Cr3TargetCount           , "Cr3TargetCount"          ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryCtlsAllowed1        , "EntryCtlsAllowed1"       ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryCtlsDisallowed0     , "EntryCtlsDisallowed0"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostCr0Fixed0       , "EntryHostCr0Fixed0"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostCr0Fixed1       , "EntryHostCr0Fixed1"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostCr3             , "EntryHostCr3"            ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostCr4Fixed0       , "EntryHostCr4Fixed0"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostCr4Fixed1       , "EntryHostCr4Fixed1"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostSysenterEspEip  , "EntryHostSysenterEspEip" ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryHostPatMsr          , "EntryHostPatMsr"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryInstrLen            , "EntryInstrLen"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryInstrLenZero        , "EntryInstrLenZero"       ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryIntInfoErrCodePe    , "EntryIntInfoErrCodePe"   ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryIntInfoErrCodeVec   , "EntryIntInfoErrCodeVec"  ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryIntInfoTypeVecRsvd  , "EntryIntInfoTypeVecRsvd" ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_EntryXcptErrCodeRsvd     , "EntryXcptErrCodeRsvd"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ExitCtlsAllowed1         , "ExitCtlsAllowed1"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ExitCtlsDisallowed0      , "ExitCtlsDisallowed0"     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_LongModeCS               , "LongModeCS"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_NmiWindowExit            , "NmiWindowExit"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_PinCtlsAllowed1          , "PinCtlsAllowed1"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_PinCtlsDisallowed0       , "PinCtlsDisallowed0"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ProcCtlsAllowed1         , "ProcCtlsAllowed1"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ProcCtlsDisallowed0      , "ProcCtlsDisallowed0"     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ProcCtls2Allowed1        , "ProcCtls2Allowed1"       ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_ProcCtls2Disallowed0     , "ProcCtls2Disallowed0"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_PtrInvalid               , "PtrInvalid"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_PtrReadPhys              , "PtrReadPhys"             ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_RealOrV86Mode            , "RealOrV86Mode"           ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_SavePreemptTimer         , "SavePreemptTimer"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_Success                  , "Success"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_TprThreshold             , "TprThreshold"            ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_TprThresholdVTpr         , "TprThresholdVTpr"        ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VirtApicPagePtrReadPhys  , "VirtApicPageReadPhys"    ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VirtIntDelivery          , "VirtIntDelivery"         ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VirtNmi                  , "VirtNmi"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VirtX2ApicTprShadow      , "VirtX2ApicTprShadow"     ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VirtX2ApicVirtApic       , "VirtX2ApicVirtApic"      ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VmcsClear                , "VmcsClear"               ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VmcsLaunch               , "VmcsLaunch"              ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_VmxRoot                  , "VmxRoot"                 ),
    VMX_INSTR_DIAG_DESC(kVmxVInstrDiag_Vmentry_Vpid                     , "Vpid"                    )
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


/**
 * Checks if a code selector (CS) is suitable for execution using hardware-assisted
 * VMX when unrestricted execution isn't available.
 *
 * @returns true if selector is suitable for VMX, otherwise
 *        false.
 * @param   pSel        Pointer to the selector to check (CS).
 * @param   uStackDpl   The CPL, aka the DPL of the stack segment.
 */
static bool hmVmxIsCodeSelectorOk(PCCPUMSELREG pSel, unsigned uStackDpl)
{
    /*
     * Segment must be an accessed code segment, it must be present and it must
     * be usable.
     * Note! These are all standard requirements and if CS holds anything else
     *       we've got buggy code somewhere!
     */
    AssertCompile(X86DESCATTR_TYPE == 0xf);
    AssertMsgReturn(   (pSel->Attr.u & (X86_SEL_TYPE_ACCESSED | X86_SEL_TYPE_CODE | X86DESCATTR_DT | X86DESCATTR_P | X86DESCATTR_UNUSABLE))
                    ==                 (X86_SEL_TYPE_ACCESSED | X86_SEL_TYPE_CODE | X86DESCATTR_DT | X86DESCATTR_P),
                    ("%#x\n", pSel->Attr.u),
                    false);

    /* For conforming segments, CS.DPL must be <= SS.DPL, while CS.DPL
       must equal SS.DPL for non-confroming segments.
       Note! This is also a hard requirement like above. */
    AssertMsgReturn(  pSel->Attr.n.u4Type & X86_SEL_TYPE_CONF
                    ? pSel->Attr.n.u2Dpl <= uStackDpl
                    : pSel->Attr.n.u2Dpl == uStackDpl,
                    ("u4Type=%#x u2Dpl=%u uStackDpl=%u\n", pSel->Attr.n.u4Type, pSel->Attr.n.u2Dpl, uStackDpl),
                    false);

    /*
     * The following two requirements are VT-x specific:
     *  - G bit must be set if any high limit bits are set.
     *  - G bit must be clear if any low limit bits are clear.
     */
    if (   ((pSel->u32Limit & 0xfff00000) == 0x00000000 ||  pSel->Attr.n.u1Granularity)
        && ((pSel->u32Limit & 0x00000fff) == 0x00000fff || !pSel->Attr.n.u1Granularity))
        return true;
    return false;
}


/**
 * Checks if a data selector (DS/ES/FS/GS) is suitable for execution using
 * hardware-assisted VMX when unrestricted execution isn't available.
 *
 * @returns true if selector is suitable for VMX, otherwise
 *        false.
 * @param   pSel        Pointer to the selector to check
 *                      (DS/ES/FS/GS).
 */
static bool hmVmxIsDataSelectorOk(PCCPUMSELREG pSel)
{
    /*
     * Unusable segments are OK.  These days they should be marked as such, as
     * but as an alternative we for old saved states and AMD<->VT-x migration
     * we also treat segments with all the attributes cleared as unusable.
     */
    if (pSel->Attr.n.u1Unusable || !pSel->Attr.u)
        return true;

    /** @todo tighten these checks. Will require CPUM load adjusting. */

    /* Segment must be accessed. */
    if (pSel->Attr.u & X86_SEL_TYPE_ACCESSED)
    {
        /* Code segments must also be readable. */
        if (  !(pSel->Attr.u & X86_SEL_TYPE_CODE)
            || (pSel->Attr.u & X86_SEL_TYPE_READ))
        {
            /* The S bit must be set. */
            if (pSel->Attr.n.u1DescType)
            {
                /* Except for conforming segments, DPL >= RPL. */
                if (   pSel->Attr.n.u2Dpl  >= (pSel->Sel & X86_SEL_RPL)
                    || pSel->Attr.n.u4Type >= X86_SEL_TYPE_ER_ACC)
                {
                    /* Segment must be present. */
                    if (pSel->Attr.n.u1Present)
                    {
                        /*
                         * The following two requirements are VT-x specific:
                         *   - G bit must be set if any high limit bits are set.
                         *   - G bit must be clear if any low limit bits are clear.
                         */
                        if (   ((pSel->u32Limit & 0xfff00000) == 0x00000000 ||  pSel->Attr.n.u1Granularity)
                            && ((pSel->u32Limit & 0x00000fff) == 0x00000fff || !pSel->Attr.n.u1Granularity))
                            return true;
                    }
                }
            }
        }
    }

    return false;
}


/**
 * Checks if the stack selector (SS) is suitable for execution using
 * hardware-assisted VMX when unrestricted execution isn't available.
 *
 * @returns true if selector is suitable for VMX, otherwise
 *        false.
 * @param   pSel        Pointer to the selector to check (SS).
 */
static bool hmVmxIsStackSelectorOk(PCCPUMSELREG pSel)
{
    /*
     * Unusable segments are OK.  These days they should be marked as such, as
     * but as an alternative we for old saved states and AMD<->VT-x migration
     * we also treat segments with all the attributes cleared as unusable.
     */
    /** @todo r=bird: actually all zeroes isn't gonna cut it... SS.DPL == CPL. */
    if (pSel->Attr.n.u1Unusable || !pSel->Attr.u)
        return true;

    /*
     * Segment must be an accessed writable segment, it must be present.
     * Note! These are all standard requirements and if SS holds anything else
     *       we've got buggy code somewhere!
     */
    AssertCompile(X86DESCATTR_TYPE == 0xf);
    AssertMsgReturn(   (pSel->Attr.u & (X86_SEL_TYPE_ACCESSED | X86_SEL_TYPE_WRITE | X86DESCATTR_DT | X86DESCATTR_P | X86_SEL_TYPE_CODE))
                    ==                 (X86_SEL_TYPE_ACCESSED | X86_SEL_TYPE_WRITE | X86DESCATTR_DT | X86DESCATTR_P),
                    ("%#x\n", pSel->Attr.u), false);

    /* DPL must equal RPL.
       Note! This is also a hard requirement like above. */
    AssertMsgReturn(pSel->Attr.n.u2Dpl == (pSel->Sel & X86_SEL_RPL),
                    ("u2Dpl=%u Sel=%#x\n", pSel->Attr.n.u2Dpl, pSel->Sel), false);

    /*
     * The following two requirements are VT-x specific:
     *   - G bit must be set if any high limit bits are set.
     *   - G bit must be clear if any low limit bits are clear.
     */
    if (   ((pSel->u32Limit & 0xfff00000) == 0x00000000 ||  pSel->Attr.n.u1Granularity)
        && ((pSel->u32Limit & 0x00000fff) == 0x00000fff || !pSel->Attr.n.u1Granularity))
        return true;
    return false;
}


/**
 * Checks if the guest is in a suitable state for hardware-assisted VMX execution.
 *
 * @returns @c true if it is suitable, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   pCtx    Pointer to the guest CPU context.
 *
 * @remarks @a pCtx can be a partial context and thus may not be necessarily the
 *          same as pVCpu->cpum.GstCtx! Thus don't eliminate the @a pCtx parameter.
 *          Secondly, if additional checks are added that require more of the CPU
 *          state, make sure REM (which supplies a partial state) is updated.
 */
VMM_INT_DECL(bool) HMVmxCanExecuteGuest(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(HMIsEnabled(pVM));
    Assert(!CPUMIsGuestVmxEnabled(pCtx));
    Assert(   ( pVM->hm.s.vmx.fUnrestrictedGuest && !pVM->hm.s.vmx.pRealModeTSS)
           || (!pVM->hm.s.vmx.fUnrestrictedGuest && pVM->hm.s.vmx.pRealModeTSS));

    pVCpu->hm.s.fActive = false;

    bool const fSupportsRealMode = pVM->hm.s.vmx.fUnrestrictedGuest || PDMVmmDevHeapIsEnabled(pVM);
    if (!pVM->hm.s.vmx.fUnrestrictedGuest)
    {
        /*
         * The VMM device heap is a requirement for emulating real mode or protected mode without paging with the unrestricted
         * guest execution feature is missing (VT-x only).
         */
        if (fSupportsRealMode)
        {
            if (CPUMIsGuestInRealModeEx(pCtx))
            {
                /*
                 * In V86 mode (VT-x or not), the CPU enforces real-mode compatible selector
                 * bases and limits, i.e. limit must be 64K and base must be selector * 16.
                 * If this is not true, we cannot execute real mode as V86 and have to fall
                 * back to emulation.
                 */
                if (   pCtx->cs.Sel != (pCtx->cs.u64Base >> 4)
                    || pCtx->ds.Sel != (pCtx->ds.u64Base >> 4)
                    || pCtx->es.Sel != (pCtx->es.u64Base >> 4)
                    || pCtx->ss.Sel != (pCtx->ss.u64Base >> 4)
                    || pCtx->fs.Sel != (pCtx->fs.u64Base >> 4)
                    || pCtx->gs.Sel != (pCtx->gs.u64Base >> 4))
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadRmSelBase);
                    return false;
                }
                if (   (pCtx->cs.u32Limit != 0xffff)
                    || (pCtx->ds.u32Limit != 0xffff)
                    || (pCtx->es.u32Limit != 0xffff)
                    || (pCtx->ss.u32Limit != 0xffff)
                    || (pCtx->fs.u32Limit != 0xffff)
                    || (pCtx->gs.u32Limit != 0xffff))
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadRmSelLimit);
                    return false;
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckRmOk);
            }
            else
            {
                /*
                 * Verify the requirements for executing code in protected mode. VT-x can't
                 * handle the CPU state right after a switch from real to protected mode
                 * (all sorts of RPL & DPL assumptions).
                 */
                if (pVCpu->hm.s.vmx.fWasInRealMode)
                {
                    /** @todo If guest is in V86 mode, these checks should be different! */
                    if ((pCtx->cs.Sel & X86_SEL_RPL) != (pCtx->ss.Sel & X86_SEL_RPL))
                    {
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadRpl);
                        return false;
                    }
                    if (   !hmVmxIsCodeSelectorOk(&pCtx->cs, pCtx->ss.Attr.n.u2Dpl)
                        || !hmVmxIsDataSelectorOk(&pCtx->ds)
                        || !hmVmxIsDataSelectorOk(&pCtx->es)
                        || !hmVmxIsDataSelectorOk(&pCtx->fs)
                        || !hmVmxIsDataSelectorOk(&pCtx->gs)
                        || !hmVmxIsStackSelectorOk(&pCtx->ss))
                    {
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadSel);
                        return false;
                    }
                }
                /* VT-x also chokes on invalid TR or LDTR selectors (minix). */
                if (pCtx->gdtr.cbGdt)
                {
                    if ((pCtx->tr.Sel | X86_SEL_RPL_LDT) > pCtx->gdtr.cbGdt)
                    {
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadTr);
                        return false;
                    }
                    else if ((pCtx->ldtr.Sel | X86_SEL_RPL_LDT) > pCtx->gdtr.cbGdt)
                    {
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckBadLdt);
                        return false;
                    }
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatVmxCheckPmOk);
            }
        }
        else
        {
            if (   !CPUMIsGuestInLongModeEx(pCtx)
                && !pVM->hm.s.vmx.fUnrestrictedGuest)
            {
                if (   !pVM->hm.s.fNestedPaging        /* Requires a fake PD for real *and* protected mode without paging - stored in the VMM device heap */
                    ||  CPUMIsGuestInRealModeEx(pCtx)) /* Requires a fake TSS for real mode - stored in the VMM device heap */
                    return false;

                /* Too early for VT-x; Solaris guests will fail with a guru meditation otherwise; same for XP. */
                if (pCtx->idtr.pIdt == 0 || pCtx->idtr.cbIdt == 0 || pCtx->tr.Sel == 0)
                    return false;

                /*
                 * The guest is about to complete the switch to protected mode. Wait a bit longer.
                 * Windows XP; switch to protected mode; all selectors are marked not present
                 * in the hidden registers (possible recompiler bug; see load_seg_vm).
                 */
                /** @todo Is this supposed recompiler bug still relevant with IEM? */
                if (pCtx->cs.Attr.n.u1Present == 0)
                    return false;
                if (pCtx->ss.Attr.n.u1Present == 0)
                    return false;

                /*
                 * Windows XP: possible same as above, but new recompiler requires new
                 * heuristics? VT-x doesn't seem to like something about the guest state and
                 * this stuff avoids it.
                 */
                /** @todo This check is actually wrong, it doesn't take the direction of the
                 *        stack segment into account. But, it does the job for now. */
                if (pCtx->rsp >= pCtx->ss.u32Limit)
                    return false;
            }
        }
    }

    if (pVM->hm.s.vmx.fEnabled)
    {
        uint32_t uCr0Mask;

        /* If bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        uCr0Mask = (uint32_t)pVM->hm.s.vmx.Msrs.u64Cr0Fixed0;

        /* We ignore the NE bit here on purpose; see HMR0.cpp for details. */
        uCr0Mask &= ~X86_CR0_NE;

        if (fSupportsRealMode)
        {
            /* We ignore the PE & PG bits here on purpose; we emulate real and protected mode without paging. */
            uCr0Mask &= ~(X86_CR0_PG|X86_CR0_PE);
        }
        else
        {
            /* We support protected mode without paging using identity mapping. */
            uCr0Mask &= ~X86_CR0_PG;
        }
        if ((pCtx->cr0 & uCr0Mask) != uCr0Mask)
            return false;

        /* If bit N is cleared in cr0_fixed1, then it must be zero in the guest's cr0. */
        uCr0Mask = (uint32_t)~pVM->hm.s.vmx.Msrs.u64Cr0Fixed1;
        if ((pCtx->cr0 & uCr0Mask) != 0)
            return false;

        /* If bit N is set in cr4_fixed0, then it must be set in the guest's cr4. */
        uCr0Mask  = (uint32_t)pVM->hm.s.vmx.Msrs.u64Cr4Fixed0;
        uCr0Mask &= ~X86_CR4_VMXE;
        if ((pCtx->cr4 & uCr0Mask) != uCr0Mask)
            return false;

        /* If bit N is cleared in cr4_fixed1, then it must be zero in the guest's cr4. */
        uCr0Mask = (uint32_t)~pVM->hm.s.vmx.Msrs.u64Cr4Fixed1;
        if ((pCtx->cr4 & uCr0Mask) != 0)
            return false;

        pVCpu->hm.s.fActive = true;
        return true;
    }

    return false;
}

