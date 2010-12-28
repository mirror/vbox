/** @file
 * DBGF - Debugger Facility, VM Core File Format.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef ___VBox_vmm_dbgfcore_h
#define ___VBox_vmm_dbgfcore_h

#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN


/** @addgroup grp_dbgf_corefmt  VM Core File Format
 * @ingroup grp_dbgf
 *
 * @todo Add description of the core file format and how the structures in this
 *       file relate to it.  Point to CPUMCTX in cpum.h for the CPU state.
 * @todo Add the note names.
 *
 * @{
 */

/** DBGCORECOREDESCRIPTOR::u32Magic. */
#define DBGFCORE_MAGIC          UINT32_C(0xc01ac0de)
/** DBGCORECOREDESCRIPTOR::u32FmtVersion. */
#define DBGFCORE_FMT_VERSION    UINT32_C(0x00010000)

/**
 * The DBGF Core descriptor.
 */
typedef struct DBGFCOREDESCRIPTOR
{
    /** The core file magic (DBGFCORE_MAGIC) */
    uint32_t                u32Magic;
    /** The core file format version (DBGFCORE_FMT_VERSION). */
    uint32_t                u32FmtVersion;
    /** Size of this structure (sizeof(DBGFCOREDESCRIPTOR)). */
    uint32_t                cbSelf;
    /** VirtualBox version. */
    uint32_t                u32VBoxVersion;
    /** VirtualBox revision. */
    uint32_t                u32VBoxRevision;
    /** Number of CPUs. */
    uint32_t                cCpus;
} DBGFCOREDESCRIPTOR;
AssertCompileSizeAlignment(DBGFCOREDESCRIPTOR, 8);
/** Pointer to DBGFCOREDESCRIPTOR data. */
typedef DBGFCOREDESCRIPTOR  *PDBGFCOREDESCRIPTOR;

/** @}  */

RT_C_DECLS_END

#endif

