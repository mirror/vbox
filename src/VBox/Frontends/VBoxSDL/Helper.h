/** @file
 *
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Miscellaneous helpers header
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __H_HELPER
#define __H_HELPER

#ifdef __LINUX__

/**
 * Creates the XPCOM event thread
 *
 * @returns VBOX status code
 * @param   eqFD XPCOM event queue file descriptor
 */
int startXPCOMEventQueueThread(int eqFD);

/**
 * Signal to the XPCOM even queue thread that it should select for more events.
 */
void signalXPCOMEventQueueThread(void);

/**
 * Indicates to the XPCOM thread that it should terminate now.
 */
void terminateXPCOMQueueThread(void);

#endif /* __LINUX__ */

#endif // __H_HELPER

