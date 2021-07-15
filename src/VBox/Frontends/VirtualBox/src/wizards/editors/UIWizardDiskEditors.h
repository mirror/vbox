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

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIWizardDiskEditors_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIWizardDiskEditors_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QGroupBox>

/* Local includes: */
#include "QIWithRetranslateUI.h"


/* Forward declarations: */
class CMediumFormat;
class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QLabel;
class QVBoxLayout;
class QILineEdit;
class QIToolButton;
class UIFilePathSelector;
class UIHostnameDomainNameEditor;
class UIPasswordLineEdit;
class UIUserNamePasswordEditor;
class UIMediumSizeEditor;

/* Other VBox includes: */
#include "COMEnums.h"
#include "CMediumFormat.h"

class UIDiskFormatsGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:


public:

    UIDiskFormatsGroupBox(QWidget *pParent = 0);
    CMediumFormat mediumFormat() const;
    void setMediumFormat(const CMediumFormat &mediumFormat);

private:

    void prepare();
    void addFormatButton(QVBoxLayout *pFormatLayout, CMediumFormat medFormat, bool fPreferred = false);
    QString defaultExtension(const CMediumFormat &mediumFormatRef);
    virtual void retranslateUi() /* override final */;

    QList<CMediumFormat>  m_formats;
    QStringList           m_formatNames;
    QStringList m_formatExtensions;
    QButtonGroup *m_pFormatButtonGroup;
};

class UIDiskVariantGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:


public:

    UIDiskVariantGroupBox(QWidget *pParent = 0);

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    QCheckBox *m_pFixedCheckBox;
    QCheckBox *m_pSplitBox;

};


class UIDiskSizeAndLocationGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

signals:


public:

    UIDiskSizeAndLocationGroupBox(QWidget *pParent = 0);

    QString location() const;
    void setLocation(const QString &strLocation);

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    QLabel *m_pLocationLabel;
    QILineEdit *m_pLocationEditor;
    QIToolButton *m_pLocationOpenButton;
    QLabel *m_pMediumSizeEditorLabel;
    UIMediumSizeEditor *m_pMediumSizeEditor;

};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardDiskEditors_h */
