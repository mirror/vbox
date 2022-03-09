/* $Id$ */
/** @file
 * VBox Qt GUI - UIColorThemeEditor class declaration.
 */

/*
 * Copyright (C) 2019-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIColorThemeEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIColorThemeEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QComboBox;
class QLabel;

/** QWidget subclass used as a color theme editor. */
class SHARED_LIBRARY_STUFF UIColorThemeEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a enmValue change. */
    void sigValueChanged(UIColorThemeType enmValue);

public:

    /** Constructs color theme editor passing @a pParent to the base-class. */
    UIColorThemeEditor(QWidget *pParent = 0);

    /** Defines editor @a enmValue. */
    void setValue(UIColorThemeType enmValue);
    /** Returns editor value. */
    UIColorThemeType value() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles current index change. */
    void sltHandleCurrentIndexChanged();

private:

    /** Prepares all. */
    void prepare();
    /** Populates combo. */
    void populateCombo();

    /** Holds whether descriptive label should be created. */
    bool  m_fWithLabel;

    /** Holds the value to be selected. */
    UIColorThemeType  m_enmValue;

    /** Holds the label instance. */
    QLabel     *m_pLabel;
    /** Holds the combo instance. */
    QComboBox  *m_pCombo;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIColorThemeEditor_h */
