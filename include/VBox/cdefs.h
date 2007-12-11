/** @file
 * VirtualBox - Common C and C++ definition.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_cdefs_h
#define ___VBox_cdefs_h

#include <iprt/cdefs.h>


/** @def VBOX_WITH_STATISTICS
 * When defined all statistics will be included in the build.
 * This is enabled by default in all debug builds.
 */
#ifndef VBOX_WITH_STATISTICS
# ifdef DEBUG
#  define VBOX_WITH_STATISTICS
# endif
#endif

/** @def VBOX_STRICT
 * Alias for RT_STRICT.
 */
#ifdef RT_STRICT
# ifndef VBOX_STRICT
#  define VBOX_STRICT
# endif
#endif


/*
 * Shut up DOXYGEN warnings and guide it properly thru the code.
 */
#ifdef  __DOXYGEN__
#define VBOX_WITH_STATISTICS
#ifndef VBOX_STRICT
# define VBOX_STRICT
#endif
#define IN_CFGLDR_R3
#define IN_CFGM_GC
#define IN_CFGM_R3
#define IN_CPUM_GC
#define IN_CPUM_R0
#define IN_CPUM_R3
#define IN_CSAM_R0
#define IN_CSAM_R3
#define IN_CSAM_GC
#define IN_DBGF_R0
#define IN_DBGF_R3
#define IN_DBGF_GC
#define IN_DIS_GC
#define IN_DIS_R0
#define IN_DIS_R3
#define IN_EM_GC
#define IN_EM_R3
#define IN_EM_R0
#define IN_HWACCM_R0
#define IN_HWACCM_R3
#define IN_HWACCM_GC
#define IN_IDE_R3
#define IN_INTNET_R0
#define IN_INTNET_R3
#define IN_IOM_GC
#define IN_IOM_R3
#define IN_IOM_R0
#define IN_MM_GC
#define IN_MM_R0
#define IN_MM_R3
#define IN_PATM_R0
#define IN_PATM_R3
#define IN_PATM_GC
#define IN_PDM_GC
#define IN_PDM_R3
#define IN_PGM_GC
#define IN_PGM_R0
#define IN_PGM_R3
#define IN_REM_GC
#define IN_REM_R0
#define IN_REM_R3
#define IN_SELM_GC
#define IN_SELM_R0
#define IN_SELM_R3
#define IN_SSM_R3
#define IN_STAM_GC
#define IN_STAM_R0
#define IN_STAM_R3
#define IN_SUP_R0
#define IN_SUP_R3
#define IN_SUP_GC
#define IN_TM_GC
#define IN_TM_R0
#define IN_TM_R3
#define IN_TRPM_GC
#define IN_TRPM_R0
#define IN_TRPM_R3
#define IN_USB_GC
#define IN_USB_R0
#define IN_USB_R3
#define IN_USBLIB
#define IN_VGADEVICE_GC
#define IN_VGADEVICE_R3
#define IN_VHHD_R3
#define IN_VM_GC
#define IN_VM_R0
#define IN_VM_R3
#define IN_VMM_GC
#define IN_VMM_R0
#define IN_VMM_R3
/** @todo fixme */
#endif




/** @def VBOXCALL
 * The standard calling convention for VBOX interfaces.
 */
#define VBOXCALL   RTCALL



/** @def IN_VM_R3
 * Used to indicate whether we're inside the same link module as the ring 3 part of the
 * virtual machine (the module) or not.
 */
/** @def VMR3DECL
 * Ring 3 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VM_R3
# define VMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define VMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_VM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the virtual machine (the module) or not.
 */
/** @def VMR0DECL
 * Ring 0 VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VM_R0
# define VMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define VMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_VM_GC
 * Used to indicate whether we're inside the same link module as the guest context
 * part of the virtual machine (the module) or not.
 */
/** @def VMGCDECL
 * Guest context VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VM_GC
# define VMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define VMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def VMDECL
 * VM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_VM_R3) || defined(IN_VM_R0) || defined(IN_VM_GC)
# define VMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define VMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_VMM_R3
 * Used to indicate whether we're inside the same link module as the ring 3 part of the
 * virtual machine monitor or not.
 */
/** @def VMMR3DECL
 * Ring 3 VMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VMM_R3
# define VMMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define VMMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_VMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0 part of the
 * virtual machine monitor or not.
 */
/** @def VMMR0DECL
 * Ring 0 VMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VMM_R0
# define VMMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define VMMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_VMM_GC
 * Used to indicate whether we're inside the same link module as the guest context
 * part of the virtual machine monitor or not.
 */
/** @def VMMGCDECL
 * Guest context VMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VMM_GC
# define VMMGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define VMMGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def VMMDECL
 * VMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_VMM_R3) || defined(IN_VMM_R0) || defined(IN_VMM_GC)
# define VMMDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define VMMDECL(type)      DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_DIS_R3
 * Used to indicate whether we're inside the same link module as the
 * Ring-3 disassembler or not.
 */
/** @def DISR3DECL(type)
 * Disassembly export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DIS_R3
# define DISR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define DISR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_DIS_R0
 * Used to indicate whether we're inside the same link module as the
 * Ring-0 disassembler or not.
 */
/** @def DISR0DECL(type)
 * Disassembly export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DIS_R0
# define DISR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define DISR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_DIS_GC
 * Used to indicate whether we're inside the same link module as the
 * GC disassembler or not.
 */
/** @def DISGCDECL(type)
 * Disassembly export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DIS_GC
# define DISGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define DISGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def DISDECL(type)
 * Disassembly export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_DIS_R3) ||defined(IN_DIS_R0) || defined(IN_DIS_GC)
# define DISDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define DISDECL(type)      DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_SUP_R3
 * Used to indicate whether we're inside the same link module as the Ring 3 Support Library or not.
 */
/** @def SUPR3DECL(type)
 * Support library export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SUP_R3
# define SUPR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define SUPR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_SUP_R0
 * Used to indicate whether we're inside the same link module as the Ring 0 Support Library or not.
 */
/** @def SUPR0DECL(type)
 * Support library export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SUP_R0
# define SUPR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define SUPR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_SUP_GC
 * Used to indicate whether we're inside the same link module as the GC Support Library or not.
 */
/** @def SUPGCDECL(type)
 * Support library export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SUP_GC
# define SUPGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define SUPGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_SUP_R0
 * Used to indicate whether we're inside the same link module as the Ring 0 Support Library or not.
 */
/** @def SUPR0DECL(type)
 * Support library export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_SUP_R0) || defined(IN_SUP_R3) || defined(IN_SUP_GC)
# define SUPDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define SUPDECL(type)    DECLIMPORT(type) VBOXCALL
#endif




/** @def IN_PDM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Pluggable Device Manager.
 */
/** @def PDMR3DECL(type)
 * Pluggable Device Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PDM_R3
# define PDMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define PDMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_PDM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Pluggable Device Manager.
 */
/** @def PDMGCDECL(type)
 * Pluggable Device Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PDM_GC
# define PDMGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define PDMGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def PDMDECL(type)
 * Pluggable Device Manager export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_PDM_R3) || defined(IN_PDM_GC) || defined(IN_PDM_R0)
# define PDMDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define PDMDECL(type)      DECLIMPORT(type) VBOXCALL
#endif




/** @def IN_CFGM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Configuration Manager.
 */
/** @def CFGMR3DECL(type)
 * Configuration Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CFGM_R3
# define CFGMR3DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define CFGMR3DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_CFGM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Configuration Manager.
 */
/** @def CFGMGCDECL(type)
 * Configuration Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CFGM_GC
# define CFGMGCDECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define CFGMGCDECL(type)   DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_CPUM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 CPU Monitor(/Manager).
 */
/** @def CPUMR0DECL(type)
 * CPU Monitor(/Manager) HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CPUM_R0
# define CPUMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CPUMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_CPUM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 CPU Monitor(/Manager).
 */
/** @def CPUMR3DECL(type)
 * CPU Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CPUM_R3
# define CPUMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CPUMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_CPUM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC CPU Monitor(/Manager).
 */
/** @def CPUMGCDECL(type)
 * CPU Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CPUM_GC
# define CPUMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CPUMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def CPUMDECL(type)
 * CPU Monitor(/Manager) export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_CPUM_R3) || defined(IN_CPUM_GC) || defined(IN_CPUM_R0)
# define CPUMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define CPUMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_PATM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 CPU Monitor(/Manager).
 */
/** @def PATMR0DECL(type)
 * Patch Manager HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PATM_R0
# define PATMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define PATMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_PATM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 CPU Monitor(/Manager).
 */
/** @def PATMR3DECL(type)
 * Patch Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PATM_R3
# define PATMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define PATMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_PATM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Patch Manager.
 */
/** @def PATMGCDECL(type)
 * Patch Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PATM_GC
# define PATMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define PATMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def PATMDECL(type)
 * Patch Manager all contexts export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_PATM_R3) || defined(IN_PATM_R0) || defined(IN_PATM_GC)
# define PATMDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define PATMDECL(type)     DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_CSAM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 CPU Monitor(/Manager).
 */
/** @def CSAMR0DECL(type)
 * Code Scanning and Analysis Manager HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CSAM_R0
# define CSAMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CSAMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_CSAM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 CPU Monitor(/Manager).
 */
/** @def CSAMR3DECL(type)
 * Code Scanning and Analysis Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CSAM_R3
# define CSAMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CSAMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_CSAM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Code Scanning and Analysis Manager.
 */
/** @def CSAMGCDECL(type)
 * Code Scanning and Analysis Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_CSAM_GC
# define CSAMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define CSAMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def CSAMDECL(type)
 * Code Scanning and Analysis Manager export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_CSAM_R3) || defined(IN_CSAM_GC) || defined(IN_CSAM_R0)
# define CSAMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define CSAMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_MM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Memory Monitor(/Manager).
 */
/** @def MMR0DECL(type)
 * Memory Monitor(/Manager) HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_MM_R0
# define MMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define MMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_MM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Memory Monitor(/Manager).
 */
/** @def MMR3DECL(type)
 * Memory Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_MM_R3
# define MMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define MMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_MM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Memory Monitor(/Manager).
 */
/** @def MMGCDECL(type)
 * Memory Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_MM_GC
# define MMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define MMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def MMDECL(type)
 * Memory Monitor(/Manager) export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_MM_R3) || defined(IN_MM_GC) || defined(IN_MM_R0)
# define MMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define MMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif




/** @def IN_SELM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Selector Monitor(/Manager).
 */
/** @def SELMR0DECL(type)
 * Selector Monitor(/Manager) HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SELM_R0
# define SELMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define SELMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_SELM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Selector Monitor(/Manager).
 */
/** @def SELMR3DECL(type)
 * Selector Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SELM_R3
# define SELMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define SELMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_SELM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Selector Monitor(/Manager).
 */
/** @def SELMGCDECL(type)
 * Selector Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SELM_GC
# define SELMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define SELMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def SELMDECL(type)
 * Selector Monitor(/Manager) export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_SELM_R3) || defined(IN_SELM_GC) || defined(IN_SELM_R0)
# define SELMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define SELMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_HWACCM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 VMX Monitor(/Manager).
 */
/** @def HWACCMR0DECL(type)
 * Selector Monitor(/Manager) HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_HWACCM_R0
# define HWACCMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define HWACCMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_HWACCM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 VMX Monitor(/Manager).
 */
/** @def HWACCMR3DECL(type)
 * Selector Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_HWACCM_R3
# define HWACCMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define HWACCMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_HWACCM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC VMX Monitor(/Manager).
 */
/** @def HWACCMGCDECL(type)
 * Selector Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_HWACCM_GC
# define HWACCMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define HWACCMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def HWACCMDECL(type)
 * VMX Monitor(/Manager) export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_HWACCM_R3) || defined(IN_HWACCM_GC) || defined(IN_HWACCM_R0)
# define HWACCMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define HWACCMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_TM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Time Manager.
 */
/** @def TMR0DECL(type)
 * Time Manager HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TM_R0
# define TMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_TM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Time Manager.
 */
/** @def TMR3DECL(type)
 * Time Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TM_R3
# define TMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_TM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Time Manager.
 */
/** @def TMGCDECL(type)
 * Time Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TM_GC
# define TMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def TMDECL(type)
 * Time Manager export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_TM_R3) || defined(IN_TM_GC) || defined(IN_TM_R0)
# define TMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define TMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif




/** @def IN_TRPM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Trap Monitor.
 */
/** @def TRPMR0DECL(type)
 * Trap Monitor HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TRPM_R0
# define TRPMR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TRPMR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_TRPM_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Trap Monitor.
 */
/** @def TRPMR3DECL(type)
 * Trap Monitor HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TRPM_R3
# define TRPMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TRPMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_TRPM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Trap Monitor.
 */
