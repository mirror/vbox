/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMultiScreenLayout class implementation
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

/* Local includes */
#include "UIMultiScreenLayout.h"
#include "COMDefs.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UISession.h"
#include "VBoxProblemReporter.h"

/* Global includes */
#include <QApplication>
#include <QDesktopWidget>
#include <QMap>
#include <QMenu>

UIMultiScreenLayout::UIMultiScreenLayout(UIMachineLogic *pMachineLogic)
    : m_pMachineLogic(pMachineLogic)
    , m_pScreenMap(new QMap<int, int>())
{
    CMachine machine = m_pMachineLogic->session().GetMachine();
    /* Get host/guest monitor count: */
#if (QT_VERSION >= 0x040600)
    m_cHostScreens = QApplication::desktop()->screenCount();
#else /* (QT_VERSION >= 0x040600) */
    m_cHostScreens = QApplication::desktop()->numScreens();
#endif /* !(QT_VERSION >= 0x040600) */
    m_cGuestScreens = machine.GetMonitorCount();
}

UIMultiScreenLayout::~UIMultiScreenLayout()
{
    delete m_pScreenMap;
}

void UIMultiScreenLayout::initialize(QMenu *pMenu)
{
    pMenu->clear();
    for (int i = 0; i < m_cGuestScreens; ++i)
    {
        QMenu *pScreenMenu = pMenu->addMenu(tr("Virtual Screen %1").arg(i + 1));
        QActionGroup *pScreenGroup = new QActionGroup(pScreenMenu);
        pScreenGroup->setExclusive(true);
        connect(pScreenGroup, SIGNAL(triggered(QAction*)),
                this, SLOT(sltScreenLayoutChanged(QAction*)));
        for (int a = 0; a < m_cHostScreens; ++a)
        {
            QAction *pAction = pScreenGroup->addAction(tr("Use Host Screen %1").arg(a + 1));
            pAction->setCheckable(true);
            pAction->setData(RT_MAKE_U32(i, a));
        }
        pScreenMenu->addActions(pScreenGroup->actions());
    }
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
    for (int i = 0; i < m_cGuestScreens; ++i)
    {
        QString strTest = machine.GetExtraData(QString("%1%2").arg(VBoxDefs::GUI_VirtualScreenToHostScreen).arg(i));
        bool fOk;
        int cScreen = strTest.toInt(&fOk);
        /* Check if valid: */
        if (!(   fOk /* Valid data */
                 && cScreen >= 0 && cScreen < m_cHostScreens /* In the host screen bounds? */
                 && m_pScreenMap->key(cScreen, -1) == -1)) /* Not taken already? */
            /* If not, use one from the available screens */
            cScreen = availableScreens.first();
        m_pScreenMap->insert(i, cScreen);
        /* Remove the current set screen from the list of available screens. */
        availableScreens.removeOne(cScreen);
    }

    QList<QAction*> actions = m_pMachineLogic->actionsPool()->action(UIActionIndex_Menu_View)->menu()->actions();
    for (int i = 0; i < m_pScreenMap->size(); ++i)
    {
        int hostScreen = m_pScreenMap->value(i);
        QList<QAction*> actions1 = actions.at(i)->menu()->actions();
        for (int w = 0; w < actions1.size(); ++w)
        {
            QAction *pTmpAction = actions1.at(w);
            pTmpAction->blockSignals(true);
            pTmpAction->setChecked(RT_HIWORD(pTmpAction->data().toInt()) == hostScreen);
            pTmpAction->blockSignals(false);
        }
    }
}

int UIMultiScreenLayout::hostScreenCount() const
{
    return m_cHostScreens;
}

int UIMultiScreenLayout::guestScreenCount() const
{
    return m_cGuestScreens;
}

int UIMultiScreenLayout::hostScreenForGuestScreen(int screenId) const
{
    return m_pScreenMap->value(screenId, 0);
}

quint64 UIMultiScreenLayout::memoryRequirements() const
{
    return memoryRequirements(m_pScreenMap);
}

void UIMultiScreenLayout::sltScreenLayoutChanged(QAction *pAction)
{
    int a = pAction->data().toInt();
    int cGuestScreen = RT_LOWORD(a);
    int cHostScreen = RT_HIWORD(a);

    CMachine machine = m_pMachineLogic->session().GetMachine();
    QMap<int,int> *pTmpMap = new QMap<int,int>(*m_pScreenMap);
    /* Search for the virtual screen which is currently displayed on the
     * requested host screen. When there is one found, we swap both. */
    int r = pTmpMap->key(cHostScreen, -1);
    if (r != -1)
        pTmpMap->insert(r, pTmpMap->value(cGuestScreen));
    /* Set the new host screen */
    pTmpMap->insert(cGuestScreen, cHostScreen);

    bool fSuccess = true;
    if (m_pMachineLogic->uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */
            * _1M /* MiB to bytes */
            * 8; /* to bits */
        quint64 usedBits = memoryRequirements(pTmpMap);

        fSuccess = availBits >= usedBits;
        if (!fSuccess)
        {
            /* We have to little video memory for the new layout, so say it to the
             * user and revert all changes. */
            if (m_pMachineLogic->visualStateType() == UIVisualStateType_Seamless)
                vboxProblem().cannotSwitchScreenInSeamless((((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M);
            else
                fSuccess = vboxProblem().cannotSwitchScreenInFullscreen((((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M) != QIMessageBox::Cancel;
        }
    }
    if (fSuccess)
    {
        /* Swap the temporary with the previous map. */
        delete m_pScreenMap;
        m_pScreenMap = pTmpMap;
    }

    /* Update the menu items. Even if we can't switch we have to revert the
     * menu items. */
    QList<QAction*> actions = m_pMachineLogic->actionsPool()->action(UIActionIndex_Menu_View)->menu()->actions();
    /* Update the settings. */
    for (int i = 0; i < m_cGuestScreens; ++i)
    {
        int hostScreen = m_pScreenMap->value(i);
        machine.SetExtraData(QString("%1%2").arg(VBoxDefs::GUI_VirtualScreenToHostScreen).arg(i), QString::number(hostScreen));
        QList<QAction*> actions1 = actions.at(i)->menu()->actions();
        for (int w = 0; w < actions1.size(); ++w)
        {
            QAction *pTmpAction = actions1.at(w);
            pTmpAction->blockSignals(true);
            pTmpAction->setChecked(RT_HIWORD(pTmpAction->data().toInt()) == hostScreen);
            pTmpAction->blockSignals(false);
        }
    }

    /* On success inform the observer. */
    if (fSuccess)
        emit screenLayoutChanged();
}

quint64 UIMultiScreenLayout::memoryRequirements(const QMap<int, int> *pScreenLayout) const
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
            screen = QApplication::desktop()->availableGeometry(pScreenLayout->value(i, 0));
        else
            screen = QApplication::desktop()->screenGeometry(pScreenLayout->value(i, 0));
        display.GetScreenResolution(i, width, height, guestBpp);
        usedBits += screen.width() * /* display width */
                    screen.height() * /* display height */
                    guestBpp + /* guest bits per pixel */
                    _1M * 8; /* current cache per screen - may be changed in future */
    }
    usedBits += 4096 * 8; /* adapter info */
    return usedBits;
}

