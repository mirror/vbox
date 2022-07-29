/* $Id$ */
/** @file
 * VBox Qt GUI - UIDisplayFeaturesEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIDisplayFeaturesEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIDisplayFeaturesEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as display features editor. */
class SHARED_LIBRARY_STUFF UIDisplayFeaturesEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIDisplayFeaturesEditor(QWidget *pParent = 0);

    /** Defines whether 'activate on mouse hover' feature in @a fOn. */
    void setActivateOnMouseHover(bool fOn);
    /** Returns 'activate on mouse hover' feature value. */
    bool activateOnMouseHover() const;

    /** Defines whether 'disable host screen-saver' feature in @a fOn. */
    void setDisableHostScreenSaver(bool fOn);
    /** Returns 'disable host screen-saver' feature value. */
    bool disableHostScreenSaver() const;

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
        /** Holds the 'activate on mouse hover' feature value. */
        bool  m_fActivateOnMouseHover;
        /** Holds the 'disable host screen-saver' feature value. */
        bool  m_fDisableHostScreenSaver;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;
        /** Holds the label instance. */
        QLabel      *m_pLabel;
        /** Holds the check-box instance. */
        QCheckBox   *m_pCheckBoxActivateOnMouseHover;
        /** Holds the check-box instance. */
        QCheckBox   *m_pCheckBoxDisableHostScreenSaver;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIDisplayFeaturesEditor_h */
