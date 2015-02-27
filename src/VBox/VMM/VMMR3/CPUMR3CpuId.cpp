/* $Id$ */
/** @file
 * CPUM - CPU ID part.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/ssm.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/mm.h>

#include <VBox/err.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The intel pentium family.
 */
static const CPUMMICROARCH g_aenmIntelFamily06[] =
{
    /* [ 0(0x00)] = */ kCpumMicroarch_Intel_P6,           /* Pentium Pro A-step (says sandpile.org). */
    /* [ 1(0x01)] = */ kCpumMicroarch_Intel_P6,           /* Pentium Pro */
    /* [ 2(0x02)] = */ kCpumMicroarch_Intel_Unknown,
    /* [ 3(0x03)] = */ kCpumMicroarch_Intel_P6_II,        /* PII Klamath */
    /* [ 4(0x04)] = */ kCpumMicroarch_Intel_Unknown,
    /* [ 5(0x05)] = */ kCpumMicroarch_Intel_P6_II,        /* PII Deschutes */
    /* [ 6(0x06)] = */ kCpumMicroarch_Intel_P6_II,        /* Celeron Mendocino. */
    /* [ 7(0x07)] = */ kCpumMicroarch_Intel_P6_III,       /* PIII Katmai. */
    /* [ 8(0x08)] = */ kCpumMicroarch_Intel_P6_III,       /* PIII Coppermine (includes Celeron). */
    /* [ 9(0x09)] = */ kCpumMicroarch_Intel_P6_M_Banias,  /* Pentium/Celeron M Banias. */
    /* [10(0x0a)] = */ kCpumMicroarch_Intel_P6_III,       /* PIII Xeon */
    /* [11(0x0b)] = */ kCpumMicroarch_Intel_P6_III,       /* PIII Tualatin (includes Celeron). */
    /* [12(0x0c)] = */ kCpumMicroarch_Intel_Unknown,
    /* [13(0x0d)] = */ kCpumMicroarch_Intel_P6_M_Dothan,  /* Pentium/Celeron M Dothan. */
    /* [14(0x0e)] = */ kCpumMicroarch_Intel_Core_Yonah,   /* Core Yonah (Enhanced Pentium M). */
    /* [15(0x0f)] = */ kCpumMicroarch_Intel_Core2_Merom,  /* Merom */
    /* [16(0x10)] = */ kCpumMicroarch_Intel_Unknown,
    /* [17(0x11)] = */ kCpumMicroarch_Intel_Unknown,
    /* [18(0x12)] = */ kCpumMicroarch_Intel_Unknown,
    /* [19(0x13)] = */ kCpumMicroarch_Intel_Unknown,
    /* [20(0x14)] = */ kCpumMicroarch_Intel_Unknown,
    /* [21(0x15)] = */ kCpumMicroarch_Intel_P6_M_Dothan,  /* Tolapai - System-on-a-chip. */
    /* [22(0x16)] = */ kCpumMicroarch_Intel_Core2_Merom,
    /* [23(0x17)] = */ kCpumMicroarch_Intel_Core2_Penryn,
    /* [24(0x18)] = */ kCpumMicroarch_Intel_Unknown,
    /* [25(0x19)] = */ kCpumMicroarch_Intel_Unknown,
    /* [26(0x1a)] = */ kCpumMicroarch_Intel_Core7_Nehalem,
    /* [27(0x1b)] = */ kCpumMicroarch_Intel_Unknown,
    /* [28(0x1c)] = */ kCpumMicroarch_Intel_Atom_Bonnell, /* Diamonville, Pineview, */
    /* [29(0x1d)] = */ kCpumMicroarch_Intel_Core2_Penryn,
    /* [30(0x1e)] = */ kCpumMicroarch_Intel_Core7_Nehalem, /* Clarksfield, Lynnfield, Jasper Forest. */
    /* [31(0x1f)] = */ kCpumMicroarch_Intel_Core7_Nehalem, /* Only listed by sandpile.org.  2 cores ABD/HVD, whatever that means. */
    /* [32(0x20)] = */ kCpumMicroarch_Intel_Unknown,
    /* [33(0x21)] = */ kCpumMicroarch_Intel_Unknown,
    /* [34(0x22)] = */ kCpumMicroarch_Intel_Unknown,
    /* [35(0x23)] = */ kCpumMicroarch_Intel_Unknown,
    /* [36(0x24)] = */ kCpumMicroarch_Intel_Unknown,
    /* [37(0x25)] = */ kCpumMicroarch_Intel_Core7_Westmere, /* Arrandale, Clarksdale. */
    /* [38(0x26)] = */ kCpumMicroarch_Intel_Atom_Lincroft,
    /* [39(0x27)] = */ kCpumMicroarch_Intel_Atom_Saltwell,
    /* [40(0x28)] = */ kCpumMicroarch_Intel_Unknown,
    /* [41(0x29)] = */ kCpumMicroarch_Intel_Unknown,
    /* [42(0x2a)] = */ kCpumMicroarch_Intel_Core7_SandyBridge,
    /* [43(0x2b)] = */ kCpumMicroarch_Intel_Unknown,
    /* [44(0x2c)] = */ kCpumMicroarch_Intel_Core7_Westmere, /* Gulftown, Westmere-EP. */
    /* [45(0x2d)] = */ kCpumMicroarch_Intel_Core7_SandyBridge, /* SandyBridge-E, SandyBridge-EN, SandyBridge-EP. */
    /* [46(0x2e)] = */ kCpumMicroarch_Intel_Core7_Nehalem,  /* Beckton (Xeon). */
    /* [47(0x2f)] = */ kCpumMicroarch_Intel_Core7_Westmere, /* Westmere-EX. */
    /* [48(0x30)] = */ kCpumMicroarch_Intel_Unknown,
    /* [49(0x31)] = */ kCpumMicroarch_Intel_Unknown,
    /* [50(0x32)] = */ kCpumMicroarch_Intel_Unknown,
    /* [51(0x33)] = */ kCpumMicroarch_Intel_Unknown,
    /* [52(0x34)] = */ kCpumMicroarch_Intel_Unknown,
    /* [53(0x35)] = */ kCpumMicroarch_Intel_Atom_Saltwell, /* ?? */
    /* [54(0x36)] = */ kCpumMicroarch_Intel_Atom_Saltwell, /* Cedarview, ++ */
    /* [55(0x37)] = */ kCpumMicroarch_Intel_Atom_Silvermont,
    /* [56(0x38)] = */ kCpumMicroarch_Intel_Unknown,
    /* [57(0x39)] = */ kCpumMicroarch_Intel_Unknown,
    /* [58(0x3a)] = */ kCpumMicroarch_Intel_Core7_IvyBridge,
    /* [59(0x3b)] = */ kCpumMicroarch_Intel_Unknown,
    /* [60(0x3c)] = */ kCpumMicroarch_Intel_Core7_Haswell,
    /* [61(0x3d)] = */ kCpumMicroarch_Intel_Core7_Broadwell,
    /* [62(0x3e)] = */ kCpumMicroarch_Intel_Core7_IvyBridge,
    /* [63(0x3f)] = */ kCpumMicroarch_Intel_Core7_Haswell,
    /* [64(0x40)] = */ kCpumMicroarch_Intel_Unknown,
    /* [65(0x41)] = */ kCpumMicroarch_Intel_Unknown,
    /* [66(0x42)] = */ kCpumMicroarch_Intel_Unknown,
    /* [67(0x43)] = */ kCpumMicroarch_Intel_Unknown,
    /* [68(0x44)] = */ kCpumMicroarch_Intel_Unknown,
    /* [69(0x45)] = */ kCpumMicroarch_Intel_Core7_Haswell,
    /* [70(0x46)] = */ kCpumMicroarch_Intel_Core7_Haswell,
    /* [71(0x47)] = */ kCpumMicroarch_Intel_Unknown,
    /* [72(0x48)] = */ kCpumMicroarch_Intel_Unknown,
    /* [73(0x49)] = */ kCpumMicroarch_Intel_Unknown,
    /* [74(0x4a)] = */ kCpumMicroarch_Intel_Atom_Silvermont,
    /* [75(0x4b)] = */ kCpumMicroarch_Intel_Unknown,
    /* [76(0x4c)] = */ kCpumMicroarch_Intel_Unknown,
    /* [77(0x4d)] = */ kCpumMicroarch_Intel_Atom_Silvermont,
    /* [78(0x4e)] = */ kCpumMicroarch_Intel_Unknown,
    /* [79(0x4f)] = */ kCpumMicroarch_Intel_Unknown,
};



/**
 * Figures out the (sub-)micro architecture given a bit of CPUID info.
 *
 * @returns Micro architecture.
 * @param   enmVendor           The CPU vendor .
 * @param   bFamily             The CPU family.
 * @param   bModel              The CPU model.
 * @param   bStepping           The CPU stepping.
 */
VMMR3DECL(CPUMMICROARCH) CPUMR3CpuIdDetermineMicroarchEx(CPUMCPUVENDOR enmVendor, uint8_t bFamily,
                                                         uint8_t bModel, uint8_t bStepping)
{
    if (enmVendor == CPUMCPUVENDOR_AMD)
    {
        switch (bFamily)
        {
            case 0x02:  return kCpumMicroarch_AMD_Am286; /* Not really kosher... */
            case 0x03:  return kCpumMicroarch_AMD_Am386;
            case 0x23:  return kCpumMicroarch_AMD_Am386; /* SX*/
            case 0x04:  return bModel < 14 ? kCpumMicroarch_AMD_Am486 : kCpumMicroarch_AMD_Am486Enh;
            case 0x05:  return bModel <  6 ? kCpumMicroarch_AMD_K5    : kCpumMicroarch_AMD_K6; /* Genode LX is 0x0a, lump it with K6. */
            case 0x06:
                switch (bModel)
                {
                    case  0: kCpumMicroarch_AMD_K7_Palomino;
                    case  1: kCpumMicroarch_AMD_K7_Palomino;
                    case  2: kCpumMicroarch_AMD_K7_Palomino;
                    case  3: kCpumMicroarch_AMD_K7_Spitfire;
                    case  4: kCpumMicroarch_AMD_K7_Thunderbird;
                    case  6: kCpumMicroarch_AMD_K7_Palomino;
                    case  7: kCpumMicroarch_AMD_K7_Morgan;
                    case  8: kCpumMicroarch_AMD_K7_Thoroughbred;
                    case 10: kCpumMicroarch_AMD_K7_Barton; /* Thorton too. */
                }
                return kCpumMicroarch_AMD_K7_Unknown;
            case 0x0f:
                /*
                 * This family is a friggin mess. Trying my best to make some
                 * sense out of it. Too much happened in the 0x0f family to
                 * lump it all together as K8 (130nm->90nm->65nm, AMD-V, ++).
                 *
                 * Emperical CPUID.01h.EAX evidence from revision guides, wikipedia,
                 * cpu-world.com, and other places:
                 *  - 130nm:
                 *     - ClawHammer:    F7A/SH-CG, F5A/-CG, F4A/-CG, F50/-B0, F48/-C0, F58/-C0,
                 *     - SledgeHammer:  F50/SH-B0, F48/-C0, F58/-C0, F4A/-CG, F5A/-CG, F7A/-CG, F51/-B3
                 *     - Newcastle:     FC0/DH-CG (errum #180: FE0/DH-CG), FF0/DH-CG
                 *     - Dublin:        FC0/-CG, FF0/-CG, F82/CH-CG, F4A/-CG, F48/SH-C0,
                 *     - Odessa:        FC0/DH-CG (errum #180: FE0/DH-CG)
                 *     - Paris:         FF0/DH-CG, FC0/DH-CG (errum #180: FE0/DH-CG),
                 *  - 90nm:
                 *     - Winchester:    10FF0/DH-D0, 20FF0/DH-E3.
                 *     - Oakville:      10FC0/DH-D0.
                 *     - Georgetown:    10FC0/DH-D0.
                 *     - Sonora:        10FC0/DH-D0.
                 *     - Venus:         20F71/SH-E4
                 *     - Troy:          20F51/SH-E4
                 *     - Athens:        20F51/SH-E4
                 *     - San Diego:     20F71/SH-E4.
                 *     - Lancaster:     20F42/SH-E5
                 *     - Newark:        20F42/SH-E5.
                 *     - Albany:        20FC2/DH-E6.
                 *     - Roma:          20FC2/DH-E6.
                 *     - Venice:        20FF0/DH-E3, 20FC2/DH-E6, 20FF2/DH-E6.
                 *     - Palermo:       10FC0/DH-D0, 20FF0/DH-E3, 20FC0/DH-E3, 20FC2/DH-E6, 20FF2/DH-E6
                 *  - 90nm introducing Dual core:
                 *     - Denmark:       20F30/JH-E1, 20F32/JH-E6
                 *     - Italy:         20F10/JH-E1, 20F12/JH-E6
                 *     - Egypt:         20F10/JH-E1, 20F12/JH-E6
                 *     - Toledo:        20F32/JH-E6, 30F72/DH-E6 (single code variant).
                 *     - Manchester:    20FB1/BH-E4, 30FF2/BH-E4.
                 *  - 90nm 2nd gen opteron ++, AMD-V introduced (might be missing in some cheaper models):
                 *     - Santa Ana:     40F32/JH-F2, /-F3
                 *     - Santa Rosa:    40F12/JH-F2, 40F13/JH-F3
                 *     - Windsor:       40F32/JH-F2, 40F33/JH-F3, C0F13/JH-F3, 40FB2/BH-F2, ??20FB1/BH-E4??.
                 *     - Manila:        50FF2/DH-F2, 40FF2/DH-F2
                 *     - Orleans:       40FF2/DH-F2, 50FF2/DH-F2, 50FF3/DH-F3.
                 *     - Keene:         40FC2/DH-F2.
                 *     - Richmond:      40FC2/DH-F2
                 *     - Taylor:        40F82/BH-F2
                 *     - Trinidad:      40F82/BH-F2
                 *
                 *  - 65nm:
                 *     - Brisbane:      60FB1/BH-G1, 60FB2/BH-G2.
                 *     - Tyler:         60F81/BH-G1, 60F82/BH-G2.
                 *     - Sparta:        70FF1/DH-G1, 70FF2/DH-G2.
                 *     - Lima:          70FF1/DH-G1, 70FF2/DH-G2.
                 *     - Sherman:       /-G1, 70FC2/DH-G2.
                 *     - Huron:         70FF2/DH-G2.
                 */
                if (bModel < 0x10)
                    return kCpumMicroarch_AMD_K8_130nm;
                if (bModel >= 0x60 && bModel < 0x80)
                    return kCpumMicroarch_AMD_K8_65nm;
                if (bModel >= 0x40)
                    return kCpumMicroarch_AMD_K8_90nm_AMDV;
                switch (bModel)
                {
                    case 0x21:
                    case 0x23:
                    case 0x2b:
                    case 0x2f:
                    case 0x37:
                    case 0x3f:
                        return kCpumMicroarch_AMD_K8_90nm_DualCore;
                }
                return kCpumMicroarch_AMD_K8_90nm;
            case 0x10:
                return kCpumMicroarch_AMD_K10;
            case 0x11:
                return kCpumMicroarch_AMD_K10_Lion;
            case 0x12:
                return kCpumMicroarch_AMD_K10_Llano;
            case 0x14:
                return kCpumMicroarch_AMD_Bobcat;
            case 0x15:
                switch (bModel)
                {
                    case 0x00:  return kCpumMicroarch_AMD_15h_Bulldozer;    /* Any? prerelease? */
                    case 0x01:  return kCpumMicroarch_AMD_15h_Bulldozer;    /* Opteron 4200, FX-81xx. */
                    case 0x02:  return kCpumMicroarch_AMD_15h_Piledriver;   /* Opteron 4300, FX-83xx. */
                    case 0x10:  return kCpumMicroarch_AMD_15h_Piledriver;   /* A10-5800K for e.g. */
                    case 0x11:  /* ?? */
                    case 0x12:  /* ?? */
                    case 0x13:  return kCpumMicroarch_AMD_15h_Piledriver;   /* A10-6800K for e.g. */
                }
                return kCpumMicroarch_AMD_15h_Unknown;
            case 0x16:
                return kCpumMicroarch_AMD_Jaguar;

        }
        return kCpumMicroarch_AMD_Unknown;
    }

    if (enmVendor == CPUMCPUVENDOR_INTEL)
    {
        switch (bFamily)
        {
            case 3:
                return kCpumMicroarch_Intel_80386;
            case 4:
                return kCpumMicroarch_Intel_80486;
            case 5:
                return kCpumMicroarch_Intel_P5;
            case 6:
                if (bModel < RT_ELEMENTS(g_aenmIntelFamily06))
                    return g_aenmIntelFamily06[bModel];
                return kCpumMicroarch_Intel_Atom_Unknown;
            case 15:
                switch (bModel)
                {
                    case 0:     return kCpumMicroarch_Intel_NB_Willamette;
                    case 1:     return kCpumMicroarch_Intel_NB_Willamette;
                    case 2:     return kCpumMicroarch_Intel_NB_Northwood;
                    case 3:     return kCpumMicroarch_Intel_NB_Prescott;
                    case 4:     return kCpumMicroarch_Intel_NB_Prescott2M; /* ?? */
                    case 5:     return kCpumMicroarch_Intel_NB_Unknown; /*??*/
                    case 6:     return kCpumMicroarch_Intel_NB_CedarMill;
                    case 7:     return kCpumMicroarch_Intel_NB_Gallatin;
                    default:    return kCpumMicroarch_Intel_NB_Unknown;
                }
                break;
            /* The following are not kosher but kind of follow intuitively from 6, 5 & 4. */
            case 1:
                return kCpumMicroarch_Intel_8086;
            case 2:
                return kCpumMicroarch_Intel_80286;
        }
        return kCpumMicroarch_Intel_Unknown;
    }

    if (enmVendor == CPUMCPUVENDOR_VIA)
    {
        switch (bFamily)
        {
            case 5:
                switch (bModel)
                {
                    case 1: return kCpumMicroarch_Centaur_C6;
                    case 4: return kCpumMicroarch_Centaur_C6;
                    case 8: return kCpumMicroarch_Centaur_C2;
                    case 9: return kCpumMicroarch_Centaur_C3;
                }
                break;

            case 6:
                switch (bModel)
                {
                    case  5: return kCpumMicroarch_VIA_C3_M2;
                    case  6: return kCpumMicroarch_VIA_C3_C5A;
                    case  7: return bStepping < 8 ? kCpumMicroarch_VIA_C3_C5B : kCpumMicroarch_VIA_C3_C5C;
                    case  8: return kCpumMicroarch_VIA_C3_C5N;
                    case  9: return bStepping < 8 ? kCpumMicroarch_VIA_C3_C5XL : kCpumMicroarch_VIA_C3_C5P;
                    case 10: return kCpumMicroarch_VIA_C7_C5J;
                    case 15: return kCpumMicroarch_VIA_Isaiah;
                }
                break;
        }
        return kCpumMicroarch_VIA_Unknown;
    }

    if (enmVendor == CPUMCPUVENDOR_CYRIX)
    {
        switch (bFamily)
        {
            case 4:
                switch (bModel)
                {
                    case 9: return kCpumMicroarch_Cyrix_5x86;
                }
                break;

            case 5:
                switch (bModel)
                {
                    case 2: return kCpumMicroarch_Cyrix_M1;
                    case 4: return kCpumMicroarch_Cyrix_MediaGX;
                    case 5: return kCpumMicroarch_Cyrix_MediaGXm;
                }
                break;

            case 6:
                switch (bModel)
                {
                    case 0: return kCpumMicroarch_Cyrix_M2;
                }
                break;

        }
        return kCpumMicroarch_Cyrix_Unknown;
    }

    return kCpumMicroarch_Unknown;
}


