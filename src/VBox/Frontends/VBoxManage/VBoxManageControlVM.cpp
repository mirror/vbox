/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <iprt/ctype.h>
#include <VBox/err.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/log.h>

#include "VBoxManage.h"


/**
 * Parses a number.
 *
 * @returns Valid number on success.
 * @returns 0 if invalid number. All necesary bitching has been done.
 * @param   psz     Pointer to the nic number.
 */
static unsigned parseNum(const char *psz, unsigned cMaxNum, const char *name)
{
    uint32_t u32;
    char *pszNext;
    int rc = RTStrToUInt32Ex(psz, &pszNext, 10, &u32);
    if (    RT_SUCCESS(rc)
        &&  *pszNext == '\0'
        &&  u32 >= 1
        &&  u32 <= cMaxNum)
        return (unsigned)u32;
    errorArgument("Invalid %s number '%s'", name, psz);
    return 0;
}


int handleControlVM(HandlerArg *a)
{
    using namespace com;
    HRESULT rc;

    if (a->argc < 2)
        return errorSyntax(USAGE_CONTROLVM, "Not enough parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Bstr machineuuid (a->argv[0]);
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR(a->virtualBox, GetMachine(machineuuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR(a->virtualBox, FindMachine(machineuuid, machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(machineuuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, machineuuid), 1);

    do
    {
        /* get the associated console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        /* which command? */
        if (!strcmp(a->argv[1], "pause"))
        {
            CHECK_ERROR_BREAK(console, Pause());
        }
        else if (!strcmp(a->argv[1], "resume"))
        {
            CHECK_ERROR_BREAK(console, Resume());
        }
        else if (!strcmp(a->argv[1], "reset"))
        {
            CHECK_ERROR_BREAK(console, Reset());
        }
        else if (!strcmp(a->argv[1], "poweroff"))
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, PowerDown(progress.asOutParam()));

            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to power off machine. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to power off machine. No error message available!\n");
                }
            }
        }
        else if (!strcmp(a->argv[1], "savestate"))
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, SaveState(progress.asOutParam()));

            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to save machine state. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to save machine state. No error message available!\n");
                }
            }
        }
        else if (!strcmp(a->argv[1], "acpipowerbutton"))
        {
            CHECK_ERROR_BREAK(console, PowerButton());
        }
        else if (!strcmp(a->argv[1], "acpisleepbutton"))
        {
            CHECK_ERROR_BREAK(console, SleepButton());
        }
        else if (!strcmp(a->argv[1], "injectnmi"))
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR_BREAK(console, COMGETTER(Debugger)(debugger.asOutParam()));
            CHECK_ERROR_BREAK(debugger, InjectNMI());
        }
        else if (!strcmp(a->argv[1], "keyboardputscancode"))
        {
            ComPtr<IKeyboard> keyboard;
            CHECK_ERROR_BREAK(console, COMGETTER(Keyboard)(keyboard.asOutParam()));

            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'. Expected IBM PC AT set 2 keyboard scancode(s) as hex byte(s).", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* Arbitrary restrict the length of a sequence of scancodes to 1024. */
            LONG alScancodes[1024];
            int cScancodes = 0;

            /* Process the command line. */
            int i;
            for (i = 1 + 1; i < a->argc && cScancodes < (int)RT_ELEMENTS(alScancodes); i++, cScancodes++)
            {
                if (   RT_C_IS_XDIGIT (a->argv[i][0])
                    && RT_C_IS_XDIGIT (a->argv[i][1])
                    && a->argv[i][2] == 0)
                {
                    uint8_t u8Scancode;
                    int rc = RTStrToUInt8Ex(a->argv[i], NULL, 16, &u8Scancode);
                    if (RT_FAILURE (rc))
                    {
                        RTPrintf("Error: converting '%s' returned %Rrc!\n", a->argv[i], rc);
                        rc = E_FAIL;
                        break;
                    }

                    alScancodes[cScancodes] = u8Scancode;
                }
                else
                {
                    RTPrintf("Error: '%s' is not a hex byte!\n", a->argv[i]);
                    rc = E_FAIL;
                    break;
                }
            }

            if (FAILED(rc))
                break;

            if (   cScancodes == RT_ELEMENTS(alScancodes)
                && i < a->argc)
            {
                RTPrintf("Error: too many scancodes, maximum %d allowed!\n", RT_ELEMENTS(alScancodes));
                rc = E_FAIL;
                break;
            }

            /* Send scancodes to the VM.
             * Note: 'PutScancodes' did not work here. Only the first scancode was transmitted.
             */
            for (i = 0; i < cScancodes; i++)
            {
                CHECK_ERROR_BREAK(keyboard, PutScancode(alScancodes[i]));
                RTPrintf("Scancode[%d]: 0x%02X\n", i, alScancodes[i]);
            }
        }
        else if (!strncmp(a->argv[1], "setlinkstate", 12))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][12], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                if (!strcmp(a->argv[2], "on"))
                {
                    CHECK_ERROR_BREAK(adapter, COMSETTER(CableConnected)(TRUE));
                }
                else if (!strcmp(a->argv[2], "off"))
                {
                    CHECK_ERROR_BREAK(adapter, COMSETTER(CableConnected)(FALSE));
                }
                else
                {
                    errorArgument("Invalid link state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
#ifdef VBOX_DYNAMIC_NET_ATTACH
        /* here the order in which strncmp is called is important
         * cause nictracefile can be very well compared with
         * nictrace and nic and thus everything will always fail
         * if the order is changed
         */
        else if (!strncmp(a->argv[1], "nictracefile", 12))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][12], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (a->argv[2])
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceFile)(Bstr(a->argv[2])), 1);
                    }
                    else
                    {
                        errorArgument("Invalid filename or filename not specified for NIC %lu", n);
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its tracefile\n", n);
                }
            }
        }
        else if (!strncmp(a->argv[1], "nictrace", 8))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][8], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (!strcmp(a->argv[2], "on"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceEnabled)(TRUE), 1);
                    }
                    else if (!strcmp(a->argv[2], "off"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceEnabled)(FALSE), 1);
                    }
                    else
                    {
                        errorArgument("Invalid nictrace%lu argument '%s'", n, Utf8Str(a->argv[2]).raw());
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its tracefile\n", n);
                }
            }
        }
        else if (!strncmp(a->argv[1], "nic", 3))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][3], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (!strcmp(a->argv[2], "null"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, Detach(), 1);
                    }
                    else if (!strcmp(a->argv[2], "nat"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        if (a->argc == 4)
                            CHECK_ERROR_RET(adapter, COMSETTER(NATNetwork)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToNAT(), 1);
                    }
                    else if (  !strcmp(a->argv[2], "bridged")
                            || !strcmp(a->argv[2], "hostif")) /* backward compatibility */
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(HostInterface)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToBridgedInterface(), 1);
                    }
                    else if (!strcmp(a->argv[2], "intnet"))
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(InternalNetwork)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToInternalNetwork(), 1);
                    }
