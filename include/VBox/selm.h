/** @file
 * SELM - The Selector Monitor(/Manager).
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

#ifndef ___VBox_selm_h
#define ___VBox_selm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>
#include <VBox/dis.h>


__BEGIN_DECLS

/** @defgroup grp_selm      The Selector Monitor(/Manager) API
 * @{
 */

/**
 * Returns Hypervisor's Trap 08 (\#DF) selector.
 *
 * @returns Hypervisor's Trap 08 (\#DF) selector.
 * @param   pVM     VM Handle.
 */
SELMDECL(RTSEL) SELMGetTrap8Selector(PVM pVM);

/**
 * Sets EIP of Hypervisor's Trap 08 (\#DF) TSS.
 *
 * @param   pVM     VM Handle.
 * @param   u32EIP  EIP of Trap 08 handler.
 */
SELMDECL(void) SELMSetTrap8EIP(PVM pVM, uint32_t u32EIP);

/**
 * Sets ss:esp for ring1 in main Hypervisor's TSS.
 *
 * @param   pVM     VM Handle.
 * @param   ss      Ring1 SS register value.
 * @param   esp     Ring1 ESP register value.
 */
SELMDECL(void) SELMSetRing1Stack(PVM pVM, uint32_t ss, RTGCPTR32 esp);

/**
 * Gets ss:esp for ring1 in main Hypervisor's TSS.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   pSS     Ring1 SS register value.
 * @param   pEsp    Ring1 ESP register value.
 */
SELMDECL(int) SELMGetRing1Stack(PVM pVM, uint32_t *pSS, PRTGCPTR32 pEsp);

/**
 * Returns Guest TSS pointer
 *
 * @param   pVM     VM Handle.
 */
SELMDECL(RTGCPTR) SELMGetGuestTSS(PVM pVM);

/**
 * Gets the hypervisor code selector (CS).
 * @returns CS selector.
 * @param   pVM     The VM handle.
 */
SELMDECL(RTSEL) SELMGetHyperCS(PVM pVM);

/**
 * Gets the 64-mode hypervisor code selector (CS64).
 * @returns CS selector.
 * @param   pVM     The VM handle.
 */
SELMDECL(RTSEL) SELMGetHyperCS64(PVM pVM);

/**
 * Gets the hypervisor data selector (DS).
 * @returns DS selector.
 * @param   pVM     The VM handle.
 */
SELMDECL(RTSEL) SELMGetHyperDS(PVM pVM);

/**
 * Gets the hypervisor TSS selector.
 * @returns TSS selector.
 * @param   pVM     The VM handle.
 */
SELMDECL(RTSEL) SELMGetHyperTSS(PVM pVM);

/**
 * Gets the hypervisor TSS Trap 8 selector.
 * @returns TSS Trap 8 selector.
 * @param   pVM     The VM handle.
 */
SELMDECL(RTSEL) SELMGetHyperTSSTrap08(PVM pVM);

/**
 * Gets the address for the hypervisor GDT.
 *
 * @returns The GDT address.
 * @param   pVM     The VM handle.
 * @remark  This is intended only for very special use, like in the world
 *          switchers. Don't exploit this API!
 */
SELMDECL(RTGCPTR) SELMGetHyperGDT(PVM pVM);

/**
 * Gets info about the current TSS.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if we've got a TSS loaded.
 * @retval  VERR_SELM_NO_TSS if we haven't got a TSS (rather unlikely).
 *
 * @param   pVM                 The VM handle.
 * @param   pGCPtrTss           Where to store the TSS address.
 * @param   pcbTss              Where to store the TSS size limit.
 * @param   pfCanHaveIOBitmap   Where to store the can-have-I/O-bitmap indicator. (optional)
 */
SELMDECL(int) SELMGetTSSInfo(PVM pVM, PRTGCUINTPTR pGCPtrTss, PRTGCUINTPTR pcbTss, bool *pfCanHaveIOBitmap);

