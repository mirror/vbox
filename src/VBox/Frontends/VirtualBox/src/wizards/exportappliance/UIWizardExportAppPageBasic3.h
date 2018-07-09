/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIWizardExportAppPageBasic3_h___
#define ___UIWizardExportAppPageBasic3_h___

/* Qt includes: */
#include <QVariant>

/* GUI includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QIRichTextLabel;
class UIEmptyFilePathSelector;


/** UIWizardPageBase extension for 3rd page of the Export Appliance wizard. */
class UIWizardExportAppPage3 : public UIWizardPageBase
{
protected:

    /** Constructs 3rd page base. */
    UIWizardExportAppPage3();

    /** Populates providers. */
    void populateProviders();

    /** Chooses default settings. */
    void chooseDefaultSettings();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Refreshes current settings. */
    void refreshCurrentSettings();

    /** Updates format combo tool-tips. */
    void updateFormatComboToolTip();
    /** Updates provider combo tool-tips. */
    void updateProviderComboToolTip();

    /** Returns path. */
    QString path() const;
    /** Defines @a strPath. */
    void setPath(const QString &strPath);

    /** Returns format. */
    QString format() const;
    /** Defines @a strFormat. */
    void setFormat(const QString &strFormat);

    /** Returns whether manifest selected. */
    bool isManifestSelected() const;
    /** Defines whether manifest @a fSelected. */
    void setManifestSelected(bool fChecked);

    /** Returns provider. */
    QString provider() const;
    /** Defines @a strProvider. */
    void setProvider(const QString &strProvider);

    /** Holds the default appliance name. */
    QString  m_strDefaultApplianceName;

    /** Holds the settings widget instance. */
    QStackedWidget *m_pSettingsWidget;

    /** Holds the file selector label instance. */
    QLabel    *m_pFileSelectorLabel;
    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;

    /** Holds the format combo-box label instance. */
    QLabel    *m_pFormatComboBoxLabel;
    /** Holds the format combo-box instance. */
    QComboBox *m_pFormatComboBox;

    /** Holds the additional label instance. */
    QLabel    *m_pAdditionalLabel;
    /** Holds the manifest check-box instance. */
    QCheckBox *m_pManifestCheckbox;

    /** Holds the provider combo-box label instance. */
    QLabel    *m_pProviderComboBoxLabel;
    /** Holds the provider combo-box instance. */
    QComboBox *m_pProviderComboBox;
};


/** UIWizardPage extension for 3rd page of the Export Appliance wizard, extends UIWizardExportAppPage3 as well. */
class UIWizardExportAppPageBasic3 : public UIWizardPage, public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString path READ path WRITE setPath);
    Q_PROPERTY(QString format READ format WRITE setFormat);
    Q_PROPERTY(bool manifestSelected READ isManifestSelected WRITE setManifestSelected);
    Q_PROPERTY(QString provider READ provider WRITE setProvider);

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Updates page appearance. */
    virtual void updatePageAppearance() /* override */;

private slots:

    /** Handles change in format combo-box. */
    void sltHandleFormatComboChange();

    /** Handles change in provider combo-box. */
    void sltHandleProviderComboChange();

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic3_h___ */
