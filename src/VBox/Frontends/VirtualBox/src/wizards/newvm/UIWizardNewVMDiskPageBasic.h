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
    QString defaultExtension(const CMediumFormat &mediumFormatRef);
    QString toFileName(const QString &strName, const QString &strExtension);
    QUuid getWithFileOpenDialog(const QString &strOSTypeID,
                                const QString &strMachineFolder,
                                const QString &strMachineBaseName,
                                QWidget *pCaller);
    QString absoluteFilePath(const QString &strFileName, const QString &strPath);
}

//     SelectedDiskSource selectedDiskSource() const;
//     void setSelectedDiskSource(SelectedDiskSource enmSelectedDiskSource);
//     bool getWithNewVirtualDiskWizard();
//     virtual QWidget *createDiskWidgets();
//     virtual QWidget *createNewDiskWidgets();
//     void getWithFileOpenDialog();
//     void retranslateWidgets();
//     void setEnableDiskSelectionWidgets(bool fEnable);

class UIWizardNewVMDiskPageBasic : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVMDiskPageBasic();
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
    void cleanupPage();
    void setEnableNewDiskWidgets(bool fEnable);
    void setVirtualDiskFromDiskCombo();
    QWidget *createDiskWidgets();
    QWidget *createMediumVariantWidgets(bool fWithLabels);

    virtual void retranslateUi() /* override final */;
    virtual void initializePage() /* override final */;
    virtual bool isComplete() const /* override final */;
    virtual bool validatePage() /* override final */;

    void setEnableDiskSelectionWidgets(bool fEnabled);
    void setWidgetVisibility(CMediumFormat &mediumFormat);

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

    /** For guided new vm wizard VDI is the only format. Thus we have no UI item for it. */
    CMediumFormat m_mediumFormat;
    SelectedDiskSource m_enmSelectedDiskSource;
    bool m_fRecommendedNoDisk;

    QString m_strDefaultExtension;
    QSet<QString> m_userModifiedParameters;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMDiskPageBasic_h */
