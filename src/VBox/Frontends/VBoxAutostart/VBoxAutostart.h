/* $Id$ */
/** @file
 * VBoxAutostart - VirtualBox Autostart service.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxAutostart_h__
#define __VBoxAutostart_h__

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cdefs.h>
#include <VBox/types.h>

#include <VBox/com/com.h>
#include <VBox/com/VirtualBox.h>

/** Flag whether we are in verbose logging mode. */
extern bool                g_fVerbose;
/** Handle to the VirtualBox interface. */
extern ComPtr<IVirtualBox> g_pVirtualBox;
/** Handle to the session interface. */
extern ComPtr<ISession>    g_pSession;

/**
 * Log information in verbose mode.
 */
#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }

/**
 * Log messages to the release log.
 *
 * @returns nothing.
 * @param   pszFormat    Format string.
 */
DECLHIDDEN(void) serviceLog(const char *pszFormat, ...);

/**
 * Parse the given configuration file and return the interesting config parameters.
 *
 * @returns VBox status code.
 * @param   pszFilename    The config file to parse.
 * @param   pfAllowed      Where to store the flag whether the user of this process
 *                         is allowed to start VMs automatically during system startup.
 * @param   puStartupDelay Where to store the startup delay for the user.
 */
DECLHIDDEN(int) autostartParseConfig(const char *pszFilename, bool *pfAllowed, uint32_t *puStartupDelay);

/**
 * Main routine for the autostart daemon.
 *
 * @returns exit status code.
 * @param   uStartupDelay    Base startup for the user.
 */
DECLHIDDEN(RTEXITCODE) autostartStartMain(uint32_t uStartupDelay);

/**
 * Main routine for the autostart daemon when stopping virtual machines
 * during system shutdown.
 *
 * @returns exit status code.
 * @param   uStartupDelay    Base stop delay for the user.
 */
DECLHIDDEN(RTEXITCODE) autostartStopMain(uint32_t uStopDelay);

#endif /* __VBoxAutostart_h__ */

