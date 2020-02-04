/* $Id$ */
/** @file
 * VBox Qt GUI - UIBaseMemoryEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_widgets_UIBaseMemoryEditor_h
#define FEQT_INCLUDED_SRC_widgets_UIBaseMemoryEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QLabel;
class QSpinBox;
class UIBaseMemorySlider;

/** QWidget subclass used as a base memory editor. */
class SHARED_LIBRARY_STUFF UIBaseMemoryEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value has became @a fValid. */
    void sigValidChanged(bool fValid);

public:

    /** Constructs base-memory editor passing @a pParent to the base-class.
      * @param  fWithLabel  Brings whether we should add label ourselves. */
    UIBaseMemoryEditor(QWidget *pParent = 0, bool fWithLabel = false);

    /** Defines editor @a iValue. */
    void setValue(int iValue);
    /** Returns editor value. */
    int value() const;

    /** Returns the maximum optimal RAM. */
    uint maxRAMOpt() const;
    /** Returns the maximum allowed RAM. */
    uint maxRAMAlw() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

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

    /** Holds whether descriptive label should be created. */
    bool  m_fWithLabel;

    /** Holds the memory label instance. */
    QLabel             *m_pLabelMemory;
    /** Holds the memory slider instance. */
    UIBaseMemorySlider *m_pSlider;
    /** Holds minimum memory label instance. */
    QLabel             *m_pLabelMemoryMin;
    /** Holds maximum memory label instance. */
    QLabel             *m_pLabelMemoryMax;
    /** Holds the memory spin-box instance. */
    QSpinBox           *m_pSpinBox;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIBaseMemoryEditor_h */
