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



/**
 * Registers a Port IO GC handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register HC ranges
 * using IOMR3IOPortRegisterHC() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMIOPortRegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTGCPTR pvUser,
                                  GCPTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, GCPTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                  GCPTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, GCPTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                  const char *pszDesc);



/**
 * Registers a Memory Mapped I/O GC handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register HC ranges
 * using IOMMR3MIORegisterHC() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMMMIORegisterGC(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                GCPTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, GCPTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                GCPTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);


/**
 * Registers a Port IO R0 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMR3IOPortRegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in GC.
 * @param   pfnOutStrCallback   Pointer to function which is gonna handle OUT operations in GC.
 * @param   pfnInStrCallback    Pointer to function which is gonna handle IN operations in GC.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMIOPortRegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTR0PTR pvUser,
                                  R0PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R0PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                  R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStrCallback, R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStrCallback,
                                  const char *pszDesc);

/**
 * Registers a Memory Mapped I/O R0 handler range.
 *
 * This API is called by PDM on behalf of a device. Devices must first register ring-3 ranges
 * using IOMMR3MIORegisterR3() before calling this function.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMDECL(int)  IOMMMIORegisterR0(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                R0PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R0PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                R0PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);


/**
 * Reads an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to read.
 * @param   pu32Value   Where to store the value read.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMIOPortRead(PVM pVM, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);

/**
 * Writes to an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to write to.
 * @param   u32Value    The value to write.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue);

/**
 * OUT <DX|imm16>, <AL|AX|EAX>
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUT(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);

/**
 * IN <AL|AX|EAX>, <DX|imm16>
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretIN(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);


/**
 * Reads the string buffer of an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to read.
 * @param   pGCPtrDst   Pointer to the destination buffer (GC, incremented appropriately).
 * @param   pcTransfers Pointer to the number of transfer units to read, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
IOMDECL(int) IOMIOPortReadString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrDst, PRTGCUINTREG pcTransfers, unsigned cb);

/**
 * Writes the string buffer of an I/O port register.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success but schedulinging information needs to be passed onto EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 *
 * @param   pVM         VM handle.
 * @param   Port        The port to write.
 * @param   pGCPtrSrc   Pointer to the source buffer (GC, incremented appropriately).
 * @param   pcTransfer  Pointer to the number of transfer units to write, on return remaining transfer units.
 * @param   cb          Size of the transfer unit (1, 2 or 4 bytes).
 */
IOMDECL(int) IOMIOPortWriteString(PVM pVM, RTIOPORT Port, PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfers, unsigned cb);

/**
 * [REP*] INSB/INSW/INSD
 * ES:EDI,DX[,ECX]
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretINS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);

/**
 * [REP*] INSB/INSW/INSD
 * ES:EDI,DX[,ECX]
 *
 * @remark Assumes caller checked the access privileges (IOMInterpretCheckPortIOAccess)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the read to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM             The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame       Pointer to CPUMCTXCORE guest registers structure.
 * @param   uPort           IO Port
 * @param   uPrefix         IO instruction prefix
 * @param   cbTransfer      Size of transfer unit
 */
IOMDECL(int) IOMInterpretINSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer);

/**
 * [REP*] OUTSB/OUTSW/OUTSD
 * DS:ESI,DX[,ECX]
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the write to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMDECL(int) IOMInterpretOUTS(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);

/**
 * [REP*] OUTSB/OUTSW/OUTSD
 * DS:ESI,DX[,ECX]
 *
 * @remark  Assumes caller checked the access privileges (IOMInterpretCheckPortIOAccess)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_WRITE    Defer the write to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_EMULATE_INSTR   Defer the write to the REM.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM             The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame       Pointer to CPUMCTXCORE guest registers structure.
 * @param   uPort           IO Port
 * @param   uPrefix         IO instruction prefix
 * @param   cbTransfer      Size of transfer unit
 */
IOMDECL(int) IOMInterpretOUTSEx(PVM pVM, PCPUMCTXCORE pRegFrame, uint32_t uPort, uint32_t uPrefix, uint32_t cbTransfer);

/**
 * Flushes the IOM port & statistics lookup cache
 *
 * @param   pVM     The VM.
 */
IOMDECL(void) IOMFlushCache(PVM pVM);

/**
 * Reads a MMIO register.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address to read.
 * @param   pu32Value   Where to store the value read.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMMMIORead(PVM pVM, RTGCPHYS GCPhys, uint32_t *pu32Value, size_t cbValue);

/**
 * Writes to a MMIO register.
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      The physical address to write to.
 * @param   u32Value    The value to write.
 * @param   cbValue     The size of the register to read in bytes. 1, 2 or 4 bytes.
 */
IOMDECL(int) IOMMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue);


/**
 * Checks that the operation is allowed according to the IOPL
 * level and I/O bitmap.
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         VM handle.
 * @param   pCtxCore    Pointer to register frame.
 * @param   Port        The I/O port number.
 * @param   cb          The access size.
 */
IOMDECL(int) IOMInterpretCheckPortIOAccess(PVM pVM, PCPUMCTXCORE pCtxCore, RTIOPORT Port, unsigned cb);


#ifdef IN_GC
/** @defgroup grp_iom_gc    The IOM Guest Context API
 * @ingroup grp_iom
 * @{
 */

/**
 * Attempts to service an IN/OUT instruction.
 *
 * The \#GP trap handler in GC will call this function if the opcode causing the
 * trap is a in or out type instruction. (Call it indirectly via EM that is.)
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VINF_IOM_HC_IOPORT_READ     Defer the read to ring-3. (R0/GC only)
 * @retval  VINF_EM_RAW_GUEST_TRAP      The exception was left pending. (TRPMRaiseXcptErr)
 * @retval  VINF_TRPM_XCPT_DISPATCHED   The exception was raised and dispatched for raw-mode execution. (TRPMRaiseXcptErr)
 * @retval  VINF_EM_RESCHEDULE_REM      The exception was dispatched and cannot be executed in raw-mode. (TRPMRaiseXcptErr)
 *
 * @param   pVM         The virtual machine (GC pointer ofcourse).
 * @param   pRegFrame   Pointer to CPUMCTXCORE guest registers structure.
 * @param   pCpu        Disassembler CPU state.
 */
IOMGCDECL(int) IOMGCIOPortHandler(PVM pVM, PCPUMCTXCORE pRegFrame, PDISCPUSTATE pCpu);

/** @} */
#endif



#ifdef IN_RING3
/** @defgroup grp_iom_r3    The IOM Host Context Ring-3 API
 * @ingroup grp_iom
 * @{
 */

/**
 * Initializes the IOM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
IOMR3DECL(int) IOMR3Init(PVM pVM);

/**
 * The VM is being reset.
 *
 * @param   pVM     VM handle.
 */
IOMR3DECL(void) IOMR3Reset(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The IOM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
IOMR3DECL(void) IOMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Terminates the IOM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
IOMR3DECL(int) IOMR3Term(PVM pVM);

/**
 * Registers a I/O port R3 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must first register
 * ring-3 ranges before any GC and R0 ranges can be registered using IOMIOPortRegisterGC()
 * and IOMIOPortRegisterR0().
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the port range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to register.
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnOutCallback      Pointer to function which is gonna handle OUT operations in R3.
 * @param   pfnInCallback       Pointer to function which is gonna handle IN operations in R3.
 * @param   pfnOutStringCallback Pointer to function which is gonna handle string OUT operations in R3.
 * @param   pfnInStringCallback Pointer to function which is gonna handle string IN operations in R3.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int) IOMR3IOPortRegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts, RTHCPTR pvUser,
                                     R3PTRTYPE(PFNIOMIOPORTOUT) pfnOutCallback, R3PTRTYPE(PFNIOMIOPORTIN) pfnInCallback,
                                     R3PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnOutStringCallback, R3PTRTYPE(PFNIOMIOPORTINSTRING) pfnInStringCallback,
                                     const char *pszDesc);

/**
 * Registers a Memory Mapped I/O R3 handler.
 *
 * This API is called by PDM on behalf of a device. Devices must register ring-3 ranges
 * before any GC and R0 ranges can be registered using IOMMMIORegisterGC() and IOMMMIORegisterR0().
 *
 * @returns VBox status code.
 *
 * @param   pVM                 VM handle.
 * @param   pDevIns             PDM device instance owning the MMIO range.
 * @param   GCPhysStart         First physical address in the range.
 * @param   cbRange             The size of the range (in bytes).
 * @param   pvUser              User argument for the callbacks.
 * @param   pfnWriteCallback    Pointer to function which is gonna handle Write operations.
 * @param   pfnReadCallback     Pointer to function which is gonna handle Read operations.
 * @param   pfnFillCallback     Pointer to function which is gonna handle Fill/memset operations.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 */
IOMR3DECL(int)  IOMR3MMIORegisterR3(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                    R3PTRTYPE(PFNIOMMMIOWRITE) pfnWriteCallback, R3PTRTYPE(PFNIOMMMIOREAD) pfnReadCallback,
                                    R3PTRTYPE(PFNIOMMMIOFILL) pfnFillCallback, const char *pszDesc);



/**
 * Deregisters a I/O Port range.
 *
 * The specified range must be registered using IOMR3IOPortRegister previous to
 * this call. The range does can be a smaller part of the range specified to
 * IOMR3IOPortRegister, but it can never be larger.
 *
 * This function will remove GC, R0 and R3 context port handlers for this range.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The virtual machine.
 * @param   pDevIns             The device instance associated with the range.
 * @param   PortStart           First port number in the range.
 * @param   cPorts              Number of ports to remove starting at PortStart.
 */
IOMR3DECL(int)  IOMR3IOPortDeregister(PVM pVM, PPDMDEVINS pDevIns, RTIOPORT PortStart, RTUINT cPorts);


/**
 * Deregisters a Memory Mapped I/O handler range.
 *
 * Registered GC, R0, and R3 ranges are affected.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The virtual machine.
 * @param   pDevIns             Device instance which the MMIO region is registered.
 * @param   GCPhysStart         First physical address (GC) in the range.
 * @param   cbRange             Number of bytes to deregister.
 *
 *
 * @remark  This function mainly for PCI PnP Config and will not do
 *          all the checks you might expect it to do.
 */
IOMR3DECL(int)  IOMR3MMIODeregister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange);


/** @} */
#endif


/** @} */

__END_DECLS

#endif

