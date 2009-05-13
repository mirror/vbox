/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMLogViewer class implementation
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

#include <VBoxVMLogViewer.h>
#include <VBoxGlobal.h>
#include <VBoxProblemReporter.h>

/* Qt includes */
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QCheckBox>
#include <QDir>
#include <QScrollBar>
#include <QFileDialog>
#include <QDateTime>
#include <QPushButton>
#include <QKeyEvent>

VBoxVMLogViewer::LogViewersMap VBoxVMLogViewer::mSelfArray = LogViewersMap();

void VBoxVMLogViewer::createLogViewer (QWidget *aCenterWidget, CMachine &aMachine)
{
    if (!mSelfArray.contains (aMachine.GetName()))
    {
        /* Creating new log viewer if there is no one existing */
#ifdef Q_WS_MAC
        VBoxVMLogViewer *lv = new VBoxVMLogViewer (aCenterWidget, Qt::Window, aMachine);
#else /* Q_WS_MAC */
        VBoxVMLogViewer *lv = new VBoxVMLogViewer (NULL, Qt::Window, aMachine);
#endif /* Q_WS_MAC */

        lv->centerAccording (aCenterWidget);
        connect (vboxGlobal().mainWindow(), SIGNAL (closing()), lv, SLOT (close()));
        lv->setAttribute (Qt::WA_DeleteOnClose);
        mSelfArray [aMachine.GetName()] = lv;
    }

    VBoxVMLogViewer *viewer = mSelfArray [aMachine.GetName()];
    viewer->show();
    viewer->raise();
    viewer->setWindowState (viewer->windowState() & ~Qt::WindowMinimized);
    viewer->activateWindow();
}


VBoxVMLogViewer::VBoxVMLogViewer (QWidget *aParent,
                                  Qt::WindowFlags aFlags,
                                  const CMachine &aMachine)
    : QIWithRetranslateUI2<QIMainDialog> (aParent, aFlags)
    , mIsPolished (false)
    , mFirstRun (true)
    , mMachine (aMachine)
{
    /* Apply UI decorations */
    Ui::VBoxVMLogViewer::setupUi (this);

    /* Apply window icons */
    setWindowIcon (vboxGlobal().iconSetFull (QSize (32, 32), QSize (16, 16),
                                             ":/vm_show_logs_32px.png", ":/show_logs_16px.png"));

    /* Enable size grip without using a status bar. */
    setSizeGripEnabled (true);

    /* Logs list creation */
    mLogList = new QTabWidget (mLogsFrame);
    QVBoxLayout *logsFrameLayout = new QVBoxLayout (mLogsFrame);
    logsFrameLayout->setContentsMargins (0, 0, 0, 0);
    logsFrameLayout->addWidget (mLogList);

    connect (mLogList, SIGNAL (currentChanged (int)),
             this, SLOT (currentLogPageChanged (int)));

    /* Search panel creation */
    mSearchPanel = new VBoxLogSearchPanel (mLogsFrame, this);
    logsFrameLayout->addWidget (mSearchPanel);
    mSearchPanel->hide();

    /* Add missing buttons & retrieve standard buttons */
    mBtnHelp = mButtonBox->button (QDialogButtonBox::Help);
    mBtnFind = mButtonBox->addButton (QString::null, QDialogButtonBox::ActionRole);
    mBtnSave = mButtonBox->button (QDialogButtonBox::Save);
    mBtnRefresh = mButtonBox->addButton (QString::null, QDialogButtonBox::ActionRole);
    mBtnClose = mButtonBox->button (QDialogButtonBox::Close);

    /* Setup connections */
    connect (mButtonBox, SIGNAL (helpRequested()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (mBtnFind, SIGNAL (clicked()), this, SLOT (search()));
    connect (mBtnSave, SIGNAL (clicked()), this, SLOT (save()));
    connect (mBtnRefresh, SIGNAL (clicked()), this, SLOT (refresh()));

    /* Reading log files */
    refresh();
    /* Set the focus to the initial default button */
    defaultButton()->setDefault (true);
    defaultButton()->setFocus();
#ifdef Q_WS_MAC
    /* We have to force this to get the default button L&F on the mac. */
    defaultButton()->setEnabled (true);
#endif /* Q_WS_MAC */
    /* Loading language constants */
    retranslateUi();
}

VBoxVMLogViewer::~VBoxVMLogViewer()
{
    if (!mMachine.isNull())
        mSelfArray.remove (mMachine.GetName());
}

QTextEdit* VBoxVMLogViewer::currentLogPage()
{
    if (mLogList->isEnabled())
    {
        QWidget *container = mLogList->currentWidget();
        QTextEdit *browser = container->findChild<QTextEdit*>();
        Assert (browser);
        return browser ? browser : 0;
    }
    else
        return 0;
}


bool VBoxVMLogViewer::close()
{
    mSearchPanel->hide();
    return QIMainDialog::close();
}

void VBoxVMLogViewer::refresh()
{
    /* Clearing old data if any */
    mLogFilesList.clear();
    mLogList->setEnabled (true);
    while (mLogList->count())
    {
        QWidget *firstPage = mLogList->widget (0);
        mLogList->removeTab (0);
        delete firstPage;
    }

    bool isAnyLogPresent = false;

    /* Entering log files folder */
    QString logFilesPath = mMachine.GetLogFolder();
    QDir logFilesDir (logFilesPath);
    if (logFilesDir.exists())
    {
        /* Reading log files folder */
        QStringList filters;
        filters << "*.log" << "*.log.*";
        logFilesDir.setNameFilters (filters);
        QStringList logList = logFilesDir.entryList (QDir::Files);
        if (!logList.empty()) isAnyLogPresent = true;
        foreach (QString logFile, logList)
            loadLogFile (logFilesDir.filePath (logFile));
    }

    /* Create an empty log page if there are no logs at all */
    if (!isAnyLogPresent)
    {
        QTextEdit *dummyLog = createLogPage ("VBox.log");
        dummyLog->setWordWrapMode (QTextOption::WordWrap);
        dummyLog->setHtml (tr ("<p>No log files found. Press the "
            "<b>Refresh</b> button to rescan the log folder "
            "<nobr><b>%1</b></nobr>.</p>")
            .arg (logFilesPath));
        /* We don't want it to remain white */
        QPalette pal = dummyLog->palette();
        pal.setColor (QPalette::Base, pal.color (QPalette::Window));
        dummyLog->setPalette (pal);
    }

    /* Show the first tab widget's page after the refresh */
    mLogList->setCurrentIndex (0);
    currentLogPageChanged (0);

    /* Enable/Disable save button & tab widget according log presence */
    mBtnFind->setEnabled (isAnyLogPresent);
    mBtnSave->setEnabled (isAnyLogPresent);
    mLogList->setEnabled (isAnyLogPresent);
    /* Default to the save button if there are any log files otherwise to the
     * close button. The initial automatic of the main dialog has to be
     * overwritten */
    setDefaultButton (isAnyLogPresent ? mBtnSave:mBtnClose);
}

void VBoxVMLogViewer::save()
{
    /* Prepare "save as" dialog */
    QFileInfo fileInfo (mLogFilesList [mLogList->currentIndex()]);
    QDateTime dtInfo = fileInfo.lastModified();
    QString dtString = dtInfo.toString ("yyyy-MM-dd-hh-mm-ss");
    QString defaultFileName = QString ("%1-%2.log")
        .arg (mMachine.GetName()).arg (dtString);
    QString defaultFullName = QDir::toNativeSeparators (
        QDir::home().absolutePath() + "/" + defaultFileName);
    QString newFileName = QFileDialog::getSaveFileName (this,
        tr ("Save VirtualBox Log As"), defaultFullName);

    /* Save new log into the file */
    if (!newFileName.isEmpty())
    {
        /* Reread log data */
        QFile oldFile (mLogFilesList [mLogList->currentIndex()]);
        QFile newFile (newFileName);
        if (!oldFile.open (QIODevice::ReadOnly) ||
            !newFile.open (QIODevice::WriteOnly))
            return;

        /* Save log data into the new file */
        newFile.write (oldFile.readAll());
    }
}

void VBoxVMLogViewer::search()
{
    mSearchPanel->isHidden() ? mSearchPanel->show() : mSearchPanel->hide();
}

void VBoxVMLogViewer::currentLogPageChanged (int aIndex)
{
    if (aIndex >= 0 &&
        aIndex < mLogFilesList.count())
        setFileForProxyIcon (mLogFilesList.at (aIndex));
}

void VBoxVMLogViewer::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMLogViewer::retranslateUi (this);

    /* Setup a dialog caption */
    if (!mMachine.isNull())
        setWindowTitle (tr ("%1 - VirtualBox Log Viewer").arg (mMachine.GetName()));

    mBtnFind->setText (tr ("&Find"));
    mBtnRefresh->setText (tr ("&Refresh"));
    mBtnSave->setText (tr ("&Save"));
    mBtnClose->setText (tr ("Close"));
}

void VBoxVMLogViewer::showEvent (QShowEvent *aEvent)
{
    QIMainDialog::showEvent (aEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (mIsPolished)
        return;

    mIsPolished = true;

    if (mFirstRun)
    {
        /* Resize the whole log-viewer to fit 80 symbols in
         * text-browser for the first time started */
        QTextEdit *firstPage = currentLogPage();
        if (firstPage)
        {
            int fullWidth =
                firstPage->fontMetrics().width (QChar ('x')) * 80 +
                firstPage->verticalScrollBar()->width() +
                firstPage->frameWidth() * 2 +
                /* mLogList margin */ 10 * 2 +
                /* CentralWidget margin */ 10 * 2;
            resize (fullWidth, height());
            mFirstRun = false;
        }
    }
}

void VBoxVMLogViewer::loadLogFile (const QString &aFileName)
{
    /* Prepare log file */
    QFile logFile (aFileName);
    if (!logFile.exists() || !logFile.open (QIODevice::ReadOnly))
        return;

    /* Read log file and write it into the log page */
    QTextEdit *logViewer = createLogPage (QFileInfo (aFileName).fileName());
    logViewer->setPlainText (logFile.readAll());

    mLogFilesList << aFileName;
}

QTextEdit* VBoxVMLogViewer::createLogPage (const QString &aName)
{
    QWidget *pageContainer = new QWidget();
    QVBoxLayout *pageLayout = new QVBoxLayout (pageContainer);
    QTextEdit *logViewer = new QTextEdit (pageContainer);
    pageLayout->addWidget (logViewer);
    pageLayout->setContentsMargins (10, 10, 10, 10);

    QFont font = logViewer->currentFont();
    font.setFamily ("Courier New,courier");
    logViewer->setFont (font);
    logViewer->setWordWrapMode (QTextOption::NoWrap);
    logViewer->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOn);
    logViewer->setReadOnly (true);

    mLogList->addTab (pageContainer, aName);
    return logViewer;
}


VBoxLogSearchPanel::VBoxLogSearchPanel (QWidget *aParent,
                                        VBoxVMLogViewer *aViewer)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mViewer (aViewer)
    , mButtonClose (0)
    , mSearchName (0), mSearchString (0)
    , mButtonPrev (0), mButtonNext (0)
    , mCaseSensitive (0)
    , mWarningSpacer (0), mWarningIcon (0), mWarningString (0)
{
    mButtonClose = new QToolButton (this);
    mButtonClose->setAutoRaise (true);
    mButtonClose->setFocusPolicy (Qt::TabFocus);
    mButtonClose->setShortcut (QKeySequence (Qt::Key_Escape));
    connect (mButtonClose, SIGNAL (clicked()), this, SLOT (hide()));
    mButtonClose->setIcon (VBoxGlobal::iconSet (":/delete_16px.png",
                                                ":/delete_dis_16px.png"));

    mSearchName = new QLabel (this);
    mSearchString = new QLineEdit (this);
    mSearchString->setSizePolicy (QSizePolicy::Preferred,
                                  QSizePolicy::Fixed);
    connect (mSearchString, SIGNAL (textChanged (const QString &)),
             this, SLOT (findCurrent (const QString &)));

    mButtonNext = new QToolButton (this);
    mButtonNext->setEnabled (false);
    mButtonNext->setAutoRaise (true);
    mButtonNext->setFocusPolicy (Qt::TabFocus);
    mButtonNext->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    connect (mButtonNext, SIGNAL (clicked()), this, SLOT (findNext()));
    mButtonNext->setIcon (VBoxGlobal::iconSet (":/list_movedown_16px.png",
                                               ":/list_movedown_disabled_16px.png"));

    mButtonPrev = new QToolButton (this);
    mButtonPrev->setEnabled (false);
    mButtonPrev->setAutoRaise (true);
    mButtonPrev->setFocusPolicy (Qt::TabFocus);
    mButtonPrev->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    connect (mButtonPrev, SIGNAL (clicked()), this, SLOT (findBack()));
    mButtonPrev->setIcon (VBoxGlobal::iconSet (":/list_moveup_16px.png",
                                               ":/list_moveup_disabled_16px.png"));

    mCaseSensitive = new QCheckBox (this);

    mWarningSpacer = new QSpacerItem (0, 0, QSizePolicy::Fixed,
                                            QSizePolicy::Minimum);
    mWarningIcon = new QLabel (this);
    mWarningIcon->hide();

    QIcon icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning);
    if (!icon.isNull())
        mWarningIcon->setPixmap (icon.pixmap (16, 16));
    mWarningString = new QLabel (this);
    mWarningString->hide();

    QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Expanding,
                                                 QSizePolicy::Minimum);

    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    mainLayout->setSpacing (5);
    mainLayout->setContentsMargins (0, 0, 0, 0);
    mainLayout->addWidget (mButtonClose);
    mainLayout->addWidget (mSearchName);
    mainLayout->addWidget (mSearchString);
    mainLayout->addWidget (mButtonNext);
    mainLayout->addWidget (mButtonPrev);
    mainLayout->addWidget (mCaseSensitive);
    mainLayout->addItem   (mWarningSpacer);
    mainLayout->addWidget (mWarningIcon);
    mainLayout->addWidget (mWarningString);
    mainLayout->addItem   (spacer);

    setFocusProxy (mCaseSensitive);
    qApp->installEventFilter (this);

    retranslateUi();
}

void VBoxLogSearchPanel::retranslateUi()
{
    mButtonClose->setToolTip (tr ("Close the search panel"));

    mSearchName->setText (tr ("Find "));
    mSearchString->setToolTip (tr ("Enter a search string here"));

    VBoxGlobal::setTextLabel (mButtonPrev, tr ("&Previous"));
    mButtonPrev->setToolTip (tr ("Search for the previous occurrence "
                                 "of the string"));

    VBoxGlobal::setTextLabel (mButtonNext, tr ("&Next"));
    mButtonNext->setToolTip (tr ("Search for the next occurrence of "
                                 "the string"));

    mCaseSensitive->setText (tr ("C&ase Sensitive"));
    mCaseSensitive->setToolTip (tr ("Perform case sensitive search "
                                    "(when checked)"));

    mWarningString->setText (tr ("String not found"));
}

void VBoxLogSearchPanel::findCurrent (const QString &aSearchString)
{
    mButtonNext->setEnabled (aSearchString.length());
    mButtonPrev->setEnabled (aSearchString.length());
    toggleWarning (!aSearchString.length());
    if (aSearchString.length())
        search (true, true);
    else
    {
        QTextEdit *browser = mViewer->currentLogPage();
        if (browser && browser->textCursor().hasSelection())
        {
            QTextCursor cursor = browser->textCursor();
            cursor.setPosition (cursor.anchor());
            browser->setTextCursor (cursor);
        }
    }
}

void VBoxLogSearchPanel::search (bool aForward,
                                 bool aStartCurrent)
{
    QTextEdit *browser = mViewer->currentLogPage();
    if (!browser) return;

    QTextCursor cursor = browser->textCursor();
    int pos = cursor.position();
    int anc = cursor.anchor();

    QString text = browser->toPlainText();
    int diff = aStartCurrent ? 0 : 1;

    int res = -1;
    if (aForward && (aStartCurrent || pos < text.size() - 1))
        res = text.indexOf (mSearchString->text(),
                            anc + diff,
                            mCaseSensitive->isChecked() ?
                            Qt::CaseSensitive : Qt::CaseInsensitive);
    else if (!aForward && anc > 0)
        res = text.lastIndexOf (mSearchString->text(), anc - 1,
                                mCaseSensitive->isChecked() ?
                                Qt::CaseSensitive : Qt::CaseInsensitive);

    if (res != -1)
    {
        cursor.movePosition (QTextCursor::Start,
                             QTextCursor::MoveAnchor);
        cursor.movePosition (QTextCursor::NextCharacter,
                             QTextCursor::MoveAnchor, res);
        cursor.movePosition (QTextCursor::NextCharacter,
                             QTextCursor::KeepAnchor,
                             mSearchString->text().size());
        browser->setTextCursor (cursor);
    }

    toggleWarning (res != -1);
}

bool VBoxLogSearchPanel::eventFilter (QObject *aObject, QEvent *aEvent)
{
    switch (aEvent->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);

            /* handle the Enter keypress for mSearchString
             * widget as a search next string action */
            if (aObject == mSearchString &&
                (e->QInputEvent::modifiers() == 0 ||
                 e->QInputEvent::modifiers() & Qt::KeypadModifier) &&
                (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return))
            {
                mButtonNext->animateClick();
                return true;
            }
            /* handle other search next/previous shortcuts */
            else if (e->key() == Qt::Key_F3)
            {
                if (e->QInputEvent::modifiers() == 0)
                    mButtonNext->animateClick();
                else if (e->QInputEvent::modifiers() == Qt::ShiftModifier)
                    mButtonPrev->animateClick();
                return true;
            }
            /* handle ctrl-f key combination as a shortcut to
             * move to the search field */
            else if (e->QInputEvent::modifiers() == Qt::ControlModifier &&
                     e->key() == Qt::Key_F)
            {
                if (mViewer->currentLogPage())
                {
                    if (isHidden()) show();
                    mSearchString->setFocus();
                    return true;
                }
            }
            /* handle alpha-numeric keys to implement the
             * "find as you type" feature */
            else if ((e->QInputEvent::modifiers() & ~Qt::ShiftModifier) == 0 &&
                     e->key() >= Qt::Key_Exclam &&
                     e->key() <= Qt::Key_AsciiTilde)
            {
                if (mViewer->currentLogPage())
                {
                    if (isHidden()) show();
                    mSearchString->setFocus();
                    mSearchString->insert (e->text());
                    return true;
                }
            }
            break;
        }
        default:
            break;
    }
    return false;
}

void VBoxLogSearchPanel::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);
    mSearchString->setFocus();
    mSearchString->selectAll();
}

void VBoxLogSearchPanel::hideEvent (QHideEvent *aEvent)
{
    QWidget *focus = QApplication::focusWidget();
    if (focus &&
        focus->parent() == this)
       focusNextPrevChild (true);
    QWidget::hideEvent (aEvent);
}

void VBoxLogSearchPanel::toggleWarning (bool aHide)
{
    mWarningSpacer->changeSize (aHide ? 0 : 16, 0, QSizePolicy::Fixed,
                                                   QSizePolicy::Minimum);
    mWarningIcon->setHidden (aHide);
    mWarningString->setHidden (aHide);
}

