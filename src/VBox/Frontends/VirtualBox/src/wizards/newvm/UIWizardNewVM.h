/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVM class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVM_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINativeWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CMedium.h"
#include "CMediumFormat.h"
#include "CGuestOSType.h"

#define newVMWizardPropertySet(functionName, value)                     \
    do                                                                  \
    {                                                                   \
        UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard()); \
        if (pWizard)                                                   \
            pWizard->set##functionName(value);                          \
    }                                                                   \
    while(0)

/** Container for unattended install related data. */
struct UIUnattendedInstallData
{
    UIUnattendedInstallData();
    QUuid m_uMachineUid;
    QString m_strISOPath;

    bool m_fUnattendedEnabled;
    bool m_fStartHeadless;
    QString m_strUserName;
    QString m_strPassword;
    QString m_strHostnameDomain;
    QString m_strProductKey;
    bool m_fInstallGuestAdditions;
    QString m_strGuestAdditionsISOPath;
#if 0
    QString m_strDetectedOSVersion;
    QString m_strDetectedOSFlavor;
    QString m_strDetectedOSLanguages;
    QString m_strDetectedOSHints;
#endif
};


/** New Virtual Machine wizard: */
class UIWizardNewVM : public UINativeWizard
{
    Q_OBJECT;

public:

    UIWizardNewVM(QWidget *pParent, const QString &strMachineGroup = QString(), WizardMode enmMode = WizardMode_Auto);
    bool isUnattendedEnabled() const;
    void setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData);
    const UIUnattendedInstallData &unattendedInstallData() const;
    bool isGuestOSTypeWindows() const;

    bool createVM();
    bool createVirtualDisk();
    void deleteVirtualDisk();


    CMedium &virtualDisk();
    void setVirtualDisk(const CMedium &medium);
    void setVirtualDisk(const QUuid &mediumId);

    const QString &machineGroup() const;

    const QString &machineFilePath() const;
    void setMachineFilePath(const QString &strMachineFilePath);

    const QString &machineFolder() const;
    void setMachineFolder(const QString &strMachineFolder);

    const QString &machineBaseName() const;
    void setMachineBaseName(const QString &strMachineBaseName);

    const QString &createdMachineFolder() const;
    void setCreatedMachineFolder(const QString &strCreatedMachineFolder);

    const QString &detectedOSTypeId() const;
    void setDetectedOSTypeId(const QString &strDetectedOSTypeId);

    const QString &guestOSFamilyId() const;
    void setGuestOSFamilyId(const QString &strGuestOSFamilyId);

    const CGuestOSType &guestOSType() const;
    void setGuestOSType(const CGuestOSType &guestOSType);

    bool installGuestAdditions() const;
    void setInstallGuestAdditions(bool fInstallGA);

    bool startHeadless() const;
    void setStartHeadless(bool fStartHeadless);

    bool skipUnattendedInstall() const;
    void setSkipUnattendedInstall(bool fSkipUnattendedInstall);

    bool EFIEnabled() const;
    void setEFIEnabled(bool fEnabled);

    const QString &ISOFilePath() const;
    void setISOFilePath(const QString &strISOFilePath);

    const QString &userName() const;
    void setUserName(const QString &strUserName);

    const QString &password() const;
    void setPassword(const QString &strPassword);

    const QString &guestAdditionsISOPath() const;
    void setGuestAdditionsISOPath(const QString &strGAISOPath);

    const QString &hostnameDomain() const;
    void setHostnameDomain(const QString &strHostname);

    const QString &productKey() const;
    void setProductKey(const QString &productKey);

    int CPUCount() const;
    void setCPUCount(int iCPUCount);

    int memorySize() const;
    void setMemorySize(int iMemory);

    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);

    const CMediumFormat &mediumFormat();
    void setMediumFormat(const CMediumFormat &mediumFormat);

    const QString &mediumPath() const;
    void setMediumPath(const QString &strMediumPath);

    qulonglong mediumSize() const;
    void setMediumSize(qulonglong mediumSize);

protected:

    /** Populates pages. */
    virtual void populatePages() /* final override */;
    void configureVM(const QString &strGuestTypeId, const CGuestOSType &comGuestType);
    bool attachDefaultDevices();

private slots:

    void sltHandleWizardCancel();

private:

    void retranslateUi();
    QString getNextControllerName(KStorageBus type);
    void setUnattendedPageVisible(bool fVisible);
    /** Returns the Id of newly created VM. */
    QUuid createdMachineId() const;

    /** @name Variables
     * @{ */
       CMedium m_virtualDisk;
       CMachine m_machine;
       QString m_strMachineGroup;
       int m_iIDECount;
       int m_iSATACount;
       int m_iSCSICount;
       int m_iFloppyCount;
       int m_iSASCount;
       int m_iUSBCount;
       mutable UIUnattendedInstallData m_unattendedInstallData;
    /** @} */

    /** Path of the ISO file attached to the new vm. Possibly used for the unattended install. */
    QString m_strISOFilePath;

    /** Path of the folder created by this wizard page. Used to remove previously created
     *  folder. see cleanupMachineFolder();*/
    QString m_strCreatedFolder;

    /** Full path (including the file name) of the machine's configuration file. */
    QString m_strMachineFilePath;
    /** Path of the folder hosting the machine's configuration file. Generated from m_strMachineFilePath. */
    QString m_strMachineFolder;
    /** Base name of the machine is generated from the m_strMachineFilePath. */
    QString m_strMachineBaseName;

    /** Type Id od the OS detected from the ISO file by IUnattended. */
    QString m_strDetectedOSTypeId;

    /** Holds the VM OS family ID. */
    QString  m_strGuestOSFamilyId;
    /** Holds the VM OS type. */
    CGuestOSType m_comGuestOSType;

    /** True if guest additions are to be installed during unattended install. */
    bool m_fInstallGuestAdditions;
    bool m_fSkipUnattendedInstall;
    bool m_fEFIEnabled;

    int m_iCPUCount;
    int m_iMemorySize;
    int m_iUnattendedInstallPageIndex;

    qulonglong m_uMediumVariant;
    CMediumFormat m_comMediumFormat;
    QString m_strMediumPath;
    qulonglong m_uMediumSize;
};

typedef QPointer<UIWizardNewVM> UISafePointerWizardNewVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVM_h */
