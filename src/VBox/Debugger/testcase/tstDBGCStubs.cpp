/* $Id$ */
/** @file
 * DBGC Testcase - Command Parser, VMM Stub Functions.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/err.h>
#include <VBox/vmm/vm.h>
#include <iprt/string.h>



#include <VBox/vmm/dbgf.h>
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromFlat(PVM pVM, PDBGFADDRESS pAddress, RTGCUINTPTR FlatPtr)
{
    return NULL;
}

VMMR3DECL(int) DBGFR3AddrFromSelOff(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int)  DBGFR3AddrToPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTGCPHYS pGCPhys)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int)  DBGFR3AddrToHostPhys(PVMCPU pVCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int)  DBGFR3AddrToVolatileR3Ptr(PVMCPU pVCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) DBGFR3Attach(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) DBGFR3BpClear(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpDisable(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpEnable(PVM pVM, RTUINT iBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpEnum(PVM pVM, PFNDBGFBPENUM pfnCallback, void *pvUser)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpSet(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpSetReg(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                              uint8_t fType, uint8_t cb, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3BpSetREM(PVM pVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, PRTUINT piBp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(bool) DBGFR3CanWait(PVM pVM)
{
    return true;
}
VMMR3DECL(int) DBGFR3Detach(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3DisasInstrEx(PVM pVM, VMCPUID idCpu, RTSEL Sel, RTGCPTR GCPtr, uint32_t fFlags,
                                  char *pszOutput, uint32_t cchOutput, uint32_t *pcbInstr)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3EventWait(PVM pVM, RTMSINTERVAL cMillies, PCDBGFEVENT *ppEvent)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3Halt(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3Info(PVM pVM, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3InfoEx(PVM pVM, VMCPUID idCpu, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(bool) DBGFR3IsHalted(PVM pVM)
{
    return true;
}
VMMR3DECL(int) DBGFR3LineByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFLINE pLine)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3LogModifyDestinations(PVM pVM, const char *pszDestSettings)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3LogModifyFlags(PVM pVM, const char *pszFlagSettings)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3LogModifyGroups(PVM pVM, const char *pszGroupSettings)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3ModuleLoad(PVM pVM, const char *pszFilename, RTGCUINTPTR AddressDelta, const char *pszName, RTGCUINTPTR ModuleAddress, unsigned cbImage)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3AsLoadImage(PVM pVM, RTDBGAS hAS, const char *pszFilename, const char *pszModName, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, uint32_t fFlags)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3AsLoadMap(PVM pVM, RTDBGAS hAS, const char *pszFilename, const char *pszModName, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, RTGCUINTPTR uSubtrahend, uint32_t fFlags)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(RTDBGAS) DBGFR3AsResolveAndRetain(PVM pVM, RTDBGAS hAlias)
{
    return NIL_RTDBGAS;
}
VMMR3DECL(int) DBGFR3Resume(PVM pVM)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3StackWalkBegin(PVM pVM, VMCPUID idCpu, DBGFCODETYPE enmCodeType, PCDBGFSTACKFRAME *ppFirstFrame)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(PCDBGFSTACKFRAME) DBGFR3StackWalkNext(PCDBGFSTACKFRAME pCurrent)
{
    return NULL;
}
VMMR3DECL(void) DBGFR3StackWalkEnd(PCDBGFSTACKFRAME pFirstFrame)
{
}
VMMR3DECL(int) DBGFR3Step(PVM pVM, VMCPUID idCpu)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3AsSymbolByAddr(PVM pVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress, PRTGCINTPTR poffDisplacement, PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3AsSymbolByName(PVM pVM, RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3MemScan(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, RTGCUINTPTR uAlign, const void *pabNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3MemRead(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3MemReadString(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, char *pszBuf, size_t cchBuf)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3MemWrite(PVM pVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, const void *pvBuf, size_t cbRead)
{
    return VERR_INTERNAL_ERROR;
}
VMMDECL(int) DBGFR3PagingDumpEx(PVM pVM, VMCPUID idCpu, uint32_t fFlags, uint64_t cr3, uint64_t u64FirstAddr,
                                uint64_t u64LastAddr, uint32_t cMaxDepth, PCDBGFINFOHLP pHlp)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegCpuQueryU8(  PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint8_t     *pu8)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegCpuQueryU16( PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint16_t    *pu16)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegCpuQueryU32( PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint32_t    *pu32)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegCpuQueryU64( PVM pVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t    *pu64)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegNmQuery(PVM pVM, VMCPUID idDefCpu, const char *pszReg, PDBGFREGVAL pValue, PDBGFREGVALTYPE penmType)
{
    if (idDefCpu == 0 || idDefCpu == DBGFREG_HYPER_VMCPUID)
    {
        if (!strcmp(pszReg, "ah"))
        {
            pValue->u16 = 0xf0;
            *penmType   = DBGFREGVALTYPE_U8;
            return VINF_SUCCESS;
        }
        if (!strcmp(pszReg, "ax"))
        {
            pValue->u16 = 0xbabe;
            *penmType   = DBGFREGVALTYPE_U16;
            return VINF_SUCCESS;
        }
        if (!strcmp(pszReg, "eax"))
        {
            pValue->u32 = 0xcafebabe;
            *penmType   = DBGFREGVALTYPE_U32;
            return VINF_SUCCESS;
        }
        if (!strcmp(pszReg, "rax"))
        {
            pValue->u64 = UINT64_C(0x00beef00feedface);
            *penmType   = DBGFREGVALTYPE_U32;
            return VINF_SUCCESS;
        }
    }
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegPrintf(PVM pVM, VMCPUID idCpu, char *pszBuf, size_t cbBuf, const char *pszFormat, ...)
{
    return VERR_INTERNAL_ERROR;
}
VMMDECL(ssize_t) DBGFR3RegFormatValue(char *pszBuf, size_t cbBuf, PCDBGFREGVAL pValue, DBGFREGVALTYPE enmType, bool fSpecial)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3RegNmSet(PVM pVM, VMCPUID idDefCpu, const char *pszReg, PCDBGFREGVAL pValue, DBGFREGVALTYPE enmType)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromPhys(PVM pVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr)
{
    return NULL;
}
VMMR3DECL(int)  DBGFR3AddrToHostPhys(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int)  DBGFR3AddrToVolatileR3Ptr(PVM pVM, VMCPUID idCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) DBGFR3OSRegister(PVM pVM, PCDBGFOSREG pReg)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3OSDetect(PVM pVM, char *pszName, size_t cchName)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) DBGFR3OSQueryNameAndVersion(PVM pVM, char *pszName, size_t cchName, char *pszVersion, size_t cchVersion)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) DBGFR3SelQueryInfo(PVM pVM, VMCPUID idCpu, RTSEL Sel, uint32_t fFlags, PDBGFSELINFO pSelInfo)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, const char *pszFilename, bool fReplaceFile)
{
    return VERR_INTERNAL_ERROR;
}


//////////////////////////////////////////////////////////////////////////
// The rest should eventually be replaced by DBGF calls and eliminated. //
/////////////////////////////////////////////////////////////////////////

#include <VBox/vmm/cpum.h>

VMMDECL(uint64_t) CPUMGetGuestCR3(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(uint64_t) CPUMGetGuestCR4(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(RTSEL) CPUMGetGuestCS(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVMCPU pVCpu)
{
    return NULL;
}

VMMDECL(uint32_t) CPUMGetGuestEIP(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(uint64_t) CPUMGetGuestRIP(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(RTGCPTR) CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit)
{
    return 0;
}

VMMDECL(CPUMMODE) CPUMGetGuestMode(PVMCPU pVCpu)
{
    return CPUMMODE_INVALID;
}

VMMDECL(RTSEL) CPUMGetHyperCS(PVMCPU pVCpu)
{
    return 0xfff8;
}

VMMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVMCPU pVCpu)
{
    return NULL;
}

VMMDECL(uint32_t) CPUMGetHyperEIP(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(PCPUMCTX) CPUMQueryGuestCtxPtr(PVMCPU pVCpu)
{
    return NULL;
}

VMMDECL(int) CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx)
{
    return VERR_INTERNAL_ERROR;
}



#include <VBox/vmm/mm.h>

VMMR3DECL(int) MMR3HCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    return VERR_INTERNAL_ERROR;
}




#include <VBox/vmm/pgm.h>

VMMDECL(RTHCPHYS) PGMGetHyperCR3(PVMCPU pVCpu)
{
    return 0;
}

VMMDECL(PGMMODE) PGMGetShadowMode(PVMCPU pVCpu)
{
    return PGMMODE_INVALID;
}

VMMR3DECL(int) PGMR3DbgR3Ptr2GCPhys(PVM pVM, RTR3PTR R3Ptr, PRTGCPHYS pGCPhys)
{
    return VERR_INTERNAL_ERROR;
}

VMMR3DECL(int) PGMR3DbgR3Ptr2HCPhys(PVM pVM, RTR3PTR R3Ptr, PRTHCPHYS pHCPhys)
{
    return VERR_INTERNAL_ERROR;
}
VMMR3DECL(int) PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys)
{
    return VERR_INTERNAL_ERROR;
}


#include <VBox/vmm/vmm.h>

VMMDECL(PVMCPU) VMMGetCpuById(PVM pVM, RTCPUID idCpu)
{
    return NULL;
}

