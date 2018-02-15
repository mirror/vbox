/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlConsole class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlConsole_h___
#define ___UIGuestControlConsole_h___

/* Qt includes: */
# include <QPlainTextEdit>

class UIGuestControlConsole : public QPlainTextEdit
{

    Q_OBJECT;

signals:

    void commandEntered(QString command);

public:

    UIGuestControlConsole(QWidget* parent = 0);
    void putOutput(const QString &strOutput);

protected:

    void keyPressEvent(QKeyEvent *pEvent) /* override */;
    void mousePressEvent(QMouseEvent *pEvent) /* override */;
    void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;
    void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;

private slots:


private:
    typedef QVector<QString> CommandHistory;
    int      currentLineNumber() const;
    void     reset();
    void     startNextLine();
    /** Return the text of the curent line */
    QString  getCommandString();
    /** Replaces the content of the last line with m_strPromt + @p stringNewContent */
    void     replaceLineContent(const QString &stringNewContent);
    /** Get next/prev command from history. Return @p originalString if history is empty. */
    QString     getNextCommandFromHistory(const QString &originalString = QString());
    QString     getPreviousCommandFromHistory(const QString &originalString = QString());
    void        printCommandHistory() const;

    /** Stores the line number the edit cursor */
    unsigned m_uLineNumber;
    const QString m_strGreet;
    const QString m_strPrompt;

    /* A vector of entered commands */
    CommandHistory m_tCommandHistory;
    unsigned       m_uCommandHistoryIndex;
};

#endif /* !___UIGuestControlConsole_h___ */
