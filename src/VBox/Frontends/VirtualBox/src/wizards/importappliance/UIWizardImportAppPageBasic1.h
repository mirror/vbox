/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardImportAppDefs.h"
#include "UIWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CCloudProfile.h"
#include "CCloudProvider.h"
#include "CCloudProviderManager.h"
#include "CVirtualSystemDescriptionForm.h"

/* Forward declarations: */
class QLabel;
class QListWidget;
class QTableWidget;
class QStackedLayout;
class QIComboBox;
class QIRichTextLabel;
class QIToolButton;
class UIEmptyFilePathSelector;

/** Source combo data fields. */
enum
{
    SourceData_ID              = Qt::UserRole + 1,
    SourceData_Name            = Qt::UserRole + 2,
    SourceData_ShortName       = Qt::UserRole + 3,
    SourceData_IsItCloudFormat = Qt::UserRole + 4
};

/** Account combo data fields. */
enum
{
    AccountData_ProfileName = Qt::UserRole + 1
};

/** UIWizardPageBase extension for 1st page of the Import Appliance wizard. */
class UIWizardImportAppPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardImportAppPage1(bool fImportFromOCIByDefault);

    /** Populates sources. */
    void populateSources();
    /** Populates accounts. */
    void populateAccounts();
    /** Populates account properties. */
    void populateAccountProperties();
    /** Populates account instances. */
    void populateAccountInstances();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Updates source combo tool-tips. */
    void updateSourceComboToolTip();
    /** Updates account property table tool-tips. */
    void updateAccountPropertyTableToolTips();
    /** Adjusts account property table. */
    void adjustAccountPropertyTable();

    /** Defines @a strSource. */
    void setSource(const QString &strSource);
    /** Returns source. */
    QString source() const;
    /** Returns whether source under certain @a iIndex is cloud one. */
    bool isSourceCloudOne(int iIndex = -1) const;

    /** Returns source ID. */
    QUuid sourceId() const;
    /** Returns profile name. */
    QString profileName() const;
    /** Returns Cloud Profile object. */
    CCloudProfile profile() const;
    /** Returns Virtual System Description Form object. */
    CVirtualSystemDescriptionForm vsdForm() const;

    /** Holds whether default source should be Import from OCI. */
    bool  m_fImportFromOCIByDefault;

    /** Holds the Cloud Provider Manager reference. */
    CCloudProviderManager          m_comCloudProviderManager;
    /** Holds the Cloud Provider object reference. */
    CCloudProvider                 m_comCloudProvider;
    /** Holds the Cloud Profile object reference. */
    CCloudProfile                  m_comCloudProfile;
    /** Holds the Cloud Client object reference. */
    CCloudClient                   m_comCloudClient;
    /** Holds the Virtual System Description Form object reference. */
    CVirtualSystemDescriptionForm  m_comVSDForm;

    /** Holds the source layout instance. */
    QGridLayout *m_pSourceLayout;
    /** Holds the source type label instance. */
    QLabel      *m_pSourceLabel;
    /** Holds the source type combo-box instance. */
    QIComboBox  *m_pSourceComboBox;

    /** Holds the stacked layout instance. */
    QStackedLayout *m_pStackedLayout;

    /** Holds the local container layout instance. */
    QGridLayout             *m_pLocalContainerLayout;
    /** Holds the file label instance. */
    QLabel                  *m_pFileLabel;
    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;

    /** Holds the cloud container layout instance. */
    QGridLayout  *m_pCloudContainerLayout;
    /** Holds the account label instance. */
    QLabel       *m_pAccountLabel;
    /** Holds the account combo-box instance. */
    QComboBox    *m_pAccountComboBox;
    /** Holds the account management tool-button instance. */
    QIToolButton *m_pAccountToolButton;
    /** Holds the account property table instance. */
    QTableWidget *m_pAccountPropertyTable;
    /** Holds the account instance label instance. */
    QLabel       *m_pAccountInstanceLabel;
    /** Holds the account instance list instance. */
    QListWidget  *m_pAccountInstanceList;
};

/** UIWizardPage extension for 1st page of the Import Appliance wizard, extends UIWizardImportAppPage1 as well. */
class UIWizardImportAppPageBasic1 : public UIWizardPage, public UIWizardImportAppPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString source READ source WRITE setSource);
    Q_PROPERTY(bool isSourceCloudOne READ isSourceCloudOne);
    Q_PROPERTY(CCloudProfile profile READ profile);
    Q_PROPERTY(CVirtualSystemDescriptionForm vsdForm READ vsdForm);

public:

    /** Constructs 1st basic page. */
    UIWizardImportAppPageBasic1(bool fImportFromOCIByDefault);

protected:

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

    /** Updates page appearance. */
    virtual void updatePageAppearance() /* override */;

private slots:

    /** Handles import source change. */
    void sltHandleSourceChange();

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

#endif /* !FEQT_INCLUDED_SRC_wizards_importappliance_UIWizardImportAppPageBasic1_h */
