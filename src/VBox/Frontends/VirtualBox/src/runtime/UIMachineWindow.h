/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef __UIMachineWindow_h__
#define __UIMachineWindow_h__

/* Local includes */
#include "COMDefs.h"
#include "UIMachineDefs.h"

/* Global forwards */
class QWidget;
class QCloseEvent;

/* Local forwards */
class UIMachineLogic;
class UIMachineView;

class UIMachineWindow
{
public:

    /* Factory function to create required machine window child: */
    static UIMachineWindow* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType);
    static void destroy(UIMachineWindow *pWhichWindow);

    /* Abstract slot to close machine window: */
    virtual void sltTryClose();

    /* Public getters: */
    virtual UIMachineLogic* machineLogic() { return m_pMachineLogic; }
    virtual QWidget* machineWindow() { return m_pMachineWindow; }
    virtual UIMachineView* machineView() { return m_pMachineView; }

protected:

    /* Common machine window constructor: */
    UIMachineWindow(UIMachineLogic *pMachineLogic);
    virtual ~UIMachineWindow();

    /* Translate routine: */
    virtual void retranslateUi();

    /* Update routines: */
    virtual void updateAppearanceOf(int iElement);

    /* Prepare helpers: */
    virtual void prepareWindowIcon();
    virtual void prepareConsoleConnections();
    virtual void loadWindowSettings();

    /* Cleanup helpers: */
    //virtual void saveWindowSettings();
    //virtual void cleanupConsoleConnections();
    //virtual void cleanupWindowIcon();

    /* Common machine window event handlers: */
    void closeEvent(QCloseEvent *pEvent);

    /* Protected getters: */
    CSession session();
    const QString& defaultWindowTitle() const { return m_strWindowTitlePrefix; }

    /* Protected signals: */
    void sltMachineStateChanged(KMachineState machineState);

    /* Protected variables: */
    UIMachineLogic *m_pMachineLogic;
    QWidget *m_pMachineWindow;
    UIMachineView *m_pMachineView;
    QString m_strWindowTitlePrefix;

    friend class UIMachineLogic;
};

#endif // __UIMachineWindow_h__

