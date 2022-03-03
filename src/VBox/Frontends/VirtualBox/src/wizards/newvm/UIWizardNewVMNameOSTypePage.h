/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMNameOSTypePage class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePage_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QIRichTextLabel;
class UINameAndSystemEditor;
class UIWizardNewVM;

namespace UIWizardNewVMNameOSTypeCommon
{
    bool guessOSTypeFromName(UINameAndSystemEditor *pNameAndSystemEditor, QString strNewName);
    bool guessOSTypeDetectedOSTypeString(UINameAndSystemEditor *pNameAndSystemEditor, QString strDetectedOSType);
    bool createMachineFolder(UINameAndSystemEditor *pNameAndSystemEditor, UIWizardNewVM *pWizard);

    /** Removes a previously created folder (if exists) before creating a new one.
     *  used during page cleanup and new folder creation. Called upon page Next/Back and
     *  wizard cancel */
    bool cleanupMachineFolder(UIWizardNewVM *pWizard, bool fWizardCancel = false);
    void composeMachineFilePath(UINameAndSystemEditor *pNameAndSystemEditor, UIWizardNewVM *pWizard);
    /** Return false if ISO path is not empty but points to an missing or unreadable file. */
    bool checkISOFile(UINameAndSystemEditor *pNameAndSystemEditor);
}

/** 1st page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMNameOSTypePage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIWizardNewVMNameOSTypePage();


protected:

    virtual bool isComplete() const; /* override final */
    /** Validation stuff. */
    virtual bool validatePage() RT_OVERRIDE;

private slots:

    void sltNameChanged(const QString &strNewText);
    void sltPathChanged(const QString &strNewPath);
    void sltOsTypeChanged();
    void sltISOPathChanged(const QString &strPath);
    void sltGuestOSFamilyChanged(const QString &strGuestOSFamilyId);
    void sltSkipUnattendedInstallChanged(bool fSkip);
    void sltSelectedEditionChanged(ulong uEditionIndex);

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void initializePage();
    QWidget *createNameOSTypeWidgets();
    void markWidgets() const;
    void setSkipCheckBoxEnable();
    bool isUnattendedEnabled() const;
    bool isUnattendedInstallSupported() const;
    void setEditionSelectorEnabled();

    /** @name Widgets
     * @{ */
        QGridLayout           *m_pNameAndSystemLayout;
        UINameAndSystemEditor *m_pNameAndSystemEditor;
        QCheckBox             *m_pSkipUnattendedCheckBox;
        QIRichTextLabel       *m_pNameOSTypeLabel;
    /** @} */
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMNameOSTypePage_h */
