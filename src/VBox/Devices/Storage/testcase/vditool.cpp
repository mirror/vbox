/** @file
 *
 * VBox HDD container maintenance/conversion utility
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
#include <VBox/VBoxHDD.h>
#include <iprt/alloc.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/runtime.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <stdlib.h>



static void ascii2upper(char *psz)
{
    for (;*psz; psz++)
        if (*psz >= 'a' && *psz <= 'z')
            *psz += 'A' - 'a';
}

static int UsageExit()
{
    RTPrintf("Usage:   vditool <Command> [Params]\n" \
             "Commands and params:\n" \
             "    NEW Filename Mbytes          - create new image;\n" \
             "    DD  Filename DDFilename      - create new image from DD format image;\n" \
             "    CONVERT Filename             - convert VDI image from old format;\n" \
             "    DUMP Filename                - debug dump;\n" \
             "    RESETGEO Filename            - reset geometry information;\n" \
             "    COPY FromImage ToImage       - make image copy;\n" \
             "    COPYDD FromImage DDFilename  - make a DD copy of the image;\n" \
             "    SHRINK Filename              - optimize (reduce) VDI image size.\n");
    return 1;
}

static int SyntaxError(const char *pszMsg)
{
    RTPrintf("Syntax error: %s\n\n", pszMsg);
    UsageExit();
    return 1;
}

/**
 * Our internal functions use UTF8
 */
