/** @file
 * IOM - Input / Output Monitor.
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

#ifndef ___VBox_iom_h
#define ___VBox_iom_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/dis.h>

__BEGIN_DECLS


/** @defgroup grp_iom   The Input / Ouput Monitor API
 * @{
 */

/** @def IOM_NO_PDMINS_CHECKS
 * Untill all devices have been fully adjusted to PDM style, the pPdmIns parameter
 * is not checked by IOM.
 */
#define IOM_NO_PDMINS_CHECKS

/**
 * Macro for checking if an I/O or MMIO emulation call succeeded.
 *
 * This macro shall only be used with the IOM APIs where it's mentioned
 * in the return value description. And there is must be used to correctly
 * determin if the call succeeded and things like the EIP needs updating.
 *
 *
 * @returns Success indicator (true/false).
 *
 * @param   rc          The status code. This may be evaluated
 *                      more than once!
 *
 * @remark  To avoid making assumptions about the layout of the
 *          VINF_EM_FIRST...VINF_EM_LAST range we're checking
 *          explicitly for each for exach the exceptions.
 *          However, for efficieny we ASSUME that the
 *          VINF_EM_LAST is smaller than most of the relevant
 *          status codes. We also ASSUME that the
 *          VINF_EM_RESCHEDULE_REM status code is the most
 *          frequent status code we'll enounter in this range.
 *
 * @todo    Will have to add VINF_EM_DBG_HYPER_BREAKPOINT if the
 *          I/O port and MMIO breakpoints should trigger before
 *          the I/O is done. Currently, we don't implement these
 *          kind of breakpoints.
 */
#define IOM_SUCCESS(rc)     (   (rc) == VINF_SUCCESS \
                             || (   (rc) <= VINF_EM_LAST \
                                 && (rc) != VINF_EM_RESCHEDULE_REM \
                                 && (rc) >= VINF_EM_FIRST \
                                 && (rc) != VINF_EM_RESCHEDULE_RAW \
                                 && (rc) != VINF_EM_RESCHEDULE_HWACC \
                                ) \
                            )


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VINF_SUCCESS or VINF_EM_*.
 * @returns VERR_IOM_IOPORT_UNUSED if the port is really unused and a ~0 value should be returned.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
