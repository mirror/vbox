/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowScale class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QDesktopWidget>
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"

#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineView.h"
#include "UIMachineWindowScale.h"
#ifdef Q_WS_MAC
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

UIMachineWindowScale::UIMachineWindowScale(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : QIWithRetranslateUI2<QMainWindow>(0, Qt::Window)
    , UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
{
    /* "This" is machine window: */
    m_pMachineWindow = this;

    /* Set the main window in VBoxGlobal */
    if (uScreenId == 0)
        vboxGlobal().setMainWindow(this);

    /* Prepare window icon: */
    prepareWindowIcon();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Prepare menu: */
    prepareMenu();

    /* Retranslate normal window finally: */
    retranslateUi();

    /* Prepare normal machine view container: */
    prepareMachineViewContainer();

    /* Prepare normal machine view: */
    prepareMachineView();

    /* Prepare handlers: */
    prepareHandlers();

    /* Load normal window settings: */
    loadWindowSettings();

    /* Update all the elements: */
    updateAppearanceOf(UIVisualElement_AllStuff);

#ifdef Q_WS_MAC
    /* Install the resize delegate for keeping the aspect ratio. */
    ::darwinInstallResizeDelegate(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* Q_WS_MAC */

    /* Show window: */
    showSimple();
}

UIMachineWindowScale::~UIMachineWindowScale()
{
#ifdef Q_WS_MAC
    /* Uninstall the resize delegate for keeping the aspect ratio. */
    ::darwinUninstallResizeDelegate(this);
#endif /* Q_WS_MAC */

    /* Save normal window settings: */
    saveWindowSettings();

    /* Prepare handlers: */
    cleanupHandlers();

    /* Cleanup normal machine view: */
    cleanupMachineView();
}

void UIMachineWindowScale::sltMachineStateChanged()
{
    UIMachineWindow::sltMachineStateChanged();
}

void UIMachineWindowScale::sltPopupMainMenu()
{
    /* Popup main menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(machineWindow()->geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltSelectFirstAction()));
    }
}

void UIMachineWindowScale::sltTryClose()
{
    UIMachineWindow::sltTryClose();
}

void UIMachineWindowScale::retranslateUi()
{
    /* Translate parent class: */
    UIMachineWindow::retranslateUi();
}

bool UIMachineWindowScale::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximizedChecked())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position */
                updateDbgWindows();
#endif
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position */
                updateDbgWindows();
#endif
            }
            break;
        }
        default:
            break;
    }
    return QIWithRetranslateUI2<QMainWindow>::event(pEvent);
}

