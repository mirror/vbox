/* $Id$ */
/** @file
 * VBoxManage - The storage controller related commands.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
    { "--storagectl",       's', RTGETOPT_REQ_STRING },
    { "--port",             'p', RTGETOPT_REQ_UINT32 },
    { "--device",           'd', RTGETOPT_REQ_UINT32 },
    { "--type",             't', RTGETOPT_REQ_STRING },
    { "--medium",           'm', RTGETOPT_REQ_STRING },
    { "--mtype",            'M', RTGETOPT_REQ_STRING },
    { "--passthrough",      'h', RTGETOPT_REQ_STRING },
    { "--tempeject",        'e', RTGETOPT_REQ_STRING },
    { "--nonrotational",    'n', RTGETOPT_REQ_STRING },
    { "--bandwidthgroup",   'b', RTGETOPT_REQ_STRING },
    { "--forceunmount",     'f', RTGETOPT_REQ_NOTHING },
    { "--comment",          'C', RTGETOPT_REQ_STRING },
    { "--setuuid",          'q', RTGETOPT_REQ_STRING },
    { "--setparentuuid",    'Q', RTGETOPT_REQ_STRING },
    // iSCSI options
    { "--server",           'S', RTGETOPT_REQ_STRING },
    { "--target",           'T', RTGETOPT_REQ_STRING },
    { "--port",             'P', RTGETOPT_REQ_STRING },
    { "--lun",              'L', RTGETOPT_REQ_STRING },
    { "--encodedlun",       'E', RTGETOPT_REQ_STRING },
    { "--username",         'U', RTGETOPT_REQ_STRING },
    { "--password",         'W', RTGETOPT_REQ_STRING },
    { "--intnet",           'I', RTGETOPT_REQ_NOTHING },
};

int handleStorageAttach(HandlerArg *a)
{
    int c = VERR_INTERNAL_ERROR;        /* initialized to shut up gcc */
    HRESULT rc = S_OK;
    ULONG port   = ~0U;
    ULONG device = 0;                   /* device is optional, default is 0 */
    bool fForceUnmount = false;
    bool fSetMediumType = false;
    bool fSetNewUuid = false;
    bool fSetNewParentUuid = false;
    MediumType_T mediumType = MediumType_Normal;
    Bstr bstrComment;
    const char *pszCtl  = NULL;
    DeviceType_T devTypeRequested = DeviceType_Null;
    const char *pszMedium = NULL;
    const char *pszPassThrough = NULL;
    const char *pszTempEject = NULL;
    const char *pszNonRotational = NULL;
    const char *pszBandwidthGroup = NULL;
    Bstr bstrNewUuid;
    Bstr bstrNewParentUuid;
    // iSCSI options
    Bstr bstrServer;
    Bstr bstrTarget;
    Bstr bstrPort;
    Bstr bstrLun;
    Bstr bstrUsername;
    Bstr bstrPassword;
    bool fIntNet = false;

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    ComPtr<IMachine> machine;
    ComPtr<IStorageController> storageCtl;
    ComPtr<ISystemProperties> systemProperties;

    RTGetOptInit(&GetState, a->argc, a->argv, g_aStorageAttachOptions,
                 RT_ELEMENTS(g_aStorageAttachOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

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

            case 'm':   // medium <none|emptydrive|uuid|filename|host:<drive>|iSCSI>
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
                {
                    if (!RTStrICmp(ValueUnion.psz, "hdd"))
                        devTypeRequested = DeviceType_HardDisk;
                    else if (!RTStrICmp(ValueUnion.psz, "fdd"))
                        devTypeRequested = DeviceType_Floppy;
                    else if (!RTStrICmp(ValueUnion.psz, "dvddrive"))
                        devTypeRequested = DeviceType_DVD;
                    else
                        return errorArgument("Invalid --type argument '%s'", ValueUnion.psz);
                }
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

            case 'e':   // tempeject <on|off>
            {
                if (ValueUnion.psz)
                    pszTempEject = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'n':   // nonrotational <on|off>
            {
                if (ValueUnion.psz)
                    pszNonRotational = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'b':   // bandwidthgroup <name>
            {
                if (ValueUnion.psz)
                    pszBandwidthGroup = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'f':   // force unmount medium during runtime
            {
                fForceUnmount = true;
                break;
            }

            case 'C':
                if (ValueUnion.psz)
                    bstrComment = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;

            case 'q':
                if (ValueUnion.psz)
                {
                    bstrNewUuid = ValueUnion.psz;
                    fSetNewUuid = true;
                }
                else
                    rc = E_FAIL;
                break;

            case 'Q':
                if (ValueUnion.psz)
                {
                    bstrNewParentUuid = ValueUnion.psz;
                    fSetNewParentUuid = true;
                }
                else
                    rc = E_FAIL;
                break;

            case 'S':   // --server
                bstrServer = ValueUnion.psz;
                break;

            case 'T':   // --target
                bstrTarget = ValueUnion.psz;
                break;

            case 'P':   // --port
                bstrPort = ValueUnion.psz;
                break;

            case 'L':   // --lun
                bstrLun = ValueUnion.psz;
                break;

            case 'E':   // --encodedlun
                bstrLun = BstrFmt("enc%s", ValueUnion.psz);
                break;

            case 'U':   // --username
                bstrUsername = ValueUnion.psz;
                break;

            case 'W':   // --password
                bstrPassword = ValueUnion.psz;
                break;

            case 'M':   // --type
            {
                int vrc = parseDiskType(ValueUnion.psz, &mediumType);
                if (RT_FAILURE(vrc))
                    return errorArgument("Invalid hard disk type '%s'", ValueUnion.psz);
                fSetMediumType = true;
                break;
            }

            case 'I':   // --intnet
                fIntNet = true;
                break;

            default:
            {
                errorGetOpt(USAGE_STORAGEATTACH, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return 1;

    if (!pszCtl)
        return errorSyntax(USAGE_STORAGEATTACH, "Storage controller name not specified");
    if (port == ~0U)
        return errorSyntax(USAGE_STORAGEATTACH, "Port not specified");

    /* get the virtualbox system properties */
    CHECK_ERROR_RET(a->virtualBox, COMGETTER(SystemProperties)(systemProperties.asOutParam()), 1);

    // find the machine, lock it, get the mutable session machine
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), 1);
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);
    SessionType_T st;
    CHECK_ERROR_RET(a->session, COMGETTER(Type)(&st), 1);
    a->session->COMGETTER(Machine)(machine.asOutParam());

    try
    {
        bool fRunTime = (st == SessionType_Shared);

        if (fRunTime)
        {
            if (pszPassThrough)
                throw Utf8Str("Drive passthrough state cannot be changed while the VM is running\n");
            else if (pszBandwidthGroup)
                throw Utf8Str("Bandwidth group cannot be changed while the VM is running\n");
        }

        /* check if the storage controller is present */
        rc = machine->GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                 storageCtl.asOutParam());
        if (FAILED(rc))
            throw Utf8StrFmt("Could not find a controller named '%s'\n", pszCtl);

        /* for sata controller check if the port count is big enough
         * to accommodate the current port which is being assigned
         * else just increase the port count
         */
        {
            ULONG ulPortCount = 0;
            ULONG ulMaxPortCount = 0;

            CHECK_ERROR(storageCtl, COMGETTER(MaxPortCount)(&ulMaxPortCount));
            CHECK_ERROR(storageCtl, COMGETTER(PortCount)(&ulPortCount));

            if (   (ulPortCount != ulMaxPortCount)
                && (port >= ulPortCount)
                && (port < ulMaxPortCount))
                CHECK_ERROR(storageCtl, COMSETTER(PortCount)(port + 1));
        }

        StorageControllerType_T ctlType = StorageControllerType_Null;
        CHECK_ERROR(storageCtl, COMGETTER(ControllerType)(&ctlType));

        if (!RTStrICmp(pszMedium, "none"))
        {
            CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl).raw(), port, device));
        }
        else if (!RTStrICmp(pszMedium, "emptydrive"))
        {
            if (fRunTime)
            {
                ComPtr<IMediumAttachment> mediumAttachment;
                DeviceType_T deviceType = DeviceType_Null;
                rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port, device,
                                                  mediumAttachment.asOutParam());
                if (SUCCEEDED(rc))
                {
                    mediumAttachment->COMGETTER(Type)(&deviceType);

                    if (   (deviceType == DeviceType_DVD)
                        || (deviceType == DeviceType_Floppy))
                    {
                        /* just unmount the floppy/dvd */
                        CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(),
                                                         port,
                                                         device,
                                                         NULL,
                                                         fForceUnmount));
                    }
                }

                if (   FAILED(rc)
                    || !(   deviceType == DeviceType_DVD
                         || deviceType == DeviceType_Floppy)
                   )
                    throw Utf8StrFmt("No DVD/Floppy Drive attached to the controller '%s'"
                                     "at the port: %u, device: %u", pszCtl, port, device);

            }
            else
            {
                StorageBus_T storageBus = StorageBus_Null;
                DeviceType_T deviceType = DeviceType_Null;
                com::SafeArray <DeviceType_T> saDeviceTypes;
                ULONG driveCheck = 0;

                /* check if the device type is supported by the controller */
                CHECK_ERROR(storageCtl, COMGETTER(Bus)(&storageBus));
                CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
                for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
                {
                    if (   (saDeviceTypes[i] == DeviceType_DVD)
                        || (saDeviceTypes[i] == DeviceType_Floppy))
                        driveCheck++;
                }

                if (!driveCheck)
                    throw Utf8StrFmt("The attachment is not supported by the storage controller '%s'", pszCtl);

                if (storageBus == StorageBus_Floppy)
                    deviceType = DeviceType_Floppy;
                else
                    deviceType = DeviceType_DVD;

                /* attach a empty floppy/dvd drive after removing previous attachment */
                machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(), port, device,
                                                  deviceType, NULL));
            }
        } // end if (!RTStrICmp(pszMedium, "emptydrive"))
        else
        {
            ComPtr<IMedium> pMedium2Mount;

            // not "none", not "emptydrive": then it must be a UUID or filename or hostdrive or iSCSI;
            // for all these we first need to know the type of drive we're attaching to
            {
                /*
                 * try to determine the type of the drive from the
                 * storage controller chipset, the attachment and
                 * the medium being attached
                 */
                if (ctlType == StorageControllerType_I82078)        // floppy controller
                    devTypeRequested = DeviceType_Floppy;
                else
                {
                    /*
                     * for SATA/SCSI/IDE it is hard to tell if it is a harddisk or
                     * a dvd being attached so lets check if the medium attachment
                     * and the medium, both are of same type. if yes then we are
                     * sure of its type and don't need the user to enter it manually
                     * else ask the user for the type.
                     */
                    ComPtr<IMediumAttachment> mediumAttachment;
                    rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                      device,
                                                      mediumAttachment.asOutParam());
                    if (SUCCEEDED(rc))
                    {
                        DeviceType_T deviceType;
                        mediumAttachment->COMGETTER(Type)(&deviceType);

                        if (pszMedium)
                        {
                            ComPtr<IMedium> pExistingMedium;
                            rc = findMedium(a, pszMedium, deviceType, true /* fSilent */,
                                            pExistingMedium);
                            if (SUCCEEDED(rc) && pExistingMedium)
                            {
                                if (    (deviceType == DeviceType_DVD)
                                     || (deviceType == DeviceType_HardDisk)
                                   )
                                    devTypeRequested = deviceType;
                            }
                        }
                        else
                            devTypeRequested = deviceType;
                    }
                }
            }

            if (devTypeRequested == DeviceType_Null)        // still the initializer value?
                throw Utf8Str("Argument --type must be specified\n");

            /* check if the device type is supported by the controller */
            {
                StorageBus_T storageBus = StorageBus_Null;
                com::SafeArray <DeviceType_T> saDeviceTypes;

                CHECK_ERROR(storageCtl, COMGETTER(Bus)(&storageBus));
                CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
                if (SUCCEEDED(rc))
                {
                    ULONG driveCheck = 0;
                    for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
                        if (saDeviceTypes[i] == devTypeRequested)
                            driveCheck++;
                    if (!driveCheck)
                        throw Utf8StrFmt("The given attachment is not supported by the storage controller '%s'", pszCtl);
                }
                else
                    goto leave;
            }

            // find the medium given
            /* host drive? */
            if (!RTStrNICmp(pszMedium, "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

                if (devTypeRequested == DeviceType_DVD)
                {
                    rc = host->FindHostDVDDrive(Bstr(pszMedium + 5).raw(),
                                                pMedium2Mount.asOutParam());
                    if (!pMedium2Mount)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(pszMedium + 5, szPathReal, sizeof(szPathReal))))
                            throw Utf8StrFmt("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                        rc = host->FindHostDVDDrive(Bstr(szPathReal).raw(),
                                                    pMedium2Mount.asOutParam());
                        if (!pMedium2Mount)
                            throw Utf8StrFmt("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                    }
                }
                else
                {
                    // floppy
                    rc = host->FindHostFloppyDrive(Bstr(pszMedium + 5).raw(),
                                                   pMedium2Mount.asOutParam());
                    if (!pMedium2Mount)
                        throw Utf8StrFmt("Invalid host floppy drive name \"%s\"", pszMedium + 5);
                }
            }
            else if (!RTStrICmp(pszMedium, "iSCSI"))
            {
                /* check for required options */
                if (bstrServer.isEmpty() || bstrTarget.isEmpty())
                    throw Utf8StrFmt("Parameters --server and --target are required for iSCSI media");

                /** @todo move the location stuff to Main, which can use pfnComposeName
                 * from the disk backends to construct the location properly. Also do
                 * not use slashes to separate the parts, as otherwise only the last
                 * element containing information will be shown. */
                Bstr bstrISCSIMedium;
                if (    bstrLun.isEmpty()
                     || (bstrLun == "0")
                     || (bstrLun == "enc0")
                   )
                    bstrISCSIMedium = BstrFmt("%ls|%ls", bstrServer.raw(), bstrTarget.raw());
                else
                    bstrISCSIMedium = BstrFmt("%ls|%ls|%ls", bstrServer.raw(), bstrTarget.raw(), bstrLun.raw());

                CHECK_ERROR(a->virtualBox, CreateHardDisk(Bstr("iSCSI").raw(),
                                                          bstrISCSIMedium.raw(),
                                                          pMedium2Mount.asOutParam()));
                if (FAILED(rc)) goto leave;
                if (!bstrPort.isEmpty())
                    bstrServer = BstrFmt("%ls:%ls", bstrServer.raw(), bstrPort.raw());

                // set the other iSCSI parameters as properties
                com::SafeArray <BSTR> names;
                com::SafeArray <BSTR> values;
                Bstr("TargetAddress").detachTo(names.appendedRaw());
                bstrServer.detachTo(values.appendedRaw());
                Bstr("TargetName").detachTo(names.appendedRaw());
                bstrTarget.detachTo(values.appendedRaw());

                if (!bstrLun.isEmpty())
                {
                    Bstr("LUN").detachTo(names.appendedRaw());
                    bstrLun.detachTo(values.appendedRaw());
                }
                if (!bstrUsername.isEmpty())
                {
                    Bstr("InitiatorUsername").detachTo(names.appendedRaw());
                    bstrUsername.detachTo(values.appendedRaw());
                }
                if (!bstrPassword.isEmpty())
                {
                    Bstr("InitiatorSecret").detachTo(names.appendedRaw());
                    bstrPassword.detachTo(values.appendedRaw());
                }

                /// @todo add --initiator option - until that happens rely on the
                // defaults of the iSCSI initiator code. Setting it to a constant
                // value does more harm than good, as the initiator name is supposed
                // to identify a particular initiator uniquely.
        //        Bstr("InitiatorName").detachTo(names.appendedRaw());
        //        Bstr("iqn.2008-04.com.sun.virtualbox.initiator").detachTo(values.appendedRaw());

                /// @todo add --targetName and --targetPassword options

                if (fIntNet)
                {
                    Bstr("HostIPStack").detachTo(names.appendedRaw());
                    Bstr("0").detachTo(values.appendedRaw());
                }

                CHECK_ERROR(pMedium2Mount, SetProperties(ComSafeArrayAsInParam(names),
                                                         ComSafeArrayAsInParam(values)));
                if (FAILED(rc)) goto leave;
                Bstr guid;
                CHECK_ERROR(pMedium2Mount, COMGETTER(Id)(guid.asOutParam()));
                if (FAILED(rc)) goto leave;
                RTPrintf("iSCSI disk created. UUID: %s\n", Utf8Str(guid).c_str());
            }
            else
            {
                if (!pszMedium)
                {
                    ComPtr<IMediumAttachment> mediumAttachment;
                    rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                      device,
                                                      mediumAttachment.asOutParam());
                    if (FAILED(rc))
                        throw Utf8Str("Missing --medium argument");
                }
                else
                {
                    Bstr bstrMedium(pszMedium);
                    rc = findOrOpenMedium(a, pszMedium, devTypeRequested,
                                          pMedium2Mount, fSetNewUuid, NULL);
                    if (FAILED(rc) || !pMedium2Mount)
                        throw Utf8StrFmt("Invalid UUID or filename \"%s\"", pszMedium);
                }
            }

            // set medium/parent medium UUID, if so desired
            if (pMedium2Mount && (fSetNewUuid || fSetNewParentUuid))
            {
                CHECK_ERROR(pMedium2Mount, SetIDs(fSetNewUuid, bstrNewUuid.raw(),
                                                  fSetNewParentUuid, bstrNewParentUuid.raw()));
                if (FAILED(rc))
                    throw  Utf8Str("Failed to set the medium/parent medium UUID");
            }

            // set medium type, if so desired
            if (pMedium2Mount && fSetMediumType)
            {
                CHECK_ERROR(pMedium2Mount, COMSETTER(Type)(mediumType));
                if (FAILED(rc))
                    throw  Utf8Str("Failed to set the medium type");
            }

            if (pMedium2Mount && !bstrComment.isEmpty())
            {
                CHECK_ERROR(pMedium2Mount, COMSETTER(Description)(bstrComment.raw()));
            }

            if (pszMedium)
            {
                switch (devTypeRequested)
                {
                    case DeviceType_DVD:
                    case DeviceType_Floppy:
                    {
                        if (!fRunTime)
                        {
                            ComPtr<IMediumAttachment> mediumAttachment;
                            // check if there is a dvd/floppy drive at the given location, if not attach one first
                            rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(),
                                                              port,
                                                              device,
                                                              mediumAttachment.asOutParam());
                            if (SUCCEEDED(rc))
                            {
                                DeviceType_T deviceType;
                                mediumAttachment->COMGETTER(Type)(&deviceType);
                                if (deviceType != devTypeRequested)
                                {
                                    machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                                    rc = machine->AttachDevice(Bstr(pszCtl).raw(),
                                                               port,
                                                               device,
                                                               devTypeRequested,    // DeviceType_DVD or DeviceType_Floppy
                                                               NULL);
                                }
                            }
                            else
                            {
                                rc = machine->AttachDevice(Bstr(pszCtl).raw(),
                                                           port,
                                                           device,
                                                           devTypeRequested,    // DeviceType_DVD or DeviceType_Floppy
                                                           NULL);
                            }
                        }

                        if (pMedium2Mount)
                        {
                            CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(),
                                                             port,
                                                             device,
                                                             pMedium2Mount,
                                                             fForceUnmount));
                        }
                    } // end DeviceType_DVD or DeviceType_Floppy:
                    break;

                    case DeviceType_HardDisk:
                    {
                        // if there is anything attached at the given location, remove it
                        machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                        CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(),
                                                          port,
                                                          device,
                                                          DeviceType_HardDisk,
                                                          pMedium2Mount));
                    }
                    break;
                }
            }
        }

        if (   pszPassThrough
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszPassThrough, "on"))
                {
                    CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                           port, device, TRUE));
                }
                else if (!RTStrICmp(pszPassThrough, "off"))
                {
                    CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                           port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --passthrough argument '%s'", pszPassThrough);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszTempEject
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszTempEject, "on"))
                {
                    CHECK_ERROR(machine, TemporaryEjectDevice(Bstr(pszCtl).raw(),
                                                              port, device, TRUE));
                }
                else if (!RTStrICmp(pszTempEject, "off"))
                {
                    CHECK_ERROR(machine, TemporaryEjectDevice(Bstr(pszCtl).raw(),
                                                              port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --tempeject argument '%s'", pszTempEject);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszNonRotational
            && (SUCCEEDED(rc)))
        {
            ComPtr<IMediumAttachment> mattach;
            CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                     device, mattach.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszNonRotational, "on"))
                {
                    CHECK_ERROR(machine, NonRotationalDevice(Bstr(pszCtl).raw(),
                                                             port, device, TRUE));
                }
                else if (!RTStrICmp(pszNonRotational, "off"))
                {
                    CHECK_ERROR(machine, NonRotationalDevice(Bstr(pszCtl).raw(),
                                                             port, device, FALSE));
                }
                else
                    throw Utf8StrFmt("Invalid --nonrotational argument '%s'", pszNonRotational);
            }
            else
                throw Utf8StrFmt("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
        }

        if (   pszBandwidthGroup
            && !fRunTime
            && SUCCEEDED(rc))
        {

            if (!RTStrICmp(pszBandwidthGroup, "none"))
            {
                /* Just remove the bandwidth gorup. */
                CHECK_ERROR(machine, SetBandwidthGroupForDevice(Bstr(pszCtl).raw(),
                                                                port, device, NULL));
            }
            else
            {
                ComPtr<IBandwidthControl> bwCtrl;
                ComPtr<IBandwidthGroup> bwGroup;

                CHECK_ERROR(machine, COMGETTER(BandwidthControl)(bwCtrl.asOutParam()));

                if (SUCCEEDED(rc))
                {
                    CHECK_ERROR(bwCtrl, GetBandwidthGroup(Bstr(pszBandwidthGroup).raw(), bwGroup.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        CHECK_ERROR(machine, SetBandwidthGroupForDevice(Bstr(pszCtl).raw(),
                                                                        port, device, bwGroup));
                    }
                }
            }
        }

        /* commit changes */
        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, SaveSettings());
    }
    catch (const Utf8Str &strError)
    {
        errorArgument("%s", strError.c_str());
        rc = E_FAIL;
    }

    // machine must always be unlocked, even on errors
