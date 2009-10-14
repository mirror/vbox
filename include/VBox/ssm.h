/** @file
 * SSM - The Save State Manager. (VMM)
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

#ifndef ___VBox_ssm_h
#define ___VBox_ssm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/tm.h>
#include <VBox/vmapi.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_ssm       The Saved State Manager API
 * @{
 */

/**
 * Determine the major version of the SSM version. If the major SSM version of two snapshots is
 * different, the snapshots are incompatible.
 */
#define SSM_VERSION_MAJOR(ver)                  ((ver) & 0xffff0000)

/**
 * Determine the minor version of the SSM version. If the major SSM version of two snapshots is
 * the same, the code must handle incompatibilies between minor version changes (e.g. use dummy
 * values for non-existent fields).
 */
#define SSM_VERSION_MINOR(ver)                  ((ver) & 0x0000ffff)

/**
 * Determine if the major version changed between two SSM versions.
 */
#define SSM_VERSION_MAJOR_CHANGED(ver1,ver2)    (SSM_VERSION_MAJOR(ver1) != SSM_VERSION_MAJOR(ver2))

/** The special value for the final pass.  */
#define SSM_PASS_FINAL                          UINT32_MAX


#ifdef IN_RING3
/** @defgroup grp_ssm_r3     The SSM Host Context Ring-3 API
 * @{
 */


/**
 * What to do after the save/load operation.
 */
typedef enum SSMAFTER
{
    /** Invalid. */
    SSMAFTER_INVALID = 0,
    /** Will resume the loaded state. */
    SSMAFTER_RESUME,
    /** Will destroy the VM after saving. */
    SSMAFTER_DESTROY,
    /** Will continue execution after saving the VM. */
    SSMAFTER_CONTINUE,
    /** Will migrate the VM.
     * The source VM will be destroyed (then one saving), the destination VM
     * will continue execution. */
    SSMAFTER_MIGRATE,
    /** Will debug the saved state.
     * This is used to drop some of the stricter consitentcy checks so it'll
     * load fine in the debugger or animator. */
    SSMAFTER_DEBUG_IT,
    /** The file was opened using SSMR3Open() and we have no idea what the plan is. */
    SSMAFTER_OPENED
} SSMAFTER;


/** Pointer to a structure field description. */
typedef struct SSMFIELD *PSSMFIELD;
/** Pointer to a const  structure field description. */
typedef const struct SSMFIELD *PCSSMFIELD;

/**
 * SSMFIELD Get/Put callback function.
 *
 * This is call for getting and putting the field it is associated with.  It's
 * up to the callback to work the saved state correctly.
 *
 * @returns VBox status code.
 *
 * @param   pSSM            The saved state handle.
 * @param   pField          The field that is being processed.
 * @param   pvStruct        Pointer to the structure.
 * @param   fFlags          SSMSTRUCT_FLAGS_XXX.
 * @param   fGetOrPut       True if getting, false if putting.
 * @param   pvUser          The user argument specified to SSMR3GetStructEx or
 *                          SSMR3PutStructEx.
 */
typedef DECLCALLBACK(int) FNSSMFIELDGETPUT(PSSMHANDLE pSSM, const struct SSMFIELD *pField, void *pvStruct,
                                           uint32_t fFlags, bool fGetOrPut, void *pvUser);
/** Pointer to a SSMFIELD Get/Put callback. */
typedef FNSSMFIELDGETPUT *PFNSSMFIELDGETPUT;

/**
 * SSM field transformers.
 *
 * These are stored in the SSMFIELD::pfnGetPutOrTransformer and must therefore
 * have values outside the valid pointer range.
 */
typedef enum SSMFIELDTRANS
{
    /** Invalid. */
    SSMFIELDTRANS_INVALID = 0,
    /** No transformation. */
    SSMFIELDTRANS_NO_TRANSFORMATION,
    /** Guest context (GC) virtual address. */
    SSMFIELDTRANS_GCPTR,
    /** Guest context (GC) physical address. */
    SSMFIELDTRANS_GCPHYS,
    /** Raw-mode context (RC) virtual address. */
    SSMFIELDTRANS_RCPTR,
    /** Array of raw-mode context (RC) virtual addresses. */
    SSMFIELDTRANS_RCPTR_ARRAY,
    /** Host context (HC) virtual address used as a NULL indicator. See
     * SSMFIELD_ENTRY_HCPTR_NI. */
    SSMFIELDTRANS_HCPTR_NI,
    /** Array of SSMFIELDTRANS_HCPTR_NI. */
    SSMFIELDTRANS_HCPTR_NI_ARRAY,
    /** Ignorable Host context (HC) virtual address. See SSMFIELD_ENTRY_HCPTR. */
    SSMFIELDTRANS_HCPTR,
    /** Ignorable field. See SSMFIELD_ENTRY_IGNORE. */
    SSMFIELDTRANS_IGNORE,
    /** Padding that differs between 32-bit and 64-bit hosts.
     * The first  byte of SSMFIELD::cb contains the size for 32-bit hosts.
     * The second byte of SSMFIELD::cb contains the size for 64-bit hosts.
     * The upper  word of SSMFIELD::cb contains the actual field size.
     */
    SSMFIELDTRANS_PAD_HC,
    /** Padding for 32-bit hosts only.
     * SSMFIELD::cb has the same format as for SSMFIELDTRANS_PAD_HC. */
    SSMFIELDTRANS_PAD_HC32,
    /** Padding for 64-bit hosts only.
     * SSMFIELD::cb has the same format as for SSMFIELDTRANS_PAD_HC. */
    SSMFIELDTRANS_PAD_HC64,
    /** Automatic compiler padding that may differ between 32-bit and
     * 64-bit hosts. SSMFIELD::cb has the same format as for
     * SSMFIELDTRANS_PAD_HC. */
    SSMFIELDTRANS_PAD_HC_AUTO
} SSMFIELDTRANS;

/** Tests if it's a padding field with the special SSMFIELD::cb format.
 * @returns true / false.
 * @param   pfn     The SSMFIELD::pfnGetPutOrTransformer value.
 */
#define SSMFIELDTRANS_IS_PADDING(pfn)   \
    (   (uintptr_t)(pfn) >= SSMFIELDTRANS_PAD_HC && (uintptr_t)(pfn) <= SSMFIELDTRANS_PAD_HC_AUTO )

/**
 * A structure field description.
 */
typedef struct SSMFIELD
{
    /** Getter and putter callback or transformer index. */
    PFNSSMFIELDGETPUT   pfnGetPutOrTransformer;
    /** Field offset into the structure. */
    uint32_t            off;
    /** The size of the field. */
    uint32_t            cb;
    /** Field name. */
    const char         *pszName;
} SSMFIELD;

/** Emit a SSMFIELD array entry.
 * @internal  */
#define SSMFIELD_ENTRY_INT(Type, Field, enmTransformer) \
    { \
        (PFNSSMFIELDGETPUT)(uintptr_t)(enmTransformer), \
        RT_OFFSETOF(Type, Field), \
        RT_SIZEOFMEMB(Type, Field), \
        #Type "::" #Field \
    }
/** Emit a SSMFIELD array entry for an alignment padding.
 * @internal  */
#define SSMFIELD_ENTRY_PAD_INT(Type, Field, cb32, cb64, enmTransformer) \
    { \
        (PFNSSMFIELDGETPUT)(uintptr_t)(enmTransformer), \
        RT_OFFSETOF(Type, Field), \
        (RT_SIZEOFMEMB(Type, Field) << 16) | (cb32) | ((cb64) << 8), \
        #Type "::" #Field \
    }
/** Emit a SSMFIELD array entry for an alignment padding.
 * @internal  */
#define SSMFIELD_ENTRY_PAD_OTHER_INT(Type, Field, cb32, cb64, enmTransformer) \
    { \
        (PFNSSMFIELDGETPUT)(uintptr_t)(enmTransformer), \
        UINT32_MAX / 2, \
        0 | (cb32) | ((cb64) << 8), \
        #Type "::" #Field \
    }

/** Emit a SSMFIELD array entry. */
#define SSMFIELD_ENTRY(Type, Field)                 SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_NO_TRANSFORMATION)
/** Emit a SSMFIELD array entry for a RTGCPTR type. */
#define SSMFIELD_ENTRY_GCPTR(Type, Field)           SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_GCPTR)
/** Emit a SSMFIELD array entry for a RTGCPHYS type. */
#define SSMFIELD_ENTRY_GCPHYS(Type, Field)          SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_GCPHYS)
/** Emit a SSMFIELD array entry for a raw-mode context pointer. */
#define SSMFIELD_ENTRY_RCPTR(Type, Field)           SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_RCPTR)
/** Emit a SSMFIELD array entry for a raw-mode context pointer. */
#define SSMFIELD_ENTRY_RCPTR_ARRAY(Type, Field)     SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_RCPTR_ARRAY)
/** Emit a SSMFIELD array entry for a ring-0 or ring-3 pointer type that is only
 * of interest as a NULL indicator.
 *
 * This is always restored as a 0 (NULL) or 1 value.  When
 * SSMSTRUCT_FLAGS_DONT_IGNORE is set, the pointer will be saved in its
 * entirety, when clear it will be saved as a boolean. */
#define SSMFIELD_ENTRY_HCPTR_NI(Type, Field)        SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_HCPTR_NI)
/** Same as SSMFIELD_ENTRY_HCPTR_NI, except it's an array of the buggers. */
#define SSMFIELD_ENTRY_HCPTR_NI_ARRAY(Type, Field)  SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_HCPTR_NI_ARRAY)
/** Emit a SSMFIELD array entry for a ring-0 or ring-3 pointer type that is
 * of no real interest to the saved state.  It follows the same save and restore
 * rules as SSMFIELD_ENTRY_IGNORE. */
#define SSMFIELD_ENTRY_HCPTR(Type, Field)           SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_HCPTR)
/** Emit a SSMFIELD array entry for a field that can be ignored.
 * It is stored as zeros if SSMSTRUCT_FLAGS_DONT_IGNORE is specified to
 * SSMR3PutStructEx.  The member is never touched upon restore. */
