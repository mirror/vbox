/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Host
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

#ifdef RT_OS_LINUX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <mntent.h>
/* bird: This is a hack to work around conflicts between these linux kernel headers
 *       and the GLIBC tcpip headers. They have different declarations of the 4
 *       standard byte order functions. */
#define _LINUX_BYTEORDER_GENERIC_H
#include <linux/cdrom.h>
#ifdef VBOX_USE_LIBHAL
// # include <libhal.h>
// /* These are defined by libhal.h and by VBox header files. */
// # undef TRUE
// # undef FALSE
#include "vbox-libhal.h"
#endif
#include <errno.h>
#endif /* RT_OS_LINUX */

#ifdef RT_OS_SOLARIS
# include <fcntl.h>
# include <unistd.h>
# include <stropts.h>
# include <errno.h>
# include <limits.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/cdio.h>
# include <sys/dkio.h>
# include <sys/mnttab.h>
# include <sys/mntent.h>
# ifdef VBOX_USE_LIBHAL
#  include "vbox-libhal.h"
extern "C" char *getfullrawname(char *);
# endif
#endif /* RT_OS_SOLARIS */

#ifdef RT_OS_WINDOWS
#define _WIN32_DCOM
#include <windows.h>
#include <shellapi.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <shlobj.h>
#include <cfgmgr32.h>
#endif /* RT_OS_WINDOWS */


#include "HostImpl.h"
#include "HostDVDDriveImpl.h"
#include "HostFloppyDriveImpl.h"
#ifdef VBOX_WITH_USB
# include "HostUSBDeviceImpl.h"
# include "USBDeviceFilterImpl.h"
# include "USBProxyService.h"
#endif
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"
#include "Logging.h"

#ifdef RT_OS_DARWIN
# include "darwin/iokit.h"
#endif

#ifdef RT_OS_WINDOWS
# include "HostNetworkInterfaceImpl.h"
#endif

#include <VBox/usb.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/param.h>
#include <iprt/env.h>
#ifdef RT_OS_SOLARIS
# include <iprt/path.h>
#endif

#include <stdio.h>

#include <algorithm>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT Host::FinalConstruct()
{
    return S_OK;
}

void Host::FinalRelease()
{
    if (isReady())
        uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT Host::init (VirtualBox *parent)
{
    LogFlowThisFunc (("isReady=%d\n", isReady()));

    ComAssertRet (parent, E_INVALIDARG);

    AutoWriteLock alock (this);
    ComAssertRet (!isReady(), E_UNEXPECTED);

    mParent = parent;

#if defined (RT_OS_DARWIN) && defined (VBOX_WITH_USB)
    mUSBProxyService = new USBProxyServiceDarwin (this);
#elif defined (RT_OS_LINUX) && defined (VBOX_WITH_USB)
    mUSBProxyService = new USBProxyServiceLinux (this);
#elif defined (RT_OS_OS2) && defined (VBOX_WITH_USB)
    mUSBProxyService = new USBProxyServiceOs2 (this);
#elif defined (RT_OS_WINDOWS) && defined (VBOX_WITH_USB)
    mUSBProxyService = new USBProxyServiceWin32 (this);
#elif defined (VBOX_WITH_USB)
    mUSBProxyService = new USBProxyService (this);
#endif
    /** @todo handle !mUSBProxySerivce->isActive() and mUSBProxyService->getLastError()
     * and somehow report or whatever that the proxy failed to startup.
     * Also, there might be init order issues... */

    setReady(true);
    return S_OK;
}

/**
 *  Uninitializes the host object and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Host::uninit()
{
    LogFlowThisFunc (("isReady=%d\n", isReady()));

    AssertReturn (isReady(), (void) 0);

#ifdef VBOX_WITH_USB
    /* wait for USB proxy service to terminate before we uninit all USB
     * devices */
    LogFlowThisFunc (("Stopping USB proxy service...\n"));
    delete mUSBProxyService;
    mUSBProxyService = NULL;
    LogFlowThisFunc (("Done stopping USB proxy service.\n"));
#endif

    /* uninit all USB device filters still referenced by clients */
    uninitDependentChildren();

#ifdef VBOX_WITH_USB
    mUSBDeviceFilters.clear();
    mUSBDevices.clear();
#endif

    setReady (FALSE);
}

// IHost properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns a list of host DVD drives.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(DVDDrives) (IHostDVDDriveCollection **drives)
{
    if (!drives)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    std::list <ComObjPtr <HostDVDDrive> > list;

#if defined(RT_OS_WINDOWS)
    int sz = GetLogicalDriveStrings(0, NULL);
    TCHAR *hostDrives = new TCHAR[sz+1];
    GetLogicalDriveStrings(sz, hostDrives);
    wchar_t driveName[3] = { '?', ':', '\0' };
    TCHAR *p = hostDrives;
    do
    {
        if (GetDriveType(p) == DRIVE_CDROM)
        {
            driveName[0] = *p;
            ComObjPtr <HostDVDDrive> hostDVDDriveObj;
            hostDVDDriveObj.createObject();
            hostDVDDriveObj->init (Bstr (driveName));
            list.push_back (hostDVDDriveObj);
        }
        p += _tcslen(p) + 1;
    }
    while (*p);
    delete[] hostDrives;

#elif defined(RT_OS_SOLARIS)
# ifdef VBOX_USE_LIBHAL
    if (!getDVDInfoFromHal(list))
# endif
    // Not all Solaris versions ship with libhal.
    // So use a fallback approach similar to Linux.
    {
        if (RTEnvGet("VBOX_CDROM"))
        {
            char *cdromEnv = strdup(RTEnvGet("VBOX_CDROM"));
            char *cdromDrive;
            cdromDrive = strtok(cdromEnv, ":"); /** @todo use strtok_r. */
            while (cdromDrive)
            {
                if (validateDevice(cdromDrive, true))
                {
                    ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init (Bstr (cdromDrive));
                    list.push_back (hostDVDDriveObj);
                }
                cdromDrive = strtok(NULL, ":");
            }
            free(cdromEnv);
        }
        else
        {
            // this might work on Solaris version older than Nevada.
            if (validateDevice("/cdrom/cdrom0", true))
            {
                ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                hostDVDDriveObj.createObject();
                hostDVDDriveObj->init (Bstr ("cdrom/cdrom0"));
                list.push_back (hostDVDDriveObj);
            }

            // check the mounted drives
            parseMountTable(MNTTAB, list);
        }
    }

#elif defined(RT_OS_LINUX)
#ifdef VBOX_USE_LIBHAL
    if (!getDVDInfoFromHal(list)) /* Playing with #defines in this way is nasty, I know. */
#endif /* USE_LIBHAL defined */
    // On Linux without hal, the situation is much more complex. We will take a
    // heuristical approach and also allow the user to specify a list of host
    // CDROMs using an environment variable.
    // The general strategy is to try some known device names and see of they
    // exist. At last, we'll enumerate the /etc/fstab file (luckily there's an
    // API to parse it) for CDROM devices. Ok, let's start!

    {
        if (RTEnvGet("VBOX_CDROM"))
        {
            char *cdromEnv = strdupa(RTEnvGet("VBOX_CDROM"));
            char *cdromDrive;
            cdromDrive = strtok(cdromEnv, ":"); /** @todo use strtok_r */
            while (cdromDrive)
            {
                if (validateDevice(cdromDrive, true))
                {
                    ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init (Bstr (cdromDrive));
                    list.push_back (hostDVDDriveObj);
                }
                cdromDrive = strtok(NULL, ":");
            }
        }
        else
        {
            // this is a good guess usually
            if (validateDevice("/dev/cdrom", true))
            {
                    ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init (Bstr ("/dev/cdrom"));
                    list.push_back (hostDVDDriveObj);
            }

            // check the mounted drives
            parseMountTable((char*)"/etc/mtab", list);

            // check the drives that can be mounted
            parseMountTable((char*)"/etc/fstab", list);
        }
    }
#elif defined(RT_OS_DARWIN)
    PDARWINDVD cur = DarwinGetDVDDrives();
    while (cur)
    {
        ComObjPtr<HostDVDDrive> hostDVDDriveObj;
        hostDVDDriveObj.createObject();
        hostDVDDriveObj->init(Bstr(cur->szName));
        list.push_back(hostDVDDriveObj);

        /* next */
        void *freeMe = cur;
        cur = cur->pNext;
        RTMemFree(freeMe);
    }

#else
    /* PORTME */
#endif

    ComObjPtr<HostDVDDriveCollection> collection;
    collection.createObject();
    collection->init (list);
    collection.queryInterfaceTo(drives);
    return S_OK;
}

/**
 * Returns a list of host floppy drives.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(FloppyDrives) (IHostFloppyDriveCollection **drives)
{
    if (!drives)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    std::list <ComObjPtr <HostFloppyDrive> > list;

#ifdef RT_OS_WINDOWS
    int sz = GetLogicalDriveStrings(0, NULL);
    TCHAR *hostDrives = new TCHAR[sz+1];
    GetLogicalDriveStrings(sz, hostDrives);
    wchar_t driveName[3] = { '?', ':', '\0' };
    TCHAR *p = hostDrives;
    do
    {
        if (GetDriveType(p) == DRIVE_REMOVABLE)
        {
            driveName[0] = *p;
            ComObjPtr <HostFloppyDrive> hostFloppyDriveObj;
            hostFloppyDriveObj.createObject();
            hostFloppyDriveObj->init (Bstr (driveName));
            list.push_back (hostFloppyDriveObj);
        }
        p += _tcslen(p) + 1;
    }
    while (*p);
    delete[] hostDrives;
#elif defined(RT_OS_LINUX)
#ifdef VBOX_USE_LIBHAL
    if (!getFloppyInfoFromHal(list)) /* Playing with #defines in this way is nasty, I know. */
#endif /* USE_LIBHAL defined */
    // As with the CDROMs, on Linux we have to take a multi-level approach
    // involving parsing the mount tables. As this is not bulletproof, we'll
    // give the user the chance to override the detection by an environment
    // variable and skip the detection.

    {
        if (RTEnvGet("VBOX_FLOPPY"))
        {
            char *floppyEnv = strdupa(RTEnvGet("VBOX_FLOPPY"));
            char *floppyDrive;
            floppyDrive = strtok(floppyEnv, ":");
            while (floppyDrive)
            {
                // check if this is an acceptable device
                if (validateDevice(floppyDrive, false))
                {
                    ComObjPtr <HostFloppyDrive> hostFloppyDriveObj;
                    hostFloppyDriveObj.createObject();
                    hostFloppyDriveObj->init (Bstr (floppyDrive));
                    list.push_back (hostFloppyDriveObj);
                }
                floppyDrive = strtok(NULL, ":");
            }
        }
        else
        {
            // we assume that a floppy is always /dev/fd[x] with x from 0 to 7
            char devName[10];
            for (int i = 0; i <= 7; i++)
            {
                sprintf(devName, "/dev/fd%d", i);
                if (validateDevice(devName, false))
                {
                    ComObjPtr <HostFloppyDrive> hostFloppyDriveObj;
                    hostFloppyDriveObj.createObject();
                    hostFloppyDriveObj->init (Bstr (devName));
                    list.push_back (hostFloppyDriveObj);
                }
            }
        }
    }
#else
    /* PORTME */
#endif

    ComObjPtr<HostFloppyDriveCollection> collection;
    collection.createObject();
    collection->init (list);
    collection.queryInterfaceTo(drives);
    return S_OK;
}

#ifdef RT_OS_WINDOWS

static bool IsTAPDevice(const char *guid)
{
    HKEY hNetcard;
    LONG status;
    DWORD len;
    int i = 0;
    bool ret = false;

    status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ, &hNetcard);
    if (status != ERROR_SUCCESS)
        return false;

    while(true)
    {
        char szEnumName[256];
        char szNetCfgInstanceId[256];
        DWORD dwKeyType;
        HKEY  hNetCardGUID;

        len = sizeof(szEnumName);
        status = RegEnumKeyExA(hNetcard, i, szEnumName, &len, NULL, NULL, NULL, NULL);
        if (status != ERROR_SUCCESS)
            break;

        status = RegOpenKeyExA(hNetcard, szEnumName, 0, KEY_READ, &hNetCardGUID);
        if (status == ERROR_SUCCESS)
        {
            len = sizeof (szNetCfgInstanceId);
            status = RegQueryValueExA(hNetCardGUID, "NetCfgInstanceId", NULL, &dwKeyType, (LPBYTE)szNetCfgInstanceId, &len);
            if (status == ERROR_SUCCESS && dwKeyType == REG_SZ)
            {
                char szNetProductName[256];
                char szNetProviderName[256];

                szNetProductName[0] = 0;
                len = sizeof(szNetProductName);
                status = RegQueryValueExA(hNetCardGUID, "ProductName", NULL, &dwKeyType, (LPBYTE)szNetProductName, &len);

                szNetProviderName[0] = 0;
                len = sizeof(szNetProviderName);
                status = RegQueryValueExA(hNetCardGUID, "ProviderName", NULL, &dwKeyType, (LPBYTE)szNetProviderName, &len);

                if (   !strcmp(szNetCfgInstanceId, guid)
                    && !strcmp(szNetProductName, "VirtualBox TAP Adapter")
                    && !strcmp(szNetProviderName, "innotek GmbH"))
                {
                    ret = true;
                    RegCloseKey(hNetCardGUID);
                    break;
                }
            }
            RegCloseKey(hNetCardGUID);
        }
        ++i;
    }

    RegCloseKey (hNetcard);
    return ret;
}

/**
 * Returns a list of host network interfaces.
 *
 * @returns COM status code
 * @param drives address of result pointer
 */
STDMETHODIMP Host::COMGETTER(NetworkInterfaces) (IHostNetworkInterfaceCollection **networkInterfaces)
{
    if (!networkInterfaces)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();

    std::list <ComObjPtr <HostNetworkInterface> > list;

    static const char *NetworkKey = "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                                    "{4D36E972-E325-11CE-BFC1-08002BE10318}";
    HKEY hCtrlNet;
    LONG status;
    DWORD len;
    status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, NetworkKey, 0, KEY_READ, &hCtrlNet);
    if (status != ERROR_SUCCESS)
        return setError (E_FAIL, tr("Could not open registry key \"%s\""), NetworkKey);

    for (int i = 0;; ++ i)
    {
        char szNetworkGUID [256];
        HKEY hConnection;
        char szNetworkConnection [256];

        len = sizeof (szNetworkGUID);
        status = RegEnumKeyExA (hCtrlNet, i, szNetworkGUID, &len, NULL, NULL, NULL, NULL);
        if (status != ERROR_SUCCESS)
            break;

        if (!IsTAPDevice(szNetworkGUID))
            continue;

        RTStrPrintf (szNetworkConnection, sizeof (szNetworkConnection),
                     "%s\\Connection", szNetworkGUID);
        status = RegOpenKeyExA (hCtrlNet, szNetworkConnection, 0, KEY_READ,  &hConnection);
        if (status == ERROR_SUCCESS)
        {
            DWORD dwKeyType;
            status = RegQueryValueExW (hConnection, TEXT("Name"), NULL,
                                       &dwKeyType, NULL, &len);
            if (status == ERROR_SUCCESS && dwKeyType == REG_SZ)
            {
                size_t uniLen = (len + sizeof (OLECHAR) - 1) / sizeof (OLECHAR);
                Bstr name (uniLen + 1 /* extra zero */);
                status = RegQueryValueExW (hConnection, TEXT("Name"), NULL,
                                           &dwKeyType, (LPBYTE) name.mutableRaw(), &len);
                if (status == ERROR_SUCCESS)
                {
                    RTLogPrintf("Connection name %ls\n", name.mutableRaw());
                    /* put a trailing zero, just in case (see MSDN) */
                    name.mutableRaw() [uniLen] = 0;
                    /* create a new object and add it to the list */
                    ComObjPtr <HostNetworkInterface> iface;
                    iface.createObject();
                    /* remove the curly bracket at the end */
                    szNetworkGUID [strlen(szNetworkGUID) - 1] = '\0';
                    if (SUCCEEDED (iface->init (name, Guid (szNetworkGUID + 1))))
                        list.push_back (iface);
                }
            }
            RegCloseKey (hConnection);
        }
    }
    RegCloseKey (hCtrlNet);

    ComObjPtr <HostNetworkInterfaceCollection> collection;
    collection.createObject();
    collection->init (list);
    collection.queryInterfaceTo (networkInterfaces);
    return S_OK;
}
#endif /* RT_OS_WINDOWS */

