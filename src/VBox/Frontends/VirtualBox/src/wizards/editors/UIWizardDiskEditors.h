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
#include <QVector>

/* Local includes: */
#include "QIComboBox.h"
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

/** Base class for disk related group boxes. */
class SHARED_LIBRARY_STUFF UIDiskEditorGroupBox : public QIWithRetranslateUI<QGroupBox>
{
    Q_OBJECT;

public:

    UIDiskEditorGroupBox(bool fExpertMode, QWidget *pParent = 0);

    static QString appendExtension(const QString &strName, const QString &strExtension);
    static QString constructMediumFilePath(const QString &strFileName, const QString &strPath);
    static bool checkFATSizeLimitation(const qulonglong uVariant, const QString &strMediumPath, const qulonglong uSize);
    static QString openFileDialogForDiskFile(const QString &strInitialPath, const CMediumFormat &comMediumFormat,
                                             KDeviceType enmDeviceType, QWidget *pParent);
    /** Attempts to find a file extention for the device type @p enmDeviceType within the extensions
      * returned by CMediumFormat::DescribeFileExtensions(..). */
    static QString defaultExtension(const CMediumFormat &mediumFormatRef, KDeviceType enmDeviceType);

protected:

    bool m_fExpertMode;
};


class SHARED_LIBRARY_STUFF UIDiskVariantGroupBox : public UIDiskEditorGroupBox
{
    Q_OBJECT;

signals:

    void sigMediumVariantChanged(qulonglong uVariant);

public:

    UIDiskVariantGroupBox(bool fExpertMode, QWidget *pParent = 0);
    /** Enable/disable medium variant radio button depending on the capabilities of the medium format. */
    void updateMediumVariantWidgetsAfterFormatChange(const CMediumFormat &mediumFormat);
    qulonglong mediumVariant() const;
    void setMediumVariant(qulonglong uMediumVariant);
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

    UIMediumSizeAndPathGroupBox(bool fExpertMode, QWidget *pParent, qulonglong uMinimumMediumSize);
    QString mediumPath() const;
    void setMediumPath(const QString &strMediumPath);
    /** Checks if the file extension is correct. Fixs it if necessary. */
    void updateMediumPath(const CMediumFormat &mediumFormat, const QStringList &formatExtensions, KDeviceType enmDeviceType);
    qulonglong mediumSize() const;
    void setMediumSize(qulonglong uSize);

    bool isComplete() const;

private:

    void prepare(qulonglong uMinimumMediumSize);
    virtual void retranslateUi() /* override final */;
    static QString stripFormatExtension(const QString &strFileName,
                                        const QStringList &formatExtensions);

    QILineEdit *m_pLocationEditor;
    QIToolButton *m_pLocationOpenButton;
    UIMediumSizeEditor *m_pMediumSizeEditor;
    QIRichTextLabel *m_pLocationLabel;
    QIRichTextLabel *m_pSizeLabel;
};

/** Base class for the widgets used to select virtual medium format. It implements mutual functioanlity
  * like finding name, extension etc for a CMediumFormat and device type. */
class SHARED_LIBRARY_STUFF UIDiskFormatBase
{
public:

    UIDiskFormatBase(KDeviceType enmDeviceType, bool fExpertMode);

    virtual CMediumFormat mediumFormat() const = 0;
    virtual void setMediumFormat(const CMediumFormat &mediumFormat) = 0;

    const CMediumFormat &VDIMediumFormat() const;
    QStringList formatExtensions() const;

protected:

    struct Format
    {
        CMediumFormat m_comFormat;
        QString       m_strExtension;
        bool          m_fPreferred;
        Format(const CMediumFormat &comFormat,
               const QString &strExtension, bool fPreferred)
               : m_comFormat(comFormat)
               , m_strExtension(strExtension)
               , m_fPreferred(fPreferred){}
        Format(){}
    };

    void addFormat(CMediumFormat medFormat, bool fPreferred = false);
    void populateFormats();
    bool isExpertMode() const;
    QVector<Format> m_formatList;

private:

    CMediumFormat m_comVDIMediumFormat;
    KDeviceType m_enmDeviceType;
    bool m_fExpertMode;
};

class SHARED_LIBRARY_STUFF UIDiskFormatsGroupBox : public QIWithRetranslateUI<QWidget>, public UIDiskFormatBase
{
    Q_OBJECT;

signals:

    void sigMediumFormatChanged();

public:

    UIDiskFormatsGroupBox(bool fExpertMode, KDeviceType enmDeviceType, QWidget *pParent = 0);
    virtual CMediumFormat mediumFormat() const /* override final */;
    virtual void setMediumFormat(const CMediumFormat &mediumFormat) /* override final */;

private:

    void prepare();
    void createFormatWidgets();
    virtual void retranslateUi() /* override final */;

    QButtonGroup *m_pFormatButtonGroup;
    QVBoxLayout *m_pMainLayout;
};

class SHARED_LIBRARY_STUFF UIDiskFormatsComboBox : public QIWithRetranslateUI<QIComboBox>, public UIDiskFormatBase
{
    Q_OBJECT;

signals:

    void sigMediumFormatChanged();

public:

    UIDiskFormatsComboBox(bool fExpertMode, KDeviceType enmDeviceType, QWidget *pParent = 0);
    virtual CMediumFormat mediumFormat() const /* override final */;
    virtual void setMediumFormat(const CMediumFormat &mediumFormat) /* override final */;

private:

    void prepare();
    virtual void retranslateUi() /* override final */;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardDiskEditors_h */
