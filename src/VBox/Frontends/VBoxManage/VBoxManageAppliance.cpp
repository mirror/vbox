/* $Id$ */
/** @file
 * VBoxManage - The appliance-related commands.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/log.h>
#include <VBox/param.h>

#include <VBox/version.h>
#include <revision-generated.h> /* VBOX_SVN_REV - PCH prevents putting it in DEFS. */

#include <list>
#include <map>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/getopt.h>
#include <iprt/ctype.h>
#include <iprt/path.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/zip.h>
#include <iprt/stream.h>
#include <iprt/vfs.h>
#include <iprt/manifest.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/x509.h>
#include <iprt/crypto/pkcs7.h>
#include <iprt/crypto/store.h>
#include <iprt/crypto/spc.h>
#include <iprt/crypto/key.h>
#include <iprt/crypto/pkix.h>



#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////

typedef std::map<Utf8Str, Utf8Str> ArgsMap;                 // pairs of strings like "vmname" => "newvmname"
typedef std::map<uint32_t, ArgsMap> ArgsMapsMap;            // map of maps, one for each virtual system, sorted by index

typedef std::map<uint32_t, bool> IgnoresMap;                // pairs of numeric description entry indices
typedef std::map<uint32_t, IgnoresMap> IgnoresMapsMap;      // map of maps, one for each virtual system, sorted by index

static bool findArgValue(Utf8Str &strOut,
                         ArgsMap *pmapArgs,
                         const Utf8Str &strKey)
{
    if (pmapArgs)
    {
        ArgsMap::iterator it;
        it = pmapArgs->find(strKey);
        if (it != pmapArgs->end())
        {
            strOut = it->second;
            pmapArgs->erase(it);
            return true;
        }
    }

    return false;
}

static int parseImportOptions(const char *psz, com::SafeArray<ImportOptions_T> *options)
{
    int rc = VINF_SUCCESS;
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
            if (!RTStrNICmp(psz, "KeepAllMACs", len))
                options->push_back(ImportOptions_KeepAllMACs);
            else if (!RTStrNICmp(psz, "KeepNATMACs", len))
                options->push_back(ImportOptions_KeepNATMACs);
            else if (!RTStrNICmp(psz, "ImportToVDI", len))
                options->push_back(ImportOptions_ImportToVDI);
            else
                rc = VERR_PARSE_ERROR;
        }
        if (pszComma)
            psz += len + 1;
        else
            psz += len;
    }

    return rc;
}

