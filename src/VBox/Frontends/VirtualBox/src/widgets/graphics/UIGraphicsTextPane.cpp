/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsTextPane and UITask class implementation.
 */

/*
 * Copyright (C) 2012-2014 Oracle Corporation
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
#include <QFontMetrics>
#include <QTextLayout>
#include <QPainter>

/* GUI includes: */
#include "UIGraphicsTextPane.h"

UIGraphicsTextPane::UIGraphicsTextPane(QIGraphicsWidget *pParent, QPaintDevice *pPaintDevice)
    : QIGraphicsWidget(pParent)
    , m_pPaintDevice(pPaintDevice)
    , m_iMargin(0)
    , m_iSpacing(10)
    , m_iMinimumTextColumnWidth(100)
    , m_fMinimumSizeHintInvalidated(true)
    , m_iMinimumTextWidth(0)
    , m_iMinimumTextHeight(0)
{
}

UIGraphicsTextPane::~UIGraphicsTextPane()
{
    /* Clear text-layouts: */
    while (!m_leftList.isEmpty()) delete m_leftList.takeLast();
    while (!m_rightList.isEmpty()) delete m_rightList.takeLast();
}

void UIGraphicsTextPane::setText(const UITextTable &text)
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
    updateTextLayout(true);

    /* Update minimum size-hint: */
    updateGeometry();
}

void UIGraphicsTextPane::updateTextLayout(bool fFull /* = false */)
{
    /* Prepare variables: */
    QFontMetrics fm(font(), m_pPaintDevice);
    int iMaximumTextWidth = (int)size().width() - 2 * m_iMargin - m_iSpacing;

    /* Search for the maximum column widths: */
    int iMaximumLeftColumnWidth = 0;
    int iMaximumRightColumnWidth = 0;
    bool fSingleColumnText = true;
    foreach (const UITextTableLine &line, m_text)
    {
        bool fRightColumnPresent = !line.second.isEmpty();
        if (fRightColumnPresent)
            fSingleColumnText = false;
        QString strLeftLine = fRightColumnPresent ? line.first + ":" : line.first;
        QString strRightLine = line.second;
        iMaximumLeftColumnWidth = qMax(iMaximumLeftColumnWidth, fm.width(strLeftLine));
        iMaximumRightColumnWidth = qMax(iMaximumRightColumnWidth, fm.width(strRightLine));
    }
    iMaximumLeftColumnWidth += 1;
    iMaximumRightColumnWidth += 1;

    /* Calculate text attributes: */
    int iLeftColumnWidth = 0;
    int iRightColumnWidth = 0;
    /* Left column only: */
    if (fSingleColumnText)
    {
        /* Full update? */
        if (fFull)
        {
            /* Minimum width for left column: */
            int iMinimumLeftColumnWidth = qMin(m_iMinimumTextColumnWidth, iMaximumLeftColumnWidth);
            /* Minimum width for whole text: */
            m_iMinimumTextWidth = iMinimumLeftColumnWidth;
        }

        /* Current width for left column: */
        iLeftColumnWidth = qMax(m_iMinimumTextColumnWidth, iMaximumTextWidth);
    }
    /* Two columns: */
    else
    {
        /* Full update? */
        if (fFull)
        {
            /* Minimum width for left column: */
            int iMinimumLeftColumnWidth = iMaximumLeftColumnWidth;
            /* Minimum width for right column: */
            int iMinimumRightColumnWidth = qMin(m_iMinimumTextColumnWidth, iMaximumRightColumnWidth);
            /* Minimum width for whole text: */
            m_iMinimumTextWidth = iMinimumLeftColumnWidth + m_iSpacing + iMinimumRightColumnWidth;
        }

        /* Current width for left column: */
        iLeftColumnWidth = iMaximumLeftColumnWidth;
        /* Current width for right column: */
        iRightColumnWidth = iMaximumTextWidth - iLeftColumnWidth;
    }

    /* Clear old text-layouts: */
    while (!m_leftList.isEmpty()) delete m_leftList.takeLast();
    while (!m_rightList.isEmpty()) delete m_rightList.takeLast();

    /* Prepare new text-layouts: */
    int iTextX = m_iMargin;
    int iTextY = m_iMargin;
    /* Populate text-layouts: */
    m_iMinimumTextHeight = 0;
    foreach (const UITextTableLine &line, m_text)
    {
        /* Left layout: */
        int iLeftColumnHeight = 0;
        if (!line.first.isEmpty())
        {
            bool fRightColumnPresent = !line.second.isEmpty();
            m_leftList << buildTextLayout(font(), m_pPaintDevice,
                                          fRightColumnPresent ? line.first + ":" : line.first,
                                          iLeftColumnWidth, iLeftColumnHeight);
            m_leftList.last()->setPosition(QPointF(iTextX, iTextY));
        }

        /* Right layout: */
        int iRightColumnHeight = 0;
        if (!line.second.isEmpty())
        {
            m_rightList << buildTextLayout(font(), m_pPaintDevice,
                                           line.second,
                                           iRightColumnWidth, iRightColumnHeight);
            m_rightList.last()->setPosition(QPointF(iTextX + iLeftColumnWidth + m_iSpacing, iTextY));
        }

        /* Maximum colum height? */
        int iMaximumColumnHeight = qMax(iLeftColumnHeight, iRightColumnHeight);

        /* Indent Y: */
        iTextY += iMaximumColumnHeight;
        /* Append summary text height: */
        m_iMinimumTextHeight += iMaximumColumnHeight;
    }
}

