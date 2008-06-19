/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Global settings" dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#include "VBoxGlobalSettingsDlg.h"
#include "VBoxVMSettingsUtils.h"

#include "VBoxGlobalSettingsGeneral.h"
#include "VBoxGlobalSettingsInput.h"
//#include "VBoxGlobalSettingsUSB.h"
//#include "VBoxGlobalSettingsLanguage.h"

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

#include <QTimer>
#include <QPushButton>

/**
 *  Returns the path to the item in the form of 'grandparent > parent > item'
 *  using the text of the first column of every item.
 */
static QString path (QTreeWidgetItem *aItem)
{
    static QString sep = ": ";
    QString p;
    QTreeWidgetItem *cur = aItem;
    while (cur)
    {
        if (!p.isNull())
            p = sep + p;
        p = cur->text (0).simplified() + p;
        cur = cur->parent();
    }
    return p;
}

static QTreeWidgetItem* findItem (QTreeWidget *aView,
                                  const QString &aMatch, int aColumn)
{
    QList<QTreeWidgetItem*> list =
        aView->findItems (aMatch, Qt::MatchExactly, aColumn);

    return list.count() ? list [0] : 0;
}

// /* Language page */
// /* Defined in VBoxGlobal.cpp */
// extern const char *gVBoxLangSubDir;
// extern const char *gVBoxLangFileBase;
// extern const char *gVBoxLangFileExt;
// extern const char *gVBoxLangIDRegExp;
// extern const char *gVBoxBuiltInLangName;
// class LanguageItem : public Q3ListViewItem
// {
// public:
//
//     enum { TypeId = 1001 };
//
//     LanguageItem (Q3ListView *aParent, const QTranslator &aTranslator,
//                   const QString &aId, bool aBuiltIn = false)
//         : Q3ListViewItem (aParent), mBuiltIn (aBuiltIn), mInvalid (false)
//     {
//         Assert (!aId.isEmpty());
//
// //        QTranslatorMessage transMes;
//
//         /* Note: context/source/comment arguments below must match strings
//          * used in VBoxGlobal::languageName() and friends (the latter are the
//          * source of information for the lupdate tool that generates
//          * translation files) */
//
//         QString nativeLanguage = tratra (aTranslator,
//             "@@@", "English", "Native language name");
//         QString nativeCountry = tratra (aTranslator,
//             "@@@", "--", "Native language country name "
//             "(empty if this language is for all countries)");
//
//         QString englishLanguage = tratra (aTranslator,
//             "@@@", "English", "Language name, in English");
//         QString englishCountry = tratra (aTranslator,
//             "@@@", "--", "Language country name, in English "
//             "(empty if native country name is empty)");
//
//         QString translatorsName = tratra (aTranslator,
//             "@@@", "Sun Microsystems, Inc.", "Comma-separated list of translators");
//
//         QString itemName = nativeLanguage;
//         QString langName = englishLanguage;
//
//         if (!aBuiltIn)
//         {
//             if (nativeCountry != "--")
//                 itemName += " (" + nativeCountry + ")";
//
//             if (englishCountry != "--")
//                 langName += " (" + englishCountry + ")";
//
//             if (itemName != langName)
//                 langName = itemName + " / " + langName;
//         }
//         else
//         {
//             itemName += VBoxGlobalSettingsDlg::tr (" (built-in)", "Language");
//             langName += VBoxGlobalSettingsDlg::tr (" (built-in)", "Language");
//         }
//
//         setText (0, itemName);
//         setText (1, aId);
//         setText (2, langName);
//         setText (3, translatorsName);
//     }
//
//     /* Constructs an item for an invalid language ID (i.e. when a language
//      * file is missing or corrupt). */
//     LanguageItem (Q3ListView *aParent, const QString &aId)
//         : Q3ListViewItem (aParent), mBuiltIn (false), mInvalid (true)
//     {
//         Assert (!aId.isEmpty());
//
//         setText (0, QString ("<%1>").arg (aId));
//         setText (1, aId);
//         setText (2, VBoxGlobalSettingsDlg::tr ("<unavailable>", "Language"));
//         setText (3, VBoxGlobalSettingsDlg::tr ("<unknown>", "Author(s)"));
//     }
//
//     /* Constructs an item for the default language ID (column 1 will be set
//      * to QString::null) */
//     LanguageItem (Q3ListView *aParent)
//         : Q3ListViewItem (aParent), mBuiltIn (false), mInvalid (false)
//     {
//         setText (0, VBoxGlobalSettingsDlg::tr ("Default", "Language"));
//         setText (1, QString::null);
//         /* empty strings of some reasonable length to prevent the info part
//          * from being shrinked too much when the list wants to be wider */
//         setText (2, "                ");
//         setText (3, "                ");
//     }
//
//     int rtti() const { return TypeId; }
//
//     int compare (Q3ListViewItem *aItem, int aColumn, bool aAscending) const
//     {
//         QString thisId = text (1);
//         QString thatId = aItem->text (1);
//         if (thisId.isNull())
//             return -1;
//         if (thatId.isNull())
//             return 1;
//         if (mBuiltIn)
//             return -1;
//         if (aItem->rtti() == TypeId && ((LanguageItem *) aItem)->mBuiltIn)
//             return 1;
//         return Q3ListViewItem::compare (aItem, aColumn, aAscending);
//     }
//
//     void paintCell (QPainter *aPainter, const QColorGroup &aGroup,
//                     int aColumn, int aWidth, int aAlign)
//     {
//         QFont font = aPainter->font();
//
//         if (mInvalid)
//             font.setItalic (true);
//         /* mark the effectively active language */
//         if (text (1) == VBoxGlobal::languageId())
//             font.setBold (true);
//
//         if (aPainter->font() != font)
//             aPainter->setFont (font);
//
//         Q3ListViewItem::paintCell (aPainter, aGroup, aColumn, aWidth, aAlign);
//
//         if (mBuiltIn)
//         {
//             int y = height() - 1;
//             aPainter->setPen (aGroup.mid());
//             aPainter->drawLine (0, y, aWidth - 1, y);
//         }
//     }
//
//     int width (const QFontMetrics &aFM, const Q3ListView *aLV, int aC) const
//     {
//         QFont font = aLV->font();
//
//         if (mInvalid)
//             font.setItalic (true);
//         /* mark the effectively active language */
//         if (text (1) == VBoxGlobal::languageId())
//             font.setBold (true);
//
//         QFontMetrics fm = aFM;
//         if (aLV->font() != font)
//             fm = QFontMetrics (font);
//
//         return Q3ListViewItem::width (fm, aLV, aC);
//     }
//
//     void setup ()
//     {
//         Q3ListViewItem::setup();
//         if (mBuiltIn)
//             setHeight (height() + 1);
//     }
//
// private:
//
//     QString tratra (const QTranslator &aTranslator, const char *aCtxt,
//                        const char *aSrc, const char *aCmnt)
//     {
// //#warning port me: check this
//         QString msg = aTranslator.translate (aCtxt, aSrc, aCmnt);
// //        QString msg = aTranslator.findMessage (aCtxt, aSrc, aCmnt).translation();
//         /* return the source text if no translation is found */
//         if (msg.isEmpty())
//             msg = QString (aSrc);
//         return msg;
//     }
//
//     bool mBuiltIn : 1;
//     bool mInvalid : 1;
// };

