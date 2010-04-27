/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIFirstRunWzd class declaration
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIFirstRunWzd_h__
#define __UIFirstRunWzd_h__

/* Local includes */
#include "QIWizard.h"
#include "COMDefs.h"

/* Generated includes */
#include "UIFirstRunWzdPage1.gen.h"
#include "UIFirstRunWzdPage2.gen.h"
#include "UIFirstRunWzdPage3.gen.h"

class UIFirstRunWzd : public QIWizard
{
    Q_OBJECT;

public:

    UIFirstRunWzd(QWidget *pParent, const CMachine &machine);

protected:

    void retranslateUi();
};

class UIFirstRunWzdPage1 : public QIWizardPage, public Ui::UIFirstRunWzdPage1
{
    Q_OBJECT;

public:

    UIFirstRunWzdPage1();

    void init();

protected:

    void retranslateUi();

    void initializePage();
};

class UIFirstRunWzdPage2 : public QIWizardPage, public Ui::UIFirstRunWzdPage2
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source WRITE setSource);
    Q_PROPERTY(QString id READ id WRITE setId);

public:

    UIFirstRunWzdPage2();

    void init();

protected:

    void retranslateUi();

    void initializePage();

    bool isComplete() const;

private slots:

    void sltMediumChanged();
    void sltOpenVirtualMediaManager();

private:

    QString source() const { return m_strSource; }
    void setSource(const QString &strSource) { m_strSource = strSource; }
    QString m_strSource;

    QString id() const { return m_strId; }
    void setId(const QString &strId) { m_strId = strId; }
    QString m_strId;
};

class UIFirstRunWzdPage3 : public QIWizardPage, public Ui::UIFirstRunWzdPage3
{
    Q_OBJECT;
    Q_PROPERTY(CMachine machine READ machine WRITE setMachine);

public:

    UIFirstRunWzdPage3();

    void init();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    bool validatePage();

private:

    bool insertDevice();

    CMachine machine() const { return m_Machine; }
    void setMachine(CMachine &machine) { m_Machine = machine; }
    CMachine m_Machine;
};

Q_DECLARE_METATYPE(CMachine);

#endif // __UIFirstRunWzd_h__

