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
typedef int (*PFNHANDLER)(HandlerArg *a);

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
                 "(C) 2005-2009 Sun Microsystems, Inc.\n"
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
        RTPrintf("VBoxManage showvminfo       <uuid>|<name> [-details] [-statistics]\n"
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
        RTPrintf("VBoxManage unregistervm     <uuid>|<name> [-delete]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CREATEVM)
    {
        RTPrintf("VBoxManage createvm         -name <name>\n"
                 "                            [-ostype <ostype>]\n"
                 "                            [-register]\n"
                 "                            [-basefolder <path> | -settingsfile <path>]\n"
                 "                            [-uuid <uuid>]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_IMPORTAPPLIANCE)
    {
        RTPrintf("VBoxManage import           <ovf>\n"
                 "\n"); // @todo
    }

    if (u64Cmd & USAGE_EXPORTAPPLIANCE)
    {
        RTPrintf("VBoxManage export           <machines> -o <ovf>\n"
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
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN)
                 "                            [-nic<1-N> none|null|nat|hostif|intnet|hostonly]\n"
#else /* !RT_OS_LINUX && !RT_OS_DARWIN */
                 "                            [-nic<1-N> none|null|nat|hostif|intnet]\n"
#endif /* !RT_OS_LINUX && !RT_OS_DARWIN  */
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
                 "                            floppyattach none|<uuid>|<filename>|host:<drive> |\n");
        if (fVRDP)
        {
            RTPrintf("                            vrdp on|off] |\n");
        }
        RTPrintf("                            setvideomodehint <xres> <yres> <bpp> [display]|\n"
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
                 "                            [-format VDI|VMDK|VHD]\n"
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
                 "                            [-format VDI|VMDK|VHD|RAW|<other>]\n"
                 "                            [-remember]\n"
                 "\n");
    }

    if (u64Cmd & USAGE_CONVERTFROMRAW)
    {
        RTPrintf("VBoxManage convertfromraw   [-static] [-format VDI|VMDK|VHD]\n"
                 "                            <filename> <outputfile>\n"
                 "VBoxManage convertfromraw   [-static] [-format VDI|VMDK|VHD]\n"
                 "                            stdin <outputfile> <bytes>\n"
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
                 "                            [-intnet]\n"
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
void showProgress(ComPtr<IProgress> progress)
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
            else if (strcmp(a->argv[2], "vrdp") == 0)
            {
                sessionType = "vrdp";
            }
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
                    ComPtr <IUSBDeviceCollection> coll;
                    CHECK_ERROR_BREAK (console, COMGETTER(USBDevices)(coll.asOutParam()));
                    ComPtr <IUSBDevice> dev;
                    CHECK_ERROR_BREAK (coll, FindByAddress (Bstr (a->argv [2]), dev.asOutParam()));
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
                ComPtr<IHostDVDDriveCollection> hostDVDs;
                CHECK_ERROR(host, COMGETTER(DVDDrives)(hostDVDs.asOutParam()));
                ComPtr<IHostDVDDrive> hostDVDDrive;
                rc = hostDVDs->FindByName(Bstr(a->argv[2] + 5), hostDVDDrive.asOutParam());
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
                ComPtr<IHostFloppyDriveCollection> hostFloppies;
                CHECK_ERROR(host, COMGETTER(FloppyDrives)(hostFloppies.asOutParam()));
                ComPtr<IHostFloppyDrive> hostFloppyDrive;
                rc = hostFloppies->FindByName(Bstr(a->argv[2] + 5), hostFloppyDrive.asOutParam());
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

static int handleSnapshot(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a VM and a command */
    if (a->argc < 2)
        return errorSyntax(USAGE_SNAPSHOT, "Not enough parameters");

    /* the first argument must be the VM */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Guid guid;
    machine->COMGETTER(Id)(guid.asOutParam());

    do
    {
        /* we have to open a session for this task. First try an existing session */
        rc = a->virtualBox->OpenExistingSession(a->session, guid);
        if (FAILED(rc))
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));

        /* switch based on the command */
        if (strcmp(a->argv[1], "take") == 0)
        {
            /* there must be a name */
            if (a->argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }
            Bstr name(a->argv[2]);
            if ((a->argc > 3) && ((a->argc != 5) || (strcmp(a->argv[3], "-desc") != 0)))
            {
                errorSyntax(USAGE_SNAPSHOT, "Incorrect description format");
                rc = E_FAIL;
                break;
            }
            Bstr desc;
            if (a->argc == 5)
                desc = a->argv[4];
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
        else if (strcmp(a->argv[1], "discard") == 0)
        {
            /* exactly one parameter: snapshot name */
            if (a->argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
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
        else if (strcmp(a->argv[1], "discardcurrent") == 0)
        {
            if (   (a->argc != 3)
                || (   (strcmp(a->argv[2], "-state") != 0)
                    && (strcmp(a->argv[2], "-all") != 0)))
            {
                errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[2]).raw());
                rc = E_FAIL;
                break;
            }
            bool fAll = false;
            if (strcmp(a->argv[2], "-all") == 0)
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
        else if (strcmp(a->argv[1], "edit") == 0)
        {
            if (a->argc < 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Missing snapshot name");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            if (strcmp(a->argv[2], "-current") == 0)
            {
                CHECK_ERROR_BREAK(machine, COMGETTER(CurrentSnapshot)(snapshot.asOutParam()));
            }
            else
            {
                /* assume it's a UUID */
                Guid guid(a->argv[2]);
                if (!guid.isEmpty())
                {
                    CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
                }
                else
                {
                    /* then it must be a name */
                    CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
                }
            }

            /* parse options */
            for (int i = 3; i < a->argc; i++)
            {
                if (strcmp(a->argv[i], "-newname") == 0)
                {
                    if (a->argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", a->argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Name)(Bstr(a->argv[i]));
                }
                else if (strcmp(a->argv[i], "-newdesc") == 0)
                {
                    if (a->argc <= i + 1)
                    {
                        errorArgument("Missing argument to '%s'", a->argv[i]);
                        rc = E_FAIL;
                        break;
                    }
                    i++;
                    snapshot->COMSETTER(Description)(Bstr(a->argv[i]));
                }
                else
                {
                    errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
                    rc = E_FAIL;
                    break;
                }
            }

        }
        else if (strcmp(a->argv[1], "showvminfo") == 0)
        {
            /* exactly one parameter: snapshot name */
            if (a->argc != 3)
            {
                errorSyntax(USAGE_SNAPSHOT, "Expecting snapshot name only");
                rc = E_FAIL;
                break;
            }

            ComPtr<ISnapshot> snapshot;

            /* assume it's a UUID */
            Guid guid(a->argv[2]);
            if (!guid.isEmpty())
            {
                CHECK_ERROR_BREAK(machine, GetSnapshot(guid, snapshot.asOutParam()));
            }
            else
            {
                /* then it must be a name */
                CHECK_ERROR_BREAK(machine, FindSnapshot(Bstr(a->argv[2]), snapshot.asOutParam()));
            }

            /* get the machine of the given snapshot */
            ComPtr<IMachine> machine;
            snapshot->COMGETTER(Machine)(machine.asOutParam());
            showVMInfo(a->virtualBox, machine, console);
        }
        else
        {
            errorSyntax(USAGE_SNAPSHOT, "Invalid parameter '%s'", Utf8Str(a->argv[1]).raw());
            rc = E_FAIL;
        }
    } while (0);

    a->session->Close();

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

static int handleUSBFilter (HandlerArg *a)
{
    HRESULT rc = S_OK;
    USBFilterCmd cmd;

    /* at least: 0: command, 1: index, 2: -target, 3: <target value> */
    if (a->argc < 4)
        return errorSyntax(USAGE_USBFILTER, "Not enough parameters");

    /* which command? */
    cmd.mAction = USBFilterCmd::Invalid;
    if      (strcmp (a->argv [0], "add") == 0)     cmd.mAction = USBFilterCmd::Add;
    else if (strcmp (a->argv [0], "modify") == 0)  cmd.mAction = USBFilterCmd::Modify;
    else if (strcmp (a->argv [0], "remove") == 0)  cmd.mAction = USBFilterCmd::Remove;

    if (cmd.mAction == USBFilterCmd::Invalid)
        return errorSyntax(USAGE_USBFILTER, "Invalid parameter '%s'", a->argv[0]);

    /* which index? */
    if (VINF_SUCCESS !=  RTStrToUInt32Full (a->argv[1], 10, &cmd.mIndex))
        return errorSyntax(USAGE_USBFILTER, "Invalid index '%s'", a->argv[1]);

    switch (cmd.mAction)
    {
        case USBFilterCmd::Add:
        case USBFilterCmd::Modify:
        {
            /* at least: 0: command, 1: index, 2: -target, 3: <target value>, 4: -name, 5: <name value> */
            if (a->argc < 6)
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

            for (int i = 2; i < a->argc; i++)
            {
                if  (strcmp(a->argv [i], "-target") == 0)
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (strcmp (a->argv [i], "global") == 0)
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = a->virtualBox->GetMachine(Guid(a->argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam()), 1);
                        }
                    }
                }
                else if (strcmp(a->argv [i], "-name") == 0)
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mName = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-active") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (strcmp (a->argv [i], "yes") == 0)
                        cmd.mFilter.mActive = true;
                    else if (strcmp (a->argv [i], "no") == 0)
                        cmd.mFilter.mActive = false;
                    else
                        return errorArgument("Invalid -active argument '%s'", a->argv[i]);
                }
                else if (strcmp(a->argv [i], "-vendorid") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mVendorId = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-productid") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mProductId = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-revision") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mRevision = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-manufacturer") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mManufacturer = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-product") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mProduct = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-remote") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mRemote = a->argv[i];
                }
                else if (strcmp(a->argv [i], "-serialnumber") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    cmd.mFilter.mSerialNumber = a->argv [i];
                }
                else if (strcmp(a->argv [i], "-maskedinterfaces") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    uint32_t u32;
                    rc = RTStrToUInt32Full(a->argv[i], 0, &u32);
                    if (RT_FAILURE(rc))
                        return errorArgument("Failed to convert the -maskedinterfaces value '%s' to a number, rc=%Rrc", a->argv[i], rc);
                    cmd.mFilter.mMaskedInterfaces = u32;
                }
                else if (strcmp(a->argv [i], "-action") == 0)
                {
                    if (a->argc <= i + 1)
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (strcmp (a->argv [i], "ignore") == 0)
                        cmd.mFilter.mAction = USBDeviceFilterAction_Ignore;
                    else if (strcmp (a->argv [i], "hold") == 0)
                        cmd.mFilter.mAction = USBDeviceFilterAction_Hold;
                    else
                        return errorArgument("Invalid USB filter action '%s'", a->argv[i]);
                }
                else
                    return errorSyntax(cmd.mAction == USBFilterCmd::Add ? USAGE_USBFILTER_ADD : USAGE_USBFILTER_MODIFY,
                                       "Unknown option '%s'", a->argv[i]);
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
            if (a->argc < 4)
                return errorSyntax(USAGE_USBFILTER_REMOVE, "Not enough parameters");

            for (int i = 2; i < a->argc; i++)
            {
                if  (strcmp(a->argv [i], "-target") == 0)
                {
                    if (a->argc <= i + 1 || !*a->argv[i+1])
                        return errorArgument("Missing argument to '%s'", a->argv[i]);
                    i++;
                    if (strcmp (a->argv [i], "global") == 0)
                        cmd.mGlobal = true;
                    else
                    {
                        /* assume it's a UUID of a machine */
                        rc = a->virtualBox->GetMachine(Guid(a->argv[i]), cmd.mMachine.asOutParam());
                        if (FAILED(rc) || !cmd.mMachine)
                        {
                            /* must be a name */
                            CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[i]), cmd.mMachine.asOutParam()), 1);
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
        CHECK_ERROR_RET (a->virtualBox, COMGETTER(Host) (host.asOutParam()), 1);
    else
    {
        Guid uuid;
        cmd.mMachine->COMGETTER(Id)(uuid.asOutParam());
        /* open a session for the VM */
        CHECK_ERROR_RET (a->virtualBox, OpenSession(a->session, uuid), 1);
        /* get the mutable session machine */
        a->session->COMGETTER(Machine)(cmd.mMachine.asOutParam());
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
        a->session->Close();
    }

    return SUCCEEDED (rc) ? 0 : 1;
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