VBoxGlobalSettingsDlg::VBoxGlobalSettingsDlg (QWidget *aParent)
    : QIWithRetranslateUI<QIMainDialog> (aParent)
    , mPolished (false)
    , mValid (true)
    , mWhatsThisTimer (new QTimer (this))
    , mWhatsThisCandidate (NULL)
{
    /* Apply UI decorations */
    Ui::VBoxGlobalSettingsDlg::setupUi (this);

#ifndef Q_WS_MAC
    setWindowIcon (QIcon (":/global_settings_16px.png"));
#endif /* Q_WS_MAC */

    mWarnIconLabel = new VBoxWarnIconLabel();
    mWarnIconLabel->setVisible (false);

    /* Setup warning icon */
    QIcon icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this);
    if (!icon.isNull())
        mWarnIconLabel->setWarningPixmap (icon.pixmap (16, 16));

    mButtonBox->addExtraWidget (mWarnIconLabel);

    /* Page title font is derived from the system font */
    QFont f = font();
    f.setBold (true);
    f.setPointSize (f.pointSize() + 2);
    mLbTitle->setFont (f);

    /* Setup the what's this label */
    qApp->installEventFilter (this);
    mWhatsThisTimer->setSingleShot (true);
    connect (mWhatsThisTimer, SIGNAL (timeout()), this, SLOT (updateWhatsThis()));

    mLbWhatsThis->setFixedHeight (mLbWhatsThis->frameWidth() * 2 +
                                  6 /* seems that RichText adds some margin */ +
                                  mLbWhatsThis->fontMetrics().lineSpacing() * 4);
    mLbWhatsThis->setMinimumWidth (mLbWhatsThis->frameWidth() * 2 +
                                   6 /* seems that RichText adds some margin */ +
                                   mLbWhatsThis->fontMetrics().width ('m') * 40);

    /* Common connections */
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (mTwSelector, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (settingsGroupChanged (QTreeWidgetItem *, QTreeWidgetItem*)));

