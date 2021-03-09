/* $Id$ */
/** @file
 * PDM - Audio Helpers, Inlined Code. (DEV,++)
 *
 * This is all inlined because it's too tedious to create a couple libraries to
 * contain it all (same bad excuse as for intnetinline.h & pdmnetinline.h).
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vmm_pdmaudiohostenuminline_h
#define VBOX_INCLUDED_vmm_pdmaudiohostenuminline_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/**
 * Allocates an audio device.
 *
 * @returns Newly allocated audio device, or NULL on failure.
 * @param   cb      The total device structure size.   This must be at least the
 *                  size of PDMAUDIOHOSTDEV.  The idea is that the caller extends
 *                  the PDMAUDIOHOSTDEV structure and appends additional data
 *                  after it in its private structure.
 */
DECLINLINE(PPDMAUDIOHOSTDEV) PDMAudioDeviceAlloc(size_t cb)
{
    AssertReturn(cb >= sizeof(PDMAUDIOHOSTDEV), NULL);
    AssertReturn(cb < _4M, NULL);

    PPDMAUDIOHOSTDEV pDev = (PPDMAUDIOHOSTDEV)RTMemAllocZ(RT_ALIGN_Z(cb, 64));
    if (pDev)
    {
        pDev->uMagic = PDMAUDIOHOSTDEV_MAGIC;
        pDev->cbSelf = (uint32_t)cb;
        RTListInit(&pDev->Node);

        //pDev->cMaxInputChannels  = 0;
        //pDev->cMaxOutputChannels = 0;
    }
    return pDev;
}

/**
 * Frees an audio device allocated by PDMAudioDeviceAlloc.
 *
 * @param   pDev    The device to free.  NULL is ignored.
 */
DECLINLINE(void) PDMAudioDeviceFree(PPDMAUDIOHOSTDEV pDev)
{
    if (pDev)
    {
        Assert(pDev->uMagic == PDMAUDIOHOSTDEV_MAGIC);
        Assert(pDev->cRefCount == 0);
        pDev->uMagic = PDMAUDIOHOSTDEV_MAGIC_DEAD;
        pDev->cbSelf = 0;

        RTMemFree(pDev);
    }
}

/**
 * Duplicates an audio device entry.
 *
 * @returns Duplicated audio device entry on success, or NULL on failure.
 * @param   pDev            The audio device enum entry to duplicate.
 * @param   fOnlyCoreData
 */
DECLINLINE(PPDMAUDIOHOSTDEV) PDMAudioDeviceDup(PPDMAUDIOHOSTDEV pDev, bool fOnlyCoreData)
{
    AssertPtrReturn(pDev, NULL);
    Assert(pDev->uMagic == PDMAUDIOHOSTDEV_MAGIC);
    Assert(fOnlyCoreData || !(pDev->fFlags & PDMAUDIOHOSTDEV_F_NO_DUP));

    uint32_t cbToDup = fOnlyCoreData ? sizeof(PDMAUDIOHOSTDEV) : pDev->cbSelf;
    AssertReturn(cbToDup >= sizeof(*pDev), NULL);

    PPDMAUDIOHOSTDEV pDevDup = PDMAudioDeviceAlloc(cbToDup);
    if (pDevDup)
    {
        memcpy(pDevDup, pDev, cbToDup);
        RTListInit(&pDevDup->Node);
        pDev->cbSelf = cbToDup;
    }

    return pDevDup;
}

/**
 * Initializes a host audio device enumeration.
 *
 * @param   pDevEnm     The enumeration to initialize.
 */
DECLINLINE(void) PDMAudioHostEnumInit(PPDMAUDIOHOSTENUM pDevEnm)
{
    AssertPtr(pDevEnm);

    pDevEnm->uMagic   = PDMAUDIOHOSTENUM_MAGIC;
    pDevEnm->cDevices = 0;
    RTListInit(&pDevEnm->LstDevices);
}

/**
 * Deletes the host audio device enumeration and frees all device entries
 * associated with it.
 *
 * The user must call PDMAudioHostEnumInit again to use it again.
 *
 * @param   pDevEnm     The host audio device enumeration to delete.
 */
