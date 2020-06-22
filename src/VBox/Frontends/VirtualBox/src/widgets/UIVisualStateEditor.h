/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisualStateEditor class declaration.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIVisualStateEditor_h
#define FEQT_INCLUDED_SRC_widgets_UIVisualStateEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QLabel;
class QIComboBox;

/** QWidget subclass used as a visual state editor. */
class SHARED_LIBRARY_STUFF UIVisualStateEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a enmValue change. */
    void sigValueChanged(UIVisualStateType enmValue);

public:

    /** Constructs visual state editor passing @a pParent to the base-class.
      * @param  fWithLabel  Brings whether we should add label ourselves. */
    UIVisualStateEditor(QWidget *pParent = 0, bool fWithLabel = false);

    /** Defines editor @a uMachineId. */
    void setMachineId(const QUuid &uMachineId);

    /** Defines editor @a enmValue. */
    void setValue(UIVisualStateType enmValue);
    /** Returns editor value. */
    UIVisualStateType value() const;

    /** Returns the vector of supported values. */
    QVector<UIVisualStateType> supportedValues() const { return m_supportedValues; }

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles current index change. */
    void sltHandleCurrentIndexChanged();

private:

    /** Prepares all. */
    void prepare();
    /** Populates combo. */
    void populateCombo();

    /** Holds the machine id. */
    QUuid  m_uMachineId;

    /** Holds whether descriptive label should be created. */
    bool  m_fWithLabel;

    /** Holds the value to be selected. */
    UIVisualStateType  m_enmValue;

    /** Holds the vector of supported values. */
    QVector<UIVisualStateType>  m_supportedValues;

    /** Holds the label instance. */
    QLabel     *m_pLabel;
    /** Holds the combo instance. */
    QIComboBox *m_pCombo;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIVisualStateEditor_h */