/**
 * Converts a GC selector based address to a flat address.
 *
 * No limit checks are done. Use the SELMToFlat*() or SELMValidate*() functions
 * for that.
 *
 * @returns Flat address.
 * @param   pVM         VM Handle.
 * @param   SelReg      Selector register
 * @param   pCtxCore    CPU context
 * @param   Addr        Address part.
 */
SELMDECL(RTGCPTR) SELMToFlat(PVM pVM, DIS_SELREG SelReg, PCPUMCTXCORE pCtxCore, RTGCPTR Addr);

/**
 * Converts a GC selector based address to a flat address.
 *
 * No limit checks are done. Use the SELMToFlat*() or SELMValidate*() functions
 * for that.
 *
 * Note: obsolete; DO NOT USE!
 *
 * @returns Flat address.
 * @param   pVM     VM Handle.
 * @param   Sel     Selector part.
 * @param   Addr    Address part.
 */
SELMDECL(RTGCPTR) SELMToFlatBySel(PVM pVM, RTSEL Sel, RTGCPTR Addr);

/** Flags for SELMToFlatEx().
 * @{ */
/** Don't check the RPL,DPL or CPL. */
#define SELMTOFLAT_FLAGS_NO_PL      RT_BIT(8)
/** Flags contains CPL information. */
#define SELMTOFLAT_FLAGS_HAVE_CPL   RT_BIT(9)
/** CPL is 3. */
#define SELMTOFLAT_FLAGS_CPL3       3
/** CPL is 2. */
#define SELMTOFLAT_FLAGS_CPL2       2
/** CPL is 1. */
#define SELMTOFLAT_FLAGS_CPL1       1
/** CPL is 0. */
#define SELMTOFLAT_FLAGS_CPL0       0
/** Get the CPL from the flags. */
#define SELMTOFLAT_FLAGS_CPL(fFlags)    ((fFlags) & X86_SEL_RPL)
/** Allow converting using Hypervisor GDT entries. */
#define SELMTOFLAT_FLAGS_HYPER      RT_BIT(10)
/** @} */

/**
 * Converts a GC selector based address to a flat address.
 *
 * Some basic checking is done, but not all kinds yet.
 *
 * @returns VBox status
 * @param   pVM         VM Handle.
 * @param   eflags      Current eflags
 * @param   Sel         Selector part.
 * @param   Addr        Address part.
 * @param   pHiddenSel  Hidden selector register (can be NULL)
 * @param   fFlags      SELMTOFLAT_FLAGS_*
 *                      GDT entires are valid.
 * @param   ppvGC       Where to store the GC flat address.
 * @param   pcb         Where to store the bytes from *ppvGC which can be accessed according to
 *                      the selector. NULL is allowed.
 */
SELMDECL(int) SELMToFlatEx(PVM pVM, X86EFLAGS eflags, RTSEL Sel, RTGCPTR Addr, PCPUMSELREGHID pHiddenSel, unsigned fFlags, PRTGCPTR ppvGC, uint32_t *pcb);

/**
 * Validates and converts a GC selector based code address to a flat address.
 *
 * @returns VBox status code.
 * @param   pVM          VM Handle.
 * @param   eflags       Current eflags
 * @param   SelCPL       Current privilege level. Get this from SS - CS might be conforming!
 *                       A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS        Selector part.
 * @param   pHiddenSel   The hidden CS selector register.
 * @param   Addr         Address part.
 * @param   ppvFlat      Where to store the flat address.
 */
SELMDECL(int) SELMValidateAndConvertCSAddr(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, PCPUMSELREGHID pHiddenCSSel, RTGCPTR Addr, PRTGCPTR ppvFlat);

/**
 * Validates and converts a GC selector based code address to a flat address.
 *
 * This is like SELMValidateAndConvertCSAddr + SELMIsSelector32Bit but with
 * invalid hidden CS data. It's customized for dealing efficiently with CS
 * at GC trap time.
 *
 * @returns VBox status code.
 * @param   pVM          VM Handle.
 * @param   eflags       Current eflags
 * @param   SelCPL       Current privilege level. Get this from SS - CS might be conforming!
 *                       A full selector can be passed, we'll only use the RPL part.
 * @param   SelCS        Selector part.
 * @param   Addr         Address part.
 * @param   ppvFlat      Where to store the flat address.
 * @param   pcBits       Where to store the 64-bit/32-bit/16-bit indicator.
 */
