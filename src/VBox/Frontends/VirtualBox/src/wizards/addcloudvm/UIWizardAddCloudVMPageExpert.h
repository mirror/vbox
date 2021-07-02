/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageExpert class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardAddCloudVMPageBasic1.h"

/* Forward declarations: */
class QGroupBox;

/** UINativeWizardPage extension for Expert page of the Add Cloud VM wizard,
  * based on UIWizardAddCloudVMPage1 namespace functions. */
class UIWizardAddCloudVMPageExpert : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs expert page. */
    UIWizardAddCloudVMPageExpert();

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
    void setShortProviderName(const QString &strShortProviderName);
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

    /** Holds the provider container instance. */
    QGroupBox *m_pCntProvider;

    /** Holds the provider layout instance. */
    QGridLayout *m_pProviderLayout;
    /** Holds the provider type label instance. */
    QLabel      *m_pProviderLabel;
    /** Holds the provider type combo-box instance. */
    QIComboBox  *m_pProviderComboBox;

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

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageExpert_h */
