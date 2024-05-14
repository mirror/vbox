/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILineEdit class implementation.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
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
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyleOptionFrame>

/* GUI includes: */
#include "QILineEdit.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIIconPool.h"

/* Other VBox includes: */
#include "iprt/assert.h"


QILineEdit::QILineEdit(QWidget *pParent /* = 0 */)
    : QLineEdit(pParent)
    , m_fAllowToCopyContentsWhenDisabled(false)
    , m_pCopyAction(0)
    , m_pIconLabel(0)
    , m_fMarkForError(false)
    , m_fMarkable(false)
    , m_iIconMargin(0)
{
    prepare();
}

QILineEdit::QILineEdit(const QString &strText, QWidget *pParent /* = 0 */)
    : QLineEdit(strText, pParent)
    , m_fAllowToCopyContentsWhenDisabled(false)
    , m_pCopyAction(0)
    , m_pIconLabel(0)
    , m_fMarkForError(false)
{
    prepare();
}

void QILineEdit::setAllowToCopyContentsWhenDisabled(bool fAllow)
{
    m_fAllowToCopyContentsWhenDisabled = fAllow;
}

void QILineEdit::setMinimumWidthByText(const QString &strText)
{
    setMinimumWidth(fitTextWidth(strText).width());
}

void QILineEdit::setFixedWidthByText(const QString &strText)
{
    setFixedWidth(fitTextWidth(strText).width());
}

void QILineEdit::mark(bool fError, const QString &strErrorMessage, const QString &strNoErrorMessage)
{
    AssertPtrReturnVoid(m_pIconLabel);
    if (!m_fMarkable)
        return;

    const QIcon icon = fError ? UIIconPool::iconSet(":/status_error_16px.png") : UIIconPool::iconSet(":/status_check_16px.png");
    const int iIconMetric = qMin((int)(QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * .625), height());
    const qreal fDevicePixelRatio = gpDesktop->devicePixelRatio(m_pIconLabel);
    const QString strToolTip = fError ? strErrorMessage : strNoErrorMessage;
    const QPixmap iconPixmap = icon.pixmap(QSize(iIconMetric, iIconMetric), fDevicePixelRatio);
    m_pIconLabel->setPixmap(iconPixmap);
    m_pIconLabel->resize(m_pIconLabel->minimumSizeHint());
    m_pIconLabel->setToolTip(strToolTip);
    m_iIconMargin = (height() - m_pIconLabel->height()) / 2;
    update();
}

void QILineEdit::setMarkable(bool fMarkable)
{
    if (m_fMarkable == fMarkable)
        return;
    m_fMarkable = fMarkable;
    update();
}

bool QILineEdit::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::ContextMenu:
        {
            /* For disabled widget if requested: */
            if (!isEnabled() && m_fAllowToCopyContentsWhenDisabled)
            {
                /* Create a context menu for the copy to clipboard action: */
                QContextMenuEvent *pContextMenuEvent = static_cast<QContextMenuEvent*>(pEvent);
                QMenu menu;
                m_pCopyAction->setText(tr("&Copy"));
                menu.addAction(m_pCopyAction);
                menu.exec(pContextMenuEvent->globalPos());
                pEvent->accept();
            }
            break;
        }
        default:
            break;
    }
    return QLineEdit::event(pEvent);
}

void QILineEdit::resizeEvent(QResizeEvent *pResizeEvent)
{
    /* Call to base-class: */
    QLineEdit::resizeEvent(pResizeEvent);
    if (m_fMarkable)
        moveIconLabel();
}

void QILineEdit::moveIconLabel()
{
    AssertPtrReturnVoid(m_pIconLabel);
    m_pIconLabel->move(width() - m_pIconLabel->width() - m_iIconMargin, m_iIconMargin);
}

void QILineEdit::showEvent(QShowEvent *pShowEvent)
{
    /* Call to base-class: */
    QLineEdit::showEvent(pShowEvent);
    if (m_fMarkable)
        moveIconLabel();
}

void QILineEdit::copy()
{
    /* Copy the current text to the global and selection clipboards: */
    QApplication::clipboard()->setText(text(), QClipboard::Clipboard);
    QApplication::clipboard()->setText(text(), QClipboard::Selection);
}

void QILineEdit::prepare()
{
    /* Prepare invisible copy action: */
    m_pCopyAction = new QAction(this);
    if (m_pCopyAction)
    {
        m_pCopyAction->setShortcut(QKeySequence(QKeySequence::Copy));
        m_pCopyAction->setShortcutContext(Qt::WidgetShortcut);
        connect(m_pCopyAction, &QAction::triggered, this, &QILineEdit::copy);
        addAction(m_pCopyAction);
    }

    /* Prepare icon label: */
    m_pIconLabel = new QLabel(this);
}

QSize QILineEdit::fitTextWidth(const QString &strText) const
{
    QStyleOptionFrame sof;
    sof.initFrom(this);
    sof.rect = contentsRect();
    sof.lineWidth = hasFrame() ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth) : 0;
    sof.midLineWidth = 0;
    sof.state |= QStyle::State_Sunken;

    /** @todo make it wise.. */
    // WORKAROUND:
    // The margins are based on qlineedit.cpp of Qt.
    // Maybe they where changed at some time in the future.
    QSize sc(fontMetrics().horizontalAdvance(strText) + 2 * 2,
             fontMetrics().xHeight()                  + 2 * 1);
    const QSize sa = style()->sizeFromContents(QStyle::CT_LineEdit, &sof, sc, this);

    return sa;
}
