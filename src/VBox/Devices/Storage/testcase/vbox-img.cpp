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
static void showUsage(void)
{
    RTStrmPrintf(g_pStdErr,
                 "Usage: %s\n"
                 "   setuuid      --filename <filename>\n"
                 "                [--format VDI|VMDK|VHD|...]\n"
                 "                [--uuid <uuid>]\n"
                 "                [--parentuuid <uuid>]\n"
                 "                [--zeroparentuuid]\n"
                 "\n"
                 "   convert      --srcfilename <filename>\n"
                 "                --dstfilename <filename>\n"
                 "                [--srcformat VDI|VMDK|VHD|RAW|..]\n"
                 "                [--dstformat VDI|VMDK|VHD|RAW|..]\n"
                 "                [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "\n"
                 "   info         --filename <filename>\n"
                 "\n"
                 "   compact      --filename <filename>\n",
                 g_pszProgName);
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

static int handleVDMessage(void *pvUser, const char *pszFormat, ...)
{
    NOREF(pvUser);
    va_list args;
    va_start(args, pszFormat);
    RTPrintfV(pszFormat, args);
    va_end(args);
    return VINF_SUCCESS;
}

/**
 * Print a usage synopsis and the syntax error message.
 */
int errorSyntax(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTPrintf("\n"
             "Syntax error: %N\n", pszFormat, &args);
    va_end(args);
    showUsage();
    return 1;
}
int errorRuntime(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    RTPrintf("\n"
             "Error: %N\n", pszFormat, &args);
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
                showUsage();
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
        rc = VDGetFormat(NULL, pszFilename, &pszFormat);
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


int handleConvert(HandlerArg *a)
{
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    char *pszSrcFormat = NULL;
    char *pszDstFormat = NULL;
    const char *pszVariant = NULL;
    PVBOXHDD pSrcDisk = NULL;
    PVBOXHDD pDstDisk = NULL;
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    int rc = VINF_SUCCESS;

    /* Parse the command line. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--srcfilename", 'i', RTGETOPT_REQ_STRING },
        { "--dstfilename", 'o', RTGETOPT_REQ_STRING },
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
                showUsage();
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszSrcFilename)
        return errorSyntax("Mandatory --srcfilename option missing\n");
    if (!pszDstFilename)
        return errorSyntax("Mandatory --dstfilename option missing\n");

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
            rc = VDGetFormat(NULL, pszSrcFilename, &pszSrcFormat);
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

        rc = VDOpen(pSrcDisk, pszSrcFormat, pszSrcFilename, VD_OPEN_FLAGS_READONLY, NULL);
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
        RTPrintf("Converting image \"%s\" with size %RU64 bytes (%RU64MB)...\n", pszSrcFilename, cbSize, (cbSize + _1M - 1) / _1M);

        /* Create the output image */
        rc = VDCopy(pSrcDisk, VD_LAST_IMAGE, pDstDisk, pszDstFormat,
                    pszDstFilename, false, 0, uImageFlags, NULL, NULL, NULL, NULL);
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
                showUsage();
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* just try it */
    char *pszFormat = NULL;
    rc = VDGetFormat(NULL, pszFilename, &pszFormat);
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
                showUsage();
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
        return errorSyntax("Mandatory --filename option missing\n");

    /* just try it */
    char *pszFormat = NULL;
    rc = VDGetFormat(NULL, pszFilename, &pszFormat);
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


void showLogo()
{
    RTPrintf(VBOX_PRODUCT" Disk Utility "
             VBOX_VERSION_STRING  "\n"
             "(C) 2005-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
             "All rights reserved.\n"
             "\n");
}


int main(int argc, char *argv[])
{
    RTR3Init();
    int rc;

    g_pszProgName = RTPathFilename(argv[0]);

    bool fShowLogo = true;
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
            showLogo();
            showUsage();
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
        showLogo();

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
