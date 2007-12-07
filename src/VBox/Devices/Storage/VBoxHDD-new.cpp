/** $Id$ */
/** @file
 * VBox HDD Container implementation.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD
#include <VBox/VBoxHDD-new.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/dir.h>
#include <iprt/path.h>

#include "VBoxHDD-newInternal.h"


#define VBOXHDDDISK_SIGNATURE 0x6f0e2a7d


/**
 * VBox HDD Container image descriptor.
 */
typedef struct VDIMAGE
{
    /** Link to parent image descriptor, if any. */
    struct VDIMAGE  *pPrev;
    /** Link to child image descriptor, if any. */
    struct VDIMAGE  *pNext;
    /** Container base filename. (UTF-8) */
    char            *pszFilename;
    /** Data managed by the backend which keeps the actual info. */
    void            *pvBackendData;
    /** Image open flags (only those handled generically in this code and which
     * the backends will never ever see). */
    unsigned        uOpenFlags;
} VDIMAGE, *PVDIMAGE;

/**
 * uModified bit flags.
 */
#define VD_IMAGE_MODIFIED_FLAG                  RT_BIT(0)
#define VD_IMAGE_MODIFIED_FIRST                 RT_BIT(1)
#define VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE   RT_BIT(2)


/**
 * VBox HDD Container main structure, private part.
 */
struct VBOXHDD
{
    /** Structure signature (VBOXHDDDISK_SIGNATURE). */
    uint32_t            u32Signature;

    /** Number of opened images. */
    unsigned            cImages;

    /** Base image. */
    PVDIMAGE            pBase;

    /** Last opened image in the chain.
     * The same as pBase if only one image is used. */
    PVDIMAGE            pLast;

    /** Flags representing the modification state. */
    unsigned            uModified;

    /** Cached size of this disk. */
    uint64_t            cbSize;
    /** Cached CHS geometry for this disk, cylinders. */
    unsigned            cCylinders;
    /** Cached CHS geometry for this disk, heads. */
    unsigned            cHeads;
    /** Cached CHS geometry for this disk, sectors. */
    unsigned            cSectors;
    /** Cached translation mode for this disk. */
    PDMBIOSTRANSLATION  enmTranslation;

    /** Error message processing callback. */
    PFNVDERROR          pfnError;
    /** Opaque data for error callback. */
    void                *pvErrorUser;

    /** Handle for the shared object / DLL. */
    RTLDRMOD            hPlugin;
    /** Function pointers for the various backend methods. */
    PVBOXHDDBACKEND     Backend;
};


typedef struct
{
    const char          *pszBackendName;
    PVBOXHDDBACKEND     Backend;
} VBOXHDDBACKENDENTRY;


extern VBOXHDDBACKEND g_VmdkBackend;


static const VBOXHDDBACKENDENTRY aBackends[] =
{
    { "VMDK", &g_VmdkBackend },
    { NULL, NULL }
};


/**
 * internal: issue early error message.
 */