#if defined(VBOX_WITH_NETFLT)
                    else if (!strcmp(a->argv[2], "hostonly"))
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(HostInterface)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToHostOnlyInterface(), 1);
                    }
#endif
                    else
                    {
                        errorArgument("Invalid type '%s' specfied for NIC %lu", Utf8Str(a->argv[2]).raw(), n);
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its attachment type\n", n);
                }
            }
        }
#endif /* VBOX_DYNAMIC_NET_ATTACH */
#ifdef VBOX_WITH_VRDP
        else if (!strcmp(a->argv[1], "vrdp"))
        {
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding VRDP server */
            ComPtr<IVRDPServer> vrdpServer;
            sessionMachine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
            ASSERT(vrdpServer);
            if (vrdpServer)
            {
                if (!strcmp(a->argv[2], "on"))
                {
                    CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled)(TRUE));
                }
                else if (!strcmp(a->argv[2], "off"))
                {
                    CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled)(FALSE));
                }
                else
                {
                    errorArgument("Invalid vrdp server state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
        else if (!strcmp(a->argv[1], "vrdpport"))
        {
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding VRDP server */
            ComPtr<IVRDPServer> vrdpServer;
            sessionMachine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
            ASSERT(vrdpServer);
            if (vrdpServer)
            {
                Bstr vrdpports;

                if (!strcmp(a->argv[2], "default"))
                    vrdpports = "0";
                else
                    vrdpports = a->argv [2];

                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Ports)(vrdpports));
            }
        }
