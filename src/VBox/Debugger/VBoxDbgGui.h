/** @file
 *
 * VBox Debugger GUI - The Manager.
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

#ifndef __VBoxDbgGui_h__
#define __VBoxDbgGui_h__

// VirtualBox COM interfaces declarations (generated header)
#ifdef VBOX_WITH_XPCOM
# include <VirtualBox_XPCOM.h>
#else
# include <VirtualBox.h>
#endif

#include "VBoxDbgStats.h"
#include "VBoxDbgConsole.h"


/**
 * The Debugger GUI manager class.
 *
 * It's job is to provide a C callable external interface and manage the
 * windows and bit making up the debugger GUI.
 */
class VBoxDbgGui : public QObject
{
    Q_OBJECT

public:
    /**
     * Create a default VBoxDbgGui object.
     */
    VBoxDbgGui();

    /**
     * Initializes a VBoxDbgGui object.
     *
     * @returns VBox status code.
     * @param   pSession    VBox Session object.
     */
    int init(ISession *pSession);

    /**
     * Destroys the VBoxDbgGui object.
     */
    virtual ~VBoxDbgGui();

    /**
     * Show the default statistics window, creating it if necessary.
     *
     * @returns VBox status code.
     */
    int showStatistics();

    /**
     * Repositions and resizes (optionally) the statistics to its defaults
     *
     * @param   fResize     If set (default) the size of window is also changed.
     */
    void repositionStatistics(bool fResize = true);

    /**
     * Show the console window (aka. command line), creating it if necessary.
     *
     * @returns VBox status code.
     */
    int showConsole();

    /**
     * Repositions and resizes (optionally) the console to its defaults
     *
     * @param   fResize     If set (default) the size of window is also changed.
     */
    void repositionConsole(bool fResize = true);

    /**
     * Update the desktop size.
     * This is called whenever the reference window changes positition.
     */
    void updateDesktopSize();

    /**
     * Notifies the debugger GUI that the console window (or whatever) has changed
     * size or position.
     *
     * @param   x           The x-coordinate of the window the debugger is relative to.
     * @param   y           The y-coordinate of the window the debugger is relative to.
     * @param   cx          The width of the window the debugger is relative to.
     * @param   cy          The height of the window the debugger is relative to.
     */
    void adjustRelativePos(int x, int y, unsigned cx, unsigned cy);

    /**
     * Resizes a QWidget given the frame size.
     *
     * @param   pWidget     The widget to resize.
     * @param   cx          The new frame height.
     * @param   cy          The new frame width.
     */
    static void resizeWidget(QWidget *pWidget, unsigned cx, unsigned cy);

protected slots:
    /**
     * Notify that a child object (i.e. a window is begin destroyed).
     * @param   pObj    The object which is being destroyed.
     */
    void notifyChildDestroyed(QObject *pObj);

protected:

    /** The debugger statistics. */
    VBoxDbgStats *m_pDbgStats;
    /** The debugger console (aka. command line). */
    VBoxDbgConsole *m_pDbgConsole;

    /** The Virtual Box session. */
    ISession *m_pSession;
    /** The Virtual Box console. */
    IConsole *m_pConsole;
    /** The Virtual Box Machine Debugger. */
    IMachineDebugger *m_pMachineDebugger;
    /** The Virtual Box Machine. */
    IMachine *m_pMachine;
    /** The VM instance. */
    PVM m_pVM;

    /** The x-coordinate of the window we're relative to. */
    int m_x;
    /** The y-coordinate of the window we're relative to. */
    int m_y;
    /** The width of the window we're relative to. */
    unsigned m_cx;
    /** The height of the window we're relative to. */
    unsigned m_cy;
    /** The x-coordianate of the desktop. */
    int m_xDesktop;
    /** The y-coordianate of the desktop. */
    int m_yDesktop;
    /** The size of the desktop. */
    unsigned m_cxDesktop;
    /** The size of the desktop. */
    unsigned m_cyDesktop;
};


#endif