/**
 * Translates a microarchitecture enum value to the corresponding string
 * constant.
 *
 * @returns Read-only string constant (omits "kCpumMicroarch_" prefix). Returns
 *          NULL if the value is invalid.
 *
 * @param   enmMicroarch    The enum value to convert.
 */
VMMR3DECL(const char *) CPUMR3MicroarchName(CPUMMICROARCH enmMicroarch)
{
    switch (enmMicroarch)
    {
#define CASE_RET_STR(enmValue)  case enmValue: return #enmValue + (sizeof("kCpumMicroarch_") - 1)
        CASE_RET_STR(kCpumMicroarch_Intel_8086);
        CASE_RET_STR(kCpumMicroarch_Intel_80186);
        CASE_RET_STR(kCpumMicroarch_Intel_80286);
        CASE_RET_STR(kCpumMicroarch_Intel_80386);
        CASE_RET_STR(kCpumMicroarch_Intel_80486);
        CASE_RET_STR(kCpumMicroarch_Intel_P5);

        CASE_RET_STR(kCpumMicroarch_Intel_P6);
        CASE_RET_STR(kCpumMicroarch_Intel_P6_II);
        CASE_RET_STR(kCpumMicroarch_Intel_P6_III);

        CASE_RET_STR(kCpumMicroarch_Intel_P6_M_Banias);
        CASE_RET_STR(kCpumMicroarch_Intel_P6_M_Dothan);
        CASE_RET_STR(kCpumMicroarch_Intel_Core_Yonah);

        CASE_RET_STR(kCpumMicroarch_Intel_Core2_Merom);
        CASE_RET_STR(kCpumMicroarch_Intel_Core2_Penryn);

        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Nehalem);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Westmere);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_SandyBridge);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_IvyBridge);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Haswell);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Broadwell);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Skylake);
        CASE_RET_STR(kCpumMicroarch_Intel_Core7_Cannonlake);

        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Bonnell);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Lincroft);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Saltwell);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Silvermont);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Airmount);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Goldmont);
        CASE_RET_STR(kCpumMicroarch_Intel_Atom_Unknown);

        CASE_RET_STR(kCpumMicroarch_Intel_NB_Willamette);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_Northwood);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_Prescott);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_Prescott2M);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_CedarMill);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_Gallatin);
        CASE_RET_STR(kCpumMicroarch_Intel_NB_Unknown);

        CASE_RET_STR(kCpumMicroarch_Intel_Unknown);

        CASE_RET_STR(kCpumMicroarch_AMD_Am286);
        CASE_RET_STR(kCpumMicroarch_AMD_Am386);
        CASE_RET_STR(kCpumMicroarch_AMD_Am486);
        CASE_RET_STR(kCpumMicroarch_AMD_Am486Enh);
        CASE_RET_STR(kCpumMicroarch_AMD_K5);
        CASE_RET_STR(kCpumMicroarch_AMD_K6);

        CASE_RET_STR(kCpumMicroarch_AMD_K7_Palomino);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Spitfire);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Thunderbird);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Morgan);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Thoroughbred);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Barton);
        CASE_RET_STR(kCpumMicroarch_AMD_K7_Unknown);

        CASE_RET_STR(kCpumMicroarch_AMD_K8_130nm);
        CASE_RET_STR(kCpumMicroarch_AMD_K8_90nm);
        CASE_RET_STR(kCpumMicroarch_AMD_K8_90nm_DualCore);
        CASE_RET_STR(kCpumMicroarch_AMD_K8_90nm_AMDV);
        CASE_RET_STR(kCpumMicroarch_AMD_K8_65nm);

        CASE_RET_STR(kCpumMicroarch_AMD_K10);
        CASE_RET_STR(kCpumMicroarch_AMD_K10_Lion);
        CASE_RET_STR(kCpumMicroarch_AMD_K10_Llano);
        CASE_RET_STR(kCpumMicroarch_AMD_Bobcat);
        CASE_RET_STR(kCpumMicroarch_AMD_Jaguar);

        CASE_RET_STR(kCpumMicroarch_AMD_15h_Bulldozer);
        CASE_RET_STR(kCpumMicroarch_AMD_15h_Piledriver);
        CASE_RET_STR(kCpumMicroarch_AMD_15h_Steamroller);
        CASE_RET_STR(kCpumMicroarch_AMD_15h_Excavator);
        CASE_RET_STR(kCpumMicroarch_AMD_15h_Unknown);

        CASE_RET_STR(kCpumMicroarch_AMD_16h_First);

        CASE_RET_STR(kCpumMicroarch_AMD_Unknown);

        CASE_RET_STR(kCpumMicroarch_Centaur_C6);
        CASE_RET_STR(kCpumMicroarch_Centaur_C2);
        CASE_RET_STR(kCpumMicroarch_Centaur_C3);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_M2);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5A);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5B);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5C);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5N);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5XL);
        CASE_RET_STR(kCpumMicroarch_VIA_C3_C5P);
        CASE_RET_STR(kCpumMicroarch_VIA_C7_C5J);
        CASE_RET_STR(kCpumMicroarch_VIA_Isaiah);
        CASE_RET_STR(kCpumMicroarch_VIA_Unknown);

        CASE_RET_STR(kCpumMicroarch_Cyrix_5x86);
        CASE_RET_STR(kCpumMicroarch_Cyrix_M1);
        CASE_RET_STR(kCpumMicroarch_Cyrix_MediaGX);
        CASE_RET_STR(kCpumMicroarch_Cyrix_MediaGXm);
        CASE_RET_STR(kCpumMicroarch_Cyrix_M2);
        CASE_RET_STR(kCpumMicroarch_Cyrix_Unknown);

        CASE_RET_STR(kCpumMicroarch_Unknown);

#undef CASE_RET_STR
        case kCpumMicroarch_Invalid:
        case kCpumMicroarch_Intel_End:
        case kCpumMicroarch_Intel_Core7_End:
        case kCpumMicroarch_Intel_Atom_End:
        case kCpumMicroarch_Intel_P6_Core_Atom_End:
        case kCpumMicroarch_Intel_NB_End:
        case kCpumMicroarch_AMD_K7_End:
        case kCpumMicroarch_AMD_K8_End:
        case kCpumMicroarch_AMD_15h_End:
        case kCpumMicroarch_AMD_16h_End:
        case kCpumMicroarch_AMD_End:
        case kCpumMicroarch_VIA_End:
        case kCpumMicroarch_Cyrix_End:
        case kCpumMicroarch_32BitHack:
            break;
        /* no default! */
    }

    return NULL;
}



/**
 * Gets a matching leaf in the CPUID leaf array.
 *
 * @returns Pointer to the matching leaf, or NULL if not found.
 * @param   paLeaves            The CPUID leaves to search.  This is sorted.
 * @param   cLeaves             The number of leaves in the array.
 * @param   uLeaf               The leaf to locate.
 * @param   uSubLeaf            The subleaf to locate.  Pass 0 if no subleaves.
 */
PCPUMCPUIDLEAF cpumR3CpuIdGetLeaf(PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf, uint32_t uSubLeaf)
{
    /* Lazy bird does linear lookup here since this is only used for the
       occational CPUID overrides. */
    for (uint32_t i = 0; i < cLeaves; i++)
        if (   paLeaves[i].uLeaf    == uLeaf
            && paLeaves[i].uSubLeaf == (uSubLeaf & paLeaves[i].fSubLeafMask))
            return &paLeaves[i];
    return NULL;
}


/**
 * Gets a matching leaf in the CPUID leaf array, converted to a CPUMCPUID.
 *
 * @returns true if found, false it not.
 * @param   paLeaves            The CPUID leaves to search.  This is sorted.
 * @param   cLeaves             The number of leaves in the array.
 * @param   uLeaf               The leaf to locate.
 * @param   uSubLeaf            The subleaf to locate.  Pass 0 if no subleaves.
 * @param   pLegacy             The legacy output leaf.
 */
bool cpumR3CpuIdGetLeafLegacy(PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf, uint32_t uSubLeaf, PCPUMCPUID pLegacy)
{
    PCPUMCPUIDLEAF pLeaf = cpumR3CpuIdGetLeaf(paLeaves, cLeaves, uLeaf, uSubLeaf);
    if (pLeaf)
    {
        pLegacy->eax = pLeaf->uEax;
        pLegacy->ebx = pLeaf->uEbx;
        pLegacy->ecx = pLeaf->uEcx;
        pLegacy->edx = pLeaf->uEdx;
        return true;
    }
    return false;
}


/**
 * Ensures that the CPUID leaf array can hold one more leaf.
 *
 * @returns Pointer to the CPUID leaf array (*ppaLeaves) on success.  NULL on
 *          failure.
 * @param   pVM         Pointer to the VM, used as the heap selector. Passing
 *                      NULL uses the host-context heap, otherwise the VM's
 *                      hyper heap is used.
 * @param   ppaLeaves   Pointer to the variable holding the array pointer
 *                      (input/output).
 * @param   cLeaves     The current array size.
 *
 * @remarks This function will automatically update the R0 and RC pointers when
 *          using the hyper heap, which means @a ppaLeaves and @a cLeaves must
 *          be the corresponding VM's CPUID arrays (which is asserted).
 */
static PCPUMCPUIDLEAF cpumR3CpuIdEnsureSpace(PVM pVM, PCPUMCPUIDLEAF *ppaLeaves, uint32_t cLeaves)
{
    uint32_t cAllocated;
    if (!pVM)
        cAllocated = RT_ALIGN(cLeaves, 16);
    else
    {
        /*
         * We're using the hyper heap now, but when the arrays were copied over to it from
         * the host-context heap, we only copy the exact size and not the ensured size.
         * See @bugref{7270}.
         */
        cAllocated = cLeaves;
    }

    if (cLeaves + 1 > cAllocated)
    {
        void *pvNew;
#ifndef IN_VBOX_CPU_REPORT
        if (pVM)
        {
            Assert(ppaLeaves == &pVM->cpum.s.GuestInfo.paCpuIdLeavesR3);
            Assert(cLeaves == pVM->cpum.s.GuestInfo.cCpuIdLeaves);

            size_t cb    = cAllocated * sizeof(**ppaLeaves);
            size_t cbNew = (cAllocated + 16) * sizeof(**ppaLeaves);
            int rc = MMR3HyperRealloc(pVM, *ppaLeaves, cb, 32, MM_TAG_CPUM_CPUID, cbNew, &pvNew);
            if (RT_FAILURE(rc))
            {
                *ppaLeaves = NULL;
                pVM->cpum.s.GuestInfo.paCpuIdLeavesR0 = NIL_RTR0PTR;
                pVM->cpum.s.GuestInfo.paCpuIdLeavesRC = NIL_RTRCPTR;
                LogRel(("CPUM: cpumR3CpuIdEnsureSpace: MMR3HyperRealloc failed. rc=%Rrc\n", rc));
                return NULL;
            }
            *ppaLeaves = (PCPUMCPUIDLEAF)pvNew;
        }
        else
#endif
        {
            pvNew = RTMemRealloc(*ppaLeaves, (cAllocated + 16) * sizeof(**ppaLeaves));
            if (!pvNew)
            {
                RTMemFree(*ppaLeaves);
                *ppaLeaves = NULL;
                return NULL;
            }
            *ppaLeaves = (PCPUMCPUIDLEAF)pvNew;
        }
    }

#ifndef IN_VBOX_CPU_REPORT
    /* Update the R0 and RC pointers. */
    if (pVM)
    {
        Assert(ppaLeaves == &pVM->cpum.s.GuestInfo.paCpuIdLeavesR3);
        pVM->cpum.s.GuestInfo.paCpuIdLeavesR0 = MMHyperR3ToR0(pVM, *ppaLeaves);
        pVM->cpum.s.GuestInfo.paCpuIdLeavesRC = MMHyperR3ToRC(pVM, *ppaLeaves);
    }
#endif

    return *ppaLeaves;
}


/**
 * Append a CPUID leaf or sub-leaf.
 *
 * ASSUMES linear insertion order, so we'll won't need to do any searching or
 * replace anything.  Use cpumR3CpuIdInsert() for those cases.
 *
 * @returns VINF_SUCCESS or VERR_NO_MEMORY.  On error, *ppaLeaves is freed, so
 *          the caller need do no more work.
 * @param   ppaLeaves       Pointer to the the pointer to the array of sorted
 *                          CPUID leaves and sub-leaves.
 * @param   pcLeaves        Where we keep the leaf count for *ppaLeaves.
 * @param   uLeaf           The leaf we're adding.
 * @param   uSubLeaf        The sub-leaf number.
 * @param   fSubLeafMask    The sub-leaf mask.
 * @param   uEax            The EAX value.
 * @param   uEbx            The EBX value.
 * @param   uEcx            The ECX value.
 * @param   uEdx            The EDX value.
 * @param   fFlags          The flags.
 */
static int cpumR3CollectCpuIdInfoAddOne(PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves,
                                        uint32_t uLeaf, uint32_t uSubLeaf, uint32_t fSubLeafMask,
                                        uint32_t uEax, uint32_t uEbx, uint32_t uEcx, uint32_t uEdx, uint32_t fFlags)
{
    if (!cpumR3CpuIdEnsureSpace(NULL /* pVM */, ppaLeaves, *pcLeaves))
        return VERR_NO_MEMORY;

    PCPUMCPUIDLEAF pNew = &(*ppaLeaves)[*pcLeaves];
    Assert(   *pcLeaves == 0
           || pNew[-1].uLeaf < uLeaf
           || (pNew[-1].uLeaf == uLeaf && pNew[-1].uSubLeaf < uSubLeaf) );

    pNew->uLeaf        = uLeaf;
    pNew->uSubLeaf     = uSubLeaf;
    pNew->fSubLeafMask = fSubLeafMask;
    pNew->uEax         = uEax;
    pNew->uEbx         = uEbx;
    pNew->uEcx         = uEcx;
    pNew->uEdx         = uEdx;
    pNew->fFlags       = fFlags;

    *pcLeaves += 1;
    return VINF_SUCCESS;
}


/**
 * Inserts a CPU ID leaf, replacing any existing ones.
 *
 * When inserting a simple leaf where we already got a series of subleaves with
 * the same leaf number (eax), the simple leaf will replace the whole series.
 *
 * When pVM is NULL, this ASSUMES that the leaves array is still on the normal
 * host-context heap and has only been allocated/reallocated by the
 * cpumR3CpuIdEnsureSpace function.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM, used as the heap selector.
 *                          Passing NULL uses the host-context heap, otherwise
 *                          the VM's hyper heap is used.
 * @param   ppaLeaves       Pointer to the the pointer to the array of sorted
 *                          CPUID leaves and sub-leaves. Must be NULL if using
 *                          the hyper heap.
 * @param   pcLeaves        Where we keep the leaf count for *ppaLeaves. Must be
 *                          NULL if using the hyper heap.
 * @param   pNewLeaf        Pointer to the data of the new leaf we're about to
 *                          insert.
 */
int cpumR3CpuIdInsert(PVM pVM, PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves, PCPUMCPUIDLEAF pNewLeaf)
{
    /*
     * Validate input parameters if we are using the hyper heap and use the VM's CPUID arrays.
     */
    if (pVM)
    {
        AssertReturn(!ppaLeaves, VERR_INVALID_PARAMETER);
        AssertReturn(!pcLeaves, VERR_INVALID_PARAMETER);

        ppaLeaves = &pVM->cpum.s.GuestInfo.paCpuIdLeavesR3;
        pcLeaves  = &pVM->cpum.s.GuestInfo.cCpuIdLeaves;
    }

    PCPUMCPUIDLEAF  paLeaves = *ppaLeaves;
    uint32_t        cLeaves  = *pcLeaves;

    /*
     * Validate the new leaf a little.
     */
    AssertReturn(!(pNewLeaf->fFlags & ~CPUMCPUIDLEAF_F_SUBLEAVES_ECX_UNCHANGED), VERR_INVALID_FLAGS);
    AssertReturn(pNewLeaf->fSubLeafMask != 0 || pNewLeaf->uSubLeaf == 0, VERR_INVALID_PARAMETER);
    AssertReturn(RT_IS_POWER_OF_TWO(pNewLeaf->fSubLeafMask + 1), VERR_INVALID_PARAMETER);
    AssertReturn((pNewLeaf->fSubLeafMask & pNewLeaf->uSubLeaf) == pNewLeaf->uSubLeaf, VERR_INVALID_PARAMETER);

    /*
     * Find insertion point. The lazy bird uses the same excuse as in
     * cpumR3CpuIdGetLeaf().
     */
    uint32_t i = 0;
    while (   i < cLeaves
           && paLeaves[i].uLeaf < pNewLeaf->uLeaf)
        i++;
    if (   i < cLeaves
        && paLeaves[i].uLeaf == pNewLeaf->uLeaf)
    {
        if (paLeaves[i].fSubLeafMask != pNewLeaf->fSubLeafMask)
        {
            /*
             * The subleaf mask differs, replace all existing leaves with the
             * same leaf number.
             */
            uint32_t c = 1;
            while (   i + c < cLeaves
                   && paLeaves[i + c].uSubLeaf == pNewLeaf->uLeaf)
                c++;
            if (c > 1 && i + c < cLeaves)
            {
                memmove(&paLeaves[i + c], &paLeaves[i + 1], (cLeaves - i - c) * sizeof(paLeaves[0]));
                *pcLeaves = cLeaves -= c - 1;
            }

            paLeaves[i] = *pNewLeaf;
            return VINF_SUCCESS;
        }

        /* Find subleaf insertion point. */
        while (   i < cLeaves
               && paLeaves[i].uSubLeaf < pNewLeaf->uSubLeaf)
            i++;

        /*
         * If we've got an exactly matching leaf, replace it.
         */
        if (   paLeaves[i].uLeaf    == pNewLeaf->uLeaf
            && paLeaves[i].uSubLeaf == pNewLeaf->uSubLeaf)
        {
            paLeaves[i] = *pNewLeaf;
            return VINF_SUCCESS;
        }
    }

    /*
     * Adding a new leaf at 'i'.
     */
    paLeaves = cpumR3CpuIdEnsureSpace(pVM, ppaLeaves, cLeaves);
    if (!paLeaves)
        return VERR_NO_MEMORY;

    if (i < cLeaves)
        memmove(&paLeaves[i + 1], &paLeaves[i], (cLeaves - i) * sizeof(paLeaves[0]));
    *pcLeaves += 1;
    paLeaves[i] = *pNewLeaf;
    return VINF_SUCCESS;
}


/**
 * Removes a range of CPUID leaves.
 *
 * This will not reallocate the array.
 *
 * @param   paLeaves        The array of sorted CPUID leaves and sub-leaves.
 * @param   pcLeaves        Where we keep the leaf count for @a paLeaves.
 * @param   uFirst          The first leaf.
 * @param   uLast           The last leaf.
 */
void cpumR3CpuIdRemoveRange(PCPUMCPUIDLEAF paLeaves, uint32_t *pcLeaves, uint32_t uFirst, uint32_t uLast)
{
    uint32_t cLeaves = *pcLeaves;

    Assert(uFirst <= uLast);

    /*
     * Find the first one.
     */
    uint32_t iFirst = 0;
    while (   iFirst < cLeaves
           && paLeaves[iFirst].uLeaf < uFirst)
        iFirst++;

    /*
     * Find the end (last + 1).
     */
    uint32_t iEnd = iFirst;
    while (   iEnd < cLeaves
           && paLeaves[iEnd].uLeaf <= uLast)
        iEnd++;

    /*
     * Adjust the array if anything needs removing.
     */
    if (iFirst < iEnd)
    {
        if (iEnd < cLeaves)
            memmove(&paLeaves[iFirst], &paLeaves[iEnd], (cLeaves - iEnd) * sizeof(paLeaves[0]));
        *pcLeaves = cLeaves -= (iEnd - iFirst);
    }
}



/**
 * Checks if ECX make a difference when reading a given CPUID leaf.
 *
 * @returns @c true if it does, @c false if it doesn't.
 * @param   uLeaf               The leaf we're reading.
 * @param   pcSubLeaves         Number of sub-leaves accessible via ECX.
 * @param   pfFinalEcxUnchanged Whether ECX is passed thru when going beyond the
 *                              final sub-leaf.
 */
static bool cpumR3IsEcxRelevantForCpuIdLeaf(uint32_t uLeaf, uint32_t *pcSubLeaves, bool *pfFinalEcxUnchanged)
{
    *pfFinalEcxUnchanged = false;

    uint32_t auCur[4];
    uint32_t auPrev[4];
    ASMCpuIdExSlow(uLeaf, 0, 0, 0, &auPrev[0], &auPrev[1], &auPrev[2], &auPrev[3]);

    /* Look for sub-leaves. */
    uint32_t uSubLeaf = 1;
    for (;;)
    {
        ASMCpuIdExSlow(uLeaf, 0, uSubLeaf, 0, &auCur[0], &auCur[1], &auCur[2], &auCur[3]);
        if (memcmp(auCur, auPrev, sizeof(auCur)))
            break;

        /* Advance / give up. */
        uSubLeaf++;
        if (uSubLeaf >= 64)
        {
            *pcSubLeaves = 1;
            return false;
        }
    }

    /* Count sub-leaves. */
    uint32_t cRepeats = 0;
    uSubLeaf = 0;
    for (;;)
    {
        ASMCpuIdExSlow(uLeaf, 0, uSubLeaf, 0, &auCur[0], &auCur[1], &auCur[2], &auCur[3]);

        /* Figuring out when to stop isn't entirely straight forward as we need
           to cover undocumented behavior up to a point and implementation shortcuts. */

        /* 1. Look for zero values. */
        if (   auCur[0] == 0
            && auCur[1] == 0
            && (auCur[2] == 0 || auCur[2] == uSubLeaf)
            && (auCur[3] == 0 || uLeaf == 0xb /* edx is fixed */) )
            break;

        /* 2. Look for more than 4 repeating value sets. */
        if (   auCur[0] == auPrev[0]
            && auCur[1] == auPrev[1]
            && (    auCur[2] == auPrev[2]
                || (   auCur[2]  == uSubLeaf
                    && auPrev[2] == uSubLeaf - 1) )
            && auCur[3] == auPrev[3])
        {
            cRepeats++;
            if (cRepeats > 4)
                break;
        }
        else
            cRepeats = 0;

        /* 3. Leaf 0xb level type 0 check. */
        if (   uLeaf == 0xb
            && (auCur[3]  & 0xff00) == 0
            && (auPrev[3] & 0xff00) == 0)
            break;

        /* 99. Give up. */
        if (uSubLeaf >= 128)
        {
#ifndef IN_VBOX_CPU_REPORT
            /* Ok, limit it according to the documentation if possible just to
               avoid annoying users with these detection issues. */
            uint32_t cDocLimit = UINT32_MAX;
            if (uLeaf == 0x4)
                cDocLimit = 4;
            else if (uLeaf == 0x7)
                cDocLimit = 1;
            else if (uLeaf == 0xf)
                cDocLimit = 2;
            if (cDocLimit != UINT32_MAX)
            {
                *pfFinalEcxUnchanged = auCur[2] == uSubLeaf;
                *pcSubLeaves = cDocLimit + 3;
                return true;
            }
#endif
            *pcSubLeaves = UINT32_MAX;
            return true;
        }

        /* Advance. */
        uSubLeaf++;
        memcpy(auPrev, auCur, sizeof(auCur));
    }

    /* Standard exit. */
    *pfFinalEcxUnchanged = auCur[2] == uSubLeaf;
    *pcSubLeaves = uSubLeaf + 1 - cRepeats;
    return true;
}


/**
 * Gets a CPU ID leaf.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pLeaf       Where to store the found leaf.
 * @param   uLeaf       The leaf to locate.
 * @param   uSubLeaf    The subleaf to locate.  Pass 0 if no subleaves.
 */
VMMR3DECL(int) CPUMR3CpuIdGetLeaf(PVM pVM, PCPUMCPUIDLEAF pLeaf, uint32_t uLeaf, uint32_t uSubLeaf)
{
    PCPUMCPUIDLEAF pcLeaf = cpumR3CpuIdGetLeaf(pVM->cpum.s.GuestInfo.paCpuIdLeavesR3, pVM->cpum.s.GuestInfo.cCpuIdLeaves,
                                               uLeaf, uSubLeaf);
    if (pcLeaf)
    {
        memcpy(pLeaf, pcLeaf, sizeof(*pLeaf));
        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}


/**
 * Inserts a CPU ID leaf, replacing any existing ones.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pNewLeaf    Pointer to the leaf being inserted.
 */
VMMR3DECL(int) CPUMR3CpuIdInsert(PVM pVM, PCPUMCPUIDLEAF pNewLeaf)
{
    /*
     * Validate parameters.
     */
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pNewLeaf, VERR_INVALID_PARAMETER);

    /*
     * Disallow replacing CPU ID leaves that this API currently cannot manage.
     * These leaves have dependencies on saved-states, see PATMCpuidReplacement().
     * If you want to modify these leaves, use CPUMSetGuestCpuIdFeature().
     */
    if (   pNewLeaf->uLeaf == UINT32_C(0x00000000)  /* Standard */
        || pNewLeaf->uLeaf == UINT32_C(0x80000000)  /* Extended */
        || pNewLeaf->uLeaf == UINT32_C(0xc0000000)) /* Centaur */
    {
        return VERR_NOT_SUPPORTED;
    }

    return cpumR3CpuIdInsert(pVM, NULL /* ppaLeaves */, NULL /* pcLeaves */, pNewLeaf);
}

/**
 * Collects CPUID leaves and sub-leaves, returning a sorted array of them.
 *
 * @returns VBox status code.
 * @param   ppaLeaves           Where to return the array pointer on success.
 *                              Use RTMemFree to release.
 * @param   pcLeaves            Where to return the size of the array on
 *                              success.
 */
