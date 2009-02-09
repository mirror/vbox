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
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/stream.h>

#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////

int handleImportAppliance(HandlerArg *a)
{
    HRESULT rc = S_OK;

    Utf8Str strOvfFilename;
    bool fExecute = false;                  // if true, then we actually do the import (-exec argument)

    for (int i = 0; i < a->argc; i++)
    {
        if (!strcmp(a->argv[i], "-exec"))
            fExecute = true;
        else if (!strOvfFilename)
            strOvfFilename = a->argv[i];
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

        RTPrintf("Interpreting...\n");
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
        if (aVirtualSystemDescriptions.size() > 0)
        {
            for (unsigned i = 0; i < aVirtualSystemDescriptions.size(); ++i)
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
                for (unsigned a = 0; a < retTypes.size(); ++a)
                {
                    VirtualSystemDescriptionType_T t = retTypes[a];

                    Bstr bstrVMname;
                    Bstr bstrOstype;
                    uint32_t ulMemMB;
                    bool fUSB = false;

                    switch (t)
                    {
                        case VirtualSystemDescriptionType_Name:
                            bstrVMname = aConfigValues[a];
                            RTPrintf("%2d: Suggested VM name \"%ls\""
                                     "\n    (change with \"-vsys %d -vmname <name>\")\n",
                                     a, bstrVMname.raw(), i);
                        break;

                        case VirtualSystemDescriptionType_OS:
                            bstrOstype = aConfigValues[a];
                            RTPrintf("%2d: Suggested OS type: \"%ls\""
                                     "\n    (change with \"-vsys %d -ostype <type>\"; use \"list ostypes\" to list all)\n",
                                     a, bstrOstype.raw(), i);
                        break;

                        case VirtualSystemDescriptionType_CPU:
                            RTPrintf("%2d: Number of CPUs (ignored): %ls\n",
                                     a, aConfigValues[a]);
                        break;

                        case VirtualSystemDescriptionType_Memory:
                            Utf8Str(Bstr(aConfigValues[a])).toInt(ulMemMB);
                            RTPrintf("%2d: Guest memory: %u MB\n    (change with \"-vsys %d -memory <MB>\")\n",
                                     a, ulMemMB, i);
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
                }
            }

            if (fExecute)
            {
                ComPtr<IProgress> progress;
                CHECK_ERROR_BREAK(appliance,
                                  ImportAppliance(progress.asOutParam()));

                showProgress(progress);

                if (SUCCEEDED(rc))
                {
                    progress->COMGETTER(ResultCode)(&rc);
                    if (FAILED (rc))
                    {
                        com::ProgressErrorInfo info(progress);
                        if (info.isBasicAvailable())
                            RTPrintf("Error: failed to import appliance. Error message: %lS\n", info.getText().raw());
                        else
                            RTPrintf("Error: failed to import appliance. No error message available!\n");
                    }
                    else
                        RTPrintf("Successfully imported the appliance.\n");
                }
            }
        } // end if (aVirtualSystemDescriptions.size() > 0)
    } while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
