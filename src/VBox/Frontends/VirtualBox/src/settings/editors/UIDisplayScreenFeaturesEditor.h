/* $Id$ */
/** @file
 * VBox Qt GUI - UIDisplayScreenFeaturesEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIDisplayScreenFeaturesEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIDisplayScreenFeaturesEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as machine display screen features editor. */
class SHARED_LIBRARY_STUFF UIDisplayScreenFeaturesEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about 'enable 3D acceleration' feature status changed. */
    void sig3DAccelerationFeatureStatusChange();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIDisplayScreenFeaturesEditor(QWidget *pParent = 0);

    /** Defines whether 'enable 3D acceleration' feature in @a fOn. */
    void setEnable3DAcceleration(bool fOn);
    /** Returns 'enable 3D acceleration' feature value. */
    bool isEnabled3DAcceleration() const;

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
        /** Holds the 'enable 3D acceleration' feature value. */
        bool  m_fEnable3DAcceleration;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;
        /** Holds the label instance. */
        QLabel      *m_pLabel;
        /** Holds the 'enable 3D acceleration' check-box instance. */
        QCheckBox   *m_pCheckBoxEnable3DAcceleration;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIDisplayScreenFeaturesEditor_h */
