/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic1 class declaration.
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

/** Location combo data fields. */
enum
{
    LocationData_ID              = Qt::UserRole + 1,
    LocationData_Name            = Qt::UserRole + 2,
    LocationData_ShortName       = Qt::UserRole + 3
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

    /** Populates locations. */
    void populateLocations();
    /** Populates accounts. */
    void populateAccounts();
    /** Populates account properties. */
    void populateAccountProperties();
    /** Populates source images. */
    void populateSourceImages();
    /** Populates form properties. */
    void populateFormProperties();

    /** Updates location combo tool-tips. */
    void updateLocationComboToolTip();
    /** Updates account property table tool-tips. */
    void updateAccountPropertyTableToolTips();
    /** Adjusts account property table. */
    void adjustAccountPropertyTable();

    /** Defines @a strLocation. */
    void setLocation(const QString &strLocation);
    /** Returns location. */
    QString location() const;
    /** Returns location ID. */
    QUuid locationId() const;

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

    /** Holds the location type label instance. */
    QLabel      *m_pLocationLabel;
    /** Holds the location type combo-box instance. */
    QIComboBox  *m_pLocationComboBox;

    /** Holds the account label instance. */
    QLabel       *m_pAccountLabel;
    /** Holds the account combo-box instance. */
    QIComboBox   *m_pAccountComboBox;
    /** Holds the account management tool-button instance. */
    QIToolButton *m_pAccountToolButton;
    /** Holds the account property table instance. */
    QTableWidget *m_pAccountPropertyTable;
    /** Holds the source image label instance. */
    QLabel       *m_pSourceImageLabel;
    /** Holds the source image list instance. */
    QListWidget  *m_pSourceImageList;
};

/** UIWizardPage extension for 1st page of the New Cloud VM wizard, extends UIWizardNewCloudVMPage1 as well. */
class UIWizardNewCloudVMPageBasic1 : public UIWizardPage, public UIWizardNewCloudVMPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString location READ location);
    Q_PROPERTY(QString profileName READ profileName);

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

    /** Handles change in location combo-box. */
    void sltHandleLocationChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();

    /** Handles account tool-button click. */
    void sltHandleAccountButtonClick();

private:

    /** Holds the main label instance. */
    QIRichTextLabel *m_pLabelMain;
    /** Holds the location layout instance. */
    QGridLayout     *m_pLocationLayout;
    /** Holds the description label instance. */
    QIRichTextLabel *m_pLabelDescription;
    /** Holds the options layout instance. */
    QGridLayout     *m_pOptionsLayout;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h */
