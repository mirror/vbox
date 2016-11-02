/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QIComboBox class implementation.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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
# include <QLineEdit>

/* GUI includes: */
# include "QIComboBox.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


QIComboBox::QIComboBox(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pComboBox(0)
{
    /* Prepare all: */
    prepare();
}

QLineEdit *QIComboBox::lineEdit() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, 0);
    return m_pComboBox->lineEdit();
}

QSize QIComboBox::iconSize() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QSize());
    return m_pComboBox->iconSize();
}

QComboBox::InsertPolicy QIComboBox::insertPolicy() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QComboBox::NoInsert);
    return m_pComboBox->insertPolicy();
}

bool QIComboBox::isEditable() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, false);
    return m_pComboBox->isEditable();
}

int QIComboBox::count() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, 0);
    return m_pComboBox->count();
}

int	QIComboBox::currentIndex() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, -1);
    return m_pComboBox->currentIndex();
}

void QIComboBox::insertItem(int iIndex, const QString &strText, const QVariant &userData /* = QVariant() */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    return m_pComboBox->insertItem(iIndex, strText, userData);
}

void QIComboBox::removeItem(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    return m_pComboBox->removeItem(iIndex);
}

QVariant QIComboBox::itemData(int iIndex, int iRole /* = Qt::UserRole */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QVariant());
    return m_pComboBox->itemData(iIndex, iRole);
}

QIcon QIComboBox::itemIcon(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QIcon());
    return m_pComboBox->itemIcon(iIndex);
}

QString QIComboBox::itemText(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QString());
    return m_pComboBox->itemText(iIndex);
}

void QIComboBox::setIconSize(const QSize &size) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setIconSize(size);
}

void QIComboBox::setInsertPolicy(QComboBox::InsertPolicy policy) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setInsertPolicy(policy);
}

void QIComboBox::setEditable(bool fEditable) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setEditable(fEditable);
}

void QIComboBox::setCurrentIndex(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setCurrentIndex(iIndex);
}

void QIComboBox::setItemData(int iIndex, const QVariant &value, int iRole /* = Qt::UserRole */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemData(iIndex, value, iRole);
}

void QIComboBox::setItemIcon(int iIndex, const QIcon &icon) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemIcon(iIndex, icon);
}

void QIComboBox::setItemText(int iIndex, const QString &strText) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemText(iIndex, strText);
}

void QIComboBox::prepare()
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);

        /* Create combo-box: */
        m_pComboBox = new QComboBox;
        AssertPtrReturnVoid(m_pComboBox);
        {
            /* Add combo-box into layout: */
            pLayout->addWidget(m_pComboBox);
        }
    }
}

