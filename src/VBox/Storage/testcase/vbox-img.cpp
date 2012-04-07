/* $Id$ */
/** @file
 * Standalone image manipulation tool
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#include <VBox/vd.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <iprt/initterm.h>
#include <iprt/buildconfig.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/stream.h>
#include <iprt/message.h>
#include <iprt/getopt.h>
#include <iprt/assert.h>

const char *g_pszProgName = "";
static void printUsage(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm,
                 "Usage: %s\n"
                 "   setuuid      --filename <filename>\n"
                 "                [--format VDI|VMDK|VHD|...]\n"
                 "                [--uuid <uuid>]\n"
                 "                [--parentuuid <uuid>]\n"
                 "                [--zeroparentuuid]\n"
                 "\n"
                 "   convert      --srcfilename <filename>\n"
                 "                --dstfilename <filename>\n"
                 "                [--stdin]|[--stdout]\n"
                 "                [--srcformat VDI|VMDK|VHD|RAW|..]\n"
                 "                [--dstformat VDI|VMDK|VHD|RAW|..]\n"
                 "                [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "\n"
                 "   info         --filename <filename>\n"
                 "\n"
                 "   compact      --filename <filename>\n"
                 "\n"
                 "   createcache  --filename <filename>\n"
                 "                --size <cache size>\n"
                 "\n"
                 "   createbase   --filename <filename>\n"
                 "                --size <size in bytes>\n"
                 "                [--format VDI|VMDK|VHD] (default: VDI)\n"
                 "                [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "\n"
                 "   repair       --filename <filename>\n"
                 "                [--dry-run]\n"
                 "                [--format VDI|VMDK|VHD] (default: autodetect)\n",
                 g_pszProgName);
}

void showLogo(PRTSTREAM pStrm)
{
    static bool s_fShown; /* show only once */

    if (!s_fShown)
    {
        RTStrmPrintf(pStrm, VBOX_PRODUCT " Disk Utility " VBOX_VERSION_STRING "\n"
                     "(C) 2005-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                     "All rights reserved.\n"
                     "\n");
        s_fShown = true;
    }
}

/** command handler argument */
struct HandlerArg
{
    int argc;
    char **argv;
};

PVDINTERFACE pVDIfs;

static DECLCALLBACK(void) handleVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                                        const char *pszFormat, va_list va)
{
    NOREF(pvUser);
    NOREF(rc);
    RTMsgErrorV(pszFormat, va);
}

static int handleVDMessage(void *pvUser, const char *pszFormat, va_list va)
{
    NOREF(pvUser);
    RTPrintfV(pszFormat, va);
    return VINF_SUCCESS;
}

/**
 * Print a usage synopsis and the syntax error message.
 */
int errorSyntax(const char *pszFormat, ...)
{
    va_list args;
    showLogo(g_pStdErr); // show logo even if suppressed
    va_start(args, pszFormat);
    RTStrmPrintf(g_pStdErr, "\nSyntax error: %N\n", pszFormat, &args);
    va_end(args);
    printUsage(g_pStdErr);
    return 1;
}

int errorRuntime(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTMsgErrorV(pszFormat, args);
    va_end(args);
    return 1;
}

static int parseDiskVariant(const char *psz, unsigned *puImageFlags)
{
    int rc = VINF_SUCCESS;
    unsigned uImageFlags = *puImageFlags;

    while (psz && *psz && RT_SUCCESS(rc))
    {
        size_t len;
        const char *pszComma = strchr(psz, ',');
        if (pszComma)
            len = pszComma - psz;
        else
            len = strlen(psz);
        if (len > 0)
        {
            /*
             * Parsing is intentionally inconsistent: "standard" resets the
             * variant, whereas the other flags are cumulative.
             */
            if (!RTStrNICmp(psz, "standard", len))
                uImageFlags = VD_IMAGE_FLAGS_NONE;
            else if (   !RTStrNICmp(psz, "fixed", len)
                     || !RTStrNICmp(psz, "static", len))
                uImageFlags |= VD_IMAGE_FLAGS_FIXED;
            else if (!RTStrNICmp(psz, "Diff", len))
                uImageFlags |= VD_IMAGE_FLAGS_DIFF;
            else if (!RTStrNICmp(psz, "split2g", len))
                uImageFlags |= VD_VMDK_IMAGE_FLAGS_SPLIT_2G;
            else if (   !RTStrNICmp(psz, "stream", len)
                     || !RTStrNICmp(psz, "streamoptimized", len))
                uImageFlags |= VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED;
            else if (!RTStrNICmp(psz, "esx", len))
                uImageFlags |= VD_VMDK_IMAGE_FLAGS_ESX;
            else
                rc = VERR_PARSE_ERROR;
        }
        if (pszComma)
            psz += len + 1;
        else
            psz += len;
    }

    if (RT_SUCCESS(rc))
        *puImageFlags = uImageFlags;
    return rc;
}


