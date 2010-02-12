/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewHDWzd class declaration
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UINewHDWzd_h__
#define __UINewHDWzd_h__

/* Local includes */
#include "QIWizard.h"
#include "COMDefs.h"

/* Generated includes */
#include "UINewHDWzdPage1.gen.h"
#include "UINewHDWzdPage2.gen.h"
#include "UINewHDWzdPage3.gen.h"
#include "UINewHDWzdPage4.gen.h"

class UINewHDWzd : public QIWizard
{
    Q_OBJECT;

public:

    UINewHDWzd(QWidget *pParent);

    CMedium hardDisk() const;

    void setRecommendedName(const QString &strName);
    void setRecommendedSize(qulonglong uSize);

    static QString composeFullFileName(const QString &strFileName);

protected:

    void retranslateUi();
};

class UINewHDWzdPage1 : public QIWizardPage, public Ui::UINewHDWzdPage1
{
    Q_OBJECT;

public:

    UINewHDWzdPage1();

protected:

    void retranslateUi();

    void initializePage();
};

class UINewHDWzdPage2 : public QIWizardPage, public Ui::UINewHDWzdPage2
{
    Q_OBJECT;
    Q_PROPERTY(QString type READ type WRITE setType);
    Q_PROPERTY(bool fixed READ fixed WRITE setFixed);

public:

    UINewHDWzdPage2();

protected:

    void retranslateUi();

    void initializePage();

private slots:

    void onTypeChanged();

private:

    QString type() const { return m_strType; }
    void setType(const QString &strType) { m_strType = strType; }
    QString m_strType;

    bool fixed() const { return m_bFixed; }
    void setFixed(bool bFixed) { m_bFixed = bFixed; }
    bool m_bFixed;
};

class UINewHDWzdPage3 : public QIWizardPage, public Ui::UINewHDWzdPage3
{
    Q_OBJECT;
    Q_PROPERTY(QString initialName READ initialName WRITE setInitialName);
    Q_PROPERTY(QString currentName READ currentName WRITE setCurrentName);
    Q_PROPERTY(QString location READ location WRITE setLocation);
    Q_PROPERTY(qulonglong initialSize READ initialSize WRITE setInitialSize);
    Q_PROPERTY(qulonglong currentSize READ currentSize WRITE setCurrentSize);

public:

    UINewHDWzdPage3();

protected:

    void retranslateUi();

    void initializePage();
    void cleanupPage();

    bool isComplete() const;
    bool validatePage();

private slots:

    void onLocationEditorTextChanged(const QString &strName);
    void onSelectLocationButtonClicked();

    void onSizeSliderValueChanged(int iValue);
    void onSizeEditorTextChanged(const QString &strValue);

private:

    static QString toFileName(const QString &strName);

    static int log2i(qulonglong uValue);
    static int sizeMBToSlider(qulonglong uValue, int iSliderScale);
    static qulonglong sliderToSizeMB(int uValue, int iSliderScale);

    void updateSizeToolTip(qulonglong uSize);

    QString initialName() const { return m_strInitialName; }
    void setInitialName(const QString &strInitialName) { m_strInitialName = strInitialName; }
    QString m_strInitialName;

    QString currentName() const { return m_strCurrentName; }
    void setCurrentName(const QString &strCurrentName) { m_strCurrentName = strCurrentName; }
    QString m_strCurrentName;

    QString location() const { return m_strLocation; }
    void setLocation(const QString &strLocation) { m_strLocation = strLocation; }
    QString m_strLocation;

    qulonglong initialSize() const { return m_uInitialSize; }
    void setInitialSize(qulonglong uInitialSize) { m_uInitialSize = uInitialSize; }
    qulonglong m_uInitialSize;

    qulonglong currentSize() const { return m_uCurrentSize; }
    void setCurrentSize(qulonglong uCurrentSize) { m_uCurrentSize = uCurrentSize; }
    qulonglong m_uCurrentSize;

    qulonglong m_uMinVDISize;
    qulonglong m_uMaxVDISize;
    int m_iSliderScale;
};

class UINewHDWzdPage4 : public QIWizardPage, public Ui::UINewHDWzdPage4
{
    Q_OBJECT;
    Q_PROPERTY(CMedium hardDisk READ hardDisk WRITE setHardDisk);

public:

    UINewHDWzdPage4();

protected:

    void retranslateUi();

    void initializePage();

    bool validatePage();

private:

    bool createHardDisk();

    CMedium hardDisk() const { return m_HardDisk; }
    void setHardDisk(const CMedium &hardDisk) { m_HardDisk = hardDisk; }
    CMedium m_HardDisk;
};

Q_DECLARE_METATYPE(CMedium);

#endif // __UINewHDWzd_h__