DECLINLINE(void) PDMAudioHostEnumDelete(PPDMAUDIOHOSTENUM pDevEnm)
{
    if (pDevEnm)
    {
        AssertPtr(pDevEnm);
        AssertReturnVoid(pDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC);

        PPDMAUDIOHOSTDEV pDev, pDevNext;
        RTListForEachSafe(&pDevEnm->LstDevices, pDev, pDevNext, PDMAUDIOHOSTDEV, Node)
        {
            RTListNodeRemove(&pDev->Node);

            PDMAudioDeviceFree(pDev);

            pDevEnm->cDevices--;
        }

        /* Sanity. */
        Assert(RTListIsEmpty(&pDevEnm->LstDevices));
        Assert(pDevEnm->cDevices == 0);

        pDevEnm->uMagic = ~PDMAUDIOHOSTENUM_MAGIC;
    }
}

/**
 * Adds an audio device to a device enumeration.
 *
 * @param  pDevEnm              Device enumeration to add device to.
 * @param  pDev                 Device to add. The pointer will be owned by the device enumeration  then.
 */
DECLINLINE(void) PDMAudioHostEnumAppend(PPDMAUDIOHOSTENUM pDevEnm, PPDMAUDIOHOSTDEV pDev)
{
    AssertPtr(pDevEnm);
    AssertPtr(pDev);
    Assert(pDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC);

    RTListAppend(&pDevEnm->LstDevices, &pDev->Node);
    pDevEnm->cDevices++;
}

/**
 * Appends copies of matching host device entries from one to another enumeration.
 *
 * @returns IPRT status code.
 * @param   pDstDevEnm      The target to append copies of matching device to.
 * @param   pSrcDevEnm      The source to copy matching devices from.
 * @param   enmUsage        The usage to match for copying.
 *                          Use PDMAUDIODIR_INVALID to match all entries.
 * @param   fOnlyCoreData   Set this to only copy the PDMAUDIOHOSTDEV part.
 *                          Careful with passing @c false here as not all
 *                          backends have data that can be copied.
 */
DECLINLINE(int) PDMAudioHostEnumCopy(PPDMAUDIOHOSTENUM pDstDevEnm, PCPDMAUDIOHOSTENUM pSrcDevEnm,
                                     PDMAUDIODIR enmUsage, bool fOnlyCoreData)
{
    AssertPtrReturn(pDstDevEnm, VERR_INVALID_POINTER);
    AssertReturn(pDstDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC, VERR_WRONG_ORDER);

    AssertPtrReturn(pSrcDevEnm, VERR_INVALID_POINTER);
    AssertReturn(pSrcDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC, VERR_WRONG_ORDER);

    PPDMAUDIOHOSTDEV pSrcDev;
    RTListForEach(&pSrcDevEnm->LstDevices, pSrcDev, PDMAUDIOHOSTDEV, Node)
    {
        if (   enmUsage == pSrcDev->enmUsage
            || enmUsage == PDMAUDIODIR_INVALID /*all*/)
        {
            PPDMAUDIOHOSTDEV pDstDev = PDMAudioDeviceDup(pSrcDev, fOnlyCoreData);
            AssertReturn(pDstDev, VERR_NO_MEMORY);

            PDMAudioHostEnumAppend(pDstDevEnm, pDstDev);
        }
    }

    return VINF_SUCCESS;
}

/**
 * Get the default device with the given usage.
 *
 * This assumes that only one default device per usage is set, if there should
 * be more than one, the first one is returned.
 *
 * @returns Default device if found, or NULL if not.
 * @param   pDevEnm     Device enumeration to get default device for.
 * @param   enmUsage    Usage to get default device for.
 *                      Pass PDMAUDIODIR_INVALID to get the first device with
 *                      PDMAUDIOHOSTDEV_F_DEFAULT set.
 */
DECLINLINE(PPDMAUDIOHOSTDEV) PDMAudioHostEnumGetDefault(PCPDMAUDIOHOSTENUM pDevEnm, PDMAUDIODIR enmUsage)
{
    AssertPtrReturn(pDevEnm, NULL);
    AssertReturn(pDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC, NULL);

    PPDMAUDIOHOSTDEV pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIOHOSTDEV, Node)
    {
        if (pDev->fFlags & PDMAUDIOHOSTDEV_F_DEFAULT)
        {
            if (   enmUsage == pDev->enmUsage
                || enmUsage == PDMAUDIODIR_INVALID)
                return pDev;
        }
    }

    return NULL;
}

/**
 * Get the number of device with the given usage.
 *
 * @returns Number of matching devices.
 * @param   pDevEnm     Device enumeration to get default device for.
 * @param   enmUsage    Usage to count devices for.
 *                      Pass PDMAUDIODIR_INVALID to get the total number of devices.
 */
