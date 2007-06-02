/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Virtual Log Viewer" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


VBoxVMLogViewer *VBoxVMLogViewer::mSelf = 0;
void VBoxVMLogViewer::createLogViewer (CMachine &aMachine)
{
    if (!mSelf)
    {
        /* creating new log viewer if there is no one existing */
        mSelf = new VBoxVMLogViewer (0, "VBoxVMLogViewer",
                                     WType_TopLevel | WDestructiveClose);
    }

    if (mSelf->machine() != aMachine)
    {
        /* re-read new machine data if the machine was changed or
         * the log-viewer is opened for the first time */
        mSelf->setup (aMachine);
    }

    mSelf->show();
    mSelf->setWindowState (mSelf->windowState() & ~WindowMinimized);
    mSelf->setActiveWindow();
}


void VBoxVMLogViewer::init()
{
    /* prepare dialog to first run */
    mFirstRun = true;

    /* dialog initially is not polished */
    mIsPolished = false;

    /* search the default button */
    mDefaultButton = searchDefaultButton();
    qApp->installEventFilter (this);

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

    /* applying language settings */
    languageChangeImp();
}


void VBoxVMLogViewer::destroy()
{
    mSelf = 0;
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
                close();
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


void VBoxVMLogViewer::save()
{
    /* prepare "save as" dialog */
    QDate fullDate = QDate::currentDate();
    QTime fullTime = QTime::currentTime();
    QString date = fullDate.toString ("yyyy-MM-dd");
    QString time = fullTime.toString ("hh-mm-ss");
    QString defaultFileName = QString ("%1-%2-%3.log")
        .arg (mMachine.GetName()).arg (date).arg (time);
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

