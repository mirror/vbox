/* $Id$ */
/** @file
 * Classes for handling hardware detection under Linux.  Please feel free to
 * expand these to work for other systems (Solaris!) or to add new ones for
 * other systems.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#define LOG_GROUP LOG_GROUP_MAIN

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include <HostHardwareLinux.h>

#include <VBox/log.h>

#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/thread.h>  /* for RTThreadSleep() */
#include <iprt/string.h>

#ifdef RT_OS_LINUX
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <sys/ioctl.h>
# include <fcntl.h>
# include <mntent.h>
/* bird: This is a hack to work around conflicts between these linux kernel headers
 *       and the GLIBC tcpip headers. They have different declarations of the 4
 *       standard byte order functions. */
// # define _LINUX_BYTEORDER_GENERIC_H
# define _LINUX_BYTEORDER_SWABB_H
# include <linux/cdrom.h>
# include <linux/fd.h>
# ifdef VBOX_WITH_DBUS
#  include <vbox-dbus.h>
# endif
# include <errno.h>
# include <scsi/scsi.h>
# include <scsi/sg.h>

# include <iprt/linux/sysfs.h>
#endif /* RT_OS_LINUX */
#include <vector>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

bool g_testHostHardwareLinux = false;
static bool testing () { return g_testHostHardwareLinux; }

/*******************************************************************************
*   Typedefs and Defines                                                       *
*******************************************************************************/

/** When waiting for hotplug events, we currently restart the wait after at
 * most this many milliseconds. */
enum { DBUS_POLL_TIMEOUT = 2000 /* ms */ };


static bool validateDevice(const char *deviceNode, bool isDVD);
static int getDriveInfoFromEnv(const char *pszVar, DriveInfoList *pList,
                               bool isDVD, bool *pfSuccess);
static int getDriveInfoFromSysfs(DriveInfoList *pList, bool isDVD,
                                 bool *pfSuccess);
int scsiDoInquiry(const char *pszNode, uint8_t *pu8Type, char *pchVendor,
                  size_t cchVendor, char *pchModel, size_t cchModel);
static int getDVDInfoFromMTab(char *mountTable, DriveInfoList *pList);
#ifdef VBOX_WITH_DBUS
/* These must be extern to be usable in the RTMemAutoPtr template */
extern void VBoxHalShutdown (DBusConnection *pConnection);
extern void VBoxHalShutdownPrivate (DBusConnection *pConnection);
extern void VBoxDBusConnectionUnref(DBusConnection *pConnection);
extern void VBoxDBusConnectionCloseAndUnref(DBusConnection *pConnection);
extern void VBoxDBusMessageUnref(DBusMessage *pMessage);

static int halInit(RTMemAutoPtr <DBusConnection, VBoxHalShutdown> *pConnection);
static int halInitPrivate(RTMemAutoPtr <DBusConnection, VBoxHalShutdownPrivate> *pConnection);
static int halFindDeviceStringMatch (DBusConnection *pConnection,
                                     const char *pszKey, const char *pszValue,
                                     RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> *pMessage);
/*
static int halFindDeviceStringMatchVector (DBusConnection *pConnection,
                                           const char *pszKey,
                                           const char *pszValue,
                                           std::vector<iprt::MiniString> *pMatches);
*/
static int halGetPropertyStrings (DBusConnection *pConnection,
                                  const char *pszUdi, size_t cKeys,
                                  const char **papszKeys, char **papszValues,
                                  RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> *pMessage);
/*
static int halGetPropertyStringsVector (DBusConnection *pConnection,
                                        const char *pszUdi, size_t cProps,
                                        const char **papszKeys,
                                        std::vector<iprt::MiniString> *pMatches,
                                        bool *pfMatches, bool *pfSuccess);
*/
static int getDriveInfoFromHal(DriveInfoList *pList, bool isDVD,
                               bool *pfSuccess);
static int getUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess);
static int getOldUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess);
static int getUSBInterfacesFromHal(std::vector <iprt::MiniString> *pList,
                                   const char *pcszUdi, bool *pfSuccess);
static DBusHandlerResult dbusFilterFunction (DBusConnection *pConnection,
                                             DBusMessage *pMessage, void *pvUser);
#endif  /* VBOX_WITH_DBUS */

/** Find the length of a string, ignoring trailing non-ascii or control
 * characters */
static size_t strLenStripped(const char *psz)
{
    size_t cch = 0;
    for (size_t i = 0; psz[i] != '\0'; ++i)
        if (psz[i] > 32 && psz[i] < 127)
            cch = i;
    return cch + 1;
}

