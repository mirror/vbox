/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * Network routine related declarations
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINetworkDefs_h__
#define __UINetworkDefs_h__

/* Network-request types: */
enum UINetworkRequestType
{
    UINetworkRequestType_HEAD,
    UINetworkRequestType_GET,
    UINetworkRequestType_GET_Our
};

/* Network-reply types: */
enum UINetworkReplyType
{
    UINetworkReplyType_Qt,
    UINetworkReplyType_Our
};

#endif // __UINetworkDefs_h__

