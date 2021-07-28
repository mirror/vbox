/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageBasic1 class declaration.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QListWidget;
class QIComboBox;
class QIRichTextLabel;
class QIToolButton;

/** Provider combo data fields. */
enum
{
    ProviderData_Name      = Qt::UserRole + 1,
    ProviderData_ShortName = Qt::UserRole + 2
};

/** Profile combo data fields. */
enum
{
    ProfileData_Name = Qt::UserRole + 1
};

/** Namespace for 1st page of the Add Cloud VM wizard. */
namespace UIWizardAddCloudVMPage1
{
    /** Populates @a pCombo with known providers. */
    void populateProviders(QIComboBox *pCombo);
    /** Populates @a pCombo with known profiles of @a comProvider specified. */
    void populateProfiles(QIComboBox *pCombo, const CCloudProvider &comProvider);
    /** Populates @a pList with profile instances available in @a comClient. */
    void populateProfileInstances(QListWidget *pList, const CCloudClient &comClient);

    /** Updates @a pCombo tool-tips. */
    void updateComboToolTip(QIComboBox *pCombo);

    /** Returns current user data for @a pList specified. */
    QStringList currentListWidgetData(QListWidget *pList);
}

/** UINativeWizardPage extension for 1st page of the Add Cloud VM wizard,
  * based on UIWizardAddCloudVMPage1 namespace functions. */
class UIWizardAddCloudVMPageBasic1 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs 1st basic page. */
    UIWizardAddCloudVMPageBasic1();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override final */;

    /** Performs page initialization. */
    virtual void initializePage() /* override final */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override final */;

    /** Performs page validation. */
    virtual bool validatePage() /* override final */;

private slots:

    /** Handles change in provider combo-box. */
    void sltHandleProviderComboChange();

    /** Handles change in profile combo-box. */
    void sltHandleProfileComboChange();
    /** Handles profile tool-button click. */
    void sltHandleProfileButtonClick();

    /** Handles change in instance list. */
    void sltHandleSourceInstanceChange();

private:

    /** Defines short provider name. */
    void setShortProviderName(const QString &strProviderShortName);
    /** Returns profile name. */
    QString shortProviderName() const;

    /** Defines profile name. */
    void setProfileName(const QString &strProfileName);
    /** Returns profile name. */
    QString profileName() const;

    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient);
    /** Returns Cloud Client object. */
    CCloudClient client() const;

    /** Defines @a instanceIds. */
    void setInstanceIds(const QStringList &instanceIds);
    /** Returns instance IDs. */
    QStringList instanceIds() const;

    /** Updates provider. */
    void updateProvider();
    /** Updates profile. */
    void updateProfile();
    /** Updates source instance. */
    void updateSourceInstance();

    /** Holds the main label instance. */
    QIRichTextLabel *m_pLabelMain;

    /** Holds the provider layout instance. */
    QGridLayout *m_pProviderLayout;
    /** Holds the provider type label instance. */
    QLabel      *m_pProviderLabel;
    /** Holds the provider type combo-box instance. */
    QIComboBox  *m_pProviderComboBox;

    /** Holds the description label instance. */
    QIRichTextLabel *m_pLabelDescription;

    /** Holds the options layout instance. */
    QGridLayout  *m_pOptionsLayout;
    /** Holds the profile label instance. */
    QLabel       *m_pProfileLabel;
    /** Holds the profile combo-box instance. */
    QIComboBox   *m_pProfileComboBox;
    /** Holds the profile management tool-button instance. */
    QIToolButton *m_pProfileToolButton;
    /** Holds the source instance label instance. */
    QLabel       *m_pSourceInstanceLabel;
    /** Holds the source instance list instance. */
    QListWidget  *m_pSourceInstanceList;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageBasic1_h */
