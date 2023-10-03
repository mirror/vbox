/* $Id$ */
/** @file
 * VBox Qt GUI - UIEditor class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QLabel>
#include <QRegularExpression>
#include <QTabWidget>
#include <QTextEdit>

/* GUI includes: */
#include "UIEditor.h"


UIEditor::UIEditor(QTabWidget *pTabWidget)
    : m_pTabWidget(pTabWidget)
{
}

UIEditor::UIEditor(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTabWidget(0)
{
}

void UIEditor::addEditor(UIEditor *pEditor)
{
    m_editors << pEditor;
}

void UIEditor::filterOut(const QString &strFilter)
{
    /* Propagate filter towards all the children: */
    foreach (UIEditor *pEditor, m_editors)
        pEditor->filterOut(strFilter);

    /* Make sure the editor is visible if filter is empty: */
    bool fVisible = strFilter.isEmpty();

    /* If editor still hidden we'll need to make it
     * visible if at least one of children is. */
    if (!fVisible)
    {
        foreach (UIEditor *pEditor, m_editors)
            if (pEditor->isVisibleTo(this))
            {
                fVisible = true;
                break;
            }
    }

    /* If editor still hidden we'll need to make it
     * visible if at least one of descriptions suits filter well: */
    if (!fVisible)
    {
        foreach (const QString &strDescription, description())
            if (strDescription.contains(strFilter, Qt::CaseInsensitive))
            {
                fVisible = true;
                break;
            }
    }

    /* Hide/show this editor special way if it's one of tab-widget tabs: */
    if (m_pTabWidget)
    {
        for (int i = 0; i < m_pTabWidget->count(); ++i)
            if (this == m_pTabWidget->widget(i))
                m_pTabWidget->setTabVisible(i, fVisible);
    }
    /* Otherwise update widget visibility usual way: */
    else
        setVisible(fVisible);
}

QStringList UIEditor::description() const
{
    QStringList result;
    /* Clean <html> tags and &mpersands: */
    QRegularExpression re("<[^>]*>|&");

    /* Adding all the buddy labels we have: */
    foreach (QLabel *pLabel, findChildren<QLabel*>())
        if (pLabel && pLabel->buddy())
            result << pLabel->text().remove(re);

    /* Adding all the button sub-types: */
    foreach (QAbstractButton *pButton, findChildren<QAbstractButton*>())
        if (pButton)
            result << pButton->text().remove(re);

    /* Adding all the horizontal headers of abstract-item-view: */
    foreach (QAbstractItemView *pView, findChildren<QAbstractItemView*>())
        if (pView)
            if (QAbstractItemModel *pModel = pView->model())
                for (int i = 0; i < pModel->columnCount(); ++i)
                    result << pModel->headerData(i, Qt::Horizontal).toString().remove(re);

    /* Adding all the text-edits having description property: */
    foreach (QTextEdit *pTextEdit, findChildren<QTextEdit*>())
        if (pTextEdit)
        {
            const QString strDescription = pTextEdit->property("description").toString();
            if (!strDescription.isEmpty())
                result << strDescription.remove(re);
        }

    return result;
}
