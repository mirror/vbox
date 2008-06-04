/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIAbstractWizard class implementation
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

#include "QIAbstractWizard.h"
#include "QILabel.h"

/* Qt includes */
#include <QStackedWidget>
#include <QPushButton>
#include <QScrollBar>
#include <VBoxGlobal.h>

QITextEdit::QITextEdit (QWidget *aParent)
    : QTextEdit (aParent)
{
    /* Disable the background painting of the editor widget. */
    viewport()->setAutoFillBackground (false);
    /* Make editor readonly by default. */
    setReadOnly (true);
}

QSize QITextEdit::sizeHint() const
{
    /* If there is an updated sizeHint() present - using it. */
    return mOwnSizeHint.isValid() ? mOwnSizeHint : QTextEdit::sizeHint();
}

QSize QITextEdit::minimumSizeHint() const
{
    /* Viewer should not influent layout if there is no text. */
    return QSize (0, 0);
}

void QITextEdit::updateSizeHint()
{
    if (document())
    {
        int w = (int)document()->size().width();
        int h = (int)document()->size().height() + 2;
        if (horizontalScrollBar()->isVisible())
            h += horizontalScrollBar()->height();
        mOwnSizeHint = QSize (w, h);
    }
    updateGeometry();
}

void QITextEdit::setText (const QString &aText)
{
    QTextEdit::setText (aText);
    updateSizeHint();
}


QIAbstractWizard::QIAbstractWizard (QWidget *aParent,
                                    Qt::WindowFlags aFlags)
    : QDialog (aParent, aFlags)
    , mStackedWidget (0)
    , mFinishButton (0)
{
}

void QIAbstractWizard::initializeWizardHdr()
{
    /* Search for widget stack of inherited dialog.
     * Please note what widget stack should have objectName()
     * equal "mPageStack". */
    mStackedWidget = findChild<QStackedWidget*> ("mPageStack");
    Assert (mStackedWidget);
    connect (mStackedWidget, SIGNAL (currentChanged (int)),
             this, SLOT (onPageShow()));
    mStackedWidget->setCurrentIndex (0);

    /* Assigning all next buttons of inherited dialog.
     * Please note what all next buttons should have objectName()
     * matching mBtnNext<number> rule. */
    for (int i = 0; i < mStackedWidget->count() - 1; ++ i)
    {
        QWidget *page = mStackedWidget->widget (i);
        QList<QPushButton*> list =
            page->findChildren<QPushButton*> (QRegExp ("mBtnNext\\d*"));
        Assert (list.count() <= 1);
        if (list.count() == 1)
        {
            connect (list [0], SIGNAL (clicked()), this, SLOT (showNextPage()));
            mNextButtons << list [0];
        }
    }

    /* Assigning all back buttons of inherited dialog.
     * Please note what all back buttons should have objectName()
     * matching mBtnBack<number> rule. */
    for (int i = 0; i < mStackedWidget->count(); ++ i)
    {
        QWidget *page = mStackedWidget->widget (i);
        QList<QPushButton*> list =
            page->findChildren<QPushButton*> (QRegExp ("mBtnBack\\d*"));
        Assert (list.count() <= 1);
        if (list.count() == 1)
        {
            connect (list [0], SIGNAL (clicked()), this, SLOT (showBackPage()));
            mBackButtons << list [0];
        }
    }

    /* Assigning all cancel buttons of inherited dialog.
     * Please note what all cancel buttons should have objectName()
     * matching mBtnCancel<number> rule. */
    for (int i = 0; i < mStackedWidget->count(); ++ i)
    {
        QWidget *page = mStackedWidget->widget (i);
        QList<QPushButton*> list =
            page->findChildren<QPushButton*> (QRegExp ("mBtnCancel\\d*"));
        Assert (list.count() <= 1);
        if (list.count() == 1)
        {
            connect (list [0], SIGNAL (clicked()), this, SLOT (reject()));
            mCancelButtons << list [0];
        }
    }

    /* Assigning finish button of inherited dialog.
     * Please note what finish button should have objectName()
     * matching mBtnFinish. */
    mFinishButton = findChild<QPushButton*> ("mBtnFinish");
    Assert (mFinishButton);
    connect (mFinishButton, SIGNAL (clicked()), this, SLOT (accept()));

    /* Decorating all wizard logo images of inherited dialog.
     * Please note what all logo images should have objectName()
     * matching mLogo<subname> rule. */
    QList<QLabel*> logoImages =
        findChildren<QLabel*> (QRegExp ("mLogo.+"));
    Assert (logoImages.count());
    for (int i = 0; i < logoImages.count(); ++ i)
        VBoxGlobal::adoptLabelPixmap (logoImages [i]);
}

void QIAbstractWizard::initializeWizardFtr()
{
    /* Update sizeHint() of all text labels of inherited dialog.
     * Please note what all text labels should have objectName()
     * matching mText<subname> rule. */
    QList<QILabel*> textLabels =
        findChildren<QILabel*> (QRegExp ("mText.+"));
    for (int i = 0; i < textLabels.count(); ++ i)
        textLabels [i]->updateSizeHint();

    /* Update sizeHint() of summary viewer of inherited dialog.
     * Please note what summary viewer should have objectName()
     * matching mTeSummary. */
    QITextEdit *teSummary = findChild<QITextEdit*> ("mTeSummary");
    if (teSummary)
        teSummary->updateSizeHint();
}

QPushButton* QIAbstractWizard::nextButton (QWidget *aOfPage)
{
    return getButton (aOfPage ? aOfPage : mStackedWidget->currentWidget(),
                      "mBtnNext\\d*");
}

QPushButton* QIAbstractWizard::backButton (QWidget *aOfPage)
{
    return getButton (aOfPage ? aOfPage : mStackedWidget->currentWidget(),
                      "mBtnBack\\d*");
}

QPushButton* QIAbstractWizard::cancelButton (QWidget *aOfPage)
{
    return getButton (aOfPage ? aOfPage : mStackedWidget->currentWidget(),
                      "mBtnCancel\\d*");
}

QPushButton* QIAbstractWizard::finishButton()
{
    return getButton (mStackedWidget->widget (mStackedWidget->count() - 1),
                      "mBtnFinish");
}

void QIAbstractWizard::showEvent (QShowEvent *aEvent)
{
    QDialog::showEvent (aEvent);

    resize (minimumSize());

    onPageShow();

    VBoxGlobal::centerWidget (this, parentWidget());
}

void QIAbstractWizard::showNextPage()
{
    /* Switch to the next page */
    mStackedWidget->setCurrentIndex (nextButtonIndex (nextButton()) + 1);
}

void QIAbstractWizard::showBackPage()
{
    /* Switch to the previous page */
    mStackedWidget->setCurrentIndex (backButtonIndex (backButton()) - 1);
}

int QIAbstractWizard::nextButtonIndex (QPushButton *aNextButton)
{
    return mNextButtons.indexOf (aNextButton);
}

int QIAbstractWizard::backButtonIndex (QPushButton *aBackButton)
{
    return mBackButtons.indexOf (aBackButton);
}

QPushButton* QIAbstractWizard::getButton (QWidget *aOfPage,
                                                  const QString &aRegExp)
{
    Assert (aOfPage && !aRegExp.isNull());
    QList<QPushButton*> buttonsList =
        aOfPage->findChildren<QPushButton*> (QRegExp (aRegExp));
    Assert (buttonsList.count() == 1);
    return buttonsList [0];
}

