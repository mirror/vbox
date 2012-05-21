/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic1 class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardNewVMPageBasic1_h__
#define __UIWizardNewVMPageBasic1_h__

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QGroupBox;
class QLineEdit;
class VBoxOSTypeSelectorWidget;
class QIRichTextLabel;

/* 1st page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPage1 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVMPage1();

    /* Handlers: */
    void onNameChanged(const QString &strNewName);
    void onOsTypeChanged();

    /* Helping stuff: */
    bool machineFolderCreated();
    bool createMachineFolder();
    bool cleanupMachineFolder();

    /* Stuff for 'machineFolder' field: */
    QString machineFolder() const { return m_strMachineFolder; }
    void setMachineFolder(const QString &strMachineFolder) { m_strMachineFolder = strMachineFolder; }

    /* Stuff for 'machineBaseName' field: */
    QString machineBaseName() const { return m_strMachineBaseName; }
    void setMachineBaseName(const QString &strMachineBaseName) { m_strMachineBaseName = strMachineBaseName; }

    /* Variables: */
    QString m_strMachineFolder;
    QString m_strMachineBaseName;

    /* Widgets: */
    QGroupBox *m_pNameCnt;
    QLineEdit *m_pNameEditor;
    QGroupBox *m_pTypeCnt;
    VBoxOSTypeSelectorWidget *m_pTypeSelector;
};

/* 1st page of the New Virtual Machine wizard (basic extension): */
class UIWizardNewVMPageBasic1 : public UIWizardPage, public UIWizardNewVMPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString machineFolder READ machineFolder WRITE setMachineFolder);
    Q_PROPERTY(QString machineBaseName READ machineBaseName WRITE setMachineBaseName);

public:

    /* Constructor: */
    UIWizardNewVMPageBasic1();

protected:

    /* Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }

private slots:

    /* Handlers: */
    void sltNameChanged(const QString &strNewText);
    void sltOsTypeChanged();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();
    void cleanupPage();

    /* Validation stuff: */
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
};

#endif // __UIWizardNewVMPageBasic1_h__