#define SSMFIELD_ENTRY_IGNORE(Type, Field)          SSMFIELD_ENTRY_INT(Type, Field, SSMFIELDTRANS_IGNORE)
/** Emit a SSMFIELD array entry for a padding that differs in size between
 * 64-bit and 32-bit hosts. */
#define SSMFIELD_ENTRY_PAD_HC(Type, Field, cb32, cb64) SSMFIELD_ENTRY_PAD_INT(   Type, Field, cb32, cb64, SSMFIELDTRANS_PAD_HC)
/** Emit a SSMFIELD array entry for a padding that is exclusive to 64-bit hosts. */
#if HC_ARCH_BITS == 64
# define SSMFIELD_ENTRY_PAD_HC64(Type, Field, cb)   SSMFIELD_ENTRY_PAD_INT(      Type, Field, 0, cb, SSMFIELDTRANS_PAD_HC64)
#else
# define SSMFIELD_ENTRY_PAD_HC64(Type, Field, cb)   SSMFIELD_ENTRY_PAD_OTHER_INT(Type, Field, 0, cb, SSMFIELDTRANS_PAD_HC64)
#endif
/** Emit a SSMFIELD array entry for a 32-bit padding for on 64-bits hosts.  */
#if HC_ARCH_BITS == 32
# define SSMFIELD_ENTRY_PAD_HC32(Type, Field, cb)   SSMFIELD_ENTRY_PAD_INT(      Type, Field, cb, 0, SSMFIELDTRANS_PAD_HC32)
#else
# define SSMFIELD_ENTRY_PAD_HC32(Type, Field, cb)   SSMFIELD_ENTRY_PAD_OTHER_INT(Type, Field, cb, 0, SSMFIELDTRANS_PAD_HC32)
#endif
/** Emit a SSMFIELD array entry for an automatic compiler padding that may
 * differ in size between 64-bit and 32-bit hosts. */
#if HC_ARCH_BITS == 64
# define SSMFIELD_ENTRY_PAD_HC_AUTO(cb32, cb64) \
    { \
        (PFNSSMFIELDGETPUT)(uintptr_t)(SSMFIELDTRANS_PAD_HC_AUTO), \
        UINT32_MAX / 2, \
        (cb64 << 16) | (cb32) | ((cb64) << 8), \
        "<compiler-padding>" \
    }
#else
# define SSMFIELD_ENTRY_PAD_HC_AUTO(cb32, cb64) \
    { \
        (PFNSSMFIELDGETPUT)(uintptr_t)(SSMFIELDTRANS_PAD_HC_AUTO), \
        UINT32_MAX / 2, \
        (cb32 << 16) | (cb32) | ((cb64) << 8), \
        "<compiler-padding>" \
    }
#endif

/** Emit a SSMFIELD array entry for a field with a custom callback. */
#define SSMFIELD_ENTRY_CALLBACK(Type, Field, pfnGetPut) \
    { (pfnGetPut), RT_OFFSETOF(Type, Field), RT_SIZEOFMEMB(Type, Field), #Type "::" #Field }
/** Emit the terminating entry of a SSMFIELD array. */
#define SSMFIELD_ENTRY_TERM()               { (PFNSSMFIELDGETPUT)(uintptr_t)SSMFIELDTRANS_INVALID, UINT32_MAX, UINT32_MAX, NULL }


/** @name SSMR3GetStructEx and SSMR3PutStructEx flags.
 * @{ */
/** The field descriptors must exactly cover the entire struct, A to Z. */
#define SSMSTRUCT_FLAGS_FULL_STRUCT         RT_BIT_32(0)
/** No start and end markers, just the raw bits. */
#define SSMSTRUCT_FLAGS_NO_MARKERS          RT_BIT_32(1)
/** Do not ignore any ignorable fields. */
#define SSMSTRUCT_FLAGS_DONT_IGNORE         RT_BIT_32(2)
/** Band-aid for old SSMR3PutMem/SSMR3GetMem of structurs with host pointers. */
#define SSMSTRUCT_FLAGS_MEM_BAND_AID        (SSMSTRUCT_FLAGS_DONT_IGNORE | SSMSTRUCT_FLAGS_FULL_STRUCT | SSMSTRUCT_FLAGS_NO_MARKERS)
/** Mask of the valid bits. */
#define SSMSTRUCT_FLAGS_VALID_MASK          UINT32_C(0x00000007)
/** @} */


/** The PDM Device callback variants.
 * @{
 */

/**
 * Prepare state live save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDEVLIVEPREP(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVLIVEPREP() function. */
typedef FNSSMDEVLIVEPREP *PFNSSMDEVLIVEPREP;

/**
 * Execute state live save operation.
 *
 * This will be called repeatedly until all units vote that the live phase has
 * been concluded.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @param   uPass           The pass.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDEVLIVEEXEC(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass);
/** Pointer to a FNSSMDEVLIVEEXEC() function. */
typedef FNSSMDEVLIVEEXEC *PFNSSMDEVLIVEEXEC;

/**
 * Vote on whether the live part of the saving has been concluded.
 *
 * The vote stops once a unit has vetoed the decision, so don't rely upon this
 * being called every time.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if done.
 * @retval  VINF_SSM_VOTE_FOR_ANOTHER_PASS if another pass is needed.
 * @retval  VINF_SSM_VOTE_DONE_DONT_CALL_AGAIN if the live saving of the unit is
 *          done and there is not need calling it again before the final pass.
 * @retval  VERR_SSM_VOTE_FOR_GIVING_UP if its time to give up.
 *
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDEVLIVEVOTE(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVLIVEVOTE() function. */
typedef FNSSMDEVLIVEVOTE *PFNSSMDEVLIVEVOTE;

/**
 * Prepare state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDEVSAVEPREP(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVSAVEPREP() function. */
typedef FNSSMDEVSAVEPREP *PFNSSMDEVSAVEPREP;

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDEVSAVEEXEC(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVSAVEEXEC() function. */
typedef FNSSMDEVSAVEEXEC *PFNSSMDEVSAVEEXEC;

/**
 * Done state save operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDEVSAVEDONE(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVSAVEDONE() function. */
typedef FNSSMDEVSAVEDONE *PFNSSMDEVSAVEDONE;

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDEVLOADPREP(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVLOADPREP() function. */
typedef FNSSMDEVLOADPREP *PFNSSMDEVLOADPREP;

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The pass. This is always SSM_PASS_FINAL for units
 *                          that doesn't specify a pfnSaveLive callback.
 */
typedef DECLCALLBACK(int) FNSSMDEVLOADEXEC(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
/** Pointer to a FNSSMDEVLOADEXEC() function. */
typedef FNSSMDEVLOADEXEC *PFNSSMDEVLOADEXEC;

/**
 * Done state load operation.
 *
 * @returns VBox load code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDEVLOADDONE(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDEVLOADDONE() function. */
typedef FNSSMDEVLOADDONE *PFNSSMDEVLOADDONE;

/** @} */


/** The PDM Driver callback variants.
 * @{
 */

/**
 * Prepare state live save operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the device which registered the
 *                          data unit.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDRVLIVEPREP(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVLIVEPREP() function. */
typedef FNSSMDRVLIVEPREP *PFNSSMDRVLIVEPREP;

/**
 * Execute state live save operation.
 *
 * This will be called repeatedly until all units vote that the live phase has
 * been concluded.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the device which registered the
 *                          data unit.
 * @param   pSSM            SSM operation handle.
 * @param   uPass           The data pass.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDRVLIVEEXEC(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM, uint32_t uPass);
/** Pointer to a FNSSMDRVLIVEEXEC() function. */
typedef FNSSMDRVLIVEEXEC *PFNSSMDRVLIVEEXEC;

/**
 * Vote on whether the live part of the saving has been concluded.
 *
 * The vote stops once a unit has vetoed the decision, so don't rely upon this
 * being called every time.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if done.
 * @retval  VINF_SSM_VOTE_FOR_ANOTHER_PASS if another pass is needed.
 * @retval  VINF_SSM_VOTE_DONE_DONT_CALL_AGAIN if the live saving of the unit is
 *          done and there is not need calling it again before the final pass.
 * @retval  VERR_SSM_VOTE_FOR_GIVING_UP if its time to give up.
 *
 * @param   pDrvIns         Driver instance of the device which registered the
 *                          data unit.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMDRVLIVEVOTE(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVLIVEVOTE() function. */
typedef FNSSMDRVLIVEVOTE *PFNSSMDRVLIVEVOTE;


/**
 * Prepare state save operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDRVSAVEPREP(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVSAVEPREP() function. */
typedef FNSSMDRVSAVEPREP *PFNSSMDRVSAVEPREP;

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDRVSAVEEXEC(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVSAVEEXEC() function. */
typedef FNSSMDRVSAVEEXEC *PFNSSMDRVSAVEEXEC;

/**
 * Done state save operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDRVSAVEDONE(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVSAVEDONE() function. */
typedef FNSSMDRVSAVEDONE *PFNSSMDRVSAVEDONE;

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDRVLOADPREP(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVLOADPREP() function. */
typedef FNSSMDRVLOADPREP *PFNSSMDRVLOADPREP;

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The pass. This is always SSM_PASS_FINAL for units
 *                          that doesn't specify a pfnSaveLive callback.
 */
typedef DECLCALLBACK(int) FNSSMDRVLOADEXEC(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
/** Pointer to a FNSSMDRVLOADEXEC() function. */
typedef FNSSMDRVLOADEXEC *PFNSSMDRVLOADEXEC;

/**
 * Done state load operation.
 *
 * @returns VBox load code.
 * @param   pDrvIns         Driver instance of the driver which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMDRVLOADDONE(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM);
/** Pointer to a FNSSMDRVLOADDONE() function. */
typedef FNSSMDRVLOADDONE *PFNSSMDRVLOADDONE;

/** @} */


/** The internal callback variants.
 * @{
 */


/**
 * Prepare state live save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMINTLIVEPREP(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTLIVEPREP() function. */
typedef FNSSMINTLIVEPREP *PFNSSMINTLIVEPREP;

/**
 * Execute state live save operation.
 *
 * This will be called repeatedly until all units vote that the live phase has
 * been concluded.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   uPass           The data pass.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMINTLIVEEXEC(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass);
/** Pointer to a FNSSMINTLIVEEXEC() function. */
typedef FNSSMINTLIVEEXEC *PFNSSMINTLIVEEXEC;

/**
 * Vote on whether the live part of the saving has been concluded.
 *
 * The vote stops once a unit has vetoed the decision, so don't rely upon this
 * being called every time.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if done.
 * @retval  VINF_SSM_VOTE_FOR_ANOTHER_PASS if another pass is needed.
 * @retval  VINF_SSM_VOTE_DONE_DONT_CALL_AGAIN if the live saving of the unit is
 *          done and there is not need calling it again before the final pass.
 * @retval  VERR_SSM_VOTE_FOR_GIVING_UP if its time to give up.
 *
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMINTLIVEVOTE(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTLIVEVOTE() function. */
typedef FNSSMINTLIVEVOTE *PFNSSMINTLIVEVOTE;

/**
 * Prepare state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMINTSAVEPREP(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTSAVEPREP() function. */
typedef FNSSMINTSAVEPREP *PFNSSMINTSAVEPREP;

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMINTSAVEEXEC(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTSAVEEXEC() function. */
typedef FNSSMINTSAVEEXEC *PFNSSMINTSAVEEXEC;

/**
 * Done state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMINTSAVEDONE(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTSAVEDONE() function. */
typedef FNSSMINTSAVEDONE *PFNSSMINTSAVEDONE;

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMINTLOADPREP(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTLOADPREP() function. */
typedef FNSSMINTLOADPREP *PFNSSMINTLOADPREP;

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The pass. This is always SSM_PASS_FINAL for units
 *                          that doesn't specify a pfnSaveLive callback.
 */
typedef DECLCALLBACK(int) FNSSMINTLOADEXEC(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
/** Pointer to a FNSSMINTLOADEXEC() function. */
typedef FNSSMINTLOADEXEC *PFNSSMINTLOADEXEC;

/**
 * Done state load operation.
 *
 * @returns VBox load code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
typedef DECLCALLBACK(int) FNSSMINTLOADDONE(PVM pVM, PSSMHANDLE pSSM);
/** Pointer to a FNSSMINTLOADDONE() function. */
typedef FNSSMINTLOADDONE *PFNSSMINTLOADDONE;

/** @} */


/** The External callback variants.
 * @{
 */

/**
 * Prepare state live save operation.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMEXTLIVEPREP(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTLIVEPREP() function. */
typedef FNSSMEXTLIVEPREP *PFNSSMEXTLIVEPREP;

/**
 * Execute state live save operation.
 *
 * This will be called repeatedly until all units vote that the live phase has
 * been concluded.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @param   uPass           The data pass.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMEXTLIVEEXEC(PSSMHANDLE pSSM, void *pvUser, uint32_t uPass);
/** Pointer to a FNSSMEXTLIVEEXEC() function. */
typedef FNSSMEXTLIVEEXEC *PFNSSMEXTLIVEEXEC;

/**
 * Vote on whether the live part of the saving has been concluded.
 *
 * The vote stops once a unit has vetoed the decision, so don't rely upon this
 * being called every time.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if done.
 * @retval  VINF_SSM_VOTE_FOR_ANOTHER_PASS if another pass is needed.
 * @retval  VINF_SSM_VOTE_DONE_DONT_CALL_AGAIN if the live saving of the unit is
 *          done and there is not need calling it again before the final pass.
 * @retval  VERR_SSM_VOTE_FOR_GIVING_UP if its time to give up.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @thread  Any.
 */
typedef DECLCALLBACK(int) FNSSMEXTLIVEVOTE(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTLIVEVOTE() function. */
typedef FNSSMEXTLIVEVOTE *PFNSSMEXTLIVEVOTE;

/**
 * Prepare state save operation.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNSSMEXTSAVEPREP(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTSAVEPREP() function. */
typedef FNSSMEXTSAVEPREP *PFNSSMEXTSAVEPREP;

/**
 * Execute state save operation.
 *
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @author  The lack of return code is for legacy reasons.
 */
typedef DECLCALLBACK(void) FNSSMEXTSAVEEXEC(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTSAVEEXEC() function. */
typedef FNSSMEXTSAVEEXEC *PFNSSMEXTSAVEEXEC;

/**
 * Done state save operation.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNSSMEXTSAVEDONE(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTSAVEDONE() function. */
typedef FNSSMEXTSAVEDONE *PFNSSMEXTSAVEDONE;

/**
 * Prepare state load operation.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNSSMEXTLOADPREP(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTLOADPREP() function. */
typedef FNSSMEXTLOADPREP *PFNSSMEXTLOADPREP;

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @param   uVersion        Data layout version.
 * @param   uPass           The pass. This is always SSM_PASS_FINAL for units
 *                          that doesn't specify a pfnSaveLive callback.
 * @remark  The odd return value is for legacy reasons.
 */
typedef DECLCALLBACK(int) FNSSMEXTLOADEXEC(PSSMHANDLE pSSM, void *pvUser, uint32_t uVersion, uint32_t uPass);
/** Pointer to a FNSSMEXTLOADEXEC() function. */
typedef FNSSMEXTLOADEXEC *PFNSSMEXTLOADEXEC;

/**
 * Done state load operation.
 *
 * @returns VBox load code.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNSSMEXTLOADDONE(PSSMHANDLE pSSM, void *pvUser);
/** Pointer to a FNSSMEXTLOADDONE() function. */
typedef FNSSMEXTLOADDONE *PFNSSMEXTLOADDONE;

/** @} */


/**
 * SSM stream method table.
 *
 * This is used for live migration as well as internally in SSM.
 */
typedef struct SSMSTRMOPS
{
    /** Struct magic + version (SSMSTRMOPS_VERSION). */
    uint32_t    u32Version;

    /**
     * Write bytes to the stream.
     *
     * @returns VBox status code.
     * @param   pvUser              The user argument.
     * @param   offStream           The stream offset we're (supposed to be) at.
     * @param   pvBuf               Pointer to the data.
     * @param   cbToWrite           The number of bytes to write.
     */
    DECLCALLBACKMEMBER(int, pfnWrite)(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite);

    /**
     * Read bytes to the stream.
     *
     * @returns VBox status code.
     * @param   pvUser              The user argument.
     * @param   offStream           The stream offset we're (supposed to be) at.
     * @param   pvBuf               Where to return the bytes.
     * @param   cbToRead            The number of bytes to read.
     * @param   pcbRead             Where to return the number of bytes actually
     *                              read.  This may differ from cbToRead when the
     *                              end of the stream is encountered.
     */
    DECLCALLBACKMEMBER(int, pfnRead)(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead);

    /**
     * Seeks in the stream.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_SUPPORTED if the stream doesn't support this action.
     *
     * @param   pvUser              The user argument.
     * @param   offSeek             The seek offset.
     * @param   uMethod             RTFILE_SEEK_BEGIN, RTFILE_SEEK_END or
     *                              RTFILE_SEEK_CURRENT.
     * @param   poffActual          Where to store the new file position. Optional.
     */
    DECLCALLBACKMEMBER(int, pfnSeek)(void *pvUser, int64_t offSeek, unsigned uMethod, uint64_t *poffActual);

    /**
     * Get the current stream position.
     *
     * @returns The correct stream position.
     * @param   pvUser              The user argument.
     */
    DECLCALLBACKMEMBER(uint64_t, pfnTell)(void *pvUser);

    /**
     * Get the size/length of the stream.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_SUPPORTED if the stream doesn't support this action.
     *
     * @param   pvUser              The user argument.
     * @param   pcb                 Where to return the size/length.
     */
    DECLCALLBACKMEMBER(int, pfnSize)(void *pvUser, uint64_t *pcb);

    /**
     * Close the stream.
     *
     * @returns VBox status code.
     * @param   pvUser              The user argument.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(void *pvUser);

    /** Struct magic + version (SSMSTRMOPS_VERSION). */
    uint32_t    u32EndVersion;
} SSMSTRMOPS;
/** Struct magic + version (SSMSTRMOPS_VERSION). */
#define SSMSTRMOPS_VERSION      UINT32_C(0x55aa0001)


VMMR3_INT_DECL(void)    SSMR3Term(PVM pVM);
VMMR3DECL(int)          SSMR3RegisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t uInstance, uint32_t uVersion, size_t cbGuess, const char *pszBefore,
                                            PFNSSMDEVLIVEPREP pfnLivePrep, PFNSSMDEVLIVEEXEC pfnLiveExec, PFNSSMDEVLIVEVOTE pfnLiveVote,
                                            PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                            PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone);
VMMR3DECL(int)          SSMR3RegisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t uInstance, uint32_t uVersion, size_t cbGuess,
                                            PFNSSMDRVLIVEPREP pfnLivePrep, PFNSSMDRVLIVEEXEC pfnLiveExec, PFNSSMDRVLIVEVOTE pfnLiveVote,
                                            PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
                                            PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone);
VMMR3DECL(int)          SSMR3RegisterInternal(PVM pVM, const char *pszName, uint32_t uInstance, uint32_t uVersion, size_t cbGuess,
                                              PFNSSMINTLIVEPREP pfnLivePrep, PFNSSMINTLIVEEXEC pfnLiveExec, PFNSSMINTLIVEVOTE pfnLiveVote,
                                              PFNSSMINTSAVEPREP pfnSavePrep, PFNSSMINTSAVEEXEC pfnSaveExec, PFNSSMINTSAVEDONE pfnSaveDone,
                                              PFNSSMINTLOADPREP pfnLoadPrep, PFNSSMINTLOADEXEC pfnLoadExec, PFNSSMINTLOADDONE pfnLoadDone);
VMMR3DECL(int)          SSMR3RegisterExternal(PVM pVM, const char *pszName, uint32_t uInstance, uint32_t uVersion, size_t cbGuess,
                                              PFNSSMEXTLIVEPREP pfnLivePrep, PFNSSMEXTLIVEEXEC pfnLiveExec, PFNSSMEXTLIVEVOTE pfnLiveVote,
                                              PFNSSMEXTSAVEPREP pfnSavePrep, PFNSSMEXTSAVEEXEC pfnSaveExec, PFNSSMEXTSAVEDONE pfnSaveDone,
                                              PFNSSMEXTLOADPREP pfnLoadPrep, PFNSSMEXTLOADEXEC pfnLoadExec, PFNSSMEXTLOADDONE pfnLoadDone, void *pvUser);
VMMR3_INT_DECL(int)     SSMR3DeregisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t uInstance);
VMMR3_INT_DECL(int)     SSMR3DeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t uInstance);
VMMR3DECL(int)          SSMR3DeregisterInternal(PVM pVM, const char *pszName);
VMMR3DECL(int)          SSMR3DeregisterExternal(PVM pVM, const char *pszName);
VMMR3DECL(int)          SSMR3Save(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser);
VMMR3_INT_DECL(int)     SSMR3LiveSave(PVM pVM, const char *pszFilename, PCSSMSTRMOPS pStreamOps, void *pvStreamOps,
                                      SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvProgressUser, PSSMHANDLE *ppSSM);
VMMR3_INT_DECL(int)     SSMR3LiveDoStep1(PSSMHANDLE pSSM);
VMMR3_INT_DECL(int)     SSMR3LiveDoStep2(PSSMHANDLE pSSM);
VMMR3_INT_DECL(int)     SSMR3LiveDone(PSSMHANDLE pSSM);
VMMR3DECL(int)          SSMR3Load(PVM pVM, const char *pszFilename, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                                  SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvProgressUser);
VMMR3DECL(int)          SSMR3ValidateFile(const char *pszFilename, bool fChecksumIt);
VMMR3DECL(int)          SSMR3Open(const char *pszFilename, unsigned fFlags, PSSMHANDLE *ppSSM);
VMMR3DECL(int)          SSMR3Close(PSSMHANDLE pSSM);
VMMR3DECL(int)          SSMR3Seek(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion);
VMMR3DECL(int)          SSMR3HandleGetStatus(PSSMHANDLE pSSM);
VMMR3DECL(int)          SSMR3HandleSetStatus(PSSMHANDLE pSSM, int iStatus);
VMMR3DECL(SSMAFTER)     SSMR3HandleGetAfter(PSSMHANDLE pSSM);
VMMR3DECL(bool)         SSMR3HandleIsLiveSave(PSSMHANDLE pSSM);
VMMR3DECL(uint32_t)     SSMR3HandleHostBits(PSSMHANDLE pSSM);
VMMR3_INT_DECL(int)     SSMR3SetGCPtrSize(PSSMHANDLE pSSM, unsigned cbGCPtr);
VMMR3DECL(int)          SSMR3Cancel(PVM pVM);


/** Save operations.
 * @{
 */
VMMR3DECL(int) SSMR3PutStruct(PSSMHANDLE pSSM, const void *pvStruct, PCSSMFIELD paFields);
VMMR3DECL(int) SSMR3PutStructEx(PSSMHANDLE pSSM, const void *pvStruct, size_t cbStruct, uint32_t fFlags, PCSSMFIELD paFields, void *pvUser);
VMMR3DECL(int) SSMR3PutBool(PSSMHANDLE pSSM, bool fBool);
VMMR3DECL(int) SSMR3PutU8(PSSMHANDLE pSSM, uint8_t u8);
VMMR3DECL(int) SSMR3PutS8(PSSMHANDLE pSSM, int8_t i8);
VMMR3DECL(int) SSMR3PutU16(PSSMHANDLE pSSM, uint16_t u16);
VMMR3DECL(int) SSMR3PutS16(PSSMHANDLE pSSM, int16_t i16);
VMMR3DECL(int) SSMR3PutU32(PSSMHANDLE pSSM, uint32_t u32);
VMMR3DECL(int) SSMR3PutS32(PSSMHANDLE pSSM, int32_t i32);
VMMR3DECL(int) SSMR3PutU64(PSSMHANDLE pSSM, uint64_t u64);
VMMR3DECL(int) SSMR3PutS64(PSSMHANDLE pSSM, int64_t i64);
VMMR3DECL(int) SSMR3PutU128(PSSMHANDLE pSSM, uint128_t u128);
VMMR3DECL(int) SSMR3PutS128(PSSMHANDLE pSSM, int128_t i128);
VMMR3DECL(int) SSMR3PutUInt(PSSMHANDLE pSSM, RTUINT u);
VMMR3DECL(int) SSMR3PutSInt(PSSMHANDLE pSSM, RTINT i);
VMMR3DECL(int) SSMR3PutGCUInt(PSSMHANDLE pSSM, RTGCUINT u);
VMMR3DECL(int) SSMR3PutGCUIntReg(PSSMHANDLE pSSM, RTGCUINTREG u);
VMMR3DECL(int) SSMR3PutGCPhys32(PSSMHANDLE pSSM, RTGCPHYS32 GCPhys);
VMMR3DECL(int) SSMR3PutGCPhys64(PSSMHANDLE pSSM, RTGCPHYS64 GCPhys);
VMMR3DECL(int) SSMR3PutGCPhys(PSSMHANDLE pSSM, RTGCPHYS GCPhys);
VMMR3DECL(int) SSMR3PutGCPtr(PSSMHANDLE pSSM, RTGCPTR GCPtr);
VMMR3DECL(int) SSMR3PutGCUIntPtr(PSSMHANDLE pSSM, RTGCUINTPTR GCPtr);
VMMR3DECL(int) SSMR3PutRCPtr(PSSMHANDLE pSSM, RTRCPTR RCPtr);
VMMR3DECL(int) SSMR3PutIOPort(PSSMHANDLE pSSM, RTIOPORT IOPort);
VMMR3DECL(int) SSMR3PutSel(PSSMHANDLE pSSM, RTSEL Sel);
VMMR3DECL(int) SSMR3PutMem(PSSMHANDLE pSSM, const void *pv, size_t cb);
VMMR3DECL(int) SSMR3PutStrZ(PSSMHANDLE pSSM, const char *psz);
/** @} */



/** Load operations.
 * @{
 */
VMMR3DECL(int) SSMR3GetStruct(PSSMHANDLE pSSM, void *pvStruct, PCSSMFIELD paFields);
VMMR3DECL(int) SSMR3GetStructEx(PSSMHANDLE pSSM, void *pvStruct, size_t cbStruct, uint32_t fFlags, PCSSMFIELD paFields, void *pvUser);
VMMR3DECL(int) SSMR3GetBool(PSSMHANDLE pSSM, bool *pfBool);
VMMR3DECL(int) SSMR3GetU8(PSSMHANDLE pSSM, uint8_t *pu8);
VMMR3DECL(int) SSMR3GetS8(PSSMHANDLE pSSM, int8_t *pi8);
VMMR3DECL(int) SSMR3GetU16(PSSMHANDLE pSSM, uint16_t *pu16);
VMMR3DECL(int) SSMR3GetS16(PSSMHANDLE pSSM, int16_t *pi16);
VMMR3DECL(int) SSMR3GetU32(PSSMHANDLE pSSM, uint32_t *pu32);
VMMR3DECL(int) SSMR3GetS32(PSSMHANDLE pSSM, int32_t *pi32);
VMMR3DECL(int) SSMR3GetU64(PSSMHANDLE pSSM, uint64_t *pu64);
VMMR3DECL(int) SSMR3GetS64(PSSMHANDLE pSSM, int64_t *pi64);
VMMR3DECL(int) SSMR3GetU128(PSSMHANDLE pSSM, uint128_t *pu128);
VMMR3DECL(int) SSMR3GetS128(PSSMHANDLE pSSM, int128_t *pi128);
VMMR3DECL(int) SSMR3GetUInt(PSSMHANDLE pSSM, PRTUINT pu);
VMMR3DECL(int) SSMR3GetSInt(PSSMHANDLE pSSM, PRTINT pi);
VMMR3DECL(int) SSMR3GetGCUInt(PSSMHANDLE pSSM, PRTGCUINT pu);
VMMR3DECL(int) SSMR3GetGCUIntReg(PSSMHANDLE pSSM, PRTGCUINTREG pu);
VMMR3DECL(int) SSMR3GetGCPhys32(PSSMHANDLE pSSM, PRTGCPHYS32 pGCPhys);
VMMR3DECL(int) SSMR3GetGCPhys64(PSSMHANDLE pSSM, PRTGCPHYS64 pGCPhys);
VMMR3DECL(int) SSMR3GetGCPhys(PSSMHANDLE pSSM, PRTGCPHYS pGCPhys);
VMMR3DECL(int) SSMR3GetGCPtr(PSSMHANDLE pSSM, PRTGCPTR pGCPtr);
VMMR3DECL(int) SSMR3GetGCUIntPtr(PSSMHANDLE pSSM, PRTGCUINTPTR pGCPtr);
VMMR3DECL(int) SSMR3GetRCPtr(PSSMHANDLE pSSM, PRTRCPTR pRCPtr);
VMMR3DECL(int) SSMR3GetIOPort(PSSMHANDLE pSSM, PRTIOPORT pIOPort);
VMMR3DECL(int) SSMR3GetSel(PSSMHANDLE pSSM, PRTSEL pSel);
VMMR3DECL(int) SSMR3GetMem(PSSMHANDLE pSSM, void *pv, size_t cb);
VMMR3DECL(int) SSMR3GetStrZ(PSSMHANDLE pSSM, char *psz, size_t cbMax);
VMMR3DECL(int) SSMR3GetStrZEx(PSSMHANDLE pSSM, char *psz, size_t cbMax, size_t *pcbStr);
VMMR3DECL(int) SSMR3GetTimer(PSSMHANDLE pSSM, PTMTIMER pTimer);
VMMR3DECL(int) SSMR3Skip(PSSMHANDLE pSSM, size_t cb);
VMMR3DECL(int) SSMR3SkipToEndOfUnit(PSSMHANDLE pSSM);

/** @} */

/** @} */
#endif /* IN_RING3 */


/** @} */

RT_C_DECLS_END

#endif

