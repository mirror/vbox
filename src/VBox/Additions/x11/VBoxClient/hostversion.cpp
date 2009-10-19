/** @file
 * X11 guest client - host version check.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <stdio.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/string.h>

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
#ifdef VBOX_WITH_DBUS
        DBusConnection *conn;
        DBusMessage* msg;
        conn = dbus_bus_get (DBUS_BUS_SESSON, NULL);
        if (conn == NULL)
        {
            LogFlow(("Could not retrieve D-BUS session bus!\n"));
            rc = VERR_INVALID_HANDLE;
        }
        else
        {
            msg = dbus_message_new_method_call("org.freedesktop.Notifications",
                                               "/org/freedesktop/Notifications",
                                               "org.freedesktop.Notifications",
                                               "Notify");
            if (conn == NULL)
            {
                LogFlow(("Could not create D-BUS message!\n"));
                rc = VERR_INVALID_HANDLE;
            }
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

            /* @todo Send with waiting for a reply! */
            if (dbus_connection_send(conn, msg, NULL))
                dbus_connection_flush(conn);
        }
        if (msg != NULL)
            dbus_message_unref(msg);
#else
        /* TODO: Implement me */
        rc = VINF_SUCCESS;
#endif /* VBOX_WITH_DBUS */
        return rc;
    }

    virtual int run()
    {
        int rc;
        LogFlowFunc(("\n"));
#ifdef VBOX_WITH_DBUS
        rc = RTDBusLoadLib();
        if (RT_FAILURE(rc))
        {
            LogRel(("VBoxClient: D-Bus seems not to be installed; no host version check/notification done.\n"));
        }
#else
        rc = VERR_NOT_IMPLEMENTED;
#endif
        if (RT_SUCCESS(rc))
        {
            int rc;
            char *pszHostVersion;
            char *pszGuestVersion;
            rc = VbglR3HostVersionCheckForUpdate(&pszHostVersion, &pszGuestVersion);
            if (RT_SUCCESS(rc))
            {
                char szMsg[256];
                char szTitle[64];

                /** @todo add some translation macros here */
                RTStrPrintf(szTitle, sizeof(szTitle), "VirtualBox Guest Additions update available!");
                RTStrPrintf(szMsg, sizeof(szMsg), "Your guest is currently running the Guest Additions version %s. "
                                                  "We recommend updating to the latest version (%s) by choosing the "
                                                  "install option from the Devices menu.", pszGuestVersion, pszHostVersion);
                rc = showNotify(szTitle, szMsg);
                if (RT_FAILURE(rc))
                    Log(("VBoxClient: Could not show version notifier tooltip! rc = %d\n", rc));

                VbglR3GuestPropReadValueFree(pszHostVersion);
                VbglR3GuestPropReadValueFree(pszGuestVersion);
            }

            /* If we didn't have to check for the host version then this is not an error */
            if (rc == VERR_NOT_SUPPORTED)
                rc = VINF_SUCCESS;
        }
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
