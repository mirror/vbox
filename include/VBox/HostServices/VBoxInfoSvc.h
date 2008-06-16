/** @file
 * Shared information services:
 * Common header for host service and guest clients.
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

#ifndef ___VBox_HostService_VBoxSharedInfoSvc_h
#define ___VBox_HostService_VBoxSharedInfoSvc_h

#include <VBox/types.h>
#include <VBox/VBoxGuest.h>
#include <VBox/hgcmsvc.h>

/** Everything defined in this file lives in this namespace. */
namespace svcInfo {

/*
 * The service functions which are callable by host.
 */
enum eHostFn
{
    /** Pass the address of the console object from Main to the service */
    SET_CFGM_NODE = 1,
    /** Get the value attached to an extra data key in the machine XML file */
    GET_CONFIG_KEY_HOST = 2,
    /** Set the value attached to an extra data key in the machine XML file */
    SET_CONFIG_KEY_HOST = 3
};

/**
 * The service functions which are called by guest.  The numbers may not change,
 * so we hardcode them.
 */
enum eGuestFn
{
    /** Get the value attached to an extra data key in the machine XML file */
    GET_CONFIG_KEY = 1,
    /** Set the value attached to an extra data key in the machine XML file */
    SET_CONFIG_KEY = 2
};

/** Prefix for extra data keys used by the get and set key value functions */
#define VBOX_SHARED_INFO_KEY_PREFIX          "Guest/"
/** Maximum length for extra data keys used by the get and set key value functions */
enum { KEY_MAX_LEN = 64 };
/** Maximum length for extra data key values used by the get and set key value functions */
enum { KEY_MAX_VALUE_LEN = 128 };
/** Maximum number of extra data keys per guest */
enum { KEY_MAX_KEYS = 256 };

/**
 * HGCM parameter structures.  Packing is explicitly defined as this is a wire format.
 */
#pragma pack (1)
/** The guest is requesting the value of a configuration key */
typedef struct _GetConfigKey
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The key to look up (IN pointer)
     * This must fit to a number of criteria, namely
     *  - Only ASCII characters with no spaces
     *  - Less than or equal to VBOX_SHARED_INFO_KEY_MAX_LEN bytes in length
     *  - Zero terminated
     *  - Starting with the string VBOX_SHARED_INFO_KEY_PREFIX
     */
    HGCMFunctionParameter key;

    /**
     * The value of the key (OUT pointer)
     */
    HGCMFunctionParameter value;

    /**
     * The size of the value.  If this is greater than the size of the array
     * supplied by the guest then no data was transferred and the call must
     * be repeated. (OUT uint32_t)
     */
    HGCMFunctionParameter size;
} GetConfigKey;

/** The guest is requesting to change the value of a configuration key */
typedef struct _SetConfigKey
{
    VBoxGuestHGCMCallInfo hdr;

    /**
     * The key to change up.  This must fit to a number of criteria, namely
     *  - Only ASCII characters with no spaces
     *  - Less than or equal to VBOX_SHARED_INFO_KEY_MAX_LEN bytes in length
     *  - Zero terminated
     *  - Starting with the string VBOX_SHARED_INFO_KEY_PREFIX
     */
    HGCMFunctionParameter key;

    /**
     * The value of the key (IN pointer)
     * Criteria as for the key parameter, but with length less that or equal to
     * VBOX_SHARED_INFO_KEY_MAX_VALUE_LEN
     */
    HGCMFunctionParameter value;
} SetConfigKey;
#pragma pack ()

} /* namespace svcInfo */

#endif  /* ___VBox_HostService_VBoxSharedInfoSvc_h defined */
