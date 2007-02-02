/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Global settings" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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


/**
 *  Returns the path to the item in the form of 'grandparent > parent > item'
 *  using the text of the first column of every item.
 */
static QString path (QListViewItem *li)
{
    static QString sep = ": ";
    QString p;
    QListViewItem *cur = li;
    while (cur)
    {
        if (!p.isNull())
            p = sep + p;
        p = cur->text (0).simplifyWhiteSpace() + p;
        cur = cur->parent();
    }
    return p;
}


enum
{
    // listView column numbers
    listView_Category = 0,
    listView_Id = 1,
    listView_Link = 2,
};

void VBoxGlobalSettingsDlg::init()
{
    setCaption (tr ("VirtualBox Global Settings"));
    setIcon (QPixmap::fromMimeSource ("global_settings_16px.png"));

    /*  all pages are initially valid */
    valid = true;
    buttonOk->setEnabled (true);
    warningSpacer->changeSize (0, 0, QSizePolicy::Expanding);
    warningLabel->setHidden (true);
    warningPixmap->setHidden (true);

    /*  disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, listView);
    /*  hide the header and internal columns */
    listView->header()->hide();
    listView->setColumnWidthMode (listView_Id, QListView::Manual);
    listView->setColumnWidthMode (listView_Link, QListView::Manual);
    listView->hideColumn (listView_Id);
    listView->hideColumn (listView_Link);
    /*  sort by the id column (to have pages in the desired order) */
    listView->setSorting (listView_Id);
    listView->sort();
    /*  disable further sorting (important for network adapters) */
    listView->setSorting (-1);
    /*  set the first item selected */
    listView->setSelected (listView->firstChild(), true);
    listView_currentChanged (listView->firstChild());

    warningPixmap->setMaximumSize( 16, 16 );
    warningPixmap->setPixmap( QMessageBox::standardIcon( QMessageBox::Warning ) );

    /* page title font is derived from the system font */
    QFont f = font();
    f.setBold( true );
    f.setPointSize( f.pointSize() + 2 );
    titleLabel->setFont( f );

    /* setup the what's this label */
    QApplication::setGlobalMouseTracking (true);
    qApp->installEventFilter (this);
    whatsThisTimer = new QTimer (this);
    connect (whatsThisTimer, SIGNAL (timeout()), this, SLOT (updateWhatsThis()));
    whatsThisCandidate = NULL;
    whatsThisLabel->setTextFormat (Qt::RichText);
    whatsThisLabel->setMinimumHeight (whatsThisLabel->frameWidth() * 2 +
                                      4 /* seems that RichText adds some margin */ +
                                      whatsThisLabel->fontMetrics().lineSpacing() * 3);
    
    /*
     *  create and layout non-standard widgets
     *  ----------------------------------------------------------------------
     */

    hkeHostKey = new QIHotKeyEdit( pageKeyboard, "hkeHostKey" );
    hkeHostKey->setSizePolicy(
        QSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed ));
    QWhatsThis::add (hkeHostKey,
        tr ("Displays the key used as a Host Key in the VM window. Activate the "
            "entry field and press a new Host Key. Note that alphanumeric, "
            "cursor movement and editing keys cannot be used as a Host Key."));
    layoutHostKey->addWidget( hkeHostKey );
    txHostKey->setBuddy( hkeHostKey );
    setTabOrder (listView, hkeHostKey);

    /*
     *  setup connections and set validation for pages
     *  ----------------------------------------------------------------------
     */

    /* General page */

/// @todo (dmik) remove
//    leVDIFolder->setValidator (new QRegExpValidator (QRegExp (".+"), this));
//    leMachineFolder->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    wvalGeneral = new QIWidgetValidator (pageGeneral, this);
    connect (wvalGeneral, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk( const QIWidgetValidator *)));

    /* Keyboard page */

    wvalKeyboard = new QIWidgetValidator( pageKeyboard, this );
    connect (wvalKeyboard, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk( const QIWidgetValidator *)));

    /*
     *  set initial values
     *  ----------------------------------------------------------------------
     */

    /* General page */

    /* keyboard page */

    /*
     *  update the Ok button state for pages with validation
     *  (validityChanged() connected to enableNext() will do the job)
     */
    wvalGeneral->revalidate();
    wvalKeyboard->revalidate();
}

bool VBoxGlobalSettingsDlg::eventFilter (QObject *object, QEvent *event)
{
    if (!object->isWidgetType())
        return QDialog::eventFilter (object, event);
    
    QWidget *widget = static_cast <QWidget *> (object);
    if (widget->topLevelWidget() != this)
        return QDialog::eventFilter (object, event);

    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::Leave:
        {
            if (event->type() == QEvent::Enter)
                whatsThisCandidate = widget;
            else
                whatsThisCandidate = NULL;
            whatsThisTimer->start (100, true /* sshot */);
            break;
        }
        case QEvent::FocusIn:
        {
            updateWhatsThis (true /* gotFocus */);
            break;
        }
        default:
            break;
    }

    return QDialog::eventFilter (object, event);
}

void VBoxGlobalSettingsDlg::listView_currentChanged (QListViewItem *item)
{
    Assert (item);
    int id = item->text (1).toInt();
    Assert (id >= 0);
    titleLabel->setText (::path (item));
    widgetStack->raiseWidget (id);
}

