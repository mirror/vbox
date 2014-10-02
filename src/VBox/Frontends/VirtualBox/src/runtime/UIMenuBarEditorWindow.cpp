/* $Id$ */
/** @file
 * VBox Qt GUI - UIMenuBarEditorWindow class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHBoxLayout>
# include <QPaintEvent>
# include <QMetaEnum>
# include <QMenuBar>
# include <QPainter>
# include <QMenu>
# include <QMap>

/* GUI includes: */
# include "UIMenuBarEditorWindow.h"
# include "UIActionPoolRuntime.h"
# include "UIExtraDataManager.h"
# include "UIMachineWindow.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIToolBar.h"
# include "QIWithRetranslateUI.h"
# include "QIToolButton.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */



/** QWidget reimplementation
  * used as menu-bar editor widget. */
class UIMenuBarEditorWidget : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about Cancel button click. */
    void sigCancelClicked();

public:

    /** Constructor, taking @a pActionPool argument. */
    UIMenuBarEditorWidget(UIActionPool *pActionPool);

    /** Returns the action-pool reference. */
    const UIActionPool* actionPool() const { return m_pActionPool; }

private slots:

    /** Handles configuration change. */
    void sltHandleConfigurationChange();

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
    /** Prepare 'Help' menu routine. */
    void prepareMenuHelp();

    /** Updates menus routine. */
    void updateMenus();
#ifdef Q_WS_MAC
    /** Mac OS X: Updates 'Application' menu routine. */
    void updateMenuApplication();
#endif /* Q_WS_MAC */
    /** Updates 'Machine' menu routine. */
    void updateMenuMachine();
    /** Updates 'View' menu routine. */
    void updateMenuView();
    /** Updates 'Input' menu routine. */
    void updateMenuInput();
    /** Updates 'Devices' menu routine. */
    void updateMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Updates 'Debug' menu routine. */
    void updateMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
    /** Updates 'Help' menu routine. */
    void updateMenuHelp();

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Paint event handler. */
    virtual void paintEvent(QPaintEvent *pEvent);

    /** @name General
      * @{ */
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
        /** Holds tool-bar action references. */
        QMap<QString, QAction*> m_actions;
    /** @} */
};


UIMenuBarEditorWidget::UIMenuBarEditorWidget(UIActionPool *pActionPool)
    : m_pActionPool(pActionPool)
    , m_pMainLayout(0)
    , m_pToolBar(0)
    , m_pButtonClose(0)
{
    /* Prepare: */
    prepare();
}

void UIMenuBarEditorWidget::sltHandleConfigurationChange()
{
    /* Update menus: */
    updateMenus();
}

