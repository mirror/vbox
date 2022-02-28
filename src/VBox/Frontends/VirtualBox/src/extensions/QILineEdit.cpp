/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILineEdit class implementation.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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
#include <QClipboard>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QStyleOptionFrame>

/* GUI includes: */
#include "QILineEdit.h"
#include "UIIconPool.h"

QILineEdit::QILineEdit(QWidget *pParent /* = 0 */)
    : QLineEdit(pParent)
    , m_fAllowToCopyContentsWhenDisabled(false)
    , m_pCopyAction(0)
    , m_pIconLabel(0)
    , m_fMarkForError(false)
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
    setMinimumWidth(featTextWidth(strText).width());
}

void QILineEdit::setFixedWidthByText(const QString &strText)
{
    setFixedWidth(featTextWidth(strText).width());
}

void QILineEdit::mark(bool fError, const QString &strErrorMessage /* = QString() */)
{
    /* Check if something really changed: */
    if (fError == m_fMarkForError && m_strErrorMessage == strErrorMessage)
        return;

    /* Save new values: */
    m_fMarkForError = fError;
    m_strErrorMessage = strErrorMessage;

    /* Update accordingly: */
    if (m_fMarkForError)
    {
        /* Create label if absent: */
        if (!m_pIconLabel)
            m_pIconLabel = new QLabel(this);

        /* Update label content, visibility & position: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * .625;
        const int iShift = height() > iIconMetric ? (height() - iIconMetric) / 2 : 0;
        m_pIconLabel->setPixmap(m_markIcon.pixmap(windowHandle(), QSize(iIconMetric, iIconMetric)));
        m_pIconLabel->setToolTip(m_strErrorMessage);
        m_pIconLabel->move(width() - iIconMetric - iShift, iShift);
        m_pIconLabel->show();
    }
    else
    {
        /* Hide label: */
        if (m_pIconLabel)
            m_pIconLabel->hide();
    }
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

    /* Update error label position: */
    if (m_pIconLabel)
    {
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * .625;
        const int iShift = height() > iIconMetric ? (height() - iIconMetric) / 2 : 0;
        m_pIconLabel->move(width() - iIconMetric - iShift, iShift);
    }
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

    /* Prepare warning icon: */
    m_markIcon = UIIconPool::iconSet(":/status_error_16px.png");
}

QSize QILineEdit::featTextWidth(const QString &strText) const
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    QSize sc(fontMetrics().horizontalAdvance(strText) + 2 * 2,
             fontMetrics().xHeight()                  + 2 * 1);
#else
    QSize sc(fontMetrics().width(strText) + 2 * 2,
             fontMetrics().xHeight()     + 2 * 1);
#endif
    const QSize sa = style()->sizeFromContents(QStyle::CT_LineEdit, &sof, sc, this);

    return sa;
}
