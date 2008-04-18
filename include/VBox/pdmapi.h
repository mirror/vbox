/** @file
 * PDM - Pluggable Device Manager, Core API.
 *
 * The 'Core API' has been put in a different header because everyone
 * is currently including pdm.h. So, pdm.h is for including all of the
 * PDM stuff, while pdmapi.h is for the core stuff.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_pdmapi_h
#define ___VBox_pdmapi_h

#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_pdm       The Pluggable Device Manager API
 * @{
 */

/**
 * Gets the pending interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pu8Interrupt    Where to store the interrupt on success.
 */
PDMDECL(int) PDMGetInterrupt(PVM pVM, uint8_t *pu8Interrupt);

/**
 * Sets the pending ISA interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level. See the PDM_IRQ_LEVEL_* \#defines.
 */
PDMDECL(int) PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);

/**
 * Sets the pending I/O APIC interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level. See the PDM_IRQ_LEVEL_* \#defines.
 */
PDMDECL(int) PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level);

/**
 * Set the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u64Base         The new base.
 */
PDMDECL(int) PDMApicSetBase(PVM pVM, uint64_t u64Base);

/**
 * Get the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pu64Base        Where to store the APIC base.
 */
PDMDECL(int) PDMApicGetBase(PVM pVM, uint64_t *pu64Base);

/**
 * Set the TPR (task priority register?).
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8TPR           The new TPR.
 */
PDMDECL(int) PDMApicSetTPR(PVM pVM, uint8_t u8TPR);

/**
 * Get the TPR (task priority register?).
 *
 * @returns The current TPR.
 * @param   pVM             VM handle.
 * @param   pu8TPR          Where to store the TRP.
 */
PDMDECL(int) PDMApicGetTPR(PVM pVM, uint8_t *pu8TPR);


#ifdef IN_RING3
/** @defgroup grp_pdm_r3    The PDM Host Context Ring-3 API
 * @ingroup grp_pdm
 * @{
 */

PDMR3DECL(int) PDMR3InitUVM(PUVM pUVM);
PDMR3DECL(int) PDMR3LdrLoadVMMR0U(PUVM pUVM);

/**
 * Initializes the PDM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PDMR3DECL(int) PDMR3Init(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being powered on.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3PowerOn(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being reset.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Reset(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being suspended.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Suspend(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM now being resumed.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3Resume(PVM pVM);

/**
 * This function will notify all the devices and their
 * attached drivers about the VM being powered off.
 *
 * @param   pVM     VM Handle.
 */
PDMR3DECL(void) PDMR3PowerOff(PVM pVM);


/**
 * Applies relocations to GC modules.
 *
 * This must be done very early in the relocation
 * process so that components can resolve GC symbols during relocation.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   offDelta    Relocation delta relative to old location.
 */
PDMR3DECL(void) PDMR3LdrRelocateU(PUVM pUVM, RTGCINTPTR offDelta);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
PDMR3DECL(void) PDMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Terminates the PDM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
PDMR3DECL(int) PDMR3Term(PVM pVM);
PDMR3DECL(void) PDMR3TermUVM(PUVM pUVM);


/**
 * Get the address of a symbol in a given HC ring-3 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR3(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue);

/**
 * Get the address of a symbol in a given HC ring-0 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue);

/**
 * Same as PDMR3GetSymbolR0 except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0Lazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue);

/**
 * Loads a module into the guest context (i.e. into the Hypervisor memory region).
 *
 * The external (to PDM) use of this interface is to load VMMGC.gc.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to load it into.
 * @param   pszFilename     Filename of the module binary.
 * @param   pszName         Module name. Case sensitive and the length is limited!
 */
PDMR3DECL(int) PDMR3LoadGC(PVM pVM, const char *pszFilename, const char *pszName);

