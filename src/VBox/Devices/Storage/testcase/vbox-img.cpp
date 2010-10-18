/* $Id$ */
/** @file
 * Standalone image manipulation tool
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <VBox/VBoxHDD.h>
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
                 "   compact      --filename <filename>\n",
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



int handleSetUUID(HandlerArg *a)
{
    const char *pszFilename = NULL;
    char *pszFormat = NULL;
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
        rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat);
        if (RT_FAILURE(rc))
            return errorRuntime("Format autodetect failed: %Rrc\n", rc);
    }

    PVBOXHDD pVD = NULL;
    rc = VDCreate(pVDIfs, &pVD);
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

    rc = VDCloseAll(pVD);
    if (RT_FAILURE(rc))
        return errorRuntime("Closing image failed! rc=%Rrc\n", rc);

    if (pszFormat)
    {
        RTStrFree(pszFormat);
        pszFormat = NULL;
    }

    return 0;
}


typedef struct FILEIOSTATE
{
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

    PFILEIOSTATE pFS = (PFILEIOSTATE)RTMemAlloc(sizeof(FILEIOSTATE));
    if (!pFS)
        return VERR_NO_MEMORY;

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
        size_t cbRead = 0;
        rc = RTFileRead(0, &pFS->abBuffer[0], sizeof(pFS->abBuffer),
                        &cbRead);
        if (RT_FAILURE(rc))
            return rc;

        pFS->offBuffer = 0;
        pFS->cbBuffer = cbRead;
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

            size_t cbRead = 0;
            rc = RTFileRead(0, &pFS->abBuffer[0], sizeof(pFS->abBuffer),
                            &cbRead);
            if (RT_FAILURE(rc))
            {
                if (pcbRead)
                    *pcbRead = cbTotalRead;
                return rc;
            }

            pFS->offBuffer += pFS->cbBuffer;
            pFS->cbBuffer = cbRead;
        }

        uint32_t cbThisRead = RT_MIN(cbBuffer,
                                     pFS->cbBuffer - uOffset % sizeof(pFS->abBuffer));
        memcpy(pvBuffer, &pFS->abBuffer[uOffset % sizeof(pFS->abBuffer)],
               cbThisRead);
        uOffset += cbThisRead;
        pvBuffer = (uint8_t *)pvBuffer + cbThisRead;
        cbBuffer -= cbThisRead;
        cbTotalRead += cbThisRead;
    } while (cbBuffer > 0);

    if (pcbRead)
        *pcbRead = cbTotalRead;

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

    PFILEIOSTATE pFS = (PFILEIOSTATE)RTMemAlloc(sizeof(FILEIOSTATE));
    if (!pFS)
        return VERR_NO_MEMORY;

    pFS->off = 0;
    pFS->offBuffer = UINT64_MAX;
    pFS->cbBuffer = 0;

    *ppStorage = pFS;
    return VINF_SUCCESS;
}

static int convOutClose(void *pvUser, void *pStorage)
{
    NOREF(pvUser);
    AssertPtrReturn(pStorage, VERR_INVALID_POINTER);

    PFILEIOSTATE pFS = (PFILEIOSTATE)pStorage;

    RTMemFree(pFS);

    return VINF_SUCCESS;
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
    NOREF(pStorage);
    NOREF(uOffset);
    NOREF(cbBuffer);
    NOREF(pcbWritten);
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
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
    char *pszSrcFormat = NULL;
    const char *pszDstFormat = NULL;
    const char *pszVariant = NULL;
    PVBOXHDD pSrcDisk = NULL;
    PVBOXHDD pDstDisk = NULL;
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    PVDINTERFACE pIfsImageInput = NULL;
    PVDINTERFACE pIfsImageOutput = NULL;
    VDINTERFACE IfsInputIO;
    VDINTERFACE IfsOutputIO;
    VDINTERFACEIO IfsInputIOCb;
    VDINTERFACEIO IfsOutputIOCb;
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
                pszSrcFormat = RTStrDup(ValueUnion.psz);
                break;
            case 'd':   // --dstformat
                pszDstFormat = RTStrDup(ValueUnion.psz);
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
    if (!pszSrcFilename)
        return errorSyntax("Mandatory --srcfilename option missing\n");
    if (!pszDstFilename)
        return errorSyntax("Mandatory --dstfilename option missing\n");

    if (fStdIn)
    {
        IfsInputIOCb.cbSize                 = sizeof(VDINTERFACEIO);
        IfsInputIOCb.enmInterface           = VDINTERFACETYPE_IO;
        IfsInputIOCb.pfnOpen                = convInOpen;
        IfsInputIOCb.pfnClose               = convInClose;
        IfsInputIOCb.pfnDelete              = convInDelete;
        IfsInputIOCb.pfnMove                = convInMove;
        IfsInputIOCb.pfnGetFreeSpace        = convInGetFreeSpace;
        IfsInputIOCb.pfnGetModificationTime = convInGetModificationTime;
        IfsInputIOCb.pfnGetSize             = convInGetSize;
        IfsInputIOCb.pfnSetSize             = convInSetSize;
        IfsInputIOCb.pfnReadSync            = convInRead;
        IfsInputIOCb.pfnWriteSync           = convInWrite;
        IfsInputIOCb.pfnFlushSync           = convInFlush;
        VDInterfaceAdd(&IfsInputIO, "stdin", VDINTERFACETYPE_IO,
                       &IfsInputIOCb, NULL, &pIfsImageInput);
    }
    if (fStdOut)
    {
        IfsOutputIOCb.cbSize                    = sizeof(VDINTERFACEIO);
        IfsOutputIOCb.enmInterface              = VDINTERFACETYPE_IO;
        IfsOutputIOCb.pfnOpen                   = convOutOpen;
        IfsOutputIOCb.pfnClose                  = convOutClose;
        IfsOutputIOCb.pfnDelete                 = convOutDelete;
        IfsOutputIOCb.pfnMove                   = convOutMove;
        IfsOutputIOCb.pfnGetFreeSpace           = convOutGetFreeSpace;
        IfsOutputIOCb.pfnGetModificationTime    = convOutGetModificationTime;
        IfsOutputIOCb.pfnGetSize                = convOutGetSize;
        IfsOutputIOCb.pfnSetSize                = convOutSetSize;
        IfsOutputIOCb.pfnReadSync               = convOutRead;
        IfsOutputIOCb.pfnWriteSync              = convOutWrite;
        IfsOutputIOCb.pfnFlushSync              = convOutFlush;
        VDInterfaceAdd(&IfsOutputIO, "stdout", VDINTERFACETYPE_IO,
                       &IfsOutputIOCb, NULL, &pIfsImageOutput);
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
            rc = VDGetFormat(NULL, NULL, pszSrcFilename, &pszSrcFormat);
            if (RT_FAILURE(rc))
            {
                errorSyntax("No file format specified, please specify format: %Rrc\n", rc);
                break;
            }
        }

        rc = VDCreate(pVDIfs, &pSrcDisk);
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

        /* output format defaults to VDI */
        if (!pszDstFormat)
            pszDstFormat = "VDI";

        rc = VDCreate(pVDIfs, &pDstDisk);
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
                    VD_OPEN_FLAGS_NORMAL, NULL, pIfsImageOutput, NULL);
        if (RT_FAILURE(rc))
        {
            errorRuntime("Error while copying the image: %Rrc\n", rc);
            break;
        }

    }
    while (0);

    if (pDstDisk)
        VDCloseAll(pDstDisk);
    if (pSrcDisk)
        VDCloseAll(pSrcDisk);

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
    rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat);
    if (RT_FAILURE(rc))
        return errorSyntax("Format autodetect failed: %Rrc\n", rc);

    rc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    /* Open the image */
    rc = VDOpen(pDisk, pszFormat, pszFilename, VD_OPEN_FLAGS_INFO, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while opening the image: %Rrc\n", rc);

    VDDumpImages(pDisk);

    VDCloseAll(pDisk);

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
    rc = VDGetFormat(NULL, NULL, pszFilename, &pszFormat);
    if (RT_FAILURE(rc))
        return errorSyntax("Format autodetect failed: %Rrc\n", rc);

    rc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while creating the virtual disk container: %Rrc\n", rc);

    /* Open the image */
    rc = VDOpen(pDisk, pszFormat, pszFilename, VD_OPEN_FLAGS_NORMAL, NULL);
    if (RT_FAILURE(rc))
        return errorRuntime("Error while opening the image: %Rrc\n", rc);

    rc = VDCompact(pDisk, 0, NULL);
    if (RT_FAILURE(rc))
        errorRuntime("Error while compacting image: %Rrc\n", rc);

    VDCloseAll(pDisk);

    return rc;
}