#ifdef Q_WS_WIN
bool UIMachineWindowScale::winEvent(MSG *pMessage, long *pResult)
{
    /* Try to keep aspect ratio during window resize if:
     * 1. machine view exists and 2. event-type is WM_SIZING and 3. shift key is NOT pressed: */
    if (machineView() && pMessage->message == WM_SIZING && !(QApplication::keyboardModifiers() & Qt::ShiftModifier))
    {
        if (double dAspectRatio = machineView()->aspectRatio())
        {
            RECT *pRect = reinterpret_cast<RECT*>(pMessage->lParam);
            switch (pMessage->wParam)
            {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                {
                    pRect->right = pRect->left + (double)(pRect->bottom - pRect->top) * dAspectRatio;
                    break;
                }
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                {
                    pRect->top = pRect->bottom - (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                default:
                    break;
            }
        }
    }
    /* Pass event to base-class: */
    return QMainWindow::winEvent(pMessage, pResult);
}
#endif /* Q_WS_WIN */

#ifdef Q_WS_X11
bool UIMachineWindowScale::x11Event(XEvent *pEvent)
{
    return UIMachineWindow::x11Event(pEvent);
}
#endif

void UIMachineWindowScale::closeEvent(QCloseEvent *pEvent)
{
    return UIMachineWindow::closeEvent(pEvent);
}

void UIMachineWindowScale::prepareMenu()
{
#ifdef Q_WS_MAC
    setMenuBar(uisession()->newMenuBar());
#endif /* Q_WS_MAC */
    m_pMainMenu = uisession()->newMenu();
}

void UIMachineWindowScale::prepareMachineViewContainer()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMachineViewContainer();

    /* Strict spacers to hide them, they are not necessary for scale-mode: */
    m_pTopSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pBottomSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pLeftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pRightSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void UIMachineWindowScale::prepareMachineView()
{
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Need to force the QGL framebuffer in case 2D Video Acceleration is supported & enabled: */
    bool bAccelerate2DVideo = session().GetMachine().GetAccelerate2DVideoEnabled() && VBoxGlobal::isAcceleration2DVideoAvailable();
#endif

    /* Set central widget: */
    setCentralWidget(new QWidget);

    /* Set central widget layout: */
    centralWidget()->setLayout(m_pMachineViewContainer);

    m_pMachineView = UIMachineView::create(  this
                                           , m_uScreenId
                                           , machineLogic()->visualStateType()
#ifdef VBOX_WITH_VIDEOHWACCEL
                                           , bAccelerate2DVideo
#endif
                                           );

    /* Add machine view into layout: */
    m_pMachineViewContainer->addWidget(m_pMachineView, 1, 1);
}

void UIMachineWindowScale::loadWindowSettings()
{
    /* Load scale window settings: */
    CMachine machine = session().GetMachine();

    /* Load extra-data settings: */
    {
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastScaleWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastScaleWindowPosition).arg(m_uScreenId);
        QStringList strPositionSettings = machine.GetExtraDataStringList(strPositionAddress);

        bool ok = !strPositionSettings.isEmpty(), max = false;
        int x = 0, y = 0, w = 0, h = 0;

        if (ok && strPositionSettings.size() > 0)
            x = strPositionSettings[0].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 1)
            y = strPositionSettings[1].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 2)
            w = strPositionSettings[2].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 3)
            h = strPositionSettings[3].toInt(&ok);
        else ok = false;
        if (ok && strPositionSettings.size() > 4)
            max = strPositionSettings[4] == VBoxDefs::GUI_LastWindowState_Max;

        QRect ar = ok ? QApplication::desktop()->availableGeometry(QPoint(x, y)) :
                        QApplication::desktop()->availableGeometry(machineWindow());

        /* If previous parameters were read correctly: */
        if (ok)
        {
            /* Restore window size and position: */
            m_normalGeometry = QRect(x, y, w, h);
            setGeometry(m_normalGeometry);
            /* Maximize if needed: */
            if (max)
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        else
        {
            /* Resize to default size: */
            resize(640, 480);
            qApp->processEvents();
            /* Move newly created window to the screen center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(ar.center());
            setGeometry(m_normalGeometry);
        }
    }
}

void UIMachineWindowScale::saveWindowSettings()
{
    CMachine machine = session().GetMachine();

    /* Save extra-data settings: */
    {
        QString strWindowPosition = QString("%1,%2,%3,%4")
                                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height());
        if (isMaximizedChecked())
            strWindowPosition += QString(",%1").arg(VBoxDefs::GUI_LastWindowState_Max);
        QString strPositionAddress = m_uScreenId == 0 ? QString("%1").arg(VBoxDefs::GUI_LastScaleWindowPosition) :
                                     QString("%1%2").arg(VBoxDefs::GUI_LastScaleWindowPosition).arg(m_uScreenId);
        machine.SetExtraData(strPositionAddress, strWindowPosition);
    }
}

void UIMachineWindowScale::cleanupMachineView()
{
    /* Do not cleanup machine view if it is not present: */
    if (!machineView())
        return;

    UIMachineView::destroy(m_pMachineView);
    m_pMachineView = 0;
}

void UIMachineWindowScale::cleanupMenu()
{
    delete m_pMainMenu;
    m_pMainMenu = 0;
}

void UIMachineWindowScale::showSimple()
{
    /* Just show window: */
    show();
}

bool UIMachineWindowScale::isMaximizedChecked()
{
#ifdef Q_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* Q_WS_MAC */
    return isMaximized();
#endif /* !Q_WS_MAC */
}