/**
 * Get the address of a symbol in a given GC module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main GC module (VMMGC.gc) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGC(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue);

/**
 * Same as PDMR3GetSymbolGC except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main GC module (VMMGC.gc) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGCLazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue);

/**
 * Queries module information from an EIP.
 *
 * This is typically used to locate a crash address.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle
 * @param   uEIP        EIP to locate.
 * @param   pszModName  Where to store the module name.
 * @param   cchModName  Size of the module name buffer.
 * @param   pMod        Base address of the module.
 * @param   pszNearSym1 Name of the closes symbol from below.
 * @param   cchNearSym1 Size of the buffer pointed to by pszNearSym1.
 * @param   pNearSym1   The address of pszNearSym1.
 * @param   pszNearSym2 Name of the closes symbol from below.
 * @param   cchNearSym2 Size of the buffer pointed to by pszNearSym2.
 * @param   pNearSym2   The address of pszNearSym2.
 */
PDMR3DECL(int) PDMR3QueryModFromEIP(PVM pVM, uint32_t uEIP,
                                    char *pszModName,  unsigned cchModName,  RTGCPTR *pMod,
                                    char *pszNearSym1, unsigned cchNearSym1, RTGCPTR *pNearSym1,
                                    char *pszNearSym2, unsigned cchNearSym2, RTGCPTR *pNearSym2);


/**
 * Module enumeration callback function.
 *
 * @returns VBox status.
 *          Failure will stop the search and return the return code.
 *          Warnings will be ignored and not returned.
 * @param   pVM             VM Handle.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name. (short and unique)
 * @param   ImageBase       Address where to executable image is loaded.
 * @param   cbImage         Size of the executable image.
 * @param   fGC             Set if guest context, clear if host context.
 * @param   pvArg           User argument.
 */
typedef DECLCALLBACK(int) FNPDMR3ENUM(PVM pVM, const char *pszFilename, const char *pszName, RTUINTPTR ImageBase, size_t cbImage, bool fGC);
/** Pointer to a FNPDMR3ENUM() function. */
typedef FNPDMR3ENUM *PFNPDMR3ENUM;


/**
 * Enumerate all PDM modules.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 * @param   pfnCallback     Function to call back for each of the modules.
 * @param   pvArg           User argument.
 */
PDMR3DECL(int)  PDMR3EnumModules(PVM pVM, PFNPDMR3ENUM pfnCallback, void *pvArg);


/**
 * Queries the base interace of a device instance.
 *
 * The caller can use this to query other interfaces the device implements
 * and use them to talk to the device.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   ppBase          Where to store the pointer to the base device interface on success.
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryDevice(PVM pVM, const char *pszDevice, unsigned iInstance, PPPDMIBASE ppBase);

/**
 * Queries the base interface of a device LUN.
 *
 * This differs from PDMR3QueryLun by that it returns the interface on the
 * device and not the top level driver.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer.
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryDeviceLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);

/**
 * Query the interface of the top level driver on a LUN.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer.
 * @remark  We're not doing any locking ATM, so don't try call this at times when the
 *          device chain is known to be updated.
 */
PDMR3DECL(int) PDMR3QueryLun(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);

/**
 * Attaches a preconfigured driver to an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @param   ppBase          Where to store the base interface pointer. Optional.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceAttach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun, PPPDMIBASE ppBase);

/**
 * Detaches a driver chain from an existing device instance.
 *
 * This is used to change drivers and suchlike at runtime.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pszDevice       Device name.
 * @param   iInstance       Device instance.
 * @param   iLun            The Logical Unit to obtain the interface of.
 * @thread  EMT
 */
PDMR3DECL(int) PDMR3DeviceDetach(PVM pVM, const char *pszDevice, unsigned iInstance, unsigned iLun);

/**
 * Executes pending DMA transfers.
 * Forced Action handler.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3DmaRun(PVM pVM);

/**
 * Call polling function.
 *
 * @param   pVM             VM handle.
 */
PDMR3DECL(void) PDMR3Poll(PVM pVM);

/**
 * Service a VMMCALLHOST_PDM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PDMR3LockCall(PVM pVM);

/** @} */
#endif


#ifdef IN_GC
/** @defgroup grp_pdm_gc    The PDM Guest Context API
 * @ingroup grp_pdm
 * @{
 */
/** @} */
#endif

__END_DECLS

/** @} */

#endif

