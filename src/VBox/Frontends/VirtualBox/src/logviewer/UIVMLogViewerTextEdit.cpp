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
# include <QTextBlock>

/* GUI includes: */
# include "UIVMLogViewerTextEdit.h"

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

class UIIndicatorScrollBar : public QScrollBar
{
    Q_OBJECT;

public:

    UIIndicatorScrollBar(QWidget *parent = 0)
        :QScrollBar(parent)
    {
        setStyleSheet(verticalScrollBarStyle);
    }

    void setMarkingsVector(const QVector<float> &vector)
    {
        m_markingsVector = vector;
    }

    void clearMarkingsVector()
    {
        m_markingsVector.clear();
    }

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */
    {
        QScrollBar::paintEvent(pEvent);
        /* Put a red line to marking position: */
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

private:

    /* Stores the relative (to scrollbar's height) positions of markings,
       where we draw a horizontal line. Values are in [0.0, 1.0]*/
    QVector<float> m_markingsVector;
};

class UILineNumberArea : public QWidget
{
public:
    UILineNumberArea(UIVMLogViewerTextEdit *textEdit)
        :QWidget(textEdit)
        , m_pTextEdit(textEdit){}

    QSize sizeHint() const
    {
        return QSize(m_pTextEdit->lineNumberAreaWidth(), 0);
    }

protected:

    void paintEvent(QPaintEvent *event)
    {
        m_pTextEdit->lineNumberAreaPaintEvent(event);
    }

private:
    UIVMLogViewerTextEdit *m_pTextEdit;
};


UIVMLogViewerTextEdit::UIVMLogViewerTextEdit(QWidget* parent /* = 0 */)
    :QPlainTextEdit(parent)
    , m_pLineNumberArea(0)
{

    //setStyleSheet("background-color: rgba(240, 240, 240, 75%) ");
    prepare();
}

void UIVMLogViewerTextEdit::prepare()
{
    prepareWidgets();
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


#if defined(RT_OS_SOLARIS)
    /* Use system fixed-width font on Solaris hosts as the Courier family fonts don't render well. */
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
    QFont font;
    font.setFamily("Courier New,courier");
#endif
    setFont(font);
    setWordWrapMode(QTextOption::NoWrap);
    setReadOnly(true);
    if(m_pLineNumberArea)
        m_pLineNumberArea->setFont(font);

}

int UIVMLogViewerTextEdit::lineNumberAreaWidth()
{
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
    QPainter painter(m_pLineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
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

void UIVMLogViewerTextEdit::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QMenu *menu = createStandardContextMenu();
    QAction *pAction = menu->addAction(tr("Bookmark"));
    QTextBlock block = cursorForPosition(pEvent->pos()).block();
    m_iContextMenuBookmark.first = block.firstLineNumber();
    m_iContextMenuBookmark.second = block.text();

    if (pAction)
        connect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

    menu->exec(pEvent->globalPos());

    if (pAction)
        disconnect(pAction, &QAction::triggered, this, &UIVMLogViewerTextEdit::sltBookmark);

    delete menu;
}

void UIVMLogViewerTextEdit::resizeEvent(QResizeEvent *pEvent)
{
    QPlainTextEdit::resizeEvent(pEvent);

    QRect cr = contentsRect();
    m_pLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
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
    emit sigContextMenuBookmarkAction(m_iContextMenuBookmark);
}

void UIVMLogViewerTextEdit::setScrollBarMarkingsVector(const QVector<float> &vector)
{
    UIIndicatorScrollBar* vScrollBar = qobject_cast<UIIndicatorScrollBar*>(verticalScrollBar());
    if(vScrollBar)
        vScrollBar->setMarkingsVector(vector);
}

void UIVMLogViewerTextEdit::clearScrollBarMarkingsVector()
{
    UIIndicatorScrollBar* vScrollBar = qobject_cast<UIIndicatorScrollBar*>(verticalScrollBar());
    if(vScrollBar)
        vScrollBar->clearMarkingsVector();
}


#include "UIVMLogViewerTextEdit.moc"
