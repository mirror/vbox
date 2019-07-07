/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic1 class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h
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
#include "CVirtualSystemDescription.h"
#include "CVirtualSystemDescriptionForm.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QListWidget;
class QTableWidget;
class QIComboBox;
class QIRichTextLabel;
class QIToolButton;

/** Destination combo data fields. */
enum
{
    DestinationData_ID              = Qt::UserRole + 1,
    DestinationData_Name            = Qt::UserRole + 2,
    DestinationData_ShortName       = Qt::UserRole + 3
};

/** Account combo data fields. */
enum
{
    AccountData_ProfileName = Qt::UserRole + 1
};

/** UIWizardPageBase extension for 1st page of the New Cloud VM wizard. */
class UIWizardNewCloudVMPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardNewCloudVMPage1();

    /** Populates destinations. */
    void populateDestinations();
    /** Populates accounts. */
    void populateAccounts();
    /** Populates account properties. */
    void populateAccountProperties();
    /** Populates account images. */
    void populateAccountImages();
    /** Populates form properties. */
    void populateFormProperties();

    /** Updates destination combo tool-tips. */
    void updateDestinationComboToolTip();
    /** Updates account property table tool-tips. */
    void updateAccountPropertyTableToolTips();
    /** Adjusts account property table. */
    void adjustAccountPropertyTable();

    /** Defines @a strDestination. */
    void setDestination(const QString &strDestination);
    /** Returns destination. */
    QString destination() const;
    /** Returns destination ID. */
    QUuid destinationId() const;

    /** Returns profile name. */
    QString profileName() const;
    /** Returns image ID. */
    QString imageId() const;

    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient);
    /** Returns Cloud Client object. */
    CCloudClient client() const;

    /** Defines Virtual System @a comDescription object. */
    void setVSD(const CVirtualSystemDescription &comDescription);
    /** Returns Virtual System Description object. */
    CVirtualSystemDescription vsd() const;

    /** Defines Virtual System Description @a comForm object. */
    void setVSDForm(const CVirtualSystemDescriptionForm &comForm);
    /** Returns Virtual System Description Form object. */
    CVirtualSystemDescriptionForm vsdForm() const;

    /** Holds whether starting page was polished. */
    bool  m_fPolished;

    /** Holds the Cloud Provider Manager reference. */
    CCloudProviderManager  m_comCloudProviderManager;
    /** Holds the Cloud Provider object reference. */
    CCloudProvider         m_comCloudProvider;
    /** Holds the Cloud Profile object reference. */
    CCloudProfile          m_comCloudProfile;

    /** Holds the destination layout instance. */
    QGridLayout *m_pDestinationLayout;
    /** Holds the destination type label instance. */
    QLabel      *m_pDestinationLabel;
    /** Holds the destination type combo-box instance. */
    QIComboBox  *m_pDestinationComboBox;

    /** Holds the cloud container layout instance. */
    QGridLayout  *m_pCloudContainerLayout;
    /** Holds the account label instance. */
    QLabel       *m_pAccountLabel;
    /** Holds the account combo-box instance. */
    QIComboBox   *m_pAccountComboBox;
    /** Holds the account management tool-button instance. */
    QIToolButton *m_pAccountToolButton;
    /** Holds the account property table instance. */
    QTableWidget *m_pAccountPropertyTable;
    /** Holds the account image label instance. */
    QLabel       *m_pAccountImageLabel;
    /** Holds the account image list instance. */
    QListWidget  *m_pAccountImageList;
};

/** UIWizardPage extension for 1st page of the New Cloud VM wizard, extends UIWizardNewCloudVMPage1 as well. */
class UIWizardNewCloudVMPageBasic1 : public UIWizardPage, public UIWizardNewCloudVMPage1
{
    Q_OBJECT;

public:

    /** Constructs 1st basic page. */
    UIWizardNewCloudVMPageBasic1();

protected:

    /** Allows access wizard from base part. */
    virtual UIWizard *wizardImp() const /* override */ { return UIWizardPage::wizard(); }

    /** Handle any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private slots:

    /** Handles change in destination combo-box. */
    void sltHandleDestinationChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();

    /** Handles account tool-button click. */
    void sltHandleAccountButtonClick();

private:

    /** Holds the main label instance. */
    QIRichTextLabel *m_pLabelMain;
    /** Holds the description label instance. */
    QIRichTextLabel *m_pLabelDescription;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h */