SELMDECL(int) SELMValidateAndConvertCSAddrGCTrap(PVM pVM, X86EFLAGS eflags, RTSEL SelCPL, RTSEL SelCS, RTGCPTR Addr, PRTGCPTR ppvFlat, uint32_t *pcBits);

/**
 * Return the cpu mode corresponding to the (CS) selector
 *
 * @returns DISCPUMODE according to the selector type (16, 32 or 64 bits)
 * @param   pVM        VM Handle.
 * @param   eflags     Current eflags register
 * @param   Sel        The selector.
 * @param   pHiddenSel The hidden selector register.
 */
SELMDECL(DISCPUMODE) SELMGetCpuModeFromSelector(PVM pVM, X86EFLAGS eflags, RTSEL Sel, CPUMSELREGHID *pHiddenSel);

/**
 * Returns flat address and limit of LDT by LDT selector.
 *
 * Fully validate selector.
 *
 * @returns VBox status.
 * @param   pVM       VM Handle.
 * @param   SelLdt    LDT selector.
 * @param   ppvLdt    Where to store the flat address of LDT.
 * @param   pcbLimit  Where to store LDT limit.
 */
SELMDECL(int) SELMGetLDTFromSel(PVM pVM, RTSEL SelLdt, PRTGCPTR ppvLdt, unsigned *pcbLimit);


/**
 * Selector information structure.
 */
typedef struct SELMSELINFO
{
    /** The base address. */
    RTGCPTR         GCPtrBase;
    /** The limit (-1). */
    RTGCUINTPTR     cbLimit;
    /** The raw descriptor. */
    VBOXDESC        Raw;
    /** The selector. */
    RTSEL           Sel;
    /** Set if the selector is used by the hypervisor. */
    bool            fHyper;
    /** Set if the selector is a real mode segment. */
    bool            fRealMode;
} SELMSELINFO;
/** Pointer to a SELM selector information struct. */
typedef SELMSELINFO *PSELMSELINFO;
/** Pointer to a const SELM selector information struct. */
typedef const SELMSELINFO *PCSELMSELINFO;

/**
 * Validates a CS selector.
 *
 * @returns VBox status code.
 * @param   pSelInfo    Pointer to the selector information for the CS selector.
 * @param   SelCPL      The selector defining the CPL (SS).
 */
SELMDECL(int) SELMSelInfoValidateCS(PCSELMSELINFO pSelInfo, RTSEL SelCPL);

/** @def SELMSelInfoIsExpandDown
 * Tests whether the selector info describes an expand-down selector or now.
 *
 * @returns true / false.
 * @param   pSelInfo        The selector info.
 *
 * @remark  Realized as a macro for reasons of speed/lazyness and to avoid
 *          dragging in VBox/x86.h for now.
 */
#define SELMSelInfoIsExpandDown(pSelInfo) \
    (   (pSelInfo)->Raw.Gen.u1DescType \
     && ((pSelInfo)->Raw.Gen.u4Type & (X86_SEL_TYPE_DOWN | X86_SEL_TYPE_CODE)) == X86_SEL_TYPE_DOWN)



#ifdef IN_RING3
/** @defgroup grp_selm_r3   The Selector Monitor(/Manager) API
 * @ingroup grp_selm
 * @{
 */

/**
 * Initializes the SELM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
SELMR3DECL(int) SELMR3Init(PVM pVM);

/**
 * Finalizes HMA page attributes.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
SELMR3DECL(int) SELMR3InitFinalize(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
SELMR3DECL(void) SELMR3Relocate(PVM pVM);

/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 * This is called by PGM.
 *
 * @param   pVM       The VM handle
 */
SELMR3DECL(void) SELMR3PagingModeChanged(PVM pVM);

/**
 * Terminates the SELM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
SELMR3DECL(int) SELMR3Term(PVM pVM);

/**
 * The VM is being reset.
 *
 * For the SELM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     VM handle.
 */
