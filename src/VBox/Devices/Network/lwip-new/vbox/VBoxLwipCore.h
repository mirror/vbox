/* $Id$ */

/** @file
 * VBox Lwip Core Initiatetor/Finilizer.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Network_lwip_new_vbox_VBoxLwipCore_h
#define VBOX_INCLUDED_SRC_Network_lwip_new_vbox_VBoxLwipCore_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/**
 * Initializes LWIP core, and do callback on tcp/ip thread.
 */
int vboxLwipCoreInitialize(PFNRT1 pfnCallback, void * pfnCallbackArg);
void vboxLwipCoreFinalize(PFNRT1 pfnCallback, void * pfnCallbackArg);

#endif /* !VBOX_INCLUDED_SRC_Network_lwip_new_vbox_VBoxLwipCore_h */
