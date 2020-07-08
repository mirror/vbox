/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageExpert class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageExpert_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageExpert_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVMPageBasic3.h"

/* Forward declarations: */
class QToolBox;

/** Expert page of the New Virtual Machine wizard. */
class UIWizardNewVMPageExpert : public UIWizardPage,
                                public UIWizardNewVMPage1,
                                public UIWizardNewVMPage2,
                                public UIWizardNewVMPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString machineFolder READ machineFolder WRITE setMachineFolder);
    Q_PROPERTY(QString machineBaseName READ machineBaseName WRITE setMachineBaseName);
    Q_PROPERTY(QString machineFilePath READ machineFilePath WRITE setMachineFilePath);
    Q_PROPERTY(CMedium virtualDisk READ virtualDisk WRITE setVirtualDisk);
    Q_PROPERTY(QUuid virtualDiskId READ virtualDiskId WRITE setVirtualDiskId);
    Q_PROPERTY(QString virtualDiskLocation READ virtualDiskLocation WRITE setVirtualDiskLocation);
    Q_PROPERTY(int baseMemory READ baseMemory);
    Q_PROPERTY(QString guestOSFamiyId READ guestOSFamiyId);
    Q_PROPERTY(QString ISOFilePath READ ISOFilePath);
    Q_PROPERTY(bool isUnattendedEnabled READ isUnattendedEnabled);
    Q_PROPERTY(bool startHeadless READ startHeadless);
    Q_PROPERTY(QString detectedOSTypeId READ detectedOSTypeId);
    Q_PROPERTY(QString userName READ userName WRITE setUserName);
    Q_PROPERTY(QString password READ password WRITE setPassword);
    Q_PROPERTY(QString hostname READ hostname WRITE setHostname);
    Q_PROPERTY(bool installGuestAdditions READ installGuestAdditions WRITE setInstallGuestAdditions);
    Q_PROPERTY(QString guestAdditionsISOPath READ guestAdditionsISOPath WRITE setGuestAdditionsISOPath);
    Q_PROPERTY(QString productKey READ productKey);
    Q_PROPERTY(int VCPUCount READ VCPUCount);

public:

    /** Constructor. */
    UIWizardNewVMPageExpert(const QString &strGroup);

protected:

    /** Wrapper to access 'wizard' from base part. */
    UIWizard *wizardImp() const { return wizard(); }
    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /** Handlers. */
    void sltNameChanged(const QString &strNewText);
    void sltPathChanged(const QString &strNewPath);
    void sltOsTypeChanged();
    void sltVirtualDiskSourceChanged();
    void sltGetWithFileOpenDialog();
    void sltUnattendedCheckBoxToggle();
    void sltISOPathChanged(const QString &strPath);
    void sltInstallGACheckBoxToggle(bool fChecked);
    void sltGAISOPathChanged(const QString &strPath);
    void sltOSFamilyTypeChanged();

private:
    enum ExpertToolboxItems
    {
        ExpertToolboxItems_NameAndOSType,
        ExpertToolboxItems_UsernameHostname,
        ExpertToolboxItems_GAInstall,
        ExpertToolboxItems_ProductKey,
        ExpertToolboxItems_Disk,
        ExpertToolboxItems_Hardware

    };
    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void createConnections();
    void initializePage();
    void cleanupPage();

    /** Validation stuff. */
    bool isComplete() const;
    bool validatePage();

    bool isProductKeyWidgetEnabled() const;
    void disableEnableUnattendedRelatedWidgets(bool fEnabled);

    /** Widgets. */
    QWidget *m_pNameAndSystemContainer;
    QWidget *m_pGAInstallContainer;
    QWidget *m_pUsernameHostnameContainer;
    QToolBox  *m_pToolBox;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageExpert_h */
