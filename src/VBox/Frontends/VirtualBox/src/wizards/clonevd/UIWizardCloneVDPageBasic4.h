/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic4 class declaration
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

#ifndef __UIWizardCloneVDPageBasic4_h__
#define __UIWizardCloneVDPageBasic4_h__

/* Local includes: */
#include "UIWizardPage.h"
#include "COMDefs.h"

/* Forward declarations: */
class QIRichTextLabel;
class QGroupBox;
class QLineEdit;
class QIToolButton;
class CMediumFormat;

/* 4th page of the Clone Virtual Disk wizard: */
class UIWizardCloneVDPageBasic4 : public UIWizardPage
{
    Q_OBJECT;
    Q_PROPERTY(QString mediumPath READ mediumPath WRITE setMediumPath);
    Q_PROPERTY(qulonglong mediumSize READ mediumSize WRITE setMediumSize);

public:

    /* Constructor: */
    UIWizardCloneVDPageBasic4();

private slots:

    /* Location editors stuff: */
    void sltLocationEditorTextChanged(const QString &strMediumName);
    void sltSelectLocationButtonClicked();

private:

    /* Translation stuff: */
    void retranslateUi();

    /* Prepare stuff: */
    void initializePage();

    /* Validation stuff: */
    bool isComplete() const;
    bool validatePage();

    /* Location-editors stuff: */
    static QString toFileName(const QString &strName, const QString &strExtension);
    static QString absoluteFilePath(const QString &strFileName, const QString &strDefaultPath);
    static QString defaultExtension(const CMediumFormat &mediumFormatRef);

    /* Stuff for 'mediumPath' field: */
    QString mediumPath() const { return m_strMediumPath; }
    void setMediumPath(const QString &strMediumPath) { m_strMediumPath = strMediumPath; }
    QString m_strMediumPath;

    /* Stuff for 'mediumSize' field: */
    qulonglong mediumSize() const { return m_uMediumSize; }
    void setMediumSize(qulonglong uMediumSize) { m_uMediumSize = uMediumSize; }
    qulonglong m_uMediumSize;

    /* Location-editors variables: */
    QString m_strDefaultPath;
    QString m_strDefaultExtension;

    /* Widgets: */
    QIRichTextLabel *m_pLabel;
    QGroupBox *m_pLocationCnt;
    QLineEdit *m_pLocationEditor;
    QIToolButton *m_pLocationSelector;
};

#endif // __UIWizardCloneVDPageBasic4_h__