DECLINLINE(uint32_t) PDMAudioHostEnumCountMatching(PCPDMAUDIOHOSTENUM pDevEnm, PDMAUDIODIR enmUsage)
{
    AssertPtrReturn(pDevEnm, 0);
    AssertReturn(pDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC, 0);

    if (enmUsage == PDMAUDIODIR_INVALID)
        return pDevEnm->cDevices;

    uint32_t        cDevs = 0;
    PPDMAUDIOHOSTDEV pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIOHOSTDEV, Node)
    {
        if (enmUsage == pDev->enmUsage)
            cDevs++;
    }

    return cDevs;
}

/** The max string length for all PDMAUDIOHOSTDEV_F_XXX.
 * @sa PDMAudioDeviceFlagsToString */
#define PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN    (7 * 8)

/**
 * Converts an audio device flags to a string.
 *
 * @returns
 * @param   pszDst      Destination buffer with a size of at least
 *                      PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN bytes (including
 *                      the string terminator).
 * @param   fFlags      Audio flags (PDMAUDIOHOSTDEV_F_XXX) to convert.
 */
DECLINLINE(const char *) PDMAudioDeviceFlagsToString(char pszDst[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN], uint32_t fFlags)
{
    static const struct { const char *pszMnemonic; uint32_t cchMnemonic; uint32_t fFlag; } s_aFlags[] =
    {
        { RT_STR_TUPLE("DEFAULT "), PDMAUDIOHOSTDEV_F_DEFAULT },
        { RT_STR_TUPLE("HOTPLUG "), PDMAUDIOHOSTDEV_F_HOTPLUG },
        { RT_STR_TUPLE("BUGGY "),   PDMAUDIOHOSTDEV_F_BUGGY   },
        { RT_STR_TUPLE("IGNORE "),  PDMAUDIOHOSTDEV_F_IGNORE  },
        { RT_STR_TUPLE("LOCKED "),  PDMAUDIOHOSTDEV_F_LOCKED  },
        { RT_STR_TUPLE("DEAD "),    PDMAUDIOHOSTDEV_F_DEAD    },
        { RT_STR_TUPLE("NO_DUP "),  PDMAUDIOHOSTDEV_F_NO_DUP  },
    };
    size_t offDst = 0;
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFlags); i++)
        if (fFlags & s_aFlags[i].fFlag)
        {
            fFlags &= ~s_aFlags[i].fFlag;
            memcpy(&pszDst[offDst], s_aFlags[i].pszMnemonic, s_aFlags[i].cchMnemonic);
            offDst += s_aFlags[i].cchMnemonic;
        }
    Assert(fFlags == 0);
    Assert(offDst < PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN);

    if (offDst)
        pszDst[offDst - 1] = '\0';
    else
        memcpy(pszDst, "NONE", sizeof("NONE"));
    return pszDst;
}

/**
 * Logs an audio device enumeration.
 *
 * @param  pDevEnm  Device enumeration to log.
 * @param  pszDesc  Logging description (prefix).
 */
DECLINLINE(void) PDMAudioHostEnumLog(PCPDMAUDIOHOSTENUM pDevEnm, const char *pszDesc)
{
    AssertPtrReturnVoid(pDevEnm);
    AssertPtrReturnVoid(pszDesc);
    AssertReturnVoid(pDevEnm->uMagic == PDMAUDIOHOSTENUM_MAGIC);

    LogFunc(("%s: %RU32 devices\n", pszDesc, pDevEnm->cDevices));

    PPDMAUDIOHOSTDEV pDev;
    RTListForEach(&pDevEnm->LstDevices, pDev, PDMAUDIOHOSTDEV, Node)
    {
        char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
        LogFunc(("Device '%s':\n", pDev->szName));
        LogFunc(("  Usage           = %s\n",             PDMAudioDirGetName(pDev->enmUsage)));
        LogFunc(("  Flags           = %s\n",             PDMAudioDeviceFlagsToString(szFlags, pDev->fFlags)));
        LogFunc(("  Input channels  = %RU8\n",           pDev->cMaxInputChannels));
        LogFunc(("  Output channels = %RU8\n",           pDev->cMaxOutputChannels));
        LogFunc(("  cbExtra         = %RU32 bytes\n",    pDev->cbSelf - sizeof(PDMAUDIOHOSTDEV)));
    }
}

#endif /* !VBOX_INCLUDED_vmm_pdmaudiohostenuminline_h */
