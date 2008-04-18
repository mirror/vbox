/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Virtual Log Viewer" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 Sun Microsystems, Inc.
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


class VBoxLogSearchPanel : public QWidget
{
    Q_OBJECT

public:

    VBoxLogSearchPanel (QWidget *aParent,
                        VBoxVMLogViewer *aViewer,
                        const char *aName)
        : QWidget (aParent, aName)
        , mViewer (aViewer)
        , mButtonClose (0)
        , mSearchName (0), mSearchString (0)
        , mButtonPrev (0), mButtonNext (0)
        , mCaseSensitive (0)
        , mWarningSpacer (0), mWarningIcon (0), mWarningString (0)
    {
        mButtonClose = new QToolButton (this);
        mButtonClose->setAutoRaise (true);
        mButtonClose->setFocusPolicy (QWidget::TabFocus);
        mButtonClose->setAccel (QKeySequence (Qt::Key_Escape));
        connect (mButtonClose, SIGNAL (clicked()), this, SLOT (hide()));
        mButtonClose->setIconSet (VBoxGlobal::iconSet ("delete_16px.png",
                                                   "delete_dis_16px.png"));

        mSearchName = new QLabel (this);
        mSearchString = new QLineEdit (this);
        mSearchString->setSizePolicy (QSizePolicy::Preferred,
                                      QSizePolicy::Fixed);
        connect (mSearchString, SIGNAL (textChanged (const QString &)),
                 this, SLOT (findCurrent (const QString &)));

        mButtonNext = new QToolButton (this);
        mButtonNext->setEnabled (false);
        mButtonNext->setAutoRaise (true);
        mButtonNext->setFocusPolicy (QWidget::TabFocus);
        mButtonNext->setUsesTextLabel (true);
        mButtonNext->setTextPosition (QToolButton::BesideIcon);
        connect (mButtonNext, SIGNAL (clicked()), this, SLOT (findNext()));
        mButtonNext->setIconSet (VBoxGlobal::iconSet ("list_movedown_16px.png",
                                           "list_movedown_disabled_16px.png"));

        mButtonPrev = new QToolButton (this);
        mButtonPrev->setEnabled (false);
        mButtonPrev->setAutoRaise (true);
        mButtonPrev->setFocusPolicy (QWidget::TabFocus);
        mButtonPrev->setUsesTextLabel (true);
        mButtonPrev->setTextPosition (QToolButton::BesideIcon);
        connect (mButtonPrev, SIGNAL (clicked()), this, SLOT (findBack()));
        mButtonPrev->setIconSet (VBoxGlobal::iconSet ("list_moveup_16px.png",
                                             "list_moveup_disabled_16px.png"));

        mCaseSensitive = new QCheckBox (this);

        mWarningSpacer = new QSpacerItem (0, 0, QSizePolicy::Fixed,
                                                QSizePolicy::Minimum);
        mWarningIcon = new QLabel (this);
        mWarningIcon->hide();
        QImage img = QMessageBox::standardIcon (QMessageBox::Warning).
                                                convertToImage();
        if (!img.isNull())
        {
            img = img.smoothScale (16, 16);
            QPixmap pixmap;
            pixmap.convertFromImage (img);
            mWarningIcon->setPixmap (pixmap);
        }
        mWarningString = new QLabel (this);
        mWarningString->hide();

        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Expanding,
                                                     QSizePolicy::Minimum);

        QHBoxLayout *mainLayout = new QHBoxLayout (this, 5, 5);
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
        topLevelWidget()->installEventFilter (this);

        languageChange();
    }

    void languageChange()
    {
        QKeySequence accel;

        QToolTip::add (mButtonClose, tr ("Close the search panel"));

        mSearchName->setText (tr ("Find "));
        QToolTip::add (mSearchString, tr ("Enter a search string here"));

        VBoxGlobal::setTextLabel (mButtonPrev, tr ("&Previous"));
        QToolTip::add (mButtonPrev,
            tr ("Search for the previous occurrence of the string"));

        VBoxGlobal::setTextLabel (mButtonNext, tr ("&Next"));
        QToolTip::add (mButtonNext,
            tr ("Search for the next occurrence of the string"));

        mCaseSensitive->setText (tr ("C&ase Sensitive"));
        QToolTip::add (mCaseSensitive,
            tr ("Perform case sensitive search (when checked)"));

        mWarningString->setText (tr ("String not found"));
    }

private slots:

    void findNext()
    {
        search (true);
    }

    void findBack()
    {
        search (false);
    }

    void findCurrent (const QString &aSearchString)
    {
        mButtonNext->setEnabled (aSearchString.length());
        mButtonPrev->setEnabled (aSearchString.length());
        toggleWarning (!aSearchString.length());
        if (aSearchString.length())
            search (true, true);
        else
            mViewer->currentLogPage()->removeSelection();
    }

private:

    void search (bool aForward, bool aStartCurrent = false)
    {
        QTextBrowser *browser = mViewer->currentLogPage();
        if (!browser) return;

        int startPrg = 0, endPrg = 0;
        int startInd = 0, endInd = 0;
        if (browser->hasSelectedText())
            browser->getSelection (&startPrg, &startInd, &endPrg, &endInd);

        bool found = false;
        int increment = aForward ? 1 : -1;
        int border = aForward ? browser->paragraphs() : -1;
        int startFrom = aStartCurrent ? startInd : startInd + increment;
        int paragraph = startFrom < 0 ? startPrg + increment : startPrg;
        for (; paragraph != border; paragraph += increment)
        {
            QString text = browser->text (paragraph);
            int res = aForward ?
                text.find (mSearchString->text(), startFrom,
                           mCaseSensitive->isChecked()) :
                text.findRev (mSearchString->text(), startFrom,
                              mCaseSensitive->isChecked());
            if (res != -1)
            {
                found = true;
                browser->setSelection (paragraph, res, paragraph,
                                       res + mSearchString->text().length());
                /* ensures the selected word visible */
                int curPrg = 0, curInd = 0;
                browser->getCursorPosition (&curPrg, &curInd);
                QRect rect = browser->paragraphRect (curPrg);
                QString string = browser->text (curPrg);
                string.truncate (curInd);
                int x = rect.x() + browser->fontMetrics().width (string);
                int y = rect.y() + browser->pointSize() / 2;
                browser->setContentsPos (0, browser->contentsY());
                browser->ensureVisible (x, y, 40, 40);
                break;
            }
            startFrom = aForward ? 0 : -1;
        }

        toggleWarning (found);
        if (!found)
            browser->setSelection (startPrg, startInd, endPrg, endInd);
    }

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        switch (aEvent->type())
        {
            case QEvent::KeyPress:
            {
                QKeyEvent *e = static_cast<QKeyEvent*> (aEvent);

                /* handle the Enter keypress for mSearchString
                 * widget as a search next string action */
                if (aObject == mSearchString &&
                    (e->state() == 0 || e->state() & Keypad) &&
                    (e->key() == Key_Enter || e->key() == Key_Return))
                {
                    findNext();
                    return true;
                }
                /* handle other search next/previous shortcuts */
                else if (e->key() == Key_F3)
                {
                    if (e->state() == 0)
                        findNext();
                    else if (e->state() == ShiftButton)
                        findBack();
                    return true;
                }
                /* handle ctrl-f key combination as a shortcut to
                 * move to the search field */
                else if (e->state() == ControlButton && e->key() == Key_F)
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
                else if ((e->state() & ~ShiftButton) == 0 &&
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

    void showEvent (QShowEvent *aEvent)
    {
        QWidget::showEvent (aEvent);
        mSearchString->setFocus();
        mSearchString->selectAll();
    }

    void hideEvent (QHideEvent *aEvent)
    {
        if (focusData()->focusWidget()->parent() == this)
           focusNextPrevChild (true);
        QWidget::hideEvent (aEvent);
    }

    void toggleWarning (bool aHide)
    {
        mWarningSpacer->changeSize (aHide ? 0 : 16, 0, QSizePolicy::Fixed,
                                                       QSizePolicy::Minimum);
        mWarningIcon->setHidden (aHide);
        mWarningString->setHidden (aHide);
    }

    VBoxVMLogViewer *mViewer;
    QToolButton     *mButtonClose;
    QLabel          *mSearchName;
    QLineEdit       *mSearchString;
    QToolButton     *mButtonPrev;
    QToolButton     *mButtonNext;
    QCheckBox       *mCaseSensitive;
    QSpacerItem     *mWarningSpacer;
    QLabel          *mWarningIcon;
    QLabel          *mWarningString;
};


VBoxVMLogViewer::LogViewersMap VBoxVMLogViewer::mSelfArray = LogViewersMap();

void VBoxVMLogViewer::createLogViewer (CMachine &aMachine)
{
    if (mSelfArray.find (aMachine.GetName()) == mSelfArray.end())
    {
        /* creating new log viewer if there is no one existing */
        mSelfArray [aMachine.GetName()] = new VBoxVMLogViewer (0,
            "VBoxVMLogViewer", WType_TopLevel | WDestructiveClose);
        /* read new machine data for this log viewer */
        mSelfArray [aMachine.GetName()]->setup (aMachine);
    }

    VBoxVMLogViewer *viewer = mSelfArray [aMachine.GetName()];
    viewer->show();
    viewer->setWindowState (viewer->windowState() & ~WindowMinimized);
    viewer->setActiveWindow();
}


void VBoxVMLogViewer::init()
{
    /* prepare dialog to first run */
    mFirstRun = true;

    /* dialog initially is not polished */
    mIsPolished = false;

    /* search the default button */
    mDefaultButton = searchDefaultButton();
    topLevelWidget()->installEventFilter (this);

    /* setup a dialog icon */
    setIcon (QPixmap::fromMimeSource ("show_logs_16px.png"));

    /* statusbar initially disabled */
    statusBar()->setHidden (true);

    /* setup size grip */
    mSizeGrip = new QSizeGrip (centralWidget(), "mSizeGrip");
    mSizeGrip->resize (mSizeGrip->sizeHint());
    mSizeGrip->stackUnder (mCloseButton);

    /* logs list creation */
    mLogList = new QTabWidget (mLogsFrame, "mLogList");
    QVBoxLayout *logsFrameLayout = new QVBoxLayout (mLogsFrame);
    logsFrameLayout->addWidget (mLogList);

    /* search panel creation */
    mSearchPanel = new VBoxLogSearchPanel (mLogsFrame, this,
                                           "VBoxLogSearchPanel");
    logsFrameLayout->addWidget (mSearchPanel);
    mSearchPanel->hide();

    /* fix the tab order to ensure the dialog keys are always the last */
    setTabOrder (mSearchPanel->focusProxy(), mHelpButton);
    setTabOrder (mHelpButton, mFindButton);
    setTabOrder (mFindButton, mSaveButton);
    setTabOrder (mSaveButton, mRefreshButton);
    setTabOrder (mRefreshButton, mCloseButton);
    setTabOrder (mCloseButton, mLogList);

    /* make the [Save] button focused by default */
    mSaveButton->setFocus();

    /* applying language settings */
    languageChangeImp();
}


void VBoxVMLogViewer::destroy()
{
    mSelfArray.erase (mMachine.GetName());
}


void VBoxVMLogViewer::setup (CMachine &aMachine)
{
    /* saving related machine */
    mMachine = aMachine;

    /* reading log files */
    refresh();

    /* loading language constants */
    languageChangeImp();
}


const CMachine& VBoxVMLogViewer::machine()
{
    return mMachine;
}


void VBoxVMLogViewer::languageChangeImp()
{
    /* setup a dialog caption */
    if (!mMachine.isNull())
        setCaption (tr ("%1 - VirtualBox Log Viewer").arg (mMachine.GetName()));
    /* translate a search panel */
    if (mSearchPanel)
        mSearchPanel->languageChange();
}


QPushButton* VBoxVMLogViewer::searchDefaultButton()
{
    /* this mechanism is used for searching the default dialog button
     * and similar the same mechanism in Qt::QDialog inner source */
    QPushButton *button = 0;
    QObjectList *list = queryList ("QPushButton");
    QObjectListIt it (*list);
    while ((button = (QPushButton*)it.current()) && !button->isDefault())
        ++ it;
    return button;
}


bool VBoxVMLogViewer::eventFilter (QObject *aObject, QEvent *aEvent)
{
    switch (aEvent->type())
    {
        /* auto-default button focus-in processor used to move the "default"
         * button property into the currently focused button */
        case QEvent::FocusIn:
        {
            if (aObject->inherits ("QPushButton") &&
                aObject->parent() == centralWidget())
            {
                ((QPushButton*)aObject)->setDefault (aObject != mDefaultButton);
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject == mDefaultButton);
            }
            break;
        }
        /* auto-default button focus-out processor used to remove the "default"
         * button property from the previously focused button */
        case QEvent::FocusOut:
        {
            if (aObject->inherits ("QPushButton") &&
                aObject->parent() == centralWidget())
            {
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject != mDefaultButton);
                ((QPushButton*)aObject)->setDefault (aObject == mDefaultButton);
            }
            break;
        }
        default:
            break;
    }
    return QMainWindow::eventFilter (aObject, aEvent);
}


