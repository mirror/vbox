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
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

#if MAC_LEOPARD_STYLE
# define VBOX_GUI_WITH_TOOLBAR_SETTINGS
#endif /* MAC_LEOPARD_STYLE */

VBoxSettingsDialog::VBoxSettingsDialog (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI <QIMainDialog> (aParent)
    , mPolished (false)
    , mValid (true)
    , mSilent (true)
    , mIconLabel (new VBoxWarnIconLabel (this))
    , mWhatsThisTimer (new QTimer (this))
    , mWhatsThisCandidate (0)
{
    /* Apply UI decorations */
    Ui::VBoxSettingsDialog::setupUi (this);

//    setToolbar (new VBoxToolBar (this));
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

    QGridLayout *mainLayout = static_cast <QGridLayout*> (mAllWidget->layout());
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    mLbTitle->hide();
    mLbWhatsThis->hide();
    mSelector = new VBoxSettingsToolBarSelector (this);
    static_cast <VBoxToolBar*> (mSelector->widget())->setMacToolbar();
    addToolBar (qobject_cast <QToolBar*> (mSelector->widget()));
    /* No title in this mode, we change the title of the window. */
    mainLayout->setColumnMinimumWidth (0, 0);
    mainLayout->setHorizontalSpacing (0);
#else
    /* Create the classical tree view selector */
    mSelector = new VBoxSettingsTreeViewSelector (this);
    mainLayout->addWidget (mSelector->widget(), 0, 0, 3, 1);
    mSelector->widget()->setFocus();
    mainLayout->setSpacing (10);
#endif

    /* Creating stack of pages */
    mStack = new QStackedWidget (mWtStackHandler);
    QVBoxLayout *layout = new QVBoxLayout (mWtStackHandler);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mStack);

    /* Setup whatsthis stuff */
    setWhatsThis (tr ("<i>Select a settings category from the list on the left side and move the mouse over a settings item to get more information</i>."));
    qApp->installEventFilter (this);
    mWhatsThisTimer->setSingleShot (true);
    connect (mWhatsThisTimer, SIGNAL (timeout()), this, SLOT (updateWhatsThis()));
    mLbWhatsThis->setAutoFillBackground (true);
    QPalette pal = mLbWhatsThis->palette();
    pal.setBrush (QPalette::Window, pal.brush (QPalette::Midlight));
    mLbWhatsThis->setPalette (pal);
    mLbWhatsThis->setFixedHeight (mLbWhatsThis->frameWidth() * 2 +
                                  mLbWhatsThis->margin() * 2 +
                                  mLbWhatsThis->fontMetrics().lineSpacing() * 4);

    /* Setup error & warning stuff */
    mButtonBox->addExtraWidget (mIconLabel);
    mErrorIcon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxCritical, this).pixmap (16, 16);
    mWarnIcon = vboxGlobal().standardIcon (QStyle::SP_MessageBoxWarning, this).pixmap (16, 16);

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

void VBoxSettingsDialog::revalidate (QIWidgetValidator *aWval)
{
    /* Perform validations for particular page */
    VBoxSettingsPage *page = qobject_cast <VBoxSettingsPage*> (aWval->widget());
    AssertMsg (page, ("Every validator should corresponds a page!\n"));

    QString warning;
    QString title = mSelector->itemTextByPage (page);
    bool valid = page->revalidate (warning, title);
    warning = warning.isEmpty() ? QString::null :
        tr ("On the <b>%1</b> page, %2").arg (title, warning);
    aWval->setLastWarning (warning);
    valid ? setWarning (warning) : setError (warning);

    aWval->setOtherValid (valid);
}

