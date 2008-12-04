/* $Id $ */
/** @file
 * VirtualBox USB Proxy Service class, test version for Linux hosts.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


#ifndef ____H_TSTUSBLINUX
#define ____H_TSTUSBLINUX

typedef int HRESULT;
enum { S_OK = 0, E_NOTIMPL = 1 };

#include <VBox/usb.h>
#include <VBox/usbfilter.h>

#include <VBox/err.h>

#ifdef VBOX_USB_WITH_SYSFS
# include <libhal.h>
#endif

#include <stdio.h>
/**
 * The Linux hosted USB Proxy Service.
 */
class USBProxyServiceLinux
{
public:
    USBProxyServiceLinux() : mLastError(VINF_SUCCESS) {}
    HRESULT initSysfs(void);
    PUSBDEVICE getDevicesFromSysfs(void);
    int getLastError(void) { return mLastError; }

private:
    int start(void) { return VINF_SUCCESS; }
    static void freeDevice(PUSBDEVICE) {}  /* We don't care about leaks in a test. */
    int usbProbeInterfacesFromLibhal(const char *pszHalUuid, PUSBDEVICE pDev);
    int mLastError;
#  ifdef VBOX_USB_WITH_SYSFS
    /** Our connection to DBus for getting information from hal.  This will be
     * NULL if the initialisation failed. */
    DBusConnection *mDBusConnection;
    /** Handle to libhal. */
    LibHalContext *mLibHalContext;
#  endif
};

#endif /* !____H_TSTUSBLINUX */

