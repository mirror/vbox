/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageDisk class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageDisk_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageDisk_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"
#include "UIWizardNewVDPageFileType.h"
#include "UIWizardNewVDPageBasic2.h"
#include "UIWizardNewVDPageBasic3.h"
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


class UIWizardNewVMPageDiskBase : public UIWizardPageBase
{

public:


protected:

    UIWizardNewVMPageDiskBase();

    SelectedDiskSource selectedDiskSource() const;
    void setSelectedDiskSource(SelectedDiskSource enmSelectedDiskSource);
    bool getWithNewVirtualDiskWizard();

    virtual QWidget *createDiskWidgets();
    virtual QWidget *createNewDiskWidgets();
    void getWithFileOpenDialog();
    void retranslateWidgets();

    void setEnableDiskSelectionWidgets(bool fEnable);
    bool m_fRecommendedNoDisk;

    /** @name Widgets
     * @{ */
       QButtonGroup *m_pDiskSourceButtonGroup;
       QRadioButton *m_pDiskEmpty;
       QRadioButton *m_pDiskNew;
       QRadioButton *m_pDiskExisting;
       UIMediaComboBox *m_pDiskSelector;
       QIToolButton *m_pDiskSelectionButton;
    /** @} */
    SelectedDiskSource m_enmSelectedDiskSource;
};

class UIWizardNewVMPageDisk : public UIWizardPage,
                                public UIWizardNewVMPageDiskBase,
                                public UIWizardNewVDPageBaseFileType,
                                public UIWizardNewVDPage2,
                                public UIWizardNewVDPage3
{
    Q_OBJECT;
    Q_PROPERTY(SelectedDiskSource selectedDiskSource READ selectedDiskSource WRITE setSelectedDiskSource);
    Q_PROPERTY(CMediumFormat mediumFormat READ mediumFormat);
    Q_PROPERTY(qulonglong mediumVariant READ mediumVariant WRITE setMediumVariant);
    Q_PROPERTY(QString mediumPath READ mediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    UIWizardNewVMPageDisk();
    /** For the guide wizard mode medium path, name and extention is static and we have
      * no UI element for this. thus override. */
    virtual QString mediumPath() const /*override */;
    CMediumFormat mediumFormat() const;

protected:

    /** Wrapper to access 'wizard' from base part. */
    UIWizardNewVM *wizardImp() const { return qobject_cast<UIWizardNewVM*>(wizard()); }
    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

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

    bool isComplete() const;
    virtual bool validatePage() /* override */;

    QIRichTextLabel *m_pLabel;

    /** This is set to true when user manually set the size. */
    bool m_fUserSetSize;
    /** For guided new vm wizard VDI is the only format. Thus we have no UI item for it. */
    CMediumFormat m_mediumFormat;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageDisk_h */
