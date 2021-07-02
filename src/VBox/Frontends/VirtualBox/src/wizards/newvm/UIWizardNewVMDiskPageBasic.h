/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMDiskPageBasic class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPageBasic_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPageBasic_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UINativeWizardPage.h"
#include "UIWizardNewVDPageFileType.h"
#include "UIWizardNewVDPageVariant.h"
#include "UIWizardNewVDPageSizeLocation.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QButtonGroup;
class QRadioButton;
class QIRichTextLabel;
class QIToolButton;
class UIMediaComboBox;


// class UIWizardNewVMDiskPage : public UIWizardPageBase
// {

// public:


// protected:

//     UIWizardNewVMDiskPage();

//     SelectedDiskSource selectedDiskSource() const;
//     void setSelectedDiskSource(SelectedDiskSource enmSelectedDiskSource);
//     bool getWithNewVirtualDiskWizard();

//     virtual QWidget *createDiskWidgets();
//     virtual QWidget *createNewDiskWidgets();
//     void getWithFileOpenDialog();
//     void retranslateWidgets();

//     void setEnableDiskSelectionWidgets(bool fEnable);
//     bool m_fRecommendedNoDisk;

//     SelectedDiskSource m_enmSelectedDiskSource;
// };

class UIWizardNewVMDiskPageBasic : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMDiskPageBasic();
    /** For the guide wizard mode medium path, name and extention is static and we have
      * no UI element for this. thus override. */
    virtual QString mediumPath() const /*override */;
    CMediumFormat mediumFormat() const;

protected:


private slots:

    void sltSelectedDiskSourceChanged();
    void sltMediaComboBoxIndexChanged();
    void sltGetWithFileOpenDialog();
    void sltHandleSizeEditorChange();

private:

    void prepare();
    void createConnections();
    QWidget *createNewDiskWidgets();
    void retranslateUi();
    void initializePage();
    void cleanupPage();
    void setEnableNewDiskWidgets(bool fEnable);
    void setVirtualDiskFromDiskCombo();
    QWidget *createDiskWidgets();
    QWidget *createMediumVariantWidgets(bool fWithLabels);
    bool isComplete() const;
    virtual bool validatePage() /* override */;

    /** @name Widgets
     * @{ */
       QButtonGroup *m_pDiskSourceButtonGroup;
       QRadioButton *m_pDiskEmpty;
       QRadioButton *m_pDiskNew;
       QRadioButton *m_pDiskExisting;
       UIMediaComboBox *m_pDiskSelector;
       QIToolButton *m_pDiskSelectionButton;
       QIRichTextLabel *m_pLabel;
       QLabel          *m_pMediumSizeEditorLabel;
       UIMediumSizeEditor *m_pMediumSizeEditor;
       QIRichTextLabel *m_pDescriptionLabel;
       QIRichTextLabel *m_pDynamicLabel;
       QIRichTextLabel *m_pFixedLabel;
       QIRichTextLabel *m_pSplitLabel;
       QCheckBox *m_pFixedCheckBox;
       QCheckBox *m_pSplitBox;
    /** @} */

    /** This is set to true when user manually set the size. */
    bool m_fUserSetSize;
    /** For guided new vm wizard VDI is the only format. Thus we have no UI item for it. */
    CMediumFormat m_mediumFormat;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPageBasic_h */
