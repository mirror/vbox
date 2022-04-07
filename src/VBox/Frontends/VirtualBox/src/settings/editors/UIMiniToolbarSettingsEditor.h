/* $Id$ */
/** @file
 * VBox Qt GUI - UIMiniToolbarSettingsEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIMiniToolbarSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIMiniToolbarSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as mini-toolbar editor. */
class SHARED_LIBRARY_STUFF UIMiniToolbarSettingsEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIMiniToolbarSettingsEditor(QWidget *pParent = 0);

    /** Defines whether 'show mini-toolbar' feature in @a fOn. */
    void setShowMiniToolbar(bool fOn);
    /** Returns 'show mini-toolbar' feature value. */
    bool showMiniToolbar() const;

    /** Defines whether 'mini-toolbar at top' feature in @a fOn. */
    void setMiniToolbarAtTop(bool fOn);
    /** Returns 'mini-toolbar at top' feature value. */
    bool miniToolbarAtTop() const;

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
        /** Holds the 'show mini-toolbar' feature value. */
        bool  m_fShowMiniToolbar;
        /** Holds the 'mini-toolbar at top' feature value. */
        bool  m_fMiniToolbarAtTop;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;
        /** Holds the label instance. */
        QLabel      *m_pLabel;
        /** Holds the 'show mini-toolbar' check-box instance. */
        QCheckBox   *m_pCheckBoxShowMiniToolBar;
        /** Holds the 'mini-toolbar alignment' check-box instance. */
        QCheckBox   *m_pCheckBoxMiniToolBarAtTop;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIMiniToolbarSettingsEditor_h */
