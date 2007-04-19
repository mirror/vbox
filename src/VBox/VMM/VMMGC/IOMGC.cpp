/* $Id$ */
/** @file
 * IOM - Input / Output Monitor - Guest Context.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_IOM
#include <VBox/iom.h>
#include <VBox/cpum.h>
#include <VBox/pgm.h>
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include "IOMInternal.h"
#include <VBox/vm.h>

#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>



/**
 * Attempts to service an IN/OUT instruction.
 *
 * The \#GP trap handler in GC will call this function if the opcode causing the
 * trap is a in or out type instruction.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMGCDECL(int) IOMGCIOPortHandler(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu)
{
    switch (pCpu->pCurInstr->opcode)
    {
        case OP_IN:
            return IOMInterpretIN(pVM, pRegFrame, pCpu);

        case OP_OUT:
            return IOMInterpretOUT(pVM, pRegFrame, pCpu);

        case OP_INSB:
        case OP_INSWD:
            return IOMInterpretINS(pVM, pRegFrame, pCpu);

        case OP_OUTSB:
        case OP_OUTSWD:
            return IOMInterpretOUTS(pVM, pRegFrame, pCpu);

        /*
         * The opcode wasn't know to us, freak out.
         */
        default:
            AssertMsgFailed(("Unknown I/O port access opcode %d.\n", pCpu->pCurInstr->opcode));
            return VERR_INTERNAL_ERROR;
    }
}

