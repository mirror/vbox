/* $Id$ */
/** @file
 * VBoxManage - Implementation of modifyvm command.
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
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <vector>
#include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;


/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

int handleModifyVM(HandlerArg *a)
{
    HRESULT rc;
    Bstr name;
    Bstr ostype;
    uint32_t memorySize = 0;
    uint32_t vramSize = 0;
    char *acpi = NULL;
    char *hwvirtex = NULL;
    char *nestedpaging = NULL;
    char *vtxvpid = NULL;
    char *pae = NULL;
    char *ioapic = NULL;
    uint32_t monitorcount = ~0;
    char *accelerate3d = NULL;
    char *bioslogofadein = NULL;
    char *bioslogofadeout = NULL;
    uint32_t bioslogodisplaytime = ~0;
    char *bioslogoimagepath = NULL;
    char *biosbootmenumode = NULL;
    char *biossystemtimeoffset = NULL;
    char *biospxedebug = NULL;
    DeviceType_T bootDevice[4];
    int bootDeviceChanged[4] = { false };
    char *hdds[50] = {0};
    char *dvd = NULL;
    char *dvdpassthrough = NULL;
    char *idecontroller = NULL;
    char *floppy = NULL;
    char *audio = NULL;
    char *audiocontroller = NULL;
    char *clipboard = NULL;
#ifdef VBOX_WITH_VRDP
    char *vrdp = NULL;
    uint16_t vrdpport = UINT16_MAX;
    char *vrdpaddress = NULL;
    char *vrdpauthtype = NULL;
    char *vrdpmulticon = NULL;
    char *vrdpreusecon = NULL;
#endif
    int   fUsbEnabled = -1;
    int   fUsbEhciEnabled = -1;
    char *snapshotFolder = NULL;
    ULONG guestMemBalloonSize = (ULONG)-1;
    ULONG guestStatInterval = (ULONG)-1;
    int   fSataEnabled = -1;
    int   fScsiEnabled = -1;
    int   fScsiLsiLogic = -1;
    int   sataPortCount = -1;
    int   sataBootDevices[4] = {-1,-1,-1,-1};

    /* VM ID + at least one parameter. Parameter arguments are checked
     * individually. */
    if (a->argc < 2)
        return errorSyntax(USAGE_MODIFYVM, "Not enough parameters");

    /* Get the number of network adapters */
    ULONG NetworkAdapterCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(NetworkAdapterCount) (&NetworkAdapterCount), 1);
    }
    ULONG SerialPortCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(SerialPortCount) (&SerialPortCount), 1);
    }

    std::vector <char *> nics (NetworkAdapterCount, 0);
    std::vector <char *> nictype (NetworkAdapterCount, 0);
    std::vector <char *> cableconnected (NetworkAdapterCount, 0);
    std::vector <char *> nictrace (NetworkAdapterCount, 0);
    std::vector <char *> nictracefile (NetworkAdapterCount, 0);
    std::vector <char *> nicspeed (NetworkAdapterCount, 0);
    std::vector <char *> bridgedifdev (NetworkAdapterCount, 0);
    std::vector <char *> hostonlyifdev (NetworkAdapterCount, 0);
    std::vector <const char *> intnet (NetworkAdapterCount, 0);
    std::vector <const char *> natnet (NetworkAdapterCount, 0);
    std::vector <char *> macs (NetworkAdapterCount, 0);
    std::vector <char *> uarts_mode (SerialPortCount, 0);
    std::vector <ULONG>  uarts_base (SerialPortCount, 0);
    std::vector <ULONG>  uarts_irq (SerialPortCount, 0);
    std::vector <char *> uarts_path (SerialPortCount, 0);

    for (int i = 1; i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--name")
            || !strcmp(a->argv[i], "-name"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            name = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--ostype")
                 || !strcmp(a->argv[i], "-ostype"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            ostype = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--memory")
                 || !strcmp(a->argv[i], "-memory"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            memorySize = RTStrToUInt32(a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--vram")
                 || !strcmp(a->argv[i], "-vram"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vramSize = RTStrToUInt32(a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--acpi")
                 || !strcmp(a->argv[i], "-acpi"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            acpi = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--ioapic")
                 || !strcmp(a->argv[i], "-ioapic"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            ioapic = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--hwvirtex")
                 || !strcmp(a->argv[i], "-hwvirtex"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hwvirtex = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--nestedpaging")
                 || !strcmp(a->argv[i], "-nestedpaging"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            nestedpaging = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--vtxvpid")
                 || !strcmp(a->argv[i], "-vtxvpid"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vtxvpid = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--pae")
                 || !strcmp(a->argv[i], "-pae"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            pae = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--monitorcount")
                 || !strcmp(a->argv[i], "-monitorcount"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            monitorcount = RTStrToUInt32(a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--accelerate3d")
                 || !strcmp(a->argv[i], "-accelerate3d"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            accelerate3d = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--bioslogofadein")
                 || !strcmp(a->argv[i], "-bioslogofadein"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            bioslogofadein = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--bioslogofadeout")
                 || !strcmp(a->argv[i], "-bioslogofadeout"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            bioslogofadeout = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--bioslogodisplaytime")
                 || !strcmp(a->argv[i], "-bioslogodisplaytime"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            bioslogodisplaytime = RTStrToUInt32(a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--bioslogoimagepath")
                 || !strcmp(a->argv[i], "-bioslogoimagepath"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            bioslogoimagepath = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--biosbootmenu")
                 || !strcmp(a->argv[i], "-biosbootmenu"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            biosbootmenumode = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--biossystemtimeoffset")
                 || !strcmp(a->argv[i], "-biossystemtimeoffset"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            biossystemtimeoffset = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--biospxedebug")
                 || !strcmp(a->argv[i], "-biospxedebug"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            biospxedebug = a->argv[i];
        }
        else if (   !strncmp(a->argv[i], "--boot", 6)
                 || !strncmp(a->argv[i], "-boot", 5))
        {
            uint32_t n = 0;
            if (!a->argv[i][5 + (a->argv[i][1] == '-')])
                return errorSyntax(USAGE_MODIFYVM, "Missing boot slot number in '%s'", a->argv[i]);
            if (VINF_SUCCESS != RTStrToUInt32Full(&a->argv[i][5 + (a->argv[i][1] == '-')], 10, &n))
                return errorSyntax(USAGE_MODIFYVM, "Invalid boot slot number in '%s'", a->argv[i]);
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "none"))
            {
                bootDevice[n - 1] = DeviceType_Null;
            }
            else if (!strcmp(a->argv[i], "floppy"))
            {
                bootDevice[n - 1] = DeviceType_Floppy;
            }
            else if (!strcmp(a->argv[i], "dvd"))
            {
                bootDevice[n - 1] = DeviceType_DVD;
            }
            else if (!strcmp(a->argv[i], "disk"))
            {
                bootDevice[n - 1] = DeviceType_HardDisk;
            }
            else if (!strcmp(a->argv[i], "net"))
            {
                bootDevice[n - 1] = DeviceType_Network;
            }
            else
                return errorArgument("Invalid boot device '%s'", a->argv[i]);

            bootDeviceChanged[n - 1] = true;
        }
        else if (   !strcmp(a->argv[i], "--hda")
                 || !strcmp(a->argv[i], "-hda"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hdds[0] = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--hdb")
                 || !strcmp(a->argv[i], "-hdb"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hdds[1] = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--hdd")
                 || !strcmp(a->argv[i], "-hdd"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hdds[2] = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--dvd")
                 || !strcmp(a->argv[i], "-dvd"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            dvd = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--dvdpassthrough")
                 || !strcmp(a->argv[i], "-dvdpassthrough"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            dvdpassthrough = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--idecontroller")
                 || !strcmp(a->argv[i], "-idecontroller"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            idecontroller = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--floppy")
                 || !strcmp(a->argv[i], "-floppy"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            floppy = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--audio")
                 || !strcmp(a->argv[i], "-audio"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            audio = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--audiocontroller")
                 || !strcmp(a->argv[i], "-audiocontroller"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            audiocontroller = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--clipboard")
                 || !strcmp(a->argv[i], "-clipboard"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            clipboard = a->argv[i];
        }
        else if (   !strncmp(a->argv[i], "--cableconnected", 16)
                 || !strncmp(a->argv[i], "-cableconnected", 15))
        {
            unsigned n = parseNum(&a->argv[i][15 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;

            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);

            cableconnected[n - 1] = a->argv[i + 1];
            i++;
        }
        /* watch for the right order of these --nic* comparisons! */
        else if (   !strncmp(a->argv[i], "--nictracefile", 14)
                 || !strncmp(a->argv[i], "-nictracefile", 13))
        {
            unsigned n = parseNum(&a->argv[i][13 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            }
            nictracefile[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--nictrace", 10)
                 || !strncmp(a->argv[i], "-nictrace", 9))
        {
            unsigned n = parseNum(&a->argv[i][9 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            nictrace[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--nictype", 9)
                 || !strncmp(a->argv[i], "-nictype", 8))
        {
            unsigned n = parseNum(&a->argv[i][8 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            nictype[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--nicspeed", 10)
                 || !strncmp(a->argv[i], "-nicspeed", 9))
        {
            unsigned n = parseNum(&a->argv[i][9 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            nicspeed[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--nic", 5)
                 || !strncmp(a->argv[i], "-nic", 4))
        {
            unsigned n = parseNum(&a->argv[i][4 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            nics[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--hostifdev", 11)
                 || !strncmp(a->argv[i], "-hostifdev", 10)) /* backward compatibility */
        {
            unsigned n = parseNum(&a->argv[i][10 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            bridgedifdev[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--bridgeadapter", 15)
                 || !strncmp(a->argv[i], "-bridgeadapter", 14))
        {
            unsigned n = parseNum(&a->argv[i][14 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            bridgedifdev[n - 1] = a->argv[i + 1];
            i++;
        }
#if defined(VBOX_WITH_NETFLT)
        else if (   !strncmp(a->argv[i], "--hostonlyadapter", 17)
                 || !strncmp(a->argv[i], "-hostonlyadapter", 16))
        {
            unsigned n = parseNum(&a->argv[i][16 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            hostonlyifdev[n - 1] = a->argv[i + 1];
            i++;
        }
#endif
        else if (   !strncmp(a->argv[i], "--intnet", 8)
                 || !strncmp(a->argv[i], "-intnet", 7))
        {
            unsigned n = parseNum(&a->argv[i][7 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            intnet[n - 1] = a->argv[i + 1];
            i++;
        }
        else if (   !strncmp(a->argv[i], "--natnet", 8)
                 || !strncmp(a->argv[i], "-natnet", 7))
        {
            unsigned n = parseNum(&a->argv[i][7 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);

            if (!strcmp(a->argv[i + 1], "default"))
                natnet[n - 1] = "";
            else
            {
                RTIPV4ADDR Network;
                RTIPV4ADDR Netmask;
                int rc = RTCidrStrToIPv4(a->argv[i + 1], &Network, &Netmask);
                if (RT_FAILURE(rc))
                    return errorArgument("Invalid IPv4 network '%s' specified -- CIDR notation expected.\n", a->argv[i + 1]);
                if (Netmask & 0x1f)
                    return errorArgument("Prefix length of the NAT network must be less than 28.\n");
                natnet[n - 1] = a->argv[i + 1];
            }
            i++;
        }
        else if (   !strncmp(a->argv[i], "--macaddress", 12)
                 || !strncmp(a->argv[i], "-macaddress", 11))
        {
            unsigned n = parseNum(&a->argv[i][11 + (a->argv[i][1] == '-')], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            macs[n - 1] = a->argv[i + 1];
            i++;
        }
#ifdef VBOX_WITH_VRDP
        else if (   !strcmp(a->argv[i], "--vrdp")
                 || !strcmp(a->argv[i], "-vrdp"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vrdp = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--vrdpport")
                 || !strcmp(a->argv[i], "-vrdpport"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "default"))
                vrdpport = 0;
            else
                vrdpport = RTStrToUInt16(a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--vrdpaddress")
                 || !strcmp(a->argv[i], "-vrdpaddress"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vrdpaddress = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--vrdpauthtype")
                 || !strcmp(a->argv[i], "-vrdpauthtype"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vrdpauthtype = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--vrdpmulticon")
                 || !strcmp(a->argv[i], "-vrdpmulticon"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vrdpmulticon = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--vrdpreusecon")
                 || !strcmp(a->argv[i], "-vrdpreusecon"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            vrdpreusecon = a->argv[i];
        }
#endif /* VBOX_WITH_VRDP */
        else if (   !strcmp(a->argv[i], "--usb")
                 || !strcmp(a->argv[i], "-usb"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "on") || !strcmp(a->argv[i], "enable"))
                fUsbEnabled = 1;
            else if (!strcmp(a->argv[i], "off") || !strcmp(a->argv[i], "disable"))
                fUsbEnabled = 0;
            else
                return errorArgument("Invalid --usb argument '%s'", a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--usbehci")
                 || !strcmp(a->argv[i], "-usbehci"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "on") || !strcmp(a->argv[i], "enable"))
                fUsbEhciEnabled = 1;
            else if (!strcmp(a->argv[i], "off") || !strcmp(a->argv[i], "disable"))
                fUsbEhciEnabled = 0;
            else
                return errorArgument("Invalid --usbehci argument '%s'", a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--snapshotfolder")
                 || !strcmp(a->argv[i], "-snapshotfolder"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            snapshotFolder = a->argv[i];
        }
        else if (   !strncmp(a->argv[i], "--uartmode", 10)
                 || !strncmp(a->argv[i], "-uartmode", 9))
        {
            unsigned n = parseNum(&a->argv[i][9 + (a->argv[i][1] == '-')], SerialPortCount, "UART");
            if (!n)
                return 1;
            i++;
            if (!strcmp(a->argv[i], "disconnected"))
            {
                uarts_mode[n - 1] = a->argv[i];
            }
            else
            {
                if (!strcmp(a->argv[i], "server") || !strcmp(a->argv[i], "client"))
                {
                    uarts_mode[n - 1] = a->argv[i];
                    i++;
#ifdef RT_OS_WINDOWS
                    if (!strncmp(a->argv[i], "\\\\.\\pipe\\", 9))
                        return errorArgument("Uart pipe must start with \\\\.\\pipe\\");
#endif
                }
                else
                {
                    uarts_mode[n - 1] = (char*)"device";
                }
                if (a->argc <= i)
                    return errorArgument("Missing argument to --uartmode");
                uarts_path[n - 1] = a->argv[i];
            }
        }
        else if (   !strncmp(a->argv[i], "--uart", 6)
                 || !strncmp(a->argv[i], "-uart", 5))
        {
            unsigned n = parseNum(&a->argv[i][5 + (a->argv[i][1] == '-')], SerialPortCount, "UART");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "off") || !strcmp(a->argv[i], "disable"))
            {
                uarts_base[n - 1] = (ULONG)-1;
            }
            else
            {
                if (a->argc <= i + 1)
                    return errorArgument("Missing argument to '%s'", a->argv[i-1]);
                uint32_t uVal;
                int vrc;
                vrc = RTStrToUInt32Ex(a->argv[i], NULL, 0, &uVal);
                if (vrc != VINF_SUCCESS || uVal == 0)
                    return errorArgument("Error parsing UART I/O base '%s'", a->argv[i]);
                uarts_base[n - 1] = uVal;
                i++;
                vrc = RTStrToUInt32Ex(a->argv[i], NULL, 0, &uVal);
                if (vrc != VINF_SUCCESS)
                    return errorArgument("Error parsing UART IRQ '%s'", a->argv[i]);
                uarts_irq[n - 1]  = uVal;
            }
        }
#ifdef VBOX_WITH_MEM_BALLOONING
        else if (   !strcmp(a->argv[i], "--guestmemoryballoon")
                 || !strcmp(a->argv[i], "-guestmemoryballoon"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[i], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
                return errorArgument("Error parsing guest memory balloon size '%s'", a->argv[i]);
            guestMemBalloonSize = uVal;
        }
#endif
        else if (   !strcmp(a->argv[i], "--gueststatisticsinterval")
                 || !strcmp(a->argv[i], "-gueststatisticsinterval"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[i], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
                return errorArgument("Error parsing guest statistics interval '%s'", a->argv[i]);
            guestStatInterval = uVal;
        }
        else if (   !strcmp(a->argv[i], "--sata")
                 || !strcmp(a->argv[i], "-sata"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "on") || !strcmp(a->argv[i], "enable"))
                fSataEnabled = 1;
            else if (!strcmp(a->argv[i], "off") || !strcmp(a->argv[i], "disable"))
                fSataEnabled = 0;
            else
                return errorArgument("Invalid --usb argument '%s'", a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--sataportcount")
                 || !strcmp(a->argv[i], "-sataportcount"))
        {
            unsigned n;

            if (a->argc <= i + 1)
                return errorArgument("Missing arguments to '%s'", a->argv[i]);
            i++;

            n = parseNum(a->argv[i], 30, "SATA");
            if (!n)
                return 1;
            sataPortCount = n;
        }
        else if (   !strncmp(a->argv[i], "--sataport", 10)
                 || !strncmp(a->argv[i], "-sataport", 9))
        {
            unsigned n = parseNum(&a->argv[i][9 + (a->argv[i][1] == '-')], 30, "SATA");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hdds[n-1+4] = a->argv[i];
        }
        else if (   !strncmp(a->argv[i], "--sataideemulation", 18)
                 || !strncmp(a->argv[i], "-sataideemulation", 17))
        {
            unsigned bootDevicePos = 0;
            unsigned n;

            bootDevicePos = parseNum(&a->argv[i][17 + (a->argv[i][1] == '-')], 4, "SATA");
            if (!bootDevicePos)
                return 1;
            bootDevicePos--;

            if (a->argc <= i + 1)
                return errorArgument("Missing arguments to '%s'", a->argv[i]);
            i++;

            n = parseNum(a->argv[i], 30, "SATA");
            if (!n)
                return 1;

            sataBootDevices[bootDevicePos] = n-1;
        }
        else if (   !strcmp(a->argv[i], "--scsi")
                 || !strcmp(a->argv[i], "-scsi"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!strcmp(a->argv[i], "on") || !strcmp(a->argv[i], "enable"))
                fScsiEnabled = 1;
            else if (!strcmp(a->argv[i], "off") || !strcmp(a->argv[i], "disable"))
                fScsiEnabled = 0;
            else
                return errorArgument("Invalid --scsi argument '%s'", a->argv[i]);
        }
        else if (   !strncmp(a->argv[i], "--scsiport", 10)
                 || !strncmp(a->argv[i], "-scsiport", 9))
        {
            unsigned n = parseNum(&a->argv[i][9 + (a->argv[i][1] == '-')], 16, "SCSI");
            if (!n)
                return 1;
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            hdds[n-1+34] = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--scsitype")
                 || !strcmp(a->argv[i], "-scsitype"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (!RTStrICmp(a->argv[i], "LsiLogic"))
                fScsiLsiLogic = 1;
            else if (!RTStrICmp(a->argv[i], "BusLogic"))
                fScsiLsiLogic = 0;
            else
                return errorArgument("Invalid --scsitype argument '%s'", a->argv[i]);
        }
        else
            return errorSyntax(USAGE_MODIFYVM, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
    }

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (a->argv[0]);
    if (!uuid.isEmpty())
    {
        CHECK_ERROR (a->virtualBox, GetMachine (uuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR (a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(uuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, uuid), 1);

    do
    {
        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(machine.asOutParam());

        ComPtr <IBIOSSettings> biosSettings;
        machine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());

        if (name)
            CHECK_ERROR(machine, COMSETTER(Name)(name));
        if (ostype)
        {
            ComPtr<IGuestOSType> guestOSType;
            CHECK_ERROR(a->virtualBox, GetGuestOSType(ostype, guestOSType.asOutParam()));
            if (SUCCEEDED(rc) && guestOSType)
            {
                CHECK_ERROR(machine, COMSETTER(OSTypeId)(ostype));
            }
            else
            {
                errorArgument("Invalid guest OS type '%s'", Utf8Str(ostype).raw());
                rc = E_FAIL;
                break;
            }
        }
        if (memorySize > 0)
            CHECK_ERROR(machine, COMSETTER(MemorySize)(memorySize));
        if (vramSize > 0)
            CHECK_ERROR(machine, COMSETTER(VRAMSize)(vramSize));
        if (acpi)
        {
            if (!strcmp(acpi, "on"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(ACPIEnabled)(true));
            }
            else if (!strcmp(acpi, "off"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(ACPIEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --acpi argument '%s'", acpi);
                rc = E_FAIL;
                break;
            }
        }
        if (ioapic)
        {
            if (!strcmp(ioapic, "on"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(IOAPICEnabled)(true));
            }
            else if (!strcmp(ioapic, "off"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(IOAPICEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --ioapic argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (hwvirtex)
        {
            if (!strcmp(hwvirtex, "on"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_True));
            }
            else if (!strcmp(hwvirtex, "off"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_False));
            }
            else if (!strcmp(hwvirtex, "default"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_Default));
            }
            else
            {
                errorArgument("Invalid --hwvirtex argument '%s'", hwvirtex);
                rc = E_FAIL;
                break;
            }
        }
        if (nestedpaging)
        {
            if (!strcmp(nestedpaging, "on"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExNestedPagingEnabled)(true));
            }
            else if (!strcmp(nestedpaging, "off"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExNestedPagingEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --nestedpaging argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (vtxvpid)
        {
            if (!strcmp(vtxvpid, "on"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExVPIDEnabled)(true));
            }
            else if (!strcmp(vtxvpid, "off"))
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExVPIDEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --vtxvpid argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (pae)
        {
            if (!strcmp(pae, "on"))
            {
                CHECK_ERROR(machine, COMSETTER(PAEEnabled)(true));
            }
            else if (!strcmp(pae, "off"))
            {
                CHECK_ERROR(machine, COMSETTER(PAEEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --pae argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (monitorcount != ~0U)
        {
            CHECK_ERROR(machine, COMSETTER(MonitorCount)(monitorcount));
        }
        if (accelerate3d)
        {
            if (!strcmp(accelerate3d, "on"))
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate3DEnabled)(true));
            }
            else if (!strcmp(accelerate3d, "off"))
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate3DEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --accelerate3d argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (bioslogofadein)
        {
            if (!strcmp(bioslogofadein, "on"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeIn)(true));
            }
            else if (!strcmp(bioslogofadein, "off"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeIn)(false));
            }
            else
            {
                errorArgument("Invalid --bioslogofadein argument '%s'", bioslogofadein);
                rc = E_FAIL;
                break;
            }
        }
        if (bioslogofadeout)
        {
            if (!strcmp(bioslogofadeout, "on"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeOut)(true));
            }
            else if (!strcmp(bioslogofadeout, "off"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeOut)(false));
            }
            else
            {
                errorArgument("Invalid --bioslogofadeout argument '%s'", bioslogofadeout);
                rc = E_FAIL;
                break;
            }
        }
        if (bioslogodisplaytime != ~0U)
        {
            CHECK_ERROR(biosSettings, COMSETTER(LogoDisplayTime)(bioslogodisplaytime));
        }
        if (bioslogoimagepath)
        {
            CHECK_ERROR(biosSettings, COMSETTER(LogoImagePath)(Bstr(bioslogoimagepath)));
        }
        if (biosbootmenumode)
        {
            if (!strcmp(biosbootmenumode, "disabled"))
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_Disabled));
            else if (!strcmp(biosbootmenumode, "menuonly"))
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MenuOnly));
            else if (!strcmp(biosbootmenumode, "messageandmenu"))
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MessageAndMenu));
            else
            {
                errorArgument("Invalid --biosbootmenu argument '%s'", biosbootmenumode);
                rc = E_FAIL;
                break;
            }

        }
        if (biossystemtimeoffset)
        {
            LONG64 timeOffset = RTStrToInt64(biossystemtimeoffset);
            CHECK_ERROR(biosSettings, COMSETTER(TimeOffset)(timeOffset));
        }
        if (biospxedebug)
        {
            if (!strcmp(biospxedebug, "on"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(PXEDebugEnabled)(true));
            }
            else if (!strcmp(biospxedebug, "off"))
            {
                CHECK_ERROR(biosSettings, COMSETTER(PXEDebugEnabled)(false));
            }
            else
            {
                errorArgument("Invalid --biospxedebug argument '%s'", biospxedebug);
                rc = E_FAIL;
                break;
            }
        }
        for (int curBootDev = 0; curBootDev < 4; curBootDev++)
        {
            if (bootDeviceChanged[curBootDev])
                CHECK_ERROR(machine, SetBootOrder (curBootDev + 1, bootDevice[curBootDev]));
        }
        if (hdds[0])
        {
            if (!strcmp(hdds[0], "none"))
            {
                machine->DetachHardDisk(Bstr("IDE"), 0, 0);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[0]);
                ComPtr<IHardDisk> hardDisk;
                rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(a->virtualBox, FindHardDisk(Bstr(hdds[0]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(a->virtualBox, OpenHardDisk(Bstr(hdds[0]), AccessMode_ReadWrite, hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk(uuid, Bstr("IDE"), 0, 0));
                }
                else
                    rc = E_FAIL;
                if (FAILED(rc))
                    break;
            }
        }
        if (hdds[1])
        {
            if (!strcmp(hdds[1], "none"))
            {
                machine->DetachHardDisk(Bstr("IDE"), 0, 1);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[1]);
                ComPtr<IHardDisk> hardDisk;
                rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(a->virtualBox, FindHardDisk(Bstr(hdds[1]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(a->virtualBox, OpenHardDisk(Bstr(hdds[1]), AccessMode_ReadWrite, hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk(uuid, Bstr("IDE"), 0, 1));
                }
                else
                    rc = E_FAIL;
                if (FAILED(rc))
                    break;
            }
        }
        if (hdds[2])
        {
            if (!strcmp(hdds[2], "none"))
            {
                machine->DetachHardDisk(Bstr("IDE"), 1, 1);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[2]);
                ComPtr<IHardDisk> hardDisk;
                rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(a->virtualBox, FindHardDisk(Bstr(hdds[2]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(a->virtualBox, OpenHardDisk(Bstr(hdds[2]), AccessMode_ReadWrite, hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk(uuid, Bstr("IDE"), 1, 1));
                }
                else
                    rc = E_FAIL;
                if (FAILED(rc))
                    break;
            }
        }
        if (dvd)
        {
            ComPtr<IDVDDrive> dvdDrive;
            machine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
            ASSERT(dvdDrive);

            /* unmount? */
            if (!strcmp(dvd, "none"))
            {
                CHECK_ERROR(dvdDrive, Unmount());
            }
            /* host drive? */
            else if (!strncmp(dvd, "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                com::SafeIfaceArray <IHostDVDDrive> hostDVDs;
                rc = host->COMGETTER(DVDDrives)(ComSafeArrayAsOutParam(hostDVDs));

                ComPtr<IHostDVDDrive> hostDVDDrive;
                rc = host->FindHostDVDDrive(Bstr(dvd + 5), hostDVDDrive.asOutParam());
                if (!hostDVDDrive)
                {
                    /* 2nd try: try with the real name, important on Linux+libhal */
                    char szPathReal[RTPATH_MAX];
                    if (RT_FAILURE(RTPathReal(dvd + 5, szPathReal, sizeof(szPathReal))))
                    {
                        errorArgument("Invalid host DVD drive name");
                        rc = E_FAIL;
                        break;
                    }
                    rc = host->FindHostDVDDrive(Bstr(szPathReal), hostDVDDrive.asOutParam());
                    if (!hostDVDDrive)
                    {
                        errorArgument("Invalid host DVD drive name");
                        rc = E_FAIL;
                        break;
                    }
                }
                CHECK_ERROR(dvdDrive, CaptureHostDrive(hostDVDDrive));
            }
            else
            {
                /* first assume it's a UUID */
                Guid uuid(dvd);
                ComPtr<IDVDImage> dvdImage;
                rc = a->virtualBox->GetDVDImage(uuid, dvdImage.asOutParam());
                if (FAILED(rc) || !dvdImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindDVDImage(Bstr(dvd), dvdImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(dvd), emptyUUID, dvdImage.asOutParam()));
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
        if (dvdpassthrough)
        {
            ComPtr<IDVDDrive> dvdDrive;
            machine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
            ASSERT(dvdDrive);

            CHECK_ERROR(dvdDrive, COMSETTER(Passthrough)(!strcmp(dvdpassthrough, "on")));
        }
        if (idecontroller)
        {
            ComPtr<IStorageController> storageController;
            CHECK_ERROR(machine, GetStorageControllerByName(Bstr("IDE"), storageController.asOutParam()));

            if (RTStrICmp(idecontroller, "PIIX3"))
            {
                CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
            }
            else if (RTStrICmp(idecontroller, "PIIX4"))
            {
                CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
            }
            else if (RTStrICmp(idecontroller, "ICH6"))
            {
                CHECK_ERROR(storageController, COMSETTER(ControllerType)(StorageControllerType_ICH6));
            }
            else
            {
                errorArgument("Invalid --idecontroller argument '%s'", idecontroller);
                rc = E_FAIL;
                break;
            }
        }
        if (floppy)
        {
            ComPtr<IFloppyDrive> floppyDrive;
            machine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
            ASSERT(floppyDrive);

            /* disable? */
            if (!strcmp(floppy, "disabled"))
            {
                /* disable the controller */
                CHECK_ERROR(floppyDrive, COMSETTER(Enabled)(false));
            }
            else
            {
                /* enable the controller */
                CHECK_ERROR(floppyDrive, COMSETTER(Enabled)(true));

                /* unmount? */
                if (!strcmp(floppy, "empty"))
                {
                    CHECK_ERROR(floppyDrive, Unmount());
                }
                /* host drive? */
                else if (!strncmp(floppy, "host:", 5))
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    com::SafeIfaceArray <IHostFloppyDrive> hostFloppies;
                    CHECK_ERROR(host, COMGETTER(FloppyDrives)(ComSafeArrayAsOutParam(hostFloppies)));
                    ComPtr<IHostFloppyDrive> hostFloppyDrive;
                    rc = host->FindHostFloppyDrive(Bstr(floppy + 5), hostFloppyDrive.asOutParam());
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
                    Guid uuid(floppy);
                    ComPtr<IFloppyImage> floppyImage;
                    rc = a->virtualBox->GetFloppyImage(uuid, floppyImage.asOutParam());
                    if (FAILED(rc) || !floppyImage)
                    {
                        /* must be a filename, check if it's in the collection */
                        rc = a->virtualBox->FindFloppyImage(Bstr(floppy), floppyImage.asOutParam());
                        /* not registered, do that on the fly */
                        if (!floppyImage)
                        {
                            Guid emptyUUID;
                            CHECK_ERROR(a->virtualBox, OpenFloppyImage(Bstr(floppy), emptyUUID, floppyImage.asOutParam()));
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
        }
        if (audio || audiocontroller)
        {
            ComPtr<IAudioAdapter> audioAdapter;
            machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
            ASSERT(audioAdapter);

            if (audio)
            {
                /* disable? */
                if (!strcmp(audio, "none"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(false));
                }
                else if (!strcmp(audio, "null"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Null));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
                else if (!strcmp(audio, "winmm"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_WinMM));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif
                else if (!strcmp(audio, "dsound"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_DirectSound));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_LINUX
                else if (!strcmp(audio, "oss"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# ifdef VBOX_WITH_ALSA
                else if (!strcmp(audio, "alsa"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_ALSA));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
# ifdef VBOX_WITH_PULSE
                else if (!strcmp(audio, "pulse"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Pulse));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
#endif /* !RT_OS_LINUX */
#ifdef RT_OS_SOLARIS
                else if (!strcmp(audio, "solaudio"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_SolAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

#endif /* !RT_OS_SOLARIS */
#ifdef RT_OS_DARWIN
                else if (!strcmp(audio, "coreaudio"))
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_CoreAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

#endif /* !RT_OS_DARWIN */
                else
                {
                    errorArgument("Invalid --audio argument '%s'", audio);
                    rc = E_FAIL;
                    break;
                }
            }
            if (audiocontroller)
            {
                if (!strcmp(audiocontroller, "sb16"))
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_SB16));
                else if (!strcmp(audiocontroller, "ac97"))
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_AC97));
                else
                {
                    errorArgument("Invalid --audiocontroller argument '%s'", audiocontroller);
                    rc = E_FAIL;
                    break;
                }
            }
        }
        /* Shared clipboard state */
        if (clipboard)
        {
/*            ComPtr<IClipboardMode> clipboardMode;
            machine->COMGETTER(ClipboardMode)(clipboardMode.asOutParam());
            ASSERT(clipboardMode);
*/
            if (!strcmp(clipboard, "disabled"))
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Disabled));
            }
            else if (!strcmp(clipboard, "hosttoguest"))
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_HostToGuest));
            }
            else if (!strcmp(clipboard, "guesttohost"))
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_GuestToHost));
            }
            else if (!strcmp(clipboard, "bidirectional"))
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Bidirectional));
            }
            else
            {
                errorArgument("Invalid --clipboard argument '%s'", clipboard);
                rc = E_FAIL;
                break;
            }
        }
        /* iterate through all possible NICs */
        for (ULONG n = 0; n < NetworkAdapterCount; n ++)
        {
            ComPtr<INetworkAdapter> nic;
            CHECK_ERROR_RET (machine, GetNetworkAdapter (n, nic.asOutParam()), 1);

            ASSERT(nic);

            /* something about the NIC? */
            if (nics[n])
            {
                if (!strcmp(nics[n], "none"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (FALSE), 1);
                }
                else if (!strcmp(nics[n], "null"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, Detach(), 1);
                }
                else if (!strcmp(nics[n], "nat"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToNAT(), 1);
                }
                else if (  !strcmp(nics[n], "bridged")
                        || !strcmp(nics[n], "hostif")) /* backward compatibility */
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToBridgedInterface(), 1);
                }
                else if (!strcmp(nics[n], "intnet"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToInternalNetwork(), 1);
                }
#if defined(VBOX_WITH_NETFLT)
                else if (!strcmp(nics[n], "hostonly"))
                {

                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToHostOnlyInterface(), 1);
                }
#endif
                else
                {
                    errorArgument("Invalid type '%s' specfied for NIC %lu", nics[n], n + 1);
                    rc = E_FAIL;
                    break;
                }
            }

            /* something about the NIC type? */
            if (nictype[n])
            {
                if (!strcmp(nictype[n], "Am79C970A"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C970A), 1);
                }
                else if (!strcmp(nictype[n], "Am79C973"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C973), 1);
                }
#ifdef VBOX_WITH_E1000
                else if (!strcmp(nictype[n], "82540EM"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82540EM), 1);
                }
                else if (!strcmp(nictype[n], "82543GC"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82543GC), 1);
                }
                else if (!strcmp(nictype[n], "82545EM"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82545EM), 1);
                }
#endif
                else
                {
                    errorArgument("Invalid NIC type '%s' specified for NIC %lu", nictype[n], n + 1);
                    rc = E_FAIL;
                    break;
                }
            }

            /* something about the MAC address? */
            if (macs[n])
            {
                /* generate one? */
                if (!strcmp(macs[n], "auto"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(MACAddress)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(MACAddress)(Bstr(macs[n])), 1);
                }
            }

            /* something about the reported link speed? */
            if (nicspeed[n])
            {
                uint32_t    u32LineSpeed;

                u32LineSpeed = RTStrToUInt32(nicspeed[n]);

                if (u32LineSpeed < 1000 || u32LineSpeed > 4000000)
                {
                    errorArgument("Invalid --nicspeed%lu argument '%s'", n + 1, nicspeed[n]);
                    rc = E_FAIL;
                    break;
                }
                CHECK_ERROR_RET(nic, COMSETTER(LineSpeed)(u32LineSpeed), 1);
            }

            /* the link status flag? */
            if (cableconnected[n])
            {
                if (!strcmp(cableconnected[n], "on"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(CableConnected)(TRUE), 1);
                }
                else if (!strcmp(cableconnected[n], "off"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(CableConnected)(FALSE), 1);
                }
                else
                {
                    errorArgument("Invalid --cableconnected%lu argument '%s'", n + 1, cableconnected[n]);
                    rc = E_FAIL;
                    break;
                }
            }

            /* the trace flag? */
            if (nictrace[n])
            {
                if (!strcmp(nictrace[n], "on"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TraceEnabled)(TRUE), 1);
                }
                else if (!strcmp(nictrace[n], "off"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TraceEnabled)(FALSE), 1);
                }
                else
                {
                    errorArgument("Invalid --nictrace%lu argument '%s'", n + 1, nictrace[n]);
                    rc = E_FAIL;
                    break;
                }
            }

            /* the tracefile flag? */
            if (nictracefile[n])
            {
                CHECK_ERROR_RET(nic, COMSETTER(TraceFile)(Bstr(nictracefile[n])), 1);
            }

            /* the host interface device? */
            if (hostonlyifdev[n])
            {
                /* remove it? */
                if (!strcmp(hostonlyifdev[n], "none"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(HostOnlyInterface)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(HostOnlyInterface)(Bstr(hostonlyifdev[n])), 1);
                }
            }

            if (bridgedifdev[n])
            {
                /* remove it? */
                if (!strcmp(bridgedifdev[n], "none"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(BridgedInterface)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(BridgedInterface)(Bstr(bridgedifdev[n])), 1);
                }
            }

            /* the internal network name? */
            if (intnet[n])
            {
                /* remove it? */
                if (!strcmp(intnet[n], "none"))
                {
                    CHECK_ERROR_RET(nic, COMSETTER(InternalNetwork)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(InternalNetwork)(Bstr(intnet[n])), 1);
                }
            }
            /* the network of the NAT */
            if (natnet[n])
            {
                CHECK_ERROR_RET(nic, COMSETTER(NATNetwork)(Bstr(natnet[n])), 1);
            }
        }
        if (FAILED(rc))
            break;

        /* iterate through all possible serial ports */
        for (ULONG n = 0; n < SerialPortCount; n ++)
        {
            ComPtr<ISerialPort> uart;
            CHECK_ERROR_RET (machine, GetSerialPort (n, uart.asOutParam()), 1);

            ASSERT(uart);

            if (uarts_base[n])
            {
                if (uarts_base[n] == (ULONG)-1)
                {
                    CHECK_ERROR_RET(uart, COMSETTER(Enabled) (FALSE), 1);
                }
                else
                {
                    CHECK_ERROR_RET(uart, COMSETTER(IOBase) (uarts_base[n]), 1);
                    CHECK_ERROR_RET(uart, COMSETTER(IRQ) (uarts_irq[n]), 1);
                    CHECK_ERROR_RET(uart, COMSETTER(Enabled) (TRUE), 1);
                }
            }
            if (uarts_mode[n])
            {
                if (!strcmp(uarts_mode[n], "disconnected"))
                {
                    CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_Disconnected), 1);
                }
                else
                {
                    CHECK_ERROR_RET(uart, COMSETTER(Path) (Bstr(uarts_path[n])), 1);
                    if (!strcmp(uarts_mode[n], "server"))
                    {
                        CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_HostPipe), 1);
                        CHECK_ERROR_RET(uart, COMSETTER(Server) (TRUE), 1);
                    }
                    else if (!strcmp(uarts_mode[n], "client"))
                    {
                        CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_HostPipe), 1);
                        CHECK_ERROR_RET(uart, COMSETTER(Server) (FALSE), 1);
                    }
                    else
                    {
                        CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_HostDevice), 1);
                    }
                }
            }
        }
        if (FAILED(rc))
            break;

#ifdef VBOX_WITH_VRDP
        if (vrdp || (vrdpport != UINT16_MAX) || vrdpaddress || vrdpauthtype || vrdpmulticon || vrdpreusecon)
        {
            ComPtr<IVRDPServer> vrdpServer;
            machine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
            ASSERT(vrdpServer);
            if (vrdpServer)
            {
                if (vrdp)
                {
                    if (!strcmp(vrdp, "on"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(Enabled)(true));
                    }
                    else if (!strcmp(vrdp, "off"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(Enabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdp argument '%s'", vrdp);
                        rc = E_FAIL;
                        break;
                    }
                }
                if (vrdpport != UINT16_MAX)
                {
                    CHECK_ERROR(vrdpServer, COMSETTER(Port)(vrdpport));
                }
                if (vrdpaddress)
                {
                    CHECK_ERROR(vrdpServer, COMSETTER(NetAddress)(Bstr(vrdpaddress)));
                }
                if (vrdpauthtype)
                {
                    if (!strcmp(vrdpauthtype, "null"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Null));
                    }
                    else if (!strcmp(vrdpauthtype, "external"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_External));
                    }
                    else if (!strcmp(vrdpauthtype, "guest"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Guest));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpauthtype argument '%s'", vrdpauthtype);
                        rc = E_FAIL;
                        break;
                    }
                }
                if (vrdpmulticon)
                {
                    if (!strcmp(vrdpmulticon, "on"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AllowMultiConnection)(true));
                    }
                    else if (!strcmp(vrdpmulticon, "off"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AllowMultiConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpmulticon argument '%s'", vrdpmulticon);
                        rc = E_FAIL;
                        break;
                    }
                }
                if (vrdpreusecon)
                {
                    if (!strcmp(vrdpreusecon, "on"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(ReuseSingleConnection)(true));
                    }
                    else if (!strcmp(vrdpreusecon, "off"))
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(ReuseSingleConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid --vrdpreusecon argument '%s'", vrdpreusecon);
                        rc = E_FAIL;
                        break;
                    }
                }
            }
        }
#endif /* VBOX_WITH_VRDP */

        /*
         * USB enable/disable
         */
        if (fUsbEnabled != -1)
        {
            ComPtr<IUSBController> UsbCtl;
            CHECK_ERROR(machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(UsbCtl, COMSETTER(Enabled)(!!fUsbEnabled));
            }
        }
        /*
         * USB EHCI enable/disable
         */
        if (fUsbEhciEnabled != -1)
        {
            ComPtr<IUSBController> UsbCtl;
            CHECK_ERROR(machine, COMGETTER(USBController)(UsbCtl.asOutParam()));
            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(UsbCtl, COMSETTER(EnabledEhci)(!!fUsbEhciEnabled));
            }
        }

        if (snapshotFolder)
        {
            if (!strcmp(snapshotFolder, "default"))
            {
                CHECK_ERROR(machine, COMSETTER(SnapshotFolder)(NULL));
            }
            else
            {
                CHECK_ERROR(machine, COMSETTER(SnapshotFolder)(Bstr(snapshotFolder)));
            }
        }

        if (guestMemBalloonSize != (ULONG)-1)
            CHECK_ERROR(machine, COMSETTER(MemoryBalloonSize)(guestMemBalloonSize));

        if (guestStatInterval != (ULONG)-1)
            CHECK_ERROR(machine, COMSETTER(StatisticsUpdateInterval)(guestStatInterval));

        /*
         * SATA controller enable/disable
         */
        if (fSataEnabled != -1)
        {
            if (fSataEnabled)
            {
                ComPtr<IStorageController> ctl;
                CHECK_ERROR(machine, AddStorageController(Bstr("SATA"), StorageBus_SATA, ctl.asOutParam()));
                CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
            }
            else
                CHECK_ERROR(machine, RemoveStorageController(Bstr("SATA")));
        }

        for (uint32_t i = 4; i < 34; i++)
        {
            if (hdds[i])
            {
                if (!strcmp(hdds[i], "none"))
                {
                    machine->DetachHardDisk(Bstr("SATA"), i-4, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Guid uuid(hdds[i]);
                    ComPtr<IHardDisk> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        CHECK_ERROR(a->virtualBox, FindHardDisk(Bstr(hdds[i]), hardDisk.asOutParam()));
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox, OpenHardDisk(Bstr(hdds[i]), AccessMode_ReadWrite, hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine, AttachHardDisk(uuid, Bstr("SATA"), i-4, 0));
                    }
                    else
                        rc = E_FAIL;
                    if (FAILED(rc))
                        break;
                }
            }
        }

        for (uint32_t i = 0; i < 4; i++)
        {
            if (sataBootDevices[i] != -1)
            {
                ComPtr<IStorageController> SataCtl;
                CHECK_ERROR(machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));
                if (SUCCEEDED(rc))
                {
                    CHECK_ERROR(SataCtl, SetIDEEmulationPort(i, sataBootDevices[i]));
                }
            }
        }

        if (sataPortCount != -1)
        {
            ComPtr<IStorageController> SataCtl;
            CHECK_ERROR(machine, GetStorageControllerByName(Bstr("SATA"), SataCtl.asOutParam()));
            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(SataCtl, COMSETTER(PortCount)(sataPortCount));
            }
        }

        /*
         * SCSI controller enable/disable
         */
        if (fScsiEnabled != -1)
        {
            if (fScsiEnabled)
            {
                ComPtr<IStorageController> ctl;
                if (!fScsiLsiLogic)
                {
                    CHECK_ERROR(machine, AddStorageController(Bstr("BusLogic"), StorageBus_SCSI, ctl.asOutParam()));
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else /* LsiLogic is default */
                {
                    CHECK_ERROR(machine, AddStorageController(Bstr("LsiLogic"), StorageBus_SCSI, ctl.asOutParam()));
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
            }
            else
            {
                rc = machine->RemoveStorageController(Bstr("LsiLogic"));
                if (!SUCCEEDED(rc))
                    CHECK_ERROR(machine, RemoveStorageController(Bstr("BusLogic")));
            }
        }

        for (uint32_t i = 34; i < 50; i++)
        {
            if (hdds[i])
            {
                if (!strcmp(hdds[i], "none"))
                {
                    rc = machine->DetachHardDisk(Bstr("LsiLogic"), i-34, 0);
                    if (!SUCCEEDED(rc))
                        CHECK_ERROR(machine, DetachHardDisk(Bstr("BusLogic"), i-34, 0));
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Guid uuid(hdds[i]);
                    ComPtr<IHardDisk> hardDisk;
                    rc = a->virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        CHECK_ERROR(a->virtualBox, FindHardDisk(Bstr(hdds[i]), hardDisk.asOutParam()));
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(a->virtualBox, OpenHardDisk(Bstr(hdds[i]), AccessMode_ReadWrite, hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        rc = machine->AttachHardDisk(uuid, Bstr("LsiLogic"), i-34, 0);
                        if (!SUCCEEDED(rc))
                            CHECK_ERROR(machine, AttachHardDisk(uuid, Bstr("BusLogic"), i-34, 0));
                    }
                    else
                        rc = E_FAIL;
                    if (FAILED(rc))
                        break;
                }
            }
        }

        /* commit changes */
        CHECK_ERROR(machine, SaveSettings());
    }
    while (0);

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */
