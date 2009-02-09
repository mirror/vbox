/* $Id$ */
/** @file
 * VBoxManage - The appliance-related commands.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <list>
#include <map>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/stream.h>

#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////

typedef std::map<Utf8Str, Utf8Str> ArgsMap;         // pairs of strings like "-vmname" => "newvmname"
typedef std::map<uint32_t, ArgsMap> ArgsMapsMap;   // map of maps, one for each virtual system, sorted by index

static bool findArgValue(Utf8Str &strOut,
                         const ArgsMap *pmapArgs,
                         const Utf8Str &strKey)
{
    if (pmapArgs)
    {
        ArgsMap::const_iterator it;
        it = pmapArgs->find(strKey);
        if (it != pmapArgs->end())
        {
            strOut = it->second;
            return true;
        }
    }

    return false;
}

int handleImportAppliance(HandlerArg *a)
{
    HRESULT rc = S_OK;

    Utf8Str strOvfFilename;
    bool fExecute = false;                  // if true, then we actually do the import (-exec argument)

    uint32_t ulCurVsys = (uint32_t)-1;

    // for each -vsys X command, maintain a map of command line items
    // (we'll parse them later after interpreting the OVF, when we can
    // actually check whether they make sense semantically)
    ArgsMapsMap mapArgsMapsPerVsys;

    for (int i = 0;
         i < a->argc;
         ++i)
    {
        Utf8Str strThisArg(a->argv[i]);
        if (strThisArg == "-exec")
            fExecute = true;
        else if (strThisArg == "-vsys")
        {
            if (++i < a->argc)
            {
                uint32_t ulVsys;
                if (VINF_SUCCESS == (rc = Utf8Str(a->argv[i]).toInt(ulVsys)))       // don't use SUCCESS() macro, fail even on warnings
                    ulCurVsys = ulVsys;
                else
                    return errorSyntax(USAGE_IMPORTAPPLIANCE, "Argument to -vsys option must be a non-negative number.");
            }
            else
                return errorSyntax(USAGE_IMPORTAPPLIANCE, "Missing argument to -vsys option.");
        }
        else if (    (strThisArg == "-ostype")
                  || (strThisArg == "-vmname")
                  || (strThisArg == "-memory")
                  || (strThisArg == "-ignore")
                  || (strThisArg.substr(0, 5) == "-type")
                  || (strThisArg.substr(0, 11) == "-controller")
                )
        {
            if (ulCurVsys == (uint32_t)-1)
                return errorSyntax(USAGE_IMPORTAPPLIANCE, "Option \"%s\" requires preceding -vsys argument.", strThisArg.c_str());

            // store both this arg and the next one in the strings map for later parsing
            if (++i < a->argc)
                mapArgsMapsPerVsys[ulCurVsys][strThisArg] = Utf8Str(a->argv[i]);
            else
                return errorSyntax(USAGE_IMPORTAPPLIANCE, "Missing argument to \"%s\" option.", strThisArg.c_str());
        }
        else if (strThisArg[0] == '-')
            return errorSyntax(USAGE_IMPORTAPPLIANCE, "Unknown option \"%s\".", strThisArg.c_str());
        else if (!strOvfFilename)
            strOvfFilename = strThisArg;
        else
            return errorSyntax(USAGE_IMPORTAPPLIANCE, "Too many arguments for \"import\" command.");
    }

    if (!strOvfFilename)
        return errorSyntax(USAGE_IMPORTAPPLIANCE, "Not enough arguments for \"import\" command.");

    do
    {
        Bstr bstrOvfFilename(strOvfFilename);
        ComPtr<IAppliance> appliance;
        CHECK_ERROR_BREAK(a->virtualBox, OpenAppliance(bstrOvfFilename, appliance.asOutParam()));

        RTPrintf("Interpreting %s... ", strOvfFilename.c_str());
        CHECK_ERROR_BREAK(appliance, Interpret());
        RTPrintf("OK.\n");

        // fetch all disks
        com::SafeArray<BSTR> retDisks;
        CHECK_ERROR_BREAK(appliance,
                          COMGETTER(Disks)(ComSafeArrayAsOutParam(retDisks)));
        if (retDisks.size() > 0)
        {
            RTPrintf("Disks:");
            for (unsigned i = 0; i < retDisks.size(); i++)
                RTPrintf("  %ls", retDisks[i]);
            RTPrintf("\n");
        }

        // fetch virtual system descriptions
        com::SafeIfaceArray<IVirtualSystemDescription> aVirtualSystemDescriptions;
        CHECK_ERROR_BREAK(appliance,
                          COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(aVirtualSystemDescriptions)));

        uint32_t cVirtualSystemDescriptions = aVirtualSystemDescriptions.size();

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
                                   "Invalid index %RI32 with -vsys option; the OVF contains only %RI32 virtual system(s).",
                                   ulVsys, cVirtualSystemDescriptions);
        }

        // dump virtual system descriptions and match command-line arguments
        if (cVirtualSystemDescriptions > 0)
        {
            for (unsigned i = 0; i < cVirtualSystemDescriptions; ++i)
            {
                com::SafeArray<VirtualSystemDescriptionType_T> retTypes;
                com::SafeArray<BSTR> aRefs;
                com::SafeArray<BSTR> aOrigValues;
                com::SafeArray<BSTR> aConfigValues;
                com::SafeArray<BSTR> aExtraConfigValues;
                CHECK_ERROR_BREAK(aVirtualSystemDescriptions[i],
                                  GetDescription(ComSafeArrayAsOutParam(retTypes),
                                                 ComSafeArrayAsOutParam(aRefs),
                                                 ComSafeArrayAsOutParam(aOrigValues),
                                                 ComSafeArrayAsOutParam(aConfigValues),
                                                 ComSafeArrayAsOutParam(aExtraConfigValues)));

                RTPrintf("Virtual system %i:\n", i);

                // look up the corresponding command line options, if any
                const ArgsMap *pmapArgs = NULL;
                ArgsMapsMap::const_iterator itm = mapArgsMapsPerVsys.find(i);
                if (itm != mapArgsMapsPerVsys.end())
                    pmapArgs = &itm->second;

//                     ArgsMap::const_iterator it3;
//                     for (it3 = pmapArgs->begin();
//                          it3 != pmapArgs->end();
//                          ++it3)
//                     {
//                         RTPrintf("%s -> %s\n", it3->first.c_str(), it3->second.c_str());
//                     }
//                 }

//                 Bstr bstrVMName;
//                 Bstr bstrOSType;

                // this collects the final values for setFinalValues()
                com::SafeArray<BOOL> aEnabled(retTypes.size());
                com::SafeArray<BSTR> aFinalValues(retTypes.size());

                for (unsigned a = 0; a < retTypes.size(); ++a)
                {
                    VirtualSystemDescriptionType_T t = retTypes[a];

                    Utf8Str strOverride;

                    Bstr bstrFinalValue = aConfigValues[a];

                    switch (t)
                    {
                        case VirtualSystemDescriptionType_Name:
                            if (findArgValue(strOverride, pmapArgs, "-vmname"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2d: VM name specified with -vmname: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2d: Suggested VM name \"%ls\""
                                        "\n    (change with \"-vsys %d -vmname <name>\")\n",
                                        a, bstrFinalValue.raw(), i);
                        break;

                        case VirtualSystemDescriptionType_OS:
                            if (findArgValue(strOverride, pmapArgs, "-ostype"))
                            {
                                bstrFinalValue = strOverride;
                                RTPrintf("%2d: OS type specified with -ostype: \"%ls\"\n",
                                        a, bstrFinalValue.raw());
                            }
                            else
                                RTPrintf("%2d: Suggested OS type: \"%ls\""
                                        "\n    (change with \"-vsys %d -ostype <type>\"; use \"list ostypes\" to list all)\n",
                                        a, bstrFinalValue.raw(), i);
                        break;

                        case VirtualSystemDescriptionType_CPU:
                            RTPrintf("%2d: Number of CPUs (ignored): %ls\n",
                                     a, aConfigValues[a]);
                        break;

                        case VirtualSystemDescriptionType_Memory:
                        {
                            if (findArgValue(strOverride, pmapArgs, "-memory"))
                            {
                                uint32_t ulMemMB;
                                if (VINF_SUCCESS == strOverride.toInt(ulMemMB))
                                {
                                    bstrFinalValue = strOverride;
                                    RTPrintf("%2d: Guest memory specified with -memory: %ls MB\n",
                                             a, bstrFinalValue.raw());
                                }
                                else
                                    return errorSyntax(USAGE_IMPORTAPPLIANCE,
                                                       "Argument to -memory option must be a non-negative number.");
                            }
                            else
                                RTPrintf("%2d: Guest memory: %ls MB\n    (change with \"-vsys %d -memory <MB>\")\n",
                                         a, bstrFinalValue.raw(), i);
                        }
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerIDE:
                            RTPrintf("%2d: IDE controller, type %ls"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a,
                                     aConfigValues[a],
                                     i, a);
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerSATA:
                            RTPrintf("%2d: SATA controller, type %ls"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a,
                                     aConfigValues[a],
                                     i, a);
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                            RTPrintf("%2d: SCSI controller, type %ls"
                                     "\n    (change with \"-vsys %d -type%d={BusLogic|LsiLogic}\";"
                                     "\n    disable with \"-vsys %d -ignore %d\")\n",
                                     a,
                                     aConfigValues[a],
                                     i, a, i, a);
                        break;

                        case VirtualSystemDescriptionType_HardDiskImage:
                            RTPrintf("%2d: Hard disk image: source image=%ls, target path=%ls, %ls"
                                     "\n    (change controller with \"-vsys %d -controller%d=<id>\";"
                                     "\n    disable with \"-vsys %d -ignore %d\")\n",
                                     a,
                                     aOrigValues[a],
                                     aConfigValues[a],
                                     aExtraConfigValues[a],
                                     i, a, i, a);
                        break;

                        case VirtualSystemDescriptionType_CDROM:
                            RTPrintf("%2d: CD-ROM"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a, i, a);
                        break;

                        case VirtualSystemDescriptionType_Floppy:
                            RTPrintf("%2d: Floppy"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a, i, a);
                        break;

                        case VirtualSystemDescriptionType_NetworkAdapter:
                            RTPrintf("%2d: Network adapter: orig %ls, config %ls, extra %ls\n",
                                     a,
                                     aOrigValues[a],
                                     aConfigValues[a],
                                     aExtraConfigValues[a]);
                        break;

                        case VirtualSystemDescriptionType_USBController:
                            RTPrintf("%2d: USB controller"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a, i, a);
                        break;

                        case VirtualSystemDescriptionType_SoundCard:
                            RTPrintf("%2d: Sound card (appliance expects \"%ls\", can change on import)"
                                     "\n    (disable with \"-vsys %d -ignore %d\")\n",
                                     a,
                                     aOrigValues[a],
                                     i,
                                     a);
                        break;
                    }

                    bstrFinalValue.detachTo(&aFinalValues[a]);
                }

                if (fExecute)
                    CHECK_ERROR_BREAK(aVirtualSystemDescriptions[i],
                                      SetFinalValues(ComSafeArrayAsInParam(aEnabled),
                                                     ComSafeArrayAsInParam(aFinalValues)));

            } // for (unsigned i = 0; i < cVirtualSystemDescriptions; ++i)

            if (fExecute)
            {
                ComPtr<IProgress> progress;
                CHECK_ERROR_BREAK(appliance,
                                  ImportAppliance(progress.asOutParam()));

                showProgress(progress);

                if (SUCCEEDED(rc))
                    progress->COMGETTER(ResultCode)(&rc);

                if (FAILED(rc))
                {
                    com::ProgressErrorInfo info(progress);
                    com::GluePrintErrorInfo(info);
                    com::GluePrintErrorContext("ImportAppliance", __FILE__, __LINE__);
                }
                else
                    RTPrintf("Successfully imported the appliance.\n");
            }
        } // end if (aVirtualSystemDescriptions.size() > 0)
    } while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
