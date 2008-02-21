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



/**
 * Register a PDM Devices data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
SSMR3DECL(int) SSMR3Register(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
    PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone);

/**
 * Register a PDM driver data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pDrvIns         Driver instance.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
SSMR3DECL(int) SSMR3RegisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
    PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone);

/**
 * Register a internal data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 */
SSMR3DECL(int) SSMR3RegisterInternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMINTSAVEPREP pfnSavePrep, PFNSSMINTSAVEEXEC pfnSaveExec, PFNSSMINTSAVEDONE pfnSaveDone,
    PFNSSMINTLOADPREP pfnLoadPrep, PFNSSMINTLOADEXEC pfnLoadExec, PFNSSMINTLOADDONE pfnLoadDone);

/**
 * Register a  unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique.
 * @param   u32Version      Data layout version number.
 * @param   cbGuess         The approximate amount of data in the unit.
 *                          Only for progress indicators.
 * @param   pfnSavePrep     Prepare save callback, optional.
 * @param   pfnSaveExec     Execute save callback, optional.
 * @param   pfnSaveDone     Done save callback, optional.
 * @param   pfnLoadPrep     Prepare load callback, optional.
 * @param   pfnLoadExec     Execute load callback, optional.
 * @param   pfnLoadDone     Done load callback, optional.
 * @param   pvUser          User argument.
 */
SSMR3DECL(int) SSMR3RegisterExternal(PVM pVM, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
    PFNSSMEXTSAVEPREP pfnSavePrep, PFNSSMEXTSAVEEXEC pfnSaveExec, PFNSSMEXTSAVEDONE pfnSaveDone,
    PFNSSMEXTLOADPREP pfnLoadPrep, PFNSSMEXTLOADEXEC pfnLoadExec, PFNSSMEXTLOADDONE pfnLoadDone, void *pvUser);

/**
 * Deregister one or more PDM Device data units.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pDevIns         Device instance.
 * @param   pszName         Data unit name.
 *                          Use NULL to deregister all data units for that device instance.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique. Ignored if pszName is NULL.
 * @remark  Only for dynmaic data units and dynamic unloaded modules.
 */
SSMR3DECL(int) SSMR3Deregister(PVM pVM, PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance);

/**
 * Deregister one or more PDM Driver data units.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pDrvIns         Driver instance.
 * @param   pszName         Data unit name.
 *                          Use NULL to deregister all data units for that driver instance.
 * @param   u32Instance     The instance identifier of the data unit.
 *                          This must together with the name be unique. Ignored if pszName is NULL.
 * @remark  Only for dynmaic data units and dynamic unloaded modules.
 */
SSMR3DECL(int) SSMR3DeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance);

/**
 * Deregister a internal data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @remark  Only for dynmaic data units.
 */
SSMR3DECL(int) SSMR3DeregisterInternal(PVM pVM, const char *pszName);

/**
 * Deregister an external data unit.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszName         Data unit name.
 * @remark  Only for dynmaic data units.
 */
SSMR3DECL(int) SSMR3DeregisterExternal(PVM pVM, const char *pszName);

/**
 * Start VM save operation.
 *
 * The caller must be the emulation thread!
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszFilename     Name of the file to save the state in.
 * @param   enmAfter        What is planned after a successful save operation.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 */
SSMR3DECL(int) SSMR3Save(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Load VM save operation.
 *
 * The caller must be the emulation thread!
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pszFilename     Name of the file to save the state in.
 * @param   enmAfter        What is planned after a successful load operation.
 *                          Only acceptable values are SSMAFTER_RESUME and SSMAFTER_DEBUG_IT.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 */
SSMR3DECL(int) SSMR3Load(PVM pVM, const char *pszFilename, SSMAFTER enmAfter, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Validates a file as a validate SSM saved state.
 *
 * This will only verify the file format, the format and content of individual
 * data units are not inspected.
 *
 * @returns VINF_SUCCESS if valid.
 * @returns VBox status code on other failures.
 * @param   pszFilename     The path to the file to validate.
 */
SSMR3DECL(int) SSMR3ValidateFile(const char *pszFilename);

/**
 * Opens a saved state file for reading.
 *
 * @returns VBox status code.
 * @param   pszFilename     The path to the saved state file.
 * @param   fFlags          Open flags. Reserved, must be 0.
 * @param   ppSSM           Where to store the SSM handle.
 */
SSMR3DECL(int) SSMR3Open(const char *pszFilename, unsigned fFlags, PSSMHANDLE *ppSSM);

/**
 * Closes a saved state file opened by SSMR3Open().
 *
 * @returns VBox status code.
 * @param   pSSM            The SSM handle returned by SSMR3Open().
 */
SSMR3DECL(int) SSMR3Close(PSSMHANDLE pSSM);

/**
 * Seeks to a specific data unit.
 *
 * After seeking it's possible to use the getters to on
 * that data unit.
 *
 * @returns VBox status code.
 * @returns VERR_SSM_UNIT_NOT_FOUND if the unit+instance wasn't found.
 * @param   pSSM            The SSM handle returned by SSMR3Open().
 * @param   pszUnit         The name of the data unit.
 * @param   iInstance       The instance number.
 * @param   piVersion       Where to store the version number. (Optional)
 */
SSMR3DECL(int) SSMR3Seek(PSSMHANDLE pSSM, const char *pszUnit, uint32_t iInstance, uint32_t *piVersion);


/** Save operations.
 * @{
 */

/**
 * Puts a structure.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 * @param   pvStruct        The structure address.
 * @param   paFields        The array of structure fields descriptions.
 *                          The array must be terminated by a SSMFIELD_ENTRY_TERM().
 */
SSMR3DECL(int) SSMR3PutStruct(PSSMHANDLE pSSM, const void *pvStruct, PCSSMFIELD paFields);

/**
 * Saves a boolean item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   fBool           Item to save.
 */
SSMR3DECL(int) SSMR3PutBool(PSSMHANDLE pSSM, bool fBool);

/**
 * Saves a 8-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u8              Item to save.
 */
SSMR3DECL(int) SSMR3PutU8(PSSMHANDLE pSSM, uint8_t u8);

/**
 * Saves a 8-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i8              Item to save.
 */
SSMR3DECL(int) SSMR3PutS8(PSSMHANDLE pSSM, int8_t i8);

/**
 * Saves a 16-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u16             Item to save.
 */
SSMR3DECL(int) SSMR3PutU16(PSSMHANDLE pSSM, uint16_t u16);

/**
 * Saves a 16-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i16             Item to save.
 */
SSMR3DECL(int) SSMR3PutS16(PSSMHANDLE pSSM, int16_t i16);

/**
 * Saves a 32-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u32             Item to save.
 */
SSMR3DECL(int) SSMR3PutU32(PSSMHANDLE pSSM, uint32_t u32);

/**
 * Saves a 32-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i32             Item to save.
 */
SSMR3DECL(int) SSMR3PutS32(PSSMHANDLE pSSM, int32_t i32);

/**
 * Saves a 64-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u64             Item to save.
 */
SSMR3DECL(int) SSMR3PutU64(PSSMHANDLE pSSM, uint64_t u64);

/**
 * Saves a 64-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i64             Item to save.
 */
SSMR3DECL(int) SSMR3PutS64(PSSMHANDLE pSSM, int64_t i64);

/**
 * Saves a 128-bit unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u128            Item to save.
 */
SSMR3DECL(int) SSMR3PutU128(PSSMHANDLE pSSM, uint128_t u128);

/**
 * Saves a 128-bit signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i128            Item to save.
 */
SSMR3DECL(int) SSMR3PutS128(PSSMHANDLE pSSM, int128_t i128);

/**
 * Saves a VBox unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 */
SSMR3DECL(int) SSMR3PutUInt(PSSMHANDLE pSSM, RTUINT u);

/**
 * Saves a VBox signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i               Item to save.
 */
SSMR3DECL(int) SSMR3PutSInt(PSSMHANDLE pSSM, RTINT i);

/**
 * Saves a GC natural unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 */
SSMR3DECL(int) SSMR3PutGCUInt(PSSMHANDLE pSSM, RTGCUINT u);

/**
 * Saves a GC natural signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i               Item to save.
 */
SSMR3DECL(int) SSMR3PutGCSInt(PSSMHANDLE pSSM, RTGCINT i);

/**
 * Saves a 32 bits GC physical address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPhys          The item to save
 */
SSMR3DECL(int) SSMR3PutGCPhys32(PSSMHANDLE pSSM, RTGCPHYS32 GCPhys);

/**
 * Saves a GC physical address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPhys          The item to save
 */
SSMR3DECL(int) SSMR3PutGCPhys(PSSMHANDLE pSSM, RTGCPHYS GCPhys);

/**
 * Saves a GC virtual address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPtr           The item to save.
 */
SSMR3DECL(int) SSMR3PutGCPtr(PSSMHANDLE pSSM, RTGCPTR GCPtr);

/**
 * Saves a GC virtual address (represented as an unsigned integer) item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   GCPtr           The item to save.
 */
SSMR3DECL(int) SSMR3PutGCUIntPtr(PSSMHANDLE pSSM, RTGCUINTPTR GCPtr);

/**
 * Saves a HC natural unsigned integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   u               Item to save.
 */
SSMR3DECL(int) SSMR3PutHCUInt(PSSMHANDLE pSSM, RTHCUINT u);

/**
 * Saves a HC natural signed integer item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   i               Item to save.
 */
SSMR3DECL(int) SSMR3PutHCSInt(PSSMHANDLE pSSM, RTHCINT i);

/**
 * Saves a I/O port address item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   IOPort          The item to save.
 */
SSMR3DECL(int) SSMR3PutIOPort(PSSMHANDLE pSSM, RTIOPORT IOPort);

/**
 * Saves a selector item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   Sel             The item to save.
 */
SSMR3DECL(int) SSMR3PutSel(PSSMHANDLE pSSM, RTSEL Sel);

/**
 * Saves a memory item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pv              Item to save.
 * @param   cb              Size of the item.
 */
SSMR3DECL(int) SSMR3PutMem(PSSMHANDLE pSSM, const void *pv, size_t cb);

/**
 * Saves a zero terminated string item to the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Item to save.
 */
SSMR3DECL(int) SSMR3PutStrZ(PSSMHANDLE pSSM, const char *psz);

/** @} */



/** Load operations.
 * @{
 */

/**
 * Gets a structure.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 * @param   pvStruct        The structure address.
 * @param   paFields        The array of structure fields descriptions.
 *                          The array must be terminated by a SSMFIELD_ENTRY_TERM().
 */
SSMR3DECL(int) SSMR3GetStruct(PSSMHANDLE pSSM, void *pvStruct, PCSSMFIELD paFields);

/**
 * Loads a boolean item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pfBool          Where to store the item.
 */
SSMR3DECL(int) SSMR3GetBool(PSSMHANDLE pSSM, bool *pfBool);

/**
 * Loads a 8-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu8             Where to store the item.
 */
SSMR3DECL(int) SSMR3GetU8(PSSMHANDLE pSSM, uint8_t *pu8);

/**
 * Loads a 8-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi8             Where to store the item.
 */
SSMR3DECL(int) SSMR3GetS8(PSSMHANDLE pSSM, int8_t *pi8);

/**
 * Loads a 16-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu16            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetU16(PSSMHANDLE pSSM, uint16_t *pu16);

/**
 * Loads a 16-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi16            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetS16(PSSMHANDLE pSSM, int16_t *pi16);

/**
 * Loads a 32-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu32            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetU32(PSSMHANDLE pSSM, uint32_t *pu32);

/**
 * Loads a 32-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi32            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetS32(PSSMHANDLE pSSM, int32_t *pi32);

/**
 * Loads a 64-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu64            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetU64(PSSMHANDLE pSSM, uint64_t *pu64);

/**
 * Loads a 64-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi64            Where to store the item.
 */
SSMR3DECL(int) SSMR3GetS64(PSSMHANDLE pSSM, int64_t *pi64);

/**
 * Loads a 128-bit unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu128           Where to store the item.
 */
SSMR3DECL(int) SSMR3GetU128(PSSMHANDLE pSSM, uint128_t *pu128);

/**
 * Loads a 128-bit signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi128           Where to store the item.
 */
SSMR3DECL(int) SSMR3GetS128(PSSMHANDLE pSSM, int128_t *pi128);

/**
 * Loads a VBox unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetUInt(PSSMHANDLE pSSM, PRTUINT pu);

/**
 * Loads a VBox signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetSInt(PSSMHANDLE pSSM, PRTINT pi);

/**
 * Loads a GC natural unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetGCUInt(PSSMHANDLE pSSM, PRTGCUINT pu);

/**
 * Loads a GC natural signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetGCSInt(PSSMHANDLE pSSM, PRTGCINT pi);

/**
 * Loads a GC physical address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPhys         Where to store the GC physical address.
 */
SSMR3DECL(int) SSMR3GetGCPhys32(PSSMHANDLE pSSM, PRTGCPHYS32 pGCPhys);

/**
 * Loads a GC physical address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPhys         Where to store the GC physical address.
 */
SSMR3DECL(int) SSMR3GetGCPhys(PSSMHANDLE pSSM, PRTGCPHYS pGCPhys);

/**
 * Loads a GC virtual address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPtr          Where to store the GC virtual address.
 */
SSMR3DECL(int) SSMR3GetGCPtr(PSSMHANDLE pSSM, PRTGCPTR pGCPtr);

/**
 * Loads a GC virtual address (represented as unsigned integer) item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pGCPtr          Where to store the GC virtual address.
 */
SSMR3DECL(int) SSMR3GetGCUIntPtr(PSSMHANDLE pSSM, PRTGCUINTPTR pGCPtr);

/**
 * Loads a I/O port address item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pIOPort         Where to store the I/O port address.
 */
SSMR3DECL(int) SSMR3GetIOPort(PSSMHANDLE pSSM, PRTIOPORT pIOPort);

/**
 * Loads a HC natural unsigned integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pu              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetHCUInt(PSSMHANDLE pSSM, PRTHCUINT pu);

/**
 * Loads a HC natural signed integer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pi              Where to store the integer.
 */
SSMR3DECL(int) SSMR3GetHCSInt(PSSMHANDLE pSSM, PRTHCINT pi);

/**
 * Loads a selector item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pSel            Where to store the selector.
 */
SSMR3DECL(int) SSMR3GetSel(PSSMHANDLE pSSM, PRTSEL pSel);

/**
 * Loads a memory item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   pv              Where to store the item.
 * @param   cb              Size of the item.
 */
SSMR3DECL(int) SSMR3GetMem(PSSMHANDLE pSSM, void *pv, size_t cb);

/**
 * Loads a string item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Where to store the item.
 * @param   cbMax           Max size of the item (including '\\0').
 */
SSMR3DECL(int) SSMR3GetStrZ(PSSMHANDLE pSSM, char *psz, size_t cbMax);

/**
 * Loads a string item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   psz             Where to store the item.
 * @param   cbMax           Max size of the item (including '\\0').
 * @param   pcbStr          The length of the loaded string excluding the '\\0'. (optional)
 */
SSMR3DECL(int) SSMR3GetStrZEx(PSSMHANDLE pSSM, char *psz, size_t cbMax, size_t *pcbStr);

/**
 * Loads a timer item from the current data unit.
 *
 * @returns VBox status.
 * @param   pSSM            SSM operation handle.
 * @param   PTMTIMER        Where to store the item.
 */
SSMR3DECL(int) SSMR3GetTimer(PSSMHANDLE pSSM, PTMTIMER pTimer);

/** @} */



/**
 * Query what the VBox status code of the operation is.
 *
 * This can be used for putting and getting a batch of values
 * without bother checking the result till all the calls have
 * been made.
 *
 * @returns VBox status code.
 * @param   pSSM            SSM operation handle.
 */
SSMR3DECL(int) SSMR3HandleGetStatus(PSSMHANDLE pSSM);

/**
 * Fail the load operation.
 *
 * This is mainly intended for sub item loaders (like timers) which
 * return code isn't necessarily heeded by the caller but is important
 * to SSM.
 *
 * @returns SSMAFTER enum value.
 * @param   pSSM            SSM operation handle.
 * @param   iStatus         Failure status code. This MUST be a VERR_*.
 */
SSMR3DECL(int) SSMR3HandleSetStatus(PSSMHANDLE pSSM, int iStatus);

/**
 * Query what to do after this operation.
 *
 * @returns SSMAFTER enum value.
 * @param   pSSM            SSM operation handle.
 */
SSMR3DECL(SSMAFTER) SSMR3HandleGetAfter(PSSMHANDLE pSSM);

/** @} */
#endif /* IN_RING3 */


/** @} */

__END_DECLS

#endif

