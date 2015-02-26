/** @file
 * VBox Qt GUI - UIMenuBarEditorWindow class declaration.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMenuBarEditorWindow_h___
#define ___UIMenuBarEditorWindow_h___

/* Qt includes: */
#include <QMap>

/* GUI includes: */
#include "UISlidingToolBar.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIMachineWindow;
class UIActionPool;
class UIToolBar;
class UIAction;
class QIToolButton;
class QCheckBox;
class QHBoxLayout;
class QAction;
class QMenu;

/** UISlidingToolBar wrapper
  * providing user with possibility to edit menu-bar layout. */
class UIMenuBarEditorWindow : public UISlidingToolBar
{
    Q_OBJECT;

public:

    /** Constructor, passes @a pParent to the UISlidingToolBar constructor.
      * @param pActionPool brings the action-pool reference for internal use. */
    UIMenuBarEditorWindow(UIMachineWindow *pParent, UIActionPool *pActionPool);
};

/** QWidget reimplementation
  * used as menu-bar editor widget. */
class UIMenuBarEditorWidget : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about Cancel button click. */
    void sigCancelClicked();

public:

    /** Constructor.
      * @param pParent                is passed to QWidget constructor,
      * @param fStartedFromVMSettings determines whether 'this' is a part of VM settings,
      * @param strMachineID           brings the machine ID to be used by the editor,
      * @param pActionPool            brings the action-pool to be used by the editor. */
    UIMenuBarEditorWidget(QWidget *pParent,
                          bool fStartedFromVMSettings = true,
                          const QString &strMachineID = QString(),
                          UIActionPool *pActionPool = 0);

    /** Returns the machine ID instance. */
    const QString& machineID() const { return m_strMachineID; }
    /** Defines the @a strMachineID instance. */
    void setMachineID(const QString &strMachineID);

    /** Returns the action-pool reference. */
    const UIActionPool* actionPool() const { return m_pActionPool; }
    /** Defines the @a pActionPool reference. */
    void setActionPool(UIActionPool *pActionPool);

private slots:

    /** Handles configuration change. */
    void sltHandleConfigurationChange(const QString &strMachineID);

    /** Handles menu-bar enable toggle. */
    void sltHandleMenuBarEnableToggle(bool fEnabled);
    /** Handles menu-bar menu click. */
    void sltHandleMenuBarMenuClick();

private:

    /** Prepare routine. */
    void prepare();

#ifdef Q_WS_MAC
    /** Prepare named menu routine. */
    QMenu* prepareNamedMenu(const QString &strName);
#endif /* Q_WS_MAC */
    /** Prepare copied menu routine. */
    QMenu* prepareCopiedMenu(const UIAction *pAction);
#if 0
    /** Prepare copied sub-menu routine. */
    QMenu* prepareCopiedSubMenu(QMenu *pMenu, const UIAction *pAction);
#endif
    /** Prepare named action routine. */
    QAction* prepareNamedAction(QMenu *pMenu, const QString &strName,
                                int iExtraDataID, const QString &strExtraDataID);
    /** Prepare copied action routine. */
    QAction* prepareCopiedAction(QMenu *pMenu, const UIAction *pAction);

    /** Prepare menus routine. */
    void prepareMenus();
#ifdef Q_WS_MAC
    /** Mac OS X: Prepare 'Application' menu routine. */
    void prepareMenuApplication();
#endif /* Q_WS_MAC */
    /** Prepare 'Machine' menu routine. */
    void prepareMenuMachine();
    /** Prepare 'View' menu routine. */
    void prepareMenuView();
    /** Prepare 'Input' menu routine. */
    void prepareMenuInput();
    /** Prepare 'Devices' menu routine. */
    void prepareMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Prepare 'Debug' menu routine. */
    void prepareMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef Q_WS_MAC
    /** Mac OS X: Prepare 'Window' menu routine. */
    void prepareMenuWindow();
#endif /* Q_WS_MAC */
    /** Prepare 'Help' menu routine. */
    void prepareMenuHelp();

    /** Update enable-checkbox routine. */
    void updateEnableCheckbox();
    /** Update menus routine. */
    void updateMenus();
#ifdef Q_WS_MAC
    /** Mac OS X: Update 'Application' menu routine. */
    void updateMenuApplication();
#endif /* Q_WS_MAC */
    /** Update 'Machine' menu routine. */
    void updateMenuMachine();
    /** Update 'View' menu routine. */
    void updateMenuView();
    /** Update 'Input' menu routine. */
    void updateMenuInput();
    /** Update 'Devices' menu routine. */
    void updateMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Update 'Debug' menu routine. */
    void updateMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef Q_WS_MAC
    /** Mac OS X: Update 'Window' menu routine. */
    void updateMenuWindow();
#endif /* Q_WS_MAC */
    /** Update 'Help' menu routine. */
    void updateMenuHelp();

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Paint event handler. */
    virtual void paintEvent(QPaintEvent *pEvent);

    /** @name General
      * @{ */
        /** Holds whether 'this' is prepared. */
        bool m_fPrepared;
        /** Holds whether 'this' is a part of VM settings. */
        bool m_fStartedFromVMSettings;
        /** Holds the machine ID instance. */
        QString m_strMachineID;
        /** Holds the action-pool reference. */
        const UIActionPool *m_pActionPool;
    /** @} */

    /** @name Contents
      * @{ */
        /** Holds the main-layout instance. */
        QHBoxLayout *m_pMainLayout;
        /** Holds the tool-bar instance. */
        UIToolBar *m_pToolBar;
        /** Holds the close-button instance. */
        QIToolButton *m_pButtonClose;
        /** Holds the enable-checkbox instance. */
        QCheckBox *m_pCheckBoxEnable;
        /** Holds tool-bar action references. */
        QMap<QString, QAction*> m_actions;
    /** @} */
};

#endif /* !___UIMenuBarEditorWindow_h___ */
