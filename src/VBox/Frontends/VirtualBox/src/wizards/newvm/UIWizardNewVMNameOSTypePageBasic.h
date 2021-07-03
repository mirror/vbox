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
class UINativeWizard;

namespace UIWizardNewVMNameOSTypePage
{
    void onNameChanged(UINameAndSystemEditor *pNameAndSystemEditor, QString strNewName);
    bool createMachineFolder(UINameAndSystemEditor *pNameAndSystemEditor,
                             UINativeWizardPage *pCaller,
                             const QString &strMachineFolder,
                             QString &strCreatedFolder);

    /** Removes a previously created folder (if exists) before creating a new one.
     *  used during page cleanup and new folder creation. Called upon page Next/Back and
     *  wizard cancel */
    bool cleanupMachineFolder(const QString &strMachineFolder,
                              QString &strCreatedFolder,bool fWizardCancel = false);
    void composeMachineFilePath(UINameAndSystemEditor *pNameAndSystemEditor, UINativeWizard *pWizard);
    void determineOSType(const QString &strISOPath, UINativeWizard *pWizard);

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


//     /** Colors the widgets red if they cause isComplete to fail. */
//     void markWidgets() const;
//     void retranslateWidgets();
//     QString ISOFilePath() const;

//     void setTypeByISODetectedOSType(const QString &strDetectedOSType);
//     /** Return false if ISO path is not empty but points to an missing or unreadable file. */
//     bool checkISOFile() const;
//     void setSkipCheckBoxEnable();

//

// private:


// };
}

/** 1st page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMNameOSTypePageBasic : public UINativeWizardPage
{

    Q_OBJECT;

public:

    /** Constructor. */
    UIWizardNewVMNameOSTypePageBasic();


protected:

    virtual bool isComplete() const; /* override final */
    /** Validation stuff. */
    virtual bool validatePage() /* override */;

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


    /** @name Widgets
     * @{ */
        QGridLayout           *m_pNameAndSystemLayout;
        UINameAndSystemEditor *m_pNameAndSystemEditor;
        QCheckBox             *m_pSkipUnattendedCheckBox;
        QIRichTextLabel       *m_pNameOSTypeLabel;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePageBasic_h */