void UIGraphicsTextPane::updateGeometry()
{
    /* Discard cached minimum size-hint: */
    m_fMinimumSizeHintInvalidated = true;

    /* Call to base-class to notify layout if any: */
    QIGraphicsWidget::updateGeometry();

    /* And notify listeners which are not layouts: */
    emit sigGeometryChanged();
}

QSizeF UIGraphicsTextPane::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* For minimum size-hint: */
    if (which == Qt::MinimumSize)
    {
        /* If minimum size-hint invalidated: */
        if (m_fMinimumSizeHintInvalidated)
        {
            /* Recache minimum size-hint: */
            m_minimumSizeHint = QSizeF(2 * m_iMargin + m_iMinimumTextWidth,
                                       2 * m_iMargin + m_iMinimumTextHeight);
            m_fMinimumSizeHintInvalidated = false;
        }
        /* Return cached minimum size-hint: */
        return m_minimumSizeHint;
    }
    /* Call to base-class for other size-hints: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGraphicsTextPane::resizeEvent(QGraphicsSceneResizeEvent*)
{
    /* Update text-layout: */
    updateTextLayout();

    /* Update minimum size-hint: */
    updateGeometry();
}

void UIGraphicsTextPane::paint(QPainter *pPainter, const QStyleOptionGraphicsItem*, QWidget*)
{
    /* Draw all the text-layouts: */
    foreach (QTextLayout *pTextLayout, m_leftList)
        pTextLayout->draw(pPainter, QPoint(0, 0));
    foreach (QTextLayout *pTextLayout, m_rightList)
        pTextLayout->draw(pPainter, QPoint(0, 0));
}

/* static  */
QTextLayout* UIGraphicsTextPane::buildTextLayout(const QFont &font, QPaintDevice *pPaintDevice,
                                                 const QString &strText, int iWidth, int &iHeight)
{
    /* Prepare variables: */
    QFontMetrics fm(font, pPaintDevice);
    int iLeading = fm.leading();

    /* Only bold sub-strings are currently handled: */
    QString strModifiedText(strText);
    QRegExp boldRegExp("<b>([\\s\\S]+)</b>");
    QList<QTextLayout::FormatRange> formatRangeList;
    while (boldRegExp.indexIn(strModifiedText) != -1)
    {
        /* Prepare format: */
        QTextLayout::FormatRange formatRange;
        QFont font = formatRange.format.font();
        font.setBold(true);
        formatRange.format.setFont(font);
        formatRange.start = boldRegExp.pos(0);
        formatRange.length = boldRegExp.cap(1).size();
        /* Add format range to list: */
        formatRangeList << formatRange;
        /* Replace sub-string: */
        strModifiedText.replace(boldRegExp.cap(0), boldRegExp.cap(1));
    }

    /* Create layout; */
    QTextLayout *pTextLayout = new QTextLayout(strModifiedText, font, pPaintDevice);
    pTextLayout->setAdditionalFormats(formatRangeList);

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

        line.setLineWidth(iWidth);
        iHeight += iLeading;
        line.setPosition(QPointF(0, iHeight));
        iHeight += line.height();
    }
    pTextLayout->endLayout();

    /* Return layout: */
    return pTextLayout;
}

