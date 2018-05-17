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
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QMenu>
# include <QPainter>
# include <QPlainTextEdit>
# include <QScrollBar>
# include <QStyle>
# include <QTextBlock>

/* GUI includes: */
# include "UIIconPool.h"
# include "UIVMLogViewerTextEdit.h"
# include "UIVMLogViewerWidget.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/** We use a modified scrollbar style for our QPlainTextEdits to get the
    markings on the scrollbars correctly. The default scrollbarstyle does not
    reveal the height of the pushbuttons on the scrollbar (on either side of it, with arrow on them)
    to compute the marking locations correctly. Thus we turn these push buttons off: */
const QString verticalScrollBarStyle("QScrollBar:vertical {"
                                     "border: 1px ridge grey; "
                                     "margin: 0px 0px 0 0px;}"
                                     "QScrollBar::handle:vertical {"
                                     "min-height: 10px;"
                                     "background: grey;}"
                                     "QScrollBar::add-line:vertical {"
                                     "width: 0px;}"
                                     "QScrollBar::sub-line:vertical {"
                                     "width: 0px;}");

const QString horizontalScrollBarStyle("QScrollBar:horizontal {"
                                       "border: 1px ridge grey; "
                                       "margin: 0px 0px 0 0px;}"
                                       "QScrollBar::handle:horizontal {"
                                       "min-height: 10px;"
                                       "background: grey;}"
                                       "QScrollBar::add-line:horizontal {"
                                       "height: 0px;}"
                                       "QScrollBar::sub-line:horizontal {"
                                       "height: 0px;}");


/*********************************************************************************************************************************
*   UIIndicatorScrollBar definition.                                                                                             *
*********************************************************************************************************************************/

class UIIndicatorScrollBar : public QScrollBar
{
    Q_OBJECT;

public:

    UIIndicatorScrollBar(QWidget *parent = 0);
    void setMarkingsVector(const QVector<float> &vector);
    void clearMarkingsVector();

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    /* Stores the relative (to scrollbar's height) positions of markings,
       where we draw a horizontal line. Values are in [0.0, 1.0]*/
    QVector<float> m_markingsVector;
};


/*********************************************************************************************************************************
*   UIIndicatorScrollBar implemetation.                                                                                          *
*********************************************************************************************************************************/

UIIndicatorScrollBar::UIIndicatorScrollBar(QWidget *parent /*= 0 */)
    :QScrollBar(parent)
{
    setStyleSheet(verticalScrollBarStyle);
}

void UIIndicatorScrollBar::setMarkingsVector(const QVector<float> &vector)
{
    m_markingsVector = vector;
}

void UIIndicatorScrollBar::clearMarkingsVector()
{
    m_markingsVector.clear();
}

void UIIndicatorScrollBar::paintEvent(QPaintEvent *pEvent) /* override */
{
    QScrollBar::paintEvent(pEvent);
    /* Put a red line to mark the bookmark positions: */
    for (int i = 0; i < m_markingsVector.size(); ++i)
    {
        QPointF p1 = QPointF(0, m_markingsVector[i] * height());
        QPointF p2 = QPointF(width(), m_markingsVector[i] * height());

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(255, 0, 0, 75), 1.1f));
        painter.drawLine(p1, p2);
    }
}


/*********************************************************************************************************************************
*   UILineNumberArea definition.                                                                                                 *
*********************************************************************************************************************************/

class UILineNumberArea : public QWidget
{
public:
    UILineNumberArea(UIVMLogViewerTextEdit *textEdit);
    QSize sizeHint() const;

protected:

    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *pEvent);
    void mousePressEvent(QMouseEvent *pEvent);

private:
    UIVMLogViewerTextEdit *m_pTextEdit;
};


/*********************************************************************************************************************************
*   UILineNumberArea implemetation.                                                                                              *
*********************************************************************************************************************************/

UILineNumberArea::UILineNumberArea(UIVMLogViewerTextEdit *textEdit)
    :QWidget(textEdit)
    , m_pTextEdit(textEdit)
{
    setMouseTracking(true);
}

QSize UILineNumberArea::sizeHint() const
{
    if (!m_pTextEdit)
        return QSize();
    return QSize(m_pTextEdit->lineNumberAreaWidth(), 0);
}

void UILineNumberArea::paintEvent(QPaintEvent *event)
{
    if (m_pTextEdit)
        m_pTextEdit->lineNumberAreaPaintEvent(event);
}

void UILineNumberArea::mouseMoveEvent(QMouseEvent *pEvent)
{
    if (m_pTextEdit)
        m_pTextEdit->setMouseCursorLine(m_pTextEdit->lineNumberForPos(pEvent->pos()));
    repaint();
}

void UILineNumberArea::mousePressEvent(QMouseEvent *pEvent)
{
    if (m_pTextEdit)
        m_pTextEdit->toggleBookmark(m_pTextEdit->bookmarkForPos(pEvent->pos()));
}


/*********************************************************************************************************************************
*   UIVMLogViewerTextEdit implemetation.                                                                                         *
*********************************************************************************************************************************/

UIVMLogViewerTextEdit::UIVMLogViewerTextEdit(QWidget* parent /* = 0 */)
    : QIWithRetranslateUI<QPlainTextEdit>(parent)
    , m_pLineNumberArea(0)
    , m_mouseCursorLine(-1)
    , m_bShownTextIsFiltered(false)
    , m_bShowLineNumbers(true)
    , m_bWrapLines(true)
    , m_bHasContextMenu(false)
{
    setMouseTracking(true);
    //setStyleSheet("background-color: rgba(240, 240, 240, 75%) ");
    prepare();


}

void UIVMLogViewerTextEdit::prepare()
{
    prepareWidgets();
    retranslateUi();
}

void UIVMLogViewerTextEdit::prepareWidgets()
{
    m_pLineNumberArea = new UILineNumberArea(this);

    connect(this, &UIVMLogViewerTextEdit::blockCountChanged, this, &UIVMLogViewerTextEdit::sltUpdateLineNumberAreaWidth);
    connect(this, &UIVMLogViewerTextEdit::updateRequest, this, &UIVMLogViewerTextEdit::sltUpdateLineNumberArea);
    sltUpdateLineNumberAreaWidth(0);

    setVerticalScrollBar(new UIIndicatorScrollBar());
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    QScrollBar *pHorizontalScrollBar = horizontalScrollBar();
    if (pHorizontalScrollBar)
        pHorizontalScrollBar->setStyleSheet(horizontalScrollBarStyle);

    /* Configure this' wrap mode: */
    setWrapLines(false);
    setReadOnly(true);
}

void UIVMLogViewerTextEdit::setCurrentFont(QFont font)
{
    setFont(font);
    if (m_pLineNumberArea)
        m_pLineNumberArea->setFont(font);
}

int UIVMLogViewerTextEdit::lineNumberAreaWidth()
{
    if (!m_bShowLineNumbers)
        return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().width(QLatin1Char('9')) * digits;

    return space;
}

void UIVMLogViewerTextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!m_bShowLineNumbers)
        return;
    QPainter painter(m_pLineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            /* Mark this line if it is bookmarked, but only if the text is not filtered. */
            if (m_bookmarkLineSet.contains(blockNumber + 1) && !m_bShownTextIsFiltered)
            {
                QPainterPath path;
                path.addRect(0, top, m_pLineNumberArea->width(), m_pLineNumberArea->fontMetrics().lineSpacing());
                painter.fillPath(path, QColor(204, 255, 51, 125));
                painter.drawPath(path);
            }
            /* Draw a unfilled red rectangled around the line number to indicate line the mouse cursor is currently
               hovering on. Do this only if mouse is over the ext edit or the context menu is around: */
            if ((blockNumber + 1) == m_mouseCursorLine && (underMouse() || m_bHasContextMenu))
            {
                painter.setPen(Qt::red);
                painter.drawRect(0, top, m_pLineNumberArea->width(), m_pLineNumberArea->fontMetrics().lineSpacing());
            }

            painter.setPen(Qt::black);
            painter.drawText(0, top, m_pLineNumberArea->width(), m_pLineNumberArea->fontMetrics().lineSpacing(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void UIVMLogViewerTextEdit::retranslateUi()
{
    m_strBackgroungText = QString(UIVMLogViewerWidget::tr("Filtered"));
}

void UIVMLogViewerTextEdit::setBackground()
{
    QPalette mPalette = palette();
    /* Paint a string to the background of the text edit to indicate that
       the text has been filtered */
    if (m_bShownTextIsFiltered)
    {
        /* For 100% scale PM_LargeIconSize is 32px, and since we want ~300x~100 pixmap we take it 9x3: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
        int imageW = 8 * iIconMetric;
        int imageH = 8 * iIconMetric;
        QImage image(imageW, imageH, QImage::Format_ARGB32_Premultiplied);
        QColor fillColor(QPalette::Light);
        fillColor.setAlpha(0);
        image.fill(fillColor);
        QPainter painter(&image);
        painter.translate(0.5 * imageW, 0.5 * imageH);
        painter.rotate(-45);

        /* Configure the font size and color: */
        QFont pfont = painter.font();
        QColor fontColor(QPalette::Dark);
        fontColor.setAlpha(22);
        painter.setPen(fontColor);
        pfont.setBold(true);
        pfont.setPixelSize(48);
        painter.setFont(pfont);
        QRect textRect(- 0.5 * imageW, - 0.5 * imageH, imageW, imageH);
        painter.drawText(textRect, Qt::AlignCenter | Qt::AlignVCenter, m_strBackgroungText);

        mPalette.setBrush(QPalette::Base, QBrush(image));
        setPalette(mPalette);
    }
    else
    {
        /* Reset this->palette back to standard one. */
        if (style())
            setPalette(style()->standardPalette());
    }
}

void UIVMLogViewerTextEdit::contextMenuEvent(QContextMenuEvent *pEvent)
{
    /* If shown text is filtered, do not create Bookmark action since
       we disable all bookmarking related functionalities in this case. */
    if (m_bShownTextIsFiltered)
    {
        QPlainTextEdit::contextMenuEvent(pEvent);
        return;
    }
    m_bHasContextMenu = true;
    QMenu *menu = createStandardContextMenu();


    QAction *pAction = menu->addAction(UIVMLogViewerWidget::tr("Bookmark"));
    if (pAction)
    {
        pAction->setCheckable(true);
        QPair<int, QString> menuBookmark = bookmarkForPos(pEvent->pos());
        pAction->setChecked(m_bookmarkLineSet.contains(menuBookmark.first));
        if (pAction->isChecked())
            pAction->setIcon(UIIconPool::iconSet(":/log_viewer_bookmark_on_16px.png"));
        else
            pAction->setIcon(UIIconPool::iconSet(":/log_viewer_bookmark_off_16px.png"));

        m_iContextMenuBookmark = menuBookmark;
        connect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

    }
    menu->exec(pEvent->globalPos());

    if (pAction)
        disconnect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

    delete menu;
    m_bHasContextMenu = false;
}

void UIVMLogViewerTextEdit::resizeEvent(QResizeEvent *pEvent)
{
    QPlainTextEdit::resizeEvent(pEvent);
    if (m_pLineNumberArea)
    {
        QRect cr = contentsRect();
        m_pLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }
}

void UIVMLogViewerTextEdit::mouseMoveEvent(QMouseEvent *pEvent)
{
    setMouseCursorLine(lineNumberForPos(pEvent->pos()));
    if (m_pLineNumberArea)
        m_pLineNumberArea->repaint();
    QPlainTextEdit::mouseMoveEvent(pEvent);
}

void UIVMLogViewerTextEdit::leaveEvent(QEvent * pEvent)
{
    QPlainTextEdit::leaveEvent(pEvent);
    /* Force a redraw as mouse leaves this to remove the mouse
       cursor track rectangle (the red rectangle we draw on the line number area). */
    update();
}

void UIVMLogViewerTextEdit::sltUpdateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void UIVMLogViewerTextEdit::sltUpdateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_pLineNumberArea->scroll(0, dy);
    else
        m_pLineNumberArea->update(0, rect.y(), m_pLineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        sltUpdateLineNumberAreaWidth(0);
}

void UIVMLogViewerTextEdit::sltBookmark()
{
    toggleBookmark(m_iContextMenuBookmark);
}

void UIVMLogViewerTextEdit::setScrollBarMarkingsVector(const QVector<float> &vector)
{
    UIIndicatorScrollBar* vScrollBar = qobject_cast<UIIndicatorScrollBar*>(verticalScrollBar());
    if (vScrollBar)
        vScrollBar->setMarkingsVector(vector);
}

void UIVMLogViewerTextEdit::clearScrollBarMarkingsVector()
{
    UIIndicatorScrollBar* vScrollBar = qobject_cast<UIIndicatorScrollBar*>(verticalScrollBar());
    if (vScrollBar)
        vScrollBar->clearMarkingsVector();
}

void UIVMLogViewerTextEdit::scrollToLine(int lineNumber)
{
    QTextDocument* pDocument = document();
    if (!pDocument)
        return;

    moveCursor(QTextCursor::End);
    int halfPageLineCount = 0.5 * visibleLineCount() ;
    QTextCursor cursor(pDocument->findBlockByLineNumber(qMax(lineNumber - halfPageLineCount, 0)));
    setTextCursor(cursor);
}

int UIVMLogViewerTextEdit::visibleLineCount()
{
    int height = 0;
    if (viewport())
        height = viewport()->height();
    if (verticalScrollBar() && verticalScrollBar()->isVisible())
        height -= horizontalScrollBar()->height();
    int singleLineHeight = fontMetrics().lineSpacing();
    if (singleLineHeight == 0)
        return 0;
    return height / singleLineHeight;
}

void UIVMLogViewerTextEdit::setBookmarkLineSet(const QSet<int>& lineSet)
{
    m_bookmarkLineSet = lineSet;
    repaint();
}

int  UIVMLogViewerTextEdit::lineNumberForPos(const QPoint &position)
{
    QTextCursor cursor = cursorForPosition(position);
    QTextBlock block = cursor.block();
    return block.blockNumber() + 1;
}


QPair<int, QString> UIVMLogViewerTextEdit::bookmarkForPos(const QPoint &position)
{
    QTextBlock block = cursorForPosition(position).block();
    return QPair<int, QString>(lineNumberForPos(position), block.text());
}

void UIVMLogViewerTextEdit::setMouseCursorLine(int lineNumber)
{
    m_mouseCursorLine = lineNumber;
}

void UIVMLogViewerTextEdit::toggleBookmark(const QPair<int, QString>& bookmark)
{
    if (m_bShownTextIsFiltered)
        return;

    int lineNumber = bookmark.first;

    if (m_bookmarkLineSet.contains(lineNumber))
        emit sigDeleteBookmark(bookmark);
    else
        emit sigAddBookmark(bookmark);
}

void UIVMLogViewerTextEdit::setShownTextIsFiltered(bool warning)
{
    if (m_bShownTextIsFiltered == warning)
        return;
    m_bShownTextIsFiltered = warning;
    setBackground();
}

void UIVMLogViewerTextEdit::setShowLineNumbers(bool bShowLineNumbers)
{
    if (m_bShowLineNumbers == bShowLineNumbers)
        return;
    m_bShowLineNumbers = bShowLineNumbers;
    emit updateRequest(viewport()->rect(), 0);
}

bool UIVMLogViewerTextEdit::showLineNumbers() const
{
    return m_bShowLineNumbers;
}

void UIVMLogViewerTextEdit::setWrapLines(bool bWrapLines)
{
    if (m_bWrapLines == bWrapLines)
        return;
    m_bWrapLines = bWrapLines;
    if (m_bWrapLines)
    {
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        setWordWrapMode(QTextOption::WordWrap);
    }
    else
    {
        setWordWrapMode(QTextOption::NoWrap);
        setWordWrapMode(QTextOption::NoWrap);
    }
    update();
}

bool UIVMLogViewerTextEdit::wrapLines() const
{
    return m_bWrapLines;
}

int  UIVMLogViewerTextEdit::currentVerticalScrollBarValue() const
{
    if (!verticalScrollBar())
        return -1;
    return verticalScrollBar()->value();
}

void UIVMLogViewerTextEdit::setCurrentVerticalScrollBarValue(int value)
{
    if (!verticalScrollBar())
        return;

    setCenterOnScroll(true);

    verticalScrollBar()->setValue(value);
    verticalScrollBar()->setSliderPosition(value);
    viewport()->update();
    update();
}

#include "UIVMLogViewerTextEdit.moc"
