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
    , m_iMinimumTextWidth(0)
    , m_iMinimumTextHeight(0)
{
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

    /* Update minimum text size-hint: */
    updateMinimumTextWidthHint();
    updateMinimumTextHeightHint();
}

void UIGraphicsTextPane::updateMinimumTextWidthHint()
{
    /* Prepare variables: */
    QFontMetrics fm(font(), m_pPaintDevice);

    /* Search for the maximum line widths: */
    int iMaximumLeftLineWidth = 0;
    int iMaximumRightLineWidth = 0;
    bool fSingleColumnText = true;
    foreach (const UITextTableLine &line, m_text)
    {
        bool fRightColumnPresent = !line.second.isEmpty();
        if (fRightColumnPresent)
            fSingleColumnText = false;
        QString strLeftLine = fRightColumnPresent ? line.first + ":" : line.first;
        QString strRightLine = line.second;
        iMaximumLeftLineWidth = qMax(iMaximumLeftLineWidth, fm.width(strLeftLine));
        iMaximumRightLineWidth = qMax(iMaximumRightLineWidth, fm.width(strRightLine));
    }
    iMaximumLeftLineWidth += 1;
    iMaximumRightLineWidth += 1;

    /* Calculate minimum-text-width: */
    int iMinimumTextWidth = 0;
    if (fSingleColumnText)
    {
        /* Take into account only left column: */
        int iMinimumLeftColumnWidth = qMin(iMaximumLeftLineWidth, m_iMinimumTextColumnWidth);
        iMinimumTextWidth = iMinimumLeftColumnWidth;
    }
    else
    {
        /* Take into account both columns, but wrap only right one: */
        int iMinimumLeftColumnWidth = iMaximumLeftLineWidth;
        int iMinimumRightColumnWidth = qMin(iMaximumRightLineWidth, m_iMinimumTextColumnWidth);
        iMinimumTextWidth = iMinimumLeftColumnWidth + m_iSpacing + iMinimumRightColumnWidth;
    }

    /* Make sure something changed: */
    if (m_iMinimumTextWidth == iMinimumTextWidth)
        return;

    /* Remember new value: */
    m_iMinimumTextWidth = iMinimumTextWidth;

    /* Notify layout if any: */
    updateGeometry();
}

void UIGraphicsTextPane::updateMinimumTextHeightHint()
{
    /* Prepare variables: */
    int iMaximumTextWidth = (int)size().width() - 2 * m_iMargin - m_iSpacing;
    QFontMetrics fm(font(), m_pPaintDevice);

    /* Search for the maximum line widths: */
    int iMaximumLeftLineWidth = 0;
    int iMaximumRightLineWidth = 0;
    bool fSingleColumnText = true;
    foreach (const UITextTableLine &line, m_text)
    {
        bool fRightColumnPresent = !line.second.isEmpty();
        if (fRightColumnPresent)
            fSingleColumnText = false;
        QString strFirstLine = fRightColumnPresent ? line.first + ":" : line.first;
        QString strSecondLine = line.second;
        iMaximumLeftLineWidth = qMax(iMaximumLeftLineWidth, fm.width(strFirstLine));
        iMaximumRightLineWidth = qMax(iMaximumRightLineWidth, fm.width(strSecondLine));
    }
    iMaximumLeftLineWidth += 1;
    iMaximumRightLineWidth += 1;

    /* Calculate column widths: */
    int iLeftColumnWidth = 0;
    int iRightColumnWidth = 0;
    if (fSingleColumnText)
    {
        /* Take into account only left column: */
        iLeftColumnWidth = qMax(m_iMinimumTextColumnWidth, iMaximumTextWidth);
    }
    else
    {
        /* Take into account both columns, but wrap only right one: */
        iLeftColumnWidth = iMaximumLeftLineWidth;
        iRightColumnWidth = iMaximumTextWidth - iLeftColumnWidth;
    }

    /* Calculate minimum-text-height: */
    int iMinimumTextHeight = 0;
    foreach (const UITextTableLine line, m_text)
    {
        /* First layout: */
        int iLeftColumnHeight = 0;
        if (!line.first.isEmpty())
        {
            bool fRightColumnPresent = !line.second.isEmpty();
            QTextLayout *pTextLayout = buildTextLayout(font(), m_pPaintDevice,
                                                       fRightColumnPresent ? line.first + ":" : line.first,
                                                       iLeftColumnWidth, iLeftColumnHeight);
            delete pTextLayout;
        }

        /* Second layout: */
        int iRightColumnHeight = 0;
        if (!line.second.isEmpty())
        {
            QTextLayout *pTextLayout = buildTextLayout(font(), m_pPaintDevice, line.second,
                                                       iRightColumnWidth, iRightColumnHeight);
            delete pTextLayout;
        }

        /* Append summary text height: */
        iMinimumTextHeight += qMax(iLeftColumnHeight, iRightColumnHeight);
    }

    /* Make sure something changed: */
    if (m_iMinimumTextHeight == iMinimumTextHeight)
        return;

    /* Remember new value: */
    m_iMinimumTextHeight = iMinimumTextHeight;

    /* Notify layout if any: */
    updateGeometry();
}

QSizeF UIGraphicsTextPane::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* Calculate minimum size-hint: */
    if (which == Qt::MinimumSize)
    {
        int iWidth = 2 * m_iMargin + m_iMinimumTextWidth;
        int iHeight = 2 * m_iMargin + m_iMinimumTextHeight;
        return QSize(iWidth, iHeight);
    }
    /* Call to base-class: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGraphicsTextPane::resizeEvent(QGraphicsSceneResizeEvent*)
{
    /* Update minimum text height-hint: */
    updateMinimumTextHeightHint();
}

void UIGraphicsTextPane::paint(QPainter *pPainter, const QStyleOptionGraphicsItem*, QWidget*)
{
    /* Prepare variables: */
    int iMaximumTextWidth = (int)size().width() - 2 * m_iMargin - m_iSpacing;
    QFontMetrics fm(font(), m_pPaintDevice);

    /* Where to paint? */
    int iTextX = m_iMargin;
    int iTextY = m_iMargin;

    /* Search for the maximum line widths: */
    int iMaximumLeftLineWidth = 0;
    int iMaximumRightLineWidth = 0;
    bool fSingleColumnText = true;
    foreach (const UITextTableLine line, m_text)
    {
        bool fRightColumnPresent = !line.second.isEmpty();
        if (fRightColumnPresent)
            fSingleColumnText = false;
        QString strFirstLine = fRightColumnPresent ? line.first + ":" : line.first;
        QString strSecondLine = line.second;
        iMaximumLeftLineWidth = qMax(iMaximumLeftLineWidth, fm.width(strFirstLine));
        iMaximumRightLineWidth = qMax(iMaximumRightLineWidth, fm.width(strSecondLine));
    }
    iMaximumLeftLineWidth += 1;
    iMaximumRightLineWidth += 1;

    /* Calculate column widths: */
    int iLeftColumnWidth = 0;
    int iRightColumnWidth = 0;
    if (fSingleColumnText)
    {
        /* Take into account only left column: */
        iLeftColumnWidth = qMax(m_iMinimumTextColumnWidth, iMaximumTextWidth);
    }
    else
    {
        /* Take into account both columns, but wrap only right one: */
        iLeftColumnWidth = iMaximumLeftLineWidth;
        iRightColumnWidth = iMaximumTextWidth - iLeftColumnWidth;
    }

    /* For each the line: */
    foreach (const UITextTableLine line, m_text)
    {
        /* First layout: */
        int iLeftColumnHeight = 0;
        if (!line.first.isEmpty())
        {
            bool fRightColumnPresent = !line.second.isEmpty();
            QTextLayout *pTextLayout = buildTextLayout(font(), m_pPaintDevice,
                                                       fRightColumnPresent ? line.first + ":" : line.first,
                                                       iLeftColumnWidth, iLeftColumnHeight);
            pTextLayout->draw(pPainter, QPointF(iTextX, iTextY));
            delete pTextLayout;
        }

        /* Second layout: */
        int iRightColumnHeight = 0;
        if (!line.second.isEmpty())
        {
            QTextLayout *pTextLayout = buildTextLayout(font(), m_pPaintDevice,
                                                       line.second, iRightColumnWidth, iRightColumnHeight);
            pTextLayout->draw(pPainter, QPointF(iTextX + iLeftColumnWidth + m_iSpacing, iTextY));
            delete pTextLayout;
        }

        /* Indent Y: */
        iTextY += qMax(iLeftColumnHeight, iRightColumnHeight);
    }
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

