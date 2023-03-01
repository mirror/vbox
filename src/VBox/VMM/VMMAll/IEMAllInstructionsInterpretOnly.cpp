/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef LOG_GROUP /* defined when included by tstIEMCheckMc.cpp */
# define LOG_GROUP LOG_GROUP_IEM
#endif
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/gim.h>
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# include <VBox/vmm/em.h>
# include <VBox/vmm/hm_svm.h>
#endif
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
# include <VBox/vmm/hmvmxinline.h>
#endif
#include <VBox/vmm/tm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgftrace.h>
#ifndef TST_IEM_CHECK_MC
# include "IEMInternal.h"
#endif
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/x86.h>

#ifndef TST_IEM_CHECK_MC
# include "IEMInline.h"
# include "IEMOpHlp.h"
# include "IEMMc.h"
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define g_apfnOneByteMap    g_apfnIemInterpretOnlyOneByteMap



/*
 * Include common bits.
 */
#include "IEMAllInstructionsCommon.cpp.h"


/*
 * Include the tables.
 */
#ifdef IEM_WITH_3DNOW
# include "IEMAllInstructions3DNow.cpp.h"
#endif

#ifdef IEM_WITH_THREE_0F_38
# include "IEMAllInstructionsThree0f38.cpp.h"
#endif

#ifdef IEM_WITH_THREE_0F_3A
# include "IEMAllInstructionsThree0f3a.cpp.h"
#endif

#include "IEMAllInstructionsTwoByte0f.cpp.h"

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4702) /* Unreachable code like return after IEM_MC_ADVANCE_RIP_AND_FINISH. */
#endif

#ifdef IEM_WITH_VEX
# include "IEMAllInstructionsVexMap1.cpp.h"
# include "IEMAllInstructionsVexMap2.cpp.h"
# include "IEMAllInstructionsVexMap3.cpp.h"
#endif

#include "IEMAllInstructionsOneByte.cpp.h"
#ifdef _MSC_VER
# pragma warning(pop)
#endif

