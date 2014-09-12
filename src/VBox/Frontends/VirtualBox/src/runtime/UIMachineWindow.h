/** @file
 * VBox Qt GUI - UIMachineWindow class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QGridLayout;
class QSpacerItem;
class QCloseEvent;
class CSession;
class UISession;
class UIMachineLogic;
class UIMachineView;
class UIActionPool;

/* Machine-window interface: */
class UIMachineWindow : public QIWithRetranslateUI2<QMainWindow>
{
    Q_OBJECT;

signals:

    /** Notifies about frame-buffer resize. */
    void sigFrameBufferResize();

public:

    /* Factory functions to create/destroy machine-window: */
    static UIMachineWindow* create(UIMachineLogic *pMachineLogic, ulong uScreenId = 0);
    static void destroy(UIMachineWindow *pWhichWindow);

    /* Prepare/cleanup machine-window: */
    void prepare();
    void cleanup();

    /* Public getters: */
    ulong screenId() const { return m_uScreenId; }
    UIMachineView* machineView() const { return m_pMachineView; }
    UIMachineLogic* machineLogic() const { return m_pMachineLogic; }
    UIActionPool* actionPool() const;
    UISession* uisession() const;
    CSession& session() const;
    CMachine machine() const;

    /** Adjusts machine-window size to correspond current machine-view size.
      * @param fAdjustPosition determines whether is it necessary to adjust position too.
      * @note  Reimplemented in sub-classes. Base implementation does nothing. */
    virtual void normalizeGeometry(bool fAdjustPosition) { Q_UNUSED(fAdjustPosition); }

    /** Adjusts machine-view size to correspond current machine-window size. */
    virtual void adjustMachineViewSize();

#ifndef VBOX_WITH_TRANSLUCENT_SEAMLESS
    /* Virtual caller for base class setMask: */
    virtual void setMask(const QRegion &region);
#endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */

protected slots:

    /* Session event-handlers: */
    virtual void sltMachineStateChanged();

protected:

    /* Constructor: */
    UIMachineWindow(UIMachineLogic *pMachineLogic, ulong uScreenId);

    /* Show stuff: */
    virtual void showInNecessaryMode() = 0;

    /* Translate stuff: */
    void retranslateUi();

    /* Event handlers: */
#ifdef Q_WS_X11
    bool x11Event(XEvent *pEvent);
#endif /* Q_WS_X11 */
    void closeEvent(QCloseEvent *pEvent);

#ifdef Q_WS_MAC
    /** Mac OS X: Handles native notifications.
      * @param  strNativeNotificationName  Native notification name. */
    virtual void handleNativeNotification(const QString & /* strNativeNotificationName */) {}
#endif /* Q_WS_MAC */

    /* Prepare helpers: */
    virtual void prepareSessionConnections();
    virtual void prepareMainLayout();
    virtual void prepareMenu() {}
    virtual void prepareStatusBar() {}
    virtual void prepareMachineView();
    virtual void prepareVisualState() {}
    virtual void prepareHandlers();
    virtual void loadSettings() {}

    /* Cleanup helpers: */
    virtual void saveSettings() {}
    virtual void cleanupHandlers();
    virtual void cleanupVisualState() {}
    virtual void cleanupMachineView();
    virtual void cleanupStatusBar() {}
    virtual void cleanupMenu() {}
    virtual void cleanupMainLayout() {}
    virtual void cleanupSessionConnections() {}

    /* Update stuff: */
    virtual void updateAppearanceOf(int iElement);
#ifdef VBOX_WITH_DEBUGGER_GUI
    void updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /* Helpers: */
    const QString& defaultWindowTitle() const { return m_strWindowTitlePrefix; }
    static Qt::Alignment viewAlignment(UIVisualStateType visualStateType);

#ifdef Q_WS_MAC
    /** Mac OS X: Handles native notifications.
      * @param  strNativeNotificationName  Native notification name.
      * @param  pWidget                    Widget, notification related to. */
    static void handleNativeNotification(const QString &strNativeNotificationName, QWidget *pWidget);
#endif /* Q_WS_MAC */

    /* Variables: */
    UIMachineLogic *m_pMachineLogic;
    UIMachineView *m_pMachineView;
    QString m_strWindowTitlePrefix;
    ulong m_uScreenId;
    QGridLayout *m_pMainLayout;
    QSpacerItem *m_pTopSpacer;
    QSpacerItem *m_pBottomSpacer;
    QSpacerItem *m_pLeftSpacer;
    QSpacerItem *m_pRightSpacer;

    /* Friend classes: */
    friend class UIMachineLogic;
    friend class UIMachineLogicFullscreen;
    friend class UIMachineLogicSeamless;
};

#endif // __UIMachineWindow_h__

