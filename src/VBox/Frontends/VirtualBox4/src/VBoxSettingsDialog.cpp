/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSettingsDialog class implementation
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

#include "VBoxSettingsDialog.h"
#include "VBoxSettingsUtils.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"

/* Qt includes */
#include <QTimer>
#include <QPushButton>
#include <QStackedWidget>

/* Returns the path to the item in the form of 'grandparent > parent > item'
 * using the text of the first column of every item. */
static QString path (QTreeWidgetItem *aItem)
{
    static QString sep = ": ";
    QString p;
    QTreeWidgetItem *cur = aItem;
    while (cur)
    {
        if (!p.isNull())
            p = sep + p;
        p = cur->text (treeWidget_Category).simplified() + p;
        cur = cur->parent();
    }
    return p;
}

VBoxSettingsDialog::VBoxSettingsDialog (QWidget *aParent)
    : QIWithRetranslateUI<QIMainDialog> (aParent)
    , mPolished (false)
    , mValid (true)
    , mWarnIconLabel (new VBoxWarnIconLabel())
    , mWhatsThisTimer (new QTimer (this))
    , mWhatsThisCandidate (0)
{
    /* Apply UI decorations */
    Ui::VBoxSettingsDialog::setupUi (this);

    /* Page title font is derived from the system font */
    QFont f = font();
    f.setBold (true);
    f.setPointSize (f.pointSize() + 2);
    mLbTitle->setFont (f);

    /* Hide unnecessary columns and header */
    mTwSelector->header()->hide();
    mTwSelector->hideColumn (treeWidget_Id);
    mTwSelector->hideColumn (treeWidget_Link);

    /* Crating stack of pages */
    mStack = new QStackedWidget (mWtStackHandler);
    QVBoxLayout *layout = new QVBoxLayout (mWtStackHandler);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mStack);

    /* Setup whatsthis stuff */
    qApp->installEventFilter (this);
    mWhatsThisTimer->setSingleShot (true);
    connect (mWhatsThisTimer, SIGNAL (timeout()), this, SLOT (updateWhatsThis()));
    mLbWhatsThis->setFixedHeight (mLbWhatsThis->frameWidth() * 2 +
                                  mLbWhatsThis->margin() * 2 +
                                  mLbWhatsThis->fontMetrics().lineSpacing() * 4);

    /* Setup warning stuff */
    QIcon icon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this);
    if (!icon.isNull())
        mWarnIconLabel->setWarningPixmap (icon.pixmap (16, 16));
    mButtonBox->addExtraWidget (mWarnIconLabel);

    /* Setup connections */
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (mTwSelector, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (settingsGroupChanged (QTreeWidgetItem *, QTreeWidgetItem*)));

    /* Applying language settings */
    retranslateUi();
}

void VBoxSettingsDialog::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSettingsDialog::retranslateUi (this);

    /* Adjust selector list */
    mTwSelector->setFixedWidth (static_cast<QAbstractItemView*> (mTwSelector)
        ->sizeHintForColumn (treeWidget_Category) + 2 * mTwSelector->frameWidth());

    /* Sort selector by the id column */
    mTwSelector->sortItems (treeWidget_Id, Qt::AscendingOrder);
    mTwSelector->resizeColumnToContents (treeWidget_Category);

    mWarnIconLabel->setWarningText (tr ("Invalid settings detected"));
    mButtonBox->button (QDialogButtonBox::Ok)
        ->setWhatsThis (tr ("Accepts (saves) changes and closes the dialog."));
    mButtonBox->button (QDialogButtonBox::Cancel)
        ->setWhatsThis (tr ("Cancels changes and closes the dialog."));
    mButtonBox->button (QDialogButtonBox::Help)
        ->setWhatsThis (tr ("Displays the dialog help."));

    /* Revalidate all pages to retranslate the warning messages also. */
    QList<QIWidgetValidator*> vlist = findChildren<QIWidgetValidator*>();
    foreach (QIWidgetValidator *wval, vlist)
        if (!wval->isValid())
            revalidate (wval);
}

/**
 *  Returns a path to the given page of this settings dialog. See ::path() for
 *  details.
 */
QString VBoxSettingsDialog::pagePath (QWidget *aPage)
{
    QTreeWidgetItem *li =
        findItem (mTwSelector,
                  QString ("%1")
                      .arg (mStack->indexOf (aPage), 2, 10, QChar ('0')),
                  treeWidget_Id);
    return ::path (li);
}

void VBoxSettingsDialog::setWarning (const QString &aWarning)
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

void VBoxSettingsDialog::enableOk (const QIWidgetValidator*)
{
    setWarning (QString::null);
    QString wvalWarning;

    /* Detect the overall validity */
    bool newValid = true;
    {
        QList<QIWidgetValidator*> vlist = findChildren<QIWidgetValidator*>();
        foreach (QIWidgetValidator *wval, vlist)
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
        /* Try to set the generic error message when invalid but
         * no specific message is provided */
        setWarning (wvalWarning);
    }

    if (mValid != newValid)
    {
        mValid = newValid;
        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (mValid);
        mWarnIconLabel->setVisible (!mValid);
    }
}

void VBoxSettingsDialog::settingsGroupChanged (QTreeWidgetItem *aItem,
                                               QTreeWidgetItem *)
{
    if (aItem)
    {
        int id = aItem->text (treeWidget_Id).toInt();
        Assert (id >= 0);
        mLbTitle->setText (::path (aItem));
        mStack->setCurrentIndex (id);
    }
}

/* Returns first item of 'aView' matching required 'aMatch' value
 * searching the 'aColumn' column. */
QTreeWidgetItem* VBoxSettingsDialog::findItem (QTreeWidget *aView,
                                               const QString &aMatch,
                                               int aColumn)
{
    QList<QTreeWidgetItem*> list =
        aView->findItems (aMatch, Qt::MatchExactly, aColumn);

    return list.count() ? list [0] : 0;
}

void VBoxSettingsDialog::updateWhatsThis (bool aGotFocus /* = false */)
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

void VBoxSettingsDialog::whatsThisCandidateDestroyed (QObject *aObj /* = 0 */)
{
    Assert (mWhatsThisCandidate == aObj);

    if (mWhatsThisCandidate == aObj)
        mWhatsThisCandidate = 0;
}

bool VBoxSettingsDialog::eventFilter (QObject *aObject, QEvent *aEvent)
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
                /* Make sure we don't reference a deleted object after the
                 * timer is shot */
                connect (mWhatsThisCandidate, SIGNAL (destroyed (QObject *)),
                         this, SLOT (whatsThisCandidateDestroyed (QObject *)));
            }
            else
            {
                /* Cleanup */
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

void VBoxSettingsDialog::showEvent (QShowEvent *aEvent)
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

