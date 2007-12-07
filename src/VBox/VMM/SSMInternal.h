/* $Id$ */
/** @file
 * SSM - Internal header file.
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
 */

#ifndef ___SSMInternal_h
#define ___SSMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/ssm.h>

__BEGIN_DECLS

/** @defgroup grp_ssm_int       Internals
 * @ingroup grp_ssm
 * @internal
 * @{
 */


/**
 * Data unit callback type.
 */
typedef enum SSMUNITTYPE
{
    /** PDM Device . */
    SSMUNITTYPE_DEV = 1,
    /** PDM Driver. */
    SSMUNITTYPE_DRV,
    /** VM Internal. */
    SSMUNITTYPE_INTERNAL,
    /** External Wrapper. */
    SSMUNITTYPE_EXTERNAL
} SSMUNITTYPE;

/** Pointer to a data unit descriptor. */
typedef struct SSMUNIT *PSSMUNIT;

/**
 * Data unit descriptor.
 */
typedef struct SSMUNIT
{
    /** Pointer ot the next one in the list. */
    PSSMUNIT                pNext;

    /** Called in this save/load operation.
     * The flag is used to determin whether there is need for a call to
     * done or not. */
    bool                    fCalled;
    /** Callback interface type. */
    SSMUNITTYPE             enmType;
    /** Type specific data. */
    union
    {
        /** SSMUNITTYPE_DEV. */
        struct
        {
            /** Prepare save. */
            PFNSSMDEVSAVEPREP   pfnSavePrep;
            /** Execute save. */
            PFNSSMDEVSAVEEXEC   pfnSaveExec;
            /** Done save. */
            PFNSSMDEVSAVEDONE   pfnSaveDone;
            /** Prepare load. */
            PFNSSMDEVLOADPREP   pfnLoadPrep;
            /** Execute load. */
            PFNSSMDEVLOADEXEC   pfnLoadExec;
            /** Done load. */
            PFNSSMDEVLOADDONE   pfnLoadDone;
            /** Device instance. */
            PPDMDEVINS          pDevIns;
        } Dev;

        /** SSMUNITTYPE_DRV. */
        struct
        {
            /** Prepare save. */
            PFNSSMDRVSAVEPREP   pfnSavePrep;
            /** Execute save. */
            PFNSSMDRVSAVEEXEC   pfnSaveExec;
            /** Done save. */
            PFNSSMDRVSAVEDONE   pfnSaveDone;
            /** Prepare load. */
            PFNSSMDRVLOADPREP   pfnLoadPrep;
            /** Execute load. */
            PFNSSMDRVLOADEXEC   pfnLoadExec;
            /** Done load. */
            PFNSSMDRVLOADDONE   pfnLoadDone;
            /** Driver instance. */
            PPDMDRVINS          pDrvIns;
        } Drv;

        /** SSMUNITTYPE_INTERNAL. */
        struct
        {
            /** Prepare save. */
            PFNSSMINTSAVEPREP   pfnSavePrep;
            /** Execute save. */
            PFNSSMINTSAVEEXEC   pfnSaveExec;
            /** Done save. */
            PFNSSMINTSAVEDONE   pfnSaveDone;
            /** Prepare load. */
            PFNSSMINTLOADPREP   pfnLoadPrep;
            /** Execute load. */
            PFNSSMINTLOADEXEC   pfnLoadExec;
            /** Done load. */
            PFNSSMINTLOADDONE   pfnLoadDone;
        } Internal;

        /** SSMUNITTYPE_EXTERNAL. */
        struct
        {
            /** Prepare save. */
            PFNSSMEXTSAVEPREP   pfnSavePrep;
            /** Execute save. */
            PFNSSMEXTSAVEEXEC   pfnSaveExec;
            /** Done save. */
            PFNSSMEXTSAVEDONE   pfnSaveDone;
            /** Prepare load. */
            PFNSSMEXTLOADPREP   pfnLoadPrep;
            /** Execute load. */
            PFNSSMEXTLOADEXEC   pfnLoadExec;
            /** Done load. */
            PFNSSMEXTLOADDONE   pfnLoadDone;
            /** User data. */
            void                   *pvUser;
        } External;
    } u;
    /** Data layout version. */
    uint32_t                u32Version;
    /** Instance number. */
    uint32_t                u32Instance;
    /** The guessed size of the data unit - used only for progress indication. */
    size_t                  cbGuess;
    /** Name size. (bytes) */
    size_t                  cchName;
    /** Name of this unit. (extends beyond the defined size) */
    char                    szName[1];
} SSMUNIT;




/**
 * Converts a SSM pointer into a VM pointer.
 * @returns Pointer to the VM structure the SSM is part of.
 * @param   pSSM   Pointer to SSM instance data.
 */
#define SSM2VM(pSSM)  ( (PVM)((char*)pSSM - pSSM->offVM) )


/**
 * SSM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct SSM
{
    /** Offset to the VM structure.
     * See SSM2VM(). */
    RTUINT                  offVM;

    /** FIFO of data entity descriptors. */
    R3PTRTYPE(PSSMUNIT)     pHead;

} SSM;
/** Pointer to SSM VM instance data. */
typedef SSM *PSSM;



/** @} */

__END_DECLS

#endif /* !___SSMInternal_h */

