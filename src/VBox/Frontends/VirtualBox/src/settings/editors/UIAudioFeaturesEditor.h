/* $Id$ */
/** @file
 * VBox Qt GUI - UIAudioFeaturesEditor class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIAudioFeaturesEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIAudioFeaturesEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as audio features editor. */
class SHARED_LIBRARY_STUFF UIAudioFeaturesEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIAudioFeaturesEditor(QWidget *pParent = 0);

    /** Defines whether 'enable output' feature in @a fOn. */
    void setEnableOutput(bool fOn);
    /** Returns 'enable output' feature value. */
    bool outputEnabled() const;

    /** Defines whether 'enable input' feature in @a fOn. */
    void setEnableInput(bool fOn);
    /** Returns 'enable input' feature value. */
    bool inputEnabled() const;

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

    /** @name Values
     * @{ */
        /** Holds the 'enable output' feature value. */
        bool  m_fEnableOutput;
        /** Holds the 'enable input' feature value. */
        bool  m_fEnableInput;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;
        /** Holds the label instance. */
        QLabel      *m_pLabel;
        /** Holds the 'enable output' check-box instance. */
        QCheckBox   *m_pCheckBoxEnableOutput;
        /** Holds the 'enable input' check-box instance. */
        QCheckBox   *m_pCheckBoxEnableInput;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIAudioFeaturesEditor_h */
