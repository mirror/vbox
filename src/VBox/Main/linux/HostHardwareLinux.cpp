/* $Id$ */
/** @file
 * Classes for handling hardware detection under Linux.  Please feel free to
 * expand these to work for other systems (Solaris!) or to add new ones for
 * other systems.
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
# include <linux/cdrom.h>
# ifdef VBOX_WITH_DBUS
#  include <vbox-dbus.h>
# endif
# include <errno.h>
#endif /* RT_OS_LINUX */

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

bool g_testHostHardwareLinux = false;
static bool testing () { return g_testHostHardwareLinux; }

/*******************************************************************************
*   Defines and Typedefs                                                       *
*******************************************************************************/

typedef VBoxMainDriveInfo::DriveInfoList DriveInfoList;
typedef VBoxMainDriveInfo::DriveInfo DriveInfo;

static bool validateDevice(const char *deviceNode, bool isDVD);
static int getDriveInfoFromEnv(const char *pszVar, DriveInfoList *pList,
                               bool isDVD, bool *pfSuccess);
static int getDVDInfoFromMTab(char *mountTable, DriveInfoList *pList);
#ifdef VBOX_WITH_DBUS
static int halInit(DBusConnection **ppConnection);
/* This must be extern to be used in the RTMemAutoPtr template */
extern void halShutdown (DBusConnection *pConnection);
static int halFindDeviceStringMatch (DBusConnection *pConnection,
                                     const char *pszKey, const char *pszValue,
                                     DBusMessage **ppMessage);
static int halGetPropertyStrings (DBusConnection *pConnection,
                                  const char *pszUdi, size_t cKeys,
                                  const char **papszKeys, char **papszValues,
                                  DBusMessage **ppMessage);
static int getDriveInfoFromHal(DriveInfoList *pList, bool isDVD,
                               bool *pfSuccess);
#endif  /* VBOX_WITH_DBUS */

/**
 * Updates the list of host DVD drives.
 *
 * @returns iprt status code
 */
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
        if (RT_SUCCESS (rc) && VBoxDBusCheckPresence() && (!success || testing()))
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
                mDVDList.push_back (DriveInfo ("/dev/cdrom"));

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

/**
 * Updates the list of host floppy drives.
 *
 * @returns iprt status code
 */
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
        if (RT_SUCCESS (rc) && VBoxDBusCheckPresence() && (!success || testing()))
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
                sprintf(devName, "/dev/fd%d", i);
                if (validateDevice(devName, false))
                    mFloppyList.push_back (DriveInfo (devName));
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
                        pList->push_back (DriveInfo (mnt_dev.get()));
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
/* Linux, load libdbus statically */

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
 * Helper function for setting up a connection to hal
 * @returns iprt status code
 * @param   ppConnection  where to store the connection handle
 */
/* static */
int halInit (DBusConnection **ppConnection)
{
    AssertReturn(VALID_PTR (ppConnection), VERR_INVALID_POINTER);
    LogFlowFunc (("ppConnection=%p\n", ppConnection));
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
    *ppConnection = halSuccess ? dbusConnection.release() : NULL;
    LogFlowFunc(("rc=%Rrc, *ppConnection=%p\n", rc, *ppConnection));
    dbusError.FlowLog();
    return rc;
}

/**
 * Helper function for shutting down a connection to hal
 * @param   pConnection  the connection handle
 */
/* static */
void halShutdown (DBusConnection *pConnection)
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
 * Find the UDIs of hal entries that contain Key=Value property.
 * @returns iprt status code
 * @param   pConnection an initialised connection DBus
 * @param   pszKey      the property key
 * @param   pszValue    the property value
 * @param   ppMessage   where to store the return DBus message.  This must be
 *                      parsed to get at the UDIs.  NOT optional.  The caller
 *                      is responsible for freeing this.
 */
/* static */
int halFindDeviceStringMatch (DBusConnection *pConnection, const char *pszKey,
                              const char *pszValue, DBusMessage **ppMessage)
{
    AssertReturn(   VALID_PTR (pConnection) && VALID_PTR (pszKey)
                 && VALID_PTR (pszValue) && VALID_PTR (ppMessage),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszKey=%s, pszValue=%s, ppMessage=%p\n",
                  pConnection, pszKey, pszValue, ppMessage));
    int rc = VINF_SUCCESS;
    bool halSuccess = true;
    autoDBusError dbusError;
    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message;
    DBusMessage *pReply = NULL;
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
        pReply = dbus_connection_send_with_reply_and_block (pConnection,
                                                            message.get(), -1,
                                                            &dbusError.get());
        if (pReply == NULL)
            halSuccess = false;
    }
    *ppMessage = pReply;
    LogFlowFunc (("rc=%Rrc, *ppMessage=%p\n", rc, *ppMessage));
    dbusError.FlowLog();
    return rc;
}

