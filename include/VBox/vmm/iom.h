/** @file
 * IOM - Input / Output Monitor.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef ___VBox_vmm_iom_h
#define ___VBox_vmm_iom_h

#include <VBox/types.h>
#include <VBox/dis.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_iom   The Input / Ouput Monitor API
 * @{
 */

/** @def IOM_NO_PDMINS_CHECKS
 * Until all devices have been fully adjusted to PDM style, the pPdmIns
 * parameter is not checked by IOM.
 * @todo Check this again, now.
 */
#define IOM_NO_PDMINS_CHECKS

/**
 * Macro for checking if an I/O or MMIO emulation call succeeded.
 *
 * This macro shall only be used with the IOM APIs where it's mentioned
 * in the return value description. And there is must be used to correctly
 * determine if the call succeeded and things like the EIP needs updating.
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

/** @name IOMMMIO_FLAGS_XXX
 * @{ */
/** Pass all reads thru unmodified. */
#define IOMMMIO_FLAGS_READ_PASSTHRU                     UINT32_C(0x00000000)
/** All read accesses are DWORD sized (32-bit). */
#define IOMMMIO_FLAGS_READ_DWORD                        UINT32_C(0x00000001)
/** All read accesses are DWORD (32-bit) or QWORD (64-bit) sized.
 * Only accesses that are both QWORD sized and aligned are performed as QWORD.
 * All other access will be done DWORD fashion (because it is way simpler). */
#define IOMMMIO_FLAGS_READ_DWORD_QWORD                  UINT32_C(0x00000002)
/** The read access mode mask. */
#define IOMMMIO_FLAGS_READ_MODE                         UINT32_C(0x00000003)

/** Pass all writes thru unmodified. */
#define IOMMMIO_FLAGS_WRITE_PASSTHRU                    UINT32_C(0x00000000)
/** All write accesses are DWORD (32-bit) sized and unspecified bytes are
 * written as zero. */
#define IOMMMIO_FLAGS_WRITE_DWORD_ZEROED                UINT32_C(0x00000010)
/** All write accesses are either DWORD (32-bit) or QWORD (64-bit) sized,
 * missing bytes will be written as zero.  Only accesses that are both QWORD
 * sized and aligned are performed as QWORD, all other accesses will be done
 * DWORD fashion (because it's way simpler). */
#define IOMMMIO_FLAGS_WRITE_DWORD_QWORD_ZEROED          UINT32_C(0x00000020)
/** All write accesses are DWORD (32-bit) sized and unspecified bytes are
 * read from the device first as DWORDs.
 * @remarks This isn't how it happens on real hardware, but it allows
 *          simplifications of devices where reads doesn't change the device
 *          state in any way. */
#define IOMMMIO_FLAGS_WRITE_DWORD_READ_MISSING          UINT32_C(0x00000030)
/** All write accesses are DWORD (32-bit) or QWORD (64-bit) sized and
 * unspecified bytes are read from the device first as DWORDs.  Only accesses
 * that are both QWORD sized and aligned are performed as QWORD, all other
 * accesses will be done DWORD fashion (because it's way simpler).
 * @remarks This isn't how it happens on real hardware, but it allows
 *          simplifications of devices where reads doesn't change the device
 *          state in any way. */
#define IOMMMIO_FLAGS_WRITE_DWORD_QWORD_READ_MISSING    UINT32_C(0x00000040)
/** The read access mode mask. */
#define IOMMMIO_FLAGS_WRITE_MODE                        UINT32_C(0x00000070)

/** Whether to do a DBGSTOP on complicated reads.
 * What this includes depends on the read mode, but generally all misaligned
 * reads as well as word and byte reads and maybe qword reads. */
#define IOMMMIO_FLAGS_DBGSTOP_ON_COMPLICATED_READ       UINT32_C(0x00000100)
/** Whether to do a DBGSTOP on complicated writes.
 * This depends on the write mode, but generally all writes where we have to
 * supply bytes (zero them or read them). */
#define IOMMMIO_FLAGS_DBGSTOP_ON_COMPLICATED_WRITE      UINT32_C(0x00000200)

/** Mask of valid flags. */
#define IOMMMIO_FLAGS_VALID_MASK                        UINT32_C(0x00000373)
/** @} */


/**
 * Port I/O Handler for IN operations.
 *
 * @returns VINF_SUCCESS or VINF_EM_*.
 * @returns VERR_IOM_IOPORT_UNUSED if the port is really unused and a ~0 value should be returned.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.  This is always a 32-bit
 *                      variable regardless of what @a cb might say.
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
typedef DECLCALLBACK(int) FNIOMIOPORTINSTRING(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb);
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
typedef DECLCALLBACK(int) FNIOMIOPORTOUTSTRING(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb);
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
 */
