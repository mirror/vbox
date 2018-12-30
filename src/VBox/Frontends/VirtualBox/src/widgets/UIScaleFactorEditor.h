/* $Id$ */
/** @file
 * VBox Qt GUI - UIScaleFactorEditor class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIScaleFactorEditor_h___
#define ___UIScaleFactorEditor_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QComboBox;
class QGridLayout;
class QLabel;
class QSpinBox;
class QWidget;
class QIAdvancedSlider;

/** QWidget reimplementation providing GUI with monitor scale factor editing functionality.
  * It includes a combo box to select a monitor, a slider, and a spinbox to display/modify values.
  * The first item in the combo box is used to change the scale factor of all monitors. */
class SHARED_LIBRARY_STUFF UIScaleFactorEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Creates scale factor editor passing @a pParent to the base-class. */
    UIScaleFactorEditor(QWidget *pParent);

    /** Defines @a iMonitorCount. */
    void setMonitorCount(int iMonitorCount);
    /** Defines a list of guest-screen @a scaleFactors. */
    void setScaleFactors(const QList<double> &scaleFactors);

    /** Returns either a single global scale factor or a list of scale factor for each monitor. */
    QList<double> scaleFactors() const;

    /** Defines @a dDefaultScaleFactor. */
    void setDefaultScaleFactor(double dDefaultScaleFactor);

    /** Defines minimum width @a iHint for internal spin-box. */
    void setSpinBoxWidthHint(int iHint);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** @name Internal slots handling respective widget's value update.
      * @{ */
        void sltScaleSpinBoxValueChanged(int iValue);
        void sltScaleSliderValueChanged(int iValue);
        void sltMonitorComboIndexChanged(int iIndex);
    /** @} */

private:

    /** Prepares all. */
    void prepare();

    /** Defines whether scale factor is @a fGlobal one. */
    void setIsGlobalScaleFactor(bool fGlobal);
    /** Defines @a iScaleFactor for certain @a iMonitorIndex. */
    void setScaleFactor(int iMonitorIndex, int iScaleFactor);
    /** Defines slider's @a iValue. */
    void setSliderValue(int iValue);
    /** Defines spinbox's @a iValue. */
    void setSpinBoxValue(int iValue);

    /** Sets the spinbox and slider to scale factor of currently selected monitor. */
    void updateValuesAfterMonitorChange();
    /** Sets the min/max values of related widgets wrt. device pixel ratio(s) */
    void configureScaleFactorMinMaxValues();
    /** @name Member widgets.
      * @{ */
        QSpinBox         *m_pScaleSpinBox;
        QGridLayout      *m_pMainLayout;
        QComboBox        *m_pMonitorComboBox;
        QIAdvancedSlider *m_pScaleSlider;
        QLabel           *m_pMaxScaleLabel;
        QLabel           *m_pMinScaleLabel;
    /** @} */

    /** Holds the per-monitor scale factors. The 0th item is for all monitors (global). */
    QList<double>  m_scaleFactors;
    /** Holds the default scale factor. */
    double         m_dDefaultScaleFactor;
};

#endif /* !___UIScaleFactorEditor_h___ */