#endif /* VBOX_WITH_VRDP */
        else if (   !strcmp(a->argv[1], "usbattach")
                 || !strcmp(a->argv[1], "usbdetach"))
        {
            if (a->argc < 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Not enough parameters");
                rc = E_FAIL;
                break;
            }

            bool attach = !strcmp(a->argv[1], "usbattach");

            Bstr usbId = a->argv [2];
            if (Guid(usbId).isEmpty())
            {
                // assume address
                if (attach)
                {
                    ComPtr <IHost> host;
                    CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    SafeIfaceArray <IHostUSBDevice> coll;
                    CHECK_ERROR_BREAK(host, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)));
                    ComPtr <IHostUSBDevice> dev;
                    CHECK_ERROR_BREAK(host, FindUSBDeviceByAddress(Bstr(a->argv [2]), dev.asOutParam()));
                    CHECK_ERROR_BREAK(dev, COMGETTER(Id)(usbId.asOutParam()));
                }
                else
                {
                    SafeIfaceArray <IUSBDevice> coll;
                    CHECK_ERROR_BREAK(console, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)));
                    ComPtr <IUSBDevice> dev;
                    CHECK_ERROR_BREAK(console, FindUSBDeviceByAddress(Bstr(a->argv [2]),
                                                       dev.asOutParam()));
                    CHECK_ERROR_BREAK(dev, COMGETTER(Id)(usbId.asOutParam()));
                }
            }

            if (attach)
                CHECK_ERROR_BREAK(console, AttachUSBDevice(usbId));
            else
            {
                ComPtr <IUSBDevice> dev;
                CHECK_ERROR_BREAK(console, DetachUSBDevice(usbId, dev.asOutParam()));
            }
        }
        else if (!strcmp(a->argv[1], "setvideomodehint"))
        {
            if (a->argc != 5 && a->argc != 6)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t xres = RTStrToUInt32(a->argv[2]);
            uint32_t yres = RTStrToUInt32(a->argv[3]);
            uint32_t bpp  = RTStrToUInt32(a->argv[4]);
            uint32_t displayIdx = 0;
            if (a->argc == 6)
                displayIdx = RTStrToUInt32(a->argv[5]);

            ComPtr<IDisplay> display;
            CHECK_ERROR_BREAK(console, COMGETTER(Display)(display.asOutParam()));
            CHECK_ERROR_BREAK(display, SetVideoModeHint(xres, yres, bpp, displayIdx));
        }
        else if (!strcmp(a->argv[1], "setcredentials"))
        {
            bool fAllowLocalLogon = true;
            if (a->argc == 7)
            {
                if (   strcmp(a->argv[5], "--allowlocallogon")
                    && strcmp(a->argv[5], "-allowlocallogon"))
                {
                    errorArgument("Invalid parameter '%s'", a->argv[5]);
                    rc = E_FAIL;
                    break;
                }
                if (!strcmp(a->argv[6], "no"))
                    fAllowLocalLogon = false;
            }
            else if (a->argc != 5)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IGuest> guest;
            CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));
            CHECK_ERROR_BREAK(guest, SetCredentials(Bstr(a->argv[2]), Bstr(a->argv[3]), Bstr(a->argv[4]), fAllowLocalLogon));
        }
        else if (!strcmp(a->argv[1], "dvdattach"))
        {
            Bstr uuid;
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IMedium> dvdMedium;

            /* unmount? */
            if (!strcmp(a->argv[2], "none"))
            {
                /* nothing to do, NULL object will cause unmount */
            }
            /* host drive? */
            else if (!strncmp(a->argv[2], "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

                rc = host->FindHostDVDDrive(Bstr(a->argv[2] + 5), dvdMedium.asOutParam());
                if (!dvdMedium)
                {
                    errorArgument("Invalid host DVD drive name \"%s\"",
                                  a->argv[2] + 5);
                    rc = E_FAIL;
                    break;
                }
            }
            else
            {
                /* first assume it's a UUID */
                uuid = a->argv[2];
                rc = a->virtualBox->GetDVDImage(uuid, dvdMedium.asOutParam());
                if (FAILED(rc) || !dvdMedium)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindDVDImage(Bstr(a->argv[2]), dvdMedium.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdMedium)
                    {
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(a->argv[2]), emptyUUID, dvdMedium.asOutParam()));
                    }
                }
                if (!dvdMedium)
                {
                    rc = E_FAIL;
                    break;
                }
            }

            /** @todo generalize this, allow arbitrary number of DVD drives
             * and as a consequence multiple attachments and different
             * storage controllers. */
            if (dvdMedium)
                dvdMedium->COMGETTER(Id)(uuid.asOutParam());
            else
                uuid = Guid().toString();
            CHECK_ERROR(machine, MountMedium(Bstr("IDE Controller"), 1, 0, uuid, FALSE /* aForce */));
        }
        else if (!strcmp(a->argv[1], "floppyattach"))
        {
            Bstr uuid;
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IMedium> floppyMedium;

            /* unmount? */
            if (!strcmp(a->argv[2], "none"))
            {
                /* nothing to do, NULL object will cause unmount */
            }
            /* host drive? */
            else if (!strncmp(a->argv[2], "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                host->FindHostFloppyDrive(Bstr(a->argv[2] + 5), floppyMedium.asOutParam());
                if (!floppyMedium)
                {
                    errorArgument("Invalid host floppy drive name \"%s\"",
                                  a->argv[2] + 5);
                    rc = E_FAIL;
                    break;
                }
            }
            else
            {
                /* first assume it's a UUID */
                uuid = a->argv[2];
                rc = a->virtualBox->GetFloppyImage(uuid, floppyMedium.asOutParam());
                if (FAILED(rc) || !floppyMedium)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindFloppyImage(Bstr(a->argv[2]), floppyMedium.asOutParam());
                    /* not registered, do that on the fly */
                    if (!floppyMedium)
                    {
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenFloppyImage(Bstr(a->argv[2]), emptyUUID, floppyMedium.asOutParam()));
                    }
                }
                if (!floppyMedium)
                {
                    rc = E_FAIL;
                    break;
                }
            }
            floppyMedium->COMGETTER(Id)(uuid.asOutParam());
            CHECK_ERROR(machine, MountMedium(Bstr("Floppy Controller"), 0, 0, uuid, FALSE /* aForce */));
        }