static const RTGETOPTDEF g_aImportApplianceOptions[] =
{
    { "--dry-run",              'n', RTGETOPT_REQ_NOTHING },
    { "-dry-run",               'n', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--dryrun",               'n', RTGETOPT_REQ_NOTHING },
    { "-dryrun",                'n', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--detailed-progress",    'P', RTGETOPT_REQ_NOTHING },
    { "-detailed-progress",     'P', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--vsys",                 's', RTGETOPT_REQ_UINT32 },
    { "-vsys",                  's', RTGETOPT_REQ_UINT32 },     // deprecated
    { "--ostype",               'o', RTGETOPT_REQ_STRING },
    { "-ostype",                'o', RTGETOPT_REQ_STRING },     // deprecated
    { "--vmname",               'V', RTGETOPT_REQ_STRING },
    { "-vmname",                'V', RTGETOPT_REQ_STRING },     // deprecated
    { "--settingsfile",         'S', RTGETOPT_REQ_STRING },
    { "--basefolder",           'p', RTGETOPT_REQ_STRING },
    { "--group",                'g', RTGETOPT_REQ_STRING },
    { "--memory",               'm', RTGETOPT_REQ_STRING },
    { "-memory",                'm', RTGETOPT_REQ_STRING },     // deprecated
    { "--cpus",                 'c', RTGETOPT_REQ_STRING },
    { "--description",          'd', RTGETOPT_REQ_STRING },
    { "--eula",                 'L', RTGETOPT_REQ_STRING },
    { "-eula",                  'L', RTGETOPT_REQ_STRING },     // deprecated
    { "--unit",                 'u', RTGETOPT_REQ_UINT32 },
    { "-unit",                  'u', RTGETOPT_REQ_UINT32 },     // deprecated
    { "--ignore",               'x', RTGETOPT_REQ_NOTHING },
    { "-ignore",                'x', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--scsitype",             'T', RTGETOPT_REQ_UINT32 },
    { "-scsitype",              'T', RTGETOPT_REQ_UINT32 },     // deprecated
    { "--type",                 'T', RTGETOPT_REQ_UINT32 },     // deprecated
    { "-type",                  'T', RTGETOPT_REQ_UINT32 },     // deprecated
#if 0 /* Changing the controller is fully valid, but the current design on how
         the params are evaluated here doesn't allow two parameter for one
         unit. The target disk path is more important. I leave it for future
         improvments. */
    { "--controller",           'C', RTGETOPT_REQ_STRING },
#endif
    { "--disk",                 'D', RTGETOPT_REQ_STRING },
    { "--options",              'O', RTGETOPT_REQ_STRING },

    { "--cloud",                'j', RTGETOPT_REQ_NOTHING},
    { "--cloudprofile",         'k', RTGETOPT_REQ_STRING },
    { "--cloudinstanceid",      'l', RTGETOPT_REQ_STRING },
    { "--cloudbucket",          'B', RTGETOPT_REQ_STRING }
};

enum actionType
{
    NOT_SET, LOCAL, CLOUD
} actionType;

RTEXITCODE handleImportAppliance(HandlerArg *arg)
{
    HRESULT rc = S_OK;
    bool fCloud = false; // the default
    actionType = NOT_SET;
    Utf8Str strOvfFilename;
    bool fExecute = true;                  // if true, then we actually do the import
    com::SafeArray<ImportOptions_T> options;
    uint32_t ulCurVsys = (uint32_t)-1;
    uint32_t ulCurUnit = (uint32_t)-1;
    // for each --vsys X command, maintain a map of command line items
    // (we'll parse them later after interpreting the OVF, when we can
    // actually check whether they make sense semantically)
    ArgsMapsMap mapArgsMapsPerVsys;
    IgnoresMapsMap mapIgnoresMapsPerVsys;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, arg->argc, arg->argv, g_aImportApplianceOptions, RT_ELEMENTS(g_aImportApplianceOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'n':   // --dry-run
                fExecute = false;
                break;

            case 'P':   // --detailed-progress
                g_fDetailedProgress = true;
                break;

            case 's':   // --vsys
                if (fCloud == false && actionType == NOT_SET)
                    actionType = LOCAL;

                if (actionType != LOCAL)
                    return errorSyntax(USAGE_EXPORTAPPLIANCE,
                                       "Option \"%s\" can't be used together with \"--cloud\" argument.",
                                       GetState.pDef->pszLong);

                ulCurVsys = ValueUnion.u32;
                ulCurUnit = (uint32_t)-1;
                break;

            case 'o':   // --ostype
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["ostype"] = ValueUnion.psz;
                break;

            case 'V':   // --vmname
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["vmname"] = ValueUnion.psz;
                break;

            case 'S':   // --settingsfile
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["settingsfile"] = ValueUnion.psz;
                break;

            case 'p':   // --basefolder
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["basefolder"] = ValueUnion.psz;
                break;

            case 'g':   // --group
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["group"] = ValueUnion.psz;
                break;

            case 'd':   // --description
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["description"] = ValueUnion.psz;
                break;

            case 'L':   // --eula
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["eula"] = ValueUnion.psz;
                break;

            case 'm':   // --memory
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["memory"] = ValueUnion.psz;
                break;

            case 'c':   // --cpus
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["cpus"] = ValueUnion.psz;
                break;

            case 'u':   // --unit
                ulCurUnit = ValueUnion.u32;
                break;

            case 'x':   // --ignore
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                if (ulCurUnit == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --unit argument.", GetState.pDef->pszLong);
                mapIgnoresMapsPerVsys[ulCurVsys][ulCurUnit] = true;
                break;

            case 'T':   // --scsitype
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                if (ulCurUnit == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --unit argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys][Utf8StrFmt("scsitype%u", ulCurUnit)] = ValueUnion.psz;
                break;

            case 'C':   // --controller
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                if (ulCurUnit == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --unit argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys][Utf8StrFmt("controller%u", ulCurUnit)] = ValueUnion.psz;
                break;

            case 'D':   // --disk
                if (actionType == LOCAL && ulCurVsys == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                if (ulCurUnit == (uint32_t)-1)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --unit argument.", GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys][Utf8StrFmt("disk%u", ulCurUnit)] = ValueUnion.psz;
                break;

            case 'O':   // --options
                if (RT_FAILURE(parseImportOptions(ValueUnion.psz, &options)))
                    return errorArgument("Invalid import options '%s'\n", ValueUnion.psz);
                break;

                /*--cloud and --vsys are orthogonal, only one must be presented*/
            case 'j':   // --cloud
                if (fCloud == false && actionType == NOT_SET)
                {
                    fCloud = true;
                    actionType = CLOUD;
                }

                if (actionType != CLOUD)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                       "Option \"%s\" can't be used together with \"--vsys\" argument.",
                                       GetState.pDef->pszLong);

                ulCurVsys = 0;
                break;

                /* Cloud export settings */
            case 'k':   // --cloudprofile
                if (actionType != CLOUD)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                       GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["cloudprofile"] = ValueUnion.psz;
                break;

            case 'l':   // --cloudinstanceid
                if (actionType != CLOUD)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                       GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["cloudinstanceid"] = ValueUnion.psz;
                break;

            case 'B':   // --cloudbucket
                if (actionType != CLOUD)
                    return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                       GetState.pDef->pszLong);
                mapArgsMapsPerVsys[ulCurVsys]["cloudbucket"] = ValueUnion.psz;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (strOvfFilename.isEmpty())
                    strOvfFilename = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_IMPORTAPPLIANCE, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_IMPORTAPPLIANCE, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "error: %Rrs", c);
        }
    }

    /* Last check after parsing all arguments */
    if (strOvfFilename.isNotEmpty())
    {
        if (actionType == NOT_SET)
        {
            if (fCloud)
                actionType = CLOUD;
            else
                actionType = LOCAL;
        }
    }
    else
        return errorSyntax(USAGE_IMPORTAPPLIANCE, "Not enough arguments for \"import\" command.");

    do
    {
        ComPtr<IAppliance> pAppliance;
        CHECK_ERROR_BREAK(arg->virtualBox, CreateAppliance(pAppliance.asOutParam()));
        //in the case of Cloud, append the instance id here because later it's harder to do
        if (actionType == CLOUD)
        {
            try
            {
                /* Check presence of cloudprofile and cloudinstanceid in the map.
                 * If there isn't the exception is triggered. It's standard std:map logic.*/
                ArgsMap a = mapArgsMapsPerVsys[ulCurVsys];
                a.at("cloudprofile");
                a.at("cloudinstanceid");
            } catch (...)
            {
                return errorSyntax(USAGE_IMPORTAPPLIANCE, "Not enough arguments for import from the Cloud.");
            }

            strOvfFilename.append(mapArgsMapsPerVsys[ulCurVsys]["cloudprofile"]);
            strOvfFilename.append("/");
            strOvfFilename.append(mapArgsMapsPerVsys[ulCurVsys]["cloudinstanceid"]);
        }

        char *pszAbsFilePath;
        if (strOvfFilename.startsWith("S3://", RTCString::CaseInsensitive) ||
            strOvfFilename.startsWith("SunCloud://", RTCString::CaseInsensitive) ||
            strOvfFilename.startsWith("webdav://", RTCString::CaseInsensitive) ||
            strOvfFilename.startsWith("OCI://", RTCString::CaseInsensitive))
            pszAbsFilePath = RTStrDup(strOvfFilename.c_str());
        else
            pszAbsFilePath = RTPathAbsDup(strOvfFilename.c_str());

        ComPtr<IProgress> progressRead;
        CHECK_ERROR_BREAK(pAppliance, Read(Bstr(pszAbsFilePath).raw(),
                                           progressRead.asOutParam()));
        RTStrFree(pszAbsFilePath);

        rc = showProgress(progressRead);
        CHECK_PROGRESS_ERROR_RET(progressRead, ("Appliance read failed"), RTEXITCODE_FAILURE);

        Bstr path; /* fetch the path, there is stuff like username/password removed if any */
        CHECK_ERROR_BREAK(pAppliance, COMGETTER(Path)(path.asOutParam()));

        size_t cVirtualSystemDescriptions = 0;
        com::SafeIfaceArray<IVirtualSystemDescription> aVirtualSystemDescriptions;

        if (actionType == LOCAL)
        {
            // call interpret(); this can yield both warnings and errors, so we need
            // to tinker with the error info a bit
            RTStrmPrintf(g_pStdErr, "Interpreting %ls...\n", path.raw());
            rc = pAppliance->Interpret();
            com::ErrorInfo info0(pAppliance, COM_IIDOF(IAppliance));

            com::SafeArray<BSTR> aWarnings;
            if (SUCCEEDED(pAppliance->GetWarnings(ComSafeArrayAsOutParam(aWarnings))))
            {
                size_t cWarnings = aWarnings.size();
                for (unsigned i = 0; i < cWarnings; ++i)
                {
                    Bstr bstrWarning(aWarnings[i]);
                    RTMsgWarning("%ls.", bstrWarning.raw());
                }
            }

            if (FAILED(rc))     // during interpret, after printing warnings
            {
                com::GluePrintErrorInfo(info0);
                com::GluePrintErrorContext("Interpret", __FILE__, __LINE__);
                break;
            }

            RTStrmPrintf(g_pStdErr, "OK.\n");

            // fetch all disks
            com::SafeArray<BSTR> retDisks;
            CHECK_ERROR_BREAK(pAppliance,
                              COMGETTER(Disks)(ComSafeArrayAsOutParam(retDisks)));
            if (retDisks.size() > 0)
            {
                RTPrintf("Disks:\n");
                for (unsigned i = 0; i < retDisks.size(); i++)
                    RTPrintf("  %ls\n", retDisks[i]);
                RTPrintf("\n");
            }

            // fetch virtual system descriptions
            CHECK_ERROR_BREAK(pAppliance,
                              COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(aVirtualSystemDescriptions)));

            cVirtualSystemDescriptions = aVirtualSystemDescriptions.size();

            // match command line arguments with virtual system descriptions;
            // this is only to sort out invalid indices at this time
            ArgsMapsMap::const_iterator it;
            for (it = mapArgsMapsPerVsys.begin();
                 it != mapArgsMapsPerVsys.end();
                 ++it)
            {
                uint32_t ulVsys = it->first;
                if (ulVsys >= cVirtualSystemDescriptions)
                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                       "Invalid index %RI32 with -vsys option; the OVF contains only %zu virtual system(s).",
                                       ulVsys, cVirtualSystemDescriptions);
            }
        }
        else if (actionType == CLOUD)
        {
            /* In the Cloud case the call of interpret() isn't needed because there isn't any OVF XML file.
             * All info is got from the Cloud and VSD is filled inside IAppliance::read(). */
            // fetch virtual system descriptions
            CHECK_ERROR_BREAK(pAppliance,
                              COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(aVirtualSystemDescriptions)));

            cVirtualSystemDescriptions = aVirtualSystemDescriptions.size();
        }

        uint32_t cLicensesInTheWay = 0;

        // dump virtual system descriptions and match command-line arguments
        if (cVirtualSystemDescriptions > 0)
        {
            for (unsigned i = 0; i < cVirtualSystemDescriptions; ++i)
            {
                com::SafeArray<VirtualSystemDescriptionType_T> retTypes;
                com::SafeArray<BSTR> aRefs;
                com::SafeArray<BSTR> aOvfValues;
                com::SafeArray<BSTR> aVBoxValues;
                com::SafeArray<BSTR> aExtraConfigValues;
                CHECK_ERROR_BREAK(aVirtualSystemDescriptions[i],
                                  GetDescription(ComSafeArrayAsOutParam(retTypes),
                                                 ComSafeArrayAsOutParam(aRefs),
                                                 ComSafeArrayAsOutParam(aOvfValues),
                                                 ComSafeArrayAsOutParam(aVBoxValues),
                                                 ComSafeArrayAsOutParam(aExtraConfigValues)));

                RTPrintf("Virtual system %u:\n", i);

                // look up the corresponding command line options, if any
                ArgsMap *pmapArgs = NULL;
                ArgsMapsMap::iterator itm = mapArgsMapsPerVsys.find(i);
                if (itm != mapArgsMapsPerVsys.end())
                    pmapArgs = &itm->second;

                // this collects the final values for setFinalValues()
                com::SafeArray<BOOL> aEnabled(retTypes.size());
                com::SafeArray<BSTR> aFinalValues(retTypes.size());

                for (unsigned a = 0; a < retTypes.size(); ++a)
                {
                    VirtualSystemDescriptionType_T t = retTypes[a];

                    Utf8Str strOverride;

                    Bstr bstrFinalValue = aVBoxValues[a];

                    bool fIgnoreThis = mapIgnoresMapsPerVsys[i][a];

                    aEnabled[a] = true;

                    switch (t)
                    {
                        case VirtualSystemDescriptionType_OS:
                            if (findArgValue(strOverride, pmapArgs, "ostype"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: OS type specified with --ostype: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested OS type: \"%ls\""
                                        "\n    (change with \"--vsys %u --ostype <type>\"; use \"list ostypes\" to list all possible values)\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_Name:
                            if (findArgValue(strOverride, pmapArgs, "vmname"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: VM name specified with --vmname: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested VM name \"%ls\""
                                        "\n    (change with \"--vsys %u --vmname <name>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_Product:
                            RTPrintf("%2u: Product (ignored): %ls\n",
                                     a, aVBoxValues[a]);
                            break;

                        case VirtualSystemDescriptionType_ProductUrl:
                            RTPrintf("%2u: ProductUrl (ignored): %ls\n",
                                     a, aVBoxValues[a]);
                            break;

                        case VirtualSystemDescriptionType_Vendor:
                            RTPrintf("%2u: Vendor (ignored): %ls\n",
                                     a, aVBoxValues[a]);
                            break;

                        case VirtualSystemDescriptionType_VendorUrl:
                            RTPrintf("%2u: VendorUrl (ignored): %ls\n",
                                     a, aVBoxValues[a]);
                            break;

                        case VirtualSystemDescriptionType_Version:
                            RTPrintf("%2u: Version (ignored): %ls\n",
                                     a, aVBoxValues[a]);
                            break;

                        case VirtualSystemDescriptionType_Description:
                            if (findArgValue(strOverride, pmapArgs, "description"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: Description specified with --description: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Description \"%ls\""
                                        "\n    (change with \"--vsys %u --description <desc>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_License:
                            ++cLicensesInTheWay;
                            if (findArgValue(strOverride, pmapArgs, "eula"))
                            {
                                if (strOverride == "show")
                                {
                                    RTPrintf("%2u: End-user license agreement"
                                             "\n    (accept with \"--vsys %u --eula accept\"):"
                                             "\n\n%ls\n\n",
                                             a, i, bstrFinalValue.raw());
                                }
                                else if (strOverride == "accept")
                                {
                                    RTPrintf("%2u: End-user license agreement (accepted)\n",
                                             a);
                                    --cLicensesInTheWay;
                                }
                                else
                                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                                       "Argument to --eula must be either \"show\" or \"accept\".");
                            }
                            else
                                RTPrintf("%2u: End-user license agreement"
                                        "\n    (display with \"--vsys %u --eula show\";"
                                        "\n    accept with \"--vsys %u --eula accept\")\n",
                                        a, i, i);
                            break;

                        case VirtualSystemDescriptionType_CPU:
                            if (findArgValue(strOverride, pmapArgs, "cpus"))
                            {
                                uint32_t cCPUs;
                                if (    strOverride.toInt(cCPUs) == VINF_SUCCESS
                                     && cCPUs >= VMM_MIN_CPU_COUNT
                                     && cCPUs <= VMM_MAX_CPU_COUNT
                                   )
                                {
                                    bstrFinalValue = strOverride;
                                    RTPrintf("%2u: No. of CPUs specified with --cpus: %ls\n",
                                             a, bstrFinalValue.raw());
                                }
                                else
                                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                                       "Argument to --cpus option must be a number greater than %d and less than %d.",
                                                       VMM_MIN_CPU_COUNT - 1, VMM_MAX_CPU_COUNT + 1);
                            }
                            else
                                RTPrintf("%2u: Number of CPUs: %ls\n    (change with \"--vsys %u --cpus <n>\")\n",
                                         a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_Memory:
                        {
                            if (findArgValue(strOverride, pmapArgs, "memory"))
                            {
                                uint32_t ulMemMB;
                                if (VINF_SUCCESS == strOverride.toInt(ulMemMB))
                                {
                                    bstrFinalValue = strOverride;
                                    RTPrintf("%2u: Guest memory specified with --memory: %ls MB\n",
                                             a, bstrFinalValue.raw());
                                }
                                else
                                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                                       "Argument to --memory option must be a non-negative number.");
                            }
                            else
                                RTPrintf("%2u: Guest memory: %ls MB\n    (change with \"--vsys %u --memory <MB>\")\n",
                                         a, bstrFinalValue.raw(), i);
                            break;
                        }

                        case VirtualSystemDescriptionType_HardDiskControllerIDE:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: IDE controller, type %ls -- disabled\n",
                                         a,
                                         aVBoxValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: IDE controller, type %ls"
                                         "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                         a,
                                         aVBoxValues[a],
                                         i, a);
                            break;

                        case VirtualSystemDescriptionType_HardDiskControllerSATA:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: SATA controller, type %ls -- disabled\n",
                                         a,
                                         aVBoxValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: SATA controller, type %ls"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a,
                                        aVBoxValues[a],
                                        i, a);
                            break;

                        case VirtualSystemDescriptionType_HardDiskControllerSAS:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: SAS controller, type %ls -- disabled\n",
                                         a,
                                         aVBoxValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: SAS controller, type %ls"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a,
                                        aVBoxValues[a],
                                        i, a);
                            break;

                        case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: SCSI controller, type %ls -- disabled\n",
                                         a,
                                         aVBoxValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                            {
                                Utf8StrFmt strTypeArg("scsitype%u", a);
                                if (findArgValue(strOverride, pmapArgs, strTypeArg))
                                {
                                    bstrFinalValue = strOverride;
                                    RTPrintf("%2u: SCSI controller, type set with --unit %u --scsitype: \"%ls\"\n",
                                            a,
                                            a,
                                            bstrFinalValue.raw());
                                }
                                else
                                    RTPrintf("%2u: SCSI controller, type %ls"
                                            "\n    (change with \"--vsys %u --unit %u --scsitype {BusLogic|LsiLogic}\";"
                                            "\n    disable with \"--vsys %u --unit %u --ignore\")\n",
                                            a,
                                            aVBoxValues[a],
                                            i, a, i, a);
                            }
                            break;

                        case VirtualSystemDescriptionType_HardDiskImage:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: Hard disk image: source image=%ls -- disabled\n",
                                         a,
                                         aOvfValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                            {
                                Utf8StrFmt strTypeArg("disk%u", a);
                                RTCList<ImportOptions_T> optionsList = options.toList();

                                bstrFinalValue = aVBoxValues[a];

                                if (findArgValue(strOverride, pmapArgs, strTypeArg))
                                {
                                    if (!optionsList.contains(ImportOptions_ImportToVDI))
                                    {
                                        RTUUID uuid;
                                        /* Check if this is a uuid. If so, don't touch. */
                                        int vrc = RTUuidFromStr(&uuid, strOverride.c_str());
                                        if (vrc != VINF_SUCCESS)
                                        {
                                            /* Make the path absolute. */
                                            if (!RTPathStartsWithRoot(strOverride.c_str()))
                                            {
                                                char pszPwd[RTPATH_MAX];
                                                vrc = RTPathGetCurrent(pszPwd, RTPATH_MAX);
                                                if (RT_SUCCESS(vrc))
                                                    strOverride = Utf8Str(pszPwd).append(RTPATH_SLASH).append(strOverride);
                                            }
                                        }
                                        bstrFinalValue = strOverride;
                                    }
                                    else
                                    {
                                        //print some error about incompatible command-line arguments
                                        return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                                           "Option --ImportToVDI shall not be used together with "
                                                           "manually set target path.");

                                    }

                                    RTPrintf("%2u: Hard disk image: source image=%ls, target path=%ls, %ls\n",
                                            a,
                                            aOvfValues[a],
                                            bstrFinalValue.raw(),
                                            aExtraConfigValues[a]);
                                }
#if 0 /* Changing the controller is fully valid, but the current design on how
         the params are evaluated here doesn't allow two parameter for one
         unit. The target disk path is more important I leave it for future
         improvments. */
                                Utf8StrFmt strTypeArg("controller%u", a);
                                if (findArgValue(strOverride, pmapArgs, strTypeArg))
                                {
                                    // strOverride now has the controller index as a number, but we
                                    // need a "controller=X" format string
                                    strOverride = Utf8StrFmt("controller=%s", strOverride.c_str());
                                    Bstr bstrExtraConfigValue = strOverride;
                                    bstrExtraConfigValue.detachTo(&aExtraConfigValues[a]);
                                    RTPrintf("%2u: Hard disk image: source image=%ls, target path=%ls, %ls\n",
                                            a,
                                            aOvfValues[a],
                                            aVBoxValues[a],
                                            aExtraConfigValues[a]);
                                }
#endif
                                else
                                {
                                    strOverride = aVBoxValues[a];

                                    /*
                                     * Current solution isn't optimal.
                                     * Better way is to provide API call for function
                                     * Appliance::i_findMediumFormatFromDiskImage()
                                     * and creating one new function which returns
                                     * struct ovf::DiskImage for currently processed disk.
                                    */

                                    /*
                                     * if user wants to convert all imported disks to VDI format
                                     * we need to replace files extensions to "vdi"
                                     * except CD/DVD disks
                                     */
                                    if (optionsList.contains(ImportOptions_ImportToVDI))
                                    {
                                        ComPtr<IVirtualBox> pVirtualBox = arg->virtualBox;
                                        ComPtr<ISystemProperties> systemProperties;
                                        com::SafeIfaceArray<IMediumFormat> mediumFormats;
                                        Bstr bstrFormatName;

                                        CHECK_ERROR(pVirtualBox,
                                                     COMGETTER(SystemProperties)(systemProperties.asOutParam()));

                                        CHECK_ERROR(systemProperties,
                                             COMGETTER(MediumFormats)(ComSafeArrayAsOutParam(mediumFormats)));

                                        /* go through all supported media formats and store files extensions only for RAW */
                                        com::SafeArray<BSTR> extensions;

                                        for (unsigned j = 0; j < mediumFormats.size(); ++j)
                                        {
                                            com::SafeArray<DeviceType_T> deviceType;
                                            ComPtr<IMediumFormat> mediumFormat = mediumFormats[j];
                                            CHECK_ERROR(mediumFormat, COMGETTER(Name)(bstrFormatName.asOutParam()));
                                            Utf8Str strFormatName = Utf8Str(bstrFormatName);

                                            if (strFormatName.compare("RAW", Utf8Str::CaseInsensitive) == 0)
                                            {
                                                /* getting files extensions for "RAW" format */
                                                CHECK_ERROR(mediumFormat,
                                                            DescribeFileExtensions(ComSafeArrayAsOutParam(extensions),
                                                                                   ComSafeArrayAsOutParam(deviceType)));
                                                break;
                                            }
                                        }

                                        /* go through files extensions for RAW format and compare them with
                                         * extension of current file
                                         */
                                        bool fReplace = true;

                                        const char *pszExtension = RTPathSuffix(strOverride.c_str());
                                        if (pszExtension)
                                            pszExtension++;

                                        for (unsigned j = 0; j < extensions.size(); ++j)
                                        {
                                            Bstr bstrExt(extensions[j]);
                                            Utf8Str strExtension(bstrExt);
                                            if(strExtension.compare(pszExtension, Utf8Str::CaseInsensitive) == 0)
                                            {
                                                fReplace = false;
                                                break;
                                            }
                                        }

                                        if (fReplace)
                                        {
                                            strOverride = strOverride.stripSuffix();
                                            strOverride = strOverride.append(".").append("vdi");
                                        }
                                    }

                                    bstrFinalValue = strOverride;

                                    RTPrintf("%2u: Hard disk image: source image=%ls, target path=%ls, %ls"
                                            "\n    (change target path with \"--vsys %u --unit %u --disk path\";"
                                            "\n    disable with \"--vsys %u --unit %u --ignore\")\n",
                                            a,
                                            aOvfValues[a],
                                            bstrFinalValue.raw(),
                                            aExtraConfigValues[a],
                                            i, a, i, a);
                                }
                            }
                            break;

                        case VirtualSystemDescriptionType_CDROM:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: CD-ROM -- disabled\n",
                                         a);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: CD-ROM"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a, i, a);
                            break;

                        case VirtualSystemDescriptionType_Floppy:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: Floppy -- disabled\n",
                                         a);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: Floppy"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a, i, a);
                            break;

                        case VirtualSystemDescriptionType_NetworkAdapter:
                            RTPrintf("%2u: Network adapter: orig %ls, config %ls, extra %ls\n",   /// @todo implement once we have a plan for the back-end
                                     a,
                                     aOvfValues[a],
                                     aVBoxValues[a],
                                     aExtraConfigValues[a]);
                            break;

                        case VirtualSystemDescriptionType_USBController:
                            if (fIgnoreThis)
                            {
                                RTPrintf("%2u: USB controller -- disabled\n",
                                         a);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: USB controller"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a, i, a);
                            break;

                        case VirtualSystemDescriptionType_SoundCard:
                            if (fIgnoreThis)
                            {
                               RTPrintf("%2u: Sound card \"%ls\" -- disabled\n",
                                         a,
                                         aOvfValues[a]);
                                aEnabled[a] = false;
                            }
                            else
                                RTPrintf("%2u: Sound card (appliance expects \"%ls\", can change on import)"
                                        "\n    (disable with \"--vsys %u --unit %u --ignore\")\n",
                                        a,
                                        aOvfValues[a],
                                        i,
                                        a);
                            break;

                        case VirtualSystemDescriptionType_SettingsFile:
                            if (findArgValue(strOverride, pmapArgs, "settingsfile"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: VM settings file name specified with --settingsfile: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested VM settings file name \"%ls\""
                                        "\n    (change with \"--vsys %u --settingsfile <filename>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_BaseFolder:
                            if (findArgValue(strOverride, pmapArgs, "basefolder"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: VM base folder specified with --basefolder: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested VM base folder \"%ls\""
                                        "\n    (change with \"--vsys %u --basefolder <path>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_PrimaryGroup:
                            if (findArgValue(strOverride, pmapArgs, "group"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: VM group specified with --group: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested VM group \"%ls\""
                                        "\n    (change with \"--vsys %u --group <group>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_CloudInstanceShape:
                            RTPrintf("%2u: Suggested cloud shape \"%ls\"\n",
                                    a, bstrFinalValue.raw());
                            break;

                        case VirtualSystemDescriptionType_CloudBucket:
                            if (findArgValue(strOverride, pmapArgs, "cloudbucket"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: Cloud bucket id specified with --cloudbucket: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested cloud bucket id \"%ls\""
                                        "\n    (change with \"--cloud %u --cloudbucket <id>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_CloudProfileName:
                            if (findArgValue(strOverride, pmapArgs, "cloudprofile"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: Cloud profile name specified with --cloudprofile: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested cloud profile name \"%ls\""
                                        "\n    (change with \"--cloud %u --cloudprofile <id>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_CloudInstanceId:
                            if (findArgValue(strOverride, pmapArgs, "cloudinstanceid"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2u: Cloud instance id specified with --cloudinstanceid: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2u: Suggested cloud instance id \"%ls\""
                                        "\n    (change with \"--cloud %u --cloudinstanceid <id>\")\n",
                                        a, bstrFinalValue.raw(), i);
                            break;

                        case VirtualSystemDescriptionType_CloudImageId:
                            RTPrintf("%2u: Suggested cloud base image id \"%ls\"\n",
                                    a, bstrFinalValue.raw());
                            break;
                        case VirtualSystemDescriptionType_CloudDomain:
                        case VirtualSystemDescriptionType_CloudBootDiskSize:
                        case VirtualSystemDescriptionType_CloudOCIVCN:
                        case VirtualSystemDescriptionType_CloudPublicIP:
                        case VirtualSystemDescriptionType_CloudOCISubnet:
                        case VirtualSystemDescriptionType_CloudKeepObject:
                        case VirtualSystemDescriptionType_CloudLaunchInstance:
                        case VirtualSystemDescriptionType_CloudInstanceState:
                        case VirtualSystemDescriptionType_CloudImageState:
                        case VirtualSystemDescriptionType_Miscellaneous:
                        case VirtualSystemDescriptionType_CloudInstanceDisplayName:
                        case VirtualSystemDescriptionType_CloudImageDisplayName:
                        case VirtualSystemDescriptionType_CloudOCILaunchMode:
                        case VirtualSystemDescriptionType_CloudPrivateIP:
                        case VirtualSystemDescriptionType_CloudBootVolumeId:
                        case VirtualSystemDescriptionType_CloudOCIVCNCompartment:
                        case VirtualSystemDescriptionType_CloudOCISubnetCompartment:
                        case VirtualSystemDescriptionType_CloudPublicSSHKey:
                        case VirtualSystemDescriptionType_BootingFirmware:
                            /** @todo  VirtualSystemDescriptionType_Miscellaneous? */
                            break;

                        case VirtualSystemDescriptionType_Ignore:
#ifdef VBOX_WITH_XPCOM_CPP_ENUM_HACK
                        case VirtualSystemDescriptionType_32BitHack:
#endif
                            break;
                    }

                    bstrFinalValue.detachTo(&aFinalValues[a]);
                }

                if (fExecute)
                    CHECK_ERROR_BREAK(aVirtualSystemDescriptions[i],
                                      SetFinalValues(ComSafeArrayAsInParam(aEnabled),
                                                     ComSafeArrayAsInParam(aFinalValues),
                                                     ComSafeArrayAsInParam(aExtraConfigValues)));

            } // for (unsigned i = 0; i < cVirtualSystemDescriptions; ++i)

            if (cLicensesInTheWay == 1)
                RTMsgError("Cannot import until the license agreement listed above is accepted.");
            else if (cLicensesInTheWay > 1)
                RTMsgError("Cannot import until the %c license agreements listed above are accepted.", cLicensesInTheWay);

            if (!cLicensesInTheWay && fExecute)
            {
                // go!
                ComPtr<IProgress> progress;
                CHECK_ERROR_BREAK(pAppliance,
                                  ImportMachines(ComSafeArrayAsInParam(options), progress.asOutParam()));

                rc = showProgress(progress);
                CHECK_PROGRESS_ERROR_RET(progress, ("Appliance import failed"), RTEXITCODE_FAILURE);

                if (SUCCEEDED(rc))
                    RTPrintf("Successfully imported the appliance.\n");
            }
        } // end if (aVirtualSystemDescriptions.size() > 0)
    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static int parseExportOptions(const char *psz, com::SafeArray<ExportOptions_T> *options)
{
    int rc = VINF_SUCCESS;
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
            if (!RTStrNICmp(psz, "CreateManifest", len))
                options->push_back(ExportOptions_CreateManifest);
            else if (!RTStrNICmp(psz, "manifest", len))
                options->push_back(ExportOptions_CreateManifest);
            else if (!RTStrNICmp(psz, "ExportDVDImages", len))
                options->push_back(ExportOptions_ExportDVDImages);
            else if (!RTStrNICmp(psz, "iso", len))
                options->push_back(ExportOptions_ExportDVDImages);
            else if (!RTStrNICmp(psz, "StripAllMACs", len))
                options->push_back(ExportOptions_StripAllMACs);
            else if (!RTStrNICmp(psz, "nomacs", len))
                options->push_back(ExportOptions_StripAllMACs);
            else if (!RTStrNICmp(psz, "StripAllNonNATMACs", len))
                options->push_back(ExportOptions_StripAllNonNATMACs);
            else if (!RTStrNICmp(psz, "nomacsbutnat", len))
                options->push_back(ExportOptions_StripAllNonNATMACs);
            else
                rc = VERR_PARSE_ERROR;
        }
        if (pszComma)
            psz += len + 1;
        else
            psz += len;
    }

    return rc;
}

static const RTGETOPTDEF g_aExportOptions[] =
{
    { "--output",               'o', RTGETOPT_REQ_STRING },
    { "--legacy09",             'l', RTGETOPT_REQ_NOTHING },
    { "--ovf09",                'l', RTGETOPT_REQ_NOTHING },
    { "--ovf10",                '1', RTGETOPT_REQ_NOTHING },
    { "--ovf20",                '2', RTGETOPT_REQ_NOTHING },
    { "--opc10",                'c', RTGETOPT_REQ_NOTHING },
    { "--manifest",             'm', RTGETOPT_REQ_NOTHING },    // obsoleted by --options
    { "--iso",                  'I', RTGETOPT_REQ_NOTHING },    // obsoleted by --options
    { "--vsys",                 's', RTGETOPT_REQ_UINT32 },
    { "--vmname",               'V', RTGETOPT_REQ_STRING },
    { "--product",              'p', RTGETOPT_REQ_STRING },
    { "--producturl",           'P', RTGETOPT_REQ_STRING },
    { "--vendor",               'n', RTGETOPT_REQ_STRING },
    { "--vendorurl",            'N', RTGETOPT_REQ_STRING },
    { "--version",              'v', RTGETOPT_REQ_STRING },
    { "--description",          'd', RTGETOPT_REQ_STRING },
    { "--eula",                 'e', RTGETOPT_REQ_STRING },
    { "--eulafile",             'E', RTGETOPT_REQ_STRING },
    { "--options",              'O', RTGETOPT_REQ_STRING },
    { "--cloud",                'C', RTGETOPT_REQ_UINT32 },
    { "--cloudshape",           'S', RTGETOPT_REQ_STRING },
    { "--clouddomain",          'D', RTGETOPT_REQ_STRING },
    { "--clouddisksize",        'R', RTGETOPT_REQ_STRING },
    { "--cloudbucket",          'B', RTGETOPT_REQ_STRING },
    { "--cloudocivcn",          'Q', RTGETOPT_REQ_STRING },
    { "--cloudpublicip",        'A', RTGETOPT_REQ_STRING },
    { "--cloudprofile",         'F', RTGETOPT_REQ_STRING },
    { "--cloudocisubnet",       'T', RTGETOPT_REQ_STRING },
    { "--cloudkeepobject",      'K', RTGETOPT_REQ_STRING },
    { "--cloudlaunchinstance",  'L', RTGETOPT_REQ_STRING },
    { "--cloudlaunchmode",      'M', RTGETOPT_REQ_STRING },
    { "--cloudprivateip",       'i', RTGETOPT_REQ_STRING },
};

RTEXITCODE handleExportAppliance(HandlerArg *a)
{
    HRESULT rc = S_OK;

    Utf8Str strOutputFile;
    Utf8Str strOvfFormat("ovf-1.0"); // the default export version
    bool fManifest = false; // the default
    bool fCloud = false; // the default
    actionType = NOT_SET;
    bool fExportISOImages = false; // the default
    com::SafeArray<ExportOptions_T> options;
    std::list< ComPtr<IMachine> > llMachines;

    uint32_t ulCurVsys = (uint32_t)-1;
    // for each --vsys X command, maintain a map of command line items
    ArgsMapsMap mapArgsMapsPerVsys;
    do
    {
        int c;

        RTGETOPTUNION ValueUnion;
        RTGETOPTSTATE GetState;
        // start at 0 because main() has hacked both the argc and argv given to us
        RTGetOptInit(&GetState, a->argc, a->argv, g_aExportOptions,
                     RT_ELEMENTS(g_aExportOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

        Utf8Str strProductUrl;
        while ((c = RTGetOpt(&GetState, &ValueUnion)))
        {
            switch (c)
            {
                case 'o':   // --output
                    if (strOutputFile.length())
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "You can only specify --output once.");
                    else
                        strOutputFile = ValueUnion.psz;
                    break;

                case 'l':   // --legacy09/--ovf09
                    strOvfFormat = "ovf-0.9";
                    break;

                case '1':   // --ovf10
                    strOvfFormat = "ovf-1.0";
                    break;

                case '2':   // --ovf20
                    strOvfFormat = "ovf-2.0";
                    break;

                case 'c':   // --opc
                    strOvfFormat = "opc-1.0";
                    break;

                case 'I':   // --iso
                    fExportISOImages = true;
                    break;

                case 'm':   // --manifest
                    fManifest = true;
                    break;

                case 's':   // --vsys
                    if (fCloud == false && actionType == NOT_SET)
                        actionType = LOCAL;

                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE,
                                           "Option \"%s\" can't be used together with \"--cloud\" argument.",
                                           GetState.pDef->pszLong);

                    ulCurVsys = ValueUnion.u32;
                    break;

                case 'V':   // --vmname
                    if (actionType == NOT_SET || ulCurVsys == (uint32_t)-1)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys or --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["vmname"] = ValueUnion.psz;
                    break;

                case 'p':   // --product
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["product"] = ValueUnion.psz;
                    break;

                case 'P':   // --producturl
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["producturl"] = ValueUnion.psz;
                    break;

                case 'n':   // --vendor
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["vendor"] = ValueUnion.psz;
                    break;

                case 'N':   // --vendorurl
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["vendorurl"] = ValueUnion.psz;
                    break;

                case 'v':   // --version
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["version"] = ValueUnion.psz;
                    break;

                case 'd':   // --description
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["description"] = ValueUnion.psz;
                    break;

                case 'e':   // --eula
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["eula"] = ValueUnion.psz;
                    break;

                case 'E':   // --eulafile
                    if (actionType != LOCAL)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --vsys argument.", GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["eulafile"] = ValueUnion.psz;
                    break;

                case 'O':   // --options
                    if (RT_FAILURE(parseExportOptions(ValueUnion.psz, &options)))
                        return errorArgument("Invalid export options '%s'\n", ValueUnion.psz);
                    break;

                    /*--cloud and --vsys are orthogonal, only one must be presented*/
                case 'C':   // --cloud
                    if (fCloud == false && actionType == NOT_SET)
                    {
                        fCloud = true;
                        actionType = CLOUD;
                    }

                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE,
                                           "Option \"%s\" can't be used together with \"--vsys\" argument.",
                                           GetState.pDef->pszLong);

                    ulCurVsys = ValueUnion.u32;
                    break;

                    /* Cloud export settings */
                case 'S':   // --cloudshape
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudshape"] = ValueUnion.psz;
                    break;

                case 'D':   // --clouddomain
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["clouddomain"] = ValueUnion.psz;
                    break;

                case 'R':   // --clouddisksize
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["clouddisksize"] = ValueUnion.psz;
                    break;

                case 'B':   // --cloudbucket
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudbucket"] = ValueUnion.psz;
                    break;

                case 'Q':   // --cloudocivcn
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudocivcn"] = ValueUnion.psz;
                    break;

                case 'A':   // --cloudpublicip
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudpublicip"] = ValueUnion.psz;
                    break;

                case 'i': /* --cloudprivateip */
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudprivateip"] = ValueUnion.psz;
                    break;

                case 'F':   // --cloudprofile
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudprofile"] = ValueUnion.psz;
                    break;

                case 'T':   // --cloudocisubnet
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudocisubnet"] = ValueUnion.psz;
                    break;

                case 'K':   // --cloudkeepobject
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudkeepobject"] = ValueUnion.psz;
                    break;

                case 'L':   // --cloudlaunchinstance
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudlaunchinstance"] = ValueUnion.psz;
                    break;

                case 'M': /* --cloudlaunchmode */
                    if (actionType != CLOUD)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "Option \"%s\" requires preceding --cloud argument.",
                                           GetState.pDef->pszLong);
                    mapArgsMapsPerVsys[ulCurVsys]["cloudlaunchmode"] = ValueUnion.psz;
                    break;

                case VINF_GETOPT_NOT_OPTION:
                {
                    Utf8Str strMachine(ValueUnion.psz);
                    // must be machine: try UUID or name
                    ComPtr<IMachine> machine;
                    CHECK_ERROR_BREAK(a->virtualBox, FindMachine(Bstr(strMachine).raw(),
                                                                 machine.asOutParam()));
                    if (machine)
                        llMachines.push_back(machine);
                    break;
                }

                default:
                    if (c > 0)
                    {
                        if (RT_C_IS_GRAPH(c))
                            return errorSyntax(USAGE_EXPORTAPPLIANCE, "unhandled option: -%c", c);
                        else
                            return errorSyntax(USAGE_EXPORTAPPLIANCE, "unhandled option: %i", c);
                    }
                    else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "unknown option: %s", ValueUnion.psz);
                    else if (ValueUnion.pDef)
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                    else
                        return errorSyntax(USAGE_EXPORTAPPLIANCE, "%Rrs", c);
            }

            if (FAILED(rc))
                break;
        }

        if (FAILED(rc))
            break;

        if (llMachines.empty())
            return errorSyntax(USAGE_EXPORTAPPLIANCE, "At least one machine must be specified with the export command.");

        /* Last check after parsing all arguments */
        if (strOutputFile.isNotEmpty())
        {
            if (actionType == NOT_SET)
            {
                if (fCloud)
                    actionType = CLOUD;
                else
                    actionType = LOCAL;
            }
        }
        else
            return errorSyntax(USAGE_EXPORTAPPLIANCE, "Missing --output argument with export command.");

        // match command line arguments with the machines count
        // this is only to sort out invalid indices at this time
        ArgsMapsMap::const_iterator it;
        for (it = mapArgsMapsPerVsys.begin();
             it != mapArgsMapsPerVsys.end();
             ++it)
        {
            uint32_t ulVsys = it->first;
            if (ulVsys >= llMachines.size())
                return errorSyntax(USAGE_EXPORTAPPLIANCE,
                                   "Invalid index %RI32 with -vsys option; you specified only %zu virtual system(s).",
                                   ulVsys, llMachines.size());
        }

        ComPtr<IAppliance> pAppliance;
        CHECK_ERROR_BREAK(a->virtualBox, CreateAppliance(pAppliance.asOutParam()));

        char *pszAbsFilePath = 0;
        if (strOutputFile.startsWith("S3://", RTCString::CaseInsensitive) ||
            strOutputFile.startsWith("SunCloud://", RTCString::CaseInsensitive) ||
            strOutputFile.startsWith("webdav://", RTCString::CaseInsensitive) ||
            strOutputFile.startsWith("OCI://", RTCString::CaseInsensitive))
            pszAbsFilePath = RTStrDup(strOutputFile.c_str());
        else
            pszAbsFilePath = RTPathAbsDup(strOutputFile.c_str());

        /*
         * The first stage - export machine/s to the Cloud or into the
         * OVA/OVF format on the local host.
         */

        /* VSDList is needed for the second stage where we launch the cloud instances if it was requested by user */
        std::list< ComPtr<IVirtualSystemDescription> > VSDList;
        std::list< ComPtr<IMachine> >::iterator itM;
        uint32_t i=0;
        for (itM = llMachines.begin();
             itM != llMachines.end();
             ++itM, ++i)
        {
            ComPtr<IMachine> pMachine = *itM;
            ComPtr<IVirtualSystemDescription> pVSD;
            CHECK_ERROR_BREAK(pMachine, ExportTo(pAppliance, Bstr(pszAbsFilePath).raw(), pVSD.asOutParam()));

            // Add additional info to the virtual system description if the user wants so
            ArgsMap *pmapArgs = NULL;
            ArgsMapsMap::iterator itm = mapArgsMapsPerVsys.find(i);
            if (itm != mapArgsMapsPerVsys.end())
                pmapArgs = &itm->second;
            if (pmapArgs)
            {
                ArgsMap::iterator itD;
                for (itD = pmapArgs->begin();
                     itD != pmapArgs->end();
                     ++itD)
                {
                    if (itD->first == "vmname")
                    {
                        //remove default value if user has specified new name (default value is set in the ExportTo())
//                      pVSD->RemoveDescriptionByType(VirtualSystemDescriptionType_Name);
                        pVSD->AddDescription(VirtualSystemDescriptionType_Name,
                                             Bstr(itD->second).raw(), NULL);
                    }
                    else if (itD->first == "product")
                        pVSD->AddDescription(VirtualSystemDescriptionType_Product,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "producturl")
                        pVSD->AddDescription(VirtualSystemDescriptionType_ProductUrl,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "vendor")
                        pVSD->AddDescription(VirtualSystemDescriptionType_Vendor,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "vendorurl")
                        pVSD->AddDescription(VirtualSystemDescriptionType_VendorUrl,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "version")
                        pVSD->AddDescription(VirtualSystemDescriptionType_Version,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "description")
                        pVSD->AddDescription(VirtualSystemDescriptionType_Description,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "eula")
                        pVSD->AddDescription(VirtualSystemDescriptionType_License,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "eulafile")
                    {
                        Utf8Str strContent;
                        void *pvFile;
                        size_t cbFile;
                        int irc = RTFileReadAll(itD->second.c_str(), &pvFile, &cbFile);
                        if (RT_SUCCESS(irc))
                        {
                            Bstr bstrContent((char*)pvFile, cbFile);
                            pVSD->AddDescription(VirtualSystemDescriptionType_License,
                                                 bstrContent.raw(), NULL);
                            RTFileReadAllFree(pvFile, cbFile);
                        }
                        else
                        {
                            RTMsgError("Cannot read license file \"%s\" which should be included in the virtual system %u.",
                                       itD->second.c_str(), i);
                            return RTEXITCODE_FAILURE;
                        }
                    }
                    /* add cloud export settings */
                    else if (itD->first == "cloudshape")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudInstanceShape,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "clouddomain")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudDomain,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "clouddisksize")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudBootDiskSize,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudbucket")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudBucket,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudocivcn")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudOCIVCN,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudpublicip")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudPublicIP,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudprivateip")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudPrivateIP,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudprofile")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudProfileName,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudocisubnet")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudOCISubnet,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudkeepobject")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudKeepObject,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudlaunchmode")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudOCILaunchMode,
                                             Bstr(itD->second).raw(), NULL);
                    else if (itD->first == "cloudlaunchinstance")
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudLaunchInstance,
                                             Bstr(itD->second).raw(), NULL);
                }
            }

            VSDList.push_back(pVSD);//store vsd for the possible second stage
        }

        if (FAILED(rc))
            break;

        /* Query required passwords and supply them to the appliance. */
        com::SafeArray<BSTR> aIdentifiers;

        CHECK_ERROR_BREAK(pAppliance, GetPasswordIds(ComSafeArrayAsOutParam(aIdentifiers)));

        if (aIdentifiers.size() > 0)
        {
            com::SafeArray<BSTR> aPasswords(aIdentifiers.size());
            RTPrintf("Enter the passwords for the following identifiers to export the apppliance:\n");
            for (unsigned idxId = 0; idxId < aIdentifiers.size(); idxId++)
            {
                com::Utf8Str strPassword;
                Bstr bstrPassword;
                Bstr bstrId = aIdentifiers[idxId];

                RTEXITCODE rcExit = readPasswordFromConsole(&strPassword, "Password ID %s:", Utf8Str(bstrId).c_str());
                if (rcExit == RTEXITCODE_FAILURE)
                {
                    RTStrFree(pszAbsFilePath);
                    return rcExit;
                }

                bstrPassword = strPassword;
                bstrPassword.detachTo(&aPasswords[idxId]);
            }

            CHECK_ERROR_BREAK(pAppliance, AddPasswords(ComSafeArrayAsInParam(aIdentifiers),
                                                       ComSafeArrayAsInParam(aPasswords)));
        }

        if (fManifest)
            options.push_back(ExportOptions_CreateManifest);

        if (fExportISOImages)
            options.push_back(ExportOptions_ExportDVDImages);

        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK(pAppliance, Write(Bstr(strOvfFormat).raw(),
                                            ComSafeArrayAsInParam(options),
                                            Bstr(pszAbsFilePath).raw(),
                                            progress.asOutParam()));
        RTStrFree(pszAbsFilePath);

        rc = showProgress(progress);
        CHECK_PROGRESS_ERROR_RET(progress, ("Appliance write failed"), RTEXITCODE_FAILURE);

        if (SUCCEEDED(rc))
            RTPrintf("Successfully exported %d machine(s).\n", llMachines.size());

        /*
         *  The second stage for the cloud case
         */
        if (actionType == CLOUD)
        {
            /* Launch the exported VM if the appropriate flag had been set on the first stage */
            for (std::list< ComPtr<IVirtualSystemDescription> >::iterator itVSD = VSDList.begin();
                 itVSD != VSDList.end();
                 ++itVSD)
            {
                ComPtr<IVirtualSystemDescription> pVSD = *itVSD;

                com::SafeArray<VirtualSystemDescriptionType_T> retTypes;
                com::SafeArray<BSTR> aRefs;
                com::SafeArray<BSTR> aOvfValues;
                com::SafeArray<BSTR> aVBoxValues;
                com::SafeArray<BSTR> aExtraConfigValues;

                CHECK_ERROR_BREAK(pVSD, GetDescriptionByType(VirtualSystemDescriptionType_CloudLaunchInstance,
                                         ComSafeArrayAsOutParam(retTypes),
                                         ComSafeArrayAsOutParam(aRefs),
                                         ComSafeArrayAsOutParam(aOvfValues),
                                         ComSafeArrayAsOutParam(aVBoxValues),
                                         ComSafeArrayAsOutParam(aExtraConfigValues)));

                Utf8Str flagCloudLaunchInstance(Bstr(aVBoxValues[0]).raw());
                retTypes.setNull(); aRefs.setNull(); aOvfValues.setNull(); aVBoxValues.setNull(); aExtraConfigValues.setNull();

                if (flagCloudLaunchInstance.equals("true"))
                {
                    /* Getting the short provider name */
                    Bstr bstrCloudProviderShortName(strOutputFile.c_str(), strOutputFile.find("://"));

                    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
                    ComPtr<ICloudProviderManager> pCloudProviderManager;
                    CHECK_ERROR_BREAK(pVirtualBox, COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()));

                    ComPtr<ICloudProvider> pCloudProvider;
                    CHECK_ERROR_BREAK(pCloudProviderManager,
                                     GetProviderByShortName(bstrCloudProviderShortName.raw(), pCloudProvider.asOutParam()));

                    CHECK_ERROR_BREAK(pVSD, GetDescriptionByType(VirtualSystemDescriptionType_CloudProfileName,
                                             ComSafeArrayAsOutParam(retTypes),
                                             ComSafeArrayAsOutParam(aRefs),
                                             ComSafeArrayAsOutParam(aOvfValues),
                                             ComSafeArrayAsOutParam(aVBoxValues),
                                             ComSafeArrayAsOutParam(aExtraConfigValues)));

                    ComPtr<ICloudProfile> pCloudProfile;
                    CHECK_ERROR_BREAK(pCloudProvider, GetProfileByName(Bstr(aVBoxValues[0]).raw(), pCloudProfile.asOutParam()));
                    retTypes.setNull(); aRefs.setNull(); aOvfValues.setNull(); aVBoxValues.setNull(); aExtraConfigValues.setNull();

                    ComObjPtr<ICloudClient> oCloudClient;
                    CHECK_ERROR_BREAK(pCloudProfile, CreateCloudClient(oCloudClient.asOutParam()));
                    RTPrintf("Creating a cloud instance...\n");

                    ComPtr<IProgress> progress1;
                    CHECK_ERROR_BREAK(oCloudClient, LaunchVM(pVSD, progress1.asOutParam()));
                    rc = showProgress(progress1);
                    CHECK_PROGRESS_ERROR_RET(progress1, ("Creating the cloud instance failed"), RTEXITCODE_FAILURE);

                    if (SUCCEEDED(rc))
                    {
                        CHECK_ERROR_BREAK(pVSD, GetDescriptionByType(VirtualSystemDescriptionType_CloudInstanceId,
                                                 ComSafeArrayAsOutParam(retTypes),
                                                 ComSafeArrayAsOutParam(aRefs),
                                                 ComSafeArrayAsOutParam(aOvfValues),
                                                 ComSafeArrayAsOutParam(aVBoxValues),
                                                 ComSafeArrayAsOutParam(aExtraConfigValues)));

                        RTPrintf("A cloud instance with id '%s' (provider '%s') was created\n",
                                 Utf8Str(Bstr(aVBoxValues[0]).raw()).c_str(),
                                 Utf8Str(bstrCloudProviderShortName.raw()).c_str());
                        retTypes.setNull(); aRefs.setNull(); aOvfValues.setNull(); aVBoxValues.setNull(); aExtraConfigValues.setNull();
                    }
                }
            }
        }
    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