void UIMenuBarEditorWidget::sltHandleMenuBarMenuClick()
{
    /* Make sure sender is valid: */
    QAction *pAction = qobject_cast<QAction*>(sender());
    AssertPtrReturnVoid(pAction);

    /* Depending on triggered action class: */
    switch (pAction->property("class").toInt())
    {
        case UIExtraDataMetaDefs::MenuType_All:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::MenuType type =
                static_cast<UIExtraDataMetaDefs::MenuType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::MenuType restrictions = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::MenuType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
#ifdef Q_WS_MAC
        case UIExtraDataMetaDefs::MenuType_Application:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::MenuApplicationActionType type =
                static_cast<UIExtraDataMetaDefs::MenuApplicationActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::MenuApplicationActionType restrictions = gEDataManager->restrictedRuntimeMenuApplicationActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::MenuApplicationActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuApplicationActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
#endif /* Q_WS_MAC */
        case UIExtraDataMetaDefs::MenuType_Machine:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::RuntimeMenuMachineActionType type =
                static_cast<UIExtraDataMetaDefs::RuntimeMenuMachineActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictions = gEDataManager->restrictedRuntimeMenuMachineActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::RuntimeMenuMachineActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuMachineActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
        case UIExtraDataMetaDefs::MenuType_View:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::RuntimeMenuViewActionType type =
                static_cast<UIExtraDataMetaDefs::RuntimeMenuViewActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictions = gEDataManager->restrictedRuntimeMenuViewActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::RuntimeMenuViewActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuViewActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
        case UIExtraDataMetaDefs::MenuType_Input:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::RuntimeMenuInputActionType type =
                static_cast<UIExtraDataMetaDefs::RuntimeMenuInputActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::RuntimeMenuInputActionType restrictions = gEDataManager->restrictedRuntimeMenuInputActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::RuntimeMenuInputActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuInputActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
        case UIExtraDataMetaDefs::MenuType_Devices:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::RuntimeMenuDevicesActionType type =
                static_cast<UIExtraDataMetaDefs::RuntimeMenuDevicesActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictions = gEDataManager->restrictedRuntimeMenuDevicesActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::RuntimeMenuDevicesActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuDevicesActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
#ifdef VBOX_WITH_DEBUGGER_GUI
        case UIExtraDataMetaDefs::MenuType_Debug:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType type =
                static_cast<UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restrictions = gEDataManager->restrictedRuntimeMenuDebuggerActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuDebuggerActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
#endif /* VBOX_WITH_DEBUGGER_GUI */
        case UIExtraDataMetaDefs::MenuType_Help:
        {
            /* Get sender type: */
            const UIExtraDataMetaDefs::MenuHelpActionType type =
                static_cast<UIExtraDataMetaDefs::MenuHelpActionType>(pAction->property("type").toInt());
            /* Load current menu-bar restrictions: */
            UIExtraDataMetaDefs::MenuHelpActionType restrictions = gEDataManager->restrictedRuntimeMenuHelpActionTypes(vboxGlobal().managedVMUuid());
            /* Invert restriction for sender type: */
            restrictions = (UIExtraDataMetaDefs::MenuHelpActionType)(restrictions ^ type);
            /* Save updated menu-bar restrictions: */
            gEDataManager->setRestrictedRuntimeMenuHelpActionTypes(restrictions, vboxGlobal().managedVMUuid());
            break;
        }
        default: break;
    }
}

void UIMenuBarEditorWidget::prepare()
{
    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
#ifdef Q_WS_MAC
        /* Standard margins on Mac OS X are too big: */
        m_pMainLayout->setContentsMargins(10, 5, 10, 10);
#else /* !Q_WS_MAC */
        /* Standard margins on Windows/X11: */
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        if (iTop >= 5)
            iTop -= 5;
        m_pMainLayout->setContentsMargins(iLeft, iTop, iRight, iBottom);
#endif /* !Q_WS_MAC */
        m_pMainLayout->setSpacing(0);
        /* Create tool-bar: */
        m_pToolBar = new UIToolBar;
        AssertPtrReturnVoid(m_pToolBar);
        {
            /* Prepare menus: */
            prepareMenus();
            /* Add tool-bar into main-layout: */
            m_pMainLayout->addWidget(m_pToolBar);
        }
        /* Insert stretch: */
        m_pMainLayout->addStretch();
        /* Create close-button: */
        m_pButtonClose = new QIToolButton;
        AssertPtrReturnVoid(m_pButtonClose);
        {
            /* Configure close-button: */
            m_pButtonClose->setFocusPolicy(Qt::StrongFocus);
            m_pButtonClose->setMinimumSize(QSize(1, 1));
            m_pButtonClose->setShortcut(Qt::Key_Escape);
            m_pButtonClose->setIcon(UIIconPool::iconSet(":/ok_16px.png"));
            connect(m_pButtonClose, SIGNAL(clicked(bool)), this, SIGNAL(sigCancelClicked()));
            /* Add close-button into main-layout: */
            m_pMainLayout->addWidget(m_pButtonClose);
        }
    }

    /* Translate contents: */
    retranslateUi();
}

void UIMenuBarEditorWidget::prepareMenus()
{
    /* Create menus: */
#ifdef Q_WS_MAC
    prepareMenuApplication();
#endif /* Q_WS_MAC */
    prepareMenuMachine();
    prepareMenuView();
    prepareMenuInput();
    prepareMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    prepareMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
    prepareMenuHelp();

    /* Listen for the menu-bar configuration changes: */
    connect(gEDataManager, SIGNAL(sigMenuBarConfigurationChange()),
            this, SLOT(sltHandleConfigurationChange()));

    /* Update menus: */
    updateMenus();
}

