/* $Id$ */
/** @file
 * VBoxCpuReport internal header file.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

typedef struct VBMSRFNS {
   int (*msrRead)(uint32_t uMsr, RTCPUID idCpu, uint64_t *puValue, bool *pfGp);
   int (*msrWrite)(uint32_t uMsr, RTCPUID idCpu, uint64_t uValue, bool *pfGp);
   int (*msrModify)(uint32_t uMsr, RTCPUID idCpu, uint64_t fAndMask, uint64_t fOrMask, PSUPMSRPROBERMODIFYRESULT pResult);
   int (*msrProberTerm)(void);
} VBMSRFNS;

extern void vbCpuRepDebug(const char *pszMsg, ...);
extern void vbCpuRepPrintf(const char *pszMsg, ...);
extern int SupDrvMsrProberInit(VBMSRFNS *fnsMsr, bool *pfAtomicMsrMod);
extern int PlatformMsrProberInit(VBMSRFNS *fnsMsr, bool *pfAtomicMsrMod);

