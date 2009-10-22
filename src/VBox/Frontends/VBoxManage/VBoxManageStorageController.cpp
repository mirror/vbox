/* $Id$ */
/** @file
 * VBoxManage - The storage controller related commands.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static const RTGETOPTDEF g_aStorageAttachOptions[] =
{
    { "--storagectl",     's', RTGETOPT_REQ_STRING },
    { "--port",           'p', RTGETOPT_REQ_UINT32 },
    { "--device",         'd', RTGETOPT_REQ_UINT32 },
    { "--medium",         'm', RTGETOPT_REQ_STRING },
    { "--type",           't', RTGETOPT_REQ_STRING },
    { "--passthrough",    'h', RTGETOPT_REQ_STRING },
};

int handleStorageAttach(HandlerArg *a)
{
    int c;
    HRESULT rc = S_OK;
    ULONG port   = ~0U;
    ULONG device = ~0U;
    bool fRunTime = false;
    const char *pszCtl  = NULL;
    const char *pszType = NULL;
    const char *pszMedium = NULL;
    const char *pszPassThrough = NULL;
    Bstr machineuuid (a->argv[0]);
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    ComPtr<IMachine> machine;
    ComPtr<IStorageController> storageCtl;

    if (a->argc < 9)
        return errorSyntax(USAGE_STORAGEATTACH, "Too few parameters");
    else if (a->argc > 13)
        return errorSyntax(USAGE_STORAGEATTACH, "Too many parameters");

    RTGetOptInit(&GetState, a->argc, a->argv, g_aStorageAttachOptions,
                 RT_ELEMENTS(g_aStorageAttachOptions), 1, 0 /* fFlags */);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 's':   // storage controller name
            {
                if (ValueUnion.psz)
                    pszCtl = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'p':   // port
            {
                port = ValueUnion.u32;
                break;
            }

            case 'd':   // device
            {
                device = ValueUnion.u32;
                break;
            }

            case 'm':   // medium <none|emptydrive|uuid|filename|host:<drive>>
            {
                if (ValueUnion.psz)
                    pszMedium = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 't':   // type <dvddrive|hdd|fdd>
            {
                if (ValueUnion.psz)
                    pszType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'h':   // passthrough <on|off>
            {
                if (ValueUnion.psz)
                    pszPassThrough = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            default:
            {
                errorGetOpt(USAGE_STORAGEATTACH, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (   FAILED(rc)
        || !pszCtl
        || port == ~0U
        || device == ~0U)
    {
        errorGetOpt(USAGE_STORAGEATTACH, c, &ValueUnion);
        return 1;
    }

    /* try to find the given machine */
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR_RET(a->virtualBox, GetMachine(machineuuid, machine.asOutParam()), 1);
    }
    else
    {
        CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()), 1);
        machine->COMGETTER(Id)(machineuuid.asOutParam());
    }

    /* open a session for the VM */
    rc = a->virtualBox->OpenSession(a->session, machineuuid);
    if (FAILED(rc))
    {
        /* try to open an existing session for the VM */
        CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, machineuuid), 1);
        fRunTime = true;
    }

    if (   fRunTime
        && (   !RTStrICmp(pszMedium, "none")
            || !RTStrICmp(pszType, "hdd")))
    {
        errorArgument("DVD/HardDisk Drives can't be changed while the VM is still running\n");
        goto leave;
    }

    if (fRunTime && pszPassThrough)
    {
        errorArgument("Drive passthrough state can't be changed while the VM is still running\n");
        goto leave;
    }

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    /* check if the storage controller is present */
    rc = machine->GetStorageControllerByName(Bstr(pszCtl), storageCtl.asOutParam());
    if (FAILED(rc))
    {
        errorSyntax(USAGE_STORAGEATTACH, "Couldn't find the controller with the name: '%s'\n", pszCtl);
        goto leave;
    }

    if (!RTStrICmp(pszMedium, "none"))
    {
        CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl), port, device));
    }
    else if (!RTStrICmp(pszMedium, "emptydrive"))
    {
        if (fRunTime)
        {
            ComPtr<IMediumAttachment> mediumAttachment;
            rc = machine->GetMediumAttachment(Bstr(pszCtl), port, device, mediumAttachment.asOutParam());
            if (SUCCEEDED(rc))
            {
                DeviceType_T deviceType;
                mediumAttachment->COMGETTER(Type)(&deviceType);

                if (   (deviceType == DeviceType_DVD)
                    || (deviceType == DeviceType_Floppy))
                {
                    /* just unmount the floppy/dvd */
                    CHECK_ERROR(machine, MountMedium(Bstr(pszCtl), port, device, Bstr("")));
                }
                else
                {
                    errorArgument("No DVD/Floppy Drive attached to the controller '%s'"
                                  "at the port: %u, device: %u", pszCtl, port, device);
                    goto leave;
                }
            }
            else
            {
                errorArgument("No DVD/Floppy Drive attached to the controller '%s'"
                              "at the port: %u, device: %u", pszCtl, port, device);
                goto leave;
            }

        }
        else
        {
            StorageBus_T storageBus = StorageBus_Null;
            DeviceType_T deviceType = DeviceType_Null;

            CHECK_ERROR(storageCtl, COMGETTER(Bus)(&storageBus));

            if (storageBus == StorageBus_Floppy)
                deviceType = DeviceType_Floppy;
            else
                deviceType = DeviceType_DVD;

            /* attach a empty floppy/dvd drive after removing previous attachment */
            machine->DetachDevice(Bstr(pszCtl), port, device);
            CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl), port, device, deviceType, Bstr("")));
        }
    }
    else
    {
        {
            /**
             * try to determine the type of the drive from the
             * storage controller chipset, the attachment and
             * the medium being attached
             */
            StorageControllerType_T ctlType = StorageControllerType_Null;
            CHECK_ERROR(storageCtl, COMGETTER(ControllerType)(&ctlType));
            if (ctlType == StorageControllerType_I82078)
            {
                /**
                 * floppy controller found so lets assume the medium
                 * given by the user is also a floppy image or floppy
                 * host drive
                 */
                pszType = "fdd";
            }
            else
            {
                /**
                 * for SATA/SCSI/IDE it is hard to tell if it is a harddisk or
                 * a dvd being attached so lets check if the medium attachment
                 * and the medium, both are of same type. if yes then we are
                 * sure of its type and don't need the user to enter it manually
                 * else ask the user for the type.
                 */
                ComPtr<IMediumAttachment> mediumAttachement;
                rc = machine->GetMediumAttachment(Bstr(pszCtl), port, device, mediumAttachement.asOutParam());
                if (SUCCEEDED(rc))
                {
                    DeviceType_T deviceType;
                    mediumAttachement->COMGETTER(Type)(&deviceType);

                    if (deviceType == DeviceType_DVD)
                    {
                        Bstr uuid(pszMedium);
                        ComPtr<IMedium> dvdMedium;
                        /* first assume it's a UUID */
                        rc = a->virtualBox->GetDVDImage(uuid, dvdMedium.asOutParam());
                        if (FAILED(rc) || !dvdMedium)
                        {
                            /* must be a filename, check if it's in the collection */
                            a->virtualBox->FindDVDImage(Bstr(pszMedium), dvdMedium.asOutParam());
                        }
                        if (dvdMedium)
                        {
                            /**
                             * ok so the medium and attachment both are DVD's so it is
                             * safe to assume that we are dealing with a DVD here
                             */
                            pszType = "dvddrive";
                        }
                    }
                    else if (deviceType == DeviceType_HardDisk)
                    {
                        Bstr uuid(pszMedium);
                        ComPtr<IMedium> hardDisk;
                        /* first assume it's a UUID */
                        rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                        if (FAILED(rc) || !hardDisk)
                        {
                            /* must be a filename, check if it's in the collection */
                            a->virtualBox->FindHardDisk(Bstr(pszMedium), hardDisk.asOutParam());
                        }
                        if (hardDisk)
                        {
                            /**
                             * ok so the medium and attachment both are hdd's so it is
                             * safe to assume that we are dealing with a hdd here
                             */
                            pszType = "hdd";
                        }
                    }
                }
            }
            /* for all other cases lets ask the user what type of drive it is */
        }

        if (!pszType)
        {
            errorSyntax(USAGE_STORAGEATTACH, "Argument --type not specified\n");
            goto leave;
        }

        if (!RTStrICmp(pszType, "dvddrive"))
        {
            Bstr uuid;
            ComPtr<IMedium> dvdMedium;

            if (!fRunTime)
            {
                ComPtr<IMediumAttachment> mediumAttachement;

                /* check if there is a dvd drive at the given location, if not attach one */
                rc = machine->GetMediumAttachment(Bstr(pszCtl), port, device, mediumAttachement.asOutParam());
                if (SUCCEEDED(rc))
                {
                    DeviceType_T deviceType;
                    mediumAttachement->COMGETTER(Type)(&deviceType);

                    if (deviceType != DeviceType_DVD)
                    {
                        machine->DetachDevice(Bstr(pszCtl), port, device);
                        rc = machine->AttachDevice(Bstr(pszCtl), port, device, DeviceType_DVD, Bstr(""));
                    }
                }
                else
                {
                    rc = machine->AttachDevice(Bstr(pszCtl), port, device, DeviceType_DVD, Bstr(""));
                }
            }

            /* Attach/Detach the dvd medium now */
            do
            {
                /* host drive? */
                if (!RTStrNICmp(pszMedium, "host:", 5))
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    rc = host->FindHostDVDDrive(Bstr(pszMedium + 5), dvdMedium.asOutParam());
                    if (!dvdMedium)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(pszMedium + 5, szPathReal, sizeof(szPathReal))))
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                            rc = E_FAIL;
                            break;
                        }
                        rc = host->FindHostDVDDrive(Bstr(szPathReal), dvdMedium.asOutParam());
                        if (!dvdMedium)
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                            rc = E_FAIL;
                            break;
                        }
                    }
                }
                else
                {
                    /* first assume it's a UUID */
                    uuid = pszMedium;
                    rc = a->virtualBox->GetDVDImage(uuid, dvdMedium.asOutParam());
                    if (FAILED(rc) || !dvdMedium)
                    {
                        /* must be a filename, check if it's in the collection */
                        rc = a->virtualBox->FindDVDImage(Bstr(pszMedium), dvdMedium.asOutParam());
                        /* not registered, do that on the fly */
                        if (!dvdMedium)
                        {
                            Bstr emptyUUID;
                            CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(pszMedium),
                                         emptyUUID, dvdMedium.asOutParam()));
                        }
                    }
                    if (!dvdMedium)
                    {
                        errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                        rc = E_FAIL;
                        break;
                    }
                }
            } while (0);

            if (dvdMedium)
            {
                dvdMedium->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(machine, MountMedium(Bstr(pszCtl), port, device, uuid));
            }
        }
        else if (   !RTStrICmp(pszType, "hdd")
                 && !fRunTime)
        {
            ComPtr<IMediumAttachment> mediumAttachement;

            /* if there is anything attached at the given location, remove it */
            machine->DetachDevice(Bstr(pszCtl), port, device);

            /* first guess is that it's a UUID */
            Bstr uuid(pszMedium);
            ComPtr<IMedium> hardDisk;
            rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());

            /* not successful? Then it must be a filename */
            if (!hardDisk)
            {
                rc = a->virtualBox->FindHardDisk(Bstr(pszMedium), hardDisk.asOutParam());
                if (FAILED(rc))
                {
                    /* open the new hard disk object */
                    CHECK_ERROR(a->virtualBox,
                                 OpenHardDisk(Bstr(pszMedium),
                                              AccessMode_ReadWrite, false, Bstr(""),
                                              false, Bstr(""), hardDisk.asOutParam()));
                }
            }

            if (hardDisk)
            {
                hardDisk->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl), port, device, DeviceType_HardDisk, uuid));
            }
            else
            {
                errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                rc = E_FAIL;
            }
        }
        else if (!RTStrICmp(pszType, "fdd"))
        {
            Bstr uuid;
            ComPtr<IMedium> floppyMedium;
            ComPtr<IMediumAttachment> floppyAttachment;
            machine->GetMediumAttachment(Bstr(pszCtl), port, device, floppyAttachment.asOutParam());

            if (   !fRunTime
                && !floppyAttachment)
                CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl), port, device, DeviceType_Floppy, Bstr("")));

            /* host drive? */
            if (!RTStrNICmp(pszMedium, "host:", 5))
            {
                ComPtr<IHost> host;

                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                rc = host->FindHostFloppyDrive(Bstr(pszMedium + 5), floppyMedium.asOutParam());
                if (!floppyMedium)
                {
                    errorArgument("Invalid host floppy drive name \"%s\"", pszMedium + 5);
                    rc = E_FAIL;
                }
            }
            else
            {
                /* first assume it's a UUID */
                uuid = pszMedium;
                rc = a->virtualBox->GetFloppyImage(uuid, floppyMedium.asOutParam());
                if (FAILED(rc) || !floppyMedium)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindFloppyImage(Bstr(pszMedium), floppyMedium.asOutParam());
                    /* not registered, do that on the fly */
                    if (!floppyMedium)
                    {
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox,
                                     OpenFloppyImage(Bstr(pszMedium),
                                                     emptyUUID,
                                                     floppyMedium.asOutParam()));
                    }
                }

                if (!floppyMedium)
                {
                    errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                    rc = E_FAIL;
                }
            }

            if (floppyMedium)
            {
                floppyMedium->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(machine, MountMedium(Bstr(pszCtl), port, device, uuid));
            }
        }
        else
        {
            errorArgument("Invalid --type argument '%s'", pszType);
            rc = E_FAIL;
        }
    }

    if (   pszPassThrough
        && (SUCCEEDED(rc)))
    {
        ComPtr<IMediumAttachment> mattach;

        CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl), port, device, mattach.asOutParam()));

        if (SUCCEEDED(rc))
        {
            if (!RTStrICmp(pszPassThrough, "on"))
            {
                CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl), port, device, TRUE));
            }
            else if (!RTStrICmp(pszPassThrough, "off"))
            {
                CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl), port, device, FALSE));
            }
            else
            {
                errorArgument("Invalid --passthrough argument '%s'", pszPassThrough);
                rc = E_FAIL;
            }
        }
        else
        {
            errorArgument("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
            rc = E_FAIL;
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

leave:
    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}


static const RTGETOPTDEF g_aStorageControllerOptions[] =
{
    { "--name",             'n', RTGETOPT_REQ_STRING },
    { "--add",              'a', RTGETOPT_REQ_STRING },
    { "--controller",       'c', RTGETOPT_REQ_STRING },
    { "--sataideemulation", 'e', RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
    { "--sataportcount",    'p', RTGETOPT_REQ_UINT32 },
    { "--remove",           'r', RTGETOPT_REQ_NOTHING },
};

int handleStorageController(HandlerArg *a)
{
    int               c;
    HRESULT           rc             = S_OK;
    const char       *pszCtl         = NULL;
    const char       *pszBusType     = NULL;
    const char       *pszCtlType     = NULL;
    ULONG             satabootdev    = ~0U;
    ULONG             sataidedev     = ~0U;
    ULONG             sataportcount  = ~0U;
    bool              fRemoveCtl     = false;
    Bstr              machineuuid (a->argv[0]);
    ComPtr<IMachine>  machine;
    RTGETOPTUNION     ValueUnion;
    RTGETOPTSTATE     GetState;

    if (a->argc < 4)
        return errorSyntax(USAGE_STORAGECONTROLLER, "Too few parameters");

    RTGetOptInit (&GetState, a->argc, a->argv, g_aStorageControllerOptions,
                  RT_ELEMENTS(g_aStorageControllerOptions), 1, 0 /* fFlags */);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'n':   // controller name
            {
                if (ValueUnion.psz)
                    pszCtl = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'a':   // controller bus type <ide/sata/scsi/floppy>
            {
                if (ValueUnion.psz)
                    pszBusType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'c':   // controller <lsilogic/buslogic/intelahci/piix3/piix4/ich6/i82078>
            {
                if (ValueUnion.psz)
                    pszCtlType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'e':   // sataideemulation
            {
                if ((GetState.uIndex < 1) && (GetState.uIndex > 4))
                    return errorSyntax(USAGE_STORAGECONTROLLER,
                                       "Missing or Invalid SATA boot slot number in '%s'",
                                       GetState.pDef->pszLong);

                if ((ValueUnion.u32 < 1) && (ValueUnion.u32 > 30))
                    return errorSyntax(USAGE_STORAGECONTROLLER,
                                       "Missing or Invalid SATA port number in '%s'",
                                       GetState.pDef->pszLong);

                satabootdev = GetState.uIndex;
                sataidedev = ValueUnion.u32;
                break;
            }

            case 'p':   // sataportcount
            {
                sataportcount = ValueUnion.u32;
                break;
            }

            case 'r':   // remove controller
            {
                fRemoveCtl = true;
                break;
            }

            default:
            {
                errorGetOpt(USAGE_STORAGECONTROLLER, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return 1;

    /* try to find the given machine */
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR_RET(a->virtualBox, GetMachine (machineuuid, machine.asOutParam()), 1);
    }
    else
    {
        CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()), 1);
        machine->COMGETTER(Id)(machineuuid.asOutParam());
    }

    /* open a session for the VM */
    CHECK_ERROR_RET(a->virtualBox, OpenSession (a->session, machineuuid), 1);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    if (!pszCtl)
    {
        /* it's important to always close sessions */
        a->session->Close();
        errorSyntax(USAGE_STORAGECONTROLLER, "Storage Controller Name not specified\n");
        return 1;
    }

    if (fRemoveCtl)
    {
        com::SafeIfaceArray<IMediumAttachment> mediumAttachments;

        CHECK_ERROR(machine,
                     GetMediumAttachmentsOfController(Bstr(pszCtl),
                                                      ComSafeArrayAsOutParam(mediumAttachments)));
        for (size_t i = 0; i < mediumAttachments.size(); ++ i)
        {
            ComPtr<IMediumAttachment> mediumAttach = mediumAttachments[i];
            LONG port = 0;
            LONG device = 0;

            CHECK_ERROR(mediumAttach, COMGETTER(Port)(&port));
            CHECK_ERROR(mediumAttach, COMGETTER(Device)(&device));
            CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl), port, device));
        }

        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, RemoveStorageController(Bstr(pszCtl)));
        else
            errorArgument("Can't detach the devices connected to '%s' Controller\n"
                          "and thus its removal failed.", pszCtl);
    }
    else
    {
        if (pszBusType)
        {
            ComPtr<IStorageController> ctl;

            if (!RTStrICmp(pszBusType, "ide"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl), StorageBus_IDE, ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sata"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl), StorageBus_SATA, ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "scsi"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl), StorageBus_SCSI, ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "floppy"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl), StorageBus_Floppy, ctl.asOutParam()));
            }
            else
            {
                errorArgument("Invalid --add argument '%s'", pszBusType);
                rc = E_FAIL;
            }
        }

        if (   pszCtlType
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl), ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszCtlType, "lsilogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
                else if (!RTStrICmp(pszCtlType, "buslogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else if (!RTStrICmp(pszCtlType, "intelahci"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
                }
                else if (!RTStrICmp(pszCtlType, "piix3"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
                }
                else if (!RTStrICmp(pszCtlType, "piix4"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
                }
                else if (!RTStrICmp(pszCtlType, "ich6"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_ICH6));
                }
                else if (!RTStrICmp(pszCtlType, "i82078"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_I82078));
                }
                else
                {
                    errorArgument("Invalid --type argument '%s'", pszCtlType);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   (sataportcount != ~0U)
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl), ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, COMSETTER(PortCount)(sataportcount));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   (sataidedev != ~0U)
            && (satabootdev != ~0U)
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl), ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, SetIDEEmulationPort(satabootdev, sataidedev));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
