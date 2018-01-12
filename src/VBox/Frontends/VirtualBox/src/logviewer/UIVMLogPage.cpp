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
# include <QDateTime>
# include <QDir>
# include <QVBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QPainter>
# include <QPlainTextEdit>
# include <QScrollBar>
# include <QTextBlock>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QITabWidget.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIVMLogPage.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIToolBar.h"

/* COM includes: */
# include "CSystemProperties.h"

# include "VBoxGlobal.h"

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

/* Sub-class QPlainTextEdit for some addtional context menu items: */
class UIVMLogViewerTextEdit : public QPlainTextEdit
{
    Q_OBJECT;

signals:

    void sigContextMenuBookmarkAction(LogBookmark bookmark);

public:
    UIVMLogViewerTextEdit(QWidget* parent = 0)
        :QPlainTextEdit(parent)
    {
        //setStyleSheet("background-color: rgba(240, 240, 240, 75%) ");
    }

protected:

    void contextMenuEvent(QContextMenuEvent *pEvent)
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

    virtual void mousePressEvent(QMouseEvent *pEvent)
    {
        QPlainTextEdit::mousePressEvent(pEvent);
    }

private slots:

    void sltBookmark()
    {
        emit sigContextMenuBookmarkAction(m_iContextMenuBookmark);
    }

private:

    /* Line number and text at the context menu position */
    LogBookmark m_iContextMenuBookmark;
};


UIVMLogPage::UIVMLogPage(QWidget *pParent /* = 0 */, int tabIndex /*= -1 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pPlainTextEdit(0)
    , m_tabIndex(tabIndex)
{
    prepare();
}

UIVMLogPage::~UIVMLogPage()
{
    cleanup();
}

int UIVMLogPage::defaultLogPageWidth() const
{
    if (!m_pPlainTextEdit)
        return 0;

    /* Compute a width for 132 characters plus scrollbar and frame width: */
    int iDefaultWidth = m_pPlainTextEdit->fontMetrics().width(QChar('x')) * 132 +
                        m_pPlainTextEdit->verticalScrollBar()->width() +
                        m_pPlainTextEdit->frameWidth() * 2;

    return iDefaultWidth;
}


void UIVMLogPage::prepare()
{
    prepareWidgets();
    retranslateUi();
}

void UIVMLogPage::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout();
    setLayout(m_pMainLayout);
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pPlainTextEdit = new UIVMLogViewerTextEdit(this);
    m_pMainLayout->addWidget(m_pPlainTextEdit);

    m_pPlainTextEdit->setVerticalScrollBar(new UIIndicatorScrollBar());
    m_pPlainTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    QScrollBar *pHorizontalScrollBar = m_pPlainTextEdit->horizontalScrollBar();
    if (pHorizontalScrollBar)
        pHorizontalScrollBar->setStyleSheet(horizontalScrollBarStyle);

#if defined(RT_OS_SOLARIS)
    /* Use system fixed-width font on Solaris hosts as the Courier family fonts don't render well. */
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
    QFont font;
    font.setFamily("Courier New,courier");
#endif
    m_pPlainTextEdit->setFont(font);
    m_pPlainTextEdit->setWordWrapMode(QTextOption::NoWrap);
    m_pPlainTextEdit->setReadOnly(true);
    connect(qobject_cast<UIVMLogViewerTextEdit*>(m_pPlainTextEdit), &UIVMLogViewerTextEdit::sigContextMenuBookmarkAction,
            this, &UIVMLogPage::sltAddBookmark);
}

QPlainTextEdit *UIVMLogPage::textEdit()
{
    return m_pPlainTextEdit;
}

QTextDocument* UIVMLogPage::document()
{
    if (!m_pPlainTextEdit)
        return 0;
    return m_pPlainTextEdit->document();
}

void UIVMLogPage::setTabIndex(int index)
{
    m_tabIndex = index;
}

int UIVMLogPage::tabIndex()  const
{
    return m_tabIndex;
}

void UIVMLogPage::retranslateUi()
{
}

void UIVMLogPage::cleanup()
{
}

void UIVMLogPage::setLogString(const QString &strLog)
{
    m_strLog = strLog;
}

const QString& UIVMLogPage::logString() const
{
    return m_strLog;
}

void UIVMLogPage::setFileName(const QString &strFileName)
{
    m_strFileName = strFileName;
}

const QString& UIVMLogPage::fileName() const
{
    return m_strFileName;
}

void UIVMLogPage::setTextEdit(const QString &strText)
{
    m_pPlainTextEdit->setPlainText(strText);
    /* Move the cursor position to end: */
    QTextCursor cursor = m_pPlainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    m_pPlainTextEdit->setTextCursor(cursor);
    repaint();
}

void UIVMLogPage::markForError()
{
    QPalette pal = m_pPlainTextEdit->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    m_pPlainTextEdit->setPalette(pal);
}

void UIVMLogPage::setScrollBarMarkingsVector(const QVector<float> &vector)
{
    if (!m_pPlainTextEdit)
        return;
    UIIndicatorScrollBar* scrollBar = qobject_cast<UIIndicatorScrollBar*>(m_pPlainTextEdit->verticalScrollBar());
    if (scrollBar)
        scrollBar->setMarkingsVector(vector);
    repaint();
}

void UIVMLogPage::clearScrollBarMarkingsVector()
{
    if (!m_pPlainTextEdit)
        return;
    UIIndicatorScrollBar* scrollBar = qobject_cast<UIIndicatorScrollBar*>(m_pPlainTextEdit->verticalScrollBar());
    if (scrollBar)
        scrollBar->clearMarkingsVector();
    repaint();
}

void UIVMLogPage::documentUndo()
{
    if (!m_pPlainTextEdit)
        return;
    if (m_pPlainTextEdit->document())
        m_pPlainTextEdit->document()->undo();
}


void UIVMLogPage::deleteBookmark(int index)
{
    if (m_bookmarkVector.size() <= index)
         return;
    m_bookmarkVector.remove(index, 1);
}

void UIVMLogPage::deleteAllBookmarks()
{
    m_bookmarkVector.clear();
}

void UIVMLogPage::scrollToBookmark(int bookmarkIndex)
{
    if(!m_pPlainTextEdit)
        return;
    if (bookmarkIndex >= m_bookmarkVector.size())
        return;

    int lineNumber = m_bookmarkVector.at(bookmarkIndex).first;
    QTextDocument* document = m_pPlainTextEdit->document();
    if(!document)
        return;

    QTextCursor cursor(document->findBlockByLineNumber(lineNumber));
    m_pPlainTextEdit->setTextCursor(cursor);
}

const QVector<LogBookmark>& UIVMLogPage::bookmarkVector() const
{
    return m_bookmarkVector;
}

void UIVMLogPage::sltAddBookmark(LogBookmark bookmark)
{
    m_bookmarkVector.push_back(bookmark);
    emit sigBookmarksUpdated();
}
// void UIVMLogViewerWidget::sltCreateBookmarkAtCurrent()
// {
    // if (!currentTextEdit())
    //     return;
    // QWidget* viewport = currentTextEdit()->viewport();
    // if (!viewport)
    //     return;
    // QPoint point(0.5 * viewport->width(), 0.5 * viewport->height());
    // QTextBlock block = currentTextEdit()->cursorForPosition(point).block();
    // LogBookmark bookmark;
    // bookmark.first = block.firstLineNumber();
    // bookmark.second = block.text();
    // sltCreateBookmarkAtLine(bookmark);
//}


#include "UIVMLogPage.moc"
