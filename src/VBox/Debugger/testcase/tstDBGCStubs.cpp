/* $Id$ */
/** @file
 * DBGC Testcase - Command Parser, VMM Stub Functions.
 */

/*
 * Copyright (C) 2007 knut st. osmundsen <bird-kStuff-spam@anduin.net>
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/err.h>
#include <VBox/cpum.h>

CPUMDECL(uint64_t) CPUMGetGuestCR3(PVM pVM)
{
    return 0;
}

CPUMDECL(uint64_t) CPUMGetGuestCR4(PVM pVM)
{
    return 0;
}

CPUMDECL(RTSEL) CPUMGetGuestCS(PVM pVM)
{
    return 0;
}

CPUMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVM pVM)
{
    return NULL;
}

CPUMDECL(uint32_t) CPUMGetGuestEIP(PVM pVM)
{
    return 0;
}

CPUMDECL(uint64_t) CPUMGetGuestRIP(PVM pVM)
{
    return 0;
}

CPUMDECL(RTGCPTR) CPUMGetGuestIDTR(PVM pVM, uint16_t *pcbLimit)
{
    return 0;
}

CPUMDECL(CPUMMODE) CPUMGetGuestMode(PVM pVM)
{
    return CPUMMODE_INVALID;
}

CPUMDECL(RTSEL) CPUMGetHyperCS(PVM pVM)
{
    return 0xfff8;
}

CPUMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVM pVM)
{
    return NULL;
}

CPUMDECL(uint32_t) CPUMGetHyperEIP(PVM pVM)
{
    return 0;
}

CPUMDECL(int) CPUMQueryGuestCtxPtr(PVM pVM, PCPUMCTX *ppCtx)
{
    return VERR_INTERNAL_ERROR;
}

CPUMDECL(int) CPUMQueryHyperCtxPtr(PVM pVM, PCPUMCTX *ppCtx)
{
    return VERR_INTERNAL_ERROR;
}


#include <VBox/mm.h>

MMR3DECL(int) MMR3HCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    return VERR_INTERNAL_ERROR;
}

MMR3DECL(int) MMR3ReadGCVirt(PVM pVM, void *pvDst, RTGCPTR GCPtr, size_t cb)
{
    return VERR_INTERNAL_ERROR;
}


#include <VBox/selm.h>

SELMR3DECL(int) SELMR3GetSelectorInfo(PVM pVM, RTSEL Sel, PSELMSELINFO pSelInfo)
{
    return VERR_INTERNAL_ERROR;
}


#include <VBox/pgm.h>

PGMDECL(RTHCPHYS) PGMGetHyperCR3(PVM pVM)
{
    return 0;
}

PGMDECL(PGMMODE) PGMGetShadowMode(PVM pVM)
{
    return PGMMODE_INVALID;
}

PGMDECL(int) PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}

PGMDECL(int) PGMPhysGCPhys2HCPtr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR pHCPtr)
{
    return VERR_INTERNAL_ERROR;
}

PGMDECL(int) PGMPhysGCPtr2GCPhys(PVM pVM, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    return VERR_INTERNAL_ERROR;
}

PGMDECL(int) PGMPhysGCPtr2HCPhys(PVM pVM, RTGCPTR GCPtr, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}

PGMDECL(int) PGMPhysGCPtr2HCPtr(PVM pVM, RTGCPTR GCPtr, PRTHCPTR pHCPtr)
{
    return VERR_INTERNAL_ERROR;
}

PGMDECL(int) PGMPhysReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb)
{
    return VERR_INTERNAL_ERROR;
}

PGMR3DECL(int) PGMR3DbgHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys)
{
    return VERR_INTERNAL_ERROR;
}

PGMR3DECL(int) PGMR3DbgHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}


#include <VBox/dbgf.h>
DBGFR3DECL(PDBGFADDRESS) DBGFR3AddrFromFlat(PVM pVM, PDBGFADDRESS pAddress, RTGCUINTPTR FlatPtr)
{
    return NULL;
}

DBGFR3DECL(int) DBGFR3AddrFromSelOff(PVM pVM, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off)
{
    return VERR_INTERNAL_ERROR;
}

DBGFR3DECL(int) DBGFR3Attach(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}

DBGFR3DECL(int) DBGFR3BpClear(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpDisable(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpEnable(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpSet(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                               uint8_t fType, uint8_t cb, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(bool) DBGFR3CanWait(PVM pVM)
{
    return true;
}
DBGFR3DECL(int) DBGFR3Detach(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3DisasInstrEx(PVM pVM, RTSEL Sel, RTGCPTR GCPtr, unsigned fFlags, char *pszOutput, uint32_t cchOutput, uint32_t *pcbInstr)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3EventWait(PVM pVM, unsigned cMillies, PCDBGFEVENT *ppEvent)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3Halt(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3Info(PVM pVM, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(bool) DBGFR3IsHalted(PVM pVM)
{
    return true;
}
DBGFR3DECL(int) DBGFR3LineByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFLINE pLine)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3ModuleLoad(PVM pVM, const char *pszFilename, RTGCUINTPTR AddressDelta, const char *pszName, RTGCUINTPTR ModuleAddress, unsigned cbImage)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3Resume(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3StackWalkBeginGuest(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3StackWalkBeginHyper(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3StackWalkNext(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3Step(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3SymbolByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFSYMBOL pSymbol)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3SymbolByName(PVM pVM, const char *pszSymbol, PDBGFSYMBOL pSymbol)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3MemScan(PVM pVM, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3MemRead(PVM pVM, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3MemReadString(PVM pVM, PCDBGFADDRESS pAddress, char *pszBuf, size_t cchBuf)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(void) DBGFR3AddrFromPhys(PVM pVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr)
{
}
DBGFR3DECL(int) DBGFR3OSRegister(PVM pVM, PCDBGFOSREG pReg)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3OSDetect(PVM pVM, char *pszName, size_t cchName)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3OSQueryNameAndVersion(PVM pVM, char *pszName, size_t cchName, char *pszVersion, size_t cchVersion)
{
    return VERR_INTERNAL_ERROR;
}
DBGFR3DECL(int) DBGFR3SymbolAdd(PVM pVM, RTGCUINTPTR ModuleAddress, RTGCUINTPTR SymbolAddress, RTUINT cbSymbol, const char *pszSymbol)
{
    return VERR_INTERNAL_ERROR;
}
