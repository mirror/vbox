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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QStack>
# include <QPainter>
# include <QApplication>
# include <QFontMetrics>
# include <QGraphicsSceneHoverEvent>

/* GUI includes: */
# include "UIGraphicsTextPane.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGraphicsTextPane::UIGraphicsTextPane(QIGraphicsWidget *pParent, QPaintDevice *pPaintDevice)
    : QIGraphicsWidget(pParent)
    , m_pPaintDevice(pPaintDevice)
    , m_iMargin(0)
    , m_iSpacing(10)
    , m_iMinimumTextColumnWidth(100)
    , m_fMinimumSizeHintInvalidated(true)
    , m_iMinimumTextWidth(0)
    , m_iMinimumTextHeight(0)
    , m_fAnchorCanBeHovered(true)
{
    /* We do support hover-events: */
    setAcceptHoverEvents(true);
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

void UIGraphicsTextPane::setAnchorRoleRestricted(const QString &strAnchorRole, bool fRestricted)
{
    /* Make sure something changed: */
    if (   (fRestricted && m_restrictedAnchorRoles.contains(strAnchorRole))
        || (!fRestricted && !m_restrictedAnchorRoles.contains(strAnchorRole)))
        return;

    /* Apply new value: */
    if (fRestricted)
        m_restrictedAnchorRoles << strAnchorRole;
    else
        m_restrictedAnchorRoles.remove(strAnchorRole);

    /* Reset hovered anchor: */
    m_strHoveredAnchor.clear();
    updateHoverStuff();
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
                                          iLeftColumnWidth, iLeftColumnHeight,
                                          m_strHoveredAnchor);
            m_leftList.last()->setPosition(QPointF(iTextX, iTextY));
        }

        /* Right layout: */
        int iRightColumnHeight = 0;
        if (!line.second.isEmpty())
        {
            m_rightList << buildTextLayout(font(), m_pPaintDevice,
                                           line.second,
                                           iRightColumnWidth, iRightColumnHeight,
                                           m_strHoveredAnchor);
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

void UIGraphicsTextPane::hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Redirect to common handler: */
    handleHoverEvent(pEvent);
}

void UIGraphicsTextPane::hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Redirect to common handler: */
    handleHoverEvent(pEvent);
}

void UIGraphicsTextPane::handleHoverEvent(QGraphicsSceneHoverEvent *pEvent)
{
    /* Ignore if anchor can't be hovered: */
    if (!m_fAnchorCanBeHovered)
        return;

    /* Prepare variables: */
    QPoint mousePosition = pEvent->pos().toPoint();
    QString strHoveredAnchor;
    QString strHoveredAnchorRole;

    /* Search for hovered-anchor in the left list: */
    strHoveredAnchor = searchForHoveredAnchor(m_pPaintDevice, m_leftList, mousePosition);
    strHoveredAnchorRole = strHoveredAnchor.section(',', 0, 0);
    if (!strHoveredAnchor.isNull() && !m_restrictedAnchorRoles.contains(strHoveredAnchorRole))
    {
        m_strHoveredAnchor = strHoveredAnchor;
        return updateHoverStuff();
    }

    /* Then search for hovered-anchor in the right one: */
    strHoveredAnchor = searchForHoveredAnchor(m_pPaintDevice, m_rightList, mousePosition);
    strHoveredAnchorRole = strHoveredAnchor.section(',', 0, 0);
    if (!strHoveredAnchor.isNull() && !m_restrictedAnchorRoles.contains(strHoveredAnchorRole))
    {
        m_strHoveredAnchor = strHoveredAnchor;
        return updateHoverStuff();
    }

    /* Finally clear it for good: */
    if (!m_strHoveredAnchor.isNull())
    {
        m_strHoveredAnchor.clear();
        return updateHoverStuff();
    }
}

void UIGraphicsTextPane::updateHoverStuff()
{
    /* Update mouse-cursor: */
    if (m_strHoveredAnchor.isNull())
        unsetCursor();
    else
        setCursor(Qt::PointingHandCursor);

    /* Update text-layout: */
    updateTextLayout();

    /* Update tool-tip: */
    setToolTip(m_strHoveredAnchor.section(',', -1));

    /* Update text-pane: */
    update();
}

void UIGraphicsTextPane::mousePressEvent(QGraphicsSceneMouseEvent*)
{
    /* Make sure some anchor hovered: */
    if (m_strHoveredAnchor.isNull())
        return;

    /* Restrict anchor hovering: */
    m_fAnchorCanBeHovered = false;

    /* Cache clicked anchor: */
    QString strClickedAnchor = m_strHoveredAnchor;

    /* Clear hovered anchor: */
    m_strHoveredAnchor.clear();
    updateHoverStuff();

    /* Notify listeners about anchor clicked: */
    emit sigAnchorClicked(strClickedAnchor);

    /* Allow anchor hovering again: */
    m_fAnchorCanBeHovered = true;
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
                                                 const QString &strText, int iWidth, int &iHeight,
                                                 const QString &strHoveredAnchor)
{
    /* Prepare variables: */
    QFontMetrics fm(font, pPaintDevice);
    int iLeading = fm.leading();

    /* Parse incoming string with UIRichTextString capabilities: */
    //printf("Text: {%s}\n", strText.toAscii().constData());
    UIRichTextString ms(strText);
    ms.setHoveredAnchor(strHoveredAnchor);

    /* Create layout; */
    QTextLayout *pTextLayout = new QTextLayout(ms.toString(), font, pPaintDevice);
    pTextLayout->setAdditionalFormats(ms.formatRanges());

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

/* static */
QString UIGraphicsTextPane::searchForHoveredAnchor(QPaintDevice *pPaintDevice, const QList<QTextLayout*> &list, const QPoint &mousePosition)
{
    /* Analyze passed text-layouts: */
    foreach (QTextLayout *pTextLayout, list)
    {
        /* Prepare variables: */
        QFontMetrics fm(pTextLayout->font(), pPaintDevice);

        /* Text-layout attributes: */
        const QPoint layoutPosition = pTextLayout->position().toPoint();
        const QString strLayoutText = pTextLayout->text();

        /* Enumerate format ranges: */
        foreach (const QTextLayout::FormatRange &range, pTextLayout->additionalFormats())
        {
            /* Skip unrelated formats: */
            if (!range.format.isAnchor())
                continue;

            /* Parse 'anchor' format: */
            const int iStart = range.start;
            const int iLength = range.length;
            QRegion formatRegion;
            for (int iTextPosition = iStart; iTextPosition < iStart + iLength; ++iTextPosition)
            {
                QTextLine layoutLine = pTextLayout->lineForTextPosition(iTextPosition);
                QPoint linePosition = layoutLine.position().toPoint();
                int iSymbolX = (int)layoutLine.cursorToX(iTextPosition);
                QRect symbolRect = QRect(layoutPosition.x() + linePosition.x() + iSymbolX,
                                         layoutPosition.y() + linePosition.y(),
                                         fm.width(strLayoutText[iTextPosition]) + 1, fm.height());
                formatRegion += symbolRect;
            }

            /* Is that something we looking for? */
            if (formatRegion.contains(mousePosition))
                return range.format.anchorHref();
        }
    }

    /* Null string by default: */
    return QString();
}


const QString UIRichTextString::m_sstrAny = QString("[\\s\\S]*");
const QMap<UIRichTextString::Type, QString> UIRichTextString::m_sPatterns = populatePatterns();
const QMap<UIRichTextString::Type, bool> UIRichTextString::m_sPatternHasMeta = populatePatternHasMeta();

UIRichTextString::UIRichTextString(Type type /* = Type_None */)
    : m_type(type)
    , m_strString(QString())
    , m_strStringMeta(QString())
{
}

UIRichTextString::UIRichTextString(const QString &strString, Type type /* = Type_None */, const QString &strStringMeta /* = QString() */)
    : m_type(type)
    , m_strString(strString)
    , m_strStringMeta(strStringMeta)
{
    //printf("Creating new UIRichTextString with string=\"%s\" and string-meta=\"%s\"\n",
    //       m_strString.toAscii().constData(), m_strStringMeta.toAscii().constData());
    parse();
}

UIRichTextString::~UIRichTextString()
{
    /* Erase the map: */
    qDeleteAll(m_strings.begin(), m_strings.end());
    m_strings.clear();
}

QString UIRichTextString::toString() const
{
    /* Add own string first: */
    QString strString = m_strString;
    /* Add all the strings of children finally: */
    foreach (const int &iPosition, m_strings.keys())
        strString.insert(iPosition, m_strings.value(iPosition)->toString());
    /* Return result: */
    return strString;
}

QList<QTextLayout::FormatRange> UIRichTextString::formatRanges(int iShift /* = 0 */) const
{
    /* Prepare format range list: */
    QList<QTextLayout::FormatRange> ranges;
    /* Add own format range first: */
    QTextLayout::FormatRange range;
    range.start = iShift;
    range.length = toString().size();
    range.format = textCharFormat(m_type);
    /* Enable anchor if present: */
    if (!m_strAnchor.isNull())
    {
        range.format.setAnchorHref(m_strAnchor);
        /* Highlight anchor if hovered: */
        if (range.format.anchorHref() == m_strHoveredAnchor)
            range.format.setForeground(qApp->palette().color(QPalette::Link));
    }
    ranges.append(range);
    /* Add all the format ranges of children finally: */
    foreach (const int &iPosition, m_strings.keys())
        ranges.append(m_strings.value(iPosition)->formatRanges(iShift + iPosition));
    /* Return result: */
    return ranges;
}

void UIRichTextString::setHoveredAnchor(const QString &strHoveredAnchor)
{
    /* Define own hovered anchor first: */
    m_strHoveredAnchor = strHoveredAnchor;
    /* Propagate hovered anchor to children finally: */
    foreach (const int &iPosition, m_strings.keys())
        m_strings.value(iPosition)->setHoveredAnchor(m_strHoveredAnchor);
}

void UIRichTextString::parse()
{
    /* Assign the meta to anchor directly for now,
     * will do a separate parsing when there will
     * be more than one type of meta: */
    if (!m_strStringMeta.isNull())
        m_strAnchor = m_strStringMeta;

    /* Parse the passed QString with all the known patterns: */
    foreach (const Type &enmPattern, m_sPatterns.keys())
    {
        /* Get the current pattern: */
        const QString strPattern = m_sPatterns.value(enmPattern);

        /* Recursively parse the string: */
        int iMaxLevel = 0;
        do
        {
            /* Search for the maximum level of the current pattern: */
            iMaxLevel = searchForMaxLevel(m_strString, strPattern, strPattern);
            //printf(" Maximum level for the pattern \"%s\" is %d.\n",
            //       strPattern.toAscii().constData(), iMaxLevel);
            /* If current pattern of at least level 1 is found: */
            if (iMaxLevel > 0)
            {
                /* Compose full pattern of the corresponding level: */
                const QString strFullPattern = composeFullPattern(strPattern, strPattern, iMaxLevel);
                //printf("  Full pattern: %s\n", strFullPattern.toAscii().constData());
                QRegExp regExp(strFullPattern);
                regExp.setMinimal(true);
                const int iPosition = regExp.indexIn(m_strString);
                AssertReturnVoid(iPosition != -1);
                if (iPosition != -1)
                {
                    /* Cut the found string: */
                    m_strString.remove(iPosition, regExp.cap(0).size());
                    /* And paste that string as our child: */
                    const bool fPatterHasMeta = m_sPatternHasMeta.value(enmPattern);
                    const QString strSubString = !fPatterHasMeta ? regExp.cap(1) : regExp.cap(2);
                    const QString strSubMeta   = !fPatterHasMeta ? QString()     : regExp.cap(1);
                    m_strings.insert(iPosition, new UIRichTextString(strSubString, enmPattern, strSubMeta));
                }
            }
        }
        while (iMaxLevel > 0);
    }
}

/* static */
QMap<UIRichTextString::Type, QString> UIRichTextString::populatePatterns()
{
    QMap<Type, QString> patterns;
    patterns.insert(Type_Anchor, QString("<a href=([^>]+)>(%1)</a>"));
    patterns.insert(Type_Bold,   QString("<b>(%1)</b>"));
    patterns.insert(Type_Italic, QString("<i>(%1)</i>"));
    return patterns;
}

/* static */
QMap<UIRichTextString::Type, bool> UIRichTextString::populatePatternHasMeta()
{
    QMap<Type, bool> patternHasMeta;
    patternHasMeta.insert(Type_Anchor, true);
    patternHasMeta.insert(Type_Bold,   false);
    patternHasMeta.insert(Type_Italic, false);
    return patternHasMeta;
}

/* static */
int UIRichTextString::searchForMaxLevel(const QString &strString, const QString &strPattern,
                                        const QString &strCurrentPattern, int iCurrentLevel /* = 0 */)
{
    QRegExp regExp(strCurrentPattern.arg(m_sstrAny));
    regExp.setMinimal(true);
    if (regExp.indexIn(strString) != -1)
        return searchForMaxLevel(strString, strPattern,
                                 strCurrentPattern.arg(m_sstrAny + strPattern + m_sstrAny),
                                 iCurrentLevel + 1);
    return iCurrentLevel;
}

/* static */
QString UIRichTextString::composeFullPattern(const QString &strPattern,
                                             const QString &strCurrentPattern, int iCurrentLevel)
{
    if (iCurrentLevel > 1)
        return composeFullPattern(strPattern,
                                  strCurrentPattern.arg(m_sstrAny + strPattern + m_sstrAny),
                                  iCurrentLevel - 1);
    return strCurrentPattern.arg(m_sstrAny);
}

/* static */
QTextCharFormat UIRichTextString::textCharFormat(Type type)
{
    QTextCharFormat format;
    switch (type)
    {
        case Type_Anchor:
        {
            format.setAnchor(true);
            break;
        }
        case Type_Bold:
        {
            QFont font = format.font();
            font.setBold(true);
            format.setFont(font);
            break;
        }
        case Type_Italic:
        {
            QFont font = format.font();
            font.setItalic(true);
            format.setFont(font);
            break;
        }
    }
    return format;
}