static int FilenameToUtf8(char **pszUtf8Filename, const char *pszFilename)
{
    int rc = RTStrCurrentCPToUtf8(pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        RTPrintf("Error converting filename '%s' to UTF8! (rc=%Rrc)\n",
                 pszFilename, rc);
    return rc;
}

/**
 * Prints a done message indicating success or failure.
 * @returns rc
 * @param   rc      Status code.
 */
static int PrintDone(int rc)
{
    if (rc == VINF_SUCCESS)
        RTPrintf("The operation completed successfully!\n");
    else if (VBOX_SUCCESS(rc))
        RTPrintf("The operation completed successfully! (rc=%Rrc)\n", rc);
    else
        RTPrintf("FAILURE: %Rrf (%Rrc)\n", rc, rc);
    return rc;
}

static int NewImage(const char *pszFilename, uint32_t cMBs)
{
    RTPrintf("Creating VDI: file=\"%s\" size=%u MB...\n",
            pszFilename, cMBs);

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = VDICreateBaseImage(pszUtf8Filename,
                            VDI_IMAGE_TYPE_NORMAL,
                            (uint64_t)cMBs * (uint64_t)(1024 * 1024),
                            "Newly created test image", NULL, NULL);
    return PrintDone(rc);
}

static int ConvertDDImage(const char *pszFilename, const char *pszDDFilename)
{
    RTPrintf("Converting VDI: from DD image file=\"%s\" to file=\"%s\"...\n",
             pszDDFilename, pszFilename);

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename, *pszUtf8DDFilename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = FilenameToUtf8(&pszUtf8DDFilename, pszDDFilename);
    if (VBOX_FAILURE(rc))
        return rc;

    /* open raw image file. */
    RTFILE File;
    rc = RTFileOpen(&File, pszUtf8DDFilename, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("File=\"%s\" open error: %Rrf\n", pszDDFilename, rc);
        return rc;
    }

    /* get image size. */
    uint64_t cbFile;
    rc = RTFileGetSize(File, &cbFile);
    if (VBOX_SUCCESS(rc))
    {
        RTPrintf("Creating fixed image with size %u Bytes...\n", (unsigned)cbFile);
        rc = VDICreateBaseImage(pszUtf8Filename,
                                VDI_IMAGE_TYPE_FIXED,
                                cbFile,
                                "Converted from DD test image", NULL, NULL);
        PrintDone(rc);
        if (VBOX_SUCCESS(rc))
        {
            RTPrintf("Writing data...\n");
            PVDIDISK pVdi = VDIDiskCreate();
            rc = VDIDiskOpenImage(pVdi, pszUtf8Filename, VDI_OPEN_FLAGS_NORMAL);
            if (VBOX_SUCCESS(rc))
            {
                /* alloc work buffer. */
                void *pvBuf = RTMemAlloc(VDIDiskGetBufferSize(pVdi));
                if (pvBuf)
                {
                    uint64_t off = 0;
                    while (off < cbFile)
                    {
                        size_t cbRead = 0;
                        rc = RTFileRead(File, pvBuf, VDIDiskGetBufferSize(pVdi), &cbRead);
                        if (VBOX_FAILURE(rc) || !cbRead)
                            break;
                        rc = VDIDiskWrite(pVdi, off, pvBuf, cbRead);
                        if (VBOX_FAILURE(rc))
                            break;
                        off += cbRead;
                    }

                    RTMemFree(pvBuf);
                }
                else
                    rc = VERR_NO_MEMORY;

                VDIDiskCloseImage(pVdi);
            }

            if (VBOX_FAILURE(rc))
            {
                /* delete image on error */
                VDIDeleteImage(pszUtf8Filename);
            }
            PrintDone(rc);
        }
    }
    RTFileClose(File);

    return rc;
}

static DECLCALLBACK(int) ProcessCallback(PVM pVM, unsigned uPercent, void *pvUser)
{
    unsigned *pPercent = (unsigned *)pvUser;

    if (*pPercent != uPercent)
    {
        *pPercent = uPercent;
        RTPrintf(".");
        if ((uPercent % 10) == 0 && uPercent)
            RTPrintf("%d%%", uPercent);
        RTStrmFlush(g_pStdOut);
    }

    return VINF_SUCCESS;
}

static int ConvertOldImage(const char *pszFilename)
{
    RTPrintf("Converting VDI image file=\"%s\" to a new format...\n"
             "progress: 0%%",
             pszFilename);

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;

    unsigned uPercent = 0;
    rc = VDIConvertImage(pszUtf8Filename, ProcessCallback, &uPercent);
    RTPrintf("\n");
    return PrintDone(rc);
}

static int DumpImage(const char *pszFilename)
{
    RTPrintf("Dumping VDI image file=\"%s\" into the log file...\n", pszFilename);
    PVDIDISK pVdi = VDIDiskCreate();

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = VDIDiskOpenImage(pVdi, pszUtf8Filename, VDI_OPEN_FLAGS_READONLY);
    if (VBOX_SUCCESS(rc))
    {
        VDIDiskDumpImages(pVdi);
        VDIDiskCloseAllImages(pVdi);
    }
    return PrintDone(rc);
}

static int ResetImageGeometry(const char *pszFilename)
{
    RTPrintf("Resetting geometry info of VDI image file=\"%s\"\n", pszFilename);
    PVDIDISK pVdi = VDIDiskCreate();

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = VDIDiskOpenImage(pVdi, pszUtf8Filename, VDI_OPEN_FLAGS_NORMAL);
    if (VBOX_SUCCESS(rc))
    {
        rc = VDIDiskSetGeometry(pVdi, 0, 0, 0);
        if (VBOX_SUCCESS(rc))
            rc = VDIDiskSetTranslation(pVdi, PDMBIOSTRANSLATION_AUTO);
    }
    VDIDiskCloseImage(pVdi);
    return PrintDone(rc);
}

static int CopyImage(const char *pszDstFile, const char *pszSrcFile)
{
    RTPrintf("Copying VDI image file=\"%s\" to image file=\"%s\"...\n"
             "progress: 0%%",
             pszSrcFile, pszDstFile);

    /* translate argv[] to UTF8 */
    char *pszUtf8SrcFile, *pszUtf8DstFile;
    int rc = FilenameToUtf8(&pszUtf8SrcFile, pszSrcFile);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = FilenameToUtf8(&pszUtf8DstFile, pszDstFile);
    if (VBOX_FAILURE(rc))
        return rc;

    unsigned uPrecent = 0;
    rc = VDICopyImage(pszUtf8DstFile, pszUtf8SrcFile, NULL, ProcessCallback, &uPrecent);
    RTPrintf("\n");
    return PrintDone(rc);
}

static int CopyToDD(const char *pszDstFile, const char *pszSrcFile)
{
    RTPrintf("Copying VDI image file=\"%s\" to DD file=\"%s\"...\n",
             pszSrcFile, pszDstFile);
    PVDIDISK pVdi = VDIDiskCreate();

    /* translate argv[] to UTF8 */
    char *pszUtf8SrcFile, *pszUtf8DstFile;
    int rc = FilenameToUtf8(&pszUtf8SrcFile, pszSrcFile);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = FilenameToUtf8(&pszUtf8DstFile, pszDstFile);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = VDIDiskOpenImage(pVdi, pszUtf8SrcFile, VDI_OPEN_FLAGS_NORMAL);
    if (VBOX_SUCCESS(rc))
    {
        RTFILE FileDst;
        rc = RTFileOpen(&FileDst, pszUtf8DstFile, RTFILE_O_CREATE | RTFILE_O_READWRITE | RTFILE_O_DENY_WRITE);
        if (VBOX_SUCCESS(rc))
        {
            uint64_t        cbSrc = VDIDiskGetSize(pVdi);
            const unsigned  cbBuf = VDIDiskGetBlockSize(pVdi); /* or perhaps VDIDiskGetBufferSize(pVdi)? */
            void *pvBuf = RTMemAlloc(cbBuf);
            if (pvBuf)
            {
                uint64_t off = 0;
                while (off < cbSrc)
                {
                    rc = VDIDiskRead(pVdi, off, pvBuf, cbBuf);
                    if (VBOX_FAILURE(rc))
                        break;
                    rc = RTFileWrite(FileDst, pvBuf, cbBuf, NULL);
                    if (VBOX_FAILURE(rc))
                        break;
                    off += cbBuf;
                }
                RTMemFree(pvBuf);
            }
            RTFileClose(FileDst);
        }
    }
    VDIDiskCloseImage(pVdi);
    return PrintDone(rc);
}

static int ShrinkImage(const char *pszFilename)
{
    RTPrintf("Shrinking VDI image file=\"%s\"...\n"
             "progress: 0%%",
             pszFilename);

    /* translate argv[] to UTF8 */
    char *pszUtf8Filename;
    int rc = FilenameToUtf8(&pszUtf8Filename, pszFilename);
    if (VBOX_FAILURE(rc))
        return rc;

    unsigned uPrecent;
    rc = VDIShrinkImage(pszUtf8Filename, ProcessCallback, &uPrecent);
    RTPrintf("\n");
    return PrintDone(rc);
}

int main(int argc, char **argv)
{
    putenv((char*)"VBOX_LOG_DEST=stdout");
    putenv((char*)"VBOX_LOG_FLAGS=");

    RTR3Init();
    RTPrintf("vditool    Copyright (c) 2004-2007 innotek GmbH.\n\n");

    /*
     * Do cmd line parsing.
     */
    if (argc < 2)
        return UsageExit();

    char szCmd[16];
    if (strlen(argv[1]) >= sizeof(szCmd))
        return SyntaxError("Invalid command!");
    strcpy(szCmd, argv[1]);
    ascii2upper(szCmd);

    PRTLOGGER pLogger;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    int rc = RTLogCreate(&pLogger, 0, "all",
                         NULL, ELEMENTS(s_apszGroups), s_apszGroups,
                          RTLOGDEST_STDOUT, NULL);
    RTLogRelSetDefaultInstance(pLogger);

    if (strcmp(szCmd, "NEW") == 0)
    {
        if (argc != 4)
            return SyntaxError("Invalid argument count!");

        uint32_t cMBs;
        rc = RTStrToUInt32Ex(argv[3], NULL, 10, &cMBs);
        if (VBOX_FAILURE(rc))
            return SyntaxError("Invalid number!");
        if (    cMBs < 2
            ||  cMBs > 1024*1024)
        {
            RTPrintf("error: Disk size %RU32 (MB) is not within the range %u-%u!\n",
                     cMBs, 2, 1024*1024);
            return 1;
        }

        rc = NewImage(argv[2], cMBs);
    }
    else if (strcmp(szCmd, "DD") == 0)
    {
        if (argc != 4)
            return SyntaxError("Invalid argument count!");
        rc = ConvertDDImage(argv[2], argv[3]);
    }
    else if (strcmp(szCmd, "CONVERT") == 0)
    {
        if (argc != 3)
            return SyntaxError("Invalid argument count!");
        rc = ConvertOldImage(argv[2]);
    }
    else if (strcmp(szCmd, "DUMP") == 0)
    {
        if (argc != 3)
            return SyntaxError("Invalid argument count!");
        rc = DumpImage(argv[2]);
    }
    else if (strcmp(szCmd, "RESETGEO") == 0)
    {
        if (argc != 3)
            return SyntaxError("Invalid argument count!");
        rc = ResetImageGeometry(argv[2]);
    }
    else if (strcmp(szCmd, "COPY") == 0)
    {
        if (argc != 4)
            return SyntaxError("Invalid argument count!");
        rc = CopyImage(argv[3], argv[2]);
    }
    else if (strcmp(szCmd, "COPYDD") == 0)
    {
        if (argc != 4)
            return SyntaxError("Invalid argument count!");
        rc = CopyToDD(argv[3], argv[2]);
    }
    else if (strcmp(szCmd, "SHRINK") == 0)
    {
        if (argc != 3)
            return SyntaxError("Invalid argument count!");
        rc = ShrinkImage(argv[2]);
    }
    else
        return SyntaxError("Invalid command!");

    RTLogFlush(NULL);
    return !VBOX_SUCCESS(rc);
}
