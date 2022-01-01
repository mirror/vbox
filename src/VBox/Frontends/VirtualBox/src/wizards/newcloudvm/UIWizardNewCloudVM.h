/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVM class declaration.
 */

/*
 * Copyright (C) 2009-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h
#define FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CVirtualSystemDescription.h"
#include "CVirtualSystemDescriptionForm.h"

/** New Cloud VM wizard. */
class UIWizardNewCloudVM : public UINativeWizard
{
    Q_OBJECT;

public:

    /** Constructs New Cloud VM wizard passing @a pParent & @a enmMode to the base-class.
      * @param  strFullGroupName  Brings full group name (/provider/profile) to create VM in. */
    UIWizardNewCloudVM(QWidget *pParent, const QString &strFullGroupName);

    /** Returns provider short name. */
    QString providerShortName() const { return m_strProviderShortName; }
    /** Returns profile name. */
    QString profileName() const { return m_strProfileName; }
    /** Returns Cloud Client object. */
    CCloudClient client() const { return m_comClient; }
    /** Returns Virtual System Description object. */
    CVirtualSystemDescription vsd() const { return m_comVSD; }
    /** Returns Virtual System Description Form object. */
    CVirtualSystemDescriptionForm vsdForm() const { return m_comVSDForm; }

    /** Creates VSD Form. */
    void createVSDForm();

    /** Creates New Cloud VM. */
    bool createCloudVM();

public slots:

    /** Defines @a strProviderShortName. */
    void setProviderShortName(const QString &strProviderShortName) { m_strProviderShortName = strProviderShortName; }
    /** Defines @a strProfileName. */
    void setProfileName(const QString &strProfileName) { m_strProfileName = strProfileName; }
    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient) { m_comClient = comClient; }
    /** Defines Virtual System @a comVSD object. */
    void setVSD(const CVirtualSystemDescription &comVSD) { m_comVSD = comVSD; }
    /** Defines Virtual System Description @a comForm object. */
    void setVSDForm(const CVirtualSystemDescriptionForm &comForm) { m_comVSDForm = comForm; }

protected:

    /** Populates pages. */
    virtual void populatePages() /* override final */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override final */;

private:

    /** Holds the short provider name. */
    QString                        m_strProviderShortName;
    /** Holds the profile name. */
    QString                        m_strProfileName;
    /** Holds the Cloud Client object reference. */
    CCloudClient                   m_comClient;
    /** Holds the Virtual System Description object reference. */
    CVirtualSystemDescription      m_comVSD;
    /** Holds the Virtual System Description Form object reference. */
    CVirtualSystemDescriptionForm  m_comVSDForm;
};

/** Safe pointer to new cloud vm wizard. */
typedef QPointer<UIWizardNewCloudVM> UISafePointerWizardNewCloudVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h */