leave:
    a->session->UnlockMachine();

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
    { "--hostiocache",      'i', RTGETOPT_REQ_STRING },
    { "--bootable",         'b', RTGETOPT_REQ_STRING },
};

int handleStorageController(HandlerArg *a)
{
    int               c;
    HRESULT           rc             = S_OK;
    const char       *pszCtl         = NULL;
    const char       *pszBusType     = NULL;
    const char       *pszCtlType     = NULL;
    const char       *pszHostIOCache = NULL;
    const char       *pszBootable    = NULL;
    ULONG             satabootdev    = ~0U;
    ULONG             sataidedev     = ~0U;
    ULONG             sataportcount  = ~0U;
    bool              fRemoveCtl     = false;
    ComPtr<IMachine>  machine;
    RTGETOPTUNION     ValueUnion;
    RTGETOPTSTATE     GetState;

    if (a->argc < 4)
        return errorSyntax(USAGE_STORAGECONTROLLER, "Too few parameters");

    RTGetOptInit (&GetState, a->argc, a->argv, g_aStorageControllerOptions,
                  RT_ELEMENTS(g_aStorageControllerOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

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

            case 'i':
            {
                pszHostIOCache = ValueUnion.psz;
                break;
            }

            case 'b':
            {
                pszBootable = ValueUnion.psz;
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
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), 1);

    /* open a session for the VM */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), 1);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    if (!pszCtl)
    {
        /* it's important to always close sessions */
        a->session->UnlockMachine();
        errorSyntax(USAGE_STORAGECONTROLLER, "Storage controller name not specified\n");
        return 1;
    }

    if (fRemoveCtl)
    {
        com::SafeIfaceArray<IMediumAttachment> mediumAttachments;

        CHECK_ERROR(machine,
                     GetMediumAttachmentsOfController(Bstr(pszCtl).raw(),
                                                      ComSafeArrayAsOutParam(mediumAttachments)));
        for (size_t i = 0; i < mediumAttachments.size(); ++ i)
        {
            ComPtr<IMediumAttachment> mediumAttach = mediumAttachments[i];
            LONG port = 0;
            LONG device = 0;

            CHECK_ERROR(mediumAttach, COMGETTER(Port)(&port));
            CHECK_ERROR(mediumAttach, COMGETTER(Device)(&device));
            CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl).raw(), port, device));
        }

        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, RemoveStorageController(Bstr(pszCtl).raw()));
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
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_IDE,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sata"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SATA,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "scsi"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SCSI,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "floppy"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_Floppy,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sas"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SAS,
                                                          ctl.asOutParam()));
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

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

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
                else if (!RTStrICmp(pszCtlType, "lsilogicsas"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogicSas));
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

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

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

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

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

        if (   pszHostIOCache
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszHostIOCache, "on"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(TRUE));
                }
                else if (!RTStrICmp(pszHostIOCache, "off"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(FALSE));
                }
                else
                {
                    errorArgument("Invalid --hostiocache argument '%s'", pszHostIOCache);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   pszBootable
            && SUCCEEDED(rc))
        {
            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszBootable, "on"))
                {
                    CHECK_ERROR(machine, SetStorageControllerBootable(Bstr(pszCtl).raw(), TRUE));
                }
                else if (!RTStrICmp(pszBootable, "off"))
                {
                    CHECK_ERROR(machine, SetStorageControllerBootable(Bstr(pszCtl).raw(), FALSE));
                }
                else
                {
                    errorArgument("Invalid --bootable argument '%s'", pszBootable);
                    rc = E_FAIL;
                }
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
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */

