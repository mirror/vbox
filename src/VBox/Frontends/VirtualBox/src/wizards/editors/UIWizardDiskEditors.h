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
class QIRichTextLabel;
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

class SHARED_LIBRARY_STUFF UIDiskEditorGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

public:

    UIDiskEditorGroupBox(bool fExpertMode, QWidget *pParent = 0);

    static QString appendExtension(const QString &strName, const QString &strExtension);
    static QString constructMediumFilePath(const QString &strFileName, const QString &strPath);
    static QString defaultExtensionForMediumFormat(const CMediumFormat &mediumFormatRef);

protected:

    bool m_fExpertMode;
};

class SHARED_LIBRARY_STUFF UIDiskFormatsGroupBox : public UIDiskEditorGroupBox
{
    Q_OBJECT;

signals:

    void sigMediumFormatChanged();

public:

    UIDiskFormatsGroupBox(bool fExpertMode, QWidget *pParent = 0);
    CMediumFormat mediumFormat() const;
    void setMediumFormat(const CMediumFormat &mediumFormat);
    const CMediumFormat &VDIMediumFormat() const;
    const QStringList formatExtensions() const;
    static QString defaultExtension(const CMediumFormat &mediumFormatRef);

private:

    void prepare();
    void addFormatButton(QVBoxLayout *pFormatLayout, CMediumFormat medFormat, bool fPreferred = false);

    virtual void retranslateUi() /* override final */;

    QList<CMediumFormat>  m_formats;
    CMediumFormat m_comVDIMediumFormat;
    QStringList           m_formatNames;
    QStringList m_formatExtensions;
    QButtonGroup *m_pFormatButtonGroup;
};

class SHARED_LIBRARY_STUFF UIDiskVariantGroupBox : public UIDiskEditorGroupBox
{
    Q_OBJECT;

signals:

    void sigMediumVariantChanged(qulonglong uVariant);

public:

    UIDiskVariantGroupBox(bool fExpertMode, QWidget *pParent = 0);
    void updateMediumVariantWidgetsAfterFormatChange(const CMediumFormat &mediumFormat, bool fHideDisabled = false);
    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);
    void setWidgetVisibility(CMediumFormat &mediumFormat);
    bool isComplete() const;

    bool isCreateDynamicPossible() const;
    bool isCreateFixedPossible() const;
    bool isCreateSplitPossible() const;

private slots:

    void sltVariantChanged();

private:

    void prepare();
    virtual void retranslateUi() /* override final */;

    QCheckBox *m_pFixedCheckBox;
    QCheckBox *m_pSplitBox;
    bool m_fIsCreateDynamicPossible;
    bool m_fIsCreateFixedPossible;
    bool m_fIsCreateSplitPossible;
};


class SHARED_LIBRARY_STUFF UIMediumSizeAndPathGroupBox : public UIDiskEditorGroupBox
{
    Q_OBJECT;

signals:

    void sigMediumSizeChanged(qulonglong uSize);
    void sigMediumPathChanged(const QString &strPath);
    void sigMediumLocationButtonClicked();

public:

    UIMediumSizeAndPathGroupBox(bool fExpertMode, QWidget *pParent = 0);
    QString mediumPath() const;
    void setMediumPath(const QString &strMediumPath);
    void updateMediumPath(const CMediumFormat &mediumFormat, const QStringList &formatExtensions);
    qulonglong mediumSize() const;
    void setMediumSize(qulonglong uSize);

private:

    void prepare();
    virtual void retranslateUi() /* override final */;
    static QString stripFormatExtension(const QString &strFileName,
                                        const QStringList &formatExtensions);

    QILineEdit *m_pLocationEditor;
    QIToolButton *m_pLocationOpenButton;
    UIMediumSizeEditor *m_pMediumSizeEditor;
    QIRichTextLabel *m_pLocationLabel;
    QIRichTextLabel *m_pSizeLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardDiskEditors_h */
