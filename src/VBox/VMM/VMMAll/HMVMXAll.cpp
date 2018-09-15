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
#define VMXV_DIAG_DESC(a_Def, a_Desc)      #a_Def " - " #a_Desc
/** VMX virtual-instructions and VM-exit diagnostics. */
static const char * const g_apszVmxVDiagDesc[] =
{
    /* Internal processing errors. */
    VMXV_DIAG_DESC(kVmxVDiag_None                             , "None"                      ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_1                            , "Ipe_1"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_2                            , "Ipe_2"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_3                            , "Ipe_3"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_4                            , "Ipe_4"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_5                            , "Ipe_5"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_6                            , "Ipe_6"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_7                            , "Ipe_7"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_8                            , "Ipe_8"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_9                            , "Ipe_9"                     ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_10                           , "Ipe_10"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_11                           , "Ipe_11"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_12                           , "Ipe_12"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_13                           , "Ipe_13"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_14                           , "Ipe_14"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_15                           , "Ipe_15"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Ipe_16                           , "Ipe_16"                    ),
    /* VMXON. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_A20M                       , "A20M"                      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_Cpl                        , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_Cr0Fixed0                  , "Cr0Fixed0"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_Cr4Fixed0                  , "Cr4Fixed0"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_Intercept                  , "Intercept"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_LongModeCS                 , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_MsrFeatCtl                 , "MsrFeatCtl"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_PtrAbnormal                , "PtrAbnormal"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_PtrAlign                   , "PtrAlign"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_PtrMap                     , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_PtrReadPhys                , "PtrReadPhys"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_PtrWidth                   , "PtrWidth"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_RealOrV86Mode              , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_ShadowVmcs                 , "ShadowVmcs"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_VmxAlreadyRoot             , "VmxAlreadyRoot"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_Vmxe                       , "Vmxe"                      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_VmcsRevId                  , "VmcsRevId"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxon_VmxRootCpl                 , "VmxRootCpl"                ),
    /* VMXOFF. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_Cpl                       , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_Intercept                 , "Intercept"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_LongModeCS                , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_RealOrV86Mode             , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_Vmxe                      , "Vmxe"                      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmxoff_VmxRoot                   , "VmxRoot"                   ),
    /* VMPTRLD. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_Cpl                      , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_LongModeCS               , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrAbnormal              , "PtrAbnormal"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrAlign                 , "PtrAlign"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrMap                   , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrReadPhys              , "PtrReadPhys"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrVmxon                 , "PtrVmxon"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_PtrWidth                 , "PtrWidth"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_RealOrV86Mode            , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_ShadowVmcs               , "ShadowVmcs"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_VmcsRevId                , "VmcsRevId"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrld_VmxRoot                  , "VmxRoot"                   ),
    /* VMPTRST. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrst_Cpl                      , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrst_LongModeCS               , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrst_PtrMap                   , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrst_RealOrV86Mode            , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmptrst_VmxRoot                  , "VmxRoot"                   ),
    /* VMCLEAR. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_Cpl                      , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_LongModeCS               , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrAbnormal              , "PtrAbnormal"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrAlign                 , "PtrAlign"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrMap                   , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrReadPhys              , "PtrReadPhys"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrVmxon                 , "PtrVmxon"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_PtrWidth                 , "PtrWidth"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_RealOrV86Mode            , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmclear_VmxRoot                  , "VmxRoot"                   ),
    /* VMWRITE. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_Cpl                      , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_FieldInvalid             , "FieldInvalid"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_FieldRo                  , "FieldRo"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_LinkPtrInvalid           , "LinkPtrInvalid"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_LongModeCS               , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_PtrInvalid               , "PtrInvalid"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_PtrMap                   , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_RealOrV86Mode            , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmwrite_VmxRoot                  , "VmxRoot"                   ),
    /* VMREAD. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_Cpl                       , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_FieldInvalid              , "FieldInvalid"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_LinkPtrInvalid            , "LinkPtrInvalid"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_LongModeCS                , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_PtrInvalid                , "PtrInvalid"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_PtrMap                    , "PtrMap"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_RealOrV86Mode             , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmread_VmxRoot                   , "VmxRoot"                   ),
    /* VMLAUNCH/VMRESUME. */
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrApicAccess           , "AddrApicAccess"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrEntryMsrLoad         , "AddrEntryMsrLoad"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrExitMsrLoad          , "AddrExitMsrLoad"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrExitMsrStore         , "AddrExitMsrStore"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrIoBitmapA            , "AddrIoBitmapA"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrIoBitmapB            , "AddrIoBitmapB"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrMsrBitmap            , "AddrMsrBitmap"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrVirtApicPage         , "AddrVirtApicPage"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrVmcsLinkPtr          , "AddrVmcsLinkPtr"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrVmreadBitmap         , "AddrVmreadBitmap"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_AddrVmwriteBitmap        , "AddrVmwriteBitmap"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ApicRegVirt              , "ApicRegVirt"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_BlocKMovSS               , "BlockMovSS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_Cpl                      , "Cpl"                       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_Cr3TargetCount           , "Cr3TargetCount"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryCtlsAllowed1        , "EntryCtlsAllowed1"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryCtlsDisallowed0     , "EntryCtlsDisallowed0"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryInstrLen            , "EntryInstrLen"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryInstrLenZero        , "EntryInstrLenZero"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryIntInfoErrCodePe    , "EntryIntInfoErrCodePe"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryIntInfoErrCodeVec   , "EntryIntInfoErrCodeVec"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryIntInfoTypeVecRsvd  , "EntryIntInfoTypeVecRsvd"   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_EntryXcptErrCodeRsvd     , "EntryXcptErrCodeRsvd"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ExitCtlsAllowed1         , "ExitCtlsAllowed1"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ExitCtlsDisallowed0      , "ExitCtlsDisallowed0"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestActStateHlt         , "GuestActStateHlt"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestActStateRsvd        , "GuestActStateRsvd"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestActStateShutdown    , "GuestActStateShutdown"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestActStateSsDpl       , "GuestActStateSsDpl"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestActStateStiMovSs    , "GuestActStateStiMovSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr0Fixed0           , "GuestCr0Fixed0"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr0Fixed1           , "GuestCr0Fixed1"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr0PgPe             , "GuestCr0PgPe"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr3                 , "GuestCr3"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr4Fixed0           , "GuestCr4Fixed0"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestCr4Fixed1           , "GuestCr4Fixed1"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestDebugCtl            , "GuestDebugCtl"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestDr7                 , "GuestDr7"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestEferMsr             , "GuestEferMsr"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestEferMsrRsvd         , "GuestEferMsrRsvd"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestGdtrBase            , "GuestGdtrBase"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestGdtrLimit           , "GuestGdtrLimit"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIdtrBase            , "GuestIdtrBase"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIdtrLimit           , "GuestIdtrLimit"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateEnclave     , "GuestIntStateEnclave"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateExtInt      , "GuestIntStateExtInt"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateNmi         , "GuestIntStateNmi"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateRFlagsSti   , "GuestIntStateRFlagsSti"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateRsvd        , "GuestIntStateRsvd"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateSmi         , "GuestIntStateSmi"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateStiMovSs    , "GuestIntStateStiMovSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestIntStateVirtNmi     , "GuestIntStateVirtNmi"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPae                 , "GuestPae"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPatMsr              , "GuestPatMsr"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPcide               , "GuestPcide"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPdpteCr3ReadPhys    , "GuestPdpteCr3ReadPhys"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPdpte0Rsvd          , "GuestPdpte0Rsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPdpte1Rsvd          , "GuestPdpte1Rsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPdpte2Rsvd          , "GuestPdpte2Rsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPdpte3Rsvd          , "GuestPdpte3Rsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPndDbgXcptBsNoTf    , "GuestPndDbgXcptBsNoTf"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPndDbgXcptBsTf      , "GuestPndDbgXcptBsTf"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPndDbgXcptRsvd      , "GuestPndDbgXcptRsvd"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestPndDbgXcptRtm       , "GuestPndDbgXcptRtm"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestRip                 , "GuestRip"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestRipRsvd             , "GuestRipRsvd"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestRFlagsIf            , "GuestRFlagsIf"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestRFlagsRsvd          , "GuestRFlagsRsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestRFlagsVm            , "GuestRFlagsVm"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsDefBig     , "GuestSegAttrCsDefBig"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsDplEqSs    , "GuestSegAttrCsDplEqSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsDplLtSs    , "GuestSegAttrCsDplLtSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsDplZero    , "GuestSegAttrCsDplZero"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsType       , "GuestSegAttrCsType"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrCsTypeRead   , "GuestSegAttrCsTypeRead"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeCs   , "GuestSegAttrDescTypeCs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeDs   , "GuestSegAttrDescTypeDs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeEs   , "GuestSegAttrDescTypeEs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeFs   , "GuestSegAttrDescTypeFs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeGs   , "GuestSegAttrDescTypeGs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDescTypeSs   , "GuestSegAttrDescTypeSs"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplCs     , "GuestSegAttrDplRplCs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplDs     , "GuestSegAttrDplRplDs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplEs     , "GuestSegAttrDplRplEs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplFs     , "GuestSegAttrDplRplFs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplGs     , "GuestSegAttrDplRplGs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrDplRplSs     , "GuestSegAttrDplRplSs"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranCs       , "GuestSegAttrGranCs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranDs       , "GuestSegAttrGranDs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranEs       , "GuestSegAttrGranEs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranFs       , "GuestSegAttrGranFs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranGs       , "GuestSegAttrGranGs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrGranSs       , "GuestSegAttrGranSs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrLdtrDescType , "GuestSegAttrLdtrDescType"  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrLdtrGran     , "GuestSegAttrLdtrGran"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrLdtrPresent  , "GuestSegAttrLdtrPresent"   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrLdtrRsvd     , "GuestSegAttrLdtrRsvd"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrLdtrType     , "GuestSegAttrLdtrType"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentCs    , "GuestSegAttrPresentCs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentDs    , "GuestSegAttrPresentDs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentEs    , "GuestSegAttrPresentEs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentFs    , "GuestSegAttrPresentFs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentGs    , "GuestSegAttrPresentGs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrPresentSs    , "GuestSegAttrPresentSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdCs       , "GuestSegAttrRsvdCs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdDs       , "GuestSegAttrRsvdDs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdEs       , "GuestSegAttrRsvdEs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdFs       , "GuestSegAttrRsvdFs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdGs       , "GuestSegAttrRsvdGs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrRsvdSs       , "GuestSegAttrRsvdSs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrSsDplEqRpl   , "GuestSegAttrSsDplEqRpl"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrSsDplZero    , "GuestSegAttrSsDplZero "    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrSsType       , "GuestSegAttrSsType"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrDescType   , "GuestSegAttrTrDescType"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrGran       , "GuestSegAttrTrGran"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrPresent    , "GuestSegAttrTrPresent"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrRsvd       , "GuestSegAttrTrRsvd"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrType       , "GuestSegAttrTrType"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTrUnusable   , "GuestSegAttrTrUnusable"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccCs    , "GuestSegAttrTypeAccCs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccDs    , "GuestSegAttrTypeAccDs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccEs    , "GuestSegAttrTypeAccEs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccFs    , "GuestSegAttrTypeAccFs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccGs    , "GuestSegAttrTypeAccGs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrTypeAccSs    , "GuestSegAttrTypeAccSs"     ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Cs        , "GuestSegAttrV86Cs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Ds        , "GuestSegAttrV86Ds"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Es        , "GuestSegAttrV86Es"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Fs        , "GuestSegAttrV86Fs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Gs        , "GuestSegAttrV86Gs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegAttrV86Ss        , "GuestSegAttrV86Ss"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseCs           , "GuestSegBaseCs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseDs           , "GuestSegBaseDs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseEs           , "GuestSegBaseEs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseFs           , "GuestSegBaseFs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseGs           , "GuestSegBaseGs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseLdtr         , "GuestSegBaseLdtr"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseSs           , "GuestSegBaseSs"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseTr           , "GuestSegBaseTr"            ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Cs        , "GuestSegBaseV86Cs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Ds        , "GuestSegBaseV86Ds"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Es        , "GuestSegBaseV86Es"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Fs        , "GuestSegBaseV86Fs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Gs        , "GuestSegBaseV86Gs"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegBaseV86Ss        , "GuestSegBaseV86Ss"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Cs       , "GuestSegLimitV86Cs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Ds       , "GuestSegLimitV86Ds"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Es       , "GuestSegLimitV86Es"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Fs       , "GuestSegLimitV86Fs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Gs       , "GuestSegLimitV86Gs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegLimitV86Ss       , "GuestSegLimitV86Ss"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegSelCsSsRpl       , "GuestSegSelCsSsRpl"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegSelLdtr          , "GuestSegSelLdtr"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSegSelTr            , "GuestSegSelTr"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_GuestSysenterEspEip      , "GuestSysenterEspEip"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsLinkPtrCurVmcs       , "VmcsLinkPtrCurVmcs"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsLinkPtrReadPhys      , "VmcsLinkPtrReadPhys"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsLinkPtrRevId         , "VmcsLinkPtrRevId"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsLinkPtrShadow        , "VmcsLinkPtrShadow"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr0Fixed0            , "HostCr0Fixed0"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr0Fixed1            , "HostCr0Fixed1"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr3                  , "HostCr3"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr4Fixed0            , "HostCr4Fixed0"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr4Fixed1            , "HostCr4Fixed1"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr4Pae               , "HostCr4Pae"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCr4Pcide             , "HostCr4Pcide"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostCsTr                 , "HostCsTr"                  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostEferMsr              , "HostEferMsr"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostEferMsrRsvd          , "HostEferMsrRsvd"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostGuestLongMode        , "HostGuestLongMode"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostGuestLongModeNoCpu   , "HostGuestLongModeNoCpu"    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostLongMode             , "HostLongMode"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostPatMsr               , "HostPatMsr"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostRip                  , "HostRip"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostRipRsvd              , "HostRipRsvd"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostSel                  , "HostSel"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostSegBase              , "HostSegBase"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostSs                   , "HostSs"                    ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_HostSysenterEspEip       , "HostSysenterEspEip"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_LongModeCS               , "LongModeCS"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_MsrLoad                  , "MsrLoad"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_MsrLoadCount             , "MsrLoadCount"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_MsrLoadPtrReadPhys       , "MsrLoadPtrReadPhys"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_MsrLoadRing3             , "MsrLoadRing3"              ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_MsrLoadRsvd              , "MsrLoadRsvd"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_NmiWindowExit            , "NmiWindowExit"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_PinCtlsAllowed1          , "PinCtlsAllowed1"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_PinCtlsDisallowed0       , "PinCtlsDisallowed0"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ProcCtlsAllowed1         , "ProcCtlsAllowed1"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ProcCtlsDisallowed0      , "ProcCtlsDisallowed0"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ProcCtls2Allowed1        , "ProcCtls2Allowed1"         ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_ProcCtls2Disallowed0     , "ProcCtls2Disallowed0"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_PtrInvalid               , "PtrInvalid"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_PtrReadPhys              , "PtrReadPhys"               ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_RealOrV86Mode            , "RealOrV86Mode"             ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_SavePreemptTimer         , "SavePreemptTimer"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_TprThresholdRsvd         , "TprThresholdRsvd"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_TprThresholdVTpr         , "TprThresholdVTpr"          ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VirtApicPagePtrReadPhys  , "VirtApicPageReadPhys"      ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VirtIntDelivery          , "VirtIntDelivery"           ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VirtNmi                  , "VirtNmi"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VirtX2ApicTprShadow      , "VirtX2ApicTprShadow"       ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VirtX2ApicVirtApic       , "VirtX2ApicVirtApic"        ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsClear                , "VmcsClear"                 ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmcsLaunch               , "VmcsLaunch"                ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmreadBitmapPtrReadPhys  , "VmreadBitmapPtrReadPhys"   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmwriteBitmapPtrReadPhys , "VmwriteBitmapPtrReadPhys"  ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_VmxRoot                  , "VmxRoot"                   ),
    VMXV_DIAG_DESC(kVmxVDiag_Vmentry_Vpid                     , "Vpid"                      )
    /* kVmxVDiag_End */
};
AssertCompile(RT_ELEMENTS(g_apszVmxVDiagDesc) == kVmxVDiag_End);
#undef VMXV_DIAG_DESC


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

    if (pVM->hm.s.vmx.fSupported)
    {
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
    return VERR_VMX_NOT_SUPPORTED;
}