VMMR3DECL(int) CPUMR3CpuIdCollectLeaves(PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves)
{
    *ppaLeaves = NULL;
    *pcLeaves = 0;

    /*
     * Try out various candidates. This must be sorted!
     */
    static struct { uint32_t uMsr; bool fSpecial; } const s_aCandidates[] =
    {
        { UINT32_C(0x00000000), false },
        { UINT32_C(0x10000000), false },
        { UINT32_C(0x20000000), false },
        { UINT32_C(0x30000000), false },
        { UINT32_C(0x40000000), false },
        { UINT32_C(0x50000000), false },
        { UINT32_C(0x60000000), false },
        { UINT32_C(0x70000000), false },
        { UINT32_C(0x80000000), false },
        { UINT32_C(0x80860000), false },
        { UINT32_C(0x8ffffffe), true  },
        { UINT32_C(0x8fffffff), true  },
        { UINT32_C(0x90000000), false },
        { UINT32_C(0xa0000000), false },
        { UINT32_C(0xb0000000), false },
        { UINT32_C(0xc0000000), false },
        { UINT32_C(0xd0000000), false },
        { UINT32_C(0xe0000000), false },
        { UINT32_C(0xf0000000), false },
    };

    for (uint32_t iOuter = 0; iOuter < RT_ELEMENTS(s_aCandidates); iOuter++)
    {
        uint32_t uLeaf = s_aCandidates[iOuter].uMsr;
        uint32_t uEax, uEbx, uEcx, uEdx;
        ASMCpuIdExSlow(uLeaf, 0, 0, 0, &uEax, &uEbx, &uEcx, &uEdx);

        /*
         * Does EAX look like a typical leaf count value?
         */
        if (   uEax         > uLeaf
            && uEax - uLeaf < UINT32_C(0xff)) /* Adjust 0xff limit when exceeded by real HW. */
        {
            /* Yes, dump them. */
            uint32_t cLeaves = uEax - uLeaf + 1;
            while (cLeaves-- > 0)
            {
                /* Check three times here to reduce the chance of CPU migration
                   resulting in false positives with things like the APIC ID. */
                uint32_t cSubLeaves;
                bool fFinalEcxUnchanged;
                if (   cpumR3IsEcxRelevantForCpuIdLeaf(uLeaf, &cSubLeaves, &fFinalEcxUnchanged)
                    && cpumR3IsEcxRelevantForCpuIdLeaf(uLeaf, &cSubLeaves, &fFinalEcxUnchanged)
                    && cpumR3IsEcxRelevantForCpuIdLeaf(uLeaf, &cSubLeaves, &fFinalEcxUnchanged))
                {
                    if (cSubLeaves > 16)
                    {
                        /* This shouldn't happen.  But in case it does, file all
                           relevant details in the release log. */
                        LogRel(("CPUM: VERR_CPUM_TOO_MANY_CPUID_SUBLEAVES! uLeaf=%#x cSubLeaves=%#x\n", uLeaf, cSubLeaves));
                        LogRel(("------------------ dump of problematic subleaves ------------------\n"));
                        for (uint32_t uSubLeaf = 0; uSubLeaf < 128; uSubLeaf++)
                        {
                            uint32_t auTmp[4];
                            ASMCpuIdExSlow(uLeaf, 0, uSubLeaf, 0, &auTmp[0], &auTmp[1], &auTmp[2], &auTmp[3]);
                            LogRel(("CPUM: %#010x, %#010x => %#010x %#010x %#010x %#010x\n",
                                    uLeaf, uSubLeaf, auTmp[0], auTmp[1], auTmp[2], auTmp[3]));
                        }
                        LogRel(("----------------- dump of what we've found so far -----------------\n"));
                        for (uint32_t i = 0 ; i < *pcLeaves; i++)
                            LogRel(("CPUM: %#010x, %#010x/%#010x => %#010x %#010x %#010x %#010x\n",
                                    (*ppaLeaves)[i].uLeaf, (*ppaLeaves)[i].uSubLeaf,  (*ppaLeaves)[i].fSubLeafMask,
                                    (*ppaLeaves)[i].uEax, (*ppaLeaves)[i].uEbx, (*ppaLeaves)[i].uEcx, (*ppaLeaves)[i].uEdx));
                        LogRel(("\nPlease create a defect on virtualbox.org and attach this log file!\n\n"));
                        return VERR_CPUM_TOO_MANY_CPUID_SUBLEAVES;
                    }

                    for (uint32_t uSubLeaf = 0; uSubLeaf < cSubLeaves; uSubLeaf++)
                    {
                        ASMCpuIdExSlow(uLeaf, 0, uSubLeaf, 0, &uEax, &uEbx, &uEcx, &uEdx);
                        int rc = cpumR3CollectCpuIdInfoAddOne(ppaLeaves, pcLeaves,
                                                              uLeaf, uSubLeaf, UINT32_MAX, uEax, uEbx, uEcx, uEdx,
                                                              uSubLeaf + 1 == cSubLeaves && fFinalEcxUnchanged
                                                              ? CPUMCPUIDLEAF_F_SUBLEAVES_ECX_UNCHANGED : 0);
                        if (RT_FAILURE(rc))
                            return rc;
                    }
                }
                else
                {
                    ASMCpuIdExSlow(uLeaf, 0, 0, 0, &uEax, &uEbx, &uEcx, &uEdx);
                    int rc = cpumR3CollectCpuIdInfoAddOne(ppaLeaves, pcLeaves,
                                                          uLeaf, 0, 0, uEax, uEbx, uEcx, uEdx, 0);
                    if (RT_FAILURE(rc))
                        return rc;
                }

                /* next */
                uLeaf++;
            }
        }
        /*
         * Special CPUIDs needs special handling as they don't follow the
         * leaf count principle used above.
         */
        else if (s_aCandidates[iOuter].fSpecial)
        {
            bool fKeep = false;
            if (uLeaf == 0x8ffffffe && uEax == UINT32_C(0x00494544))
                fKeep = true;
            else if (   uLeaf == 0x8fffffff
                     && RT_C_IS_PRINT(RT_BYTE1(uEax))
                     && RT_C_IS_PRINT(RT_BYTE2(uEax))
                     && RT_C_IS_PRINT(RT_BYTE3(uEax))
                     && RT_C_IS_PRINT(RT_BYTE4(uEax))
                     && RT_C_IS_PRINT(RT_BYTE1(uEbx))
                     && RT_C_IS_PRINT(RT_BYTE2(uEbx))
                     && RT_C_IS_PRINT(RT_BYTE3(uEbx))
                     && RT_C_IS_PRINT(RT_BYTE4(uEbx))
                     && RT_C_IS_PRINT(RT_BYTE1(uEcx))
                     && RT_C_IS_PRINT(RT_BYTE2(uEcx))
                     && RT_C_IS_PRINT(RT_BYTE3(uEcx))
                     && RT_C_IS_PRINT(RT_BYTE4(uEcx))
                     && RT_C_IS_PRINT(RT_BYTE1(uEdx))
                     && RT_C_IS_PRINT(RT_BYTE2(uEdx))
                     && RT_C_IS_PRINT(RT_BYTE3(uEdx))
                     && RT_C_IS_PRINT(RT_BYTE4(uEdx)) )
                fKeep = true;
            if (fKeep)
            {
                int rc = cpumR3CollectCpuIdInfoAddOne(ppaLeaves, pcLeaves,
                                                      uLeaf, 0, 0, uEax, uEbx, uEcx, uEdx, 0);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Determines the method the CPU uses to handle unknown CPUID leaves.
 *
 * @returns VBox status code.
 * @param   penmUnknownMethod   Where to return the method.
 * @param   pDefUnknown         Where to return default unknown values.  This
 *                              will be set, even if the resulting method
 *                              doesn't actually needs it.
 */
VMMR3DECL(int) CPUMR3CpuIdDetectUnknownLeafMethod(PCPUMUKNOWNCPUID penmUnknownMethod, PCPUMCPUID pDefUnknown)
{
    uint32_t uLastStd = ASMCpuId_EAX(0);
    uint32_t uLastExt = ASMCpuId_EAX(0x80000000);
    if (!ASMIsValidExtRange(uLastExt))
        uLastExt = 0x80000000;

    uint32_t auChecks[] =
    {
        uLastStd + 1,
        uLastStd + 5,
        uLastStd + 8,
        uLastStd + 32,
        uLastStd + 251,
        uLastExt + 1,
        uLastExt + 8,
        uLastExt + 15,
        uLastExt + 63,
        uLastExt + 255,
        0x7fbbffcc,
        0x833f7872,
        0xefff2353,
        0x35779456,
        0x1ef6d33e,
    };

    static const uint32_t s_auValues[] =
    {
        0xa95d2156,
        0x00000001,
        0x00000002,
        0x00000008,
        0x00000000,
        0x55773399,
        0x93401769,
        0x12039587,
    };

    /*
     * Simple method, all zeros.
     */
    *penmUnknownMethod = CPUMUKNOWNCPUID_DEFAULTS;
    pDefUnknown->eax = 0;
    pDefUnknown->ebx = 0;
    pDefUnknown->ecx = 0;
    pDefUnknown->edx = 0;

    /*
     * Intel has been observed returning the last standard leaf.
     */
    uint32_t auLast[4];
    ASMCpuIdExSlow(uLastStd, 0, 0, 0, &auLast[0], &auLast[1], &auLast[2], &auLast[3]);

    uint32_t cChecks = RT_ELEMENTS(auChecks);
    while (cChecks > 0)
    {
        uint32_t auCur[4];
        ASMCpuIdExSlow(auChecks[cChecks - 1], 0, 0, 0, &auCur[0], &auCur[1], &auCur[2], &auCur[3]);
        if (memcmp(auCur, auLast, sizeof(auCur)))
            break;
        cChecks--;
    }
    if (cChecks == 0)
    {
        /* Now, what happens when the input changes? Esp. ECX. */
        uint32_t cTotal       = 0;
        uint32_t cSame        = 0;
        uint32_t cLastWithEcx = 0;
        uint32_t cNeither     = 0;
        uint32_t cValues = RT_ELEMENTS(s_auValues);
        while (cValues > 0)
        {
            uint32_t uValue = s_auValues[cValues - 1];
            uint32_t auLastWithEcx[4];
            ASMCpuIdExSlow(uLastStd, uValue, uValue, uValue,
                           &auLastWithEcx[0], &auLastWithEcx[1], &auLastWithEcx[2], &auLastWithEcx[3]);

            cChecks = RT_ELEMENTS(auChecks);
            while (cChecks > 0)
            {
                uint32_t auCur[4];
                ASMCpuIdExSlow(auChecks[cChecks - 1], uValue, uValue, uValue, &auCur[0], &auCur[1], &auCur[2], &auCur[3]);
                if (!memcmp(auCur, auLast, sizeof(auCur)))
                {
                    cSame++;
                    if (!memcmp(auCur, auLastWithEcx, sizeof(auCur)))
                        cLastWithEcx++;
                }
                else if (!memcmp(auCur, auLastWithEcx, sizeof(auCur)))
                    cLastWithEcx++;
                else
                    cNeither++;
                cTotal++;
                cChecks--;
            }
            cValues--;
        }

        Log(("CPUM: cNeither=%d cSame=%d cLastWithEcx=%d cTotal=%d\n", cNeither, cSame, cLastWithEcx, cTotal));
        if (cSame == cTotal)
            *penmUnknownMethod = CPUMUKNOWNCPUID_LAST_STD_LEAF;
        else if (cLastWithEcx == cTotal)
            *penmUnknownMethod = CPUMUKNOWNCPUID_LAST_STD_LEAF_WITH_ECX;
        else
            *penmUnknownMethod = CPUMUKNOWNCPUID_LAST_STD_LEAF;
        pDefUnknown->eax = auLast[0];
        pDefUnknown->ebx = auLast[1];
        pDefUnknown->ecx = auLast[2];
        pDefUnknown->edx = auLast[3];
        return VINF_SUCCESS;
    }

    /*
     * Unchanged register values?
     */
    cChecks = RT_ELEMENTS(auChecks);
    while (cChecks > 0)
    {
        uint32_t const  uLeaf   = auChecks[cChecks - 1];
        uint32_t        cValues = RT_ELEMENTS(s_auValues);
        while (cValues > 0)
        {
            uint32_t uValue = s_auValues[cValues - 1];
            uint32_t auCur[4];
            ASMCpuIdExSlow(uLeaf, uValue, uValue, uValue, &auCur[0], &auCur[1], &auCur[2], &auCur[3]);
            if (   auCur[0] != uLeaf
                || auCur[1] != uValue
                || auCur[2] != uValue
                || auCur[3] != uValue)
                break;
            cValues--;
        }
        if (cValues != 0)
            break;
        cChecks--;
    }
    if (cChecks == 0)
    {
        *penmUnknownMethod = CPUMUKNOWNCPUID_PASSTHRU;
        return VINF_SUCCESS;
    }

    /*
     * Just go with the simple method.
     */
    return VINF_SUCCESS;
}


/**
 * Translates a unknow CPUID leaf method into the constant name (sans prefix).
 *
 * @returns Read only name string.
 * @param   enmUnknownMethod    The method to translate.
 */
VMMR3DECL(const char *) CPUMR3CpuIdUnknownLeafMethodName(CPUMUKNOWNCPUID enmUnknownMethod)
{
    switch (enmUnknownMethod)
    {
        case CPUMUKNOWNCPUID_DEFAULTS:                  return "DEFAULTS";
        case CPUMUKNOWNCPUID_LAST_STD_LEAF:             return "LAST_STD_LEAF";
        case CPUMUKNOWNCPUID_LAST_STD_LEAF_WITH_ECX:    return "LAST_STD_LEAF_WITH_ECX";
        case CPUMUKNOWNCPUID_PASSTHRU:                  return "PASSTHRU";

        case CPUMUKNOWNCPUID_INVALID:
        case CPUMUKNOWNCPUID_END:
        case CPUMUKNOWNCPUID_32BIT_HACK:
            break;
    }
    return "Invalid-unknown-CPUID-method";
}


/**
 * Detect the CPU vendor give n the
 *
 * @returns The vendor.
 * @param   uEAX                EAX from CPUID(0).
 * @param   uEBX                EBX from CPUID(0).
 * @param   uECX                ECX from CPUID(0).
 * @param   uEDX                EDX from CPUID(0).
 */
VMMR3DECL(CPUMCPUVENDOR) CPUMR3CpuIdDetectVendorEx(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX)
{
    if (ASMIsValidStdRange(uEAX))
    {
        if (ASMIsAmdCpuEx(uEBX, uECX, uEDX))
            return CPUMCPUVENDOR_AMD;

        if (ASMIsIntelCpuEx(uEBX, uECX, uEDX))
            return CPUMCPUVENDOR_INTEL;

        if (ASMIsViaCentaurCpuEx(uEBX, uECX, uEDX))
            return CPUMCPUVENDOR_VIA;

        if (   uEBX == UINT32_C(0x69727943) /* CyrixInstead */
            && uECX == UINT32_C(0x64616574)
            && uEDX == UINT32_C(0x736E4978))
            return CPUMCPUVENDOR_CYRIX;

        /* "Geode by NSC", example: family 5, model 9.  */

        /** @todo detect the other buggers... */
    }

    return CPUMCPUVENDOR_UNKNOWN;
}


/**
 * Translates a CPU vendor enum value into the corresponding string constant.
 *
 * The named can be prefixed with 'CPUMCPUVENDOR_' to construct a valid enum
 * value name.  This can be useful when generating code.
 *
 * @returns Read only name string.
 * @param   enmVendor           The CPU vendor value.
 */
VMMR3DECL(const char *) CPUMR3CpuVendorName(CPUMCPUVENDOR enmVendor)
{
    switch (enmVendor)
    {
        case CPUMCPUVENDOR_INTEL:       return "INTEL";
        case CPUMCPUVENDOR_AMD:         return "AMD";
        case CPUMCPUVENDOR_VIA:         return "VIA";
        case CPUMCPUVENDOR_CYRIX:       return "CYRIX";
        case CPUMCPUVENDOR_UNKNOWN:     return "UNKNOWN";

        case CPUMCPUVENDOR_INVALID:
        case CPUMCPUVENDOR_32BIT_HACK:
            break;
    }
    return "Invalid-cpu-vendor";
}


static PCCPUMCPUIDLEAF cpumR3CpuIdFindLeaf(PCCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf)
{
    /* Could do binary search, doing linear now because I'm lazy. */
    PCCPUMCPUIDLEAF pLeaf = paLeaves;
    while (cLeaves-- > 0)
    {
        if (pLeaf->uLeaf == uLeaf)
            return pLeaf;
        pLeaf++;
    }
    return NULL;
}


int cpumR3CpuIdExplodeFeatures(PCCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, PCPUMFEATURES pFeatures)
{
    RT_ZERO(*pFeatures);
    if (cLeaves >= 2)
    {
        AssertLogRelReturn(paLeaves[0].uLeaf == 0, VERR_CPUM_IPE_1);
        AssertLogRelReturn(paLeaves[1].uLeaf == 1, VERR_CPUM_IPE_1);

        pFeatures->enmCpuVendor = CPUMR3CpuIdDetectVendorEx(paLeaves[0].uEax,
                                                            paLeaves[0].uEbx,
                                                            paLeaves[0].uEcx,
                                                            paLeaves[0].uEdx);
        pFeatures->uFamily      = ASMGetCpuFamily(paLeaves[1].uEax);
        pFeatures->uModel       = ASMGetCpuModel(paLeaves[1].uEax, pFeatures->enmCpuVendor == CPUMCPUVENDOR_INTEL);
        pFeatures->uStepping    = ASMGetCpuStepping(paLeaves[1].uEax);
        pFeatures->enmMicroarch = CPUMR3CpuIdDetermineMicroarchEx((CPUMCPUVENDOR)pFeatures->enmCpuVendor,
                                                                  pFeatures->uFamily,
                                                                  pFeatures->uModel,
                                                                  pFeatures->uStepping);

        PCCPUMCPUIDLEAF pLeaf = cpumR3CpuIdFindLeaf(paLeaves, cLeaves, 0x80000008);
        if (pLeaf)
            pFeatures->cMaxPhysAddrWidth = pLeaf->uEax & 0xff;
        else if (paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_PSE36)
            pFeatures->cMaxPhysAddrWidth = 36;
        else
            pFeatures->cMaxPhysAddrWidth = 32;

        /* Standard features. */
        pFeatures->fMsr                 = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_MSR);
        pFeatures->fApic                = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_APIC);
        pFeatures->fX2Apic              = RT_BOOL(paLeaves[1].uEcx & X86_CPUID_FEATURE_ECX_X2APIC);
        pFeatures->fPse                 = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_PSE);
        pFeatures->fPse36               = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_PSE36);
        pFeatures->fPae                 = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_PAE);
        pFeatures->fPat                 = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_PAT);
        pFeatures->fFxSaveRstor         = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_FXSR);
        pFeatures->fSysEnter            = RT_BOOL(paLeaves[1].uEdx & X86_CPUID_FEATURE_EDX_SEP);
        pFeatures->fHypervisorPresent   = RT_BOOL(paLeaves[1].uEcx & X86_CPUID_FEATURE_ECX_HVP);
        pFeatures->fMonitorMWait        = RT_BOOL(paLeaves[1].uEcx & X86_CPUID_FEATURE_ECX_MONITOR);

        /* MWAIT/MONITOR leaf. */
        PCCPUMCPUIDLEAF const pMWaitLeaf = cpumR3CpuIdFindLeaf(paLeaves, cLeaves, 5);
        if (pMWaitLeaf)
        {
            pFeatures->fMWaitExtensions = (pMWaitLeaf->uEcx & (X86_CPUID_MWAIT_ECX_EXT | X86_CPUID_MWAIT_ECX_BREAKIRQIF0))
                                                           == (X86_CPUID_MWAIT_ECX_EXT | X86_CPUID_MWAIT_ECX_BREAKIRQIF0);
        }

        /* Extended features. */
        PCCPUMCPUIDLEAF const pExtLeaf  = cpumR3CpuIdFindLeaf(paLeaves, cLeaves, 0x80000001);
        if (pExtLeaf)
        {
            pFeatures->fLongMode        = RT_BOOL(pExtLeaf->uEdx & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE);
            pFeatures->fSysCall         = RT_BOOL(pExtLeaf->uEdx & X86_CPUID_EXT_FEATURE_EDX_SYSCALL);
            pFeatures->fNoExecute       = RT_BOOL(pExtLeaf->uEdx & X86_CPUID_EXT_FEATURE_EDX_NX);
            pFeatures->fLahfSahf        = RT_BOOL(pExtLeaf->uEcx & X86_CPUID_EXT_FEATURE_ECX_LAHF_SAHF);
            pFeatures->fRdTscP          = RT_BOOL(pExtLeaf->uEdx & X86_CPUID_EXT_FEATURE_EDX_RDTSCP);
        }

        if (   pExtLeaf
            && pFeatures->enmCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* AMD features. */
            pFeatures->fMsr            |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_MSR);
            pFeatures->fApic           |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_APIC);
            pFeatures->fPse            |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_PSE);
            pFeatures->fPse36          |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_PSE36);
            pFeatures->fPae            |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_PAE);
            pFeatures->fPat            |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_PAT);
            pFeatures->fFxSaveRstor    |= RT_BOOL(pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_FXSR);
        }

        /*
         * Quirks.
         */
        pFeatures->fLeakyFxSR = pExtLeaf
                             && (pExtLeaf->uEdx & X86_CPUID_AMD_FEATURE_EDX_FFXSR)
                             && pFeatures->enmCpuVendor == CPUMCPUVENDOR_AMD
                             && pFeatures->uFamily >= 6 /* K7 and up */;
    }
    else
        AssertLogRelReturn(cLeaves == 0, VERR_CPUM_IPE_1);
    return VINF_SUCCESS;
}


/*
 *
 * Init related code.
 * Init related code.
 * Init related code.
 *
 *
 */
#ifdef VBOX_IN_VMM

/**
 * Loads MSR range overrides.
 *
 * This must be called before the MSR ranges are moved from the normal heap to
 * the hyper heap!
 *
 * @returns VBox status code (VMSetError called).
 * @param   pVM                 Pointer to the cross context VM structure
 * @param   pMsrNode            The CFGM node with the MSR overrides.
 */
static int cpumR3LoadMsrOverrides(PVM pVM, PCFGMNODE pMsrNode)
{
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pMsrNode); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Assemble a valid MSR range.
         */
        CPUMMSRRANGE MsrRange;
        MsrRange.offCpumCpu = 0;
        MsrRange.fReserved  = 0;

        int rc = CFGMR3GetName(pNode, MsrRange.szName, sizeof(MsrRange.szName));
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry (name is probably too long): %Rrc\n", rc);

        rc = CFGMR3QueryU32(pNode, "First", &MsrRange.uFirst);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying mandatory 'First' value: %Rrc\n",
                              MsrRange.szName, rc);

        rc = CFGMR3QueryU32Def(pNode, "Last", &MsrRange.uLast, MsrRange.uFirst);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying 'Last' value: %Rrc\n",
                              MsrRange.szName, rc);

        char szType[32];
        rc = CFGMR3QueryStringDef(pNode, "Type", szType, sizeof(szType), "FixedValue");
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying 'Type' value: %Rrc\n",
                              MsrRange.szName, rc);
        if (!RTStrICmp(szType, "FixedValue"))
        {
            MsrRange.enmRdFn = kCpumMsrRdFn_FixedValue;
            MsrRange.enmWrFn = kCpumMsrWrFn_IgnoreWrite;

            rc = CFGMR3QueryU64Def(pNode, "Value", &MsrRange.uValue, 0);
            if (RT_FAILURE(rc))
                return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying 'Value' value: %Rrc\n",
                                  MsrRange.szName, rc);

            rc = CFGMR3QueryU64Def(pNode, "WrGpMask", &MsrRange.fWrGpMask, 0);
            if (RT_FAILURE(rc))
                return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying 'WrGpMask' value: %Rrc\n",
                                  MsrRange.szName, rc);

            rc = CFGMR3QueryU64Def(pNode, "WrIgnMask", &MsrRange.fWrIgnMask, 0);
            if (RT_FAILURE(rc))
                return VMSetError(pVM, rc, RT_SRC_POS, "Invalid MSR entry '%s': Error querying 'WrIgnMask' value: %Rrc\n",
                                  MsrRange.szName, rc);
        }
        else
            return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                              "Invalid MSR entry '%s': Unknown type '%s'\n", MsrRange.szName, szType);

        /*
         * Insert the range into the table (replaces/splits/shrinks existing
         * MSR ranges).
         */
        rc = cpumR3MsrRangesInsert(NULL /* pVM */, &pVM->cpum.s.GuestInfo.paMsrRangesR3, &pVM->cpum.s.GuestInfo.cMsrRanges,
                                   &MsrRange);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Error adding MSR entry '%s': %Rrc\n", MsrRange.szName, rc);
    }

    return VINF_SUCCESS;
}


/**
 * Loads CPUID leaf overrides.
 *
 * This must be called before the CPUID leaves are moved from the normal
 * heap to the hyper heap!
 *
 * @returns VBox status code (VMSetError called).
 * @param   pVM             Pointer to the cross context VM structure
 * @param   pParentNode     The CFGM node with the CPUID leaves.
 * @param   pszLabel        How to label the overrides we're loading.
 */
static int cpumR3LoadCpuIdOverrides(PVM pVM, PCFGMNODE pParentNode, const char *pszLabel)
{
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pParentNode); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Get the leaf and subleaf numbers.
         */
        char szName[128];
        int rc = CFGMR3GetName(pNode, szName, sizeof(szName));
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry (name is probably too long): %Rrc\n", pszLabel, rc);

        /* The leaf number is either specified directly or thru the node name. */
        uint32_t uLeaf;
        rc = CFGMR3QueryU32(pNode, "Leaf", &uLeaf);
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            rc = RTStrToUInt32Full(szName, 16, &uLeaf);
            if (rc != VINF_SUCCESS)
                return VMSetError(pVM, VERR_INVALID_NAME, RT_SRC_POS,
                                  "Invalid %s entry: Invalid leaf number: '%s' \n", pszLabel, szName);
        }
        else if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'Leaf' value: %Rrc\n",
                              pszLabel, szName, rc);

        uint32_t uSubLeaf;
        rc = CFGMR3QueryU32Def(pNode, "SubLeaf", &uSubLeaf, 0);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'SubLeaf' value: %Rrc\n",
                              pszLabel, szName, rc);

        uint32_t fSubLeafMask;
        rc = CFGMR3QueryU32Def(pNode, "SubLeafMask", &fSubLeafMask, 0);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'SubLeafMask' value: %Rrc\n",
                              pszLabel, szName, rc);

        /*
         * Look up the specified leaf, since the output register values
         * defaults to any existing values.  This allows overriding a single
         * register, without needing to know the other values.
         */
        PCCPUMCPUIDLEAF pLeaf = cpumR3CpuIdGetLeaf(pVM->cpum.s.GuestInfo.paCpuIdLeavesR3, pVM->cpum.s.GuestInfo.cCpuIdLeaves,
                                                   uLeaf, uSubLeaf);
        CPUMCPUIDLEAF   Leaf;
        if (pLeaf)
            Leaf = *pLeaf;
        else
            RT_ZERO(Leaf);
        Leaf.uLeaf          = uLeaf;
        Leaf.uSubLeaf       = uSubLeaf;
        Leaf.fSubLeafMask   = fSubLeafMask;

        rc = CFGMR3QueryU32Def(pNode, "eax", &Leaf.uEax, Leaf.uEax);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'eax' value: %Rrc\n",
                              pszLabel, szName, rc);
        rc = CFGMR3QueryU32Def(pNode, "ebx", &Leaf.uEbx, Leaf.uEbx);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'ebx' value: %Rrc\n",
                              pszLabel, szName, rc);
        rc = CFGMR3QueryU32Def(pNode, "ecx", &Leaf.uEcx, Leaf.uEcx);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'ecx' value: %Rrc\n",
                              pszLabel, szName, rc);
        rc = CFGMR3QueryU32Def(pNode, "edx", &Leaf.uEdx, Leaf.uEdx);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Invalid %s entry '%s': Error querying 'edx' value: %Rrc\n",
                              pszLabel, szName, rc);

        /*
         * Insert the leaf into the table (replaces existing ones).
         */
        rc = cpumR3CpuIdInsert(NULL /* pVM */, &pVM->cpum.s.GuestInfo.paCpuIdLeavesR3, &pVM->cpum.s.GuestInfo.cCpuIdLeaves,
                               &Leaf);
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Error adding CPUID leaf entry '%s': %Rrc\n", szName, rc);
    }

    return VINF_SUCCESS;
}



