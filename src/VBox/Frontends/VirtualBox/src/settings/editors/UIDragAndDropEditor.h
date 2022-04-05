/* $Id$ */
/** @file
 * VBox Qt GUI - UIDragAndDropEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIDragAndDropEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIDragAndDropEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QComboBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as a drag&drop editor. */
class SHARED_LIBRARY_STUFF UIDragAndDropEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIDragAndDropEditor(QWidget *pParent = 0);

    /** Defines editor @a enmValue. */
    void setValue(KDnDMode enmValue);
    /** Returns editor value. */
    KDnDMode value() const;

    /** Returns the vector of supported values. */
    QVector<KDnDMode> supportedValues() const { return m_supportedValues; }

    /** Returns minimum layout hint. */
    int minimumLabelHorizontalHint() const;
    /** Defines minimum layout @a iIndent. */
    void setMinimumLayoutIndent(int iIndent);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();
    /** Populates combo. */
    void populateCombo();

    /** Holds the value to be selected. */
    KDnDMode  m_enmValue;

    /** Holds the vector of supported values. */
    QVector<KDnDMode>  m_supportedValues;

    /** Holds the main layout instance. */
    QGridLayout *m_pLayout;
    /** Holds the label instance. */
    QLabel      *m_pLabel;
    /** Holds the combo instance. */
    QComboBox   *m_pCombo;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIDragAndDropEditor_h */