bool VBoxVMLogViewer::event (QEvent *aEvent)
{
    bool result = QMainWindow::event (aEvent);
    switch (aEvent->type())
    {
        case QEvent::LanguageChange:
        {
            languageChangeImp();
            break;
        }
        default:
            break;
    }
    return result;
}


void VBoxVMLogViewer::keyPressEvent (QKeyEvent *aEvent)
{
    if (aEvent->state() == 0 ||
        (aEvent->state() & Keypad && aEvent->key() == Key_Enter))
    {
        switch (aEvent->key())
        {
            /* processing the return keypress for the auto-default button */
            case Key_Enter:
            case Key_Return:
            {
                QPushButton *currentDefault = searchDefaultButton();
                if (currentDefault)
                    currentDefault->animateClick();
                break;
            }
            /* processing the escape keypress as the close dialog action */
            case Key_Escape:
            {
                mCloseButton->animateClick();
                break;
            }
        }
    }
    else
        aEvent->ignore();
}


void VBoxVMLogViewer::showEvent (QShowEvent *aEvent)
{
    QMainWindow::showEvent (aEvent);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (mIsPolished)
        return;

    mIsPolished = true;

    VBoxGlobal::centerWidget (this, parentWidget());
}


void VBoxVMLogViewer::resizeEvent (QResizeEvent*)
{
    /* adjust the size-grip location for the current resize event */
    mSizeGrip->move (centralWidget()->rect().bottomRight() -
                     QPoint (mSizeGrip->rect().width() - 1,
                     mSizeGrip->rect().height() - 1));
}


void VBoxVMLogViewer::refresh()
{
    /* clearing old data if any */
    mLogFilesList.clear();
    mLogList->setEnabled (true);
    while (mLogList->count())
    {
        QWidget *logPage = mLogList->page (0);
        mLogList->removePage (logPage);
        delete logPage;
    }

    bool isAnyLogPresent = false;

    /* entering log files folder */
    QString logFilesPath = mMachine.GetLogFolder();
    QDir logFilesDir (logFilesPath);
    if (logFilesDir.exists())
    {
        /* reading log files folder */
        logFilesDir.setNameFilter ("*.log *.log.*");
        QStringList logList = logFilesDir.entryList (QDir::Files);
        if (!logList.empty()) isAnyLogPresent = true;
        for (QStringList::Iterator it = logList.begin(); it != logList.end(); ++it)
            loadLogFile (logFilesDir.filePath (*it));
    }

    /* create an empty log page if there are no logs at all */
    if (!isAnyLogPresent)
    {
        QTextBrowser *dummyLog = createLogPage ("VBox.log");
        dummyLog->setTextFormat (Qt::RichText);
        dummyLog->setWordWrap (QTextEdit::WidgetWidth);
        dummyLog->setText (tr ("<p>No log files found. Press the <b>Refresh</b> "
            "button to rescan the log folder <nobr><b>%1</b></nobr>.</p>")
            .arg (logFilesPath));
        /* we don't want it to remain white */
        dummyLog->setPaper (backgroundBrush());
    }

    /* restore previous tab-widget margin which was reseted when
     * the tab widget's children was removed */
    mLogList->setMargin (10);

    /* show the first tab widget's page after the refresh */
    mLogList->showPage (mLogList->page(0));

    /* enable/disable save button & tab widget according log presence */
    mFindButton->setEnabled (isAnyLogPresent);
    mSaveButton->setEnabled (isAnyLogPresent);
    mLogList->setEnabled (isAnyLogPresent);

    if (mFirstRun)
    {
        /* resize the whole log-viewer to fit 80 symbols in text-browser for
         * the first time started */
        QTextBrowser *firstPage = static_cast <QTextBrowser *> (mLogList->page(0));
        int fullWidth = firstPage->fontMetrics().width (QChar ('x')) * 80 +
                        firstPage->verticalScrollBar()->width() +
                        firstPage->frameWidth() * 2 +
                        5 + 4 /* left text margin + QTabWidget frame width */ +
                        mLogList->margin() * 2 +
                        centralWidget()->layout()->margin() * 2;
        resize (fullWidth, height());
        mFirstRun = false;
    }
}