/**
 * Fetches overrides for a CPUID leaf.
 *
 * @returns VBox status code.
 * @param   pLeaf               The leaf to load the overrides into.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 * @param   iLeaf               The CPUID leaf number.
 */
static int cpumR3CpuIdFetchLeafOverride(PCPUMCPUID pLeaf, PCFGMNODE pCfgNode, uint32_t iLeaf)
{
    PCFGMNODE pLeafNode = CFGMR3GetChildF(pCfgNode, "%RX32", iLeaf);
    if (pLeafNode)
    {
        uint32_t u32;
        int rc = CFGMR3QueryU32(pLeafNode, "eax", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->eax = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ebx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ebx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ecx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ecx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "edx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->edx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

    }
    return VINF_SUCCESS;
}


/**
 * Load the overrides for a set of CPUID leaves.
 *
 * @returns VBox status code.
 * @param   paLeaves            The leaf array.
 * @param   cLeaves             The number of leaves.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 */
static int cpumR3CpuIdInitLoadOverrideSet(uint32_t uStart, PCPUMCPUID paLeaves, uint32_t cLeaves, PCFGMNODE pCfgNode)
{
    for (uint32_t i = 0; i < cLeaves; i++)
    {
        int rc = cpumR3CpuIdFetchLeafOverride(&paLeaves[i], pCfgNode, uStart + i);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

/**
 * Init a set of host CPUID leaves.
 *
 * @returns VBox status code.
 * @param   paLeaves            The leaf array.
 * @param   cLeaves             The number of leaves.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The /CPUM/HostCPUID/ node.
 */
static int cpumR3CpuIdInitHostSet(uint32_t uStart, PCPUMCPUID paLeaves, uint32_t cLeaves, PCFGMNODE pCfgNode)
{
    /* Using the ECX variant for all of them can't hurt... */
    for (uint32_t i = 0; i < cLeaves; i++)
        ASMCpuIdExSlow(uStart + i, 0, 0, 0, &paLeaves[i].eax, &paLeaves[i].ebx, &paLeaves[i].ecx, &paLeaves[i].edx);

    /* Load CPUID leaf override; we currently don't care if the user
       specifies features the host CPU doesn't support. */
    return cpumR3CpuIdInitLoadOverrideSet(uStart, paLeaves, cLeaves, pCfgNode);
}


static int cpumR3CpuIdInstallAndExplodeLeaves(PVM pVM, PCPUM pCPUM, PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves)
{
    /*
     * Install the CPUID information.
     */
    int rc = MMHyperDupMem(pVM, paLeaves, sizeof(paLeaves[0]) * cLeaves, 32,
                           MM_TAG_CPUM_CPUID, (void **)&pCPUM->GuestInfo.paCpuIdLeavesR3);

    AssertLogRelRCReturn(rc, rc);


    pCPUM->GuestInfo.paCpuIdLeavesR0 = MMHyperR3ToR0(pVM, pCPUM->GuestInfo.paCpuIdLeavesR3);
    pCPUM->GuestInfo.paCpuIdLeavesRC = MMHyperR3ToRC(pVM, pCPUM->GuestInfo.paCpuIdLeavesR3);
    Assert(MMHyperR0ToR3(pVM, pCPUM->GuestInfo.paCpuIdLeavesR0) == (void *)pCPUM->GuestInfo.paCpuIdLeavesR3);
    Assert(MMHyperRCToR3(pVM, pCPUM->GuestInfo.paCpuIdLeavesRC) == (void *)pCPUM->GuestInfo.paCpuIdLeavesR3);

    /*
     * Explode the guest CPU features.
     */
    rc = cpumR3CpuIdExplodeFeatures(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, &pCPUM->GuestFeatures);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Adjust the scalable bus frequency according to the CPUID information
     * we're now using.
     */
    if (CPUMMICROARCH_IS_INTEL_CORE7(pVM->cpum.s.GuestFeatures.enmMicroarch))
        pCPUM->GuestInfo.uScalableBusFreq = pCPUM->GuestFeatures.enmMicroarch >= kCpumMicroarch_Intel_Core7_SandyBridge
                                          ? UINT64_C(100000000)  /* 100MHz */
                                          : UINT64_C(133333333); /* 133MHz */

    /*
     * Populate the legacy arrays.  Currently used for everything, later only
     * for patch manager.
     */
    struct { PCPUMCPUID paCpuIds; uint32_t cCpuIds, uBase; } aOldRanges[] =
    {
        { pCPUM->aGuestCpuIdStd,        RT_ELEMENTS(pCPUM->aGuestCpuIdStd),     0x00000000 },
        { pCPUM->aGuestCpuIdExt,        RT_ELEMENTS(pCPUM->aGuestCpuIdExt),     0x80000000 },
        { pCPUM->aGuestCpuIdCentaur,    RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur), 0xc0000000 },
    };
    for (uint32_t i = 0; i < RT_ELEMENTS(aOldRanges); i++)
    {
        uint32_t    cLeft       = aOldRanges[i].cCpuIds;
        uint32_t    uLeaf       = aOldRanges[i].uBase + cLeft;
        PCPUMCPUID  pLegacyLeaf = &aOldRanges[i].paCpuIds[cLeft];
        while (cLeft-- > 0)
        {
            uLeaf--;
            pLegacyLeaf--;

            PCCPUMCPUIDLEAF pLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, uLeaf,
                                                       0 /* uSubLeaf */);
            if (pLeaf)
            {
                pLegacyLeaf->eax = pLeaf->uEax;
                pLegacyLeaf->ebx = pLeaf->uEbx;
                pLegacyLeaf->ecx = pLeaf->uEcx;
                pLegacyLeaf->edx = pLeaf->uEdx;
            }
            else
                *pLegacyLeaf = pCPUM->GuestInfo.DefCpuId;
        }
    }

    pCPUM->GuestCpuIdDef = pCPUM->GuestInfo.DefCpuId;

    return VINF_SUCCESS;
}


/**
 * Initializes the emulated CPU's cpuid information.
 *
 * @returns VBox status code.
 * @param   pVM          Pointer to the VM.
 */
int cpumR3CpuIdInit(PVM pVM)
{
    PCPUM       pCPUM    = &pVM->cpum.s;
    PCFGMNODE   pCpumCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM");
    int         rc;

#define PORTABLE_CLEAR_BITS_WHEN(Lvl, a_pLeafReg, FeatNm, fMask, uValue) \
    if ( pCPUM->u8PortableCpuIdLevel >= (Lvl) && ((a_pLeafReg) & (fMask)) == (uValue) ) \
    { \
        LogRel(("PortableCpuId: " #a_pLeafReg "[" #FeatNm "]: %#x -> 0\n", (a_pLeafReg) & (fMask))); \
        (a_pLeafReg) &= ~(uint32_t)(fMask); \
    }
#define PORTABLE_DISABLE_FEATURE_BIT(Lvl, a_pLeafReg, FeatNm, fBitMask) \
    if ( pCPUM->u8PortableCpuIdLevel >= (Lvl) && ((a_pLeafReg) & (fBitMask)) ) \
    { \
        LogRel(("PortableCpuId: " #a_pLeafReg "[" #FeatNm "]: 1 -> 0\n")); \
        (a_pLeafReg) &= ~(uint32_t)(fBitMask); \
    }

    /*
     * Read the configuration.
     */
    /** @cfgm{/CPUM/SyntheticCpu, boolean, false}
     * Enables the Synthetic CPU.  The Vendor ID and Processor Name are
     * completely overridden by VirtualBox custom strings.  Some
     * CPUID information is withheld, like the cache info.
     *
     * This is obsoleted by PortableCpuIdLevel. */
    bool fSyntheticCpu;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "SyntheticCpu",  &fSyntheticCpu, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{/CPUM/PortableCpuIdLevel, 8-bit, 0, 3, 0}
     * When non-zero CPUID features that could cause portability issues will be
     * stripped.  The higher the value the more features gets stripped.  Higher
     * values should only be used when older CPUs are involved since it may
     * harm performance and maybe also cause problems with specific guests. */
    rc = CFGMR3QueryU8Def(pCpumCfg, "PortableCpuIdLevel", &pCPUM->u8PortableCpuIdLevel, fSyntheticCpu ? 1 : 0);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/GuestCpuName, string}
     * The name of the CPU we're to emulate.  The default is the host CPU.
     * Note! CPUs other than "host" one is currently unsupported. */
    char szCpuName[128];
    rc = CFGMR3QueryStringDef(pCpumCfg, "GuestCpuName", szCpuName, sizeof(szCpuName), "host");
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/CMPXCHG16B, boolean, false}
     * Expose CMPXCHG16B to the guest if supported by the host.
     */
    bool fCmpXchg16b;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "CMPXCHG16B", &fCmpXchg16b, false);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/MONITOR, boolean, true}
     * Expose MONITOR/MWAIT instructions to the guest.
     */
    bool fMonitor;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "MONITOR", &fMonitor, true);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/MWaitExtensions, boolean, false}
     * Expose MWAIT extended features to the guest.  For now we expose just MWAIT
     * break on interrupt feature (bit 1).
     */
    bool fMWaitExtensions;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "MWaitExtensions", &fMWaitExtensions, false);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/SSE4.1, boolean, true}
     * Expose SSE4.1 to the guest if available.
     */
    bool fSse41;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "SSE4.1", &fSse41, true);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/SSE4.2, boolean, true}
     * Expose SSE4.2 to the guest if available.
     */
    bool fSse42;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "SSE4.2", &fSse42, true);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/NT4LeafLimit, boolean, false}
     * Limit the number of standard CPUID leaves to 0..3 to prevent NT4 from
     * bugchecking with MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED (0x3e).
     * This option corresponds somewhat to IA32_MISC_ENABLES.BOOT_NT4[bit 22].
     */
    bool fNt4LeafLimit;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "NT4LeafLimit", &fNt4LeafLimit, false);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/MaxIntelFamilyModelStep, uint32_t, UINT32_MAX}
     * Restrict the reported CPU family+model+stepping of intel CPUs.  This is
     * probably going to be a temporary hack, so don't depend on this.
     * The 1st byte of the value is the stepping, the 2nd byte value is the model
     * number and the 3rd byte value is the family, and the 4th value must be zero.
     */
    uint32_t uMaxIntelFamilyModelStep;
    rc = CFGMR3QueryU32Def(pCpumCfg, "MaxIntelFamilyModelStep", &uMaxIntelFamilyModelStep, UINT32_MAX);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Get the guest CPU data from the database and/or the host.
     */
    rc = cpumR3DbGetCpuInfo(szCpuName, &pCPUM->GuestInfo);
    if (RT_FAILURE(rc))
        return rc == VERR_CPUM_DB_CPU_NOT_FOUND
             ? VMSetError(pVM, rc, RT_SRC_POS,
                          "Info on guest CPU '%s' could not be found. Please, select a different CPU.", szCpuName)
             : rc;

    /** @cfgm{/CPUM/MSRs/[Name]/[First|Last|Type|Value|...],}
     * Overrides the guest MSRs.
     */
    rc = cpumR3LoadMsrOverrides(pVM, CFGMR3GetChild(pCpumCfg, "MSRs"));

    /** @cfgm{/CPUM/HostCPUID/[000000xx|800000xx|c000000x]/[eax|ebx|ecx|edx],32-bit}
     * Overrides the CPUID leaf values (from the host CPU usually) used for
     * calculating the guest CPUID leaves.  This can be used to preserve the CPUID
     * values when moving a VM to a different machine.  Another use is restricting
     * (or extending) the feature set exposed to the guest. */
    if (RT_SUCCESS(rc))
        rc = cpumR3LoadCpuIdOverrides(pVM, CFGMR3GetChild(pCpumCfg, "HostCPUID"), "HostCPUID");

    if (RT_SUCCESS(rc) && CFGMR3GetChild(pCpumCfg, "CPUID")) /* 2nd override, now discontinued. */
        rc = VMSetError(pVM, VERR_CFGM_CONFIG_UNKNOWN_NODE, RT_SRC_POS,
                        "Found unsupported configuration node '/CPUM/CPUID/'. "
                        "Please use IMachine::setCPUIDLeaf() instead.");

    /*
     * Pre-explode the CPUID info.
     */
    if (RT_SUCCESS(rc))
        rc = cpumR3CpuIdExplodeFeatures(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, &pCPUM->GuestFeatures);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pCPUM->GuestInfo.paCpuIdLeavesR3);
        pCPUM->GuestInfo.paCpuIdLeavesR3 = NULL;
        RTMemFree(pCPUM->GuestInfo.paMsrRangesR3);
        pCPUM->GuestInfo.paMsrRangesR3 = NULL;
        return rc;
    }


    /* ... split this function about here ... */


    /* Cpuid 1:
     * Only report features we can support.
     *
     * Note! When enabling new features the Synthetic CPU and Portable CPUID
     *       options may require adjusting (i.e. stripping what was enabled).
     */
    PCPUMCPUIDLEAF pStdFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves,
                                                        1, 0); /* Note! Must refetch when used later. */
    AssertLogRelReturn(pStdFeatureLeaf, VERR_CPUM_IPE_2);
    pStdFeatureLeaf->uEdx        &= X86_CPUID_FEATURE_EDX_FPU
                                  | X86_CPUID_FEATURE_EDX_VME
                                  | X86_CPUID_FEATURE_EDX_DE
                                  | X86_CPUID_FEATURE_EDX_PSE
                                  | X86_CPUID_FEATURE_EDX_TSC
                                  | X86_CPUID_FEATURE_EDX_MSR
                                  //| X86_CPUID_FEATURE_EDX_PAE   - set later if configured.
                                  | X86_CPUID_FEATURE_EDX_MCE
                                  | X86_CPUID_FEATURE_EDX_CX8
                                  //| X86_CPUID_FEATURE_EDX_APIC  - set by the APIC device if present.
                                  /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see @bugref{1757}) */
                                  //| X86_CPUID_FEATURE_EDX_SEP
                                  | X86_CPUID_FEATURE_EDX_MTRR
                                  | X86_CPUID_FEATURE_EDX_PGE
                                  | X86_CPUID_FEATURE_EDX_MCA
                                  | X86_CPUID_FEATURE_EDX_CMOV
                                  | X86_CPUID_FEATURE_EDX_PAT
                                  | X86_CPUID_FEATURE_EDX_PSE36
                                  //| X86_CPUID_FEATURE_EDX_PSN   - no serial number.
                                  | X86_CPUID_FEATURE_EDX_CLFSH
                                  //| X86_CPUID_FEATURE_EDX_DS    - no debug store.
                                  //| X86_CPUID_FEATURE_EDX_ACPI  - not virtualized yet.
                                  | X86_CPUID_FEATURE_EDX_MMX
                                  | X86_CPUID_FEATURE_EDX_FXSR
                                  | X86_CPUID_FEATURE_EDX_SSE
                                  | X86_CPUID_FEATURE_EDX_SSE2
                                  //| X86_CPUID_FEATURE_EDX_SS    - no self snoop.
                                  //| X86_CPUID_FEATURE_EDX_HTT   - no hyperthreading.
                                  //| X86_CPUID_FEATURE_EDX_TM    - no thermal monitor.
                                  //| X86_CPUID_FEATURE_EDX_PBE   - no pending break enabled.
                                  | 0;
    pStdFeatureLeaf->uEcx        &= 0
                                  | X86_CPUID_FEATURE_ECX_SSE3
                                  /* Can't properly emulate monitor & mwait with guest SMP; force the guest to use hlt for idling VCPUs. */
                                  | ((fMonitor && pVM->cCpus == 1) ? X86_CPUID_FEATURE_ECX_MONITOR : 0)
                                  //| X86_CPUID_FEATURE_ECX_CPLDS - no CPL qualified debug store.
                                  //| X86_CPUID_FEATURE_ECX_VMX   - not virtualized.
                                  //| X86_CPUID_FEATURE_ECX_EST   - no extended speed step.
                                  //| X86_CPUID_FEATURE_ECX_TM2   - no thermal monitor 2.
                                    | X86_CPUID_FEATURE_ECX_SSSE3
                                  //| X86_CPUID_FEATURE_ECX_CNTXID - no L1 context id (MSR++).
                                    | (fCmpXchg16b ? X86_CPUID_FEATURE_ECX_CX16 : 0)
                                  /* ECX Bit 14 - xTPR Update Control. Processor supports changing IA32_MISC_ENABLES[bit 23]. */
                                  //| X86_CPUID_FEATURE_ECX_TPRUPDATE
                                  | (fSse41 ? X86_CPUID_FEATURE_ECX_SSE4_1 : 0)
                                  | (fSse42 ? X86_CPUID_FEATURE_ECX_SSE4_2 : 0)
                                  /* ECX Bit 21 - x2APIC support - not yet. */
                                  // | X86_CPUID_FEATURE_ECX_X2APIC
                                  /* ECX Bit 23 - POPCNT instruction. */
                                  //| X86_CPUID_FEATURE_ECX_POPCNT
                                  | 0;
    if (pCPUM->u8PortableCpuIdLevel > 0)
    {
        PORTABLE_CLEAR_BITS_WHEN(1, pStdFeatureLeaf->uEax, ProcessorType, (UINT32_C(3) << 12), (UINT32_C(2) << 12));
        PORTABLE_DISABLE_FEATURE_BIT(1, pStdFeatureLeaf->uEcx, SSSE3, X86_CPUID_FEATURE_ECX_SSSE3);
        PORTABLE_DISABLE_FEATURE_BIT(1, pStdFeatureLeaf->uEcx, SSE3,  X86_CPUID_FEATURE_ECX_SSE3);
        PORTABLE_DISABLE_FEATURE_BIT(1, pStdFeatureLeaf->uEcx, SSE4_1, X86_CPUID_FEATURE_ECX_SSE4_1);
        PORTABLE_DISABLE_FEATURE_BIT(1, pStdFeatureLeaf->uEcx, SSE4_2, X86_CPUID_FEATURE_ECX_SSE4_2);
        PORTABLE_DISABLE_FEATURE_BIT(1, pStdFeatureLeaf->uEcx, CX16,  X86_CPUID_FEATURE_ECX_CX16);
        PORTABLE_DISABLE_FEATURE_BIT(2, pStdFeatureLeaf->uEdx, SSE2,  X86_CPUID_FEATURE_EDX_SSE2);
        PORTABLE_DISABLE_FEATURE_BIT(3, pStdFeatureLeaf->uEdx, SSE,   X86_CPUID_FEATURE_EDX_SSE);
        PORTABLE_DISABLE_FEATURE_BIT(3, pStdFeatureLeaf->uEdx, CLFSH, X86_CPUID_FEATURE_EDX_CLFSH);
        PORTABLE_DISABLE_FEATURE_BIT(3, pStdFeatureLeaf->uEdx, CMOV,  X86_CPUID_FEATURE_EDX_CMOV);

        Assert(!(pStdFeatureLeaf->uEdx        & (  X86_CPUID_FEATURE_EDX_SEP
                                                 | X86_CPUID_FEATURE_EDX_PSN
                                                 | X86_CPUID_FEATURE_EDX_DS
                                                 | X86_CPUID_FEATURE_EDX_ACPI
                                                 | X86_CPUID_FEATURE_EDX_SS
                                                 | X86_CPUID_FEATURE_EDX_TM
                                                 | X86_CPUID_FEATURE_EDX_PBE
                                                 )));
        Assert(!(pStdFeatureLeaf->uEcx        & (  X86_CPUID_FEATURE_ECX_PCLMUL
                                                 | X86_CPUID_FEATURE_ECX_DTES64
                                                 | X86_CPUID_FEATURE_ECX_CPLDS
                                                 | X86_CPUID_FEATURE_ECX_VMX
                                                 | X86_CPUID_FEATURE_ECX_SMX
                                                 | X86_CPUID_FEATURE_ECX_EST
                                                 | X86_CPUID_FEATURE_ECX_TM2
                                                 | X86_CPUID_FEATURE_ECX_CNTXID
                                                 | X86_CPUID_FEATURE_ECX_FMA
                                                 | X86_CPUID_FEATURE_ECX_CX16
                                                 | X86_CPUID_FEATURE_ECX_TPRUPDATE
                                                 | X86_CPUID_FEATURE_ECX_PDCM
                                                 | X86_CPUID_FEATURE_ECX_DCA
                                                 | X86_CPUID_FEATURE_ECX_MOVBE
                                                 | X86_CPUID_FEATURE_ECX_AES
                                                 | X86_CPUID_FEATURE_ECX_POPCNT
                                                 | X86_CPUID_FEATURE_ECX_XSAVE
                                                 | X86_CPUID_FEATURE_ECX_OSXSAVE
                                                 | X86_CPUID_FEATURE_ECX_AVX
                                                 )));
    }

    /* Cpuid 0x80000001:
     * Only report features we can support.
     *
     * Note! When enabling new features the Synthetic CPU and Portable CPUID
     *       options may require adjusting (i.e. stripping what was enabled).
     *
     * ASSUMES that this is ALWAYS the AMD defined feature set if present.
     */
    PCPUMCPUIDLEAF pExtFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves,
                                                        UINT32_C(0x80000001), 0); /* Note! Must refetch when used later. */
    if (pExtFeatureLeaf)
    {
        pExtFeatureLeaf->uEdx    &= X86_CPUID_AMD_FEATURE_EDX_FPU
                                  | X86_CPUID_AMD_FEATURE_EDX_VME
                                  | X86_CPUID_AMD_FEATURE_EDX_DE
                                  | X86_CPUID_AMD_FEATURE_EDX_PSE
                                  | X86_CPUID_AMD_FEATURE_EDX_TSC
                                  | X86_CPUID_AMD_FEATURE_EDX_MSR //?? this means AMD MSRs..
                                  //| X86_CPUID_AMD_FEATURE_EDX_PAE    - not implemented yet.
                                  //| X86_CPUID_AMD_FEATURE_EDX_MCE    - not virtualized yet.
                                  | X86_CPUID_AMD_FEATURE_EDX_CX8
                                  //| X86_CPUID_AMD_FEATURE_EDX_APIC   - set by the APIC device if present.
                                  /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see @bugref{1757}) */
                                  //| X86_CPUID_EXT_FEATURE_EDX_SEP
                                  | X86_CPUID_AMD_FEATURE_EDX_MTRR
                                  | X86_CPUID_AMD_FEATURE_EDX_PGE
                                  | X86_CPUID_AMD_FEATURE_EDX_MCA
                                  | X86_CPUID_AMD_FEATURE_EDX_CMOV
                                  | X86_CPUID_AMD_FEATURE_EDX_PAT
                                  | X86_CPUID_AMD_FEATURE_EDX_PSE36
                                  //| X86_CPUID_EXT_FEATURE_EDX_NX     - not virtualized, requires PAE.
                                  //| X86_CPUID_AMD_FEATURE_EDX_AXMMX
                                  | X86_CPUID_AMD_FEATURE_EDX_MMX
                                  | X86_CPUID_AMD_FEATURE_EDX_FXSR
                                  | X86_CPUID_AMD_FEATURE_EDX_FFXSR
                                  //| X86_CPUID_EXT_FEATURE_EDX_PAGE1GB
                                  | X86_CPUID_EXT_FEATURE_EDX_RDTSCP
                                  //| X86_CPUID_EXT_FEATURE_EDX_LONG_MODE - turned on when necessary
                                  | X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX
                                  | X86_CPUID_AMD_FEATURE_EDX_3DNOW
                                  | 0;
        pExtFeatureLeaf->uEcx    &= 0
                                  //| X86_CPUID_EXT_FEATURE_ECX_LAHF_SAHF
                                  //| X86_CPUID_AMD_FEATURE_ECX_CMPL
                                  //| X86_CPUID_AMD_FEATURE_ECX_SVM    - not virtualized.
                                  //| X86_CPUID_AMD_FEATURE_ECX_EXT_APIC
                                  /* Note: This could prevent teleporting from AMD to Intel CPUs! */
                                  | X86_CPUID_AMD_FEATURE_ECX_CR8L         /* expose lock mov cr0 = mov cr8 hack for guests that can use this feature to access the TPR. */
                                  //| X86_CPUID_AMD_FEATURE_ECX_ABM
                                  //| X86_CPUID_AMD_FEATURE_ECX_SSE4A
                                  //| X86_CPUID_AMD_FEATURE_ECX_MISALNSSE
                                  //| X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF
                                  //| X86_CPUID_AMD_FEATURE_ECX_OSVW
                                  //| X86_CPUID_AMD_FEATURE_ECX_IBS
                                  //| X86_CPUID_AMD_FEATURE_ECX_SSE5
                                  //| X86_CPUID_AMD_FEATURE_ECX_SKINIT
                                  //| X86_CPUID_AMD_FEATURE_ECX_WDT
                                  | 0;
        if (pCPUM->u8PortableCpuIdLevel > 0)
        {
            PORTABLE_DISABLE_FEATURE_BIT(1, pExtFeatureLeaf->uEcx, CR8L,       X86_CPUID_AMD_FEATURE_ECX_CR8L);
            PORTABLE_DISABLE_FEATURE_BIT(1, pExtFeatureLeaf->uEdx, 3DNOW,      X86_CPUID_AMD_FEATURE_EDX_3DNOW);
            PORTABLE_DISABLE_FEATURE_BIT(1, pExtFeatureLeaf->uEdx, 3DNOW_EX,   X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
            PORTABLE_DISABLE_FEATURE_BIT(1, pExtFeatureLeaf->uEdx, FFXSR,      X86_CPUID_AMD_FEATURE_EDX_FFXSR);
            PORTABLE_DISABLE_FEATURE_BIT(1, pExtFeatureLeaf->uEdx, RDTSCP,     X86_CPUID_EXT_FEATURE_EDX_RDTSCP);
            PORTABLE_DISABLE_FEATURE_BIT(2, pExtFeatureLeaf->uEcx, LAHF_SAHF,  X86_CPUID_EXT_FEATURE_ECX_LAHF_SAHF);
            PORTABLE_DISABLE_FEATURE_BIT(3, pExtFeatureLeaf->uEcx, CMOV,       X86_CPUID_AMD_FEATURE_EDX_CMOV);

            Assert(!(pExtFeatureLeaf->uEcx & (  X86_CPUID_AMD_FEATURE_ECX_CMPL
                                              | X86_CPUID_AMD_FEATURE_ECX_SVM
                                              | X86_CPUID_AMD_FEATURE_ECX_EXT_APIC
                                              | X86_CPUID_AMD_FEATURE_ECX_CR8L
                                              | X86_CPUID_AMD_FEATURE_ECX_ABM
                                              | X86_CPUID_AMD_FEATURE_ECX_SSE4A
                                              | X86_CPUID_AMD_FEATURE_ECX_MISALNSSE
                                              | X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF
                                              | X86_CPUID_AMD_FEATURE_ECX_OSVW
                                              | X86_CPUID_AMD_FEATURE_ECX_IBS
                                              | X86_CPUID_AMD_FEATURE_ECX_SSE5
                                              | X86_CPUID_AMD_FEATURE_ECX_SKINIT
                                              | X86_CPUID_AMD_FEATURE_ECX_WDT
                                              | UINT32_C(0xffffc000)
                                              )));
            Assert(!(pExtFeatureLeaf->uEdx & (  RT_BIT(10)
                                              | X86_CPUID_EXT_FEATURE_EDX_SYSCALL
                                              | RT_BIT(18)
                                              | RT_BIT(19)
                                              | RT_BIT(21)
                                              | X86_CPUID_AMD_FEATURE_EDX_AXMMX
                                              | X86_CPUID_EXT_FEATURE_EDX_PAGE1GB
                                              | RT_BIT(28)
                                              )));
        }
    }

    /*
     * Hide HTT, multicode, SMP, whatever.
     * (APIC-ID := 0 and #LogCpus := 0)
     */
    pStdFeatureLeaf->uEbx &= 0x0000ffff;
