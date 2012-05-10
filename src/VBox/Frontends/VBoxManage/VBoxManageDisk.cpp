/* $Id$ */
/** @file
 * VBoxManage - The disk related commands.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/getopt.h>
#include <VBox/log.h>
#include <VBox/vd.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static DECLCALLBACK(void) handleVDError(void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    RTMsgError(pszFormat, va);
    RTMsgError("Error code %Rrc at %s(%u) in function %s", rc, RT_SRC_POS_ARGS);
}


static int parseDiskVariant(const char *psz, MediumVariant_T *pDiskVariant)
{
    int rc = VINF_SUCCESS;
    unsigned DiskVariant = (unsigned)(*pDiskVariant);
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
            // Parsing is intentionally inconsistent: "standard" resets the
            // variant, whereas the other flags are cumulative.
            if (!RTStrNICmp(psz, "standard", len))
                DiskVariant = MediumVariant_Standard;
            else if (   !RTStrNICmp(psz, "fixed", len)
                     || !RTStrNICmp(psz, "static", len))
                DiskVariant |= MediumVariant_Fixed;
            else if (!RTStrNICmp(psz, "Diff", len))
                DiskVariant |= MediumVariant_Diff;
            else if (!RTStrNICmp(psz, "split2g", len))
                DiskVariant |= MediumVariant_VmdkSplit2G;
            else if (   !RTStrNICmp(psz, "stream", len)
                     || !RTStrNICmp(psz, "streamoptimized", len))
                DiskVariant |= MediumVariant_VmdkStreamOptimized;
            else if (!RTStrNICmp(psz, "esx", len))
                DiskVariant |= MediumVariant_VmdkESX;
            else
                rc = VERR_PARSE_ERROR;
        }
        if (pszComma)
            psz += len + 1;
        else
            psz += len;
    }

    if (RT_SUCCESS(rc))
        *pDiskVariant = (MediumVariant_T)DiskVariant;
    return rc;
}

int parseDiskType(const char *psz, MediumType_T *pDiskType)
{
    int rc = VINF_SUCCESS;
    MediumType_T DiskType = MediumType_Normal;
    if (!RTStrICmp(psz, "normal"))
        DiskType = MediumType_Normal;
    else if (!RTStrICmp(psz, "immutable"))
        DiskType = MediumType_Immutable;
    else if (!RTStrICmp(psz, "writethrough"))
        DiskType = MediumType_Writethrough;
    else if (!RTStrICmp(psz, "shareable"))
        DiskType = MediumType_Shareable;
    else if (!RTStrICmp(psz, "readonly"))
        DiskType = MediumType_Readonly;
    else if (!RTStrICmp(psz, "multiattach"))
        DiskType = MediumType_MultiAttach;
    else
        rc = VERR_PARSE_ERROR;

    if (RT_SUCCESS(rc))
        *pDiskType = DiskType;
    return rc;
}

/** @todo move this into getopt, as getting bool values is generic */
static int parseBool(const char *psz, bool *pb)
{
    int rc = VINF_SUCCESS;
    if (    !RTStrICmp(psz, "on")
        ||  !RTStrICmp(psz, "yes")
        ||  !RTStrICmp(psz, "true")
        ||  !RTStrICmp(psz, "1")
        ||  !RTStrICmp(psz, "enable")
        ||  !RTStrICmp(psz, "enabled"))
    {
        *pb = true;
    }
    else if (   !RTStrICmp(psz, "off")
             || !RTStrICmp(psz, "no")
             || !RTStrICmp(psz, "false")
             || !RTStrICmp(psz, "0")
             || !RTStrICmp(psz, "disable")
             || !RTStrICmp(psz, "disabled"))
    {
        *pb = false;
    }
    else
        rc = VERR_PARSE_ERROR;

    return rc;
}

HRESULT findMedium(HandlerArg *a, const char *pszFilenameOrUuid,
                   DeviceType_T enmDevType, bool fSilent,
                   ComPtr<IMedium> &pMedium)
{
    HRESULT rc;
    Guid id(pszFilenameOrUuid);
    char szFilenameAbs[RTPATH_MAX] = "";

    /* If it is no UUID, convert the filename to an absolute one. */
    if (id.isEmpty())
    {
        int irc = RTPathAbs(pszFilenameOrUuid, szFilenameAbs, sizeof(szFilenameAbs));
        if (RT_FAILURE(irc))
        {
            if (!fSilent)
                RTMsgError("Cannot convert filename \"%s\" to absolute path", pszFilenameOrUuid);
            return E_FAIL;
        }
        pszFilenameOrUuid = szFilenameAbs;
    }

    if (!fSilent)
        CHECK_ERROR(a->virtualBox, OpenMedium(Bstr(pszFilenameOrUuid).raw(), 
                                   enmDevType, 
                                   AccessMode_ReadWrite, 
                                   /*fForceNewUidOnOpen */ false, 
                                   pMedium.asOutParam()));
    else
        rc = a->virtualBox->OpenMedium(Bstr(pszFilenameOrUuid).raw(), 
                                       enmDevType, 
                                       AccessMode_ReadWrite,
                                       /*fForceNewUidOnOpen */ false,
                                       pMedium.asOutParam());
    return rc;
}

