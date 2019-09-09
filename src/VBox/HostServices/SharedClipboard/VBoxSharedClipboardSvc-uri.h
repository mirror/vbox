/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal header for URI (list) handling.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_uri_h
#define VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_uri_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

int sharedClipboardSvcURIHandler(PSHCLCLIENT pClient, VBOXHGCMCALLHANDLE callHandle, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival);
int sharedClipboardSvcURIHostHandler(uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

int sharedClipboardSvcURIAreaRegister(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer);
int sharedClipboardSvcURIAreaUnregister(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer);
int sharedClipboardSvcURIAreaAttach(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer, SHCLAREAID uID);
int sharedClipboardSvcURIAreaDetach(PSHCLCLIENTSTATE pClientState, PSHCLURITRANSFER pTransfer);

#endif /* !VBOX_INCLUDED_SRC_SharedClipboard_VBoxSharedClipboardSvc_uri_h */

