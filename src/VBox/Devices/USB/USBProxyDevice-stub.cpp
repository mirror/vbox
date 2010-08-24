/* $Id$ */
/** @file
 * USB device proxy - Stub.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/pdm.h>

#include "USBProxyDevice.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Stub USB Proxy Backend.
 */
extern const USBPROXYBACK g_USBProxyDeviceHost =
{
    "host",
    NULL,       /* Open */
    NULL,       /* Init */
    NULL,       /* Close */
    NULL,       /* Reset */
    NULL,       /* SetConfig */
    NULL,       /* ClaimInterface */
    NULL,       /* ReleaseInterface */
    NULL,       /* SetInterface */
    NULL,       /* ClearHaltedEp */
    NULL,       /* UrbQueue */
    NULL,       /* UrbCancel */
    NULL,       /* UrbReap */
    0
};

