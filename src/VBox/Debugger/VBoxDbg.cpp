/* $Id$ */
/** @file
 * VBox Debugger GUI.
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


#define VBOX_COM_NO_ATL
#include <VBox/dbggui.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#include "VBoxDbgGui.h"


/**
 * Debugger GUI instance data.
 */
typedef struct DBGGUI
{
    /** Magic number (DBGGUI_MAGIC). */
    uint32_t    u32Magic;
    /** Pointer to the Debugger GUI manager object. */
    VBoxDbgGui *pVBoxDbgGui;
} DBGGUI;

/** DBGGUI magic value (Elizabeth Kostova). */
#define DBGGUI_MAGIC    0x19640804


/**
 * Creates the debugger GUI.
 *
 * @returns VBox status code.
 * @param   pSession    The Virtual Box session.
 * @param   ppGui       Where to store the pointer to the debugger instance.
 */
DBGDECL(int) DBGGuiCreate(ISession *pSession, PDBGGUI *ppGui)
{
    PDBGGUI pGui = (PDBGGUI)RTMemAlloc(sizeof(*pGui));
    if (!pGui)
        return VERR_NO_MEMORY;
    pGui->u32Magic = DBGGUI_MAGIC;
    pGui->pVBoxDbgGui = new VBoxDbgGui();

    int rc = pGui->pVBoxDbgGui->init(pSession);
    if (VBOX_SUCCESS(rc))
    {
        *ppGui = pGui;
        return rc;
    }

    delete pGui->pVBoxDbgGui;
    RTMemFree(pGui);
    *ppGui = NULL;
    return rc;
}


/**
 * Destroys the debugger GUI.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiDestroy(PDBGGUI pGui)
{
    /*
     * Validate.
     */
    if (!pGui)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(pGui->u32Magic == DBGGUI_MAGIC, ("u32Magic=%#x\n", pGui->u32Magic), VERR_INVALID_PARAMETER);

    /*
     * Do the job.
     */
    pGui->u32Magic++;
    delete pGui->pVBoxDbgGui;
    RTMemFree(pGui);

    return VINF_SUCCESS;
}


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
DBGDECL(void) DBGGuiAdjustRelativePos(PDBGGUI pGui, int x, int y, unsigned cx, unsigned cy)
{
    AssertReturn(pGui, (void)VERR_INVALID_PARAMETER);
    AssertMsgReturn(pGui->u32Magic == DBGGUI_MAGIC, ("u32Magic=%#x\n", pGui->u32Magic), (void)VERR_INVALID_PARAMETER);
    pGui->pVBoxDbgGui->adjustRelativePos(x, y, cx, cy);
}


/**
 * Shows the default statistics window.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiShowStatistics(PDBGGUI pGui)
{
    AssertReturn(pGui, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pGui->u32Magic == DBGGUI_MAGIC, ("u32Magic=%#x\n", pGui->u32Magic), VERR_INVALID_PARAMETER);
    return pGui->pVBoxDbgGui->showStatistics();
}


/**
 * Shows the default command line window.
 *
 * @returns VBox status code.
 * @param   pGui        The instance returned by DBGGuiCreate().
 */
DBGDECL(int) DBGGuiShowCommandLine(PDBGGUI pGui)
{
    AssertReturn(pGui, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pGui->u32Magic == DBGGUI_MAGIC, ("u32Magic=%#x\n", pGui->u32Magic), VERR_INVALID_PARAMETER);
    return pGui->pVBoxDbgGui->showConsole();
}

