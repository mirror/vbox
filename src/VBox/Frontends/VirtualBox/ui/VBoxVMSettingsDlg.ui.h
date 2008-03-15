/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "VM settings" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
 *  QDialog class reimplementation to use for adding network interface.
 *  It has one line-edit field for entering network interface's name and
 *  common dialog's ok/cancel buttons.
 */
class VBoxAddNIDialog : public QDialog
{
    Q_OBJECT

public:

    VBoxAddNIDialog (QWidget *aParent, const QString &aIfaceName) :
        QDialog (aParent, "VBoxAddNIDialog", true /* modal */),
        mLeName (0)
    {
        setCaption (tr ("Add Host Interface"));
        QVBoxLayout *mainLayout = new QVBoxLayout (this, 10, 10, "mainLayout");

        /* Setup Input layout */
        QHBoxLayout *inputLayout = new QHBoxLayout (mainLayout, 10, "inputLayout");
        QLabel *lbName = new QLabel (tr ("Interface Name"), this);
        mLeName = new QLineEdit (aIfaceName, this);
        QWhatsThis::add (mLeName, tr ("Descriptive name of the new network interface"));
        inputLayout->addWidget (lbName);
        inputLayout->addWidget (mLeName);
        connect (mLeName, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));

        /* Setup Button layout */
        QHBoxLayout *buttonLayout = new QHBoxLayout (mainLayout, 10, "buttonLayout");
        mBtOk = new QPushButton (tr ("&OK"), this, "mBtOk");
        QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        QPushButton *btCancel = new QPushButton (tr ("Cancel"), this, "btCancel");
        connect (mBtOk, SIGNAL (clicked()), this, SLOT (accept()));
        connect (btCancel, SIGNAL (clicked()), this, SLOT (reject()));
        buttonLayout->addWidget (mBtOk);
        buttonLayout->addItem (spacer);
        buttonLayout->addWidget (btCancel);

        /* resize to fit the aIfaceName in one string */
        int requiredWidth = mLeName->fontMetrics().width (aIfaceName) +
                            mLeName->frameWidth() * 2 +
                            mLeName->lineWidth() * 2 +
                            inputLayout->spacing() +
                            lbName->fontMetrics().width (lbName->text()) +
                            lbName->frameWidth() * 2 +
                            lbName->lineWidth() * 2 +
                            mainLayout->margin() * 2;
        resize (requiredWidth, minimumHeight());

        /* Validate interface name field */
        validate();
    }

    ~VBoxAddNIDialog() {}

    QString getName() { return mLeName->text(); }

private slots:

    void validate()
    {
        mBtOk->setEnabled (!mLeName->text().isEmpty());
    }

private:

    void showEvent (QShowEvent *aEvent)
    {
        setFixedHeight (height());
        QDialog::showEvent (aEvent);
    }

    QPushButton *mBtOk;
    QLineEdit *mLeName;
};


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
 *  QListView class reimplementation to use as boot items table.
 *  It has one unsorted column without header with automated width
 *  resize management.
 *  Keymapping handlers for ctrl-up & ctrl-down are translated into
 *  boot-items up/down moving.
 */
class BootItemsTable : public QListView
{
    Q_OBJECT

public:

    BootItemsTable (QWidget *aParent, const char *aName)
        : QListView (aParent, aName)
    {
        addColumn (QString::null);
        header()->hide();
        setSorting (-1);
        setColumnWidthMode (0, Maximum);
        setResizeMode (AllColumns);
        QWhatsThis::add (this, tr ("Defines the boot device order. "
                                   "Use checkboxes to the left to enable or disable "
                                   "individual boot devices. Move items up and down to "
                                   "change the device order."));
        setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect (this, SIGNAL (pressed (QListViewItem*)),
                 this, SLOT (processPressed (QListViewItem*)));
    }

    ~BootItemsTable() {}

    void emitItemToggled() { emit itemToggled(); }

signals:

    void moveItemUp();
    void moveItemDown();
    void itemToggled();

private slots:

    void processPressed (QListViewItem *aItem)
    {
        if (!aItem)
            setSelected (currentItem(), true);
    }

    void keyPressEvent (QKeyEvent *aEvent)
    {
        if (aEvent->state() == Qt::ControlButton)
        {
            switch (aEvent->key())
            {
                case Qt::Key_Up:
                    emit moveItemUp();
                    return;
                case Qt::Key_Down:
                    emit moveItemDown();
                    return;
                default:
                    break;
            }
        }
        QListView::keyPressEvent (aEvent);
    }
};


/**
 *  QWidget class reimplementation to use as boot items widget.
 *  It contains BootItemsTable and two tool-buttons for moving
 *  boot-items up/down.
 *  This widget handles saving/loading CMachine information related
 *  to boot sequience.
 */
class BootItemsList : public QWidget
{
    Q_OBJECT

    class BootItem : public QCheckListItem
    {
        public:

            BootItem (BootItemsTable *aParent, QListViewItem *aAfter,
                      const QString &aName, Type aType)
                : QCheckListItem (aParent, aAfter, aName, aType) {}

        private:

            void stateChange (bool)
            {
                BootItemsTable *table = static_cast<BootItemsTable*> (listView());
                table->emitItemToggled();
            }
    };

public:

    BootItemsList (QWidget *aParent, const char *aName)
        : QWidget (aParent, aName), mBootTable (0)
    {
        /* Setup main widget layout */
        QHBoxLayout *mainLayout = new QHBoxLayout (this, 0, 6, "mainLayout");

        /* Setup settings layout */
        mBootTable = new BootItemsTable (this, "mBootTable");
        connect (mBootTable, SIGNAL (currentChanged (QListViewItem*)),
                 this, SLOT (processCurrentChanged (QListViewItem*)));
        mainLayout->addWidget (mBootTable);

        /* Setup button's layout */
        QVBoxLayout *buttonLayout = new QVBoxLayout (mainLayout, 0, "buttonLayout");
        mBtnUp = new QToolButton (this, "mBtnUp");
        mBtnDown = new QToolButton (this, "mBtnDown");
        mBtnUp->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
        mBtnDown->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
        QWhatsThis::add (mBtnUp, tr ("Moves the selected boot device up."));
        QWhatsThis::add (mBtnDown, tr ("Moves the selected boot device down."));
        QToolTip::add (mBtnUp, tr ("Move Up (Ctrl-Up)"));
        QToolTip::add (mBtnDown, tr ("Move Down (Ctrl-Down)"));
        mBtnUp->setAutoRaise (true);
        mBtnDown->setAutoRaise (true);
        mBtnUp->setFocusPolicy (QWidget::StrongFocus);
        mBtnDown->setFocusPolicy (QWidget::StrongFocus);
        mBtnUp->setIconSet (VBoxGlobal::iconSet ("list_moveup_16px.png",
                                                 "list_moveup_disabled_16px.png"));
        mBtnDown->setIconSet (VBoxGlobal::iconSet ("list_movedown_16px.png",
                                                   "list_movedown_disabled_16px.png"));
        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Minimum,
                                                     QSizePolicy::Minimum);
        connect (mBtnUp, SIGNAL (clicked()), this, SLOT (moveItemUp()));
        connect (mBtnDown, SIGNAL (clicked()), this, SLOT (moveItemDown()));
        connect (mBootTable, SIGNAL (moveItemUp()), this, SLOT (moveItemUp()));
        connect (mBootTable, SIGNAL (moveItemDown()), this, SLOT (moveItemDown()));
        connect (mBootTable, SIGNAL (itemToggled()), this, SLOT (onItemToggled()));
        buttonLayout->addWidget (mBtnUp);
        buttonLayout->addWidget (mBtnDown);
        buttonLayout->addItem (spacer);

        /* Setup focus proxy for BootItemsList */
        setFocusProxy (mBootTable);
    }

    ~BootItemsList() {}

    void fixTabStops()
    {
        /* fix focus order for BootItemsList */
        setTabOrder (mBootTable, mBtnUp);
        setTabOrder (mBtnUp, mBtnDown);
    }

    void getFromMachine (const CMachine &aMachine)
    {
        /* Load boot-items of current VM */
        QStringList uniqueList;
        int minimumWidth = 0;
        for (int i = 1; i <= 4; ++ i)
        {
            KDeviceType type = aMachine.GetBootOrder (i);
            if (type != KDeviceType_Null)
            {
                QString name = vboxGlobal().toString (type);
                QCheckListItem *item = new BootItem (mBootTable,
                    mBootTable->lastItem(), name, QCheckListItem::CheckBox);
                item->setOn (true);
                uniqueList << name;
                int width = item->width (mBootTable->fontMetrics(), mBootTable, 0);
                if (width > minimumWidth) minimumWidth = width;
            }
        }
        /* Load other unique boot-items */
        for (int i = KDeviceType_Floppy; i < KDeviceType_USB; ++ i)
        {
            QString name = vboxGlobal().toString ((KDeviceType) i);
            if (!uniqueList.contains (name))
            {
                QCheckListItem *item = new BootItem (mBootTable,
                    mBootTable->lastItem(), name, QCheckListItem::CheckBox);
                uniqueList << name;
                int width = item->width (mBootTable->fontMetrics(), mBootTable, 0);
                if (width > minimumWidth) minimumWidth = width;
            }
        }
        processCurrentChanged (mBootTable->firstChild());
        mBootTable->setFixedWidth (minimumWidth +
                                   4 /* viewport margin */);
        mBootTable->setFixedHeight (mBootTable->childCount() *
                                    mBootTable->firstChild()->totalHeight() +
                                    4 /* viewport margin */);
    }

    void putBackToMachine (CMachine &aMachine)
    {
        QCheckListItem *item = 0;
        /* Search for checked items */
        int index = 1;
        item = static_cast<QCheckListItem*> (mBootTable->firstChild());
        while (item)
        {
            if (item->isOn())
            {
                KDeviceType type =
                    vboxGlobal().toDeviceType (item->text (0));
                aMachine.SetBootOrder (index++, type);
            }
            item = static_cast<QCheckListItem*> (item->nextSibling());
        }
        /* Search for non-checked items */
        item = static_cast<QCheckListItem*> (mBootTable->firstChild());
        while (item)
        {
            if (!item->isOn())
                aMachine.SetBootOrder (index++, KDeviceType_Null);
            item = static_cast<QCheckListItem*> (item->nextSibling());
        }
    }

    void processFocusIn (QWidget *aWidget)
    {
        if (aWidget == mBootTable)
        {
            mBootTable->setSelected (mBootTable->currentItem(), true);
            processCurrentChanged (mBootTable->currentItem());
        }
        else if (aWidget != mBtnUp && aWidget != mBtnDown)
        {
            mBootTable->setSelected (mBootTable->currentItem(), false);
            processCurrentChanged (mBootTable->currentItem());
        }
    }

signals:

    void bootSequenceChanged();

private slots:

    void moveItemUp()
    {
        QListViewItem *item = mBootTable->currentItem();
        Assert (item);
        QListViewItem *itemAbove = item->itemAbove();
        if (!itemAbove) return;
        itemAbove->moveItem (item);
        processCurrentChanged (item);
        emit bootSequenceChanged();
    }

    void moveItemDown()
    {
        QListViewItem *item = mBootTable->currentItem();
        Assert (item);
        QListViewItem *itemBelow = item->itemBelow();
        if (!itemBelow) return;
        item->moveItem (itemBelow);
        processCurrentChanged (item);
        emit bootSequenceChanged();
    }

    void onItemToggled()
    {
        emit bootSequenceChanged();
    }

    void processCurrentChanged (QListViewItem *aItem)
    {
        bool upEnabled   = aItem && aItem->isSelected() && aItem->itemAbove();
        bool downEnabled = aItem && aItem->isSelected() && aItem->itemBelow();
        if ((mBtnUp->hasFocus() && !upEnabled) ||
            (mBtnDown->hasFocus() && !downEnabled))
            mBootTable->setFocus();
        mBtnUp->setEnabled (upEnabled);
        mBtnDown->setEnabled (downEnabled);
    }

private:

    BootItemsTable *mBootTable;
    QToolButton *mBtnUp;
    QToolButton *mBtnDown;
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

    /* disallow resetting First Run Wizard flag until media enumeration
     * process is finished and all data is finally loaded into ui */
    mAllowResetFirstRunFlag = false;
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (onMediaEnumerationDone()));

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

    whatsThisLabel = new QIRichLabel (this, "whatsThisLabel");
    VBoxVMSettingsDlgLayout->addWidget (whatsThisLabel, 2, 1);

#ifndef DEBUG
    /* Enforce rich text format to avoid jumping margins (margins of plain
     * text labels seem to be smaller). We don't do it in the DEBUG builds to
     * be able to immediately catch badly formatted text (i.e. text that
     * contains HTML tags but doesn't start with <qt> so that Qt isn't able to
     * recognize it as rich text and draws all tags as is instead of doing
     * formatting). We want to catch this text because this is how it will look
     * in the whatsthis balloon where we cannot enforce rich text. */
    whatsThisLabel->setTextFormat (Qt::RichText);
#endif

    whatsThisLabel->setMaxHeightMode (true);
    whatsThisLabel->setFocusPolicy (QWidget::NoFocus);
    whatsThisLabel->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    whatsThisLabel->setBackgroundMode (QLabel::PaletteMidlight);
    whatsThisLabel->setFrameShape (QLabel::Box);
    whatsThisLabel->setFrameShadow (QLabel::Sunken);
    whatsThisLabel->setMargin (7);
    whatsThisLabel->setScaledContents (FALSE);
    whatsThisLabel->setAlignment (int (QLabel::WordBreak |
                                       QLabel::AlignJustify |
                                       QLabel::AlignTop));

    whatsThisLabel->setFixedHeight (whatsThisLabel->frameWidth() * 2 +
                                    6 /* seems that RichText adds some margin */ +
                                    whatsThisLabel->fontMetrics().lineSpacing() * 4);
    whatsThisLabel->setMinimumWidth (whatsThisLabel->frameWidth() * 2 +
                                     6 /* seems that RichText adds some margin */ +
                                     whatsThisLabel->fontMetrics().width ('m') * 40);

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

    leName->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    leRAM->setValidator (new QIntValidator (MinRAM, MaxRAM, this));
    leVRAM->setValidator (new QIntValidator (MinVRAM, MaxVRAM, this));

    wvalGeneral = new QIWidgetValidator (pagePath (pageGeneral), pageGeneral, this);
    connect (wvalGeneral, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT(enableOk (const QIWidgetValidator *)));

    tbSelectSavedStateFolder->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                               "select_file_dis_16px.png"));
    tbResetSavedStateFolder->setIconSet (VBoxGlobal::iconSet ("eraser_16px.png",
                                                              "eraser_disabled_16px.png"));

    teDescription->setTextFormat (Qt::PlainText);

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

    wvalHDD = new QIWidgetValidator (pagePath (pageHDD), pageHDD, this);
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

    wvalDVD = new QIWidgetValidator (pagePath (pageDVD), pageDVD, this);
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

    wvalFloppy = new QIWidgetValidator (pagePath (pageFloppy), pageFloppy, this);
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

#ifndef Q_WS_WIN
    gbInterfaceList->setHidden (true);
#endif
    /* setup tab widget */
    mNoInterfaces = tr ("<No suitable interfaces>");
    /* setup iconsets */
    pbHostAdd->setIconSet (VBoxGlobal::iconSet ("add_host_iface_16px.png",
                                                "add_host_iface_disabled_16px.png"));
    pbHostRemove->setIconSet (VBoxGlobal::iconSet ("remove_host_iface_16px.png",
                                                   "remove_host_iface_disabled_16px.png"));
    /* setup languages */
    QToolTip::add (pbHostAdd, tr ("Add"));
    QToolTip::add (pbHostRemove, tr ("Remove"));

    /* Serial Port Page */

    /* Parallel Port Page (currently disabled) */
    QListViewItem *item = listView->findItem ("#parallelPorts", listView_Link);
    if (item) item->setVisible (false);

    /* USB Page */

    connect (cbEnableUSBController, SIGNAL (toggled (bool)),
             this, SLOT (usbAdapterToggled (bool)));

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
    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->setup (VBoxUSBFilterSettings::MachineType);
    wstUSBFilters->addWidget (settings, 0);
    lvUSBFilters_currentChanged (NULL);

    /* setup iconsets -- qdesigner is not capable... */
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
    usbDevicesMenu = new VBoxUSBMenu (this);
    connect (usbDevicesMenu, SIGNAL(activated(int)), this, SLOT(menuAddUSBFilterFrom_activated(int)));
    mUSBFilterListModified = false;

    /* VRDP Page */

    QWhatsThis::add (static_cast <QWidget *> (grbVRDP->child ("qt_groupbox_checkbox")),
                     tr ("When checked, the VM will act as a Remote Desktop "
                         "Protocol (RDP) server, allowing remote clients to connect "
                         "and operate the VM (when it is running) "
                         "using a standard RDP client."));

    leVRDPPort->setValidator (new QIntValidator (0, 0xFFFF, this));
    leVRDPTimeout->setValidator (new QIntValidator (this));
    wvalVRDP = new QIWidgetValidator (pagePath (pageVRDP), pageVRDP, this);
    connect (wvalVRDP, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wvalVRDP, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    connect (grbVRDP, SIGNAL (toggled (bool)), wvalFloppy, SLOT (revalidate()));
    connect (leVRDPPort, SIGNAL (textChanged (const QString&)), wvalFloppy, SLOT (revalidate()));
    connect (leVRDPTimeout, SIGNAL (textChanged (const QString&)), wvalFloppy, SLOT (revalidate()));

    /* Shared Folders Page */

    QVBoxLayout* pageFoldersLayout = new QVBoxLayout (pageFolders, 0, 10, "pageFoldersLayout");
    mSharedFolders = new VBoxSharedFoldersSettings (pageFolders, "sharedFolders");
    mSharedFolders->setDialogType (VBoxSharedFoldersSettings::MachineType);
    pageFoldersLayout->addWidget (mSharedFolders);

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

    /* Boot-order table */
    tblBootOrder = new BootItemsList (groupBox12, "tblBootOrder");
    connect (tblBootOrder, SIGNAL (bootSequenceChanged()),
             this, SLOT (resetFirstRunFlag()));

    /* Fixing focus order for BootItemsList */
    setTabOrder (tbwGeneral, tblBootOrder);
    setTabOrder (tblBootOrder->focusProxy(), chbEnableACPI);
    groupBox12Layout->addWidget (tblBootOrder);
    tblBootOrder->fixTabStops();
    /* Shared Clipboard mode */
    cbSharedClipboard->insertItem (vboxGlobal().toString (KClipboardMode_Disabled));
    cbSharedClipboard->insertItem (vboxGlobal().toString (KClipboardMode_HostToGuest));
    cbSharedClipboard->insertItem (vboxGlobal().toString (KClipboardMode_GuestToHost));
    cbSharedClipboard->insertItem (vboxGlobal().toString (KClipboardMode_Bidirectional));
    /* IDE Controller Type */
    cbIdeController->insertItem (vboxGlobal().toString (KIDEControllerType_PIIX3));
    cbIdeController->insertItem (vboxGlobal().toString (KIDEControllerType_PIIX4));

    /* HDD Images page */

    /* CD-ROM Drive Page */

    /* Audio Page */

    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_Null));
#if defined Q_WS_WIN32
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_DirectSound));
# ifdef VBOX_WITH_WINMM
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_WinMM));
# endif
#elif defined Q_OS_LINUX
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_OSS));
# ifdef VBOX_WITH_ALSA
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_ALSA));
# endif
# ifdef VBOX_WITH_PULSE
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_Pulse));
# endif
#elif defined Q_OS_MACX
    cbAudioDriver->insertItem (vboxGlobal().toString (KAudioDriverType_CoreAudio));
#endif

    cbAudioController->insertItem (vboxGlobal().toString (KAudioControllerType_AC97));
    cbAudioController->insertItem (vboxGlobal().toString (KAudioControllerType_SB16));

    /* Network Page */

    loadInterfacesList();
    loadNetworksList();

    /*
     *  update the Ok button state for pages with validation
     *  (validityChanged() connected to enableNext() will do the job)
     */
    wvalGeneral->revalidate();
    wvalHDD->revalidate();
    wvalDVD->revalidate();
    wvalFloppy->revalidate();

    /* VRDP Page */

    cbVRDPAuthType->insertItem (vboxGlobal().toString (KVRDPAuthType_Null));
    cbVRDPAuthType->insertItem (vboxGlobal().toString (KVRDPAuthType_External));
    cbVRDPAuthType->insertItem (vboxGlobal().toString (KVRDPAuthType_Guest));
}

/**
 *  Returns a path to the given page of this settings dialog. See ::path() for
 *  details.
 */
QString VBoxVMSettingsDlg::pagePath (QWidget *aPage)
{
    QListViewItem *li = listView->
        findItem (QString::number (widgetStack->id (aPage)), 1);
    return ::path (li);
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
            tblBootOrder->processFocusIn (widget);
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

    /* update geometry for the dynamically added usb-page to ensure proper
     * sizeHint calculation by the Qt layout manager */
    wstUSBFilters->updateGeometry();
    /* let our toplevel widget calculate its sizeHint properly */
    QApplication::sendPostedEvents (0, 0);

    layout()->activate();

    /* resize to the miminum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
}

void VBoxVMSettingsDlg::updateShortcuts()
{
    /* setup necessary combobox item */
    cbHDA->setCurrentItem (uuidHDA);
    cbHDB->setCurrentItem (uuidHDB);
    cbHDD->setCurrentItem (uuidHDD);
    cbISODVD->setCurrentItem (uuidISODVD);
    cbISOFloppy->setCurrentItem (uuidISOFloppy);
    /* check if the enumeration process has been started yet */
    if (!vboxGlobal().isMediaEnumerationStarted())
        vboxGlobal().startEnumeratingMedia();
    else
    {
        cbHDA->refresh();
        cbHDB->refresh();
        cbHDD->refresh();
        cbISODVD->refresh();
        cbISOFloppy->refresh();
    }
}

void VBoxVMSettingsDlg::loadInterfacesList()
{
#if defined Q_WS_WIN
    /* clear inner list */
    mInterfaceList.clear();
    /* load current inner list */
    CHostNetworkInterfaceEnumerator en =
        vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces().Enumerate();
    while (en.HasMore())
        mInterfaceList += en.GetNext().GetName();
    /* save current list item name */
    QString currentListItemName = lbHostInterface->currentText();
    /* load current list items */
    lbHostInterface->clear();
    if (mInterfaceList.count())
        lbHostInterface->insertStringList (mInterfaceList);
    else
        lbHostInterface->insertItem (mNoInterfaces);
    /* select current list item */
    int index = lbHostInterface->index (
        lbHostInterface->findItem (currentListItemName));
    if (index == -1)
        index = 0;
    lbHostInterface->setCurrentItem (index);
    lbHostInterface->setSelected (index, true);
    /* enable/disable interface delete button */
    pbHostRemove->setEnabled (!mInterfaceList.isEmpty());
#endif
}

void VBoxVMSettingsDlg::loadNetworksList()
{
    /* clear inner list */
    mNetworksList.clear();

    /* load internal network list */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
    CMachineVector vec =  vbox.GetMachines2();
    for (CMachineVector::ConstIterator m = vec.begin();
         m != vec.end(); ++ m)
    {
        if (m->GetAccessible())
        {
            for (ulong slot = 0; slot < count; ++ slot)
            {
                CNetworkAdapter adapter = m->GetNetworkAdapter (slot);
                if (adapter.GetAttachmentType() == KNetworkAttachmentType_Internal &&
                        !mNetworksList.contains (adapter.GetInternalNetwork()))
                    mNetworksList << adapter.GetInternalNetwork();
            }
        }
    }

    mLockNetworkListUpdate = false;
}

void VBoxVMSettingsDlg::hostInterfaceAdd()
{
#if defined Q_WS_WIN

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* search for the max available interface index */
    int ifaceNumber = 0;
    QString ifaceName = tr ("VirtualBox Host Interface %1");
    QRegExp regExp (QString ("^") + ifaceName.arg ("([0-9]+)") + QString ("$"));
    for (uint index = 0; index < lbHostInterface->count(); ++ index)
    {
        QString iface = lbHostInterface->text (index);
        int pos = regExp.search (iface);
        if (pos != -1)
            ifaceNumber = regExp.cap (1).toInt() > ifaceNumber ?
                          regExp.cap (1).toInt() : ifaceNumber;
    }

    /* creating add host interface dialog */
    VBoxAddNIDialog dlg (this, ifaceName.arg (++ ifaceNumber));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString iName = dlg.getName();

    /* create interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace;
    CProgress progress = host.CreateHostNetworkInterface (iName, iFace);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress, iName, this);
        if (progress.GetResultCode() == 0)
        {
            /* add&select newly created interface */
            delete lbHostInterface->findItem (mNoInterfaces);
            lbHostInterface->insertItem (iName);
            mInterfaceList += iName;
            lbHostInterface->setCurrentItem (lbHostInterface->count() - 1);
            lbHostInterface->setSelected (lbHostInterface->count() - 1, true);
            for (int index = 0; index < tbwNetwork->count(); ++ index)
                networkPageUpdate (tbwNetwork->page (index));
            /* enable interface delete button */
            pbHostRemove->setEnabled (true);
        }
        else
            vboxProblem().cannotCreateHostInterface (progress, iName, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, iName, this);

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

#endif
}

void VBoxVMSettingsDlg::hostInterfaceRemove()
{
#if defined Q_WS_WIN

    /* allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* check interface name */
    QString iName = lbHostInterface->currentText();
    if (iName.isEmpty())
        return;

    /* asking user about deleting selected network interface */
    int delNetIface = vboxProblem().message (this, VBoxProblemReporter::Question,
        tr ("<p>Do you want to remove the selected host network interface "
            "<nobr><b>%1</b>?</nobr></p>"
            "<p><b>Note:</b> This interface may be in use by one or more "
            "network adapters of this or another VM. After it is removed, these "
            "adapters will no longer work until you correct their settings by "
            "either choosing a different interface name or a different adapter "
            "attachment type.</p>").arg (iName),
        0, /* autoConfirmId */
        QIMessageBox::Ok | QIMessageBox::Default,
        QIMessageBox::Cancel | QIMessageBox::Escape);
    if (delNetIface == QIMessageBox::Cancel)
        return;

    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iFace = host.GetNetworkInterfaces().FindByName (iName);
    if (host.isOk())
    {
        /* delete interface */
        CProgress progress = host.RemoveHostNetworkInterface (iFace.GetId(), iFace);
        if (host.isOk())
        {
            vboxProblem().showModalProgressDialog (progress, iName, this);
            if (progress.GetResultCode() == 0)
            {
                if (lbHostInterface->count() == 1)
                {
                    lbHostInterface->insertItem (mNoInterfaces);
                    /* disable interface delete button */
                    pbHostRemove->setEnabled (false);
                }
                delete lbHostInterface->findItem (iName);
                lbHostInterface->setSelected (lbHostInterface->currentItem(), true);
                mInterfaceList.erase (mInterfaceList.find (iName));
                for (int index = 0; index < tbwNetwork->count(); ++ index)
                    networkPageUpdate (tbwNetwork->page (index));
            }
            else
                vboxProblem().cannotRemoveHostInterface (progress, iFace, this);
        }
    }

    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iFace, this);
#endif
}

void VBoxVMSettingsDlg::networkPageUpdate (QWidget *aWidget)
{
    if (!aWidget) return;
#if defined Q_WS_WIN
    VBoxVMNetworkSettings *set = static_cast<VBoxVMNetworkSettings*> (aWidget);
    set->loadInterfaceList (mInterfaceList, mNoInterfaces);
    set->revalidate();
#endif
}


void VBoxVMSettingsDlg::onMediaEnumerationDone()
{
    mAllowResetFirstRunFlag = true;
}


void VBoxVMSettingsDlg::resetFirstRunFlag()
{
    if (mAllowResetFirstRunFlag)
        mResetFirstRunFlag = true;
}


void VBoxVMSettingsDlg::hdaMediaChanged()
{
    resetFirstRunFlag();
    uuidHDA = grbHDA->isChecked() ? cbHDA->getId() : QUuid();
    txHDA->setText (getHdInfo (grbHDA, uuidHDA));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::hdbMediaChanged()
{
    resetFirstRunFlag();
    uuidHDB = grbHDB->isChecked() ? cbHDB->getId() : QUuid();
    txHDB->setText (getHdInfo (grbHDB, uuidHDB));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::hddMediaChanged()
{
    resetFirstRunFlag();
    uuidHDD = grbHDD->isChecked() ? cbHDD->getId() : QUuid();
    txHDD->setText (getHdInfo (grbHDD, uuidHDD));
    /* revailidate */
    wvalHDD->revalidate();
}


void VBoxVMSettingsDlg::cdMediaChanged()
{
    resetFirstRunFlag();
    uuidISODVD = bgDVD->isChecked() ? cbISODVD->getId() : QUuid();
    /* revailidate */
    wvalDVD->revalidate();
}


void VBoxVMSettingsDlg::fdMediaChanged()
{
    resetFirstRunFlag();
    uuidISOFloppy = bgFloppy->isChecked() ? cbISOFloppy->getId() : QUuid();
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
 *  If @a aCategory is non-null, it should be one of values from the hidden
 *  '[cat]' column of #listView (see VBoxVMSettingsDlg.ui in qdesigner)
 *  prepended with the '#' sign. In this case, the specified category page
 *  will be activated when the dialog is open.
 *
 *  If @a aWidget is non-null, it should be a name of one of widgets
 *  from the given category page. In this case, the specified widget
 *  will get focus when the dialog is open.
 *
 *  @note Calling this method after the dialog is open has no sense.
 *
 *  @param  aCategory   Category to select when the dialog is open or null.
 *  @param  aWidget     Category to select when the dialog is open or null.
 */
void VBoxVMSettingsDlg::setup (const QString &aCategory, const QString &aControl)
{
    if (!aCategory.isNull())
    {
        /* search for a list view item corresponding to the category */
        QListViewItem *item = listView->findItem (aCategory, listView_Link);
        if (item)
        {
            listView->setSelected (item, true);

            /* search for a widget with the given name */
            if (!aControl.isNull())
            {
                QObject *obj = widgetStack->visibleWidget()->child (aControl);
                if (obj && obj->isWidgetType())
                {
                    QWidget *w = static_cast <QWidget *> (obj);
                    QWidgetList parents;
                    QWidget *p = w;
                    while ((p = p->parentWidget()) != NULL)
                    {
                        if (!strcmp (p->className(), "QTabWidget"))
                        {
                            /* the tab contents widget is two steps down
                             * (QTabWidget -> QWidgetStack -> QWidget) */
                            QWidget *c = parents.last();
                            if (c)
                                c = parents.prev();
                            if (c)
                                static_cast <QTabWidget *> (p)->showPage (c);
                        }
                        parents.append (p);
                    }

                    w->setFocus();
                }
            }
        }
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


void VBoxVMSettingsDlg::enableOk (const QIWidgetValidator *wval)
{
    Q_UNUSED (wval);

    /* reset the warning text; interested parties will set it during
     * validation */
    setWarning (QString::null);

    QString wvalWarning;

    /* detect the overall validity */
    bool newValid = true;
    {
        QObjectList *l = this->queryList ("QIWidgetValidator");
        QObjectListIt it (*l);
        QObject *obj;
        while ((obj = it.current()) != 0)
        {
            QIWidgetValidator *wval = (QIWidgetValidator *) obj;
            newValid = wval->isValid();
            if (!newValid)
            {
                wvalWarning = wval->warningText();
                break;
            }
            ++ it;
        }
        delete l;
    }

    if (warningString.isNull() && !wvalWarning.isNull())
    {
        /* try to set the generic error message when invalid but no specific
         * message is provided */
        setWarning (wvalWarning);
    }

    if (valid != newValid)
    {
        valid = newValid;
        buttonOk->setEnabled (valid);
        warningLabel->setHidden (valid);
        warningPixmap->setHidden (valid);
    }
}


void VBoxVMSettingsDlg::revalidate (QIWidgetValidator *wval)
{
    /* do individual validations for pages */
    QWidget *pg = wval->widget();
    bool valid = wval->isOtherValid();

    QString warningText;
    QString pageTitle = pagePath (pg);

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
                warningText = tr ("Primary Master hard disk is not selected");
            }
            else uuids << uuidHDA;
        }

        if (valid && grbHDB->isChecked())
        {
            if (uuidHDB.isNull())
            {
                valid = false;
                warningText = tr ("Primary Slave hard disk is not selected");
            }
            else
            {
                bool found = uuids.findIndex (uuidHDB) >= 0;
                if (found)
                {
                    CHardDisk hd = vbox.GetHardDisk (uuidHDB);
                    valid = hd.GetType() == KHardDiskType_Immutable;
                }
                if (valid)
                    uuids << uuidHDB;
                else
                    warningText = tr ("Primary Slave hard disk is already attached "
                                      "to a different slot");
            }
        }

        if (valid && grbHDD->isChecked())
        {
            if (uuidHDD.isNull())
            {
                valid = false;
                warningText = tr ("Secondary Slave hard disk is not selected");
            }
            else
            {
                bool found = uuids.findIndex (uuidHDD) >= 0;
                if (found)
                {
                    CHardDisk hd = vbox.GetHardDisk (uuidHDD);
                    valid = hd.GetType() == KHardDiskType_Immutable;
                }
                if (valid)
                    uuids << uuidHDB;
                else
                    warningText = tr ("Secondary Slave hard disk is already attached "
                                      "to a different slot");
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
        cbPassthrough->setEnabled (rbHostDVD->isChecked());

        cbISODVD->setEnabled (rbISODVD->isChecked());
        tbISODVD->setEnabled (rbISODVD->isChecked());

        if (!valid)
            warningText = tr ("CD/DVD image file is not selected");
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
            warningText = tr ("Floppy image file is not selected");
    }
    else if (pg == pageNetwork)
    {
        QWidget *tab = NULL;
        VBoxVMNetworkSettings::CheckPageResult error =
                VBoxVMNetworkSettings::CheckPage_Ok;
        for (int index = 0; index < tbwNetwork->count(); ++ index)
        {
            tab = tbwNetwork->page (index);
            VBoxVMNetworkSettings *page =
                static_cast <VBoxVMNetworkSettings *> (tab);
            error = page->checkPage (mInterfaceList);
            valid = !error;
            if (!valid) break;
        }
        if (!valid)
        {
            Assert (tab);
            warningText =
                error == VBoxVMNetworkSettings::CheckPage_InvalidInterface ?
                    tr ("Incorrect host network interface is selected") :
                error == VBoxVMNetworkSettings::CheckPage_NoNetworkName ?
                    tr ("Internal network name is not set") :
                QString::null;
            pageTitle += ": " + tbwNetwork->tabLabel (tab);
        }
    }
    else if (pg == pageSerial)
    {
        valid = true;
        QValueList <QString> ports;
        QValueList <QString> paths;

        int index = 0;
        for (; index < tbwSerialPorts->count(); ++ index)
        {
            QWidget *tab = tbwSerialPorts->page (index);
            VBoxVMSerialPortSettings *page =
                static_cast <VBoxVMSerialPortSettings *> (tab);

            /* check the predefined port number unicity */
            if (page->mSerialPortBox->isChecked() && !page->isUserDefined())
            {
                QString port = page->mPortNumCombo->currentText();
                valid = !ports.contains (port);
                if (!valid)
                {
                    warningText = tr ("Duplicate port number is selected ");
                    pageTitle += ": " + tbwSerialPorts->tabLabel (tab);
                    break;
                }
                ports << port;
            }
            /* check the port path emptiness & unicity */
            KPortMode mode =
                vboxGlobal().toPortMode (page->mHostModeCombo->currentText());
            if (mode != KPortMode_Disconnected)
            {
                QString path = page->mPortPathLine->text();
                valid = !path.isEmpty() && !paths.contains (path);
                if (!valid)
                {
                    warningText = path.isEmpty() ?
                        tr ("Port path is not specified ") :
                        tr ("Duplicate port path is entered ");
                    pageTitle += ": " + tbwSerialPorts->tabLabel (tab);
                    break;
                }
                paths << path;
            }
        }
    }
    else if (pg == pageParallel)
    {
        valid = true;
        QValueList <QString> ports;
        QValueList <QString> paths;

        int index = 0;
        for (; index < tbwParallelPorts->count(); ++ index)
        {
            QWidget *tab = tbwParallelPorts->page (index);
            VBoxVMParallelPortSettings *page =
                static_cast <VBoxVMParallelPortSettings *> (tab);

            /* check the predefined port number unicity */
            if (page->mParallelPortBox->isChecked() && !page->isUserDefined())
            {
                QString port = page->mPortNumCombo->currentText();
                valid = !ports.contains (port);
                if (!valid)
                {
                    warningText = tr ("Duplicate port number is selected ");
                    pageTitle += ": " + tbwParallelPorts->tabLabel (tab);
                    break;
                }
                ports << port;
            }
            /* check the port path emptiness & unicity */
            if (page->mParallelPortBox->isChecked())
            {
                QString path = page->mPortPathLine->text();
                valid = !path.isEmpty() && !paths.contains (path);
                if (!valid)
                {
                    warningText = path.isEmpty() ?
                        tr ("Port path is not specified ") :
                        tr ("Duplicate port path is entered ");
                    pageTitle += ": " + tbwParallelPorts->tabLabel (tab);
                    break;
                }
                paths << path;
            }
        }
    }

    if (!valid)
        setWarning (tr ("%1 on the <b>%2</b> page.")
                    .arg (warningText, pageTitle));

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
    QString typeId = machine.GetOSTypeId();
    cbOS->setCurrentItem (vboxGlobal().vmGuestOSTypeIndex (typeId));
    cbOS_activated (cbOS->currentItem());

    /* RAM size */
    slRAM->setValue (machine.GetMemorySize());

    /* VRAM size */
    slVRAM->setValue (machine.GetVRAMSize());

    /* Boot-order */
    tblBootOrder->getFromMachine (machine);

    /* ACPI */
    chbEnableACPI->setChecked (biosSettings.GetACPIEnabled());

    /* IO APIC */
    chbEnableIOAPIC->setChecked (biosSettings.GetIOAPICEnabled());

    /* VT-x/AMD-V */
    machine.GetHWVirtExEnabled() == KTSBool_False ? chbVTX->setChecked (false) :
    machine.GetHWVirtExEnabled() == KTSBool_True ?  chbVTX->setChecked (true) :
                                                    chbVTX->setNoChange();

    /* Saved state folder */
    leSnapshotFolder->setText (machine.GetSnapshotFolder());

    /* Description */
    teDescription->setText (machine.GetDescription());

    /* Shared clipboard mode */
    cbSharedClipboard->setCurrentItem (machine.GetClipboardMode());

    /* IDE controller type */
    cbIdeController->setCurrentText (vboxGlobal().toString (biosSettings.GetIDEControllerType()));

    /* other features */
    QString saveRtimeImages = cmachine.GetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime);
    chbRememberMedia->setChecked (saveRtimeImages != "no");

    /* hard disk images */
    {
        struct
        {
            KStorageBus bus;
            LONG channel;
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
            { KStorageBus_IDE, 0, 0, {grbHDA, cbHDA, txHDA, &uuidHDA} },
            { KStorageBus_IDE, 0, 1, {grbHDB, cbHDB, txHDB, &uuidHDB} },
            { KStorageBus_IDE, 1, 1, {grbHDD, cbHDD, txHDD, &uuidHDD} },
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
                if (diskSet [i].bus == hda.GetBus() &&
                    diskSet [i].channel == hda.GetChannel() &&
                    diskSet [i].dev == hda.GetDevice())
                {
                    CHardDisk hd = hda.GetHardDisk();
                    CHardDisk root = hd.GetRoot();
                    QString src = root.GetLocation();
                    if (hd.GetStorageType() == KHardDiskStorageType_VirtualDiskImage)
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
            QString name = hostFloppy.GetName();
            QString description = hostFloppy.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            cbHostFloppy->insertItem (fullName, id);
            hostFloppies [id] = hostFloppy;
            ++ id;
        }

        CFloppyDrive floppy = machine.GetFloppyDrive();
        switch (floppy.GetState())
        {
            case KDriveState_HostDriveCaptured:
            {
                CHostFloppyDrive drv = floppy.GetHostDrive();
                QString name = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    name :
                    QString ("%1 (%2)").arg (description, name);
                if (coll.FindByName (name).isNull())
                {
                    /*
                     *  if the floppy drive is not currently available,
                     *  add it to the end of the list with a special mark
                     */
                    cbHostFloppy->insertItem ("* " + fullName);
                    cbHostFloppy->setCurrentItem (cbHostFloppy->count() - 1);
                }
                else
                {
                    /* this will select the correct item from the prepared list */
                    cbHostFloppy->setCurrentText (fullName);
                }
                rbHostFloppy->setChecked (true);
                break;
            }
            case KDriveState_ImageMounted:
            {
                CFloppyImage img = floppy.GetImage();
                QString src = img.GetFilePath();
                AssertMsg (!src.isNull(), ("Image file must not be null"));
                QFileInfo fi (src);
                rbISOFloppy->setChecked (true);
                uuidISOFloppy = QUuid (img.GetId());
                break;
            }
            case KDriveState_NotMounted:
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
            QString name = hostDVD.GetName();
            QString description = hostDVD.GetDescription();
            QString fullName = description.isEmpty() ?
                name :
                QString ("%1 (%2)").arg (description, name);
            cbHostDVD->insertItem (fullName, id);
            hostDVDs [id] = hostDVD;
            ++ id;
        }

        CDVDDrive dvd = machine.GetDVDDrive();
        switch (dvd.GetState())
        {
            case KDriveState_HostDriveCaptured:
            {
                CHostDVDDrive drv = dvd.GetHostDrive();
                QString name = drv.GetName();
                QString description = drv.GetDescription();
                QString fullName = description.isEmpty() ?
                    name :
                    QString ("%1 (%2)").arg (description, name);
                if (coll.FindByName (name).isNull())
                {
                    /*
                     *  if the DVD drive is not currently available,
                     *  add it to the end of the list with a special mark
                     */
                    cbHostDVD->insertItem ("* " + fullName);
                    cbHostDVD->setCurrentItem (cbHostDVD->count() - 1);
                }
                else
                {
                    /* this will select the correct item from the prepared list */
                    cbHostDVD->setCurrentText (fullName);
                }
                rbHostDVD->setChecked (true);
                cbPassthrough->setChecked (dvd.GetPassthrough());
                break;
            }
            case KDriveState_ImageMounted:
            {
                CDVDImage img = dvd.GetImage();
                QString src = img.GetFilePath();
                AssertMsg (!src.isNull(), ("Image file must not be null"));
                QFileInfo fi (src);
                rbISODVD->setChecked (true);
                uuidISODVD = QUuid (img.GetId());
                break;
            }
            case KDriveState_NotMounted:
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
        cbAudioController->setCurrentText (vboxGlobal().toString (audio.GetAudioController()));
    }

    /* network */
    {
        ulong count = vbox.GetSystemProperties().GetNetworkAdapterCount();
        for (ulong slot = 0; slot < count; ++ slot)
        {
            CNetworkAdapter adapter = machine.GetNetworkAdapter (slot);
            addNetworkAdapter (adapter);
        }
    }

    /* serial ports */
    {
        ulong count = vbox.GetSystemProperties().GetSerialPortCount();
        for (ulong slot = 0; slot < count; ++ slot)
        {
            CSerialPort port = machine.GetSerialPort (slot);
            addSerialPort (port);
        }
    }

    /* parallel ports */
    {
        ulong count = vbox.GetSystemProperties().GetParallelPortCount();
        for (ulong slot = 0; slot < count; ++ slot)
        {
            CParallelPort port = machine.GetParallelPort (slot);
            addParallelPort (port);
        }
    }

    /* USB */
    {
        CUSBController ctl = machine.GetUSBController();

        /* Show an error message (if there is any).
         * Note that we don't use the generic cannotLoadMachineSettings()
         * call here because we want this message to be suppressable. */
        if (!machine.isReallyOk())
            vboxProblem().cannotAccessUSB (machine);

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
        }
        else
        {
            cbEnableUSBController->setChecked (ctl.GetEnabled());
            cbEnableUSBEhci->setChecked (ctl.GetEnabledEhci());
            usbAdapterToggled (cbEnableUSBController->isChecked());

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

    /* shared folders */
    {
        mSharedFolders->getFromMachine (machine);
    }

    /* request for media shortcuts update */
    cbHDA->setBelongsTo (machine.GetId());
    cbHDB->setBelongsTo (machine.GetId());
    cbHDD->setBelongsTo (machine.GetId());
    updateShortcuts();

    /* revalidate pages with custom validation */
    wvalHDD->revalidate();
    wvalDVD->revalidate();
    wvalFloppy->revalidate();
    wvalVRDP->revalidate();

    /* finally set the reset First Run Wizard flag to "false" to make sure
     * user will see this dialog if he hasn't change the boot-order
     * and/or mounted images configuration */
    mResetFirstRunFlag = false;
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
    cmachine.SetOSTypeId (type.GetId());

    /* RAM size */
    cmachine.SetMemorySize (slRAM->value());

    /* VRAM size */
    cmachine.SetVRAMSize (slVRAM->value());

    /* boot order */
    tblBootOrder->putBackToMachine (cmachine);

    /* ACPI */
    biosSettings.SetACPIEnabled (chbEnableACPI->isChecked());

    /* IO APIC */
    biosSettings.SetIOAPICEnabled (chbEnableIOAPIC->isChecked());

    /* VT-x/AMD-V */
    cmachine.SetHWVirtExEnabled (
        chbVTX->state() == QButton::Off ? KTSBool_False :
        chbVTX->state() == QButton::On ? KTSBool_True : KTSBool_Default);

    /* Saved state folder */
    if (leSnapshotFolder->isModified())
    {
        cmachine.SetSnapshotFolder (leSnapshotFolder->text());
        if (!cmachine.isOk())
            vboxProblem()
                .cannotSetSnapshotFolder (cmachine,
                    QDir::convertSeparators (leSnapshotFolder->text()));
    }

    /* Description (set empty to null to avoid an empty <Description> node
     * in the settings file) */
    cmachine.SetDescription (teDescription->text().isEmpty() ? QString::null :
                             teDescription->text());

    /* Shared clipboard mode */
    cmachine.SetClipboardMode ((KClipboardMode) cbSharedClipboard->currentItem());

    /* IDE controller type */
    biosSettings.SetIDEControllerType (vboxGlobal().toIDEControllerType (cbIdeController->currentText()));

    /* other features */
    cmachine.SetExtraData (VBoxDefs::GUI_SaveMountedAtRuntime,
                           chbRememberMedia->isChecked() ? "yes" : "no");

    /* hard disk images */
    {
        struct
        {
            KStorageBus bus;
            LONG channel;
            LONG dev;
            struct {
                QGroupBox *grb;
                QUuid *uuid;
            } data;
        }
        diskSet[] =
        {
            { KStorageBus_IDE, 0, 0, {grbHDA, &uuidHDA} },
            { KStorageBus_IDE, 0, 1, {grbHDB, &uuidHDB} },
            { KStorageBus_IDE, 1, 1, {grbHDD, &uuidHDD} }
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
                if (diskSet [i].bus == hda.GetBus() &&
                    diskSet [i].channel == hda.GetChannel() &&
                    diskSet [i].dev == hda.GetDevice())
                {
                    cmachine.DetachHardDisk (diskSet [i].bus, diskSet [i].channel, diskSet [i].dev);
                    if (!cmachine.isOk())
                        vboxProblem().cannotDetachHardDisk (
                            this, cmachine, diskSet [i].bus, diskSet [i].channel, diskSet [i].dev);
                }
            }
        }

        /* now, attach new disks */
        for (uint i = 0; i < SIZEOF_ARRAY (diskSet); i++)
        {
            QUuid *newId = diskSet [i].data.uuid;
            if (diskSet [i].data.grb->isChecked() && !(*newId).isNull())
            {
                cmachine.AttachHardDisk (*newId, diskSet [i].bus, diskSet [i].channel, diskSet [i].dev);
                if (!cmachine.isOk())
                    vboxProblem().cannotAttachHardDisk (
                        this, cmachine, *newId, diskSet [i].bus, diskSet [i].channel, diskSet [i].dev);
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
            dvd.SetPassthrough (false);
            dvd.Unmount();
        }
        else if (rbHostDVD->isChecked())
        {
            dvd.SetPassthrough (cbPassthrough->isChecked());
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
            dvd.SetPassthrough (false);
            Assert (!uuidISODVD.isNull());
            dvd.MountImage (uuidISODVD);
        }
    }

    /* Clear the "GUI_FirstRun" extra data key in case if the boot order
     * and/or disk configuration were changed */
    if (mResetFirstRunFlag)
        cmachine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);

    /* audio */
    {
        CAudioAdapter audio = cmachine.GetAudioAdapter();
        audio.SetAudioDriver (vboxGlobal().toAudioDriverType (cbAudioDriver->currentText()));
        audio.SetAudioController (vboxGlobal().toAudioControllerType (cbAudioController->currentText()));
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

    /* serial ports */
    {
        for (int index = 0; index < tbwSerialPorts->count(); index++)
        {
            VBoxVMSerialPortSettings *page =
                (VBoxVMSerialPortSettings *) tbwSerialPorts->page (index);
            Assert (page);
            page->putBackToPort();
        }
    }

    /* parallel ports */
    {
        for (int index = 0; index < tbwParallelPorts->count(); index++)
        {
            VBoxVMParallelPortSettings *page =
                (VBoxVMParallelPortSettings *) tbwParallelPorts->page (index);
            Assert (page);
            page->putBackToPort();
        }
    }

    /* usb */
    {
        CUSBController ctl = cmachine.GetUSBController();

        if (!ctl.isNull())
        {
            /* the USB controller may be unavailable (i.e. in VirtualBox OSE) */

            ctl.SetEnabled (cbEnableUSBController->isChecked());
            ctl.SetEnabledEhci (cbEnableUSBEhci->isChecked());

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

    /* shared folders */
    {
        mSharedFolders->putBackToMachine();
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
    QUuid hdId = type == VBoxDefs::HD ? cbb->getId() : QUuid();
    QUuid cdId = type == VBoxDefs::CD ? cbb->getId() : QUuid();
    QUuid fdId = type == VBoxDefs::FD ? cbb->getId() : QUuid();
    dlg.setup (type, true, &machineId, true /* aRefresh */, cmachine,
               hdId, cdId, fdId);
    if (dlg.exec() == VBoxDiskImageManagerDlg::Accepted)
    {
        *id = dlg.getSelectedUuid();
        resetFirstRunFlag();
    }
    else
    {
        *id = cbb->getId();
    }

    cbb->setCurrentItem (*id);
    cbb->setFocus();

    /* revalidate pages with custom validation */
    wvalHDD->revalidate();
    wvalDVD->revalidate();
    wvalFloppy->revalidate();
}

void VBoxVMSettingsDlg::addNetworkAdapter (const CNetworkAdapter &aAdapter)
{
    VBoxVMNetworkSettings *page = new VBoxVMNetworkSettings();
    page->loadInterfaceList (mInterfaceList, mNoInterfaces);
    page->loadNetworksList (mNetworksList);
    page->getFromAdapter (aAdapter);
    QString pageTitle = QString (tr ("Adapter %1", "network"))
                                 .arg (aAdapter.GetSlot());
    tbwNetwork->addTab (page, pageTitle);

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (page->leTAPTerminate, buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    /* setup validation */
    QIWidgetValidator *wval =
        new QIWidgetValidator (QString ("%1: %2")
                               .arg (pagePath (pageNetwork), pageTitle),
                               pageNetwork, this);
    connect (page->grbEnabled, SIGNAL (toggled (bool)), wval, SLOT (revalidate()));
    connect (page->cbNetworkAttachment, SIGNAL (activated (const QString &)),
             wval, SLOT (revalidate()));
    connect (page->cbInternalNetworkName, SIGNAL (activated (const QString &)),
             wval, SLOT (revalidate()));
    connect (page->cbInternalNetworkName, SIGNAL (textChanged (const QString &)),
             this, SLOT (updateNetworksList()));
    connect (page->cbInternalNetworkName, SIGNAL (textChanged (const QString &)),
             wval, SLOT (revalidate()));
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate( QIWidgetValidator *)));

    page->setValidator (wval);
    page->revalidate();

#ifdef Q_WS_WIN

    /* fix focus order (make sure the Host Interface list UI goes after the
     * last network adapter UI item) */

    setTabOrder (page->chbCableConnected, lbHostInterface);
    setTabOrder (lbHostInterface, pbHostAdd);
    setTabOrder (pbHostAdd, pbHostRemove);

#endif
}

void VBoxVMSettingsDlg::updateNetworksList()
{
    if (mLockNetworkListUpdate)
        return;
    mLockNetworkListUpdate = true;

    QStringList curList (mNetworksList);
    for (int index = 0; index < tbwNetwork->count(); ++ index)
    {
        VBoxVMNetworkSettings *pg = tbwNetwork->page (index) ?
            static_cast <VBoxVMNetworkSettings*> (tbwNetwork->page (index)) : 0;
        if (pg)
        {
            QString curText = pg->cbInternalNetworkName->currentText();
            if (!curText.isEmpty() && !curList.contains (curText))
                curList << curText;
        }
    }

    for (int index = 0; index < tbwNetwork->count(); ++ index)
    {
        VBoxVMNetworkSettings *pg = tbwNetwork->page (index) ?
            static_cast <VBoxVMNetworkSettings*> (tbwNetwork->page (index)) : 0;
        pg->loadNetworksList (curList);
    }

    mLockNetworkListUpdate = false;
}

void VBoxVMSettingsDlg::addSerialPort (const CSerialPort &aPort)
{
    VBoxVMSerialPortSettings *page = new VBoxVMSerialPortSettings();
    page->getFromPort (aPort);
    QString pageTitle = QString (tr ("Port %1", "serial ports"))
                                 .arg (aPort.GetSlot());
    tbwSerialPorts->addTab (page, pageTitle);

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (page->mPortPathLine, buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    /* setup validation */
    QIWidgetValidator *wval =
        new QIWidgetValidator (QString ("%1: %2")
                               .arg (pagePath (pageSerial), pageTitle),
                               pageSerial, this);
    connect (page->mSerialPortBox, SIGNAL (toggled (bool)),
             wval, SLOT (revalidate()));
    connect (page->mIRQLine, SIGNAL (textChanged (const QString &)),
             wval, SLOT (revalidate()));
    connect (page->mIOPortLine, SIGNAL (textChanged (const QString &)),
             wval, SLOT (revalidate()));
    connect (page->mHostModeCombo, SIGNAL (activated (const QString &)),
             wval, SLOT (revalidate()));
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

    wval->revalidate();
}

void VBoxVMSettingsDlg::addParallelPort (const CParallelPort &aPort)
{
    VBoxVMParallelPortSettings *page = new VBoxVMParallelPortSettings();
    page->getFromPort (aPort);
    QString pageTitle = QString (tr ("Port %1", "parallel ports"))
                                 .arg (aPort.GetSlot());
    tbwParallelPorts->addTab (page, pageTitle);

    /* fix the tab order so that main dialog's buttons are always the last */
    setTabOrder (page->mPortPathLine, buttonHelp);
    setTabOrder (buttonHelp, buttonOk);
    setTabOrder (buttonOk, buttonCancel);

    /* setup validation */
    QIWidgetValidator *wval =
        new QIWidgetValidator (QString ("%1: %2")
                               .arg (pagePath (pageParallel), pageTitle),
                               pageParallel, this);
    connect (page->mParallelPortBox, SIGNAL (toggled (bool)),
             wval, SLOT (revalidate()));
    connect (page->mIRQLine, SIGNAL (textChanged (const QString &)),
             wval, SLOT (revalidate()));
    connect (page->mIOPortLine, SIGNAL (textChanged (const QString &)),
             wval, SLOT (revalidate()));
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));

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
    QString settingsFolder = VBoxGlobal::getFirstExistingDir (leSnapshotFolder->text());
    if (settingsFolder.isNull())
        settingsFolder = QFileInfo (cmachine.GetSettingsFilePath()).dirPath (true);

    QString folder = vboxGlobal().getExistingDirectory (settingsFolder, this);
    if (folder.isNull())
        return;

    folder = QDir::convertSeparators (folder);
    /* remove trailing slash if any */
    folder.remove (QRegExp ("[\\\\/]$"));

    /*
     *  do this instead of le->setText (folder) to cause
     *  isModified() return true
     */
    leSnapshotFolder->selectAll();
    leSnapshotFolder->insert (folder);
}

// USB Filter stuff
////////////////////////////////////////////////////////////////////////////////

void VBoxVMSettingsDlg::usbAdapterToggled (bool aOn)
{
    if (!aOn)
        cbEnableUSBEhci->setChecked (aOn);
    grbUSBFilters->setEnabled (aOn);
}

void VBoxVMSettingsDlg::addUSBFilter (const CUSBDeviceFilter &aFilter, bool isNew)
{
    QListViewItem *currentItem = isNew
        ? lvUSBFilters->currentItem()
        : lvUSBFilters->lastItem();

    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->setup (VBoxUSBFilterSettings::MachineType);
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

    QIWidgetValidator *wval =
        new QIWidgetValidator (pagePath (pageUSB), settings, settings);
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
    /* search for the max available filter index */
    int maxFilterIndex = 0;
    QString usbFilterName = tr ("New Filter %1", "usb");
    QRegExp regExp (QString ("^") + usbFilterName.arg ("([0-9]+)") + QString ("$"));
    QListViewItemIterator iterator (lvUSBFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text (lvUSBFilters_Name);
        int pos = regExp.search (filterName);
        if (pos != -1)
            maxFilterIndex = regExp.cap (1).toInt() > maxFilterIndex ?
                             regExp.cap (1).toInt() : maxFilterIndex;
        ++ iterator;
    }

    /* creating new usb filter */
    CUSBDeviceFilter filter = cmachine.GetUSBController()
        .CreateDeviceFilter (usbFilterName.arg (maxFilterIndex + 1));

    filter.SetActive (true);
    addUSBFilter (filter, true /* isNew */);

    mUSBFilterListModified = true;
}

void VBoxVMSettingsDlg::tbAddUSBFilterFrom_clicked()
{
    usbDevicesMenu->exec (QCursor::pos());
}

void VBoxVMSettingsDlg::menuAddUSBFilterFrom_activated (int aIndex)
{
    CUSBDevice usb = usbDevicesMenu->getUSB (aIndex);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    CUSBDeviceFilter filter = cmachine.GetUSBController()
        .CreateDeviceFilter (vboxGlobal().details (usb));

    filter.SetVendorId (QString().sprintf ("%04hX", usb.GetVendorId()));
    filter.SetProductId (QString().sprintf ("%04hX", usb.GetProductId()));
    filter.SetRevision (QString().sprintf ("%04hX", usb.GetRevision()));
    /* The port property depends on the host computer rather than on the USB
     * device itself; for this reason only a few people will want to use it in
     * the filter since the same device plugged into a different socket will
     * not match the filter in this case.  */
#if 0
    /// @todo set it anyway if Alt is currently pressed
    filter.SetPort (QString().sprintf ("%04hX", usb.GetPort()));
#endif
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

