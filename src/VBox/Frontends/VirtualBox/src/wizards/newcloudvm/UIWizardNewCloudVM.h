/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVM class declaration.
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

    /** Constructs New Cloud VM wizard passing @a pParent to the base-class.
      * @param  strFullGroupName  Brings full group name (/provider/profile) to create VM in.
      * @param  comClient         Brings the Cloud Client object to work with.
      * @param  comVSD            Brings the Virtual System Description object to use. */
    UIWizardNewCloudVM(QWidget *pParent,
                       const QString &strFullGroupName = QString(),
                       const CCloudClient &comClient = CCloudClient(),
                       const CVirtualSystemDescription &comVSD = CVirtualSystemDescription(),
                       WizardMode enmMode = WizardMode_Auto);

    /** Sets whether the final step is @a fPrevented. */
    void setFinalStepPrevented(bool fPrevented) { m_fFinalStepPrevented = fPrevented; }

    /** Returns full group name. */
    QString fullGroupName() const { return m_strFullGroupName; }

    /** Defines @a strShortProviderName. */
    void setShortProviderName(const QString &strShortProviderName) { m_strShortProviderName = strShortProviderName; }
    /** Returns short provider name. */
    QString shortProviderName() const { return m_strShortProviderName; }

    /** Defines @a strProfileName. */
    void setProfileName(const QString &strProfileName) { m_strProfileName = strProfileName; }
    /** Returns profile name. */
    QString profileName() const { return m_strProfileName; }

    /** Defines Cloud @a comClient object. */
    void setClient(const CCloudClient &comClient) { m_comClient = comClient; }
    /** Returns Cloud Client object. */
    CCloudClient client() const { return m_comClient; }

    /** Defines Virtual System @a comVSD object. */
    void setVSD(const CVirtualSystemDescription &comVSD) { m_comVSD = comVSD; }
    /** Returns Virtual System Description object. */
    CVirtualSystemDescription vsd() const { return m_comVSD; }

    /** Defines Virtual System Description @a comForm object. */
    void setVSDForm(const CVirtualSystemDescriptionForm &comForm) { m_comVSDForm = comForm; }
    /** Returns Virtual System Description Form object. */
    CVirtualSystemDescriptionForm vsdForm() const { return m_comVSDForm; }

    /** Creates VSD Form. */
    bool createVSDForm();

    /** Creates New Cloud VM. */
    bool createCloudVM();

    /** Schedules Finish button trigger for
      * the next event-loop cicle. */
    void scheduleAutoFinish();

protected:

    /** Populates pages. */
    virtual void populatePages() /* final */;

    /** Handles translation event. */
    virtual void retranslateUi() /* final */;

private slots:

    /** Triggers Finish button. */
    void sltTriggerFinishButton();

private:

    /** Holds the full group name (/provider/profile) to add VM to. */
    QString                        m_strFullGroupName;
    /** Holds the short provider name. */
    QString                        m_strShortProviderName;
    /** Holds the profile name. */
    QString                        m_strProfileName;
    /** Holds the Cloud Client object reference. */
    CCloudClient                   m_comClient;
    /** Holds the Virtual System Description object reference. */
    CVirtualSystemDescription      m_comVSD;
    /** Holds the Virtual System Description Form object reference. */
    CVirtualSystemDescriptionForm  m_comVSDForm;

    /** Holds whether we want full wizard form or short one. */
    bool  m_fFullWizard;
    /** Holds whether the final step is prevented. */
    bool  m_fFinalStepPrevented;
};

/** Safe pointer to new cloud vm wizard. */
typedef QPointer<UIWizardNewCloudVM> UISafePointerWizardNewCloudVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_newcloudvm_UIWizardNewCloudVM_h */
