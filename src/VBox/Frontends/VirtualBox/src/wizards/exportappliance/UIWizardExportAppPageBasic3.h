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

/* COM includes: */
#include "COMEnums.h"
#include "CCloudUserProfileList.h"
#include "CCloudUserProfileManager.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTableWidget;
class QIRichTextLabel;
class UIEmptyFilePathSelector;


/** MAC address policies. */
enum MACAddressPolicy
{
    MACAddressPolicy_KeepAllMACs,
    MACAddressPolicy_StripAllNonNATMACs,
    MACAddressPolicy_StripAllMACs,
    MACAddressPolicy_MAX
};
Q_DECLARE_METATYPE(MACAddressPolicy);

/** Account combo data fields. */
enum
{
    ProviderID  = Qt::UserRole + 1,
    ProfileName = Qt::UserRole + 2
};


/** UIWizardPageBase extension for 3rd page of the Export Appliance wizard. */
class UIWizardExportAppPage3 : public UIWizardPageBase
{
protected:

    /** Constructs 3rd page base. */
    UIWizardExportAppPage3();

    /** Populates formats. */
    void populateFormats();
    /** Populates MAC address policies. */
    void populateMACAddressPolicies();
    /** Populates accounts. */
    void populateAccounts();
    /** Populates account properties. */
    void populateAccountProperties();

    /** Updates page appearance. */
    virtual void updatePageAppearance();

    /** Refresh file selector name. */
    void refreshFileSelectorName();
    /** Refresh file selector extension. */
    void refreshFileSelectorExtension();
    /** Refresh file selector path. */
    void refreshFileSelectorPath();
    /** Refresh Manifest check-box access. */
    void refreshManifestCheckBoxAccess();
    /** Refresh Include ISOs check-box access. */
    void refreshIncludeISOsCheckBoxAccess();

    /** Updates format combo tool-tips. */
    void updateFormatComboToolTip();
    /** Updates MAC address policy combo tool-tips. */
    void updateMACAddressPolicyComboToolTip();
    /** Updates account property table tool-tips. */
    void updateAccountPropertyTableToolTips();
    /** Adjusts account property table. */
    void adjustAccountPropertyTable();

    /** Returns format. */
    QString format() const;
    /** Defines @a strFormat. */
    void setFormat(const QString &strFormat);

    /** Returns path. */
    QString path() const;
    /** Defines @a strPath. */
    void setPath(const QString &strPath);

    /** Returns MAC address policy. */
    MACAddressPolicy macAddressPolicy() const;
    /** Defines @a enmMACAddressPolicy. */
    void setMACAddressPolicy(MACAddressPolicy enmMACAddressPolicy);

    /** Returns whether manifest selected. */
    bool isManifestSelected() const;
    /** Defines whether manifest @a fSelected. */
    void setManifestSelected(bool fChecked);

    /** Returns whether include ISOs selected. */
    bool isIncludeISOsSelected() const;
    /** Defines whether include ISOs @a fSelected. */
    void setIncludeISOsSelected(bool fChecked);

    /** Defines @a strProvider. */
    void setProvider(KCloudProviderId enmProvider);
    /** Returns provider. */
    KCloudProviderId provider() const;
    /** Returns profile. */
    QString profile() const;
    /** Returns Cloud User-profile object. */
    CCloudUserProfileList profiles() const;

    /** Holds the Cloud User-profile Manager reference. */
    CCloudUserProfileManager  m_comCloudUserProfileManager;
    /** Holds the Cloud User-profile object reference. */
    CCloudUserProfileList     m_comCloudUserProfiles;

    /** Holds the default appliance name. */
    QString  m_strDefaultApplianceName;

    /** Holds the file selector name. */
    QString  m_strFileSelectorName;
    /** Holds the file selector ext. */
    QString  m_strFileSelectorExt;

    /** Holds the format layout. */
    QGridLayout *m_pFormatLayout;
    /** Holds the settings layout 1. */
    QGridLayout *m_pSettingsLayout1;
    /** Holds the settings layout 2. */
    QGridLayout *m_pSettingsLayout2;

    /** Holds the format combo-box label instance. */
    QLabel    *m_pFormatComboBoxLabel;
    /** Holds the format combo-box instance. */
    QComboBox *m_pFormatComboBox;

    /** Holds the settings widget instance. */
    QStackedWidget *m_pSettingsWidget;

    /** Holds the file selector label instance. */
    QLabel    *m_pFileSelectorLabel;
    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;

    /** Holds the MAC address policy combo-box label instance. */
    QLabel    *m_pMACComboBoxLabel;
    /** Holds the MAC address policy check-box instance. */
    QComboBox *m_pMACComboBox;

    /** Holds the additional label instance. */
    QLabel    *m_pAdditionalLabel;
    /** Holds the manifest check-box instance. */
    QCheckBox *m_pManifestCheckbox;
    /** Holds the include ISOs check-box instance. */
    QCheckBox *m_pIncludeISOsCheckbox;

    /** Holds the account combo-box label instance. */
    QLabel       *m_pAccountComboBoxLabel;
    /** Holds the account combo-box instance. */
    QComboBox    *m_pAccountComboBox;
    /** Holds the account property table instance. */
    QTableWidget *m_pAccountPropertyTable;
};


/** UIWizardPage extension for 3rd page of the Export Appliance wizard, extends UIWizardExportAppPage3 as well. */
class UIWizardExportAppPageBasic3 : public UIWizardPage, public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString format READ format WRITE setFormat);
    Q_PROPERTY(QString path READ path WRITE setPath);
    Q_PROPERTY(MACAddressPolicy macAddressPolicy READ macAddressPolicy WRITE setMACAddressPolicy);
    Q_PROPERTY(bool manifestSelected READ isManifestSelected WRITE setManifestSelected);
    Q_PROPERTY(bool includeISOsSelected READ isIncludeISOsSelected WRITE setIncludeISOsSelected);
    Q_PROPERTY(CCloudUserProfileList profiles READ profiles);
    Q_PROPERTY(QString profile READ profile);

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Handle any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

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

    /** Handles change in MAC address policy combo-box. */
    void sltHandleMACAddressPolicyComboChange();

    /** Handles change in account combo-box. */
    void sltHandleAccountComboChange();

private:

    /** Holds the format label instance. */
    QIRichTextLabel *m_pLabelFormat;

    /** Holds the settings label instance. */
    QIRichTextLabel *m_pLabelSettings;
};

#endif /* !___UIWizardExportAppPageBasic3_h___ */
