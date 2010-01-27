/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIRegistrationWzd class declaration
 */

/*
 * Copyright (C) 2008-2010 Sun Microsystems, Inc.
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

#ifndef __UIRegistrationWzd_h__
#define __UIRegistrationWzd_h__

/* Local includes */
#include "QIWizard.h"

/* Generated includes */
#include "UIRegistrationWzdPage1.gen.h"

class UIRegistrationWzd : public QIWizard
{
    Q_OBJECT;

public:

    static bool hasToBeShown();

    UIRegistrationWzd(UIRegistrationWzd **ppThis);
   ~UIRegistrationWzd();

protected:

    void retranslateUi();

private slots:

    void reject();

private:

    UIRegistrationWzd **m_ppThis;
};

class UIRegistrationWzdPage1 : public QIWizardPage, public Ui::UIRegistrationWzdPage1
{
    Q_OBJECT;

public:

    UIRegistrationWzdPage1();

protected:

    void retranslateUi();

    bool isComplete() const;
    bool validatePage();

private slots:

    void sltAccountTypeChanged();

private:

    void populateCountries();

    bool isFieldValid(QWidget *pWidget) const;
};

#endif // __UIRegistrationWzd_h__
