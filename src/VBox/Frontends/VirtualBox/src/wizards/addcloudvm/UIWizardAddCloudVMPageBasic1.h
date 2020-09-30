/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVMPageBasic1 class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include "UIWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CCloudProfile.h"
#include "CCloudProvider.h"
#include "CCloudProviderManager.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QListWidget;
class QTableWidget;
class QIComboBox;
class QIRichTextLabel;
class QIToolButton;

/** Source combo data fields. */
enum
{
    SourceData_ID        = Qt::UserRole + 1,
    SourceData_Name      = Qt::UserRole + 2,
    SourceData_ShortName = Qt::UserRole + 3
};

/** Profile combo data fields. */
enum
{
    ProfileData_Name = Qt::UserRole + 1
};

/** UIWizardPageBase extension for 1st page of the Add Cloud VM wizard. */
class UIWizardAddCloudVMPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardAddCloudVMPage1();

    /** Populates sources. */
    void populateSources();
    /** Populates profiles. */
    void populateProfiles();
    /** Populates profile. */
    void populateProfile();
    /** Populates profile instances. */
    void populateProfileInstances();

    /** Updates source combo tool-tips. */
    void updateSourceComboToolTip();

    /** Defines @a strSource. */
    void setSource(const QString &strSource);
    /** Returns source. */
    QString source() const;
    /** Returns source ID. */
    QUuid sourceId() const;

    /** Returns profile name. */
    QString profileName() const;
    /** Returns instance IDs. */
    QStringList instanceIds() const;

    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient);
    /** Returns Cloud Client object. */
    CCloudClient client() const;

    /** Holds whether starting page was polished. */
    bool  m_fPolished;

    /** Holds the Cloud Provider Manager reference. */
    CCloudProviderManager  m_comCloudProviderManager;
    /** Holds the Cloud Provider object reference. */
    CCloudProvider         m_comCloudProvider;
    /** Holds the Cloud Profile object reference. */
    CCloudProfile          m_comCloudProfile;

    /** Holds the source layout instance. */
    QGridLayout *m_pSourceLayout;
    /** Holds the source type label instance. */
    QLabel      *m_pSourceLabel;
    /** Holds the source type combo-box instance. */
    QIComboBox  *m_pSourceComboBox;

    /** Holds the cloud container layout instance. */
    QGridLayout  *m_pCloudContainerLayout;
    /** Holds the profile label instance. */
    QLabel       *m_pProfileLabel;
    /** Holds the profile combo-box instance. */
    QIComboBox   *m_pProfileComboBox;
    /** Holds the profile management tool-button instance. */
    QIToolButton *m_pProfileToolButton;
    /** Holds the profile instance label instance. */
    QLabel       *m_pProfileInstanceLabel;
    /** Holds the profile instance list instance. */
    QListWidget  *m_pProfileInstanceList;
};

/** UIWizardPage extension for 1st page of the Add Cloud VM wizard, extends UIWizardAddCloudVMPage1 as well. */
class UIWizardAddCloudVMPageBasic1 : public UIWizardPage, public UIWizardAddCloudVMPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source);
    Q_PROPERTY(QString profileName READ profileName);
    Q_PROPERTY(QStringList instanceIds READ instanceIds);

public:

    /** Constructs 1st basic page. */
    UIWizardAddCloudVMPageBasic1();

protected:

    /** Allows access wizard from base part. */
    virtual UIWizard *wizardImp() const /* override */ { return UIWizardPage::wizard(); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles change in source combo-box. */
    void sltHandleSourceChange();

    /** Handles change in profile combo-box. */
    void sltHandleProfileComboChange();

    /** Handles profile tool-button click. */
    void sltHandleProfileButtonClick();

private:

    /** Holds the main label instance. */
    QIRichTextLabel *m_pLabelMain;
    /** Holds the description label instance. */
    QIRichTextLabel *m_pLabelDescription;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_addcloudvm_UIWizardAddCloudVMPageBasic1_h */
