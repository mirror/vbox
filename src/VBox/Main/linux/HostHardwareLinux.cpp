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

#include <iprt/env.h>
#include <iprt/mem.h>
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
# ifdef VBOX_WITH_DBUS
#  include <vbox-dbus.h>
# endif
# include <errno.h>
#endif /* RT_OS_LINUX */
#include <string>
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
                                           std::vector<std::string> *pMatches);
*/
static int halGetPropertyStrings (DBusConnection *pConnection,
                                  const char *pszUdi, size_t cKeys,
                                  const char **papszKeys, char **papszValues,
                                  RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> *pMessage);
/*
static int halGetPropertyStringsVector (DBusConnection *pConnection,
                                        const char *pszUdi, size_t cProps,
                                        const char **papszKeys,
                                        std::vector<std::string> *pMatches,
                                        bool *pfMatches, bool *pfSuccess);
*/
static int getDriveInfoFromHal(DriveInfoList *pList, bool isDVD,
                               bool *pfSuccess);
static int getUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess);
static int getOldUSBDeviceInfoFromHal(USBDeviceInfoList *pList, bool *pfSuccess);
static int getUSBInterfacesFromHal(std::vector <std::string> *pList,
                                   const char *pcszUdi, bool *pfSuccess);
static DBusHandlerResult dbusFilterFunction (DBusConnection *pConnection,
                                             DBusMessage *pMessage, void *pvUser);
#endif  /* VBOX_WITH_DBUS */

