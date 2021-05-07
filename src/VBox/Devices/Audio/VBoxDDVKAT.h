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

typedef struct PDMDRVREG
{
    /** Driver name. */
    char                szName[32];
    /** Size of the instance data. */
    uint32_t            cbInstance;
    /** Construct instance - required. */
    PFNPDMDRVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    PFNPDMDRVDESTRUCT   pfnDestruct;
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

extern const PDMDRVREG g_DrvVKATPulseAudio;
extern const PDMDRVREG g_DrvVKATAlsa;
extern const PDMDRVREG g_DrvVKATOss;

#endif /* !VBOX_INCLUDED_SRC_Audio_VBoxDDVKAT_h */
