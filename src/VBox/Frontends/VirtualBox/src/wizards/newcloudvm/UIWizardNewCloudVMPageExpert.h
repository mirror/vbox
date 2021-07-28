/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVMPageExpert class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardNewCloudVMPageBasic1.h"
#include "UIWizardNewCloudVMPageBasic2.h"

/* Forward declarations: */
class QGroupBox;

/** UINativeWizardPage extension for Expert page of the New Cloud VM wizard,
  * based on UIWizardNewCloudVMPage1 & UIWizardNewCloudVMPage2 namespace functions. */
class UIWizardNewCloudVMPageExpert : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructs expert page. */
    UIWizardNewCloudVMPageExpert(bool fFullWizard);

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

    /** Handles change in instance list. */
    void sltHandleSourceImageChange();

    /** Initializes short wizard form. */
    void sltInitShortWizardForm();

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

    /** Defines Virtual System Description @a comForm object. */
    void setVSDForm(const CVirtualSystemDescriptionForm &comForm);
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
    /** Updates VSD form. */
    void updateVSDForm();
    /** Updates properties table. */
    void updatePropertiesTable();

    /** Holds whether we want full wizard form or short one. */
    bool     m_fFullWizard;
    /** Holds the image ID. */
    QString  m_strSourceImageId;

    /** Holds the location container instance. */
    QGroupBox    *m_pCntLocation;
    /** Holds the location type combo-box instance. */
    QIComboBox   *m_pProviderComboBox;
    /** Holds the profile combo-box instance. */
    QIComboBox   *m_pProfileComboBox;
    /** Holds the profile management tool-button instance. */
    QIToolButton *m_pProfileToolButton;

    /** Holds the source container instance. */
    QGroupBox    *m_pCntSource;
    /** Holds the source tab-bar instance. */
    QTabBar      *m_pSourceTabBar;
    /** Holds the source image list instance. */
    QListWidget  *m_pSourceImageList;

    /** Holds the settings container instance. */
    QGroupBox                 *m_pSettingsCnt;
    /** Holds the Form Editor widget instance. */
    UIFormEditorWidgetPointer  m_pFormEditor;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVMPageExpert_h */
