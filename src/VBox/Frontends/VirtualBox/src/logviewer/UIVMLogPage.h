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

#ifndef ___UIVMLogPage_h___
#define ___UIVMLogPage_h___

/* Qt includes: */
#include <QWidget>
/* #include <QMap> */
#include <QPair>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QITabWidget;
class QHBoxLayout;
class QPlainTextEdit;
class UIVMLogViewerTextEdit;

/* Type definitions: */
/** first is line number, second is block text */
typedef QPair<int, QString> LogBookmark;

class UIVMLogPage  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigBookmarksUpdated();
    void sigLogPageFilteredChanged(bool isFiltered);

public:

    UIVMLogPage(QWidget *pParent = 0, int tabIndex = -1);
    /** Destructs the VM Log-Viewer. */
    ~UIVMLogPage();
    /** Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    QPlainTextEdit *textEdit();
    QTextDocument  *document();

    void setTabIndex(int index);
    int tabIndex()  const;

    /* Only to be called when log file is re-read. */
    void setLogString(const QString &strLog);
    const QString& logString() const;

    void setFileName(const QString &strFileName);
    const QString& fileName() const;

    /** Ses plaintextEdit's text. Note that the text we
        show currently might be different than
        m_strLog. For example during filtering. */
    void setTextEdit(const QString &strText);
    void setTextEditAsHtml(const QString &strText);

    /** Marks the plain text edit When we dont have a log content. */
    void markForError();

    void setScrollBarMarkingsVector(const QVector<float> &vector);
    void clearScrollBarMarkingsVector();

    /** Undos the changes done to textDocument */
    void documentUndo();

    void deleteBookmark(int index);

    const QVector<LogBookmark>& bookmarkVector() const;
    void deleteAllBookmarks();
    /** Scrolls the plain text edit to the bookmark with index @a bookmarkIndex. */
    void scrollToBookmark(int bookmarkIndex);

    bool isFiltered() const;
    void setFiltered(bool filtered);

    void setShowLineNumbers(bool bShowLineNumbers);
    void setWrapLines(bool bWrapLines);

protected:

    virtual void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    void sltAddBookmark(LogBookmark bookmark);
    void sltDeleteBookmark(LogBookmark bookmark);

private:

    void prepare();
    void prepareWidgets();
    void cleanup();
    void retranslateUi();
    void updateTextEditBookmarkLineSet();
    void deleteBookmark(LogBookmark bookmark);

    QHBoxLayout    *m_pMainLayout;
    UIVMLogViewerTextEdit *m_pTextEdit;
    /** Stores the log file (unmodified) content. */
    QString         m_strLog;
    /** Stores full path and name of the log file. */
    QString         m_strFileName;
    /** This is the index of the tab containing this widget in UIVMLogViewerWidget. */
    int             m_tabIndex;
    /** Stores the bookmarks of the logpage. All other bookmark related containers are updated wrt. this one. */
    QVector<LogBookmark> m_bookmarkVector;
    use following two variables
    /** Keeps the index of the selected bookmark. Used especially when moving from one tab to another. */
    int                  m_iSelectedBookmarkIndex;
    /** Keeps the scrolled line number. Used when switching between tabs. */
    int                  m_iScrolledLineNumber;
    /** Designates whether currently displayed text is log text or a filtered version of it. That is
        if m_bFiltered is false than (m_strLog == m_pTextEdit->text()). */
    bool m_bFiltered;
    bool m_bShowLineNumbers;
    bool m_bWrapLines;
};

#endif /* !___UIVMLogPage_h___ */
