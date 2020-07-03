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
#include "UIWizard.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/** Container for unattended install related data. */
struct UIUnattendedInstallData
{
    UIUnattendedInstallData();
    bool m_fUnattendedEnabled;
    QUuid m_uMachineUid;
    QString m_strISOPath;
    bool m_fStartHeadless;
    QString m_strUserName;
    QString m_strPassword;
    QString m_strHostname;
    QString m_strDetectedOSTypeId;
    QString m_strDetectedOSVersion;
    QString m_strDetectedOSFlavor;
    QString m_strDetectedOSLanguages;
    QString m_strDetectedOSHints;
    QString m_strProductKey;
    bool m_fInstallGuestAdditions;
    QString m_strGuestAdditionsISOPath;
};

/** New Virtual Machine wizard: */
class UIWizardNewVM : public UIWizard
{
    Q_OBJECT;

public:

    /* Page IDs: */
    enum
    {
        PageNameType,
        PageUserNameHostname,
        PageGAInstall,
        PageProductKey,
        PageHardware,
        PageDisk,
        PageMax
    };

    /* Page IDs: */
    enum
    {
        PageExpert
    };

    /* Constructor: */
    UIWizardNewVM(QWidget *pParent, const QString &strGroup = QString());

    /** Prepare routine. */
    void prepare();

    /** Returns the Id of newly created VM. */
    QUuid createdMachineId() const;
    void setDefaultUnattendedInstallData(const UIUnattendedInstallData &unattendedInstallData);
    const UIUnattendedInstallData &unattendedInstallData() const;
    bool isUnattendedInstallEnabled() const;
    bool isGuestOSTypeWindows() const;

protected:

    /* Creates a new VM: */
    bool createVM();
    /* Configures the newly created VM: */
    void configureVM(const QString &strGuestTypeId, const CGuestOSType &comGuestType);
    /* Attaches default devices: */
    bool attachDefaultDevices(const CGuestOSType &comGuestType);

    QString getStringFieldValue(const QString &strFieldName) const;
    bool getBoolFieldValue(const QString &strFieldName) const;

    /* Who will be able to create virtual-machine: */
    friend class UIWizardNewVMPageBasicDisk;
    friend class UIWizardNewVMPageExpert;

private slots:

    void sltHandleWizardCancel();
    void sltHandleDetectedOSTypeChange();

private:

    /* Translation stuff: */
    void retranslateUi();


    /* Helping stuff: */
    QString getNextControllerName(KStorageBus type);

    /* Variables: */
    CMachine m_machine;
    QString m_strGroup;
    int m_iIDECount;
    int m_iSATACount;
    int m_iSCSICount;
    int m_iFloppyCount;
    int m_iSASCount;
    int m_iUSBCount;

    mutable UIUnattendedInstallData m_unattendedInstallData;
};

typedef QPointer<UIWizardNewVM> UISafePointerWizardNewVM;

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVM_h */
