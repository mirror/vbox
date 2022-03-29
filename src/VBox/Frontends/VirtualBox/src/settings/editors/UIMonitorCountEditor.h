/* $Id$ */
/** @file
 * VBox Qt GUI - UIMonitorCountEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIMonitorCountEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIMonitorCountEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QSpinBox;
class QIAdvancedSlider;

/** QWidget subclass used as a monitor count editor. */
class SHARED_LIBRARY_STUFF UIMonitorCountEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value changed. */
    void sigValidChanged();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIMonitorCountEditor(QWidget *pParent = 0);

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

    /** Revalidates and emits validity change signal. */
    void revalidate();

    /** Holds the value to be selected. */
    int  m_iValue;

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout      *m_pLayout;
        /** Holds the main label instance. */
        QLabel           *m_pLabel;
        /** Holds the slider instance. */
        QIAdvancedSlider *m_pSlider;
        /** Holds the spin-box instance. */
        QSpinBox         *m_pSpinBox;
        /** Holds minimum label instance. */
        QLabel           *m_pLabelMin;
        /** Holds maximum label instance. */
        QLabel           *m_pLabelMax;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIMonitorCountEditor_h */
