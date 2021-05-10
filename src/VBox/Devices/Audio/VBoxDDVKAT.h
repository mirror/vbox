/* $Id$ */
/** @file
 * Validation Kit Audio Test (VKAT) header for handling the device drivers.
 *
 ** @todo Move / rename this header into ValidationKit/utils/audio/whatever.h?
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_VBoxDDVKAT_h
#define VBOX_INCLUDED_SRC_Audio_VBoxDDVKAT_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdmifs.h>

/**
 * !!! HACK ALERT !!!
 * This totally ignores *any* IIDs for now!
 * !!! HACK ALERT !!!
 */

#ifdef PDMDRV_CHECK_VERSIONS_RETURN_VOID
# undef PDMDRV_CHECK_VERSIONS_RETURN_VOID
#endif
#define PDMDRV_CHECK_VERSIONS_RETURN_VOID(...) do { } while (0)

#ifdef PDMINS_2_DATA
# undef PDMINS_2_DATA
#endif
#define PDMINS_2_DATA(pIns, type)       ( (type)(pIns)->pvInstanceData )

#define PDMIBASE_2_PDMDRV(pInterface)   ( (PPDMDRVINS)((char *)(pInterface) - RT_UOFFSETOF(PDMDRVINS, IBase)) )

#ifdef PDMDRV_CHECK_VERSIONS_RETURN
# undef PDMDRV_CHECK_VERSIONS_RETURN
#endif
#define PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns) do { } while (0)
#define PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, pszValidValues, pszValidNodes) do { } while (0)

typedef struct PDMDRVINS
{
    /** Driver instance number. */
    uint32_t       iInstance;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO  IHostAudio;
    void          *pvInstanceData;
    PDMIBASE       IBase;
} PDMDRVINS;
/** Pointer to a PDM Driver Instance. */
typedef struct PDMDRVINS *PPDMDRVINS;

typedef DECLCALLBACKTYPE(int, FNPDMDRVCONSTRUCT,(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags));
/** Pointer to a FNPDMDRVCONSTRUCT() function. */
typedef FNPDMDRVCONSTRUCT *PFNPDMDRVCONSTRUCT;

typedef DECLCALLBACKTYPE(void, FNPDMDRVDESTRUCT,(PPDMDRVINS pDrvIns));
/** Pointer to a FNPDMDRVDESTRUCT() function. */
typedef FNPDMDRVDESTRUCT *PFNPDMDRVDESTRUCT;

#define PDM_DRVREG_VERSION                 0x0
#define PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT 0x0
#define PDM_DRVREG_CLASS_AUDIO             0x0

typedef struct PDMDRVREG
{
    /** Structure version. PDM_DRVREG_VERSION defines the current version. */
    uint32_t            u32Version;
    /** Driver name. */
    char                szName[32];
    /** Name of the raw-mode context module (no path).
     * Only evalutated if PDM_DRVREG_FLAGS_RC is set. */
    char                szRCMod[32];
    /** Name of the ring-0 module (no path).
     * Only evalutated if PDM_DRVREG_FLAGS_R0 is set. */
    char                szR0Mod[32];
    /** The description of the driver. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    const char         *pszDescription;

    /** Flags, combination of the PDM_DRVREG_FLAGS_* \#defines. */
    uint32_t            fFlags;
    /** Driver class(es), combination of the PDM_DRVREG_CLASS_* \#defines. */
    uint32_t            fClass;
    /** Maximum number of instances (per VM). */
    uint32_t            cMaxInstances;
    /** Size of the instance data. */
    uint32_t            cbInstance;

    /** Construct instance - required. */
    PFNPDMDRVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    PFNPDMDRVDESTRUCT   pfnDestruct;
    /** Relocation command - optional. */
    PFNRT               pfnRelocate;
    /** I/O control - optional. */
    PFNRT               pfnIOCtl;
    /** Power on notification - optional. */
    PFNRT               pfnPowerOn;
    /** Reset notification - optional. */
    PFNRT               pfnReset;
    /** Suspend notification  - optional. */
    PFNRT               pfnSuspend;
    /** Resume notification - optional. */
    PFNRT               pfnResume;
    /** Attach command - optional. */
    PFNRT               pfnAttach;
    /** Detach notification - optional. */
    PFNRT               pfnDetach;
    /** Power off notification - optional. */
    PFNRT               pfnPowerOff;
    /** @todo */
    PFNRT               pfnSoftReset;
    /** Initialization safty marker. */
    uint32_t            u32VersionEnd;
} PDMDRVREG;
/** Pointer to a PDM Driver Structure. */
typedef PDMDRVREG *PPDMDRVREG;

DECLINLINE(int) CFGMR3QueryString(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString)
{
    RT_NOREF(pNode, pszName, pszString, cchString);
    return 0;
}

DECLINLINE(int) CFGMR3QueryStringDef(PCFGMNODE pNode, const char *pszName, char *pszString, size_t cchString, const char *pszDef)
{
    RT_NOREF(pNode, pszName, pszString, cchString, pszDef);
    return 0;
}

#ifdef VBOX_WITH_AUDIO_PULSE
extern const PDMDRVREG g_DrvHostPulseAudio;
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
extern const PDMDRVREG g_DrvHostALSAAudio;
#endif
#ifdef VBOX_WITH_AUDIO_OSS
extern const PDMDRVREG g_DrvHostOSSAudio;
#endif
#if defined(RT_OS_WINDOWS)
extern const PDMDRVREG g_DrvHostAudioWas;
extern const PDMDRVREG g_DrvHostDSound;
#endif
#if defined(RT_OS_DARWIN)
extern const PDMDRVREG g_DrvHostCoreAudio;
#endif

#endif /* !VBOX_INCLUDED_SRC_Audio_VBoxDDVKAT_h */