static int vdEarlyError(PFNVDERROR pfnError, void *pvErrorUser, int rc,
                        RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pfnError)
        pfnError(pvErrorUser, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * internal: issue error message.
 */
static int vdError(PVBOXHDD pDisk, int rc, RT_SRC_POS_DECL,
                   const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pDisk->pfnError)
        pDisk->pfnError(pDisk->pvErrorUser, rc, RT_SRC_POS_ARGS, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * internal: add image structure to the end of images list.
 */
static void vdAddImageToList(PVBOXHDD pDisk, PVDIMAGE pImage)
{
    pImage->pPrev = NULL;
    pImage->pNext = NULL;

    if (pDisk->pBase)
    {
        Assert(pDisk->cImages > 0);
        pImage->pPrev = pDisk->pLast;
        pDisk->pLast->pNext = pImage;
        pDisk->pLast = pImage;
    }
    else
    {
        Assert(pDisk->cImages == 0);
        pDisk->pBase = pImage;
        pDisk->pLast = pImage;
    }

    pDisk->cImages++;
}

/**
 * internal: remove image structure from the images list.
 */
static void vdRemoveImageFromList(PVBOXHDD pDisk, PVDIMAGE pImage)
{
    Assert(pDisk->cImages > 0);

    if (pImage->pPrev)
        pImage->pPrev->pNext = pImage->pNext;
    else
        pDisk->pBase = pImage->pNext;

    if (pImage->pNext)
        pImage->pNext->pPrev = pImage->pPrev;
    else
        pDisk->pLast = pImage->pPrev;

    pImage->pPrev = NULL;
    pImage->pNext = NULL;

    pDisk->cImages--;
}

/**
 * internal: find image by index into the images list.
 */
static PVDIMAGE vdGetImageByNumber(PVBOXHDD pDisk, unsigned nImage)
{
    PVDIMAGE pImage = pDisk->pBase;
    while (pImage && nImage)
    {
        pImage = pImage->pNext;
        nImage--;
    }
    return pImage;
}

/**
 * internal: read the specified amount of data in whatever blocks the backend
 * will give us.
 */
static int vdReadHelper(PVBOXHDD pDisk, PVDIMAGE pImage, uint64_t uOffset,
                        void *pvBuf, size_t cbRead)
{
    int rc;
    size_t cbThisRead;
    PVDIMAGE pCurrImage;

    /* Loop until all read. */
    do
    {
        /* Search for image with allocated block. Do not attempt to read more
         * than the previous reads marked as valid. Otherwise this would return
         * stale data when different block sizes are used for the images. */
        cbThisRead = cbRead;
        rc = VINF_VDI_BLOCK_FREE;
        for (pCurrImage = pImage;
             pCurrImage != NULL && rc == VINF_VDI_BLOCK_FREE;
             pCurrImage = pCurrImage->pPrev)
        {
            rc = pDisk->Backend->pfnRead(pCurrImage->pvBackendData, uOffset,
                                         pvBuf, cbThisRead, &cbThisRead);
        }

        /* No image in the chain contains the data for the block. */
        if (rc == VINF_VDI_BLOCK_FREE)
        {
            memset(pvBuf, '\0', cbThisRead);
            rc = VINF_SUCCESS;
        }

        cbRead -= cbThisRead;
        uOffset += cbThisRead;
        pvBuf = (char *)pvBuf + cbThisRead;
    } while (cbRead != 0 && VBOX_SUCCESS(rc));

    return rc;
}

/**
 * internal: mark the disk as not modified.
 */
static void vdResetModifiedFlag(PVBOXHDD pDisk)
{
    if (pDisk->uModified & VD_IMAGE_MODIFIED_FLAG)
    {
        /* generate new last-modified uuid */
        if (!(pDisk->uModified | VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE))
        {
            RTUUID Uuid;

            RTUuidCreate(&Uuid);
            pDisk->Backend->pfnSetModificationUuid(pDisk->pLast->pvBackendData,
                                                   &Uuid);
        }

        pDisk->uModified &= ~VD_IMAGE_MODIFIED_FLAG;
    }
}

/**
 * internal: mark the disk as modified.
 */
static void vdSetModifiedFlag(PVBOXHDD pDisk)
{
    pDisk->uModified |= VD_IMAGE_MODIFIED_FLAG;
    if (pDisk->uModified & VD_IMAGE_MODIFIED_FIRST)
    {
        pDisk->uModified &= ~VD_IMAGE_MODIFIED_FIRST;

        /* First modify, so create a UUID and ensure it's written to disk. */
        vdResetModifiedFlag(pDisk);

        if (!(pDisk->uModified | VD_IMAGE_MODIFIED_DISABLE_UUID_UPDATE))
            pDisk->Backend->pfnFlush(pDisk->pLast->pvBackendData);
    }
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation does not optimize
 * anything (except that it tries to read only that portions from parent
 * images that are really needed).
 */
static int vdWriteHelperStandard(PVBOXHDD pDisk, PVDIMAGE pImage,
                                 uint64_t uOffset, size_t cbWrite,
                                 size_t cbThisWrite, size_t cbPreRead,
                                 size_t cbPostRead, const void *pvBuf,
                                 void *pvTmp)
{
    int rc = VINF_SUCCESS;

    /* Read the data that goes before the write to fill the block. */
    if (cbPreRead)
    {
        rc = vdReadHelper(pDisk, pImage->pPrev, uOffset - cbPreRead,
                          pvTmp, cbPreRead);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /* Copy the data to the right place in the buffer. */
    memcpy((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite);

    /* Read the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* If we have data to be written, use that instead of reading
         * data from the image. */
        size_t cbWriteCopy;
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);
        else
            cbWriteCopy = 0;
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        size_t cbFill;
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;
        else
            cbFill = 0;
        /* The rest must be read from the image. */
        size_t cbReadImage = cbPostRead - cbWriteCopy - cbFill;

        /* Now assemble the remaining data. */
        if (cbWriteCopy)
            memcpy((char *)pvTmp + cbPreRead + cbThisWrite,
                   (char *)pvBuf + cbThisWrite, cbWriteCopy);
        if (cbReadImage)
            rc = vdReadHelper(pDisk, pImage->pPrev,
                              uOffset + cbThisWrite + cbWriteCopy,
                              (char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy,
                              cbReadImage);
        if (VBOX_FAILURE(rc))
            return rc;
        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
            memset((char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy + cbReadImage,
                   '\0', cbFill);
    }

    /* Write the full block to the virtual disk. */
    rc = pDisk->Backend->pfnWrite(pImage->pvBackendData,
                                  uOffset - cbPreRead, pvTmp,
                                  cbPreRead + cbThisWrite + cbPostRead,
                                  NULL,
                                  &cbPreRead, &cbPostRead);
    Assert(rc != VINF_VDI_BLOCK_FREE);
    Assert(cbPreRead == 0);
    Assert(cbPostRead == 0);

    return rc;
}

/**
 * internal: write a complete block (only used for diff images), taking the
 * remaining data from parent images. This implementation optimized out writes
 * that do not change the data relative to the state as of the parent images.
 * All backends which support differential/growing images support this.
 */
static int vdWriteHelperOptimized(PVBOXHDD pDisk, PVDIMAGE pImage,
                                  uint64_t uOffset, size_t cbWrite,
                                  size_t cbThisWrite, size_t cbPreRead,
                                  size_t cbPostRead, const void *pvBuf,
                                  void *pvTmp)
{
    size_t cbFill = 0;
    size_t cbWriteCopy = 0;
    size_t cbReadImage = 0;
    int rc;

    if (cbPostRead)
    {
        /* Figure out how much we cannnot read from the image, because
         * the last block to write might exceed the nominal size of the
         * image for technical reasons. */
        if (uOffset + cbThisWrite + cbPostRead > pDisk->cbSize)
            cbFill = uOffset + cbThisWrite + cbPostRead - pDisk->cbSize;

        /* If we have data to be written, use that instead of reading
         * data from the image. */
        if (cbWrite > cbThisWrite)
            cbWriteCopy = RT_MIN(cbWrite - cbThisWrite, cbPostRead);

        /* The rest must be read from the image. */
        cbReadImage = cbPostRead - cbWriteCopy - cbFill;
    }

    /* Read the entire data of the block so that we can compare whether it will
     * be modified by the write or not. */
    rc = vdReadHelper(pDisk, pImage->pPrev, uOffset - cbPreRead,
                      pvTmp, cbPreRead + cbThisWrite + cbPostRead - cbFill);
    if (VBOX_FAILURE(rc))
        return rc;

    /* Check if the write would modify anything in this block. */
    if (   !memcmp((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite)
        && (!cbWriteCopy || !memcmp((char *)pvTmp + cbPreRead + cbThisWrite,
                                    (char *)pvBuf + cbThisWrite, cbWriteCopy)))
    {
        /* Block is completely unchanged, so no need to write anything. */
        return VINF_SUCCESS;
    }

    /* Copy the data to the right place in the buffer. */
    memcpy((char *)pvTmp + cbPreRead, pvBuf, cbThisWrite);

    /* Handle the data that goes after the write to fill the block. */
    if (cbPostRead)
    {
        /* Now assemble the remaining data. */
        if (cbWriteCopy)
            memcpy((char *)pvTmp + cbPreRead + cbThisWrite,
                   (char *)pvBuf + cbThisWrite, cbWriteCopy);
        /* Zero out the remainder of this block. Will never be visible, as this
         * is beyond the limit of the image. */
        if (cbFill)
            memset((char *)pvTmp + cbPreRead + cbThisWrite + cbWriteCopy + cbReadImage,
                   '\0', cbFill);
    }

    /* Write the full block to the virtual disk. */
    rc = pDisk->Backend->pfnWrite(pImage->pvBackendData,
                                  uOffset - cbPreRead, pvTmp,
                                  cbPreRead + cbThisWrite + cbPostRead,
                                  NULL,
                                  &cbPreRead, &cbPostRead);
    Assert(rc != VINF_VDI_BLOCK_FREE);
    Assert(cbPreRead == 0);
    Assert(cbPostRead == 0);

    return rc;
}

/**
 * Allocates and initializes an empty VBox HDD container.
 * No image files are opened.
 *
 * @returns VBox status code.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pfnError        Callback for setting extended error information.
 * @param   pvErrorUser     Opaque parameter for pfnError.
 * @param   ppDisk          Where to store the reference to the VBox HDD container.
 */
VBOXDDU_DECL(int) VDCreate(const char *pszBackend, PFNVDERROR pfnError,
                           void *pvErrorUser, PVBOXHDD *ppDisk)
{
    int rc = VINF_SUCCESS;
    PVBOXHDDBACKEND pBackend = NULL;
    PVBOXHDD pDisk = NULL;

    /* Passing an error callback is strictly not necessary any more. Any code
     * calling the HDD container functions should provide one, as otherwise
     * many detailed error messages will go unnoticed. If you find a situation
     * where you get no sensible error message from this code but you think
     * there should be one, shout loudly. There are no error messages for rare
     * and obvious error codes such as VERR_NO_MEMORY, and for situations which
     * the user cannot be made responsible for, such as program bugs causing
     * parameter checks to fail etc. */
    Assert(pfnError);

    /* Find backend. */
    for (unsigned i = 0; aBackends[i].pszBackendName != NULL; i++)
    {
        if (!strcmp(pszBackend, aBackends[i].pszBackendName))
        {
            pBackend = aBackends[i].Backend;
            break;
        }
    }

    /* If no static backend is found try loading a shared module with pszBackend as filename. */
    if (!pBackend)
    {
        RTLDRMOD hPlugin;
        char *pszPluginName;
        int cbPluginName;

        /* HDD Format Plugins have VBoxHDD as prefix, thatswhy we have to prepend it. 
         * @todo: find out what to do if filenames are case sensitive.
         */
        cbPluginName = RTStrAPrintf(&pszPluginName, "%s%s", VBOX_HDDFORMAT_PLUGIN_PREFIX, pszBackend);
        if (cbPluginName == -1)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            /* try to load the plugin (RTldrLoad appends the suffix for the shared object/DLL). */
            rc = RTLdrLoad(pszPluginName, &hPlugin);
            if (VBOX_SUCCESS(rc))
            {
                PFNVBOXHDDFORMATLOAD pfnHDDFormatLoad;

                rc = RTLdrGetSymbol(hPlugin, VBOX_HDDFORMAT_LOAD_NAME, (void**)&pfnHDDFormatLoad);
                if (VBOX_FAILURE(rc))
                {
                    Log(("%s: Error resolving the entry point %s, rc = %d, pfnHDDFormat = %p\n", VBOX_HDDFORMAT_LOAD_NAME, rc, pfnHDDFormatLoad));
                    if (VBOX_SUCCESS(rc))
                       rc = VERR_SYMBOL_NOT_FOUND;
                }
                else
                {
                    /* Get the function table. */
                    pBackend = (PVBOXHDDBACKEND)RTMemAllocZ(sizeof(VBOXHDDBACKEND));
                    if (!pBackend)
                    {
                        rc = VERR_NO_MEMORY;
                    }
                    else
                    {
                        pBackend->cbSize = sizeof(VBOXHDDBACKEND);
                        rc = pfnHDDFormatLoad(pBackend);
                        if (VBOX_FAILURE(rc))
                        {
                            RTMemFree(pBackend);
                            pBackend = NULL;
                        }
                    }
                }
            }
            RTStrFree(pszPluginName);
        }
    }

    if (pBackend)
    {
        pDisk = (PVBOXHDD)RTMemAllocZ(sizeof(VBOXHDD));
        if (pDisk)
        {
            pDisk->u32Signature = VBOXHDDDISK_SIGNATURE;
            pDisk->cImages      = 0;
            pDisk->pBase        = NULL;
            pDisk->pLast        = NULL;
            pDisk->cbSize       = 0;
            pDisk->cCylinders   = 0;
            pDisk->cHeads       = 0;
            pDisk->cSectors     = 0;
            pDisk->pfnError     = pfnError;
            pDisk->pvErrorUser  = pvErrorUser;
            pDisk->Backend      = pBackend;
            pDisk->hPlugin      = NIL_RTLDRMOD;
            *ppDisk = pDisk;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = vdEarlyError(pfnError, pvErrorUser, VERR_INVALID_PARAMETER,
                          RT_SRC_POS, "VD: unknown backend name '%s'",
                          pszBackend);

    LogFlow(("%s: returns %Vrc (pDisk=%#p)\n", __FUNCTION__, rc, pDisk));
    return rc;
}

/**
 * Try to get the backend name which can use this image. 
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file for which the backend is queried.
 * @param   ppszFormat      Where to store the name of the plugin.
 */

VBOXDDU_DECL(int) VDGetFormat(const char *pszFilename, char **ppszFormat)
{
    char pszProgramPath[1024]; /* Far too much I think but to be on the safe side. */
    char *pszPluginFilter;
    PRTDIR pPluginDir = NULL;
    PRTDIRENTRY pPluginDirEntry = NULL;
    unsigned cbPluginDirEntry;
    int rc = VERR_NOT_SUPPORTED;
    bool fPluginFound = false;

    if (!ppszFormat)
        return VERR_INVALID_PARAMETER;
	
    memset(pszProgramPath, 0, 1024);
    rc = RTPathProgram(pszProgramPath, 1024);
    if (VBOX_FAILURE(rc))
    {
        return rc;
    }

    /* To get all entries with VBoxHDD as prefix. */
    rc = RTStrAPrintf(&pszPluginFilter, "%s/%s*", pszProgramPath, VBOX_HDDFORMAT_PLUGIN_PREFIX);
    if (VBOX_FAILURE(rc))
    {
        RTStrFree(pszProgramPath);
        rc = VERR_NO_MEMORY;
        return rc;
    }

    /* The plugins are in the same directory as the program. */
    rc = RTDirOpenFiltered(&pPluginDir, pszPluginFilter, RTDIRFILTER_WINNT);
    if (VBOX_FAILURE(rc))
        goto out;

    pPluginDirEntry = (PRTDIRENTRY)RTMemAllocZ(sizeof(RTDIRENTRY));
    if (!pPluginDir)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    while ((rc = RTDirRead(pPluginDir, pPluginDirEntry, &cbPluginDirEntry)) != VERR_NO_MORE_FILES)
    {
        RTLDRMOD hPlugin = NIL_RTLDRMOD;
        PFNVBOXHDDFORMATLOAD pfnHDDFormatLoad = NULL;
        PVBOXHDDBACKEND pBackend = NULL;

        if (rc == VERR_BUFFER_OVERFLOW)
        {
            /* allocate new buffer. */
            RTMemFree(pPluginDirEntry);
            pPluginDirEntry = (PRTDIRENTRY)RTMemAllocZ(cbPluginDirEntry);
            /* Retry. */
            rc = RTDirRead(pPluginDir, pPluginDirEntry, &cbPluginDirEntry);
            if (VBOX_FAILURE(rc))
                break;
        }
        else if (VBOX_FAILURE(rc))
                break;

        /* We got the new entry. */
        if (pPluginDirEntry->enmType != RTDIRENTRYTYPE_FILE)
            continue;

        rc = RTLdrLoad(pPluginDirEntry->szName, &hPlugin);
        if (VBOX_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hPlugin, VBOX_HDDFORMAT_LOAD_NAME, (void**)&pfnHDDFormatLoad);
            if (VBOX_FAILURE(rc) || !pfnHDDFormatLoad)
            {
                Log(("%s: Error resolving the entry point %s, rc = %d, pfnHDDFormat = %p\n", VBOX_HDDFORMAT_LOAD_NAME, rc, pfnHDDFormatLoad));
                if (VBOX_SUCCESS(rc))
                    rc = VERR_SYMBOL_NOT_FOUND;
            }
            else
            {
                /* Get the function table. */
                pBackend = (PVBOXHDDBACKEND)RTMemAllocZ(sizeof(VBOXHDDBACKEND));
                if (!pBackend)
                {
                    rc = VERR_NO_MEMORY;
                }
                else
                {
                    pBackend->cbSize = sizeof(VBOXHDDBACKEND);
                    rc = pfnHDDFormatLoad(pBackend);
                    if (VBOX_FAILURE(rc))
                    {
                        RTMemFree(pBackend);
                        pBackend = NULL;
                    }

                    /* Check if the plugin can handle this file. */
                    rc = pBackend->pfnCheckIfValid(pszFilename);
                    if (VBOX_FAILURE(rc))
                    {
                        RTMemFree(pBackend);
                        RTLdrClose(hPlugin);
                    }
                    else
                    {
                        RTMemFree(pBackend);
                        RTLdrClose(hPlugin);
                        fPluginFound = true;

                        /* Report the format name. */
                        char *pszName = pPluginDirEntry->szName + VBOX_HDDFORMAT_PLUGIN_PREFIX_LENGTH; /* Point to the rest after the prefix. */
                        char *pszFormat = NULL;
                        unsigned cbFormat = 0;
                          
                        while((*pszName != '.') && (*pszName != '\0'))
                        {
                            cbFormat++;
                            pszName++;
                        }

                        pszName = pPluginDirEntry->szName + VBOX_HDDFORMAT_PLUGIN_PREFIX_LENGTH;

                        /* Copy the name into the new string. */   
                        pszFormat = (char *)RTMemAllocZ(cbFormat+1);

                        if (!pszFormat)
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }

                        memcpy(pszFormat, pszName, cbFormat);
                        
                        *ppszFormat = pszFormat;

                        break;
                    }
                }
            }
        }
    }

out:
    if (pPluginDirEntry)
        RTMemFree(pPluginDirEntry);
    if (pPluginDir)
        RTDirClose(pPluginDir);

    RTStrFree(pszPluginFilter);
    RTStrFree(pszProgramPath);

    if ((fPluginFound == true) && (*ppszFormat != NULL))
        rc = VINF_SUCCESS;

    return rc;
}

/**
 * Destroys the VBox HDD container.
 * If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VDDestroy(PVBOXHDD pDisk)
{
    LogFlow(("%s: pDisk=%#p\n", __FUNCTION__, pDisk));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    if (pDisk)
    {
        VDCloseAll(pDisk);
        if (pDisk->hPlugin != NIL_RTLDRMOD)
        {
            RTLdrClose(pDisk->hPlugin);
            pDisk->hPlugin = NIL_RTLDRMOD;
            RTMemFree(pDisk->Backend);
        }
        RTMemFree(pDisk);
    }
}

/**
 * Opens an image file.
 *
 * The first opened image file in a HDD container must have a base image type,
 * others (next opened images) must be a differencing or undo images.
 * Linkage is checked for differencing image to be in consistence with the previously opened image.
 * When another differencing image is opened and the last image was opened in read/write access
 * mode, then the last image is reopened in read-only with deny write sharing mode. This allows
 * other processes to use images in read-only mode too.
 *
 * Note that the image can be opened in read-only mode if a read/write open is not possible.
 * Use VDIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDOpen(PVBOXHDD pDisk, const char *pszFilename,
                         unsigned uOpenFlags)
{
    int rc = VINF_SUCCESS;
    LogFlow(("%s: pszFilename=\"%s\" uOpenFlags=%#x\n", __FUNCTION__,
             pszFilename, uOpenFlags));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    /* Check arguments. */
    if (    !pszFilename
        ||  *pszFilename == '\0'
        ||  (uOpenFlags & ~VD_OPEN_FLAGS_MASK))
    {
        AssertMsgFailed(("Invalid arguments: pszFilename=%#p uOpenFlags=%#x\n", pszFilename, uOpenFlags));
        return VERR_INVALID_PARAMETER;
    }

    /* Force readonly for images without base/diff consistency checking. */
    if (uOpenFlags & VD_OPEN_FLAGS_INFO)
        uOpenFlags |= VD_OPEN_FLAGS_READONLY;

    /* Set up image descriptor. */
    PVDIMAGE pImage = (PVDIMAGE)RTMemAllocZ(sizeof(VDIMAGE));
    if (!pImage)
        return VERR_NO_MEMORY;
    pImage->pszFilename = RTStrDup(pszFilename);
    if (!pImage->pszFilename)
        rc = VERR_NO_MEMORY;

    if (VBOX_SUCCESS(rc))
    {
        pImage->uOpenFlags = uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME;
        rc = pDisk->Backend->pfnOpen(pImage->pszFilename,
                                     uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME,
                                     pDisk->pfnError, pDisk->pvErrorUser,
                                     &pImage->pvBackendData);
    }
    /* If the open in read-write mode failed, retry in read-only mode. */
    if (VBOX_FAILURE(rc))
    {
        if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY)
            &&  (rc == VERR_ACCESS_DENIED
                 || rc == VERR_PERMISSION_DENIED
                 || rc == VERR_WRITE_PROTECT
                 || rc == VERR_SHARING_VIOLATION
                 || rc == VERR_FILE_LOCK_FAILED))
            rc = pDisk->Backend->pfnOpen(pImage->pszFilename,
                                           (uOpenFlags & ~VD_OPEN_FLAGS_HONOR_SAME)
                                         | VD_OPEN_FLAGS_READONLY,
                                         pDisk->pfnError, pDisk->pvErrorUser,
                                         &pImage->pvBackendData);
        if (VBOX_FAILURE(rc))
            rc = vdError(pDisk, rc, RT_SRC_POS,
                         N_("VD: error opening image file '%s'"), pszFilename);
    }

    if (VBOX_SUCCESS(rc))
    {
        VDIMAGETYPE enmImageType;
        rc = pDisk->Backend->pfnGetImageType(pImage->pvBackendData,
                                             &enmImageType);
        /* Check image type. As the image itself has no idea whether it's a
         * base image or not, this info is derived here. Image 0 can be fixed
         * or normal, all others must be normal images. */
        if (    VBOX_SUCCESS(rc)
            &&  !(uOpenFlags & VD_OPEN_FLAGS_INFO)
            &&  pDisk->cImages != 0
            &&  enmImageType != VD_IMAGE_TYPE_NORMAL)
            rc = VERR_VDI_INVALID_TYPE;

        /** @todo optionally check UUIDs */

        if (VBOX_SUCCESS(rc))
        {
            uint64_t cbSize = pDisk->Backend->pfnGetSize(pImage->pvBackendData);
            if (pDisk->cImages == 0)
            {
                /* Cache disk information. */
                pDisk->cbSize = cbSize;

                /* Cache CHS geometry. */
                int rc2 = pDisk->Backend->pfnGetGeometry(pImage->pvBackendData,
                                                         &pDisk->cCylinders,
                                                         &pDisk->cHeads,
                                                         &pDisk->cSectors);
                if (VBOX_FAILURE(rc2))
                {
                    pDisk->cCylinders = 0;
                    pDisk->cHeads = 0;
                    pDisk->cSectors = 0;
                }
                else
                {
                    /* Make sure the CHS geometry is properly clipped. */
                    pDisk->cCylinders = RT_MIN(pDisk->cCylinders, 16383);
                    pDisk->cHeads = RT_MIN(pDisk->cHeads, 255);
                    pDisk->cSectors = RT_MIN(pDisk->cSectors, 255);
                }

                /* Cache translation mode. */
                rc2 = pDisk->Backend->pfnGetTranslation(pImage->pvBackendData,
                                                        &pDisk->enmTranslation);
                if (VBOX_FAILURE(rc2))
                    pDisk->enmTranslation = (PDMBIOSTRANSLATION)0;
            }
            else
            {
                /* Check image size/block size for consistency. */
                if (cbSize != pDisk->cbSize)
                    rc = VERR_VDI_INVALID_TYPE;
            }
        }

        if (VBOX_SUCCESS(rc) && pDisk->cImages != 0)
        {
            /* Switch previous image to read-only mode. */
            unsigned uOpenFlagsPrevImg;
            uOpenFlagsPrevImg = pDisk->Backend->pfnGetOpenFlags(pDisk->pLast->pvBackendData);
            if (!(uOpenFlagsPrevImg & VD_OPEN_FLAGS_READONLY))
            {
                uOpenFlagsPrevImg |= VD_OPEN_FLAGS_READONLY;
                rc = pDisk->Backend->pfnSetOpenFlags(pDisk->pLast->pvBackendData, uOpenFlagsPrevImg);
            }
        }

        if (VBOX_SUCCESS(rc))
        {
            /* Image successfully opened, make it the last image. */
            vdAddImageToList(pDisk, pImage);
        }
        else
        {
            /* Error detected, but image opened. Close image. */
            int rc2;
            rc2 = pDisk->Backend->pfnClose(pImage->pvBackendData, false);
            AssertRC(rc2);
            pImage->pvBackendData = NULL;
        }
    }

    if (VBOX_FAILURE(rc))
    {
        if (pImage)
        {
            if (pImage->pszFilename)
                RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
        }
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Creates and opens a new base image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the image file to create.
 * @param   enmType         Image type, only base image types are acceptable.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   cCylinders      Number of cylinders (must be <= 16383).
 * @param   cHeads          Number of heads (must be <= 16).
 * @param   cSectors        Number of sectors (must be <= 63);
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateBase(PVBOXHDD pDisk, const char *pszFilename,
                               VDIMAGETYPE enmType, uint64_t cbSize,
                               unsigned uImageFlags, const char *pszComment,
                               unsigned cCylinders, unsigned cHeads,
                               unsigned cSectors, unsigned uOpenFlags,
                               PFNVMPROGRESS pfnProgress, void *pvUser)
{
    int rc = VINF_SUCCESS;
    LogFlow(("%s: pszFilename=\"%s\" uOpenFlags=%#x\n", __FUNCTION__,
             pszFilename, uOpenFlags));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    /* Check arguments. */
    if (    !pszFilename
        ||  *pszFilename == '\0'
        ||  (enmType != VD_IMAGE_TYPE_NORMAL && enmType != VD_IMAGE_TYPE_FIXED)
        ||  !cbSize
        ||  (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
        ||  cCylinders == 0
        ||  cCylinders > 16383
        ||  cHeads == 0
        ||  cHeads > 16
        ||  cSectors == 0
        ||  cSectors > 63)
    {
        AssertMsgFailed(("Invalid arguments: pszFilename=%#p uOpenFlags=%#x\n", pszFilename, uOpenFlags));
        return VERR_INVALID_PARAMETER;
    }

    /* Check state. */
    if (pDisk->cImages != 0)
    {
        AssertMsgFailed(("Create base image cannot be done with other images open\n"));
        return VERR_VDI_INVALID_STATE;
    }

    /* Set up image descriptor. */
    PVDIMAGE pImage = (PVDIMAGE)RTMemAllocZ(sizeof(VDIMAGE));
    if (!pImage)
        return VERR_NO_MEMORY;
    pImage->pszFilename = RTStrDup(pszFilename);
    if (!pImage->pszFilename)
        rc = VERR_NO_MEMORY;

    if (VBOX_SUCCESS(rc))
        rc = pDisk->Backend->pfnCreate(pImage->pszFilename, enmType, cbSize,
                                       uImageFlags, pszComment, cCylinders,
                                       cHeads, cSectors, uOpenFlags,
                                       pfnProgress, pvUser,
                                       pDisk->pfnError, pDisk->pvErrorUser,
                                       &pImage->pvBackendData);

    if (VBOX_SUCCESS(rc))
    {
        /** @todo optionally check UUIDs */

        if (VBOX_SUCCESS(rc))
        {
            uint64_t cbSize = pDisk->Backend->pfnGetSize(pImage->pvBackendData);
            if (pDisk->cImages == 0)
            {
                /* Cache disk information. */
                pDisk->cbSize = cbSize;

                /* Cache CHS geometry. */
                int rc2 = pDisk->Backend->pfnGetGeometry(pImage->pvBackendData,
                                                         &pDisk->cCylinders,
                                                         &pDisk->cHeads,
                                                         &pDisk->cSectors);
                if (VBOX_FAILURE(rc2))
                {
                    pDisk->cCylinders = 0;
                    pDisk->cHeads = 0;
                    pDisk->cSectors = 0;
                }
                else
                {
                    /* Make sure the CHS geometry is properly clipped. */
                    pDisk->cCylinders = RT_MIN(pDisk->cCylinders, 16383);
                    pDisk->cHeads = RT_MIN(pDisk->cHeads, 255);
                    pDisk->cSectors = RT_MIN(pDisk->cSectors, 255);
                }

                /* Cache translation mode. */
                rc2 = pDisk->Backend->pfnGetTranslation(pImage->pvBackendData,
                                                        &pDisk->enmTranslation);
                if (VBOX_FAILURE(rc2))
                    pDisk->enmTranslation = (PDMBIOSTRANSLATION)0;
            }
            else
            {
                /* Check image size/block size for consistency. */
                if (cbSize != pDisk->cbSize)
                    rc = VERR_VDI_INVALID_TYPE;
            }
        }

        if (VBOX_SUCCESS(rc))
        {
            /* Image successfully opened, make it the last image. */
            vdAddImageToList(pDisk, pImage);
        }
        else
        {
            /* Error detected, but image opened. Close and delete image. */
            int rc2;
            rc2 = pDisk->Backend->pfnClose(pImage->pvBackendData, true);
            AssertRC(rc2);
            pImage->pvBackendData = NULL;
        }
    }

    if (VBOX_FAILURE(rc))
    {
        if (pImage)
        {
            if (pImage->pszFilename)
                RTStrFree(pImage->pszFilename);
            RTMemFree(pImage);
        }
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDOpen function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateDiff(PVBOXHDD pDisk, const char *pszFilename,
                               unsigned uImageFlags, const char *pszComment,
                               unsigned uOpenFlags,
                               PFNVMPROGRESS pfnProgress, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Merges two images having a parent/child relationship (both directions).
 * As a side effect the source image is deleted from both the disk and
 * the images in the VBox HDD container.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImageFrom      Name of the image file to merge from.
 * @param   nImageTo        Name of the image file to merge to.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDMerge(PVBOXHDD pDisk, unsigned nImageFrom,
                          unsigned nImageTo, PFNVMPROGRESS pfnProgress,
                          void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Copies an image from one VBox HDD container to another.
 * The copy is opened in the target VBox HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination VBox HDD container may be different from the
 * source container.
 * If both the source and destination reference the same VBox HDD container,
 * then the image is moved (by copying/deleting) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @returns VBox status code.
 * @param   pDiskFrom       Pointer to source VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination VBox HDD container.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCopy(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                         PFNVMPROGRESS pfnProgress, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Compacts a growing image file by removing zeroed data blocks.
 * Optionally defragments data in the image so that ascending sector numbers
 * are stored in ascending location in the image file.
 *
 * @todo maybe include this function in VDCopy.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   fDefragment     If true, reorder file data so that sectors are stored in ascending order.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCompact(PVBOXHDD pDisk, unsigned nImage,
                            bool fDefragment,
                            PFNVMPROGRESS pfnProgress, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Resizes an image. Allows setting the disk size to both larger and smaller
 * values than the current disk size.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   cbSize          New image size in bytes.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDResize(PVBOXHDD pDisk, unsigned nImage, uint64_t cbSize,
                           PFNVMPROGRESS pfnProgress, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Closes the last opened image file in the HDD container. Leaves all changes inside it.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete)
{
    LogFlow(("%s: fDelete=%d\n", __FUNCTION__, fDelete));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    PVDIMAGE pImage = pDisk->pLast;
    unsigned uOpenFlags = pDisk->Backend->pfnGetOpenFlags(pImage->pvBackendData);
    /* Remove image from list of opened images. */
    vdRemoveImageFromList(pDisk, pImage);
    /* Close (and optionally delete) image. */
    int rc = pDisk->Backend->pfnClose(pImage->pvBackendData, fDelete);
    /* Free remaining resources related to the image. */
    RTStrFree(pImage->pszFilename);
    RTMemFree(pImage);

    /* If disk was previously in read/write mode, make sure it will stay like
     * this after closing this image. Set the open flags accordingly. */
    if (!(uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        uOpenFlags = pDisk->Backend->pfnGetOpenFlags(pDisk->pLast->pvBackendData);
        uOpenFlags &= ~ VD_OPEN_FLAGS_READONLY;
        rc = pDisk->Backend->pfnSetOpenFlags(pDisk->pLast->pvBackendData,
                                                     uOpenFlags);
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Closes all opened image files in HDD container.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(int) VDCloseAll(PVBOXHDD pDisk)
{
    LogFlow(("%s:\n", __FUNCTION__));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    PVDIMAGE pImage = pDisk->pLast;
    int rc = VINF_SUCCESS;
    while (pImage)
    {
        PVDIMAGE pPrev = pImage->pPrev;
        /* Remove image from list of opened images. */
        vdRemoveImageFromList(pDisk, pImage);
        /* Close image. */
        int rc2 = pDisk->Backend->pfnClose(pImage->pvBackendData, false);
        if (VBOX_FAILURE(rc2) && VBOX_SUCCESS(rc))
            rc = rc2;
        /* Free remaining resources related to the image. */
        RTStrFree(pImage->pszFilename);
        RTMemFree(pImage);
        pImage = pPrev;
    }
    Assert(pDisk->pLast == NULL);

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   uOffset         Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbRead          Number of bytes to read.
 */
VBOXDDU_DECL(int) VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuf, size_t cbRead)
{
    /* sanity check */
    LogFlow(("%s: offset=%llu cbRead=%u\n", __FUNCTION__, uOffset, (unsigned)cbRead));
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pLast;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
        goto out;
    }

    /* Check params. */
    if (uOffset + cbRead > pDisk->cbSize || cbRead == 0)
    {
        AssertMsgFailed(("uOffset=%llu cbRead=%u\n", uOffset, cbRead));
        return VERR_INVALID_PARAMETER;
    }

    rc = vdReadHelper(pDisk, pImage, uOffset, pvBuf, cbRead);
out:
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   uOffset         Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for writing data.
 * @param   cbWrite         Number of bytes to write.
 */
VBOXDDU_DECL(int) VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void *pvBuf, size_t cbWrite)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    size_t cbThisWrite;
    size_t cbPreRead, cbPostRead;
    PVDIMAGE pImage = pDisk->pLast;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
        goto out;
    }

    /* Check params. */
    if (uOffset + cbWrite > pDisk->cbSize || cbWrite == 0)
    {
        AssertMsgFailed(("uOffset=%llu cbWrite=%u\n", uOffset, cbWrite));
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    vdSetModifiedFlag(pDisk);

    /* Loop until all written. */
    do
    {
        /* Try to write the possibly partial block to the last opened image.
         * This works when the block is already allocated in this image or
         * if it is a full-block write, which automatically allocates a new
         * block if needed. */
        cbThisWrite = cbWrite;
        rc = pDisk->Backend->pfnWrite(pImage->pvBackendData, uOffset, pvBuf,
                                      cbThisWrite, &cbThisWrite,
                                      &cbPreRead, &cbPostRead);
        if (rc == VINF_VDI_BLOCK_FREE)
        {
            void *pvTmp = RTMemTmpAlloc(cbPreRead + cbThisWrite + cbPostRead);
            if (!pvBuf)
            {
                Assert(!pvBuf);
                rc = VERR_NO_MEMORY;
                break;
            }

            if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_HONOR_SAME))
            {
                /* Optimized write, suppress writing to a so far unallocated
                 * block when the data is identical than as of the parent. */
                rc = vdWriteHelperOptimized(pDisk, pImage, uOffset,
                                            cbWrite, cbThisWrite,
                                            cbPreRead, cbPostRead,
                                            pvBuf, pvTmp);
            }
            else
            {
                /* Normal write, not optimized in any way. The block will be
                 * written no matter what. This will usually (unless the
                 * backend has some further optimization enabled) cause the
                 * block to be allocated. */
                rc = vdWriteHelperStandard(pDisk, pImage, uOffset,
                                           cbWrite, cbThisWrite,
                                           cbPreRead, cbPostRead,
                                           pvBuf, pvTmp);
            }
            RTMemTmpFree(pvTmp);
            if (VBOX_FAILURE(rc))
                break;
        }

        cbWrite -= cbThisWrite;
        uOffset += cbThisWrite;
        pvBuf = (char *)pvBuf + cbThisWrite;
    } while (cbWrite != 0 && VBOX_SUCCESS(rc));

out:
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Make sure the on disk representation of a virtual HDD is up to date.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(int) VDFlush(PVBOXHDD pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pLast;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        vdResetModifiedFlag(pDisk);
        rc = pDisk->Backend->pfnFlush(pImage->pvBackendData);
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images have been opened.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(unsigned) VDGetCount(PVBOXHDD pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    unsigned c = pDisk->cImages;
    LogFlow(("%s: returns %d\n", __FUNCTION__, c));
    return c;
}

/**
 * Get read/write mode of the VBox HDD container.
 *
 * @returns Virtual disk ReadOnly status.
 * @returns true if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(bool) VDIsReadOnly(PVBOXHDD pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    bool f;
    if (pDisk->pLast)
    {
        unsigned uOpenFlags;
        uOpenFlags = pDisk->Backend->pfnGetOpenFlags(pDisk->pLast->pvBackendData);
        f = !!(uOpenFlags & VD_OPEN_FLAGS_READONLY);
    }
    else
    {
        AssertMsgFailed(("No disk image is opened!\n"));
        f = true;
    }

    LogFlow(("%s: returns %d\n", __FUNCTION__, f));
    return f;
}

/**
 * Get total disk size of the VBox HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(uint64_t) VDGetSize(PVBOXHDD pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    uint64_t cb = pDisk->cbSize;
    LogFlow(("%s: returns %lld\n", __FUNCTION__, cb));
    return cb;
}

/**
 * Get virtual disk geometry stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pcCylinders     Where to store the number of cylinders. NULL is ok.
 * @param   pcHeads         Where to store the number of heads. NULL is ok.
 * @param   pcSectors       Where to store the number of sectors. NULL is ok.
 */
VBOXDDU_DECL(int) VDGetGeometry(PVBOXHDD pDisk, unsigned *pcCylinders,
                                unsigned *pcHeads, unsigned *pcSectors)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pBase;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        if (pDisk->cCylinders != 0)
        {
            if (pcCylinders)
                *pcCylinders = pDisk->cCylinders;
            if (pcHeads)
                *pcHeads = pDisk->cHeads;
            if (pcSectors)
                *pcSectors = pDisk->cSectors;
        }
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    LogFlow(("%s: %Vrc (CHS=%u/%u/%u)\n", __FUNCTION__, rc,
             pDisk->cCylinders, pDisk->cHeads, pDisk->cSectors));
    return rc;
}

/**
 * Store virtual disk geometry in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   cCylinders      Number of cylinders.
 * @param   cHeads          Number of heads.
 * @param   cSectors        Number of sectors.
 */
VBOXDDU_DECL(int) VDSetGeometry(PVBOXHDD pDisk, unsigned cCylinders,
                                unsigned cHeads, unsigned cSectors)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pBase;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        if (    cCylinders != pDisk->cCylinders
            ||  cHeads != pDisk->cHeads
            ||  cSectors != pDisk->cSectors)
        {
            /* Only update geometry if it is changed. Avoids similar checks
             * in every backend. Most of the time the new geometry is set to
             * the previous values, so no need to go through the hassle of
             * updating an image which could be opened in read-only mode right
             * now. */
            rc = pDisk->Backend->pfnSetGeometry(pImage->pvBackendData,
                                                cCylinders, cHeads, cSectors);

            /* Cache new geometry values in any case, whether successful or not. */
            int rc2 = pDisk->Backend->pfnGetGeometry(pImage->pvBackendData,
                                                     &pDisk->cCylinders,
                                                     &pDisk->cHeads,
                                                     &pDisk->cSectors);
            if (VBOX_FAILURE(rc2))
            {
                pDisk->cCylinders = 0;
                pDisk->cHeads = 0;
                pDisk->cSectors = 0;
            }
            else
            {
                /* Make sure the CHS geometry is properly clipped. */
                pDisk->cCylinders = RT_MIN(pDisk->cCylinders, 16383);
                pDisk->cHeads = RT_MIN(pDisk->cHeads, 255);
                pDisk->cSectors = RT_MIN(pDisk->cSectors, 255);
            }
        }
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get virtual disk translation mode stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   penmTranslation Where to store the translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VDGetTranslation(PVBOXHDD pDisk,
                                   PPDMBIOSTRANSLATION penmTranslation)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pBase;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        if (pDisk->enmTranslation != 0)
            *penmTranslation = pDisk->enmTranslation;
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    LogFlow(("%s: %Vrc (translation=%u)\n", __FUNCTION__, rc,
             pDisk->enmTranslation));
    return rc;
}

/**
 * Store virtual disk translation mode in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   enmTranslation  Translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VDSetTranslation(PVBOXHDD pDisk,
                                   PDMBIOSTRANSLATION enmTranslation)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pBase;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        if (enmTranslation == 0)
            rc = VERR_INVALID_PARAMETER;
        else if (enmTranslation != pDisk->enmTranslation)
        {
            /* Only update translation mode if it is changed. Avoids similar
             * checks in every backend. Most of the time the new translation
             * mode is set to the previous value, so no need to go through the
             * hassle of updating an image which could be opened in read-only
             * mode right now. */
            rc = pDisk->Backend->pfnSetTranslation(pImage->pvBackendData,
                                                   enmTranslation);

            /* Cache new translation mode in any case, whether successful or not. */
            int rc2 = pDisk->Backend->pfnGetTranslation(pImage->pvBackendData,
                                                        &pDisk->enmTranslation);
            if (VBOX_FAILURE(rc2))
                pDisk->enmTranslation = (PDMBIOSTRANSLATION)0;
        }
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get version of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDGetVersion(PVBOXHDD pDisk, unsigned nImage,
                               unsigned *puVersion)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Get type of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VDGetImageType(PVBOXHDD pDisk, unsigned nImage,
                                 PVDIMAGETYPE penmType)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Get flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VDGetImageFlags(PVBOXHDD pDisk, unsigned nImage,
                                  unsigned *puImageFlags)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Get open flags of last opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   puOpenFlags     Where to store the image open flags.
 */
VBOXDDU_DECL(int) VDGetOpenFlags(PVBOXHDD pDisk, unsigned *puOpenFlags)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    unsigned uOpenFlags = 0;
    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pLast;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
    {
        uOpenFlags = pDisk->Backend->pfnGetOpenFlags(pDisk->pLast->pvBackendData);
        *puOpenFlags = uOpenFlags;
    }
    LogFlow(("%s: returns %Vrc uOpenFlags=%#x\n", __FUNCTION__, rc, uOpenFlags));
    return uOpenFlags;
}

/**
 * Set open flags of last opened image in HDD container.
 * This operation may cause file locking changes and/or files being reopened.
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns Virtual disk block size in bytes.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDSetOpenFlags(PVBOXHDD pDisk, unsigned uOpenFlags)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    int rc = VINF_SUCCESS;
    PVDIMAGE pImage = pDisk->pLast;
    if (RT_UNLIKELY(!pImage))
    {
        Assert(pImage);
        rc = VERR_VDI_NOT_OPENED;
    }
    else
        rc = pDisk->Backend->pfnSetOpenFlags(pDisk->pLast->pvBackendData,
                                             uOpenFlags);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get base filename of image in HDD container. Some image formats use
 * other filenames as well, so don't use this for anything but for informational
 * purposes.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VDGetFilename(PVBOXHDD pDisk, unsigned nImage,
                                char *pszFilename, unsigned cbFilename)
{
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Get the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VDGetComment(PVBOXHDD pDisk, unsigned nImage,
                               char *pszComment, unsigned cbComment)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pszComment);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnGetComment(pImage->pvBackendData, pszComment,
                                           cbComment);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc, comment='%s' nImage=%u\n", __FUNCTION__,
             rc, pszComment, nImage));
    return rc;
}

/**
 * Changes the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDSetComment(PVBOXHDD pDisk, unsigned nImage,
                               const char *pszComment)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    LogFlow(("%s: comment='%s' nImage=%u\n", __FUNCTION__, pszComment, nImage));

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnSetComment(pImage->pvBackendData, pszComment);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * Get UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image creation UUID.
 */
VBOXDDU_DECL(int) VDGetUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnGetUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc, uuid={%Vuuid} nImage=%u\n", __FUNCTION__,
             rc, pUuid, nImage));
    return rc;
}

/**
 * Set the image's UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new UUID of the image.
 */
VBOXDDU_DECL(int) VDSetUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid)
{
    LogFlow(("%s: uuid={%Vuuid} nImage=%u\n", __FUNCTION__, pUuid, nImage));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnSetUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get last modification UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification UUID.
 */
VBOXDDU_DECL(int) VDGetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnGetModificationUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc, uuid={%Vuuid} nImage=%u\n", __FUNCTION__,
             rc, pUuid, nImage));
    return rc;
}

/**
 * Set the image's last modification UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new last modification UUID of the image.
 */
VBOXDDU_DECL(int) VDSetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid)
{
    LogFlow(("%s: uuid={%Vuuid} nImage=%u\n", __FUNCTION__, pUuid, nImage));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnSetModificationUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Get parent UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the parent image UUID.
 */
VBOXDDU_DECL(int) VDGetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PRTUUID pUuid)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnGetParentUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc, uuid={%Vuuid} nImage=%u\n", __FUNCTION__,
             rc, pUuid, nImage));
    return rc;
}

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new parent UUID of the image.
 */
VBOXDDU_DECL(int) VDSetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PCRTUUID pUuid)
{
    LogFlow(("%s: uuid={%Vuuid} nImage=%u\n", __FUNCTION__, pUuid, nImage));
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));
    Assert(pUuid);

    PVDIMAGE pImage = vdGetImageByNumber(pDisk, nImage);
    int rc;
    if (pImage)
        rc = pDisk->Backend->pfnSetParentUuid(pImage->pvBackendData, pUuid);
    else
        rc = VERR_VDI_IMAGE_NOT_FOUND;

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * Debug helper - dumps all opened images in HDD container into the log file.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(void) VDDumpImages(PVBOXHDD pDisk)
{
    /* sanity check */
    Assert(pDisk);
    AssertMsg(pDisk->u32Signature == VBOXHDDDISK_SIGNATURE, ("u32Signature=%08x\n", pDisk->u32Signature));

    RTLogPrintf("--- Dumping VDI Disk, Images=%u\n", pDisk->cImages);
    for (PVDIMAGE pImage = pDisk->pBase; pImage; pImage = pImage->pNext)
    {
        RTLogPrintf("Dumping VDI image \"%s\" file=%#p\n", pImage->pszFilename);
        /** @todo call backend to print its part. */
    }
}

