/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlConsole class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIGuestControlConsole_h
#define FEQT_INCLUDED_SRC_guestctrl_UIGuestControlConsole_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
# include <QPlainTextEdit>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"

class UIGuestControlInterface;
/** QPlainTextEdit extension to provide a simple terminal like widget. */
class UIGuestControlConsole : public QPlainTextEdit
{

    Q_OBJECT;


public:

    UIGuestControlConsole(const CGuest &comGuest, QWidget* parent = 0);
    /* @p strOutput is displayed in the console */
    void putOutput(const QString &strOutput);

protected:

    void keyPressEvent(QKeyEvent *pEvent) /* override */;
    void mousePressEvent(QMouseEvent *pEvent) /* override */;
    void mouseDoubleClickEvent(QMouseEvent *pEvent) /* override */;
    void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;

private slots:

    void sltOutputReceived(const QString &strOutput);

private:

    typedef QVector<QString> CommandHistory;
    typedef QMap<QString, int> TabDictionary;

    void           reset();
    void           startNextLine();
    /** Return the text of the curent line */
    QString        getCommandString();
    /** Replaces the content of the last line with m_strPromt + @p stringNewContent */
    void           replaceLineContent(const QString &stringNewContent);
    /** Get next/prev command from history. Return @p originalString if history is empty. */
    QString        getNextCommandFromHistory(const QString &originalString = QString());
    QString        getPreviousCommandFromHistory(const QString &originalString = QString());
    void           completeByTab();
    void           commandEntered(const QString &strCommand);

    /* Return a list of words that start with @p strSearch */
    QList<QString> matchedWords(const QString &strSearch) const;
    CGuest         m_comGuest;
    const QString  m_strGreet;
    const QString  m_strPrompt;
    TabDictionary  m_tabDictinary;
    /* A vector of entered commands */
    CommandHistory m_tCommandHistory;
    unsigned       m_uCommandHistoryIndex;
    UIGuestControlInterface  *m_pControlInterface;
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIGuestControlConsole_h */
