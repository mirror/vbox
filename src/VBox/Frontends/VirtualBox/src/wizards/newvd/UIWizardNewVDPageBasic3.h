/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageBasic3 class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic3_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic3_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class CMediumFormat;
class QLabel;
class QLineEdit;
class QIToolButton;
class QIRichTextLabel;
class UIMediumSizeEditor;


/* 3rd page of the New Virtual Hard Drive wizard (base part): */
class SHARED_LIBRARY_STUFF UIWizardNewVDPage3 : public UIWizardPageBase
{

public:

    static QString defaultExtension(const CMediumFormat &mediumFormatRef);
    /* Checks if the medium file is bigger than what is allowed in FAT file systems. */
    static bool checkFATSizeLimitation(const qulonglong uVariant, const QString &strMediumPath, const qulonglong uSize);

protected:

    UIWizardNewVDPage3(const QString &strDefaultName, const QString &strDefaultPath);
    UIWizardNewVDPage3();

    void onSelectLocationButtonClicked();

    static QString toFileName(const QString &strName, const QString &strExtension);
    /** Returns the full image file path except the extension. */
    static QString absoluteFilePath(const QString &strFileName, const QString &strPath);
    /** Returns the full image file path including the extension. */
    static QString absoluteFilePath(const QString &strFileName, const QString &strPath, const QString &strExtension);

    virtual QString mediumPath() const;

    qulonglong mediumSize() const;
    void setMediumSize(qulonglong uMediumSize);
    void retranslateWidgets();

    /** @name Widgets
     * @{ */
       QString m_strDefaultName;
       QString m_strDefaultPath;
       QString m_strDefaultExtension;
       qulonglong m_uMediumSizeMin;
       qulonglong m_uMediumSizeMax;
    /** @} */

    /** @name Widgets
     * @{ */
       QLineEdit *m_pLocationEditor;
       QIToolButton *m_pLocationOpenButton;
       UIMediumSizeEditor *m_pSizeEditor;
       QLabel          *m_pSizeEditorLabel;
       QIRichTextLabel *m_pLocationLabel;
       QIRichTextLabel *m_pSizeLabel;
    /** @} */
};


/* 3rd page of the New Virtual Hard Drive wizard (basic extension): */
class SHARED_LIBRARY_STUFF UIWizardNewVDPageBasic3 : public UIWizardPage, public UIWizardNewVDPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString mediumPath READ mediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    UIWizardNewVDPageBasic3(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize);

protected:

    /** Wrapper to access 'this' from base part: */
    UIWizardPage* thisImp() { return this; }
    /** Wrapper to access 'wizard-field' from base part: */
    QVariant fieldImp(const QString &strFieldName) const { return UIWizardPage::field(strFieldName); }

private slots:

    /** Location editors stuff: */
    void sltSelectLocationButtonClicked();

private:

    void retranslateUi();
    void initializePage();
    bool isComplete() const;
    bool validatePage();
};


#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDPageBasic3_h */
