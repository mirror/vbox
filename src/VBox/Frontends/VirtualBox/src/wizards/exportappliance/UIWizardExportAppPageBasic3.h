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
class QLabel;
class QLineEdit;
class UIEmptyFilePathSelector;
class QComboBox;
class QCheckBox;
class QIRichTextLabel;


/** UIWizardPageBase extension for 3rd page of the Export Appliance wizard. */
class UIWizardExportAppPage3 : public UIWizardPageBase
{
protected:

    /** Constructs 3rd page base. */
    UIWizardExportAppPage3();

    /** Chooses default settings. */
    void chooseDefaultSettings();
    /** Refreshes current settings. */
    virtual void refreshCurrentSettings();
    /** Updates format combo tool-tips. */
    virtual void updateFormatComboToolTip();

    /** Returns format. */
    QString format() const;
    /** Defines @a strFormat. */
    void setFormat(const QString &strFormat);

    /** Returns whether manifest selected. */
    bool isManifestSelected() const;
    /** Defines whether manifest @a fSelected. */
    void setManifestSelected(bool fChecked);

    /** Returns user name. */
    QString username() const;
    /** Defines @a strUserName. */
    void setUserName(const QString &strUserName);

    /** Returns password. */
    QString password() const;
    /** Defines @a strPassword. */
    void setPassword(const QString &strPassword);

    /** Returns hostname. */
    QString hostname() const;
    /** Defines @a strHostname. */
    void setHostname(const QString &strHostname);

    /** Returns bucket. */
    QString bucket() const;
    /** Defines @a strBucket. */
    void setBucket(const QString &strBucket);

    /** Returns path. */
    QString path() const;
    /** Defines @a strPath. */
    void setPath(const QString &strPath);

    /** Holds the default appliance name. */
    QString m_strDefaultApplianceName;

    /** Holds the username label instance. */
    QLabel *m_pUsernameLabel;
    /** Holds the username editor instance. */
    QLineEdit *m_pUsernameEditor;

    /** Holds the password label instance. */
    QLabel *m_pPasswordLabel;
    /** Holds the password editor instance. */
    QLineEdit *m_pPasswordEditor;

    /** Holds the hostname label instance. */
    QLabel *m_pHostnameLabel;
    /** Holds the hostname editor instance. */
    QLineEdit *m_pHostnameEditor;

    /** Holds the bucket label instance. */
    QLabel *m_pBucketLabel;
    /** Holds the bucket editor instance. */
    QLineEdit *m_pBucketEditor;

    /** Holds the file selector label instance. */
    QLabel *m_pFileSelectorLabel;
    /** Holds the file selector instance. */
    UIEmptyFilePathSelector *m_pFileSelector;

    /** Holds the format combo-box label instance. */
    QLabel *m_pFormatComboBoxLabel;
    /** Holds the format combo-box instance. */
    QComboBox *m_pFormatComboBox;

    /** Holds the manifest check-box instance. */
    QCheckBox *m_pManifestCheckbox;
};


/** UIWizardPage extension for 3rd page of the Export Appliance wizard, extends UIWizardExportAppPage3 as well. */
class UIWizardExportAppPageBasic3 : public UIWizardPage, public UIWizardExportAppPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString format READ format WRITE setFormat);
    Q_PROPERTY(bool manifestSelected READ isManifestSelected WRITE setManifestSelected);
    Q_PROPERTY(QString username READ username WRITE setUserName);
    Q_PROPERTY(QString password READ password WRITE setPassword);
    Q_PROPERTY(QString hostname READ hostname WRITE setHostname);
    Q_PROPERTY(QString bucket READ bucket WRITE setBucket);
    Q_PROPERTY(QString path READ path WRITE setPath);

public:

    /** Constructs 3rd basic page. */
    UIWizardExportAppPageBasic3();

protected:

    /** Allows access wizard-field from base part. */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /** Handles change in format combo-box. */
    void sltHandleFormatComboChange();

private:

    /** Handles translation event. */
    void retranslateUi();

    /** Performs page initialization. */
    void initializePage();

    /** Returns whether page is complete. */
    bool isComplete() const;

    /** Refreshes current settings. */
    void refreshCurrentSettings();

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !___UIWizardExportAppPageBasic3_h___ */
