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
#include "VBoxVMSettingsUSB.h"
#include "VBoxGlobalSettingsLanguage.h"

#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

#include <QTimer>
#include <QPushButton>

//#define ENABLE_GLOBAL_USB

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
                                  mLbWhatsThis->margin() * 2 +
                                  mLbWhatsThis->fontMetrics().lineSpacing() * 4);

    /* Common connections */
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (mTwSelector, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (settingsGroupChanged (QTreeWidgetItem *, QTreeWidgetItem*)));

    /* Hide unnecessary columns and header */
    mTwSelector->header()->hide();
    mTwSelector->hideColumn (listView_Id);
    mTwSelector->hideColumn (listView_Link);

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
    /* General Page */
    VBoxGlobalSettingsGeneral::getFrom (aProps, aGs, mPageGeneral, this);

    /* Input Page */
    VBoxGlobalSettingsInput::getFrom (aProps, aGs, mPageInput, this);

    /* USB Page */
    if (mPageUSB->isEnabled())
        VBoxVMSettingsUSB::getFrom (mPageUSB, this, pagePath (mPageUSB));

    /* Language Page */
    VBoxGlobalSettingsLanguage::getFrom (aProps, aGs, mPageLanguage, this);
}

/**
 *  Writes global settings to the given VBoxGlobalSettings instance
 *  and to the given CSystemProperties object.
 */
COMResult VBoxGlobalSettingsDlg::putBackTo (CSystemProperties &aProps,
                                            VBoxGlobalSettings &aGs)
{
    /* General Page */
    VBoxGlobalSettingsGeneral::putBackTo (aProps, aGs);

    /* Input Page */
    VBoxGlobalSettingsInput::putBackTo (aProps, aGs);

    /* USB Page */
    VBoxVMSettingsUSB::putBackTo();

    /* Language Page */
    VBoxGlobalSettingsLanguage::putBackTo (aProps, aGs);

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

    /* Update QTreeWidget with available items */
    updateAvailability();

    /* Adjust selector list */
    mTwSelector->setFixedWidth (static_cast<QAbstractItemView*> (mTwSelector)
        ->sizeHintForColumn (0) + 2 * mTwSelector->frameWidth());

    /* Sort selector by the id column (to have pages in the desired order) */
    mTwSelector->sortItems (listView_Id, Qt::AscendingOrder);
    mTwSelector->resizeColumnToContents (0);

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
    /* Not touching QILabel until dialog is polished otherwise
     * it can change its size to undefined */
    if (!mPolished)
        return;

    mWarnString = aWarning;
    if (!aWarning.isEmpty())
        mWarnString = QString ("<font color=red>%1</font>").arg (aWarning);

    if (!mWarnString.isEmpty())
        mLbWhatsThis->setText (mWarnString);
    else
        updateWhatsThis (true);
}

void VBoxGlobalSettingsDlg::updateAvailability()
{
#ifdef ENABLE_GLOBAL_USB
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();

    /* Show an error message (if there is any).
     * This message box may be suppressed if the user wishes so. */
    if (!host.isReallyOk())
        vboxProblem().cannotAccessUSB (host);

    if (coll.isNull())
    {
#endif
        /* Disable the USB controller category if the USB controller is
         * not available (i.e. in VirtualBox OSE) */
        QTreeWidgetItem *usbItem = findItem (mTwSelector, "#usb", listView_Link);
        Assert (usbItem);
        if (usbItem)
            usbItem->setHidden (true);
        mPageUSB->setEnabled (false);
#ifdef ENABLE_GLOBAL_USB
    }
#endif
}

