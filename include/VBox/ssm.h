/** @file
 * SSM - The Save State Manager.
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

#ifndef ___VBox_ssm_h
#define ___VBox_ssm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/tm.h>
#include <VBox/vmapi.h>

__BEGIN_DECLS

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


#ifdef IN_RING3
/** @defgroup grp_ssm_r3     The SSM Host Context Ring-3 API
 * @{
 */


/**
 * What to do after the save/load operation.
 */
typedef enum SSMAFTER
{
    /** Will resume the loaded state. */
    SSMAFTER_RESUME = 1,
    /** Will destroy the VM after saving. */
    SSMAFTER_DESTROY,
    /** Will continue execution after saving the VM. */
    SSMAFTER_CONTINUE,
    /** Will debug the saved state.
     * This is used to drop some of the stricter consitentcy checks so it'll
     * load fine in the debugger or animator. */
    SSMAFTER_DEBUG_IT,
    /** The file was opened using SSMR3Open() and we have no idea what the plan is. */
    SSMAFTER_OPENED
} SSMAFTER;


/**
 * A structure field description.
 */
typedef struct SSMFIELD
{
    /** Field offset into the structure. */
    uint32_t    off;
    /** The size of the field. */
    uint32_t    cb;
} SSMFIELD;
/** Pointer to a structure field description. */
typedef SSMFIELD *PSSMFIELD;
/** Pointer to a const  structure field description. */
typedef const SSMFIELD *PCSSMFIELD;

/** Emit a SSMFIELD array entry. */
#define SSMFIELD_ENTRY(Type, Field) { RT_OFFSETOF(Type, Field), RT_SIZEOFMEMB(Type, Field) }
/** Emit the terminating entry of a SSMFIELD array. */
#define SSMFIELD_ENTRY_TERM()       { UINT32_MAX, UINT32_MAX }



/** The PDM Device callback variants.
 * @{
 */

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
 * @param   u32Version      Data layout version.
 */
typedef DECLCALLBACK(int) FNSSMDEVLOADEXEC(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t u32Version);
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
 * @param   u32Version      Data layout version.
 */
typedef DECLCALLBACK(int) FNSSMDRVLOADEXEC(PPDMDRVINS pDrvIns, PSSMHANDLE pSSM, uint32_t u32Version);
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
 * @param   u32Version      Data layout version.
 */
typedef DECLCALLBACK(int) FNSSMINTLOADEXEC(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
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
 * @returns 0 on success.
 * @returns Not 0 on failure.
 * @param   pSSM            SSM operation handle.
 * @param   pvUser          User argument.
 * @param   u32Version      Data layout version.
 * @remark  The odd return value is for legacy reasons.
 */
typedef DECLCALLBACK(int) FNSSMEXTLOADEXEC(PSSMHANDLE pSSM, void *pvUser, uint32_t u32Version);
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


SSMR3DECL(int) SSMR3Register(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
    PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone);
SSMR3DECL(int) SSMR3RegisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
    PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone);
SSMR3DECL(int) SSMR3RegisterInternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMINTSAVEPREP pfnSavePrep, PFNSSMINTSAVEEXEC pfnSaveExec, PFNSSMINTSAVEDONE pfnSaveDone,
    PFNSSMINTLOADPREP pfnLoadPrep, PFNSSMINTLOADEXEC pfnLoadExec, PFNSSMINTLOADDONE pfnLoadDone);
SSMR3DECL(int) SSMR3RegisterExternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMEXTSAVEPREP pfnSavePrep, PFNSSMEXTSAVEEXEC pfnSaveExec, PFNSSMEXTSAVEDONE pfnSaveDone,
    PFNSSMEXTLOADPREP pfnLoadPrep, PFNSSMEXTLOADEXEC pfnLoadExec, PFNSSMEXTLOADDONE pfnLoadDone, void *pvUser);
SSMR3DECL(int) SSMR3Deregister(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance);
SSMR3DECL(int) SSMR3DeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance);
SSMR3DECL(int) SSMR3DeregisterInternal(PVM pVM, const char *pszName);
SSMR3DECL(int) SSMR3DeregisterExternal(PVM pVM, const char *pszName);
SSMR3DECL(int) SSMR3Save(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser);
SSMR3DECL(int) SSMR3Load(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser);
SSMR3DECL(int) SSMR3ValidateFile(const char *pszFilename);
SSMR3DECL(int) SSMR3Open(const char *pszFilename, unsigned fFlags, PSSMHANDLE *ppSSM);
SSMR3DECL(int) SSMR3Close(PSSMHANDLE pSSM);
SSMR3DECL(int) SSMR3Seek(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion);
SSMR3DECL(int) SSMR3HandleGetStatus(PSSMHANDLE pSSM);
SSMR3DECL(int) SSMR3HandleSetStatus(PSSMHANDLE pSSM, int iStatus);
SSMR3DECL(SSMAFTER) SSMR3HandleGetAfter(PSSMHANDLE pSSM);


/** Save operations.
 * @{
 */
SSMR3DECL(int) SSMR3PutStruct(PSSMHANDLE pSSM, const void *pvStruct, PCSSMFIELD paFields);
SSMR3DECL(int) SSMR3PutBool(PSSMHANDLE pSSM, bool fBool);
SSMR3DECL(int) SSMR3PutU8(PSSMHANDLE pSSM, uint8_t u8);
SSMR3DECL(int) SSMR3PutS8(PSSMHANDLE pSSM, int8_t i8);
SSMR3DECL(int) SSMR3PutU16(PSSMHANDLE pSSM, uint16_t u16);
SSMR3DECL(int) SSMR3PutS16(PSSMHANDLE pSSM, int16_t i16);
SSMR3DECL(int) SSMR3PutU32(PSSMHANDLE pSSM, uint32_t u32);
SSMR3DECL(int) SSMR3PutS32(PSSMHANDLE pSSM, int32_t i32);
SSMR3DECL(int) SSMR3PutU64(PSSMHANDLE pSSM, uint64_t u64);
SSMR3DECL(int) SSMR3PutS64(PSSMHANDLE pSSM, int64_t i64);
SSMR3DECL(int) SSMR3PutU128(PSSMHANDLE pSSM, uint128_t u128);
SSMR3DECL(int) SSMR3PutS128(PSSMHANDLE pSSM, int128_t i128);
SSMR3DECL(int) SSMR3PutUInt(PSSMHANDLE pSSM, RTUINT u);
SSMR3DECL(int) SSMR3PutSInt(PSSMHANDLE pSSM, RTINT i);
SSMR3DECL(int) SSMR3PutGCUInt(PSSMHANDLE pSSM, RTGCUINT u);
SSMR3DECL(int) SSMR3PutGCSInt(PSSMHANDLE pSSM, RTGCINT i);
SSMR3DECL(int) SSMR3PutGCPhys32(PSSMHANDLE pSSM, RTGCPHYS32 GCPhys);
SSMR3DECL(int) SSMR3PutGCPhys(PSSMHANDLE pSSM, RTGCPHYS GCPhys);
SSMR3DECL(int) SSMR3PutGCPtr(PSSMHANDLE pSSM, RTGCPTR GCPtr);
SSMR3DECL(int) SSMR3PutGCUIntPtr(PSSMHANDLE pSSM, RTGCUINTPTR GCPtr);
SSMR3DECL(int) SSMR3PutHCUInt(PSSMHANDLE pSSM, RTHCUINT u);
SSMR3DECL(int) SSMR3PutHCSInt(PSSMHANDLE pSSM, RTHCINT i);
SSMR3DECL(int) SSMR3PutIOPort(PSSMHANDLE pSSM, RTIOPORT IOPort);
SSMR3DECL(int) SSMR3PutSel(PSSMHANDLE pSSM, RTSEL Sel);
SSMR3DECL(int) SSMR3PutMem(PSSMHANDLE pSSM, const void *pv, size_t cb);
SSMR3DECL(int) SSMR3PutStrZ(PSSMHANDLE pSSM, const char *psz);
/** @} */



/** Load operations.
 * @{
 */
SSMR3DECL(int) SSMR3GetStruct(PSSMHANDLE pSSM, void *pvStruct, PCSSMFIELD paFields);
SSMR3DECL(int) SSMR3GetBool(PSSMHANDLE pSSM, bool *pfBool);
SSMR3DECL(int) SSMR3GetU8(PSSMHANDLE pSSM, uint8_t *pu8);
SSMR3DECL(int) SSMR3GetS8(PSSMHANDLE pSSM, int8_t *pi8);
SSMR3DECL(int) SSMR3GetU16(PSSMHANDLE pSSM, uint16_t *pu16);
SSMR3DECL(int) SSMR3GetS16(PSSMHANDLE pSSM, int16_t *pi16);
SSMR3DECL(int) SSMR3GetU32(PSSMHANDLE pSSM, uint32_t *pu32);
SSMR3DECL(int) SSMR3GetS32(PSSMHANDLE pSSM, int32_t *pi32);
SSMR3DECL(int) SSMR3GetU64(PSSMHANDLE pSSM, uint64_t *pu64);
SSMR3DECL(int) SSMR3GetS64(PSSMHANDLE pSSM, int64_t *pi64);
SSMR3DECL(int) SSMR3GetU128(PSSMHANDLE pSSM, uint128_t *pu128);
SSMR3DECL(int) SSMR3GetS128(PSSMHANDLE pSSM, int128_t *pi128);
SSMR3DECL(int) SSMR3GetUInt(PSSMHANDLE pSSM, PRTUINT pu);
SSMR3DECL(int) SSMR3GetSInt(PSSMHANDLE pSSM, PRTINT pi);
SSMR3DECL(int) SSMR3GetGCUInt(PSSMHANDLE pSSM, PRTGCUINT pu);
SSMR3DECL(int) SSMR3GetGCSInt(PSSMHANDLE pSSM, PRTGCINT pi);
SSMR3DECL(int) SSMR3GetGCPhys32(PSSMHANDLE pSSM, PRTGCPHYS32 pGCPhys);
SSMR3DECL(int) SSMR3GetGCPhys(PSSMHANDLE pSSM, PRTGCPHYS pGCPhys);
SSMR3DECL(int) SSMR3GetGCPtr(PSSMHANDLE pSSM, PRTGCPTR pGCPtr);
SSMR3DECL(int) SSMR3GetGCUIntPtr(PSSMHANDLE pSSM, PRTGCUINTPTR pGCPtr);
SSMR3DECL(int) SSMR3GetIOPort(PSSMHANDLE pSSM, PRTIOPORT pIOPort);
SSMR3DECL(int) SSMR3GetHCUInt(PSSMHANDLE pSSM, PRTHCUINT pu);
SSMR3DECL(int) SSMR3GetHCSInt(PSSMHANDLE pSSM, PRTHCINT pi);
SSMR3DECL(int) SSMR3GetSel(PSSMHANDLE pSSM, PRTSEL pSel);
SSMR3DECL(int) SSMR3GetMem(PSSMHANDLE pSSM, void *pv, size_t cb);
SSMR3DECL(int) SSMR3GetStrZ(PSSMHANDLE pSSM, char *psz, size_t cbMax);
SSMR3DECL(int) SSMR3GetStrZEx(PSSMHANDLE pSSM, char *psz, size_t cbMax, size_t *pcbStr);
SSMR3DECL(int) SSMR3GetTimer(PSSMHANDLE pSSM, PTMTIMER pTimer);

/** @} */

/** @} */
#endif /* IN_RING3 */


/** @} */

__END_DECLS

#endif

