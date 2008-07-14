/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSettingsDialog class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "VBoxSettingsSelector.h"
#include "VBoxSettingsPage.h"
#include "VBoxToolBar.h"

#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

/* Qt includes */
#include <QTimer>
#include <QPushButton>
#include <QStackedWidget>


#if MAC_LEOPARD_STYLE
# define VBOX_GUI_WITH_TOOLBAR_SETTINGS
#endif /* MAC_LEOPARD_STYLE */

VBoxSettingsDialog::VBoxSettingsDialog (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QIMainDialog> (aParent)
    , mPolished (false)
    , mValid (true)
    , mWarnIconLabel (new VBoxWarnIconLabel())
    , mWhatsThisTimer (new QTimer (this))
    , mWhatsThisCandidate (0)
{
    /* Apply UI decorations */
    Ui::VBoxSettingsDialog::setupUi (this);

#ifdef Q_WS_MAC
//    VBoxGlobal::setLayoutMargin (centralWidget()->layout(), 0);
    /* No status bar on the mac */
    setSizeGripEnabled (false);
    setStatusBar (NULL);
#endif

    /* Page title font is derived from the system font */
    QFont f = font();
    f.setBold (true);
    f.setPointSize (f.pointSize() + 2);
    mLbTitle->setFont (f);

    QGridLayout *mainLayout = static_cast<QGridLayout*> (mAllWidget->layout());
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    mLbTitle->hide();
    mLbWhatsThis->hide();
    mSelector = new VBoxSettingsToolBarSelector (this);
    static_cast<VBoxToolBar*> (mSelector->widget())->setMacToolbar();
    addToolBar (qobject_cast<QToolBar*> (mSelector->widget()));
    /* No title in this mode, we change the title of the window. */
    mainLayout->setColumnMinimumWidth (0, 0);
    mainLayout->setHorizontalSpacing (0);
#else
    /* Create the classical tree view selector */
    mSelector = new VBoxSettingsTreeViewSelector (this);
    mainLayout->addWidget (mSelector->widget(), 0, 0, 3, 1);
    mSelector->widget()->setFocus();
#endif

    /* Creating stack of pages */
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

    /* Set the default button */
    mButtonBox->button (QDialogButtonBox::Ok)->setDefault (true);

    /* Setup connections */
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (helpRequested()), &vboxProblem(), SLOT (showHelpHelpDialog()));
    connect (mSelector, SIGNAL (categoryChanged (int)),
             this, SLOT (categoryChanged (int)));

    /* Applying language settings */
    retranslateUi();
}

void VBoxSettingsDialog::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSettingsDialog::retranslateUi (this);

    /* Revalidate all pages to retranslate the warning messages also. */
    QList<QIWidgetValidator*> vlist = findChildren<QIWidgetValidator*>();
    foreach (QIWidgetValidator *wval, vlist)
        if (!wval->isValid())
            revalidate (wval);
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

QString VBoxSettingsDialog::titleExtension() const
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    return mSelector->itemText (mSelector->currentId());
#else
    return tr ("Settings");
#endif
}

void VBoxSettingsDialog::categoryChanged (int aId)
{
    QWidget *rootPage = mSelector->rootPage (aId);
#ifndef Q_WS_MAC
    mLbTitle->setText (mSelector->itemText (aId));
    mStack->setCurrentIndex (mStack->indexOf (rootPage));
#else /* Q_WS_MAC */
    /* We will update at once later */
    setUpdatesEnabled (false);
    setMinimumSize (QSize (minimumWidth(), 0));
    setMaximumSize (QSize (minimumWidth(), QWIDGETSIZE_MAX));
    /* Set all tab size policies to ignored */
    QList<QWidget*> list = mSelector->rootPages ();
    for (int i = 0; i < list.count(); ++i)
        list.at (i)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Ignored);
    /* Set the size policy of the current tab to preferred */
    if (mStack->widget (mStack->indexOf (rootPage)))
        mStack->widget (mStack->indexOf (rootPage))->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Preferred);
    /* Set the new current tab */
    mLbTitle->setText (mSelector->itemText (aId));
    mStack->setCurrentIndex (mStack->indexOf (rootPage));
    /* Activate the new layout */
    layout()->activate();
    setUpdatesEnabled (true);
    //        mAllWidget->hide();
    QSize s = minimumSize();
    int minWidth = mSelector->minWidth();
    if (minWidth > s.width())
        s.setWidth (minWidth);
    /* Play the resize animation */
    ::darwinWindowAnimateResize (this, QRect (x(), y(), 
                                              s.width(), s.height()));
    //        mAllWidget->show();
    /* Set the new size to Qt also */
    setFixedSize (s);
#endif /* !Q_WS_MAC */
# ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle (dialogTitle());
# endif
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
    int minWidth = mSelector->minWidth();
    QSize s = minimumSize();
    if (minWidth > s.width())
        s.setWidth (minWidth);
#ifdef Q_WS_MAC
    categoryChanged (mSelector->currentId());
#else /* Q_WS_MAC */
    resize (s);
#endif /* Q_WS_MAC */

    VBoxGlobal::centerWidget (this, parentWidget());
}

