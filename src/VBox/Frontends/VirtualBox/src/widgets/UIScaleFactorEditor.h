/* $Id$ */
/** @file
 * VBox Qt GUI - UIScaleFactorEditor class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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

/* Qt includes: */
//# include <QWidget>

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

/** QWidget reimplementation
 * providing GUI with monitor scale factor editing functionality. */
class SHARED_LIBRARY_STUFF UIScaleFactorEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructor, passes @a pParent to the QWidget constructor.*/
    UIScaleFactorEditor(QWidget *pParent);
    void setMonitorCount(int iMonitorCount);
    void setScaleFactors(const QList<double> &scaleFactors);
    const QList<double>& scaleFactors() const;

protected:

    virtual void retranslateUi() /* override */;

private slots:

    void sltScaleSpinBoxValueChanged(int value);
    void sltScaleSliderValueChanged(int value);
    void sltMonitorComboIndexChanged(int index);

private:

    void               prepare();
    void               setScaleFactor(int iMonitorIndex, int iScaleFactor);
    /* Blocks slider's signals before settting the value. */
    void               setSliderValue(int iValue);
    /* Blocks slider's signals before settting the value. */
    void               setSpinBoxValue(int iValue);
    /* Set the spinbox and slider to scale factor of currently selected monitor */
    void               showMonitorScaleFactor();
    QSpinBox          *m_pScaleSpinBox;
    QGridLayout       *m_pMainLayout;
    QComboBox         *m_pMonitorComboBox;
    QIAdvancedSlider  *m_pScaleSlider;
    QLabel            *m_pMaxScaleLabel;
    QLabel            *m_pMinScaleLabel;
    /* Stores the per-monitor scale factors in range [.., 1, ..] */
    QList<double>      m_scaleFactors;
};

#endif /* !___UIScaleFactorEditor_h___ */
