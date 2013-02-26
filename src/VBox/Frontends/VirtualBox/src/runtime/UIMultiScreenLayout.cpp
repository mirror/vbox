/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMultiScreenLayout class implementation
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

/* Qt includes: */
#include <QApplication>
#include <QDesktopWidget>
#include <QMenu>

/* GUI includes: */
#include "UIDefs.h"
#include "UIMultiScreenLayout.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UISession.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"
#include "CConsole.h"
#include "CMachine.h"
#include "CDisplay.h"

UIMultiScreenLayout::UIMultiScreenLayout(UIMachineLogic *pMachineLogic)
    : m_pMachineLogic(pMachineLogic)
    , m_pViewMenu(0)
{
    /* Calculate host/guest screen count: */
    calculateHostMonitorCount();
    calculateGuestScreenCount();
}

UIMultiScreenLayout::~UIMultiScreenLayout()
{
    /* Cleanup view-menu: */
    cleanupViewMenu();
}

void UIMultiScreenLayout::setViewMenu(QMenu *pViewMenu)
{
    /* Assign view-menu: */
    m_pViewMenu = pViewMenu;
    /* Prepare view-menu: */
    prepareViewMenu();
}

void UIMultiScreenLayout::update()
{
    CMachine machine = m_pMachineLogic->session().GetMachine();
    /* Make a pool of available host screens. */
    QList<int> availableScreens;
    for (int i = 0; i < m_cHostScreens; ++i)
        availableScreens << i;
    /* Load all combinations stored in the settings file. We have to make sure
     * they are valid, which means there have to be unique combinations and all
     * guests screens need there own host screen. */
    QDesktopWidget *pDW = QApplication::desktop();
    for (int i = 0; i < m_cGuestScreens; ++i)
    {
        /* If the user ever selected a combination in the view menu, we have the following entry: */
        QString strTest = machine.GetExtraData(QString("%1%2").arg(GUI_VirtualScreenToHostScreen).arg(i));
        bool fOk;
        int cScreen = strTest.toInt(&fOk);
        /* Check if valid: */
        if (!(   fOk /* Valid data */
              && cScreen >= 0 && cScreen < m_cHostScreens /* In the host screen bounds? */
              && m_screenMap.key(cScreen, -1) == -1)) /* Not taken already? */
        {
            /* If not, check the position of the guest window in normal mode.
             * This makes sure that on first use the window opens on the same
             * screen as the normal window was before. This even works with
             * multi-screen. The user just have to move all the normal windows
             * to the target screens and they will magically open there in
             * seamless/fullscreen also. */
            QString strTest1 = machine.GetExtraData(GUI_LastNormalWindowPosition + (i > 0 ? QString::number(i): ""));
            QRegExp posParser("(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)");
            if (posParser.exactMatch(strTest1))
            {
                /* If parsing was successfully, convert it to a position. */
                bool fOk1, fOk2;
                QPoint p(posParser.cap(1).toInt(&fOk1), posParser.cap(2).toInt(&fOk2));
                /* Check to which screen the position belongs. */
                cScreen = pDW->screenNumber(p);
                if (!(   fOk1 /* Valid data */
                      && fOk2 /* Valid data */
                      && cScreen >= 0 && cScreen < m_cHostScreens /* In the host screen bounds? */
                      && m_screenMap.key(cScreen, -1) == -1)) /* Not taken already? */
                    /* If not, simply pick the next one of the still available
                     * host screens. */
                    cScreen = availableScreens.first();
            }
            else
                /* If not, simply pick the next one of the still available host
                 * screens. */
                cScreen = availableScreens.first();
        }
        m_screenMap.insert(i, cScreen);
        /* Remove the just selected screen from the list of available screens. */
        availableScreens.removeOne(cScreen);
    }

    /* Update menu actions: */
    updateMenuActions(false);
}

int UIMultiScreenLayout::hostScreenCount() const
{
    return m_cHostScreens;
}

int UIMultiScreenLayout::guestScreenCount() const
{
    return m_cGuestScreens;
}

int UIMultiScreenLayout::hostScreenForGuestScreen(int iScreenId) const
{
    return m_screenMap.value(iScreenId, 0);
}

quint64 UIMultiScreenLayout::memoryRequirements() const
{
    return memoryRequirements(m_screenMap);
}

bool UIMultiScreenLayout::isHostTaskbarCovert() const
{
    /* Check for all screens which are in use if they have some
     * taskbar/menubar/dock on it. Its done by comparing the available with the
     * screen geometry. Only if they are the same for all screens, there are no
     * host area covert. This is a little bit ugly, but there seems no other
     * way to find out if we are on a screen where the taskbar/dock or whatever
     * is present. */
    QDesktopWidget *pDW = QApplication::desktop();
    for (int i = 0; i < m_screenMap.size(); ++i)
    {
        int hostScreen = m_screenMap.value(i);
        if (pDW->availableGeometry(hostScreen) != pDW->screenGeometry(hostScreen))
            return true;
    }
    return false;
}

void UIMultiScreenLayout::sltScreenLayoutChanged(QAction *pAction)
{
    int a = pAction->data().toInt();
    int cGuestScreen = RT_LOWORD(a);
    int cHostScreen = RT_HIWORD(a);

    CMachine machine = m_pMachineLogic->session().GetMachine();
    QMap<int,int> tmpMap(m_screenMap);
    /* Search for the virtual screen which is currently displayed on the
     * requested host screen. When there is one found, we swap both. */
    int r = tmpMap.key(cHostScreen, -1);
    if (r != -1)
        tmpMap.insert(r, tmpMap.value(cGuestScreen));
    /* Set the new host screen */
    tmpMap.insert(cGuestScreen, cHostScreen);

    bool fSuccess = true;
    if (m_pMachineLogic->uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */
            * _1M /* MiB to bytes */
            * 8; /* to bits */
        quint64 usedBits = memoryRequirements(tmpMap);

        fSuccess = availBits >= usedBits;
        if (!fSuccess)
        {
            /* We have to little video memory for the new layout, so say it to the
             * user and revert all changes. */
            if (m_pMachineLogic->visualStateType() == UIVisualStateType_Seamless)
                msgCenter().cannotSwitchScreenInSeamless((((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            else
                fSuccess = msgCenter().cannotSwitchScreenInFullscreen((((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M) != QIMessageBox::Cancel;
        }
    }
    if (fSuccess)
    {
        /* Swap the temporary with the previous map. */
        m_screenMap = tmpMap;
    }

    /* Update menu actions: */
    updateMenuActions(true);

    /* On success inform the observer. */
    if (fSuccess)
        emit sigScreenLayoutChanged();
}

void UIMultiScreenLayout::calculateHostMonitorCount()
{
#if (QT_VERSION >= 0x040600)
    m_cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    m_cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */
}

void UIMultiScreenLayout::calculateGuestScreenCount()
{
    CMachine machine = m_pMachineLogic->session().GetMachine();
    m_cGuestScreens = machine.GetMonitorCount();
}

void UIMultiScreenLayout::prepareViewMenu()
{
    /* Make sure view-menu was set: */
    if (!m_pViewMenu)
        return;

    /* Cleanup menu first: */
    cleanupViewMenu();

    /* If we do have more than one host screen: */
    if (m_cHostScreens > 1)
    {
        for (int i = 0; i < m_cGuestScreens; ++i)
        {
            m_screenMenuList << m_pViewMenu->addMenu(tr("Virtual Screen %1").arg(i + 1));
            m_screenMenuList.last()->menuAction()->setData(true);
            QActionGroup *pScreenGroup = new QActionGroup(m_screenMenuList.last());
            pScreenGroup->setExclusive(true);
            connect(pScreenGroup, SIGNAL(triggered(QAction*)), this, SLOT(sltScreenLayoutChanged(QAction*)));
            for (int a = 0; a < m_cHostScreens; ++a)
            {
                QAction *pAction = pScreenGroup->addAction(tr("Use Host Screen %1").arg(a + 1));
                pAction->setCheckable(true);
                pAction->setData(RT_MAKE_U32(i, a));
            }
            m_screenMenuList.last()->addActions(pScreenGroup->actions());
        }
    }
}

void UIMultiScreenLayout::cleanupViewMenu()
{
    /* Make sure view-menu was set: */
    if (!m_pViewMenu)
        return;

    /* Cleanup view-menu actions: */
    while (!m_screenMenuList.isEmpty())
        delete m_screenMenuList.takeFirst();
}

void UIMultiScreenLayout::updateMenuActions(bool fWithSave)
{
    /* Get the list of all view-menu actions: */
    QList<QAction*> viewMenuActions = gActionPool->action(UIActionIndexRuntime_Menu_View)->menu()->actions();
    /* Get the list of all view related actions: */
    QList<QAction*> viewActions;
    for (int i = 0; i < viewMenuActions.size(); ++i)
        if (viewMenuActions[i]->data().toBool())
            viewActions << viewMenuActions[i];
    /* Update view actions: */
    CMachine machine = m_pMachineLogic->session().GetMachine();
    for (int i = 0; i < viewActions.size(); ++i)
    {
        int iHostScreen = m_screenMap.value(i);
        if (fWithSave)
            machine.SetExtraData(QString("%1%2").arg(GUI_VirtualScreenToHostScreen).arg(i), QString::number(iHostScreen));
        QList<QAction*> screenActions = viewActions.at(i)->menu()->actions();
        /* Update screen actions: */
        for (int j = 0; j < screenActions.size(); ++j)
        {
            QAction *pTmpAction = screenActions.at(j);
            pTmpAction->blockSignals(true);
            pTmpAction->setChecked(RT_HIWORD(pTmpAction->data().toInt()) == iHostScreen);
            pTmpAction->blockSignals(false);
        }
    }
}

quint64 UIMultiScreenLayout::memoryRequirements(const QMap<int, int> &screenLayout) const
{
    ULONG width = 0;
    ULONG height = 0;
    ULONG guestBpp = 0;
    quint64 usedBits = 0;
    CDisplay display = m_pMachineLogic->uisession()->session().GetConsole().GetDisplay();
    for (int i = 0; i < m_cGuestScreens; ++ i)
    {
        QRect screen;
        if (m_pMachineLogic->visualStateType() == UIVisualStateType_Seamless)
            screen = QApplication::desktop()->availableGeometry(screenLayout.value(i, 0));
        else
            screen = QApplication::desktop()->screenGeometry(screenLayout.value(i, 0));
        display.GetScreenResolution(i, width, height, guestBpp);
        usedBits += screen.width() * /* display width */
                    screen.height() * /* display height */
                    guestBpp + /* guest bits per pixel */
                    _1M * 8; /* current cache per screen - may be changed in future */
    }
    usedBits += 4096 * 8; /* adapter info */
    return usedBits;
}

