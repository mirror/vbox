/* $Id$ */
/** @file
 * tstIEMAImplDataSseBinary - Test data for SSE binary instructions.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifdef RT_OS_DARWIN
#define NAME(a_Name) _##a_Name
#else
#define NAME(a_Name) a_Name
#endif

#ifdef RT_OS_DARWIN
.section __TEXT,__const
#else
.section __TEXT
#endif

#ifdef ASM_FORMAT_ELF
# define IEM_TEST_DATA(a_Instr, a_DataFile) \
    .p2align 6 \
    .globl NAME(g_aTests_ ## a_Instr) \
    NAME(g_aTests_ ## a_Instr): \
        .incbin # a_DataFile \
    end_g_aTests_ ## a_Instr: \
    .p2align 2 \
    .globl NAME(g_cbTests_ ## a_Instr) \
    NAME(g_cbTests_ ## a_Instr): \
            .long end_g_aTests_ ## a_Instr - NAME(g_aTests_ ## a_Instr) \
    .size g_aTests_ ## a_Instr, end_g_aTests_ ## a_Instr - NAME(g_aTests_ ## a_Instr) \
    .type g_aTests_ ## a_Instr, object \
    .size g_cbTests_ ## a_Instr, 4 \
    .type g_cbTests_ ## a_Instr, object \
#else
# define IEM_TEST_DATA(a_Instr, a_DataFile) \
    .p2align 6 \
    .globl NAME(g_aTests_ ## a_Instr) \
    NAME(g_aTests_ ## a_Instr): \
        .incbin # a_DataFile \
    end_g_aTests_ ## a_Instr: \
    .p2align 2 \
    .globl NAME(g_cbTests_ ## a_Instr) \
    NAME(g_cbTests_ ## a_Instr): \
            .long end_g_aTests_ ## a_Instr - NAME(g_aTests_ ## a_Instr)
#endif

IEM_TEST_DATA(addps_u128, "tstIEMAImplDataSseBinary-addps_u128.bin")
IEM_TEST_DATA(mulps_u128, "tstIEMAImplDataSseBinary-mulps_u128.bin")
IEM_TEST_DATA(subps_u128, "tstIEMAImplDataSseBinary-subps_u128.bin")
IEM_TEST_DATA(minps_u128, "tstIEMAImplDataSseBinary-minps_u128.bin")
IEM_TEST_DATA(divps_u128, "tstIEMAImplDataSseBinary-divps_u128.bin")
IEM_TEST_DATA(maxps_u128, "tstIEMAImplDataSseBinary-maxps_u128.bin")

IEM_TEST_DATA(addss_u128_r32, "tstIEMAImplDataSseBinary-addss_u128_r32.bin")
IEM_TEST_DATA(mulss_u128_r32, "tstIEMAImplDataSseBinary-mulss_u128_r32.bin")

IEM_TEST_DATA(addpd_u128, "tstIEMAImplDataSseBinary-addpd_u128.bin")
IEM_TEST_DATA(mulpd_u128, "tstIEMAImplDataSseBinary-mulpd_u128.bin")
IEM_TEST_DATA(subpd_u128, "tstIEMAImplDataSseBinary-subpd_u128.bin")
IEM_TEST_DATA(minpd_u128, "tstIEMAImplDataSseBinary-minpd_u128.bin")
IEM_TEST_DATA(divpd_u128, "tstIEMAImplDataSseBinary-divpd_u128.bin")
IEM_TEST_DATA(maxpd_u128, "tstIEMAImplDataSseBinary-maxpd_u128.bin")

IEM_TEST_DATA(addsd_u128_r64, "tstIEMAImplDataSseBinary-addsd_u128_r64.bin")
IEM_TEST_DATA(mulsd_u128_r64, "tstIEMAImplDataSseBinary-mulsd_u128_r64.bin")
