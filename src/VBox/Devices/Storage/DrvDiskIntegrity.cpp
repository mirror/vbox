/* $Id$ */
/** @file
 * VBox storage devices: Disk integrity check.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_DISK_INTEGRITY
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/avl.h>
#include <iprt/mem.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Disk segment.
 */
typedef struct DRVDISKSEGMENT
{
    /** AVL core. */
    AVLRFOFFNODECORE Core;
    /** Size of the segment */
    size_t           cbSeg;
    /** Data for this segment */
    uint8_t         *pbSeg;
} DRVDISKSEGMENT, *PDRVDISKSEGMENT;

/**
 * Disk integrity driver instance data.
 *
 * @implements  PDMIMEDIA
 */
typedef struct DRVDISKINTEGRITY
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to the media driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIA              pDrvMedia;
    /** Our media interface */
    PDMIMEDIA               IMedia;

    /** AVL tree containing the disk blocks to check. */
    PAVLRFOFFTREE           pTreeSegments;
} DRVDISKINTEGRITY, *PDRVDISKINTEGRITY;


/* -=-=-=-=- IMedia -=-=-=-=- */

/** Makes a PDRVDISKINTEGRITY out of a PPDMIMEDIA. */
#define PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface)        ( (PDRVDISKINTEGRITY)((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMedia)) )

/*******************************************************************************
*   Media interface methods                                                    *
*******************************************************************************/

/** @copydoc PDMIMEDIA::pfnRead */
static DECLCALLBACK(int) drvdiskintRead(PPDMIMEDIA pInterface,
                                        uint64_t off, void *pvBuf, size_t cbRead)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    int rc = pThis->pDrvMedia->pfnRead(pThis->pDrvMedia, off, pvBuf, cbRead);
    if (RT_FAILURE(rc))
        return rc;

    /* Compare read data */
    size_t cbLeft   = cbRead;
    RTFOFF offCurr  = (RTFOFF)off;
    uint8_t *pbBuf  = (uint8_t *)pvBuf;

    while (cbLeft)
    {
        PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetRangeGet(pThis->pTreeSegments, offCurr);
        size_t cbRange  = 0;
        bool fCmp       = false;
        unsigned offSeg = 0;

        if (!pSeg)
        {
            /* Get next segment */
            pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetGetBestFit(pThis->pTreeSegments, offCurr, true);
            if (!pSeg)
            {
                /* No data in the tree for this read. Assume everything is ok. */
                cbRange = cbLeft;
            }
            else if (offCurr + (RTFOFF)cbLeft <= pSeg->Core.Key)
                cbRange = cbLeft;
            else
                cbRange = pSeg->Core.Key - offCurr;
        }
        else
        {
            fCmp    = true;
            offSeg  = offCurr - pSeg->Core.Key;
            cbRange = RT_MIN(cbLeft, pSeg->cbSeg - offCurr);
        }

        if (   fCmp
            && memcmp(pbBuf, pSeg->pbSeg + offSeg, cbRange))
        {
            unsigned offWrong = 0;
            for (offWrong = 0; offWrong < cbRange; offWrong++)
                if (pbBuf[offWrong] != pSeg->pbSeg[offSeg + offWrong])
                    AssertMsgFailed(("Corrupted disk at offset %llu (%u bytes in the current read buffer)!\n",
                                     offCurr + offWrong, offWrong));
        }

        offCurr += cbRange;
        cbLeft  -= cbRange;
        pbBuf   += cbRange;
    }

    return rc;
}

/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvdiskintWrite(PPDMIMEDIA pInterface,
                                         uint64_t off, const void *pvBuf,
                                         size_t cbWrite)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    int rc = pThis->pDrvMedia->pfnWrite(pThis->pDrvMedia, off, pvBuf, cbWrite);
    if (RT_FAILURE(rc))
        return rc;

    /* Update the segments */
    size_t cbLeft   = cbWrite;
    RTFOFF offCurr  = (RTFOFF)off;
    uint8_t *pbBuf  = (uint8_t *)pvBuf;

    while (cbLeft)
    {
        PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetRangeGet(pThis->pTreeSegments, offCurr);
        size_t cbRange  = 0;
        bool fSet       = false;
        unsigned offSeg = 0;

        if (!pSeg)
        {
            /* Get next segment */
            pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetGetBestFit(pThis->pTreeSegments, offCurr, true);
            if (   !pSeg
                || offCurr + (RTFOFF)cbLeft <= pSeg->Core.Key)
                cbRange = cbLeft;
            else
                cbRange = pSeg->Core.Key - offCurr;

            /* Create new segment */
            pSeg = (PDRVDISKSEGMENT)RTMemAllocZ(sizeof(DRVDISKSEGMENT));
            if (pSeg)
            {
                pSeg->Core.Key     = offCurr;
                pSeg->Core.KeyLast = offCurr + (RTFOFF)cbRange - 1;
                pSeg->cbSeg        = cbRange;
                pSeg->pbSeg        = (uint8_t *)RTMemAllocZ(cbRange);
                if (!pSeg->pbSeg)
                    RTMemFree(pSeg);
                else
                {
                    bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSeg->Core);
                    AssertMsg(fInserted, ("Bug!\n"));
                    fSet = true;
                }
            }
        }
        else
        {
            fSet    = true;
            offSeg  = offCurr - pSeg->Core.Key;
            cbRange = RT_MIN(cbLeft, pSeg->cbSeg - offCurr);
        }

        if (fSet)
        {
            AssertPtr(pSeg);
            memcpy(pSeg->pbSeg + offSeg, pbBuf, cbRange);
        }

        offCurr += cbRange;
        cbLeft  -= cbRange;
        pbBuf   += cbRange;
    }

    return rc;
}

