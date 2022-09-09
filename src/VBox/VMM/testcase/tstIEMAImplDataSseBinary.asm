; $Id$
;; @file
; tstIEMAImplDataSseBinary - Test data for SSE binary instructions.
;

;
; Copyright (C) 2022 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; SPDX-License-Identifier: GPL-3.0-only
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

IEM_TEST_DATA addps_u128,           "tstIEMAImplDataSseBinary-addps_u128.bin"
IEM_TEST_DATA mulps_u128,           "tstIEMAImplDataSseBinary-mulps_u128.bin"
IEM_TEST_DATA subps_u128,           "tstIEMAImplDataSseBinary-subps_u128.bin"
IEM_TEST_DATA minps_u128,           "tstIEMAImplDataSseBinary-minps_u128.bin"
IEM_TEST_DATA divps_u128,           "tstIEMAImplDataSseBinary-divps_u128.bin"
IEM_TEST_DATA maxps_u128,           "tstIEMAImplDataSseBinary-maxps_u128.bin"
IEM_TEST_DATA haddps_u128,          "tstIEMAImplDataSseBinary-haddps_u128.bin"
IEM_TEST_DATA hsubps_u128,          "tstIEMAImplDataSseBinary-hsubps_u128.bin"
IEM_TEST_DATA sqrtps_u128,          "tstIEMAImplDataSseBinary-sqrtps_u128.bin"
IEM_TEST_DATA addsubps_u128,        "tstIEMAImplDataSseBinary-addsubps_u128.bin"
IEM_TEST_DATA cvtps2pd_u128,        "tstIEMAImplDataSseBinary-cvtps2pd_u128.bin"

IEM_TEST_DATA addss_u128_r32,       "tstIEMAImplDataSseBinary-addss_u128_r32.bin"
IEM_TEST_DATA mulss_u128_r32,       "tstIEMAImplDataSseBinary-mulss_u128_r32.bin"
IEM_TEST_DATA subss_u128_r32,       "tstIEMAImplDataSseBinary-subss_u128_r32.bin"
IEM_TEST_DATA minss_u128_r32,       "tstIEMAImplDataSseBinary-minss_u128_r32.bin"
IEM_TEST_DATA divss_u128_r32,       "tstIEMAImplDataSseBinary-divss_u128_r32.bin"
IEM_TEST_DATA maxss_u128_r32,       "tstIEMAImplDataSseBinary-maxss_u128_r32.bin"
IEM_TEST_DATA cvtss2sd_u128_r32,    "tstIEMAImplDataSseBinary-cvtss2sd_u128_r32.bin"
IEM_TEST_DATA sqrtss_u128_r32,      "tstIEMAImplDataSseBinary-sqrtss_u128_r32.bin"

IEM_TEST_DATA addpd_u128,           "tstIEMAImplDataSseBinary-addpd_u128.bin"
IEM_TEST_DATA mulpd_u128,           "tstIEMAImplDataSseBinary-mulpd_u128.bin"
IEM_TEST_DATA subpd_u128,           "tstIEMAImplDataSseBinary-subpd_u128.bin"
IEM_TEST_DATA minpd_u128,           "tstIEMAImplDataSseBinary-minpd_u128.bin"
IEM_TEST_DATA divpd_u128,           "tstIEMAImplDataSseBinary-divpd_u128.bin"
IEM_TEST_DATA maxpd_u128,           "tstIEMAImplDataSseBinary-maxpd_u128.bin"
IEM_TEST_DATA haddpd_u128,          "tstIEMAImplDataSseBinary-haddpd_u128.bin"
IEM_TEST_DATA hsubpd_u128,          "tstIEMAImplDataSseBinary-hsubpd_u128.bin"
IEM_TEST_DATA sqrtpd_u128,          "tstIEMAImplDataSseBinary-sqrtpd_u128.bin"
IEM_TEST_DATA addsubpd_u128,        "tstIEMAImplDataSseBinary-addsubpd_u128.bin"
IEM_TEST_DATA cvtpd2ps_u128,        "tstIEMAImplDataSseBinary-cvtpd2ps_u128.bin"

IEM_TEST_DATA addsd_u128_r64,       "tstIEMAImplDataSseBinary-addsd_u128_r64.bin"
IEM_TEST_DATA mulsd_u128_r64,       "tstIEMAImplDataSseBinary-mulsd_u128_r64.bin"
IEM_TEST_DATA subsd_u128_r64,       "tstIEMAImplDataSseBinary-subsd_u128_r64.bin"
IEM_TEST_DATA minsd_u128_r64,       "tstIEMAImplDataSseBinary-minsd_u128_r64.bin"
IEM_TEST_DATA divsd_u128_r64,       "tstIEMAImplDataSseBinary-divsd_u128_r64.bin"
IEM_TEST_DATA maxsd_u128_r64,       "tstIEMAImplDataSseBinary-maxsd_u128_r64.bin"
IEM_TEST_DATA cvtsd2ss_u128_r64,    "tstIEMAImplDataSseBinary-cvtsd2ss_u128_r64.bin"
IEM_TEST_DATA sqrtsd_u128_r64,      "tstIEMAImplDataSseBinary-sqrtsd_u128_r64.bin"
