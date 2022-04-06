/* $Id$ */
/** @file
 * VBox Qt GUI - UIDiskEncryptionSettingsEditor class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIDiskEncryptionSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIDiskEncryptionSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QWidget;

/** QWidget subclass used as a disk encryption settings editor. */
class SHARED_LIBRARY_STUFF UIDiskEncryptionSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notify listeners about status changed. */
    void sigStatusChanged();
    /** Notify listeners about cipher changed. */
    void sigCipherChanged();
    /** Notify listeners about password changed. */
    void sigPasswordChanged();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIDiskEncryptionSettingsEditor(QWidget *pParent = 0);

    /** Defines whether feature is @a fEnabled. */
    void setFeatureEnabled(bool fEnabled);
    /** Returns whether feature is enabled. */
    bool isFeatureEnabled() const;

    /** Defines cipher @a enmType. */
    void setCipherType(const UIDiskEncryptionCipherType &enmType);
    /** Returns cipher type. */
    UIDiskEncryptionCipherType cipherType() const;

    /** Returns password 1. */
    QString password1() const;
    /** Returns password 2. */
    QString password2() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles whether VRDE is @a fEnabled. */
    void sltHandleFeatureToggled(bool fEnabled);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Repopulates combo-box. */
    void repopulateCombo();

    /** @name Values
     * @{ */
        /** Holds whether feature is enabled. */
        bool                        m_fFeatureEnabled;
        /** Holds the cipher type. */
        UIDiskEncryptionCipherType  m_enmCipherType;
        /** Holds the password 1. */
        QString                     m_strPassword1;
        /** Holds the password 2. */
        QString                     m_strPassword2;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the feature check-box instance. */
        QCheckBox *m_pCheckboxFeature;
        /** Holds the settings widget instance. */
        QWidget   *m_pWidgetSettings;
        /** Holds the cipher type label instance. */
        QLabel    *m_pLabelCipher;
        /** Holds the cipher type combo instance. */
        QComboBox *m_pComboCipher;
        /** Holds the enter password label instance. */
        QLabel    *m_pLabelEncryptionPassword;
        /** Holds the enter password editor instance. */
        QLineEdit *m_pEditorEncryptionPassword;
        /** Holds the confirm password label instance. */
        QLabel    *m_pLabelEncryptionPasswordConfirm;
        /** Holds the confirm password editor instance. */
        QLineEdit *m_pEditorEncryptionPasswordConfirm;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIDiskEncryptionSettingsEditor_h */
