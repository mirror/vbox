/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UICloneVMWizard class declaration
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UICloneVMWizard_h__
#define __UICloneVMWizard_h__

/* Local includes: */
#include "QIWizard.h"
#include "COMDefs.h"

/* Generated includes: */
#include "UICloneVMWizardPage1.gen.h"
#include "UICloneVMWizardPage2.gen.h"
#include "UICloneVMWizardPage3.gen.h"

/* Clone vm wizard class: */
class UICloneVMWizard : public QIWizard
{
    Q_OBJECT;

public:

    enum
    {
        PageIntro,
        PageType,
        PageMode
    };

    /* Constructor: */
    UICloneVMWizard(QWidget *pParent, const CMachine &machine, CSnapshot snapshot = CSnapshot());

    bool createClone(const QString &strName, KCloneMode mode, bool fReinitMACs, bool fLinked = false);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Private member vars */
    CMachine m_machine;
    CSnapshot m_snapshot;
};

/* Base wrapper for the wizard page
 * of the new clonevm wizard class: */
class UICloneVMWizardPage : public QIWizardPage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UICloneVMWizardPage() {}

protected:

    /* Returns parent wizard object: */
    UICloneVMWizard* wizard() const { return qobject_cast<UICloneVMWizard*>(QIWizardPage::wizard()); }
};

/* Page1 of the new clonevm wizard: */
class UICloneVMWizardPage1 : public UICloneVMWizardPage, public Ui::UICloneVMWizardPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString cloneName READ cloneName WRITE setCloneName);
    Q_PROPERTY(bool reinitMACs READ isReinitMACsChecked);

public:

    /* Constructor: */
    UICloneVMWizardPage1(const QString &strOriName);

    QString cloneName() const;
    void setCloneName(const QString &strName);

    bool isReinitMACsChecked() const;

protected:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();

    bool isComplete() const;
    bool validatePage();

private slots:

    void sltNameEditorTextChanged(const QString &strText);

private:

    /* Private member vars */
    QString m_strOriName;
};

/* Page2 of the new clonevm wizard: */
class UICloneVMWizardPage2 : public UICloneVMWizardPage, public Ui::UICloneVMWizardPage2
{
    Q_OBJECT;

public:

    /* Constructor: */
    UICloneVMWizardPage2(bool fAdditionalInfo);
    int nextId() const;

protected:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();

    bool validatePage();

private slots:

    void buttonClicked(QAbstractButton *pButton);

private:

    bool m_fAdditionalInfo;
};

/* Page3 of the new clonevm wizard: */
class UICloneVMWizardPage3 : public UICloneVMWizardPage, public Ui::UICloneVMWizardPage3
{
    Q_OBJECT;
    Q_PROPERTY(KCloneMode cloneMode READ cloneMode WRITE setCloneMode);

public:

    /* Constructor: */
    UICloneVMWizardPage3(bool fShowChildsOption = true);

protected:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();

    bool validatePage();

    KCloneMode cloneMode() const;
    void setCloneMode(KCloneMode mode);

private:

    bool m_fShowChildsOption;
};

#endif // __UICloneVMWizard_h__