RTEXITCODE handleSignAppliance(HandlerArg *arg)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--private-key-form",         'k', RTGETOPT_REQ_STRING },
        { "--private-key-password",     'p', RTGETOPT_REQ_STRING },
        { "--private-key-password-file",'f', RTGETOPT_REQ_STRING },
        { "--cert-file",                'c', RTGETOPT_REQ_STRING },
        { "--intermediate-cert-file",   'i', RTGETOPT_REQ_STRING },
        { "--out-cert",                 'o', RTGETOPT_REQ_NOTHING },
        { "--pkcs7",                    's', RTGETOPT_REQ_NOTHING },
        { "--no-pkcs7",                 'S', RTGETOPT_REQ_NOTHING },
        { "--force",                    'F', RTGETOPT_REQ_NOTHING },
        { "--dry-run",                  'd', RTGETOPT_REQ_NOTHING },
        { "help",                       1001, RTGETOPT_REQ_NOTHING },
        { "--help",                     1002, RTGETOPT_REQ_NOTHING }
    };

    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;

    int rc = RTGetOptInit(&GetState, arg->argc, arg->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    if (arg->argc == 1)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strOvfFilename;
    Utf8Str strPrivateKeyForm;
    Utf8Str strPrivateKeyPassword;
    Utf8Str strPrivateKeyPasswordFile;
    Utf8Str strX509CertificateFile;
    Utf8Str strInterimCertificateFile;
    bool    fOutCert = false; // the default
    bool    fPKCS7 = false; // the default
    bool    fResign = false; // the default
    bool    fDry = false; // the default
    com::SafeArray<BSTR>  parameters;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'k':
                strPrivateKeyForm=ValueUnion.psz;
                break;

            case 'p':
                strPrivateKeyPassword=ValueUnion.psz;
                break;

            case 'f':
                strPrivateKeyPasswordFile=ValueUnion.psz;
                break;

            case 'c':
                strX509CertificateFile=ValueUnion.psz;
                break;

            case 'i':
                strInterimCertificateFile=ValueUnion.psz;
                break;

            case 'o':
                fOutCert = true;
                break;

            case 's':
                fPKCS7 = true;
                break;

            case 'F':
                fResign = true;
                break;

            case 'd':
                fDry = true;
                break;

            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
                if (strOvfFilename.isEmpty())
                    strOvfFilename = ValueUnion.psz;
                else
                    return errorGetOpt(c, &ValueUnion);
                break;

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    Utf8Str strManifestData;
    Utf8Str strManifestName;
    Utf8Str strAppliancePath;
    Utf8Str strApplianceFullPath;

    do
    {
        ComPtr<IAppliance> pAppliance;
        CHECK_ERROR_BREAK(arg->virtualBox, CreateAppliance(pAppliance.asOutParam()));

        char *pszAbsFilePath = RTPathAbsDup(strOvfFilename.c_str());

        ComPtr<IProgress> progressRead;
        CHECK_ERROR_BREAK(pAppliance, Read(Bstr(pszAbsFilePath).raw(), progressRead.asOutParam()));
        RTStrFree(pszAbsFilePath);

        hrc = showProgress(progressRead);
        CHECK_PROGRESS_ERROR_RET(progressRead, ("Appliance read failed"), RTEXITCODE_FAILURE);

        /* fetch the path, there is stuff like username/password removed if any */
        Bstr path;
        CHECK_ERROR_BREAK(pAppliance, COMGETTER(Path)(path.asOutParam()));

        strAppliancePath = path;
        strApplianceFullPath = strAppliancePath;
        strAppliancePath.stripFilename();

        RTPrintf("The original OVA package folder is %s\n\n", strAppliancePath.c_str());

        /* fetch the manifest */
        Bstr manifest;
        Bstr manifestName;
        CHECK_ERROR_BREAK(pAppliance, GetManifest(manifest.asOutParam(), manifestName.asOutParam()));

        strManifestData = manifest;
        strManifestName = manifestName;

        if (strManifestName.isEmpty() || strManifestData.isEmpty())
        {
            RTPrintf("Manifest file wasn't found in the OVA package %s\n\n", strApplianceFullPath.c_str());
            hrc = E_FAIL;
        }

    }while (0);

    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;


    /* Read the private key */
    RTCRKEY hPrivateKey;
    RTERRINFO ErrInfo;
    uint32_t fFlags = 0;

    if (strPrivateKeyForm.equalsIgnoreCase("pem"))
        fFlags = RTCRPEMREADFILE_F_VALID_MASK;//|RTCRKEYFROM_F_VALID_MASK;

    /* check the key file existence */
    if (!RTFileExists(strPrivateKeyPasswordFile.c_str()))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The file %s with a private key wasn't found",
                              strPrivateKeyPasswordFile.c_str());

    rc = RTCrKeyCreateFromFile(&hPrivateKey, 0, strPrivateKeyPasswordFile.c_str(), strPrivateKeyPassword.c_str(), &ErrInfo);
    if (RT_SUCCESS(rc))
    {
        RTPrintf("Reading the private key from %s was done.\n\n", strPrivateKeyPasswordFile.c_str());

        /* check the certificate file existence */
        if (!RTFileExists(strX509CertificateFile.c_str()))
        {
            RTCrKeyRelease(hPrivateKey);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "The file %s with a X509 certificate wasn't found",
                                  strX509CertificateFile.c_str());
        }

        /* Read the certificate */
        RTCRX509CERTIFICATE     Certificate;
        rc = RTCrX509Certificate_ReadFromFile(&Certificate, strX509CertificateFile.c_str(), 0, &g_RTAsn1DefaultAllocator,
                                              &ErrInfo);
        if (RT_FAILURE(rc))
        {
            RTCrKeyRelease(hPrivateKey);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error reading certificate from %s: %Rrc - %s",
                                  strX509CertificateFile.c_str(), rc, ErrInfo.pszMsg);
        }

        RTPrintf("Reading the certificate from %s was done.\n\n", strX509CertificateFile.c_str());

        /*
         * Get the manifest from Appliance and sign it.
         * We have the private key, the password, the certificate.
         * Also it's needed a digist algorithm SHA1/SHA256/SHA512.
         * OVF2.0 standard proposes to use SHA256.
         */

        RTDIGESTTYPE digestType = RTDIGESTTYPE_SHA256;
        RTMANIFEST hManifest;
        RTCRDIGEST hDigest;

        rc = RTManifestCreate(0 /*fFlags*/, &hManifest);
        if (RT_FAILURE(rc))
        {
            RTCrKeyRelease(hPrivateKey);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error manifest creation: %Rrc", rc);
        }

        /* Calc the digest of the manifest using the algorithm found above. */
        rc = RTCrDigestCreateByType(&hDigest, digestType);
        if (RT_SUCCESS(rc))
        {
            rc = RTCrDigestUpdate(hDigest, strManifestData.c_str(), strManifestData.length());
            if (RT_SUCCESS(rc))
            {
                uint8_t const * bHash =  RTCrDigestGetHash(hDigest);

                char szDigest[_4K];
                rc = RTSha256ToString(bHash, szDigest, sizeof(szDigest));
                RTPrintf("The digest of manifest is:\n%s\n\n", szDigest);

                PCRTASN1DYNTYPE pParameters = NULL;

                char   signatureBuf[_16K];
                size_t cbSignature = sizeof(signatureBuf);
                rc= RTCrPkixPubKeySignDigest(&Certificate.SignatureAlgorithm.Algorithm,
                                              hPrivateKey,
                                              pParameters,
                                              hDigest,
                                              0,
                                              signatureBuf,
                                              &cbSignature,
                                              &ErrInfo);
                if (RT_SUCCESS(rc))
                {
                    char szSignature[_4K];
                    RTStrPrintHexBytes(szSignature, sizeof(szSignature), signatureBuf, cbSignature, 0 /*fFlags*/);

                    /* Verify the signature back using the public key information from the certificate */
                    rc = RTCrPkixPubKeyVerifySignedDigestByCertPubKeyInfo(&Certificate.TbsCertificate.SubjectPublicKeyInfo,
                                                                          signatureBuf, cbSignature, hDigest, &ErrInfo);
                    if (RT_FAILURE(rc))
                    {
                        /* Dont' forget */
                        RTCrDigestRelease(hDigest);
                        RTCrKeyRelease(hPrivateKey);
                        if (rc == VERR_CR_PKIX_SIGNATURE_MISMATCH)
                            return RTMsgErrorExit(RTEXITCODE_FAILURE, "The manifest signature does not match");

                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Error validating the manifest signature (%Rrc, %s)",
                                              rc, ErrInfo.pszMsg);
                    }
                    else
                        RTPrintf("The manifest signature was validated successfully\n\n");

                    /*
                     * Preparing the digest signature according to OVF2.0 standard.
                     * Only SHA1 and SHA256 are supported for now.
                     */
                    Utf8Str strDigestSignature;

                    switch (digestType)
                    {
                        case RTDIGESTTYPE_SHA1:
                            strDigestSignature.append("SHA1");
                            break;
                        case RTDIGESTTYPE_SHA256:
                        default:
                            strDigestSignature.append("SHA256");
                            break;
                    }

                    strDigestSignature.append('(').append(strManifestName).append(')').append(" = ");
                    strDigestSignature.append(szSignature);

                    /*
                     * Especially add a new line character to avoid placing the certificate on the same line
                     * with the digest signature.
                     */
                    strDigestSignature.append('\n');

                    RTPrintf("The signed digest is:\n%s\n", strDigestSignature.c_str());

                    /* Just stop here in the case of dry-run scenario */
                    if (fDry)
                    {
                        /* Dont' forget */
                        RTCrDigestRelease(hDigest);
                        RTCrKeyRelease(hPrivateKey);
                        return RTEXITCODE_SUCCESS;
                    }

                    /* Make up the certificate name */
                    const char *pszSuffix = strrchr(strManifestName.c_str(), '.');
                    Utf8Str strOVFCertificateName = Utf8Str(strManifestName.c_str(), pszSuffix - strManifestName.c_str());
                    strOVFCertificateName.append(".cert");

                    /* Create a memory I/O stream and write the digest signature and the certificate to it. */
                    RTVFSIOSTREAM hVfsIosOVFCertificate;

                    rc = RTVfsMemIoStrmCreate(NIL_RTVFSIOSTREAM, _1K, &hVfsIosOVFCertificate);
                    if (RT_SUCCESS(rc))
                    {
                        size_t cbWritten = 0;
                        rc = RTVfsIoStrmWrite(hVfsIosOVFCertificate, strDigestSignature.c_str(), strDigestSignature.length(),
                                              true /*fBlocking*/, &cbWritten);
                        if (RT_SUCCESS(rc))
                        {
                            Utf8Str strX509CertificateContent;
                            /* Open and read the passed certificate file as a standard file */
                            RTVFSFILE     hVfsOriginalX509Certificate;
                            rc = RTVfsFileOpenNormal(strX509CertificateFile.c_str(),
                                                     RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE,
                                                     &hVfsOriginalX509Certificate);
                            if (RT_SUCCESS(rc))
                            {
                                for (;;)
                                {
                                    char   abBuf[_4K];
                                    size_t cbRead;
                                    rc = RTVfsFileRead(hVfsOriginalX509Certificate, abBuf, sizeof(abBuf), &cbRead);
                                    if (RT_SUCCESS(rc))
                                    {
                                        bool const fEof = rc == VINF_EOF;
                                        strX509CertificateContent.append(abBuf, cbRead);
                                        if (fEof)
                                            break;
                                    }
                                    else
                                        break;
                                }
                            }
                            else
                                RTPrintf("Reading the certificate from the file %s failed (%Rrc)",
                                         strX509CertificateFile.c_str(), rc);

                            /* Dont' forget */
                            RTVfsFileRelease(hVfsOriginalX509Certificate);

                            if (RT_SUCCESS(rc))
                            {
                                cbWritten = 0;
                                /* Write out the certificate into the stream */
                                rc = RTVfsIoStrmWrite(hVfsIosOVFCertificate, strX509CertificateContent.c_str(),
                                                      strX509CertificateContent.length(), true /*fBlocking*/, &cbWritten);

                                if (RT_FAILURE(rc))
                                    RTPrintf("RTVfsIoStrmWrite failed on adding the certificate into the stream (%Rrc)", rc);
                            }
                            else
                                RTPrintf("Reading the certificate from the file %s failed (%Rrc)",
                                         strX509CertificateFile.c_str(), rc);
                        }
                        else
                            RTPrintf("RTVfsIoStrmWrite failed  on adding the digest into the stream (%Rrc)", rc);
                    }
                    else
                        RTPrintf("RTVfsMemIoStrmCreate failed (%Rrc)", rc);

                    if (RT_FAILURE(rc))
                    {
                        /* Dont' forget */
                        RTVfsIoStrmRelease(hVfsIosOVFCertificate);
                        return RTEXITCODE_FAILURE;
                    }


                    /* Make up new appliance name */
                    const char *pszSuffix1 = strrchr(strManifestName.c_str(), '.');
                    Utf8Str strSignedOVAName(strAppliancePath);
                    strSignedOVAName.append(RTPATH_DELIMITER).append(strManifestName.c_str(),
                                                                     pszSuffix1 - strManifestName.c_str());
                    strSignedOVAName.append("-signed.ova");

                    RTPrintf("The path for new OVA signed package is '%s'\n\n", strSignedOVAName.c_str());

                    /*
                     * Open new OVA file (it's a standard TAR file) as a file stream
                     */
                    RTVFSIOSTREAM hVfsIosSignedOVAPackage;
                    rc = RTVfsIoStrmOpenNormal(strSignedOVAName.c_str(),
                                               RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE,
                                               &hVfsIosSignedOVAPackage);
                    if (RT_SUCCESS(rc))
                    {
                        RTVFSFSSTREAM hVfsFssOVADest;
                        rc = RTZipTarFsStreamToIoStream(hVfsIosSignedOVAPackage, RTZIPTARFORMAT_USTAR,
                                                        0 /*fFlags*/, &hVfsFssOVADest);
                        RTVfsIoStrmRelease(hVfsIosSignedOVAPackage);

                        if (RT_SUCCESS(rc))
                        {
                            bool fCertPresence = false;
                            RTZipTarFsStreamSetFileMode(hVfsFssOVADest, 0660, 0440);

                            /*
                             * Open the original OVA file (it's a standard TAR file) as a file stream
                             */
                            RTVFSIOSTREAM hVfsIosOVASrc;
                            rc = RTVfsIoStrmOpenNormal(strApplianceFullPath.c_str(),
                                                       RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                                       &hVfsIosOVASrc);
                            if (RT_SUCCESS(rc))
                            {
                                RTVFSFSSTREAM hVfsFssOVASrc;
                                rc = RTZipTarFsStreamFromIoStream(hVfsIosOVASrc, 0 /*fFlags*/, &hVfsFssOVASrc);
                                /* Dont' forget */
                                RTVfsIoStrmRelease(hVfsIosOVASrc);

                                if (RT_SUCCESS(rc))
                                {
                                    for (;;)
                                    {
                                        char *pszName = NULL;
                                        RTVFSOBJTYPE enmType;
                                        RTVFSOBJ     hVfsObj;
                                        hrc = S_OK;
                                        rc = RTVfsFsStrmNext(hVfsFssOVASrc, &pszName, &enmType, &hVfsObj);
                                        if (RT_FAILURE(rc))
                                        {
                                            if (rc != VERR_EOF)
                                                RTPrintf("Error reading the OVA file '%s' (%Rrc)",
                                                         strApplianceFullPath.c_str(), rc);
                                            else
                                                rc = VINF_SUCCESS;

                                            break;
                                        }

                                        /*
                                         * in the case when the certificate has been already presented in the OVA package
                                         */
                                        if (strOVFCertificateName.equals(pszName))
                                        {
                                            RTPrintf("Some certificate has already presented in the OVA package\n\n");
                                            fCertPresence = true;//remember for later usage
                                            /* if the flag --force has been set just skip it and go further */
                                            if (fResign)
                                                continue;
                                        }

                                        /** todo: some progress object should be added here to display the action progress */
                                        /* Read the input stream and add the content into the output stream */
                                        rc = RTVfsFsStrmAdd(hVfsFssOVADest, pszName, hVfsObj, 0 /*fFlags*/);
                                        if (RT_FAILURE(rc))
                                            RTPrintf("RTVfsFsStrmAdd failed for the %s (%Rrc)", pszName, rc);

                                        /* Free resources */
                                        RTVfsObjRelease(hVfsObj);
                                        RTStrFree(pszName);

                                        if (RT_FAILURE(rc))
                                        {
                                            RTPrintf("Error writing to new OVA package '%s' (%Rrc)",
                                                     strSignedOVAName.c_str(), rc);
                                            break;
                                        }
                                    }

                                    /* Dont' forget */
                                    RTVfsFsStrmRelease(hVfsFssOVASrc);
                                }
                                else
                                    RTPrintf("Error reading the OVA file '%s' (%Rrc)", strApplianceFullPath.c_str(), rc);
                            }
                            else
                                RTPrintf("Error opening the OVA file '%s' (%Rrc)", strApplianceFullPath.c_str(), rc);

                            /*
                             * Now add the digest signature into new OVA package
                             */
                            if (RT_SUCCESS(rc))
                            {
                                /* Add only if no cetificate or the flag fResign was set and certificate is presented */
                                if ( !fCertPresence || (fCertPresence && fResign) )
                                {
                                    size_t cbWritten;
                                    size_t cbRead;
                                    RTFOFF off = RTVfsIoStrmTell(hVfsIosOVFCertificate);
                                    void *pvBuf = RTMemAlloc(off);
                                    size_t cbBuf = off;

                                    rc = RTVfsIoStrmReadAt(hVfsIosOVFCertificate, 0, pvBuf, cbBuf,
                                                           true /*fBlocking*/, &cbRead);
                                    if (RT_SUCCESS(rc))
                                    {
                                        RTVFSIOSTREAM hVfsIosSrc;
                                        rc = RTVfsIoStrmFromBuffer(RTFILE_O_READ, pvBuf, cbRead, &hVfsIosSrc);
                                        RTVFSOBJ hVfsObjCert = RTVfsObjFromIoStream(hVfsIosSrc);
                                        RTVfsIoStrmRelease(hVfsIosSrc);

                                        RTZipTarFsStreamSetOwner(hVfsFssOVADest, VBOX_VERSION_MAJOR, "vboxovf20");
                                        RTZipTarFsStreamSetGroup(hVfsFssOVADest, VBOX_VERSION_MINOR,
                                                                 "vbox_v" RT_XSTR(VBOX_VERSION_MAJOR) "."
                                                                 RT_XSTR(VBOX_VERSION_MINOR) "."
                                                                 RT_XSTR(VBOX_VERSION_BUILD) "r"
                                                                 RT_XSTR(VBOX_SVN_REV) "\0");

                                        /* Write out the certificate into the stream */
                                        rc = RTVfsFsStrmAdd(hVfsFssOVADest, strOVFCertificateName.c_str(),
                                                            hVfsObjCert, 0 /*fFlags*/);
                                        if (RT_FAILURE(rc))
                                            RTPrintf("RTVfsFsStrmAdd failed for the %s (%Rrc)",
                                                     strOVFCertificateName.c_str(), rc);

                                        /* Dont' forget */
                                        RTVfsObjRelease(hVfsObjCert);

                                        /* Save the OVA certificate file next to the original OVA package */
                                        if (fOutCert)
                                        {
                                            Utf8Str strCertificateFileFullPath(strAppliancePath);
                                            strCertificateFileFullPath.append(RTPATH_DELIMITER).
                                                                       append(strOVFCertificateName.c_str());

                                            RTVFSIOSTREAM hVfsIosCertFile;
                                            rc = RTVfsIoStrmOpenNormal(strCertificateFileFullPath.c_str(),
                                                                       RTFILE_O_CREATE_REPLACE |
                                                                       RTFILE_O_WRITE |
                                                                       RTFILE_O_DENY_NONE,
                                                                       &hVfsIosCertFile);
                                            if (RT_SUCCESS(rc))
                                            {
                                                /* Write out the certificate into the file */
                                                rc = RTVfsIoStrmWrite(hVfsIosCertFile, pvBuf,
                                                                      cbBuf, true /*fBlocking*/, &cbWritten);
                                                if (RT_FAILURE(rc))
                                                    RTPrintf("Error writing the certificate file '%s' (%Rrc)",
                                                             strCertificateFileFullPath.c_str(), rc);

                                                /* Dont' forget */
                                                RTVfsIoStrmFlush(hVfsIosCertFile);
                                                RTVfsIoStrmRelease(hVfsIosCertFile);
                                            }
                                            else
                                                RTPrintf("Error opening the certificate file '%s' (%Rrc)",
                                                         strCertificateFileFullPath.c_str(), rc);
                                        }
                                    }
                                    else
                                        RTPrintf("Error writing the certificate file '%s' (%Rrc)",
                                                 strOVFCertificateName.c_str(), rc);

                                    /* Dont' forget */
                                    RTMemFree(pvBuf);
                                }
                            }

                            /* Dont' forget */
                            RTVfsFsStrmRelease(hVfsFssOVADest);
                        }
                        else
                            RTPrintf("Failed create TAR creator for '%s' (%Rrc)", strSignedOVAName.c_str(), rc);
                    }
                    else
                        RTPrintf("Error opening new OVA signed package '%s'\n", strSignedOVAName.c_str());

                    /* Dont' forget */
                     RTVfsIoStrmRelease(hVfsIosOVFCertificate);
                }
                else
                    RTPrintf("Error signing the digest of manifest: %Rrc", rc);
            }
            else
                RTPrintf("Error updating the digest: %Rrc", rc);
        }
        else
            RTPrintf("Error digest creation: %Rrc", rc);

        /* Don't forget */
        RTCrDigestRelease(hDigest);
        RTCrKeyRelease(hPrivateKey);
    }
    else
        RTPrintf("Error reading the private key from %s: %Rrc - %s", strPrivateKeyPasswordFile.c_str(), rc, ErrInfo.pszMsg);

    /* Dont' forget */
    if (RT_SUCCESS(rc))
        hrc = S_OK;

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */
