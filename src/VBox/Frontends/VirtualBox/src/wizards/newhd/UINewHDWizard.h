/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewHDWizard class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINewHDWizard_h__
#define __UINewHDWizard_h__

/* Local includes: */
#include "QIWizard.h"
#include "COMDefs.h"

/* Generated includes: */
#include "UINewHDWizardPageWelcome.gen.h"
#include "UINewHDWizardPageFormat.gen.h"
#include "UINewHDWizardPageVariant.gen.h"
#include "UINewHDWizardPageOptions.gen.h"
#include "UINewHDWizardPageSummary.gen.h"

/* Forward declarations: */
class QRadioButton;
class QCheckBox;
class UIExclusivenessManager;

/* Wizard type: */
enum UINewHDWizardType
{
    UINewHDWizardType_Creating,
    UINewHDWizardType_Copying
};

/* New hard disk wizard class: */
class UINewHDWizard : public QIWizard
{
    Q_OBJECT;

public:

    enum
    {
        PageWelcome,
        PageFormat,
        PageVariant,
        PageOptions,
        PageSummary
    };

    /* Constructor: */
    UINewHDWizard(QWidget *pParent,
                  const QString &strDefaultName = QString(), const QString &strDefaultPath = QString(),
                  qulonglong uDefaultSize = 0, const CMedium &sourceHardDisk = CMedium());

    /* Stuff for wizard type: */
    UINewHDWizardType wizardType() const { return m_wizardType; }

    /* Returns created hard disk: */
    CMedium hardDisk() const;

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Wizard type: */
    UINewHDWizardType m_wizardType;
};

/* Base wrapper for the wizard page
 * of the new hard disk wizard class: */
class UINewHDWizardPage : public QIWizardPage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UINewHDWizardPage() {}

protected:

    /* Returns parent wizard object: */
    UINewHDWizard* wizard() const { return qobject_cast<UINewHDWizard*>(QIWizardPage::wizard()); }

    /* Returns parent wizard type: */
    UINewHDWizardType wizardType() const { return wizard()->wizardType(); }
};

/* Welcome page of the new hard-disk wizard: */
class UINewHDWizardPageWelcome : public UINewHDWizardPage, public Ui::UINewHDWizardPageWelcome
{
    Q_OBJECT;
    Q_PROPERTY(CMedium sourceHardDisk READ sourceHardDisk WRITE setSourceHardDisk);

public:

    /* Constructor: */
    UINewHDWizardPageWelcome(const CMedium &sourceHardDisk);

private slots:

    /* Handlers for source disk change: */
    void sltHandleSourceDiskChange();
    void sltHandleOpenSourceDiskClick();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();

    /* Completeness validator: */
    bool isComplete() const;

    /* Stuff for 'sourceHardDisk' field: */
    CMedium sourceHardDisk() const { return m_sourceHardDisk; }
    void setSourceHardDisk(const CMedium &sourceHardDisk) { m_sourceHardDisk = sourceHardDisk; }
    CMedium m_sourceHardDisk;
};

/* Format page of the new hard-disk wizard: */
class UINewHDWizardPageFormat : public UINewHDWizardPage, public Ui::UINewHDWizardPageFormat
{
    Q_OBJECT;
    Q_PROPERTY(CMediumFormat mediumFormat READ mediumFormat WRITE setMediumFormat);

public:

    /* Constructor: */
    UINewHDWizardPageFormat();

    /* Returns full medium format name: */
    static QString fullFormatName(const QString &strBaseFormatName);

private slots:

    /* Handler for the 'format'-change signal: */
    void sltUpdateFormat(QVariant formatData);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();
    /* Cleanup page: */
    void cleanupPage();

    /* Completeness validator: */
    bool isComplete() const;

    int nextId() const;

    /* Helping stuff: */
    void processFormat(CMediumFormat mediumFormat);

    /* Exclusiveness manager: */
    UIExclusivenessManager *m_pExclusivenessManager;

    /* Defaut format (VDI) button: */
    QRadioButton *m_pDefaultButton;

    /* Stuff for 'mediumFormat' field: */
    CMediumFormat mediumFormat() const { return m_mediumFormat; }
    void setMediumFormat(const CMediumFormat &mediumFormat) { m_mediumFormat = mediumFormat; }
    CMediumFormat m_mediumFormat;
};

/* Variant page of the new hard-disk wizard: */
class UINewHDWizardPageVariant : public UINewHDWizardPage, public Ui::UINewHDWizardPageVariant
{
    Q_OBJECT;
    Q_PROPERTY(qulonglong mediumVariant READ mediumVariant WRITE setMediumVariant);

public:

