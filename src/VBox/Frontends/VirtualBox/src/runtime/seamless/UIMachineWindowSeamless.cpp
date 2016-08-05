/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineWindowSeamless class implementation.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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
# include <QMenu>
# include <QTimer>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UISession.h"
# include "UIActionPoolRuntime.h"
# include "UIMachineLogicSeamless.h"
# include "UIMachineWindowSeamless.h"
# include "UIMachineView.h"
# if   defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
#  include "UIMachineDefs.h"
#  include "UIMiniToolBar.h"
# elif defined(VBOX_WS_MAC)
#  include "VBoxUtils.h"
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineWindowSeamless::UIMachineWindowSeamless(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    , m_pMiniToolBar(0)
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */
    , m_fWasMinimized(false)
{
}

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowSeamless::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}

void UIMachineWindowSeamless::sltRevokeWindowActivation()
{
    /* Make sure window is visible: */
    if (!isVisible() || isMinimized())
        return;

    /* Revoke stolen activation: */
#ifdef VBOX_WS_X11
    raise();
#endif /* VBOX_WS_X11 */
    activateWindow();
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

void UIMachineWindowSeamless::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* Make sure we have no background
     * until the first one paint-event: */
    setAttribute(Qt::WA_NoSystemBackground);

#ifdef VBOX_WITH_TRANSLUCENT_SEAMLESS
# if defined(VBOX_WS_MAC) && QT_VERSION < 0x050000
    /* Using native API to enable translucent background for the Mac host.
     * - We also want to disable window-shadows which is possible
     *   using Qt::WA_MacNoShadow only since Qt 4.8,
     *   while minimum supported version is 4.7.1 for now: */
    ::darwinSetShowsWindowTransparent(this, true);
# else /* !VBOX_WS_MAC || QT_VERSION >= 0x050000 */
    /* Using Qt API to enable translucent background:
     * - Under Win host Qt conflicts with 3D stuff (black seamless regions).
     * - Under Mac host Qt doesn't allows to disable window-shadows
     *   until version 4.8, but minimum supported version is 4.7.1 for now. */
    setAttribute(Qt::WA_TranslucentBackground);
# endif /* !VBOX_WS_MAC || QT_VERSION >= 0x050000 */
#endif /* VBOX_WITH_TRANSLUCENT_SEAMLESS */

#ifdef VBOX_WITH_MASKED_SEAMLESS
    /* Make sure we have no background
     * until the first one set-region-event: */
    setMask(m_maskGuest);
#endif /* VBOX_WITH_MASKED_SEAMLESS */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /* Prepare mini-toolbar: */
    prepareMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */
}

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowSeamless::prepareMiniToolbar()
{
    /* Make sure mini-toolbar is not restricted: */
    if (!gEDataManager->miniToolbarEnabled(vboxGlobal().managedVMUuid()))
        return;

    /* Create mini-toolbar: */
    m_pMiniToolBar = new UIMiniToolBar(this,
                                       GeometryType_Available,
                                       gEDataManager->miniToolbarAlignment(vboxGlobal().managedVMUuid()),
                                       gEDataManager->autoHideMiniToolbar(vboxGlobal().managedVMUuid()));
    AssertPtrReturnVoid(m_pMiniToolBar);
    {
        /* Configure mini-toolbar: */
        m_pMiniToolBar->addMenus(actionPool()->menus());
        connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()),
                this, SLOT(showMinimized()), Qt::QueuedConnection);
        connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
                actionPool()->action(UIActionIndexRT_M_View_T_Seamless), SLOT(trigger()));
        connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
                actionPool()->action(UIActionIndex_M_Application_S_Close), SLOT(trigger()));
        connect(m_pMiniToolBar, SIGNAL(sigNotifyAboutWindowActivationStolen()),
                this, SLOT(sltRevokeWindowActivation()), Qt::QueuedConnection);
    }
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowSeamless::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    gEDataManager->setAutoHideMiniToolbar(m_pMiniToolBar->autoHide(), vboxGlobal().managedVMUuid());
    /* Delete mini-toolbar: */
    delete m_pMiniToolBar;
    m_pMiniToolBar = 0;
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

