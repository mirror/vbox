/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic1 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Local includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;
class QRadioButton;
class QIRichTextLabel;
class UIFilePathSelector;
class UINameAndSystemEditor;
class QButtonGroup;

/** 1st page of the New Virtual Machine wizard (base part): */
class UIWizardNewVMPage1 : public UIWizardPageBase
{
protected:

    /** Constructor. */
    UIWizardNewVMPage1(const QString &strGroup);

    /** Handlers. */
    void onNameChanged(QString strNewName);
    void onOsTypeChanged();

    bool createMachineFolder();
    /** Removes a previously created folder (if exists) before creating a new one.
     *  used during page cleanup and new folder creation. Called upon page Next/Back and
     *  wizard cancel */
    bool cleanupMachineFolder(bool fWizardCancel = false);

    /** @name Property getters/setters
      * @{ */
        QString machineFilePath() const;
        void setMachineFilePath(const QString &strMachineFilePath);

        QString machineFolder() const;
        void setMachineFolder(const QString &strMachineFolder);

        QString machineBaseName() const;
        void setMachineBaseName(const QString &strMachineBaseName);

        QString guestOSFamiyId() const;
        QString ISOFilePath() const;
        bool isUnattendedEnabled() const;
        bool startHeadless() const;
        const QString &detectedOSTypeId() const;
    /** @} */


    bool determineOSType(const QString &strISOPath);
    /** calls CVirtualBox::ComposeMachineFilename(...) and sets related member variables */
    void composeMachineFilePath();
    bool checkISOFile() const;
    /** Creates the page widgets and adds them into the @p pGridLayout. */
    void createWidgets(QGridLayout *pGridLayout);

    /** @name Widgets
     * @{ */
       /** Holds the simple variant button instance. */
       QRadioButton *m_pButtonSimple;
       /** Holds the unattended variant button instance. */
       QRadioButton *m_pButtonUnattended;
       /** Holds the ISO selector label instance. */
       QLabel *m_pISOSelectorLabel;
       /** Holds the ISO selector editor instance. */
       UIFilePathSelector *m_pISOFilePathSelector;
       /** Holds the headless start label instance. */
       QLabel *m_pStartHeadlessLabel;
       /** Holds the headless start checkbox instance. */
       QCheckBox *m_pStartHeadlessCheckBox;
       /** Provides a path selector and a line edit field for path and name entry. */
       UINameAndSystemEditor *m_pNameAndSystemEditor;
       QIRichTextLabel *m_pLabel1;
       QIRichTextLabel *m_pLabel2;
       QButtonGroup *m_pButtonGroup;
    /** @} */

    QString m_strDetectedOSTypeId;

private:


    /** Full path (including the file name) of the machine's configuration file. */
    QString m_strMachineFilePath;
    /** Path of the folder hosting the machine's configuration file. Generated from m_strMachineFilePath. */
    QString m_strMachineFolder;
    /** Path of the folder created by this wizard page. Used to remove previously created
     *  folder. see cleanupMachineFolder();*/
    QString m_strCreatedFolder;
    /** Base name of the machine is generated from the m_strMachineFilePath. */
    QString m_strMachineBaseName;


    QString m_strGroup;
    bool m_fSupportsHWVirtEx;
    bool m_fSupportsLongMode;


    friend class UIWizardNewVM;
};

/** 1st page of the New Virtual Machine wizard (basic extension). */
class UIWizardNewVMPageBasic1 : public UIWizardPage, public UIWizardNewVMPage1
{
    Q_OBJECT;
    Q_PROPERTY(QString machineFilePath READ machineFilePath WRITE setMachineFilePath);
    Q_PROPERTY(QString machineFolder READ machineFolder WRITE setMachineFolder);
    Q_PROPERTY(QString machineBaseName READ machineBaseName WRITE setMachineBaseName);
    Q_PROPERTY(QString guestOSFamiyId READ guestOSFamiyId);
    Q_PROPERTY(QString ISOFilePath READ ISOFilePath);
    Q_PROPERTY(bool isUnattendedEnabled READ isUnattendedEnabled);
    Q_PROPERTY(bool startHeadless READ startHeadless);
    Q_PROPERTY(QString detectedOSTypeId READ detectedOSTypeId);


public:

    /** Constructor. */
    UIWizardNewVMPageBasic1(const QString &strGroup);
    virtual int nextId() const /* override */;
    void setTypeByISODetectedOSType(const QString &strDetectedOSType);
    virtual bool isComplete() const; /* override */

protected:

    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }

private slots:

    /** Handlers. */
    void sltNameChanged(const QString &strNewText);
    void sltPathChanged(const QString &strNewPath);
    void sltOsTypeChanged();
    void sltISOPathChanged(const QString &strPath);
    void sltUnattendedCheckBoxToggle();

private:

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void prepare();
    void createConnections();
    void initializePage();
    void cleanupPage();

    /** Validation stuff. */
    virtual bool validatePage() /* override */;

};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic1_h */
