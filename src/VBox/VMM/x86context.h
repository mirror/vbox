/** @file
 *
 * VMM - Context switcher macros & definitions
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

#ifndef __X86CONTEXT_H__
#define __X86CONTEXT_H__

#include <VBox/types.h>

// Selector types (memory/system)
#define X86_SELTYPE_MEM_READONLY                0
#define X86_SELTYPE_MEM_READONLY_ACC            1
#define X86_SELTYPE_MEM_READWRITE               2
#define X86_SELTYPE_MEM_READWRITE_ACC           3
#define X86_SELTYPE_MEM_READONLY_EXPDOWN        4
#define X86_SELTYPE_MEM_READONLY_EXPDOWN_ACC    5
#define X86_SELTYPE_MEM_READWRITE_EXPDOWN       6
#define X86_SELTYPE_MEM_READWRITE_EXPDOWN_ACC   7
#define X86_SELTYPE_MEM_EXECUTEONLY             8
#define X86_SELTYPE_MEM_EXECUTEONLY_ACC         9
#define X86_SELTYPE_MEM_EXECUTEREAD             0xA
#define X86_SELTYPE_MEM_EXECUTEREAD_ACC         0xB
#define X86_SELTYPE_MEM_EXECUTEONLY_CONF        0xC
#define X86_SELTYPE_MEM_EXECUTEONLY_CONF_ACC    0xD
#define X86_SELTYPE_MEM_EXECUTEREAD_CONF        0xE
#define X86_SELTYPE_MEM_EXECUTEREAD_CONF_ACC    0xF

#define X86_SELTYPE_SYS_UNDEFINED               0
#define X86_SELTYPE_SYS_286_TSS_AVAIL           1
#define X86_SELTYPE_SYS_LDT                     2
#define X86_SELTYPE_SYS_286_TSS_BUSY            3
#define X86_SELTYPE_SYS_286_CALL_GATE           4
#define X86_SELTYPE_SYS_TASK_GATE               5
#define X86_SELTYPE_SYS_286_INT_GATE            6
#define X86_SELTYPE_SYS_286_TRAP_GATE           7
#define X86_SELTYPE_SYS_UNDEFINED2              8
#define X86_SELTYPE_SYS_386_TSS_AVAIL           9
#define X86_SELTYPE_SYS_UNDEFINED3              0xA
#define X86_SELTYPE_SYS_386_TSS_BUSY            0xB
#define X86_SELTYPE_SYS_386_CALL_GATE           0xC
#define X86_SELTYPE_SYS_UNDEFINED4              0xD
#define X86_SELTYPE_SYS_386_INT_GATE            0xE
#define X86_SELTYPE_SYS_386_TRAP_GATE           0xF


#endif //__X86CONTEXT_H__
