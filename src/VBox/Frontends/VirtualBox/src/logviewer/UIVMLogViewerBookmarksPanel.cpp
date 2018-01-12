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
# include <QPushButton>
# include <QSpacerItem>
/* GUI includes: */
# include "QIToolButton.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerBookmarksPanel::UIVMLogViewerBookmarksPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer)
: UIVMLogViewerPanel(pParent, pViewer)
    , m_iMaxBookmarkTextLength(60)
    , m_pBookmarksComboBox(0)
    , m_pDeleteAllButton(0)
    , m_pDeleteCurrentButton(0)
    , m_pSpacerItem(0)
{
    prepare();
}

void UIVMLogViewerBookmarksPanel::updateBookmarkList(const QVector<QPair<int, QString> > &bookmarkVector)
{
    if (!m_pBookmarksComboBox || !viewer())
        return;

    m_pBookmarksComboBox->clear();
    QStringList bList;
    bList << "Bookmarks List";
    for(int i = 0; i < bookmarkVector.size(); ++i)
    {
        QString strItem = QString("BookMark %1 at Line %2: %3").arg(QString::number(i)).
            arg(QString::number(bookmarkVector.at(i).first)).arg(bookmarkVector.at(i).second);

        if (strItem.length() > m_iMaxBookmarkTextLength)
        {
            strItem.resize(m_iMaxBookmarkTextLength);
            strItem.replace(m_iMaxBookmarkTextLength, 3, QString("..."));
        }
        bList << strItem;
    }
    m_pBookmarksComboBox->addItems(bList);
    /* Goto last item of the combobox: */
    m_pBookmarksComboBox->setCurrentIndex(m_pBookmarksComboBox->count()-1);
}

void UIVMLogViewerBookmarksPanel::setBookmarkIndex(int index)
{
    if (!m_pBookmarksComboBox)
        return;
    /* If there is only Title of the combo, then goto that item: */
    if (m_pBookmarksComboBox->count() == 1 || index >= m_pBookmarksComboBox->count())
    {
        m_pBookmarksComboBox->setCurrentIndex(0);
        return;
    }
    /* index+1 since we always have a 0th item in our combo box. */
    m_pBookmarksComboBox->setCurrentIndex(index+1);
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
    /* Make sure we have 0th item in our combo box. */
    m_pBookmarksComboBox->insertItem(0, "Bookmarks List");


    mainLayout()->addWidget(m_pBookmarksComboBox, 2/*, Qt::AlignLeft*/);

    m_pDeleteCurrentButton = new QIToolButton(this);
    m_pDeleteCurrentButton->setIcon(m_pDeleteCurrentButton->style()->standardIcon(QStyle::SP_TitleBarCloseButton));

    AssertPtrReturnVoid(m_pBookmarksComboBox);
    mainLayout()->addWidget(m_pDeleteCurrentButton, 0);

    m_pDeleteAllButton = new QPushButton(this);
    AssertPtrReturnVoid(m_pDeleteAllButton);
    mainLayout()->addWidget(m_pDeleteAllButton, 0);


    m_pSpacerItem = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    AssertPtrReturnVoid(m_pSpacerItem);
    mainLayout()->addItem(m_pSpacerItem);
}

void UIVMLogViewerBookmarksPanel::prepareConnections()
{
    connect(m_pBookmarksComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UIVMLogViewerBookmarksPanel::sltBookmarkSelected);

    connect(m_pDeleteAllButton, &QPushButton::clicked, this, &UIVMLogViewerBookmarksPanel::sigDeleteAllBookmarks);
    connect(m_pDeleteCurrentButton, &QIToolButton::clicked, this, &UIVMLogViewerBookmarksPanel::sltDeleteCurrentBookmark);
}


void UIVMLogViewerBookmarksPanel::retranslateUi()
{
    if (m_pDeleteCurrentButton)
        m_pDeleteCurrentButton->setToolTip(UIVMLogViewerWidget::tr("Delete the current bookmark."));
    if (m_pDeleteAllButton)
    {
        m_pDeleteAllButton->setToolTip(UIVMLogViewerWidget::tr("Delete all bookmarks."));
        m_pDeleteAllButton->setText(UIVMLogViewerWidget::tr("Delete all"));
    }
    UIVMLogViewerPanel::retranslateUi();
}

void UIVMLogViewerBookmarksPanel::sltDeleteCurrentBookmark()
{
    if (!m_pBookmarksComboBox)
        return;

    if (m_pBookmarksComboBox->currentIndex() == 0)
        return;
    emit sigDeleteBookmark(m_pBookmarksComboBox->currentIndex() - 1);
}

void UIVMLogViewerBookmarksPanel::sltBookmarkSelected(int index)
{
    /* Do nothing if the index is 0, that is combo box title item: */
    if (index <= 0)
        return;
   emit sigBookmarkSelected(index - 1);
}
