/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogPage_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QUuid>
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

/** UIVMLogPage defines data and functionalities of the each tab page of a UIVMLogViewerWidget.
 *  It stores the original log file content , a list of bookmarks, etc */
class UIVMLogPage  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigBookmarksUpdated();
    void sigLogPageFilteredChanged(bool isFiltered);

public:

    UIVMLogPage(QWidget *pParent = 0);
    ~UIVMLogPage();

    /** Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    QPlainTextEdit *textEdit();
    QTextDocument  *document();

    void setLogContent(const QString &strLogContent, bool fError);
    const QString& logString() const;

    void setLogFileName(const QString &strFileName);
    const QString& logFileName() const;

    /** Marks the plain text edit When we dont have a log content. */
    void markForError();

    void setScrollBarMarkingsVector(const QVector<float> &vector);
    void clearScrollBarMarkingsVector();

    /** Undos the changes done to textDocument */
    void documentUndo();

    void deleteBookmark(int index);

    const QVector<LogBookmark>& bookmarkVector() const;
    void setBookmarkVector(const QVector<LogBookmark>& booksmarks);

    void deleteAllBookmarks();
    /** Scrolls the plain text edit to the bookmark with index @a bookmarkIndex. */
    void scrollToBookmark(int bookmarkIndex);

    bool isFiltered() const;
    void setFiltered(bool filtered);

    void setShowLineNumbers(bool bShowLineNumbers);
    void setWrapLines(bool bWrapLines);

    QFont currentFont() const;
    void setCurrentFont(QFont font);

    const QUuid &machineId() const;
    void setMachineId(const QUuid &machineId);

    void setLogFileId(int iLogFileId);
    int logFileId() const;

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

    /** Set plaintextEdit's text. Note that the text we
     *  show currently might be different than
     *  m_strLog. For example during filtering. */
    void setTextEditText(const QString &strText);
    void setTextEditTextAsHtml(const QString &strText);

    QHBoxLayout    *m_pMainLayout;
    UIVMLogViewerTextEdit *m_pTextEdit;
    /** Stores the log file (unmodified by filtering etc) content. */
    QString         m_strLog;
    /** Stores full path and name of the log file. */
    QString         m_strLogFileName;
    /** Stores the bookmarks of the logpage. All other bookmark related containers are updated wrt. this one. */
    QVector<LogBookmark> m_bookmarkVector;

    /** Keeps the index of the selected bookmark. Used especially when moving from one tab to another. */
    int                  m_iSelectedBookmarkIndex;

    /** @name Filtering related state variables
     * @{ */
    /** Designates whether currently displayed text is log text or a filtered version of it. That is
        if m_bFiltered is false than (m_strLog == m_pTextEdit->text()). */
        bool           m_bFiltered;
    /** @} */
    /** Id of the machine the log shown in this page belongs to. */
    QUuid m_machineId;
    /** The id we pass to CMachine::ReadLog. Used while refreshing and saving page content. */
    int m_iLogFileId;
};

#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogPage_h */