HRESULT findOrOpenMedium(HandlerArg *a, const char *pszFilenameOrUuid,
                         DeviceType_T enmDevType, ComPtr<IMedium> &pMedium,
                         bool fForceNewUuidOnOpen, bool *pfWasUnknown)
{
    HRESULT rc;
    bool fWasUnknown = false;
    Guid id(pszFilenameOrUuid);
    char szFilenameAbs[RTPATH_MAX] = "";

    /* If it is no UUID, convert the filename to an absolute one. */
    if (id.isEmpty())
    {
        int irc = RTPathAbs(pszFilenameOrUuid, szFilenameAbs, sizeof(szFilenameAbs));
        if (RT_FAILURE(irc))
        {
            RTMsgError("Cannot convert filename \"%s\" to absolute path", pszFilenameOrUuid);
            return E_FAIL;
        }
        pszFilenameOrUuid = szFilenameAbs;
    }

    rc = a->virtualBox->OpenMedium(Bstr(pszFilenameOrUuid).raw(), 
                                   enmDevType,
		                   AccessMode_ReadWrite,
                                   /*fForceNewUidOnOpen */ false,
                                   pMedium.asOutParam());
    /* If the medium is unknown try to open it. */
    if (!pMedium)
    {
        CHECK_ERROR(a->virtualBox, OpenMedium(Bstr(pszFilenameOrUuid).raw(),
                                              enmDevType, AccessMode_ReadWrite,
                                              fForceNewUuidOnOpen,
                                              pMedium.asOutParam()));
        if (SUCCEEDED(rc))
            fWasUnknown = true;
    }
    if (RT_VALID_PTR(pfWasUnknown))
        *pfWasUnknown = fWasUnknown;
    return rc;
}

static HRESULT createHardDisk(HandlerArg *a, const char *pszFormat,
                              const char *pszFilename, ComPtr<IMedium> &pMedium)
{
    HRESULT rc;
    char szFilenameAbs[RTPATH_MAX] = "";

    /** @todo laziness shortcut. should really check the MediumFormatCapabilities */
    if (RTStrICmp(pszFormat, "iSCSI"))
    {
        int irc = RTPathAbs(pszFilename, szFilenameAbs, sizeof(szFilenameAbs));
        if (RT_FAILURE(irc))
        {
            RTMsgError("Cannot convert filename \"%s\" to absolute path", pszFilename);
            return E_FAIL;
        }
        pszFilename = szFilenameAbs;
    }

    CHECK_ERROR(a->virtualBox, CreateHardDisk(Bstr(pszFormat).raw(),
                                              Bstr(pszFilename).raw(),
                                              pMedium.asOutParam()));
    return rc;
}

static const RTGETOPTDEF g_aCreateHardDiskOptions[] =
{
    { "--filename",     'f', RTGETOPT_REQ_STRING },
    { "-filename",      'f', RTGETOPT_REQ_STRING },     // deprecated
    { "--diffparent",   'd', RTGETOPT_REQ_STRING },
    { "--size",         's', RTGETOPT_REQ_UINT64 },
    { "-size",          's', RTGETOPT_REQ_UINT64 },     // deprecated
    { "--sizebyte",     'S', RTGETOPT_REQ_UINT64 },
    { "--format",       'o', RTGETOPT_REQ_STRING },
    { "-format",        'o', RTGETOPT_REQ_STRING },     // deprecated
    { "--static",       'F', RTGETOPT_REQ_NOTHING },
    { "-static",        'F', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--variant",      'm', RTGETOPT_REQ_STRING },
    { "-variant",       'm', RTGETOPT_REQ_STRING },     // deprecated
};

int handleCreateHardDisk(HandlerArg *a)
{
    HRESULT rc;
    int vrc;
    const char *filename = NULL;
    const char *diffparent = NULL;
    uint64_t size = 0;
    const char *format = NULL;
    bool fBase = true;
    MediumVariant_T DiskVariant = MediumVariant_Standard;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aCreateHardDiskOptions, RT_ELEMENTS(g_aCreateHardDiskOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'f':   // --filename
                filename = ValueUnion.psz;
                break;

            case 'd':   // --diffparent
                diffparent = ValueUnion.psz;
                fBase = false;
                break;

            case 's':   // --size
                size = ValueUnion.u64 * _1M;
                break;

            case 'S':   // --sizebyte
                size = ValueUnion.u64;
                break;

            case 'o':   // --format
                format = ValueUnion.psz;
                break;

            case 'F':   // --static ("fixed"/"flat")
            {
                unsigned uDiskVariant = (unsigned)DiskVariant;
                uDiskVariant |= MediumVariant_Fixed;
                DiskVariant = (MediumVariant_T)uDiskVariant;
                break;
            }

            case 'm':   // --variant
                vrc = parseDiskVariant(ValueUnion.psz, &DiskVariant);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid hard disk variant '%s'", ValueUnion.psz);
                break;

            case VINF_GETOPT_NOT_OPTION:
                return errorSyntax(USAGE_CREATEHD, "Invalid parameter '%s'", ValueUnion.psz);

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_CREATEHD, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_CREATEHD, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_CREATEHD, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_CREATEHD, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_CREATEHD, "error: %Rrs", c);
        }
    }

    /* check the outcome */
    bool fUnknownParent = false;
    ComPtr<IMedium> parentHardDisk;
    if (fBase)
    {
        if (   !filename
            || !*filename
            || size == 0)
            return errorSyntax(USAGE_CREATEHD, "Parameters --filename and --size are required");
        if (!format || !*format)
            format = "VDI";
    }
    else
    {
        if (   !filename
            || !*filename)
            return errorSyntax(USAGE_CREATEHD, "Parameters --filename is required");
        size = 0;
        DiskVariant = MediumVariant_Diff;
        if (!format || !*format)
        {
            const char *pszExt = RTPathExt(filename);
            /* Skip over . if there is an extension. */
            if (pszExt)
                pszExt++;
            if (!pszExt || !*pszExt)
                format = "VDI";
            else
                format = pszExt;
        }
        rc = findOrOpenMedium(a, diffparent, DeviceType_HardDisk,
                              parentHardDisk, false /* fForceNewUuidOnOpen */,
                              &fUnknownParent);
        if (FAILED(rc))
            return 1;
        if (parentHardDisk.isNull())
        {
            RTMsgError("Invalid parent hard disk reference, avoiding crash");
            return 1;
        }
        MediumState_T state;
        CHECK_ERROR(parentHardDisk, COMGETTER(State)(&state));
        if (FAILED(rc))
            return 1;
        if (state == MediumState_Inaccessible)
        {
            CHECK_ERROR(parentHardDisk, RefreshState(&state));
            if (FAILED(rc))
                return 1;
        }
    }
    /* check for filename extension */
    /** @todo use IMediumFormat to cover all extensions generically */
    Utf8Str strName(filename);
    if (!RTPathHaveExt(strName.c_str()))
    {
        Utf8Str strFormat(format);
        if (strFormat.compare("vmdk", RTCString::CaseInsensitive) == 0)
            strName.append(".vmdk");
        else if (strFormat.compare("vhd", RTCString::CaseInsensitive) == 0)
            strName.append(".vhd");
        else
            strName.append(".vdi");
        filename = strName.c_str();
    }

    ComPtr<IMedium> hardDisk;
    rc = createHardDisk(a, format, filename, hardDisk);
    if (SUCCEEDED(rc) && hardDisk)
    {
        ComPtr<IProgress> progress;
        if (fBase)
            CHECK_ERROR(hardDisk, CreateBaseStorage(size, DiskVariant, progress.asOutParam()));
        else
            CHECK_ERROR(parentHardDisk, CreateDiffStorage(hardDisk, DiskVariant, progress.asOutParam()));
        if (SUCCEEDED(rc) && progress)
        {
            rc = showProgress(progress);
            CHECK_PROGRESS_ERROR(progress, ("Failed to create hard disk"));
            if (SUCCEEDED(rc))
            {
                Bstr uuid;
                CHECK_ERROR(hardDisk, COMGETTER(Id)(uuid.asOutParam()));
                RTPrintf("Disk image created. UUID: %s\n", Utf8Str(uuid).c_str());
            }
        }

        CHECK_ERROR(hardDisk, Close());
        if (!fBase && fUnknownParent)
            CHECK_ERROR(parentHardDisk, Close());
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aModifyHardDiskOptions[] =
{
    { "--type",         't', RTGETOPT_REQ_STRING },
    { "-type",          't', RTGETOPT_REQ_STRING },     // deprecated
    { "settype",        't', RTGETOPT_REQ_STRING },     // deprecated
    { "--autoreset",    'z', RTGETOPT_REQ_STRING },
    { "-autoreset",     'z', RTGETOPT_REQ_STRING },     // deprecated
    { "autoreset",      'z', RTGETOPT_REQ_STRING },     // deprecated
    { "--compact",      'c', RTGETOPT_REQ_NOTHING },
    { "-compact",       'c', RTGETOPT_REQ_NOTHING },    // deprecated
    { "compact",        'c', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--resize",       'r', RTGETOPT_REQ_UINT64 },
    { "--resizebyte",   'R', RTGETOPT_REQ_UINT64 }
};

int handleModifyHardDisk(HandlerArg *a)
{
    HRESULT rc;
    int vrc;
    ComPtr<IMedium> hardDisk;
    MediumType_T DiskType;
    bool AutoReset = false;
    bool fModifyDiskType = false, fModifyAutoReset = false, fModifyCompact = false;
    bool fModifyResize = false;
    uint64_t cbResize = 0;
    const char *FilenameOrUuid = NULL;
    bool unknown = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aModifyHardDiskOptions, RT_ELEMENTS(g_aModifyHardDiskOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 't':   // --type
                vrc = parseDiskType(ValueUnion.psz, &DiskType);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid hard disk type '%s'", ValueUnion.psz);
                fModifyDiskType = true;
                break;

            case 'z':   // --autoreset
                vrc = parseBool(ValueUnion.psz, &AutoReset);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid autoreset parameter '%s'", ValueUnion.psz);
                fModifyAutoReset = true;
                break;

            case 'c':   // --compact
                fModifyCompact = true;
                break;

            case 'r':   // --resize
                cbResize = ValueUnion.u64 * _1M;
                fModifyResize = true;
                break;

            case 'R':   // --resizebyte
                cbResize = ValueUnion.u64;
                fModifyResize = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!FilenameOrUuid)
                    FilenameOrUuid = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_CREATEHD, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_MODIFYHD, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_MODIFYHD, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_MODIFYHD, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_MODIFYHD, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_MODIFYHD, "error: %Rrs", c);
        }
    }

    if (!FilenameOrUuid)
        return errorSyntax(USAGE_MODIFYHD, "Disk name or UUID required");

    if (!fModifyDiskType && !fModifyAutoReset && !fModifyCompact && !fModifyResize)
        return errorSyntax(USAGE_MODIFYHD, "No operation specified");

    /* Depending on the operation the medium must be in the registry or
     * may be opened on demand. */
    if (fModifyDiskType || fModifyAutoReset)
        rc = findMedium(a, FilenameOrUuid, DeviceType_HardDisk, false /* fSilent */, hardDisk);
    else
        rc = findOrOpenMedium(a, FilenameOrUuid, DeviceType_HardDisk,
                              hardDisk, false /* fForceNewUuidOnOpen */, &unknown);
    if (FAILED(rc))
        return 1;
    if (hardDisk.isNull())
    {
        RTMsgError("Invalid hard disk reference, avoiding crash");
        return 1;
    }

    if (fModifyDiskType)
    {
        MediumType_T hddType;
        CHECK_ERROR(hardDisk, COMGETTER(Type)(&hddType));

        if (hddType != DiskType)
            CHECK_ERROR(hardDisk, COMSETTER(Type)(DiskType));
    }

    if (fModifyAutoReset)
    {
        CHECK_ERROR(hardDisk, COMSETTER(AutoReset)(AutoReset));
    }

    if (fModifyCompact)
    {
        ComPtr<IProgress> progress;
        CHECK_ERROR(hardDisk, Compact(progress.asOutParam()));
        if (SUCCEEDED(rc))
            rc = showProgress(progress);
        if (FAILED(rc))
        {
            if (rc == E_NOTIMPL)
                RTMsgError("Compact hard disk operation is not implemented!");
            else if (rc == VBOX_E_NOT_SUPPORTED)
                RTMsgError("Compact hard disk operation for this format is not implemented yet!");
            else if (!progress.isNull())
                CHECK_PROGRESS_ERROR(progress, ("Failed to compact hard disk"));
            else
                RTMsgError("Failed to compact hard disk!");
        }
    }

    if (fModifyResize)
    {
        ComPtr<IProgress> progress;
        CHECK_ERROR(hardDisk, Resize(cbResize, progress.asOutParam()));
        if (SUCCEEDED(rc))
            rc = showProgress(progress);
        if (FAILED(rc))
        {
            if (rc == E_NOTIMPL)
                RTMsgError("Resize hard disk operation is not implemented!");
            else if (rc == VBOX_E_NOT_SUPPORTED)
                RTMsgError("Resize hard disk operation for this format is not implemented yet!");
            else
                CHECK_PROGRESS_ERROR(progress, ("Failed to resize hard disk"));
        }
    }

    if (unknown)
        hardDisk->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aCloneHardDiskOptions[] =
{
    { "--format",       'o', RTGETOPT_REQ_STRING },
    { "-format",        'o', RTGETOPT_REQ_STRING },
    { "--static",       'F', RTGETOPT_REQ_NOTHING },
    { "-static",        'F', RTGETOPT_REQ_NOTHING },
    { "--existing",     'E', RTGETOPT_REQ_NOTHING },
    { "--variant",      'm', RTGETOPT_REQ_STRING },
    { "-variant",       'm', RTGETOPT_REQ_STRING },
};

int handleCloneHardDisk(HandlerArg *a)
{
    HRESULT rc;
    int vrc;
    const char *pszSrc = NULL;
    const char *pszDst = NULL;
    Bstr format;
    MediumVariant_T DiskVariant = MediumVariant_Standard;
    bool fExisting = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aCloneHardDiskOptions, RT_ELEMENTS(g_aCloneHardDiskOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'o':   // --format
                format = ValueUnion.psz;
                break;

            case 'F':   // --static
            {
                unsigned uDiskVariant = (unsigned)DiskVariant;
                uDiskVariant |= MediumVariant_Fixed;
                DiskVariant = (MediumVariant_T)uDiskVariant;
                break;
            }

            case 'E':   // --existing
                fExisting = true;
                break;

            case 'm':   // --variant
                vrc = parseDiskVariant(ValueUnion.psz, &DiskVariant);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid hard disk variant '%s'", ValueUnion.psz);
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!pszSrc)
                    pszSrc = ValueUnion.psz;
                else if (!pszDst)
                    pszDst = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_CLONEHD, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_GRAPH(c))
                        return errorSyntax(USAGE_CLONEHD, "unhandled option: -%c", c);
                    else
                        return errorSyntax(USAGE_CLONEHD, "unhandled option: %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_CLONEHD, "unknown option: %s", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_CLONEHD, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_CLONEHD, "error: %Rrs", c);
        }
    }

    if (!pszSrc)
        return errorSyntax(USAGE_CLONEHD, "Mandatory UUID or input file parameter missing");
    if (!pszDst)
        return errorSyntax(USAGE_CLONEHD, "Mandatory output file parameter missing");
    if (fExisting && (!format.isEmpty() || DiskVariant != MediumType_Normal))
        return errorSyntax(USAGE_CLONEHD, "Specified options which cannot be used with --existing");

    ComPtr<IMedium> srcDisk;
    ComPtr<IMedium> dstDisk;
    bool fSrcUnknown = false;
    bool fDstUnknown = false;

    rc = findOrOpenMedium(a, pszSrc, DeviceType_HardDisk, srcDisk,
                          false /* fForceNewUuidOnOpen */, &fSrcUnknown);
    if (FAILED(rc))
        return 1;

    do
    {
        /* open/create destination hard disk */
        if (fExisting)
        {
            rc = findOrOpenMedium(a, pszDst, DeviceType_HardDisk, dstDisk,
                                  false /* fForceNewUuidOnOpen */, &fDstUnknown);
            if (FAILED(rc))
                break;

            /* Perform accessibility check now. */
            MediumState_T state;
            CHECK_ERROR_BREAK(dstDisk, RefreshState(&state));
            CHECK_ERROR_BREAK(dstDisk, COMGETTER(Format)(format.asOutParam()));
        }
        else
        {
            /* use the format of the source hard disk if unspecified */
            if (format.isEmpty())
                CHECK_ERROR_BREAK(srcDisk, COMGETTER(Format)(format.asOutParam()));
            rc = createHardDisk(a, Utf8Str(format).c_str(), pszDst, dstDisk);
            if (FAILED(rc))
                break;
            fDstUnknown = true;
        }

        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK(srcDisk, CloneTo(dstDisk, DiskVariant, NULL, progress.asOutParam()));

        rc = showProgress(progress);
        CHECK_PROGRESS_ERROR_BREAK(progress, ("Failed to clone hard disk"));

        Bstr uuid;
        CHECK_ERROR_BREAK(dstDisk, COMGETTER(Id)(uuid.asOutParam()));

        RTPrintf("Clone hard disk created in format '%ls'. UUID: %s\n",
                 format.raw(), Utf8Str(uuid).c_str());
    }
    while (0);

    if (fDstUnknown && !dstDisk.isNull())
    {
        /* forget the created clone */
        dstDisk->Close();
    }
    if (fSrcUnknown)
    {
        /* close the unknown hard disk to forget it again */
        srcDisk->Close();
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aConvertFromRawHardDiskOptions[] =
{
    { "--format",       'o', RTGETOPT_REQ_STRING },
    { "-format",        'o', RTGETOPT_REQ_STRING },
    { "--static",       'F', RTGETOPT_REQ_NOTHING },
    { "-static",        'F', RTGETOPT_REQ_NOTHING },
    { "--variant",      'm', RTGETOPT_REQ_STRING },
    { "-variant",       'm', RTGETOPT_REQ_STRING },
    { "--uuid",         'u', RTGETOPT_REQ_STRING },
};

RTEXITCODE handleConvertFromRaw(int argc, char *argv[])
{
    int rc = VINF_SUCCESS;
    bool fReadFromStdIn = false;
    const char *format = "VDI";
    const char *srcfilename = NULL;
    const char *dstfilename = NULL;
    const char *filesize = NULL;
    unsigned uImageFlags = VD_IMAGE_FLAGS_NONE;
    void *pvBuf = NULL;
    RTUUID uuid;
    PCRTUUID pUuid = NULL;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, argc, argv, g_aConvertFromRawHardDiskOptions, RT_ELEMENTS(g_aConvertFromRawHardDiskOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'u':   // --uuid
                if (RT_FAILURE(RTUuidFromStr(&uuid, ValueUnion.psz)))
                    return errorSyntax(USAGE_CONVERTFROMRAW, "Invalid UUID '%s'", ValueUnion.psz);
                pUuid = &uuid;
                break;
            case 'o':   // --format
                format = ValueUnion.psz;
                break;

            case 'm':   // --variant
                MediumVariant_T DiskVariant;
                rc = parseDiskVariant(ValueUnion.psz, &DiskVariant);
                if (RT_FAILURE(rc))
                    return errorArgument("Invalid hard disk variant '%s'", ValueUnion.psz);
                /// @todo cleaner solution than assuming 1:1 mapping?
                uImageFlags = (unsigned)DiskVariant;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!srcfilename)
                {
                    srcfilename = ValueUnion.psz;
                    fReadFromStdIn = !strcmp(srcfilename, "stdin");
                }
                else if (!dstfilename)
                    dstfilename = ValueUnion.psz;
                else if (fReadFromStdIn && !filesize)
                    filesize = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_CONVERTFROMRAW, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                return errorGetOpt(USAGE_CONVERTFROMRAW, c, &ValueUnion);
        }
    }

    if (!srcfilename || !dstfilename || (fReadFromStdIn && !filesize))
        return errorSyntax(USAGE_CONVERTFROMRAW, "Incorrect number of parameters");
    RTStrmPrintf(g_pStdErr, "Converting from raw image file=\"%s\" to file=\"%s\"...\n",
                 srcfilename, dstfilename);

    PVBOXHDD pDisk = NULL;

    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACEERROR vdInterfaceError;
    vdInterfaceError.pfnError     = handleVDError;
    vdInterfaceError.pfnMessage   = NULL;

    rc = VDInterfaceAdd(&vdInterfaceError.Core, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                        NULL, sizeof(VDINTERFACEERROR), &pVDIfs);
    AssertRC(rc);

    /* open raw image file. */
    RTFILE File;
    if (fReadFromStdIn)
        rc = RTFileFromNative(&File, RTFILE_NATIVE_STDIN);
    else
        rc = RTFileOpen(&File, srcfilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot open file \"%s\": %Rrc", srcfilename, rc);
        goto out;
    }

    uint64_t cbFile;
    /* get image size. */
    if (fReadFromStdIn)
        cbFile = RTStrToUInt64(filesize);
    else
        rc = RTFileGetSize(File, &cbFile);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot get image size for file \"%s\": %Rrc", srcfilename, rc);
        goto out;
    }

    RTStrmPrintf(g_pStdErr, "Creating %s image with size %RU64 bytes (%RU64MB)...\n",
                 (uImageFlags & VD_IMAGE_FLAGS_FIXED) ? "fixed" : "dynamic", cbFile, (cbFile + _1M - 1) / _1M);
    char pszComment[256];
    RTStrPrintf(pszComment, sizeof(pszComment), "Converted image from %s", srcfilename);
    rc = VDCreate(pVDIfs, VDTYPE_HDD, &pDisk);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot create the virtual disk container: %Rrc", rc);
        goto out;
    }

    Assert(RT_MIN(cbFile / 512 / 16 / 63, 16383) -
           (unsigned int)RT_MIN(cbFile / 512 / 16 / 63, 16383) == 0);
    VDGEOMETRY PCHS, LCHS;
    PCHS.cCylinders = (unsigned int)RT_MIN(cbFile / 512 / 16 / 63, 16383);
    PCHS.cHeads = 16;
    PCHS.cSectors = 63;
    LCHS.cCylinders = 0;
    LCHS.cHeads = 0;
    LCHS.cSectors = 0;
    rc = VDCreateBase(pDisk, format, dstfilename, cbFile,
                      uImageFlags, pszComment, &PCHS, &LCHS, pUuid,
                      VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Cannot create the disk image \"%s\": %Rrc", dstfilename, rc);
        goto out;
    }

    size_t cbBuffer;
    cbBuffer = _1M;
    pvBuf = RTMemAlloc(cbBuffer);
    if (!pvBuf)
    {
        rc = VERR_NO_MEMORY;
        RTMsgError("Out of memory allocating buffers for image \"%s\": %Rrc", dstfilename, rc);
        goto out;
    }

    uint64_t offFile;
    offFile = 0;
    while (offFile < cbFile)
    {
        size_t cbRead;
        size_t cbToRead;
        cbRead = 0;
        cbToRead = cbFile - offFile >= (uint64_t)cbBuffer ?
                            cbBuffer : (size_t)(cbFile - offFile);
        rc = RTFileRead(File, pvBuf, cbToRead, &cbRead);
        if (RT_FAILURE(rc) || !cbRead)
            break;
        rc = VDWrite(pDisk, offFile, pvBuf, cbRead);
        if (RT_FAILURE(rc))
        {
            RTMsgError("Failed to write to disk image \"%s\": %Rrc", dstfilename, rc);
            goto out;
        }
        offFile += cbRead;
    }

