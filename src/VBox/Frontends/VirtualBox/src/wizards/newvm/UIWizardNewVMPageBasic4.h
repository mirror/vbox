/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic4 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#define FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"
#include "UIWizardNewVDPageBasic1.h"
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


class UIWizardNewVMPage4 : public UIWizardPageBase
{

public:

    const CMedium &virtualDisk() const { return m_virtualDisk; }
    void setVirtualDisk(const CMedium &virtualDisk) { m_virtualDisk = virtualDisk; }

protected:

    UIWizardNewVMPage4();

    SelectedDiskSource selectedDiskSource() const;
    void setSelectedDiskSource(SelectedDiskSource enmSelectedDiskSource);

    void getWithFileOpenDialog();
    bool getWithNewVirtualDiskWizard();


    QWidget *createDiskWidgets();

    void ensureNewVirtualDiskDeleted();
    void retranslateWidgets();

    void setEnableDiskSelectionWidgets(bool fEnable);
    bool m_fRecommendedNoDisk;

    /** @name Variables
     * @{ */
       CMedium m_virtualDisk;
    /** @} */

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

class UIWizardNewVMPageBasic4 : public UIWizardPage,
                                public UIWizardNewVMPage4,
                                public UIWizardNewVDPage1,
                                public UIWizardNewVDPage2,
                                public UIWizardNewVDPage3
{
    Q_OBJECT;
    Q_PROPERTY(CMedium virtualDisk READ virtualDisk WRITE setVirtualDisk);
    Q_PROPERTY(SelectedDiskSource selectedDiskSource READ selectedDiskSource WRITE setSelectedDiskSource);
    Q_PROPERTY(CMediumFormat mediumFormat READ mediumFormat);
    Q_PROPERTY(qulonglong mediumVariant READ mediumVariant WRITE setMediumVariant);
    Q_PROPERTY(QString mediumPath READ mediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    UIWizardNewVMPageBasic4();
    /** For the guide wizard mode medium path, name and extention is static and we have
      * no UI element for this. thus override. */
    virtual QString mediumPath() const /*override */;
    virtual int nextId() const /* override */;
    CMediumFormat mediumFormat() const;

protected:

    /** Wrapper to access 'wizard' from base part. */
    UIWizard *wizardImp() const { return wizard(); }
    /** Wrapper to access 'this' from base part. */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    void sltHandleSelectedDiskSourceChange();
    void sltVirtualSelectedDiskSourceChanged();
    void sltGetWithFileOpenDialog();
    void sltHandleSizeEditorChange();

private:


    void prepare();
    void createConnections();
    void retranslateUi();
    void initializePage();
    void cleanupPage();

    bool isComplete() const;
    bool validatePage();

    QIRichTextLabel *m_pLabel;

    /** This is set to true when user manually set the size. */
    bool m_fUserSetSize;
    /** For guided new vm wizard VDI is the only format. Thus we have no UI item for it. */
    CMediumFormat m_mediumFormat;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_newvm_UIWizardNewVMPageBasic4_h */
