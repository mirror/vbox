/* $Id$ */
/** @file
 * CPUM - CPU ID part.
 */

/*
 * Copyright (C) 2013-2014 Oracle Corporation
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
bool cpumR3CpuIdGetLeafLegacy(PCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, uint32_t uLeaf, uint32_t uSubLeaf, PCPUMCPUID pLeagcy)
{
    PCPUMCPUIDLEAF pLeaf = cpumR3CpuIdGetLeaf(paLeaves, cLeaves, uLeaf, uSubLeaf);
    if (pLeaf)
    {
        pLeagcy->eax = pLeaf->uEax;
        pLeagcy->ebx = pLeaf->uEbx;
        pLeagcy->ecx = pLeaf->uEcx;
        pLeagcy->edx = pLeaf->uEdx;
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
     * Disallow replacing CPU ID leaves that this API currently cannot manage.                                                                                .
     * These leaves have dependencies on saved-states, see PATMCpuidReplacement().
     * If you want to modify these leaves, use CPUMSetGuestCpuIdFeature().                                                                         .
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