void VBoxSettingsDialog::categoryChanged (int aId)
{
    QWidget *rootPage = mSelector->rootPage (aId);
#ifndef Q_WS_MAC
    mLbTitle->setText (mSelector->itemText (aId));
    mStack->setCurrentIndex (mStack->indexOf (rootPage));
#else /* Q_WS_MAC */
    QSize cs = size();
    /* First make all fully resizeable */
    setMinimumSize (QSize (minimumWidth(), 0));
    setMaximumSize (QSize (minimumWidth(), QWIDGETSIZE_MAX));
    for (int i = 0; i < mStack->count(); ++i)
        mStack->widget (i)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Ignored);
    int a = mStack->indexOf (rootPage);
    if (a < mSizeList.count())
    {
        QSize ss = mSizeList.at (a);
        mStack->widget (a)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Preferred);
        /* Switch to the new page first if we are shrinking */
        if (cs.height() > ss.height())
            mStack->setCurrentIndex (mStack->indexOf (rootPage));
        /* Do the animation */
        ::darwinWindowAnimateResize (this, QRect (x(), y(),
                                                  ss.width(), ss.height()));
        /* Switch to the new page last if we are zooming */
        if (cs.height() <= ss.height())
            mStack->setCurrentIndex (mStack->indexOf (rootPage));
        /* Make the widget fixed size */
        setFixedSize (ss);
    }
    ::darwinSetShowsResizeIndicator (this, false);
#endif /* !Q_WS_MAC */
# ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle (dialogTitle());
# endif
}

void VBoxSettingsDialog::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSettingsDialog::retranslateUi (this);

    mErrorHint = tr ("Invalid settings detected");
    mWarnHint = tr ("Non-optimal settings detected");
    if (!mValid)
        mIconLabel->setWarningText (mErrorHint);
    else if (!mSilent)
        mIconLabel->setWarningText (mWarnHint);

    QList <QIWidgetValidator*> vlist = findChildren <QIWidgetValidator*> ();

    /* Rename all validators to make them feat new language. */
    foreach (QIWidgetValidator *wval, vlist)
        wval->setCaption (mSelector->itemTextByPage (
            qobject_cast <VBoxSettingsPage*> (wval->widget())));

    /* Revalidate all pages to retranslate the warning messages also. */
    foreach (QIWidgetValidator *wval, vlist)
        if (!wval->isValid())
            revalidate (wval);
}

QString VBoxSettingsDialog::titleExtension() const
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    return mSelector->itemText (mSelector->currentId());
#else
    return tr ("Settings");
#endif
}

void VBoxSettingsDialog::setError (const QString &aError)
{
    mErrorString = aError.isEmpty() ? QString::null :
                   QString ("<font color=red>%1</font>").arg (aError);

    /* Not touching QILabel until dialog is polished otherwise
     * it can change its size to undefined */
    if (!mPolished)
        return;

    if (!mErrorString.isEmpty())
    {
#ifndef Q_WS_MAC
        mLbWhatsThis->setText (mErrorString);
#else /* Q_WS_MAC */
        mIconLabel->setToolTip (mErrorString);
#endif /* Q_WS_MAC */
    }
    else
        updateWhatsThis (true);
}

void VBoxSettingsDialog::setWarning (const QString &aWarning)
{
    mWarnString = aWarning.isEmpty() ? QString::null :
                  QString ("<font color=#ff5400>%1</font>").arg (aWarning);

    /* Not touching QILabel until dialog is polished otherwise
     * it can change its size to undefined */
    if (!mPolished)
        return;

    if (!mWarnString.isEmpty())
    {
#ifndef Q_WS_MAC
        mLbWhatsThis->setText (mWarnString);
#else /* Q_WS_MAC */
        mIconLabel->setToolTip (mWarnString);
#endif /* Q_WS_MAC */
    }
    else
        updateWhatsThis (true);
}

void VBoxSettingsDialog::addItem (const QString &aBigIcon,
                                  const QString &aBigIconDisabled,
                                  const QString &aSmallIcon,
                                  const QString &aSmallIconDisabled,
                                  int aId,
                                  const QString &aLink,
                                  VBoxSettingsPage* aPrefPage /* = NULL*/,
                                  int aParentId /* = -1 */)
{
    QWidget *page = mSelector->addItem (aBigIcon, aBigIconDisabled, aSmallIcon, aSmallIconDisabled,
                                        aId, aLink,
                                        aPrefPage, aParentId);
    if (page)
        mStack->addWidget (page);
    if (aPrefPage)
        attachValidator (aPrefPage);
}

