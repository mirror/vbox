/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxOSTypeSelectorButton class implementation
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

/* VBox includes */
#include "VBoxOSTypeSelectorButton.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QMenu>
#include <QSignalMapper>

VBoxOSTypeSelectorButton::VBoxOSTypeSelectorButton (QWidget *aParent)
  : QIWithRetranslateUI <QPushButton> (aParent)
{
    /* We have to make sure that the button has strong focus, otherwise the
     * editing is ended when the menu is shown */
    setFocusPolicy (Qt::StrongFocus);
    setIconSize (QSize (16, 16));
    /* Create a signal mapper so that we not have to react to every single
     * menu activation ourself. */
    mSignalMapper = new QSignalMapper (this);
    connect (mSignalMapper, SIGNAL (mapped (const QString &)),
             this, SLOT (setOSTypeId (const QString &)));
    mMainMenu = new QMenu (aParent);
    setMenu (mMainMenu);

    retranslateUi();
}

void VBoxOSTypeSelectorButton::setOSTypeId (const QString& aOSTypeId)
{
    mOSTypeId = aOSTypeId;
    CGuestOSType type = vboxGlobal().vmGuestOSType (aOSTypeId);
    /* Looks ugly on the Mac */
#ifndef Q_WS_MAC
    setIcon (vboxGlobal().vmGuestOSTypeIcon (type.GetId()));
#endif /* Q_WS_MAC */
    setText (type.GetDescription());
}

bool VBoxOSTypeSelectorButton::isMenuShown() const
{
    return mMainMenu->isVisible();
}

void VBoxOSTypeSelectorButton::retranslateUi()
{
    populateMenu();
}

void VBoxOSTypeSelectorButton::populateMenu()
{
    mMainMenu->clear();
    /* Create a list of all possible OS types */
    QList <CGuestOSType> families = vboxGlobal().vmGuestOSFamilyList();
    foreach (const CGuestOSType& family, families)
    {
        QMenu *subMenu = mMainMenu->addMenu (family.GetFamilyDescription());
        QList <CGuestOSType> types = vboxGlobal().vmGuestOSTypeList (family.GetFamilyId());
        foreach (const CGuestOSType& type, types)
        {
            QAction *a = subMenu->addAction (vboxGlobal().vmGuestOSTypeIcon (type.GetId()), type.GetDescription());
            connect(a, SIGNAL (triggered()),
                    mSignalMapper, SLOT(map()));
            mSignalMapper->setMapping (a, type.GetId());
        }
    }
}