int VBoxMainDriveInfo::updateDVDs ()
{
    LogFlowThisFunc (("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        mDVDList.clear ();
#if defined(RT_OS_LINUX)
#ifdef VBOX_WITH_DBUS
        if (RT_SUCCESS (rc) && RT_SUCCESS(VBoxLoadDBusLib()) && (!success || testing()))
            rc = getDriveInfoFromHal(&mDVDList, true /* isDVD */, &success);
#endif /* VBOX_WITH_DBUS defined */
        // On Linux without hal, the situation is much more complex. We will take a
        // heuristical approach and also allow the user to specify a list of host
        // CDROMs using an environment variable.
        // The general strategy is to try some known device names and see of they
        // exist. At last, we'll enumerate the /etc/fstab file (luckily there's an
        // API to parse it) for CDROM devices. Ok, let's start!
        if (RT_SUCCESS (rc) && (!success || testing()))
            rc = getDriveInfoFromEnv ("VBOX_CDROM", &mDVDList, true /* isDVD */,
                                      &success);
        if (RT_SUCCESS (rc) && (!success || testing()))
        {
            // this is a good guess usually
            if (validateDevice("/dev/cdrom", true))
                try
                {
                    mDVDList.push_back (DriveInfo ("/dev/cdrom"));
                }
                catch (std::bad_alloc)
                {
                    rc = VERR_NO_MEMORY;
                }

            // check the mounted drives
            rc = getDVDInfoFromMTab((char*)"/etc/mtab", &mDVDList);

            // check the drives that can be mounted
            if (RT_SUCCESS (rc))
                rc = getDVDInfoFromMTab((char*)"/etc/fstab", &mDVDList);
        }
#endif
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc (("rc=%Rrc\n", rc));
    return rc;
}

int VBoxMainDriveInfo::updateFloppies ()
{
    LogFlowThisFunc (("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        mFloppyList.clear ();
#if defined(RT_OS_LINUX)
#ifdef VBOX_WITH_DBUS
        if (   RT_SUCCESS (rc)
            && RT_SUCCESS(VBoxLoadDBusLib())
            && (!success || testing()))
            rc = getDriveInfoFromHal(&mFloppyList, false /* isDVD */, &success);
#endif /* VBOX_WITH_DBUS defined */
        // As with the CDROMs, on Linux we have to take a multi-level approach
        // involving parsing the mount tables. As this is not bulletproof, we'll
        // give the user the chance to override the detection by an environment
        // variable and skip the detection.
        if (RT_SUCCESS (rc) && (!success || testing()))
            rc = getDriveInfoFromEnv ("VBOX_FLOPPY", &mFloppyList, false /* isDVD */,
                                      &success);

        if (RT_SUCCESS (rc) && (!success || testing()))
        {
            // we assume that a floppy is always /dev/fd[x] with x from 0 to 7
            char devName[10];
            for (int i = 0; i <= 7; i++)
            {
                RTStrPrintf(devName, sizeof(devName), "/dev/fd%d", i);
                if (validateDevice(devName, false))
                    try
                    {
                        mFloppyList.push_back (DriveInfo (devName));
                    }
                    catch (std::bad_alloc)
                    {
                        rc = VERR_NO_MEMORY;
                    }
            }
        }
#endif
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc (("rc=%Rrc\n", rc));
    return rc;
}

int VBoxMainUSBDeviceInfo::UpdateDevices ()
{
    LogFlowThisFunc (("entered\n"));
    int rc = VINF_SUCCESS;
    bool success = false;  /* Have we succeeded in finding anything yet? */
    try
    {
        bool halSuccess = false;
        mDeviceList.clear();
#if defined(RT_OS_LINUX)
#ifdef VBOX_WITH_DBUS
        if (   RT_SUCCESS (rc)
            && RT_SUCCESS(VBoxLoadDBusLib())
            && (!success || testing()))
            rc = getUSBDeviceInfoFromHal(&mDeviceList, &halSuccess);
        /* Try the old API if the new one *succeeded* as only one of them will
         * pick up devices anyway. */
        if (RT_SUCCESS (rc) && halSuccess && (!success || testing()))
            rc = getOldUSBDeviceInfoFromHal(&mDeviceList, &halSuccess);
        if (!success)
            success = halSuccess;
#endif /* VBOX_WITH_DBUS defined */
#endif /* RT_OS_LINUX */
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }
    LogFlowThisFunc (("rc=%Rrc\n", rc));
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
        while (   RT_SUCCESS (rc)
               && (pMessage = dbus_connection_pop_message (mContext->mConnection.get())) != NULL)
            dbus_message_unref (pMessage); /* empty the message queue. */
        if (   RT_SUCCESS (rc)
            && !dbus_connection_add_filter (mContext->mConnection.get(),
                                            dbusFilterFunction,
                                            (void *) &mContext->mTriggered, NULL))
            rc = VERR_NO_MEMORY;
        if (RT_FAILURE (rc))
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
    while (   RT_SUCCESS (rc) && connected && !mContext->mTriggered
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
    RTMemAutoPtr<char, RTStrFree> drive;
    const char *pszValue = RTEnvGet (pszVar);
    if (pszValue != NULL)
    {
        drive = RTStrDup (pszValue);
        if (!drive)
            rc = VERR_NO_MEMORY;
    }
    if (pszValue != NULL && RT_SUCCESS (rc))
    {
        char *pDrive = drive.get();
        char *pDriveNext = strchr (pDrive, ':');
        while (pDrive != NULL && *pDrive != '\0')
        {
            if (pDriveNext != NULL)
                *pDriveNext = '\0';
            if (validateDevice(pDrive, isDVD))
            {
                try
                {
                    pList->push_back (DriveInfo (pDrive));
                }
                catch (std::bad_alloc)
                {
                    rc = VERR_NO_MEMORY;
                }
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
        struct mntent *mntent;
        RTMemAutoPtr <char, RTStrFree> mnt_type, mnt_dev;
        char *tmp;
        while (RT_SUCCESS (rc) && (mntent = getmntent(mtab)))
        {
            mnt_type = RTStrDup (mntent->mnt_type);
            mnt_dev = RTStrDup (mntent->mnt_fsname);
            if (!mnt_type || !mnt_dev)
                rc = VERR_NO_MEMORY;
            // supermount fs case
            if (RT_SUCCESS (rc) && strcmp(mnt_type.get(), "supermount") == 0)
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
            if (RT_SUCCESS (rc) && strstr(mnt_type.get(), "iso9660") == 0)
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
                        try
                        {
                            pList->push_back (DriveInfo (mnt_dev.get()));
                        }
                        catch (std::bad_alloc)
                        {
                            rc = VERR_NO_MEMORY;
                        }
                }
            }
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                                "/org/freedesktop/Hal/Manager",
                                                "org.freedesktop.Hal.Manager",
                                                "FindDeviceStringMatch");
        if (!message)
            rc = VERR_NO_MEMORY;
    }
    if (halSuccess && RT_SUCCESS (rc))
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
 * result on the end of a vector of std::string.
 * @returns iprt status code.  If a non-fatal error occurs, we return success
 *          but set *pfSuccess to false.
 * @param   pConnection an initialised connection DBus
 * @param   pszKey      the property key
 * @param   pszValue    the property value
 * @param   pMatches    pointer to an array of std::string to append the
 *                      results to.  NOT optional.
 * @param   pfSuccess   will be set to true if the operation succeeds
 */
/* static */
int halFindDeviceStringMatchVector (DBusConnection *pConnection,
                                    const char *pszKey, const char *pszValue,
                                    std::vector<std::string> *pMatches,
                                    bool *pfSuccess)
{
    AssertPtrReturn (pConnection, VERR_INVALID_POINTER);
    AssertPtrReturn (pszKey, VERR_INVALID_POINTER);
    AssertPtrReturn (pszValue, VERR_INVALID_POINTER);
    AssertPtrReturn (pMatches, VERR_INVALID_POINTER);
    AssertReturn (pfSuccess == NULL || VALID_PTR (pfSuccess), VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszKey=%s, pszValue=%s, pMatches=%p, pfSuccess=%p\n",
                  pConnection, pszKey, pszValue, pMatches, pfSuccess));
    int rc = VINF_SUCCESS;  /* We set this to failure on fatal errors. */
    bool halSuccess = true;  /* We set this to false to abort the operation. */

    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind;
    DBusMessageIter iterFind, iterUdis;

    if (halSuccess && RT_SUCCESS (rc))
    {
        rc = halFindDeviceStringMatch (pConnection, pszKey, pszValue,
                                       &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS (rc)
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
        catch (std::bad_alloc)
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        reply = dbus_connection_send_with_reply_and_block (pConnection,
                                                           message.get(), -1,
                                                           &dbusError.get());
        if (!reply)
            halSuccess = false;
    }

    /* Parse the reply */
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (reply.get(), &iterGet);
        if (   dbus_message_iter_get_arg_type (&iterGet) != DBUS_TYPE_ARRAY
            && dbus_message_iter_get_element_type (&iterGet) != DBUS_TYPE_DICT_ENTRY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterGet, &iterProps);
    /* Go through all entries in the reply and see if any match our keys. */
    while (   halSuccess && RT_SUCCESS (rc)
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
    if (RT_SUCCESS (rc) && halSuccess)
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
 * @param   pMatches     pointer to an empty array of std::string to append the
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
                                 std::vector<std::string> *pMatches,
                                 bool *pfMatches, bool *pfSuccess)
{
    AssertPtrReturn (pConnection, VERR_INVALID_POINTER);
    AssertPtrReturn (pszUdi, VERR_INVALID_POINTER);
    AssertPtrReturn (papszKeys, VERR_INVALID_POINTER);
    AssertPtrReturn (pMatches, VERR_INVALID_POINTER);
    AssertReturn ((pfMatches == NULL) || VALID_PTR (pfMatches), VERR_INVALID_POINTER);
    AssertReturn ((pfSuccess == NULL) || VALID_PTR (pfSuccess), VERR_INVALID_POINTER);
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
        catch (std::bad_alloc)
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

    rc = halInit (&dbusConnection);
    if (!dbusConnection)
        halSuccess = false;
    if (halSuccess && RT_SUCCESS (rc))
    {
        rc = halFindDeviceStringMatch (dbusConnection.get(), "storage.drive_type",
                                       isDVD ? "cdrom" : "floppy", &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS (rc)
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
        std::string description;
        const char *pszDevice = papszValues[0], *pszProduct = papszValues[1],
                   *pszVendor = papszValues[2];
        if (!!replyGet && pszDevice == NULL)
            halSuccess = false;
        if (!!replyGet && pszDevice != NULL)
        {
            if ((pszVendor != NULL) && (pszVendor[0] != '\0'))
                (description += pszVendor) += " ";
            if ((pszProduct != NULL && pszProduct[0] != '\0'))
                description += pszProduct;
            try
            {
                pList->push_back (DriveInfo (pszDevice, pszUdi, description));
            }
            catch (std::bad_alloc)
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.subsystem",
                                       "usb_device", &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    /* Recurse down into the array and query interesting information about the
     * entries. */
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS (rc)
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
                catch (std::bad_alloc)
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.category",
                                       "usbraw", &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    /* Recurse down into the array and query interesting information about the
     * entries. */
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS (rc)
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
                catch (std::bad_alloc)
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
int getUSBInterfacesFromHal(std::vector <std::string> *pList,
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        /* Look for children of the current UDI. */
        rc = halFindDeviceStringMatch (dbusConnection.get(), "info.parent",
                                       pcszUdi, &replyFind);
        if (!replyFind)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (replyFind.get(), &iterFind);
        if (dbus_message_iter_get_arg_type (&iterFind) != DBUS_TYPE_ARRAY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterFind, &iterUdis);
    for (;    halSuccess && RT_SUCCESS (rc)
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
        if (   halSuccess && RT_SUCCESS (rc)
            && RTStrCmp (pszInfoSubsystem, "usb_device") != 0  /* Children of buses can also be devices. */
            && RTStrCmp (pszLinuxSubsystem, "usb_device") != 0)
            try
            {
                pList->push_back (pszSysfsPath);
            }
            catch (std::bad_alloc)
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

