/** @file
 * DBGGUI - The VirtualBox Debugger GUI.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_dbggui_h
#define ___VBox_dbggui_h

#if defined(RT_OS_WINDOWS)
# include <VirtualBox.h>
#else
# include <VirtualBox_XPCOM.h>
#endif

#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_dbggui    VirtualBox Debugger GUI
 * @{
 */

/** Pointer to the debugger GUI instance structure. */
typedef struct DBGGUI *PDBGGUI;

/**
 * Creates the debugger GUI.
 *
 * @returns VBox status code.
 * @param   pSession    The VirtualBox session.
 * @param   ppGui       Where to store the pointer to the debugger instance.
 */
DBGDECL(int) DBGGuiCreate(ISession *pSession, PDBGGUI *ppGui);

/**
 * Destroys the debugger GUI.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiDestroy(PDBGGUI pGui);

/**
 * Notifies the debugger GUI that the console window (or whatever) has changed
 * size or position.
 *
 * @param   pGui        The instance returned by DBGGuiCreate().
 * @param   x           The x-coordinate of the window the debugger is relative to.
 * @param   y           The y-coordinate of the window the debugger is relative to.
 * @param   cx          The width of the window the debugger is relative to.
 * @param   cy          The height of the window the debugger is relative to.
 */
DBGDECL(void) DBGGuiAdjustRelativePos(PDBGGUI pGui, int x, int y, unsigned cx, unsigned cy);

/**
 * Shows the default statistics window.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiShowStatistics(PDBGGUI pGui);

/**
 * Shows the default command line window.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiShowCommandLine(PDBGGUI pGui);

/** @} */

__END_DECLS

#endif