#ifdef VBOX_WITH_MEM_BALLOONING
        else if (   !strcmp(a->argv[1], "--guestmemoryballoon")
                 || !strcmp(a->argv[1], "-guestmemoryballoon"))
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest memory balloon size '%s'", a->argv[2]);
                rc = E_FAIL;
                break;
            }

            /* guest is running; update IGuest */
            ComPtr <IGuest> guest;

            rc = console->COMGETTER(Guest)(guest.asOutParam());
            if (SUCCEEDED(rc))
                CHECK_ERROR(guest, COMSETTER(MemoryBalloonSize)(uVal));
        }
#endif
        else if (   !strcmp(a->argv[1], "--gueststatisticsinterval")
                 || !strcmp(a->argv[1], "-gueststatisticsinterval"))
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest statistics interval '%s'", a->argv[2]);
                rc = E_FAIL;
                break;
            }

            /* guest is running; update IGuest */
            ComPtr <IGuest> guest;

            rc = console->COMGETTER(Guest)(guest.asOutParam());
            if (SUCCEEDED(rc))
                CHECK_ERROR(guest, COMSETTER(StatisticsUpdateInterval)(uVal));
        }
        else if (!strcmp(a->argv[1], "teleport"))
        {
            Bstr        bstrHostname;
            uint32_t    uMaxDowntime = 250 /*ms*/;
            uint32_t    uPort        = UINT32_MAX;
            Bstr        bstrPassword("");
            static const RTGETOPTDEF s_aTeleportOptions[] =
            {
                { "--host",        'h', RTGETOPT_REQ_STRING }, /** @todo RTGETOPT_FLAG_MANDATORY */
                { "--hostname",    'h', RTGETOPT_REQ_STRING }, /** @todo remove this */
                { "--maxdowntime", 'd', RTGETOPT_REQ_UINT32 },
                { "--port",        'p', RTGETOPT_REQ_UINT32 }, /** @todo RTGETOPT_FLAG_MANDATORY */
                { "--password",    'P', RTGETOPT_REQ_STRING }
            };
            RTGETOPTSTATE GetOptState;
            RTGetOptInit(&GetOptState, a->argc, a->argv, s_aTeleportOptions, RT_ELEMENTS(s_aTeleportOptions), 2, 0 /*fFlags*/);
            int ch;
            RTGETOPTUNION Value;
            while (   SUCCEEDED(rc)
                   && (ch = RTGetOpt(&GetOptState, &Value)))
            {
                switch (ch)
                {
                    case 'h': bstrHostname  = Value.psz; break;
                    case 'd': uMaxDowntime  = Value.u32; break;
                    case 'p': uPort         = Value.u32; break;
                    case 'P': bstrPassword  = Value.psz; break;
                    default:
                        errorGetOpt(USAGE_CONTROLVM, ch, &Value);
                        rc = E_FAIL;
                        break;
                }
            }
            if (FAILED(rc))
                break;

            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, Teleport(bstrHostname, uPort, bstrPassword, uMaxDowntime, progress.asOutParam()));
            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: teleportation failed. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: teleportation failed. No error message available!\n");
            }
        }
        else
        {
            errorSyntax(USAGE_CONTROLVM, "Invalid parameter '%s'", Utf8Str(a->argv[1]).raw());
            rc = E_FAIL;
        }
    } while (0);

    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