//     /* Language page */
//     lvLanguages->header()->hide();
//     lvLanguages->setSorting (0);
//
//     char szNlsPath[RTPATH_MAX];
//     int rc = RTPathAppPrivateNoArch (szNlsPath, sizeof(szNlsPath));
//     AssertRC (rc);
//     QString nlsPath = QString (szNlsPath) + gVBoxLangSubDir;
//     QDir nlsDir (nlsPath);
//     QStringList files = nlsDir.entryList (QString ("%1*%2")
//                                           .arg (gVBoxLangFileBase, gVBoxLangFileExt),
//                                           QDir::Files);
//     QTranslator translator;
//     /* add the default language */
//     new LanguageItem (lvLanguages);
//     /* add the built-in language */
//     new LanguageItem (lvLanguages, translator, gVBoxBuiltInLangName, true /* built-in */);
//     /* add all existing languages */
//     for (QStringList::Iterator it = files.begin(); it != files.end(); ++ it)
//     {
//         QString fileName = *it;
//         QRegExp regExp (QString (gVBoxLangFileBase) + gVBoxLangIDRegExp);
//         int pos = regExp.search (fileName);
//         if (pos == -1)
//             continue;
//
//         bool loadOk = translator.load (fileName, nlsPath);
//         if (!loadOk)
//             continue;
//
//         new LanguageItem (lvLanguages, translator, regExp.cap (1));
//     }
//     lvLanguages->adjustColumn (0);

    /* Hide unnecessary columns and header */
    mTwSelector->header()->hide();
    mTwSelector->hideColumn (listView_Id);
    mTwSelector->hideColumn (listView_Link);

    /* Adjust selector list */
    int minWid = 0;
    for (int i = 0; i < mTwSelector->topLevelItemCount(); ++ i)
    {
        QTreeWidgetItem *item = mTwSelector->topLevelItem (i);
        QFontMetrics fm (item->font (0));
        int wid = fm.width (item->text (0)) +
                  16 /* icon */ + 2 * 8 /* 2 margins */;
        minWid = wid > minWid ? wid : minWid;
        int hei = fm.height() > 16 ?
                  fm.height() /* text height */ :
                  16 /* icon */ + 2 * 2 /* 2 margins */;
        item->setSizeHint (0, QSize (wid, hei));
    }
    mTwSelector->setFixedWidth (minWid);

    /* Sort selector by the id column (to have pages in the desired order) */
    mTwSelector->sortItems (listView_Id, Qt::AscendingOrder);

    /* Initially select the first settings page */
    mTwSelector->setCurrentItem (mTwSelector->topLevelItem (0));

    /* Applying language settings */
    retranslateUi();
}

/**
 *  Reads global settings from the given VBoxGlobalSettings instance
 *  and from the given CSystemProperties object.
 */
void VBoxGlobalSettingsDlg::getFrom (const CSystemProperties &aProps,
                                     const VBoxGlobalSettings &aGs)
{
//     /* USB Page */
// #ifdef DEBUG_dmik
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();
//
//     /* Show an error message (if there is any).
//      * This message box may be suppressed if the user wishes so. */
//     if (!host.isReallyOk())
//         vboxProblem().cannotAccessUSB (host);
//
//     if (coll.isNull())
//     {
// #endif
//         /* disable the USB host filters category if the USB is
//          * not available (i.e. in VirtualBox OSE) */
//
//         Q3ListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
//         Assert (usbItem);
//         usbItem->setVisible (false);
//
//         /* disable validators if any */
//         pageUSB->setEnabled (false);
//
// #ifdef DEBUG_dmik
//     }
//     else
//     {
//         CHostUSBDeviceFilterEnumerator en = coll.Enumerate();
//         while (en.HasMore())
//         {
//             CHostUSBDeviceFilter hostFilter = en.GetNext();
//             CUSBDeviceFilter filter = CUnknown (hostFilter);
//             addUSBFilter (filter, false);
//         }
//         lvUSBFilters->setCurrentItem (lvUSBFilters->firstChild());
//         lvUSBFilters_currentChanged (lvUSBFilters->firstChild());
//     }
// #endif

//     /* Language page */
//     QString langId = gs.languageId();
//     Q3ListViewItem *item = lvLanguages->findItem (langId, 1);
//     if (!item)
//     {
//         /* add an item for an invalid language to represent it in the list */
//         item = new LanguageItem (lvLanguages, langId);
//         lvLanguages->adjustColumn (0);
//     }
//     Assert (item);
//     if (item)
//     {
//         lvLanguages->setCurrentItem (item);
//         lvLanguages->setSelected (item, true);
//     }

    /* General Page */
    VBoxGlobalSettingsGeneral::getFrom (aProps, aGs, mPageGeneral, this);

    /* Input Page */
    VBoxGlobalSettingsInput::getFrom (aProps, aGs, mPageInput, this);

    /* USB Page */
    //VBoxVMSettingsUSB::getFrom (aProps, aGs, mPageUSB,
    //                            this, pagePath (mPageUSB));

    /* Language Page */
    //VBoxGlobalSettingsLanguage::getFrom (aProps, aGs, mPageLanguage);
}

/**
 *  Writes global settings to the given VBoxGlobalSettings instance
 *  and to the given CSystemProperties object.
 */
COMResult VBoxGlobalSettingsDlg::putBackTo (CSystemProperties &aProps,
                                            VBoxGlobalSettings &aGs)
{
//     /* USB Page */
//     /*
//      *  first, remove all old filters (only if the list is changed,
//      *  not only individual properties of filters)
//      */
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     if (mUSBFilterListModified)
//         for (ulong cnt = host.GetUSBDeviceFilters().GetCount(); cnt; -- cnt)
//             host.RemoveUSBDeviceFilter (0);
//
//     /* then add all new filters */
//     for (Q3ListViewItem *item = lvUSBFilters->firstChild(); item;
//          item = item->nextSibling())
//     {
//         USBListItem *uli = static_cast<USBListItem*> (item);
//         /*
//         VBoxUSBFilterSettings *settings =
//             static_cast <VBoxUSBFilterSettings *>
//                 (wstUSBFilters->widget (uli->mId));
//         Assert (settings);
//
//         COMResult res = settings->putBackToFilter();
//         if (!res.isOk())
//             return;
//
//         CUSBDeviceFilter filter = settings->filter();
//         filter.SetActive (uli->isOn());
//
//         CHostUSBDeviceFilter insertedFilter = CUnknown (filter);
//         if (mUSBFilterListModified)
//             host.InsertUSBDeviceFilter (host.GetUSBDeviceFilters().GetCount(),
//                                         insertedFilter);
//         */
//     }
//     mUSBFilterListModified = false;

//     /* Language Page */
//     Q3ListViewItem *selItem = lvLanguages->selectedItem();
//     Assert (selItem);
//     if (mLanguageChanged && selItem)
//     {
//         gs.setLanguageId (selItem->text (1));
//         VBoxGlobal::loadLanguage (selItem->text (1));
//     }

    /* General Page */
    VBoxGlobalSettingsGeneral::putBackTo (aProps, aGs);

    /* Input Page */
    VBoxGlobalSettingsInput::putBackTo (aProps, aGs);

    /* USB Page */
    //VBoxVMSettingsUSB::putBackTo (aProps, aGs);

    /* Language Page */
    //VBoxGlobalSettingsLanguage::putBackTo (aProps, aGs);

    return COMResult();
}

void VBoxGlobalSettingsDlg::retranslateUi()
{
    /* Unfortunately retranslateUi clears the QTreeWidget to do the
     * translation. So save the current selected index. */
    int ci = mPageStack->currentIndex();
    /* Translate uic generated strings */
    Ui::VBoxGlobalSettingsDlg::retranslateUi (this);
    /* Set the old index */
    mTwSelector->setCurrentItem (mTwSelector->topLevelItem (ci));

    mWarnIconLabel->setWarningText (tr ("Invalid settings detected"));
    mButtonBox->button (QDialogButtonBox::Ok)->setWhatsThis (tr ("Accepts (saves) changes and closes the dialog."));
    mButtonBox->button (QDialogButtonBox::Cancel)->setWhatsThis (tr ("Cancels changes and closes the dialog."));
    mButtonBox->button (QDialogButtonBox::Help)->setWhatsThis (tr ("Displays the dialog help."));

    /* Revalidate all pages to retranslate the warning messages also. */
    QList<QIWidgetValidator*> l = this->findChildren<QIWidgetValidator*>();
    foreach (QIWidgetValidator *wval, l)
        if (!wval->isValid())
            revalidate (wval);
}

void VBoxGlobalSettingsDlg::enableOk (const QIWidgetValidator*)
{
    setWarning (QString::null);
    QString wvalWarning;

    /* Detect the overall validity */
    bool newValid = true;
    {
        QList<QIWidgetValidator*> l = this->findChildren<QIWidgetValidator*>();
        foreach (QIWidgetValidator *wval, l)
        {
            newValid = wval->isValid();
            if (!newValid)
            {
                wvalWarning = wval->warningText();
                break;
            }
        }
    }

    if (mWarnString.isNull() && !wvalWarning.isNull())
    {
        /* Try to set the generic error message when invalid but no specific
         * message is provided */
        setWarning (wvalWarning);
    }

    if (mValid != newValid)
    {
        mValid = newValid;
        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (mValid);
        mWarnIconLabel->setVisible (!mValid);
    }
}

void VBoxGlobalSettingsDlg::revalidate (QIWidgetValidator*)
{
    /* Do individual validations for pages */
}

void VBoxGlobalSettingsDlg::settingsGroupChanged (QTreeWidgetItem *aItem,
                                                  QTreeWidgetItem *)
{
    if (aItem)
    {
        int id = aItem->text (1).toInt();
        Assert (id >= 0);
        mLbTitle->setText (::path (aItem));
        mPageStack->setCurrentIndex (id);
    }
}

void VBoxGlobalSettingsDlg::updateWhatsThis (bool aGotFocus /* = false */)
{
    QString text;

    QWidget *widget = 0;
    if (!aGotFocus)
    {
        if (mWhatsThisCandidate && mWhatsThisCandidate != this)
            widget = mWhatsThisCandidate;
    }
    else
    {
        widget = QApplication::focusWidget();
    }
    /* If the given widget lacks the whats'this text, look at its parent */
    while (widget && widget != this)
    {
        text = widget->whatsThis();
        if (!text.isEmpty())
            break;
        widget = widget->parentWidget();
    }

    if (text.isEmpty() && !mWarnString.isEmpty())
        text = mWarnString;
    if (text.isEmpty())
        text = whatsThis();

    mLbWhatsThis->setText (text);
}

void VBoxGlobalSettingsDlg::whatsThisCandidateDestroyed (QObject *aObj /*= NULL*/)
{
    /* sanity */
    Assert (mWhatsThisCandidate == aObj);

    if (mWhatsThisCandidate == aObj)
        mWhatsThisCandidate = NULL;
}

bool VBoxGlobalSettingsDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!aObject->isWidgetType())
        return QIMainDialog::eventFilter (aObject, aEvent);

    QWidget *widget = static_cast<QWidget*> (aObject);
    if (widget->topLevelWidget() != this)
        return QIMainDialog::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        case QEvent::Enter:
        case QEvent::Leave:
        {
            if (aEvent->type() == QEvent::Enter)
            {
                /* What if Qt sends Enter w/o Leave... */
                if (mWhatsThisCandidate)
                    disconnect (mWhatsThisCandidate, SIGNAL (destroyed (QObject *)),
                                this, SLOT (whatsThisCandidateDestroyed (QObject *)));

                mWhatsThisCandidate = widget;
                /* make sure we don't reference a deleted object after the
                 * timer is shot */
                connect (mWhatsThisCandidate, SIGNAL (destroyed (QObject *)),
                         this, SLOT (whatsThisCandidateDestroyed (QObject *)));
            }
            else
            {
                /* cleanup */
                if (mWhatsThisCandidate)
                    disconnect (mWhatsThisCandidate, SIGNAL (destroyed (QObject *)),
                                this, SLOT (whatsThisCandidateDestroyed (QObject *)));
                mWhatsThisCandidate = NULL;
            }

            mWhatsThisTimer->start (100);
            break;
        }
        case QEvent::FocusIn:
        {
            updateWhatsThis (true /* aGotFocus */);
            break;
        }
        default:
            break;
    }

    return QIMainDialog::eventFilter (aObject, aEvent);
}

