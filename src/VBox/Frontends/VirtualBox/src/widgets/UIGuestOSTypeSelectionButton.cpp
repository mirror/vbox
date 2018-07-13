/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSTypeSelectionButton class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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

/* Qt includes */
# include <QMenu>
# include <QSignalMapper>
# include <QStyle>

/* GUI includes */
# include "VBoxGlobal.h"
# include "UIGuestOSTypeSelectionButton.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGuestOSTypeSelectionButton::UIGuestOSTypeSelectionButton(QWidget *pParent)
    : QIWithRetranslateUI<QPushButton>(pParent)
{
    /* Determine icon metric: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    setIconSize(QSize(iIconMetric, iIconMetric));

    /* We have to make sure that the button has strong focus, otherwise
     * the editing is ended when the menu is shown: */
    setFocusPolicy(Qt::StrongFocus);

    /* Create a signal mapper so that we not have to react to
     * every single menu activation ourself: */
    m_pSignalMapper = new QSignalMapper(this);
    if (m_pSignalMapper)
        connect(m_pSignalMapper, static_cast<void(QSignalMapper::*)(const QString &)>(&QSignalMapper::mapped),
                this, &UIGuestOSTypeSelectionButton::setOSTypeId);

    /* Create main menu: */
    m_pMainMenu = new QMenu(pParent);
    if (m_pMainMenu)
        setMenu(m_pMainMenu);

    /* Apply language settings: */
    retranslateUi();
}

bool UIGuestOSTypeSelectionButton::isMenuShown() const
{
    return m_pMainMenu->isVisible();
}

void UIGuestOSTypeSelectionButton::setOSTypeId(const QString &strOSTypeId)
{
    m_strOSTypeId = strOSTypeId;
    CGuestOSType enmType = vboxGlobal().vmGuestOSType(strOSTypeId);

#ifndef VBOX_WS_MAC
    /* Looks ugly on the Mac: */
    setIcon(vboxGlobal().vmGuestOSTypePixmapDefault(enmType.GetId()));
#endif

    setText(enmType.GetDescription());
}

void UIGuestOSTypeSelectionButton::retranslateUi()
{
    populateMenu();
}

void UIGuestOSTypeSelectionButton::populateMenu()
{
    /* Clea initially: */
    m_pMainMenu->clear();

    /* Create a list of all possible OS types: */
    foreach(const QString &strFamilyId, vboxGlobal().vmGuestOSFamilyIDs())
    {
        QMenu *pSubMenu = m_pMainMenu->addMenu(vboxGlobal().vmGuestOSFamilyDescription(strFamilyId));
        foreach (const CGuestOSType &comType, vboxGlobal().vmGuestOSTypeList(strFamilyId))
        {
            QAction *pAction = pSubMenu->addAction(vboxGlobal().vmGuestOSTypePixmapDefault(comType.GetId()),
                                                   comType.GetDescription());
            connect(pAction, &QAction::triggered,
                    m_pSignalMapper, static_cast<void(QSignalMapper::*)(void)>(&QSignalMapper::map));
            m_pSignalMapper->setMapping(pAction, comType.GetId());
        }
    }
}