out:
    if (pvBuf)
        RTMemFree(pvBuf);
    if (pDisk)
        VDClose(pDisk, RT_FAILURE(rc));
    if (File != NIL_RTFILE)
        RTFileClose(File);

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static const RTGETOPTDEF g_aShowHardDiskInfoOptions[] =
{
    { "--dummy",    256, RTGETOPT_REQ_NOTHING },   // placeholder for C++
};

int handleShowHardDiskInfo(HandlerArg *a)
{
    HRESULT rc;
    const char *FilenameOrUuid = NULL;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aShowHardDiskInfoOptions, RT_ELEMENTS(g_aShowHardDiskInfoOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case VINF_GETOPT_NOT_OPTION:
                if (!FilenameOrUuid)
                    FilenameOrUuid = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_SHOWHDINFO, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_SHOWHDINFO, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_SHOWHDINFO, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_SHOWHDINFO, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_SHOWHDINFO, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_SHOWHDINFO, "error: %Rrs", c);
        }
    }

    /* check for required options */
    if (!FilenameOrUuid)
        return errorSyntax(USAGE_SHOWHDINFO, "Disk name or UUID required");

    ComPtr<IMedium> hardDisk;
    bool unknown = false;

    rc = findOrOpenMedium(a, FilenameOrUuid, DeviceType_HardDisk, hardDisk,
                          false /* fForceNewUuidOnOpen */, &unknown);
    if (FAILED(rc))
        return 1;

    do
    {
        Bstr uuid;
        hardDisk->COMGETTER(Id)(uuid.asOutParam());
        RTPrintf("UUID:                 %s\n", Utf8Str(uuid).c_str());

        /* check for accessibility */
        /// @todo NEWMEDIA check accessibility of all parents
        /// @todo NEWMEDIA print the full state value
        MediumState_T state;
        CHECK_ERROR_BREAK(hardDisk, RefreshState(&state));
        RTPrintf("Accessible:           %s\n", state != MediumState_Inaccessible ? "yes" : "no");

        if (state == MediumState_Inaccessible)
        {
            Bstr err;
            CHECK_ERROR_BREAK(hardDisk, COMGETTER(LastAccessError)(err.asOutParam()));
            RTPrintf("Access Error:         %ls\n", err.raw());
        }

        Bstr description;
        hardDisk->COMGETTER(Description)(description.asOutParam());
        if (!description.isEmpty())
        {
            RTPrintf("Description:          %ls\n", description.raw());
        }

        LONG64 logicalSize;
        hardDisk->COMGETTER(LogicalSize)(&logicalSize);
        RTPrintf("Logical size:         %lld MBytes\n", logicalSize >> 20);
        LONG64 actualSize;
        hardDisk->COMGETTER(Size)(&actualSize);
        RTPrintf("Current size on disk: %lld MBytes\n", actualSize >> 20);

        ComPtr <IMedium> parent;
        hardDisk->COMGETTER(Parent)(parent.asOutParam());

        MediumType_T type;
        hardDisk->COMGETTER(Type)(&type);
        const char *typeStr = "unknown";
        switch (type)
        {
            case MediumType_Normal:
                if (!parent.isNull())
                    typeStr = "normal (differencing)";
                else
                    typeStr = "normal (base)";
                break;
            case MediumType_Immutable:
                typeStr = "immutable";
                break;
            case MediumType_Writethrough:
                typeStr = "writethrough";
                break;
            case MediumType_Shareable:
                typeStr = "shareable";
                break;
            case MediumType_Readonly:
                typeStr = "readonly";
                break;
            case MediumType_MultiAttach:
                typeStr = "multiattach";
                break;
        }
        RTPrintf("Type:                 %s\n", typeStr);

        Bstr format;
        hardDisk->COMGETTER(Format)(format.asOutParam());
        RTPrintf("Storage format:       %ls\n", format.raw());
        ULONG variant;
        hardDisk->COMGETTER(Variant)(&variant);
        const char *variantStr = "unknown";
        switch (variant & ~(MediumVariant_Fixed | MediumVariant_Diff))
        {
            case MediumVariant_VmdkSplit2G:
                variantStr = "split2G";
                break;
            case MediumVariant_VmdkStreamOptimized:
                variantStr = "streamOptimized";
                break;
            case MediumVariant_VmdkESX:
                variantStr = "ESX";
                break;
            case MediumVariant_Standard:
                variantStr = "default";
                break;
        }
        const char *variantTypeStr = "dynamic";
        if (variant & MediumVariant_Fixed)
            variantTypeStr = "fixed";
        else if (variant & MediumVariant_Diff)
            variantTypeStr = "differencing";
        RTPrintf("Format variant:       %s %s\n", variantTypeStr, variantStr);

        /// @todo also dump config parameters (iSCSI)

        if (!unknown)
        {
            com::SafeArray<BSTR> machineIds;
            hardDisk->COMGETTER(MachineIds)(ComSafeArrayAsOutParam(machineIds));
            for (size_t j = 0; j < machineIds.size(); ++ j)
            {
                ComPtr<IMachine> machine;
                CHECK_ERROR(a->virtualBox, FindMachine(machineIds[j], machine.asOutParam()));
                ASSERT(machine);
                Bstr name;
                machine->COMGETTER(Name)(name.asOutParam());
                machine->COMGETTER(Id)(uuid.asOutParam());
                RTPrintf("%s%ls (UUID: %ls)\n",
                         j == 0 ? "In use by VMs:        " : "                      ",
                         name.raw(), machineIds[j]);
            }
            /// @todo NEWMEDIA check usage in snapshots too
            /// @todo NEWMEDIA also list children
        }

        Bstr loc;
        hardDisk->COMGETTER(Location)(loc.asOutParam());
        RTPrintf("Location:             %ls\n", loc.raw());

        /* print out information specific for differencing hard disks */
        if (!parent.isNull())
        {
            BOOL autoReset = FALSE;
            hardDisk->COMGETTER(AutoReset)(&autoReset);
            RTPrintf("Auto-Reset:           %s\n", autoReset ? "on" : "off");
        }
    }
    while (0);

    if (unknown)
    {
        /* close the unknown hard disk to forget it again */
        hardDisk->Close();
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aCloseMediumOptions[] =
{
    { "disk",           'd', RTGETOPT_REQ_NOTHING },
    { "dvd",            'D', RTGETOPT_REQ_NOTHING },
    { "floppy",         'f', RTGETOPT_REQ_NOTHING },
    { "--delete",       'r', RTGETOPT_REQ_NOTHING },
};

int handleCloseMedium(HandlerArg *a)
{
    HRESULT rc = S_OK;
    enum {
        CMD_NONE,
        CMD_DISK,
        CMD_DVD,
        CMD_FLOPPY
    } cmd = CMD_NONE;
    const char *FilenameOrUuid = NULL;
    bool fDelete = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aCloseMediumOptions, RT_ELEMENTS(g_aCloseMediumOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'd':   // disk
                if (cmd != CMD_NONE)
                    return errorSyntax(USAGE_CLOSEMEDIUM, "Only one command can be specified: '%s'", ValueUnion.psz);
                cmd = CMD_DISK;
                break;

            case 'D':   // DVD
                if (cmd != CMD_NONE)
                    return errorSyntax(USAGE_CLOSEMEDIUM, "Only one command can be specified: '%s'", ValueUnion.psz);
                cmd = CMD_DVD;
                break;

            case 'f':   // floppy
                if (cmd != CMD_NONE)
                    return errorSyntax(USAGE_CLOSEMEDIUM, "Only one command can be specified: '%s'", ValueUnion.psz);
                cmd = CMD_FLOPPY;
                break;

            case 'r':   // --delete
                fDelete = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!FilenameOrUuid)
                    FilenameOrUuid = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_CLOSEMEDIUM, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_CLOSEMEDIUM, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_CLOSEMEDIUM, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_CLOSEMEDIUM, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_CLOSEMEDIUM, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_CLOSEMEDIUM, "error: %Rrs", c);
        }
    }

    /* check for required options */
    if (cmd == CMD_NONE)
        return errorSyntax(USAGE_CLOSEMEDIUM, "Command variant disk/dvd/floppy required");
    if (!FilenameOrUuid)
        return errorSyntax(USAGE_CLOSEMEDIUM, "Disk name or UUID required");

    ComPtr<IMedium> medium;

    if (cmd == CMD_DISK)
        rc = findMedium(a, FilenameOrUuid, DeviceType_HardDisk, false /* fSilent */, medium);
    else if (cmd == CMD_DVD)
        rc = findMedium(a, FilenameOrUuid, DeviceType_DVD, false /* fSilent */, medium);
    else if (cmd == CMD_FLOPPY)
        rc = findMedium(a, FilenameOrUuid, DeviceType_Floppy, false /* fSilent */, medium);

    if (SUCCEEDED(rc) && medium)
    {
        if (fDelete)
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR(medium, DeleteStorage(progress.asOutParam()));
            if (SUCCEEDED(rc))
            {
                rc = showProgress(progress);
                CHECK_PROGRESS_ERROR(progress, ("Failed to delete medium"));
            }
            else
                RTMsgError("Failed to delete medium. Error code %Rrc", rc);
        }
        CHECK_ERROR(medium, Close());
    }

    return SUCCEEDED(rc) ? 0 : 1;
}
#endif /* !VBOX_ONLY_DOCS */
