/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic3 class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIWizardNewVDPageBasic3_h__
#define __UIWizardNewVDPageBasic3_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class CMediumFormat;
class QGroupBox;
class QLineEdit;
class QIToolButton;
class QSlider;
class QILineEdit;
class QIRichTextLabel;

/* 3rd page of the New Virtual Hard Drive wizard (base part): */
class UIWizardNewVDPage3 : public UIWizardPageBase
{
protected:

    /* Constructor: */
    UIWizardNewVDPage3(const QString &strDefaultName, const QString &strDefaultPath);

    /* Handlers: */
    void onSelectLocationButtonClicked();
    void onSizeSliderValueChanged(int iValue);
    void onSizeEditorTextChanged(const QString &strValue);

    /* Location-editors stuff: */
    static QString toFileName(const QString &strName, const QString &strExtension);
    static QString absoluteFilePath(const QString &strFileName, const QString &strDefaultPath);
    static QString defaultExtension(const CMediumFormat &mediumFormatRef);

    /* Size-editors stuff: */
    static int calculateSliderScale(qulonglong uMaximumMediumSize);
    static int log2i(qulonglong uValue);
    static int sizeMBToSlider(qulonglong uValue, int iSliderScale);
    static qulonglong sliderToSizeMB(int uValue, int iSliderScale);
    void updateSizeToolTips(qulonglong uSize);

    /* Stuff for 'mediumPath' field: */
    QString mediumPath() const;

    /* Stuff for 'mediumSize' field: */
    qulonglong mediumSize() const;
    void setMediumSize(qulonglong uMediumSize);

    /* Variables: */
    QString m_strDefaultName;
    QString m_strDefaultPath;
    QString m_strDefaultExtension;
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;
    int m_iSliderScale;

    /* Widgets: */
    QGroupBox *m_pLocationCnt;
    QLineEdit *m_pLocationEditor;
    QIToolButton *m_pLocationOpenButton;
    QGroupBox *m_pSizeCnt;
    QSlider *m_pSizeSlider;
    QILineEdit *m_pSizeEditor;
};

/* 3rd page of the New Virtual Hard Drive wizard (basic extension): */
class UIWizardNewVDPageBasic3 : public UIWizardPage, public UIWizardNewVDPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString mediumPath READ mediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    /* Constructor: */
    UIWizardNewVDPageBasic3(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

protected:

    /* Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }
    /* Wrapper to access 'wizard-field' from base part: */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /* Location editors stuff: */
    void sltSelectLocationButtonClicked();

    /* Size editors stuff: */
    void sltSizeSliderValueChanged(int iValue);
    void sltSizeEditorTextChanged(const QString &strValue);

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Widgets: */
    QIRichTextLabel *m_pLocationLabel;
    QIRichTextLabel *m_pSizeLabel;
};

#endif // __UIWizardNewVDPageBasic3_h__