void VBoxVMLogViewer::loadLogFile (const QString &aFileName)
{
    /* prepare log file */
    QFile logFile (aFileName);
    if (!logFile.exists() || !logFile.open (IO_ReadOnly))
        return;

    /* read log file and write it into the log page */
    QTextBrowser *logViewer = createLogPage (QFileInfo (aFileName).fileName());
    logViewer->setText (logFile.readAll());

    mLogFilesList << aFileName;
}


QTextBrowser* VBoxVMLogViewer::createLogPage (const QString &aName)
{
    QTextBrowser *logViewer = new QTextBrowser();
    logViewer->setTextFormat (Qt::PlainText);
    QFont font = logViewer->currentFont();
    font.setFamily ("Courier New,courier");
    logViewer->setFont (font);
    logViewer->setWordWrap (QTextEdit::NoWrap);
    logViewer->setVScrollBarMode (QScrollView::AlwaysOn);
    mLogList->addTab (logViewer, aName);
    return logViewer;
}


QTextBrowser* VBoxVMLogViewer::currentLogPage()
{
    return mLogList->isEnabled() ?
        static_cast<QTextBrowser*> (mLogList->currentPage()) : 0;
}


void VBoxVMLogViewer::save()
{
    /* prepare "save as" dialog */
    QFileInfo fileInfo (mLogFilesList [mLogList->currentPageIndex()]);
    QDateTime dtInfo = fileInfo.lastModified();
    QString dtString = dtInfo.toString ("yyyy-MM-dd-hh-mm-ss");
    QString defaultFileName = QString ("%1-%2.log")
        .arg (mMachine.GetName()).arg (dtString);
    QString defaultFullName = QDir::convertSeparators (QDir::home().absPath() +
                                                       "/" + defaultFileName);

    QString newFileName = QFileDialog::getSaveFileName (defaultFullName,
        QString::null, this, "SaveLogAsDialog", tr ("Save VirtualBox Log As"));

    /* save new log into the file */
    if (!newFileName.isEmpty())
    {
        /* reread log data */
        QFile oldFile (mLogFilesList [mLogList->currentPageIndex()]);
        QFile newFile (newFileName);
        if (!oldFile.open (IO_ReadOnly) || !newFile.open (IO_WriteOnly))
            return;

        /* save log data into the new file */
        newFile.writeBlock (oldFile.readAll());
    }
}

void VBoxVMLogViewer::search()
{
    mSearchPanel->isHidden() ? mSearchPanel->show() : mSearchPanel->hide();
}

#include "VBoxVMLogViewer.ui.moc"

