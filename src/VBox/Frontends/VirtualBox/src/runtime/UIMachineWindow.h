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
#include "UIMachineDefs.h"

/* Global forwards */
class QWidget;

/* Local forwards */
class UIMachineLogic;
class UIMachineView;

class UIMachineWindow
{
public:

    /* Factory function to create required machine window child: */
    static UIMachineWindow* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType);

    /* Abstract slot to close machine window: */
    virtual void sltTryClose() = 0;

    /* Public getters: */
    virtual UIMachineLogic* machineLogic() { return m_pMachineLogic; }
    virtual QWidget* machineWindow() { return m_pMachineWindow; }
    virtual UIMachineView* machineView() { return m_pMachineView; }

protected:

    /* Common machine window constructor: */
    UIMachineWindow(UIMachineLogic *pMachineLogic);
    virtual ~UIMachineWindow();

    /* Translate routine: */
    void retranslateWindow();

    /* Update routines: */
    virtual void updateAppearanceOf(int iElement);

    /* Common machine window event handlers: */
    void closeEvent(QCloseEvent *pEvent);

    /* Protected getters: */
    const QString& defaultWindowTitle() const { return m_strWindowTitlePrefix; }

    /* Protected variables: */
    QWidget *m_pMachineWindow;
    UIMachineView *m_pMachineView;

private:

    /* Prepare helpers: */
    void prepareWindowIcon();
    void loadWindowSettings();

    /* Cleanup helpers: */
    //void saveWindowSettings();
    //void cleanupWindowIcon();

    /* Getter variables: */
    UIMachineLogic *m_pMachineLogic;

    /* Helper variables: */
    QString m_strWindowTitlePrefix;
};

#endif // __UIMachineWindow_h__
