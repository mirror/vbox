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
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <vector>
#include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/asm.h>
#include <iprt/cidr.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <VBox/err.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <VBox/version.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;

/** command handler type */
typedef int (*PFNHANDLER)(HandlerArg *a);

#endif /* !VBOX_ONLY_DOCS */

// funcs
///////////////////////////////////////////////////////////////////////////////

void showLogo(void)
{
    static bool fShown; /* show only once */

    if (!fShown)
    {
        RTPrintf("VirtualBox Command Line Management Interface Version "
                 VBOX_VERSION_STRING  "\n"
                 "(C) 2005-2009 Sun Microsystems, Inc.\n"
                 "All rights reserved.\n"
                 "\n");
        fShown = true;
    }
}

#ifndef VBOX_ONLY_DOCS

static int handleRegisterVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_REGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, OpenMachine(Bstr(a->argv[0]), machine.asOutParam()));
    if (SUCCEEDED(rc))
    {
        ASSERT(machine);
        CHECK_ERROR(a->virtualBox, RegisterMachine(machine));
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleUnregisterVM(HandlerArg *a)
{
    HRESULT rc;

    if ((a->argc != 1) && (a->argc != 2))
        return errorSyntax(USAGE_UNREGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());
        machine = NULL;
        CHECK_ERROR(a->virtualBox, UnregisterMachine(uuid, machine.asOutParam()));
        if (SUCCEEDED(rc) && machine)
        {
            /* are we supposed to delete the config file? */
            if ((a->argc == 2) && (strcmp(a->argv[1], "-delete") == 0))
            {
                CHECK_ERROR(machine, DeleteSettings());
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCreateVM(HandlerArg *a)
{
    HRESULT rc;
    Bstr baseFolder;
    Bstr settingsFile;
    Bstr name;
    Bstr osTypeId;
    RTUUID id;
    bool fRegister = false;

    RTUuidClear(&id);
    for (int i = 0; i < a->argc; i++)
    {
        if (strcmp(a->argv[i], "-basefolder") == 0)
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            baseFolder = a->argv[i];
        }
        else if (strcmp(a->argv[i], "-settingsfile") == 0)
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            settingsFile = a->argv[i];
        }
        else if (strcmp(a->argv[i], "-name") == 0)
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            name = a->argv[i];
        }
        else if (strcmp(a->argv[i], "-ostype") == 0)
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            osTypeId = a->argv[i];
        }
        else if (strcmp(a->argv[i], "-uuid") == 0)
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (RT_FAILURE(RTUuidFromStr(&id, a->argv[i])))
                return errorArgument("Invalid UUID format %s\n", a->argv[i]);
        }
        else if (strcmp(a->argv[i], "-register") == 0)
        {
            fRegister = true;
        }
        else
            return errorSyntax(USAGE_CREATEVM, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
    }
    if (!name)
        return errorSyntax(USAGE_CREATEVM, "Parameter -name is required");

    if (!!baseFolder && !!settingsFile)
        return errorSyntax(USAGE_CREATEVM, "Either -basefolder or -settingsfile must be specified");

    do
    {
        ComPtr<IMachine> machine;

        if (!settingsFile)
            CHECK_ERROR_BREAK(a->virtualBox,
                CreateMachine(name, osTypeId, baseFolder, Guid(id), machine.asOutParam()));
        else
            CHECK_ERROR_BREAK(a->virtualBox,
                CreateLegacyMachine(name, osTypeId, settingsFile, Guid(id), machine.asOutParam()));

        CHECK_ERROR_BREAK(machine, SaveSettings());
        if (fRegister)
        {
            CHECK_ERROR_BREAK(a->virtualBox, RegisterMachine(machine));
        }
        Guid uuid;
        CHECK_ERROR_BREAK(machine, COMGETTER(Id)(uuid.asOutParam()));
        CHECK_ERROR_BREAK(machine, COMGETTER(SettingsFilePath)(settingsFile.asOutParam()));
        RTPrintf("Virtual machine '%ls' is created%s.\n"
                 "UUID: %s\n"
                 "Settings file: '%ls'\n",
                 name.raw(), fRegister ? " and registered" : "",
                 uuid.toString().raw(), settingsFile.raw());
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Parses a number.
 *
 * @returns Valid number on success.
 * @returns 0 if invalid number. All necesary bitching has been done.
 * @param   psz     Pointer to the nic number.
 */
unsigned parseNum(const char *psz, unsigned cMaxNum, const char *name)
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


/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

static int handleStartVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc < 1)
        return errorSyntax(USAGE_STARTVM, "Not enough parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* default to GUI session type */
        Bstr sessionType = "gui";
        /* has a session type been specified? */
        if ((a->argc > 2) && (strcmp(a->argv[1], "-type") == 0))
        {
            if (strcmp(a->argv[2], "gui") == 0)
            {
                sessionType = "gui";
            }
#ifdef VBOX_WITH_VRDP
            else if (strcmp(a->argv[2], "vrdp") == 0)
            {
                sessionType = "vrdp";
            }
#endif
            else if (strcmp(a->argv[2], "capture") == 0)
            {
                sessionType = "capture";
            }
            else
                return errorArgument("Invalid session type argument '%s'", a->argv[2]);
        }

        Bstr env;
#ifdef RT_OS_LINUX
        /* make sure the VM process will start on the same display as VBoxManage */
        {
            const char *display = RTEnvGet ("DISPLAY");
            if (display)
                env = Utf8StrFmt ("DISPLAY=%s", display);
        }
#endif
        ComPtr<IProgress> progress;
        CHECK_ERROR_RET(a->virtualBox, OpenRemoteSession(a->session, uuid, sessionType,
                                                         env, progress.asOutParam()), rc);
        RTPrintf("Waiting for the remote session to open...\n");
        CHECK_ERROR_RET(progress, WaitForCompletion (-1), 1);

        BOOL completed;
        CHECK_ERROR_RET(progress, COMGETTER(Completed)(&completed), rc);
        ASSERT(completed);

        HRESULT resultCode;
        CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&resultCode), rc);
        if (FAILED(resultCode))
        {
            ComPtr <IVirtualBoxErrorInfo> errorInfo;
            CHECK_ERROR_RET(progress, COMGETTER(ErrorInfo)(errorInfo.asOutParam()), 1);
            ErrorInfo info (errorInfo);
            GluePrintErrorInfo(info);
        }
        else
        {
            RTPrintf("Remote session has been successfully opened.\n");
        }
    }

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleControlVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc < 2)
        return errorSyntax(USAGE_CONTROLVM, "Not enough parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (a->argv[0]);
    if (!uuid.isEmpty())
    {
        CHECK_ERROR (a->virtualBox, GetMachine (uuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR (a->virtualBox, FindMachine (Bstr(a->argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id) (uuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET (a->virtualBox, OpenExistingSession (a->session, uuid), 1);

    do
    {
        /* get the associated console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK (a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK (a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        /* which command? */
        if (strcmp(a->argv[1], "pause") == 0)
        {
            CHECK_ERROR_BREAK (console, Pause());
        }
        else if (strcmp(a->argv[1], "resume") == 0)
        {
            CHECK_ERROR_BREAK (console, Resume());
        }
        else if (strcmp(a->argv[1], "reset") == 0)
        {
            CHECK_ERROR_BREAK (console, Reset());
        }
        else if (strcmp(a->argv[1], "poweroff") == 0)
        {
            CHECK_ERROR_BREAK (console, PowerDown());
        }
        else if (strcmp(a->argv[1], "savestate") == 0)
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK (console, SaveState(progress.asOutParam()));

            showProgress(progress);

            progress->COMGETTER(ResultCode)(&rc);
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
        else if (strcmp(a->argv[1], "acpipowerbutton") == 0)
        {
            CHECK_ERROR_BREAK (console, PowerButton());
        }
        else if (strcmp(a->argv[1], "acpisleepbutton") == 0)
        {
            CHECK_ERROR_BREAK (console, SleepButton());
        }
        else if (strcmp(a->argv[1], "injectnmi") == 0)
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR_BREAK(console, COMGETTER(Debugger)(debugger.asOutParam()));
            CHECK_ERROR_BREAK(debugger, InjectNMI());
        }
        else if (strcmp(a->argv[1], "keyboardputscancode") == 0)
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
        else if (strncmp(a->argv[1], "setlinkstate", 12) == 0)
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK (a->virtualBox, COMGETTER(SystemProperties) (info.asOutParam()));
            CHECK_ERROR_BREAK (info, COMGETTER(NetworkAdapterCount) (&NetworkAdapterCount));

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
            CHECK_ERROR_BREAK (sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                if (strcmp(a->argv[2], "on") == 0)
                {
                    CHECK_ERROR_BREAK (adapter, COMSETTER(CableConnected)(TRUE));
                }
                else if (strcmp(a->argv[2], "off") == 0)
                {
                    CHECK_ERROR_BREAK (adapter, COMSETTER(CableConnected)(FALSE));
                }
                else
                {
                    errorArgument("Invalid link state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
#ifdef VBOX_WITH_VRDP
        else if (strcmp(a->argv[1], "vrdp") == 0)
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
                if (strcmp(a->argv[2], "on") == 0)
                {
                    CHECK_ERROR_BREAK (vrdpServer, COMSETTER(Enabled)(TRUE));
                }
                else if (strcmp(a->argv[2], "off") == 0)
                {
                    CHECK_ERROR_BREAK (vrdpServer, COMSETTER(Enabled)(FALSE));
                }
                else
                {
                    errorArgument("Invalid vrdp server state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
#endif /* VBOX_WITH_VRDP */
        else if (strcmp (a->argv[1], "usbattach") == 0 ||
                 strcmp (a->argv[1], "usbdetach") == 0)
        {
            if (a->argc < 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Not enough parameters");
                rc = E_FAIL;
                break;
            }

            bool attach = strcmp (a->argv[1], "usbattach") == 0;

            Guid usbId = a->argv [2];
            if (usbId.isEmpty())
            {
                // assume address
                if (attach)
                {
                    ComPtr <IHost> host;
                    CHECK_ERROR_BREAK (a->virtualBox, COMGETTER(Host) (host.asOutParam()));
                    ComPtr <IHostUSBDeviceCollection> coll;
                    CHECK_ERROR_BREAK (host, COMGETTER(USBDevices) (coll.asOutParam()));
                    ComPtr <IHostUSBDevice> dev;
                    CHECK_ERROR_BREAK (coll, FindByAddress (Bstr (a->argv [2]), dev.asOutParam()));
                    CHECK_ERROR_BREAK (dev, COMGETTER(Id) (usbId.asOutParam()));
                }
                else
                {
                    SafeIfaceArray <IUSBDevice> coll;
                    CHECK_ERROR_BREAK (console, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)));
                    ComPtr <IUSBDevice> dev;
                    CHECK_ERROR_BREAK (console, FindUSBDeviceByAddress (Bstr (a->argv [2]),
                                                       dev.asOutParam()));
                    CHECK_ERROR_BREAK (dev, COMGETTER(Id) (usbId.asOutParam()));
                }
            }

            if (attach)
                CHECK_ERROR_BREAK (console, AttachUSBDevice (usbId));
            else
            {
                ComPtr <IUSBDevice> dev;
                CHECK_ERROR_BREAK (console, DetachUSBDevice (usbId, dev.asOutParam()));
            }
        }
        else if (strcmp(a->argv[1], "setvideomodehint") == 0)
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
        else if (strcmp(a->argv[1], "setcredentials") == 0)
        {
            bool fAllowLocalLogon = true;
            if (a->argc == 7)
            {
                if (strcmp(a->argv[5], "-allowlocallogon") != 0)
                {
                    errorArgument("Invalid parameter '%s'", a->argv[5]);
                    rc = E_FAIL;
                    break;
                }
                if (strcmp(a->argv[6], "no") == 0)
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
        else if (strcmp(a->argv[1], "dvdattach") == 0)
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            ComPtr<IDVDDrive> dvdDrive;
            sessionMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
            ASSERT(dvdDrive);

            /* unmount? */
            if (strcmp(a->argv[2], "none") == 0)
            {
                CHECK_ERROR(dvdDrive, Unmount());
            }
            /* host drive? */
            else if (strncmp(a->argv[2], "host:", 5) == 0)
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                com::SafeIfaceArray <IHostDVDDrive> hostDVDs;
                rc = host->COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(hostDVDs));

                ComPtr<IHostDVDDrive> hostDVDDrive;
                rc = host->FindHostDVDDrive(Bstr(a->argv[2] + 5), hostDVDDrive.asOutParam());
                if (!hostDVDDrive)
                {
                    errorArgument("Invalid host DVD drive name");
                    rc = E_FAIL;
                    break;
                }
                CHECK_ERROR(dvdDrive, CaptureHostDrive(hostDVDDrive));
            }
            else
            {
                /* first assume it's a UUID */
                Guid uuid(a->argv[2]);
                ComPtr<IDVDImage> dvdImage;
                rc = a->virtualBox->GetDVDImage(uuid, dvdImage.asOutParam());
                if (FAILED(rc) || !dvdImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindDVDImage(Bstr(a->argv[2]), dvdImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(a->argv[2]), emptyUUID, dvdImage.asOutParam()));
                    }
                }
                if (!dvdImage)
                {
                    rc = E_FAIL;
                    break;
                }
                dvdImage->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(dvdDrive, MountImage(uuid));
            }
        }
        else if (strcmp(a->argv[1], "floppyattach") == 0)
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IFloppyDrive> floppyDrive;
            sessionMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
            ASSERT(floppyDrive);

            /* unmount? */
            if (strcmp(a->argv[2], "none") == 0)
            {
                CHECK_ERROR(floppyDrive, Unmount());
            }
            /* host drive? */
            else if (strncmp(a->argv[2], "host:", 5) == 0)
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                com::SafeIfaceArray <IHostFloppyDrive> hostFloppies;
                rc = host->COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(hostFloppies));
				CheckComRCReturnRC (rc);
                ComPtr<IHostFloppyDrive> hostFloppyDrive;
                host->FindHostFloppyDrive(Bstr(a->argv[2] + 5), hostFloppyDrive.asOutParam());
                if (!hostFloppyDrive)
                {
                    errorArgument("Invalid host floppy drive name");
                    rc = E_FAIL;
                    break;
                }
                CHECK_ERROR(floppyDrive, CaptureHostDrive(hostFloppyDrive));
            }
            else
            {
                /* first assume it's a UUID */
                Guid uuid(a->argv[2]);
                ComPtr<IFloppyImage> floppyImage;
                rc = a->virtualBox->GetFloppyImage(uuid, floppyImage.asOutParam());
                if (FAILED(rc) || !floppyImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindFloppyImage(Bstr(a->argv[2]), floppyImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!floppyImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenFloppyImage(Bstr(a->argv[2]), emptyUUID, floppyImage.asOutParam()));
                    }
                }
                if (!floppyImage)
                {
                    rc = E_FAIL;
                    break;
                }
                floppyImage->COMGETTER(Id)(uuid.asOutParam());
                CHECK_ERROR(floppyDrive, MountImage(uuid));
            }
        }
#ifdef VBOX_WITH_MEM_BALLOONING
        else if (strncmp(a->argv[1], "-guestmemoryballoon", 19) == 0)
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
        else if (strncmp(a->argv[1], "-gueststatisticsinterval", 24) == 0)
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
        else
        {
            errorSyntax(USAGE_CONTROLVM, "Invalid parameter '%s'", Utf8Str(a->argv[1]).raw());
            rc = E_FAIL;
        }
    }
    while (0);

    a->session->Close();

    return SUCCEEDED (rc) ? 0 : 1;
}

static int handleDiscardState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_DISCARDSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Guid guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, DiscardSavedState());
            }
            while (0);
            CHECK_ERROR_BREAK(a->session, Close());
        }
        while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleAdoptdState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 2)
        return errorSyntax(USAGE_ADOPTSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Guid guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, AdoptSavedState (Bstr (a->argv[1])));
            }
            while (0);
            CHECK_ERROR_BREAK(a->session, Close());
        }
        while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleGetExtraData(HandlerArg *a)
{
    HRESULT rc = S_OK;

    if (a->argc != 2)
        return errorSyntax(USAGE_GETEXTRADATA, "Incorrect number of parameters");

    /* global data? */
    if (strcmp(a->argv[0], "global") == 0)
    {
        /* enumeration? */
        if (strcmp(a->argv[1], "enumerate") == 0)
        {
            Bstr extraDataKey;

            do
            {
                Bstr nextExtraDataKey;
                Bstr nextExtraDataValue;
                HRESULT rcEnum = a->virtualBox->GetNextExtraDataKey(extraDataKey, nextExtraDataKey.asOutParam(),
                                                                    nextExtraDataValue.asOutParam());
                extraDataKey = nextExtraDataKey;

                if (SUCCEEDED(rcEnum) && extraDataKey)
                    RTPrintf("Key: %lS, Value: %lS\n", nextExtraDataKey.raw(), nextExtraDataValue.raw());
            } while (extraDataKey);
        }
        else
        {
            Bstr value;
            CHECK_ERROR(a->virtualBox, GetExtraData(Bstr(a->argv[1]), value.asOutParam()));
            if (value)
                RTPrintf("Value: %lS\n", value.raw());
            else
                RTPrintf("No value set!\n");
        }
    }
    else
    {
        ComPtr<IMachine> machine;
        /* assume it's a UUID */
        rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            /* enumeration? */
            if (strcmp(a->argv[1], "enumerate") == 0)
            {
                Bstr extraDataKey;

                do
                {
                    Bstr nextExtraDataKey;
                    Bstr nextExtraDataValue;
                    HRESULT rcEnum = machine->GetNextExtraDataKey(extraDataKey, nextExtraDataKey.asOutParam(),
                                                                  nextExtraDataValue.asOutParam());
                    extraDataKey = nextExtraDataKey;

                    if (SUCCEEDED(rcEnum) && extraDataKey)
                    {
                        RTPrintf("Key: %lS, Value: %lS\n", nextExtraDataKey.raw(), nextExtraDataValue.raw());
                    }
                } while (extraDataKey);
            }
            else
            {
                Bstr value;
                CHECK_ERROR(machine, GetExtraData(Bstr(a->argv[1]), value.asOutParam()));
                if (value)
                    RTPrintf("Value: %lS\n", value.raw());
                else
                    RTPrintf("No value set!\n");
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetExtraData(HandlerArg *a)
{
    HRESULT rc = S_OK;

    if (a->argc < 2)
        return errorSyntax(USAGE_SETEXTRADATA, "Not enough parameters");

    /* global data? */
    if (strcmp(a->argv[0], "global") == 0)
    {
        if (a->argc < 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]), NULL));
        else if (a->argc == 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]), Bstr(a->argv[2])));
        else
            return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
    }
    else
    {
        ComPtr<IMachine> machine;
        /* assume it's a UUID */
        rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            if (a->argc < 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]), NULL));
            else if (a->argc == 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]), Bstr(a->argv[2])));
            else
                return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetProperty(HandlerArg *a)
{
    HRESULT rc;

    /* there must be two arguments: property name and value */
    if (a->argc != 2)
        return errorSyntax(USAGE_SETPROPERTY, "Incorrect number of parameters");

    ComPtr<ISystemProperties> systemProperties;
    a->virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

    if (strcmp(a->argv[0], "hdfolder") == 0)
    {
        /* reset to default? */
        if (strcmp(a->argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(Bstr(a->argv[1])));
    }
    else if (strcmp(a->argv[0], "machinefolder") == 0)
    {
        /* reset to default? */
        if (strcmp(a->argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(Bstr(a->argv[1])));
    }
    else if (strcmp(a->argv[0], "vrdpauthlibrary") == 0)
    {
        /* reset to default? */
        if (strcmp(a->argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(Bstr(a->argv[1])));
    }
    else if (strcmp(a->argv[0], "websrvauthlibrary") == 0)
    {
        /* reset to default? */
        if (strcmp(a->argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(Bstr(a->argv[1])));
    }
    else if (strcmp(a->argv[0], "hwvirtexenabled") == 0)
    {
        if (strcmp(a->argv[1], "yes") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(HWVirtExEnabled)(TRUE));
        else if (strcmp(a->argv[1], "no") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(HWVirtExEnabled)(FALSE));
        else
            return errorArgument("Invalid value '%s' for hardware virtualization extension flag", a->argv[1]);
    }
    else if (strcmp(a->argv[0], "loghistorycount") == 0)
    {
        uint32_t uVal;
        int vrc;
        vrc = RTStrToUInt32Ex(a->argv[1], NULL, 0, &uVal);
        if (vrc != VINF_SUCCESS)
            return errorArgument("Error parsing Log history count '%s'", a->argv[1]);
        CHECK_ERROR(systemProperties, COMSETTER(LogHistoryCount)(uVal));
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", a->argv[0]);

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSharedFolder (HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a command and target */
    if (a->argc < 2)
        return errorSyntax(USAGE_SHAREDFOLDER, "Not enough parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[1]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[1]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Guid uuid;
    machine->COMGETTER(Id)(uuid.asOutParam());

    if (strcmp(a->argv[0], "add") == 0)
    {
        /* we need at least four more parameters */
        if (a->argc < 5)
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Not enough parameters");

        char *name = NULL;
        char *hostpath = NULL;
        bool fTransient = false;
        bool fWritable = true;

        for (int i = 2; i < a->argc; i++)
        {
            if (strcmp(a->argv[i], "-name") == 0)
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                name = a->argv[i];
            }
            else if (strcmp(a->argv[i], "-hostpath") == 0)
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                hostpath = a->argv[i];
            }
            else if (strcmp(a->argv[i], "-readonly") == 0)
            {
                fWritable = false;
            }
            else if (strcmp(a->argv[i], "-transient") == 0)
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
        }

        if (NULL != strstr(name, " "))
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "No spaces allowed in parameter '-name'!");

        /* required arguments */
        if (!name || !hostpath)
        {
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Parameters -name and -hostpath are required");
        }

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenExistingSession (a->session, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (console)
                a->session->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, uuid), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (SUCCEEDED(rc))
                CHECK_ERROR(machine, SaveSettings());

            a->session->Close();
        }
    }
    else if (strcmp(a->argv[0], "remove") == 0)
    {
        /* we need at least two more parameters */
        if (a->argc < 3)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Not enough parameters");

        char *name = NULL;
        bool fTransient = false;

        for (int i = 2; i < a->argc; i++)
        {
            if (strcmp(a->argv[i], "-name") == 0)
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                name = a->argv[i];
            }
            else if (strcmp(a->argv[i], "-transient") == 0)
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
        }

        /* required arguments */
        if (!name)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Parameter -name is required");

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenExistingSession (a->session, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, RemoveSharedFolder(Bstr(name)));

            if (console)
                a->session->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, uuid), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, RemoveSharedFolder(Bstr(name)));

            /* commit and close the session */
            CHECK_ERROR(machine, SaveSettings());
            a->session->Close();
        }
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", Utf8Str(a->argv[0]).raw());

    return 0;
}

static int handleVMStatistics(HandlerArg *a)
{
    HRESULT rc;

    /* at least one option: the UUID or name of the VM */
    if (a->argc < 1)
        return errorSyntax(USAGE_VM_STATISTICS, "Incorrect number of parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (a->argv[0]);
    if (!uuid.isEmpty())
        CHECK_ERROR(a->virtualBox, GetMachine(uuid, machine.asOutParam()));
    else
    {
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(uuid.asOutParam());
    }
    if (FAILED(rc))
        return 1;

    /* parse arguments. */
    bool fReset = false;
    bool fWithDescriptions = false;
    const char *pszPattern = NULL; /* all */
    for (int i = 1; i < a->argc; i++)
    {
        if (!strcmp(a->argv[i], "-pattern"))
        {
            if (pszPattern)
                return errorSyntax(USAGE_VM_STATISTICS, "Multiple -patterns options is not permitted");
            if (i + 1 >= a->argc)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            pszPattern = a->argv[++i];
        }
        else if (!strcmp(a->argv[i], "-descriptions"))
            fWithDescriptions = true;
        /* add: -file <filename> and -formatted */
        else if (!strcmp(a->argv[i], "-reset"))
            fReset = true;
        else
            return errorSyntax(USAGE_VM_STATISTICS, "Unknown option '%s'", a->argv[i]);
    }
    if (fReset && fWithDescriptions)
        return errorSyntax(USAGE_VM_STATISTICS, "The -reset and -descriptions options does not mix");


    /* open an existing session for the VM. */
    CHECK_ERROR(a->virtualBox, OpenExistingSession(a->session, uuid));
    if (SUCCEEDED(rc))
    {
        /* get the session console. */
        ComPtr <IConsole> console;
        CHECK_ERROR(a->session, COMGETTER(Console)(console.asOutParam()));
        if (SUCCEEDED(rc))
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR(console, COMGETTER(Debugger)(debugger.asOutParam()));
            if (SUCCEEDED(rc))
            {
                if (fReset)
                    CHECK_ERROR(debugger, ResetStats(Bstr(pszPattern)));
                else
                {
                    Bstr stats;
                    CHECK_ERROR(debugger, GetStats(Bstr(pszPattern), fWithDescriptions, stats.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        /* if (fFormatted)
                         { big mess }
                         else
                         */
                        RTPrintf("%ls\n", stats.raw());
                    }
                }
            }
            a->session->Close();
        }
    }

    return SUCCEEDED(rc) ? 0 : 1;
}
#endif /* !VBOX_ONLY_DOCS */

enum ConvertSettings
{
    ConvertSettings_No      = 0,
    ConvertSettings_Yes     = 1,
    ConvertSettings_Backup  = 2,
    ConvertSettings_Ignore  = 3,
};

#ifndef VBOX_ONLY_DOCS
/**
 * Checks if any of the settings files were auto-converted and informs the
 * user if so.
 *
 * @return @false if the program should terminate and @true otherwise.
 */
static bool checkForAutoConvertedSettings (ComPtr<IVirtualBox> virtualBox,
                                           ComPtr<ISession> session,
                                           ConvertSettings fConvertSettings)
{
    /* return early if nothing to do */
    if (fConvertSettings == ConvertSettings_Ignore)
        return true;

    HRESULT rc;

    do
    {
        Bstr formatVersion;
        CHECK_ERROR_BREAK(virtualBox, COMGETTER(SettingsFormatVersion) (formatVersion.asOutParam()));

        bool isGlobalConverted = false;
        std::list <ComPtr <IMachine> > cvtMachines;
        std::list <Utf8Str> fileList;
        Bstr version;
        Bstr filePath;

        com::SafeIfaceArray <IMachine> machines;
        CHECK_ERROR_BREAK(virtualBox, COMGETTER(Machines2) (ComSafeArrayAsOutParam (machines)));

        for (size_t i = 0; i < machines.size(); ++ i)
        {
            BOOL accessible;
            CHECK_ERROR_BREAK(machines[i], COMGETTER(Accessible) (&accessible));
            if (!accessible)
                continue;

            CHECK_ERROR_BREAK(machines[i], COMGETTER(SettingsFileVersion) (version.asOutParam()));

            if (version != formatVersion)
            {
                cvtMachines.push_back (machines [i]);
                Bstr filePath;
                CHECK_ERROR_BREAK(machines[i], COMGETTER(SettingsFilePath) (filePath.asOutParam()));
                fileList.push_back (Utf8StrFmt ("%ls  (%ls)", filePath.raw(),
                                                version.raw()));
            }
        }

        if (FAILED(rc))
            break;

        CHECK_ERROR_BREAK(virtualBox, COMGETTER(SettingsFileVersion) (version.asOutParam()));
        if (version != formatVersion)
        {
            isGlobalConverted = true;
            CHECK_ERROR_BREAK(virtualBox, COMGETTER(SettingsFilePath) (filePath.asOutParam()));
            fileList.push_back (Utf8StrFmt ("%ls  (%ls)", filePath.raw(),
                                            version.raw()));
        }

        if (fileList.size() > 0)
        {
            switch (fConvertSettings)
            {
                case ConvertSettings_No:
                {
                    RTPrintf (
"WARNING! The following VirtualBox settings files have been automatically\n"
"converted to the new settings file format version '%ls':\n"
"\n",
                              formatVersion.raw());

                    for (std::list <Utf8Str>::const_iterator f = fileList.begin();
                         f != fileList.end(); ++ f)
                        RTPrintf ("  %S\n", (*f).raw());
                    RTPrintf (
"\n"
"The current command was aborted to prevent overwriting the above settings\n"
"files with the results of the auto-conversion without your permission.\n"
"Please put one of the following command line switches to the beginning of\n"
"the VBoxManage command line and repeat the command:\n"
"\n"
"  -convertSettings       - to save all auto-converted files (it will not\n"
"                           be possible to use these settings files with an\n"
"                           older version of VirtualBox in the future);\n"
"  -convertSettingsBackup - to create backup copies of the settings files in\n"
"                           the old format before saving them in the new format;\n"
"  -convertSettingsIgnore - to not save the auto-converted settings files.\n"
"\n"
"Note that if you use -convertSettingsIgnore, the auto-converted settings files\n"
"will be implicitly saved in the new format anyway once you change a setting or\n"
"start a virtual machine, but NO backup copies will be created in this case.\n");
                    return false;
                }
                case ConvertSettings_Yes:
                case ConvertSettings_Backup:
                {
                    break;
                }
                default:
                    AssertFailedReturn (false);
            }

            for (std::list <ComPtr <IMachine> >::const_iterator m = cvtMachines.begin();
                 m != cvtMachines.end(); ++ m)
            {
                Guid id;
                CHECK_ERROR_BREAK((*m), COMGETTER(Id) (id.asOutParam()));

                /* open a session for the VM */
                CHECK_ERROR_BREAK (virtualBox, OpenSession (session, id));

                ComPtr <IMachine> sm;
                CHECK_ERROR_BREAK(session, COMGETTER(Machine) (sm.asOutParam()));

                Bstr bakFileName;
                if (fConvertSettings == ConvertSettings_Backup)
                    CHECK_ERROR (sm, SaveSettingsWithBackup (bakFileName.asOutParam()));
                else
                    CHECK_ERROR (sm, SaveSettings());

                session->Close();

                if (FAILED(rc))
                    break;
            }

            if (FAILED(rc))
                break;

            if (isGlobalConverted)
            {
                Bstr bakFileName;
                if (fConvertSettings == ConvertSettings_Backup)
                    CHECK_ERROR (virtualBox, SaveSettingsWithBackup (bakFileName.asOutParam()));
                else
                    CHECK_ERROR (virtualBox, SaveSettings());
            }

            if (FAILED(rc))
                break;
        }
    }
    while (0);

    return SUCCEEDED (rc);
}
#endif /* !VBOX_ONLY_DOCS */

// main
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    RTR3Init();

    bool fShowLogo = true;
    int  iCmd      = 1;
    int  iCmdArg;

    ConvertSettings fConvertSettings = ConvertSettings_No;

    /* global options */
    for (int i = 1; i < argc || argc <= iCmd; i++)
    {
        if (    argc <= iCmd
            ||  (strcmp(argv[i], "help")   == 0)
            ||  (strcmp(argv[i], "-?")     == 0)
            ||  (strcmp(argv[i], "-h")     == 0)
            ||  (strcmp(argv[i], "-help")  == 0)
            ||  (strcmp(argv[i], "--help") == 0))
        {
            showLogo();
            printUsage(USAGE_ALL);
            return 0;
        }
        else if (   strcmp(argv[i], "-v") == 0
                 || strcmp(argv[i], "-version") == 0
                 || strcmp(argv[i], "-Version") == 0
                 || strcmp(argv[i], "--version") == 0)
        {
            /* Print version number, and do nothing else. */
            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, VBoxSVNRev ());
            return 0;
        }
        else if (strcmp(argv[i], "-dumpopts") == 0)
        {
            /* Special option to dump really all commands,
             * even the ones not understood on this platform. */
            printUsage(USAGE_DUMPOPTS);
            return 0;
        }
        else if (strcmp(argv[i], "-nologo") == 0)
        {
            /* suppress the logo */
            fShowLogo = false;
            iCmd++;
        }
        else if (strcmp(argv[i], "-convertSettings") == 0)
        {
            fConvertSettings = ConvertSettings_Yes;
            iCmd++;
        }
        else if (strcmp(argv[i], "-convertSettingsBackup") == 0)
        {
            fConvertSettings = ConvertSettings_Backup;
            iCmd++;
        }
        else if (strcmp(argv[i], "-convertSettingsIgnore") == 0)
        {
            fConvertSettings = ConvertSettings_Ignore;
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


#ifdef VBOX_ONLY_DOCS
    int rc = 0;
#else /* !VBOX_ONLY_DOCS */
    HRESULT rc = 0;

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
        return rc;
    }

    /*
     * The input is in the host OS'es codepage (NT guarantees ACP).
     * For VBox we use UTF-8 and convert to UCS-2 when calling (XP)COM APIs.
     * For simplicity, just convert the argv[] array here.
     */
    for (int i = iCmdArg; i < argc; i++)
    {
        char *converted;
        RTStrCurrentCPToUtf8(&converted, argv[i]);
        argv[i] = converted;
    }

    do
    {
    // scopes all the stuff till shutdown
    ////////////////////////////////////////////////////////////////////////////

    /* convertfromraw: does not need a VirtualBox instantiation. */
    if (argc >= iCmdArg && (   !strcmp(argv[iCmd], "convertfromraw")
                            || !strcmp(argv[iCmd], "convertdd")))
    {
        rc = handleConvertFromRaw(argc - iCmdArg, argv + iCmdArg);
        break;
    }

    ComPtr<IVirtualBox> virtualBox;
    ComPtr<ISession> session;

    rc = virtualBox.createLocalObject(CLSID_VirtualBox);
    if (FAILED(rc))
        RTPrintf("ERROR: failed to create the VirtualBox object!\n");
    else
    {
        rc = session.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTPrintf("ERROR: failed to create a session object!\n");
    }

    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(rc);
            RTPrintf("Most likely, the VirtualBox COM server is not running or failed to start.\n");
        }
        else
            GluePrintErrorInfo(info);
        break;
    }

    /* create the event queue
     * (here it is necessary only to process remaining XPCOM/IPC events
     * after the session is closed) */

#ifdef USE_XPCOM_QUEUE
    nsCOMPtr<nsIEventQueue> eventQ;
    NS_GetMainEventQ(getter_AddRefs(eventQ));
#endif

    if (!checkForAutoConvertedSettings (virtualBox, session, fConvertSettings))
        break;

#ifdef USE_XPCOM_QUEUE
    HandlerArg handlerArg = { 0, NULL, eventQ, virtualBox, session };
#else
    HandlerArg handlerArg = { 0, NULL, virtualBox, session };
#endif

    /*
     * All registered command handlers
     */
    struct
    {
        const char *command;
        PFNHANDLER handler;
    } commandHandlers[] =
    {
        { "internalcommands", handleInternalCommands },
        { "list",             handleList },
        { "showvminfo",       handleShowVMInfo },
        { "registervm",       handleRegisterVM },
        { "unregistervm",     handleUnregisterVM },
        { "createhd",         handleCreateHardDisk },
        { "createvdi",        handleCreateHardDisk }, /* backward compatiblity */
        { "modifyhd",         handleModifyHardDisk },
        { "modifyvdi",        handleModifyHardDisk }, /* backward compatiblity */
        { "clonehd",          handleCloneHardDisk },
        { "clonevdi",         handleCloneHardDisk }, /* backward compatiblity */
        { "addiscsidisk",     handleAddiSCSIDisk },
        { "createvm",         handleCreateVM },
        { "modifyvm",         handleModifyVM },
        { "startvm",          handleStartVM },
        { "controlvm",        handleControlVM },
        { "discardstate",     handleDiscardState },
        { "adoptstate",       handleAdoptdState },
        { "snapshot",         handleSnapshot },
        { "openmedium",       handleOpenMedium },
        { "registerimage",    handleOpenMedium }, /* backward compatiblity */
        { "closemedium",      handleCloseMedium },
        { "unregisterimage",  handleCloseMedium }, /* backward compatiblity */
        { "showhdinfo",       handleShowHardDiskInfo },
        { "showvdiinfo",      handleShowHardDiskInfo }, /* backward compatiblity */
        { "getextradata",     handleGetExtraData },
        { "setextradata",     handleSetExtraData },
        { "setproperty",      handleSetProperty },
        { "usbfilter",        handleUSBFilter },
        { "sharedfolder",     handleSharedFolder },
        { "vmstatistics",     handleVMStatistics },
#ifdef VBOX_WITH_GUEST_PROPS
        { "guestproperty",    handleGuestProperty },
#endif /* VBOX_WITH_GUEST_PROPS defined */
        { "metrics",          handleMetrics },
        { "import",           handleImportAppliance },
        { "export",           handleExportAppliance },
        { "hostonlyif",       handleHostonlyIf },
        { NULL,               NULL }
    };

    int commandIndex;
    for (commandIndex = 0; commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (strcmp(commandHandlers[commandIndex].command, argv[iCmd]) == 0)
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            rc = commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!commandHandlers[commandIndex].command)
    {
        rc = errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[iCmd]).raw());
    }

    /* Although all handlers should always close the session if they open it,
     * we do it here just in case if some of the handlers contains a bug --
     * leaving the direct session not closed will turn the machine state to
     * Aborted which may have unwanted side effects like killing the saved
     * state file (if the machine was in the Saved state before). */
    session->Close();

#ifdef USE_XPCOM_QUEUE
    eventQ->ProcessPendingEvents();
#endif

    // end "all-stuff" scope
    ////////////////////////////////////////////////////////////////////////////
    }
    while (0);

    com::Shutdown();
#endif /* !VBOX_ONLY_DOCS */

    /*
     * Free converted argument vector
     */
    for (int i = iCmdArg; i < argc; i++)
        RTStrFree(argv[i]);

    return rc != 0;
}
