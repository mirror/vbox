/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM settings" dialog UI include (Qt Designer)
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
 *  Calculates a suitable page step size for the given max value.
 *  The returned size is so that there will be no more than 32 pages.
 *  The minimum returned page size is 4.
 */
static int calcPageStep (int aMax)
{
    /* reasonable max. number of page steps is 32 */
    uint page = ((uint) aMax + 31) / 32;
    /* make it a power of 2 */
    uint p = page, p2 = 0x1;
    while ((p >>= 1))
        p2 <<= 1;
    if (page != p2)
        p2 <<= 1;
    if (p2 < 4)
        p2 = 4;
    return (int) p2;
}

/**
 *  Simple QTableItem subclass to use QComboBox as the cell editor.
 *  This subclass (as opposed to QComboTableItem) allows to specify the
 *  EditType::WhenCurrent edit type of the cell (to let it look like a normal
 *  text cell when not in focus).
 *
 *  Additionally, this subclass supports unicity of a group of values
 *  among a group of ComboTableItem items that refer to the same list of
 *  unique values currently being used.
 */
class ComboTableItem : public QObject, public QTableItem
{
    Q_OBJECT

public:

    ComboTableItem (QTable *aTable, EditType aEditType,
                    const QStringList &aList, const QStringList &aUnique,
                    QStringList *aUniqueInUse)
        : QTableItem (aTable, aEditType)
        , mList (aList), mUnique (aUnique), mUniqueInUse (aUniqueInUse), mComboBoxSelector(0)
    {
        setReplaceable (FALSE);
    }

    // reimplemented QTableItem members
    QWidget *createEditor() const
    {
        mComboBoxSelector = new QComboBox (table()->viewport());
        QStringList list = mList;
        if (mUniqueInUse)
        {
            /* remove unique values currently in use */
            for (QStringList::Iterator it = mUniqueInUse->begin();
                 it != mUniqueInUse->end(); ++ it)
            {
                if (*it != text())
                    list.remove (*it);
            }
        }
        mComboBoxSelector->insertStringList (list);
        mComboBoxSelector->setCurrentText (text());
        QObject::connect (mComboBoxSelector,   SIGNAL (highlighted (const QString &)),
                          this,                SLOT   (doValueChanged (const QString &)));
        QObject::connect (mComboBoxSelector,   SIGNAL (activated (const QString &)),
                          this,                SLOT   (focusClearing ()));

        return mComboBoxSelector;
    }

    void setContentFromEditor (QWidget *aWidget)
    {
        if (aWidget->inherits ("QComboBox"))
        {
            QString text = ((QComboBox *) aWidget)->currentText();
            setText (text);
        }
        else
            QTableItem::setContentFromEditor (aWidget);
    }
    void setText (const QString &aText)
    {
        if (aText != text())
        {
            /* update the list of unique values currently in use */
            if (mUniqueInUse)
            {
                QStringList::Iterator it = mUniqueInUse->find (text());
                if (it != mUniqueInUse->end())
                    mUniqueInUse->remove (it);
                if (mUnique.contains (aText))
                    (*mUniqueInUse) += aText;
            }
            QTableItem::setText (aText);
        }
    }

    /*
     *  Function: rtti()
     *  Target:   required for runtime information about ComboTableItem class
     *            used for static_cast from QTableItem
     */
    int rtti() const { return 1001; }

    /*
     *  Function: getEditor()
     *  Target:   returns pointer to stored combo-box
     */
    QComboBox* getEditor() { return mComboBoxSelector; }

private slots:

    /*
     *  QTable doesn't call endEdit() when item's EditType is WhenCurrent and
     *  the table widget loses focus or gets destroyed (assuming the user will
     *  hit Enter if he wants to keep the value), so we need to do it ourselves
     */
    void doValueChanged (const QString &text) { setText (text); }

    /*
     *  Function: focusClearing()
     *  Target:   required for removing focus from combo-box
     */
    void focusClearing() { mComboBoxSelector->clearFocus(); }

private:

    const QStringList  mList;
    const QStringList  mUnique;
    QStringList*       mUniqueInUse;
    mutable QComboBox* mComboBoxSelector;
};


/// @todo (dmik) remove?
///**
// *  Returns the through position of the item in the list view.
// */
//static int pos (QListView *lv, QListViewItem *li)
//{
//    QListViewItemIterator it (lv);
//    int p = -1, c = 0;
//    while (it.current() && p < 0)
//    {
//        if (it.current() == li)
//            p = c;
//        ++ it;
//        ++ c;
//    }
//    return p;
//}

class USBListItem : public QCheckListItem
{
public:

    USBListItem (QListView *aParent, QListViewItem *aAfter)
        : QCheckListItem (aParent, aAfter, QString::null, CheckBox)
        , mId (-1) {}

    int mId;
};

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
    /* listView column numbers */
    listView_Category = 0,
    listView_Id = 1,
    listView_Link = 2,
    /* lvUSBFilters column numbers */
    lvUSBFilters_Name = 0,
};

void VBoxVMSettingsDlg::init()
{
    polished = false;

    setIcon (QPixmap::fromMimeSource ("settings_16px.png"));

    /* all pages are initially valid */
    valid = true;
    buttonOk->setEnabled( true );

    /* disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, listView);
    /* hide the header and internal columns */
    listView->header()->hide();
    listView->setColumnWidthMode (listView_Id, QListView::Manual);
    listView->setColumnWidthMode (listView_Link, QListView::Manual);
    listView->hideColumn (listView_Id);
    listView->hideColumn (listView_Link);
    /* sort by the id column (to have pages in the desired order) */
    listView->setSorting (listView_Id);
    listView->sort();
    /* disable further sorting (important for network adapters) */
    listView->setSorting (-1);
    /* set the first item selected */
    listView->setSelected (listView->firstChild(), true);
    listView_currentChanged (listView->firstChild());
    /* setup status bar icon */
    warningPixmap->setMaximumSize( 16, 16 );
    warningPixmap->setPixmap( QMessageBox::standardIcon( QMessageBox::Warning ) );

    /* page title font is derived from the system font */
    QFont f = font();
    f.setBold (true);
    f.setPointSize (f.pointSize() + 2);
    titleLabel->setFont (f);

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
     *  setup connections and set validation for pages
     *  ----------------------------------------------------------------------
     */

    /* General page */

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();

    const uint MinRAM = sysProps.GetMinGuestRAM();
    const uint MaxRAM = sysProps.GetMaxGuestRAM();
    const uint MinVRAM = sysProps.GetMinGuestVRAM();
    const uint MaxVRAM = sysProps.GetMaxGuestVRAM();

    leName->setValidator( new QRegExpValidator( QRegExp( ".+" ), this ) );

    leRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));
    leVRAM->setValidator (new QIntValidator (MinVRAM, MaxVRAM, this));

    wvalGeneral = new QIWidgetValidator( pageGeneral, this );
    connect (wvalGeneral, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT(enableOk (const QIWidgetValidator *)));

    tbSelectSavedStateFolder->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                               "select_file_dis_16px.png"));
    tbResetSavedStateFolder->setIconSet (VBoxGlobal::iconSet ("delete_16px.png",
                                                              "delete_dis_16px.png"));

    /* HDD Images page */

    QWhatsThis::add (static_cast <QWidget *> (grbHDA->child ("qt_groupbox_checkbox")),
                     tr ("When checked, attaches the specified virtual hard disk to the "
                         "Master slot of the Primary IDE controller."));
    QWhatsThis::add (static_cast <QWidget *> (grbHDB->child ("qt_groupbox_checkbox")),
                     tr ("When checked, attaches the specified virtual hard disk to the "
                         "Slave slot of the Primary IDE controller."));
    QWhatsThis::add (static_cast <QWidget *> (grbHDD->child ("qt_groupbox_checkbox")),
                     tr ("When checked, attaches the specified virtual hard disk to the "
                         "Slave slot of the Secondary IDE controller."));
    cbHDA = new VBoxMediaComboBox (grbHDA, "cbHDA", VBoxDefs::HD);
    cbHDB = new VBoxMediaComboBox (grbHDB, "cbHDB", VBoxDefs::HD);
    cbHDD = new VBoxMediaComboBox (grbHDD, "cbHDD", VBoxDefs::HD);
    hdaLayout->insertWidget (0, cbHDA);
    hdbLayout->insertWidget (0, cbHDB);
    hddLayout->insertWidget (0, cbHDD);
    /* sometimes the weirdness of Qt just kills... */
    setTabOrder (static_cast <QWidget *> (grbHDA->child ("qt_groupbox_checkbox")),
                 cbHDA);
    setTabOrder (static_cast <QWidget *> (grbHDB->child ("qt_groupbox_checkbox")),
                 cbHDB);
    setTabOrder (static_cast <QWidget *> (grbHDD->child ("qt_groupbox_checkbox")),
                 cbHDD);

    QWhatsThis::add (cbHDB, tr ("Displays the virtual hard disk to attach to this IDE slot "
                                "and allows to quickly select a different hard disk."));
    QWhatsThis::add (cbHDD, tr ("Displays the virtual hard disk to attach to this IDE slot "
                                "and allows to quickly select a different hard disk."));
    QWhatsThis::add (cbHDA, tr ("Displays the virtual hard disk to attach to this IDE slot "
                                "and allows to quickly select a different hard disk."));
    QWhatsThis::add (cbHDB, tr ("Displays the virtual hard disk to attach to this IDE slot "
                                "and allows to quickly select a different hard disk."));
    QWhatsThis::add (cbHDD, tr ("Displays the virtual hard disk to attach to this IDE slot "
                                "and allows to quickly select a different hard disk."));

    wvalHDD = new QIWidgetValidator( pageHDD, this );
    connect (wvalHDD, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wvalHDD, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

    connect (grbHDA, SIGNAL (toggled (bool)), this, SLOT (hdaMediaChanged()));
    connect (grbHDB, SIGNAL (toggled (bool)), this, SLOT (hdbMediaChanged()));
    connect (grbHDD, SIGNAL (toggled (bool)), this, SLOT (hddMediaChanged()));
    connect (cbHDA, SIGNAL (activated (int)), this, SLOT (hdaMediaChanged()));
    connect (cbHDB, SIGNAL (activated (int)), this, SLOT (hdbMediaChanged()));
    connect (cbHDD, SIGNAL (activated (int)), this, SLOT (hddMediaChanged()));
    connect (tbHDA, SIGNAL (clicked()), this, SLOT (showImageManagerHDA()));
    connect (tbHDB, SIGNAL (clicked()), this, SLOT (showImageManagerHDB()));
    connect (tbHDD, SIGNAL (clicked()), this, SLOT (showImageManagerHDD()));

    /* setup iconsets -- qdesigner is not capable... */
    tbHDA->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                            "select_file_dis_16px.png"));
    tbHDB->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                            "select_file_dis_16px.png"));
    tbHDD->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                            "select_file_dis_16px.png"));

    /* CD/DVD-ROM Drive Page */

    QWhatsThis::add (static_cast <QWidget *> (bgDVD->child ("qt_groupbox_checkbox")),
                     tr ("When checked, mounts the specified media to the CD/DVD drive of the "
                         "virtual machine. Note that the CD/DVD drive is always connected to the "
                         "Secondary Master IDE controller of the machine."));
    cbISODVD = new VBoxMediaComboBox (bgDVD, "cbISODVD", VBoxDefs::CD);
    cdLayout->insertWidget(0, cbISODVD);
    QWhatsThis::add (cbISODVD, tr ("Displays the image file to mount to the virtual CD/DVD "
                                   "drive and allows to quickly select a different image."));

    wvalDVD = new QIWidgetValidator (pageDVD, this);
    connect (wvalDVD, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wvalDVD, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    connect (bgDVD, SIGNAL (toggled (bool)), this, SLOT (cdMediaChanged()));
    connect (rbHostDVD, SIGNAL (stateChanged (int)), wvalDVD, SLOT (revalidate()));
    connect (rbISODVD, SIGNAL (stateChanged (int)), wvalDVD, SLOT (revalidate()));
    connect (cbISODVD, SIGNAL (activated (int)), this, SLOT (cdMediaChanged()));
    connect (tbISODVD, SIGNAL (clicked()), this, SLOT (showImageManagerISODVD()));

    /* setup iconsets -- qdesigner is not capable... */
    tbISODVD->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                               "select_file_dis_16px.png"));

    /* Floppy Drive Page */

    QWhatsThis::add (static_cast <QWidget *> (bgFloppy->child ("qt_groupbox_checkbox")),
                     tr ("When checked, mounts the specified media to the Floppy drive of the "
                         "virtual machine."));
    cbISOFloppy = new VBoxMediaComboBox (bgFloppy, "cbISOFloppy", VBoxDefs::FD);
    fdLayout->insertWidget(0, cbISOFloppy);
    QWhatsThis::add (cbISOFloppy, tr ("Displays the image file to mount to the virtual Floppy "
                                      "drive and allows to quickly select a different image."));

    wvalFloppy = new QIWidgetValidator (pageFloppy, this);
    connect (wvalFloppy, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wvalFloppy, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    connect (bgFloppy, SIGNAL (toggled (bool)), this, SLOT (fdMediaChanged()));
    connect (rbHostFloppy, SIGNAL (stateChanged (int)), wvalFloppy, SLOT (revalidate()));
    connect (rbISOFloppy, SIGNAL (stateChanged (int)), wvalFloppy, SLOT (revalidate()));
    connect (cbISOFloppy, SIGNAL (activated (int)), this, SLOT (fdMediaChanged()));
    connect (tbISOFloppy, SIGNAL (clicked()), this, SLOT (showImageManagerISOFloppy()));

    /* setup iconsets -- qdesigner is not capable... */
    tbISOFloppy->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                  "select_file_dis_16px.png"));

    /* Audio Page */

    QWhatsThis::add (static_cast <QWidget *> (grbAudio->child ("qt_groupbox_checkbox")),
                     tr ("When checked, the virtual PCI audio card is plugged into the "
                         "virtual machine that uses the specified driver to communicate "
                         "to the host audio card."));

    /* Network Page */

    QVBoxLayout* pageNetworkLayout = new QVBoxLayout (pageNetwork, 0, 10, "pageNetworkLayout");
    tbwNetwork = new QTabWidget (pageNetwork, "tbwNetwork");
    pageNetworkLayout->addWidget (tbwNetwork);

    /* USB Page */

    lvUSBFilters->header()->hide();
    /* disable sorting */
    lvUSBFilters->setSorting (-1);
    /* disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, lvUSBFilters);
    /* create the widget stack for filter settings */
    /// @todo (r=dmik) having a separate settings widget for every USB filter
    //  is not that smart if there are lots of USB filters. The reason for
    //  stacking here is that the stacked widget is used to temporarily store
    //  data of the associated USB filter until the dialog window is accepted.
    //  If we remove stacking, we will have to create a structure to store
    //  editable data of all USB filters while the dialog is open.
    wstUSBFilters = new QWidgetStack (grbUSBFilters, "wstUSBFilters");
    grbUSBFiltersLayout->addWidget (wstUSBFilters);
    /* create a default (disabled) filter settings widget at index 0 */
    wstUSBFilters->addWidget (new VBoxUSBFilterSettings (wstUSBFilters), 0);
    lvUSBFilters_currentChanged (NULL);

    /* setup iconsets -- qdesigner is not capable... */
    tbAddUSBFilter->setIconSet (VBoxGlobal::iconSet ("usb_16px.png",
                                                     "usb_disabled_16px.png"));
    tbAddUSBFilterFrom->setIconSet (VBoxGlobal::iconSet ("usb_add_16px.png",
                                                         "usb_add_disabled_16px.png"));
    tbRemoveUSBFilter->setIconSet (VBoxGlobal::iconSet ("usb_remove_16px.png",
                                                        "usb_remove_disabled_16px.png"));
    tbUSBFilterUp->setIconSet (VBoxGlobal::iconSet ("usb_moveup_16px.png",
                                                    "usb_moveup_disabled_16px.png"));
    tbUSBFilterDown->setIconSet (VBoxGlobal::iconSet ("usb_movedown_16px.png",
                                                      "usb_movedown_disabled_16px.png"));
    usbDevicesMenu = new QPopupMenu (this, "usbDevicesMenu");
    connect (usbDevicesMenu, SIGNAL(activated(int)), this, SLOT(menuAddUSBFilterFrom_activated(int)));
    connect (usbDevicesMenu, SIGNAL(highlighted(int)), this, SLOT(menuAddUSBFilterFrom_highlighted(int)));
    mLastUSBFilterNum = 0;
    mUSBFilterListModified = false;

    /* VRDP Page */

    QWhatsThis::add (static_cast <QWidget *> (grbVRDP->child ("qt_groupbox_checkbox")),
                     tr ("When checked, the VM will act as a Remote Desktop "
                         "Protocol (RDP) server, allowing remote clients to connect "
                         "and operate the VM (when it is running) "
                         "using a standard RDP client."));

    ULONG maxPort = 65535;
    leVRDPPort->setValidator (new QIntValidator (0, maxPort, this));
    leVRDPTimeout->setValidator (new QIntValidator (0, maxPort, this));
    wvalVRDP = new QIWidgetValidator (pageVRDP, this);
    connect (wvalVRDP, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wvalVRDP, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    connect (grbVRDP, SIGNAL (toggled (bool)), wvalFloppy, SLOT (revalidate()));
    connect (leVRDPPort, SIGNAL (textChanged (const QString&)), wvalFloppy, SLOT (revalidate()));
    connect (leVRDPTimeout, SIGNAL (textChanged (const QString&)), wvalFloppy, SLOT (revalidate()));

    /*
     *  set initial values
     *  ----------------------------------------------------------------------
     */

    /* General page */

    cbOS->insertStringList (vboxGlobal().vmGuestOSTypeDescriptions());

    slRAM->setPageStep (calcPageStep (MaxRAM));
    slRAM->setLineStep (slRAM->pageStep() / 4);
    slRAM->setTickInterval (slRAM->pageStep());
    /* setup the scale so that ticks are at page step boundaries */
    slRAM->setMinValue ((MinRAM / slRAM->pageStep()) * slRAM->pageStep());
    slRAM->setMaxValue (MaxRAM);
    txRAMMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MinRAM));
    txRAMMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MaxRAM));
    /* limit min/max. size of QLineEdit */
    leRAM->setMaximumSize (leRAM->fontMetrics().width ("99999")
                           + leRAM->frameWidth() * 2,
                           leRAM->minimumSizeHint().height());
    leRAM->setMinimumSize (leRAM->maximumSize());
    /* ensure leRAM value and validation is updated */
    slRAM_valueChanged (slRAM->value());

    slVRAM->setPageStep (calcPageStep (MaxVRAM));
    slVRAM->setLineStep (slVRAM->pageStep() / 4);
    slVRAM->setTickInterval (slVRAM->pageStep());
    /* setup the scale so that ticks are at page step boundaries */
    slVRAM->setMinValue ((MinVRAM / slVRAM->pageStep()) * slVRAM->pageStep());
    slVRAM->setMaxValue (MaxVRAM);
    txVRAMMin->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MinVRAM));
    txVRAMMax->setText (tr ("<qt>%1&nbsp;MB</qt>").arg (MaxVRAM));
    /* limit min/max. size of QLineEdit */
    leVRAM->setMaximumSize (leVRAM->fontMetrics().width ("99999")
                            + leVRAM->frameWidth() * 2,
                            leVRAM->minimumSizeHint().height());
    leVRAM->setMinimumSize (leVRAM->maximumSize());
    /* ensure leVRAM value and validation is updated */
    slVRAM_valueChanged (slVRAM->value());

    tblBootOrder->horizontalHeader()->hide();
    tblBootOrder->setTopMargin (0);
    tblBootOrder->verticalHeader()->hide();
    tblBootOrder->setLeftMargin (0);
    tblBootOrder->setNumCols (1);
    tblBootOrder->setNumRows (sysProps.GetMaxBootPosition());
    {
        QStringList list = vboxGlobal().deviceTypeStrings();
        QStringList unique;
        unique
            << vboxGlobal().toString (CEnums::FloppyDevice)
            << vboxGlobal().toString (CEnums::DVDDevice)
            << vboxGlobal().toString (CEnums::HardDiskDevice)
            << vboxGlobal().toString (CEnums::NetworkDevice);
        for (int i = 0; i < tblBootOrder->numRows(); i++)
        {
            ComboTableItem *item = new ComboTableItem (
                tblBootOrder, QTableItem::OnTyping,
                list, unique, &bootDevicesInUse);
            tblBootOrder->setItem (i, 0, item);
        }
    }
    connect (tblBootOrder, SIGNAL (clicked(int, int, int, const QPoint&)),
             this,  SLOT (bootItemActivate(int, int, int, const QPoint&)));

    tblBootOrder->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
    tblBootOrder->setMinimumHeight (tblBootOrder->rowHeight(0) * 4 +
                                    tblBootOrder->frameWidth() * 2);
    tblBootOrder->setColumnStretchable (0, true);
    tblBootOrder->verticalHeader()->setResizeEnabled (false);
    tblBootOrder->verticalHeader()->setClickEnabled (false);
//    tblBootOrder->setFocusStyle (QTable::FollowStyle);

    /* HDD Images page */

    /* CD-ROM Drive Page */

    /* Audio Page */

    cbAudioDriver->insertItem (vboxGlobal().toString (CEnums::NullAudioDriver));
#if defined Q_WS_WIN32
    cbAudioDriver->insertItem (vboxGlobal().toString (CEnums::WINMMAudioDriver));
#elif defined Q_WS_X11
    cbAudioDriver->insertItem (vboxGlobal().toString (CEnums::OSSAudioDriver));
#ifdef VBOX_WITH_ALSA
    cbAudioDriver->insertItem (vboxGlobal().toString (CEnums::ALSAAudioDriver));
#endif
#endif

    /* Network Page */

    /*
     *  update the Ok button state for pages with validation
     *  (validityChanged() connected to enableNext() will do the job)
     */
    wvalGeneral->revalidate();
    wvalHDD->revalidate();
    wvalDVD->revalidate();
    wvalFloppy->revalidate();

    /* VRDP Page */

    leVRDPPort->setAlignment (Qt::AlignRight);
    cbVRDPAuthType->insertItem (vboxGlobal().toString (CEnums::VRDPAuthNull));
    cbVRDPAuthType->insertItem (vboxGlobal().toString (CEnums::VRDPAuthExternal));
    cbVRDPAuthType->insertItem (vboxGlobal().toString (CEnums::VRDPAuthGuest));
    leVRDPTimeout->setAlignment (Qt::AlignRight);
}

bool VBoxVMSettingsDlg::eventFilter (QObject *object, QEvent *event)
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

void VBoxVMSettingsDlg::showEvent (QShowEvent *e)
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

void VBoxVMSettingsDlg::bootItemActivate (int row, int col, int /* button */,
                                          const QPoint &/* mousePos */)
{
    tblBootOrder->editCell(row, col);
    QTableItem* tableItem = tblBootOrder->item(row, col);
    if (tableItem->rtti() == 1001)
        (static_cast<ComboTableItem*>(tableItem))->getEditor()->popup();
}

void VBoxVMSettingsDlg::updateShortcuts()
{
    /* setup necessary combobox item */
    cbHDA->setRequiredItem (uuidHDA);
    cbHDB->setRequiredItem (uuidHDB);
    cbHDD->setRequiredItem (uuidHDD);
    cbISODVD->setRequiredItem (uuidISODVD);
    cbISOFloppy->setRequiredItem (uuidISOFloppy);
    /* request for refresh every combo-box */
    cbHDA->setReadyForRefresh();
    cbHDB->setReadyForRefresh();
    cbHDD->setReadyForRefresh();
    cbISODVD->setReadyForRefresh();
    cbISOFloppy->setReadyForRefresh();
    /* starting media-enumerating process */
    vboxGlobal().startEnumeratingMedia();
}


void VBoxVMSettingsDlg::hdaMediaChanged()
{
    uuidHDA = grbHDA->isChecked() ? cbHDA->getId() : QUuid();
    cbHDA->setRequiredItem (uuidHDA);
    txHDA->setText (getHdInfo (grbHDA, uuidHDA));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::hdbMediaChanged()
{
    uuidHDB = grbHDB->isChecked() ? cbHDB->getId() : QUuid();
    cbHDB->setRequiredItem (uuidHDB);
    txHDB->setText (getHdInfo (grbHDB, uuidHDB));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::hddMediaChanged()
{
    uuidHDD = grbHDD->isChecked() ? cbHDD->getId() : QUuid();
    cbHDD->setRequiredItem (uuidHDD);
    txHDD->setText (getHdInfo (grbHDD, uuidHDD));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::cdMediaChanged()
{
    uuidISODVD = bgDVD->isChecked() ? cbISODVD->getId() : QUuid();
    cbISODVD->setRequiredItem (uuidISODVD);
    /* revailidate */
    wvalDVD->revalidate();
}


void VBoxVMSettingsDlg::fdMediaChanged()
{
    uuidISOFloppy = bgFloppy->isChecked() ? cbISOFloppy->getId() : QUuid();
    cbISOFloppy->setRequiredItem (uuidISOFloppy);
    /* revailidate */
    wvalFloppy->revalidate();
}


QString VBoxVMSettingsDlg::getHdInfo (QGroupBox *aGroupBox, QUuid aId)
{
    QString notAttached = tr ("<not attached>", "hard disk");
    if (aId.isNull())
        return notAttached;
    return aGroupBox->isChecked() ?
        vboxGlobal().details (vboxGlobal().virtualBox().GetHardDisk (aId), true) :
        notAttached;
}

void VBoxVMSettingsDlg::updateWhatsThis (bool gotFocus /* = false */)
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

void VBoxVMSettingsDlg::setWarning (const QString &warning)
{
    warningString = warning;
    if (!warning.isEmpty())
        warningString = QString ("<font color=red>%1</font>").arg (warning);

    if (!warningString.isEmpty())
        whatsThisLabel->setText (warningString);
    else
        updateWhatsThis (true);
}

/**
 *  Sets up this dialog.
 *
 *  @note Calling this method after the dialog is open has no sense.
 *
 *  @param  category
 *      Category to select when the dialog is open. Must be one of
 *      values from the hidden '[cat]' column of #listView
 *      (see VBoxVMSettingsDlg.ui in qdesigner) prepended with the '#'
 *      sign.
 */
void VBoxVMSettingsDlg::setup (const QString &category)
{
    if (category)
    {
        QListViewItem *item = listView->findItem (category, listView_Link);
        if (item)
            listView->setSelected (item, true);
    }
}

void VBoxVMSettingsDlg::listView_currentChanged (QListViewItem *item)
{
    Assert (item);
    int id = item->text (1).toInt();
    Assert (id >= 0);
    titleLabel->setText (::path (item));
    widgetStack->raiseWidget (id);
}


void VBoxVMSettingsDlg::enableOk( const QIWidgetValidator *wval )
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
            setWarning(0);
        warningLabel->setHidden(valid);
        warningPixmap->setHidden(valid);
    }
}


void VBoxVMSettingsDlg::revalidate( QIWidgetValidator *wval )
{
    /* do individual validations for pages */
    QWidget *pg = wval->widget();
    bool valid = wval->isOtherValid();

    if (pg == pageHDD)
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        valid = true;

        QValueList <QUuid> uuids;

        if (valid && grbHDA->isChecked())
        {
            if (uuidHDA.isNull())
            {
                valid = false;
                setWarning (tr ("Primary Master hard disk is not selected."));
            }
            else uuids << uuidHDA;
        }

        if (valid && grbHDB->isChecked())
        {
            if (uuidHDB.isNull())
            {
                valid = false;
                setWarning (tr ("Primary Slave hard disk is not selected."));
            }
            else
            {
                bool found = uuids.findIndex (uuidHDB) >= 0;
                if (found)
                {
                    CHardDisk hd = vbox.GetHardDisk (uuidHDB);
                    valid = hd.GetType() == CEnums::ImmutableHardDisk;
                }
                if (valid)
                    uuids << uuidHDB;
                else
                    setWarning (tr ("Primary Slave hard disk is already attached "
                                    "to a different slot."));
            }
        }

        if (valid && grbHDD->isChecked())
        {
            if (uuidHDD.isNull())
            {
                valid = false;
                setWarning (tr ("Secondary Slave hard disk is not selected."));
            }
            else
            {
                bool found = uuids.findIndex (uuidHDD) >= 0;
                if (found)
                {
                    CHardDisk hd = vbox.GetHardDisk (uuidHDD);
                    valid = hd.GetType() == CEnums::ImmutableHardDisk;
                }
                if (valid)
                    uuids << uuidHDB;
                else
                    setWarning (tr ("Secondary Slave hard disk is already attached "
                                    "to a different slot."));
            }
        }

        cbHDA->setEnabled (grbHDA->isChecked());
        cbHDB->setEnabled (grbHDB->isChecked());
        cbHDD->setEnabled (grbHDD->isChecked());
        tbHDA->setEnabled (grbHDA->isChecked());
        tbHDB->setEnabled (grbHDB->isChecked());
        tbHDD->setEnabled (grbHDD->isChecked());
    }
    else if (pg == pageDVD)
    {
        if (!bgDVD->isChecked())
            rbHostDVD->setChecked(false), rbISODVD->setChecked(false);
        else if (!rbHostDVD->isChecked() && !rbISODVD->isChecked())
            rbHostDVD->setChecked(true);

        valid = !(rbISODVD->isChecked() && uuidISODVD.isNull());

        cbHostDVD->setEnabled (rbHostDVD->isChecked());

        cbISODVD->setEnabled (rbISODVD->isChecked());
        tbISODVD->setEnabled (rbISODVD->isChecked());

        if (!valid)
            setWarning (tr ("CD/DVD drive image file is not selected."));
    }
    else if (pg == pageFloppy)
    {
        if (!bgFloppy->isChecked())
            rbHostFloppy->setChecked(false), rbISOFloppy->setChecked(false);
        else if (!rbHostFloppy->isChecked() && !rbISOFloppy->isChecked())
            rbHostFloppy->setChecked(true);

        valid = !(rbISOFloppy->isChecked() && uuidISOFloppy.isNull());

        cbHostFloppy->setEnabled (rbHostFloppy->isChecked());

        cbISOFloppy->setEnabled (rbISOFloppy->isChecked());
        tbISOFloppy->setEnabled (rbISOFloppy->isChecked());

        if (!valid)
            setWarning (tr ("Floppy drive image file is not selected."));
    }
    else if (pg == pageNetwork)
    {
        int slot = -1;
        for (int index = 0; index < tbwNetwork->count(); index++)
        {
            QWidget* pTab = tbwNetwork->page (index);
            Assert(pTab);
            VBoxVMNetworkSettings* pNetSet = static_cast <VBoxVMNetworkSettings*>(pTab);
            if (!pNetSet->grbEnabled->isChecked())
            {
                pNetSet->cbNetworkAttachment->setCurrentItem (0);
                pNetSet->cbNetworkAttachment_activated (pNetSet->cbNetworkAttachment->currentText());
            }

            CEnums::NetworkAttachmentType type =
                vboxGlobal().toNetworkAttachmentType (pNetSet->cbNetworkAttachment->currentText());
            valid = (slot == -1) &&
                    !(type == CEnums::HostInterfaceNetworkAttachment &&
                      !pNetSet->checkNetworkInterface (pNetSet->lbHostInterface->currentText()));
            if (slot == -1 && !valid)
                slot = index;
        }
        if (!valid)
            setWarning (tr ("Incorrect host network interface is selected for Adapter %1.")
                        .arg (slot));
    }
    else if (pg == pageVRDP)
    {
        if (pageVRDP->isEnabled())
        {
            valid = !(grbVRDP->isChecked() &&
                    (leVRDPPort->text().isEmpty() || leVRDPTimeout->text().isEmpty()));
            if (!valid && leVRDPPort->text().isEmpty())
                setWarning (tr ("VRDP Port is not set."));
            if (!valid && leVRDPTimeout->text().isEmpty())
                setWarning (tr ("VRDP Timeout is not set."));
        }
        else
            valid = true;
    }

    wval->setOtherValid (valid);
}


void VBoxVMSettingsDlg::getFromMachine (const CMachine &machine)
{
    cmachine = machine;

    setCaption (machine.GetName() + tr (" - Settings"));

    CVirtualBox vbox = vboxGlobal().virtualBox();
    CBIOSSettings biosSettings = cmachine.GetBIOSSettings();

    /* name */
    leName->setText (machine.GetName());

    /* OS type */
    CGuestOSType type = machine.GetOSType();
    cbOS->setCurrentItem (vboxGlobal().vmGuestOSTypeIndex(type));
    cbOS_activated (cbOS->currentItem());

    /* RAM size */
    slRAM->setValue (machine.GetMemorySize());

    /* VRAM size */
    slVRAM->setValue (machine.GetVRAMSize());

    /* boot order */
    bootDevicesInUse.clear();
    for (int i = 0; i < tblBootOrder->numRows(); i ++)
    {
        QTableItem *item = tblBootOrder->item (i, 0);
        item->setText (vboxGlobal().toString (machine.GetBootOrder (i + 1)));
    }

    /* ACPI */
    chbEnableACPI->setChecked (biosSettings.GetACPIEnabled());

    /* IO APIC */
    chbEnableIOAPIC->setChecked (biosSettings.GetIOAPICEnabled());

    /* Saved state folder */
    leSnapshotFolder->setText (machine.GetSnapshotFolder());

    /* hard disk images */
    {
        struct
        {
            CEnums::DiskControllerType ctl;
            LONG dev;
            struct {
                QGroupBox *grb;
                QComboBox *cbb;
                QLabel *tx;
                QUuid *uuid;
            } data;
        }
        diskSet[] =
        {
            { CEnums::IDE0Controller, 0, {grbHDA, cbHDA, txHDA, &uuidHDA} },
            { CEnums::IDE0Controller, 1, {grbHDB, cbHDB, txHDB, &uuidHDB} },
            { CEnums::IDE1Controller, 1, {grbHDD, cbHDD, txHDD, &uuidHDD} },
        };

        grbHDA->setChecked (false);
        grbHDB->setChecked (false);
        grbHDD->setChecked (false);

        CHardDiskAttachmentEnumerator en =
            machine.GetHardDiskAttachments().Enumerate();
        while (en.HasMore())
        {
            CHardDiskAttachment hda = en.GetNext();
            for (uint i = 0; i < SIZEOF_ARRAY (diskSet); i++)
            {
                if (diskSet [i].ctl == hda.GetController() &&
                    diskSet [i].dev == hda.GetDeviceNumber())
                {
                    CHardDisk hd = hda.GetHardDisk();
                    CHardDisk root = hd.GetRoot();
                    QString src = root.GetLocation();
                    if (hd.GetStorageType() == CEnums::VirtualDiskImage)
                    {
                        QFileInfo fi (src);
                        src = fi.fileName() + " (" +
                              QDir::convertSeparators (fi.dirPath (true)) + ")";
                    }
                    diskSet [i].data.grb->setChecked (true);
                    diskSet [i].data.tx->setText (vboxGlobal().details (hd));
                    *(diskSet [i].data.uuid) = QUuid (root.GetId());
                }
            }
        }
    }

    /* floppy image */
    {
        /* read out the host floppy drive list and prepare the combobox */
        CHostFloppyDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetFloppyDrives();
        hostFloppies.resize (coll.GetCount());
        cbHostFloppy->clear();
        int id = 0;
        CHostFloppyDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostFloppyDrive hostFloppy = en.GetNext();
            /** @todo set icon? */
            cbHostFloppy->insertItem (hostFloppy.GetName(), id);
            hostFloppies [id] = hostFloppy;
            ++ id;
        }

        CFloppyDrive floppy = machine.GetFloppyDrive();
        switch (floppy.GetState())
        {
            case CEnums::HostDriveCaptured:
            {
                CHostFloppyDrive drv = floppy.GetHostDrive();
                QString name = drv.GetName();
                if (coll.FindByName (name).isNull())
                {
                    /*
                     *  if the floppy drive is not currently available,
                     *  add it to the end of the list with a special mark
                     */
                    cbHostFloppy->insertItem ("* " + name);
                    cbHostFloppy->setCurrentItem (cbHostFloppy->count() - 1);
                }
                else
                {
                    /* this will select the correct item from the prepared list */
                    cbHostFloppy->setCurrentText (name);
                }
                rbHostFloppy->setChecked (true);
                break;
            }
            case CEnums::ImageMounted:
            {
                CFloppyImage img = floppy.GetImage();
                QString src = img.GetFilePath();
                AssertMsg (!src.isNull(), ("Image file must not be null"));
                QFileInfo fi (src);
                rbISOFloppy->setChecked (true);
                uuidISOFloppy = QUuid (img.GetId());
                break;
            }
            case CEnums::NotMounted:
            {
                bgFloppy->setChecked(false);
                break;
            }
            default:
                AssertMsgFailed (("invalid floppy state: %d\n", floppy.GetState()));
        }
    }

    /* CD/DVD-ROM image */
    {
        /* read out the host DVD drive list and prepare the combobox */
        CHostDVDDriveCollection coll =
            vboxGlobal().virtualBox().GetHost().GetDVDDrives();
        hostDVDs.resize (coll.GetCount());
        cbHostDVD->clear();
        int id = 0;
        CHostDVDDriveEnumerator en = coll.Enumerate();
        while (en.HasMore())
        {
            CHostDVDDrive hostDVD = en.GetNext();
            /// @todo (r=dmik) set icon?
            cbHostDVD->insertItem (hostDVD.GetName(), id);
            hostDVDs [id] = hostDVD;
            ++ id;
        }

        CDVDDrive dvd = machine.GetDVDDrive();
        switch (dvd.GetState())
        {
            case CEnums::HostDriveCaptured:
            {
                CHostDVDDrive drv = dvd.GetHostDrive();
                QString name = drv.GetName();
                if (coll.FindByName (name).isNull())
                {
                    /*
                     *  if the DVD drive is not currently available,
                     *  add it to the end of the list with a special mark
                     */
                    cbHostDVD->insertItem ("* " + name);
                    cbHostDVD->setCurrentItem (cbHostDVD->count() - 1);
                }
                else
                {
                    /* this will select the correct item from the prepared list */
                    cbHostDVD->setCurrentText (name);
                }
                rbHostDVD->setChecked (true);
                break;
            }
            case CEnums::ImageMounted:
            {
                CDVDImage img = dvd.GetImage();
                QString src = img.GetFilePath();
                AssertMsg (!src.isNull(), ("Image file must not be null"));
                QFileInfo fi (src);
                rbISODVD->setChecked (true);
                uuidISODVD = QUuid (img.GetId());
                break;
            }
            case CEnums::NotMounted:
            {
                bgDVD->setChecked(false);
                break;
            }
            default:
                AssertMsgFailed (("invalid DVD state: %d\n", dvd.GetState()));
        }
    }

    /* audio */
    {
        CAudioAdapter audio = machine.GetAudioAdapter();
        grbAudio->setChecked (audio.GetEnabled());
        cbAudioDriver->setCurrentText (vboxGlobal().toString (audio.GetAudioDriver()));
    }

    /* network */
    {
        ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
        for (ulong slot = 0; slot < count; slot ++)
        {
            CNetworkAdapter adapter = machine.GetNetworkAdapter (slot);
            addNetworkAdapter (adapter, false);
        }
    }

    /* USB */
    {
        CUSBController ctl = machine.GetUSBController();

        if (ctl.isNull())
        {
            /* disable the USB controller category if the USB controller is
             * not available (i.e. in VirtualBox OSE) */

            QListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
            Assert (usbItem);
            if (usbItem)
                usbItem->setVisible (false);

            /* disable validators if any */
            pageUSB->setEnabled (false);

            /* if machine has something to say, show the message */
            vboxProblem().cannotLoadMachineSettings (machine, false /* strict */);
        }
        else
        {
            cbEnableUSBController->setChecked (ctl.GetEnabled());

            CUSBDeviceFilterEnumerator en = ctl.GetDeviceFilters().Enumerate();
            while (en.HasMore())
                addUSBFilter (en.GetNext(), false /* isNew */);

            lvUSBFilters->setCurrentItem (lvUSBFilters->firstChild());
            /* silly Qt -- doesn't emit currentChanged after adding the
             * first item to an empty list */
            lvUSBFilters_currentChanged (lvUSBFilters->firstChild());
        }
    }

    /* vrdp */
    {
        CVRDPServer vrdp = machine.GetVRDPServer();

        if (vrdp.isNull())
        {
            /* disable the VRDP category if VRDP is
             * not available (i.e. in VirtualBox OSE) */

            QListViewItem *vrdpItem = listView->findItem ("#vrdp", listView_Link);
            Assert (vrdpItem);
            if (vrdpItem)
                vrdpItem->setVisible (false);

            /* disable validators if any */
            pageVRDP->setEnabled (false);

            /* if machine has something to say, show the message */
            vboxProblem().cannotLoadMachineSettings (machine, false /* strict */);
        }
        else
        {
            grbVRDP->setChecked (vrdp.GetEnabled());
            leVRDPPort->setText (QString::number (vrdp.GetPort()));
            cbVRDPAuthType->setCurrentText (vboxGlobal().toString (vrdp.GetAuthType()));
            leVRDPTimeout->setText (QString::number (vrdp.GetAuthTimeout()));
        }
    }

    /* request for media shortcuts update */
    cbHDA->setBelongsTo(machine.GetId());
    cbHDB->setBelongsTo(machine.GetId());
    cbHDD->setBelongsTo(machine.GetId());
    updateShortcuts();

    /* revalidate pages with custom validation */
    wvalHDD->revalidate();
    wvalDVD->revalidate();
    wvalFloppy->revalidate();
    wvalVRDP->revalidate();
}


COMResult VBoxVMSettingsDlg::putBackToMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CBIOSSettings biosSettings = cmachine.GetBIOSSettings();

    /* name */
    cmachine.SetName (leName->text());

    /* OS type */
    CGuestOSType type = vboxGlobal().vmGuestOSType (cbOS->currentItem());
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    cmachine.SetOSType (type);

    /* RAM size */
    cmachine.SetMemorySize (slRAM->value());

    /* VRAM size */
    cmachine.SetVRAMSize (slVRAM->value());

    /* boot order */
    for (int i = 0; i < tblBootOrder->numRows(); i ++)
    {
        QTableItem *item = tblBootOrder->item (i, 0);
        cmachine.SetBootOrder (i + 1, vboxGlobal().toDeviceType (item->text()));
    }

    /* ACPI */
    biosSettings.SetACPIEnabled (chbEnableACPI->isChecked());

    /* IO APIC */
    biosSettings.SetIOAPICEnabled (chbEnableIOAPIC->isChecked());

    /* Saved state folder */
    if (leSnapshotFolder->isModified())
        cmachine.SetSnapshotFolder (leSnapshotFolder->text());

    /* hard disk images */
    {
        struct
        {
            CEnums::DiskControllerType ctl;
            LONG dev;
            struct {
                QGroupBox *grb;
                QUuid *uuid;
            } data;
        }
        diskSet[] =
        {
            { CEnums::IDE0Controller, 0, {grbHDA, &uuidHDA} },
            { CEnums::IDE0Controller, 1, {grbHDB, &uuidHDB} },
            { CEnums::IDE1Controller, 1, {grbHDD, &uuidHDD} }
        };

        /*
         *  first, detach all disks (to ensure we can reattach them to different
         *  controllers / devices, when appropriate)
         */
        CHardDiskAttachmentEnumerator en =
            cmachine.GetHardDiskAttachments().Enumerate();
        while (en.HasMore())
        {
            CHardDiskAttachment hda = en.GetNext();
            for (uint i = 0; i < SIZEOF_ARRAY (diskSet); i++)
            {
                if (diskSet [i].ctl == hda.GetController() &&
                    diskSet [i].dev == hda.GetDeviceNumber())
                {
                    cmachine.DetachHardDisk (diskSet [i].ctl, diskSet [i].dev);
                    if (!cmachine.isOk())
                        vboxProblem().cannotDetachHardDisk (
                            this, cmachine, diskSet [i].ctl, diskSet [i].dev);
                }
            }
        }

        /* now, attach new disks */
        for (uint i = 0; i < SIZEOF_ARRAY (diskSet); i++)
        {
            QUuid *newId = diskSet [i].data.uuid;
            if (diskSet [i].data.grb->isChecked() && !(*newId).isNull())
            {
                cmachine.AttachHardDisk (*newId, diskSet [i].ctl, diskSet [i].dev);
                if (!cmachine.isOk())
                    vboxProblem().cannotAttachHardDisk (
                        this, cmachine, *newId, diskSet [i].ctl, diskSet [i].dev);
            }
        }
    }

    /* floppy image */
    {
        CFloppyDrive floppy = cmachine.GetFloppyDrive();
        if (!bgFloppy->isChecked())
        {
            floppy.Unmount();
        }
        else if (rbHostFloppy->isChecked())
        {
            int id = cbHostFloppy->currentItem();
            Assert (id >= 0);
            if (id < (int) hostFloppies.count())
                floppy.CaptureHostDrive (hostFloppies [id]);
            /*
             *  otherwise the selected drive is not yet available, leave it
             *  as is
             */
        }
        else if (rbISOFloppy->isChecked())
        {
            Assert (!uuidISOFloppy.isNull());
            floppy.MountImage (uuidISOFloppy);
        }
    }

    /* CD/DVD-ROM image */
    {
        CDVDDrive dvd = cmachine.GetDVDDrive();
        if (!bgDVD->isChecked())
        {
            dvd.Unmount();
        }
        else if (rbHostDVD->isChecked())
        {
            int id = cbHostDVD->currentItem();
            Assert (id >= 0);
            if (id < (int) hostDVDs.count())
                dvd.CaptureHostDrive (hostDVDs [id]);
            /*
             *  otherwise the selected drive is not yet available, leave it
             *  as is
             */
        }
        else if (rbISODVD->isChecked())
        {
            Assert (!uuidISODVD.isNull());
            dvd.MountImage (uuidISODVD);
        }
    }

    /* audio */
    {
        CAudioAdapter audio = cmachine.GetAudioAdapter();
        audio.SetAudioDriver (vboxGlobal().toAudioDriverType (cbAudioDriver->currentText()));
        audio.SetEnabled (grbAudio->isChecked());
        AssertWrapperOk (audio);
    }

    /* network */
    {
        for (int index = 0; index < tbwNetwork->count(); index++)
        {
            VBoxVMNetworkSettings *page =
                (VBoxVMNetworkSettings *) tbwNetwork->page (index);
            Assert (page);
            page->putBackToAdapter();
        }
    }

    /* usb */
    {
        CUSBController ctl = cmachine.GetUSBController();

        if (!ctl.isNull())
        {
            /* the USB controller may be unavailable (i.e. in VirtualBox OSE) */

            ctl.SetEnabled (cbEnableUSBController->isChecked());

            /*
             *  first, remove all old filters (only if the list is changed,
             *  not only individual properties of filters)
             */
            if (mUSBFilterListModified)
                for (ulong count = ctl.GetDeviceFilters().GetCount(); count; -- count)
                    ctl.RemoveDeviceFilter (0);

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
                    return res;

                CUSBDeviceFilter filter = settings->filter();
                filter.SetActive (uli->isOn());

                if (mUSBFilterListModified)
                    ctl.InsertDeviceFilter (~0, filter);
            }
        }

        mUSBFilterListModified = false;
    }

    /* vrdp */
    {
        CVRDPServer vrdp = cmachine.GetVRDPServer();

        if (!vrdp.isNull())
        {
            /* VRDP may be unavailable (i.e. in VirtualBox OSE) */
            vrdp.SetEnabled (grbVRDP->isChecked());
            vrdp.SetPort (leVRDPPort->text().toULong());
            vrdp.SetAuthType (vboxGlobal().toVRDPAuthType (cbVRDPAuthType->currentText()));
            vrdp.SetAuthTimeout (leVRDPTimeout->text().toULong());
        }
    }

    return COMResult();
}


void VBoxVMSettingsDlg::showImageManagerHDA() { showVDImageManager (&uuidHDA, cbHDA); }
void VBoxVMSettingsDlg::showImageManagerHDB() { showVDImageManager (&uuidHDB, cbHDB); }
void VBoxVMSettingsDlg::showImageManagerHDD() { showVDImageManager (&uuidHDD, cbHDD); }
void VBoxVMSettingsDlg::showImageManagerISODVD() { showVDImageManager (&uuidISODVD, cbISODVD); }
void VBoxVMSettingsDlg::showImageManagerISOFloppy() { showVDImageManager(&uuidISOFloppy, cbISOFloppy); }

void VBoxVMSettingsDlg::showVDImageManager (QUuid *id, VBoxMediaComboBox *cbb, QLabel*)
{
    VBoxDefs::DiskType type = VBoxDefs::InvalidType;
    if (cbb == cbISODVD)
        type = VBoxDefs::CD;
    else if (cbb == cbISOFloppy)
        type = VBoxDefs::FD;
    else
        type = VBoxDefs::HD;

    VBoxDiskImageManagerDlg dlg (this, "VBoxDiskImageManagerDlg",
                                 WType_Dialog | WShowModal);
    QUuid machineId = cmachine.GetId();
    dlg.setup (type, true, &machineId, (const VBoxMediaList*)0, cmachine);
    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
        *id = dlg.getSelectedUuid();
    updateShortcuts();
    cbb->setFocus();
}

void VBoxVMSettingsDlg::addNetworkAdapter (const CNetworkAdapter &adapter,
                                           bool /* select */)
{
    VBoxVMNetworkSettings *page = new VBoxVMNetworkSettings ();
    page->getFromAdapter (adapter);
    tbwNetwork->addTab(page, QString("Adapter %1").arg(adapter.GetSlot()));

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (page->leTAPTerminate, buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    /* setup validation */
    QIWidgetValidator *wval = new QIWidgetValidator (pageNetwork, this);
    connect (page->cbNetworkAttachment, SIGNAL (activated (const QString &)),
             wval, SLOT (revalidate()));

    connect (page->lbHostInterface, SIGNAL ( selectionChanged () ),
             wval, SLOT (revalidate()));
    connect (page->lbHostInterface, SIGNAL ( currentChanged ( QListBoxItem * ) ),
             wval, SLOT (revalidate()));
    connect (page->lbHostInterface, SIGNAL ( highlighted ( QListBoxItem * ) ),
             wval, SLOT (revalidate()));
    connect (page->grbEnabled, SIGNAL (toggled (bool)),
             wval, SLOT (revalidate()));

    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    wval->revalidate();
}

void VBoxVMSettingsDlg::slRAM_valueChanged( int val )
{
    leRAM->setText( QString().setNum( val ) );
}

void VBoxVMSettingsDlg::leRAM_textChanged( const QString &text )
{
    slRAM->setValue( text.toInt() );
}

void VBoxVMSettingsDlg::slVRAM_valueChanged( int val )
{
    leVRAM->setText( QString().setNum( val ) );
}

void VBoxVMSettingsDlg::leVRAM_textChanged( const QString &text )
{
    slVRAM->setValue( text.toInt() );
}

void VBoxVMSettingsDlg::cbOS_activated (int item)
{
    Q_UNUSED (item);
/// @todo (dmik) remove?
//    CGuestOSType type = vboxGlobal().vmGuestOSType (item);
//    txRAMBest->setText (tr ("<qt>Best&nbsp;%1&nbsp;MB<qt>")
//                            .arg (type.GetRecommendedRAM()));
//    txVRAMBest->setText (tr ("<qt>Best&nbsp;%1&nbsp;MB</qt>")
//                             .arg (type.GetRecommendedVRAM()));
    txRAMBest->setText (QString::null);
    txVRAMBest->setText (QString::null);
}

void VBoxVMSettingsDlg::tbResetSavedStateFolder_clicked()
{
    /*
     *  do this instead of le->setText (QString::null) to cause
     *  isModified() return true
     */
    leSnapshotFolder->selectAll();
    leSnapshotFolder->del();
}

void VBoxVMSettingsDlg::tbSelectSavedStateFolder_clicked()
{
    QString settingsFolder =
        QFileInfo (cmachine.GetSettingsFilePath()).dirPath (true);

    QFileDialog dlg (settingsFolder, QString::null, this);
    dlg.setMode (QFileDialog::DirectoryOnly);

    if (!leSnapshotFolder->text().isEmpty())
    {
        /* set the first parent directory that exists as the current */
        QDir dir (settingsFolder);
        QFileInfo fld (dir, leSnapshotFolder->text());
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
        leSnapshotFolder->selectAll();
        leSnapshotFolder->insert (folder);
    }
}

// USB Filter stuff
////////////////////////////////////////////////////////////////////////////////

void VBoxVMSettingsDlg::addUSBFilter (const CUSBDeviceFilter &aFilter, bool isNew)
{
    QListViewItem *currentItem = isNew
        ? lvUSBFilters->currentItem()
        : lvUSBFilters->lastItem();

    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->getFromFilter (aFilter);

    USBListItem *item = new USBListItem (lvUSBFilters, currentItem);
    item->setOn (aFilter.GetActive());
    item->setText (lvUSBFilters_Name, aFilter.GetName());

    item->mId = wstUSBFilters->addWidget (settings);

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (settings->focusProxy(), buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    if (isNew)
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

void VBoxVMSettingsDlg::lvUSBFilters_currentChanged (QListViewItem *item)
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

void VBoxVMSettingsDlg::lvUSBFilters_setCurrentText (const QString &aText)
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    item->setText (lvUSBFilters_Name, aText);
}

void VBoxVMSettingsDlg::tbAddUSBFilter_clicked()
{
    CUSBDeviceFilter filter = cmachine.GetUSBController()
        .CreateDeviceFilter (tr ("New Filter %1", "usb")
                                 .arg (++ mLastUSBFilterNum));

    filter.SetActive (true);
    addUSBFilter (filter, true /* isNew */);

    mUSBFilterListModified = true;
}

void VBoxVMSettingsDlg::tbAddUSBFilterFrom_clicked()
{
    usbDevicesMenu->clear();
    usbDevicesMap.clear();
    CHost host = vboxGlobal().virtualBox().GetHost();

    bool isUSBEmpty = host.GetUSBDevices().GetCount() == 0;
    if (isUSBEmpty)
    {
        usbDevicesMenu->insertItem (
            tr ("<no available devices>", "USB devices"),
            USBDevicesMenuNoDevicesId);
        usbDevicesMenu->setItemEnabled (USBDevicesMenuNoDevicesId, false);
    }
    else
    {
        CHostUSBDeviceEnumerator en = host.GetUSBDevices().Enumerate();
        while (en.HasMore())
        {
            CHostUSBDevice iterator = en.GetNext();
            CUSBDevice usb = CUnknown (iterator);
            int id = usbDevicesMenu->insertItem (vboxGlobal().details (usb));
            usbDevicesMap [id] = usb;
        }
    }

    usbDevicesMenu->exec (QCursor::pos());
}

void VBoxVMSettingsDlg::menuAddUSBFilterFrom_highlighted (int aIndex)
{
    /* the <no available devices> item is highlighted */
    if (aIndex == USBDevicesMenuNoDevicesId)
    {
        QToolTip::add (usbDevicesMenu,
            tr ("No supported devices connected to the host PC",
                "USB device tooltip"));
        return;
    }

    CUSBDevice usb = usbDevicesMap [aIndex];
    /* if null then some other item but a USB device is highlighted */
    if (usb.isNull())
    {
        QToolTip::remove (usbDevicesMenu);
        return;
    }

    QToolTip::remove (usbDevicesMenu);
    QToolTip::add (usbDevicesMenu, vboxGlobal().toolTip (usb));
}

void VBoxVMSettingsDlg::menuAddUSBFilterFrom_activated (int aIndex)
{
    CUSBDevice usb = usbDevicesMap [aIndex];
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    CUSBDeviceFilter filter = cmachine.GetUSBController()
        .CreateDeviceFilter (vboxGlobal().details (usb));

    filter.SetVendorId (QString().sprintf ("%04hX", usb.GetVendorId()));
    filter.SetProductId (QString().sprintf ("%04hX", usb.GetProductId()));
    filter.SetRevision (QString().sprintf ("%04hX", usb.GetRevision()));
    filter.SetPort (QString().sprintf ("%04hX", usb.GetPort()));
    filter.SetManufacturer (usb.GetManufacturer());
    filter.SetProduct (usb.GetProduct());
    filter.SetSerialNumber (usb.GetSerialNumber());
    filter.SetRemote (usb.GetRemote() ? "yes" : "no");

    filter.SetActive (true);
    addUSBFilter (filter, true /* isNew */);

    mUSBFilterListModified = true;
}

void VBoxVMSettingsDlg::tbRemoveUSBFilter_clicked()
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

void VBoxVMSettingsDlg::tbUSBFilterUp_clicked()
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    QListViewItem *itemAbove = item->itemAbove();
    Assert (itemAbove);
    itemAbove = itemAbove->itemAbove();

    if (!itemAbove)
    {
        /* overcome Qt stupidity */
        item->itemAbove()->moveItem (item);
    }
    else
        item->moveItem (itemAbove);

    lvUSBFilters_currentChanged (item);
    mUSBFilterListModified = true;
}

void VBoxVMSettingsDlg::tbUSBFilterDown_clicked()
{
    QListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    QListViewItem *itemBelow = item->itemBelow();
    Assert (itemBelow);

    item->moveItem (itemBelow);

    lvUSBFilters_currentChanged (item);
    mUSBFilterListModified = true;
}

#include "VBoxVMSettingsDlg.ui.moc"

