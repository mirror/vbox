/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <VBox/VBoxHDD.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;

/* missing XPCOM <-> COM wrappers */
#ifndef STDMETHOD_
# define STDMETHOD_(ret, meth) NS_IMETHOD_(ret) meth
#endif
#ifndef NS_GET_IID
# define NS_GET_IID(I) IID_##I
#endif
#ifndef RT_OS_WINDOWS
#define IUnknown nsISupports
#endif

/** command handler type */
typedef int (*PFNHANDLER)(int argc, char *argv[], ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);

#ifdef USE_XPCOM_QUEUE
/** A pointer to the event queue, set by main() before calling any handlers. */
nsCOMPtr<nsIEventQueue> g_pEventQ;
#endif

/**
 * Quick IUSBDevice implementation for detaching / attaching
 * devices to the USB Controller.
 */
class MyUSBDevice : public IUSBDevice
{
public:
    // public initializer/uninitializer for internal purposes only
    MyUSBDevice(uint16_t a_u16VendorId, uint16_t a_u16ProductId, uint16_t a_bcdRevision, uint64_t a_u64SerialHash, const char *a_pszComment)
        :  m_usVendorId(a_u16VendorId), m_usProductId(a_u16ProductId),
           m_bcdRevision(a_bcdRevision), m_u64SerialHash(a_u64SerialHash),
           m_bstrComment(a_pszComment),
           m_cRefs(0)
    {
    }

    STDMETHOD_(ULONG, AddRef)(void)
    {
        return ASMAtomicIncU32(&m_cRefs);
    }
    STDMETHOD_(ULONG, Release)(void)
    {
        ULONG cRefs = ASMAtomicDecU32(&m_cRefs);
        if (!cRefs)
            delete this;
        return cRefs;
    }
    STDMETHOD(QueryInterface)(const IID &iid, void **ppvObject)
    {
        Guid guid(iid);
        if (guid == Guid(NS_GET_IID(IUnknown)))
            *ppvObject = (IUnknown *)this;
        else if (guid == Guid(NS_GET_IID(IUSBDevice)))
            *ppvObject = (IUSBDevice *)this;
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    STDMETHOD(COMGETTER(Id))(OUT_GUID a_pId)                    { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(VendorId))(USHORT *a_pusVendorId)       { *a_pusVendorId    = m_usVendorId;     return S_OK; }
    STDMETHOD(COMGETTER(ProductId))(USHORT *a_pusProductId)     { *a_pusProductId   = m_usProductId;    return S_OK; }
    STDMETHOD(COMGETTER(Revision))(USHORT *a_pusRevision)       { *a_pusRevision    = m_bcdRevision;    return S_OK; }
    STDMETHOD(COMGETTER(SerialHash))(ULONG64 *a_pullSerialHash) { *a_pullSerialHash = m_u64SerialHash;  return S_OK; }
    STDMETHOD(COMGETTER(Manufacturer))(BSTR *a_pManufacturer)   { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(Product))(BSTR *a_pProduct)             { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(SerialNumber))(BSTR *a_pSerialNumber)   { return E_NOTIMPL; }
    STDMETHOD(COMGETTER(Address))(BSTR *a_pAddress)             { return E_NOTIMPL; }

private:
    /** The vendor id of this USB device. */
    USHORT m_usVendorId;
    /** The product id of this USB device. */
    USHORT m_usProductId;
    /** The product revision number of this USB device.
     * (high byte = integer; low byte = decimal) */
    USHORT m_bcdRevision;
    /** The USB serial hash of the device. */
    uint64_t m_u64SerialHash;
    /** The user comment string. */
    Bstr     m_bstrComment;
    /** Reference counter. */
    uint32_t volatile m_cRefs;
};


// types
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class Nullable
{
public:

    Nullable() : mIsNull (true) {}
    Nullable (const T &aValue, bool aIsNull = false)
        : mIsNull (aIsNull), mValue (aValue) {}

    bool isNull() const { return mIsNull; };
    void setNull (bool aIsNull = true) { mIsNull = aIsNull; }

    operator const T&() const { return mValue; }

    Nullable &operator= (const T &aValue)
    {
        mValue = aValue;
        mIsNull = false;
        return *this;
    }

private:

    bool mIsNull;
    T mValue;
};

/** helper structure to encapsulate USB filter manipulation commands */
struct USBFilterCmd
{
    struct USBFilter
    {
        USBFilter ()
            : mAction (USBDeviceFilterAction_Null)
            {}

        Bstr mName;
        Nullable <bool> mActive;
        Bstr mVendorId;
        Bstr mProductId;
        Bstr mRevision;
        Bstr mManufacturer;
        Bstr mProduct;
        Bstr mRemote;
        Bstr mSerialNumber;
        Nullable <ULONG> mMaskedInterfaces;
        USBDeviceFilterAction_T mAction;
    };

    enum Action { Invalid, Add, Modify, Remove };

    USBFilterCmd() : mAction (Invalid), mIndex (0), mGlobal (false) {}

    Action mAction;
    uint32_t mIndex;
    /** flag whether the command target is a global filter */
    bool mGlobal;
    /** machine this command is targeted at (null for global filters) */
    ComPtr<IMachine> mMachine;
    USBFilter mFilter;
};
#endif /* !VBOX_ONLY_DOCS */

// funcs
///////////////////////////////////////////////////////////////////////////////

static void showLogo(void)
{
    static bool fShown; /* show only once */

    if (!fShown)
    {
        RTPrintf("VirtualBox Command Line Management Interface Version "
                 VBOX_VERSION_STRING  "\n"
                 "(C) 2005-2008 Sun Microsystems, Inc.\n"
                 "All rights reserved.\n"
                 "\n");
        fShown = true;
    }
}

static void printUsage(USAGECATEGORY u64Cmd)
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

    if (u64Cmd == USAGE_DUMPOPTS)
    {
        fLinux = true;
        fWin = true;
        fSolaris = true;
        fDarwin = true;
        fVRDP = true;
        u64Cmd = USAGE_ALL;
    }

    RTPrintf("Usage:\n"
             "\n");

    if (u64Cmd == USAGE_ALL)
    {
        RTPrintf("VBoxManage [-v|-version]    print version number and exit\n"
                 "VBoxManage -nologo ...      suppress the logo\n"
                 "\n"
                 "VBoxManage -convertSettings ...        allow to auto-convert settings files\n"
                 "VBoxManage -convertSettingsBackup ...  allow to auto-convert settings files\n"
                 "                                       but create backup copies before\n"
                 "VBoxManage -convertSettingsIgnore ...  allow to auto-convert settings files\n"
                 "                                       but don't explicitly save the results\n"
                 "\n");
    }

