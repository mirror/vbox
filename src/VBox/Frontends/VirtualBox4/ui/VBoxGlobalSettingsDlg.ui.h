/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Global settings" dialog UI include (Qt Designer)
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

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>
//Added by qt3to4:
#include <QTranslator>
#include <QLabel>
#include <q3mimefactory.h>
#include <QEvent>
#include <QShowEvent>
#include <Q3WhatsThis>

/* defined in VBoxGlobal.cpp */
extern const char *gVBoxLangSubDir;
extern const char *gVBoxLangFileBase;
extern const char *gVBoxLangFileExt;
extern const char *gVBoxLangIDRegExp;
extern const char *gVBoxBuiltInLangName;

/**
 *  Returns the path to the item in the form of 'grandparent > parent > item'
 *  using the text of the first column of every item.
 */
static QString path (Q3ListViewItem *li)
{
    static QString sep = ": ";
    QString p;
    Q3ListViewItem *cur = li;
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


class USBListItem : public Q3CheckListItem
{
public:

    USBListItem (Q3ListView *aParent, Q3ListViewItem *aAfter)
        : Q3CheckListItem (aParent, aAfter, QString::null, CheckBox)
        , mId (-1) {}

    int mId;
};
enum { lvUSBFilters_Name = 0 };


class LanguageItem : public Q3ListViewItem
{
public:

    enum { TypeId = 1001 };

    LanguageItem (Q3ListView *aParent, const QTranslator &aTranslator,
                  const QString &aId, bool aBuiltIn = false)
        : Q3ListViewItem (aParent), mBuiltIn (aBuiltIn), mInvalid (false)
    {
        Assert (!aId.isEmpty());

//        QTranslatorMessage transMes;

        /* Note: context/source/comment arguments below must match strings
         * used in VBoxGlobal::languageName() and friends (the latter are the
         * source of information for the lupdate tool that generates
         * translation files) */

        QString nativeLanguage = tratra (aTranslator,
            "@@@", "English", "Native language name");
        QString nativeCountry = tratra (aTranslator,
            "@@@", "--", "Native language country name "
            "(empty if this language is for all countries)");

        QString englishLanguage = tratra (aTranslator,
            "@@@", "English", "Language name, in English");
        QString englishCountry = tratra (aTranslator,
            "@@@", "--", "Language country name, in English "
            "(empty if native country name is empty)");

        QString translatorsName = tratra (aTranslator,
            "@@@", "innotek", "Comma-separated list of translators");

        QString itemName = nativeLanguage;
        QString langName = englishLanguage;

        if (!aBuiltIn)
        {
            if (nativeCountry != "--")
                itemName += " (" + nativeCountry + ")";

            if (englishCountry != "--")
                langName += " (" + englishCountry + ")";

            if (itemName != langName)
                langName = itemName + " / " + langName;
        }
        else
        {
            itemName += VBoxGlobalSettingsDlg::tr (" (built-in)", "Language");
            langName += VBoxGlobalSettingsDlg::tr (" (built-in)", "Language");
        }

        setText (0, itemName);
        setText (1, aId);
        setText (2, langName);
        setText (3, translatorsName);
    }

    /* Constructs an item for an invalid language ID (i.e. when a language
     * file is missing or corrupt). */
    LanguageItem (Q3ListView *aParent, const QString &aId)
        : Q3ListViewItem (aParent), mBuiltIn (false), mInvalid (true)
    {
        Assert (!aId.isEmpty());

        setText (0, QString ("<%1>").arg (aId));
        setText (1, aId);
        setText (2, VBoxGlobalSettingsDlg::tr ("<unavailable>", "Language"));
        setText (3, VBoxGlobalSettingsDlg::tr ("<unknown>", "Author(s)"));
    }

    /* Constructs an item for the default language ID (column 1 will be set
     * to QString::null) */
    LanguageItem (Q3ListView *aParent)
        : Q3ListViewItem (aParent), mBuiltIn (false), mInvalid (false)
    {
        setText (0, VBoxGlobalSettingsDlg::tr ("Default", "Language"));
        setText (1, QString::null);
        /* empty strings of some reasonable length to prevent the info part
         * from being shrinked too much when the list wants to be wider */
        setText (2, "                ");
        setText (3, "                ");
    }

    int rtti() const { return TypeId; }

    int compare (Q3ListViewItem *aItem, int aColumn, bool aAscending) const
    {
        QString thisId = text (1);
        QString thatId = aItem->text (1);
        if (thisId.isNull())
            return -1;
        if (thatId.isNull())
            return 1;
        if (mBuiltIn)
            return -1;
        if (aItem->rtti() == TypeId && ((LanguageItem *) aItem)->mBuiltIn)
            return 1;
        return Q3ListViewItem::compare (aItem, aColumn, aAscending);
    }

    void paintCell (QPainter *aPainter, const QColorGroup &aGroup,
                    int aColumn, int aWidth, int aAlign)
    {
        QFont font = aPainter->font();

        if (mInvalid)
            font.setItalic (true);
        /* mark the effectively active language */
        if (text (1) == VBoxGlobal::languageId())
            font.setBold (true);

        if (aPainter->font() != font)
            aPainter->setFont (font);

        Q3ListViewItem::paintCell (aPainter, aGroup, aColumn, aWidth, aAlign);

        if (mBuiltIn)
        {
            int y = height() - 1;
            aPainter->setPen (aGroup.mid());
            aPainter->drawLine (0, y, aWidth - 1, y);
        }
    }

    int width (const QFontMetrics &aFM, const Q3ListView *aLV, int aC) const
    {
        QFont font = aLV->font();

        if (mInvalid)
            font.setItalic (true);
        /* mark the effectively active language */
        if (text (1) == VBoxGlobal::languageId())
            font.setBold (true);

        QFontMetrics fm = aFM;
        if (aLV->font() != font)
            fm = QFontMetrics (font);

        return Q3ListViewItem::width (fm, aLV, aC);
    }

    void setup ()
    {
        Q3ListViewItem::setup();
        if (mBuiltIn)
            setHeight (height() + 1);
    }

private:

    QString tratra (const QTranslator &aTranslator, const char *aCtxt,
                       const char *aSrc, const char *aCmnt)
    {
#warning port me: check this
        QString msg = aTranslator.translate (aCtxt, aSrc, aCmnt);
//        QString msg = aTranslator.findMessage (aCtxt, aSrc, aCmnt).translation();
        /* return the source text if no translation is found */
        if (msg.isEmpty())
            msg = QString (aSrc);
        return msg;
    }

    bool mBuiltIn : 1;
    bool mInvalid : 1;
};


void VBoxGlobalSettingsDlg::init()
{
    polished = false;

    setIcon (QPixmap (":/global_settings_16px.png"));

    /*  all pages are initially valid */
    valid = true;
    buttonOk->setEnabled (true);
#warning port me
//    warningSpacer->changeSize (0, 0, QSizePolicy::Expanding);
    warningLabel->setHidden (true);
    warningPixmap->setHidden (true);

    /*  disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, listView);
    /*  hide the header and internal columns */
    listView->header()->hide();
    listView->setColumnWidthMode (listView_Id, Q3ListView::Manual);
    listView->setColumnWidthMode (listView_Link, Q3ListView::Manual);
    listView->hideColumn (listView_Id);
    listView->hideColumn (listView_Link);
    /*  sort by the id column (to have pages in the desired order) */
    listView->setSorting (listView_Id);
    listView->sort();

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

    whatsThisLabel = new QIRichLabel (this, "whatsThisLabel");
#warning port me
//    VBoxGlobalSettingsDlgLayout->addWidget (whatsThisLabel, 2, 1);

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
    whatsThisLabel->setFocusPolicy (Qt::NoFocus);
    whatsThisLabel->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
    whatsThisLabel->setBackgroundMode (Qt::PaletteMidlight);
    whatsThisLabel->setFrameShape (QLabel::Box);
    whatsThisLabel->setFrameShadow (QLabel::Sunken);
    whatsThisLabel->setMargin (7);
    whatsThisLabel->setScaledContents (FALSE);
    whatsThisLabel->setAlignment (int (Qt::TextWordWrap |
                                       Qt::AlignJustify |
                                       Qt::AlignTop));

    whatsThisLabel->setFixedHeight (whatsThisLabel->frameWidth() * 2 +
                                    6 /* seems that RichText adds some margin */ +
                                    whatsThisLabel->fontMetrics().lineSpacing() * 4);
    whatsThisLabel->setMinimumWidth (whatsThisLabel->frameWidth() * 2 +
                                     6 /* seems that RichText adds some margin */ +
                                     whatsThisLabel->fontMetrics().width ('m') * 40);

    /*
     *  create and layout non-standard widgets
     *  ----------------------------------------------------------------------
     */

    hkeHostKey = new QIHotKeyEdit (grbKeyboard, "hkeHostKey");
    hkeHostKey->setSizePolicy (QSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed));
    Q3WhatsThis::add (hkeHostKey,
        tr ("Displays the key used as a Host Key in the VM window. Activate the "
            "entry field and press a new Host Key. Note that alphanumeric, "
            "cursor movement and editing keys cannot be used as a Host Key."));
#warning port me
//    layoutHostKey->addWidget (hkeHostKey);
    txHostKey->setBuddy (hkeHostKey);
    setTabOrder (listView, hkeHostKey);

    /*
     *  setup connections and set validation for pages
     *  ----------------------------------------------------------------------
     */

    /* General page */

    wvalGeneral = new QIWidgetValidator (pagePath (pageGeneral), pageGeneral, this);
    connect (wvalGeneral, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk( const QIWidgetValidator *)));

    /* Keyboard page */

    wvalKeyboard = new QIWidgetValidator (pagePath (pageKeyboard), pageKeyboard, this);
    connect (wvalKeyboard, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk( const QIWidgetValidator *)));

    /* USB page */

    lvUSBFilters->header()->hide();
    /* disable sorting */
    lvUSBFilters->setSorting (-1);
    /* disable unselecting items by clicking in the unused area of the list */
    new QIListViewSelectionPreserver (this, lvUSBFilters);
    wstUSBFilters = new Q3WidgetStack (grbUSBFilters, "wstUSBFilters");
#warning port me
//    grbUSBFiltersLayout->addWidget (wstUSBFilters);
    /* create a default (disabled) filter settings widget at index 0 */
    VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
    settings->setup (VBoxUSBFilterSettings::HostType);
    wstUSBFilters->addWidget (settings, 0);
    lvUSBFilters_currentChanged (NULL);
    /* setup toolbutton icons */
    tbAddUSBFilter->setIconSet (VBoxGlobal::iconSet (":/usb_new_16px.png",
                                                     ":/usb_new_disabled_16px.png"));
    tbAddUSBFilterFrom->setIconSet (VBoxGlobal::iconSet (":/usb_add_16px.png",
                                                         ":/usb_add_disabled_16px.png"));
    tbRemoveUSBFilter->setIconSet (VBoxGlobal::iconSet (":/usb_remove_16px.png",
                                                        ":/usb_remove_disabled_16px.png"));
    tbUSBFilterUp->setIconSet (VBoxGlobal::iconSet (":/usb_moveup_16px.png",
                                                    ":/usb_moveup_disabled_16px.png"));
    tbUSBFilterDown->setIconSet (VBoxGlobal::iconSet (":/usb_movedown_16px.png",
                                                      ":/usb_movedown_disabled_16px.png"));
    /* create menu of existing usb-devices */
    usbDevicesMenu = new VBoxUSBMenu (this);
    connect (usbDevicesMenu, SIGNAL(activated(int)), this, SLOT(menuAddUSBFilterFrom_activated(int)));
    mUSBFilterListModified = false;

    /*
     *  set initial values
     *  ----------------------------------------------------------------------
     */

    /* General page */

    /* keyboard page */

    /* Language page */

    lvLanguages->header()->hide();
    lvLanguages->setSorting (0);

    char szNlsPath[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch (szNlsPath, sizeof(szNlsPath));
    AssertRC (rc);
    QString nlsPath = QString (szNlsPath) + gVBoxLangSubDir;
    QDir nlsDir (nlsPath);
    QStringList files = nlsDir.entryList (QString ("%1*%2")
                                          .arg (gVBoxLangFileBase, gVBoxLangFileExt),
                                          QDir::Files);
    QTranslator translator;
    /* add the default language */
    new LanguageItem (lvLanguages);
    /* add the built-in language */
    new LanguageItem (lvLanguages, translator, gVBoxBuiltInLangName, true /* built-in */);
    /* add all existing languages */
    for (QStringList::Iterator it = files.begin(); it != files.end(); ++ it)
    {
        QString fileName = *it;
        QRegExp regExp (QString (gVBoxLangFileBase) + gVBoxLangIDRegExp);
        int pos = regExp.search (fileName);
        if (pos == -1)
            continue;

        bool loadOk = translator.load (fileName, nlsPath);
        if (!loadOk)
            continue;

        new LanguageItem (lvLanguages, translator, regExp.cap (1));
    }
    lvLanguages->adjustColumn (0);

    /*
     *  update the Ok button state for pages with validation
     *  (validityChanged() connected to enableNext() will do the job)
     */
    wvalGeneral->revalidate();
    wvalKeyboard->revalidate();
}

/**
 *  Returns a path to the given page of this settings dialog. See ::path() for
 *  details.
 */
QString VBoxGlobalSettingsDlg::pagePath (QWidget *aPage)
{
    Q3ListViewItem *li = listView->
        findItem (QString::number (widgetStack->id (aPage)), 1);
    return ::path (li);
}

bool VBoxGlobalSettingsDlg::event (QEvent *aEvent)
{
    bool result = QWidget::event (aEvent);
    if (aEvent->type() == QEvent::LanguageChange)
    {
        /* set the first item selected */
        listView->setSelected (listView->firstChild(), true);
        listView_currentChanged (listView->firstChild());
        lvLanguages_currentChanged (lvLanguages->currentItem());
        mLanguageChanged = false;
        fixLanguageChange();
    }
    return result;
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
        case QEvent::Show:
        {
            if (widget == pageLanguage)
                lvLanguages->updateGeometry();
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

    /* update geometry for the dynamically added usb-page to ensure proper
     * sizeHint calculation by the Qt layout manager */
    wstUSBFilters->updateGeometry();
    /* let our toplevel widget calculate its sizeHint properly */
    QApplication::sendPostedEvents (0, 0);

    /* resize to the miminum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
}

void VBoxGlobalSettingsDlg::listView_currentChanged (Q3ListViewItem *item)
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

    /* reset the warning text; interested parties will set it during
     * validation */
    setWarning (QString::null);

    QString wvalWarning;

    /* detect the overall validity */
    bool newValid = true;
    {
        QObjectList l = this->queryList ("QIWidgetValidator");
        foreach (QObject *obj, l)
        {
            QIWidgetValidator *wval = (QIWidgetValidator *) obj;
            newValid = wval->isValid();
            if (!newValid)
            {
                wvalWarning = wval->warningText();
                break;
            }
        }
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
        /// @todo in VBoxVMSettingsDlg.ui.h, this is absent at all. Is it
        /// really what we want?
#if 0
        if (valid)
            warningSpacer->changeSize (0, 0, QSizePolicy::Expanding);
        else
            warningSpacer->changeSize (0, 0);
#endif
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
 *  Reads global settings from the given VBoxGlobalSettings instance
 *  and from the given CSystemProperties object.
 */
void VBoxGlobalSettingsDlg::getFrom (const CSystemProperties &props,
                                     const VBoxGlobalSettings &gs)
{
    /* default folders */

    leVDIFolder->setText (props.GetDefaultVDIFolder());
    leMachineFolder->setText (props.GetDefaultMachineFolder());

    /* vrdp lib path */
    leVRDPLib->setText (props.GetRemoteDisplayAuthLibrary());

    /* VT-x/AMD-V */
    chbVTX->setChecked (props.GetHWVirtExEnabled());

    /* proprietary GUI settings */

    hkeHostKey->setKey (gs.hostKey() );
    chbAutoCapture->setChecked (gs.autoCapture());

    /* usb filters page */

#ifdef DEBUG_dmik
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();

    /* Show an error message (if there is any).
     * This message box may be suppressed if the user wishes so. */
    if (!host.isReallyOk())
        vboxProblem().cannotAccessUSB (host);

    if (coll.isNull())
    {
#endif
        /* disable the USB host filters category if the USB is
         * not available (i.e. in VirtualBox OSE) */

        Q3ListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
        Assert (usbItem);
        usbItem->setVisible (false);

        /* disable validators if any */
        pageUSB->setEnabled (false);

#ifdef DEBUG_dmik
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

    /* language properties */

    QString langId = gs.languageId();
    Q3ListViewItem *item = lvLanguages->findItem (langId, 1);
    if (!item)
    {
        /* add an item for an invalid language to represent it in the list */
        item = new LanguageItem (lvLanguages, langId);
        lvLanguages->adjustColumn (0);
    }
    Assert (item);
    if (item)
    {
        lvLanguages->setCurrentItem (item);
        lvLanguages->setSelected (item, true);
    }
}

/**
 *  Writes global settings to the given VBoxGlobalSettings instance
 *  and to the given CSystemProperties object.
 */
void VBoxGlobalSettingsDlg::putBackTo (CSystemProperties &props,
                                       VBoxGlobalSettings &gs)
{
    /* default folders */

    if (leVDIFolder->isModified())
        props.SetDefaultVDIFolder (leVDIFolder->text());
    if (props.isOk() && leMachineFolder->isModified())
        props.SetDefaultMachineFolder (leMachineFolder->text());

    /* vrdp lib path */
    if (leVRDPLib->isModified())
        props.SetRemoteDisplayAuthLibrary (leVRDPLib->text());

    /* VT-x/AMD-V */
    props.SetHWVirtExEnabled (chbVTX->isChecked());

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
    if (mUSBFilterListModified)
        for (ulong cnt = host.GetUSBDeviceFilters().GetCount(); cnt; -- cnt)
            host.RemoveUSBDeviceFilter (0);

    /* then add all new filters */
    for (Q3ListViewItem *item = lvUSBFilters->firstChild(); item;
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

    /* language properties */

    Q3ListViewItem *selItem = lvLanguages->selectedItem();
    Assert (selItem);
    if (mLanguageChanged && selItem)
    {
        gs.setLanguageId (selItem->text (1));
        VBoxGlobal::loadLanguage (selItem->text (1));
    }
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
#warning port me
//        widget = focusData()->focusWidget();
    }
    /* if the given widget lacks the whats'this text, look at its parent */
    while (widget && widget != this)
    {
#warning port me
//        text = Q3WhatsThis::textFor (widget);
        if (!text.isEmpty())
            break;
        widget = widget->parentWidget();
    }

    if (text.isEmpty() && !warningString.isEmpty())
        text = warningString;
#warning port me
//    if (text.isEmpty())
//        text = Q3WhatsThis::textFor (this);

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
    QToolButton *tb = qobject_cast <QToolButton *> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == tbResetVDIFolder) le = leVDIFolder;
    else if (tb == tbResetMachineFolder) le = leMachineFolder;
    else if (tb == tbResetVRDPLib) le = leVRDPLib;
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
    QToolButton *tb = qobject_cast <QToolButton *> (sender());
    Assert (tb);

    QLineEdit *le = 0;
    if (tb == tbSelectVDIFolder) le = leVDIFolder;
    else if (tb == tbSelectMachineFolder) le = leMachineFolder;
    else if (tb == tbSelectVRDPLib) le = leVRDPLib;
    Assert (le);

    QString initDir = VBoxGlobal::getFirstExistingDir (le->text());
    if (initDir.isNull())
        initDir = vboxGlobal().virtualBox().GetHomeFolder();

    QString path = le == leVRDPLib ?
        VBoxGlobal::getOpenFileName (initDir, QString::null, this,
                                     "getFile", QString::null) :
        VBoxGlobal::getExistingDirectory (initDir, this);
    if (path.isNull())
        return;

    path = QDir::convertSeparators (path);
    /* remove trailing slash if any */
    path.remove (QRegExp ("[\\\\/]$"));

    /*
     *  do this instead of le->setText (path) to cause
     *  isModified() return true
     */
    le->selectAll();
    le->insert (path);
}

// USB Filter stuff
////////////////////////////////////////////////////////////////////////////////

void VBoxGlobalSettingsDlg::addUSBFilter (const CUSBDeviceFilter &aFilter,
                                          bool aIsNew)
{
    Q3ListViewItem *currentItem = aIsNew
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

    QIWidgetValidator *wval =
        new QIWidgetValidator (pagePath (pageUSB), settings, settings);
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableOk (const QIWidgetValidator *)));

    wval->revalidate();
}

void VBoxGlobalSettingsDlg::lvUSBFilters_currentChanged (Q3ListViewItem *item)
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
    Q3ListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    item->setText (lvUSBFilters_Name, aText);
}

void VBoxGlobalSettingsDlg::tbAddUSBFilter_clicked()
{
    /* search for the max available filter index */
    int maxFilterIndex = 0;
    QString usbFilterName = tr ("New Filter %1", "usb");
    QRegExp regExp (QString ("^") + usbFilterName.arg ("([0-9]+)") + QString ("$"));
    Q3ListViewItemIterator iterator (lvUSBFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text (lvUSBFilters_Name);
        int pos = regExp.search (filterName);
        if (pos != -1)
            maxFilterIndex = regExp.cap (1).toInt() > maxFilterIndex ?
                             regExp.cap (1).toInt() : maxFilterIndex;
        ++ iterator;
    }

    /* create a new usb filter */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilter hostFilter = host
        .CreateUSBDeviceFilter (usbFilterName.arg (maxFilterIndex + 1));
    hostFilter.SetAction (KUSBDeviceFilterAction_Hold);

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
    CHostUSBDeviceFilter hostFilter = host
        .CreateUSBDeviceFilter (vboxGlobal().details (usb));
    hostFilter.SetAction (KUSBDeviceFilterAction_Hold);

    CUSBDeviceFilter filter = CUnknown (hostFilter);
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
    addUSBFilter (filter, true);

    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::tbRemoveUSBFilter_clicked()
{
    Q3ListViewItem *item = lvUSBFilters->currentItem();
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
    Q3ListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    Q3ListViewItem *itemAbove = item->itemAbove();
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
    Q3ListViewItem *item = lvUSBFilters->currentItem();
    Assert (item);

    Q3ListViewItem *itemBelow = item->itemBelow();
    Assert (itemBelow);

    item->moveItem (itemBelow);

    lvUSBFilters_currentChanged (item);
    mUSBFilterListModified = true;
}

void VBoxGlobalSettingsDlg::lvLanguages_currentChanged (Q3ListViewItem *aItem)
{
    Assert (aItem);
    if (!aItem) return;

    /* disable labels for the Default language item */
    bool enabled = !aItem->text (1).isNull();

    tlLangName->setEnabled (enabled);
    tlAuthorName->setEnabled (enabled);
    tlLangData->setText (aItem->text (2));
    tlAuthorData->setText (aItem->text (3));

    mLanguageChanged = true;
}

void VBoxGlobalSettingsDlg::fixLanguageChange()
{
    /* fix for usb page */

#ifdef DEBUG_dmik
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();
    if (coll.isNull())
    {
#endif
        /* disable the USB host filters category if the USB is
         * not available (i.e. in VirtualBox OSE) */

        Q3ListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
        Assert (usbItem);
        usbItem->setVisible (false);

        /* disable validators if any */
        pageUSB->setEnabled (false);

#ifdef DEBUG_dmik
    }
#endif
}