SELMR3DECL(void) SELMR3Reset(PVM pVM);

/**
 * Updates the Guest GDT & LDT virtualization based on current CPU state.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
SELMR3DECL(int) SELMR3UpdateFromCPUM(PVM pVM);

/**
 * Compares the Guest GDT and LDT with the shadow tables.
 * This is a VBOX_STRICT only function.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle.
 */
SELMR3DECL(int) SELMR3DebugCheck(PVM pVM);
#ifdef VBOX_STRICT
# define SELMR3DEBUGCHECK(pVM)     SELMR3DebugCheck(pVM)
#else
# define SELMR3DEBUGCHECK(pVM)     do { } while (0)
#endif

/**
 * Check if the TSS ring 0 stack selector and pointer were updated (for now)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
SELMR3DECL(int) SELMR3SyncTSS(PVM pVM);

/**
 * Gets information about a selector.
 * Intended for the debugger mostly and will prefer the guest
 * descriptor tables over the shadow ones.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INVALID_SELECTOR if the selector isn't fully inside the descriptor table.
 * @returns VERR_SELECTOR_NOT_PRESENT if the selector wasn't present.
 * @returns VERR_PAGE_TABLE_NOT_PRESENT or VERR_PAGE_NOT_PRESENT if the pagetable or page
 *          backing the selector table wasn't present.
 * @returns Other VBox status code on other errros.
 *
 * @param   pVM         VM handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 */
SELMR3DECL(int) SELMR3GetSelectorInfo(PVM pVM, RTSEL Sel, PSELMSELINFO pSelInfo);

/**
 * Gets information about a selector from the shadow tables.
 *
 * This is intended to be faster than the SELMR3GetSelectorInfo() method, but requires
 * that the caller ensures that the shadow tables are up to date.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INVALID_SELECTOR if the selector isn't fully inside the descriptor table.
 * @returns VERR_SELECTOR_NOT_PRESENT if the selector wasn't present.
 * @returns VERR_PAGE_TABLE_NOT_PRESENT or VERR_PAGE_NOT_PRESENT if the pagetable or page
 *          backing the selector table wasn't present.
 * @returns Other VBox status code on other errors.
 *
 * @param   pVM         VM handle.
 * @param   Sel         The selector to get info about.
 * @param   pSelInfo    Where to store the information.
 */
SELMR3DECL(int) SELMR3GetShadowSelectorInfo(PVM pVM, RTSEL Sel, PSELMSELINFO pSelInfo);

/**
 * Validates the RawR0 TSS values against the one in the Guest TSS.
 *
 * @returns true if it matches.
 * @returns false and assertions on mismatch..
 * @param   pVM     VM Handle.
 */
SELMR3DECL(bool) SELMR3CheckTSS(PVM pVM);


/**
 * Disable GDT/LDT/TSS monitoring and syncing
 *
 * @param   pVM         The VM to operate on.
 */
SELMR3DECL(void) SELMR3DisableMonitoring(PVM pVM);


/**
 * Dumps a descriptor.
 *
 * @param   Desc    Descriptor to dump.
 * @param   Sel     Selector number.
 * @param   pszMsg  Message to prepend the log entry with.
 */
SELMR3DECL(void) SELMR3DumpDescriptor(VBOXDESC  Desc, RTSEL Sel, const char *pszMsg);

/**
 * Dumps the hypervisor GDT.
 *
 * @param   pVM     VM handle.
 */
SELMR3DECL(void) SELMR3DumpHyperGDT(PVM pVM);

/**
 * Dumps the hypervisor LDT.
 *
 * @param   pVM     VM handle.
 */
SELMR3DECL(void) SELMR3DumpHyperLDT(PVM pVM);

/**
 * Dumps the guest GDT.
 *
 * @param   pVM     VM handle.
 */
SELMR3DECL(void) SELMR3DumpGuestGDT(PVM pVM);

/**
 * Dumps the guest LDT.
 *
 * @param   pVM     VM handle.
 */
SELMR3DECL(void) SELMR3DumpGuestLDT(PVM pVM);

/** @} */
#endif


/** @} */
__END_DECLS


#endif
