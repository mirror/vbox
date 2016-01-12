/* $Id$ */
/** @file
 * BS3Kit header for multi-mode code templates.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include "bs3kit.h"

/** @defgroup grp_bs3kit_tmpl       BS3Kit Multi-Mode Code Templates
 * @ingroup grp_bs3kit
 *
 * Multi-mode code templates avoid duplicating code for each of the CPU modes.
 * Instead the code is compiled multiple times, either via multiple inclusions
 * into a source files with different mode selectors defined or by multiple
 * compiler invocations.
 *
 * In C/C++ code we're restricted to the compiler target bit count, whereas in
 * assembly we can do everything in assembler run (with some 64-bit
 * restrictions, that is).
 *
 * Before \#defining the next mode selector and including
 * bs3kit-template-header.h again, include bs3kit-template-footer.h to undefine
 * all the previous mode selectors and the macros defined by the header.
 *
 * @{
 */

#ifdef DOXYGEN_RUNNING
/** @name Template mode selectors.
 *
 * Exactly one of these are defined by the file including the
 * bs3kit-template-header.h header file.  When building the code libraries, the
 * kBuild target defines this.
 *
 * @{ */
# define TMPL_RM     /**< real mode. */
# define TMPL_PE16   /**< 16-bit protected mode, unpaged. */
# define TMPL_PE32   /**< 32-bit protected mode, unpaged. */
# define TMPL_PEV86  /**< virtual 8086 mode under protected mode, unpaged. */
# define TMPL_PP16   /**< 16-bit protected mode, paged. */
# define TMPL_PP32   /**< 32-bit protected mode, paged. */
# define TMPL_PPV86  /**< virtual 8086 mode under protected mode, paged. */
# define TMPL_PAE16  /**< 16-bit protected mode with PAE (paged). */
# define TMPL_PAE32  /**< 16-bit protected mode with PAE (paged). */
# define TMPL_PAEV86 /**< virtual 8086 mode under protected mode with PAE (paged). */
# define TMPL_LM16   /**< 16-bit long mode (paged). */
# define TMPL_LM32   /**< 32-bit long mode (paged). */
# define TMPL_LM64   /**< 64-bit long mode (paged). */
/** @} */

/** @name Derived Indicators
 * @{ */
# define TMPL_CMN_PE    /**< TMPL_PE16  | TMPL_PE32  | TMPL_PEV86  */
# define TMPL_CMN_PP    /**< TMPL_PP16  | TMPL_PP32  | TMPL_PPV86  */
# define TMPL_CMN_PAE   /**< TMPL_PAE16 | TMPL_PAE32 | TMPL_PAEV86 */
# define TMPL_CMN_LM    /**< TMPL_LM16  | TMPL_LM32  | TMPL_LM64   */
# define TMPL_CMN_V86   /**< TMPL_PEV86 | TMPL_PPV86 | TMPL_PAEV86 */
# define TMPL_CMN_R86   /**< TMPL_CMN_V86 | TMPL_RM                */
# define TMPL_CMN_PAGING /**< TMPL_CMN_PP | TMPL_CMN_PAE | TMPL_CMN_LM */
/** @} */

/** @def TMPL_NM
 * Name mangling macro for the current mode.
 *
 * Example: TMPL_NM(PrintChr)
 *
 * @param   Name        The function or variable name to mangle.
 */
# define TMPL_NM(Name)  RT_CONCAT(Name,_mode)

/** @def TMPL_MODE_STR
 * Short mode description. */
# define TMPL_MODE_STR

/** @def TMPL_HAVE_BIOS
 * Indicates that we have direct access to the BIOS (only in real mode). */
# define TMPL_HAVE_BIOS


/** @name For ASM compatability
 * @{ */
/** @def TMPL_16BIT
 * For ASM compatibility - please use ARCH_BITS == 16. */
# define TMPL_16BIT
/** @def TMPL_32BIT
 * For ASM compatibility - please use ARCH_BITS == 32. */
# define TMPL_32BIT
/** @def TMPL_64BIT
 * For ASM compatibility - please use ARCH_BITS == 64. */
# define TMPL_64BIT

/** @def TMPL_BITS
 * For ASM compatibility - please use ARCH_BITS instead. */
# define TMPL_BITS  ARCH_BITS
/** @} */

#else /* !DOXYGEN_RUNNING */

#undef BS3_CMN_NM