int VBoxMainDriveInfo::updateDVDs ()
{
    LogFlowThisFunc(("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        mDVDList.clear ();
#if defined(RT_OS_LINUX)
        /* Always allow the user to override our auto-detection using an
         * environment variable. */
        if (RT_SUCCESS(rc) && (!success || testing()))
            rc = getDriveInfoFromEnv ("VBOX_CDROM", &mDVDList, true /* isDVD */,
                                      &success);
#ifdef VBOX_WITH_DBUS
        if (RT_SUCCESS(rc) && RT_SUCCESS(VBoxLoadDBusLib()) && (!success || testing()))
            rc = getDriveInfoFromHal(&mDVDList, true /* isDVD */, &success);
#endif /* VBOX_WITH_DBUS defined */
        if (RT_SUCCESS(rc) && (!success | testing()))
            rc = getDriveInfoFromSysfs(&mDVDList, true /* isDVD */, &success);
        /* On Linux without hal, the situation is much more complex. We will
         * take a heuristical approach.  The general strategy is to try some
         * known device names and see of they exist.  Failing that, we
         * enumerate the /etc/fstab file (luckily there's an API to parse it)
         * for CDROM devices. Ok, let's start! */
        if (RT_SUCCESS(rc) && (!success || testing()))
        {
            // this is a good guess usually
            if (validateDevice("/dev/cdrom", true))
                mDVDList.push_back(DriveInfo ("/dev/cdrom"));

            // check the mounted drives
            rc = getDVDInfoFromMTab((char*)"/etc/mtab", &mDVDList);

            // check the drives that can be mounted
            if (RT_SUCCESS(rc))
                rc = getDVDInfoFromMTab((char*)"/etc/fstab", &mDVDList);
        }
#endif
    }
    catch(std::bad_alloc &e)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc(("rc=%Rrc\n", rc));
    return rc;
}

int VBoxMainDriveInfo::updateFloppies ()
{
    LogFlowThisFunc(("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        mFloppyList.clear ();
#if defined(RT_OS_LINUX)
        if (RT_SUCCESS(rc) && (!success || testing()))
            rc = getDriveInfoFromEnv ("VBOX_FLOPPY", &mFloppyList, false /* isDVD */,
                                      &success);
#ifdef VBOX_WITH_DBUS
        if (   RT_SUCCESS(rc)
            && RT_SUCCESS(VBoxLoadDBusLib())
            && (!success || testing()))
            rc = getDriveInfoFromHal(&mFloppyList, false /* isDVD */, &success);
#endif /* VBOX_WITH_DBUS defined */
        if (   RT_SUCCESS(rc)
            && RT_SUCCESS(VBoxLoadDBusLib())
            && (!success || testing()))
            rc = getDriveInfoFromHal(&mFloppyList, false /* isDVD */, &success);
        /* As with the CDROMs, on Linux we have to take a multi-level approach
         * involving parsing the mount tables. As this is not bulletproof, we
         * give the user the chance to override the detection using an
         * environment variable, skiping the detection. */

        if (RT_SUCCESS(rc) && (!success || testing()))
        {
            // we assume that a floppy is always /dev/fd[x] with x from 0 to 7
            char devName[10];
            for (int i = 0; i <= 7; i++)
            {
                RTStrPrintf(devName, sizeof(devName), "/dev/fd%d", i);
                if (validateDevice(devName, false))
                    mFloppyList.push_back (DriveInfo (devName));
            }
        }
#endif
    }
    catch(std::bad_alloc &e)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc(("rc=%Rrc\n", rc));
    return rc;
}

int VBoxMainUSBDeviceInfo::UpdateDevices ()
{
    LogFlowThisFunc(("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        bool halSuccess = false;
        mDeviceList.clear();
#if defined(RT_OS_LINUX)
#ifdef VBOX_WITH_DBUS
        if (   RT_SUCCESS(rc)
            && RT_SUCCESS(VBoxLoadDBusLib())
            && (!success || testing()))
            rc = getUSBDeviceInfoFromHal(&mDeviceList, &halSuccess);
        /* Try the old API if the new one *succeeded* as only one of them will
         * pick up devices anyway. */
        if (RT_SUCCESS(rc) && halSuccess && (!success || testing()))
            rc = getOldUSBDeviceInfoFromHal(&mDeviceList, &halSuccess);
        if (!success)
            success = halSuccess;
#endif /* VBOX_WITH_DBUS defined */
#endif /* RT_OS_LINUX */
    }
    catch(std::bad_alloc &e)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc(("rc=%Rrc\n", rc));
    return rc;
}

struct VBoxMainHotplugWaiter::Context
{
#if defined RT_OS_LINUX && defined VBOX_WITH_DBUS
    /** The connection to DBus */
    RTMemAutoPtr <DBusConnection, VBoxHalShutdownPrivate> mConnection;
    /** Semaphore which is set when a device is hotplugged and reset when
     * it is read. */
    volatile bool mTriggered;
    /** A flag to say that we wish to interrupt the current wait. */
    volatile bool mInterrupt;
    /** Constructor */
    Context() : mTriggered(false), mInterrupt(false) {}
#endif  /* defined RT_OS_LINUX && defined VBOX_WITH_DBUS */
};

/* This constructor sets up a private connection to the DBus daemon, connects
 * to the hal service and installs a filter which sets the mTriggered flag in
 * the Context structure when a device (not necessarily USB) is added or
 * removed. */
VBoxMainHotplugWaiter::VBoxMainHotplugWaiter ()
{
#if defined RT_OS_LINUX && defined VBOX_WITH_DBUS
    int rc = VINF_SUCCESS;

    mContext = new Context;
    if (RT_SUCCESS(VBoxLoadDBusLib()))
    {
        for (unsigned i = 0; RT_SUCCESS(rc) && i < 5 && !mContext->mConnection; ++i)
        {
            rc = halInitPrivate (&mContext->mConnection);
        }
        if (!mContext->mConnection)
            rc = VERR_NOT_SUPPORTED;
        DBusMessage *pMessage;
        while (   RT_SUCCESS(rc)
               && (pMessage = dbus_connection_pop_message (mContext->mConnection.get())) != NULL)
            dbus_message_unref (pMessage); /* empty the message queue. */
        if (   RT_SUCCESS(rc)
            && !dbus_connection_add_filter (mContext->mConnection.get(),
                                            dbusFilterFunction,
                                            (void *) &mContext->mTriggered, NULL))
            rc = VERR_NO_MEMORY;
        if (RT_FAILURE(rc))
            mContext->mConnection.reset();
    }
#endif /* defined RT_OS_LINUX && defined VBOX_WITH_DBUS */
}

/* Destructor */
VBoxMainHotplugWaiter::~VBoxMainHotplugWaiter ()
{
#if defined RT_OS_LINUX && defined VBOX_WITH_DBUS
    if (!!mContext->mConnection)
        dbus_connection_remove_filter (mContext->mConnection.get(), dbusFilterFunction,
                                       (void *) &mContext->mTriggered);
    delete mContext;
#endif /* defined RT_OS_LINUX && defined VBOX_WITH_DBUS */
}

/* Currently this is implemented using a timed out wait on our private DBus
 * connection.  Because the connection is private we don't have to worry about
 * blocking other users. */
int VBoxMainHotplugWaiter::Wait(unsigned cMillies)
{
    int rc = VINF_SUCCESS;
#if defined RT_OS_LINUX && defined VBOX_WITH_DBUS
    if (!mContext->mConnection)
        rc = VERR_NOT_SUPPORTED;
    bool connected = true;
    mContext->mTriggered = false;
    mContext->mInterrupt = false;
    unsigned cRealMillies;
    if (cMillies != RT_INDEFINITE_WAIT)
        cRealMillies = cMillies;
    else
        cRealMillies = DBUS_POLL_TIMEOUT;
    while (   RT_SUCCESS(rc) && connected && !mContext->mTriggered
           && !mContext->mInterrupt)
    {
        connected = dbus_connection_read_write_dispatch (mContext->mConnection.get(),
                                                         cRealMillies);
        if (mContext->mInterrupt)
            LogFlowFunc(("wait loop interrupted\n"));
        if (cMillies != RT_INDEFINITE_WAIT)
            mContext->mInterrupt = true;
    }
    if (!connected)
        rc = VERR_TRY_AGAIN;
#else  /* !(defined RT_OS_LINUX && defined VBOX_WITH_DBUS) */
    rc = VERR_NOT_IMPLEMENTED;
#endif  /* !(defined RT_OS_LINUX && defined VBOX_WITH_DBUS) */
    return rc;
}

/* Set a flag to tell the Wait not to resume next time it times out. */
void VBoxMainHotplugWaiter::Interrupt()
{
#if defined RT_OS_LINUX && defined VBOX_WITH_DBUS
    LogFlowFunc(("\n"));
    mContext->mInterrupt = true;
#endif  /* defined RT_OS_LINUX && defined VBOX_WITH_DBUS */
}

#ifdef RT_OS_LINUX
/**
 * Helper function to check whether the given device node is a valid drive
 */
/* static */
bool validateDevice(const char *deviceNode, bool isDVD)
{
    AssertReturn(VALID_PTR (deviceNode), VERR_INVALID_POINTER);
    LogFlowFunc (("deviceNode=%s, isDVD=%d\n", deviceNode, isDVD));
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
        if (isDVD)
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
    LogFlowFunc (("retValue=%d\n", retValue));
    return retValue;
}
#else  /* !RT_OS_LINUX */
# error Port me!  Copying code over from HostImpl.cpp should be most of the job though.
#endif  /* !RT_OS_LINUX */

/**
 * Extract the names of drives from an environment variable and add them to a
 * list if they are valid.
 * @returns iprt status code
 * @param   pszVar     the name of the environment variable.  The variable
 *                     value should be a list of device node names, separated
 *                     by ':' characters.
 * @param   pList      the list to append the drives found to
 * @param   isDVD      are we looking for DVD drives or for floppies?
 * @param   pfSuccess  this will be set to true if we found at least one drive
 *                     and to false otherwise.  Optional.
 */
/* static */
int getDriveInfoFromEnv(const char *pszVar, DriveInfoList *pList,
                               bool isDVD, bool *pfSuccess)
{
    AssertReturn(   VALID_PTR (pszVar) && VALID_PTR (pList)
                 && (pfSuccess == NULL || VALID_PTR (pfSuccess)),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pszVar=%s, pList=%p, isDVD=%d, pfSuccess=%p\n", pszVar,
                  pList, isDVD, pfSuccess));
    int rc = VINF_SUCCESS;
    bool success = false;

    try
    {
        RTMemAutoPtr<char, RTStrFree> drive;
        const char *pszValue = RTEnvGet (pszVar);
        if (pszValue != NULL)
        {
            drive = RTStrDup (pszValue);
            if (!drive)
                rc = VERR_NO_MEMORY;
        }
        if (pszValue != NULL && RT_SUCCESS(rc))
        {
            char *pDrive = drive.get();
            char *pDriveNext = strchr (pDrive, ':');
            while (pDrive != NULL && *pDrive != '\0')
            {
                if (pDriveNext != NULL)
                    *pDriveNext = '\0';
                if (validateDevice(pDrive, isDVD))
                {
                    pList->push_back (DriveInfo (pDrive));
                    success = true;
                }
                if (pDriveNext != NULL)
                {
                    pDrive = pDriveNext + 1;
                    pDriveNext = strchr (pDrive, ':');
                }
                else
                    pDrive = NULL;
            }
        }
        if (pfSuccess != NULL)
            *pfSuccess = success;
    }
    catch(std::bad_alloc &e)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowFunc (("rc=%Rrc, success=%d\n", rc, success));
    return rc;
}

#ifdef RT_OS_LINUX
/**
 * Helper function to parse the given mount file and add found entries
 */
/* static */
int getDVDInfoFromMTab(char *mountTable, DriveInfoList *pList)
{
    AssertReturn(VALID_PTR (mountTable) && VALID_PTR (pList),
                 VERR_INVALID_POINTER);
#ifdef RT_OS_LINUX
    LogFlowFunc (("mountTable=%s, pList=%p\n", mountTable, pList));
    int rc = VINF_SUCCESS;
    FILE *mtab = setmntent(mountTable, "r");
    if (mtab)
    {
        try
        {
            struct mntent *mntent;
            RTMemAutoPtr <char, RTStrFree> mnt_type, mnt_dev;
            char *tmp;
            while (RT_SUCCESS(rc) && (mntent = getmntent(mtab)))
            {
                mnt_type = RTStrDup (mntent->mnt_type);
                mnt_dev = RTStrDup (mntent->mnt_fsname);
                if (!mnt_type || !mnt_dev)
                    rc = VERR_NO_MEMORY;
                // supermount fs case
                if (RT_SUCCESS(rc) && strcmp(mnt_type.get(), "supermount") == 0)
                {
                    tmp = strstr(mntent->mnt_opts, "fs=");
                    if (tmp)
                    {
                        mnt_type = RTStrDup(tmp + strlen("fs="));
                        if (!mnt_type)
                            rc = VERR_NO_MEMORY;
                        else
                        {
                            tmp = strchr(mnt_type.get(), ',');
                            if (tmp)
                                *tmp = '\0';
                        }
                    }
                    tmp = strstr(mntent->mnt_opts, "dev=");
                    if (tmp)
                    {
                        mnt_dev = RTStrDup(tmp + strlen("dev="));
                        if (!mnt_dev)
                            rc = VERR_NO_MEMORY;
                        else
                        {
                            tmp = strchr(mnt_dev.get(), ',');
                            if (tmp)
                                *tmp = '\0';
                        }
                    }
                }
                // use strstr here to cover things fs types like "udf,iso9660"
                if (RT_SUCCESS(rc) && strstr(mnt_type.get(), "iso9660") == 0)
                {
                    if (validateDevice(mnt_dev.get(), true))
                    {
                        bool insert = true;
                        struct stat srcInfo;
                        if (stat (mnt_dev.get(), &srcInfo) < 0)
                            insert = false;
                        for (DriveInfoList::const_iterator it = pList->begin();
                            insert && it != pList->end(); ++it)
                        {
                            struct stat destInfo;
                            if (   (stat (it->mDevice.c_str(), &destInfo) == 0)
                                && (srcInfo.st_rdev == destInfo.st_rdev))
                                insert = false;
                        }
                        if (insert)
                            pList->push_back (DriveInfo (mnt_dev.get()));
                    }
                }
            }
        }
        catch(std::bad_alloc &e)
        {
            rc = VERR_NO_MEMORY;
        }
        endmntent(mtab);
    }
    return rc;
#endif
}

#endif  /* RT_OS_LINUX */

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_DBUS)
/** Wrapper class around DBusError for automatic cleanup */
class autoDBusError
{
    DBusError mError;
public:
    autoDBusError () { dbus_error_init (&mError); }
    ~autoDBusError ()
    {
        if (IsSet())
            dbus_error_free (&mError);
    }
    DBusError &get () { return mError; }
    bool IsSet ()
    {
        Assert ((mError.name == NULL) == (mError.message == NULL));
        return (mError.name != NULL);
    }
    bool HasName (const char *pszName)
    {
        Assert ((mError.name == NULL) == (mError.message == NULL));
        return (RTStrCmp (mError.name, pszName) == 0);
    }
    void FlowLog ()
    {
        if (IsSet ())
            LogFlow(("DBus error %s: %s\n", mError.name, mError.message));
    }
};

/**
 * Helper function for setting up a connection to the DBus daemon and
 * registering with the hal service.
 *
 * @note If libdbus is being loaded at runtime then be sure to call
 *       VBoxDBusCheckPresence before calling this.
 * @returns iprt status code
 * @param   ppConnection  where to store the connection handle
 */
/* static */
int halInit (RTMemAutoPtr <DBusConnection, VBoxHalShutdown> *pConnection)
{
    AssertReturn(VALID_PTR (pConnection), VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p\n", pConnection));
    int rc = VINF_SUCCESS;
    bool halSuccess = true;
    autoDBusError dbusError;

    RTMemAutoPtr <DBusConnection, VBoxDBusConnectionUnref> dbusConnection;
    dbusConnection = dbus_bus_get (DBUS_BUS_SYSTEM, &dbusError.get());
    if (!dbusConnection)
        halSuccess = false;
    if (halSuccess)
    {
        dbus_connection_set_exit_on_disconnect (dbusConnection.get(), false);
        halSuccess = dbus_bus_name_has_owner (dbusConnection.get(),
                                              "org.freedesktop.Hal", &dbusError.get());
    }
    if (halSuccess)
    {
        dbus_bus_add_match (dbusConnection.get(),
                            "type='signal',"
                            "interface='org.freedesktop.Hal.Manager',"
                            "sender='org.freedesktop.Hal',"
                            "path='/org/freedesktop/Hal/Manager'",
                            &dbusError.get());
        halSuccess = !dbusError.IsSet();
    }
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    if (halSuccess)
        *pConnection = dbusConnection.release();
    LogFlowFunc(("rc=%Rrc, (*pConnection).get()=%p\n", rc, (*pConnection).get()));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function for setting up a private connection to the DBus daemon and
 * registering with the hal service.  Private connections are considered
 * unsociable and should not be used unnecessarily (as per the DBus API docs).
 *
 * @note If libdbus is being loaded at runtime then be sure to call
 *       VBoxDBusCheckPresence before calling this.
 * @returns iprt status code
 * @param   pConnection  where to store the connection handle
 */
/* static */
int halInitPrivate (RTMemAutoPtr <DBusConnection, VBoxHalShutdownPrivate> *pConnection)
{
    AssertReturn(VALID_PTR (pConnection), VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p\n", pConnection));
    int rc = VINF_SUCCESS;
    bool halSuccess = true;
    autoDBusError dbusError;

    RTMemAutoPtr <DBusConnection, VBoxDBusConnectionCloseAndUnref> dbusConnection;
    dbusConnection = dbus_bus_get_private (DBUS_BUS_SYSTEM, &dbusError.get());
    if (!dbusConnection)
        halSuccess = false;
    if (halSuccess)
    {
        dbus_connection_set_exit_on_disconnect (dbusConnection.get(), false);
        halSuccess = dbus_bus_name_has_owner (dbusConnection.get(),
                                              "org.freedesktop.Hal", &dbusError.get());
    }
    if (halSuccess)
    {
        dbus_bus_add_match (dbusConnection.get(),
                            "type='signal',"
                            "interface='org.freedesktop.Hal.Manager',"
                            "sender='org.freedesktop.Hal',"
                            "path='/org/freedesktop/Hal/Manager'",
                            &dbusError.get());
        halSuccess = !dbusError.IsSet();
    }
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    if (halSuccess)
        *pConnection = dbusConnection.release();
    LogFlowFunc(("rc=%Rrc, (*pConnection).get()=%p\n", rc, (*pConnection).get()));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function for shutting down a connection to DBus and hal.
 * @param   pConnection  the connection handle
 */
/* extern */
void VBoxHalShutdown (DBusConnection *pConnection)
{
    AssertReturnVoid(VALID_PTR (pConnection));
    LogFlowFunc (("pConnection=%p\n", pConnection));
    autoDBusError dbusError;

    dbus_bus_remove_match (pConnection,
                           "type='signal',"
                           "interface='org.freedesktop.Hal.Manager',"
                           "sender='org.freedesktop.Hal',"
                           "path='/org/freedesktop/Hal/Manager'",
                           &dbusError.get());
    dbus_connection_unref (pConnection);
    LogFlowFunc(("returning\n"));
    dbusError.FlowLog();
}

/**
 * Helper function for shutting down a private connection to DBus and hal.
 * @param   pConnection  the connection handle
 */
/* extern */
void VBoxHalShutdownPrivate (DBusConnection *pConnection)
{
    AssertReturnVoid(VALID_PTR (pConnection));
    LogFlowFunc (("pConnection=%p\n", pConnection));
    autoDBusError dbusError;

    dbus_bus_remove_match (pConnection,
                           "type='signal',"
                           "interface='org.freedesktop.Hal.Manager',"
                           "sender='org.freedesktop.Hal',"
                           "path='/org/freedesktop/Hal/Manager'",
                           &dbusError.get());
    dbus_connection_close (pConnection);
    dbus_connection_unref (pConnection);
    LogFlowFunc(("returning\n"));
    dbusError.FlowLog();
}

/** Wrapper around dbus_connection_unref.  We need this to use it as a real
 * function in auto pointers, as a function pointer won't wash here. */
/* extern */
void VBoxDBusConnectionUnref(DBusConnection *pConnection)
{
    dbus_connection_unref(pConnection);
}

/**
 * This function closes and unrefs a private connection to dbus.  It should
 * only be called once no-one else is referencing the connection.
 */
/* extern */
void VBoxDBusConnectionCloseAndUnref(DBusConnection *pConnection)
{
    dbus_connection_close(pConnection);
    dbus_connection_unref(pConnection);
}

/** Wrapper around dbus_message_unref.  We need this to use it as a real
 * function in auto pointers, as a function pointer won't wash here. */
/* extern */
void VBoxDBusMessageUnref(DBusMessage *pMessage)
{
    dbus_message_unref(pMessage);
}

/**
 * Find the UDIs of hal entries that contain Key=Value property.
 * @returns iprt status code.  If a non-fatal error occurs, we return success
 *          but reset pMessage to NULL.
 * @param   pConnection an initialised connection DBus
 * @param   pszKey      the property key
 * @param   pszValue    the property value
 * @param   pMessage    where to store the return DBus message.  This must be
 *                      parsed to get at the UDIs.  NOT optional.
 */
/* static */
int halFindDeviceStringMatch (DBusConnection *pConnection, const char *pszKey,
                              const char *pszValue,
                              RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> *pMessage)
{
    AssertReturn(   VALID_PTR (pConnection) && VALID_PTR (pszKey)
                 && VALID_PTR (pszValue) && VALID_PTR (pMessage),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszKey=%s, pszValue=%s, pMessage=%p\n",
                  pConnection, pszKey, pszValue, pMessage));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, reply;
    if (halSuccess && RT_SUCCESS(rc))
    {
        message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                                "/org/freedesktop/Hal/Manager",
                                                "org.freedesktop.Hal.Manager",
                                                "FindDeviceStringMatch");
        if (!message)
            rc = VERR_NO_MEMORY;
    }
    if (halSuccess && RT_SUCCESS(rc))
    {
        DBusMessageIter iterAppend;
        dbus_message_iter_init_append (message.get(), &iterAppend);
        dbus_message_iter_append_basic (&iterAppend, DBUS_TYPE_STRING, &pszKey);
        dbus_message_iter_append_basic (&iterAppend, DBUS_TYPE_STRING, &pszValue);
        reply = dbus_connection_send_with_reply_and_block (pConnection,
                                                            message.get(), -1,
                                                            &dbusError.get());
        if (!reply)
            halSuccess = false;
    }
    *pMessage = reply.release ();
    LogFlowFunc (("rc=%Rrc, *pMessage.value()=%p\n", rc, (*pMessage).get()));
    dbusError.FlowLog();
    return rc;
}

/**
 * Find the UDIs of hal entries that contain Key=Value property and return the
 * result on the end of a vector of iprt::MiniString.
 * @returns iprt status code.  If a non-fatal error occurs, we return success
 *          but set *pfSuccess to false.
 * @param   pConnection an initialised connection DBus
 * @param   pszKey      the property key
 * @param   pszValue    the property value
 * @param   pMatches    pointer to an array of iprt::MiniString to append the
 *                      results to.  NOT optional.
 * @param   pfSuccess   will be set to true if the operation succeeds
 */
/* static */
int halFindDeviceStringMatchVector (DBusConnection *pConnection,
                                    const char *pszKey, const char *pszValue,
                                    std::vector<iprt::MiniString> *pMatches,
                                    bool *pfSuccess)
{
    AssertPtrReturn (pConnection, VERR_INVALID_POINTER);
    AssertPtrReturn (pszKey, VERR_INVALID_POINTER);
    AssertPtrReturn (pszValue, VERR_INVALID_POINTER);
    AssertPtrReturn (pMatches, VERR_INVALID_POINTER);
    AssertReturn(pfSuccess == NULL || VALID_PTR (pfSuccess), VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszKey=%s, pszValue=%s, pMatches=%p, pfSuccess=%p\n",
                  pConnection, pszKey, pszValue, pMatches, pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind;
    DBusMessageIter iterFind, iterUdis;

    if (halSuccess && RT_SUCCESS(rc))
    {
        rc = halFindDeviceStringMatch (pConnection, pszKey, pszValue,
                                       &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS(rc)
           && dbus_message_iter_get_arg_type (&iterUdis) == DBUS_TYPE_STRING;
         dbus_message_iter_next(&iterUdis))
    {
        /* Now get all UDIs from the iterator */
        const char *pszUdi;
        dbus_message_iter_get_basic (&iterUdis, &pszUdi);
        try
        {
            pMatches->push_back(pszUdi);
        }
        catch(std::bad_alloc &e)
        {
            rc = VERR_NO_MEMORY;
        }
    }
    if (pfSuccess != NULL)
        *pfSuccess = halSuccess;
    LogFlow (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    return rc;
}

/**
 * Read a set of string properties for a device.  If some of the properties are
 * not of type DBUS_TYPE_STRING or do not exist then a NULL pointer will be
 * returned for them.
 * @returns iprt status code.  If the operation failed for non-fatal reasons
 *          then we return success and leave pMessage untouched - reset it
 *          before the call to detect this.
 * @param   pConnection  an initialised connection DBus
 * @param   pszUdi       the Udi of the device
 * @param   cProps       the number of property values to look up
 * @param   papszKeys    the keys of the properties to be looked up
 * @param   papszValues  where to store the values of the properties.  The
 *                       strings returned will be valid until the message
 *                       returned in @a ppMessage is freed.  Undefined if
 *                       the message is NULL.
 * @param   pMessage     where to store the return DBus message.  The caller
 *                       is responsible for freeing this once they have
 *                       finished with the value strings.  NOT optional.
 */
/* static */
int halGetPropertyStrings (DBusConnection *pConnection, const char *pszUdi,
                           size_t cProps, const char **papszKeys,
                           char **papszValues,
                           RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> *pMessage)
{
    AssertReturn(   VALID_PTR (pConnection) && VALID_PTR (pszUdi)
                 && VALID_PTR (papszKeys) && VALID_PTR (papszValues)
                 && VALID_PTR (pMessage),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszUdi=%s, cProps=%llu, papszKeys=%p, papszValues=%p, pMessage=%p\n",
                  pConnection, pszUdi, cProps, papszKeys, papszValues, pMessage));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, reply;
    DBusMessageIter iterGet, iterProps;

    /* Initialise the return array to NULLs */
    for (size_t i = 0; i < cProps; ++i)
        papszValues[i] = NULL;

    /* Send a GetAllProperties message to hald */
    message = dbus_message_new_method_call ("org.freedesktop.Hal", pszUdi,
                                            "org.freedesktop.Hal.Device",
                                            "GetAllProperties");
    if (!message)
        rc = VERR_NO_MEMORY;
    if (halSuccess && RT_SUCCESS(rc))
    {
        reply = dbus_connection_send_with_reply_and_block (pConnection,
                                                           message.get(), -1,
                                                           &dbusError.get());
        if (!reply)
            halSuccess = false;
    }

    /* Parse the reply */
    if (halSuccess && RT_SUCCESS(rc))
    {
        dbus_message_iter_init (reply.get(), &iterGet);
        if (   dbus_message_iter_get_arg_type (&iterGet) != DBUS_TYPE_ARRAY
            && dbus_message_iter_get_element_type (&iterGet) != DBUS_TYPE_DICT_ENTRY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
        dbus_message_iter_recurse (&iterGet, &iterProps);
    /* Go through all entries in the reply and see if any match our keys. */
    while (   halSuccess && RT_SUCCESS(rc)
           &&    dbus_message_iter_get_arg_type (&iterProps)
              == DBUS_TYPE_DICT_ENTRY)
    {
        const char *pszKey;
        DBusMessageIter iterEntry, iterValue;
        dbus_message_iter_recurse (&iterProps, &iterEntry);
        dbus_message_iter_get_basic (&iterEntry, &pszKey);
        dbus_message_iter_next (&iterEntry);
        dbus_message_iter_recurse (&iterEntry, &iterValue);
        /* Fill in any matches. */
        for (size_t i = 0; i < cProps; ++i)
            if (strcmp (pszKey, papszKeys[i]) == 0)
            {
                if (dbus_message_iter_get_arg_type (&iterValue) == DBUS_TYPE_STRING)
                    dbus_message_iter_get_basic (&iterValue, &papszValues[i]);
            }
        dbus_message_iter_next (&iterProps);
    }
    if (RT_SUCCESS(rc) && halSuccess)
        *pMessage = reply.release();
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    LogFlowFunc (("rc=%Rrc, *pMessage.value()=%p\n", rc, (*pMessage).get()));
    dbusError.FlowLog();
    return rc;
}

/**
 * Read a set of string properties for a device.  If some properties do not
 * exist or are not of type DBUS_TYPE_STRING, we will still fetch the others.
 * @returns iprt status code.  If the operation failed for non-fatal reasons
 *          then we return success and set *pfSuccess to false.
 * @param   pConnection  an initialised connection DBus
 * @param   pszUdi       the Udi of the device
 * @param   cProps       the number of property values to look up
 * @param   papszKeys    the keys of the properties to be looked up
 * @param   pMatches     pointer to an empty array of iprt::MiniString to append the
 *                       results to.  NOT optional.
 * @param   pfMatches    pointer to an array of boolean values indicating
 *                       whether the respective property is a string.  If this
 *                       is not supplied then all properties must be strings
 *                       for the operation to be considered successful
 * @param   pfSuccess    will be set to true if the operation succeeds
 */
/* static */
int halGetPropertyStringsVector (DBusConnection *pConnection,
                                 const char *pszUdi, size_t cProps,
                                 const char **papszKeys,
                                 std::vector<iprt::MiniString> *pMatches,
                                 bool *pfMatches, bool *pfSuccess)
{
    AssertPtrReturn (pConnection, VERR_INVALID_POINTER);
    AssertPtrReturn (pszUdi, VERR_INVALID_POINTER);
    AssertPtrReturn (papszKeys, VERR_INVALID_POINTER);
    AssertPtrReturn (pMatches, VERR_INVALID_POINTER);
    AssertReturn((pfMatches == NULL) || VALID_PTR (pfMatches), VERR_INVALID_POINTER);
    AssertReturn((pfSuccess == NULL) || VALID_PTR (pfSuccess), VERR_INVALID_POINTER);
    AssertReturn(pMatches->empty(), VERR_INVALID_PARAMETER);
    LogFlowFunc (("pConnection=%p, pszUdi=%s, cProps=%llu, papszKeys=%p, pMatches=%p, pfMatches=%p, pfSuccess=%p\n",
                  pConnection, pszUdi, cProps, papszKeys, pMatches, pfMatches, pfSuccess));
    RTMemAutoPtr <char *> values(cProps);
    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message;
    bool halSuccess = true;
    int rc = halGetPropertyStrings (pConnection, pszUdi, cProps, papszKeys,
                                    values.get(), &message);
    if (!message)
        halSuccess = false;
    for (size_t i = 0; RT_SUCCESS(rc) && halSuccess && i < cProps; ++i)
    {
        bool fMatches = values[i] != NULL;
        if (pfMatches != NULL)
            pfMatches[i] = fMatches;
        else
            halSuccess = fMatches;
        try
        {
            pMatches->push_back(fMatches ? values[i] : "");
        }
        catch(std::bad_alloc &e)
        {
            rc = VERR_NO_MEMORY;
        }
    }
    if (pfSuccess != NULL)
        *pfSuccess = halSuccess;
    if (RT_SUCCESS(rc) && halSuccess)
    {
        Assert (pMatches->size() == cProps);
        AssertForEach (j, size_t, 0, cProps,    (pfMatches == NULL)
                                             || (pfMatches[j] == true)
                                             || ((pfMatches[j] == false) && (pMatches[j].size() == 0)));
    }
    LogFlowFunc (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    return rc;
}

/**
 * Send an SCSI INQUIRY command to a device and return selected information.
 * @returns  iprt status code
 * @returns  VERR_TRY_AGAIN if the query failed but might succeed next time
 * @param pszNode    the full path to the device node
 * @param pu8Type    where to store the SCSI device type on success (optional)
 * @param pchVendor  where to store the vendor id string on success (optional)
 * @param cchVendor  the size of the @a pchVendor buffer
 * @param pchModel   where to store the product id string on success (optional)
 * @param cchModel   the size of the @a pchModel buffer
 * @note check documentation on the SCSI INQUIRY command and the Linux kernel
 *       SCSI headers included above if you want to understand what is going
 *       on in this method.
 */
/* static */
int scsiDoInquiry(const char *pszNode, uint8_t *pu8Type, char *pchVendor,
                  size_t cchVendor, char *pchModel, size_t cchModel)
{
    LogRelFlowFunc(("pszNode=%s, pu8Type=%p, pchVendor=%p, cchVendor=%llu, pchModel=%p, cchModel=%llu\n",
                    pszNode, pu8Type, pchVendor, cchVendor, pchModel,
                    cchModel));
    AssertPtrReturn(pszNode, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pu8Type, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pchVendor, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pchModel, VERR_INVALID_POINTER);

    sg_io_hdr_t ScsiIoReq = {0};
    unsigned char u8Response[96] = { 0 };
    unsigned char u8Command[6] =
        { INQUIRY, 0, 0, 0, sizeof(u8Response), 0 };  /* INQUIRY */
    int rc, rcIoCtl = 0;
    RTFILE file;
    rc = RTFileOpen(&file, pszNode, RTFILE_O_READ);
    if (RT_SUCCESS(rc))
    {
        ScsiIoReq.interface_id = 'S';
        ScsiIoReq.dxfer_direction = SG_DXFER_FROM_DEV;
        ScsiIoReq.cmd_len = sizeof(u8Command);
        ScsiIoReq.dxfer_len = sizeof(u8Response);
        ScsiIoReq.dxferp = u8Response;
        ScsiIoReq.cmdp = u8Command;
        ScsiIoReq.timeout = 5000 /* ms */;
        rc = RTFileIoCtl(file, SG_IO, &ScsiIoReq, 0, &rcIoCtl);
        if (RT_SUCCESS(rc) && rcIoCtl < 0)
            rc = VERR_NOT_SUPPORTED;
        RTFileClose(file);
    }
    if (RT_SUCCESS(rc))
    {
        if (   (ScsiIoReq.status >> 1 == GOOD)
            || (ScsiIoReq.status >> 1 == INTERMEDIATE_GOOD)
            || (ScsiIoReq.status >> 1 == INTERMEDIATE_C_GOOD)
            || (ScsiIoReq.status >> 1 == COMMAND_TERMINATED))
        {
            if (pu8Type)
                *pu8Type = u8Response[0] & 0x1f;
            if (pchVendor)
                RTStrPrintf(pchVendor, cchVendor, "%.8s",
                            (char *) &u8Response[8] /* vendor id string */);
            if (pchModel)
                RTStrPrintf(pchModel, cchModel, "%.16s",
                            (char *) &u8Response[16] /* product id string */);
        }
        else if (   ScsiIoReq.status >> 1 != BUSY
                 && ScsiIoReq.status >> 1 != QUEUE_FULL
                 && ScsiIoReq.status >> 1 != CHECK_CONDITION)
            rc = VERR_DEV_IO_ERROR;  /* Actually, these should never happen */
        else
            rc = VERR_TRY_AGAIN;
    }
    LogRelFlowFunc(("returning %Rrc\n", rc));
    if (RT_SUCCESS(rc))
        LogRelFlowFunc(("    type=%u, vendor=%.8s, product=%.16s\n",
                        u8Response[0] & 0x1f, (char *) &u8Response[8],
                        (char *) &u8Response[16]));
    return rc;
}

class sysfsBlockDev
{
public:
    sysfsBlockDev(const char *pcszName, bool wantDVD)
            : mpcszName(pcszName), mwantDVD(wantDVD), misValid(false)
    {
        if (findDeviceNode())
        {
            if (mwantDVD)
                validateAndInitForDVD();
            else
                validateAndInitForFloppy();
        }
    }
private:
    /** The name of the subdirectory of /sys/block for this device */
    const char *mpcszName;
    /** Are we looking for a floppy or a DVD device? */
    bool mwantDVD;
    /** The device node for the device */
    char mszNode[RTPATH_MAX];
    /** Is this entry a valid specimen of what we are looking for? */
    bool misValid;
    /** Human readible drive description string */
    char mszDesc[256];
    /** Unique identifier for the drive.  Should be identical to hal's UDI for
     * the device.  May not be unique for two identical drives. */
    char mszUdi[256];
private:
    /* Private methods */

    /**
     * Fill in the device node member based on the /sys/block subdirectory.
     * @returns boolean success value
     */
    bool findDeviceNode()
    {
        dev_t dev = RTLinuxSysFsReadDevNumFile("block/%s/dev", mpcszName);
        if (dev == 0)
            return false;
        if (RTLinuxFindDevicePath(dev, RTFS_TYPE_DEV_BLOCK, mszNode,
                                  sizeof(mszNode), "%s", mpcszName) < 0)
            return false;
        return true;
    }

    /** Check whether the sysfs block entry is valid for a DVD device and
     * initialise the string data members for the object.  We try to get all
     * the information we need from sysfs if possible, to avoid unnecessarily
     * poking the device, and if that fails we fall back to an SCSI INQUIRY
     * command. */
    void validateAndInitForDVD()
    {
        char szVendor[128], szModel[128];
        ssize_t cchVendor, cchModel;
        int64_t type = RTLinuxSysFsReadIntFile(10, "block/%s/device/type",
                                               mpcszName);
        if (type >= 0 && type != TYPE_ROM)
            return;
        if (type == 5)
        {
            cchVendor = RTLinuxSysFsReadStrFile(szVendor, sizeof(szVendor),
                                                "block/%s/device/vendor",
                                                mpcszName);
            if (cchVendor >= 0)
            {
                cchModel = RTLinuxSysFsReadStrFile(szModel, sizeof(szModel),
                                                   "block/%s/device/model",
                                                   mpcszName);
                if (cchModel >= 0)
                {
                    misValid = true;
                    setDeviceStrings(szVendor, szModel);
                    return;
                }
            }
        }
        probeAndInitForDVD();
    }

    /** Try to find out whether a device is a DVD drive by sending it an
     * SCSI INQUIRY command.  If it is, initialise the string and validity
     * data members for the object based on the returned data.
     */
    void probeAndInitForDVD()
    {
        AssertReturnVoid(mszNode[0] != '\0');
        uint8_t u8Type = 0;
        char szVendor[128] = "";
        char szModel[128] = "";
        for (unsigned i = 0; i < 5; ++i)  /* Give the device five chances */
        {
            int rc = scsiDoInquiry(mszNode, &u8Type, szVendor,
                                   sizeof(szVendor), szModel,
                                   sizeof(szModel));
            if (RT_SUCCESS(rc))
            {
                if (u8Type != TYPE_ROM)
                    return;
                misValid = true;
                setDeviceStrings(szVendor, szModel);
                return;
            }
            if (rc != VERR_TRY_AGAIN)
                return;
            RTThreadSleep(100);  /* wait a little before retrying */
        }
    }

    /**
     * Initialise the object device strings (description and UDI) based on
     * vendor and model name strings.
     * @param pszVendor  the vendor ID string
     * @param pszModel   the product ID string
     */
    void setDeviceStrings(const char *pszVendor, const char *pszModel)
    {
        char szCleaned[128];
        size_t cchVendor = strLenStripped(pszVendor);
        size_t cchModel = strLenStripped(pszModel);

        /* Create a cleaned version of the model string for the UDI string. */
        for (unsigned i = 0; pszModel[i] != '\0' && i < sizeof(szCleaned); ++i)
            if (   (pszModel[i] >= '0' && pszModel[i] <= '9')
                || (pszModel[i] >= 'A' && pszModel[i] <= 'z'))
                szCleaned[i] = pszModel[i];
            else
                szCleaned[i] = '_';
        szCleaned[RT_MIN(cchModel, sizeof(szCleaned) - 1)] = '\0';

        /* Construct the description string as "Vendor Product" */
        if (cchVendor > 0)
            RTStrPrintf(mszDesc, sizeof(mszDesc), "%.*s %s", cchVendor,
                        pszVendor,
                        cchModel > 0 ? pszModel : "(unknown drive model)");
        else
            RTStrPrintf(mszDesc, sizeof(mszDesc), "%s", pszModel);
        /* Construct the UDI string */
        if (cchModel)
            RTStrPrintf(mszUdi, sizeof(mszUdi),
                        "/org/freedesktop/Hal/devices/storage_model_%s",
                        szCleaned);
        else
            mszUdi[0] = '\0';
    }

    /** Check whether the sysfs block entry is valid for a floppy device and
     * initialise the string data members for the object.  Since we only
     * support floppies using the basic "floppy" driver, we just check the
     * entry name and the bus type ("platform"). */
    void validateAndInitForFloppy()
    {
        floppy_drive_name szName;
        int rcIoCtl;
        if (   mpcszName[0] != 'f'
            || mpcszName[1] != 'd'
            || mpcszName[2] < '0'
            || mpcszName[2] > '3'
            || mpcszName[3] != '\0')
            return;
        RTFILE file;
        int rc = RTFileOpen(&file, mszNode, RTFILE_O_READ);
        /** @note the next line can produce a warning, as the ioctl request
         * field is defined as signed, but the Linux ioctl definition macros
         * produce unsigned constants. */
        rc = RTFileIoCtl(file, FDGETDRVTYP, szName, 0, &rcIoCtl);
        RTFileClose(file);
        if (rcIoCtl < 0)
            return;
        misValid = true;
        strcpy(mszDesc,   (mpcszName[2] == '0') ? "PC Floppy drive"
                        : (mpcszName[2] == '1') ? "Second PC Floppy drive"
                        : (mpcszName[2] == '2') ? "Third PC Floppy drive"
                        : "Fourth PC Floppy drive");
        RTStrPrintf(mszUdi, sizeof(mszUdi),
                    "/org/freedesktop/Hal/devices/platform_floppy_%u_storage",
                    mpcszName[2]);
    }

public:
    bool isValid()
    {
        return misValid;
    }
    const char *getDesc()
    {
        return mszDesc;
    }
    const char *getUdi()
    {
        return mszUdi;
    }
    const char *getNode()
    {
        return mszNode;
    }
};

/**
 * Helper function to query the sysfs subsystem for information about DVD
 * drives attached to the system.
 * @returns iprt status code
 * @param   pList      where to add information about the drives detected
 * @param   isDVD      are we looking for DVDs or floppies?
 * @param   pfSuccess  Did we find anything?
 *
 * @returns IPRT status code
 */
/* static */
int getDriveInfoFromSysfs(DriveInfoList *pList, bool isDVD, bool *pfSuccess)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfSuccess, VERR_INVALID_POINTER); /* Valid or Null */
    LogFlowFunc (("pList=%p, isDVD=%u, pfSuccess=%p\n",
                  pList, (unsigned) isDVD, pfSuccess));
    PRTDIR pDir = NULL;
    RTDIRENTRY entry = {0};
    int rc;
    bool fSuccess;
    unsigned cFound = 0;

    rc = RTDirOpen(&pDir, "/sys/block");
    if (RT_SUCCESS(rc))
        while (true)
        {
            rc = RTDirRead(pDir, &entry, NULL);
            Assert(rc != VERR_BUFFER_OVERFLOW);  /* Should never happen... */
            if (RT_FAILURE(rc))  /* Including overflow and no more files */
                break;
            if (entry.szName[0] == '.')
                continue;
            sysfsBlockDev dev(entry.szName, isDVD);
            if (!dev.isValid())
                continue;
            try
            {
                pList->push_back(DriveInfo(dev.getNode(), dev.getUdi(),
                                           dev.getDesc()));
            }
            catch(std::bad_alloc &e)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            ++cFound;
        }
    RTDirClose(pDir);
    if (rc == VERR_NO_MORE_FILES)
        rc = VINF_SUCCESS;
    fSuccess = (RT_SUCCESS(rc) && cFound > 0);
    if (!fSuccess)
        /* Clean up again */
        for (unsigned i = 0; i < cFound; ++i)
            pList->pop_back();
    if (pfSuccess)
        *pfSuccess = fSuccess;
    LogFlow (("rc=%Rrc, fSuccess=%u\n", rc, (unsigned) fSuccess));
    return rc;
}

/**
 * Helper function to query the hal subsystem for information about drives
 * attached to the system.
 * @returns iprt status code
 * @param   pList      where to add information about the drives detected
 * @param   isDVD      are we looking for DVDs or floppies?
 * @param   pfSuccess  will be set to true if all interactions with hal
 *                     succeeded and to false otherwise.  Optional.
 *
 * @returns IPRT status code
 */
/* static */
int getDriveInfoFromHal(DriveInfoList *pList, bool isDVD, bool *pfSuccess)
{
    AssertReturn(VALID_PTR (pList) && (pfSuccess == NULL || VALID_PTR (pfSuccess)),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pList=%p, isDVD=%d, pfSuccess=%p\n", pList, isDVD, pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind, replyGet;
    RTMemAutoPtr <DBusConnection, VBoxHalShutdown> dbusConnection;
    DBusMessageIter iterFind, iterUdis;

    try
    {
        rc = halInit (&dbusConnection);
        if (!dbusConnection)
            halSuccess = false;
        if (halSuccess && RT_SUCCESS(rc))
        {
            rc = halFindDeviceStringMatch (dbusConnection.get(), "storage.drive_type",
                                        isDVD ? "cdrom" : "floppy", &replyFind);
            if (!replyFind)
                halSuccess = false;
        }
        if (halSuccess && RT_SUCCESS(rc))
        {
            dbus_message_iter_init (replyFind.get(), &iterFind);
            if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
                halSuccess = false;
        }
        if (halSuccess && RT_SUCCESS(rc))
            dbus_message_iter_recurse (&iterFind, &iterUdis);
        for (;    halSuccess && RT_SUCCESS(rc)
            && dbus_message_iter_get_arg_type (&iterUdis) == DBUS_TYPE_STRING;
            dbus_message_iter_next(&iterUdis))
        {
            /* Now get all properties from the iterator */
            const char *pszUdi;
            dbus_message_iter_get_basic (&iterUdis, &pszUdi);
            static const char *papszKeys[] =
                    { "block.device", "info.product", "info.vendor" };
            char *papszValues[RT_ELEMENTS (papszKeys)];
            rc = halGetPropertyStrings (dbusConnection.get(), pszUdi, RT_ELEMENTS (papszKeys),
                                        papszKeys, papszValues, &replyGet);
            iprt::MiniString description;
            const char *pszDevice = papszValues[0], *pszProduct = papszValues[1],
                    *pszVendor = papszValues[2];
            if (!!replyGet && pszDevice == NULL)
                halSuccess = false;
            if (!!replyGet && pszDevice != NULL)
            {
                if ((pszVendor != NULL) && (pszVendor[0] != '\0'))
                {
                    description.append(pszVendor);
                    description.append(" ");
                }
                if ((pszProduct != NULL && pszProduct[0] != '\0'))
                    description.append(pszProduct);
                pList->push_back (DriveInfo (pszDevice, pszUdi, description));
            }
        }
        if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
            rc = VERR_NO_MEMORY;
        /* If we found nothing something may have gone wrong with hal, so
         * report failure to fall back to other methods. */
        if (pList->size() == 0)
            halSuccess = false;
        if (pfSuccess != NULL)
            *pfSuccess = halSuccess;
    }
    catch(std::bad_alloc &e)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlow (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function to query the hal subsystem for information about USB devices
 * attached to the system.
 * @returns iprt status code
 * @param   pList      where to add information about the devices detected
 * @param   pfSuccess  will be set to true if all interactions with hal
 *                     succeeded and to false otherwise.  Optional.
 *
 * @returns IPRT status code
 */
/* static */
int getUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess)
{
    AssertReturn(VALID_PTR (pList) && (pfSuccess == NULL || VALID_PTR (pfSuccess)),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pList=%p, pfSuccess=%p\n", pList, pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind, replyGet;
    RTMemAutoPtr <DBusConnection, VBoxHalShutdown> dbusConnection;
    DBusMessageIter iterFind, iterUdis;

    /* Connect to hal */
    rc = halInit (&dbusConnection);
    if (!dbusConnection)
        halSuccess = false;
    /* Get an array of all devices in the usb_device subsystem */
    if (halSuccess && RT_SUCCESS(rc))
    {
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.subsystem",
                                       "usb_device", &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    /* Recurse down into the array and query interesting information about the
     * entries. */
    if (halSuccess && RT_SUCCESS(rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS(rc)
           && dbus_message_iter_get_arg_type (&iterUdis) == DBUS_TYPE_STRING;
         dbus_message_iter_next(&iterUdis))
    {
        /* Get the device node and the sysfs path for the current entry. */
        const char *pszUdi;
        dbus_message_iter_get_basic (&iterUdis, &pszUdi);
        static const char *papszKeys[] = { "linux.device_file", "linux.sysfs_path" };
        char *papszValues[RT_ELEMENTS (papszKeys)];
        rc = halGetPropertyStrings (dbusConnection.get(), pszUdi, RT_ELEMENTS (papszKeys),
                                    papszKeys, papszValues, &replyGet);
        const char *pszDevice = papszValues[0], *pszSysfsPath = papszValues[1];
        /* Get the interfaces. */
        if (!!replyGet && pszDevice && pszSysfsPath)
        {
            USBDeviceInfo info (pszDevice, pszSysfsPath);
            bool ifaceSuccess = true;  /* If we can't get the interfaces, just
                                        * skip this one device. */
            rc = getUSBInterfacesFromHal (&info.mInterfaces, pszUdi, &ifaceSuccess);
            if (RT_SUCCESS(rc) && halSuccess && ifaceSuccess)
                try
                {
                    pList->push_back (info);
                }
                catch(std::bad_alloc &e)
                {
                    rc = VERR_NO_MEMORY;
                }
        }
    }
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    if (pfSuccess != NULL)
        *pfSuccess = halSuccess;
    LogFlow (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function to query the hal subsystem for information about USB devices
 * attached to the system, using the older API.
 * @returns iprt status code
 * @param   pList      where to add information about the devices detected
 * @param   pfSuccess  will be set to true if all interactions with hal
 *                     succeeded and to false otherwise.  Optional.
 *
 * @returns IPRT status code
 */
/* static */
int getOldUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess)
{
    AssertReturn(VALID_PTR (pList) && (pfSuccess == NULL || VALID_PTR (pfSuccess)),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pList=%p, pfSuccess=%p\n", pList, pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind, replyGet;
    RTMemAutoPtr <DBusConnection, VBoxHalShutdown> dbusConnection;
    DBusMessageIter iterFind, iterUdis;

    /* Connect to hal */
    rc = halInit (&dbusConnection);
    if (!dbusConnection)
        halSuccess = false;
    /* Get an array of all devices in the usb_device subsystem */
    if (halSuccess && RT_SUCCESS(rc))
    {
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.category",
                                       "usbraw", &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    /* Recurse down into the array and query interesting information about the
     * entries. */
    if (halSuccess && RT_SUCCESS(rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS(rc)
           && dbus_message_iter_get_arg_type (&iterUdis) == DBUS_TYPE_STRING;
         dbus_message_iter_next(&iterUdis))
    {
        /* Get the device node and the sysfs path for the current entry. */
        const char *pszUdi;
        dbus_message_iter_get_basic (&iterUdis, &pszUdi);
        static const char *papszKeys[] = { "linux.device_file", "info.parent" };
        char *papszValues[RT_ELEMENTS (papszKeys)];
        rc = halGetPropertyStrings (dbusConnection.get(), pszUdi, RT_ELEMENTS (papszKeys),
                                    papszKeys, papszValues, &replyGet);
        const char *pszDevice = papszValues[0], *pszSysfsPath = papszValues[1];
        /* Get the interfaces. */
        if (!!replyGet && pszDevice && pszSysfsPath)
        {
            USBDeviceInfo info (pszDevice, pszSysfsPath);
            bool ifaceSuccess = false;  /* If we can't get the interfaces, just
                                         * skip this one device. */
            rc = getUSBInterfacesFromHal (&info.mInterfaces, pszSysfsPath,
                                          &ifaceSuccess);
            if (RT_SUCCESS(rc) && halSuccess && ifaceSuccess)
                try
                {
                    pList->push_back (info);
                }
                catch(std::bad_alloc &e)
                {
                    rc = VERR_NO_MEMORY;
                }
        }
    }
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    if (pfSuccess != NULL)
        *pfSuccess = halSuccess;
    LogFlow (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function to query the hal subsystem for information about USB devices
 * attached to the system.
 * @returns iprt status code
 * @param   pList      where to add information about the devices detected.  If
 *                     certain interfaces are not found (@a pfFound is false on
 *                     return) this may contain invalid information.
 * @param   pcszUdi    the hal UDI of the device
 * @param   pfSuccess  will be set to true if the operation succeeds and to
 *                     false if it fails for non-critical reasons.  Optional.
 *
 * @returns IPRT status code
 */
/* static */
int getUSBInterfacesFromHal(std::vector<iprt::MiniString> *pList,
                            const char *pcszUdi, bool *pfSuccess)
{
    AssertReturn(VALID_PTR (pList) && VALID_PTR (pcszUdi) &&
                 (pfSuccess == NULL || VALID_PTR (pfSuccess)),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pList=%p, pcszUdi=%s, pfSuccess=%p\n", pList, pcszUdi,
                  pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */
    autoDBusError dbusError;

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind, replyGet;
    RTMemAutoPtr <DBusConnection, VBoxHalShutdown> dbusConnection;
    DBusMessageIter iterFind, iterUdis;

    rc = halInit (&dbusConnection);
    if (!dbusConnection)
        halSuccess = false;
    if (halSuccess && RT_SUCCESS(rc))
    {
        /* Look for children of the current UDI. */
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.parent",
                                       pcszUdi, &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS(rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS(rc)
           && dbus_message_iter_get_arg_type (&iterUdis) == DBUS_TYPE_STRING;
         dbus_message_iter_next(&iterUdis))
    {
        /* Now get the sysfs path and the subsystem from the iterator */
        const char *pszUdi;
        dbus_message_iter_get_basic (&iterUdis, &pszUdi);
        static const char *papszKeys[] = { "linux.sysfs_path", "info.subsystem",
                                           "linux.subsystem" };
        char *papszValues[RT_ELEMENTS (papszKeys)];
        rc = halGetPropertyStrings (dbusConnection.get(), pszUdi, RT_ELEMENTS (papszKeys),
                                    papszKeys, papszValues, &replyGet);
        const char *pszSysfsPath = papszValues[0], *pszInfoSubsystem = papszValues[1],
                   *pszLinuxSubsystem = papszValues[2];
        if (!replyGet)
            halSuccess = false;
        if (!!replyGet && pszSysfsPath == NULL)
            halSuccess = false;
        if (   halSuccess && RT_SUCCESS(rc)
            && RTStrCmp (pszInfoSubsystem, "usb_device") != 0  /* Children of buses can also be devices. */
            && RTStrCmp (pszLinuxSubsystem, "usb_device") != 0)
            try
            {
                pList->push_back (pszSysfsPath);
            }
            catch(std::bad_alloc &e)
            {
               rc = VERR_NO_MEMORY;
            }
    }
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
        rc = VERR_NO_MEMORY;
    if (pfSuccess != NULL)
        *pfSuccess = halSuccess;
    LogFlow (("rc=%Rrc, halSuccess=%d\n", rc, halSuccess));
    dbusError.FlowLog();
    return rc;
}

/**
 * When it is registered with DBus, this function will be called by
 * dbus_connection_read_write_dispatch each time a message is received over the
 * DBus connection.  We check whether that message was caused by a hal device
 * hotplug event, and if so we set a flag.  dbus_connection_read_write_dispatch
 * will return after calling its filter functions, and its caller should then
 * check the status of the flag passed to the filter function.
 *
 * @param   pConnection The DBus connection we are using.
 * @param   pMessage    The DBus message which just arrived.
 * @param   pvUser      A pointer to the flag variable we are to set.
 */
/* static */
DBusHandlerResult dbusFilterFunction (DBusConnection * /* pConnection */,
                                      DBusMessage *pMessage, void *pvUser)
{
    volatile bool *pTriggered = reinterpret_cast<volatile bool *> (pvUser);
    if (   dbus_message_is_signal (pMessage, "org.freedesktop.Hal.Manager",
                                   "DeviceAdded")
        || dbus_message_is_signal (pMessage, "org.freedesktop.Hal.Manager",
                                   "DeviceRemoved"))
    {
        *pTriggered = true;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif  /* RT_OS_LINUX && VBOX_WITH_DBUS */

