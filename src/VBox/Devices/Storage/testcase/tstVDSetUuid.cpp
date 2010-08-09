/* $Id$ */
/** @file
 * Simple VBox HDD container test utility for changing the uuid of images.
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
#include <iprt/initterm.h>
#include <iprt/buildconfig.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/stream.h>
#include <iprt/message.h>
#include <iprt/getopt.h>

const char *g_pszProgName = "";

static const RTGETOPTDEF s_aOptions[] =
{
    { "--filename", 'f', RTGETOPT_REQ_STRING },
    { "--format", 'o', RTGETOPT_REQ_STRING },
    { "--uuid", 'u', RTGETOPT_REQ_UUID },
    { "--parentuuid", 'p', RTGETOPT_REQ_UUID },
    { "--zeroparentuuid", 'P', RTGETOPT_REQ_NOTHING }
};

static void showUsage(void)
{
    RTStrmPrintf(g_pStdErr,
                 "Usage: %s\n"
                 "                --filename <filename>\n"
                 "                [--format VDI|VMDK|VHD|...]\n"
                 "                [--uuid <uuid>]\n"
                 "                [--parentuuid <uuid>]\n"
                 "                [--zeroparentuuid]\n",
                 g_pszProgName);
}

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
    RTMsgWarningV(pszFormat, args);
    va_end(args);
    return VINF_SUCCESS;
}

int main(int argc, char *argv[])
{
    RTR3Init();
    int rc;
    const char *pszFilename = NULL;
    char *pszFormat = NULL;
    RTUUID imageUuid;
    RTUUID parentUuid;
    bool fSetImageUuid = false;
    bool fSetParentUuid = false;

    g_pszProgName = RTPathFilename(argv[0]);

    RTUuidClear(&imageUuid);
    RTUuidClear(&parentUuid);

    /* Parse the command line. */
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
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
            case 'h':   // --help
                showUsage();
                return 0;
            case 'V':   // --version
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                showUsage();
                return ch;
        }
    }

    /* Check for mandatory parameters. */
    if (!pszFilename)
    {
        RTMsgError("Mandatory --filename option missing\n");
        showUsage();
        return 1;
    }

    /* Check for consistency of optional parameters. */
    if (fSetImageUuid && RTUuidIsNull(&imageUuid))
    {
        RTMsgError("Invalid parameter to --uuid option\n");
        showUsage();
        return 1;
    }

    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;
    vdInterfaceErrorCallbacks.pfnMessage   = handleVDMessage;

    rc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                        &vdInterfaceErrorCallbacks, NULL, &pVDIfs);

    /* Autodetect image format. */
    if (!pszFormat)
    {
        /* Don't pass error interface, as that would triggers error messages
         * because some backends fail to open the image. */
        rc = VDGetFormat(NULL, pszFilename, &pszFormat);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Format autodetect failed: %Rrc\n", rc);
            return 1;
        }
    }

    PVBOXHDD pVD = NULL;
    rc = VDCreate(pVDIfs, &pVD);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot create the virtual disk container: %Rrc\n", rc);
        return 1;
    }

    rc = VDOpen(pVD, pszFormat, pszFilename, VD_OPEN_FLAGS_NORMAL, NULL);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot open the virtual disk image \"%s\": %Rrc\n",
                   pszFilename, rc);
        return 1;
    }

    RTUUID oldImageUuid;
    rc = VDGetUuid(pVD, VD_LAST_IMAGE, &oldImageUuid);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot get UUID of virtual disk image \"%s\": %Rrc\n",
                   pszFilename, rc);
        return 1;
    }
    RTPrintf("Old image UUID:  %RTuuid\n", &oldImageUuid);

    RTUUID oldParentUuid;
    rc = VDGetParentUuid(pVD, VD_LAST_IMAGE, &oldParentUuid);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot get parent UUID of virtual disk image \"%s\": %Rrc\n",
                   pszFilename, rc);
        return 1;
    }
    RTPrintf("Old parent UUID: %RTuuid\n", &oldParentUuid);

    if (fSetImageUuid)
    {
        RTPrintf("New image UUID:  %RTuuid\n", &imageUuid);
        rc = VDSetUuid(pVD, VD_LAST_IMAGE, &imageUuid);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Cannot set UUID of virtual disk image \"%s\": %Rrc\n",
                       pszFilename, rc);
            return 1;
        }
    }

    if (fSetParentUuid)
    {
        RTPrintf("New parent UUID: %RTuuid\n", &parentUuid);
        rc = VDSetParentUuid(pVD, VD_LAST_IMAGE, &parentUuid);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Cannot set parent UUID of virtual disk image \"%s\": %Rrc\n",
                       pszFilename, rc);
            return 1;
        }
    }

    rc = VDCloseAll(pVD);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Closing image failed! rc=%Rrc\n", rc);
        return 1;
    }

    if (pszFormat)
    {
        RTStrFree(pszFormat);
        pszFormat = NULL;
    }

    rc = VDShutdown();
    if (RT_FAILURE(rc))
    {
        RTMsgError("Unloading backends failed! rc=%Rrc\n", rc);
        return 1;
    }

    return 0;
}