typedef DECLCALLBACK(int) FNIOMMMIOWRITE(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb);
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

VMMDECL(VBOXSTRICTRC)   IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);
VMMDECL(VBOXSTRICTRC)   IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue);
VMMDECL(VBOXSTRICTRC)   IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
VMMDECL(VBOXSTRICTRC)   IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
VMMDECL(VBOXSTRICTRC)   IOMIOPortReadString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb);
VMMDECL(VBOXSTRICTRC)   IOMIOPortWriteString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb);
VMMDECL(VBOXSTRICTRC)   IOMInterpretINS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
VMMDECL(VBOXSTRICTRC)   IOMInterpretINSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, DISCPUMODE enmAddrMode, uint32_t cbTransfer);
VMMDECL(VBOXSTRICTRC)   IOMInterpretOUTS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
VMMDECL(VBOXSTRICTRC)   IOMInterpretOUTSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, DISCPUMODE enmAddrMode, uint32_t cbTransfer);
VMMDECL(VBOXSTRICTRC)   IOMMMIORead(PVM pVM, RTGCPHYS GCPhys, uint32_t *pu32Value, size_t cbValue);
VMMDECL(VBOXSTRICTRC)   IOMMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue);
VMMDECL(VBOXSTRICTRC)   IOMMMIOPhysHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pCtxCore, RTGCPHYS GCPhysFault);
VMMDECL(VBOXSTRICTRC)   IOMInterpretCheckPortIOAccess(PVM pVM, PCPUMCTXCORE pCtxCore, RTIOPORT Port, unsigned cb);
VMMDECL(int)            IOMMMIOMapMMIO2Page(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysRemapped, uint64_t fPageFlags);
VMMDECL(int)            IOMMMIOMapMMIOHCPage(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint64_t fPageFlags);
VMMDECL(int)            IOMMMIOResetRegion(PVM pVM, RTGCPHYS GCPhys);
VMMDECL(bool)           IOMIsLockOwner(PVM pVM);

#ifdef IN_RC
/** @defgroup grp_iom_gc    The IOM Guest Context API
 * @ingroup grp_iom
 * @{
 */
VMMRCDECL(VBOXSTRICTRC) IOMGCIOPortHandler(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);
/** @} */
#endif /* IN_RC */



#ifdef IN_RING3
/** @defgroup grp_iom_r3    The IOM Host Context Ring-3 API
 * @ingroup grp_iom
 * @{
 */
VMMR3_INT_DECL(int)  IOMR3Init(PVM pVM);
VMMR3_INT_DECL(void) IOMR3Reset(PVM pVM);
VMMR3_INT_DECL(void) IOMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3_INT_DECL(int)  IOMR3Term(PVM pVM);
VMMR3_INT_DECL(int)  IOMR3IOPortRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
                                           R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                           R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback, R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStringCallback,
                                           const char *pszDesc);
VMMR3_INT_DECL(int)  IOMR3IOPortRegisterRC(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTRCPTR pvUser,
                                           RCPTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, RCPTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                           RCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, RCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                           const char *pszDesc);
VMMR3_INT_DECL(int)  IOMR3IOPortRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTR0PTR pvUser,
                                           R0PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R0PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                           R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                           const char *pszDesc);
VMMR3_INT_DECL(int)  IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts);

VMMR3_INT_DECL(int)  IOMR3MmioRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, uint32_t cbRange, RTHCPTR pvUser,
                                         R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                                         R3PTRTYPE(PFNIOMMMIOREAD)  pfnReadCallback,
                                         R3PTRTYPE(PFNIOMMMIOFILL)  pfnFillCallback,
                                         uint32_t fFlags, const char *pszDesc);
VMMR3_INT_DECL(int)  IOMR3MmioRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, uint32_t cbRange, RTR0PTR pvUser,
                                         R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                                         R0PTRTYPE(PFNIOMMMIOREAD)  pfnReadCallback,
                                         R0PTRTYPE(PFNIOMMMIOFILL)  pfnFillCallback);
VMMR3_INT_DECL(int)  IOMR3MmioRegisterRC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, uint32_t cbRange, RTGCPTR pvUser,
                                         RCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback,
                                         RCPTRTYPE(PFNIOMMMIOREAD)  pfnReadCallback,
                                         RCPTRTYPE(PFNIOMMMIOFILL)  pfnFillCallback);
VMMR3_INT_DECL(int)  IOMR3MmioDeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, uint32_t cbRange);

/** @} */
#endif /* IN_RING3 */


/** @} */

RT_C_DECLS_END

#endif