void VBoxGlobalSettingsDlg::showEvent (QShowEvent *aEvent)
{
    QIMainDialog::showEvent (aEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    if (mPolished)
        return;

    mPolished = true;

    /* Resize to the minimum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
}

/**
 *  Returns a path to the given page of this settings dialog. See ::path() for
 *  details.
 */
QString VBoxGlobalSettingsDlg::pagePath (QWidget *aPage)
{
    QTreeWidgetItem *li =
        findItem (mTwSelector,
                  QString ("%1")
                      .arg (mPageStack->indexOf (aPage), 2, 10, QChar ('0')),
                  1);
    return ::path (li);
}

void VBoxGlobalSettingsDlg::setWarning (const QString &aWarning)
{
    mWarnString = aWarning;
    if (!aWarning.isEmpty())
        mWarnString = QString ("<font color=red>%1</font>").arg (aWarning);

    if (!mWarnString.isEmpty())
        mLbWhatsThis->setText (mWarnString);
    else
        updateWhatsThis (true);
}

// /* USB Page */
// void VBoxGlobalSettingsDlg::addUSBFilter (const CUSBDeviceFilter &aFilter,
//                                           bool aIsNew)
// {
//     /*
//     Q3ListViewItem *currentItem = aIsNew
//         ? lvUSBFilters->currentItem()
//         : lvUSBFilters->lastItem();
//
//     VBoxUSBFilterSettings *settings = new VBoxUSBFilterSettings (wstUSBFilters);
//     settings->setup (VBoxUSBFilterSettings::HostType);
//     settings->getFromFilter (aFilter);
//
//     USBListItem *item = new USBListItem (lvUSBFilters, currentItem);
//     item->setOn (aFilter.GetActive());
//     item->setText (lvUSBFilters_Name, aFilter.GetName());
//
//     item->mId = wstUSBFilters->addWidget (settings);
//
//     // fix the tab order so that main dialog's buttons are always the last
//     setTabOrder (settings->focusProxy(), buttonHelp);
//     setTabOrder (buttonHelp, buttonOk);
//     setTabOrder (buttonOk, buttonCancel);
//
//     if (aIsNew)
//     {
//         lvUSBFilters->setSelected (item, true);
//         lvUSBFilters_currentChanged (item);
//         settings->leUSBFilterName->setFocus();
//     }
//
//     connect (settings->leUSBFilterName, SIGNAL (textChanged (const QString &)),
//              this, SLOT (lvUSBFilters_setCurrentText (const QString &)));
//
//     // setup validation
//
//     QIWidgetValidator *wval =
//         new QIWidgetValidator (pagePath (pageUSB), settings, settings);
//     connect (wval, SIGNAL (validityChanged (const QIWidgetValidator *)),
//              this, SLOT (enableOk (const QIWidgetValidator *)));
//
//     wval->revalidate();
//     */
// }
//
// void VBoxGlobalSettingsDlg::lvUSBFilters_currentChanged (Q3ListViewItem *item)
// {
//     if (item && lvUSBFilters->selectedItem() != item)
//         lvUSBFilters->setSelected (item, true);
//
//     tbRemoveUSBFilter->setEnabled (!!item);
//
//     tbUSBFilterUp->setEnabled (!!item && item->itemAbove());
//     tbUSBFilterDown->setEnabled (!!item && item->itemBelow());
//
//     if (item)
//     {
//         USBListItem *uli = static_cast <USBListItem *> (item);
//         wstUSBFilters->raiseWidget (uli->mId);
//     }
//     else
//     {
//         /* raise the disabled widget */
//         wstUSBFilters->raiseWidget (0);
//     }
// }
//
// void VBoxGlobalSettingsDlg::lvUSBFilters_setCurrentText (const QString &aText)
// {
//     Q3ListViewItem *item = lvUSBFilters->currentItem();
//     Assert (item);
//
//     item->setText (lvUSBFilters_Name, aText);
// }
//
// void VBoxGlobalSettingsDlg::tbAddUSBFilter_clicked()
// {
//     /* search for the max available filter index */
//     int maxFilterIndex = 0;
//     QString usbFilterName = tr ("New Filter %1", "usb");
//     QRegExp regExp (QString ("^") + usbFilterName.arg ("([0-9]+)") + QString ("$"));
//     Q3ListViewItemIterator iterator (lvUSBFilters);
//     while (*iterator)
//     {
//         QString filterName = (*iterator)->text (lvUSBFilters_Name);
//         int pos = regExp.search (filterName);
//         if (pos != -1)
//             maxFilterIndex = regExp.cap (1).toInt() > maxFilterIndex ?
//                              regExp.cap (1).toInt() : maxFilterIndex;
//         ++ iterator;
//     }
//
//     /* create a new usb filter */
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostUSBDeviceFilter hostFilter = host
//         .CreateUSBDeviceFilter (usbFilterName.arg (maxFilterIndex + 1));
//     hostFilter.SetAction (KUSBDeviceFilterAction_Hold);
//
//     CUSBDeviceFilter filter = CUnknown (hostFilter);
//     filter.SetActive (true);
//     addUSBFilter (filter, true);
//
//     mUSBFilterListModified = true;
// }
//
// void VBoxGlobalSettingsDlg::tbAddUSBFilterFrom_clicked()
// {
//     usbDevicesMenu->exec (QCursor::pos());
// }
//
// void VBoxGlobalSettingsDlg::menuAddUSBFilterFrom_activated (QAction *aAction)
// {
//     CUSBDevice usb = usbDevicesMenu->getUSB (aAction);
//
//     // if null then some other item but a USB device is selected
//     if (usb.isNull())
//         return;
//
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostUSBDeviceFilter hostFilter = host
//         .CreateUSBDeviceFilter (vboxGlobal().details (usb));
//     hostFilter.SetAction (KUSBDeviceFilterAction_Hold);
//
//     CUSBDeviceFilter filter = CUnknown (hostFilter);
//     filter.SetVendorId (QString().sprintf ("%04hX", usb.GetVendorId()));
//     filter.SetProductId (QString().sprintf ("%04hX", usb.GetProductId()));
//     filter.SetRevision (QString().sprintf ("%04hX", usb.GetRevision()));
//     /* The port property depends on the host computer rather than on the USB
//      * device itself; for this reason only a few people will want to use it in
//      * the filter since the same device plugged into a different socket will
//      * not match the filter in this case.  */
// #if 0
//     /// @todo set it anyway if Alt is currently pressed
//     filter.SetPort (QString().sprintf ("%04hX", usb.GetPort()));
// #endif
//     filter.SetManufacturer (usb.GetManufacturer());
//     filter.SetProduct (usb.GetProduct());
//     filter.SetSerialNumber (usb.GetSerialNumber());
//     filter.SetRemote (usb.GetRemote() ? "yes" : "no");
//
//     filter.SetActive (true);
//     addUSBFilter (filter, true);
//
//     mUSBFilterListModified = true;
// }
//
// void VBoxGlobalSettingsDlg::tbRemoveUSBFilter_clicked()
// {
//     Q3ListViewItem *item = lvUSBFilters->currentItem();
//     Assert (item);
//
//     USBListItem *uli = static_cast <USBListItem *> (item);
//     QWidget *settings = wstUSBFilters->widget (uli->mId);
//     Assert (settings);
//     wstUSBFilters->removeWidget (settings);
//     delete settings;
//
//     delete item;
//
//     lvUSBFilters->setSelected (lvUSBFilters->currentItem(), true);
//     mUSBFilterListModified = true;
// }
//
// void VBoxGlobalSettingsDlg::tbUSBFilterUp_clicked()
// {
//     Q3ListViewItem *item = lvUSBFilters->currentItem();
//     Assert (item);
//
//     Q3ListViewItem *itemAbove = item->itemAbove();
//     Assert (itemAbove);
//     itemAbove = itemAbove->itemAbove();
//
//     if (!itemAbove)
//         item->itemAbove()->moveItem (item);
//     else
//         item->moveItem (itemAbove);
//
//     lvUSBFilters_currentChanged (item);
//     mUSBFilterListModified = true;
// }
//
// void VBoxGlobalSettingsDlg::tbUSBFilterDown_clicked()
// {
//     Q3ListViewItem *item = lvUSBFilters->currentItem();
//     Assert (item);
//
//     Q3ListViewItem *itemBelow = item->itemBelow();
//     Assert (itemBelow);
//
//     item->moveItem (itemBelow);
//
//     lvUSBFilters_currentChanged (item);
//     mUSBFilterListModified = true;
// }

// /* Language Page */
// void VBoxGlobalSettingsDlg::lvLanguages_currentChanged (Q3ListViewItem *aItem)
// {
//     Assert (aItem);
//     if (!aItem) return;
//
//     /* disable labels for the Default language item */
//     bool enabled = !aItem->text (1).isNull();
//
//     tlLangName->setEnabled (enabled);
//     tlAuthorName->setEnabled (enabled);
//     tlLangData->setText (aItem->text (2));
//     tlAuthorData->setText (aItem->text (3));
//
//     mLanguageChanged = true;
// }
//
// void VBoxGlobalSettingsDlg::fixLanguageChange()
// {
//     /* fix for usb page */
//
// #ifdef DEBUG_dmik
//     CHost host = vboxGlobal().virtualBox().GetHost();
//     CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();
//     if (coll.isNull())
//     {
// #endif
//         /* disable the USB host filters category if the USB is
//          * not available (i.e. in VirtualBox OSE) */
//
//         Q3ListViewItem *usbItem = listView->findItem ("#usb", listView_Link);
//         Assert (usbItem);
//         usbItem->setVisible (false);
//
//         /* disable validators if any */
//         pageUSB->setEnabled (false);
//
// #ifdef DEBUG_dmik
//     }
// #endif
// }

