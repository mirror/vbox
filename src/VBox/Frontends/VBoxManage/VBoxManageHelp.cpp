/* $Id$ */
/** @file
 * VBoxManage - help and other message output.
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

#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>

#include "VBoxManage.h"

void printUsage(USAGECATEGORY u64Cmd)
{
#ifdef RT_OS_LINUX
    bool fLinux = true;
#else
    bool fLinux = false;
#endif
#ifdef RT_OS_WINDOWS
    bool fWin = true;
#else
    bool fWin = false;
#endif
#ifdef RT_OS_SOLARIS
    bool fSolaris = true;
#else
    bool fSolaris = false;
#endif
#ifdef RT_OS_DARWIN
    bool fDarwin = true;
#else
    bool fDarwin = false;
#endif
#ifdef VBOX_WITH_VRDP
    bool fVRDP = true;
#else
    bool fVRDP = false;
#endif
#ifdef VBOX_WITH_VBOXSDL
    bool fVBoxSDL = true;
#else
    bool fVBoxSDL = false;
#endif

    if (u64Cmd == USAGE_DUMPOPTS)
    {
        fLinux = true;
        fWin = true;
        fSolaris = true;
        fDarwin = true;
        fVRDP = true;
        fVBoxSDL = true;
        u64Cmd = USAGE_ALL;
    }

    RTPrintf("Usage:\n"
             "\n");

    if (u64Cmd == USAGE_ALL)
    {
        RTPrintf("VBoxManage [-v|--version]    print version number and exit\n"
                 "VBoxManage [-q|--nologo] ... suppress the logo\n"
                 "\n");
    }

    if (u64Cmd & USAGE_LIST)
    {
        RTPrintf("VBoxManage list [--long|-l] vms|runningvms|ostypes|hostdvds|hostfloppies|\n"
#if defined(VBOX_WITH_NETFLT)
                 "                            bridgedifs|hostonlyifs|dhcpservers|hostinfo|\n"
#else
                 "                            bridgedifs|dhcpservers|hostinfo|\n"
#endif
                 "                            hostcpuids|hddbackends|hdds|dvds|floppies|\n"
                 "                            usbhost|usbfilters|systemproperties\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHOWVMINFO)
    {
        RTPrintf("VBoxManage showvminfo       <uuid>|<name> [--details] [--statistics]\n"
                 "                            [--machinereadable]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_REGISTERVM)
    {
        RTPrintf("VBoxManage registervm       <filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_UNREGISTERVM)
    {
        RTPrintf("VBoxManage unregistervm     <uuid>|<name> [--delete]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEVM)
    {
        RTPrintf("VBoxManage createvm         --name <name>\n"
                 "                            [--ostype <ostype>]\n"
                 "                            [--register]\n"
                 "                            [--basefolder <path> | --settingsfile <path>]\n"
                 "                            [--uuid <uuid>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_MODIFYVM)
    {
        RTPrintf("VBoxManage modifyvm         <uuid|name>\n"
                 "                            [--name <name>]\n"
                 "                            [--ostype <ostype>]\n"
                 "                            [--memory <memorysize in MB>]\n"
                 "                            [--vram <vramsize in MB>]\n"
                 "                            [--acpi on|off]\n"
                 "                            [--ioapic on|off]\n"
                 "                            [--pae on|off]\n"
                 "                            [--hwvirtex on|off]\n"
                 "                            [--nestedpaging on|off]\n"
                 "                            [--vtxvpid on|off]\n"
                 "                            [--cpuidset <leaf> <eax> <ebx> <ecx> <edx>]\n"
                 "                            [--cpuidremove <leaf>]\n"
                 "                            [--cpuidremoveall]\n"
                 "                            [--cpus <number>]\n"
                 "                            [--monitorcount <number>]\n"
                 "                            [--accelerate3d <on|off>]\n"
#ifdef VBOX_WITH_VIDEOHWACCEL
                 "                            [--accelerate2dvideo <on|off>]\n"
#endif
                 "                            [--firmware bios|efi]\n"
                 "                            [--bioslogofadein on|off]\n"
                 "                            [--bioslogofadeout on|off]\n"
                 "                            [--bioslogodisplaytime <msec>]\n"
                 "                            [--bioslogoimagepath <imagepath>]\n"
                 "                            [--biosbootmenu disabled|menuonly|messageandmenu]\n"
                 "                            [--biossystemtimeoffset <msec>]\n"
                 "                            [--biospxedebug on|off]\n"
                 "                            [--boot<1-4> none|floppy|dvd|disk|net>]\n"
#if defined(VBOX_WITH_NETFLT)
                 "                            [--nic<1-N> none|null|nat|bridged|intnet|hostonly]\n"
#else /* !RT_OS_LINUX && !RT_OS_DARWIN */
                 "                            [--nic<1-N> none|null|nat|bridged|intnet]\n"
#endif /* !RT_OS_LINUX && !RT_OS_DARWIN  */
                 "                            [--nictype<1-N> Am79C970A|Am79C973"
#ifdef VBOX_WITH_E1000
              "|\n                                            82540EM|82543GC|82545EM"
#endif
#ifdef VBOX_WITH_VIRTIO
              "|\n                                            virtio"
#endif /* VBOX_WITH_VIRTIO */
                 "]\n"
                 "                            [--cableconnected<1-N> on|off]\n"
                 "                            [--nictrace<1-N> on|off]\n"
                 "                            [--nictracefile<1-N> <filename>]\n"
                 "                            [--nicspeed<1-N> <kbps>]\n"
                 "                            [--bridgeadapter<1-N> none|<devicename>]\n"
#if defined(VBOX_WITH_NETFLT)
                 "                            [--hostonlyadapter<1-N> none|<devicename>]\n"
#endif
                 "                            [--intnet<1-N> <network name>]\n"
                 "                            [--natnet<1-N> <network>|default]\n"
                 "                            [--macaddress<1-N> auto|<mac>]\n"
                 "                            [--uart<1-N> off|<I/O base> <IRQ>]\n"
                 "                            [--uartmode<1-N> disconnected|\n"
                 "                                             server <pipe>|\n"
                 "                                             client <pipe>|\n"
                 "                                             file <file>|\n"
                 "                                             <devicename>]\n"
#ifdef VBOX_WITH_MEM_BALLOONING
                 "                            [--guestmemoryballoon <balloonsize in MB>]\n"
