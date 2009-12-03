/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMLogViewer class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxVMLogViewer_h__
#define __VBoxVMLogViewer_h__

#include "VBoxVMLogViewer.gen.h"
#include "QIMainDialog.h"
#include "COMDefs.h"
#include "QIWithRetranslateUI.h"

class VBoxLogSearchPanel;
class QTabWidget;
class QTextEdit;
class VBoxSearchField;
class QLabel;
class QIToolButton;
class VBoxMiniCancelButton;
class VBoxSegmentedButton;
class QCheckBox;

class VBoxVMLogViewer : public QIWithRetranslateUI2<QIMainDialog>,
                        public Ui::VBoxVMLogViewer
{
    Q_OBJECT;

public:

    typedef QMap <QString, VBoxVMLogViewer*> LogViewersMap;

    VBoxVMLogViewer (QWidget *aParent, Qt::WindowFlags aFlags,
                     const CMachine &aMachine);
   ~VBoxVMLogViewer();

    static void createLogViewer (QWidget *aParent, CMachine &aMachine);

    QTextEdit* currentLogPage();


protected:

    void retranslateUi();

private slots:

    bool close();
    void refresh();
    void save();
    void search();

    void currentLogPageChanged (int aIndex);

private:

    void showEvent (QShowEvent *aEvent);
    void loadLogFile (const QString &aName);
    QTextEdit* createLogPage (const QString &aPage);

    static LogViewersMap  mSelfArray;

    bool                  mIsPolished;
    bool                  mFirstRun;
    CMachine              mMachine;
    QTabWidget           *mLogList;
    QStringList           mLogFilesList;
    VBoxLogSearchPanel   *mSearchPanel;

    QPushButton *mBtnHelp;
    QPushButton *mBtnFind;
    QPushButton *mBtnSave;
    QPushButton *mBtnRefresh;
    QPushButton *mBtnClose;
};

class VBoxLogSearchPanel : public QIWithRetranslateUI <QWidget>
{
    Q_OBJECT;

public:

    VBoxLogSearchPanel (QWidget *aParent,
                        VBoxVMLogViewer *aViewer);
protected:

    void retranslateUi();

private slots:

    void find (int aButton);
    void findNext() { search (true); }
    void findBack() { search (false); }

    void findCurrent (const QString &aSearchString);

private:

    void search (bool aForward, bool aStartCurrent = false);

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    void showEvent (QShowEvent *aEvent);
    void hideEvent (QHideEvent *aEvent);

    void toggleWarning (bool aHide);

    VBoxVMLogViewer      *mViewer;
    VBoxMiniCancelButton *mButtonClose;
    QLabel               *mSearchName;
    VBoxSearchField      *mSearchString;
    VBoxSegmentedButton  *mButtonsNextPrev;
    QCheckBox            *mCaseSensitive;
    QSpacerItem          *mWarningSpacer;
    QLabel               *mWarningIcon;
    QLabel               *mWarningString;
};

#endif // __VBoxVMLogViewer_h__