#ifdef VBOX_WITH_MULTI_CORE
    if (pVM->cCpus > 1)
    {
        /* If CPUID Fn0000_0001_EDX[HTT] = 1 then LogicalProcessorCount is the number of threads per CPU core times the number of CPU cores per processor */
        pStdFeatureLeaf->uEbx |= (pVM->cCpus << 16);
        pStdFeatureLeaf->uEdx |= X86_CPUID_FEATURE_EDX_HTT;  /* necessary for hyper-threading *or* multi-core CPUs */
    }
#endif

    /* Cpuid 2:
     * Intel: Cache and TLB information
     * AMD:   Reserved
     * VIA:   Reserved
     * Safe to expose; restrict the number of calls to 1 for the portable case.
     */
    PCPUMCPUIDLEAF pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 2, 0);
    if (   pCPUM->u8PortableCpuIdLevel > 0
        && pCurLeaf
        && (pCurLeaf->uEax & 0xff) > 1)
    {
        LogRel(("PortableCpuId: Std[2].al: %d -> 1\n", pCurLeaf->uEax & 0xff));
        pCurLeaf->uEax &= UINT32_C(0xfffffffe);
    }

    /* Cpuid 3:
     * Intel: EAX, EBX - reserved (transmeta uses these)
     *        ECX, EDX - Processor Serial Number if available, otherwise reserved
     * AMD:   Reserved
     * VIA:   Reserved
     * Safe to expose
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 3, 0);
    pStdFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 1, 0);
    if (   !(pStdFeatureLeaf->uEdx & X86_CPUID_FEATURE_EDX_PSN)
        && pCurLeaf)
    {
        pCurLeaf->uEcx = pCurLeaf->uEdx = 0;
        if (pCPUM->u8PortableCpuIdLevel > 0)
            pCurLeaf->uEax = pCurLeaf->uEbx = 0;
    }

    /* Cpuid 4:
     * Intel: Deterministic Cache Parameters Leaf
     *        Note: Depends on the ECX input! -> Feeling rather lazy now, so we just return 0
     * AMD:   Reserved
     * VIA:   Reserved
     * Safe to expose, except for EAX:
     *      Bits 25-14: Maximum number of addressable IDs for logical processors sharing this cache (see note)**
     *      Bits 31-26: Maximum number of processor cores in this physical package**
     * Note: These SMP values are constant regardless of ECX
     */
    CPUMCPUIDLEAF NewLeaf;
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 4, 0);
    if (pCurLeaf)
    {
        NewLeaf.uLeaf        = 4;
        NewLeaf.uSubLeaf     = 0;
        NewLeaf.fSubLeafMask = 0;
        NewLeaf.uEax         = 0;
        NewLeaf.uEbx         = 0;
        NewLeaf.uEcx         = 0;
        NewLeaf.uEdx         = 0;
        NewLeaf.fFlags       = 0;
#ifdef VBOX_WITH_MULTI_CORE
        if (   pVM->cCpus > 1
            && pCPUM->GuestFeatures.enmCpuVendor == CPUMCPUVENDOR_INTEL)
        {
            AssertReturn(pVM->cCpus <= 64, VERR_TOO_MANY_CPUS);
            /* One logical processor with possibly multiple cores. */
            /* See  http://www.intel.com/Assets/PDF/appnote/241618.pdf p. 29 */
            NewLeaf.uEax |= ((pVM->cCpus - 1) << 26);   /* 6 bits only -> 64 cores! */
        }
#endif
        rc = cpumR3CpuIdInsert(NULL /* pVM */, &pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves, &NewLeaf);
        AssertLogRelRCReturn(rc, rc);
    }

    /* Cpuid 5:     Monitor/mwait Leaf
     * Intel: ECX, EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     * AMD:   EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     *        ECX - extensions (ignored for now)
     * VIA:   Reserved
     * Safe to expose
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 5, 0);
    if (pCurLeaf)
    {
        pStdFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 1, 0);
        if (!(pStdFeatureLeaf->uEcx & X86_CPUID_FEATURE_ECX_MONITOR))
            pCurLeaf->uEax = pCurLeaf->uEbx = 0;

        pCurLeaf->uEcx = pCurLeaf->uEdx = 0;
        if (fMWaitExtensions)
        {
            pCurLeaf->uEcx = X86_CPUID_MWAIT_ECX_EXT | X86_CPUID_MWAIT_ECX_BREAKIRQIF0;
            /** @todo: for now we just expose host's MWAIT C-states, although conceptually
               it shall be part of our power management virtualization model */
#if 0
            /* MWAIT sub C-states */
            pCurLeaf->uEdx =
                    (0 << 0)  /* 0 in C0 */ |
                    (2 << 4)  /* 2 in C1 */ |
                    (2 << 8)  /* 2 in C2 */ |
                    (2 << 12) /* 2 in C3 */ |
                    (0 << 16) /* 0 in C4 */
                    ;
#endif
        }
        else
            pCurLeaf->uEcx = pCurLeaf->uEdx = 0;
    }

    /* Cpuid 0x800000005 & 0x800000006 contain information about L1, L2 & L3 cache and TLB identifiers.
     * Safe to pass on to the guest.
     *
     * Intel: 0x800000005 reserved
     *        0x800000006 L2 cache information
     * AMD:   0x800000005 L1 cache information
     *        0x800000006 L2/L3 cache information
     * VIA:   0x800000005 TLB and L1 cache information
     *        0x800000006 L2 cache information
     */

    /* Cpuid 0x800000007:
     * Intel:             Reserved
     * AMD:               EAX, EBX, ECX - reserved
     *                    EDX: Advanced Power Management Information
     * VIA:               Reserved
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, UINT32_C(0x80000007), 0);
    if (pCurLeaf)
    {
        Assert(pCPUM->GuestFeatures.enmCpuVendor != CPUMCPUVENDOR_INVALID);

        pCurLeaf->uEax = pCurLeaf->uEbx = pCurLeaf->uEcx = 0;

        if (pCPUM->GuestFeatures.enmCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Only expose the TSC invariant capability bit to the guest. */
            pCurLeaf->uEdx                  &= 0
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TS
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_FID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_VID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TTP
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TM
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_STC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_MC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_HWPSTATE
#if 0
        /*
         * We don't expose X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR, because newer
         * Linux kernels blindly assume that the AMD performance counters work
         * if this is set for 64 bits guests. (Can't really find a CPUID feature
         * bit for them though.)
         */
                                            | X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR
#endif
                                            | 0;
        }
        else
            pCurLeaf->uEdx = 0;
    }

    /* Cpuid 0x800000008:
     * Intel:             EAX: Virtual/Physical address Size
     *                    EBX, ECX, EDX - reserved
     * AMD:               EBX, EDX - reserved
     *                    EAX: Virtual/Physical/Guest address Size
     *                    ECX: Number of cores + APICIdCoreIdSize
     * VIA:               EAX: Virtual/Physical address Size
     *                    EBX, ECX, EDX - reserved
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, UINT32_C(0x80000008), 0);
    if (pCurLeaf)
    {
        /* Only expose the virtual and physical address sizes to the guest. */
        pCurLeaf->uEax &= UINT32_C(0x0000ffff);
        pCurLeaf->uEbx = pCurLeaf->uEdx = 0;  /* reserved */
        /* Set APICIdCoreIdSize to zero (use legacy method to determine the number of cores per cpu)
         * NC (0-7) Number of cores; 0 equals 1 core */
        pCurLeaf->uEcx = 0;
#ifdef VBOX_WITH_MULTI_CORE
        if (    pVM->cCpus > 1
            &&  pCPUM->GuestFeatures.enmCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Legacy method to determine the number of cores. */
            pCurLeaf->uEcx |= (pVM->cCpus - 1); /* NC: Number of CPU cores - 1; 8 bits */
            pExtFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves,
                                                 UINT32_C(0x80000001), 0);
            if (pExtFeatureLeaf)
                pExtFeatureLeaf->uEcx |= X86_CPUID_AMD_FEATURE_ECX_CMPL;
        }
