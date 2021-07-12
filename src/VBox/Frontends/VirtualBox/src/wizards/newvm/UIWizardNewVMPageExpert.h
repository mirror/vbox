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

/* Qt includes: */
#include <QSet>

#include "UINativeWizardPage.h"

#include "COMEnums.h"
#include "CMediumFormat.h"

/* Forward declarations: */
class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QVBoxLayout;
class UIFilePathSelector;
class UIHostnameDomainNameEditor;
class UIMediumSizeEditor;
class UINameAndSystemEditor;
class UIToolBox;
class UIUserNamePasswordEditor;
class QIToolButton;

/** Expert page of the New Virtual Machine wizard. */
class UIWizardNewVMPageExpert : public UINativeWizardPage
{

    Q_OBJECT;

public:

    UIWizardNewVMPageExpert();

protected:


private slots:

    void sltNameChanged(const QString &strNewText);
    void sltPathChanged(const QString &strNewPath);
    void sltOsTypeChanged();
    void sltMediaComboBoxIndexChanged();
    void sltGetWithFileOpenDialog();
    void sltISOPathChanged(const QString &strPath);
    void sltGAISOPathChanged(const QString &strPath);
    void sltOSFamilyTypeChanged();
    void sltInstallGACheckBoxToggle(bool fEnabled);
    void sltValueModified();
    void sltSkipUnattendedCheckBoxChecked();
    void sltMediumFormatChanged();
    void sltMediumSizeChanged();
    void sltSelectedDiskSourceChanged();
    void sltSelectLocationButtonClicked();

private:

    enum ExpertToolboxItems
    {
        ExpertToolboxItems_NameAndOSType,
        ExpertToolboxItems_Unattended,
        ExpertToolboxItems_Hardware,
        ExpertToolboxItems_Disk
    };

    /** Translation stuff. */
    void retranslateUi();

    /** Prepare stuff. */
    void createConnections();
    void initializePage();
    void initializeWidgets();
    /** Set the values of the widget if they depend on OS
      * type like recommended RAM size. The widgets whose values are
      * are explicitly modified are exempt from this. */
    void setOSTypeDependedValues();
    void cleanupPage();

    /** Validation stuff. */
    bool isComplete() const;
    bool validatePage();

    bool isProductKeyWidgetEnabled() const;
    void disableEnableUnattendedRelatedWidgets(bool fEnabled);
    void markWidgets() const;
    QWidget *createUnattendedWidgets();
    virtual QWidget *createNewDiskWidgets() /* override */;
    QWidget *createFormatButtonGroup(bool fExpertMode);
    QWidget *createMediumVariantWidgets();
    void addFormatButton(QWidget *pParent, QVBoxLayout *pFormatLayout,
                                                  CMediumFormat medFormat, bool fPreferred /* = false */);
    QWidget *createNameOSTypeWidgets();
    QWidget *createUserNameWidgets();
    QWidget *createAdditionalOptionsWidgets();
    QWidget *createGAInstallWidgets();

    void updateVirtualDiskPathFromMachinePathName();
    void updateWidgetAterMediumFormatChange();
    void setEnableNewDiskWidgets(bool fEnable);
    void setVirtualDiskFromDiskCombo();



    /** @name Variables
     * @{ */
       UIToolBox  *m_pToolBox;
       QGroupBox *m_pDiskFormatGroupBox;
       QGroupBox *m_pDiskVariantGroupBox;
       QLabel *m_pLocationLabel;
       QLineEdit *m_pLocationEditor;
       QIToolButton *m_pLocationOpenButton;
       QLabel *m_pMediumSizeEditorLabel;
       UIMediumSizeEditor *m_pMediumSizeEditor;
       QButtonGroup *m_pFormatButtonGroup;
       QCheckBox *m_pFixedCheckBox;
       QCheckBox *m_pSplitBox;
       UINameAndSystemEditor *m_pNameAndSystemEditor;
       QCheckBox *m_pSkipUnattendedCheckBox;
       QGridLayout *m_pNameAndSystemLayout;
       QGroupBox *m_pAdditionalOptionsContainer;
       QLabel *m_pProductKeyLabel;
       QLineEdit *m_pProductKeyLineEdit;
       UIHostnameDomainNameEditor *m_pHostnameDomainNameEditor;


    QCheckBox *m_pStartHeadlessCheckBox;
    QGroupBox *m_pGAInstallationISOContainer;
    QLabel *m_pGAISOPathLabel;
    UIFilePathSelector *m_pGAISOFilePathSelector;
    QGroupBox *m_pUserNameContainer;
    UIUserNamePasswordEditor *m_pUserNamePasswordEditor;

    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageExpert_h */