/** @def TRPMGCDECL(type)
 * Trap Monitor GC export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_TRPM_GC
# define TRPMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define TRPMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def TRPMDECL(type)
 * Trap Monitor export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_TRPM_R3) || defined(IN_TRPM_GC) || defined(IN_TRPM_R0)
# define TRPMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define TRPMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif




/** @def IN_USB_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 USB device / service.
 */
/** @def USBR0DECL(type)
 * USB device / service HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_USB_R0
# define USBR0DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define USBR0DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_USB_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 USB device / service.
 */
/** @def USBR3DECL(type)
 * USB device / services HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_USB_R3
# define USBR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define USBR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_USB_GC
 * Used to indicate whether we're inside the same link module as
 * the GC USB device.
 */
/** @def USBGCDECL(type)
 * USB device GC export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_USB_GC
# define USBGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define USBGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def USBDECL(type)
 * Trap Monitor export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_USB_R3) || defined(IN_USB_GC) || defined(IN_USB_R0)
# define USBDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define USBDECL(type)       DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_USBLIB
 * Used to indicate whether we're inside the same link module as the USBLib.
 */
/** @def USBLIB_DECL
 * USBLIB export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_RING0
# define USBLIB_DECL(type)   type VBOXCALL
#elif defined(IN_USBLIB)
# define USBLIB_DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define USBLIB_DECL(type)   DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_INTNET_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Internal Networking Service.
 */
/** @def INTNETR3DECL(type)
 * Internal Networking Service export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_INTNET_R3
# define INTNETR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define INTNETR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_INTNET_R0
 * Used to indicate whether we're inside the same link module as the R0
 * Internal Network Service.
 */
/** @def INTNETR0DECL(type)
 * Internal Networking Service export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_INTNET_R0
# define INTNETR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define INTNETR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_IOM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Input/Output Monitor.
 */
/** @def IOMR3DECL(type)
 * Input/Output Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_IOM_R3
# define IOMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define IOMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_IOM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Input/Output Monitor.
 */
/** @def IOMGCDECL(type)
 * Input/Output Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_IOM_GC
# define IOMGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define IOMGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_IOM_R0
 * Used to indicate whether we're inside the same link module as the R0
 * Input/Output Monitor.
 */
/** @def IOMR0DECL(type)
 * Input/Output Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_IOM_R0
# define IOMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define IOMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IOMDECL(type)
 * Input/Output Monitor export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_IOM_R3) || defined(IN_IOM_GC) || defined(IN_IOM_R0)
# define IOMDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define IOMDECL(type)      DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_VGADEVICE_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 VGA Monitor(/Manager).
 */
/** @def VGAR3DECL(type)
 * VGA Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VGADEVICE_R3
# define VGAR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define VGAR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_VGADEVICE_GC
 * Used to indicate whether we're inside the same link module as
 * the GC VGA Monitor(/Manager).
 */
/** @def VGAGCDECL(type)
 * VGA Monitor(/Manager) HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VGADEVICE_GC
# define VGAGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define VGAGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def VGADECL(type)
 * VGA Monitor(/Manager) export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_VGADEVICE_R3) || defined(IN_VGADEVICE_GC)
# define VGADECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define VGADECL(type)       DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_PGM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Page Monitor and Manager.
 */
/** @def PGMR3DECL(type)
 * Page Monitor and Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PGM_R3
# define PGMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define PGMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_PGM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Page Monitor and Manager.
 */
/** @def PGMGCDECL(type)
 * Page Monitor and Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PGM_GC
# define PGMGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define PGMGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_PGM_R0
 * Used to indicate whether we're inside the same link module as the R0
 * Page Monitor and Manager.
 */
/** @def PGMR0DECL(type)
 * Page Monitor and Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_PGM_R0
# define PGMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define PGMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def PGMDECL(type)
 * Page Monitor and Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_PGM_R3) || defined(IN_PGM_R0) || defined(IN_PGM_GC)
# define PGMDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define PGMDECL(type)      DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_DBGF_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Debugging Facility.
 */
/** @def DBGFR3DECL(type)
 * R3 Debugging Facility export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DBGF_R3
# define DBGFR3DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define DBGFR3DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_DBGF_R0
 * Used to indicate whether we're inside the same link module as the Ring 0
 * Debugging Facility.
 */
/** @def DBGFR0DECL(type)
 * R0 Debugging Facility export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DBGF_R0
# define DBGFR0DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define DBGFR0DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_DBGF_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Debugging Facility.
 */
/** @def DBGFGCDECL(type)
 * GC Debugging Facility export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_DBGF_GC
# define DBGFGCDECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define DBGFGCDECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def DBGFGCDECL(type)
 * Debugging Facility export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_DBGF_R3) || defined(IN_DBGF_R0) || defined(IN_DBGF_GC)
# define DBGFDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define DBGFDECL(type)     DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_SSM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Saved State Manager.
 */
/** @def SSMR3DECL(type)
 * R3 Saved State export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_SSM_R3
# define SSMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define SSMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_STAM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Statistics Manager.
 */
/** @def STAMR3DECL(type)
 * R3 Statistics Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_STAM_R3
# define STAMR3DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define STAMR3DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_STAM_R0
 * Used to indicate whether we're inside the same link module as the Ring 0
 * Statistics Manager.
 */
/** @def STAMR0DECL(type)
 * R0 Statistics Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_STAM_R0
# define STAMR0DECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define STAMR0DECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_STAM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Statistics Manager.
 */
/** @def STAMGCDECL(type)
 * GC Statistics Manager export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_STAM_GC
# define STAMGCDECL(type)   DECLEXPORT(type) VBOXCALL
#else
# define STAMGCDECL(type)   DECLIMPORT(type) VBOXCALL
#endif

/** @def STAMGCDECL(type)
 * Debugging Facility export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_STAM_R3) || defined(IN_STAM_R0) || defined(IN_STAM_GC)
# define STAMDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define STAMDECL(type)     DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_EM_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * Execution Monitor.
 */
/** @def EMR3DECL(type)
 * Execution Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_EM_R3
# define EMR3DECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define EMR3DECL(type)     DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_EM_GC
 * Used to indicate whether we're inside the same link module as the GC
 * Execution Monitor.
 */
/** @def EMGCDECL(type)
 * Execution Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_EM_GC
# define EMGCDECL(type)     DECLEXPORT(type) VBOXCALL
#else
# define EMGCDECL(type)     DECLIMPORT(type) VBOXCALL
#endif


/** @def EMDECL(type)
 * Execution Monitor export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_EM_R3) || defined(IN_EM_GC) || defined(IN_EM_R0)
# define EMDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define EMDECL(type)       DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_IDE_R3
 * Used to indicate whether we're inside the same link module as the Ring 3
 * IDE device.
 */
/** @def IDER3DECL(type)
 * Ring-3 IDE device export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_IDE_R3
# define IDER3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define IDER3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif


/** @def IN_VBOXDDU
 * Used to indicate whether we're inside the VBoxDDU shared object.
 */
/** @def VBOXDDU_DECL(type)
 * VBoxDDU export or import (ring-3).
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_VBOXDDU
# define VBOXDDU_DECL(type) DECLEXPORT(type) VBOXCALL
#else
# define VBOXDDU_DECL(type) DECLIMPORT(type) VBOXCALL
#endif



/** @def IN_REM_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Recompiled Execution Manager.
 */
/** @def REMR0DECL(type)
 * Recompiled Execution Manager HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_REM_R0
# define REMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define REMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_RT_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Recompiled Execution Manager.
 */
/** @def RTR3DECL(type)
 * Recompiled Execution Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_REM_R3
# define REMR3DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define REMR3DECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def IN_REM_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Recompiled Execution Manager.
 */
/** @def REMGCDECL(type)
 * Recompiled Execution Manager HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_REM_GC
# define REMGCDECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define REMGCDECL(type)    DECLIMPORT(type) VBOXCALL
#endif

/** @def REMDECL(type)
 * Recompiled Execution Manager export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_REM_R3) || defined(IN_REM_GC) || defined(IN_REM_R0)
# define REMDECL(type)      DECLEXPORT(type) VBOXCALL
#else
# define REMDECL(type)      DECLIMPORT(type) VBOXCALL
#endif




/** @def DBGDECL(type)
 * Debugger module export or import declaration.
 * Functions declared using this exists only in R3 since the
 * debugger modules is R3 only.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_DBG_R3) || defined(IN_DBG)
# define DBGDECL(type)       DECLEXPORT(type) VBOXCALL
#else
# define DBGDECL(type)       DECLIMPORT(type) VBOXCALL
#endif




#endif