void UIMachineWindowSeamless::cleanupVisualState()
{
#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /* Cleanup mini-toolbar: */
    cleanupMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowSeamless::placeOnScreen()
{
    /* Get corresponding host-screen: */
    const int iHostScreen = qobject_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* And corresponding working area: */
    const QRect workingArea = gpDesktop->availableGeometry(iHostScreen);

    /* Set appropriate geometry for window: */
    resize(workingArea.size());
    move(workingArea.topLeft());
}

void UIMachineWindowSeamless::showInNecessaryMode()
{
    /* Make sure window has seamless logic: */
    UIMachineLogicSeamless *pSeamlessLogic = qobject_cast<UIMachineLogicSeamless*>(machineLogic());
    AssertPtrReturnVoid(pSeamlessLogic);

    /* If window shouldn't be shown or mapped to some host-screen: */
    if (!uisession()->isScreenVisible(m_uScreenId) ||
        !pSeamlessLogic->hasHostScreenForGuestScreen(m_uScreenId))
    {
        /* Remember whether the window was minimized: */
        if (isMinimized())
            m_fWasMinimized = true;

        /* Hide window and reset it's state to NONE: */
        setWindowState(Qt::WindowNoState);
        hide();
    }
    /* If window should be shown and mapped to some host-screen: */
    else
    {
        /* Check whether window was minimized: */
        const bool fWasMinimized = isMinimized() && isVisible();
        /* And reset it's state in such case before exposing: */
        if (fWasMinimized)
            setWindowState(Qt::WindowNoState);

        /* Make sure window have appropriate geometry: */
        placeOnScreen();

        /* Show window in normal mode: */
        show();

        /* Restore minimized state if necessary: */
        if (m_fWasMinimized || fWasMinimized)
        {
            m_fWasMinimized = false;
            QMetaObject::invokeMethod(this, "showMinimized", Qt::QueuedConnection);
        }

        /* Adjust machine-view size if necessary: */
        adjustMachineViewSize();

        /* Make sure machine-view have focus: */
        m_pMachineView->setFocus();
    }
}

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowSeamless::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update mini-toolbar: */
    if (iElement & UIVisualElement_MiniToolBar)
    {
        /* If there is a mini-toolbar: */
        if (m_pMiniToolBar)
        {
            /* Get snapshot(s): */
            QString strSnapshotName;
            if (machine().GetSnapshotCount() > 0)
            {
                CSnapshot snapshot = machine().GetCurrentSnapshot();
                strSnapshotName = " (" + snapshot.GetName() + ")";
            }
            /* Update mini-toolbar text: */
            m_pMiniToolBar->setText(machineName() + strSnapshotName);
        }
    }
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_WIN
# if QT_VERSION >= 0x050000
void UIMachineWindowSeamless::showEvent(QShowEvent *pEvent)
{
    /* Expose workaround again,
     * Qt devs will never fix that it seems.
     * This time they forget to set 'Mapped'
     * attribute for initially frame-less window. */
    setAttribute(Qt::WA_Mapped);

    /* Call to base-class: */
    UIMachineWindow::showEvent(pEvent);
}
# endif /* QT_VERSION >= 0x050000 */
#endif /* VBOX_WS_WIN */

#ifdef VBOX_WITH_MASKED_SEAMLESS
void UIMachineWindowSeamless::setMask(const QRegion &maskGuest)
{
    /* Remember new guest mask: */
    m_maskGuest = maskGuest;

    /* Prepare full mask: */
    QRegion maskFull(m_maskGuest);

    /* Shift full mask if left or top spacer width is NOT zero: */
    if (m_pLeftSpacer->geometry().width() || m_pTopSpacer->geometry().height())
        maskFull.translate(m_pLeftSpacer->geometry().width(), m_pTopSpacer->geometry().height());

    /* Seamless-window for empty full mask should be empty too,
     * but the QWidget::setMask() wrapper doesn't allow this.
     * Instead, we see a full guest-screen of available-geometry size.
     * So we have to make sure full mask have at least one pixel. */
    if (maskFull.isEmpty())
        maskFull += QRect(0, 0, 1, 1);

    /* Make sure full mask had changed: */
    if (m_maskFull != maskFull)
    {
        /* Compose viewport region to update: */
        QRegion toUpdate = m_maskFull + maskFull;
        /* Remember new full mask: */
        m_maskFull = maskFull;
        /* Assign new full mask: */
        UIMachineWindow::setMask(m_maskFull);
        /* Update viewport region finally: */
        if (m_pMachineView)
            m_pMachineView->viewport()->update(toUpdate);
    }
}
#endif /* VBOX_WITH_MASKED_SEAMLESS */