void VBoxSettingsDialog::enableOk (const QIWidgetValidator*)
{
    QList <QIWidgetValidator*> vlist (findChildren <QIWidgetValidator*> ());

    /* Detect ERROR presence */
    {
        setError (QString::null);
        QString wvalError;
        bool newValid = true;
        foreach (QIWidgetValidator *wval, vlist)
        {
            newValid = wval->isValid();
            if (!newValid)
            {
                wvalError = wval->warningText();
                if (wvalError.isNull())
                    wvalError = wval->lastWarning();
                break;
            }
        }

        /* Try to set the generic error message when invalid
         * but no specific message is provided */
        if (mErrorString.isNull() && !wvalError.isNull())
            setError (wvalError);

        mValid = newValid;
        mIconLabel->setWarningPixmap (mErrorIcon);
        mIconLabel->setWarningText (mErrorHint);
        mIconLabel->setToolTip ("");
        mIconLabel->setVisible (!mValid);
        mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (mValid);

        if (!mValid) return;
    }

    /* Detect WARNING presence */
    {
        setWarning (QString::null);
        QString wvalWarning;
        bool newSilent = true;
        foreach (QIWidgetValidator *wval, vlist)
        {
            if (!wval->warningText().isNull() ||
                !wval->lastWarning().isNull())
            {
                newSilent = false;
                wvalWarning = wval->warningText();
                if (wvalWarning.isNull())
                    wvalWarning = wval->lastWarning();
                break;
            }
        }

        /* Try to set the generic error message when invalid
         * but no specific message is provided */
        if (mWarnString.isNull() && !wvalWarning.isNull())
            setWarning (wvalWarning);

        mSilent = newSilent;
        mIconLabel->setWarningPixmap (mWarnIcon);
        mIconLabel->setWarningText (mWarnHint);
        mIconLabel->setToolTip ("");
        mIconLabel->setVisible (!mSilent);
    }
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

    if (text.isEmpty() && !mErrorString.isEmpty())
        text = mErrorString;
    else if (text.isEmpty() && !mWarnString.isEmpty())
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

    QWidget *widget = static_cast <QWidget*> (aObject);
    if (widget->window() != this)
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

    int minWidth = mSelector->minWidth();
#ifdef Q_WS_MAC
    /* Remove all title bar buttons (Buggy Qt) */
    ::darwinSetHidesAllTitleButtons (this);

    /* Set all size policies to ignored */
    for (int i = 0; i < mStack->count(); ++i)
        mStack->widget (i)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Ignored);
    /* Activate every single page to get the optimal size */
    for (int i = mStack->count() - 1; i >= 0; --i)
    {
        mStack->widget (i)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Preferred);
        mStack->setCurrentIndex (i);
        layout()->activate();
        QSize s = minimumSize();
        if (minWidth > s.width())
            s.setWidth (minWidth);
        mSizeList.insert (0, s);
        mStack->widget (i)->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Ignored);
    }

    categoryChanged (mSelector->currentId());
#else /* Q_WS_MAC */
    /* Resize to the minimum possible size */
    QSize s = minimumSize();
    if (minWidth > s.width())
        s.setWidth (minWidth);
    resize (s);
#endif /* Q_WS_MAC */

    VBoxGlobal::centerWidget (this, parentWidget());
}

VBoxSettingsPage* VBoxSettingsDialog::attachValidator (VBoxSettingsPage *aPage)
{
    QIWidgetValidator *wval = new QIWidgetValidator (mSelector->itemTextByPage (aPage),
                                                     aPage, this);
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableOk (const QIWidgetValidator*)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));

    aPage->setValidator (wval);
    aPage->setOrderAfter (mSelector->widget());

    return aPage;
}