#endif
                 "                            [--gueststatisticsinterval <seconds>]\n"
                 );
        RTPrintf("                            [--audio none|null");
        if (fWin)
        {
#ifdef VBOX_WITH_WINMM
            RTPrintf(                        "|winmm|dsound");
#else
            RTPrintf(                        "|dsound");
#endif
        }
        if (fSolaris)
        {
            RTPrintf(                        "|solaudio"
#ifdef VBOX_WITH_SOLARIS_OSS
                                             "|oss"
#endif
                                              );
        }
        if (fLinux)
        {
            RTPrintf(                        "|oss"
#ifdef VBOX_WITH_ALSA
                                             "|alsa"
#endif
#ifdef VBOX_WITH_PULSE
                                             "|pulse"
#endif
                                             );
        }
        if (fDarwin)
        {
            RTPrintf(                        "|coreaudio");
        }
        RTPrintf(                            "]\n");
        RTPrintf("                            [--audiocontroller ac97|sb16]\n"
                 "                            [--clipboard disabled|hosttoguest|guesttohost|\n"
                 "                                         bidirectional]\n");
        if (fVRDP)
        {
            RTPrintf("                            [--vrdp on|off]\n"
                     "                            [--vrdpport default|<ports>]\n"
                     "                            [--vrdpaddress <host>]\n"
                     "                            [--vrdpauthtype null|external|guest]\n"
                     "                            [--vrdpmulticon on|off]\n"
                     "                            [--vrdpreusecon on|off]\n");
        }
        RTPrintf("                            [--usb on|off]\n"
                 "                            [--usbehci on|off]\n"
                 "                            [--snapshotfolder default|<path>]\n"
                 "                            [--teleporterenabled on|off]\n"
                 "                            [--teleporterport <port>]\n"
                 "                            [--teleporteraddress <address|empty>\n"
                 "                            [--teleporterpassword <password>]\n"
                 "                            [--hardwareuuid <uuid>]\n"
                );
        RTPrintf("\n");
    }

    if (u64Cmd & USAGE_IMPORTAPPLIANCE)
    {
        RTPrintf("VBoxManage import           <ovf> [--dry-run|-n] [more options]\n"
                 "                            (run with -n to have options displayed\n"
                 "                             for a particular OVF)\n\n");
    }

    if (u64Cmd & USAGE_EXPORTAPPLIANCE)
    {
        RTPrintf("VBoxManage export           <machines> --output|-o <ovf>\n"
                 "                            [--legacy09]\n"
                 "                            [--vsys <number of virtual system>]\n"
                 "                                    [--product <product name>]\n"
                 "                                    [--producturl <product url>]\n"
                 "                                    [--vendor <vendor name>]\n"
                 "                                    [--vendorurl <vendor url>]\n"
                 "                                    [--version <version info>]\n"
                 "                                    [--eula <license text>]\n"
                 "                                    [--eulafile <filename>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_STARTVM)
    {
        RTPrintf("VBoxManage startvm          <uuid>|<name>\n");
        RTPrintf("                            [--type gui");
        if (fVBoxSDL)
            RTPrintf(                         "|sdl");
        if (fVRDP)
            RTPrintf(                         "|vrdp");
        RTPrintf(                             "|headless]\n");
        RTPrintf("\n");
    }

    if (u64Cmd & USAGE_CONTROLVM)
    {
        RTPrintf("VBoxManage controlvm        <uuid>|<name>\n"
                 "                            pause|resume|reset|poweroff|savestate|\n"
                 "                            acpipowerbutton|acpisleepbutton|\n"
                 "                            keyboardputscancode <hex> [<hex> ...]|\n"
                 "                            injectnmi|\n"
                 "                            setlinkstate<1-N> on|off |\n"
#ifdef VBOX_DYNAMIC_NET_ATTACH
#if defined(VBOX_WITH_NETFLT)
                 "                            nic<1-N> null|nat|bridged|intnet|hostonly\n"
                 "                                     [<devicename>] |\n"
#else /* !RT_OS_LINUX && !RT_OS_DARWIN */
                 "                            nic<1-N> null|nat|bridged|intnet\n"
                 "                                     [<devicename>] |\n"
#endif /* !RT_OS_LINUX && !RT_OS_DARWIN  */
                 "                            nictrace<1-N> on|off\n"
                 "                            nictracefile<1-N> <filename>\n"
#endif /* VBOX_DYNAMIC_NET_ATTACH */
                 "                            usbattach <uuid>|<address> |\n"
                 "                            usbdetach <uuid>|<address> |\n");
        if (fVRDP)
        {
            RTPrintf("                            vrdp on|off |\n");
            RTPrintf("                            vrdpport default|<ports> |\n");
        }
        RTPrintf("                            setvideomodehint <xres> <yres> <bpp> [display] |\n"
                 "                            setcredentials <username> <password> <domain>\n"
                 "                                           [--allowlocallogon <yes|no>] |\n"
                 "                            teleport --hostname <name> --port <port>\n"
                 "                                   [--password password]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_DISCARDSTATE)
    {
        RTPrintf("VBoxManage discardstate     <uuid>|<name>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_ADOPTSTATE)
    {
        RTPrintf("VBoxManage adoptstate       <uuid>|<name> <state_file>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SNAPSHOT)
    {
        RTPrintf("VBoxManage snapshot         <uuid>|<name>\n"
                 "                            take <name> [--description <desc>] [--pause] |\n"
                 "                            delete <uuid>|<name> |\n"
                 "                            restore <uuid>|<name> |\n"
                 "                            edit <uuid>|<name>|--current\n"
                 "                                 [--name <name>]\n"
                 "                                 [--description <desc>] |\n"
                 "                            showvminfo <uuid>|<name>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_OPENMEDIUM)
    {
        RTPrintf("VBoxManage openmedium       disk|dvd|floppy <filename>\n"
                 "                            [--type normal|immutable|writethrough] (disk only)\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CLOSEMEDIUM)
    {
        RTPrintf("VBoxManage closemedium      disk|dvd|floppy <uuid>|<filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_STORAGEATTACH)
    {
        RTPrintf("VBoxManage storageattach    <uuid|vmname>\n"
                 "                            --storagectl <name>\n"
                 "                            --port <number>\n"
                 "                            --device <number>\n"
                 "                            [--type <dvddrive|hdd|fdd>\n"
                 "                             --medium <none|emptydrive|uuid|filename|host:<drive>>]\n"
                 "                            [--passthrough <on|off>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_STORAGECONTROLLER)
    {
        RTPrintf("VBoxManage storagectl       <uuid|vmname>\n"
                 "                            --name <name>\n"
                 "                            [--add <ide/sata/scsi/floppy>]\n"
                 "                            [--controller <LsiLogic/BusLogic/IntelAhci/PIIX3/PIIX4/ICH6/I82078>]\n"
                 "                            [--sataideemulation<1-4> <1-30>]\n"
                 "                            [--sataportcount <1-30>]\n"
                 "                            [--remove]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHOWHDINFO)
    {
        RTPrintf("VBoxManage showhdinfo       <uuid>|<filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEHD)
    {
        RTPrintf("VBoxManage createhd         --filename <filename>\n"
                 "                            --size <megabytes>\n"
                 "                            [--format VDI|VMDK|VHD] (default: VDI)\n"
                 "                            [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "                            [--type normal|writethrough] (default: normal)\n"
                 "                            [--comment <comment>]\n"
                 "                            [--remember]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_MODIFYHD)
    {
        RTPrintf("VBoxManage modifyhd         <uuid>|<filename>\n"
                 "                            [--type normal|writethrough|immutable]\n"
                 "                            [--autoreset on|off]\n"
                 "                            [--compact]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CLONEHD)
    {
        RTPrintf("VBoxManage clonehd          <uuid>|<filename> <outputfile>\n"
                 "                            [--format VDI|VMDK|VHD|RAW|<other>]\n"
                 "                            [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "                            [--type normal|writethrough|immutable]\n"
                 "                            [--remember] [--existing]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CONVERTFROMRAW)
    {
        RTPrintf("VBoxManage convertfromraw   <filename> <outputfile>\n"
                 "                            [--format VDI|VMDK|VHD]\n"
                 "                            [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "VBoxManage convertfromraw   stdin <outputfile> <bytes>\n"
                 "                            [--format VDI|VMDK|VHD]\n"
                 "                            [--variant Standard,Fixed,Split2G,Stream,ESX]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_ADDISCSIDISK)
    {
        RTPrintf("VBoxManage addiscsidisk     --server <name>|<ip>\n"
                 "                            --target <target>\n"
                 "                            [--port <port>]\n"
                 "                            [--lun <lun>]\n"
                 "                            [--encodedlun <lun>]\n"
                 "                            [--username <username>]\n"
                 "                            [--password <password>]\n"
                 "                            [--type normal|writethrough|immutable]\n"
                 "                            [--comment <comment>]\n"
                 "                            [--intnet]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_GETEXTRADATA)
    {
        RTPrintf("VBoxManage getextradata     global|<uuid>|<name>\n"
                 "                            <key>|enumerate\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SETEXTRADATA)
    {
        RTPrintf("VBoxManage setextradata     global|<uuid>|<name>\n"
                 "                            <key>\n"
                 "                            [<value>] (no value deletes key)\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SETPROPERTY)
    {
        RTPrintf("VBoxManage setproperty      hdfolder default|<folder> |\n"
                 "                            machinefolder default|<folder> |\n"
                 "                            vrdpauthlibrary default|<library> |\n"
                 "                            websrvauthlibrary default|null|<library> |\n"
                 "                            loghistorycount <value>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_ADD)
    {
        RTPrintf("VBoxManage usbfilter        add <index,0-N>\n"
                 "                            --target <uuid>|<name>|global\n"
                 "                            --name <string>\n"
                 "                            --action ignore|hold (global filters only)\n"
                 "                            [--active yes|no] (yes)\n"
                 "                            [--vendorid <XXXX>] (null)\n"
                 "                            [--productid <XXXX>] (null)\n"
                 "                            [--revision <IIFF>] (null)\n"
                 "                            [--manufacturer <string>] (null)\n"
                 "                            [--product <string>] (null)\n"
                 "                            [--remote yes|no] (null, VM filters only)\n"
                 "                            [--serialnumber <string>] (null)\n"
                 "                            [--maskedinterfaces <XXXXXXXX>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_MODIFY)
    {
        RTPrintf("VBoxManage usbfilter        modify <index,0-N>\n"
                 "                            --target <uuid>|<name>|global\n"
                 "                            [--name <string>]\n"
                 "                            [--action ignore|hold] (global filters only)\n"
                 "                            [--active yes|no]\n"
                 "                            [--vendorid <XXXX>|\"\"]\n"
                 "                            [--productid <XXXX>|\"\"]\n"
                 "                            [--revision <IIFF>|\"\"]\n"
                 "                            [--manufacturer <string>|\"\"]\n"
                 "                            [--product <string>|\"\"]\n"
                 "                            [--remote yes|no] (null, VM filters only)\n"
                 "                            [--serialnumber <string>|\"\"]\n"
                 "                            [--maskedinterfaces <XXXXXXXX>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_REMOVE)
    {
        RTPrintf("VBoxManage usbfilter        remove <index,0-N>\n"
                 "                            --target <uuid>|<name>|global\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHAREDFOLDER_ADD)
    {
        RTPrintf("VBoxManage sharedfolder     add <vmname>|<uuid>\n"
                 "                            --name <name> --hostpath <hostpath>\n"
                 "                            [--transient] [--readonly]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHAREDFOLDER_REMOVE)
    {
        RTPrintf("VBoxManage sharedfolder     remove <vmname>|<uuid>\n"
                 "                            --name <name> [--transient]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_VM_STATISTICS)
    {
        RTPrintf("VBoxManage vmstatistics     <vmname>|<uuid> [--reset]\n"
                 "                            [--pattern <pattern>] [--descriptions]\n"
                 "\n");
    }

#ifdef VBOX_WITH_GUEST_PROPS
    if (u64Cmd & USAGE_GUESTPROPERTY)
        usageGuestProperty();
#endif /* VBOX_WITH_GUEST_PROPS defined */

    if (u64Cmd & USAGE_METRICS)
    {
        RTPrintf("VBoxManage metrics          list [*|host|<vmname> [<metric_list>]]\n"
                 "                                                 (comma-separated)\n\n"
                 "VBoxManage metrics          setup\n"
                 "                            [--period <seconds>]\n"
                 "                            [--samples <count>]\n"
                 "                            [--list]\n"
                 "                            [*|host|<vmname> [<metric_list>]]\n\n"
                 "VBoxManage metrics          query [*|host|<vmname> [<metric_list>]]\n\n"
                 "VBoxManage metrics          collect\n"
                 "                            [--period <seconds>]\n"
                 "                            [--samples <count>]\n"
                 "                            [--list]\n"
                 "                            [--detach]\n"
                 "                            [*|host|<vmname> [<metric_list>]]\n"
                 "\n");
    }
#if defined(VBOX_WITH_NETFLT)
    if (u64Cmd & USAGE_HOSTONLYIFS)
    {
        RTPrintf("VBoxManage hostonlyif       ipconfig <name>\n"
                 "                            [--dhcp |\n"
                 "                            --ip<ipv4> [--netmask<ipv4> (def: 255.255.255.0)] |\n"
                 "                            --ipv6<ipv6> [--netmasklengthv6<length> (def: 64)]]\n"
# if defined(RT_OS_WINDOWS)
                 "                            create |\n"
                 "                            remove <name>\n"
# endif
                 "\n");
    }
#endif

    if (u64Cmd & USAGE_DHCPSERVER)
    {
        RTPrintf("VBoxManage dhcpserver       add|modify --netname <network_name> |\n"
#if defined(VBOX_WITH_NETFLT)
                 "                                       --ifname <hostonly_if_name>\n"
#endif
                 "                                [--ip <ip_address>\n"
                 "                                 --netmask <network_mask>\n"
                 "                                 --lowerip <lower_ip>\n"
                 "                                 --upperip <upper_ip>]\n"
                 "                                [--enable | --disable]\n"
                 "VBoxManage dhcpserver       remove --netname <network_name> |\n"
#if defined(VBOX_WITH_NETFLT)
                 "                                   --ifname <hostonly_if_name>\n"
#endif
                 "\n");
    }
}

/**
 * Print a usage synopsis and the syntax error message.
 */
int errorSyntax(USAGECATEGORY u64Cmd, const char *pszFormat, ...)
{
    va_list args;
    showLogo(); // show logo even if suppressed
#ifndef VBOX_ONLY_DOCS
    if (g_fInternalMode)
        printUsageInternal(u64Cmd);
    else
        printUsage(u64Cmd);
#endif /* !VBOX_ONLY_DOCS */
    va_start(args, pszFormat);
    RTPrintf("\n"
             "Syntax error: %N\n", pszFormat, &args);
    va_end(args);
    return 1;
}

/**
 * errorSyntax for RTGetOpt users.
 *
 * @returns 1.
 *
 * @param   fUsageCategory  The usage category of the command.
 * @param   rc              The RTGetOpt return code.
 * @param   pValueUnion     The value union.
 */
int errorGetOpt(USAGECATEGORY fUsageCategory, int rc, union RTGETOPTUNION const *pValueUnion)
{
    showLogo(); // show logo even if suppressed
#ifndef VBOX_ONLY_DOCS
    if (g_fInternalMode)
        printUsageInternal(fUsageCategory);
    else
        printUsage(fUsageCategory);
#endif /* !VBOX_ONLY_DOCS */

    if (rc == VINF_GETOPT_NOT_OPTION)
        return RTPrintf("error: Invalid parameter '%s'\n", pValueUnion->psz);
    if (rc > 0)
    {
        if (RT_C_IS_PRINT(rc))
            return RTPrintf("error: Invalid option -%c\n", rc);
        return RTPrintf("error: Invalid option case %i\n", rc);
    }
    if (rc == VERR_GETOPT_UNKNOWN_OPTION)
        return RTPrintf("error: unknown option: %s\n", pValueUnion->psz);
    if (pValueUnion->pDef)
        return RTPrintf("error: %s: %Rrs\n", pValueUnion->pDef->pszLong, rc);
    return RTPrintf("error: %Rrs\n", rc);
}

/**
 * Print an error message without the syntax stuff.
 */
int errorArgument(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTPrintf("error: %N\n", pszFormat, &args);
    va_end(args);
    return 1;
}

