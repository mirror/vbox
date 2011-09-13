/** @file
 * X11 guest client - host version check.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <stdio.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#ifdef VBOX_WITH_DBUS
 #include <VBox/dbus.h>
#endif
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

#include "VBoxClient.h"

class HostVersionService : public VBoxClient::Service
{

public:

    virtual const char *getPidFilePath()
    {
        return ".vboxclient-hostversion.pid";
    }

    virtual int showNotify(const char *pcHeader, const char *pcBody)
    {
        int rc;
# ifdef VBOX_WITH_DBUS
        DBusConnection *conn;
        DBusMessage* msg;
        conn = dbus_bus_get (DBUS_BUS_SESSON, NULL);
        if (conn == NULL)
        {
            LogRelFlowFunc(("Could not retrieve D-BUS session bus!\n"));
            rc = VERR_INVALID_HANDLE;
        }
        else
        {
            msg = dbus_message_new_method_call("org.freedesktop.Notifications",
                                               "/org/freedesktop/Notifications",
                                               "org.freedesktop.Notifications",
                                               "Notify");
            if (msg == NULL)
            {
                LogRel(("Could not create D-BUS message!\n"));
                rc = VERR_INVALID_HANDLE;
            }
        else
        rc = VINF_SUCCESS;
        }
        if (RT_SUCCESS(rc))
        {
            uint32_t msg_replace_id = 0;
            const char *msg_app = "VBoxClient";
            const char *msg_icon = "";
            const char *msg_summary = pcHeader;
            const char *msg_body = pcBody;
            int32_t msg_timeout = -1;           /* Let the notification server decide */

            DBusMessageIter iter;
            DBusMessageIter array;
            DBusMessageIter dict;
            DBusMessageIter value;
            DBusMessageIter variant;
            DBusMessageIter data;

            /* Format: UINT32 org.freedesktop.Notifications.Notify
             *         (STRING app_name, UINT32 replaces_id, STRING app_icon, STRING summary, STRING body,
             *          ARRAY actions, DICT hints, INT32 expire_timeout)
             */
            dbus_message_iter_init_append(msg,&iter);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&msg_app);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_UINT32,&msg_replace_id);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&msg_icon);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&msg_summary);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_STRING,&msg_body);
            dbus_message_iter_open_container(&iter,DBUS_TYPE_ARRAY,DBUS_TYPE_STRING_AS_STRING,&array);
            dbus_message_iter_close_container(&iter,&array);
            dbus_message_iter_open_container(&iter,DBUS_TYPE_ARRAY,"{sv}",&array);
            dbus_message_iter_close_container(&iter,&array);
            dbus_message_iter_append_basic(&iter,DBUS_TYPE_INT32,&msg_timeout);

            DBusError err;
            dbus_error_init(&err);

            DBusMessage *reply;
            reply = dbus_connection_send_with_reply_and_block(conn, msg,
                30 * 1000 /* 30 seconds timeout */, &err);
            if (dbus_error_is_set(&err))
            {
                LogRel(("D-BUS returned an error while sending the notification: %s", err.message));
            }
            else if (reply)
            {
                dbus_connection_flush(conn);
                dbus_message_unref(reply);
            }
            if (dbus_error_is_set(&err))
            dbus_error_free(&err);
        }
        if (msg != NULL)
            dbus_message_unref(msg);
# else
        /* TODO: Implement me */
        rc = VINF_SUCCESS;
# endif /* VBOX_WITH_DBUS */
        return rc;
    }

    /** @todo Move this part in VbglR3 and just provide a callback for the platform-specific
              notification stuff, since this is very similar to the VBoxTray code. */
    virtual int run(bool fDaemonised /* = false */)
    {
        int rc;
        LogFlowFunc(("\n"));

        /* Because we need desktop notifications to be displayed, wait
         * some time to make the desktop environment load (as a work around). */
        if (fDaemonised)
            RTThreadSleep(30 * 1000 /* Wait 30 seconds */);

# ifdef VBOX_WITH_DBUS
        rc = RTDBusLoadLib();
        if (RT_FAILURE(rc))
            LogRel(("VBoxClient: D-Bus seems not to be installed; no host version check/notification done.\n"));
# else
        rc = VERR_NOT_IMPLEMENTED;
# endif /* VBOX_WITH_DBUS */

# ifdef VBOX_WITH_GUEST_PROPS
        uint32_t uGuestPropSvcClientID;
        if (RT_SUCCESS(rc))
        {
            rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
            if (RT_FAILURE(rc))
                LogRel(("VBoxClient: Cannot connect to guest property service while chcking for host version! rc = %Rrc\n", rc));
        }

        if (RT_SUCCESS(rc))
        {
            char *pszHostVersion;
            char *pszGuestVersion;
            bool bUpdate;

            rc = VbglR3HostVersionCheckForUpdate(uGuestPropSvcClientID, &bUpdate, &pszHostVersion, &pszGuestVersion);
            if (RT_SUCCESS(rc))
            {
                if (bUpdate)
                {
                    char szMsg[1024];
                    char szTitle[64];

                    /** @todo add some translation macros here */
                    RTStrPrintf(szTitle, sizeof(szTitle), "VirtualBox Guest Additions update available!");
#ifndef VBOX_OSE
                    RTStrPrintf(szMsg, sizeof(szMsg), "Your guest is currently running the Guest Additions version %s. "
                                                      "We recommend updating to the latest version (%s) by choosing the "
                                                      "install option from the Devices menu.", pszGuestVersion, pszHostVersion);
#else
/* This is the message which appears for non-Oracle builds of the
 * Guest Additions.  Distributors are encouraged to customise this. */
                    RTStrPrintf(szMsg, sizeof(szMsg), "Your virtual machine is currently running the Guest Additions version %s. Since you are running a version of the Guest Additions provided by the operating system you installed in the virtual machine we recommend that you update it to at least version %s using that system's update features, or alternatively that you remove this version and then install the " VBOX_VENDOR_SHORT " Guest Additions package using the install option from the Devices menu. Please consult the documentation for the operating system you are running to find out how to update or remove the current Guest Additions package.", pszGuestVersion, pszHostVersion);
#endif
                    rc = showNotify(szTitle, szMsg);
                    LogRel(("VBoxClient: VirtualBox Guest Additions update available!"));
                    if (RT_FAILURE(rc))
                        LogRel(("VBoxClient: Could not show version notifier tooltip! rc = %d\n", rc));
                }

                /* Store host version to not notify again */
                rc = VbglR3HostVersionLastCheckedStore(uGuestPropSvcClientID, pszHostVersion);

                VbglR3GuestPropReadValueFree(pszHostVersion);
                VbglR3GuestPropReadValueFree(pszGuestVersion);
            }
            VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
        }
# endif /* VBOX_WITH_GUEST_PROPS */
        LogFlowFunc(("returning %Rrc\n", rc));
        return rc;
    }

    virtual void cleanup()
    {

    }
};

/* Static factory */
VBoxClient::Service *VBoxClient::GetHostVersionService()
{
    return new HostVersionService;
}