#ifdef TMPL_RM
# ifdef TMPL_PE16
#  error "Both 'TMPL_RM' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_RM' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_RM' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_RM' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_RM' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_RM' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_RM' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_RM' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_RM' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_RM' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_RM' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_RM' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_RM requires ARCH_BITS to be 16."
# endif
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_rm)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "real mode"
# define TMPL_HAVE_BIOS
# define TMPL_CMN_R86
#endif

#ifdef TMPL_PE16
# ifdef TMPL_RM
#  error "Both 'TMPL_PE16' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PE16' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_RM' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PE16' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PE16' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PE16' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PE16' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PE16' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PE32' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PE16' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PE16' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PE16' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PE16 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PE
# define TMPL_CMN_P16
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_pe16)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "16-bit unpaged protected mode"
#endif

#ifdef TMPL_PE32
# ifdef TMPL_RM
#  error "Both 'TMPL_PE32' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PE32' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PE32' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PE32' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PE32' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PE32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PE32' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PE32' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAE86
#  error "Both 'TMPL_PE32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PE32' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PE32' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PE32' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 32
#  error "TMPL_PE32 requires ARCH_BITS to be 32."
# endif
# define TMPL_CMN_PE
# define TMPL_CMN_P32
# define TMPL_32BIT
# define TMPL_BITS              32
# define TMPL_NM(Name)          RT_CONCAT(Name,_pe32)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c32)
# define TMPL_MODE_STR          "32-bit unpaged protected mode"
#endif

#ifdef TMPL_PEV86
# ifdef TMPL_RM
#  error "Both 'TMPL_PEV86' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PEV86' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PEV86' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PEV86' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PEV86' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PEV86' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PEV86' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PEV86' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAE86
#  error "Both 'TMPL_PEV86' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PEV86' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PEV86' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PEV86' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PEV86 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PE
# define TMPL_CMN_V86
# define TMPL_CMN_R86
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_pev86)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "v8086 unpaged protected mode"
#endif

#ifdef TMPL_PP16
# ifdef TMPL_RM
#  error "Both 'TMPL_PP16' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PP16' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PP16' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PP16' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PP16' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PP32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PP16' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PP16' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PP16' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PP16' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PP16' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PP16' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PP16 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PP
# define TMPL_CMN_PAGING
# define TMPL_CMN_P16
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_pp16)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "16-bit paged protected mode"
#endif

#ifdef TMPL_PP32
# ifdef TMPL_RM
#  error "Both 'TMPL_PP32' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PP32' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PP32' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PP32' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PP32' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PP32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PP32' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PP32' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PP32' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PP32' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PP32' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PP32' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 32
#  error "TMPL_PP32 requires ARCH_BITS to be 32."
# endif
# define TMPL_CMN_PP
# define TMPL_CMN_PAGING
# define TMPL_CMN_P32
# define TMPL_32BIT
# define TMPL_BITS              32
# define TMPL_NM(Name)          RT_CONCAT(Name,_pp32)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c32)
# define TMPL_MODE_STR          "32-bit paged protected mode"
#endif

#ifdef TMPL_PPV86
# ifdef TMPL_RM
#  error "Both 'TMPL_PPV86' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PPV86' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PPV86' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PPV86' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PPV86' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PPV86' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PPV86' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PPV86' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PPV86' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PPV86' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PPV86' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PPV86' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PPV86 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PP
# define TMPL_CMN_PAGING
# define TMPL_CMN_V86
# define TMPL_CMN_R86
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_ppv86)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c86)
# define TMPL_MODE_STR          "v8086 paged protected mode"
#endif

#ifdef TMPL_PAE16
# ifdef TMPL_RM
#  error "Both 'TMPL_PAE16' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PAE16' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PAE16' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PAE16' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PAE16' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PAE16' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PAE16' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_PAE16' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PAE16' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PAE16' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PAE16' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PAE16' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PAE16 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PAE
# define TMPL_CMN_PAGING
# define TMPL_16BIT
# define TMPL_CMN_P16
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_pae16)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "16-bit pae protected mode"
#endif

#ifdef TMPL_PAE32
# ifdef TMPL_RM
#  error "Both 'TMPL_PAE32' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PAE32' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PAE32' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PAE32' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PAE32' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PAE32' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PAE32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PAE32' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PAE32' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PAE32' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PAE32' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PAE32' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 32
#  error "TMPL_PAE32 requires ARCH_BITS to be 32."
# endif
# define TMPL_CMN_PAE
# define TMPL_CMN_PAGING
# define TMPL_CMN_P32
# define TMPL_32BIT
# define TMPL_BITS              32
# define TMPL_NM(Name)          RT_CONCAT(Name,_pae32)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c32)
# define TMPL_MODE_STR          "32-bit pae protected mode"
#endif

