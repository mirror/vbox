/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardDiskEditors class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIWizardCloneVMEditors_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIWizardCloneVMEditors_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
// #include <QIcon>
#include <QGroupBox>

/* Local includes: */
#include "QIWithRetranslateUI.h"


/* Forward declarations: */
// class CMediumFormat;
// class QButtonGroup;
// class QCheckBox;
// class QGridLayout;
class QComboBox;
class QLabel;
// class QVBoxLayout;
// class QIRichTextLabel;
class QILineEdit;
// class QIToolButton;
class UIFilePathSelector;
// class UIHostnameDomainNameEditor;
// class UIPasswordLineEdit;
// class UIUserNamePasswordEditor;
// class UIMediumSizeEditor;

/* Other VBox includes: */
// #include "COMEnums.h"
// #include "CMediumFormat.h"

/** MAC address policies. */
enum MACAddressClonePolicy
{
    MACAddressClonePolicy_KeepAllMACs,
    MACAddressClonePolicy_KeepNATMACs,
    MACAddressClonePolicy_StripAllMACs,
    MACAddressClonePolicy_MAX
};
Q_DECLARE_METATYPE(MACAddressClonePolicy);

class UICloneVMNamePathEditor : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:

    // void sigNameChanged(const QString &strUserName);
    // void sigPathChanged(const QString &strPassword);

public:

    UICloneVMNamePathEditor(const QString &strOriginalName, const QString &strDefaultPath, QWidget *pParent = 0);

    QString name() const;
    void setName(const QString &strName);

    QString path() const;
    void setPath(const QString &strPath);
    bool isComplete();

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    QILineEdit  *m_pNameLineEdit;
    UIFilePathSelector *m_pPathSelector;
    QLabel      *m_pNameLabel;
    QLabel      *m_pPathLabel;

    QString      m_strOriginalName;
    QString      m_strDefaultPath;
};


class UICloneVMAdditionalOptionsEditor : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:


public:

    UICloneVMAdditionalOptionsEditor(QWidget *pParent = 0);

    bool isComplete();

    MACAddressClonePolicy macAddressClonePolicy() const;
    void setMACAddressClonePolicy(MACAddressClonePolicy enmMACAddressClonePolicy);

private:

    void prepare();
    virtual void retranslateUi() /* override final */;
    void populateMACAddressClonePolicies();

    QLabel *m_pMACComboBoxLabel;
    QComboBox *m_pMACComboBox;

};


#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardCloneVMEditors_h */
