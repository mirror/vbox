/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
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

#ifndef ___UIVMLogViewerTextEdit_h___
#define ___UIVMLogViewerTextEdit_h___

/* Qt includes: */
#include <QPlainTextEdit>
#include <QPair>


/* QPlainTextEdit extension for some addtional context menu items,
   a special scrollbar, line number area, and bookmarking support: */
class UIVMLogViewerTextEdit : public QPlainTextEdit
{
    Q_OBJECT;

signals:

    void sigAddBookmark(QPair<int, QString> bookmark);
    void sigDeleteBookmark(QPair<int, QString> bookmark);

public:

    UIVMLogViewerTextEdit(QWidget* parent = 0);
    int  lineNumberAreaWidth();
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    /** Forwards the call to scroll bar class */
    void setScrollBarMarkingsVector(const QVector<float> &vector);
    /** Forwards the call to scroll bar class */
    void clearScrollBarMarkingsVector();

    void scrollToLine(int lineNumber);
    void setBookmarkLineSet(const QSet<int>& lineSet);
    void setShownTextIsFiltered(bool warning);

    void setShowLineNumbers(bool bShowLineNumbers);
    void setWrapLines(bool bWrapLines);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    virtual void leaveEvent(QEvent * event) /* override */;

private slots:

    void sltBookmark();
    void sltUpdateLineNumberAreaWidth(int newBlockCount);
    void sltUpdateLineNumberArea(const QRect &, int);
    int  visibleLineCount();

private:

    void prepare();
    void prepareWidgets();
    QPair<int, QString> bookmarkForPos(const QPoint &position);
    int  lineNumberForPos(const QPoint &position);
    void setMouseCursorLine(int lineNumber);
    /** If bookmark exists this function removes it, if not it adds the bookmark. */
    void toggleBookmark(const QPair<int, QString>& bookmark);
    /** Line number and text at the context menu position */
    QPair<int, QString>  m_iContextMenuBookmark;
    QWidget             *m_pLineNumberArea;
    /** Set of bookmarked lines. This set is updated from UIVMLogPage. This set is
        used only for lookup in this class. */
    QSet<int>            m_bookmarkLineSet;
    /** Number of the line under the mouse cursor. */
    int                  m_mouseCursorLine;
    /** If true the we draw a text near the top right corner of the text edit to warn
        the user the text edit's content is filtered (as oppesed to whole log file content.
        And we dont display bookmarks and adding/deleting bookmarks are disabled. */
    bool                 m_bShownTextIsFiltered;
    bool                 m_bShowLineNumbers;
    bool                 m_bWrapLines;

    friend class UILineNumberArea;
};




#endif /* !___UIVMLogPage_h___ */
