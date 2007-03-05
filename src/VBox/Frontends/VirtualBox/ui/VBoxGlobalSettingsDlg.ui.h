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


class USBListItem : public QCheckListItem
{
public:

    USBListItem (QListView *aParent, QListViewItem *aAfter)
        : QCheckListItem (aParent, aAfter, QString::null, CheckBox)
        , mId (-1) {}

    int mId;
};
enum { lvUSBFilters_Name = 0 };


void VBoxGlobalSettingsDlg::init()
{
    polished = false;

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
                                      6 /* seems that RichText adds some margin */ +
                                      whatsThisLabel->fontMetrics().lineSpacing() * 3);
    whatsThisLabel->setMinimumWidth (whatsThisLabel->frameWidth() * 2 +
                                     6 /* seems that RichText adds some margin */ +
                                     whatsThisLabel->fontMetrics().width ('m') * 40);

    /*
     *  create and layout non-standard widgets
     *  ----------------------------------------------------------------------
     */

    hkeHostKey = new QIHotKeyEdit (grbKeyboard, "hkeHostKey");
    hkeHostKey->setSizePolicy (QSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed));
    QWhatsThis::add (hkeHostKey,
        tr ("Displays the key used as a Host Key in the VM window. Activate the "
            "entry field and press a new Host Key. Note that alphanumeric, "
            "cursor movement and editing keys cannot be used as a Host Key."));
    layoutHostKey->addWidget (hkeHostKey);
    txHostKey->setBuddy (hkeHostKey);
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

    /* USB page */

    lvUSBFilters->header()->hide();
    /* disable sorting */
    lvUSBFilters->setSorting (-1);
    /* disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, lvUSBFilters);
    wstUSBFilters = new QWidgetStack (grbUSBFilters, "wstUSBFilters");
    grbUSBFiltersLayout->addWidget (wstUSBFilters);
    /* create a default (disabled) filter settings widget at index 0 */
    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->setup (VBoxUSBFilterSettings::HostType);
    wstUSBFilters->addWidget (settings, 0);
    lvUSBFilters_currentChanged (NULL);
    /* setup toolbutton icons */
    tbAddUSBFilter->setIconSet (VBoxGlobal::iconSet ("usb_new_16px.png",
                                                     "usb_new_disabled_16px.png"));
    tbAddUSBFilterFrom->setIconSet (VBoxGlobal::iconSet ("usb_add_16px.png",
                                                         "usb_add_disabled_16px.png"));
    tbRemoveUSBFilter->setIconSet (VBoxGlobal::iconSet ("usb_remove_16px.png",
                                                        "usb_remove_disabled_16px.png"));
    tbUSBFilterUp->setIconSet (VBoxGlobal::iconSet ("usb_moveup_16px.png",
                                                    "usb_moveup_disabled_16px.png"));
    tbUSBFilterDown->setIconSet (VBoxGlobal::iconSet ("usb_movedown_16px.png",
                                                      "usb_movedown_disabled_16px.png"));
    /* create menu of existing usb-devices */
    usbDevicesMenu = new VBoxUSBMenu (this);
    connect (usbDevicesMenu, SIGNAL(activated(int)), this, SLOT(menuAddUSBFilterFrom_activated(int)));
    mLastUSBFilterNum = 0;
    mUSBFilterListModified = false;

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

void VBoxGlobalSettingsDlg::showEvent (QShowEvent *e)
{
    QDialog::showEvent (e);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (polished)
        return;

    polished = true;

    /* resize to the miminum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
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

    /* usb filters page */

    /// @todo currently, we always disable USB UI on XPCOM-based hosts because
    /// QueryInterface on CUSBDeviceFilter doesn't return CHostUSBDeviceFilter
    /// for host filters (most likely, our XPCOM/IPC/DCONNECT bug).

#ifdef Q_OS_WIN32
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();
    if (coll.isNull())
    {
#endif
        /* disable the USB host filters category if the USB is
         * not available (i.e. in VirtualBox OSE) */

        QListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
        Assert (usbItem);
        usbItem->setVisible (false);

        /* disable validators if any */
        pageUSB->setEnabled (false);
        
#ifdef Q_OS_WIN32
        /* Show an error message (if there is any).
         * This message box may be suppressed if the user wishes so. */
        vboxProblem().cannotAccessUSB (host);
    }
    else
    {
        CHostUSBDeviceFilterEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostUSBDeviceFilter hostFilter = en.GetNext();
            CUSBDeviceFilter filter = CUnknown (hostFilter);
            addUSBFilter (filter, false);
        }
        lvUSBFilters->setCurrentItem (lvUSBFilters->firstChild());
        lvUSBFilters_currentChanged (lvUSBFilters->firstChild());
    }
#endif
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

    /* usb filter page */

    /*
     *  first, remove all old filters (only if the list is changed,
     *  not only individual properties of filters)
     */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilter removedFilter;
    if (mUSBFilterListModified)
        for (ulong count = host.GetUSBDeviceFilters().GetCount(); count; -- count)
            host.RemoveUSBDeviceFilter (0, removedFilter);

    /* then add all new filters */
    for (QListViewItem *item = lvUSBFilters->firstChild(); item;
         item = item->nextSibling())
    {
        USBListItem *uli = static_cast <USBListItem *> (item);
        VBoxUSBFilterSettings *settings =
            static_cast <VBoxUSBFilterSettings *>
                (wstUSBFilters->widget (uli->mId));
        Assert (settings);

        COMResult res = settings->putBackToFilter();
        if (!res.isOk())
            return;

        CUSBDeviceFilter filter = settings->filter();
        filter.SetActive (uli->isOn());

        CHostUSBDeviceFilter insertedFilter = CUnknown (filter);
        if (mUSBFilterListModified)
            host.InsertUSBDeviceFilter (host.GetUSBDeviceFilters().GetCount(),
                                        insertedFilter);
    }
    mUSBFilterListModified = false;
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

// USB Filter stuff
////////////////////////////////////////////////////////////////////////////////

void VBoxGlobalSettingsDlg::addUSBFilter (const CUSBDeviceFilter &aFilter,
                                          bool aIsNew)
{
    QListViewItem *currentItem = aIsNew
        ? lvUSBFilters->currentItem()
        : lvUSBFilters->lastItem();

    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->setup (VBoxUSBFilterSettings::HostType);
    settings->getFromFilter (aFilter);

    USBListItem *item = new USBListItem (lvUSBFilters, currentItem);
    item->setOn (aFilter.GetActive());
    item->setText (lvUSBFilters_Name, aFilter.GetName());

    item->mId = wstUSBFilters->addWidget (settings);

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (settings->focusProxy(), buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    if (aIsNew)
    {
        lvUSBFilters->setSelected (item, true);
        lvUSBFilters_currentChanged (item);
        settings->leUSBFilterName->setFocus();
    }

    connect (settings->leUSBFilterName, SIGNAL (textChanged (const QString &)),
             this, SLOT (lvUSBFilters_setCurrentText (const QString &)));

    /* setup validation */

    QIWidgetValidator *wval = new QIWidgetValidator (settings, settings);
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));

    wval->revalidate();
}

void VBoxGlobalSettingsDlg::lvUSBFilters_currentChanged (QListViewItem *item)
{
    if (item && lvUSBFilters->selectedItem() != item)
        lvUSBFilters->setSelected (item, true);

    tbRemoveUSBFilter->setEnabled (!!item);

    tbUSBFilterUp->setEnabled (!!item && item->itemAbove());
    tbUSBFilterDown->setEnabled (!!item && item->itemBelow());

    if (item)
    {
        USBListItem *uli = static_cast <USBListItem *> (item);
        wstUSBFilters->raiseWidget (uli->mId);
    }
    else
    {
        /* raise the disabled widget */
        wstUSBFilters->raiseWidget (0);
    }
}

void VBoxGlobalSettingsDlg::lvUSBFilters_setCurrentText (const QString &aText)
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    item->setText (lvUSBFilters_Name, aText);
}

void VBoxGlobalSettingsDlg::tbAddUSBFilter_clicked()
{
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilter hostFilter;
    host.CreateUSBDeviceFilter (tr ("New Filter %1", "usb")
                                    .arg (++ mLastUSBFilterNum), hostFilter);
    hostFilter.SetAction (CEnums::USBDeviceFilterHold);

    CUSBDeviceFilter filter = CUnknown (hostFilter);
    filter.SetActive (true);
    addUSBFilter (filter, true);

    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::tbAddUSBFilterFrom_clicked()
{
    usbDevicesMenu->exec (QCursor::pos());
}

void VBoxGlobalSettingsDlg::menuAddUSBFilterFrom_activated (int aIndex)
{
    CUSBDevice usb = usbDevicesMenu->getUSB (aIndex);

    // if null then some other item but a USB device is selected
    if (usb.isNull())
        return;

    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilter hostFilter;
    host.CreateUSBDeviceFilter (vboxGlobal().details (usb), hostFilter);
    hostFilter.SetAction (CEnums::USBDeviceFilterHold);

    CUSBDeviceFilter filter = CUnknown (hostFilter);
    filter.SetVendorId (QString().sprintf ("%04hX", usb.GetVendorId()));
    filter.SetProductId (QString().sprintf ("%04hX", usb.GetProductId()));
    filter.SetRevision (QString().sprintf ("%04hX", usb.GetRevision()));
    filter.SetPort (QString().sprintf ("%04hX", usb.GetPort()));
    filter.SetManufacturer (usb.GetManufacturer());
    filter.SetProduct (usb.GetProduct());
    filter.SetSerialNumber (usb.GetSerialNumber());
    filter.SetRemote (usb.GetRemote() ? "yes" : "no");
    filter.SetActive (true);
    addUSBFilter (filter, true);

    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::tbRemoveUSBFilter_clicked()
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    USBListItem *uli = static_cast <USBListItem *> (item);
    QWidget *settings = wstUSBFilters->widget (uli->mId);
    Assert (settings);
    wstUSBFilters->removeWidget (settings);
    delete settings;

    delete item;

    lvUSBFilters->setSelected (lvUSBFilters->currentItem(), true);
    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::tbUSBFilterUp_clicked()
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    QListViewItem *itemAbove = item->itemAbove();
    Assert (itemAbove);
    itemAbove = itemAbove->itemAbove();

    if (!itemAbove)
        item->itemAbove()->moveItem (item);
    else
        item->moveItem (itemAbove);

    lvUSBFilters_currentChanged (item);
    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::tbUSBFilterDown_clicked()
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    QListViewItem *itemBelow = item->itemBelow();
    Assert (itemBelow);

    item->moveItem (itemBelow);

    lvUSBFilters_currentChanged (item);
    mUSBFilterListModified = true;
}