/**
 * Gets the description of a VMX instruction/Vm-exit diagnostic.
 *
 * @returns The descriptive string.
 * @param   enmDiag    The VMX diagnostic.
 */
VMM_INT_DECL(const char *) HMVmxGetDiagDesc(VMXVDIAG enmDiag)
{
    if (RT_LIKELY((unsigned)enmDiag < RT_ELEMENTS(g_apszVmxVDiagDesc)))
        return g_apszVmxVDiagDesc[enmDiag];
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
            uCr0Mask &= ~(X86_CR0_PG | X86_CR0_PE);
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


/**
 * Injects an event using TRPM given a VM-entry interruption info. and related
 * fields.
 *
 * @returns VBox status code.
 * @param   uEntryIntInfo       The VM-entry interruption info.
 * @param   uErrCode            The error code associated with the event if any.
 * @param   cbInstr             The VM-entry instruction length (for software
 *                              interrupts and software exceptions). Pass 0
 *                              otherwise.
 * @param   GCPtrFaultAddress   The guest CR2 if this is a \#PF event.
 */
VMM_INT_DECL(int) HMVmxEntryIntInfoInjectTrpmEvent(PVMCPU pVCpu, uint32_t uEntryIntInfo, uint32_t uErrCode, uint32_t cbInstr,
                                                   RTGCUINTPTR GCPtrFaultAddress)
{
    Assert(VMX_ENTRY_INT_INFO_IS_VALID(uEntryIntInfo));

    uint8_t const uType         = VMX_ENTRY_INT_INFO_TYPE(uEntryIntInfo);
    uint8_t const uVector       = VMX_ENTRY_INT_INFO_VECTOR(uEntryIntInfo);
    bool const    fErrCodeValid = VMX_ENTRY_INT_INFO_IS_ERROR_CODE_VALID(uEntryIntInfo);

    TRPMEVENT enmTrapType;
    switch (uType)
    {
        case VMX_ENTRY_INT_INFO_TYPE_EXT_INT:
           enmTrapType = TRPM_HARDWARE_INT;
           break;

        case VMX_ENTRY_INT_INFO_TYPE_SW_INT:
            enmTrapType = TRPM_SOFTWARE_INT;
            break;

        case VMX_ENTRY_INT_INFO_TYPE_NMI:
        case VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT:  /* ICEBP. */
        case VMX_ENTRY_INT_INFO_TYPE_SW_XCPT:       /* #BP and #OF */
        case VMX_ENTRY_INT_INFO_TYPE_HW_XCPT:
            enmTrapType = TRPM_TRAP;
            break;

        default:
            /* Shouldn't really happen. */
            AssertMsgFailedReturn(("Invalid trap type %#x\n", uType), VERR_VMX_IPE_4);
            break;
    }

    int rc = TRPMAssertTrap(pVCpu, uVector, enmTrapType);
    AssertRCReturn(rc, rc);

    if (fErrCodeValid)
        TRPMSetErrorCode(pVCpu, uErrCode);

    if (   uType   == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
        && uVector == X86_XCPT_PF)
        TRPMSetFaultAddress(pVCpu, GCPtrFaultAddress);
    else if (   uType == VMX_ENTRY_INT_INFO_TYPE_SW_INT
             || uType == VMX_ENTRY_INT_INFO_TYPE_SW_XCPT
             || uType == VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT)
    {
        AssertMsg(   uType == VMX_IDT_VECTORING_INFO_TYPE_SW_INT
                  || (uVector == X86_XCPT_BP || uVector == X86_XCPT_OF),
                  ("Invalid vector: uVector=%#x uVectorType=%#x\n", uVector, uType));
        TRPMSetInstrLength(pVCpu, cbInstr);
    }

    return VINF_SUCCESS;
}

