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
: UIVMLogViewerPanel(pParent, pViewer)
    , m_iMaxBookmarkTextLength(60)
    , m_pBookmarksComboBox(0)
    , m_clearAllButton(0)
    , m_clearCurrentButton(0)
{
    prepare();
}

void UIVMLogViewerBookmarksPanel::updateBookmarkList()
{
    if (!m_pBookmarksComboBox || !viewer())
        return;
    const QVector<QPair<int, QString> > *bookmarkVector = viewer()->currentBookmarkVector();
    if (!bookmarkVector)
        return;

    m_pBookmarksComboBox->clear();
    QStringList bList;
    for(int i = 0; i < bookmarkVector->size(); ++i)
    {
        QString strItem = QString("BookMark %1 at Line %2: %3").arg(QString::number(i)).
            arg(QString::number(bookmarkVector->at(i).first)).arg(bookmarkVector->at(i).second);

        if (strItem.length() > m_iMaxBookmarkTextLength)
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

void UIVMLogViewerBookmarksPanel::prepareWidgets()
{
    if (!mainLayout())
        return;

    m_pBookmarksComboBox = new QComboBox(this);
    QFontMetrics fontMetrics = m_pBookmarksComboBox->fontMetrics();
    AssertPtrReturnVoid(m_pBookmarksComboBox);
    m_pBookmarksComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_pBookmarksComboBox->setMaximumWidth(fontMetrics.width('a') * (m_iMaxBookmarkTextLength + 2));
    mainLayout()->addWidget(m_pBookmarksComboBox, 2, Qt::AlignLeft);
}

void UIVMLogViewerBookmarksPanel::prepareConnections()
{
}


void UIVMLogViewerBookmarksPanel::retranslateUi()
{
    UIVMLogViewerPanel::retranslateUi();
}
