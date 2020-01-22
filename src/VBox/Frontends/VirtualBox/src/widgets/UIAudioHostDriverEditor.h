/* $Id$ */
/** @file
 * VBox Qt GUI - UIAudioHostDriverEditor class declaration.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UIAudioHostDriverEditor_h
#define FEQT_INCLUDED_SRC_widgets_UIAudioHostDriverEditor_h
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
class QLabel;
class QIComboBox;

/** QWidget subclass used as an audio host driver editor. */
class SHARED_LIBRARY_STUFF UIAudioHostDriverEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a enmValue change. */
    void sigValueChanged(KAudioDriverType enmValue);

public:

    /** Constructs audio host driver editor passing @a pParent to the base-class.
      * @param  fWithLabel  Brings whether we should add label ourselves. */
    UIAudioHostDriverEditor(QWidget *pParent = 0, bool fWithLabel = false);

    /** Defines editor @a enmValue. */
    void setValue(KAudioDriverType enmValue);
    /** Returns editor value. */
    KAudioDriverType value() const;

    /** Returns the vector of supported values. */
    QVector<KAudioDriverType> supportedValues() const { return m_supportedValues; }

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

    /** Holds whether descriptive label should be created. */
    bool  m_fWithLabel;

    /** Holds the value to be selected. */
    KAudioDriverType  m_enmValue;

    /** Holds the vector of supported values. */
    QVector<KAudioDriverType>  m_supportedValues;

    /** Holds the label instance. */
    QLabel     *m_pLabel;
    /** Holds the combo instance. */
    QIComboBox *m_pCombo;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UIAudioHostDriverEditor_h */