typedef DECLCALLBACK(int) FNIOMIOPORTIN(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
/** Pointer to a FNIOMIOPORTIN(). */
typedef FNIOMIOPORTIN *PFNIOMIOPORTIN;

/**
 * Port I/O Handler for string IN operations.
 *
 * @returns VINF_SUCCESS or VINF_EM_*.
 * @returns VERR_IOM_IOPORT_UNUSED if the port is really unused and a ~0 value should be returned.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pGCPtrDst   Pointer to the destination buffer (GC, incremented appropriately).
 * @param   pcTransfers Pointer to the number of transfer units to read, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
typedef DECLCALLBACK(int) FNIOMIOPORTINSTRING(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, unsigned *pcTransfers, unsigned cb);
/** Pointer to a FNIOMIOPORTINSTRING(). */
typedef FNIOMIOPORTINSTRING *PFNIOMIOPORTINSTRING;

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VINF_SUCCESS or VINF_EM_*.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
typedef DECLCALLBACK(int) FNIOMIOPORTOUT(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
/** Pointer to a FNIOMIOPORTOUT(). */
typedef FNIOMIOPORTOUT *PFNIOMIOPORTOUT;

/**
 * Port I/O Handler for string OUT operations.
 *
 * @returns VINF_SUCCESS or VINF_EM_*.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the OUT operation.
 * @param   pGCPtrSrc   Pointer to the source buffer (GC, incremented appropriately).
 * @param   pcTransfers Pointer to the number of transfer units to write, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
typedef DECLCALLBACK(int) FNIOMIOPORTOUTSTRING(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, unsigned *pcTransfers, unsigned cb);
/** Pointer to a FNIOMIOPORTOUTSTRING(). */
typedef FNIOMIOPORTOUTSTRING *PFNIOMIOPORTOUTSTRING;


/**
 * Memory mapped I/O Handler for read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 *
 * @remark wonder if we could merge the IOMMMIO* and IOMPORT* callbacks...
 */
typedef DECLCALLBACK(int) FNIOMMMIOREAD(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
/** Pointer to a FNIOMMMIOREAD(). */
typedef FNIOMMMIOREAD *PFNIOMMMIOREAD;

/**
 * Port I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 *
 * @remark wonder if we could merge the IOMMMIO* and IOMPORT* callbacks...
 */
typedef DECLCALLBACK(int) FNIOMMMIOWRITE(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
/** Pointer to a FNIOMMMIOWRITE(). */
typedef FNIOMMMIOWRITE *PFNIOMMMIOWRITE;

/**
 * Port I/O Handler for memset operations, actually for REP STOS* instructions handling.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the write starts.
 * @param   u32Item     Byte/Word/Dword data to fill.
 * @param   cbItem      Size of data in u32Item parameter, restricted to 1/2/4 bytes.
 * @param   cItems      Number of iterations.
 */
typedef DECLCALLBACK(int) FNIOMMMIOFILL(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, uint32_t u32Item, unsigned cbItem, unsigned cItems);
/** Pointer to a FNIOMMMIOFILL(). */
typedef FNIOMMMIOFILL *PFNIOMMMIOFILL;

IOMDECL(int)  IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);
IOMDECL(int)  IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue);
IOMDECL(int)  IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
IOMDECL(int)  IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
IOMDECL(int)  IOMIOPortReadString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb);
IOMDECL(int)  IOMIOPortWriteString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb);
IOMDECL(int)  IOMInterpretINS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
IOMDECL(int)  IOMInterpretINSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer);
IOMDECL(int)  IOMInterpretOUTS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
IOMDECL(int)  IOMInterpretOUTSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer);
IOMDECL(int)  IOMMMIORead(PVM pVM, RTGCPHYS GCPhys, uint32_t *pu32Value, size_t cbValue);
IOMDECL(int)  IOMMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue);
IOMDECL(int)  IOMInterpretCheckPortIOAccess(PVM pVM, PCPUMCTXCORE pCtxCore, RTIOPORT Port, unsigned cb);


#ifdef IN_GC
/** @defgroup grp_iom_gc    The IOM Guest Context API
 * @ingroup grp_iom
 * @{
 */
IOMGCDECL(int) IOMGCIOPortHandler(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
/** @} */
#endif /* IN_GC */



#ifdef IN_RING3
/** @defgroup grp_iom_r3    The IOM Host Context Ring-3 API
 * @ingroup grp_iom
 * @{
 */
IOMR3DECL(int)  IOMR3Init(PVM pVM);
IOMR3DECL(void) IOMR3Reset(PVM pVM);
IOMR3DECL(void) IOMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
IOMR3DECL(int)  IOMR3Term(PVM pVM);
IOMR3DECL(int)  IOMR3IOPortRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
                                      R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                      R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback, R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStringCallback,
                                      const char *pszDesc);
IOMR3DECL(int)  IOMR3IOPortRegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTGCPTR pvUser,
                                      GCPTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, GCPTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                      GCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, GCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                      const char *pszDesc);
IOMR3DECL(int)  IOMR3IOPortRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTR0PTR pvUser,
                                      R0PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R0PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                      R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                      const char *pszDesc);
IOMR3DECL(int)  IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts);

IOMR3DECL(int)  IOMR3MMIORegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                    R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R3PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    R3PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);
IOMR3DECL(int)  IOMR3MMIORegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                    R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R0PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    R0PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);
IOMR3DECL(int)  IOMR3MMIORegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                    GCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, GCPTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    GCPTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);
IOMR3DECL(int)  IOMR3MMIODeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange);
/** @} */
#endif /* IN_RING3 */


/** @} */

__END_DECLS

#endif

