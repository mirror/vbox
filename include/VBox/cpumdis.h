/** @file
 * CPUM - CPU Monitor(/Manager)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_cpumdis_h
#define ___VBox_cpumdis_h

#include <VBox/cpum.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>
#include <VBox/dis.h>


__BEGIN_DECLS
/** @addtogroup grp_cpum
 * @{
 */

#ifdef IN_RING3

/**
 * Disassemble an instruction and return the information in the provided structure.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   GCPtrPC     Program counter (relative to CS) to disassemble from.
 * @param   pCpu        Disassembly state
 * @param   pszPrefix   String prefix for logging (debug only)
 *
 */
CPUMR3DECL(int) CPUMR3DisasmInstrCPU(PVM pVM, PCPUMCTX pCtx, RTGCPTR GCPtrPC, PDISCPUSTATE pCpu, const char *pszPrefix);

# ifdef DEBUG
/**
 * Disassemble an instruction and dump it to the log
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   pc          GC instruction pointer
 * @param   prefix      String prefix for logging
 *
 * @deprecated  Use DBGFR3DisasInstrCurrentLog().
 */
CPUMR3DECL(void) CPUMR3DisasmInstr(PVM pVM, PCPUMCTX pCtx, RTGCPTR pc, char *prefix);

/**
 * Disassemble an instruction and dump it to the log
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pCtx        CPU context
 * @param   pc          GC instruction pointer
 * @param   prefix      String prefix for logging
 * @param   nrInstructions
 *
 * @deprecated  Create new DBGFR3Disas function to do this.
 */
CPUMR3DECL(void) CPUMR3DisasmBlock(PVM pVM, PCPUMCTX pCtx, RTGCPTR pc, char *prefix, int nrInstructions);

# else
#  define CPUMR3DisasmInstr(pVM, pCtx, pc, prefix)
#  define CPUMR3DisasmBlock(pVM, pCtx, pc, prefix, nrInstructions)
# endif

#endif /* IN_RING3 */

/** @} */
__END_DECLS


#endif





