/** @file
 * USBLib - Library for wrapping up the VBoxUSB functionality, Darwin flavor.
 * (DEV,HDrv,Main)
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___VBox_usblib_darwin_h
#define ___VBox_usblib_darwin_h

#include <VBox/cdefs.h>
#include <VBox/usbfilter.h>

RT_C_DECLS_BEGIN
/** @defgroup grp_USBLib_darwin Darwin Specifics
 * @addtogroup grp_USBLib
 * @{ */

/** @name VBoxUSB specific device properties.
 * VBoxUSB makes use of the OWNER property for communicating between the probe and
 * start stage.
 * USBProxyServiceDarwin makes use of all of them to correctly determin the state
 * of the device.
 * @{ */
/** Contains the pid of the current client. If 0, the kernel is the current client. */
#define VBOXUSB_CLIENT_KEY  "VBoxUSB-Client"
/** Contains the pid of the filter owner (i.e. the VBoxSVC pid). */
#define VBOXUSB_OWNER_KEY   "VBoxUSB-Owner"
/** Contains the ID of the matching filter. */
#define VBOXUSB_FILTER_KEY  "VBoxUSB-Filter"
/** @} */

/** @} */
RT_C_DECLS_END

#endif