int main(int argc, char *argv[])
{
    RTR3Init();
    int rc;

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
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;
    vdInterfaceErrorCallbacks.pfnMessage   = handleVDMessage;

    rc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                        &vdInterfaceErrorCallbacks, NULL, &pVDIfs);

    rc = VDInit();
    if (RT_FAILURE(rc))
        return errorSyntax("Initalizing backends failed! rc=%Rrc\n");

    /*
     * All registered command handlers
     */
    static const struct
    {
        const char *command;
        int (*handler)(HandlerArg *a);
    } s_commandHandlers[] =
    {
        { "setuuid", handleSetUUID },
        { "convert", handleConvert },
        { "info",    handleInfo    },
        { "compact", handleCompact },
        { NULL,               NULL }
    };

    HandlerArg handlerArg = { 0, NULL };
    int commandIndex;
    for (commandIndex = 0; s_commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (!strcmp(s_commandHandlers[commandIndex].command, argv[iCmd]))
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            rc = s_commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!s_commandHandlers[commandIndex].command)
        return errorSyntax("Invalid command '%s'", argv[iCmd]);

    rc = VDShutdown();
    if (RT_FAILURE(rc))
        return errorSyntax("Unloading backends failed! rc=%Rrc\n", rc);

    return rc;
}

/* dummy stub for RuntimeR3 */
#ifndef RT_OS_WINDOWS
RTDECL(bool) RTAssertShouldPanic(void)
{
    return true;
}
#endif
