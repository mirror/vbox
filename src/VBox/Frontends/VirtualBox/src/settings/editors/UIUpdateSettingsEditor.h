/* $Id$ */
/** @file
 * VBox Qt GUI - UIUpdateSettingsEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIUpdateDefs.h"

/* Forward declarations: */
class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;

/** QWidget subclass used as a update settings editor. */
class SHARED_LIBRARY_STUFF UIUpdateSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs update settings editor passing @a pParent to the base-class. */
    UIUpdateSettingsEditor(QWidget *pParent = 0);

    /** Defines editor @a guiValue. */
    void setValue(const VBoxUpdateData &guiValue);
    /** Returns editor value. */
    VBoxUpdateData value() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles whether update is @a fEnabled. */
    void sltHandleUpdateToggle(bool fEnabled);
    /** Handles update period change. */
    void sltHandleUpdatePeriodChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Returns period type. */
    VBoxUpdateData::PeriodType periodType() const;
    /** Returns branch type. */
    VBoxUpdateData::BranchType branchType() const;

    /** Holds the value to be set. */
    VBoxUpdateData  m_guiValue;

    /** @name Widgets
     * @{ */
        /** Holds the update check-box instance. */
        QCheckBox    *m_pCheckBox;
        /** Holds the update settings widget instance. */
        QWidget      *m_pWidgetUpdateSettings;
        /** Holds the update period label instance. */
        QLabel       *m_pLabelUpdatePeriod;
        /** Holds the update period combo instance. */
        QComboBox    *m_pComboUpdatePeriod;
        /** Holds the update date label instance. */
        QLabel       *m_pLabelUpdateDate;
        /** Holds the update date field instance. */
        QLabel       *m_pFieldUpdateDate;
        /** Holds the update filter label instance. */
        QLabel       *m_pLabelUpdateFilter;

        /** Holds the radio button group instance. */
        QButtonGroup                                       *m_pRadioButtonGroup;
        /** Holds the radio button map instance. */
        QMap<VBoxUpdateData::BranchType, QAbstractButton*>  m_mapRadioButtons;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h */
