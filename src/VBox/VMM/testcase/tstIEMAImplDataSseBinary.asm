; $Id$
;; @file
; tstIEMAImplDataSseBinary - Test data for SSE binary instructions.
;

;
; Copyright (C) 2022-2023 Oracle and/or its affiliates.
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
g_aTests_ %+ %1 %+ _end:
        align   4, db 0
EXPORTEDNAME g_cbTests_ %+ %1
        dd  g_aTests_ %+ %1 %+ _end - NAME(g_aTests_ %+ %1)

 %ifdef ASM_FORMAT_ELF
size g_aTests_ %+ %1  g_aTests_ %+ %1 %+ _end - NAME(g_aTests_ %+ %1)
type g_aTests_ %+ %1  object
size g_cbTests_ %+ %1 4
type g_cbTests_ %+ %1 object
 %endif
%endmacro

IEM_TEST_DATA addps_u128,           "tstIEMAImplDataSseBinary-addps_u128.bin.gz"
IEM_TEST_DATA mulps_u128,           "tstIEMAImplDataSseBinary-mulps_u128.bin.gz"
IEM_TEST_DATA subps_u128,           "tstIEMAImplDataSseBinary-subps_u128.bin.gz"
IEM_TEST_DATA minps_u128,           "tstIEMAImplDataSseBinary-minps_u128.bin.gz"
IEM_TEST_DATA divps_u128,           "tstIEMAImplDataSseBinary-divps_u128.bin.gz"
IEM_TEST_DATA maxps_u128,           "tstIEMAImplDataSseBinary-maxps_u128.bin.gz"
IEM_TEST_DATA haddps_u128,          "tstIEMAImplDataSseBinary-haddps_u128.bin.gz"
IEM_TEST_DATA hsubps_u128,          "tstIEMAImplDataSseBinary-hsubps_u128.bin.gz"
IEM_TEST_DATA sqrtps_u128,          "tstIEMAImplDataSseBinary-sqrtps_u128.bin.gz"
IEM_TEST_DATA addsubps_u128,        "tstIEMAImplDataSseBinary-addsubps_u128.bin.gz"
IEM_TEST_DATA cvtps2pd_u128,        "tstIEMAImplDataSseBinary-cvtps2pd_u128.bin.gz"

IEM_TEST_DATA addss_u128_r32,       "tstIEMAImplDataSseBinary-addss_u128_r32.bin.gz"
IEM_TEST_DATA mulss_u128_r32,       "tstIEMAImplDataSseBinary-mulss_u128_r32.bin.gz"
IEM_TEST_DATA subss_u128_r32,       "tstIEMAImplDataSseBinary-subss_u128_r32.bin.gz"
IEM_TEST_DATA minss_u128_r32,       "tstIEMAImplDataSseBinary-minss_u128_r32.bin.gz"
IEM_TEST_DATA divss_u128_r32,       "tstIEMAImplDataSseBinary-divss_u128_r32.bin.gz"
IEM_TEST_DATA maxss_u128_r32,       "tstIEMAImplDataSseBinary-maxss_u128_r32.bin.gz"
IEM_TEST_DATA cvtss2sd_u128_r32,    "tstIEMAImplDataSseBinary-cvtss2sd_u128_r32.bin.gz"
IEM_TEST_DATA sqrtss_u128_r32,      "tstIEMAImplDataSseBinary-sqrtss_u128_r32.bin.gz"

IEM_TEST_DATA addpd_u128,           "tstIEMAImplDataSseBinary-addpd_u128.bin.gz"
IEM_TEST_DATA mulpd_u128,           "tstIEMAImplDataSseBinary-mulpd_u128.bin.gz"
IEM_TEST_DATA subpd_u128,           "tstIEMAImplDataSseBinary-subpd_u128.bin.gz"
IEM_TEST_DATA minpd_u128,           "tstIEMAImplDataSseBinary-minpd_u128.bin.gz"
IEM_TEST_DATA divpd_u128,           "tstIEMAImplDataSseBinary-divpd_u128.bin.gz"
IEM_TEST_DATA maxpd_u128,           "tstIEMAImplDataSseBinary-maxpd_u128.bin.gz"
IEM_TEST_DATA haddpd_u128,          "tstIEMAImplDataSseBinary-haddpd_u128.bin.gz"
IEM_TEST_DATA hsubpd_u128,          "tstIEMAImplDataSseBinary-hsubpd_u128.bin.gz"
IEM_TEST_DATA sqrtpd_u128,          "tstIEMAImplDataSseBinary-sqrtpd_u128.bin.gz"
IEM_TEST_DATA addsubpd_u128,        "tstIEMAImplDataSseBinary-addsubpd_u128.bin.gz"
IEM_TEST_DATA cvtpd2ps_u128,        "tstIEMAImplDataSseBinary-cvtpd2ps_u128.bin.gz"

IEM_TEST_DATA addsd_u128_r64,       "tstIEMAImplDataSseBinary-addsd_u128_r64.bin.gz"
IEM_TEST_DATA mulsd_u128_r64,       "tstIEMAImplDataSseBinary-mulsd_u128_r64.bin.gz"
IEM_TEST_DATA subsd_u128_r64,       "tstIEMAImplDataSseBinary-subsd_u128_r64.bin.gz"
IEM_TEST_DATA minsd_u128_r64,       "tstIEMAImplDataSseBinary-minsd_u128_r64.bin.gz"
IEM_TEST_DATA divsd_u128_r64,       "tstIEMAImplDataSseBinary-divsd_u128_r64.bin.gz"
IEM_TEST_DATA maxsd_u128_r64,       "tstIEMAImplDataSseBinary-maxsd_u128_r64.bin.gz"
IEM_TEST_DATA cvtsd2ss_u128_r64,    "tstIEMAImplDataSseBinary-cvtsd2ss_u128_r64.bin.gz"
IEM_TEST_DATA sqrtsd_u128_r64,      "tstIEMAImplDataSseBinary-sqrtsd_u128_r64.bin.gz"

IEM_TEST_DATA cvttsd2si_i32_r64,    "tstIEMAImplDataSseBinary-cvttsd2si_i32_r64.bin.gz"
IEM_TEST_DATA cvtsd2si_i32_r64,     "tstIEMAImplDataSseBinary-cvtsd2si_i32_r64.bin.gz"

IEM_TEST_DATA cvttsd2si_i64_r64,    "tstIEMAImplDataSseBinary-cvttsd2si_i64_r64.bin.gz"
IEM_TEST_DATA cvtsd2si_i64_r64,     "tstIEMAImplDataSseBinary-cvtsd2si_i64_r64.bin.gz"

IEM_TEST_DATA cvttss2si_i32_r32,    "tstIEMAImplDataSseBinary-cvttss2si_i32_r32.bin.gz"
IEM_TEST_DATA cvtss2si_i32_r32,     "tstIEMAImplDataSseBinary-cvtss2si_i32_r32.bin.gz"

IEM_TEST_DATA cvttss2si_i64_r32,    "tstIEMAImplDataSseBinary-cvttss2si_i64_r32.bin.gz"
IEM_TEST_DATA cvtss2si_i64_r32,     "tstIEMAImplDataSseBinary-cvtss2si_i64_r32.bin.gz"

IEM_TEST_DATA cvtsi2ss_r32_i32,     "tstIEMAImplDataSseBinary-cvtsi2ss_r32_i32.bin.gz"
IEM_TEST_DATA cvtsi2ss_r32_i64,     "tstIEMAImplDataSseBinary-cvtsi2ss_r32_i64.bin.gz"

IEM_TEST_DATA cvtsi2sd_r64_i32,     "tstIEMAImplDataSseBinary-cvtsi2sd_r64_i32.bin.gz"
IEM_TEST_DATA cvtsi2sd_r64_i64,     "tstIEMAImplDataSseBinary-cvtsi2sd_r64_i64.bin.gz"

IEM_TEST_DATA ucomiss_u128,         "tstIEMAImplDataSseCompare-ucomiss_u128.bin.gz"
IEM_TEST_DATA vucomiss_u128,        "tstIEMAImplDataSseCompare-vucomiss_u128.bin.gz"
IEM_TEST_DATA comiss_u128,          "tstIEMAImplDataSseCompare-comiss_u128.bin.gz"
IEM_TEST_DATA vcomiss_u128,         "tstIEMAImplDataSseCompare-vcomiss_u128.bin.gz"

IEM_TEST_DATA ucomisd_u128,         "tstIEMAImplDataSseCompare-ucomisd_u128.bin.gz"
IEM_TEST_DATA vucomisd_u128,        "tstIEMAImplDataSseCompare-vucomisd_u128.bin.gz"
IEM_TEST_DATA comisd_u128,          "tstIEMAImplDataSseCompare-comisd_u128.bin.gz"
IEM_TEST_DATA vcomisd_u128,         "tstIEMAImplDataSseCompare-vcomisd_u128.bin.gz"

IEM_TEST_DATA cmpps_u128,           "tstIEMAImplDataSseCompare-cmpps_u128.bin.gz"
IEM_TEST_DATA cmpss_u128,           "tstIEMAImplDataSseCompare-cmpss_u128.bin.gz"
IEM_TEST_DATA cmppd_u128,           "tstIEMAImplDataSseCompare-cmppd_u128.bin.gz"
IEM_TEST_DATA cmpsd_u128,           "tstIEMAImplDataSseCompare-cmpsd_u128.bin.gz"

IEM_TEST_DATA cvtdq2ps_u128,        "tstIEMAImplDataSseConvert-cvtdq2ps_u128.bin.gz"
IEM_TEST_DATA cvtps2dq_u128,        "tstIEMAImplDataSseConvert-cvtps2dq_u128.bin.gz"
IEM_TEST_DATA cvttps2dq_u128,       "tstIEMAImplDataSseConvert-cvttps2dq_u128.bin.gz"

IEM_TEST_DATA cvtdq2pd_u128,        "tstIEMAImplDataSseConvert-cvtdq2pd_u128.bin.gz"
IEM_TEST_DATA cvtpd2dq_u128,        "tstIEMAImplDataSseConvert-cvtpd2dq_u128.bin.gz"
IEM_TEST_DATA cvttpd2dq_u128,       "tstIEMAImplDataSseConvert-cvttpd2dq_u128.bin.gz"

IEM_TEST_DATA cvtpd2pi_u128,        "tstIEMAImplDataSseConvert-cvtpd2pi_u128.bin.gz"
IEM_TEST_DATA cvttpd2pi_u128,       "tstIEMAImplDataSseConvert-cvttpd2pi_u128.bin.gz"

IEM_TEST_DATA cvtpi2ps_u128,        "tstIEMAImplDataSseConvert-cvtpi2ps_u128.bin.gz"
IEM_TEST_DATA cvtpi2pd_u128,        "tstIEMAImplDataSseConvert-cvtpi2pd_u128.bin.gz"

IEM_TEST_DATA cvtps2pi_u128,        "tstIEMAImplDataSseConvert-cvtps2pi_u128.bin.gz"
IEM_TEST_DATA cvttps2pi_u128,       "tstIEMAImplDataSseConvert-cvttps2pi_u128.bin.gz"

IEM_TEST_DATA pcmpistri_u128,       "tstIEMAImplDataSsePcmpxstrx-pcmpistri_u128.bin.gz"
IEM_TEST_DATA pcmpistrm_u128,       "tstIEMAImplDataSsePcmpxstrx-pcmpistrm_u128.bin.gz"
IEM_TEST_DATA pcmpestri_u128,       "tstIEMAImplDataSsePcmpxstrx-pcmpestri_u128.bin.gz"
IEM_TEST_DATA pcmpestrm_u128,       "tstIEMAImplDataSsePcmpxstrx-pcmpestrm_u128.bin.gz"

;
; Integer stuff.
; dir tstIEMAImplDataInt*bin.gz /b | sed -e 's/tstIEMAImplDataInt-\([^.]*\)\.bin\.gz$/IEM_TEST_DATA \1, "tstIEMAImplDataInt-\1.bin.gz"/'
;
IEM_TEST_DATA adcx_u32, "tstIEMAImplDataInt-adcx_u32.bin.gz"
IEM_TEST_DATA adcx_u64, "tstIEMAImplDataInt-adcx_u64.bin.gz"
IEM_TEST_DATA adc_u8, "tstIEMAImplDataInt-adc_u8.bin.gz"
IEM_TEST_DATA adc_u8_locked, "tstIEMAImplDataInt-adc_u8_locked.bin.gz"
IEM_TEST_DATA adc_u16, "tstIEMAImplDataInt-adc_u16.bin.gz"
IEM_TEST_DATA adc_u16_locked, "tstIEMAImplDataInt-adc_u16_locked.bin.gz"
IEM_TEST_DATA adc_u32, "tstIEMAImplDataInt-adc_u32.bin.gz"
IEM_TEST_DATA adc_u32_locked, "tstIEMAImplDataInt-adc_u32_locked.bin.gz"
IEM_TEST_DATA adc_u64, "tstIEMAImplDataInt-adc_u64.bin.gz"
IEM_TEST_DATA adc_u64_locked, "tstIEMAImplDataInt-adc_u64_locked.bin.gz"
IEM_TEST_DATA add_u8, "tstIEMAImplDataInt-add_u8.bin.gz"
IEM_TEST_DATA add_u8_locked, "tstIEMAImplDataInt-add_u8_locked.bin.gz"
IEM_TEST_DATA add_u16, "tstIEMAImplDataInt-add_u16.bin.gz"
IEM_TEST_DATA add_u16_locked, "tstIEMAImplDataInt-add_u16_locked.bin.gz"
IEM_TEST_DATA add_u32, "tstIEMAImplDataInt-add_u32.bin.gz"
IEM_TEST_DATA add_u32_locked, "tstIEMAImplDataInt-add_u32_locked.bin.gz"
IEM_TEST_DATA add_u64, "tstIEMAImplDataInt-add_u64.bin.gz"
IEM_TEST_DATA add_u64_locked, "tstIEMAImplDataInt-add_u64_locked.bin.gz"
IEM_TEST_DATA adox_u32, "tstIEMAImplDataInt-adox_u32.bin.gz"
IEM_TEST_DATA adox_u64, "tstIEMAImplDataInt-adox_u64.bin.gz"
IEM_TEST_DATA and_u8, "tstIEMAImplDataInt-and_u8.bin.gz"
IEM_TEST_DATA and_u8_locked, "tstIEMAImplDataInt-and_u8_locked.bin.gz"
IEM_TEST_DATA and_u16, "tstIEMAImplDataInt-and_u16.bin.gz"
IEM_TEST_DATA and_u16_locked, "tstIEMAImplDataInt-and_u16_locked.bin.gz"
IEM_TEST_DATA and_u32, "tstIEMAImplDataInt-and_u32.bin.gz"
IEM_TEST_DATA and_u32_locked, "tstIEMAImplDataInt-and_u32_locked.bin.gz"
IEM_TEST_DATA and_u64, "tstIEMAImplDataInt-and_u64.bin.gz"
IEM_TEST_DATA and_u64_locked, "tstIEMAImplDataInt-and_u64_locked.bin.gz"
IEM_TEST_DATA arpl, "tstIEMAImplDataInt-arpl.bin.gz"
IEM_TEST_DATA bsf_u16_amd, "tstIEMAImplDataInt-bsf_u16_amd-Amd.bin.gz"
IEM_TEST_DATA bsf_u16_intel, "tstIEMAImplDataInt-bsf_u16_intel-Intel.bin.gz"
IEM_TEST_DATA bsf_u32_amd, "tstIEMAImplDataInt-bsf_u32_amd-Amd.bin.gz"
IEM_TEST_DATA bsf_u32_intel, "tstIEMAImplDataInt-bsf_u32_intel-Intel.bin.gz"
IEM_TEST_DATA bsf_u64_amd, "tstIEMAImplDataInt-bsf_u64_amd-Amd.bin.gz"
IEM_TEST_DATA bsf_u64_intel, "tstIEMAImplDataInt-bsf_u64_intel-Intel.bin.gz"
IEM_TEST_DATA bsr_u16_amd, "tstIEMAImplDataInt-bsr_u16_amd-Amd.bin.gz"
IEM_TEST_DATA bsr_u16_intel, "tstIEMAImplDataInt-bsr_u16_intel-Intel.bin.gz"
IEM_TEST_DATA bsr_u32_amd, "tstIEMAImplDataInt-bsr_u32_amd-Amd.bin.gz"
IEM_TEST_DATA bsr_u32_intel, "tstIEMAImplDataInt-bsr_u32_intel-Intel.bin.gz"
IEM_TEST_DATA bsr_u64_amd, "tstIEMAImplDataInt-bsr_u64_amd-Amd.bin.gz"
IEM_TEST_DATA bsr_u64_intel, "tstIEMAImplDataInt-bsr_u64_intel-Intel.bin.gz"
IEM_TEST_DATA btc_u16, "tstIEMAImplDataInt-btc_u16.bin.gz"
IEM_TEST_DATA btc_u16_locked, "tstIEMAImplDataInt-btc_u16_locked.bin.gz"
IEM_TEST_DATA btc_u32, "tstIEMAImplDataInt-btc_u32.bin.gz"
IEM_TEST_DATA btc_u32_locked, "tstIEMAImplDataInt-btc_u32_locked.bin.gz"
IEM_TEST_DATA btc_u64, "tstIEMAImplDataInt-btc_u64.bin.gz"
IEM_TEST_DATA btc_u64_locked, "tstIEMAImplDataInt-btc_u64_locked.bin.gz"
IEM_TEST_DATA btr_u16, "tstIEMAImplDataInt-btr_u16.bin.gz"
IEM_TEST_DATA btr_u16_locked, "tstIEMAImplDataInt-btr_u16_locked.bin.gz"
IEM_TEST_DATA btr_u32, "tstIEMAImplDataInt-btr_u32.bin.gz"
IEM_TEST_DATA btr_u32_locked, "tstIEMAImplDataInt-btr_u32_locked.bin.gz"
IEM_TEST_DATA btr_u64, "tstIEMAImplDataInt-btr_u64.bin.gz"
IEM_TEST_DATA btr_u64_locked, "tstIEMAImplDataInt-btr_u64_locked.bin.gz"
IEM_TEST_DATA bts_u16, "tstIEMAImplDataInt-bts_u16.bin.gz"
IEM_TEST_DATA bts_u16_locked, "tstIEMAImplDataInt-bts_u16_locked.bin.gz"
IEM_TEST_DATA bts_u32, "tstIEMAImplDataInt-bts_u32.bin.gz"
IEM_TEST_DATA bts_u32_locked, "tstIEMAImplDataInt-bts_u32_locked.bin.gz"
IEM_TEST_DATA bts_u64, "tstIEMAImplDataInt-bts_u64.bin.gz"
IEM_TEST_DATA bts_u64_locked, "tstIEMAImplDataInt-bts_u64_locked.bin.gz"
IEM_TEST_DATA bt_u16, "tstIEMAImplDataInt-bt_u16.bin.gz"
IEM_TEST_DATA bt_u32, "tstIEMAImplDataInt-bt_u32.bin.gz"
IEM_TEST_DATA bt_u64, "tstIEMAImplDataInt-bt_u64.bin.gz"
IEM_TEST_DATA cmp_u8, "tstIEMAImplDataInt-cmp_u8.bin.gz"
IEM_TEST_DATA cmp_u16, "tstIEMAImplDataInt-cmp_u16.bin.gz"
IEM_TEST_DATA cmp_u32, "tstIEMAImplDataInt-cmp_u32.bin.gz"
IEM_TEST_DATA cmp_u64, "tstIEMAImplDataInt-cmp_u64.bin.gz"
IEM_TEST_DATA dec_u8, "tstIEMAImplDataInt-dec_u8.bin.gz"
IEM_TEST_DATA dec_u8_locked, "tstIEMAImplDataInt-dec_u8_locked.bin.gz"
IEM_TEST_DATA dec_u16, "tstIEMAImplDataInt-dec_u16.bin.gz"
IEM_TEST_DATA dec_u16_locked, "tstIEMAImplDataInt-dec_u16_locked.bin.gz"
IEM_TEST_DATA dec_u32, "tstIEMAImplDataInt-dec_u32.bin.gz"
IEM_TEST_DATA dec_u32_locked, "tstIEMAImplDataInt-dec_u32_locked.bin.gz"
IEM_TEST_DATA dec_u64, "tstIEMAImplDataInt-dec_u64.bin.gz"
IEM_TEST_DATA dec_u64_locked, "tstIEMAImplDataInt-dec_u64_locked.bin.gz"
IEM_TEST_DATA div_u8_amd, "tstIEMAImplDataInt-div_u8_amd-Amd.bin.gz"
IEM_TEST_DATA div_u8_intel, "tstIEMAImplDataInt-div_u8_intel-Intel.bin.gz"
IEM_TEST_DATA div_u16_amd, "tstIEMAImplDataInt-div_u16_amd-Amd.bin.gz"
IEM_TEST_DATA div_u16_intel, "tstIEMAImplDataInt-div_u16_intel-Intel.bin.gz"
IEM_TEST_DATA div_u32_amd, "tstIEMAImplDataInt-div_u32_amd-Amd.bin.gz"
IEM_TEST_DATA div_u32_intel, "tstIEMAImplDataInt-div_u32_intel-Intel.bin.gz"
IEM_TEST_DATA div_u64_amd, "tstIEMAImplDataInt-div_u64_amd-Amd.bin.gz"
IEM_TEST_DATA div_u64_intel, "tstIEMAImplDataInt-div_u64_intel-Intel.bin.gz"
IEM_TEST_DATA idiv_u8_amd, "tstIEMAImplDataInt-idiv_u8_amd-Amd.bin.gz"
IEM_TEST_DATA idiv_u8_intel, "tstIEMAImplDataInt-idiv_u8_intel-Intel.bin.gz"
IEM_TEST_DATA idiv_u16_amd, "tstIEMAImplDataInt-idiv_u16_amd-Amd.bin.gz"
IEM_TEST_DATA idiv_u16_intel, "tstIEMAImplDataInt-idiv_u16_intel-Intel.bin.gz"
IEM_TEST_DATA idiv_u32_amd, "tstIEMAImplDataInt-idiv_u32_amd-Amd.bin.gz"
IEM_TEST_DATA idiv_u32_intel, "tstIEMAImplDataInt-idiv_u32_intel-Intel.bin.gz"
IEM_TEST_DATA idiv_u64_amd, "tstIEMAImplDataInt-idiv_u64_amd-Amd.bin.gz"
IEM_TEST_DATA idiv_u64_intel, "tstIEMAImplDataInt-idiv_u64_intel-Intel.bin.gz"
IEM_TEST_DATA imul_two_u16_amd, "tstIEMAImplDataInt-imul_two_u16_amd-Amd.bin.gz"
IEM_TEST_DATA imul_two_u16_intel, "tstIEMAImplDataInt-imul_two_u16_intel-Intel.bin.gz"
IEM_TEST_DATA imul_two_u32_amd, "tstIEMAImplDataInt-imul_two_u32_amd-Amd.bin.gz"
IEM_TEST_DATA imul_two_u32_intel, "tstIEMAImplDataInt-imul_two_u32_intel-Intel.bin.gz"
IEM_TEST_DATA imul_two_u64_amd, "tstIEMAImplDataInt-imul_two_u64_amd-Amd.bin.gz"
IEM_TEST_DATA imul_two_u64_intel, "tstIEMAImplDataInt-imul_two_u64_intel-Intel.bin.gz"
IEM_TEST_DATA imul_u8_amd, "tstIEMAImplDataInt-imul_u8_amd-Amd.bin.gz"
IEM_TEST_DATA imul_u8_intel, "tstIEMAImplDataInt-imul_u8_intel-Intel.bin.gz"
IEM_TEST_DATA imul_u16_amd, "tstIEMAImplDataInt-imul_u16_amd-Amd.bin.gz"
IEM_TEST_DATA imul_u16_intel, "tstIEMAImplDataInt-imul_u16_intel-Intel.bin.gz"
IEM_TEST_DATA imul_u32_amd, "tstIEMAImplDataInt-imul_u32_amd-Amd.bin.gz"
IEM_TEST_DATA imul_u32_intel, "tstIEMAImplDataInt-imul_u32_intel-Intel.bin.gz"
IEM_TEST_DATA imul_u64_amd, "tstIEMAImplDataInt-imul_u64_amd-Amd.bin.gz"
IEM_TEST_DATA imul_u64_intel, "tstIEMAImplDataInt-imul_u64_intel-Intel.bin.gz"
IEM_TEST_DATA inc_u8, "tstIEMAImplDataInt-inc_u8.bin.gz"
IEM_TEST_DATA inc_u8_locked, "tstIEMAImplDataInt-inc_u8_locked.bin.gz"
IEM_TEST_DATA inc_u16, "tstIEMAImplDataInt-inc_u16.bin.gz"
IEM_TEST_DATA inc_u16_locked, "tstIEMAImplDataInt-inc_u16_locked.bin.gz"
IEM_TEST_DATA inc_u32, "tstIEMAImplDataInt-inc_u32.bin.gz"
IEM_TEST_DATA inc_u32_locked, "tstIEMAImplDataInt-inc_u32_locked.bin.gz"
IEM_TEST_DATA inc_u64, "tstIEMAImplDataInt-inc_u64.bin.gz"
IEM_TEST_DATA inc_u64_locked, "tstIEMAImplDataInt-inc_u64_locked.bin.gz"
IEM_TEST_DATA mul_u8_amd, "tstIEMAImplDataInt-mul_u8_amd-Amd.bin.gz"
IEM_TEST_DATA mul_u8_intel, "tstIEMAImplDataInt-mul_u8_intel-Intel.bin.gz"
IEM_TEST_DATA mul_u16_amd, "tstIEMAImplDataInt-mul_u16_amd-Amd.bin.gz"
IEM_TEST_DATA mul_u16_intel, "tstIEMAImplDataInt-mul_u16_intel-Intel.bin.gz"
IEM_TEST_DATA mul_u32_amd, "tstIEMAImplDataInt-mul_u32_amd-Amd.bin.gz"
IEM_TEST_DATA mul_u32_intel, "tstIEMAImplDataInt-mul_u32_intel-Intel.bin.gz"
IEM_TEST_DATA mul_u64_amd, "tstIEMAImplDataInt-mul_u64_amd-Amd.bin.gz"
IEM_TEST_DATA mul_u64_intel, "tstIEMAImplDataInt-mul_u64_intel-Intel.bin.gz"
IEM_TEST_DATA neg_u8, "tstIEMAImplDataInt-neg_u8.bin.gz"
IEM_TEST_DATA neg_u8_locked, "tstIEMAImplDataInt-neg_u8_locked.bin.gz"
IEM_TEST_DATA neg_u16, "tstIEMAImplDataInt-neg_u16.bin.gz"
IEM_TEST_DATA neg_u16_locked, "tstIEMAImplDataInt-neg_u16_locked.bin.gz"
IEM_TEST_DATA neg_u32, "tstIEMAImplDataInt-neg_u32.bin.gz"
IEM_TEST_DATA neg_u32_locked, "tstIEMAImplDataInt-neg_u32_locked.bin.gz"
IEM_TEST_DATA neg_u64, "tstIEMAImplDataInt-neg_u64.bin.gz"
IEM_TEST_DATA neg_u64_locked, "tstIEMAImplDataInt-neg_u64_locked.bin.gz"
IEM_TEST_DATA not_u8, "tstIEMAImplDataInt-not_u8.bin.gz"
IEM_TEST_DATA not_u8_locked, "tstIEMAImplDataInt-not_u8_locked.bin.gz"
IEM_TEST_DATA not_u16, "tstIEMAImplDataInt-not_u16.bin.gz"
IEM_TEST_DATA not_u16_locked, "tstIEMAImplDataInt-not_u16_locked.bin.gz"
IEM_TEST_DATA not_u32, "tstIEMAImplDataInt-not_u32.bin.gz"
IEM_TEST_DATA not_u32_locked, "tstIEMAImplDataInt-not_u32_locked.bin.gz"
IEM_TEST_DATA not_u64, "tstIEMAImplDataInt-not_u64.bin.gz"
IEM_TEST_DATA not_u64_locked, "tstIEMAImplDataInt-not_u64_locked.bin.gz"
IEM_TEST_DATA or_u8, "tstIEMAImplDataInt-or_u8.bin.gz"
IEM_TEST_DATA or_u8_locked, "tstIEMAImplDataInt-or_u8_locked.bin.gz"
IEM_TEST_DATA or_u16, "tstIEMAImplDataInt-or_u16.bin.gz"
IEM_TEST_DATA or_u16_locked, "tstIEMAImplDataInt-or_u16_locked.bin.gz"
IEM_TEST_DATA or_u32, "tstIEMAImplDataInt-or_u32.bin.gz"
IEM_TEST_DATA or_u32_locked, "tstIEMAImplDataInt-or_u32_locked.bin.gz"
IEM_TEST_DATA or_u64, "tstIEMAImplDataInt-or_u64.bin.gz"
IEM_TEST_DATA or_u64_locked, "tstIEMAImplDataInt-or_u64_locked.bin.gz"
IEM_TEST_DATA rcl_u8_amd, "tstIEMAImplDataInt-rcl_u8_amd-Amd.bin.gz"
IEM_TEST_DATA rcl_u8_intel, "tstIEMAImplDataInt-rcl_u8_intel-Intel.bin.gz"
IEM_TEST_DATA rcl_u16_amd, "tstIEMAImplDataInt-rcl_u16_amd-Amd.bin.gz"
IEM_TEST_DATA rcl_u16_intel, "tstIEMAImplDataInt-rcl_u16_intel-Intel.bin.gz"
IEM_TEST_DATA rcl_u32_amd, "tstIEMAImplDataInt-rcl_u32_amd-Amd.bin.gz"
IEM_TEST_DATA rcl_u32_intel, "tstIEMAImplDataInt-rcl_u32_intel-Intel.bin.gz"
IEM_TEST_DATA rcl_u64_amd, "tstIEMAImplDataInt-rcl_u64_amd-Amd.bin.gz"
IEM_TEST_DATA rcl_u64_intel, "tstIEMAImplDataInt-rcl_u64_intel-Intel.bin.gz"
IEM_TEST_DATA rcr_u8_amd, "tstIEMAImplDataInt-rcr_u8_amd-Amd.bin.gz"
IEM_TEST_DATA rcr_u8_intel, "tstIEMAImplDataInt-rcr_u8_intel-Intel.bin.gz"
IEM_TEST_DATA rcr_u16_amd, "tstIEMAImplDataInt-rcr_u16_amd-Amd.bin.gz"
IEM_TEST_DATA rcr_u16_intel, "tstIEMAImplDataInt-rcr_u16_intel-Intel.bin.gz"
IEM_TEST_DATA rcr_u32_amd, "tstIEMAImplDataInt-rcr_u32_amd-Amd.bin.gz"
IEM_TEST_DATA rcr_u32_intel, "tstIEMAImplDataInt-rcr_u32_intel-Intel.bin.gz"
IEM_TEST_DATA rcr_u64_amd, "tstIEMAImplDataInt-rcr_u64_amd-Amd.bin.gz"
IEM_TEST_DATA rcr_u64_intel, "tstIEMAImplDataInt-rcr_u64_intel-Intel.bin.gz"
IEM_TEST_DATA rol_u8_amd, "tstIEMAImplDataInt-rol_u8_amd-Amd.bin.gz"
IEM_TEST_DATA rol_u8_intel, "tstIEMAImplDataInt-rol_u8_intel-Intel.bin.gz"
IEM_TEST_DATA rol_u16_amd, "tstIEMAImplDataInt-rol_u16_amd-Amd.bin.gz"
IEM_TEST_DATA rol_u16_intel, "tstIEMAImplDataInt-rol_u16_intel-Intel.bin.gz"
IEM_TEST_DATA rol_u32_amd, "tstIEMAImplDataInt-rol_u32_amd-Amd.bin.gz"
IEM_TEST_DATA rol_u32_intel, "tstIEMAImplDataInt-rol_u32_intel-Intel.bin.gz"
IEM_TEST_DATA rol_u64_amd, "tstIEMAImplDataInt-rol_u64_amd-Amd.bin.gz"
IEM_TEST_DATA rol_u64_intel, "tstIEMAImplDataInt-rol_u64_intel-Intel.bin.gz"
IEM_TEST_DATA ror_u8_amd, "tstIEMAImplDataInt-ror_u8_amd-Amd.bin.gz"
IEM_TEST_DATA ror_u8_intel, "tstIEMAImplDataInt-ror_u8_intel-Intel.bin.gz"
IEM_TEST_DATA ror_u16_amd, "tstIEMAImplDataInt-ror_u16_amd-Amd.bin.gz"
IEM_TEST_DATA ror_u16_intel, "tstIEMAImplDataInt-ror_u16_intel-Intel.bin.gz"
IEM_TEST_DATA ror_u32_amd, "tstIEMAImplDataInt-ror_u32_amd-Amd.bin.gz"
IEM_TEST_DATA ror_u32_intel, "tstIEMAImplDataInt-ror_u32_intel-Intel.bin.gz"
IEM_TEST_DATA ror_u64_amd, "tstIEMAImplDataInt-ror_u64_amd-Amd.bin.gz"
IEM_TEST_DATA ror_u64_intel, "tstIEMAImplDataInt-ror_u64_intel-Intel.bin.gz"
IEM_TEST_DATA sar_u8_amd, "tstIEMAImplDataInt-sar_u8_amd-Amd.bin.gz"
IEM_TEST_DATA sar_u8_intel, "tstIEMAImplDataInt-sar_u8_intel-Intel.bin.gz"
IEM_TEST_DATA sar_u16_amd, "tstIEMAImplDataInt-sar_u16_amd-Amd.bin.gz"
IEM_TEST_DATA sar_u16_intel, "tstIEMAImplDataInt-sar_u16_intel-Intel.bin.gz"
IEM_TEST_DATA sar_u32_amd, "tstIEMAImplDataInt-sar_u32_amd-Amd.bin.gz"
IEM_TEST_DATA sar_u32_intel, "tstIEMAImplDataInt-sar_u32_intel-Intel.bin.gz"
IEM_TEST_DATA sar_u64_amd, "tstIEMAImplDataInt-sar_u64_amd-Amd.bin.gz"
IEM_TEST_DATA sar_u64_intel, "tstIEMAImplDataInt-sar_u64_intel-Intel.bin.gz"
IEM_TEST_DATA sbb_u8, "tstIEMAImplDataInt-sbb_u8.bin.gz"
IEM_TEST_DATA sbb_u8_locked, "tstIEMAImplDataInt-sbb_u8_locked.bin.gz"
IEM_TEST_DATA sbb_u16, "tstIEMAImplDataInt-sbb_u16.bin.gz"
IEM_TEST_DATA sbb_u16_locked, "tstIEMAImplDataInt-sbb_u16_locked.bin.gz"
IEM_TEST_DATA sbb_u32, "tstIEMAImplDataInt-sbb_u32.bin.gz"
IEM_TEST_DATA sbb_u32_locked, "tstIEMAImplDataInt-sbb_u32_locked.bin.gz"
IEM_TEST_DATA sbb_u64, "tstIEMAImplDataInt-sbb_u64.bin.gz"
IEM_TEST_DATA sbb_u64_locked, "tstIEMAImplDataInt-sbb_u64_locked.bin.gz"
IEM_TEST_DATA shld_u16_amd, "tstIEMAImplDataInt-shld_u16_amd-Amd.bin.gz"
IEM_TEST_DATA shld_u16_intel, "tstIEMAImplDataInt-shld_u16_intel-Intel.bin.gz"
IEM_TEST_DATA shld_u32_amd, "tstIEMAImplDataInt-shld_u32_amd-Amd.bin.gz"
IEM_TEST_DATA shld_u32_intel, "tstIEMAImplDataInt-shld_u32_intel-Intel.bin.gz"
IEM_TEST_DATA shld_u64_amd, "tstIEMAImplDataInt-shld_u64_amd-Amd.bin.gz"
IEM_TEST_DATA shld_u64_intel, "tstIEMAImplDataInt-shld_u64_intel-Intel.bin.gz"
IEM_TEST_DATA shl_u8_amd, "tstIEMAImplDataInt-shl_u8_amd-Amd.bin.gz"
IEM_TEST_DATA shl_u8_intel, "tstIEMAImplDataInt-shl_u8_intel-Intel.bin.gz"
IEM_TEST_DATA shl_u16_amd, "tstIEMAImplDataInt-shl_u16_amd-Amd.bin.gz"
IEM_TEST_DATA shl_u16_intel, "tstIEMAImplDataInt-shl_u16_intel-Intel.bin.gz"
IEM_TEST_DATA shl_u32_amd, "tstIEMAImplDataInt-shl_u32_amd-Amd.bin.gz"
IEM_TEST_DATA shl_u32_intel, "tstIEMAImplDataInt-shl_u32_intel-Intel.bin.gz"
IEM_TEST_DATA shl_u64_amd, "tstIEMAImplDataInt-shl_u64_amd-Amd.bin.gz"
IEM_TEST_DATA shl_u64_intel, "tstIEMAImplDataInt-shl_u64_intel-Intel.bin.gz"
IEM_TEST_DATA shrd_u16_amd, "tstIEMAImplDataInt-shrd_u16_amd-Amd.bin.gz"
IEM_TEST_DATA shrd_u16_intel, "tstIEMAImplDataInt-shrd_u16_intel-Intel.bin.gz"
IEM_TEST_DATA shrd_u32_amd, "tstIEMAImplDataInt-shrd_u32_amd-Amd.bin.gz"
IEM_TEST_DATA shrd_u32_intel, "tstIEMAImplDataInt-shrd_u32_intel-Intel.bin.gz"
IEM_TEST_DATA shrd_u64_amd, "tstIEMAImplDataInt-shrd_u64_amd-Amd.bin.gz"
IEM_TEST_DATA shrd_u64_intel, "tstIEMAImplDataInt-shrd_u64_intel-Intel.bin.gz"
IEM_TEST_DATA shr_u8_amd, "tstIEMAImplDataInt-shr_u8_amd-Amd.bin.gz"
IEM_TEST_DATA shr_u8_intel, "tstIEMAImplDataInt-shr_u8_intel-Intel.bin.gz"
IEM_TEST_DATA shr_u16_amd, "tstIEMAImplDataInt-shr_u16_amd-Amd.bin.gz"
IEM_TEST_DATA shr_u16_intel, "tstIEMAImplDataInt-shr_u16_intel-Intel.bin.gz"
IEM_TEST_DATA shr_u32_amd, "tstIEMAImplDataInt-shr_u32_amd-Amd.bin.gz"
IEM_TEST_DATA shr_u32_intel, "tstIEMAImplDataInt-shr_u32_intel-Intel.bin.gz"
IEM_TEST_DATA shr_u64_amd, "tstIEMAImplDataInt-shr_u64_amd-Amd.bin.gz"
IEM_TEST_DATA shr_u64_intel, "tstIEMAImplDataInt-shr_u64_intel-Intel.bin.gz"
IEM_TEST_DATA sub_u8, "tstIEMAImplDataInt-sub_u8.bin.gz"
IEM_TEST_DATA sub_u8_locked, "tstIEMAImplDataInt-sub_u8_locked.bin.gz"
IEM_TEST_DATA sub_u16, "tstIEMAImplDataInt-sub_u16.bin.gz"
IEM_TEST_DATA sub_u16_locked, "tstIEMAImplDataInt-sub_u16_locked.bin.gz"
IEM_TEST_DATA sub_u32, "tstIEMAImplDataInt-sub_u32.bin.gz"
IEM_TEST_DATA sub_u32_locked, "tstIEMAImplDataInt-sub_u32_locked.bin.gz"
IEM_TEST_DATA sub_u64, "tstIEMAImplDataInt-sub_u64.bin.gz"
IEM_TEST_DATA sub_u64_locked, "tstIEMAImplDataInt-sub_u64_locked.bin.gz"
IEM_TEST_DATA test_u8, "tstIEMAImplDataInt-test_u8.bin.gz"
IEM_TEST_DATA test_u16, "tstIEMAImplDataInt-test_u16.bin.gz"
IEM_TEST_DATA test_u32, "tstIEMAImplDataInt-test_u32.bin.gz"
IEM_TEST_DATA test_u64, "tstIEMAImplDataInt-test_u64.bin.gz"
IEM_TEST_DATA xor_u8, "tstIEMAImplDataInt-xor_u8.bin.gz"
IEM_TEST_DATA xor_u8_locked, "tstIEMAImplDataInt-xor_u8_locked.bin.gz"
IEM_TEST_DATA xor_u16, "tstIEMAImplDataInt-xor_u16.bin.gz"
IEM_TEST_DATA xor_u16_locked, "tstIEMAImplDataInt-xor_u16_locked.bin.gz"
IEM_TEST_DATA xor_u32, "tstIEMAImplDataInt-xor_u32.bin.gz"
IEM_TEST_DATA xor_u32_locked, "tstIEMAImplDataInt-xor_u32_locked.bin.gz"
IEM_TEST_DATA xor_u64, "tstIEMAImplDataInt-xor_u64.bin.gz"
IEM_TEST_DATA xor_u64_locked, "tstIEMAImplDataInt-xor_u64_locked.bin.gz"