/**
 * Read a set of string properties for a device.  If some of the properties are
 * not of type DBUS_TYPE_STRING then a NULL pointer will be returned for them.
 * @returns iprt status code
 * @param   pConnection  an initialised connection DBus
 * @param   pszUdi       the Udi of the device
 * @param   cProps       the number of property values to look up
 * @param   papszKeys    the keys of the properties to be looked up
 * @param   papszValues  where to store the values of the properties.  The
 *                       strings returned will be valid until the message
 *                       returned in @a ppMessage is freed.  Undefined if
 *                       the message is NULL.
 * @param   ppMessage    where to store the return DBus message.  The caller
 *                       is responsible for freeing this once they have
 *                       finished with the value strings.  NOT optional.
 */
/* static */
int halGetPropertyStrings (DBusConnection *pConnection, const char *pszUdi,
                           size_t cProps, const char **papszKeys,
                           char **papszValues, DBusMessage **ppMessage)
{
    AssertReturn(   VALID_PTR (pConnection) && VALID_PTR (pszUdi)
                 && VALID_PTR (papszKeys) && VALID_PTR (papszValues)
                 && VALID_PTR (ppMessage),
                 VERR_INVALID_POINTER);
    LogFlowFunc (("pConnection=%p, pszUdi=%s, cProps=%llu, papszKeys=%p, papszValues=%p, ppMessage=%p\n",
                  pConnection, pszUdi, cProps, papszKeys, papszValues, ppMessage));
    int rc = VINF_SUCCESS;
    bool halSuccess = true;
    autoDBusError dbusError;
    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, reply;
    DBusMessageIter iterGet, iterProps, iterKey, iterValue;

    /* Initialise the return array to NULLs */
    for (size_t i = 0; i < cProps; ++i)
        papszValues[i] = NULL;
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
    if (halSuccess && RT_SUCCESS (rc))
    {
        dbus_message_iter_init (reply.get(), &iterGet);
        if (   dbus_message_iter_get_arg_type (&iterGet) != DBUS_TYPE_ARRAY
            && dbus_message_iter_get_element_type (&iterGet) != DBUS_TYPE_DICT_ENTRY)
            halSuccess = false;
    }
    if (halSuccess && RT_SUCCESS (rc))
        dbus_message_iter_recurse (&iterGet, &iterProps);
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
        for (size_t i = 0; i < cProps; ++i)
            if (strcmp (pszKey, papszKeys[i]) == 0)
            {
                if (dbus_message_iter_get_arg_type (&iterValue) == DBUS_TYPE_STRING)
                    dbus_message_iter_get_basic (&iterValue, &papszValues[i]);
            }
        dbus_message_iter_next (&iterProps);
    }
    if (RT_SUCCESS (rc) && halSuccess)
        *ppMessage = reply.release();
    else
        *ppMessage = NULL;
    if (dbusError.HasName (DBUS_ERROR_NO_MEMORY))
            rc = VERR_NO_MEMORY;
    LogFlowFunc (("rc=%Rrc, *ppMessage=%p\n", rc, *ppMessage));
    dbusError.FlowLog();
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
    DBusConnection *pConnection;
    autoDBusError dbusError;
    DBusMessage *pReply;
    RTMemAutoPtr <DBusMessage, VBoxDBusMessageUnref> message, replyFind, replyGet;
    DBusMessageIter iterFind, iterUdis;
    bool halSuccess = true;  /* Did something go wrong with hal or DBus? */
    int rc = VINF_SUCCESS;  /* Did a fatal error occur? */

    rc = halInit (&pConnection);
    RTMemAutoPtr <DBusConnection, halShutdown> dbusConnection;
    dbusConnection = pConnection;
    if (!dbusConnection)
        halSuccess = false;
    if (halSuccess && RT_SUCCESS (rc))
    {
        rc = halFindDeviceStringMatch (pConnection, "storage.drive_type",
                                       isDVD ? "cdrom" : "floppy", &pReply);
        replyFind = pReply;
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
        rc = halGetPropertyStrings (pConnection, pszUdi, RT_ELEMENTS (papszKeys),
                                    papszKeys, papszValues, &pReply);
        replyGet = pReply;
        std::string description;
        const char *pszDevice = papszValues[0], *pszProduct = papszValues[1],
                   *pszVendor = papszValues[2];
        if (!!replyGet && pszDevice)
        {
            if ((pszVendor != NULL) && (pszVendor[0] != '\0'))
                (description += pszVendor) += " ";
            if ((pszProduct != NULL && pszProduct[0] != '\0'))
                description += pszProduct;
            pList->push_back (DriveInfo (pszDevice, pszUdi, description));
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
#endif  /* RT_OS_LINUX && VBOX_WITH_DBUS */