void VBoxGlobalSettingsDlg::enableOk (const QIWidgetValidator *wval)
{
    Q_UNUSED (wval);

    /* detect the overall validity */
    bool newValid = true;
    {
        QObjectList *l = this->queryList ("QIWidgetValidator");
        QObjectListIt it (*l);
        QObject *obj;
        while ((obj = it.current()) != 0)
        {
            newValid &= ((QIWidgetValidator *) obj)->isValid();
            ++it;
        }
        delete l;
    }

    if (valid != newValid)
    {
        valid = newValid;
        buttonOk->setEnabled (valid);
        if (valid)
            warningSpacer->changeSize (0, 0, QSizePolicy::Expanding);
        else
            warningSpacer->changeSize (0, 0);
        warningLabel->setHidden (valid);
        warningPixmap->setHidden (valid);
    }
}

void VBoxGlobalSettingsDlg::revalidate (QIWidgetValidator * /*wval*/)
{
    /* do individual validations for pages */

    /* currently nothing */
}

/**
 *  Reads global settings from the given VMGlobalSettings instance
 *  and from the given CSystemProperties object.
 */
void VBoxGlobalSettingsDlg::getFrom (const CSystemProperties &props,
                                     const VMGlobalSettings &gs)
{
    /* default folders */

    leVDIFolder->setText (props.GetDefaultVDIFolder());
    leMachineFolder->setText (props.GetDefaultMachineFolder());

    /* proprietary GUI settings */

    hkeHostKey->setKey (gs.hostKey() );
    chbAutoCapture->setChecked (gs.autoCapture());

//    wvalXXXX->revalidate();
}

/**
 *  Writes global settings to the given VMGlobalSettings instance
 *  and to the given CSystemProperties object.
 */
void VBoxGlobalSettingsDlg::putBackTo (CSystemProperties &props,
                                       VMGlobalSettings &gs)
{
    /* default folders */

    if (leVDIFolder->isModified())
        props.SetDefaultVDIFolder (leVDIFolder->text());
    if (props.isOk() && leMachineFolder->isModified())
        props.SetDefaultMachineFolder (leMachineFolder->text());

    if (!props.isOk())
        return;

    /* proprietary GUI settings */

    gs.setHostKey (hkeHostKey->key());
    gs.setAutoCapture (chbAutoCapture->isChecked());
}

void VBoxGlobalSettingsDlg::updateWhatsThis (bool gotFocus /* = false */)
{
    QString text;

    QWidget *widget = NULL;
    if (!gotFocus)
    {
        if (whatsThisCandidate != NULL && whatsThisCandidate != this)
            widget = whatsThisCandidate;
    }
    else
    {
        widget = focusData()->focusWidget();
    }
    /* if the given widget lacks the whats'this text, look at its parent */
    while (widget && widget != this)
    {
        text = QWhatsThis::textFor (widget);
        if (!text.isEmpty())
            break;
        widget = widget->parentWidget();
    }

    if (text.isEmpty() && !warningString.isEmpty())
        text = warningString;
    if (text.isEmpty())
        text = QWhatsThis::textFor (this);

    whatsThisLabel->setText (text);
}

void VBoxGlobalSettingsDlg::setWarning (const QString &warning)
{
    warningString = warning;
    if (!warning.isEmpty())
        warningString = QString ("<font color=red>%1</font>").arg (warning);
    
    if (!warningString.isEmpty())
        whatsThisLabel->setText (warningString);
    else
        updateWhatsThis (true);
}

void VBoxGlobalSettingsDlg::tbResetFolder_clicked()
{
    QToolButton *tb = ::qt_cast <QToolButton *> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == tbResetVDIFolder) le = leVDIFolder;
    else if (tb == tbResetMachineFolder) le = leMachineFolder;
    Assert (le);

    /*
     *  do this instead of le->setText (QString::null) to cause
     *  isModified() return true
     */
    le->selectAll();
    le->del();
}

void VBoxGlobalSettingsDlg::tbSelectFolder_clicked()
{
    QToolButton *tb = ::qt_cast <QToolButton *> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == tbSelectVDIFolder) le = leVDIFolder;
    else if (tb == tbSelectMachineFolder) le = leMachineFolder;
    Assert (le);

    QString homeFolder = vboxGlobal().virtualBox().GetHomeFolder();

    QFileDialog dlg (homeFolder, QString::null, this);
    dlg.setMode (QFileDialog::DirectoryOnly);

    if (!le->text().isEmpty())
    {
        /* set the first parent directory that exists as the current */
#if 0 /** @todo fix this linux bustage properly */
        QFileInfo fld (QDir (homeFolder), le->text());
#else
        const QDir _dir (homeFolder);
        QFileInfo fld (_dir, le->text());
#endif
        do
        {
            QString dp = fld.dirPath (false);
            fld = QFileInfo (dp);
        }
        while (!fld.exists() && !QDir (fld.absFilePath()).isRoot());

        if (fld.exists())
            dlg.setDir (fld.absFilePath());
    }

    if (dlg.exec() == QDialog::Accepted)
    {
        QString folder = QDir::convertSeparators (dlg.selectedFile());
        /* remove trailing slash */
        folder.truncate (folder.length() - 1);

        /*
         *  do this instead of le->setText (folder) to cause
         *  isModified() return true
         */
        le->selectAll();
        le->insert (folder);
    }
}
