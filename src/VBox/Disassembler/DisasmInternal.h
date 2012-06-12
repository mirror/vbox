/* $Id$ */
/** @file
 * VBox disassembler - Internal header.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___DisasmInternal_h___
#define ___DisasmInternal_h___

#include <VBox/types.h>
#include <VBox/dis.h>

#define ExceptionMemRead          0x666
#define ExceptionInvalidModRM     0x667
#define ExceptionInvalidParameter 0x668

#define IDX_ParseNop                0
#define IDX_ParseModRM              1
#define IDX_UseModRM                2
#define IDX_ParseImmByte            3
#define IDX_ParseImmBRel            4
#define IDX_ParseImmUshort          5
#define IDX_ParseImmV               6
#define IDX_ParseImmVRel            7
#define IDX_ParseImmAddr            8
#define IDX_ParseFixedReg           9
#define IDX_ParseImmUlong           10
#define IDX_ParseImmQword           11
#define IDX_ParseTwoByteEsc         12
#define IDX_ParseImmGrpl            13
#define IDX_ParseShiftGrp2          14
#define IDX_ParseGrp3               15
#define IDX_ParseGrp4               16
#define IDX_ParseGrp5               17
#define IDX_Parse3DNow              18
#define IDX_ParseGrp6               19
#define IDX_ParseGrp7               20
#define IDX_ParseGrp8               21
#define IDX_ParseGrp9               22
#define IDX_ParseGrp10              23
#define IDX_ParseGrp12              24
#define IDX_ParseGrp13              25
#define IDX_ParseGrp14              26
#define IDX_ParseGrp15              27
#define IDX_ParseGrp16              28
#define IDX_ParseModFence           29
#define IDX_ParseYv                 30
#define IDX_ParseYb                 31
#define IDX_ParseXv                 32
#define IDX_ParseXb                 33
#define IDX_ParseEscFP              34
#define IDX_ParseNopPause           35
#define IDX_ParseImmByteSX          36
#define IDX_ParseImmZ               37
#define IDX_ParseThreeByteEsc4      38
#define IDX_ParseThreeByteEsc5      39
#define IDX_ParseImmAddrF           40
#define IDX_ParseMax                (IDX_ParseImmAddrF+1)


extern PFNDISPARSE  g_apfnFullDisasm[IDX_ParseMax];
extern PFNDISPARSE  g_apfnCalcSize[IDX_ParseMax];

unsigned ParseInstruction(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, PDISCPUSTATE pCpu);
void disValidateLockSequence(PDISCPUSTATE pCpu);

/* Read functions */
DECLCALLBACK(int) disReadBytesDefault(PDISCPUSTATE pCpu, uint8_t *pbDst, RTUINTPTR uSrcAddr, uint32_t cbToRead);
uint8_t  DISReadByte(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint16_t DISReadWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint32_t DISReadDWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);
uint64_t DISReadQWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress);

size_t disFormatBytes(PCDISCPUSTATE pCpu, char *pszDst, size_t cchDst, uint32_t fFlags);

#endif