#ifdef Q_WS_MAC
QMenu* UIMenuBarEditorWidget::prepareNamedMenu(const QString &strName)
{
    /* Create named menu: */
    QMenu *pNamedMenu = new QMenu(strName, m_pToolBar);
    AssertPtrReturn(pNamedMenu, 0);
    {
        /* Configure named menu: */
        pNamedMenu->setProperty("class", UIExtraDataMetaDefs::MenuType_Application);
        /* Get named menu action: */
        QAction *pNamedMenuAction = pNamedMenu->menuAction();
        AssertPtrReturn(pNamedMenuAction, 0);
        {
            /* Add menu action into tool-bar: */
            m_pToolBar->addAction(pNamedMenuAction);
            /* Get named menu tool-button: */
            QToolButton *pNamedMenuToolButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(pNamedMenuAction));
            AssertPtrReturn(pNamedMenuToolButton, 0);
            {
                /* Configure named menu tool-button: */
                pNamedMenuToolButton->setPopupMode(QToolButton::MenuButtonPopup);
                pNamedMenuToolButton->setAutoRaise(true);
                /* Create spacing after named menu tool-button: */
                QWidget *pSpacing = new QWidget;
                AssertPtrReturn(pSpacing, 0);
                {
                    /* Configure spacing: */
                    pSpacing->setFixedSize(5, 1);
                    /* Add spacing into tool-bar: */
                    m_pToolBar->addWidget(pSpacing);
                }
            }
        }
    }
    /* Return named menu: */
    return pNamedMenu;
}
#endif /* Q_WS_MAC */

QMenu* UIMenuBarEditorWidget::prepareCopiedMenu(const UIAction *pAction)
{
    /* Create copied menu: */
    QMenu *pCopiedMenu = new QMenu(pAction->name(), m_pToolBar);
    AssertPtrReturn(pCopiedMenu, 0);
    {
        /* Configure copied menu: */
        pCopiedMenu->setProperty("class", pAction->extraDataID());
        /* Get copied menu action: */
        QAction *pCopiedMenuAction = pCopiedMenu->menuAction();
        AssertPtrReturn(pCopiedMenuAction, 0);
        {
            /* Configure copied menu action: */
            pCopiedMenuAction->setCheckable(true);
            pCopiedMenuAction->setProperty("class", UIExtraDataMetaDefs::MenuType_All);
            pCopiedMenuAction->setProperty("type", pAction->extraDataID());
            connect(pCopiedMenuAction, SIGNAL(triggered(bool)), this, SLOT(sltHandleMenuBarMenuClick()));
            m_actions.insert(pAction->extraDataKey(), pCopiedMenuAction);
            /* Add menu action into tool-bar: */
            m_pToolBar->addAction(pCopiedMenuAction);
            /* Get copied menu tool-button: */
            QToolButton *pCopiedMenuToolButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(pCopiedMenuAction));
            AssertPtrReturn(pCopiedMenuToolButton, 0);
            {
                /* Configure copied menu tool-button: */
                pCopiedMenuToolButton->setPopupMode(QToolButton::MenuButtonPopup);
                pCopiedMenuToolButton->setAutoRaise(true);
                /* Create spacing after copied menu tool-button: */
                QWidget *pSpacing = new QWidget;
                AssertPtrReturn(pSpacing, 0);
                {
                    /* Configure spacing: */
                    pSpacing->setFixedSize(5, 1);
                    /* Add spacing into tool-bar: */
                    m_pToolBar->addWidget(pSpacing);
                }
            }
        }
    }
    /* Return copied menu: */
    return pCopiedMenu;
}

#if 0
QMenu* UIMenuBarEditorWidget::prepareCopiedSubMenu(QMenu *pMenu, const UIAction *pAction)
{
    /* Create copied sub-menu: */
    QMenu *pCopiedMenu = pMenu->addMenu(pAction->name());
    AssertPtrReturn(pCopiedMenu, 0);
    {
        /* Configure copied sub-menu: */
        pCopiedMenu->setProperty("class", pMenu->property("class"));
        /* Get copied sub-menu action: */
        QAction *pCopiedMenuAction = pCopiedMenu->menuAction();
        AssertPtrReturn(pCopiedMenuAction, 0);
        {
            /* Configure copied sub-menu: */
            pCopiedMenuAction->setCheckable(true);
            pCopiedMenuAction->setProperty("class", pCopiedMenu->property("class"));
            pCopiedMenuAction->setProperty("type", pAction->extraDataID());
            connect(pCopiedMenuAction, SIGNAL(triggered(bool)), this, SLOT(sltHandleMenuBarMenuClick()));
            m_actions.insert(pAction->extraDataKey(), pCopiedMenuAction);
        }
    }
    /* Return copied sub-menu: */
    return pCopiedMenu;
}
#endif

QAction* UIMenuBarEditorWidget::prepareCopiedAction(QMenu *pMenu, const UIAction *pAction)
{
    /* Create copied action: */
    QAction *pCopiedAction = pMenu->addAction(pAction->name());
    AssertPtrReturn(pCopiedAction, 0);
    {
        /* Configure copied action: */
        pCopiedAction->setCheckable(true);
        pCopiedAction->setProperty("class", pMenu->property("class"));
        pCopiedAction->setProperty("type", pAction->extraDataID());
        connect(pCopiedAction, SIGNAL(triggered(bool)), this, SLOT(sltHandleMenuBarMenuClick()));
        m_actions.insert(pAction->extraDataKey(), pCopiedAction);
    }
    /* Return copied action: */
    return pCopiedAction;
}

#ifdef Q_WS_MAC
void UIMenuBarEditorWidget::prepareMenuApplication()
{
    /* Copy menu: */
    QMenu *pMenu = prepareNamedMenu("VirtualBox");
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_M_Application_S_About));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_M_Application_S_Preferences));
    }
}
#endif /* Q_WS_MAC */

void UIMenuBarEditorWidget::prepareMenuMachine()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndexRT_M_Machine));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_Settings));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_TakeSnapshot));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_TakeScreenshot));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_ShowInformation));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_T_Pause));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_Reset));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_Save));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_Shutdown));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Machine_S_PowerOff));
    }
}

void UIMenuBarEditorWidget::prepareMenuView()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndexRT_M_View));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_T_Seamless));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_T_Scale));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_S_AdjustWindow));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_M_MenuBar));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_View_M_StatusBar));
        pMenu->addSeparator();
//        prepareCopiedAction(pMenu, Resize);
//        prepareCopiedAction(pMenu, MultiScreen);
    }
}

void UIMenuBarEditorWidget::prepareMenuInput()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndexRT_M_Input));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Input_M_Keyboard));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Input_M_Mouse_T_Integration));
    }
}

void UIMenuBarEditorWidget::prepareMenuDevices()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndexRT_M_Devices));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_HardDrives));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_OpticalDevices));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_FloppyDevices));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_Network));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_USBDevices));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_WebCams));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_SharedFolders));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_SharedClipboard));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_DragAndDrop));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_T_VRDEServer));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_M_VideoCapture_T_Start));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Devices_S_InstallGuestTools));
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMenuBarEditorWidget::prepareMenuDebug()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndexRT_M_Debug));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Debug_S_ShowStatistics));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Debug_S_ShowCommandLine));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Debug_T_Logging));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndexRT_M_Debug_S_ShowLogDialog));
    }
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMenuBarEditorWidget::prepareMenuHelp()
{
    /* Copy menu: */
    QMenu *pMenu = prepareCopiedMenu(actionPool()->action(UIActionIndex_Menu_Help));
    AssertPtrReturnVoid(pMenu);
    {
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_Contents));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_WebSite));
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_ResetWarnings));
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_NetworkAccessManager));
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
#ifndef Q_WS_MAC
        pMenu->addSeparator();
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_About));
        prepareCopiedAction(pMenu, actionPool()->action(UIActionIndex_Simple_Preferences));
#endif /* !Q_WS_MAC */
    }
}

void UIMenuBarEditorWidget::updateMenus()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::MenuType restrictionsMenuBar = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::MenuType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("MenuType");
    const QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::MenuType enumValue =
            static_cast<const UIExtraDataMetaDefs::MenuType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip MenuType_Invalid & MenuType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::MenuType_Invalid ||
            enumValue == UIExtraDataMetaDefs::MenuType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuBar & enumValue));
    }

    /* Update known menu-bar menus: */
#ifdef Q_WS_MAC
    updateMenuApplication();
#endif /* Q_WS_MAC */
    updateMenuMachine();
    updateMenuView();
    updateMenuInput();
    updateMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    updateMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
    updateMenuHelp();
}

#ifdef Q_WS_MAC
void UIMenuBarEditorWidget::updateMenuApplication()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::MenuApplicationActionType restrictionsMenuApplication = gEDataManager->restrictedRuntimeMenuApplicationActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::MenuApplicationActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("MenuApplicationActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::MenuApplicationActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::MenuApplicationActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip MenuApplicationActionType_Invalid & MenuApplicationActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::MenuApplicationActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::MenuApplicationActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuApplication & enumValue));
    }
}
#endif /* Q_WS_MAC */

void UIMenuBarEditorWidget::updateMenuMachine()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictionsMenuMachine = gEDataManager->restrictedRuntimeMenuMachineActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::RuntimeMenuMachineActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("RuntimeMenuMachineActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::RuntimeMenuMachineActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::RuntimeMenuMachineActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip RuntimeMenuMachineActionType_Invalid & RuntimeMenuMachineActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::RuntimeMenuMachineActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuMachine & enumValue));
    }
}

void UIMenuBarEditorWidget::updateMenuView()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictionsMenuView = gEDataManager->restrictedRuntimeMenuViewActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::RuntimeMenuViewActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("RuntimeMenuViewActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::RuntimeMenuViewActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::RuntimeMenuViewActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip RuntimeMenuViewActionType_Invalid & RuntimeMenuViewActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::RuntimeMenuViewActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::RuntimeMenuViewActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuView & enumValue));
    }
}

void UIMenuBarEditorWidget::updateMenuInput()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::RuntimeMenuInputActionType restrictionsMenuInput = gEDataManager->restrictedRuntimeMenuInputActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::RuntimeMenuInputActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("RuntimeMenuInputActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::RuntimeMenuInputActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::RuntimeMenuInputActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip RuntimeMenuInputActionType_Invalid & RuntimeMenuInputActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::RuntimeMenuInputActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::RuntimeMenuInputActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuInput & enumValue));
    }
}

void UIMenuBarEditorWidget::updateMenuDevices()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictionsMenuDevices = gEDataManager->restrictedRuntimeMenuDevicesActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::RuntimeMenuDevicesActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("RuntimeMenuDevicesActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::RuntimeMenuDevicesActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::RuntimeMenuDevicesActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip RuntimeMenuDevicesActionType_Invalid & RuntimeMenuDevicesActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuDevices & enumValue));
    }
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIMenuBarEditorWidget::updateMenuDebug()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restrictionsMenuDebug = gEDataManager->restrictedRuntimeMenuDebuggerActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("RuntimeMenuDebuggerActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip RuntimeMenuDebuggerActionType_Invalid & RuntimeMenuDebuggerActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuDebug & enumValue));
    }
}
#endif /* VBOX_WITH_DEBUGGER_GUI */

void UIMenuBarEditorWidget::updateMenuHelp()
{
    /* Recache menu-bar configuration: */
    const UIExtraDataMetaDefs::MenuHelpActionType restrictionsMenuHelp = gEDataManager->restrictedRuntimeMenuHelpActionTypes(vboxGlobal().managedVMUuid());
    /* Get static meta-object: */
    const QMetaObject &smo = UIExtraDataMetaDefs::staticMetaObject;

    /* We have UIExtraDataMetaDefs::MenuHelpActionType enum registered, so we can enumerate it: */
    const int iEnumIndex = smo.indexOfEnumerator("MenuHelpActionType");
    QMetaEnum metaEnum = smo.enumerator(iEnumIndex);
    /* Handle other enum-values: */
    for (int iKeyIndex = 0; iKeyIndex < metaEnum.keyCount(); ++iKeyIndex)
    {
        /* Get iterated enum-value: */
        const UIExtraDataMetaDefs::MenuHelpActionType enumValue =
            static_cast<const UIExtraDataMetaDefs::MenuHelpActionType>(metaEnum.keyToValue(metaEnum.key(iKeyIndex)));
        /* Skip MenuHelpActionType_Invalid & MenuHelpActionType_All enum-value: */
        if (enumValue == UIExtraDataMetaDefs::MenuHelpActionType_Invalid ||
            enumValue == UIExtraDataMetaDefs::MenuHelpActionType_All)
            continue;
        /* Which key required action registered under? */
        const QString strKey = gpConverter->toInternalString(enumValue);
        if (!m_actions.contains(strKey))
            continue;
        /* Update action 'checked' state: */
        m_actions.value(strKey)->setChecked(!(restrictionsMenuHelp & enumValue));
    }
}

void UIMenuBarEditorWidget::retranslateUi()
{
    /* Translate close-button: */
    m_pButtonClose->setToolTip(tr("Close"));
}

void UIMenuBarEditorWidget::paintEvent(QPaintEvent*)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
    QColor color0 = pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);
#if defined(Q_WS_WIN) || defined(Q_WS_X11)
    QColor color3 = pal.color(QPalette::Window).darker(120);
#endif /* Q_WS_WIN || Q_WS_X11 */

    /* Left corner: */
    QRadialGradient grad1(QPointF(5, height() - 5), 5);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Right corner: */
    QRadialGradient grad2(QPointF(width() - 5, height() - 5), 5);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }
    /* Bottom line: */
    QLinearGradient grad3(QPointF(5, height()), QPointF(5, height() - 5));
    {
        grad3.setColorAt(0, color1);
        grad3.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad4(QPointF(0, height() - 5), QPointF(5, height() - 5));
    {
        grad4.setColorAt(0, color1);
        grad4.setColorAt(1, color2);
    }
    /* Right line: */
    QLinearGradient grad5(QPointF(width(), height() - 5), QPointF(width() - 5, height() - 5));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(5, 0, width() - 5 * 2, height() - 5), color0); // background
    painter.fillRect(QRect(0,           height() - 5, 5, 5), grad1); // left corner
    painter.fillRect(QRect(width() - 5, height() - 5, 5, 5), grad2); // right corner
    painter.fillRect(QRect(5, height() - 5, width() - 5 * 2, 5), grad3); // bottom line
    painter.fillRect(QRect(0,           0, 5, height() - 5), grad4); // left line
    painter.fillRect(QRect(width() - 5, 0, 5, height() - 5), grad5); // right line

#if defined(Q_WS_WIN) || defined(Q_WS_X11)
    /* Paint frames: */
    painter.save();
    painter.setPen(color3);
    painter.drawLine(QLine(QPoint(5 + 1, 0),                                  QPoint(5 + 1, height() - 1 - 5 - 1)));
    painter.drawLine(QLine(QPoint(5 + 1, height() - 1 - 5 - 1),               QPoint(width() - 1 - 5 - 1, height() - 1 - 5 - 1)));
    painter.drawLine(QLine(QPoint(width() - 1 - 5 - 1, height() - 1 - 5 - 1), QPoint(width() - 1 - 5 - 1, 0)));
    painter.restore();
#endif /* Q_WS_WIN || Q_WS_X11 */
}


UIMenuBarEditorWindow::UIMenuBarEditorWindow(UIMachineWindow *pParent, UIActionPool *pActionPool)
#ifndef Q_WS_MAC
    : UISlidingToolBar(pParent, pParent->menuBar(), new UIMenuBarEditorWidget(pActionPool), UISlidingToolBar::Position_Top)
#else /* Q_WS_MAC */
    : UISlidingToolBar(pParent, 0, new UIMenuBarEditorWidget(pActionPool), UISlidingToolBar::Position_Top)
#endif /* Q_WS_MAC */
{
}

#include "UIMenuBarEditorWindow.moc"