int handleSetUUID(HandlerArg *a)
{
    const char *pszFilename = NULL;
    char *pszFormat = NULL;
    VDTYPE enmType = VDTYPE_INVALID;
    RTUUID imageUuid;
    RTUUID parentUuid;
    bool fSetImageUuid = false;
    bool fSetParentUuid = false;
    RTUuidClear(&imageUuid);
    RTUuidClear(&parentUuid);
    int rc;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING },
        { "--format", 'o', RTGETOPT_REQ_STRING },
        { "--uuid", 'u', RTGETOPT_REQ_UUID },
        { "--parentuuid", 'p', RTGETOPT_REQ_UUID },
        { "--zeroparentuuid", 'P', RTGETOPT_REQ_NOTHING }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;
            case 'o':   // --format
                pszFormat = RTStrDup(ValueUnion.psz);
                break;
            case 'u':   // --uuid
                imageUuid = ValueUnion.Uuid;
                fSetImageUuid = true;
                break;
            case 'p':   // --parentuuid
                parentUuid = ValueUnion.Uuid;
                fSetParentUuid = true;
                break;
            case 'P':   // --zeroparentuuid
                RTUuidClear(&parentUuid);
                fSetParentUuid = true;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* Check for consistency of optional parameters. */
    if (fSetImageUuid && RTUuidIsNull(&imageUuid))
        return errorSyntax("Invalid parameter to --uuid option\n");

    /* Autodetect image format. */
    if (!pszFormat)
    {
        /* Don't pass error interface, as that would triggers error messages
         * because some backends fail to open the image. */
        rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat, &enmType);
        if (RT_FAILURE(rc))
            return errorRuntime("Format autodetect failed: %Rrc\n", rc);
    }

    PVBOXHDD pVD = NULL;
    rc = VDCreate(pVDIfs, enmType, &pVD);
    if (RT_FAILURE(rc))
        return errorRuntime("Cannot create the virtual disk container: %Rrc\n", rc);


    rc = VDOpen(pVD, pszFormat, pszFilename, VD_OPEN_FLAGS_NORMAL, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Cannot open the virtual disk image \"%s\": %Rrc\n",
                            pszFilename, rc);

    RTUUID oldImageUuid;
    rc = VDGetUuid(pVD, VD_LAST_IMAGE, &oldImageUuid);
    if (RT_FAILURE(rc))
        return errorRuntime("Cannot get UUID of virtual disk image \"%s\": %Rrc\n",
                            pszFilename, rc);

    RTPrintf("Old image UUID:  %RTuuid\n", &oldImageUuid);

    RTUUID oldParentUuid;
    rc = VDGetParentUuid(pVD, VD_LAST_IMAGE, &oldParentUuid);
    if (RT_FAILURE(rc))
        return errorRuntime("Cannot get parent UUID of virtual disk image \"%s\": %Rrc\n",
                            pszFilename, rc);

    RTPrintf("Old parent UUID: %RTuuid\n", &oldParentUuid);

    if (fSetImageUuid)
    {
        RTPrintf("New image UUID:  %RTuuid\n", &imageUuid);
        rc = VDSetUuid(pVD, VD_LAST_IMAGE, &imageUuid);
        if (RT_FAILURE(rc))
            return errorRuntime("Cannot set UUID of virtual disk image \"%s\": %Rrc\n",
                                pszFilename, rc);
    }

    if (fSetParentUuid)
    {
        RTPrintf("New parent UUID: %RTuuid\n", &parentUuid);
        rc = VDSetParentUuid(pVD, VD_LAST_IMAGE, &parentUuid);
        if (RT_FAILURE(rc))
            return errorRuntime("Cannot set parent UUID of virtual disk image \"%s\": %Rrc\n",
                                pszFilename, rc);
    }

    VDDestroy(pVD);

    if (pszFormat)
    {
        RTStrFree(pszFormat);
        pszFormat = NULL;
    }

    return 0;
}


typedef struct FILEIOSTATE
{
    RTFILE file;
    /** Offset in the file. */
    uint64_t off;
    /** Offset where the buffer contents start. UINT64_MAX=buffer invalid. */
    uint64_t offBuffer;
    /** Size of valid data in the buffer. */
    uint32_t cbBuffer;
    /** Buffer for efficient I/O */
    uint8_t abBuffer[16 *_1M];
} FILEIOSTATE, *PFILEIOSTATE;

static int convInOpen(void *pvUser, const char *pszLocation,
                      uint32_t fOpen, PFNVDCOMPLETED pfnCompleted,
                      void **ppStorage)
{
    NOREF(pvUser);
    /* Validate input. */
    AssertPtrReturn(ppStorage, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCompleted, VERR_INVALID_PARAMETER);
    AssertReturn((fOpen & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ, VERR_INVALID_PARAMETER);
    RTFILE file;
    int rc = RTFileFromNative(&file, RTFILE_NATIVE_STDIN);
    if (RT_FAILURE(rc))
        return rc;

    /* No need to clear the buffer, the data will be read from disk. */
    PFILEIOSTATE pFS = (PFILEIOSTATE)RTMemAlloc(sizeof(FILEIOSTATE));
    if (!pFS)
        return VERR_NO_MEMORY;

    pFS->file = file;
    pFS->off = 0;
    pFS->offBuffer = UINT64_MAX;
    pFS->cbBuffer = 0;

    *ppStorage = pFS;
    return VINF_SUCCESS;
}

static int convInClose(void *pvUser, void *pStorage)
{
    NOREF(pvUser);
    AssertPtrReturn(pStorage, VERR_INVALID_POINTER);
    PFILEIOSTATE pFS = (PFILEIOSTATE)pStorage;

    RTMemFree(pFS);

    return VINF_SUCCESS;
}

static int convInDelete(void *pvUser, const char *pcszFilename)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInMove(void *pvUser, const char *pcszSrc, const char *pcszDst,
                      unsigned fMove)
{
    NOREF(pvUser);
    NOREF(pcszSrc);
    NOREF(pcszDst);
    NOREF(fMove);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInGetFreeSpace(void *pvUser, const char *pcszFilename,
                              int64_t *pcbFreeSpace)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertPtrReturn(pcbFreeSpace, VERR_INVALID_POINTER);
    *pcbFreeSpace = 0;
    return VINF_SUCCESS;
}

static int convInGetModificationTime(void *pvUser, const char *pcszFilename,
                                     PRTTIMESPEC pModificationTime)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertPtrReturn(pModificationTime, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInGetSize(void *pvUser, void *pStorage, uint64_t *pcbSize)
{
    NOREF(pvUser);
    NOREF(pStorage);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInSetSize(void *pvUser, void *pStorage, uint64_t cbSize)
{
    NOREF(pvUser);
    NOREF(pStorage);
    NOREF(cbSize);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInRead(void *pvUser, void *pStorage, uint64_t uOffset,
                      void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    NOREF(pvUser);
    AssertPtrReturn(pStorage, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    PFILEIOSTATE pFS = (PFILEIOSTATE)pStorage;
    AssertReturn(uOffset >= pFS->off, VERR_INVALID_PARAMETER);
    int rc;

    /* Fill buffer if it is empty. */
    if (pFS->offBuffer == UINT64_MAX)
    {
        /* Repeat reading until buffer is full or EOF. */
        size_t cbSumRead = 0, cbRead;
        uint8_t *pTmp = (uint8_t *)&pFS->abBuffer[0];
        size_t cbTmp = sizeof(pFS->abBuffer);
        do
        {
            rc = RTFileRead(pFS->file, pTmp, cbTmp, &cbRead);
            if (RT_FAILURE(rc))
                return rc;
            pTmp += cbRead;
            cbTmp -= cbRead;
            cbSumRead += cbRead;
        } while (cbTmp && cbRead);

        pFS->offBuffer = 0;
        pFS->cbBuffer = cbSumRead;
        if (!cbSumRead && !pcbRead) /* Caller can't handle partial reads. */
            return VERR_EOF;
    }

    /* Read several blocks and assemble the result if necessary */
    size_t cbTotalRead = 0;
    do
    {
        /* Skip over areas no one wants to read. */
        while (uOffset > pFS->offBuffer + pFS->cbBuffer - 1)
        {
            if (pFS->cbBuffer < sizeof(pFS->abBuffer))
            {
                if (pcbRead)
                    *pcbRead = cbTotalRead;
                return VERR_EOF;
            }

            /* Repeat reading until buffer is full or EOF. */
            size_t cbSumRead = 0, cbRead;
            uint8_t *pTmp = (uint8_t *)&pFS->abBuffer[0];
            size_t cbTmp = sizeof(pFS->abBuffer);
            do
            {
                rc = RTFileRead(pFS->file, pTmp, cbTmp, &cbRead);
                if (RT_FAILURE(rc))
                    return rc;
                pTmp += cbRead;
                cbTmp -= cbRead;
                cbSumRead += cbRead;
            } while (cbTmp && cbRead);

            pFS->offBuffer += pFS->cbBuffer;
            pFS->cbBuffer = cbSumRead;
        }

        uint32_t cbThisRead = RT_MIN(cbBuffer,
                                     pFS->cbBuffer - uOffset % sizeof(pFS->abBuffer));
        memcpy(pvBuffer, &pFS->abBuffer[uOffset % sizeof(pFS->abBuffer)],
               cbThisRead);
        uOffset += cbThisRead;
        pvBuffer = (uint8_t *)pvBuffer + cbThisRead;
        cbBuffer -= cbThisRead;
        cbTotalRead += cbThisRead;
        if (!cbTotalRead && !pcbRead) /* Caller can't handle partial reads. */
            return VERR_EOF;
    } while (cbBuffer > 0);

    if (pcbRead)
        *pcbRead = cbTotalRead;

    pFS->off = uOffset;

    return VINF_SUCCESS;
}

static int convInWrite(void *pvUser, void *pStorage, uint64_t uOffset,
                       const void *pvBuffer, size_t cbBuffer,
                       size_t *pcbWritten)
{
    NOREF(pvUser);
    NOREF(pStorage);
    NOREF(uOffset);
    NOREF(cbBuffer);
    NOREF(pcbWritten);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convInFlush(void *pvUser, void *pStorage)
{
    NOREF(pvUser);
    NOREF(pStorage);
    return VINF_SUCCESS;
}

static int convOutOpen(void *pvUser, const char *pszLocation,
                       uint32_t fOpen, PFNVDCOMPLETED pfnCompleted,
                       void **ppStorage)
{
    NOREF(pvUser);
    /* Validate input. */
    AssertPtrReturn(ppStorage, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCompleted, VERR_INVALID_PARAMETER);
    AssertReturn((fOpen & RTFILE_O_ACCESS_MASK) == RTFILE_O_WRITE, VERR_INVALID_PARAMETER);
    RTFILE file;
    int rc = RTFileFromNative(&file, RTFILE_NATIVE_STDOUT);
    if (RT_FAILURE(rc))
        return rc;

    /* Must clear buffer, so that skipped over data is initialized properly. */
    PFILEIOSTATE pFS = (PFILEIOSTATE)RTMemAllocZ(sizeof(FILEIOSTATE));
    if (!pFS)
        return VERR_NO_MEMORY;

    pFS->file = file;
    pFS->off = 0;
    pFS->offBuffer = 0;
    pFS->cbBuffer = sizeof(FILEIOSTATE);

    *ppStorage = pFS;
    return VINF_SUCCESS;
}

static int convOutClose(void *pvUser, void *pStorage)
{
    NOREF(pvUser);
    AssertPtrReturn(pStorage, VERR_INVALID_POINTER);
    PFILEIOSTATE pFS = (PFILEIOSTATE)pStorage;
    int rc = VINF_SUCCESS;

    /* Flush any remaining buffer contents. */
    if (pFS->cbBuffer)
        rc = RTFileWrite(pFS->file, &pFS->abBuffer[0], pFS->cbBuffer, NULL);

    RTMemFree(pFS);

    return rc;
}

static int convOutDelete(void *pvUser, const char *pcszFilename)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutMove(void *pvUser, const char *pcszSrc, const char *pcszDst,
                       unsigned fMove)
{
    NOREF(pvUser);
    NOREF(pcszSrc);
    NOREF(pcszDst);
    NOREF(fMove);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutGetFreeSpace(void *pvUser, const char *pcszFilename,
                               int64_t *pcbFreeSpace)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertPtrReturn(pcbFreeSpace, VERR_INVALID_POINTER);
    *pcbFreeSpace = INT64_MAX;
    return VINF_SUCCESS;
}

static int convOutGetModificationTime(void *pvUser, const char *pcszFilename,
                                      PRTTIMESPEC pModificationTime)
{
    NOREF(pvUser);
    NOREF(pcszFilename);
    AssertPtrReturn(pModificationTime, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutGetSize(void *pvUser, void *pStorage, uint64_t *pcbSize)
{
    NOREF(pvUser);
    NOREF(pStorage);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutSetSize(void *pvUser, void *pStorage, uint64_t cbSize)
{
    NOREF(pvUser);
    NOREF(pStorage);
    NOREF(cbSize);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutRead(void *pvUser, void *pStorage, uint64_t uOffset,
                       void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    NOREF(pvUser);
    NOREF(pStorage);
    NOREF(uOffset);
    NOREF(cbBuffer);
    NOREF(pcbRead);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}

static int convOutWrite(void *pvUser, void *pStorage, uint64_t uOffset,
                        const void *pvBuffer, size_t cbBuffer,
                        size_t *pcbWritten)
{
    NOREF(pvUser);
    AssertPtrReturn(pStorage, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    PFILEIOSTATE pFS = (PFILEIOSTATE)pStorage;
    AssertReturn(uOffset >= pFS->off, VERR_INVALID_PARAMETER);
    int rc;

    /* Write the data to the buffer, flushing as required. */
    size_t cbTotalWritten = 0;
    do
    {
        /* Flush the buffer if we need a new one. */
        while (uOffset > pFS->offBuffer + sizeof(pFS->abBuffer) - 1)
        {
            rc = RTFileWrite(pFS->file, &pFS->abBuffer[0],
                             sizeof(pFS->abBuffer), NULL);
            RT_ZERO(pFS->abBuffer);
            pFS->offBuffer += sizeof(pFS->abBuffer);
            pFS->cbBuffer = 0;
        }

        uint32_t cbThisWrite = RT_MIN(cbBuffer,
                                      sizeof(pFS->abBuffer) - uOffset % sizeof(pFS->abBuffer));
        memcpy(&pFS->abBuffer[uOffset % sizeof(pFS->abBuffer)], pvBuffer,
               cbThisWrite);
        uOffset += cbThisWrite;
        pvBuffer = (uint8_t *)pvBuffer + cbThisWrite;
        cbBuffer -= cbThisWrite;
        cbTotalWritten += cbThisWrite;
    } while (cbBuffer > 0);

    if (pcbWritten)
        *pcbWritten = cbTotalWritten;

    pFS->cbBuffer = uOffset % sizeof(pFS->abBuffer);
    if (!pFS->cbBuffer)
        pFS->cbBuffer = sizeof(pFS->abBuffer);
    pFS->off = uOffset;

    return VINF_SUCCESS;
}

static int convOutFlush(void *pvUser, void *pStorage)
{
    NOREF(pvUser);
    NOREF(pStorage);
    return VINF_SUCCESS;
}

int handleConvert(HandlerArg *a)
{
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    bool fStdIn = false;
    bool fStdOut = false;
    const char *pszSrcFormat = NULL;
    VDTYPE enmSrcType = VDTYPE_HDD;
    const char *pszDstFormat = NULL;
    const char *pszVariant = NULL;
    PVBOXHDD pSrcDisk = NULL;
    PVBOXHDD pDstDisk = NULL;
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    PVDINTERFACE pIfsImageInput = NULL;
    PVDINTERFACE pIfsImageOutput = NULL;
    VDINTERFACEIO IfsInputIO;
    VDINTERFACEIO IfsOutputIO;
    int rc = VINF_SUCCESS;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--srcfilename", 'i', RTGETOPT_REQ_STRING },
        { "--dstfilename", 'o', RTGETOPT_REQ_STRING },
        { "--stdin", 'p', RTGETOPT_REQ_NOTHING },
        { "--stdout", 'P', RTGETOPT_REQ_NOTHING },
        { "--srcformat", 's', RTGETOPT_REQ_STRING },
        { "--dstformat", 'd', RTGETOPT_REQ_STRING },
        { "--variant", 'v', RTGETOPT_REQ_STRING }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'i':   // --srcfilename
                pszSrcFilename = ValueUnion.psz;
                break;
            case 'o':   // --dstfilename
                pszDstFilename = ValueUnion.psz;
                break;
            case 'p':   // --stdin
                fStdIn = true;
                break;
            case 'P':   // --stdout
                fStdOut = true;
                break;
            case 's':   // --srcformat
                pszSrcFormat = ValueUnion.psz;
                break;
            case 'd':   // --dstformat
                pszDstFormat = ValueUnion.psz;
                break;
            case 'v':   // --variant
                pszVariant = ValueUnion.psz;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters and handle dummies/defaults. */
    if (fStdIn && !pszSrcFormat)
        return errorSyntax("Mandatory --srcformat option missing\n");
    if (!pszDstFormat)
        pszDstFormat = "VDI";
    if (fStdIn && !pszSrcFilename)
    {
        /* Complete dummy, will be just passed to various calls to fulfill
         * the "must be non-NULL" requirement, and is completely ignored
         * otherwise. It shown in the stderr message below. */
        pszSrcFilename = "stdin";
    }
    if (fStdOut && !pszDstFilename)
    {
        /* Will be stored in the destination image if it is a streamOptimized
         * VMDK, but it isn't really relevant - use it for "branding". */
        if (!RTStrICmp(pszDstFormat, "VMDK"))
            pszDstFilename = "VirtualBoxStream.vmdk";
        else
            pszDstFilename = "stdout";
    }
    if (!pszSrcFilename)
        return errorSyntax("Mandatory --srcfilename option missing\n");
    if (!pszDstFilename)
        return errorSyntax("Mandatory --dstfilename option missing\n");

    if (fStdIn)
    {
        IfsInputIO.pfnOpen                = convInOpen;
        IfsInputIO.pfnClose               = convInClose;
        IfsInputIO.pfnDelete              = convInDelete;
        IfsInputIO.pfnMove                = convInMove;
        IfsInputIO.pfnGetFreeSpace        = convInGetFreeSpace;
        IfsInputIO.pfnGetModificationTime = convInGetModificationTime;
        IfsInputIO.pfnGetSize             = convInGetSize;
        IfsInputIO.pfnSetSize             = convInSetSize;
        IfsInputIO.pfnReadSync            = convInRead;
        IfsInputIO.pfnWriteSync           = convInWrite;
        IfsInputIO.pfnFlushSync           = convInFlush;
        VDInterfaceAdd(&IfsInputIO.Core, "stdin", VDINTERFACETYPE_IO,
                       NULL, sizeof(VDINTERFACEIO), &pIfsImageInput);
    }
    if (fStdOut)
    {
        IfsOutputIO.pfnOpen                   = convOutOpen;
        IfsOutputIO.pfnClose                  = convOutClose;
        IfsOutputIO.pfnDelete                 = convOutDelete;
        IfsOutputIO.pfnMove                   = convOutMove;
        IfsOutputIO.pfnGetFreeSpace           = convOutGetFreeSpace;
        IfsOutputIO.pfnGetModificationTime    = convOutGetModificationTime;
        IfsOutputIO.pfnGetSize                = convOutGetSize;
        IfsOutputIO.pfnSetSize                = convOutSetSize;
        IfsOutputIO.pfnReadSync               = convOutRead;
        IfsOutputIO.pfnWriteSync              = convOutWrite;
        IfsOutputIO.pfnFlushSync              = convOutFlush;
        VDInterfaceAdd(&IfsOutputIO.Core, "stdout", VDINTERFACETYPE_IO,
                       NULL, sizeof(VDINTERFACEIO), &pIfsImageOutput);
    }

    /* check the variant parameter */
    if (pszVariant)
    {
        char *psz = (char*)pszVariant;
        while (psz && *psz && RT_SUCCESS(rc))
        {
            size_t len;
            const char *pszComma = strchr(psz, ',');
            if (pszComma)
                len = pszComma - psz;
            else
                len = strlen(psz);
            if (len > 0)
            {
                if (!RTStrNICmp(pszVariant, "standard", len))
                    uImageFlags |= VD_IMAGE_FLAGS_NONE;
                else if (!RTStrNICmp(pszVariant, "fixed", len))
                    uImageFlags |= VD_IMAGE_FLAGS_FIXED;
                else if (!RTStrNICmp(pszVariant, "split2g", len))
                    uImageFlags |= VD_VMDK_IMAGE_FLAGS_SPLIT_2G;
                else if (!RTStrNICmp(pszVariant, "stream", len))
                    uImageFlags |= VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED;
                else if (!RTStrNICmp(pszVariant, "esx", len))
                    uImageFlags |= VD_VMDK_IMAGE_FLAGS_ESX;
                else
                    return errorSyntax("Invalid --variant option\n");
            }
            if (pszComma)
                psz += len + 1;
            else
                psz += len;
        }
    }

    do
    {
        /* try to determine input format if not specified */
        if (!pszSrcFormat)
        {
            char *pszFormat = NULL;
            VDTYPE enmType = VDTYPE_INVALID;
            rc = VDGetFormat(NULL, NULL, pszSrcFilename, &pszFormat, &enmType);
            if (RT_FAILURE(rc))
            {
                errorSyntax("No file format specified, please specify format: %Rrc\n", rc);
                break;
            }
            pszSrcFormat = pszFormat;
            enmSrcType = enmType;
        }

        rc = VDCreate(pVDIfs, enmSrcType, &pSrcDisk);
        if (RT_FAILURE(rc))
        {
            errorRuntime("Error while creating source disk container: %Rrc\n", rc);
            break;
        }

        rc = VDOpen(pSrcDisk, pszSrcFormat, pszSrcFilename,
                    VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_SEQUENTIAL,
                    pIfsImageInput);
        if (RT_FAILURE(rc))
        {
            errorRuntime("Error while opening source image: %Rrc\n", rc);
            break;
        }

        rc = VDCreate(pVDIfs, VDTYPE_HDD, &pDstDisk);
        if (RT_FAILURE(rc))
        {
            errorRuntime("Error while creating the destination disk container: %Rrc\n", rc);
            break;
        }

        uint64_t cbSize = VDGetSize(pSrcDisk, VD_LAST_IMAGE);
        RTStrmPrintf(g_pStdErr, "Converting image \"%s\" with size %RU64 bytes (%RU64MB)...\n", pszSrcFilename, cbSize, (cbSize + _1M - 1) / _1M);

        /* Create the output image */
        rc = VDCopy(pSrcDisk, VD_LAST_IMAGE, pDstDisk, pszDstFormat,
                    pszDstFilename, false, 0, uImageFlags, NULL,
                    VD_OPEN_FLAGS_NORMAL | VD_OPEN_FLAGS_SEQUENTIAL, NULL,
                    pIfsImageOutput, NULL);
        if (RT_FAILURE(rc))
        {
            errorRuntime("Error while copying the image: %Rrc\n", rc);
            break;
        }

    }
    while (0);

    if (pDstDisk)
        VDDestroy(pDstDisk);
    if (pSrcDisk)
        VDDestroy(pSrcDisk);

    return RT_SUCCESS(rc) ? 0 : 1;
}


int handleInfo(HandlerArg *a)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;
    const char *pszFilename = NULL;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* just try it */
    char *pszFormat = NULL;
    VDTYPE enmType = VDTYPE_INVALID;
    rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat, &enmType);
    if (RT_FAILURE(rc))
        return errorSyntax("Format autodetect failed: %Rrc\n", rc);

    rc = VDCreate(pVDIfs, enmType, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    /* Open the image */
    rc = VDOpen(pDisk, pszFormat, pszFilename, VD_OPEN_FLAGS_INFO, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while opening the image: %Rrc\n", rc);

    VDDumpImages(pDisk);

    VDDestroy(pDisk);

    return rc;
}


int handleCompact(HandlerArg *a)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;
    const char *pszFilename = NULL;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* just try it */
    char *pszFormat = NULL;
    VDTYPE enmType = VDTYPE_INVALID;
    rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat, &enmType);
    if (RT_FAILURE(rc))
        return errorSyntax("Format autodetect failed: %Rrc\n", rc);

    rc = VDCreate(pVDIfs, enmType, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    /* Open the image */
    rc = VDOpen(pDisk, pszFormat, pszFilename, VD_OPEN_FLAGS_NORMAL, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while opening the image: %Rrc\n", rc);

    rc = VDCompact(pDisk, 0, NULL);
    if (RT_FAILURE(rc))
        errorRuntime("Error while compacting image: %Rrc\n", rc);

    VDDestroy(pDisk);

    return rc;
}


int handleCreateCache(HandlerArg *a)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;
    const char *pszFilename = NULL;
    uint64_t cbSize = 0;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING },
        { "--size",     's', RTGETOPT_REQ_UINT64 }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;

            case 's':   // --size
                cbSize = ValueUnion.u64;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    if (!cbSize)
        return errorSyntax("Mandatory --size option missing\n");

    /* just try it */
    rc = VDCreate(pVDIfs, VDTYPE_HDD, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    rc = VDCreateCache(pDisk, "VCI", pszFilename, cbSize, VD_IMAGE_FLAGS_DEFAULT,
                       NULL, NULL, VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk cache: %Rrc\n", rc);

    VDDestroy(pDisk);

    return rc;
}


int handleCreateBase(HandlerArg *a)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;
    const char *pszFilename = NULL;
    const char *pszBackend  = "VDI";
    const char *pszVariant  = NULL;
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    uint64_t cbSize = 0;
    VDGEOMETRY LCHSGeometry, PCHSGeometry;

    memset(&LCHSGeometry, 0, sizeof(VDGEOMETRY));
    memset(&PCHSGeometry, 0, sizeof(VDGEOMETRY));

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING },
        { "--size",     's', RTGETOPT_REQ_UINT64 },
        { "--format",   'b', RTGETOPT_REQ_STRING },
        { "--variant",  'v', RTGETOPT_REQ_STRING }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;

            case 's':   // --size
                cbSize = ValueUnion.u64;
                break;

            case 'b':   // --format
                pszBackend = ValueUnion.psz;
                break;

            case 'v':   // --variant
                pszVariant = ValueUnion.psz;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    if (!cbSize)
        return errorSyntax("Mandatory --size option missing\n");

    if (pszVariant)
    {
        rc = parseDiskVariant(pszVariant, &uImageFlags);
        if (RT_FAILURE(rc))
            return errorSyntax("Invalid variant %s given\n", pszVariant);
    }

    /* just try it */
    rc = VDCreate(pVDIfs, VDTYPE_HDD, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    rc = VDCreateBase(pDisk, pszBackend, pszFilename, cbSize, uImageFlags,
                      NULL, &PCHSGeometry, &LCHSGeometry, NULL, VD_OPEN_FLAGS_NORMAL,
                      NULL, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk: %Rrc\n", rc);

    VDDestroy(pDisk);

    return rc;
}


int handleRepair(HandlerArg *a)
{
    int rc = VINF_SUCCESS;
    PVBOXHDD pDisk = NULL;
    const char *pszFilename = NULL;
    char *pszBackend = NULL;
    const char *pszFormat  = NULL;
    bool fDryRun = false;
    VDTYPE enmType = VDTYPE_HDD;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--filename", 'f', RTGETOPT_REQ_STRING  },
        { "--dry-run",  'd', RTGETOPT_REQ_NOTHING },
        { "--format",   'b', RTGETOPT_REQ_STRING  }
    };
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'f':   // --filename
                pszFilename = ValueUnion.psz;
                break;

            case 'd':   // --dry-run
                fDryRun = true;
                break;

            case 'b':   // --format
                pszFormat = ValueUnion.psz;
                break;

            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                printUsage(g_pStdErr);
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* just try it */
    if (!pszFormat)
    {
        rc = VDGetFormat(NULL, NULL, pszFilename, &pszBackend, &enmType);
        if (RT_FAILURE(rc))
            return errorSyntax("Format autodetect failed: %Rrc\n", rc);
        pszFormat = pszBackend;
    }

    rc = VDRepair(pVDIfs, NULL, pszFilename, pszFormat, fDryRun ? VD_REPAIR_DRY_RUN : 0);
    if (RT_FAILURE(rc))
        rc = errorRuntime("Error while repairing the virtual disk: %Rrc\n", rc);

    if (pszBackend)
        RTStrFree(pszBackend);
    return rc;
}


int main(int argc, char *argv[])
{
    int exitcode = 0;

    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    g_pszProgName = RTPathFilename(argv[0]);

    bool fShowLogo = false;
    int  iCmd      = 1;
    int  iCmdArg;

    /* global options */
    for (int i = 1; i < argc || argc <= iCmd; i++)
    {
        if (    argc <= iCmd
            ||  !strcmp(argv[i], "help")
            ||  !strcmp(argv[i], "-?")
            ||  !strcmp(argv[i], "-h")
            ||  !strcmp(argv[i], "-help")
            ||  !strcmp(argv[i], "--help"))
        {
            showLogo(g_pStdOut);
            printUsage(g_pStdOut);
            return 0;
        }

        if (   !strcmp(argv[i], "-v")
            || !strcmp(argv[i], "-version")
            || !strcmp(argv[i], "-Version")
            || !strcmp(argv[i], "--version"))
        {
            /* Print version number, and do nothing else. */
            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
            return 0;
        }

        if (   !strcmp(argv[i], "--nologo")
            || !strcmp(argv[i], "-nologo")
            || !strcmp(argv[i], "-q"))
        {
            /* suppress the logo */
            fShowLogo = false;
            iCmd++;
        }
        else
        {
            break;
        }
    }

    iCmdArg = iCmd + 1;

    if (fShowLogo)
        showLogo(g_pStdOut);

    /* initialize the VD backend with dummy handlers */
    VDINTERFACEERROR vdInterfaceError;
    vdInterfaceError.pfnError     = handleVDError;
    vdInterfaceError.pfnMessage   = handleVDMessage;

    rc = VDInterfaceAdd(&vdInterfaceError.Core, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                        NULL, sizeof(VDINTERFACEERROR), &pVDIfs);

    rc = VDInit();
    if (RT_FAILURE(rc))
    {
        errorSyntax("Initializing backends failed! rc=%Rrc\n", rc);
        return 1;
    }

    /*
     * All registered command handlers
     */
    static const struct
    {
        const char *command;
        int (*handler)(HandlerArg *a);
    } s_commandHandlers[] =
    {
        { "setuuid",     handleSetUUID     },
        { "convert",     handleConvert     },
        { "info",        handleInfo        },
        { "compact",     handleCompact     },
        { "createcache", handleCreateCache },
        { "createbase",  handleCreateBase  },
        { "repair",      handleRepair      },
        { NULL,                       NULL }
    };

    HandlerArg handlerArg = { 0, NULL };
    int commandIndex;
    for (commandIndex = 0; s_commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (!strcmp(s_commandHandlers[commandIndex].command, argv[iCmd]))
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            exitcode = s_commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!s_commandHandlers[commandIndex].command)
    {
        errorSyntax("Invalid command '%s'", argv[iCmd]);
        return 1;
    }

    rc = VDShutdown();
    if (RT_FAILURE(rc))
    {
        errorSyntax("Unloading backends failed! rc=%Rrc\n", rc);
        return 1;
    }

    return exitcode;
}

/* dummy stub for RuntimeR3 */
#ifndef RT_OS_WINDOWS
RTDECL(bool) RTAssertShouldPanic(void)
{
    return true;
}
#endif
