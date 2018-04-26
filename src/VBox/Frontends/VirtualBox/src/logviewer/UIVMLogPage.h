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

/** UIVMLogPage defines data and functionalities of the each tab page of a UIVMLogViewerWidget.
 *  It stores the original log file content , a list of bookmarks, etc */
class UIVMLogPage  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigBookmarksUpdated();
    void sigLogPageFilteredChanged(bool isFiltered);

public:

    UIVMLogPage(QWidget *pParent = 0, int tabIndex = -1);
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

    void setLogFileName(const QString &strFileName);
    const QString& logFileName() const;

    /** Set plaintextEdit's text. Note that the text we
     *  show currently might be different than
     *  m_strLog. For example during filtering. */
    void setTextEditText(const QString &strText);
    void setTextEditTextAsHtml(const QString &strText);

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

    /** setFilterParameters is called at the end of filtering operation to store the parameter etc.
     *  these parameters are used to decide whether we have to reapply the filter, and if not to
     *  update filter panel with correct line counts etc.*/
    void setFilterParameters(const QSet<QString> &filterTermSet, int filterOperationType,
                             int iFilteredLineCount, int iUnfilteredLineCount);
    int  filteredLineCount() const;
    int  unfilteredLineCount() const;
    /** Compares filter parameters with previous filter operation's parameters to decide if the
     *  filter should be applied again. */
    bool shouldFilterBeApplied(const QSet<QString> &filterTermSet, int filterOperationType) const;

    QFont currentFont() const;
    void setCurrentFont(QFont font);

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
    QString         m_strLogFileName;
    /** This is the index of the tab containing this widget in UIVMLogViewerWidget. */
    int             m_tabIndex;
    /** Stores the bookmarks of the logpage. All other bookmark related containers are updated wrt. this one. */
    QVector<LogBookmark> m_bookmarkVector;

    /** Keeps the index of the selected bookmark. Used especially when moving from one tab to another. */
    int                  m_iSelectedBookmarkIndex;

    /** @name Filtering related state variables
     * @{ */
    /** Designates whether currently displayed text is log text or a filtered version of it. That is
        if m_bFiltered is false than (m_strLog == m_pTextEdit->text()). */
        bool           m_bFiltered;
        /** The set of filter terms used in the last filtering.
            Used when deciding whether we have to reapply the filter or not. see shouldFilterBeApplied function. */
        QSet<QString>  m_filterTermSet;
        /** The type of the boolean last filtering operation. Used in deciding whether we have to reapply the
            filter. see shouldFilterBeApplied function. This is int cast of enum FilterOperatorButton
            of UIVMLogViewerFilterPanel. */
        int            m_filterOperationType;
        /** These counts are saveds and restored during filtering operation. If filter is not reapplied these counts
            are shown in the filter panel. */
        int            m_iFilteredLineCount;
        int            m_iUnfilteredLineCount;
    /** @} */

};

#endif /* !___UIVMLogPage_h___ */
