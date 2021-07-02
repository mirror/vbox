/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMNameOSTypePageBasic class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePageBasic_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePageBasic_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UINativeWizardPage.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QBoxLayout;
class QCheckBox;
class QFrame;
class QLabel;
class QRadioButton;
class QGridLayout;
class QIRichTextLabel;
class UIFilePathSelector;
class UINameAndSystemEditor;

// /** 1st page of the New Virtual Machine wizard (base part): */
// class UIWizardNewVMNameOSTypePage  : public UIWizardPageBase
// {
// protected:

//     /** Constructor. */
//     UIWizardNewVMNameOSTypePage(const QString &strGroup);

//     /** Handlers. */
//     void onNameChanged(QString strNewName);
//     void onOsTypeChanged();

//     bool createMachineFolder();
//     /** Removes a previously created folder (if exists) before creating a new one.
//      *  used during page cleanup and new folder creation. Called upon page Next/Back and
//      *  wizard cancel */
//     bool cleanupMachineFolder(bool fWizardCancel = false);

//     /** @name Property getters/setters
//       * @{ */
//         QString machineFilePath() const;
//         void setMachineFilePath(const QString &strMachineFilePath);

//         QString machineFolder() const;
//         void setMachineFolder(const QString &strMachineFolder);

//         QString machineBaseName() const;
//         void setMachineBaseName(const QString &strMachineBaseName);

//         QString guestOSFamiyId() const;

//         bool isUnattendedEnabled() const;
//         const QString &detectedOSTypeId() const;

//         bool skipUnattendedInstall() const;
//     /** @} */

//     /** calls CVirtualBox::ComposeMachineFilename(...) and sets related member variables */
//     void composeMachineFilePath();

//     /** Colors the widgets red if they cause isComplete to fail. */
//     void markWidgets() const;
//     void retranslateWidgets();
//     QString ISOFilePath() const;
//     bool determineOSType(const QString &strISOPath);
//     void setTypeByISODetectedOSType(const QString &strDetectedOSType);
//     /** Return false if ISO path is not empty but points to an missing or unreadable file. */
//     bool checkISOFile() const;
//     void setSkipCheckBoxEnable();

//     QString m_strDetectedOSTypeId;

// private:

//     /** Full path (including the file name) of the machine's configuration file. */
//     QString m_strMachineFilePath;
//     /** Path of the folder hosting the machine's configuration file. Generated from m_strMachineFilePath. */
//     QString m_strMachineFolder;
//     /** Path of the folder created by this wizard page. Used to remove previously created
//      *  folder. see cleanupMachineFolder();*/
//     QString m_strCreatedFolder;
//     /** Base name of the machine is generated from the m_strMachineFilePath. */
//     QString m_strMachineBaseName;

//     QString m_strGroup;
//     bool m_fSupportsHWVirtEx;
//     bool m_fSupportsLongMode;

//     friend class UIWizardNewVM;
// };

/** 1st page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMNameOSTypePageBasic : public UINativeWizardPage
{

    Q_OBJECT;

public:

    /** Constructor. */
    UIWizardNewVMNameOSTypePageBasic(const QString &strGroup);
    virtual bool isComplete() const; /* override */
    virtual int nextId() const /* override */;

protected:

private slots:

    /** Handlers. */
    void sltNameChanged(const QString &strNewText);
    void sltPathChanged(const QString &strNewPath);
    void sltOsTypeChanged();
    void sltISOPathChanged(const QString &strPath);

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void initializePage();
    void cleanupPage();

    QWidget *createNameOSTypeWidgets();

    /** Validation stuff. */
    virtual bool validatePage() /* override */;


    /** @name Widgets
     * @{ */
        QGridLayout           *m_pNameAndSystemLayout;
        UINameAndSystemEditor *m_pNameAndSystemEditor;
        QCheckBox             *m_pSkipUnattendedCheckBox;
        QIRichTextLabel       *m_pNameOSTypeLabel;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePageBasic_h */