#endif
    }


    /*
     * Limit it the number of entries, zapping the remainder.
     *
     * The limits are masking off stuff about power saving and similar, this
     * is perhaps a bit crudely done as there is probably some relatively harmless
     * info too in these leaves (like words about having a constant TSC).
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 0, 0);
    if (pCurLeaf)
    {
        if (pCurLeaf->uEax > 5)
        {
            pCurLeaf->uEax = 5;
            cpumR3CpuIdRemoveRange(pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves,
                                   pCurLeaf->uEax + 1, UINT32_C(0x000fffff));
        }

        /* NT4 hack, no zapping of extra leaves here. */
        if (fNt4LeafLimit && pCurLeaf->uEax > 3)
            pCurLeaf->uEax = 3;
    }

    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, UINT32_C(0x80000000), 0);
    if (pCurLeaf)
    {
        if (pCurLeaf->uEax > UINT32_C(0x80000008))
        {
            pCurLeaf->uEax = UINT32_C(0x80000008);
            cpumR3CpuIdRemoveRange(pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves,
                                   pCurLeaf->uEax + 1, UINT32_C(0x800fffff));
        }
    }

    /*
     * Centaur stuff (VIA).
     *
     * The important part here (we think) is to make sure the 0xc0000000
     * function returns 0xc0000001. As for the features, we don't currently
     * let on about any of those... 0xc0000002 seems to be some
     * temperature/hz/++ stuff, include it as well (static).
     */
    pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, UINT32_C(0xc0000000), 0);
    if (pCurLeaf)
    {
        if (   pCurLeaf->uEax >= UINT32_C(0xc0000000)
            && pCurLeaf->uEax <= UINT32_C(0xc0000004))
        {
            pCurLeaf->uEax = RT_MIN(pCurLeaf->uEax, UINT32_C(0xc0000002));
            cpumR3CpuIdRemoveRange(pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves,
                                   UINT32_C(0xc0000002), UINT32_C(0xc00fffff));

            pCurLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves,
                                          UINT32_C(0xc0000001), 0);
            if (pCurLeaf)
                pCurLeaf->uEdx = 0; /* all features hidden */
        }
        else
            cpumR3CpuIdRemoveRange(pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves,
                                   UINT32_C(0xc0000000), UINT32_C(0xc00fffff));
    }

    /*
     * Hypervisor identification.
     *
     * We only return minimal information, primarily ensuring that the
     * 0x40000000 function returns 0x40000001 and identifying ourselves.
     * Hypervisor-specific interface is supported through GIM which will
     * modify these leaves if required depending on the GIM provider.
     */
    NewLeaf.uLeaf        = UINT32_C(0x40000000);
    NewLeaf.uSubLeaf     = 0;
    NewLeaf.fSubLeafMask = 0;
    NewLeaf.uEax         = UINT32_C(0x40000001);
    NewLeaf.uEbx         = 0x786f4256 /* 'VBox' */;
    NewLeaf.uEcx         = 0x786f4256 /* 'VBox' */;
    NewLeaf.uEdx         = 0x786f4256 /* 'VBox' */;
    NewLeaf.fFlags       = 0;
    rc = cpumR3CpuIdInsert(NULL /* pVM */, &pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves, &NewLeaf);
    AssertLogRelRCReturn(rc, rc);

    NewLeaf.uLeaf        = UINT32_C(0x40000001);
    NewLeaf.uEax         = 0x656e6f6e;                            /* 'none' */
    NewLeaf.uEbx         = 0;
    NewLeaf.uEcx         = 0;
    NewLeaf.uEdx         = 0;
    NewLeaf.fFlags       = 0;
    rc = cpumR3CpuIdInsert(NULL /* pVM */, &pCPUM->GuestInfo.paCpuIdLeavesR3, &pCPUM->GuestInfo.cCpuIdLeaves, &NewLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Mini CPU selection support for making Mac OS X happy.
     */
    if (pCPUM->GuestFeatures.enmCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        pStdFeatureLeaf = cpumR3CpuIdGetLeaf(pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves, 1, 0);
        uint32_t uCurIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(ASMGetCpuStepping(pStdFeatureLeaf->uEax),
                                                                ASMGetCpuModelIntel(pStdFeatureLeaf->uEax),
                                                                ASMGetCpuFamily(pStdFeatureLeaf->uEax),
                                                                0);
        if (uMaxIntelFamilyModelStep < uCurIntelFamilyModelStep)
        {
            uint32_t uNew = pStdFeatureLeaf->uEax & UINT32_C(0xf0003000);
            uNew |= RT_BYTE1(uMaxIntelFamilyModelStep) & 0xf; /* stepping */
            uNew |= (RT_BYTE2(uMaxIntelFamilyModelStep) & 0xf) << 4; /* 4 low model bits */
            uNew |= (RT_BYTE2(uMaxIntelFamilyModelStep) >> 4) << 16; /* 4 high model bits */
            uNew |= (RT_BYTE3(uMaxIntelFamilyModelStep) & 0xf) << 8; /* 4 low family bits */
            if (RT_BYTE3(uMaxIntelFamilyModelStep) > 0xf) /* 8 high family bits, using intel's suggested calculation. */
                uNew |= ( (RT_BYTE3(uMaxIntelFamilyModelStep) - (RT_BYTE3(uMaxIntelFamilyModelStep) & 0xf)) & 0xff ) << 20;
            LogRel(("CPU: CPUID(0).EAX %#x -> %#x (uMaxIntelFamilyModelStep=%#x, uCurIntelFamilyModelStep=%#x\n",
                    pStdFeatureLeaf->uEax, uNew, uMaxIntelFamilyModelStep, uCurIntelFamilyModelStep));
            pStdFeatureLeaf->uEax = uNew;
        }
    }

    /*
     * MSR fudging.
     */
    /** @cfgm{/CPUM/FudgeMSRs, boolean, true}
     * Fudges some common MSRs if not present in the selected CPU database entry.
     * This is for trying to keep VMs running when moved between different hosts
     * and different CPU vendors. */
    bool fEnable;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "FudgeMSRs", &fEnable, true);       AssertRCReturn(rc, rc);
    if (fEnable)
    {
        rc = cpumR3MsrApplyFudge(pVM);
        AssertLogRelRCReturn(rc, rc);
    }

    /*
     * Move the MSR and CPUID arrays over on the hypervisor heap, and explode
     * guest CPU features again.
     */
    void *pvFree = pCPUM->GuestInfo.paCpuIdLeavesR3;
    int rc1 = cpumR3CpuIdInstallAndExplodeLeaves(pVM, pCPUM, pCPUM->GuestInfo.paCpuIdLeavesR3, pCPUM->GuestInfo.cCpuIdLeaves);
    RTMemFree(pvFree);

    pvFree = pCPUM->GuestInfo.paMsrRangesR3;
    int rc2 = MMHyperDupMem(pVM, pvFree,
                            sizeof(pCPUM->GuestInfo.paMsrRangesR3[0]) * pCPUM->GuestInfo.cMsrRanges, 32,
                            MM_TAG_CPUM_MSRS, (void **)&pCPUM->GuestInfo.paMsrRangesR3);
    RTMemFree(pvFree);
    AssertLogRelRCReturn(rc1, rc1);
    AssertLogRelRCReturn(rc2, rc2);

    pCPUM->GuestInfo.paMsrRangesR0 = MMHyperR3ToR0(pVM, pCPUM->GuestInfo.paMsrRangesR3);
    pCPUM->GuestInfo.paMsrRangesRC = MMHyperR3ToRC(pVM, pCPUM->GuestInfo.paMsrRangesR3);
    cpumR3MsrRegStats(pVM);

    /*
     * Some more configuration that we're applying at the end of everything
     * via the CPUMSetGuestCpuIdFeature API.
     */

    /* Check if PAE was explicitely enabled by the user. */
    rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "EnablePAE", &fEnable, false);      AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);

    /* We don't normally enable NX for raw-mode, so give the user a chance to force it on. */
    rc = CFGMR3QueryBoolDef(pCpumCfg, "EnableNX", &fEnable, false);                 AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);

    /* We don't enable the Hypervisor Present bit by default, but it may be needed by some guests. */
    rc = CFGMR3QueryBoolDef(pCpumCfg, "EnableHVP", &fEnable, false);                AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

#undef PORTABLE_DISABLE_FEATURE_BIT
#undef PORTABLE_CLEAR_BITS_WHEN

    return VINF_SUCCESS;
}



/*
 *
 *
 * Saved state related code.
 * Saved state related code.
 * Saved state related code.
 *
 *
 */

/**
 * Called both in pass 0 and the final pass.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   pSSM                The saved state handle.
 */
void cpumR3SaveCpuId(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save all the CPU ID leaves here so we can check them for compatibility
     * upon loading.
     */
    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3PutMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Save a good portion of the raw CPU IDs as well as they may come in
     * handy when validating features for raw mode.
     */
    CPUMCPUID   aRawStd[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuIdExSlow(i, 0, 0, 0, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawStd));
    SSMR3PutMem(pSSM, &aRawStd[0], sizeof(aRawStd));

    CPUMCPUID   aRawExt[32];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuIdExSlow(i | UINT32_C(0x80000000), 0, 0, 0, &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawExt));
    SSMR3PutMem(pSSM, &aRawExt[0], sizeof(aRawExt));
}