STDMETHODIMP Host::COMGETTER(USBDevices)(IHostUSBDeviceCollection **aUSBDevices)
{
#ifdef VBOX_WITH_USB
    if (!aUSBDevices)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    MultiResult rc = checkUSBProxyService();
    CheckComRCReturnRC (rc);

    ComObjPtr <HostUSBDeviceCollection> collection;
    collection.createObject();
    collection->init (mUSBDevices);
    collection.queryInterfaceTo (aUSBDevices);

    return rc;
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::COMGETTER(USBDeviceFilters) (IHostUSBDeviceFilterCollection **aUSBDeviceFilters)
{
#ifdef VBOX_WITH_USB
    if (!aUSBDeviceFilters)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    MultiResult rc = checkUSBProxyService();
    CheckComRCReturnRC (rc);

    ComObjPtr <HostUSBDeviceFilterCollection> collection;
    collection.createObject();
    collection->init (mUSBDeviceFilters);
    collection.queryInterfaceTo (aUSBDeviceFilters);

    return rc;
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

/**
 * Returns the number of installed logical processors
 *
 * @returns COM status code
 * @param   count address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorCount)(ULONG *count)
{
    if (!count)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    *count = RTSystemProcessorGetCount();
    return S_OK;
}

/**
 * Returns the (approximate) speed of the host CPU in MHz
 *
 * @returns COM status code
 * @param   speed address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorSpeed)(ULONG *speed)
{
    if (!speed)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo Add a runtime function for this which uses GIP. */
    return S_OK;
}
/**
 * Returns a description string for the host CPU
 *
 * @returns COM status code
 * @param   description address of result variable
 */
STDMETHODIMP Host::COMGETTER(ProcessorDescription)(BSTR *description)
{
    if (!description)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo */
    return S_OK;
}


/**
 * Returns the amount of installed system memory in megabytes
 *
 * @returns COM status code
 * @param   size address of result variable
 */
STDMETHODIMP Host::COMGETTER(MemorySize)(ULONG *size)
{
    if (!size)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo */
    return S_OK;
}

/**
 * Returns the current system memory free space in megabytes
 *
 * @returns COM status code
 * @param   available address of result variable
 */
STDMETHODIMP Host::COMGETTER(MemoryAvailable)(ULONG *available)
{
    if (!available)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo */
    return S_OK;
}

/**
 * Returns the name string of the host operating system
 *
 * @returns COM status code
 * @param   os address of result variable
 */
STDMETHODIMP Host::COMGETTER(OperatingSystem)(BSTR *os)
{
    if (!os)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo */
    return S_OK;
}

/**
 * Returns the version string of the host operating system
 *
 * @returns COM status code
 * @param   os address of result variable
 */
STDMETHODIMP Host::COMGETTER(OSVersion)(BSTR *version)
{
    if (!version)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    /** @todo */
    return S_OK;
}

/**
 * Returns the current host time in milliseconds since 1970-01-01 UTC.
 *
 * @returns COM status code
 * @param   time address of result variable
 */
STDMETHODIMP Host::COMGETTER(UTCTime)(LONG64 *aUTCTime)
{
    if (!aUTCTime)
        return E_POINTER;
    AutoWriteLock alock (this);
    CHECK_READY();
    RTTIMESPEC now;
    *aUTCTime = RTTimeSpecGetMilli(RTTimeNow(&now));
    return S_OK;
}

// IHost methods
////////////////////////////////////////////////////////////////////////////////

#ifdef RT_OS_WINDOWS

/**
 * Returns TRUE if the Windows version is 6.0 or greater (i.e. it's Vista and
 * later OSes) and it has the UAC (User Account Control) feature enabled.
 */
static BOOL IsUACEnabled()
{
    LONG rc = 0;

    OSVERSIONINFOEX info;
    ZeroMemory (&info, sizeof (OSVERSIONINFOEX));
    info.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
    rc = GetVersionEx ((OSVERSIONINFO *) &info);
    AssertReturn (rc != 0, FALSE);

    LogFlowFunc (("dwMajorVersion=%d, dwMinorVersion=%d\n",
                  info.dwMajorVersion, info.dwMinorVersion));

    /* we are interested only in Vista (and newer versions...). In all
     * earlier versions UAC is not present. */
    if (info.dwMajorVersion < 6)
        return FALSE;

    /* the default EnableLUA value is 1 (Enabled) */
    DWORD dwEnableLUA = 1;

    HKEY hKey;
    rc = RegOpenKeyExA (HKEY_LOCAL_MACHINE,
                        "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                        0, KEY_QUERY_VALUE, &hKey);

    Assert (rc == ERROR_SUCCESS || rc == ERROR_PATH_NOT_FOUND);
    if (rc == ERROR_SUCCESS)
    {

        DWORD cbEnableLUA = sizeof (dwEnableLUA);
        rc = RegQueryValueExA (hKey, "EnableLUA", NULL, NULL,
                               (LPBYTE) &dwEnableLUA, &cbEnableLUA);

        RegCloseKey (hKey);

        Assert (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
    }

    LogFlowFunc (("rc=%d, dwEnableLUA=%d\n", rc, dwEnableLUA));

    return dwEnableLUA == 1;
}

struct NetworkInterfaceHelperClientData
{
    SVCHlpMsg::Code msgCode;
    /* for SVCHlpMsg::CreateHostNetworkInterface */
    Bstr name;
    ComObjPtr <HostNetworkInterface> iface;
    /* for SVCHlpMsg::RemoveHostNetworkInterface */
    Guid guid;
};

STDMETHODIMP
Host::CreateHostNetworkInterface (INPTR BSTR aName,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
    if (!aName)
        return E_INVALIDARG;
    if (!aHostNetworkInterface)
        return E_POINTER;
    if (!aProgress)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = S_OK;

    /* first check whether an interface with the given name already exists */
    {
        ComPtr <IHostNetworkInterfaceCollection> coll;
        rc = COMGETTER(NetworkInterfaces) (coll.asOutParam());
        CheckComRCReturnRC (rc);
        ComPtr <IHostNetworkInterface> iface;
        if (SUCCEEDED (coll->FindByName (aName, iface.asOutParam())))
            return setError (E_FAIL,
                tr ("Host network interface '%ls' already exists"), aName);
    }

    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    rc = progress->init (mParent, static_cast <IHost *> (this),
                         Bstr (tr ("Creating host network interface")),
                         FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);
    progress.queryInterfaceTo (aProgress);

    /* create a new uninitialized host interface object */
    ComObjPtr <HostNetworkInterface> iface;
    iface.createObject();
    iface.queryInterfaceTo (aHostNetworkInterface);

    /* create the networkInterfaceHelperClient() argument */
    std::auto_ptr <NetworkInterfaceHelperClientData>
        d (new NetworkInterfaceHelperClientData());
    AssertReturn (d.get(), E_OUTOFMEMORY);

    d->msgCode = SVCHlpMsg::CreateHostNetworkInterface;
    d->name = aName;
    d->iface = iface;

    rc = mParent->startSVCHelperClient (
        IsUACEnabled() == TRUE /* aPrivileged */,
        networkInterfaceHelperClient,
        static_cast <void *> (d.get()),
        progress);

    if (SUCCEEDED (rc))
    {
        /* d is now owned by networkInterfaceHelperClient(), so release it */
        d.release();
    }

    return rc;
}

STDMETHODIMP
Host::RemoveHostNetworkInterface (INPTR GUIDPARAM aId,
                                  IHostNetworkInterface **aHostNetworkInterface,
                                  IProgress **aProgress)
{
    if (!aHostNetworkInterface)
        return E_POINTER;
    if (!aProgress)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    HRESULT rc = S_OK;

    /* first check whether an interface with the given name already exists */
    {
        ComPtr <IHostNetworkInterfaceCollection> coll;
        rc = COMGETTER(NetworkInterfaces) (coll.asOutParam());
        CheckComRCReturnRC (rc);
        ComPtr <IHostNetworkInterface> iface;
        if (FAILED (coll->FindById (aId, iface.asOutParam())))
            return setError (E_FAIL,
                tr ("Host network interface with UUID {%Vuuid} does not exist"),
                Guid (aId).raw());

        /* return the object to be removed to the caller */
        iface.queryInterfaceTo (aHostNetworkInterface);
    }

    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    rc = progress->init (mParent, static_cast <IHost *> (this),
                        Bstr (tr ("Removing host network interface")),
                        FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);
    progress.queryInterfaceTo (aProgress);

    /* create the networkInterfaceHelperClient() argument */
    std::auto_ptr <NetworkInterfaceHelperClientData>
        d (new NetworkInterfaceHelperClientData());
    AssertReturn (d.get(), E_OUTOFMEMORY);

    d->msgCode = SVCHlpMsg::RemoveHostNetworkInterface;
    d->guid = aId;

    rc = mParent->startSVCHelperClient (
        IsUACEnabled() == TRUE /* aPrivileged */,
        networkInterfaceHelperClient,
        static_cast <void *> (d.get()),
        progress);

    if (SUCCEEDED (rc))
    {
        /* d is now owned by networkInterfaceHelperClient(), so release it */
        d.release();
    }

    return rc;
}

#endif /* RT_OS_WINDOWS */

STDMETHODIMP Host::CreateUSBDeviceFilter (INPTR BSTR aName, IHostUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_POINTER;

    if (!aName || *aName == 0)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    ComObjPtr <HostUSBDeviceFilter> filter;
    filter.createObject();
    HRESULT rc = filter->init (this, aName);
    ComAssertComRCRet (rc, rc);
    rc = filter.queryInterfaceTo (aFilter);
    AssertComRCReturn (rc, rc);
    return S_OK;
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::InsertUSBDeviceFilter (ULONG aPosition, IHostUSBDeviceFilter *aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_INVALIDARG;

    AutoWriteLock alock (this);
    CHECK_READY();

    MultiResult rc = checkUSBProxyService();
    CheckComRCReturnRC (rc);

    ComObjPtr <HostUSBDeviceFilter> filter = getDependentChild (aFilter);
    if (!filter)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is not created within "
                "this VirtualBox instance"));

    if (filter->mInList)
        return setError (E_INVALIDARG,
            tr ("The given USB device filter is already in the list"));

    /* iterate to the position... */
    USBDeviceFilterList::iterator it = mUSBDeviceFilters.begin();
    std::advance (it, aPosition);
    /* ...and insert */
    mUSBDeviceFilters.insert (it, filter);
    filter->mInList = true;

    /* notify the proxy (only when the filter is active) */
    if (mUSBProxyService->isActive() && filter->data().mActive)
    {
        ComAssertRet (filter->id() == NULL, E_FAIL);
        filter->id() = mUSBProxyService->insertFilter (&filter->data().mUSBFilter);
    }

    /* save the global settings */
    alock.unlock();
    return rc = mParent->saveSettings();
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Host::RemoveUSBDeviceFilter (ULONG aPosition, IHostUSBDeviceFilter **aFilter)
{
#ifdef VBOX_WITH_USB
    if (!aFilter)
        return E_POINTER;

    AutoWriteLock alock (this);
    CHECK_READY();

    MultiResult rc = checkUSBProxyService();
    CheckComRCReturnRC (rc);

    if (!mUSBDeviceFilters.size())
        return setError (E_INVALIDARG,
            tr ("The USB device filter list is empty"));

    if (aPosition >= mUSBDeviceFilters.size())
        return setError (E_INVALIDARG,
            tr ("Invalid position: %lu (must be in range [0, %lu])"),
            aPosition, mUSBDeviceFilters.size() - 1);

    ComObjPtr <HostUSBDeviceFilter> filter;
    {
        /* iterate to the position... */
        USBDeviceFilterList::iterator it = mUSBDeviceFilters.begin();
        std::advance (it, aPosition);
        /* ...get an element from there... */
        filter = *it;
        /* ...and remove */
        filter->mInList = false;
        mUSBDeviceFilters.erase (it);
    }

    filter.queryInterfaceTo (aFilter);

    /* notify the proxy (only when the filter is active) */
    if (mUSBProxyService->isActive() && filter->data().mActive)
    {
        ComAssertRet (filter->id() != NULL, E_FAIL);
        mUSBProxyService->removeFilter (filter->id());
        filter->id() = NULL;
    }

    /* save the global settings */
    alock.unlock();
    return rc = mParent->saveSettings();
#else
    /* Note: The GUI depends on this method returning E_NOTIMPL with no
     * extended error info to indicate that USB is simply not available
     * (w/o treting it as a failure), for example, as in OSE */
    return E_NOTIMPL;
#endif
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

HRESULT Host::loadSettings (const settings::Key &aGlobal)
{
    using namespace settings;

    AutoWriteLock alock (this);
    CHECK_READY();

    AssertReturn (!aGlobal.isNull(), E_FAIL);

    HRESULT rc = S_OK;

#ifdef VBOX_WITH_USB
    Key::List filters = aGlobal.key ("USBDeviceFilters").keys ("DeviceFilter");
    for (Key::List::const_iterator it = filters.begin();
         it != filters.end(); ++ it)
    {
        Bstr name = (*it).stringValue ("name");
        bool active = (*it).value <bool> ("active");

        Bstr vendorId = (*it).stringValue ("vendorId");
        Bstr productId = (*it).stringValue ("productId");
        Bstr revision = (*it).stringValue ("revision");
        Bstr manufacturer = (*it).stringValue ("manufacturer");
        Bstr product = (*it).stringValue ("product");
        Bstr serialNumber = (*it).stringValue ("serialNumber");
        Bstr port = (*it).stringValue ("port");

        USBDeviceFilterAction_T action;
        action = USBDeviceFilterAction_Ignore;
        const char *actionStr = (*it).stringValue ("action");
        if (strcmp (actionStr, "Ignore") == 0)
            action = USBDeviceFilterAction_Ignore;
        else
        if (strcmp (actionStr, "Hold") == 0)
            action = USBDeviceFilterAction_Hold;
        else
            AssertMsgFailed (("Invalid action: '%s'\n", actionStr));

        ComObjPtr <HostUSBDeviceFilter> filterObj;
        filterObj.createObject();
        rc = filterObj->init (this,
                              name, active, vendorId, productId, revision,
                              manufacturer, product, serialNumber, port,
                              action);
        /* error info is set by init() when appropriate */
        CheckComRCBreakRC (rc);

        mUSBDeviceFilters.push_back (filterObj);
        filterObj->mInList = true;

        /* notify the proxy (only when the filter is active) */
        if (filterObj->data().mActive)
        {
            HostUSBDeviceFilter *flt = filterObj; /* resolve ambiguity */
            flt->id() = mUSBProxyService->insertFilter (&filterObj->data().mUSBFilter);
        }
    }
#endif /* VBOX_WITH_USB */

    return rc;
}

HRESULT Host::saveSettings (settings::Key &aGlobal)
{
    using namespace settings;

    AutoWriteLock alock (this);
    CHECK_READY();

    ComAssertRet (!aGlobal.isNull(), E_FAIL);

#ifdef VBOX_WITH_USB
    /* first, delete the entry */
    Key filters = aGlobal.findKey ("USBDeviceFilters");
    if (!filters.isNull())
        filters.zap();
    /* then, recreate it */
    filters = aGlobal.createKey ("USBDeviceFilters");

    USBDeviceFilterList::const_iterator it = mUSBDeviceFilters.begin();
    while (it != mUSBDeviceFilters.end())
    {
        AutoWriteLock filterLock (*it);
        const HostUSBDeviceFilter::Data &data = (*it)->data();

        Key filter = filters.appendKey ("DeviceFilter");

        filter.setValue <Bstr> ("name", data.mName);
        filter.setValue <bool> ("active", !!data.mActive);

        /* all are optional */
        Bstr str;
        (*it)->COMGETTER (VendorId) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("vendorId", str);

        (*it)->COMGETTER (ProductId) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("productId", str);

        (*it)->COMGETTER (Revision) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("revision", str);

        (*it)->COMGETTER (Manufacturer) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("manufacturer", str);

        (*it)->COMGETTER (Product) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("product", str);

        (*it)->COMGETTER (SerialNumber) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("serialNumber", str);

        (*it)->COMGETTER (Port) (str.asOutParam());
        if (!str.isNull())
            filter.setValue <Bstr> ("port", str);

        /* action is mandatory */
        USBDeviceFilterAction_T action = USBDeviceFilterAction_Null;
        (*it)->COMGETTER (Action) (&action);
        if (action == USBDeviceFilterAction_Ignore)
            filter.setStringValue ("action", "Ignore");
        else if (action == USBDeviceFilterAction_Hold)
            filter.setStringValue ("action", "Hold");
        else
            AssertMsgFailed (("Invalid action: %d\n", action));

        ++ it;
    }
#endif /* VBOX_WITH_USB */

    return S_OK;
}

#ifdef VBOX_WITH_USB

/**
 *  Called by setter methods of all USB device filters.
 */
HRESULT Host::onUSBDeviceFilterChange (HostUSBDeviceFilter *aFilter,
                                       BOOL aActiveChanged /* = FALSE */)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    if (aFilter->mInList)
    {
        if (aActiveChanged)
        {
            // insert/remove the filter from the proxy
            if (aFilter->data().mActive)
            {
                ComAssertRet (aFilter->id() == NULL, E_FAIL);
                aFilter->id() = mUSBProxyService->insertFilter (&aFilter->data().mUSBFilter);
            }
            else
            {
                ComAssertRet (aFilter->id() != NULL, E_FAIL);
                mUSBProxyService->removeFilter (aFilter->id());
                aFilter->id() = NULL;
            }
        }
        else
        {
            if (aFilter->data().mActive)
            {
                // update the filter in the proxy
                ComAssertRet (aFilter->id() != NULL, E_FAIL);
                mUSBProxyService->removeFilter (aFilter->id());
                aFilter->id() = mUSBProxyService->insertFilter (&aFilter->data().mUSBFilter);
            }
        }

        // save the global settings... yeah, on every single filter property change
        alock.unlock();
        return mParent->saveSettings();
    }

    return S_OK;
}


/**
 *  Requests the USB proxy service to capture the given host USB device.
 *
 *  When the request is completed,
 *  IInternalSessionControl::onUSBDeviceAttach() will be called on the given
 *  machine object.
 *
 *  Called by Console from the VM process (throug IInternalMachineControl).
 *  Must return extended error info in case of errors.
 */
HRESULT Host::captureUSBDevice (SessionMachine *aMachine, INPTR GUIDPARAM aId)
{
    ComAssertRet (aMachine, E_INVALIDARG);

    AutoWriteLock alock (this);
    CHECK_READY();

    Guid id (aId);

    ComObjPtr <HostUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (!device && it != mUSBDevices.end())
    {
        if ((*it)->id() == id)
            device = (*it);
        ++ it;
    }

    if (!device)
        return setError (E_INVALIDARG,
            tr ("USB device with UUID {%Vuuid} is not currently attached to the host"),
            id.raw());

/** @todo r=bird this doesn't belong here but in the HostUSBDevice bit. */
    AutoWriteLock devLock (device);

    if (device->isStatePending())
        return setError (E_INVALIDARG,
            tr ("USB device '%s' with UUID {%Vuuid} is busy (waiting for a pending "
                "state change). Please try later"),
            device->name().raw(), id.raw());

    if (device->state() == USBDeviceState_NotSupported)
        return setError (E_INVALIDARG,
            tr ("USB device '%s' with UUID {%Vuuid} cannot be accessed by guest "
                "computers"),
            device->name().raw(), id.raw());

    if (device->state() == USBDeviceState_Unavailable)
        return setError (E_INVALIDARG,
            tr ("USB device '%s' with UUID {%Vuuid} is being exclusively used by the "
                "host computer"),
            device->name().raw(), id.raw());

    if (device->state() == USBDeviceState_Captured)
    {
        /* Machine::name() requires a read lock */
        AutoReadLock machLock (device->machine());

        return setError (E_INVALIDARG,
            tr ("USB device '%s' with UUID {%Vuuid} is already captured by the virtual "
                "machine '%ls'"),
            device->name().raw(), id.raw(),
            device->machine()->name().raw());
    }

    /* try to capture the device */
    device->requestCaptureForVM (aMachine);

    return S_OK;
}

/**
 *  Notification from the VM process that it is going to detach (\a aDone = false)
 *  or that is has just detach (\a aDone = true) the given USB device.
 *
 *  When \a aDone = false we only inform the USB Proxy about what the vm is
 *  up to so it doesn't get confused and create a new USB host device object
 *  (a Darwin issue).
 *
 *  When \a aDone = true we replay all filters against the given USB device
 *  excluding filters of the machine the device is currently marked as
 *  captured by.
 *
 *  When the \a aDone = true request is completed,
 *  IInternalSessionControl::onUSBDeviceDetach() will be called on the given
 *  machine object.
 *
 *  Called by Console from the VM process (throug IInternalMachineControl).
 *
 */
HRESULT Host::detachUSBDevice (SessionMachine *aMachine, INPTR GUIDPARAM aId, BOOL aDone)
{
    LogFlowThisFunc (("aMachine=%p, aId={%Vuuid}\n", aMachine, Guid (aId).raw()));

    AutoWriteLock alock (this);
    CHECK_READY();

    ComObjPtr <HostUSBDevice> device;
    USBDeviceList::iterator it = mUSBDevices.begin();
    while (!device && it != mUSBDevices.end())
    {
        if ((*it)->id() == aId)
            device = (*it);
        ++ it;
    }

    ComAssertRet (!!device, E_FAIL);

    AutoWriteLock devLock (device);

    LogFlowThisFunc (("id={%Vuuid} state=%d isStatePending=%RTbool pendingState=%d aDone=%RTbool\n",
                      device->id().raw(), device->state(), device->isStatePending(),
                      device->pendingState(), aDone));
    HRESULT rc = S_OK;
    if (!aDone)
    {
        if (device->isStatePending())
            rc = setError (E_INVALIDARG,
                tr ("USB device '%s' with UUID {%Vuuid} is busy (waiting for a pending "
                    "state change). Please try later"),
                             device->name().raw(), device->id().raw());
        else
            mUSBProxyService->detachingDevice (device);
    }
    else
    {
        if (device->isStatePending())
        {
            /* If an async detach operation is still pending (darwin), postpone
               the setHeld() + the re-applying of filters until it is completed.
               We indicate this by moving to the '*Filters' state variant. */
            if (device->pendingStateEx() == HostUSBDevice::kDetachingPendingAttach)
                device->setLogicalReconnect (HostUSBDevice::kDetachingPendingAttachFilters);
            else if (device->pendingStateEx() == HostUSBDevice::kDetachingPendingDetach)
                device->setLogicalReconnect (HostUSBDevice::kDetachingPendingDetachFilters);
            else
            {
                Assert (device->pendingStateEx() == HostUSBDevice::kNothingPending);
                rc = setError (E_INVALIDARG,
                    tr ("USB device '%s' with UUID {%Vuuid} is busy (waiting for a pending "
                        "state change). Please try later"),
                                 device->name().raw(), device->id().raw());
            }
        }
        else
        {
            ComAssertRet (device->machine() == aMachine, E_FAIL);

            /* re-apply filters on the device before giving it back to the host */
            device->setHeld();
            rc = applyAllUSBFilters (device, aMachine);
            ComAssertComRC (rc);
        }
    }

    return rc;
}

/**
 *  Asks the USB proxy service to capture all currently available USB devices
 *  that match filters of the given machine.
 *
 *  When the request is completed,
 *  IInternalSessionControl::onUSBDeviceDetach() will be called on the given
 *  machine object per every captured USB device.
 *
 *  Called by Console from the VM process (through IInternalMachineControl)
 *  upon VM startup.
 *
 *  @note Locks this object for reading (@todo for writing now, until switched
 *  to the new locking scheme).
 */
HRESULT Host::autoCaptureUSBDevices (SessionMachine *aMachine)
{
    LogFlowThisFunc (("aMachine=%p\n", aMachine));

    AutoWriteLock alock (this);
    CHECK_READY();

    for (USBDeviceList::iterator it = mUSBDevices.begin();
         it != mUSBDevices.end();
         ++ it)
    {
        ComObjPtr <HostUSBDevice> device = *it;

        AutoWriteLock devLock (device);

        /* skip pending devices */
        if (device->isStatePending())
            continue;

        if (device->state() == USBDeviceState_Busy ||
            device->state() == USBDeviceState_Available ||
            device->state() == USBDeviceState_Held)
        {
            applyMachineUSBFilters (aMachine, device);
        }
    }

    return S_OK;
}

/**
 *  Replays all filters against all USB devices currently marked as captured
 *  by the given machine (excluding this machine's filters).
 *
 *  Called by Console from the VM process (throug IInternalMachineControl)
 *  upon normal VM termination or by SessionMachine::uninit() upon abnormal
 *  VM termination (from under the Machine/SessionMachine lock).
 *
 *  @note Locks this object for reading (@todo for writing now, until switched
 *  to the new locking scheme).
 */
HRESULT Host::detachAllUSBDevices (SessionMachine *aMachine, BOOL aDone)
{
    AutoWriteLock alock (this);
    CHECK_READY();

    USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        ComObjPtr <HostUSBDevice> device = *it;

        AutoWriteLock devLock (device);

        if (device->machine() == aMachine)
        {
            if (!aDone)
            {
                if (!device->isStatePending())
                    mUSBProxyService->detachingDevice (device);
            }
            else
            {
                if (!device->isStatePending())
                {
                    Assert (device->state() == USBDeviceState_Captured);

                    /* re-apply filters on the device before giving it back to the
                     * host */
                    device->setHeld();
                    HRESULT rc = applyAllUSBFilters (device, aMachine);
                    AssertComRC (rc);
                }
                else if (device->pendingStateEx() == HostUSBDevice::kNothingPending)
                    device->cancelPendingState();
            }
        }
        ++ it;
    }

    return S_OK;
}

#endif /* VBOX_HOST_USB */

// private methods
////////////////////////////////////////////////////////////////////////////////

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
# ifdef VBOX_USE_LIBHAL
/**
 * Helper function to query the hal subsystem for information about DVD drives attached to the
 * system.
 *
 * @returns true if information was successfully obtained, false otherwise
 * @retval  list drives found will be attached to this list
 */
bool Host::getDVDInfoFromHal(std::list <ComObjPtr <HostDVDDrive> > &list)
{
    bool halSuccess = false;
    DBusError dbusError;
    if (!gLibHalCheckPresence())
        return false;
    gDBusErrorInit (&dbusError);
    DBusConnection *dbusConnection = gDBusBusGet(DBUS_BUS_SYSTEM, &dbusError);
    if (dbusConnection != 0)
    {
        LibHalContext *halContext = gLibHalCtxNew();
        if (halContext != 0)
        {
            if (gLibHalCtxSetDBusConnection (halContext, dbusConnection))
            {
                if (gLibHalCtxInit(halContext, &dbusError))
                {
                    int numDevices;
                    char **halDevices = gLibHalFindDeviceStringMatch(halContext,
                                                "storage.drive_type", "cdrom",
                                                &numDevices, &dbusError);
                    if (halDevices != 0)
                    {
                        /* Hal is installed and working, so if no devices are reported, assume
                           that there are none. */
                        halSuccess = true;
                        for (int i = 0; i < numDevices; i++)
                        {
                            char *devNode = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "block.device", &dbusError);
#ifdef RT_OS_SOLARIS
                            /* The CD/DVD ioctls work only for raw device nodes. */
                            char *tmp = getfullrawname(devNode);
                            gLibHalFreeString(devNode);
                            devNode = tmp;
#endif
                            if (devNode != 0)
                            {
//                                if (validateDevice(devNode, true))
//                                {
                                    Utf8Str description;
                                    char *vendor, *product;
                                    /* We do not check the error here, as this field may
                                       not even exist. */
                                    vendor = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.vendor", 0);
                                    product = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.product", &dbusError);
                                    if ((product != 0 && product[0] != 0))
                                    {
                                        if ((vendor != 0) && (vendor[0] != 0))
                                        {
                                            description = Utf8StrFmt ("%s %s",
                                                                      vendor, product);
                                        }
                                        else
                                        {
                                            description = product;
                                        }
                                        ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                                        hostDVDDriveObj.createObject();
                                        hostDVDDriveObj->init (Bstr (devNode),
                                                               Bstr (halDevices[i]),
                                                               Bstr (description));
                                        list.push_back (hostDVDDriveObj);
                                    }
                                    else
                                    {
                                        if (product == 0)
                                        {
                                            LogRel(("Host::COMGETTER(DVDDrives): failed to get property \"info.product\" for device %s.  dbus error: %s (%s)\n",
                                                    halDevices[i], dbusError.name, dbusError.message));
                                            gDBusErrorFree(&dbusError);
                                        }
                                        ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                                        hostDVDDriveObj.createObject();
                                        hostDVDDriveObj->init (Bstr (devNode),
                                                               Bstr (halDevices[i]));
                                        list.push_back (hostDVDDriveObj);
                                    }
                                    if (vendor != 0)
                                    {
                                        gLibHalFreeString(vendor);
                                    }
                                    if (product != 0)
                                    {
                                        gLibHalFreeString(product);
                                    }
//                                }
//                                else
//                                {
//                                    LogRel(("Host::COMGETTER(DVDDrives): failed to validate the block device %s as a DVD drive\n"));
//                                }
#ifndef RT_OS_SOLARIS
                                gLibHalFreeString(devNode);
#else
                                free(devNode);
#endif
                            }
                            else
                            {
                                LogRel(("Host::COMGETTER(DVDDrives): failed to get property \"block.device\" for device %s.  dbus error: %s (%s)\n",
                                        halDevices[i], dbusError.name, dbusError.message));
                                gDBusErrorFree(&dbusError);
                            }
                        }
                        gLibHalFreeStringArray(halDevices);
                    }
                    else
                    {
                        LogRel(("Host::COMGETTER(DVDDrives): failed to get devices with capability \"storage.cdrom\".  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                    if (!gLibHalCtxShutdown(halContext, &dbusError))  /* what now? */
                    {
                        LogRel(("Host::COMGETTER(DVDDrives): failed to shutdown the libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                }
                else
                {
                    LogRel(("Host::COMGETTER(DVDDrives): failed to initialise libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                    gDBusErrorFree(&dbusError);
                }
                gLibHalCtxFree(halContext);
            }
            else
            {
                LogRel(("Host::COMGETTER(DVDDrives): failed to set libhal connection to dbus.\n"));
            }
        }
        else
        {
            LogRel(("Host::COMGETTER(DVDDrives): failed to get a libhal context - out of memory?\n"));
        }
        gDBusConnectionUnref(dbusConnection);
    }
    else
    {
        LogRel(("Host::COMGETTER(DVDDrives): failed to connect to dbus.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
        gDBusErrorFree(&dbusError);
    }
    return halSuccess;
}


/**
 * Helper function to query the hal subsystem for information about floppy drives attached to the
 * system.
 *
 * @returns true if information was successfully obtained, false otherwise
 * @retval  list drives found will be attached to this list
 */
bool Host::getFloppyInfoFromHal(std::list <ComObjPtr <HostFloppyDrive> > &list)
{
    bool halSuccess = false;
    DBusError dbusError;
    if (!gLibHalCheckPresence())
        return false;
    gDBusErrorInit (&dbusError);
    DBusConnection *dbusConnection = gDBusBusGet(DBUS_BUS_SYSTEM, &dbusError);
    if (dbusConnection != 0)
    {
        LibHalContext *halContext = gLibHalCtxNew();
        if (halContext != 0)
        {
            if (gLibHalCtxSetDBusConnection (halContext, dbusConnection))
            {
                if (gLibHalCtxInit(halContext, &dbusError))
                {
                    int numDevices;
                    char **halDevices = gLibHalFindDeviceStringMatch(halContext,
                                                "storage.drive_type", "floppy",
                                                &numDevices, &dbusError);
                    if (halDevices != 0)
                    {
                        /* Hal is installed and working, so if no devices are reported, assume
                           that there are none. */
                        halSuccess = true;
                        for (int i = 0; i < numDevices; i++)
                        {
                            char *driveType = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "storage.drive_type", 0);
                            if (driveType != 0)
                            {
                                if (strcmp(driveType, "floppy") != 0)
                                {
                                    gLibHalFreeString(driveType);
                                    continue;
                                }
                                gLibHalFreeString(driveType);
                            }
                            else
                            {
                                /* An error occurred.  The attribute "storage.drive_type"
                                   probably didn't exist. */
                                continue;
                            }
                            char *devNode = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "block.device", &dbusError);
                            if (devNode != 0)
                            {
//                                if (validateDevice(devNode, false))
//                                {
                                    Utf8Str description;
                                    char *vendor, *product;
                                    /* We do not check the error here, as this field may
                                       not even exist. */
                                    vendor = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.vendor", 0);
                                    product = gLibHalDeviceGetPropertyString(halContext,
                                                    halDevices[i], "info.product", &dbusError);
                                    if ((product != 0) && (product[0] != 0))
                                    {
                                        if ((vendor != 0) && (vendor[0] != 0))
                                        {
                                            description = Utf8StrFmt ("%s %s",
                                                                      vendor, product);
                                        }
                                        else
                                        {
                                            description = product;
                                        }
                                        ComObjPtr <HostFloppyDrive> hostFloppyDrive;
                                        hostFloppyDrive.createObject();
                                        hostFloppyDrive->init (Bstr (devNode),
                                                               Bstr (halDevices[i]),
                                                               Bstr (description));
                                        list.push_back (hostFloppyDrive);
                                    }
                                    else
                                    {
                                        if (product == 0)
                                        {
                                            LogRel(("Host::COMGETTER(FloppyDrives): failed to get property \"info.product\" for device %s.  dbus error: %s (%s)\n",
                                                    halDevices[i], dbusError.name, dbusError.message));
                                            gDBusErrorFree(&dbusError);
                                        }
                                        ComObjPtr <HostFloppyDrive> hostFloppyDrive;
                                        hostFloppyDrive.createObject();
                                        hostFloppyDrive->init (Bstr (devNode),
                                                               Bstr (halDevices[i]));
                                        list.push_back (hostFloppyDrive);
                                    }
                                    if (vendor != 0)
                                    {
                                        gLibHalFreeString(vendor);
                                    }
                                    if (product != 0)
                                    {
                                        gLibHalFreeString(product);
                                    }
//                                }
//                                else
//                                {
//                                    LogRel(("Host::COMGETTER(FloppyDrives): failed to validate the block device %s as a floppy drive\n"));
//                                }
                                gLibHalFreeString(devNode);
                            }
                            else
                            {
                                LogRel(("Host::COMGETTER(FloppyDrives): failed to get property \"block.device\" for device %s.  dbus error: %s (%s)\n",
                                        halDevices[i], dbusError.name, dbusError.message));
                                gDBusErrorFree(&dbusError);
                            }
                        }
                        gLibHalFreeStringArray(halDevices);
                    }
                    else
                    {
                        LogRel(("Host::COMGETTER(FloppyDrives): failed to get devices with capability \"storage.cdrom\".  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                    if (!gLibHalCtxShutdown(halContext, &dbusError))  /* what now? */
                    {
                        LogRel(("Host::COMGETTER(FloppyDrives): failed to shutdown the libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                        gDBusErrorFree(&dbusError);
                    }
                }
                else
                {
                    LogRel(("Host::COMGETTER(FloppyDrives): failed to initialise libhal context.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
                    gDBusErrorFree(&dbusError);
                }
                gLibHalCtxFree(halContext);
            }
            else
            {
                LogRel(("Host::COMGETTER(FloppyDrives): failed to set libhal connection to dbus.\n"));
            }
        }
        else
        {
            LogRel(("Host::COMGETTER(FloppyDrives): failed to get a libhal context - out of memory?\n"));
        }
        gDBusConnectionUnref(dbusConnection);
    }
    else
    {
        LogRel(("Host::COMGETTER(FloppyDrives): failed to connect to dbus.  dbus error: %s (%s)\n", dbusError.name, dbusError.message));
        gDBusErrorFree(&dbusError);
    }
    return halSuccess;
}
# endif  /* VBOX_USE_HAL defined */

/**
 * Helper function to parse the given mount file and add found entries
 */
void Host::parseMountTable(char *mountTable, std::list <ComObjPtr <HostDVDDrive> > &list)
{
#ifdef RT_OS_LINUX
    FILE *mtab = setmntent(mountTable, "r");
    if (mtab)
    {
        struct mntent *mntent;
        char *mnt_type;
        char *mnt_dev;
        char *tmp;
        while ((mntent = getmntent(mtab)))
        {
            mnt_type = (char*)malloc(strlen(mntent->mnt_type) + 1);
            mnt_dev = (char*)malloc(strlen(mntent->mnt_fsname) + 1);
            strcpy(mnt_type, mntent->mnt_type);
            strcpy(mnt_dev, mntent->mnt_fsname);
            // supermount fs case
            if (strcmp(mnt_type, "supermount") == 0)
            {
                tmp = strstr(mntent->mnt_opts, "fs=");
                if (tmp)
                {
                    free(mnt_type);
                    mnt_type = strdup(tmp + strlen("fs="));
                    if (mnt_type)
                    {
                        tmp = strchr(mnt_type, ',');
                        if (tmp)
                            *tmp = '\0';
                    }
                }
                tmp = strstr(mntent->mnt_opts, "dev=");
                if (tmp)
                {
                    free(mnt_dev);
                    mnt_dev = strdup(tmp + strlen("dev="));
                    if (mnt_dev)
                    {
                        tmp = strchr(mnt_dev, ',');
                        if (tmp)
                            *tmp = '\0';
                    }
                }
            }
            // use strstr here to cover things fs types like "udf,iso9660"
            if (strstr(mnt_type, "iso9660") == 0)
            {
                /** @todo check whether we've already got the drive in our list! */
                if (validateDevice(mnt_dev, true))
                {
                    ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init (Bstr (mnt_dev));
                    list.push_back (hostDVDDriveObj);
                }
            }
            free(mnt_dev);
            free(mnt_type);
        }
        endmntent(mtab);
    }
#else  // RT_OS_SOLARIS
    FILE *mntFile = fopen(mountTable, "r");
    if (mntFile)
    {
        struct mnttab mntTab;
        while (getmntent(mntFile, &mntTab) == 0)
        {
            char *mountName = strdup(mntTab.mnt_special);
            char *mountPoint = strdup(mntTab.mnt_mountp);
            char *mountFSType = strdup(mntTab.mnt_fstype);

            // skip devices we are not interested in
            if ((*mountName && mountName[0] == '/') &&                  // skip 'fake' devices (like -hosts, proc, fd, swap)
                (*mountFSType && (strcmp(mountFSType, "devfs") != 0 &&  // skip devfs (i.e. /devices)
                                  strcmp(mountFSType, "dev") != 0 &&    // skip dev (i.e. /dev)
                                  strcmp(mountFSType, "lofs") != 0)) && // skip loop-back file-system (lofs)
                (*mountPoint && strcmp(mountPoint, "/") != 0))          // skip point '/' (Can CD/DVD be mounted at '/' ???)
            {
                char *rawDevName = getfullrawname(mountName);
                if (validateDevice(rawDevName, true))
                {
                    ComObjPtr <HostDVDDrive> hostDVDDriveObj;
                    hostDVDDriveObj.createObject();
                    hostDVDDriveObj->init (Bstr (rawDevName));
                    list.push_back (hostDVDDriveObj);
                }
                free(rawDevName);
            }

            free(mountName);
            free(mountPoint);
            free(mountFSType);
        }

        fclose(mntFile);
    }
#endif
}

/**
 * Helper function to check whether the given device node is a valid drive
 */
bool Host::validateDevice(const char *deviceNode, bool isCDROM)
{
    struct stat statInfo;
    bool retValue = false;

    // sanity check
    if (!deviceNode)
    {
        return false;
    }

    // first a simple stat() call
    if (stat(deviceNode, &statInfo) < 0)
    {
        return false;
    } else
    {
        if (isCDROM)
        {
            if (S_ISCHR(statInfo.st_mode) || S_ISBLK(statInfo.st_mode))
            {
                int fileHandle;
                // now try to open the device
                fileHandle = open(deviceNode, O_RDONLY | O_NONBLOCK, 0);
                if (fileHandle >= 0)
                {
                    cdrom_subchnl cdChannelInfo;
                    cdChannelInfo.cdsc_format = CDROM_MSF;
                    // this call will finally reveal the whole truth
#ifdef RT_OS_LINUX
                    if ((ioctl(fileHandle, CDROMSUBCHNL, &cdChannelInfo) == 0) ||
                        (errno == EIO) || (errno == ENOENT) ||
                        (errno == EINVAL) || (errno == ENOMEDIUM))
#else
                    if ((ioctl(fileHandle, CDROMSUBCHNL, &cdChannelInfo) == 0) ||
                        (errno == EIO) || (errno == ENOENT) ||
                        (errno == EINVAL))
#endif
                    {
                        retValue = true;
                    }
                    close(fileHandle);
                }
            }
        } else
        {
            // floppy case
            if (S_ISCHR(statInfo.st_mode) || S_ISBLK(statInfo.st_mode))
            {
                /// @todo do some more testing, maybe a nice IOCTL!
                retValue = true;
            }
        }
    }
    return retValue;
}
#endif // RT_OS_LINUX || RT_OS_SOLARIS

#ifdef VBOX_WITH_USB

/**
 *  Applies all (golbal and VM) filters to the given USB device. The device
 *  must be either a newly attached device or a device released by a VM.
 *
 *  This method will request the USB proxy service to release the device (give
 *  it back to the host) if none of the global or VM filters want to capture
 *  the device.
 *
 *  @param aDevice  USB device to apply filters to.
 *  @param aMachine Machine the device was released by or @c NULL.
 *
 *  @note the method must be called from under this object's write lock and
 *  from the aDevice's write lock.
 */
HRESULT Host::applyAllUSBFilters (ComObjPtr <HostUSBDevice> &aDevice,
                                  SessionMachine *aMachine /* = NULL */)
{
    LogFlowThisFunc (("\n"));

    /// @todo must check for read lock, it's enough here
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    AssertReturn (aDevice->isWriteLockOnCurrentThread(), E_FAIL);

    AssertReturn (aDevice->state() != USBDeviceState_Captured, E_FAIL);

    AssertReturn (aDevice->isStatePending() == false, E_FAIL);

    /* ignore unsupported devices */
    if (aDevice->state() == USBDeviceState_NotSupported)
        return S_OK;
    /* ignore unavailable devices as well */
    if (aDevice->state() == USBDeviceState_Unavailable)
        return S_OK;

    VirtualBox::SessionMachineVector machines;
    mParent->getOpenedMachines (machines);

    /// @todo it may be better to take a copy of filters to iterate and leave
    /// the host lock before calling HostUSBDevice:requestCaptureForVM() (which
    /// may call the VM process).
    /// Moving this matching into USBProxyService at the same time would be
    /// nice too, that'll simplify the locking a bit more.

    /* apply global filters */
    USBDeviceFilterList::const_iterator it = mUSBDeviceFilters.begin();
    for (; it != mUSBDeviceFilters.end(); ++ it)
    {
        AutoWriteLock filterLock (*it);
        const HostUSBDeviceFilter::Data &data = (*it)->data();
        if (aDevice->isMatch (data))
        {
            USBDeviceFilterAction_T action = USBDeviceFilterAction_Null;
            (*it)->COMGETTER (Action) (&action);
            if (action == USBDeviceFilterAction_Ignore)
            {
                /* request to give the device back to the host */
                aDevice->requestReleaseToHost();
                /* nothing to do any more */
                return S_OK;
            }
            if (action == USBDeviceFilterAction_Hold)
                break;
        }
    }

    /* apply machine filters */
    size_t i = 0;
    for (; i < machines.size(); ++ i)
    {
        /* skip the machine the device was just detached from */
        if (aMachine && machines [i] == aMachine)
            continue;

        if (applyMachineUSBFilters (machines [i], aDevice))
            break;
    }

    if (i == machines.size())
    {
        /* no matched machine filters, check what to do */
        if (it == mUSBDeviceFilters.end())
        {
            /* no any filter matched at all */
            /* request to give the device back to the host */
            aDevice->requestReleaseToHost();
        }
        else
        {
            /* there was a global Hold filter */
            aDevice->requestHold();
        }
    }

    return S_OK;
}

/**
 *  Runs through filters of the given machine and asks the USB proxy service
 *  to capture the given USB device when there is a match.
 *
 *  @param aMachine Machine whose filters are to be run.
 *  @param aDevice  USB device, a candidate for auto-capturing.
 *  @return         @c true if there was a match and @c false otherwise.
 *
 *  @note the method must be called from under this object's write lock and
 *  from the aDevice's write lock.
 *
 *  @note Locks aMachine for reading.
 */
bool Host::applyMachineUSBFilters (SessionMachine *aMachine,
                                   ComObjPtr <HostUSBDevice> &aDevice)
{
    LogFlowThisFunc (("\n"));

    AssertReturn (aMachine, false);

    /// @todo must check for read lock, it's enough here
    AssertReturn (isWriteLockOnCurrentThread(), false);

    AssertReturn (aDevice->isWriteLockOnCurrentThread(), false);

    AssertReturn (aDevice->state() != USBDeviceState_NotSupported, false);
    AssertReturn (aDevice->state() != USBDeviceState_Unavailable, false);

    AssertReturn (aDevice->isStatePending() == false, false);

    ULONG maskedIfs;
    bool hasMatch = aMachine->hasMatchingUSBFilter (aDevice, &maskedIfs);

    if (hasMatch)
    {
        /** @todo r=bird: this is wrong as requestAttachToVM may return false for different reasons that we. */
        /* try to capture the device */
        return aDevice->requestCaptureForVM (aMachine, maskedIfs);
    }

    return hasMatch;
}

/**
 *  Called by USB proxy service when a new device is physically attached
 *  to the host.
 *
 *  @param      aDevice     Pointer to the device which has been attached.
 */
void Host::onUSBDeviceAttached (HostUSBDevice *aDevice)
{
    LogFlowThisFunc (("aDevice=%p\n", aDevice));

    AssertReturnVoid (aDevice);

    AssertReturnVoid (isWriteLockOnCurrentThread());
    AssertReturnVoid (aDevice->isWriteLockOnCurrentThread());

    LogFlowThisFunc (("id={%Vuuid} state=%d isStatePending=%RTbool pendingState=%d\n",
                      aDevice->id().raw(), aDevice->state(), aDevice->isStatePending(),
                      aDevice->pendingState()));

    Assert (aDevice->isStatePending() == false);

    /* add to the collecion */
    mUSBDevices.push_back (aDevice);

    /* apply all filters */
    ComObjPtr <HostUSBDevice> device (aDevice);
    HRESULT rc = applyAllUSBFilters (device);
    AssertComRC (rc);
}

/**
 *  Called by USB proxy service when the device is physically detached
 *  from the host.
 *
 *  @param      aDevice     Pointer to the device which has been detached.
 */
void Host::onUSBDeviceDetached (HostUSBDevice *aDevice)
{
    LogFlowThisFunc (("aDevice=%p\n", aDevice));

    AssertReturnVoid (aDevice);

    AssertReturnVoid (isWriteLockOnCurrentThread());
    AssertReturnVoid (aDevice->isWriteLockOnCurrentThread());

    LogFlowThisFunc (("id={%Vuuid} state=%d isStatePending=%RTbool pendingState=%d\n",
                      aDevice->id().raw(), aDevice->state(), aDevice->isStatePending(),
                      aDevice->pendingState()));

    Guid id = aDevice->id();

    ComObjPtr <HostUSBDevice> device;
    Host::USBDeviceList::iterator it = mUSBDevices.begin();
    while (it != mUSBDevices.end())
    {
        if ((*it)->id() == id)
        {
            device = (*it);
            break;
        }
        ++ it;
    }

    AssertReturnVoid (!!device);

    /* remove from the collecion */
    mUSBDevices.erase (it);

    /* Detach the device from any machine currently using it,
       reset all data and uninitialize the device object. */
    device->onPhysicalDetached();
}

/**
 *  Called by USB proxy service when the state of the device has changed
 *  either because of the state change request or because of some external
 *  interaction.
 *
 *  @param  aDevice     The device in question.
 */
void Host::onUSBDeviceStateChanged (HostUSBDevice *aDevice)
{
    LogFlowThisFunc (("aDevice=%p\n", aDevice));

    AssertReturnVoid (aDevice);

    AssertReturnVoid (isWriteLockOnCurrentThread());
    AssertReturnVoid (aDevice->isWriteLockOnCurrentThread());

    LogFlowThisFunc (("id={%Vuuid} state=%d isStatePending=%RTbool pendingState=%d\n",
                      aDevice->id().raw(), aDevice->state(), aDevice->isStatePending(),
                      aDevice->pendingState()));


    ComObjPtr <HostUSBDevice> device (aDevice);
    if (device->isStatePending())
    {
        /* it was a state change request */
        if (device->pendingStateEx() == HostUSBDevice::kDetachingPendingAttachFilters)
        {
            /* The device has completed an asynchronous detach operation, subject
               it to the filters and such if the current state permits this.
               (handlePendingStateChange will disassociate itself from the machine.) */
            ComObjPtr <SessionMachine> machine (device->machine());
            device->handlePendingStateChange();
            if (device->state() == USBDeviceState_Captured)
            {
                Log (("USB: running filters on async detached device\n"));
                device->setHeld();
                HRESULT rc = applyAllUSBFilters (device, machine);
                AssertComRC (rc);
            }
            else
                Log (("USB: async detached devices reappeared in stated %d instead of %d!\n",
                      device->state(), USBDeviceState_Captured));
        }
        else
            device->handlePendingStateChange();
    }
    else if (   device->state() == USBDeviceState_Available
             || device->state() == USBDeviceState_Busy)
    {
        /* The device has gone from being unavailable (not subject to filters) to being
           available / busy. This transition can be triggered by udevd or manual
           permission changes on Linux. On all systems may be triggered by the host
           ceasing to use the device - like unmounting an MSD in the Finder or invoking
           the "Safely remove XXXX" stuff on Windows (perhaps). */
        HRESULT rc = applyAllUSBFilters (device);
        AssertComRC (rc);
    }
    else
    {
        /* some external state change */

        /// @todo re-run all USB filters probably
        AssertFailed();
    }
}
#endif /* VBOX_WITH_USB */

/**
 *  Checks for the presense and status of the USB Proxy Service.
 *  Returns S_OK when the Proxy is present and OK, or E_FAIL and a
 *  corresponding error message otherwise. Intended to be used by methods
 *  that rely on the Proxy Service availability.
 *
 *  @note This method may return a warning result code. It is recommended to use
 *        MultiError to store the return value.
 *
 *  @note Locks this object for reading.
 */
HRESULT Host::checkUSBProxyService()
{
#ifdef VBOX_WITH_USB
    AutoWriteLock alock (this);
    CHECK_READY();

    AssertReturn (mUSBProxyService, E_FAIL);
    if (!mUSBProxyService->isActive())
    {
        /* disable the USB controller completely to avoid assertions if the
         * USB proxy service could not start. */

        if (mUSBProxyService->getLastError() == VERR_FILE_NOT_FOUND)
            return setWarning (E_FAIL,
                tr ("Could not load the Host USB Proxy Service (%Vrc). "
                    "The service might be not installed on the host computer"),
                mUSBProxyService->getLastError());
        if (mUSBProxyService->getLastError() == VINF_SUCCESS)
            return setWarning (E_FAIL,
                tr ("The USB Proxy Service has not yet been ported to this host"));
        return setWarning (E_FAIL,
            tr ("Could not load the Host USB Proxy service (%Vrc)"),
            mUSBProxyService->getLastError());
    }

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

#ifdef RT_OS_WINDOWS

/* The original source of the VBoxTAP adapter creation/destruction code has the following copyright */
/*
   Copyright 2004 by the Massachusetts Institute of Technology

   All rights reserved.

   Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee is hereby granted,
   provided that the above copyright notice appear in all copies and that
   both that copyright notice and this permission notice appear in
   supporting documentation, and that the name of the Massachusetts
   Institute of Technology (M.I.T.) not be used in advertising or publicity
   pertaining to distribution of the software without specific, written
   prior permission.

   M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
   ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
   M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
   SOFTWARE.
*/


#define NETSHELL_LIBRARY _T("netshell.dll")

/**
 *  Use the IShellFolder API to rename the connection.
 */
static HRESULT rename_shellfolder (PCWSTR wGuid, PCWSTR wNewName)
{
    /* This is the GUID for the network connections folder. It is constant.
     * {7007ACC7-3202-11D1-AAD2-00805FC1270E} */
    const GUID CLSID_NetworkConnections = {
        0x7007ACC7, 0x3202, 0x11D1, {
            0xAA, 0xD2, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E
        }
    };

    LPITEMIDLIST pidl = NULL;
    IShellFolder *pShellFolder = NULL;
    HRESULT hr;

    /* Build the display name in the form "::{GUID}". */
    if (wcslen (wGuid) >= MAX_PATH)
        return E_INVALIDARG;
    WCHAR szAdapterGuid[MAX_PATH + 2] = {0};
    swprintf (szAdapterGuid, L"::%ls", wGuid);

    /* Create an instance of the network connections folder. */
    hr = CoCreateInstance (CLSID_NetworkConnections, NULL,
                           CLSCTX_INPROC_SERVER, IID_IShellFolder,
                           reinterpret_cast <LPVOID *> (&pShellFolder));
    /* Parse the display name. */
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->ParseDisplayName (NULL, NULL, szAdapterGuid, NULL,
                                             &pidl, NULL);
    }
    if (SUCCEEDED (hr))
    {
        hr = pShellFolder->SetNameOf (NULL, pidl, wNewName, SHGDN_NORMAL,
                                      &pidl);
    }

    CoTaskMemFree (pidl);

    if (pShellFolder)
        pShellFolder->Release();

    return hr;
}

extern "C" HRESULT RenameConnection (PCWSTR GuidString, PCWSTR NewName)
{
    typedef HRESULT (WINAPI *lpHrRenameConnection) (const GUID *, PCWSTR);
    lpHrRenameConnection RenameConnectionFunc = NULL;
    HRESULT status;

    /* First try the IShellFolder interface, which was unimplemented
     * for the network connections folder before XP. */
    status = rename_shellfolder (GuidString, NewName);
    if (status == E_NOTIMPL)
    {
/** @todo that code doesn't seem to work! */
        /* The IShellFolder interface is not implemented on this platform.
         * Try the (undocumented) HrRenameConnection API in the netshell
         * library. */
        CLSID clsid;
        HINSTANCE hNetShell;
        status = CLSIDFromString ((LPOLESTR) GuidString, &clsid);
        if (FAILED(status))
            return E_FAIL;
        hNetShell = LoadLibrary (NETSHELL_LIBRARY);
        if (hNetShell == NULL)
            return E_FAIL;
        RenameConnectionFunc =
          (lpHrRenameConnection) GetProcAddress (hNetShell,
                                                 "HrRenameConnection");
        if (RenameConnectionFunc == NULL)
        {
            FreeLibrary (hNetShell);
            return E_FAIL;
        }
        status = RenameConnectionFunc (&clsid, NewName);
        FreeLibrary (hNetShell);
    }
    if (FAILED (status))
        return status;

    return S_OK;
}

#define DRIVERHWID _T("vboxtap")

#define SetErrBreak(strAndArgs) \
    if (1) { \
        aErrMsg = Utf8StrFmt strAndArgs; vrc = VERR_GENERAL_FAILURE; break; \
    } else do {} while (0)

/* static */
int Host::createNetworkInterface (SVCHlpClient *aClient,
                                  const Utf8Str &aName,
                                  Guid &aGUID, Utf8Str &aErrMsg)
{
    LogFlowFuncEnter();
    LogFlowFunc (("Network connection name = '%s'\n", aName.raw()));

    AssertReturn (aClient, VERR_INVALID_POINTER);
    AssertReturn (!aName.isNull(), VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD ret = 0;
    BOOL found = FALSE;
    BOOL registered = FALSE;
    BOOL destroyList = FALSE;
    TCHAR pCfgGuidString [50];

    do
    {
        BOOL ok;
        GUID netGuid;
        SP_DRVINFO_DATA DriverInfoData;
        SP_DEVINSTALL_PARAMS  DeviceInstallParams;
        TCHAR className [MAX_PATH];
        DWORD index = 0;
        PSP_DRVINFO_DETAIL_DATA pDriverInfoDetail;
        /* for our purposes, 2k buffer is more
         * than enough to obtain the hardware ID
         * of the VBoxTAP driver. */
        DWORD detailBuf [2048];

        HKEY hkey = NULL;
        DWORD cbSize;
        DWORD dwValueType;

        /* initialize the structure size */
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

        /* copy the net class GUID */
        memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof(GUID_DEVCLASS_NET));

        /* create an empty device info set associated with the net class GUID */
        hDeviceInfo = SetupDiCreateDeviceInfoList (&netGuid, NULL);
        if (hDeviceInfo == INVALID_HANDLE_VALUE)
            SetErrBreak (("SetupDiCreateDeviceInfoList failed (0x%08X)",
                          GetLastError()));

        /* get the class name from GUID */
        ok = SetupDiClassNameFromGuid (&netGuid, className, MAX_PATH, NULL);
        if (!ok)
            SetErrBreak (("SetupDiClassNameFromGuid failed (0x%08X)",
                          GetLastError()));

        /* create a device info element and add the new device instance
         * key to registry */
        ok = SetupDiCreateDeviceInfo (hDeviceInfo, className, &netGuid, NULL, NULL,
                                     DICD_GENERATE_ID, &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCreateDeviceInfo failed (0x%08X)",
                          GetLastError()));

        /* select the newly created device info to be the currently
           selected member */
        ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiSetSelectedDevice failed (0x%08X)",
                          GetLastError()));

        /* build a list of class drivers */
        ok = SetupDiBuildDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                        SPDIT_CLASSDRIVER);
        if (!ok)
            SetErrBreak (("SetupDiBuildDriverInfoList failed (0x%08X)",
                          GetLastError()));

        destroyList = TRUE;

        /* enumerate the driver info list */
        while (TRUE)
        {
            BOOL ret;

            ret = SetupDiEnumDriverInfo (hDeviceInfo, &DeviceInfoData,
                                         SPDIT_CLASSDRIVER, index, &DriverInfoData);

            /* if the function failed and GetLastError() returned
             * ERROR_NO_MORE_ITEMS, then we have reached the end of the
             * list.  Othewise there was something wrong with this
             * particular driver. */
            if (!ret)
            {
                if(GetLastError() == ERROR_NO_MORE_ITEMS)
                    break;
                else
                {
                    index++;
                    continue;
                }
            }

            pDriverInfoDetail = (PSP_DRVINFO_DETAIL_DATA) detailBuf;
            pDriverInfoDetail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);

            /* if we successfully find the hardware ID and it turns out to
             * be the one for the loopback driver, then we are done. */
            if (SetupDiGetDriverInfoDetail (hDeviceInfo,
                                            &DeviceInfoData,
                                            &DriverInfoData,
                                            pDriverInfoDetail,
                                            sizeof (detailBuf),
                                            NULL))
            {
                TCHAR * t;

                /* pDriverInfoDetail->HardwareID is a MULTISZ string.  Go through the
                 * whole list and see if there is a match somewhere. */
                t = pDriverInfoDetail->HardwareID;
                while (t && *t && t < (TCHAR *) &detailBuf [sizeof(detailBuf) / sizeof (detailBuf[0])])
                {
                    if (!_tcsicmp(t, DRIVERHWID))
                        break;

                    t += _tcslen(t) + 1;
                }

                if (t && *t && t < (TCHAR *) &detailBuf [sizeof(detailBuf) / sizeof (detailBuf[0])])
                {
                    found = TRUE;
                    break;
                }
            }

            index ++;
        }

        if (!found)
            SetErrBreak ((tr ("Could not find Host Interface Networking driver! "
                              "Please reinstall")));

        /* set the loopback driver to be the currently selected */
        ok = SetupDiSetSelectedDriver (hDeviceInfo, &DeviceInfoData,
                                       &DriverInfoData);
        if (!ok)
            SetErrBreak (("SetupDiSetSelectedDriver failed (0x%08X)",
                          GetLastError()));

        /* register the phantom device to prepare for install */
        ok = SetupDiCallClassInstaller (DIF_REGISTERDEVICE, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller failed (0x%08X)",
                          GetLastError()));

        /* registered, but remove if errors occur in the following code */
        registered = TRUE;

        /* ask the installer if we can install the device */
        ok = SetupDiCallClassInstaller (DIF_ALLOW_INSTALL, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
        {
            if (GetLastError() != ERROR_DI_DO_DEFAULT)
                SetErrBreak (("SetupDiCallClassInstaller (DIF_ALLOW_INSTALL) failed (0x%08X)",
                              GetLastError()));
            /* that's fine */
        }

        /* install the files first */
        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES, hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICEFILES) failed (0x%08X)",
                          GetLastError()));

        /* get the device install parameters and disable filecopy */
        DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
        ok = SetupDiGetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                            &DeviceInstallParams);
        if (ok)
        {
            DeviceInstallParams.Flags |= DI_NOFILECOPY;
            ok = SetupDiSetDeviceInstallParams (hDeviceInfo, &DeviceInfoData,
                                                &DeviceInstallParams);
            if (!ok)
                SetErrBreak (("SetupDiSetDeviceInstallParams failed (0x%08X)",
                              GetLastError()));
        }

        /*
         * Register any device-specific co-installers for this device,
         */

        ok = SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_REGISTER_COINSTALLERS) failed (0x%08X)",
                          GetLastError()));

        /*
         * install any  installer-specified interfaces.
         * and then do the real install
         */
        ok = SetupDiCallClassInstaller (DIF_INSTALLINTERFACES,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLINTERFACES) failed (0x%08X)",
                          GetLastError()));

        ok = SetupDiCallClassInstaller (DIF_INSTALLDEVICE,
                                        hDeviceInfo,
                                        &DeviceInfoData);
        if (!ok)
            SetErrBreak (("SetupDiCallClassInstaller (DIF_INSTALLDEVICE) failed (0x%08X)",
                          GetLastError()));

        /* Figure out NetCfgInstanceId */
        hkey = SetupDiOpenDevRegKey (hDeviceInfo,
                                     &DeviceInfoData,
                                     DICS_FLAG_GLOBAL,
                                     0,
                                     DIREG_DRV,
                                     KEY_READ);
        if (hkey == INVALID_HANDLE_VALUE)
            SetErrBreak (("SetupDiOpenDevRegKey failed (0x%08X)",
                          GetLastError()));

        cbSize = sizeof (pCfgGuidString);
        DWORD ret;
        ret = RegQueryValueEx (hkey, _T ("NetCfgInstanceId"), NULL,
                               &dwValueType, (LPBYTE) pCfgGuidString, &cbSize);
        RegCloseKey (hkey);

        ret = RenameConnection (pCfgGuidString, Bstr (aName));
        if (FAILED (ret))
            SetErrBreak (("Failed to set interface name (ret=0x%08X, "
                          "pCfgGuidString='%ls', cbSize=%d)",
                           ret, pCfgGuidString, cbSize));
    }
    while (0);

    /*
     * cleanup
     */

    if (hDeviceInfo != INVALID_HANDLE_VALUE)
    {
        /* an error has occured, but the device is registered, we must remove it */
        if (ret != 0 && registered)
            SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);

        found = SetupDiDeleteDeviceInfo (hDeviceInfo, &DeviceInfoData);

        /* destroy the driver info list */
        if (destroyList)
            SetupDiDestroyDriverInfoList (hDeviceInfo, &DeviceInfoData,
                                          SPDIT_CLASSDRIVER);
        /* clean up the device info set */
        SetupDiDestroyDeviceInfoList (hDeviceInfo);
    }

    /* return the network connection GUID on success */
    if (VBOX_SUCCESS (vrc))
    {
        /* remove the curly bracket at the end */
        pCfgGuidString [_tcslen (pCfgGuidString) - 1] = '\0';
        LogFlowFunc (("Network connection GUID string = {%ls}\n", pCfgGuidString + 1));

        aGUID = Guid (Utf8Str (pCfgGuidString + 1));
        LogFlowFunc (("Network connection GUID = {%Vuuid}\n", aGUID.raw()));
        Assert (!aGUID.isEmpty());
    }

    LogFlowFunc (("vrc=%Vrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

/* static */
int Host::removeNetworkInterface (SVCHlpClient *aClient,
                                  const Guid &aGUID,
                                  Utf8Str &aErrMsg)
{
    LogFlowFuncEnter();
    LogFlowFunc (("Network connection GUID = {%Vuuid}\n", aGUID.raw()));

    AssertReturn (aClient, VERR_INVALID_POINTER);
    AssertReturn (!aGUID.isEmpty(), VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;

    do
    {
        TCHAR lszPnPInstanceId [512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do
        {
            char strRegLocation [256];
            sprintf (strRegLocation,
                     "SYSTEM\\CurrentControlSet\\Control\\Network\\"
                     "{4D36E972-E325-11CE-BFC1-08002BE10318}\\{%s}",
                     aGUID.toString().raw());
            LONG status;
            status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                    KEY_READ, &hkeyNetwork);
            if ((status != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak ((
                    tr ("Host interface network is not found in registry (%s) [1]"),
                    strRegLocation));

            status = RegOpenKeyExA (hkeyNetwork, "Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((status != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak ((
                    tr ("Host interface network is not found in registry (%s) [2]"),
                    strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            status = RegQueryValueExW (hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((status != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak ((
                    tr ("Host interface network is not found in registry (%s) [3]"),
                    strRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey (hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey (hkeyNetwork);

        if (VBOX_FAILURE (vrc))
            break;

        /*
         * Now we are going to enumerate all network devices and
         * wait until we encounter the right device instance ID
         */

        HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;

        do
        {
            BOOL ok;
            DWORD ret = 0;
            GUID netGuid;
            SP_DEVINFO_DATA DeviceInfoData;
            DWORD index = 0;
            BOOL found = FALSE;
            DWORD size = 0;

            /* initialize the structure size */
            DeviceInfoData.cbSize = sizeof (SP_DEVINFO_DATA);

            /* copy the net class GUID */
            memcpy (&netGuid, &GUID_DEVCLASS_NET, sizeof (GUID_DEVCLASS_NET));

            /* return a device info set contains all installed devices of the Net class */
            hDeviceInfo = SetupDiGetClassDevs (&netGuid, NULL, NULL, DIGCF_PRESENT);

            if (hDeviceInfo == INVALID_HANDLE_VALUE)
                SetErrBreak (("SetupDiGetClassDevs failed (0x%08X)", GetLastError()));

            /* enumerate the driver info list */
            while (TRUE)
            {
                TCHAR *deviceHwid;

                ok = SetupDiEnumDeviceInfo (hDeviceInfo, index, &DeviceInfoData);

                if (!ok)
                {
                    if (GetLastError() == ERROR_NO_MORE_ITEMS)
                        break;
                    else
                    {
                        index++;
                        continue;
                    }
                }

                /* try to get the hardware ID registry property */
                ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                       &DeviceInfoData,
                                                       SPDRP_HARDWAREID,
                                                       NULL,
                                                       NULL,
                                                       0,
                                                       &size);
                if (!ok)
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                    {
                        index++;
                        continue;
                    }

                    deviceHwid = (TCHAR *) malloc (size);
                    ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                           &DeviceInfoData,
                                                           SPDRP_HARDWAREID,
                                                           NULL,
                                                           (PBYTE)deviceHwid,
                                                           size,
                                                           NULL);
                    if (!ok)
                    {
                        free (deviceHwid);
                        deviceHwid = NULL;
                        index++;
                        continue;
                    }
                }
                else
                {
                    /* something is wrong.  This shouldn't have worked with a NULL buffer */
                    index++;
                    continue;
                }

                for (TCHAR *t = deviceHwid;
                     t && *t && t < &deviceHwid[size / sizeof(TCHAR)];
                     t += _tcslen (t) + 1)
                {
                    if (!_tcsicmp (DRIVERHWID, t))
                    {
                          /* get the device instance ID */
                          TCHAR devID [MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_ID(DeviceInfoData.DevInst,
                                               devID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (wcscmp(devID, lszPnPInstanceId) == 0)
                              {
                                  found = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (deviceHwid)
                {
                    free (deviceHwid);
                    deviceHwid = NULL;
                }

                if (found)
                    break;

                index++;
            }

            if (found == FALSE)
                SetErrBreak ((tr ("Host Interface Network driver not found (0x%08X)"),
                              GetLastError()));

            ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak (("SetupDiSetSelectedDevice failed (0x%08X)",
                              GetLastError()));

            ok = SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
            if (!ok)
                SetErrBreak (("SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)",
                              GetLastError()));
        }
        while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList (hDeviceInfo);

        if (VBOX_FAILURE (vrc))
            break;
    }
    while (0);

    LogFlowFunc (("vrc=%Vrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

#undef SetErrBreak

/* static */
HRESULT Host::networkInterfaceHelperClient (SVCHlpClient *aClient,
                                            Progress *aProgress,
                                            void *aUser, int *aVrc)
{
    LogFlowFuncEnter();
    LogFlowFunc (("aClient={%p}, aProgress={%p}, aUser={%p}\n",
                  aClient, aProgress, aUser));

    AssertReturn ((aClient == NULL && aProgress == NULL && aVrc == NULL) ||
                  (aClient != NULL && aProgress != NULL && aVrc != NULL),
                  E_POINTER);
    AssertReturn (aUser, E_POINTER);

    std::auto_ptr <NetworkInterfaceHelperClientData>
        d (static_cast <NetworkInterfaceHelperClientData *> (aUser));

    if (aClient == NULL)
    {
        /* "cleanup only" mode, just return (it will free aUser) */
        return S_OK;
    }

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    switch (d->msgCode)
    {
        case SVCHlpMsg::CreateHostNetworkInterface:
        {
            LogFlowFunc (("CreateHostNetworkInterface:\n"));
            LogFlowFunc (("Network connection name = '%ls'\n", d->name.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (VBOX_FAILURE (vrc)) break;
            vrc = aClient->write (Utf8Str (d->name));
            if (VBOX_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (VBOX_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::CreateHostNetworkInterface_OK:
                    {
                        /* read the GUID */
                        Guid guid;
                        vrc = aClient->read (guid);
                        if (VBOX_FAILURE (vrc)) break;

                        LogFlowFunc (("Network connection GUID = {%Vuuid}\n", guid.raw()));

                        /* initialize the object returned to the caller by
                         * CreateHostNetworkInterface() */
                        rc = d->iface->init (d->name, guid);
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (VBOX_FAILURE (vrc)) break;

                        rc = setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        ComAssertMsgFailedBreak ((
                            "Invalid message code %d (%08lX)\n",
                            reply, reply),
                            rc = E_FAIL);
                    }
                }
            }

            break;
        }
        case SVCHlpMsg::RemoveHostNetworkInterface:
        {
            LogFlowFunc (("RemoveHostNetworkInterface:\n"));
            LogFlowFunc (("Network connection GUID = {%Vuuid}\n", d->guid.raw()));

            /* write message and parameters */
            vrc = aClient->write (d->msgCode);
            if (VBOX_FAILURE (vrc)) break;
            vrc = aClient->write (d->guid);
            if (VBOX_FAILURE (vrc)) break;

            /* wait for a reply */
            bool endLoop = false;
            while (!endLoop)
            {
                SVCHlpMsg::Code reply = SVCHlpMsg::Null;

                vrc = aClient->read (reply);
                if (VBOX_FAILURE (vrc)) break;

                switch (reply)
                {
                    case SVCHlpMsg::OK:
                    {
                        /* no parameters */
                        rc = S_OK;
                        endLoop = true;
                        break;
                    }
                    case SVCHlpMsg::Error:
                    {
                        /* read the error message */
                        Utf8Str errMsg;
                        vrc = aClient->read (errMsg);
                        if (VBOX_FAILURE (vrc)) break;

                        rc = setError (E_FAIL, errMsg);
                        endLoop = true;
                        break;
                    }
                    default:
                    {
                        endLoop = true;
                        ComAssertMsgFailedBreak ((
                            "Invalid message code %d (%08lX)\n",
                            reply, reply),
                            rc = E_FAIL);
                    }
                }
            }

            break;
        }
        default:
            ComAssertMsgFailedBreak ((
                "Invalid message code %d (%08lX)\n",
                d->msgCode, d->msgCode),
                rc = E_FAIL);
    }

    if (aVrc)
        *aVrc = vrc;

    LogFlowFunc (("rc=0x%08X, vrc=%Vrc\n", rc, vrc));
    LogFlowFuncLeave();
    return rc;
}

/* static */
int Host::networkInterfaceHelperServer (SVCHlpClient *aClient,
                                        SVCHlpMsg::Code aMsgCode)
{
    LogFlowFuncEnter();
    LogFlowFunc (("aClient={%p}, aMsgCode=%d\n", aClient, aMsgCode));

    AssertReturn (aClient, VERR_INVALID_POINTER);

    int vrc = VINF_SUCCESS;

    switch (aMsgCode)
    {
        case SVCHlpMsg::CreateHostNetworkInterface:
        {
            LogFlowFunc (("CreateHostNetworkInterface:\n"));

            Utf8Str name;
            vrc = aClient->read (name);
            if (VBOX_FAILURE (vrc)) break;

            Guid guid;
            Utf8Str errMsg;
            vrc = createNetworkInterface (aClient, name, guid, errMsg);

            if (VBOX_SUCCESS (vrc))
            {
                /* write success followed by GUID */
                vrc = aClient->write (SVCHlpMsg::CreateHostNetworkInterface_OK);
                if (VBOX_FAILURE (vrc)) break;
                vrc = aClient->write (guid);
                if (VBOX_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Vrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (VBOX_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (VBOX_FAILURE (vrc)) break;
            }

            break;
        }
        case SVCHlpMsg::RemoveHostNetworkInterface:
        {
            LogFlowFunc (("RemoveHostNetworkInterface:\n"));

            Guid guid;
            vrc = aClient->read (guid);
            if (VBOX_FAILURE (vrc)) break;

            Utf8Str errMsg;
            vrc = removeNetworkInterface (aClient, guid, errMsg);

            if (VBOX_SUCCESS (vrc))
            {
                /* write parameter-less success */
                vrc = aClient->write (SVCHlpMsg::OK);
                if (VBOX_FAILURE (vrc)) break;
            }
            else
            {
                /* write failure followed by error message */
                if (errMsg.isEmpty())
                    errMsg = Utf8StrFmt ("Unspecified error (%Vrc)", vrc);
                vrc = aClient->write (SVCHlpMsg::Error);
                if (VBOX_FAILURE (vrc)) break;
                vrc = aClient->write (errMsg);
                if (VBOX_FAILURE (vrc)) break;
            }

            break;
        }
        default:
            AssertMsgFailedBreak ((
                "Invalid message code %d (%08lX)\n", aMsgCode, aMsgCode),
                VERR_GENERAL_FAILURE);
    }

    LogFlowFunc (("vrc=%Vrc\n", vrc));
    LogFlowFuncLeave();
    return vrc;
}

#endif /* RT_OS_WINDOWS */