/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvdiskintFlush(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnFlush(pThis->pDrvMedia);
}

/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvdiskintGetSize(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnGetSize(pThis->pDrvMedia);
}

/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvdiskintIsReadOnly(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnIsReadOnly(pThis->pDrvMedia);
}

/** @copydoc PDMIMEDIA::pfnBiosGetPCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosGetPCHSGeometry(PPDMIMEDIA pInterface,
                                                       PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosGetPCHSGeometry(pThis->pDrvMedia, pPCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosSetPCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosSetPCHSGeometry(PPDMIMEDIA pInterface,
                                                       PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosSetPCHSGeometry(pThis->pDrvMedia, pPCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosGetLCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosGetLCHSGeometry(PPDMIMEDIA pInterface,
                                                       PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosGetLCHSGeometry(pThis->pDrvMedia, pLCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosSetLCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosSetLCHSGeometry(PPDMIMEDIA pInterface,
                                                  PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosSetLCHSGeometry(pThis->pDrvMedia, pLCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvdiskintGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnGetUuid(pThis->pDrvMedia, pUuid);
}

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvdiskintQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIA, &pThis->IMedia);
    return NULL;
}


/* -=-=-=-=- driver interface -=-=-=-=- */

static int drvdiskintTreeDestroy(PAVLRFOFFNODECORE pNode, void *pvUser)
{
    PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)pNode;

    RTMemFree(pSeg->pbSeg);
    RTMemFree(pSeg);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDRVDESTRUCT
 */
static DECLCALLBACK(void) drvdiskintDestruct(PPDMDRVINS pDrvIns)
{
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);

    if (pThis->pTreeSegments)
    {
        RTAvlrFileOffsetDestroy(pThis->pTreeSegments, drvdiskintTreeDestroy, NULL);
        RTMemFree(pThis->pTreeSegments);
    }
}

/**
 * Construct a disk integrity driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvdiskintConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);
    LogFlow(("drvdiskintConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, ""))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Initialize most of the data members.
     */
    pThis->pDrvIns                       = pDrvIns;

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface     = drvdiskintQueryInterface;

    /* IMedia */
    pThis->IMedia.pfnRead                = drvdiskintRead;
    pThis->IMedia.pfnWrite               = drvdiskintWrite;
    pThis->IMedia.pfnFlush               = drvdiskintFlush;
    pThis->IMedia.pfnGetSize             = drvdiskintGetSize;
    pThis->IMedia.pfnIsReadOnly          = drvdiskintIsReadOnly;
    pThis->IMedia.pfnBiosGetPCHSGeometry = drvdiskintBiosGetPCHSGeometry;
    pThis->IMedia.pfnBiosSetPCHSGeometry = drvdiskintBiosSetPCHSGeometry;
    pThis->IMedia.pfnBiosGetLCHSGeometry = drvdiskintBiosGetLCHSGeometry;
    pThis->IMedia.pfnBiosSetLCHSGeometry = drvdiskintBiosSetLCHSGeometry;
    pThis->IMedia.pfnGetUuid             = drvdiskintGetUuid;

    /*
     * Try attach driver below and query it's media interface.
     */
    PPDMIBASE pBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBase);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to attach driver below us! %Rrf"), rc);

    pThis->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIA);
    if (!pThis->pDrvMedia)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("No media or async media interface below"));

    /** Create the AVL tree. */
    pThis->pTreeSegments = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
    if (!pThis->pTreeSegments)
        rc = VERR_NO_MEMORY;

    return VINF_SUCCESS;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvDiskIntegrity =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "DiskIntegrity",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Disk integrity driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVDISKINTEGRITY),
    /* pfnConstruct */
    drvdiskintConstruct,
    /* pfnDestruct */
    drvdiskintDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

