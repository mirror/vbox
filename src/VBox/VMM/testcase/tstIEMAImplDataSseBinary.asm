; $Id$
;; @file
; tstIEMAImplDataSseBinary - Test data for SSE binary instructions.
;

;
; Copyright (C) 2022 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "iprt/x86.mac"


BEGINCONST

;;
; Generate the include statement
;
; @param    1       The instruction handler
; @param    2       The filename
;
%macro IEM_TEST_DATA 2
EXPORTEDNAME g_aTests_ %+ %1
        incbin %2
end_g_aTests_ %+ %1:
EXPORTEDNAME g_cbTests_ %+ %1
        dd  end_g_aTests_ %+ %1 - NAME(g_aTests_ %+ %1)

%ifdef ASM_FORMAT_ELF
size g_aTests_ %+ %1  end_g_aTests_ %+ %1 - NAME(g_aTests_ %+ %1)
type g_aTests_ %+ %1  object
size g_cbTests_ %+ %1 4
type g_cbTests_ %+ %1 object
%endif
%endmacro

IEM_TEST_DATA addps_u128, "tstIEMAImplDataSseBinary-addps_u128.bin"
IEM_TEST_DATA mulps_u128, "tstIEMAImplDataSseBinary-mulps_u128.bin"
IEM_TEST_DATA subps_u128, "tstIEMAImplDataSseBinary-subps_u128.bin"
IEM_TEST_DATA minps_u128, "tstIEMAImplDataSseBinary-minps_u128.bin"
IEM_TEST_DATA divps_u128, "tstIEMAImplDataSseBinary-divps_u128.bin"
IEM_TEST_DATA maxps_u128, "tstIEMAImplDataSseBinary-maxps_u128.bin"

IEM_TEST_DATA addss_u128_r32, "tstIEMAImplDataSseBinary-addss_u128_r32.bin"
IEM_TEST_DATA mulss_u128_r32, "tstIEMAImplDataSseBinary-mulss_u128_r32.bin"
IEM_TEST_DATA subss_u128_r32, "tstIEMAImplDataSseBinary-subss_u128_r32.bin"
IEM_TEST_DATA minss_u128_r32, "tstIEMAImplDataSseBinary-minss_u128_r32.bin"
IEM_TEST_DATA divss_u128_r32, "tstIEMAImplDataSseBinary-divss_u128_r32.bin"

IEM_TEST_DATA addpd_u128, "tstIEMAImplDataSseBinary-addpd_u128.bin"
IEM_TEST_DATA mulpd_u128, "tstIEMAImplDataSseBinary-mulpd_u128.bin"
IEM_TEST_DATA subpd_u128, "tstIEMAImplDataSseBinary-subpd_u128.bin"
IEM_TEST_DATA minpd_u128, "tstIEMAImplDataSseBinary-minpd_u128.bin"
IEM_TEST_DATA divpd_u128, "tstIEMAImplDataSseBinary-divpd_u128.bin"
IEM_TEST_DATA maxpd_u128, "tstIEMAImplDataSseBinary-maxpd_u128.bin"

IEM_TEST_DATA addsd_u128_r64, "tstIEMAImplDataSseBinary-addsd_u128_r64.bin"
IEM_TEST_DATA mulsd_u128_r64, "tstIEMAImplDataSseBinary-mulsd_u128_r64.bin"
IEM_TEST_DATA subsd_u128_r64, "tstIEMAImplDataSseBinary-subsd_u128_r64.bin"
IEM_TEST_DATA minsd_u128_r64, "tstIEMAImplDataSseBinary-minsd_u128_r64.bin"
IEM_TEST_DATA divsd_u128_r64, "tstIEMAImplDataSseBinary-divsd_u128_r64.bin"
