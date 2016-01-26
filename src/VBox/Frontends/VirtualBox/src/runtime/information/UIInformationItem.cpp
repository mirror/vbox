/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationItem class definition.
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
# include <QDebug>
# include <QPainter>
# include <QApplication>

/* GUI includes: */
# include "UIInformationItem.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIInformationItem::UIInformationItem(QObject *pParent)
    : QStyledItemDelegate(pParent)
    , m_fontMetrics(QApplication::fontMetrics())
    , m_iMinimumLeftColumnWidth(300)
    , m_textpaneheight(20)
{
}

void UIInformationItem::setIcon(const QIcon &icon) const
{
    /* Cache icon: */
    if (icon.isNull())
    {
        /* No icon provided: */
        m_pixmapSize = QSize();
        m_pixmap = QPixmap();
    }
    else
    {
        /* Determine default the icon size: */
        const QStyle *pStyle = QApplication::style();
        const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
        m_pixmapSize = QSize(iIconMetric, iIconMetric);
        /* Acquire the icon of the corresponding size: */
        m_pixmap = icon.pixmap(m_pixmapSize);
    }
}

void UIInformationItem::setName(const QString &strName) const
{
    /* Cache name: */
    m_strName = strName;
    m_nameSize = QSize(m_fontMetrics.width(m_strName), m_fontMetrics.height());
}

const UITextTable& UIInformationItem::text() const
{
    return m_text;
}

void UIInformationItem::setText(const UITextTable &text) const
{
    /* Clear text: */
    m_text.clear();

    /* For each the line of the passed table: */
    foreach (const UITextTableLine &line, text)
    {
        /* Lines: */
        QString strLeftLine = line.first;
        QString strRightLine = line.second;

        /* If 2nd line is NOT empty: */
        if (!strRightLine.isEmpty())
        {
            /* Take both lines 'as is': */
            m_text << UITextTableLine(strLeftLine, strRightLine);
        }
        /* If 2nd line is empty: */
        else
        {
            /* Parse the 1st one to sub-lines: */
            QStringList subLines = strLeftLine.split(QRegExp("\\n"));
            foreach (const QString &strSubLine, subLines)
                m_text << UITextTableLine(strSubLine, QString());
        }
    }

    /* Update text-layout: */
    updateTextLayout();
}

void UIInformationItem::paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    /* Initialize some necessary variables: */
    const int iMargin = 2;
    const int iSpacing = 5;

    /* Item rect: */
    const QRect optionRect = option.rect;
    /* Palette: */
    const QPalette pal = option.palette;
    /* Font metrics: */
    const QFontMetrics fm = option.fontMetrics;

    const int iPositionX = optionRect.topLeft().x() + iMargin;
    const int iPositionY = optionRect.topLeft().y() + iMargin;

    /* Set Name: */
    m_strName = index.data().toString();
    m_nameSize = QSize(fm.width(m_strName), fm.height());

    /* Set Icon: */
    setIcon(QIcon(index.data(Qt::DecorationRole).toString()));

    /* Paint pixmap: */
    const int iElementPixmapX = iMargin + iPositionX;
    const int iElementPixmapY = iPositionY;
    pPainter->drawPixmap(QRect(QPoint(iElementPixmapX, iElementPixmapY), m_pixmapSize), m_pixmap);

    /* Paint name: */
    const int iNameX = iElementPixmapX + m_pixmapSize.width() + iSpacing;
    const int iNameY = optionRect.topLeft().y();
    QFont namefont = QApplication::font();
    namefont.setWeight(QFont::Bold);
    QPoint namePos(iNameX, iNameY);
    namePos += QPoint(0, fm.ascent());
    const QColor buttonTextColor = pal.color(QPalette::Active, QPalette::ButtonText);
    pPainter->save();
    pPainter->setFont(namefont);
    pPainter->setPen(buttonTextColor);
    pPainter->drawText(namePos, m_strName);
    pPainter->restore();
    setText(index.data(Qt::UserRole+1).value<UITextTable>());

    /* Layout text-pane: */
    const int iTextPaneY = qMax(m_pixmapSize.height(), m_nameSize.height()) + iPositionY;
    /* Draw all the text-layouts: */
    foreach (QTextLayout *pTextLayout, m_leftList)
        pTextLayout->draw(pPainter, QPoint(iNameX, iTextPaneY));
    foreach (QTextLayout *pTextLayout, m_rightList)
        pTextLayout->draw(pPainter, QPoint(iNameX, iTextPaneY));

    m_textpaneheight = m_leftList.count() * fm.lineSpacing();
}

/*QSize UIInformationItem::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const int iWidth = m_nameSize.width() + m_pixmapSize.width() + 10 + m_textpanewidth;
    const int iHeight = qMax(m_pixmapSize.height(), m_nameSize.height())+ m_textpaneheight +20;
    return QSize(iWidth, iHeight);
}*/

QTextLayout* UIInformationItem::buildTextLayout(const QString &strText) const
{
    /* Create layout; */
    QTextLayout *pTextLayout = new QTextLayout(strText);

    /* Configure layout: */
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    pTextLayout->setTextOption(textOption);

    /* Build layout: */
    pTextLayout->beginLayout();
    while (1)
    {
        QTextLine line = pTextLayout->createLine();
        if (!line.isValid())
            break;
    }
    pTextLayout->endLayout();

    /* Return layout: */
    return pTextLayout;
}

void UIInformationItem::updateTextLayout() const
{
    /* Initialize text attributes: */
    int iSpacing = 20;

    /* Clear old text-layouts: */
    while (!m_leftList.isEmpty()) delete m_leftList.takeLast();
    while (!m_rightList.isEmpty()) delete m_rightList.takeLast();

    /* Prepare new text-layouts: */
    /* Left layout: */
    int iTextY = 0;
    foreach (const UITextTableLine &line, m_text)
    {
        if (!line.first.isEmpty())
        {
            m_leftList << buildTextLayout(line.first + ":");
            m_leftList.last()->setPosition(QPointF(0, iTextY));
        }
        /* Indent Y: */
        iTextY += m_fontMetrics.lineSpacing() + 1;
        /* Re-calculate maximum width: */
        m_iMinimumLeftColumnWidth = qMax(m_iMinimumLeftColumnWidth, m_fontMetrics.width(line.first));
    }

    /* Right layout: */
    iTextY = 0;
    foreach (const UITextTableLine &line, m_text)
    {
        if (!line.second.isEmpty())
        {
            m_rightList << buildTextLayout(line.second);
            m_rightList.last()->setPosition(QPointF(0 + m_iMinimumLeftColumnWidth + iSpacing, iTextY));
            m_textpanewidth = qMax(0 + m_iMinimumLeftColumnWidth + iSpacing, m_textpanewidth);
        }

        /* Indent Y: */
        iTextY += m_fontMetrics.lineSpacing() + 1;
    }
}

