/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
# include <QComboBox>
# include <QHBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QLabel>
# include <QLineEdit>
# include <QPlainTextEdit>
# include <QPushButton>
# include <QTextCursor>
# include <QToolButton>
# include <QScrollArea>

/* GUI includes: */
# include "UIIconPool.h"
# include "UISpecialControls.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerWidget.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerBookmarksPanel::UIVMLogViewerBookmarksPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_iMaxBookmarkTextLength(60)
    , m_pViewer(pViewer)
    , m_pMainLayout(0)
    , m_pCloseButton(0)
    , m_pBookmarksComboBox(0)
{
    prepare();
}

void UIVMLogViewerBookmarksPanel::update()
{
    if(!m_pBookmarksComboBox)
        return;
    const QVector<QPair<int, QString> > *bookmarkVector = m_pViewer->currentBookmarkVector();
    if(!bookmarkVector)
        return;

    m_pBookmarksComboBox->clear();
    QStringList bList;
    for(int i = 0; i < bookmarkVector->size(); ++i)
    {
        QString strItem = QString("BookMark %1 at Line %2: %3").arg(QString::number(i)).
            arg(QString::number(bookmarkVector->at(i).first)).arg(bookmarkVector->at(i).second);

        if(strItem.length() > m_iMaxBookmarkTextLength)
        {
            strItem.resize(m_iMaxBookmarkTextLength);
            strItem.replace(m_iMaxBookmarkTextLength, 3, QString("..."));
        }
        bList << strItem;
    }
    m_pBookmarksComboBox->addItems(bList);
}

void UIVMLogViewerBookmarksPanel::setBookmarkIndex(int index)
{
    if (!m_pBookmarksComboBox)
        return;
    if (index >= m_pBookmarksComboBox->count())
        return;
    m_pBookmarksComboBox->setCurrentIndex(index);
}

void UIVMLogViewerBookmarksPanel::prepare()
{
    prepareWidgets();
    prepareConnections();
    retranslateUi();
}

void UIVMLogViewerBookmarksPanel::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pMainLayout->setSpacing(4);

    m_pCloseButton = new UIMiniCancelButton(this);
    AssertPtrReturnVoid(m_pCloseButton);
    m_pMainLayout->addWidget(m_pCloseButton, 0, Qt::AlignLeft);

    m_pBookmarksComboBox = new QComboBox(this);
    QFontMetrics fontMetrics = m_pBookmarksComboBox->fontMetrics();
    AssertPtrReturnVoid(m_pBookmarksComboBox);
    m_pBookmarksComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_pBookmarksComboBox->setMaximumWidth(fontMetrics.width('a') * (m_iMaxBookmarkTextLength + 2));
    printf("max max %d\n", fontMetrics.width('a') * (m_iMaxBookmarkTextLength + 2));
    m_pMainLayout->addWidget(m_pBookmarksComboBox, 2, Qt::AlignLeft);

}

void UIVMLogViewerBookmarksPanel::prepareConnections()
{
    connect(m_pCloseButton, &UIMiniCancelButton::clicked, this, &UIVMLogViewerBookmarksPanel::hide);

}


void UIVMLogViewerBookmarksPanel::retranslateUi()
{
    m_pCloseButton->setToolTip(UIVMLogViewerWidget::tr("Close the search panel."));
}

bool UIVMLogViewerBookmarksPanel::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Process key press only: */
    case QEvent::KeyPress:
        {
            break;
        }
    default:
        break;
    }
    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

/** Handles the Qt show @a pEvent. */
void UIVMLogViewerBookmarksPanel::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::showEvent(pEvent);
}

/** Handles the Qt hide @a pEvent. */
void UIVMLogViewerBookmarksPanel::hideEvent(QHideEvent *pEvent)
{
    /* Get focused widget: */
    QWidget *pFocus = QApplication::focusWidget();
    /* If focus-widget is valid and child-widget of search-panel,
     * focus next child-widget in line: */
    if (pFocus && pFocus->parent() == this)
        focusNextPrevChild(true);
    /* Call to base-class: */
    QWidget::hideEvent(pEvent);
}