    if (u64Cmd & USAGE_LIST)
    {
        RTPrintf("VBoxManage list             vms|runningvms|ostypes|hostdvds|hostfloppies|\n"
                 "                            hostifs|hostinfo|hddbackends|hdds|dvds|floppies|\n"
                 "                            usbhost|usbfilters|systemproperties\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHOWVMINFO)
    {
        RTPrintf("VBoxManage showvminfo       <uuid>|<name>\n"
                 "                            [-details]\n"
                 "                            [-statistics]\n"
                 "                            [-machinereadable]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_REGISTERVM)
    {
        RTPrintf("VBoxManage registervm       <filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_UNREGISTERVM)
    {
        RTPrintf("VBoxManage unregistervm     <uuid>|<name>\n"
                 "                            [-delete]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEVM)
    {
        RTPrintf("VBoxManage createvm         -name <name>\n"
                 "                            [-ostype <ostype>]\n"
                 "                            [-register]\n"
                 "                            [-basefolder <path> | -settingsfile <path>]\n"
                 "                            [-uuid <uuid>]\n"
                 "                            \n"
                 "\n");
    }

    if (u64Cmd & USAGE_MODIFYVM)
    {
        RTPrintf("VBoxManage modifyvm         <uuid|name>\n"
                 "                            [-name <name>]\n"
                 "                            [-ostype <ostype>]\n"
                 "                            [-memory <memorysize in MB>]\n"
                 "                            [-vram <vramsize in MB>]\n"
                 "                            [-acpi on|off]\n"
                 "                            [-ioapic on|off]\n"
                 "                            [-pae on|off]\n"
                 "                            [-hwvirtex on|off|default]\n"
                 "                            [-nestedpaging on|off]\n"
                 "                            [-vtxvpid on|off]\n"
                 "                            [-monitorcount <number>]\n"
                 "                            [-accelerate3d <on|off>]\n"
                 "                            [-bioslogofadein on|off]\n"
                 "                            [-bioslogofadeout on|off]\n"
                 "                            [-bioslogodisplaytime <msec>]\n"
                 "                            [-bioslogoimagepath <imagepath>]\n"
                 "                            [-biosbootmenu disabled|menuonly|messageandmenu]\n"
                 "                            [-biossystemtimeoffset <msec>]\n"
                 "                            [-biospxedebug on|off]\n"
                 "                            [-boot<1-4> none|floppy|dvd|disk|net>]\n"
                 "                            [-hd<a|b|d> none|<uuid>|<filename>]\n"
                 "                            [-idecontroller PIIX3|PIIX4]\n"
#ifdef VBOX_WITH_AHCI
                 "                            [-sata on|off]\n"
                 "                            [-sataportcount <1-30>]\n"
                 "                            [-sataport<1-30> none|<uuid>|<filename>]\n"
                 "                            [-sataideemulation<1-4> <1-30>]\n"
#endif
                 "                            [-dvd none|<uuid>|<filename>|host:<drive>]\n"
                 "                            [-dvdpassthrough on|off]\n"
                 "                            [-floppy disabled|empty|<uuid>|\n"
                 "                                     <filename>|host:<drive>]\n"
                 "                            [-nic<1-N> none|null|nat|hostif|intnet]\n"
                 "                            [-nictype<1-N> Am79C970A|Am79C973"
#ifdef VBOX_WITH_E1000
                                                                              "|82540EM|82543GC"
#endif
                 "]\n"
                 "                            [-cableconnected<1-N> on|off]\n"
                 "                            [-nictrace<1-N> on|off]\n"
                 "                            [-nictracefile<1-N> <filename>]\n"
                 "                            [-nicspeed<1-N> <kbps>]\n"
                 "                            [-hostifdev<1-N> none|<devicename>]\n"
                 "                            [-intnet<1-N> <network name>]\n"
                 "                            [-natnet<1-N> <network>|default]\n"
                 "                            [-macaddress<1-N> auto|<mac>]\n"
                 "                            [-uart<1-N> off|<I/O base> <IRQ>]\n"
                 "                            [-uartmode<1-N> disconnected|\n"
                 "                                            server <pipe>|\n"
                 "                                            client <pipe>|\n"
                 "                                            <devicename>]\n"
#ifdef VBOX_WITH_MEM_BALLOONING
                 "                            [-guestmemoryballoon <balloonsize in MB>]\n"
#endif
                 "                            [-gueststatisticsinterval <seconds>]\n"
                 );
        if (fLinux)
        {
            RTPrintf("                            [-tapsetup<1-N> none|<application>]\n"
                     "                            [-tapterminate<1-N> none|<application>]\n");
        }
        RTPrintf("                            [-audio none|null");
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
            RTPrintf(                        "|solaudio");
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
        RTPrintf("                            [-audiocontroller ac97|sb16]\n"
                 "                            [-clipboard disabled|hosttoguest|guesttohost|\n"
                 "                                        bidirectional]\n");
        if (fVRDP)
        {
            RTPrintf("                            [-vrdp on|off]\n"
                     "                            [-vrdpport default|<port>]\n"
                     "                            [-vrdpaddress <host>]\n"
                     "                            [-vrdpauthtype null|external|guest]\n"
                     "                            [-vrdpmulticon on|off]\n"
                     "                            [-vrdpreusecon on|off]\n");
        }
        RTPrintf("                            [-usb on|off]\n"
                 "                            [-usbehci on|off]\n"
                 "                            [-snapshotfolder default|<path>]\n");
        RTPrintf("\n");
    }

    if (u64Cmd & USAGE_STARTVM)
    {
        RTPrintf("VBoxManage startvm          <uuid>|<name>\n");
        if (fVRDP)
            RTPrintf("                            [-type gui|vrdp]\n");
        RTPrintf("\n");
    }

    if (u64Cmd & USAGE_CONTROLVM)
    {
        RTPrintf("VBoxManage controlvm        <uuid>|<name>\n"
                 "                            pause|resume|reset|poweroff|savestate|\n"
                 "                            acpipowerbutton|acpisleepbutton|\n"
                 "                            keyboardputscancode <hex> [<hex> ...]|\n"
                 "                            injectnmi|\n"
                 "                            setlinkstate<1-4> on|off |\n"
                 "                            usbattach <uuid>|<address> |\n"
                 "                            usbdetach <uuid>|<address> |\n"
                 "                            dvdattach none|<uuid>|<filename>|host:<drive> |\n"
                 "                            floppyattach none|<uuid>|<filename>|host:<drive> |\n"
                 "                            setvideomodehint <xres> <yres> <bpp> [display]|\n"
                 "                            setcredentials <username> <password> <domain>\n"
                 "                                           [-allowlocallogon <yes|no>]\n"
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
                 "                            take <name> [-desc <desc>] |\n"
                 "                            discard <uuid>|<name> |\n"
                 "                            discardcurrent -state|-all |\n"
                 "                            edit <uuid>|<name>|-current\n"
                 "                                 [-newname <name>]\n"
                 "                                 [-newdesc <desc>] |\n"
                 "                            showvminfo <uuid>|<name>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_REGISTERIMAGE)
    {
        RTPrintf("VBoxManage openmedium       disk|dvd|floppy <filename>\n"
                 "                            [-type normal|immutable|writethrough] (disk only)\n"
                 "\n");
    }

    if (u64Cmd & USAGE_UNREGISTERIMAGE)
    {
        RTPrintf("VBoxManage closemedium      disk|dvd|floppy <uuid>|<filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHOWHDINFO)
    {
        RTPrintf("VBoxManage showhdinfo       <uuid>|<filename>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEHD)
    {
        /// @todo NEWMEDIA add -format to specify the hard disk backend
        RTPrintf("VBoxManage createhd         -filename <filename>\n"
                 "                            -size <megabytes>\n"
                 "                            [-static]\n"
                 "                            [-comment <comment>]\n"
                 "                            [-register]\n"
                 "                            [-type normal|writethrough] (default: normal)\n"
                 "\n");
    }

    if (u64Cmd & USAGE_MODIFYHD)
    {
        RTPrintf("VBoxManage modifyhd         <uuid>|<filename>\n"
                 "                            settype normal|writethrough|immutable |\n"
                 "                            compact\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CLONEHD)
    {
        RTPrintf("VBoxManage clonehd          <uuid>|<filename> <outputfile>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CONVERTDD)
    {
        RTPrintf("VBoxManage convertdd        [-static] <filename> <outputfile>\n"
                 "VBoxManage convertdd        [-static] stdin <outputfile> <bytes>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_ADDISCSIDISK)
    {
        RTPrintf("VBoxManage addiscsidisk     -server <name>|<ip>\n"
                 "                            -target <target>\n"
                 "                            [-port <port>]\n"
                 "                            [-lun <lun>]\n"
                 "                            [-encodedlun <lun>]\n"
                 "                            [-username <username>]\n"
                 "                            [-password <password>]\n"
                 "                            [-comment <comment>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEHOSTIF && fWin)
    {
        RTPrintf("VBoxManage createhostif     <name>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_REMOVEHOSTIF && fWin)
    {
        RTPrintf("VBoxManage removehostif     <uuid>|<name>\n"
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
                 "                            hwvirtexenabled yes|no\n"
                 "                            loghistorycount <value>\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_ADD)
    {
        RTPrintf("VBoxManage usbfilter        add <index,0-N>\n"
                 "                            -target <uuid>|<name>|global\n"
                 "                            -name <string>\n"
                 "                            -action ignore|hold (global filters only)\n"
                 "                            [-active yes|no] (yes)\n"
                 "                            [-vendorid <XXXX>] (null)\n"
                 "                            [-productid <XXXX>] (null)\n"
                 "                            [-revision <IIFF>] (null)\n"
                 "                            [-manufacturer <string>] (null)\n"
                 "                            [-product <string>] (null)\n"
                 "                            [-remote yes|no] (null, VM filters only)\n"
                 "                            [-serialnumber <string>] (null)\n"
                 "                            [-maskedinterfaces <XXXXXXXX>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_MODIFY)
    {
        RTPrintf("VBoxManage usbfilter        modify <index,0-N>\n"
                 "                            -target <uuid>|<name>|global\n"
                 "                            [-name <string>]\n"
                 "                            [-action ignore|hold] (global filters only)\n"
                 "                            [-active yes|no]\n"
                 "                            [-vendorid <XXXX>|\"\"]\n"
                 "                            [-productid <XXXX>|\"\"]\n"
                 "                            [-revision <IIFF>|\"\"]\n"
                 "                            [-manufacturer <string>|\"\"]\n"
                 "                            [-product <string>|\"\"]\n"
                 "                            [-remote yes|no] (null, VM filters only)\n"
                 "                            [-serialnumber <string>|\"\"]\n"
                 "                            [-maskedinterfaces <XXXXXXXX>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_USBFILTER_REMOVE)
    {
        RTPrintf("VBoxManage usbfilter        remove <index,0-N>\n"
                 "                            -target <uuid>|<name>|global\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHAREDFOLDER_ADD)
    {
        RTPrintf("VBoxManage sharedfolder     add <vmname>|<uuid>\n"
                 "                            -name <name> -hostpath <hostpath>\n"
                 "                            [-transient] [-readonly]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_SHAREDFOLDER_REMOVE)
    {
        RTPrintf("VBoxManage sharedfolder     remove <vmname>|<uuid>\n"
                 "                            -name <name> [-transient]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_VM_STATISTICS)
    {
        RTPrintf("VBoxManage vmstatistics     <vmname>|<uuid> [-reset]\n"
                 "                            [-pattern <pattern>] [-descriptions]\n"
                 "\n");
    }

#ifdef VBOX_WITH_GUEST_PROPS
    if (u64Cmd & USAGE_GUESTPROPERTY)
        usageGuestProperty();
#endif /* VBOX_WITH_GUEST_PROPS defined */

    if (u64Cmd & USAGE_METRICS)
    {
        RTPrintf("VBoxManage metrics          list [*|host|<vmname> [<metric_list>]] (comma-separated)\n\n"
                 "VBoxManage metrics          setup\n"
                 "                            [-period <seconds>]\n"
                 "                            [-samples <count>]\n"
                 "                            [-list]\n"
                 "                            [*|host|<vmname> [<metric_list>]]\n\n"
                 "VBoxManage metrics          query [*|host|<vmname> [<metric_list>]]\n\n"
                 "VBoxManage metrics          collect\n"
                 "                            [-period <seconds>]\n"
                 "                            [-samples <count>]\n"
                 "                            [-list]\n"
                 "                            [-detach]\n"
                 "                            [*|host|<vmname> [<metric_list>]]\n"
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

#ifndef VBOX_ONLY_DOCS
/**
 * Print out progress on the console
 */
static void showProgress(ComPtr<IProgress> progress)
{
    BOOL fCompleted;
    LONG currentPercent;
    LONG lastPercent = 0;

    RTPrintf("0%%...");
    RTStrmFlush(g_pStdOut);
    while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
    {
        progress->COMGETTER(Percent(&currentPercent));

        /* did we cross a 10% mark? */
        if (((currentPercent / 10) > (lastPercent / 10)))
        {
            /* make sure to also print out missed steps */
            for (LONG curVal = (lastPercent / 10) * 10 + 10; curVal <= (currentPercent / 10) * 10; curVal += 10)
            {
                if (curVal < 100)
                {
                    RTPrintf("%ld%%...", curVal);
                    RTStrmFlush(g_pStdOut);
                }
            }
            lastPercent = (currentPercent / 10) * 10;
        }
        if (fCompleted)
            break;

        /* make sure the loop is not too tight */
        progress->WaitForCompletion(100);
    }

    /* complete the line. */
    HRESULT rc;
    if (SUCCEEDED(progress->COMGETTER(ResultCode)(&rc)))
    {
        if (SUCCEEDED(rc))
            RTPrintf("100%%\n");
        else
            RTPrintf("FAILED\n");
    }
    else
        RTPrintf("\n");
    RTStrmFlush(g_pStdOut);
}

static int handleRegisterVM(int argc, char *argv[],
                            ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc != 1)
        return errorSyntax(USAGE_REGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    CHECK_ERROR(virtualBox, OpenMachine(Bstr(argv[0]), machine.asOutParam()));
    if (SUCCEEDED(rc))
    {
        ASSERT(machine);
        CHECK_ERROR(virtualBox, RegisterMachine(machine));
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleUnregisterVM(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if ((argc != 1) && (argc != 2))
        return errorSyntax(USAGE_UNREGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());
        machine = NULL;
        CHECK_ERROR(virtualBox, UnregisterMachine(uuid, machine.asOutParam()));
        if (SUCCEEDED(rc) && machine)
        {
            /* are we supposed to delete the config file? */
            if ((argc == 2) && (strcmp(argv[1], "-delete") == 0))
            {
                CHECK_ERROR(machine, DeleteSettings());
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCreateHardDisk(int argc, char *argv[],
                                ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;
    Bstr filename;
    uint64_t sizeMB = 0;
    bool fStatic = false;
    Bstr comment;
    bool fRegister = false;
    const char *type = "normal";

    /* let's have a closer look at the arguments */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-filename") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            filename = argv[i];
        }
        else if (strcmp(argv[i], "-size") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            sizeMB = RTStrToUInt64(argv[i]);
        }
        else if (strcmp(argv[i], "-static") == 0)
        {
            fStatic = true;
        }
        else if (strcmp(argv[i], "-comment") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            comment = argv[i];
        }
        else if (strcmp(argv[i], "-register") == 0)
        {
            fRegister = true;
        }
        else if (strcmp(argv[i], "-type") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            type = argv[i];
        }
        else
            return errorSyntax(USAGE_CREATEHD, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
    }
    /* check the outcome */
    if (!filename || (sizeMB == 0))
        return errorSyntax(USAGE_CREATEHD, "Parameters -filename and -size are required");

    if (strcmp(type, "normal") && strcmp(type, "writethrough"))
        return errorArgument("Invalid hard disk type '%s' specified", Utf8Str(type).raw());

    ComPtr<IHardDisk2> hardDisk;
    CHECK_ERROR(virtualBox, CreateHardDisk2(Bstr("VDI"), filename, hardDisk.asOutParam()));
    if (SUCCEEDED(rc) && hardDisk)
    {
        /* we will close the hard disk after the storage has been successfully
         * created unless fRegister is set */
        bool doClose = false;

        CHECK_ERROR(hardDisk,COMSETTER(Description)(comment));
        ComPtr<IProgress> progress;
        if (fStatic)
        {
            CHECK_ERROR(hardDisk, CreateFixedStorage(sizeMB, progress.asOutParam()));
        }
        else
        {
            CHECK_ERROR(hardDisk, CreateDynamicStorage(sizeMB, progress.asOutParam()));
        }
        if (SUCCEEDED(rc) && progress)
        {
            if (fStatic)
                showProgress(progress);
            else
                CHECK_ERROR(progress, WaitForCompletion(-1));
            if (SUCCEEDED(rc))
            {
                progress->COMGETTER(ResultCode)(&rc);
                if (FAILED(rc))
                {
                    com::ProgressErrorInfo info(progress);
                    if (info.isBasicAvailable())
                        RTPrintf("Error: failed to create hard disk. Error message: %lS\n", info.getText().raw());
                    else
                        RTPrintf("Error: failed to create hard disk. No error message available!\n");
                }
                else
                {
                    doClose = !fRegister;

                    Guid uuid;
                    CHECK_ERROR(hardDisk, COMGETTER(Id)(uuid.asOutParam()));

                    if (strcmp(type, "normal") == 0)
                    {
                        /* nothing required, default */
                    }
                    else if (strcmp(type, "writethrough") == 0)
                    {
                        CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Writethrough));
                    }

                    RTPrintf("Disk image created. UUID: %s\n", uuid.toString().raw());
                }
            }
        }
        if (doClose)
        {
            CHECK_ERROR(hardDisk, Close());
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static DECLCALLBACK(int) hardDiskProgressCallback(PVM pVM, unsigned uPercent, void *pvUser)
{
    unsigned *pPercent = (unsigned *)pvUser;

    if (*pPercent != uPercent)
    {
        *pPercent = uPercent;
        RTPrintf(".");
        if ((uPercent % 10) == 0 && uPercent)
            RTPrintf("%d%%", uPercent);
        RTStrmFlush(g_pStdOut);
    }

    return VINF_SUCCESS;
}


static int handleModifyHardDisk(int argc, char *argv[],
                                ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    /* The uuid/filename and a command */
    if (argc < 2)
        return errorSyntax(USAGE_MODIFYHD, "Incorrect number of parameters");

    ComPtr<IHardDisk2> hardDisk;
    Bstr filepath;

    /* first guess is that it's a UUID */
    Guid uuid(argv[0]);
    rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
    /* no? then it must be a filename */
    if (!hardDisk)
    {
        filepath = argv[0];
        CHECK_ERROR(virtualBox, FindHardDisk2(filepath, hardDisk.asOutParam()));
    }

    /* let's find out which command */
    if (strcmp(argv[1], "settype") == 0)
    {
        /* hard disk must be registered */
        if (SUCCEEDED(rc) && hardDisk)
        {
            char *type = NULL;

            if (argc <= 2)
                return errorArgument("Missing argument to for settype");

            type = argv[2];

            HardDiskType_T hddType;
            CHECK_ERROR(hardDisk, COMGETTER(Type)(&hddType));

            if (strcmp(type, "normal") == 0)
            {
                if (hddType != HardDiskType_Normal)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Normal));
            }
            else if (strcmp(type, "writethrough") == 0)
            {
                if (hddType != HardDiskType_Writethrough)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Writethrough));

            }
            else if (strcmp(type, "immutable") == 0)
            {
                if (hddType != HardDiskType_Immutable)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Immutable));
            }
            else
            {
                return errorArgument("Invalid hard disk type '%s' specified", Utf8Str(type).raw());
            }
        }
        else
            return errorArgument("Hard disk image not registered");
    }
    else if (strcmp(argv[1], "compact") == 0)
    {
        /* the hard disk image might not be registered */
        if (!hardDisk)
        {
            virtualBox->OpenHardDisk2(Bstr(argv[0]), hardDisk.asOutParam());
            if (!hardDisk)
                return errorArgument("Hard disk image not found");
        }

        Bstr format;
        hardDisk->COMGETTER(Format)(format.asOutParam());
        if (format != "VDI")
            return errorArgument("Invalid hard disk type. The command only works on VDI files\n");

        Bstr fileName;
        hardDisk->COMGETTER(Location)(fileName.asOutParam());

        /* make sure the object reference is released */
        hardDisk = NULL;

        unsigned uProcent;

        RTPrintf("Shrinking '%lS': 0%%", fileName.raw());
        int vrc = VDIShrinkImage(Utf8Str(fileName).raw(), hardDiskProgressCallback, &uProcent);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while shrinking hard disk image: %Rrc\n", vrc);
            rc = E_FAIL;
        }
    }
    else
        return errorSyntax(USAGE_MODIFYHD, "Invalid parameter '%s'", Utf8Str(argv[1]).raw());

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCloneHardDisk(int argc, char *argv[],
                               ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
#if 1
    RTPrintf("Error: Clone hard disk operation is temporarily unavailable!\n");
    return 1;
#else
    /// @todo NEWMEDIA use IHardDisk2::cloneTo/flattenTo (not yet implemented)
    HRESULT rc;

    /* source hard disk and target path */
    if (argc != 2)
        return errorSyntax(USAGE_CLONEHD, "Incorrect number of parameters");

    /* first guess is that it's a UUID */
    Guid uuid(argv[0]);
    ComPtr<IHardDisk2> hardDisk;
    rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
    if (!hardDisk)
    {
        /* not successful? Then it must be a filename */
        CHECK_ERROR(virtualBox, OpenHardDisk2(Bstr(argv[0]), hardDisk.asOutParam()));
    }
    if (hardDisk)
    {
        ComPtr<IProgress> progress;
        CHECK_ERROR(hardDisk, CloneToImage(Bstr(argv[1]), hardDisk.asOutParam(), progress.asOutParam()));
        if (SUCCEEDED(rc))
        {
            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to clone disk image. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to clone disk image. No error message available!\n");
                }
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
#endif
}

static int handleConvertDDImage(int argc, char *argv[])
{
    int arg = 0;
    VDIIMAGETYPE enmImgType = VDI_IMAGE_TYPE_NORMAL;
    if (argc >= 1 && !strcmp(argv[arg], "-static"))
    {
        arg++;
        enmImgType = VDI_IMAGE_TYPE_FIXED;
    }

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
    const bool fReadFromStdIn = (argc >= arg + 1) && !strcmp(argv[arg], "stdin");
#else
    const bool fReadFromStdIn = false;
#endif

    if ((!fReadFromStdIn && argc != arg + 2) || (fReadFromStdIn && argc != arg + 3))
        return errorSyntax(USAGE_CONVERTDD, "Incorrect number of parameters");

    RTPrintf("Converting VDI: from DD image file=\"%s\" to file=\"%s\"...\n",
             argv[arg], argv[arg + 1]);

    /* open raw image file. */
    RTFILE File;
    int rc = VINF_SUCCESS;
    if (fReadFromStdIn)
        File = 0;
    else
        rc = RTFileOpen(&File, argv[arg], RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
    {
        RTPrintf("File=\"%s\" open error: %Rrf\n", argv[arg], rc);
        return rc;
    }

    uint64_t cbFile;
    /* get image size. */
    if (fReadFromStdIn)
        cbFile = RTStrToUInt64(argv[arg + 2]);
    else
        rc = RTFileGetSize(File, &cbFile);
    if (RT_SUCCESS(rc))
    {
        RTPrintf("Creating %s image with size %RU64 bytes (%RU64MB)...\n", (enmImgType == VDI_IMAGE_TYPE_FIXED) ? "fixed" : "dynamic", cbFile, (cbFile + _1M - 1) / _1M);
        char pszComment[256];
        RTStrPrintf(pszComment, sizeof(pszComment), "Converted image from %s", argv[arg]);
        rc = VDICreateBaseImage(argv[arg + 1],
                                enmImgType,
                                cbFile,
                                pszComment, NULL, NULL);
        if (RT_SUCCESS(rc))
        {
            PVDIDISK pVdi = VDIDiskCreate();
            rc = VDIDiskOpenImage(pVdi, argv[arg + 1], VDI_OPEN_FLAGS_NORMAL);
            if (RT_SUCCESS(rc))
            {
                /* alloc work buffer. */
                size_t cbBuffer = VDIDiskGetBufferSize(pVdi);
                void   *pvBuf = RTMemAlloc(cbBuffer);
                if (pvBuf)
                {
                    uint64_t offFile = 0;
                    while (offFile < cbFile)
                    {
                        size_t cbRead = 0;
                        size_t cbToRead = cbFile - offFile >= (uint64_t) cbBuffer ?
                            cbBuffer : (size_t) (cbFile - offFile);
                        rc = RTFileRead(File, pvBuf, cbToRead, &cbRead);
                        if (RT_FAILURE(rc) || !cbRead)
                            break;
                        rc = VDIDiskWrite(pVdi, offFile, pvBuf, cbRead);
                        if (RT_FAILURE(rc))
                            break;
                        offFile += cbRead;
                    }

                    RTMemFree(pvBuf);
                }
                else
                    rc = VERR_NO_MEMORY;

                VDIDiskCloseImage(pVdi);
            }

            if (RT_FAILURE(rc))
            {
                /* delete image on error */
                RTPrintf("Failed (%Rrc)!\n", rc);
                VDIDeleteImage(argv[arg + 1]);
            }
        }
        else
            RTPrintf("Failed to create output file (%Rrc)!\n", rc);
    }
    RTFileClose(File);

    return rc;
}

static int handleAddiSCSIDisk(int argc, char *argv[],
                              ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc;
    Bstr server;
    Bstr target;
    Bstr port;
    Bstr lun;
    Bstr username;
    Bstr password;
    Bstr comment;

    /* at least server and target */
    if (argc < 4)
        return errorSyntax(USAGE_ADDISCSIDISK, "Not enough parameters");

    /* let's have a closer look at the arguments */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-server") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            server = argv[i];
        }
        else if (strcmp(argv[i], "-target") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            target = argv[i];
        }
        else if (strcmp(argv[i], "-port") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            port = argv[i];
        }
        else if (strcmp(argv[i], "-lun") == 0)
        {
            /// @todo is the below todo still relevant? Note that we do a
            /// strange string->int->string conversion here.

            /** @todo move the LUN encoding algorithm into IISCSIHardDisk, add decoding */

            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            char *pszNext;
            uint64_t lunNum;
            int rc = RTStrToUInt64Ex(argv[i], &pszNext, 0, &lunNum);
            if (RT_FAILURE(rc) || *pszNext != '\0' || lunNum >= 16384)
                return errorArgument("Invalid LUN number '%s'", argv[i]);
            if (lunNum <= 255)
            {
                /* Assume bus identifier = 0. */
                lunNum = (lunNum << 48); /* uses peripheral device addressing method */
            }
            else
            {
                /* Check above already limited the LUN to 14 bits. */
                lunNum = (lunNum << 48) | RT_BIT_64(62); /* uses flat space addressing method */
            }

            lun = BstrFmt ("%llu", lunNum);
        }
        else if (strcmp(argv[i], "-encodedlun") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            lun = argv[i];
        }
        else if (strcmp(argv[i], "-username") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            username = argv[i];
        }
        else if (strcmp(argv[i], "-password") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            password = argv[i];
        }
        else if (strcmp(argv[i], "-comment") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            comment = argv[i];
        }
        else
            return errorSyntax(USAGE_ADDISCSIDISK, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
    }

    /* check for required options */
    if (!server || !target)
        return errorSyntax(USAGE_ADDISCSIDISK, "Parameters -server and -target are required");

    do
    {
        ComPtr<IHardDisk2> hardDisk;
        CHECK_ERROR_BREAK (aVirtualBox,
            CreateHardDisk2(Bstr ("iSCSI"),
                            BstrFmt ("%ls/%ls", server.raw(), target.raw()),
                            hardDisk.asOutParam()));
        CheckComRCBreakRC (rc);

        if (!comment.isNull())
            CHECK_ERROR_BREAK(hardDisk, COMSETTER(Description)(comment));

        if (!port.isNull())
            server = BstrFmt ("%ls:%ls", server.raw(), port.raw());

        com::SafeArray <BSTR> names;
        com::SafeArray <BSTR> values;

        Bstr ("TargetAddress").detachTo (names.appendedRaw());
        server.detachTo (values.appendedRaw());
        Bstr ("TargetName").detachTo (names.appendedRaw());
        target.detachTo (values.appendedRaw());

        if (!lun.isNull())
        {
            Bstr ("LUN").detachTo (names.appendedRaw());
            lun.detachTo (values.appendedRaw());
        }
        if (!username.isNull())
        {
            Bstr ("InitiatorUsername").detachTo (names.appendedRaw());
            username.detachTo (values.appendedRaw());
        }
        if (!password.isNull())
        {
            Bstr ("InitiatorSecret").detachTo (names.appendedRaw());
            password.detachTo (values.appendedRaw());
        }

        /// @todo add -initiator option
        Bstr ("InitiatorName").detachTo (names.appendedRaw());
        Bstr ("iqn.2008-04.com.sun.virtualbox.initiator").detachTo (values.appendedRaw());

        /// @todo add -targetName and -targetPassword options

        CHECK_ERROR_BREAK (hardDisk,
            SetProperties (ComSafeArrayAsInParam (names),
                           ComSafeArrayAsInParam (values)));

        Guid guid;
        CHECK_ERROR(hardDisk, COMGETTER(Id)(guid.asOutParam()));
        RTPrintf("iSCSI disk created. UUID: %s\n", guid.toString().raw());
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCreateVM(int argc, char *argv[],
                          ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;
    Bstr baseFolder;
    Bstr settingsFile;
    Bstr name;
    Bstr osTypeId;
    RTUUID id;
    bool fRegister = false;

    RTUuidClear(&id);
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-basefolder") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            baseFolder = argv[i];
        }
        else if (strcmp(argv[i], "-settingsfile") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            settingsFile = argv[i];
        }
        else if (strcmp(argv[i], "-name") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            name = argv[i];
        }
        else if (strcmp(argv[i], "-ostype") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            osTypeId = argv[i];
        }
        else if (strcmp(argv[i], "-uuid") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (RT_FAILURE(RTUuidFromStr(&id, argv[i])))
                return errorArgument("Invalid UUID format %s\n", argv[i]);
        }
        else if (strcmp(argv[i], "-register") == 0)
        {
            fRegister = true;
        }
        else
            return errorSyntax(USAGE_CREATEVM, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
    }
    if (!name)
        return errorSyntax(USAGE_CREATEVM, "Parameter -name is required");

    if (!!baseFolder && !!settingsFile)
        return errorSyntax(USAGE_CREATEVM, "Either -basefolder or -settingsfile must be specified");

    do
    {
        ComPtr<IMachine> machine;

        if (!settingsFile)
            CHECK_ERROR_BREAK(virtualBox,
                CreateMachine(name, osTypeId, baseFolder, Guid(id), machine.asOutParam()));
        else
            CHECK_ERROR_BREAK(virtualBox,
                CreateLegacyMachine(name, osTypeId, settingsFile, Guid(id), machine.asOutParam()));

        CHECK_ERROR_BREAK(machine, SaveSettings());
        if (fRegister)
        {
            CHECK_ERROR_BREAK(virtualBox, RegisterMachine(machine));
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

/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

static int handleModifyVM(int argc, char *argv[],
                          ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
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
    char *hdds[34] = {0};
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
    int   sataPortCount = -1;
    int   sataBootDevices[4] = {-1,-1,-1,-1};

    /* VM ID + at least one parameter. Parameter arguments are checked
     * individually. */
    if (argc < 2)
        return errorSyntax(USAGE_MODIFYVM, "Not enough parameters");

    /* Get the number of network adapters */
    ULONG NetworkAdapterCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET (virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(NetworkAdapterCount) (&NetworkAdapterCount), 1);
    }
    ULONG SerialPortCount = 0;
    {
        ComPtr <ISystemProperties> info;
        CHECK_ERROR_RET (virtualBox, COMGETTER(SystemProperties) (info.asOutParam()), 1);
        CHECK_ERROR_RET (info, COMGETTER(SerialPortCount) (&SerialPortCount), 1);
    }

    std::vector <char *> nics (NetworkAdapterCount, 0);
    std::vector <char *> nictype (NetworkAdapterCount, 0);
    std::vector <char *> cableconnected (NetworkAdapterCount, 0);
    std::vector <char *> nictrace (NetworkAdapterCount, 0);
    std::vector <char *> nictracefile (NetworkAdapterCount, 0);
    std::vector <char *> nicspeed (NetworkAdapterCount, 0);
    std::vector <char *> hostifdev (NetworkAdapterCount, 0);
    std::vector <const char *> intnet (NetworkAdapterCount, 0);
    std::vector <const char *> natnet (NetworkAdapterCount, 0);
#ifdef RT_OS_LINUX
    std::vector <char *> tapsetup (NetworkAdapterCount, 0);
    std::vector <char *> tapterm (NetworkAdapterCount, 0);
#endif
    std::vector <char *> macs (NetworkAdapterCount, 0);
    std::vector <char *> uarts_mode (SerialPortCount, 0);
    std::vector <ULONG>  uarts_base (SerialPortCount, 0);
    std::vector <ULONG>  uarts_irq (SerialPortCount, 0);
    std::vector <char *> uarts_path (SerialPortCount, 0);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-name") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            name = argv[i];
        }
        else if (strcmp(argv[i], "-ostype") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            ostype = argv[i];
        }
        else if (strcmp(argv[i], "-memory") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            memorySize = RTStrToUInt32(argv[i]);
        }
        else if (strcmp(argv[i], "-vram") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vramSize = RTStrToUInt32(argv[i]);
        }
        else if (strcmp(argv[i], "-acpi") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            acpi = argv[i];
        }
        else if (strcmp(argv[i], "-ioapic") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            ioapic = argv[i];
        }
        else if (strcmp(argv[i], "-hwvirtex") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            hwvirtex = argv[i];
        }
        else if (strcmp(argv[i], "-nestedpaging") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            nestedpaging = argv[i];
        }
        else if (strcmp(argv[i], "-vtxvpid") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vtxvpid = argv[i];
        }
        else if (strcmp(argv[i], "-pae") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            pae = argv[i];
        }
        else if (strcmp(argv[i], "-monitorcount") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            monitorcount = RTStrToUInt32(argv[i]);
        }
        else if (strcmp(argv[i], "-accelerate3d") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            accelerate3d = argv[i];
        }
        else if (strcmp(argv[i], "-bioslogofadein") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            bioslogofadein = argv[i];
        }
        else if (strcmp(argv[i], "-bioslogofadeout") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            bioslogofadeout = argv[i];
        }
        else if (strcmp(argv[i], "-bioslogodisplaytime") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            bioslogodisplaytime = RTStrToUInt32(argv[i]);
        }
        else if (strcmp(argv[i], "-bioslogoimagepath") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            bioslogoimagepath = argv[i];
        }
        else if (strcmp(argv[i], "-biosbootmenu") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            biosbootmenumode = argv[i];
        }
        else if (strcmp(argv[i], "-biossystemtimeoffset") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            biossystemtimeoffset = argv[i];
        }
        else if (strcmp(argv[i], "-biospxedebug") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            biospxedebug = argv[i];
        }
        else if (strncmp(argv[i], "-boot", 5) == 0)
        {
            uint32_t n = 0;
            if (!argv[i][5])
                return errorSyntax(USAGE_MODIFYVM, "Missing boot slot number in '%s'", argv[i]);
            if (VINF_SUCCESS != RTStrToUInt32Full(&argv[i][5], 10, &n))
                return errorSyntax(USAGE_MODIFYVM, "Invalid boot slot number in '%s'", argv[i]);
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "none") == 0)
            {
                bootDevice[n - 1] = DeviceType_Null;
            }
            else if (strcmp(argv[i], "floppy") == 0)
            {
                bootDevice[n - 1] = DeviceType_Floppy;
            }
            else if (strcmp(argv[i], "dvd") == 0)
            {
                bootDevice[n - 1] = DeviceType_DVD;
            }
            else if (strcmp(argv[i], "disk") == 0)
            {
                bootDevice[n - 1] = DeviceType_HardDisk;
            }
            else if (strcmp(argv[i], "net") == 0)
            {
                bootDevice[n - 1] = DeviceType_Network;
            }
            else
                return errorArgument("Invalid boot device '%s'", argv[i]);

            bootDeviceChanged[n - 1] = true;
        }
        else if (strcmp(argv[i], "-hda") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            hdds[0] = argv[i];
        }
        else if (strcmp(argv[i], "-hdb") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            hdds[1] = argv[i];
        }
        else if (strcmp(argv[i], "-hdd") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            hdds[2] = argv[i];
        }
        else if (strcmp(argv[i], "-dvd") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            dvd = argv[i];
        }
        else if (strcmp(argv[i], "-dvdpassthrough") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            dvdpassthrough = argv[i];
        }
        else if (strcmp(argv[i], "-idecontroller") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            idecontroller = argv[i];
        }
        else if (strcmp(argv[i], "-floppy") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            floppy = argv[i];
        }
        else if (strcmp(argv[i], "-audio") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            audio = argv[i];
        }
        else if (strcmp(argv[i], "-audiocontroller") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            audiocontroller = argv[i];
        }
        else if (strcmp(argv[i], "-clipboard") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            clipboard = argv[i];
        }
        else if (strncmp(argv[i], "-cableconnected", 15) == 0)
        {
            unsigned n = parseNum(&argv[i][15], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;

            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);

            cableconnected[n - 1] = argv[i + 1];
            i++;
        }
        /* watch for the right order of these -nic* comparisons! */
        else if (strncmp(argv[i], "-nictracefile", 13) == 0)
        {
            unsigned n = parseNum(&argv[i][13], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            nictracefile[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-nictrace", 9) == 0)
        {
            unsigned n = parseNum(&argv[i][9], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            nictrace[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-nictype", 8) == 0)
        {
            unsigned n = parseNum(&argv[i][8], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            nictype[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-nicspeed", 9) == 0)
        {
            unsigned n = parseNum(&argv[i][9], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            nicspeed[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-nic", 4) == 0)
        {
            unsigned n = parseNum(&argv[i][4], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            nics[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-hostifdev", 10) == 0)
        {
            unsigned n = parseNum(&argv[i][10], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            hostifdev[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-intnet", 7) == 0)
        {
            unsigned n = parseNum(&argv[i][7], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            intnet[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-natnet", 7) == 0)
        {
            unsigned n = parseNum(&argv[i][7], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);

            if (!strcmp(argv[i + 1], "default"))
                natnet[n - 1] = "";
            else
            {
                RTIPV4ADDR Network;
                RTIPV4ADDR Netmask;
                int rc = RTCidrStrToIPv4(argv[i + 1], &Network, &Netmask);
                if (RT_FAILURE(rc))
                    return errorArgument("Invalid IPv4 network '%s' specified -- CIDR notation expected.\n", argv[i + 1]);
                if (Netmask & 0x1f)
                    return errorArgument("Prefix length of the NAT network must be less than 28.\n");
                natnet[n - 1] = argv[i + 1];
            }
            i++;
        }
#ifdef RT_OS_LINUX
        else if (strncmp(argv[i], "-tapsetup", 9) == 0)
        {
            unsigned n = parseNum(&argv[i][9], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            tapsetup[n - 1] = argv[i + 1];
            i++;
        }
        else if (strncmp(argv[i], "-tapterminate", 13) == 0)
        {
            unsigned n = parseNum(&argv[i][13], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            tapterm[n - 1] = argv[i + 1];
            i++;
        }
#endif /* RT_OS_LINUX */
        else if (strncmp(argv[i], "-macaddress", 11) == 0)
        {
            unsigned n = parseNum(&argv[i][11], NetworkAdapterCount, "NIC");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            macs[n - 1] = argv[i + 1];
            i++;
        }
#ifdef VBOX_WITH_VRDP
        else if (strcmp(argv[i], "-vrdp") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vrdp = argv[i];
        }
        else if (strcmp(argv[i], "-vrdpport") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "default") == 0)
                vrdpport = 0;
            else
                vrdpport = RTStrToUInt16(argv[i]);
        }
        else if (strcmp(argv[i], "-vrdpaddress") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vrdpaddress = argv[i];
        }
        else if (strcmp(argv[i], "-vrdpauthtype") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vrdpauthtype = argv[i];
        }
        else if (strcmp(argv[i], "-vrdpmulticon") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vrdpmulticon = argv[i];
        }
        else if (strcmp(argv[i], "-vrdpreusecon") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            vrdpreusecon = argv[i];
        }
#endif /* VBOX_WITH_VRDP */
        else if (strcmp(argv[i], "-usb") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "on") == 0 || strcmp(argv[i], "enable") == 0)
                fUsbEnabled = 1;
            else if (strcmp(argv[i], "off") == 0 || strcmp(argv[i], "disable") == 0)
                fUsbEnabled = 0;
            else
                return errorArgument("Invalid -usb argument '%s'", argv[i]);
        }
        else if (strcmp(argv[i], "-usbehci") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "on") == 0 || strcmp(argv[i], "enable") == 0)
                fUsbEhciEnabled = 1;
            else if (strcmp(argv[i], "off") == 0 || strcmp(argv[i], "disable") == 0)
                fUsbEhciEnabled = 0;
            else
                return errorArgument("Invalid -usbehci argument '%s'", argv[i]);
        }
        else if (strcmp(argv[i], "-snapshotfolder") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            snapshotFolder = argv[i];
        }
        else if (strncmp(argv[i], "-uartmode", 9) == 0)
        {
            unsigned n = parseNum(&argv[i][9], SerialPortCount, "UART");
            if (!n)
                return 1;
            i++;
            if (strcmp(argv[i], "disconnected") == 0)
            {
                uarts_mode[n - 1] = argv[i];
            }
            else
            {
                if (strcmp(argv[i], "server") == 0 || strcmp(argv[i], "client") == 0)
                {
                    uarts_mode[n - 1] = argv[i];
                    i++;
#ifdef RT_OS_WINDOWS
                    if (strncmp(argv[i], "\\\\.\\pipe\\", 9))
                        return errorArgument("Uart pipe must start with \\\\.\\pipe\\");
#endif
                }
                else
                {
                    uarts_mode[n - 1] = (char*)"device";
                }
                if (argc <= i)
                    return errorArgument("Missing argument to -uartmode");
                uarts_path[n - 1] = argv[i];
            }
        }
        else if (strncmp(argv[i], "-uart", 5) == 0)
        {
            unsigned n = parseNum(&argv[i][5], SerialPortCount, "UART");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "off") == 0 || strcmp(argv[i], "disable") == 0)
            {
                uarts_base[n - 1] = (ULONG)-1;
            }
            else
            {
                if (argc <= i + 1)
                    return errorArgument("Missing argument to '%s'", argv[i-1]);
                uint32_t uVal;
                int vrc;
                vrc = RTStrToUInt32Ex(argv[i], NULL, 0, &uVal);
                if (vrc != VINF_SUCCESS || uVal == 0)
                    return errorArgument("Error parsing UART I/O base '%s'", argv[i]);
                uarts_base[n - 1] = uVal;
                i++;
                vrc = RTStrToUInt32Ex(argv[i], NULL, 0, &uVal);
                if (vrc != VINF_SUCCESS)
                    return errorArgument("Error parsing UART IRQ '%s'", argv[i]);
                uarts_irq[n - 1]  = uVal;
            }
        }
#ifdef VBOX_WITH_MEM_BALLOONING
        else if (strncmp(argv[i], "-guestmemoryballoon", 19) == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(argv[i], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
                return errorArgument("Error parsing guest memory balloon size '%s'", argv[i]);
            guestMemBalloonSize = uVal;
        }
#endif
        else if (strncmp(argv[i], "-gueststatisticsinterval", 24) == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(argv[i], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
                return errorArgument("Error parsing guest statistics interval '%s'", argv[i]);
            guestStatInterval = uVal;
        }
        else if (strcmp(argv[i], "-sata") == 0)
        {
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            if (strcmp(argv[i], "on") == 0 || strcmp(argv[i], "enable") == 0)
                fSataEnabled = 1;
            else if (strcmp(argv[i], "off") == 0 || strcmp(argv[i], "disable") == 0)
                fSataEnabled = 0;
            else
                return errorArgument("Invalid -usb argument '%s'", argv[i]);
        }
        else if (strcmp(argv[i], "-sataportcount") == 0)
        {
            unsigned n;

            if (argc <= i + 1)
                return errorArgument("Missing arguments to '%s'", argv[i]);
            i++;

            n = parseNum(argv[i], 30, "SATA");
            if (!n)
                return 1;
            sataPortCount = n;
        }
        else if (strncmp(argv[i], "-sataport", 9) == 0)
        {
            unsigned n = parseNum(&argv[i][9], 30, "SATA");
            if (!n)
                return 1;
            if (argc <= i + 1)
                return errorArgument("Missing argument to '%s'", argv[i]);
            i++;
            hdds[n-1+4] = argv[i];
        }
        else if (strncmp(argv[i], "-sataideemulation", 17) == 0)
        {
            unsigned bootDevicePos = 0;
            unsigned n;

            bootDevicePos = parseNum(&argv[i][17], 4, "SATA");
            if (!bootDevicePos)
                return 1;
            bootDevicePos--;

            if (argc <= i + 1)
                return errorArgument("Missing arguments to '%s'", argv[i]);
            i++;

            n = parseNum(argv[i], 30, "SATA");
            if (!n)
                return 1;

            sataBootDevices[bootDevicePos] = n-1;
        }
        else
            return errorSyntax(USAGE_MODIFYVM, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
    }

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (argv[0]);
    if (!uuid.isEmpty())
    {
        CHECK_ERROR (virtualBox, GetMachine (uuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR (virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(uuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET (virtualBox, OpenSession(session, uuid), 1);

    do
    {
        /* get the mutable session machine */
        session->COMGETTER(Machine)(machine.asOutParam());

        ComPtr <IBIOSSettings> biosSettings;
        machine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());

        if (name)
            CHECK_ERROR(machine, COMSETTER(Name)(name));
        if (ostype)
        {
            ComPtr<IGuestOSType> guestOSType;
            CHECK_ERROR(virtualBox, GetGuestOSType(ostype, guestOSType.asOutParam()));
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
            if (strcmp(acpi, "on") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(ACPIEnabled)(true));
            }
            else if (strcmp(acpi, "off") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(ACPIEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -acpi argument '%s'", acpi);
                rc = E_FAIL;
                break;
            }
        }
        if (ioapic)
        {
            if (strcmp(ioapic, "on") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(IOAPICEnabled)(true));
            }
            else if (strcmp(ioapic, "off") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(IOAPICEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -ioapic argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (hwvirtex)
        {
            if (strcmp(hwvirtex, "on") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_True));
            }
            else if (strcmp(hwvirtex, "off") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_False));
            }
            else if (strcmp(hwvirtex, "default") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExEnabled)(TSBool_Default));
            }
            else
            {
                errorArgument("Invalid -hwvirtex argument '%s'", hwvirtex);
                rc = E_FAIL;
                break;
            }
        }
        if (nestedpaging)
        {
            if (strcmp(nestedpaging, "on") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExNestedPagingEnabled)(true));
            }
            else if (strcmp(nestedpaging, "off") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExNestedPagingEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -nestedpaging argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (vtxvpid)
        {
            if (strcmp(vtxvpid, "on") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExVPIDEnabled)(true));
            }
            else if (strcmp(vtxvpid, "off") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(HWVirtExVPIDEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -vtxvpid argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (pae)
        {
            if (strcmp(pae, "on") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(PAEEnabled)(true));
            }
            else if (strcmp(pae, "off") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(PAEEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -pae argument '%s'", ioapic);
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
            if (strcmp(accelerate3d, "on") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate3DEnabled)(true));
            }
            else if (strcmp(accelerate3d, "off") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(Accelerate3DEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -accelerate3d argument '%s'", ioapic);
                rc = E_FAIL;
                break;
            }
        }
        if (bioslogofadein)
        {
            if (strcmp(bioslogofadein, "on") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeIn)(true));
            }
            else if (strcmp(bioslogofadein, "off") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeIn)(false));
            }
            else
            {
                errorArgument("Invalid -bioslogofadein argument '%s'", bioslogofadein);
                rc = E_FAIL;
                break;
            }
        }
        if (bioslogofadeout)
        {
            if (strcmp(bioslogofadeout, "on") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeOut)(true));
            }
            else if (strcmp(bioslogofadeout, "off") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(LogoFadeOut)(false));
            }
            else
            {
                errorArgument("Invalid -bioslogofadeout argument '%s'", bioslogofadeout);
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
            if (strcmp(biosbootmenumode, "disabled") == 0)
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_Disabled));
            else if (strcmp(biosbootmenumode, "menuonly") == 0)
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MenuOnly));
            else if (strcmp(biosbootmenumode, "messageandmenu") == 0)
                CHECK_ERROR(biosSettings, COMSETTER(BootMenuMode)(BIOSBootMenuMode_MessageAndMenu));
            else
            {
                errorArgument("Invalid -biosbootmenu argument '%s'", biosbootmenumode);
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
            if (strcmp(biospxedebug, "on") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(PXEDebugEnabled)(true));
            }
            else if (strcmp(biospxedebug, "off") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(PXEDebugEnabled)(false));
            }
            else
            {
                errorArgument("Invalid -biospxedebug argument '%s'", biospxedebug);
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
            if (strcmp(hdds[0], "none") == 0)
            {
                machine->DetachHardDisk2(StorageBus_IDE, 0, 0);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[0]);
                ComPtr<IHardDisk2> hardDisk;
                rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(virtualBox, FindHardDisk2(Bstr(hdds[0]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(virtualBox, OpenHardDisk2(Bstr(hdds[0]), hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk2(uuid, StorageBus_IDE, 0, 0));
                }
                else
                    rc = E_FAIL;
                if (FAILED(rc))
                    break;
            }
        }
        if (hdds[1])
        {
            if (strcmp(hdds[1], "none") == 0)
            {
                machine->DetachHardDisk2(StorageBus_IDE, 0, 1);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[1]);
                ComPtr<IHardDisk2> hardDisk;
                rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(virtualBox, FindHardDisk2(Bstr(hdds[1]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(virtualBox, OpenHardDisk2(Bstr(hdds[1]), hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk2(uuid, StorageBus_IDE, 0, 1));
                }
                else
                    rc = E_FAIL;
                if (FAILED(rc))
                    break;
            }
        }
        if (hdds[2])
        {
            if (strcmp(hdds[2], "none") == 0)
            {
                machine->DetachHardDisk2(StorageBus_IDE, 1, 1);
            }
            else
            {
                /* first guess is that it's a UUID */
                Guid uuid(hdds[2]);
                ComPtr<IHardDisk2> hardDisk;
                rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
                /* not successful? Then it must be a filename */
                if (!hardDisk)
                {
                    CHECK_ERROR(virtualBox, FindHardDisk2(Bstr(hdds[2]), hardDisk.asOutParam()));
                    if (FAILED(rc))
                    {
                        /* open the new hard disk object */
                        CHECK_ERROR(virtualBox, OpenHardDisk2(Bstr(hdds[2]), hardDisk.asOutParam()));
                    }
                }
                if (hardDisk)
                {
                    hardDisk->COMGETTER(Id)(uuid.asOutParam());
                    CHECK_ERROR(machine, AttachHardDisk2(uuid, StorageBus_IDE, 1, 1));
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
            if (strcmp(dvd, "none") == 0)
            {
                CHECK_ERROR(dvdDrive, Unmount());
            }
            /* host drive? */
            else if (strncmp(dvd, "host:", 5) == 0)
            {
                ComPtr<IHost> host;
                CHECK_ERROR(virtualBox, COMGETTER(Host)(host.asOutParam()));
                ComPtr<IHostDVDDriveCollection> hostDVDs;
                CHECK_ERROR(host, COMGETTER(DVDDrives)(hostDVDs.asOutParam()));
                ComPtr<IHostDVDDrive> hostDVDDrive;
                rc = hostDVDs->FindByName(Bstr(dvd + 5), hostDVDDrive.asOutParam());
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
                    rc = hostDVDs->FindByName(Bstr(szPathReal), hostDVDDrive.asOutParam());
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
                ComPtr<IDVDImage2> dvdImage;
                rc = virtualBox->GetDVDImage(uuid, dvdImage.asOutParam());
                if (FAILED(rc) || !dvdImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = virtualBox->FindDVDImage(Bstr(dvd), dvdImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(virtualBox, OpenDVDImage(Bstr(dvd), emptyUUID, dvdImage.asOutParam()));
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

            CHECK_ERROR(dvdDrive, COMSETTER(Passthrough)(strcmp(dvdpassthrough, "on") == 0));
        }
        if (idecontroller)
        {
            if (RTStrICmp(idecontroller, "PIIX3") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(IDEControllerType)(IDEControllerType_PIIX3));
            }
            else if (RTStrICmp(idecontroller, "PIIX4") == 0)
            {
                CHECK_ERROR(biosSettings, COMSETTER(IDEControllerType)(IDEControllerType_PIIX4));
            }
            else
            {
                errorArgument("Invalid -idecontroller argument '%s'", idecontroller);
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
            if (strcmp(floppy, "disabled") == 0)
            {
                /* disable the controller */
                CHECK_ERROR(floppyDrive, COMSETTER(Enabled)(false));
            }
            else
            {
                /* enable the controller */
                CHECK_ERROR(floppyDrive, COMSETTER(Enabled)(true));

                /* unmount? */
                if (strcmp(floppy, "empty") == 0)
                {
                    CHECK_ERROR(floppyDrive, Unmount());
                }
                /* host drive? */
                else if (strncmp(floppy, "host:", 5) == 0)
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR(virtualBox, COMGETTER(Host)(host.asOutParam()));
                    ComPtr<IHostFloppyDriveCollection> hostFloppies;
                    CHECK_ERROR(host, COMGETTER(FloppyDrives)(hostFloppies.asOutParam()));
                    ComPtr<IHostFloppyDrive> hostFloppyDrive;
                    rc = hostFloppies->FindByName(Bstr(floppy + 5), hostFloppyDrive.asOutParam());
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
                    ComPtr<IFloppyImage2> floppyImage;
                    rc = virtualBox->GetFloppyImage(uuid, floppyImage.asOutParam());
                    if (FAILED(rc) || !floppyImage)
                    {
                        /* must be a filename, check if it's in the collection */
                        rc = virtualBox->FindFloppyImage(Bstr(floppy), floppyImage.asOutParam());
                        /* not registered, do that on the fly */
                        if (!floppyImage)
                        {
                            Guid emptyUUID;
                            CHECK_ERROR(virtualBox, OpenFloppyImage(Bstr(floppy), emptyUUID, floppyImage.asOutParam()));
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
                if (strcmp(audio, "none") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(false));
                }
                else if (strcmp(audio, "null") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Null));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
                else if (strcmp(audio, "winmm") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_WinMM));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif
                else if (strcmp(audio, "dsound") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_DirectSound));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_LINUX
                else if (strcmp(audio, "oss") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_OSS));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# ifdef VBOX_WITH_ALSA
                else if (strcmp(audio, "alsa") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_ALSA));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
# ifdef VBOX_WITH_PULSE
                else if (strcmp(audio, "pulse") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_Pulse));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }
# endif
#endif /* !RT_OS_LINUX */
#ifdef RT_OS_SOLARIS
                else if (strcmp(audio, "solaudio") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_SolAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

#endif /* !RT_OS_SOLARIS */
#ifdef RT_OS_DARWIN
                else if (strcmp(audio, "coreaudio") == 0)
                {
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioDriver)(AudioDriverType_CoreAudio));
                    CHECK_ERROR(audioAdapter, COMSETTER(Enabled)(true));
                }

#endif /* !RT_OS_DARWIN */
                else
                {
                    errorArgument("Invalid -audio argument '%s'", audio);
                    rc = E_FAIL;
                    break;
                }
            }
            if (audiocontroller)
            {
                if (strcmp(audiocontroller, "sb16") == 0)
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_SB16));
                else if (strcmp(audiocontroller, "ac97") == 0)
                    CHECK_ERROR(audioAdapter, COMSETTER(AudioController)(AudioControllerType_AC97));
                else
                {
                    errorArgument("Invalid -audiocontroller argument '%s'", audiocontroller);
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
            if (strcmp(clipboard, "disabled") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Disabled));
            }
            else if (strcmp(clipboard, "hosttoguest") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_HostToGuest));
            }
            else if (strcmp(clipboard, "guesttohost") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_GuestToHost));
            }
            else if (strcmp(clipboard, "bidirectional") == 0)
            {
                CHECK_ERROR(machine, COMSETTER(ClipboardMode)(ClipboardMode_Bidirectional));
            }
            else
            {
                errorArgument("Invalid -clipboard argument '%s'", clipboard);
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
                if (strcmp(nics[n], "none") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (FALSE), 1);
                }
                else if (strcmp(nics[n], "null") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, Detach(), 1);
                }
                else if (strcmp(nics[n], "nat") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToNAT(), 1);
                }
                else if (strcmp(nics[n], "hostif") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToHostInterface(), 1);
                }
                else if (strcmp(nics[n], "intnet") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(Enabled) (TRUE), 1);
                    CHECK_ERROR_RET(nic, AttachToInternalNetwork(), 1);
                }
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
                if (strcmp(nictype[n], "Am79C970A") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C970A), 1);
                }
                else if (strcmp(nictype[n], "Am79C973") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_Am79C973), 1);
                }
#ifdef VBOX_WITH_E1000
                else if (strcmp(nictype[n], "82540EM") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82540EM), 1);
                }
                else if (strcmp(nictype[n], "82543GC") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(AdapterType)(NetworkAdapterType_I82543GC), 1);
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
                if (strcmp(macs[n], "auto") == 0)
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
                    errorArgument("Invalid -nicspeed%lu argument '%s'", n + 1, nicspeed[n]);
                    rc = E_FAIL;
                    break;
                }
                CHECK_ERROR_RET(nic, COMSETTER(LineSpeed)(u32LineSpeed), 1);
            }

            /* the link status flag? */
            if (cableconnected[n])
            {
                if (strcmp(cableconnected[n], "on") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(CableConnected)(TRUE), 1);
                }
                else if (strcmp(cableconnected[n], "off") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(CableConnected)(FALSE), 1);
                }
                else
                {
                    errorArgument("Invalid -cableconnected%lu argument '%s'", n + 1, cableconnected[n]);
                    rc = E_FAIL;
                    break;
                }
            }

            /* the trace flag? */
            if (nictrace[n])
            {
                if (strcmp(nictrace[n], "on") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TraceEnabled)(TRUE), 1);
                }
                else if (strcmp(nictrace[n], "off") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TraceEnabled)(FALSE), 1);
                }
                else
                {
                    errorArgument("Invalid -nictrace%lu argument '%s'", n + 1, nictrace[n]);
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
            if (hostifdev[n])
            {
                /* remove it? */
                if (strcmp(hostifdev[n], "none") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(HostInterface)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(HostInterface)(Bstr(hostifdev[n])), 1);
                }
            }

            /* the internal network name? */
            if (intnet[n])
            {
                /* remove it? */
                if (strcmp(intnet[n], "none") == 0)
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
#ifdef RT_OS_LINUX
            /* the TAP setup application? */
            if (tapsetup[n])
            {
                /* remove it? */
                if (strcmp(tapsetup[n], "none") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TAPSetupApplication)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TAPSetupApplication)(Bstr(tapsetup[n])), 1);
                }
            }

            /* the TAP terminate application? */
            if (tapterm[n])
            {
                /* remove it? */
                if (strcmp(tapterm[n], "none") == 0)
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TAPTerminateApplication)(NULL), 1);
                }
                else
                {
                    CHECK_ERROR_RET(nic, COMSETTER(TAPTerminateApplication)(Bstr(tapterm[n])), 1);
                }
            }
#endif /* RT_OS_LINUX */

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
                if (strcmp(uarts_mode[n], "disconnected") == 0)
                {
                    CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_Disconnected), 1);
                }
                else
                {
                    CHECK_ERROR_RET(uart, COMSETTER(Path) (Bstr(uarts_path[n])), 1);
                    if (strcmp(uarts_mode[n], "server") == 0)
                    {
                        CHECK_ERROR_RET(uart, COMSETTER(HostMode) (PortMode_HostPipe), 1);
                        CHECK_ERROR_RET(uart, COMSETTER(Server) (TRUE), 1);
                    }
                    else if (strcmp(uarts_mode[n], "client") == 0)
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
                    if (strcmp(vrdp, "on") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(Enabled)(true));
                    }
                    else if (strcmp(vrdp, "off") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(Enabled)(false));
                    }
                    else
                    {
                        errorArgument("Invalid -vrdp argument '%s'", vrdp);
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
                    if (strcmp(vrdpauthtype, "null") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Null));
                    }
                    else if (strcmp(vrdpauthtype, "external") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_External));
                    }
                    else if (strcmp(vrdpauthtype, "guest") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AuthType)(VRDPAuthType_Guest));
                    }
                    else
                    {
                        errorArgument("Invalid -vrdpauthtype argument '%s'", vrdpauthtype);
                        rc = E_FAIL;
                        break;
                    }
                }
                if (vrdpmulticon)
                {
                    if (strcmp(vrdpmulticon, "on") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AllowMultiConnection)(true));
                    }
                    else if (strcmp(vrdpmulticon, "off") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(AllowMultiConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid -vrdpmulticon argument '%s'", vrdpmulticon);
                        rc = E_FAIL;
                        break;
                    }
                }
                if (vrdpreusecon)
                {
                    if (strcmp(vrdpreusecon, "on") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(ReuseSingleConnection)(true));
                    }
                    else if (strcmp(vrdpreusecon, "off") == 0)
                    {
                        CHECK_ERROR(vrdpServer, COMSETTER(ReuseSingleConnection)(false));
                    }
                    else
                    {
                        errorArgument("Invalid -vrdpreusecon argument '%s'", vrdpreusecon);
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
            if (strcmp(snapshotFolder, "default") == 0)
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
            ComPtr<ISATAController> SataCtl;
            CHECK_ERROR(machine, COMGETTER(SATAController)(SataCtl.asOutParam()));
            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(SataCtl, COMSETTER(Enabled)(!!fSataEnabled));
            }
        }

        for (uint32_t i = 4; i < 34; i++)
        {
            if (hdds[i])
            {
                if (strcmp(hdds[i], "none") == 0)
                {
                    machine->DetachHardDisk2(StorageBus_SATA, i-4, 0);
                }
                else
                {
                    /* first guess is that it's a UUID */
                    Guid uuid(hdds[i]);
                    ComPtr<IHardDisk2> hardDisk;
                    rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
                    /* not successful? Then it must be a filename */
                    if (!hardDisk)
                    {
                        CHECK_ERROR(virtualBox, FindHardDisk2(Bstr(hdds[i]), hardDisk.asOutParam()));
                        if (FAILED(rc))
                        {
                            /* open the new hard disk object */
                            CHECK_ERROR(virtualBox, OpenHardDisk2(Bstr(hdds[i]), hardDisk.asOutParam()));
                        }
                    }
                    if (hardDisk)
                    {
                        hardDisk->COMGETTER(Id)(uuid.asOutParam());
                        CHECK_ERROR(machine, AttachHardDisk2(uuid, StorageBus_SATA, i-4, 0));
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
                ComPtr<ISATAController> SataCtl;
                CHECK_ERROR(machine, COMGETTER(SATAController)(SataCtl.asOutParam()));
                if (SUCCEEDED(rc))
                {
                    CHECK_ERROR(SataCtl, SetIDEEmulationPort(i, sataBootDevices[i]));
                }
            }
        }

        if (sataPortCount != -1)
        {
            ComPtr<ISATAController> SataCtl;
            CHECK_ERROR(machine, COMGETTER(SATAController)(SataCtl.asOutParam()));
            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(SataCtl, COMSETTER(PortCount)(sataPortCount));
            }
        }

        /* commit changes */
        CHECK_ERROR(machine, SaveSettings());
    }
    while (0);

    /* it's important to always close sessions */
    session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

static int handleStartVM(int argc, char *argv[],
                         ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc < 1)
        return errorSyntax(USAGE_STARTVM, "Not enough parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        Guid uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        /* default to GUI session type */
        Bstr sessionType = "gui";
        /* has a session type been specified? */
        if ((argc > 2) && (strcmp(argv[1], "-type") == 0))
        {
            if (strcmp(argv[2], "gui") == 0)
            {
                sessionType = "gui";
            }
            else if (strcmp(argv[2], "vrdp") == 0)
            {
                sessionType = "vrdp";
            }
            else if (strcmp(argv[2], "capture") == 0)
            {
                sessionType = "capture";
            }
            else
                return errorArgument("Invalid session type argument '%s'", argv[2]);
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
        CHECK_ERROR_RET(virtualBox, OpenRemoteSession(session, uuid, sessionType,
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
            PRINT_ERROR_INFO(info);
        }
        else
        {
            RTPrintf("Remote session has been successfully opened.\n");
        }
    }

    /* it's important to always close sessions */
    session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleControlVM(int argc, char *argv[],
                           ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc < 2)
        return errorSyntax(USAGE_CONTROLVM, "Not enough parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (argv[0]);
    if (!uuid.isEmpty())
    {
        CHECK_ERROR (virtualBox, GetMachine (uuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR (virtualBox, FindMachine (Bstr(argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id) (uuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET (virtualBox, OpenExistingSession (session, uuid), 1);

    do
    {
        /* get the associated console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK (session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK (session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        /* which command? */
        if (strcmp(argv[1], "pause") == 0)
        {
            CHECK_ERROR_BREAK (console, Pause());
        }
        else if (strcmp(argv[1], "resume") == 0)
        {
            CHECK_ERROR_BREAK (console, Resume());
        }
        else if (strcmp(argv[1], "reset") == 0)
        {
            CHECK_ERROR_BREAK (console, Reset());
        }
        else if (strcmp(argv[1], "poweroff") == 0)
        {
            CHECK_ERROR_BREAK (console, PowerDown());
        }
        else if (strcmp(argv[1], "savestate") == 0)
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
        else if (strcmp(argv[1], "acpipowerbutton") == 0)
        {
            CHECK_ERROR_BREAK (console, PowerButton());
        }
        else if (strcmp(argv[1], "acpisleepbutton") == 0)
        {
            CHECK_ERROR_BREAK (console, SleepButton());
        }
        else if (strcmp(argv[1], "injectnmi") == 0)
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR_BREAK(console, COMGETTER(Debugger)(debugger.asOutParam()));
            CHECK_ERROR_BREAK(debugger, InjectNMI());
        }
        else if (strcmp(argv[1], "keyboardputscancode") == 0)
        {
            ComPtr<IKeyboard> keyboard;
            CHECK_ERROR_BREAK(console, COMGETTER(Keyboard)(keyboard.asOutParam()));

            if (argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'. Expected IBM PC AT set 2 keyboard scancode(s) as hex byte(s).", argv[1]);
                rc = E_FAIL;
                break;
            }

            /* Arbitrary restrict the length of a sequence of scancodes to 1024. */
            LONG alScancodes[1024];
            int cScancodes = 0;

            /* Process the command line. */
            int i;
            for (i = 1 + 1; i < argc && cScancodes < (int)RT_ELEMENTS(alScancodes); i++, cScancodes++)
            {
                if (   RT_C_IS_XDIGIT (argv[i][0])
                    && RT_C_IS_XDIGIT (argv[i][1])
                    && argv[i][2] == 0)
                {
                    uint8_t u8Scancode;
                    int rc = RTStrToUInt8Ex(argv[i], NULL, 16, &u8Scancode);
                    if (RT_FAILURE (rc))
                    {
                        RTPrintf("Error: converting '%s' returned %Rrc!\n", argv[i], rc);
                        rc = E_FAIL;
                        break;
                    }

                    alScancodes[cScancodes] = u8Scancode;
                }
                else
                {
                    RTPrintf("Error: '%s' is not a hex byte!\n", argv[i]);
                    rc = E_FAIL;
                    break;
                }
            }

            if (FAILED(rc))
                break;

            if (   cScancodes == RT_ELEMENTS(alScancodes)
                && i < argc)
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
        else if (strncmp(argv[1], "setlinkstate", 12) == 0)
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK (virtualBox, COMGETTER(SystemProperties) (info.asOutParam()));
            CHECK_ERROR_BREAK (info, COMGETTER(NetworkAdapterCount) (&NetworkAdapterCount));

            unsigned n = parseNum(&argv[1][12], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK (sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                if (strcmp(argv[2], "on") == 0)
                {
                    CHECK_ERROR_BREAK (adapter, COMSETTER(CableConnected)(TRUE));
                }
                else if (strcmp(argv[2], "off") == 0)
                {
                    CHECK_ERROR_BREAK (adapter, COMSETTER(CableConnected)(FALSE));
                }
                else
                {
                    errorArgument("Invalid link state '%s'", Utf8Str(argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
        else if (strcmp (argv[1], "usbattach") == 0 ||
                 strcmp (argv[1], "usbdetach") == 0)
        {
            if (argc < 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Not enough parameters");
                rc = E_FAIL;
                break;
            }

            bool attach = strcmp (argv[1], "usbattach") == 0;

            Guid usbId = argv [2];
            if (usbId.isEmpty())
            {
                // assume address
                if (attach)
                {
                    ComPtr <IHost> host;
                    CHECK_ERROR_BREAK (virtualBox, COMGETTER(Host) (host.asOutParam()));
                    ComPtr <IHostUSBDeviceCollection> coll;
                    CHECK_ERROR_BREAK (host, COMGETTER(USBDevices) (coll.asOutParam()));
                    ComPtr <IHostUSBDevice> dev;
                    CHECK_ERROR_BREAK (coll, FindByAddress (Bstr (argv [2]), dev.asOutParam()));
                    CHECK_ERROR_BREAK (dev, COMGETTER(Id) (usbId.asOutParam()));
                }
                else
                {
                    ComPtr <IUSBDeviceCollection> coll;
                    CHECK_ERROR_BREAK (console, COMGETTER(USBDevices)(coll.asOutParam()));
                    ComPtr <IUSBDevice> dev;
                    CHECK_ERROR_BREAK (coll, FindByAddress (Bstr (argv [2]), dev.asOutParam()));
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
        else if (strcmp(argv[1], "setvideomodehint") == 0)
        {
            if (argc != 5 && argc != 6)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t xres = RTStrToUInt32(argv[2]);
            uint32_t yres = RTStrToUInt32(argv[3]);
            uint32_t bpp  = RTStrToUInt32(argv[4]);
            uint32_t displayIdx = 0;
            if (argc == 6)
                displayIdx = RTStrToUInt32(argv[5]);

            ComPtr<IDisplay> display;
            CHECK_ERROR_BREAK(console, COMGETTER(Display)(display.asOutParam()));
            CHECK_ERROR_BREAK(display, SetVideoModeHint(xres, yres, bpp, displayIdx));
        }
        else if (strcmp(argv[1], "setcredentials") == 0)
        {
            bool fAllowLocalLogon = true;
            if (argc == 7)
            {
                if (strcmp(argv[5], "-allowlocallogon") != 0)
                {
                    errorArgument("Invalid parameter '%s'", argv[5]);
                    rc = E_FAIL;
                    break;
                }
                if (strcmp(argv[6], "no") == 0)
                    fAllowLocalLogon = false;
            }
            else if (argc != 5)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IGuest> guest;
            CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));
            CHECK_ERROR_BREAK(guest, SetCredentials(Bstr(argv[2]), Bstr(argv[3]), Bstr(argv[4]), fAllowLocalLogon));
        }
        else if (strcmp(argv[1], "dvdattach") == 0)
        {
            if (argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            ComPtr<IDVDDrive> dvdDrive;
            sessionMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());
            ASSERT(dvdDrive);

            /* unmount? */
            if (strcmp(argv[2], "none") == 0)
            {
                CHECK_ERROR(dvdDrive, Unmount());
            }
            /* host drive? */
            else if (strncmp(argv[2], "host:", 5) == 0)
            {
                ComPtr<IHost> host;
                CHECK_ERROR(virtualBox, COMGETTER(Host)(host.asOutParam()));
                ComPtr<IHostDVDDriveCollection> hostDVDs;
                CHECK_ERROR(host, COMGETTER(DVDDrives)(hostDVDs.asOutParam()));
                ComPtr<IHostDVDDrive> hostDVDDrive;
                rc = hostDVDs->FindByName(Bstr(argv[2] + 5), hostDVDDrive.asOutParam());
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
                Guid uuid(argv[2]);
                ComPtr<IDVDImage2> dvdImage;
                rc = virtualBox->GetDVDImage(uuid, dvdImage.asOutParam());
                if (FAILED(rc) || !dvdImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = virtualBox->FindDVDImage(Bstr(argv[2]), dvdImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(virtualBox, OpenDVDImage(Bstr(argv[2]), emptyUUID, dvdImage.asOutParam()));
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
        else if (strcmp(argv[1], "floppyattach") == 0)
        {
            if (argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IFloppyDrive> floppyDrive;
            sessionMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
            ASSERT(floppyDrive);

            /* unmount? */
            if (strcmp(argv[2], "none") == 0)
            {
                CHECK_ERROR(floppyDrive, Unmount());
            }
            /* host drive? */
            else if (strncmp(argv[2], "host:", 5) == 0)
            {
                ComPtr<IHost> host;
                CHECK_ERROR(virtualBox, COMGETTER(Host)(host.asOutParam()));
                ComPtr<IHostFloppyDriveCollection> hostFloppies;
                CHECK_ERROR(host, COMGETTER(FloppyDrives)(hostFloppies.asOutParam()));
                ComPtr<IHostFloppyDrive> hostFloppyDrive;
                rc = hostFloppies->FindByName(Bstr(argv[2] + 5), hostFloppyDrive.asOutParam());
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
                Guid uuid(argv[2]);
                ComPtr<IFloppyImage2> floppyImage;
                rc = virtualBox->GetFloppyImage(uuid, floppyImage.asOutParam());
                if (FAILED(rc) || !floppyImage)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = virtualBox->FindFloppyImage(Bstr(argv[2]), floppyImage.asOutParam());
                    /* not registered, do that on the fly */
                    if (!floppyImage)
                    {
                        Guid emptyUUID;
                        CHECK_ERROR(virtualBox, OpenFloppyImage(Bstr(argv[2]), emptyUUID, floppyImage.asOutParam()));
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
        else if (strncmp(argv[1], "-guestmemoryballoon", 19) == 0)
        {
            if (argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest memory balloon size '%s'", argv[2]);
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
        else if (strncmp(argv[1], "-gueststatisticsinterval", 24) == 0)
        {
            if (argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest statistics interval '%s'", argv[2]);
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
            errorSyntax(USAGE_CONTROLVM, "Invalid parameter '%s'", Utf8Str(argv[1]).raw());
            rc = E_FAIL;
        }
    }
    while (0);

    session->Close();

    return SUCCEEDED (rc) ? 0 : 1;
}

static int handleDiscardState(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc != 1)
        return errorSyntax(USAGE_DISCARDSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Guid guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(virtualBox, OpenSession(session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, DiscardSavedState());
            }
            while (0);
            CHECK_ERROR_BREAK(session, Close());
        }
        while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleAdoptdState(int argc, char *argv[],
                             ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc != 2)
        return errorSyntax(USAGE_ADOPTSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Guid guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(virtualBox, OpenSession(session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, AdoptSavedState (Bstr (argv[1])));
            }
            while (0);
            CHECK_ERROR_BREAK(session, Close());
        }
        while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSnapshot(int argc, char *argv[],
                          ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    /* we need at least a VM and a command */
    if (argc < 2)
        return errorSyntax(USAGE_SNAPSHOT, "Not enough parameters");

    /* the first argument must be the VM */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Guid guid;
    machine->COMGETTER(Id)(guid.asOutParam());

    do
    {
        /* we have to open a session for this task. First try an existing session */
        rc = virtualBox->OpenExistingSession(session, guid);
        if (FAILED(rc))
            CHECK_ERROR_BREAK(virtualBox, OpenSession(session, guid));
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));

        /* switch based on the command */
        if (strcmp(argv[1], "take") == 0)
        {
            /* there must be a name */
            if (argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }
            Bstr name(argv[2]);
            if ((argc > 3) && ((argc != 5) || (strcmp(argv[3], "-desc") != 0)))
            {
                errorSyntax(USAGE_SNAPSHOT, "Incorrect description format");
                rc = E_FAIL;
                break;
            }
            Bstr desc;
            if (argc == 5)
                desc = argv[4];
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, TakeSnapshot(name, desc, progress.asOutParam()));

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to take snapshot. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to take snapshot. No error message available!\n");
            }
        }
        else if (strcmp(argv[1], "discard") == 0)
        {
            /* exactly one parameter: snapshot name */
            if (argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(argv[2]), snapshot.asOutParam()));
            }

            snapshot->COMGETTER(Id)(guid.asOutParam());

            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, DiscardSnapshot(guid, progress.asOutParam()));

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to discard snapshot. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to discard snapshot. No error message available!\n");
            }
        }
        else if (strcmp(argv[1], "discardcurrent") == 0)
        {
            if (   (argc != 3)
                || (   (strcmp(argv[2], "-state") != 0)
                    && (strcmp(argv[2], "-all") != 0)))
            {
                errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(argv[2]).raw());
                rc = E_FAIL;
                break;
            }
            bool fAll = false;
            if (strcmp(argv[2], "-all") == 0)
                fAll = true;

            ComPtr<IProgress> progress;

            if (fAll)
            {
                CHECK_ERROR_BREAK(console, DiscardCurrentSnapshotAndState(progress.asOutParam()));
            }
            else
            {
                CHECK_ERROR_BREAK(console, DiscardCurrentState(progress.asOutParam()));
            }

            showProgress(progress);
            progress->COMGETTER(ResultCode)(&rc);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: failed to discard. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: failed to discard. No error message available!\n");
            }

        }
        else if (strcmp(argv[1], "edit") == 0)
        {
            if (argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            if (strcmp(argv[2], "-current") == 0)
            {
                CHECK_ERROR_BREAK(machine, COMGETTER(CurrentSnapshot)(snapshot.asOutParam()));
            }
            else
            {
                /* assume it's a UUID */
                Guid guid(argv[2]);
                if (!guid.isEmpty())
                {
                    CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
                }
                else
                {
                    /* then it must be a name */
                    CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(argv[2]), snapshot.asOutParam()));
                }
            }

            /* parse options */
            for (int i = 3; i < argc; i++)
            {
                if (strcmp(argv[i], "-newname") == 0)
                {
                    if (argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Name)(Bstr(argv[i]));
                }
                else if (strcmp(argv[i], "-newdesc") == 0)
                {
                    if (argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Description)(Bstr(argv[i]));
                }
                else
                {
                    errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
                    rc = E_FAIL;
                    break;
                }
            }

        }
        else if (strcmp(argv[1], "showvminfo") == 0)
        {
            /* exactly one parameter: snapshot name */
            if (argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(argv[2]), snapshot.asOutParam()));
            }

            /* get the machine of the given snapshot */
            ComPtr<IMachine> machine;
            snapshot->COMGETTER(Machine)(machine.asOutParam());
            showVMInfo(virtualBox, machine, console);
        }
        else
        {
            errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(argv[1]).raw());
            rc = E_FAIL;
        }
    } while (0);

    session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleShowHardDiskInfo(int argc, char *argv[],
                                  ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc != 1)
        return errorSyntax(USAGE_SHOWHDINFO, "Incorrect number of parameters");

    ComPtr<IHardDisk2> hardDisk;
    Bstr filepath;

    bool unknown = false;

    /* first guess is that it's a UUID */
    Guid uuid(argv[0]);
    rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
    /* no? then it must be a filename */
    if (FAILED (rc))
    {
        filepath = argv[0];
        rc = virtualBox->FindHardDisk2(filepath, hardDisk.asOutParam());
        /* no? well, then it's an unkwnown image */
        if (FAILED (rc))
        {
            CHECK_ERROR(virtualBox, OpenHardDisk2(filepath, hardDisk.asOutParam()));
            if (SUCCEEDED (rc))
            {
                unknown = true;
            }
        }
    }
    do
    {
        if (!SUCCEEDED(rc))
            break;

        hardDisk->COMGETTER(Id)(uuid.asOutParam());
        RTPrintf("UUID:                 %s\n", uuid.toString().raw());

        /* check for accessibility */
        /// @todo NEWMEDIA check accessibility of all parents
        /// @todo NEWMEDIA print the full state value
        MediaState_T state;
        CHECK_ERROR_BREAK (hardDisk, COMGETTER(State)(&state));
        RTPrintf("Accessible:           %s\n", state != MediaState_Inaccessible ? "yes" : "no");

        if (state == MediaState_Inaccessible)
        {
            Bstr err;
            CHECK_ERROR_BREAK (hardDisk, COMGETTER(LastAccessError)(err.asOutParam()));
            RTPrintf("Access Error:         %lS\n", err.raw());
        }

        Bstr description;
        hardDisk->COMGETTER(Description)(description.asOutParam());
        if (description)
        {
            RTPrintf("Description:          %lS\n", description.raw());
        }

        ULONG64 logicalSize;
        hardDisk->COMGETTER(LogicalSize)(&logicalSize);
        RTPrintf("Logical size:         %llu MBytes\n", logicalSize);
        ULONG64 actualSize;
        hardDisk->COMGETTER(Size)(&actualSize);
        RTPrintf("Current size on disk: %llu MBytes\n", actualSize >> 20);

        HardDiskType_T type;
        hardDisk->COMGETTER(Type)(&type);
        const char *typeStr = "unknown";
        switch (type)
        {
            case HardDiskType_Normal:
                typeStr = "normal";
                break;
            case HardDiskType_Immutable:
                typeStr = "immutable";
                break;
            case HardDiskType_Writethrough:
                typeStr = "writethrough";
                break;
        }
        RTPrintf("Type:                 %s\n", typeStr);

        Bstr format;
        hardDisk->COMGETTER(Format)(format.asOutParam());
        RTPrintf("Storage format:       %lS\n", format.raw());

        if (!unknown)
        {
            com::SafeGUIDArray machineIds;
            hardDisk->COMGETTER(MachineIds)(ComSafeArrayAsOutParam(machineIds));
            for (size_t j = 0; j < machineIds.size(); ++ j)
            {
                ComPtr<IMachine> machine;
                CHECK_ERROR(virtualBox, GetMachine(machineIds[j], machine.asOutParam()));
                ASSERT(machine);
                Bstr name;
                machine->COMGETTER(Name)(name.asOutParam());
                machine->COMGETTER(Id)(uuid.asOutParam());
                RTPrintf("%s%lS (UUID: %RTuuid)\n",
                         j == 0 ? "In use by VMs:        " : "                      ",
                         name.raw(), &machineIds[j]);
            }
            /// @todo NEWMEDIA check usage in snapshots too
            /// @todo NEWMEDIA also list children and say 'differencing' for
            /// hard disks with the parent or 'base' otherwise.
        }

        Bstr loc;
        hardDisk->COMGETTER(Location)(loc.asOutParam());
        RTPrintf("Location:             %lS\n", loc.raw());
    }
    while (0);

    if (unknown)
    {
        /* close the unknown hard disk to forget it again */
        hardDisk->Close();
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleOpenMedium(int argc, char *argv[],
                            ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc < 2)
        return errorSyntax(USAGE_REGISTERIMAGE, "Not enough parameters");

    Bstr filepath(argv[1]);

    if (strcmp(argv[0], "disk") == 0)
    {
        const char *type = NULL;
        /* there can be a type parameter */
        if ((argc > 2) && (argc != 4))
            return errorSyntax(USAGE_REGISTERIMAGE, "Incorrect number of parameters");
        if (argc == 4)
        {
            if (strcmp(argv[2], "-type") != 0)
                return errorSyntax(USAGE_REGISTERIMAGE, "Invalid parameter '%s'", Utf8Str(argv[2]).raw());
            if (   (strcmp(argv[3], "normal") != 0)
                && (strcmp(argv[3], "immutable") != 0)
                && (strcmp(argv[3], "writethrough") != 0))
                return errorArgument("Invalid hard disk type '%s' specified", Utf8Str(argv[3]).raw());
            type = argv[3];
        }

        ComPtr<IHardDisk2> hardDisk;
        CHECK_ERROR(virtualBox, OpenHardDisk2(filepath, hardDisk.asOutParam()));
        if (SUCCEEDED(rc) && hardDisk)
        {
            /* change the type if requested */
            if (type)
            {
                if (strcmp(type, "normal") == 0)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Normal));
                else if (strcmp(type, "immutable") == 0)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Immutable));
                else if (strcmp(type, "writethrough") == 0)
                    CHECK_ERROR(hardDisk, COMSETTER(Type)(HardDiskType_Writethrough));
            }
        }
    }
    else if (strcmp(argv[0], "dvd") == 0)
    {
        ComPtr<IDVDImage2> dvdImage;
        CHECK_ERROR(virtualBox, OpenDVDImage(filepath, Guid(), dvdImage.asOutParam()));
    }
    else if (strcmp(argv[0], "floppy") == 0)
    {
        ComPtr<IFloppyImage2> floppyImage;
        CHECK_ERROR(virtualBox, OpenFloppyImage(filepath, Guid(), floppyImage.asOutParam()));
    }
    else
        return errorSyntax(USAGE_REGISTERIMAGE, "Invalid parameter '%s'", Utf8Str(argv[1]).raw());

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCloseMedium(int argc, char *argv[],
                             ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    if (argc != 2)
        return errorSyntax(USAGE_UNREGISTERIMAGE, "Incorrect number of parameters");

    /* first guess is that it's a UUID */
    Guid uuid(argv[1]);

    if (strcmp(argv[0], "disk") == 0)
    {
        ComPtr<IHardDisk2> hardDisk;
        rc = virtualBox->GetHardDisk2(uuid, hardDisk.asOutParam());
        /* not a UUID or not registered? Then it must be a filename */
        if (!hardDisk)
        {
            CHECK_ERROR(virtualBox, FindHardDisk2(Bstr(argv[1]), hardDisk.asOutParam()));
        }
        if (SUCCEEDED(rc) && hardDisk)
        {
            CHECK_ERROR(hardDisk, Close());
        }
    }
    else
    if (strcmp(argv[0], "dvd") == 0)
    {
        ComPtr<IDVDImage2> dvdImage;
        rc = virtualBox->GetDVDImage(uuid, dvdImage.asOutParam());
        /* not a UUID or not registered? Then it must be a filename */
        if (!dvdImage)
        {
            CHECK_ERROR(virtualBox, FindDVDImage(Bstr(argv[1]), dvdImage.asOutParam()));
        }
        if (SUCCEEDED(rc) && dvdImage)
        {
            CHECK_ERROR(dvdImage, Close());
        }
    }
    else
    if (strcmp(argv[0], "floppy") == 0)
    {
        ComPtr<IFloppyImage2> floppyImage;
        rc = virtualBox->GetFloppyImage(uuid, floppyImage.asOutParam());
        /* not a UUID or not registered? Then it must be a filename */
        if (!floppyImage)
        {
            CHECK_ERROR(virtualBox, FindFloppyImage(Bstr(argv[1]), floppyImage.asOutParam()));
        }
        if (SUCCEEDED(rc) && floppyImage)
        {
            CHECK_ERROR(floppyImage, Close());
        }
    }
    else
        return errorSyntax(USAGE_UNREGISTERIMAGE, "Invalid parameter '%s'", Utf8Str(argv[1]).raw());

    return SUCCEEDED(rc) ? 0 : 1;
}

#ifdef RT_OS_WINDOWS
static int handleCreateHostIF(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    if (argc != 1)
        return errorSyntax(USAGE_CREATEHOSTIF, "Incorrect number of parameters");

    HRESULT rc = S_OK;

    do
    {
        ComPtr<IHost> host;
        CHECK_ERROR_BREAK(virtualBox, COMGETTER(Host)(host.asOutParam()));

        ComPtr<IHostNetworkInterface> hostif;
        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK(host,
            CreateHostNetworkInterface(Bstr(argv[0]),
                                       hostif.asOutParam(),
                                       progress.asOutParam()));

        showProgress(progress);
        HRESULT result;
        CHECK_ERROR_BREAK(progress, COMGETTER(ResultCode)(&result));
        if (FAILED(result))
        {
            com::ProgressErrorInfo info(progress);
            PRINT_ERROR_INFO(info);
            rc = result;
        }
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleRemoveHostIF(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    if (argc != 1)
        return errorSyntax(USAGE_REMOVEHOSTIF, "Incorrect number of parameters");

    HRESULT rc = S_OK;

    do
    {
        ComPtr<IHost> host;
        CHECK_ERROR_BREAK(virtualBox, COMGETTER(Host)(host.asOutParam()));

        ComPtr<IHostNetworkInterface> hostif;

        /* first guess is that it's a UUID */
        Guid uuid(argv[0]);
        if (uuid.isEmpty())
        {
            /* not a valid UUID, search for it */
            ComPtr<IHostNetworkInterfaceCollection> coll;
            CHECK_ERROR_BREAK(host, COMGETTER(NetworkInterfaces)(coll.asOutParam()));
            CHECK_ERROR_BREAK(coll, FindByName(Bstr(argv[0]), hostif.asOutParam()));
            CHECK_ERROR_BREAK(hostif, COMGETTER(Id)(uuid.asOutParam()));
        }

        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK(host,
            RemoveHostNetworkInterface(uuid,
                                       hostif.asOutParam(),
                                       progress.asOutParam()));

        showProgress(progress);
        HRESULT result;
        CHECK_ERROR_BREAK(progress, COMGETTER(ResultCode)(&result));
        if (FAILED(result))
        {
            com::ProgressErrorInfo info(progress);
            PRINT_ERROR_INFO(info);
            rc = result;
        }
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}
#endif /* RT_OS_WINDOWS */

static int handleGetExtraData(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc = S_OK;

    if (argc != 2)
        return errorSyntax(USAGE_GETEXTRADATA, "Incorrect number of parameters");

    /* global data? */
    if (strcmp(argv[0], "global") == 0)
    {
        /* enumeration? */
        if (strcmp(argv[1], "enumerate") == 0)
        {
            Bstr extraDataKey;

            do
            {
                Bstr nextExtraDataKey;
                Bstr nextExtraDataValue;
                HRESULT rcEnum = virtualBox->GetNextExtraDataKey(extraDataKey, nextExtraDataKey.asOutParam(),
                                                                 nextExtraDataValue.asOutParam());
                extraDataKey = nextExtraDataKey;

                if (SUCCEEDED(rcEnum) && extraDataKey)
                    RTPrintf("Key: %lS, Value: %lS\n", nextExtraDataKey.raw(), nextExtraDataValue.raw());
            } while (extraDataKey);
        }
        else
        {
            Bstr value;
            CHECK_ERROR(virtualBox, GetExtraData(Bstr(argv[1]), value.asOutParam()));
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
        rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            /* enumeration? */
            if (strcmp(argv[1], "enumerate") == 0)
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
                CHECK_ERROR(machine, GetExtraData(Bstr(argv[1]), value.asOutParam()));
                if (value)
                    RTPrintf("Value: %lS\n", value.raw());
                else
                    RTPrintf("No value set!\n");
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetExtraData(int argc, char *argv[],
                              ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc = S_OK;

    if (argc < 2)
        return errorSyntax(USAGE_SETEXTRADATA, "Not enough parameters");

    /* global data? */
    if (strcmp(argv[0], "global") == 0)
    {
        if (argc < 3)
            CHECK_ERROR(virtualBox, SetExtraData(Bstr(argv[1]), NULL));
        else if (argc == 3)
            CHECK_ERROR(virtualBox, SetExtraData(Bstr(argv[1]), Bstr(argv[2])));
        else
            return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
    }
    else
    {
        ComPtr<IMachine> machine;
        /* assume it's a UUID */
        rc = virtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(virtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            if (argc < 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(argv[1]), NULL));
            else if (argc == 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(argv[1]), Bstr(argv[2])));
            else
                return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetProperty(int argc, char *argv[],
                             ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> session)
{
    HRESULT rc;

    /* there must be two arguments: property name and value */
    if (argc != 2)
        return errorSyntax(USAGE_SETPROPERTY, "Incorrect number of parameters");

    ComPtr<ISystemProperties> systemProperties;
    virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

    if (strcmp(argv[0], "hdfolder") == 0)
    {
        /* reset to default? */
        if (strcmp(argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(Bstr(argv[1])));
    }
    else if (strcmp(argv[0], "machinefolder") == 0)
    {
        /* reset to default? */
        if (strcmp(argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(Bstr(argv[1])));
    }
    else if (strcmp(argv[0], "vrdpauthlibrary") == 0)
    {
        /* reset to default? */
        if (strcmp(argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(Bstr(argv[1])));
    }
    else if (strcmp(argv[0], "websrvauthlibrary") == 0)
    {
        /* reset to default? */
        if (strcmp(argv[1], "default") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(Bstr(argv[1])));
    }
    else if (strcmp(argv[0], "hwvirtexenabled") == 0)
    {
        if (strcmp(argv[1], "yes") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(HWVirtExEnabled)(TRUE));
        else if (strcmp(argv[1], "no") == 0)
            CHECK_ERROR(systemProperties, COMSETTER(HWVirtExEnabled)(FALSE));
        else
            return errorArgument("Invalid value '%s' for hardware virtualization extension flag", argv[1]);
    }
    else if (strcmp(argv[0], "loghistorycount") == 0)
    {
        uint32_t uVal;
        int vrc;
        vrc = RTStrToUInt32Ex(argv[1], NULL, 0, &uVal);
        if (vrc != VINF_SUCCESS)
            return errorArgument("Error parsing Log history count '%s'", argv[1]);
        CHECK_ERROR(systemProperties, COMSETTER(LogHistoryCount)(uVal));
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", argv[0]);

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleUSBFilter (int argc, char *argv[],
                            ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc = S_OK;
    USBFilterCmd cmd;

    /* at least: 0: command, 1: index, 2: -target, 3: <target value> */
    if (argc < 4)
        return errorSyntax(USAGE_USBFILTER, "Not enough parameters");

    /* which command? */
    cmd.mAction = USBFilterCmd::Invalid;
    if      (strcmp (argv [0], "add") == 0)     cmd.mAction = USBFilterCmd::Add;
    else if (strcmp (argv [0], "modify") == 0)  cmd.mAction = USBFilterCmd::Modify;
    else if (strcmp (argv [0], "remove") == 0)  cmd.mAction = USBFilterCmd::Remove;

    if (cmd.mAction == USBFilterCmd::Invalid)
        return errorSyntax(USAGE_USBFILTER, "Invalid parameter '%s'", argv[0]);

    /* which index? */
    if (VINF_SUCCESS !=  RTStrToUInt32Full (argv[1], 10, &cmd.mIndex))
        return errorSyntax(USAGE_USBFILTER, "Invalid index '%s'", argv[1]);

    switch (cmd.mAction)
    {
        case USBFilterCmd::Add:
        case USBFilterCmd::Modify:
        {
            /* at least: 0: command, 1: index, 2: -target, 3: <target value>, 4: -name, 5: <name value> */
            if (argc < 6)
            {
                if (cmd.mAction == USBFilterCmd::Add)
                    return errorSyntax(USAGE_USBFILTER_ADD, "Not enough parameters");

                return errorSyntax(USAGE_USBFILTER_MODIFY, "Not enough parameters");
            }

            // set Active to true by default
            // (assuming that the user sets up all necessary attributes
            // at once and wants the filter to be active immediately)
            if (cmd.mAction == USBFilterCmd::Add)
                cmd.mFilter.mActive = true;

            for (int i = 2; i < argc; i++)
            {
                if  (strcmp(argv [i], "-target") == 0)
                {
                    if (argc <= i + 1 || !*argv[i+1])
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    if (strcmp (argv [i], "global") == 0)
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = aVirtualBox->GetMachine(Guid(argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(aVirtualBox, FindMachine(Bstr(argv[i]), cmd.mMachine.asOutParam()), 1);
                        }
                    }
                }
                else if (strcmp(argv [i], "-name") == 0)
                {
                    if (argc <= i + 1 || !*argv[i+1])
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mName = argv [i];
                }
                else if (strcmp(argv [i], "-active") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    if (strcmp (argv [i], "yes") == 0)
                        cmd.mFilter.mActive = true;
                    else if (strcmp (argv [i], "no") == 0)
                        cmd.mFilter.mActive = false;
                    else
                        return errorArgument("Invalid -active argument '%s'", argv[i]);
                }
                else if (strcmp(argv [i], "-vendorid") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mVendorId = argv [i];
                }
                else if (strcmp(argv [i], "-productid") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mProductId = argv [i];
                }
                else if (strcmp(argv [i], "-revision") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mRevision = argv [i];
                }
                else if (strcmp(argv [i], "-manufacturer") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mManufacturer = argv [i];
                }
                else if (strcmp(argv [i], "-product") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mProduct = argv [i];
                }
                else if (strcmp(argv [i], "-remote") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mRemote = argv[i];
                }
                else if (strcmp(argv [i], "-serialnumber") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    cmd.mFilter.mSerialNumber = argv [i];
                }
                else if (strcmp(argv [i], "-maskedinterfaces") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    uint32_t u32;
                    rc = RTStrToUInt32Full(argv[i], 0, &u32);
                    if (RT_FAILURE(rc))
                        return errorArgument("Failed to convert the -maskedinterfaces value '%s' to a number, rc=%Rrc", argv[i], rc);
                    cmd.mFilter.mMaskedInterfaces = u32;
                }
                else if (strcmp(argv [i], "-action") == 0)
                {
                    if (argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    if (strcmp (argv [i], "ignore") == 0)
                        cmd.mFilter.mAction = USBDeviceFilterAction_Ignore;
                    else if (strcmp (argv [i], "hold") == 0)
                        cmd.mFilter.mAction = USBDeviceFilterAction_Hold;
                    else
                        return errorArgument("Invalid USB filter action '%s'", argv[i]);
                }
                else
                    return errorSyntax(cmd.mAction == USBFilterCmd::Add ? USAGE_USBFILTER_ADD : USAGE_USBFILTER_MODIFY,
                                       "Unknown option '%s'", argv[i]);
            }

            if (cmd.mAction == USBFilterCmd::Add)
            {
                // mandatory/forbidden options
                if (   cmd.mFilter.mName.isEmpty()
                    ||
                       (   cmd.mGlobal
                        && cmd.mFilter.mAction == USBDeviceFilterAction_Null
                       )
                    || (   !cmd.mGlobal
                        && !cmd.mMachine)
                    || (   cmd.mGlobal
                        && cmd.mFilter.mRemote)
                   )
                {
                    return errorSyntax(USAGE_USBFILTER_ADD, "Mandatory options not supplied");
                }
            }
            break;
        }

        case USBFilterCmd::Remove:
        {
            /* at least: 0: command, 1: index, 2: -target, 3: <target value> */
            if (argc < 4)
                return errorSyntax(USAGE_USBFILTER_REMOVE, "Not enough parameters");

            for (int i = 2; i < argc; i++)
            {
                if  (strcmp(argv [i], "-target") == 0)
                {
                    if (argc <= i + 1 || !*argv[i+1])
                        return errorArgument("Missing argument to '%s'", argv[i]);
                    i++;
                    if (strcmp (argv [i], "global") == 0)
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = aVirtualBox->GetMachine(Guid(argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(aVirtualBox, FindMachine(Bstr(argv[i]), cmd.mMachine.asOutParam()), 1);
                        }
                    }
                }
            }

            // mandatory options
            if (!cmd.mGlobal && !cmd.mMachine)
                return errorSyntax(USAGE_USBFILTER_REMOVE, "Mandatory options not supplied");

            break;
        }

        default: break;
    }

    USBFilterCmd::USBFilter &f = cmd.mFilter;

    ComPtr <IHost> host;
    ComPtr <IUSBController> ctl;
    if (cmd.mGlobal)
        CHECK_ERROR_RET (aVirtualBox, COMGETTER(Host) (host.asOutParam()), 1);
    else
    {
        Guid uuid;
        cmd.mMachine->COMGETTER(Id)(uuid.asOutParam());
        /* open a session for the VM */
        CHECK_ERROR_RET (aVirtualBox, OpenSession(aSession, uuid), 1);
        /* get the mutable session machine */
        aSession->COMGETTER(Machine)(cmd.mMachine.asOutParam());
        /* and get the USB controller */
        CHECK_ERROR_RET (cmd.mMachine, COMGETTER(USBController) (ctl.asOutParam()), 1);
    }

    switch (cmd.mAction)
    {
        case USBFilterCmd::Add:
        {
            if (cmd.mGlobal)
            {
                ComPtr <IHostUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (host, CreateUSBDeviceFilter (f.mName, flt.asOutParam()));

                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                if (f.mAction != USBDeviceFilterAction_Null)
                    CHECK_ERROR_BREAK (flt, COMSETTER(Action) (f.mAction));

                CHECK_ERROR_BREAK (host, InsertUSBDeviceFilter (cmd.mIndex, flt));
            }
            else
            {
                ComPtr <IUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (ctl, CreateDeviceFilter (f.mName, flt.asOutParam()));

                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mRemote.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Remote) (f.mRemote.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                CHECK_ERROR_BREAK (ctl, InsertDeviceFilter (cmd.mIndex, flt));
            }
            break;
        }
        case USBFilterCmd::Modify:
        {
            if (cmd.mGlobal)
            {
                ComPtr <IHostUSBDeviceFilterCollection> coll;
                CHECK_ERROR_BREAK (host, COMGETTER(USBDeviceFilters) (coll.asOutParam()));
                ComPtr <IHostUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (coll, GetItemAt (cmd.mIndex, flt.asOutParam()));

                if (!f.mName.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Name) (f.mName.setNullIfEmpty()));
                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));

                if (f.mAction != USBDeviceFilterAction_Null)
                    CHECK_ERROR_BREAK (flt, COMSETTER(Action) (f.mAction));
            }
            else
            {
                ComPtr <IUSBDeviceFilterCollection> coll;
                CHECK_ERROR_BREAK (ctl, COMGETTER(DeviceFilters) (coll.asOutParam()));

                ComPtr <IUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (coll, GetItemAt (cmd.mIndex, flt.asOutParam()));

                if (!f.mName.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Name) (f.mName.setNullIfEmpty()));
                if (!f.mActive.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Active) (f.mActive));
                if (!f.mVendorId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(VendorId) (f.mVendorId.setNullIfEmpty()));
                if (!f.mProductId.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(ProductId) (f.mProductId.setNullIfEmpty()));
                if (!f.mRevision.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Revision) (f.mRevision.setNullIfEmpty()));
                if (!f.mManufacturer.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Manufacturer) (f.mManufacturer.setNullIfEmpty()));
                if (!f.mRemote.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(Remote) (f.mRemote.setNullIfEmpty()));
                if (!f.mSerialNumber.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(SerialNumber) (f.mSerialNumber.setNullIfEmpty()));
                if (!f.mMaskedInterfaces.isNull())
                    CHECK_ERROR_BREAK (flt, COMSETTER(MaskedInterfaces) (f.mMaskedInterfaces));
            }
            break;
        }
        case USBFilterCmd::Remove:
        {
            if (cmd.mGlobal)
            {
                ComPtr <IHostUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (host, RemoveUSBDeviceFilter (cmd.mIndex, flt.asOutParam()));
            }
            else
            {
                ComPtr <IUSBDeviceFilter> flt;
                CHECK_ERROR_BREAK (ctl, RemoveDeviceFilter (cmd.mIndex, flt.asOutParam()));
            }
            break;
        }
        default:
            break;
    }

    if (cmd.mMachine)
    {
        /* commit and close the session */
        CHECK_ERROR(cmd.mMachine, SaveSettings());
        aSession->Close();
    }

    return SUCCEEDED (rc) ? 0 : 1;
}

static int handleSharedFolder (int argc, char *argv[],
                               ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc;

    /* we need at least a command and target */
    if (argc < 2)
        return errorSyntax(USAGE_SHAREDFOLDER, "Not enough parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = aVirtualBox->GetMachine(Guid(argv[1]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[1]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Guid uuid;
    machine->COMGETTER(Id)(uuid.asOutParam());

    if (strcmp(argv[0], "add") == 0)
    {
        /* we need at least four more parameters */
        if (argc < 5)
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Not enough parameters");

        char *name = NULL;
        char *hostpath = NULL;
        bool fTransient = false;
        bool fWritable = true;

        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "-name") == 0)
            {
                if (argc <= i + 1 || !*argv[i+1])
                    return errorArgument("Missing argument to '%s'", argv[i]);
                i++;
                name = argv[i];
            }
            else if (strcmp(argv[i], "-hostpath") == 0)
            {
                if (argc <= i + 1 || !*argv[i+1])
                    return errorArgument("Missing argument to '%s'", argv[i]);
                i++;
                hostpath = argv[i];
            }
            else if (strcmp(argv[i], "-readonly") == 0)
            {
                fWritable = false;
            }
            else if (strcmp(argv[i], "-transient") == 0)
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
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
            CHECK_ERROR_RET(aVirtualBox, OpenExistingSession (aSession, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(aSession, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(aSession, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (console)
                aSession->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET (aVirtualBox, OpenSession(aSession, uuid), 1);

            /* get the mutable session machine */
            aSession->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (SUCCEEDED(rc))
                CHECK_ERROR(machine, SaveSettings());

            aSession->Close();
        }
    }
    else if (strcmp(argv[0], "remove") == 0)
    {
        /* we need at least two more parameters */
        if (argc < 3)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Not enough parameters");

        char *name = NULL;
        bool fTransient = false;

        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "-name") == 0)
            {
                if (argc <= i + 1 || !*argv[i+1])
                    return errorArgument("Missing argument to '%s'", argv[i]);
                i++;
                name = argv[i];
            }
            else if (strcmp(argv[i], "-transient") == 0)
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }

        /* required arguments */
        if (!name)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Parameter -name is required");

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(aVirtualBox, OpenExistingSession (aSession, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(aSession, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(aSession, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, RemoveSharedFolder(Bstr(name)));

            if (console)
                aSession->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET (aVirtualBox, OpenSession(aSession, uuid), 1);

            /* get the mutable session machine */
            aSession->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, RemoveSharedFolder(Bstr(name)));

            /* commit and close the session */
            CHECK_ERROR(machine, SaveSettings());
            aSession->Close();
        }
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", Utf8Str(argv[0]).raw());

    return 0;
}

static int handleVMStatistics(int argc, char *argv[],
                              ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc;

    /* at least one option: the UUID or name of the VM */
    if (argc < 1)
        return errorSyntax(USAGE_VM_STATISTICS, "Incorrect number of parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Guid uuid (argv[0]);
    if (!uuid.isEmpty())
        CHECK_ERROR(aVirtualBox, GetMachine(uuid, machine.asOutParam()));
    else
    {
        CHECK_ERROR(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(uuid.asOutParam());
    }
    if (FAILED(rc))
        return 1;

    /* parse arguments. */
    bool fReset = false;
    bool fWithDescriptions = false;
    const char *pszPattern = NULL; /* all */
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-pattern"))
        {
            if (pszPattern)
                return errorSyntax(USAGE_VM_STATISTICS, "Multiple -patterns options is not permitted");
            if (i + 1 >= argc)
                return errorArgument("Missing argument to '%s'", argv[i]);
            pszPattern = argv[++i];
        }
        else if (!strcmp(argv[i], "-descriptions"))
            fWithDescriptions = true;
        /* add: -file <filename> and -formatted */
        else if (!strcmp(argv[i], "-reset"))
            fReset = true;
        else
            return errorSyntax(USAGE_VM_STATISTICS, "Unknown option '%s'", argv[i]);
    }
    if (fReset && fWithDescriptions)
        return errorSyntax(USAGE_VM_STATISTICS, "The -reset and -descriptions options does not mix");


    /* open an existing session for the VM. */
    CHECK_ERROR(aVirtualBox, OpenExistingSession(aSession, uuid));
    if (SUCCEEDED(rc))
    {
        /* get the session console. */
        ComPtr <IConsole> console;
        CHECK_ERROR(aSession, COMGETTER(Console)(console.asOutParam()));
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
            aSession->Close();
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
        CHECK_RC_BREAK (virtualBox->
                        COMGETTER(SettingsFormatVersion) (formatVersion.asOutParam()));

        bool isGlobalConverted = false;
        std::list <ComPtr <IMachine> > cvtMachines;
        std::list <Utf8Str> fileList;
        Bstr version;
        Bstr filePath;

        com::SafeIfaceArray <IMachine> machines;
        CHECK_RC_BREAK (virtualBox->
                        COMGETTER(Machines2) (ComSafeArrayAsOutParam (machines)));

        for (size_t i = 0; i < machines.size(); ++ i)
        {
            BOOL accessible;
            CHECK_RC_BREAK (machines [i]->
                            COMGETTER(Accessible) (&accessible));
            if (!accessible)
                continue;

            CHECK_RC_BREAK (machines [i]->
                            COMGETTER(SettingsFileVersion) (version.asOutParam()));

            if (version != formatVersion)
            {
                cvtMachines.push_back (machines [i]);
                Bstr filePath;
                CHECK_RC_BREAK (machines [i]->
                                COMGETTER(SettingsFilePath) (filePath.asOutParam()));
                fileList.push_back (Utf8StrFmt ("%ls  (%ls)", filePath.raw(),
                                                version.raw()));
            }
        }

        CHECK_RC_BREAK (rc);

        CHECK_RC_BREAK (virtualBox->
                        COMGETTER(SettingsFileVersion) (version.asOutParam()));
        if (version != formatVersion)
        {
            isGlobalConverted = true;
            CHECK_RC_BREAK (virtualBox->
                            COMGETTER(SettingsFilePath) (filePath.asOutParam()));
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
                CHECK_RC_BREAK ((*m)->COMGETTER(Id) (id.asOutParam()));

                /* open a session for the VM */
                CHECK_ERROR_BREAK (virtualBox, OpenSession (session, id));

                ComPtr <IMachine> sm;
                CHECK_RC_BREAK (session->COMGETTER(Machine) (sm.asOutParam()));

                Bstr bakFileName;
                if (fConvertSettings == ConvertSettings_Backup)
                    CHECK_ERROR (sm, SaveSettingsWithBackup (bakFileName.asOutParam()));
                else
                    CHECK_ERROR (sm, SaveSettings());

                session->Close();

                CHECK_RC_BREAK (rc);
            }

            CHECK_RC_BREAK (rc);

            if (isGlobalConverted)
            {
                Bstr bakFileName;
                if (fConvertSettings == ConvertSettings_Backup)
                    CHECK_ERROR (virtualBox, SaveSettingsWithBackup (bakFileName.asOutParam()));
                else
                    CHECK_ERROR (virtualBox, SaveSettings());
            }

            CHECK_RC_BREAK (rc);
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

    CHECK_RC_RET (com::Initialize());

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

    /* convertdd: does not need a VirtualBox instantiation) */
    if (argc >= iCmdArg && (strcmp(argv[iCmd], "convertdd") == 0))
    {
        rc = handleConvertDDImage(argc - iCmdArg, argv + iCmdArg);
        break;
    }

    ComPtr <IVirtualBox> virtualBox;
    ComPtr <ISession> session;

    rc = virtualBox.createLocalObject (CLSID_VirtualBox);
    if (FAILED(rc))
    {
        RTPrintf ("[!] Failed to create the VirtualBox object!\n");
        PRINT_RC_MESSAGE (rc);

        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
            RTPrintf ("[!] Most likely, the VirtualBox COM server is not running "
                      "or failed to start.\n");
        else
            PRINT_ERROR_INFO (info);
        break;
    }

    CHECK_RC_BREAK (session.createInprocObject (CLSID_Session));

    /* create the event queue
     * (here it is necessary only to process remaining XPCOM/IPC events
     * after the session is closed) */

#ifdef USE_XPCOM_QUEUE
    NS_GetMainEventQ(getter_AddRefs(g_pEventQ));
#endif

    if (!checkForAutoConvertedSettings (virtualBox, session, fConvertSettings))
        break;

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
        { "addiscsidisk",     handleAddiSCSIDisk },
        { "createvm",         handleCreateVM },
        { "modifyvm",         handleModifyVM },
        { "clonehd",          handleCloneHardDisk },
        { "clonevdi",         handleCloneHardDisk }, /* backward compatiblity */
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
#ifdef RT_OS_WINDOWS
        { "createhostif",     handleCreateHostIF },
        { "removehostif",     handleRemoveHostIF },
#endif
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
        { NULL,               NULL }
    };

    int commandIndex;
    for (commandIndex = 0; commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (strcmp(commandHandlers[commandIndex].command, argv[iCmd]) == 0)
        {
            rc = commandHandlers[commandIndex].handler(argc - iCmdArg, &argv[iCmdArg], virtualBox, session);
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
    g_pEventQ->ProcessPendingEvents();
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
