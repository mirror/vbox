/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QIDialogButtonBox class implementation.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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
# include <QBoxLayout>
# include <QEvent>
# include <QPushButton>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "UISpecialControls.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QIDialogButtonBox::QIDialogButtonBox(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QDialogButtonBox>(pParent)
{
}

QIDialogButtonBox::QIDialogButtonBox(Qt::Orientation enmOrientation, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QDialogButtonBox>(pParent)
{
    setOrientation(enmOrientation);
}

QIDialogButtonBox::QIDialogButtonBox(StandardButtons enmButtonTypes, Qt::Orientation enmOrientation, QWidget *pParent)
   : QIWithRetranslateUI<QDialogButtonBox>(pParent)
{
    setOrientation(enmOrientation);
    setStandardButtons(enmButtonTypes);
    retranslateUi();
}

QPushButton *QIDialogButtonBox::button(StandardButton enmButtonType) const
{
    QPushButton *pButton = QDialogButtonBox::button(enmButtonType);
    if (   !pButton
        && enmButtonType == QDialogButtonBox::Help)
        pButton = m_pHelpButton;
    return pButton;
}

QPushButton *QIDialogButtonBox::addButton(const QString &strText, ButtonRole enmRole)
{
    QPushButton *pButton = QDialogButtonBox::addButton(strText, enmRole);
    retranslateUi();
    return pButton;
}

QPushButton *QIDialogButtonBox::addButton(StandardButton enmButtonType)
{
    QPushButton *pButton = QDialogButtonBox::addButton(enmButtonType);
    retranslateUi();
    return pButton;
}

void QIDialogButtonBox::setStandardButtons(StandardButtons enmButtonTypes)
{
    QDialogButtonBox::setStandardButtons(enmButtonTypes);
    retranslateUi();
}

void QIDialogButtonBox::addExtraWidget(QWidget *pInsertedWidget)
{
    QBoxLayout *pLayout = boxLayout();
    if (pLayout)
    {
        int iIndex = findEmptySpace(pLayout);
        pLayout->insertWidget(iIndex + 1, pInsertedWidget);
        pLayout->insertStretch(iIndex + 2);
    }
}

void QIDialogButtonBox::addExtraLayout(QLayout *pInsertedLayout)
{
    QBoxLayout *pLayout = boxLayout();
    if (pLayout)
    {
        int iIndex = findEmptySpace(pLayout);
        pLayout->insertLayout(iIndex + 1, pInsertedLayout);
        pLayout->insertStretch(iIndex + 2);
    }
}

void QIDialogButtonBox::retranslateUi()
{
    QPushButton *pButton = QDialogButtonBox::button(QDialogButtonBox::Help);
    if (pButton)
    {
        /* Use our very own help button if the user requested for one. */
        if (!m_pHelpButton)
            m_pHelpButton = new UIHelpButton;
        m_pHelpButton->initFrom(pButton);
        removeButton(pButton);
        QDialogButtonBox::addButton(m_pHelpButton, QDialogButtonBox::HelpRole);
    }
}

QBoxLayout *QIDialogButtonBox::boxLayout() const
{
    QBoxLayout *pLayout = qobject_cast<QBoxLayout*>(layout());
    AssertMsg(VALID_PTR(pLayout), ("Layout of the QDialogButtonBox isn't a box layout."));
    return pLayout;
}

int QIDialogButtonBox::findEmptySpace(QBoxLayout *pLayout) const
{
    /* Search for the first occurrence of QSpacerItem and return the index. */
    int i = 0;
    for (; i < pLayout->count(); ++i)
    {
        QLayoutItem *pItem = pLayout->itemAt(i);
        if (pItem && pItem->spacerItem())
            break;
    }
    return i;
}