    /* Constructor: */
    UINewHDWizardPageVariant();

private slots:

    /* Handler for the 'variant'-change signal: */
    void sltUpdateVariant(QVariant exclusiveData, QList<QVariant> optionsData);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();
    /* Cleanup page: */
    void cleanupPage();

    /* Completeness validator: */
    bool isComplete() const;

    /* Exclusiveness manager: */
    UIExclusivenessManager *m_pExclusivenessManager;

    /* Defaut variant (dynamic) button: */
    QRadioButton *m_pDynamicalButton;
    QRadioButton *m_pFixedButton;
    QCheckBox *m_pSplitBox;

    /* Stuff for 'variant' field: */
    qulonglong mediumVariant() const { return m_uMediumVariant; }
    void setMediumVariant(qulonglong uMediumVariant) { m_uMediumVariant = uMediumVariant; }
    qulonglong m_uMediumVariant;
};

/* Options page of the new hard-disk wizard: */
class UINewHDWizardPageOptions : public UINewHDWizardPage, public Ui::UINewHDWizardPageOptions
{
    Q_OBJECT;
    Q_PROPERTY(QString mediumName READ mediumName WRITE setMediumName);
    Q_PROPERTY(QString mediumPath READ mediumPath WRITE setMediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    /* Constructor: */
    UINewHDWizardPageOptions(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

protected:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();
    /* Cleanup page: */
    void cleanupPage();

    /* Completeness validator: */
    bool isComplete() const;
    /* Completeness finisher: */
    bool validatePage();

private slots:

    /* Location editors stuff: */
    void sltLocationEditorTextChanged(const QString &strName);
    void sltSelectLocationButtonClicked();

    /* Size editors stuff: */
    void sltSizeSliderValueChanged(int iValue);
    void sltSizeEditorTextChanged(const QString &strValue);

private:

    /* Returns 'file name' for the passed 'name': */
    static QString toFileName(const QString &strName, const QString &strExtension);
    /* Returns 'absolute file path' for the passed 'file name': */
    static QString absoluteFilePath(const QString &strFileName, const QString &strDefaultPath);
    /* Returns default extension for the passed medium format: */
    static QString defaultExtension(CMediumFormat mediumFormat);

    /* Size editors stuff: */
    static int log2i(qulonglong uValue);
    static int sizeMBToSlider(qulonglong uValue, int iSliderScale);
    static qulonglong sliderToSizeMB(int uValue, int iSliderScale);
    void updateSizeToolTip(qulonglong uSize);

    /* The default extension for the hard disk file: */
    QString m_strDefaultExtension;

    /* The default path for the hard disk file: */
    QString m_strDefaultPath;

    /* Stuff for 'mediumName' field: */
    QString mediumName() const { return m_strMediumName; }
    void setMediumName(const QString &strMediumName) { m_strMediumName = strMediumName; }
    QString m_strMediumName;

    /* Stuff for 'mediumPath' field: */
    QString mediumPath() const { return m_strMediumPath; }
    void setMediumPath(const QString &strMediumPath) { m_strMediumPath = strMediumPath; }
    QString m_strMediumPath;

    /* Stuff for 'mediumSize' field: */
    qulonglong mediumSize() const { return m_uMediumSize; }
    void setMediumSize(qulonglong uMediumSize) { m_uMediumSize = uMediumSize; }
    qulonglong m_uMediumSize;

    /* Other size editors stuff: */
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;
    int m_iSliderScale;
};

/* Summary page of the new hard-disk wizard: */
class UINewHDWizardPageSummary : public UINewHDWizardPage, public Ui::UINewHDWizardPageSummary
{
    Q_OBJECT;
    Q_PROPERTY(CMedium hardDisk READ hardDisk WRITE setHardDisk);

public:

    /* Constructor: */
    UINewHDWizardPageSummary();

protected:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare page: */
    void initializePage();

    /* Completeness finisher: */
    bool validatePage();

private:

    /* Creates hard disk: */
    bool createHardDisk();

    /* Stuff for 'hardDisk' field: */
    CMedium hardDisk() const { return m_hardDisk; }
    void setHardDisk(const CMedium &hardDisk) { m_hardDisk = hardDisk; }
    CMedium m_hardDisk;
};

Q_DECLARE_METATYPE(CMedium);

#endif // __UINewHDWizard_h__

