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
#include <QSet>

/* GUI includes: */
#include "QIFileDialog.h"
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

enum SelectedDiskSource
{
    SelectedDiskSource_Empty = 0,
    SelectedDiskSource_New,
    SelectedDiskSource_Existing,
    SelectedDiskSource_Max
};

namespace UIWizardNewVMDiskPage
{
    QUuid getWithFileOpenDialog(const QString &strOSTypeID,
                                const QString &strMachineFolder,
                                const QString &strMachineBaseName,
                                QWidget *pCaller);
    bool checkFATSizeLimitation(const qulonglong uVariant, const QString &strMediumPath, const qulonglong uSize);
    QString selectNewMediumLocation(UIWizardNewVM *pWizard);
}


class UIWizardNewVMDiskPageBasic : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMDiskPageBasic();

protected:


private slots:

    void sltSelectedDiskSourceChanged();
    void sltMediaComboBoxIndexChanged();
    void sltGetWithFileOpenDialog();
    void sltHandleSizeEditorChange(qulonglong uSize);
    void sltFixedCheckBoxToggled(bool fChecked);

private:

    void prepare();
    void createConnections();
    QWidget *createNewDiskWidgets();
    void cleanupPage();
    void setEnableNewDiskWidgets(bool fEnable);
    QWidget *createDiskWidgets();
    QWidget *createMediumVariantWidgets(bool fWithLabels);

    virtual void retranslateUi() /* override final */;
    virtual void initializePage() /* override final */;
    virtual bool isComplete() const /* override final */;
    virtual bool validatePage() /* override final */;

    void setEnableDiskSelectionWidgets(bool fEnabled);
    void setWidgetVisibility(const CMediumFormat &mediumFormat);

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
       QCheckBox *m_pFixedCheckBox;
    /** @} */

    SelectedDiskSource m_enmSelectedDiskSource;
    bool m_fRecommendedNoDisk;

    QSet<QString> m_userModifiedParameters;
    bool m_fVDIFormatFound;
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;

};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPageBasic_h */
