/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizardPage.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CVirtualSystemDescription.h"
#include "CVirtualSystemDescriptionForm.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QListWidget;
class QTabBar;
class QIComboBox;
class QIRichTextLabel;
class QIToolButton;
class CCloudProvider;

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

/** Namespace for 1st page of the New Cloud VM wizard. */
namespace UIWizardNewCloudVMPage1
{
    /** Populates @a pCombo with known providers. */
    void populateProviders(QIComboBox *pCombo);
    /** Populates @a pCombo with known profiles of @a comProvider specified. */
    void populateProfiles(QIComboBox *pCombo, const CCloudProvider &comProvider);
    /** Populates @a pList with source images from tab of @a pTabBar available in @a comClient. */
    void populateSourceImages(QListWidget *pList, QTabBar *pTabBar, const CCloudClient &comClient);
    /** Populates @a comVSD with form property suitable for tab of @a pTabBar with @a strImageId value. */
    void populateFormProperties(CVirtualSystemDescription comVSD, QTabBar *pTabBar, const QString &strImageId);

    /** Updates @a pCombo tool-tips. */
    void updateComboToolTip(QIComboBox *pCombo);

    /** Returns current user data for @a pList specified. */
    QString currentListWidgetData(QListWidget *pList);
}

/** UINativeWizardPage extension for 1st page of the New Cloud VM wizard,
  * based on UIWizardNewCloudVMPage1 namespace functions. */
class UIWizardNewCloudVMPageBasic1 : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs 1st basic page. */
    UIWizardNewCloudVMPageBasic1();

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

    /** Handles change in source tab-bar. */
    void sltHandleSourceChange();

    /** Handles change in image list. */
    void sltHandleSourceImageChange();

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

    /** Defines Virtual System @a comDescription object. */
    void setVSD(const CVirtualSystemDescription &comDescription);
    /** Returns Virtual System Description object. */
    CVirtualSystemDescription vsd() const;

    /** Returns Virtual System Description Form object. */
    CVirtualSystemDescriptionForm vsdForm() const;

    /** Updates provider. */
    void updateProvider();
    /** Updates profile. */
    void updateProfile();
    /** Updates source. */
    void updateSource();
    /** Updates source image. */
    void updateSourceImage();

    /** Holds the image ID. */
    QString  m_strSourceImageId;

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
    /** Holds the source image label instance. */
    QLabel       *m_pSourceImageLabel;
    /** Holds the source tab-bar instance. */
    QTabBar      *m_pSourceTabBar;
    /** Holds the source image list instance. */
    QListWidget  *m_pSourceImageList;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageBasic1_h */