static int cpumR3LoadCpuIdOneGuestArray(PSSMHANDLE pSSM, uint32_t uBase, PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves)
{
    uint32_t cCpuIds;
    int rc = SSMR3GetU32(pSSM, &cCpuIds);
    if (RT_SUCCESS(rc))
    {
        if (cCpuIds < 64)
        {
            for (uint32_t i = 0; i < cCpuIds; i++)
            {
                CPUMCPUID CpuId;
                rc = SSMR3GetMem(pSSM, &CpuId, sizeof(CpuId));
                if (RT_FAILURE(rc))
                    break;

                CPUMCPUIDLEAF NewLeaf;
                NewLeaf.uLeaf           = uBase + i;
                NewLeaf.uSubLeaf        = 0;
                NewLeaf.fSubLeafMask    = 0;
                NewLeaf.uEax            = CpuId.eax;
                NewLeaf.uEbx            = CpuId.ebx;
                NewLeaf.uEcx            = CpuId.ecx;
                NewLeaf.uEdx            = CpuId.edx;
                NewLeaf.fFlags          = 0;
                rc = cpumR3CpuIdInsert(NULL /* pVM */, ppaLeaves, pcLeaves, &NewLeaf);
            }
        }
        else
            rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    if (RT_FAILURE(rc))
    {
        RTMemFree(*ppaLeaves);
        *ppaLeaves = NULL;
        *pcLeaves = 0;
    }
    return rc;
}


static int cpumR3LoadCpuIdGuestArrays(PSSMHANDLE pSSM, uint32_t uVersion, PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves)
{
    *ppaLeaves = NULL;
    *pcLeaves = 0;

    int rc = cpumR3LoadCpuIdOneGuestArray(pSSM, UINT32_C(0x00000000), ppaLeaves, pcLeaves);
    if (RT_SUCCESS(rc))
        rc = cpumR3LoadCpuIdOneGuestArray(pSSM, UINT32_C(0x80000000), ppaLeaves, pcLeaves);
    if (RT_SUCCESS(rc))
        rc = cpumR3LoadCpuIdOneGuestArray(pSSM, UINT32_C(0xc0000000), ppaLeaves, pcLeaves);

    return rc;
}


/**
 * Loads the CPU ID leaves saved by pass 0.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM.
 * @param   pSSM                The saved state handle.
 * @param   uVersion            The format version.
 */
int cpumR3LoadCpuId(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion)
{
    AssertMsgReturn(uVersion >= CPUM_SAVED_STATE_VERSION_VER3_2, ("%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    /*
     * Define a bunch of macros for simplifying the code.
     */
    /* Generic expression + failure message. */
#define CPUID_CHECK_RET(expr, fmt) \
    do { \
        if (!(expr)) \
        { \
            char *pszMsg = RTStrAPrintf2 fmt; /* lack of variadic macros sucks */ \
            if (fStrictCpuIdChecks) \
            { \
                int rcCpuid = SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, "%s", pszMsg); \
                RTStrFree(pszMsg); \
                return rcCpuid; \
            } \
            LogRel(("CPUM: %s\n", pszMsg)); \
            RTStrFree(pszMsg); \
        } \
    } while (0)
#define CPUID_CHECK_WRN(expr, fmt) \
    do { \
        if (!(expr)) \
            LogRel(fmt); \
    } while (0)

    /* For comparing two values and bitch if they differs. */
#define CPUID_CHECK2_RET(what, host, saved) \
    do { \
        if ((host) != (saved)) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#what " mismatch: host=%#x saved=%#x"), (host), (saved)); \
            LogRel(("CPUM: " #what " differs: host=%#x saved=%#x\n", (host), (saved))); \
        } \
    } while (0)
#define CPUID_CHECK2_WRN(what, host, saved) \
    do { \
        if ((host) != (saved)) \
            LogRel(("CPUM: " #what " differs: host=%#x saved=%#x\n", (host), (saved))); \
    } while (0)

    /* For checking raw cpu features (raw mode). */
#define CPUID_RAW_FEATURE_RET(set, reg, bit) \
    do { \
        if ((aHostRaw##set [1].reg & bit) != (aRaw##set [1].reg & bit)) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " mismatch: host=%d saved=%d"), \
                                         !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) ); \
            LogRel(("CPUM: " #bit" differs: host=%d saved=%d\n", \
                    !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) )); \
        } \
    } while (0)
#define CPUID_RAW_FEATURE_WRN(set, reg, bit) \
    do { \
        if ((aHostRaw##set [1].reg & bit) != (aRaw##set [1].reg & bit)) \
            LogRel(("CPUM: " #bit" differs: host=%d saved=%d\n", \
                    !!(aHostRaw##set [1].reg & (bit)), !!(aRaw##set [1].reg & (bit)) )); \
    } while (0)
#define CPUID_RAW_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking guest features. */
#define CPUID_GST_FEATURE_RET(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_FEATURE_WRN(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_FEATURE_EMU(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            && !(aHostRaw##set [1].reg & bit) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: Warning - " #bit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking guest features if AMD guest CPU. */
#define CPUID_GST_AMD_FEATURE_RET(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#bit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_AMD_FEATURE_WRN(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: " #bit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_AMD_FEATURE_EMU(set, reg, bit) \
    do { \
        if (    (aGuestCpuId##set [1].reg & bit) \
            &&  fGuestAmd \
            && (!fGuestAmd || !(aHostRaw##set [1].reg & bit)) \
            && !(aHostOverride##set [1].reg & bit) \
           ) \
            LogRel(("CPUM: Warning - " #bit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_AMD_FEATURE_IGN(set, reg, bit) do { } while (0)

    /* For checking AMD features which have a corresponding bit in the standard
       range.  (Intel defines very few bits in the extended feature sets.) */
#define CPUID_GST_FEATURE2_RET(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
           ) \
        { \
            if (fStrictCpuIdChecks) \
                return SSMR3SetLoadError(pSSM, VERR_SSM_LOAD_CPUID_MISMATCH, RT_SRC_POS, \
                                         N_(#ExtBit " is not supported by the host but has already exposed to the guest")); \
            LogRel(("CPUM: " #ExtBit " is not supported by the host but has already exposed to the guest\n")); \
        } \
    } while (0)
#define CPUID_GST_FEATURE2_WRN(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
           ) \
            LogRel(("CPUM: " #ExtBit " is not supported by the host but has already exposed to the guest\n")); \
    } while (0)
#define CPUID_GST_FEATURE2_EMU(reg, ExtBit, StdBit) \
    do { \
        if (    (aGuestCpuIdExt [1].reg    & (ExtBit)) \
            && !(fHostAmd  \
                 ? aHostRawExt[1].reg      & (ExtBit) \
                 : aHostRawStd[1].reg      & (StdBit)) \
            && !(aHostOverrideExt[1].reg   & (ExtBit)) \
           ) \
            LogRel(("CPUM: Warning - " #ExtBit " is not supported by the host but already exposed to the guest. This may impact performance.\n")); \
    } while (0)
#define CPUID_GST_FEATURE2_IGN(reg, ExtBit, StdBit) do { } while (0)

    /*
     * Load them into stack buffers first.
     */
    PCPUMCPUIDLEAF paLeaves;
    uint32_t       cLeaves;
    int rc = cpumR3LoadCpuIdGuestArrays(pSSM, uVersion, &paLeaves, &cLeaves);
    AssertRCReturn(rc, rc);

    /** @todo we'll be leaking paLeaves on error return... */

    CPUMCPUID   GuestCpuIdDef;
    rc = SSMR3GetMem(pSSM, &GuestCpuIdDef, sizeof(GuestCpuIdDef));
    AssertRCReturn(rc, rc);

    CPUMCPUID   aRawStd[16];
    uint32_t    cRawStd;
    rc = SSMR3GetU32(pSSM, &cRawStd); AssertRCReturn(rc, rc);
    if (cRawStd > RT_ELEMENTS(aRawStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    rc = SSMR3GetMem(pSSM, &aRawStd[0], cRawStd * sizeof(aRawStd[0]));
    AssertRCReturn(rc, rc);
    for (uint32_t i = cRawStd; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuIdExSlow(i, 0, 0, 0, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);

    CPUMCPUID   aRawExt[32];
    uint32_t    cRawExt;
    rc = SSMR3GetU32(pSSM, &cRawExt); AssertRCReturn(rc, rc);
    if (cRawExt > RT_ELEMENTS(aRawExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    rc = SSMR3GetMem(pSSM, &aRawExt[0], cRawExt * sizeof(aRawExt[0]));
    AssertRCReturn(rc, rc);
    for (uint32_t i = cRawExt; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuIdExSlow(i | UINT32_C(0x80000000), 0, 0, 0, &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);

    /*
     * Get the raw CPU IDs for the current host.
     */
    CPUMCPUID   aHostRawStd[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawStd); i++)
        ASMCpuIdExSlow(i, 0, 0, 0, &aHostRawStd[i].eax, &aHostRawStd[i].ebx, &aHostRawStd[i].ecx, &aHostRawStd[i].edx);

    CPUMCPUID   aHostRawExt[32];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawExt); i++)
        ASMCpuIdExSlow(i | UINT32_C(0x80000000), 0, 0, 0,
                       &aHostRawExt[i].eax, &aHostRawExt[i].ebx, &aHostRawExt[i].ecx, &aHostRawExt[i].edx);

    /*
     * Get the host and guest overrides so we don't reject the state because
     * some feature was enabled thru these interfaces.
     * Note! We currently only need the feature leaves, so skip rest.
     */
    PCFGMNODE   pOverrideCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM/HostCPUID");
    CPUMCPUID   aHostOverrideStd[2];
    memcpy(&aHostOverrideStd[0], &aHostRawStd[0], sizeof(aHostOverrideStd));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x00000000), &aHostOverrideStd[0], RT_ELEMENTS(aHostOverrideStd), pOverrideCfg);

    CPUMCPUID   aHostOverrideExt[2];
    memcpy(&aHostOverrideExt[0], &aHostRawExt[0], sizeof(aHostOverrideExt));
    cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x80000000), &aHostOverrideExt[0], RT_ELEMENTS(aHostOverrideExt), pOverrideCfg);

    /*
     * This can be skipped.
     */
    bool fStrictCpuIdChecks;
    CFGMR3QueryBoolDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM"), "StrictCpuIdChecks", &fStrictCpuIdChecks, true);



    /*
     * For raw-mode we'll require that the CPUs are very similar since we don't
     * intercept CPUID instructions for user mode applications.
     */
    if (!HMIsEnabled(pVM))
    {
        /* CPUID(0) */
        CPUID_CHECK_RET(   aHostRawStd[0].ebx == aRawStd[0].ebx
                        && aHostRawStd[0].ecx == aRawStd[0].ecx
                        && aHostRawStd[0].edx == aRawStd[0].edx,
                        (N_("CPU vendor mismatch: host='%.4s%.4s%.4s' saved='%.4s%.4s%.4s'"),
                         &aHostRawStd[0].ebx, &aHostRawStd[0].edx, &aHostRawStd[0].ecx,
                         &aRawStd[0].ebx, &aRawStd[0].edx, &aRawStd[0].ecx));
        CPUID_CHECK2_WRN("Std CPUID max leaf",   aHostRawStd[0].eax, aRawStd[0].eax);
        CPUID_CHECK2_WRN("Reserved bits 15:14", (aHostRawExt[1].eax >> 14) & 3, (aRawExt[1].eax >> 14) & 3);
        CPUID_CHECK2_WRN("Reserved bits 31:28",  aHostRawExt[1].eax >> 28,       aRawExt[1].eax >> 28);

        bool const fIntel = ASMIsIntelCpuEx(aRawStd[0].ebx, aRawStd[0].ecx, aRawStd[0].edx);

        /* CPUID(1).eax */
        CPUID_CHECK2_RET("CPU family",          ASMGetCpuFamily(aHostRawStd[1].eax),        ASMGetCpuFamily(aRawStd[1].eax));
        CPUID_CHECK2_RET("CPU model",           ASMGetCpuModel(aHostRawStd[1].eax, fIntel), ASMGetCpuModel(aRawStd[1].eax, fIntel));
        CPUID_CHECK2_WRN("CPU type",            (aHostRawStd[1].eax >> 12) & 3,             (aRawStd[1].eax >> 12) & 3 );

        /* CPUID(1).ebx - completely ignore CPU count and APIC ID. */
        CPUID_CHECK2_RET("CPU brand ID",         aHostRawStd[1].ebx & 0xff,                 aRawStd[1].ebx & 0xff);
        CPUID_CHECK2_WRN("CLFLUSH chunk count", (aHostRawStd[1].ebx >> 8) & 0xff,           (aRawStd[1].ebx >> 8) & 0xff);

        /* CPUID(1).ecx */
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE3);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PCLMUL);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_DTES64);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MONITOR);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CPLDS);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_VMX);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_SMX);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_EST);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_TM2);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSSE3);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_CNTXID);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(11) /*reserved*/ );
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_FMA);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CX16);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_TPRUPDATE);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_PDCM);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(16) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(17) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_DCA);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_1);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_2);
        CPUID_RAW_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_X2APIC);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MOVBE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_POPCNT);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(24) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AES);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_XSAVE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_OSXSAVE);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AVX);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(29) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, RT_BIT_32(30) /*reserved*/);
        CPUID_RAW_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_HVP);

        /* CPUID(1).edx */
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FPU);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_VME);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TSC);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MSR);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CX8);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_APIC);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(10) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SEP);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MTRR);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PGE);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCA);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CMOV);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAT);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE36);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSN);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CLFSH);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(20) /*reserved*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_DS);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_ACPI);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MMX);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FXSR);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE);
        CPUID_RAW_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE2);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SS);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_HTT);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_TM);
        CPUID_RAW_FEATURE_RET(Std, edx, RT_BIT_32(30) /*JMPE/IA64*/);
        CPUID_RAW_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PBE);

        /* CPUID(2) - config, mostly about caches. ignore. */
        /* CPUID(3) - processor serial number. ignore. */
        /* CPUID(4) - config, cache and topology - takes ECX as input. ignore. */
        /* CPUID(5) - mwait/monitor config. ignore. */
        /* CPUID(6) - power management. ignore. */
        /* CPUID(7) - ???. ignore. */
        /* CPUID(8) - ???. ignore. */
        /* CPUID(9) - DCA. ignore for now. */
        /* CPUID(a) - PeMo info. ignore for now. */
        /* CPUID(b) - topology info - takes ECX as input. ignore. */

        /* CPUID(d) - XCR0 stuff - takes ECX as input. We only warn about the main level (ECX=0) for now. */
        CPUID_CHECK_WRN(   aRawStd[0].eax     <  UINT32_C(0x0000000d)
                        || aHostRawStd[0].eax >= UINT32_C(0x0000000d),
                        ("CPUM: Standard leaf D was present on saved state host, not present on current.\n"));
        if (   aRawStd[0].eax     >= UINT32_C(0x0000000d)
            && aHostRawStd[0].eax >= UINT32_C(0x0000000d))
        {
            CPUID_CHECK2_WRN("Valid low XCR0 bits",             aHostRawStd[0xd].eax, aRawStd[0xd].eax);
            CPUID_CHECK2_WRN("Valid high XCR0 bits",            aHostRawStd[0xd].edx, aRawStd[0xd].edx);
            CPUID_CHECK2_WRN("Current XSAVE/XRSTOR area size",  aHostRawStd[0xd].ebx, aRawStd[0xd].ebx);
            CPUID_CHECK2_WRN("Max XSAVE/XRSTOR area size",      aHostRawStd[0xd].ecx, aRawStd[0xd].ecx);
        }

        /* CPUID(0x80000000) - same as CPUID(0) except for eax.
           Note! Intel have/is marking many of the fields here as reserved. We
                 will verify them as if it's an AMD CPU. */
        CPUID_CHECK_RET(   (aHostRawExt[0].eax >= UINT32_C(0x80000001) && aHostRawExt[0].eax <= UINT32_C(0x8000007f))
                        || !(aRawExt[0].eax    >= UINT32_C(0x80000001) && aRawExt[0].eax     <= UINT32_C(0x8000007f)),
                        (N_("Extended leaves was present on saved state host, but is missing on the current\n")));
        if (aRawExt[0].eax >= UINT32_C(0x80000001) && aRawExt[0].eax     <= UINT32_C(0x8000007f))
        {
            CPUID_CHECK_RET(   aHostRawExt[0].ebx == aRawExt[0].ebx
                            && aHostRawExt[0].ecx == aRawExt[0].ecx
                            && aHostRawExt[0].edx == aRawExt[0].edx,
                            (N_("CPU vendor mismatch: host='%.4s%.4s%.4s' saved='%.4s%.4s%.4s'"),
                             &aHostRawExt[0].ebx, &aHostRawExt[0].edx, &aHostRawExt[0].ecx,
                             &aRawExt[0].ebx,     &aRawExt[0].edx,     &aRawExt[0].ecx));
            CPUID_CHECK2_WRN("Ext CPUID max leaf",   aHostRawExt[0].eax, aRawExt[0].eax);

            /* CPUID(0x80000001).eax - same as CPUID(0).eax. */
            CPUID_CHECK2_RET("CPU family",          ASMGetCpuFamily(aHostRawExt[1].eax),        ASMGetCpuFamily(aRawExt[1].eax));
            CPUID_CHECK2_RET("CPU model",           ASMGetCpuModel(aHostRawExt[1].eax, fIntel), ASMGetCpuModel(aRawExt[1].eax, fIntel));
            CPUID_CHECK2_WRN("CPU type",            (aHostRawExt[1].eax >> 12) & 3, (aRawExt[1].eax >> 12) & 3 );
            CPUID_CHECK2_WRN("Reserved bits 15:14", (aHostRawExt[1].eax >> 14) & 3, (aRawExt[1].eax >> 14) & 3 );
            CPUID_CHECK2_WRN("Reserved bits 31:28",  aHostRawExt[1].eax >> 28, aRawExt[1].eax >> 28);

            /* CPUID(0x80000001).ebx - Brand ID (maybe), just warn if things differs. */
            CPUID_CHECK2_WRN("CPU BrandID",          aHostRawExt[1].ebx & 0xffff, aRawExt[1].ebx & 0xffff);
            CPUID_CHECK2_WRN("Reserved bits 16:27", (aHostRawExt[1].ebx >> 16) & 0xfff, (aRawExt[1].ebx >> 16) & 0xfff);
            CPUID_CHECK2_WRN("PkgType",             (aHostRawExt[1].ebx >> 28) &   0xf, (aRawExt[1].ebx >> 28) &   0xf);

            /* CPUID(0x80000001).ecx */
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_EXT_FEATURE_ECX_LAHF_SAHF);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CMPL);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SVM);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_EXT_APIC);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CR8L);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_ABM);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE4A);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_OSVW);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_IBS);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE5);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SKINIT);
            CPUID_RAW_FEATURE_IGN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_WDT);
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(14));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(15));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(16));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(17));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(18));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(19));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(20));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(21));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(22));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(23));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(24));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(25));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(26));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(27));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(28));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(29));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(30));
            CPUID_RAW_FEATURE_WRN(Ext, ecx, RT_BIT_32(31));

            /* CPUID(0x80000001).edx */
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FPU);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_VME);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_DE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PSE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_TSC);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MSR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAE);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MCE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_CX8);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_APIC);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(10) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_SEP);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MTRR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PGE);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MCA);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_CMOV);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PAT);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_PSE36);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(18) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(19) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_NX);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(21) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_AXMMX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_MMX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FXSR);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FFXSR);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_PAGE1GB);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_RDTSCP);
            CPUID_RAW_FEATURE_IGN(Ext, edx, RT_BIT_32(28) /*reserved*/);
            CPUID_RAW_FEATURE_IGN(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_LONG_MODE);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
            CPUID_RAW_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW);

            /** @todo verify the rest as well. */
        }
    }



    /*
     * Verify that we can support the features already exposed to the guest on
     * this host.
     *
     * Most of the features we're emulating requires intercepting instruction
     * and doing it the slow way, so there is no need to warn when they aren't
     * present in the host CPU.  Thus we use IGN instead of EMU on these.
     *
     * Trailing comments:
     *      "EMU"  - Possible to emulate, could be lots of work and very slow.
     *      "EMU?" - Can this be emulated?
     */
    CPUMCPUID aGuestCpuIdStd[2];
    RT_ZERO(aGuestCpuIdStd);
    cpumR3CpuIdGetLeafLegacy(paLeaves, cLeaves, 1, 0, &aGuestCpuIdStd[1]);

    /* CPUID(1).ecx */
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE3);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PCLMUL);  // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_DTES64);  // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_MONITOR);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CPLDS);   // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_VMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_EST);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_TM2);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSSE3);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CNTXID);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(11) /*reserved*/ );
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_FMA);     // -> EMU? what's this?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_CX16);    // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_TPRUPDATE);//-> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_PDCM);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(16) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(17) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_DCA);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_1);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_SSE4_2);  // -> EMU
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_X2APIC);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_MOVBE);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_POPCNT);  // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(24) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AES);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_XSAVE);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_OSXSAVE); // -> EMU
    CPUID_GST_FEATURE_RET(Std, ecx, X86_CPUID_FEATURE_ECX_AVX);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(29) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, ecx, RT_BIT_32(30) /*reserved*/);
    CPUID_GST_FEATURE_IGN(Std, ecx, X86_CPUID_FEATURE_ECX_HVP);     // Normally not set by host

    /* CPUID(1).edx */
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FPU);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_VME);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DE);      // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TSC);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MSR);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_PAE);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCE);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CX8);     // -> EMU?
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_APIC);
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(10) /*reserved*/);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_SEP);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MTRR);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PGE);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_MCA);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CMOV);    // -> EMU
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PAT);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSE36);
    CPUID_GST_FEATURE_IGN(Std, edx, X86_CPUID_FEATURE_EDX_PSN);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_CLFSH);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(20) /*reserved*/);
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_DS);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_ACPI);    // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_MMX);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_FXSR);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE);     // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SSE2);    // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_SS);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_HTT);     // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_TM);      // -> EMU?
    CPUID_GST_FEATURE_RET(Std, edx, RT_BIT_32(30) /*JMPE/IA64*/);   // -> EMU
    CPUID_GST_FEATURE_RET(Std, edx, X86_CPUID_FEATURE_EDX_PBE);     // -> EMU?

    /* CPUID(0x80000000). */
    CPUMCPUID aGuestCpuIdExt[2];
    RT_ZERO(aGuestCpuIdExt);
    if (cpumR3CpuIdGetLeafLegacy(paLeaves, cLeaves, UINT32_C(0x80000001), 0, &aGuestCpuIdExt[1]))
    {
        /** @todo deal with no 0x80000001 on the host. */
        bool const fHostAmd  = ASMIsAmdCpuEx(aHostRawStd[0].ebx, aHostRawStd[0].ecx, aHostRawStd[0].edx);
        bool const fGuestAmd = ASMIsAmdCpuEx(aGuestCpuIdExt[0].ebx, aGuestCpuIdExt[0].ecx, aGuestCpuIdExt[0].edx);

        /* CPUID(0x80000001).ecx */
        CPUID_GST_FEATURE_WRN(Ext, ecx, X86_CPUID_EXT_FEATURE_ECX_LAHF_SAHF);   // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CMPL);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SVM);     // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_EXT_APIC);// ???
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_CR8L);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_ABM);     // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE4A);   // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);//-> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF);// -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_OSVW);    // -> EMU?
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_IBS);     // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SSE5);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_SKINIT);  // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, ecx, X86_CPUID_AMD_FEATURE_ECX_WDT);     // -> EMU
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(14));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(15));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(16));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(17));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(18));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(19));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(20));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(21));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(22));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(23));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(24));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(25));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(26));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(27));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(28));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(29));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(30));
        CPUID_GST_AMD_FEATURE_WRN(Ext, ecx, RT_BIT_32(31));

        /* CPUID(0x80000001).edx */
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_FPU,   X86_CPUID_FEATURE_EDX_FPU);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_VME,   X86_CPUID_FEATURE_EDX_VME);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_DE,    X86_CPUID_FEATURE_EDX_DE);      // -> EMU
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PSE,   X86_CPUID_FEATURE_EDX_PSE);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_TSC,   X86_CPUID_FEATURE_EDX_TSC);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_MSR,   X86_CPUID_FEATURE_EDX_MSR);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_PAE,   X86_CPUID_FEATURE_EDX_PAE);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MCE,   X86_CPUID_FEATURE_EDX_MCE);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_CX8,   X86_CPUID_FEATURE_EDX_CX8);     // -> EMU?
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_APIC,  X86_CPUID_FEATURE_EDX_APIC);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(10) /*reserved*/);
        CPUID_GST_FEATURE_IGN(    Ext, edx, X86_CPUID_EXT_FEATURE_EDX_SYSCALL);                              // On Intel: long mode only.
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MTRR,  X86_CPUID_FEATURE_EDX_MTRR);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PGE,   X86_CPUID_FEATURE_EDX_PGE);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_MCA,   X86_CPUID_FEATURE_EDX_MCA);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_CMOV,  X86_CPUID_FEATURE_EDX_CMOV);    // -> EMU
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PAT,   X86_CPUID_FEATURE_EDX_PAT);
        CPUID_GST_FEATURE2_IGN(        edx, X86_CPUID_AMD_FEATURE_EDX_PSE36, X86_CPUID_FEATURE_EDX_PSE36);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(18) /*reserved*/);
        CPUID_GST_AMD_FEATURE_WRN(Ext, edx, RT_BIT_32(19) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_EXT_FEATURE_EDX_NX);
        CPUID_GST_FEATURE_WRN(    Ext, edx, RT_BIT_32(21) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_AMD_FEATURE_EDX_AXMMX);
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_MMX,   X86_CPUID_FEATURE_EDX_MMX);     // -> EMU
        CPUID_GST_FEATURE2_RET(        edx, X86_CPUID_AMD_FEATURE_EDX_FXSR,  X86_CPUID_FEATURE_EDX_FXSR);    // -> EMU
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_FFXSR);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_PAGE1GB);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_EXT_FEATURE_EDX_RDTSCP);
        CPUID_GST_FEATURE_IGN(    Ext, edx, RT_BIT_32(28) /*reserved*/);
        CPUID_GST_FEATURE_RET(    Ext, edx, X86_CPUID_EXT_FEATURE_EDX_LONG_MODE);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX);
        CPUID_GST_AMD_FEATURE_RET(Ext, edx, X86_CPUID_AMD_FEATURE_EDX_3DNOW);
    }

    /*
     * We're good, commit the CPU ID leaves.
     */
    MMHyperFree(pVM, pVM->cpum.s.GuestInfo.paCpuIdLeavesR3);
    pVM->cpum.s.GuestInfo.paCpuIdLeavesR0 = NIL_RTR0PTR;
    pVM->cpum.s.GuestInfo.paCpuIdLeavesRC = NIL_RTRCPTR;
    pVM->cpum.s.GuestInfo.DefCpuId = GuestCpuIdDef;
    rc = cpumR3CpuIdInstallAndExplodeLeaves(pVM, &pVM->cpum.s, paLeaves, cLeaves);
    RTMemFree(paLeaves);
    AssertLogRelRCReturn(rc, rc);


#undef CPUID_CHECK_RET
#undef CPUID_CHECK_WRN
#undef CPUID_CHECK2_RET
#undef CPUID_CHECK2_WRN
#undef CPUID_RAW_FEATURE_RET
#undef CPUID_RAW_FEATURE_WRN
#undef CPUID_RAW_FEATURE_IGN
#undef CPUID_GST_FEATURE_RET
#undef CPUID_GST_FEATURE_WRN
#undef CPUID_GST_FEATURE_EMU
#undef CPUID_GST_FEATURE_IGN
#undef CPUID_GST_FEATURE2_RET
#undef CPUID_GST_FEATURE2_WRN
#undef CPUID_GST_FEATURE2_EMU
#undef CPUID_GST_FEATURE2_IGN
#undef CPUID_GST_AMD_FEATURE_RET
#undef CPUID_GST_AMD_FEATURE_WRN
#undef CPUID_GST_AMD_FEATURE_EMU
#undef CPUID_GST_AMD_FEATURE_IGN

    return VINF_SUCCESS;
}

#endif /* VBOX_IN_VMM */
