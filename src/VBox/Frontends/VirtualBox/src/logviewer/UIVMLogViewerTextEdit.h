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
   a special scrollbar, line number area, and bookmark support: */
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

protected:

    void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;
    void resizeEvent(QResizeEvent *pEvent) /* override */;
    void mouseMoveEvent(QMouseEvent *pEvent) /* override */;

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

    friend class UILineNumberArea;
};




#endif /* !___UIVMLogPage_h___ */
