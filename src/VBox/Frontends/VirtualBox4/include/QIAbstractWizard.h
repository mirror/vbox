/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIAbstractWizard class declaration
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

#ifndef __QIAbstractWizard_h__
#define __QIAbstractWizard_h__

/* Qt includes */
#include <QTextEdit>
#include <QLabel>
#include <QDialog>

class QStackedWidget;

/**
 *  QTextEdit reimplementation used as summary viewer in different wizards:
 *  1. Enforces parent layout to ignore viewer if there is no text.
 *  2. Updates sizeHint() & geometry after setting new text for getting
 *     more compact layout.
 *  3. Tries to take into account horizontal scrollbar if present to avoid
 *     vertical scrollbar appearing.
 *  4. Makes the paper color similar to the background color.
 */
class QITextEdit : public QTextEdit
{
    Q_OBJECT;

public:

    QITextEdit (QWidget *aParent);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    void updateSizeHint();

public slots:

    void setText (const QString &aText);

private:

    QSize mOwnSizeHint;
};

/**
 *  QLabel reimplementation used as text viewers in different wizards:
 *  1. Updates sizeHint() on request to feat some minimum width
 *     for getting more compact layout.
 *  2. Tries to take into account horizontal scrollbar if present to avoid
 *     vertical scrollbar appearing.
 *
 * --> uses now global QILabel
 */

/**
 *  QDialog reimplementation used as abstract interface in different wizards:
 *  1. Performs most of initializations for inherited wizards including
 *     next, back, cancel, finish buttons, icons and text labels.
 *  2. Handles show event common for all wizards.
 */
class QIAbstractWizard : public QDialog
{
    Q_OBJECT;

public:

    QIAbstractWizard (QWidget *aParent = 0, Qt::WindowFlags aFlags = 0);

protected:

    void initializeWizardHdr();
    void initializeWizardFtr();

    QPushButton* nextButton (QWidget *aOfPage = 0);
    QPushButton* backButton (QWidget *aOfPage = 0);
    QPushButton* cancelButton (QWidget *aOfPage = 0);
    QPushButton* finishButton();

    void showEvent (QShowEvent *aEvent);

protected slots:

    virtual void showNextPage();
    virtual void showBackPage();
    virtual void onPageShow() = 0;

private:

    int nextButtonIndex (QPushButton *aNextButton);
    int backButtonIndex (QPushButton *aBackButton);
    QPushButton* getButton (QWidget *aOfPage, const QString &aRegExp);

    QStackedWidget      *mStackedWidget;
    QList<QPushButton*>  mNextButtons;
    QList<QPushButton*>  mBackButtons;
    QList<QPushButton*>  mCancelButtons;
    QPushButton         *mFinishButton;
};

#endif // __QIAbstractWizard_h__

