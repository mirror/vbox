/* $Id$ */
/** @file
 * X11 guest client - Host version check.
 */

/*
 * Copyright (C) 2011-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/ldr.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#ifdef VBOX_WITH_DBUS
# include <VBox/dbus.h>
#endif
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#ifdef VBOX_OSE
# include <VBox/version.h>
#endif

#include "VBoxClient.h"

static int showNotify(const char *pszHeader, const char *pszBody)
{
    int rc;
# ifdef VBOX_WITH_DBUS
    DBusConnection *conn;
    DBusMessage* msg = NULL;
    conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
    if (conn == NULL)
    {
        VBClLogError("Could not retrieve D-BUS session bus\n");
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
            VBClLogError("Could not create D-BUS message!\n");
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
        const char *msg_summary = pszHeader;
        const char *msg_body = pszBody;
        int32_t msg_timeout = -1;           /* Let the notification server decide */

        DBusMessageIter iter;
        DBusMessageIter array;
        /*DBusMessageIter dict; - unused */
        /*DBusMessageIter value; - unused */
        /*DBusMessageIter variant; - unused */
        /*DBusMessageIter data; - unused */

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
        reply = dbus_connection_send_with_reply_and_block(conn, msg, 30 * 1000 /* 30 seconds timeout */, &err);
        if (dbus_error_is_set(&err))
            VBClLogError("D-BUS returned an error while sending the notification: %s", err.message);
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
    /** @todo Implement me */
    RT_NOREF(pszHeader, pszBody);
    rc = VINF_SUCCESS;
# endif /* VBOX_WITH_DBUS */
    return rc;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnWorker}
 */
static DECLCALLBACK(int) vbclHostVerWorker(bool volatile *pfShutdown)
{
    /** @todo Move this part in VbglR3 and just provide a callback for the platform-specific
              notification stuff, since this is very similar to the VBoxTray code. */

    RT_NOREF(pfShutdown);

    LogFlowFuncEnter();

    int rc;
# ifdef VBOX_WITH_DBUS
    rc = RTDBusLoadLib();
    if (RT_FAILURE(rc))
        VBClLogError("D-Bus seems not to be installed; no host version check/notification done\n");
# else
    rc = VERR_NOT_IMPLEMENTED;
# endif /* VBOX_WITH_DBUS */

# ifdef VBOX_WITH_GUEST_PROPS
    uint32_t uGuestPropSvcClientID;
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
        if (RT_FAILURE(rc))
            VBClLogError("Cannot connect to guest property service while chcking for host version, rc = %Rrc\n", rc);
    }

    if (RT_SUCCESS(rc))
    {
        /* Let the main thread know that it can continue spawning services. */
        RTThreadUserSignal(RTThreadSelf());

        /* Because we need desktop notifications to be displayed, wait
         * some time to make the desktop environment load (as a work around). */
        if (g_fDaemonized)
            RTThreadSleep(RT_MS_30SEC);

        char *pszHostVersion;
        char *pszGuestVersion;
        bool  fUpdate;

        rc = VbglR3HostVersionCheckForUpdate(uGuestPropSvcClientID, &fUpdate, &pszHostVersion, &pszGuestVersion);
        if (RT_SUCCESS(rc))
        {
            if (fUpdate)
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
                VBClLogInfo("VirtualBox Guest Additions update available!\n");
                if (RT_FAILURE(rc))
                    VBClLogError("Could not show version notifier tooltip! rc = %d\n", rc);
            }

            /* Store host version to not notify again */
            rc = VbglR3HostVersionLastCheckedStore(uGuestPropSvcClientID, pszHostVersion);

            VbglR3GuestPropReadValueFree(pszHostVersion);
            VbglR3GuestPropReadValueFree(pszGuestVersion);
        }
        VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
    }
# endif /* VBOX_WITH_GUEST_PROPS */

    return rc;
}

VBCLSERVICE g_SvcHostVersion =
{
    "hostversion",                   /* szName */
    "VirtualBox host version check", /* pszDescription */
    ".vboxclient-hostversion.pid",   /* pszPidFilePath */
    NULL,                            /* pszUsage */
    NULL,                            /* pszOptions */
    NULL,                            /* pfnOption */
    NULL,                            /* pfnInit */
    vbclHostVerWorker,               /* pfnWorker */
    NULL,                            /* pfnStop*/
    NULL                             /* pfnTerm */
};

