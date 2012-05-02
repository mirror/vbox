/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindow class declaration
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineWindow_h__
#define __UIMachineWindow_h__

/* Global includes */
#include <QMainWindow>

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "UIMachineDefs.h"
#include "COMDefs.h"

/* Global forwards */
class QWidget;
class QGridLayout;
class QSpacerItem;
class QCloseEvent;

/* Local forwards */
class CSession;
class UISession;
class UIMachineLogic;
class UIMachineView;

class UIMachineWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

public:

    /* Factory function to create required machine window child: */
    static UIMachineWindow* create(UIMachineLogic *pMachineLogic, UIVisualStateType visualStateType, ulong uScreenId = 0);
    static void destroy(UIMachineWindow *pWhichWindow);

    /* Public getters: */
    virtual UIMachineLogic* machineLogic() const { return m_pMachineLogic; }
    virtual UIMachineView* machineView() const { return m_pMachineView; }
    UISession* uisession() const;
    CSession& session() const;

protected slots:

    /* Session event-handlers: */
    virtual void sltMachineStateChanged();
    virtual void sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);

    /* Slot to safe close machine-window: */
    void sltTryClose();

protected:

    /* Machine window constructor/destructor: */
    UIMachineWindow(UIMachineLogic *pMachineLogic, ulong uScreenId);
    virtual ~UIMachineWindow();

    /* Protected getters: */
    const QString& defaultWindowTitle() const { return m_strWindowTitlePrefix; }

    /* Translate routine: */
    virtual void retranslateUi();

    /* Common machine window event handlers: */
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif
    void closeEvent(QCloseEvent *pEvent);

    /* Prepare helpers: */
    virtual void prepareWindowIcon();
    virtual void prepareConsoleConnections();
    virtual void prepareMachineViewContainer();
    //virtual void loadWindowSettings() {}
    virtual void prepareHandlers();

    /* Cleanup helpers: */
    virtual void cleanupHandlers();
    //virtual void saveWindowSettings() {}
    //virtual void cleanupMachineViewContainer() {}
    //virtual void cleanupConsoleConnections() {}
    //virtual void cleanupWindowIcon() {}

    /* Update routines: */
    virtual void updateAppearanceOf(int iElement);
#ifdef VBOX_WITH_DEBUGGER_GUI
    virtual void updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Helpers: */
    Qt::WindowFlags windowFlags(UIVisualStateType visualStateType);

    /* Show routine: */
    virtual void showInNecessaryMode() = 0;

    /* Protected variables: */
    UIMachineLogic *m_pMachineLogic;

    /* Virtual screen number: */
    ulong m_uScreenId;

    QGridLayout *m_pMachineViewContainer;
    QSpacerItem *m_pTopSpacer;
    QSpacerItem *m_pBottomSpacer;
    QSpacerItem *m_pLeftSpacer;
    QSpacerItem *m_pRightSpacer;

    UIMachineView *m_pMachineView;
    QString m_strWindowTitlePrefix;

    friend class UIMachineLogic;
};

#endif // __UIMachineWindow_h__

