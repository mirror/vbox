/* $Id$ */
/** @file
 * VBox Qt GUI - UIExecutionCapEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIExecutionCapEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIExecutionCapEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QSpinBox;
class QIAdvancedSlider;

/** QWidget subclass used as a execution cap editor. */
class SHARED_LIBRARY_STUFF UIExecutionCapEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value changed. */
    void sigValueChanged();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIExecutionCapEditor(QWidget *pParent = 0);

    /** Returns the medium execution cap. */
    int medExecCap() const;

    /** Defines editor @a iValue. */
    void setValue(int iValue);
    /** Returns editor value. */
    int value() const;

    /** Returns minimum layout hint. */
    int minimumLabelHorizontalHint() const;
    /** Defines minimum layout @a iIndent. */
    void setMinimumLayoutIndent(int iIndent);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles slider value changes. */
    void sltHandleSliderChange();
    /** Handles spin-box value changes. */
    void sltHandleSpinBoxChange();

private:

    /** Prepares all. */
    void prepare();

    /** @name Options
     * @{ */
        /** Holds the minimum execution cap. */
        uint  m_uMinExecCap;
        /** Holds the medium execution cap. */
        uint  m_uMedExecCap;
        /** Holds the maximum execution cap. */
        uint  m_uMaxExecCap;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout      *m_pLayout;
        /** Holds the main label instance. */
        QLabel           *m_pLabelExecCap;
        /** Holds the slider instance. */
        QIAdvancedSlider *m_pSlider;
        /** Holds the spinbox instance. */
        QSpinBox         *m_pSpinBox;
        /** Holds the minimum label instance. */
        QLabel           *m_pLabelExecCapMin;
        /** Holds the maximum label instance. */
        QLabel           *m_pLabelExecCapMax;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIExecutionCapEditor_h */
