/* $Id$ */
/** @file
 * VBox Qt GUI - UIProxyFeaturesEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIProxyFeaturesEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIProxyFeaturesEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QButtonGroup;
class QLabel;
class QRadioButton;
class QILineEdit;

/** QWidget subclass used as global proxy features editor. */
class SHARED_LIBRARY_STUFF UIProxyFeaturesEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about proxy mode changed. */
    void sigProxyModeChanged();
    /** Notifies listeners about proxy host changed. */
    void sigProxyHostChanged();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIProxyFeaturesEditor(QWidget *pParent = 0);

    /** Defines proxy @a enmMode. */
    void setProxyMode(KProxyMode enmMode);
    /** Returns proxy mode. */
    KProxyMode proxyMode() const;

    /** Defines proxy @a strHost. */
    void setProxyHost(const QString &strHost);
    /** Returns proxy host. */
    QString proxyHost() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles proxy mode change. */
    void sltHandleProxyModeChanged();

private:

    /** Prepares all. */
    void prepare();

    /** @name Values
     * @{ */
        /** Holds the proxy mode. */
        KProxyMode  m_enmProxyMode;
        /** Holds the proxy host. */
        QString     m_strProxyHost;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the button-group instance. */
        QButtonGroup *m_pButtonGroup;
        /** Holds the 'proxy auto' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyAuto;
        /** Holds the 'proxy disabled' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyDisabled;
        /** Holds the 'proxy enabled' radio-button instance. */
        QRadioButton *m_pRadioButtonProxyEnabled;
        /** Holds the settings widget instance. */
        QWidget      *m_pWidgetSettings;
        /** Holds the host label instance. */
        QLabel       *m_pLabelHost;
        /** Holds the host editor instance. */
        QILineEdit   *m_pEditorHost;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIProxyFeaturesEditor_h */