#ifdef TMPL_PAEV86
# ifdef TMPL_RM
#  error "Both 'TMPL_PAEV86' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_PAEV86' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_PAEV86' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_PAEV86' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_PAEV86' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_PAEV86' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_PAEV86' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_PAEV86' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_PAEV86' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_PAEV86' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_PAEV86' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_PAEV86' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_PAEV86 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_PAE
# define TMPL_CMN_PAGING
# define TMPL_CMN_V86
# define TMPL_CMN_R86
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_paev86)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c86)
# define TMPL_MODE_STR          "v8086 pae protected mode"
#endif

#ifdef TMPL_LM16
# ifdef TMPL_RM
#  error "Both 'TMPL_LM16' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_LM16' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_LM16' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_LM16' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_LM16' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_LM16' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_LM16' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_LM16' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_LM16' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_LM16' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_LM16' and 'TMPL_LM32' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_LM16' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 16
#  error "TMPL_LM16 requires ARCH_BITS to be 16."
# endif
# define TMPL_CMN_LM
# define TMPL_CMN_PAGING
# define TMPL_CMN_P16
# define TMPL_16BIT
# define TMPL_BITS              16
# define TMPL_NM(Name)          RT_CONCAT(Name,_lm16)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c16)
# define TMPL_MODE_STR          "16-bit long mode"
#endif

#ifdef TMPL_LM32
# ifdef TMPL_RM
#  error "Both 'TMPL_LM32' and 'TMPL_RM' are defined."
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_LM32' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_LM32' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_LM32' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_LM32' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_LM32' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_LM32' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_LM32' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_LM32' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_LM32' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_LM32' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM64
#  error "Both 'TMPL_LM32' and 'TMPL_LM64' are defined."
# endif
# if ARCH_BITS != 32
#  error "TMPL_LM32 requires ARCH_BITS to be 32."
# endif
# define TMPL_CMN_LM
# define TMPL_CMN_PAGING
# define TMPL_CMN_P32
# define TMPL_32BIT
# define TMPL_BITS              32
# define TMPL_NM(Name)          RT_CONCAT(Name,_lm32)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c32)
# define TMPL_MODE_STR          "32-bit long mode"
#endif

#ifdef TMPL_LM64
# ifdef TMPL_RM
#  error ""Both 'TMPL_LM64' and 'TMPL_RM' are defined.""
# endif
# ifdef TMPL_PE16
#  error "Both 'TMPL_LM64' and 'TMPL_PE16' are defined."
# endif
# ifdef TMPL_PE32
#  error "Both 'TMPL_LM64' and 'TMPL_PE32' are defined."
# endif
# ifdef TMPL_PEV86
#  error "Both 'TMPL_LM64' and 'TMPL_PEV86' are defined."
# endif
# ifdef TMPL_PP16
#  error "Both 'TMPL_LM64' and 'TMPL_PP16' are defined."
# endif
# ifdef TMPL_PP32
#  error "Both 'TMPL_LM64' and 'TMPL_PP32' are defined."
# endif
# ifdef TMPL_PPV86
#  error "Both 'TMPL_LM64' and 'TMPL_PPV86' are defined."
# endif
# ifdef TMPL_PAE16
#  error "Both 'TMPL_LM64' and 'TMPL_PAE16' are defined."
# endif
# ifdef TMPL_PAE32
#  error "Both 'TMPL_LM64' and 'TMPL_PAE32' are defined."
# endif
# ifdef TMPL_PAEV86
#  error "Both 'TMPL_LM64' and 'TMPL_PAEV86' are defined."
# endif
# ifdef TMPL_LM16
#  error "Both 'TMPL_LM64' and 'TMPL_LM16' are defined."
# endif
# ifdef TMPL_LM32
#  error "Both 'TMPL_LM64' and 'TMPL_LM32' are defined."
# endif
# if ARCH_BITS != 64
#  error "TMPL_LM64 requires ARCH_BITS to be 64."
# endif
# define TMPL_CMN_LM
# define TMPL_CMN_PAGING
# define TMPL_CMN_P64
# define TMPL_64BIT
# define TMPL_BITS              64
# define TMPL_NM(Name)          RT_CONCAT(Name,_lm64)
# define BS3_CMN_NM(Name)       RT_CONCAT(Name,_c64)
# define TMPL_MODE_STR          "64-bit long mode"
#endif

#ifndef TMPL_MODE_STR
# error "internal error"
#endif

#endif /* !DOXYGEN_RUNNING */
/** @} */

