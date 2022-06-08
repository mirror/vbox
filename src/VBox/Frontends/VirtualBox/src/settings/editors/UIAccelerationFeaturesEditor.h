/* $Id$ */
/** @file
 * VBox Qt GUI - UIAccelerationFeaturesEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIAccelerationFeaturesEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIAccelerationFeaturesEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QLabel;

/** QWidget subclass used as acceleration features editor. */
class SHARED_LIBRARY_STUFF UIAccelerationFeaturesEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about virtualization change. */
    void sigChangedVirtualization();
    /** Notifies listeners about nested paging change. */
    void sigChangedNestedPaging();

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIAccelerationFeaturesEditor(QWidget *pParent = 0);

    /** Defines whether 'enable virtualization' feature in @a fOn. */
    void setEnableVirtualization(bool fOn);
    /** Returns 'enable virtualization' feature value. */
    bool isEnabledVirtualization() const;
    /** Defines whether 'enable virtualization' option @a fAvailable. */
    void setEnableVirtualizationAvailable(bool fAvailable);

    /** Defines whether 'enable nested paging' feature in @a fOn. */
    void setEnableNestedPaging(bool fOn);
    /** Returns 'enable nested paging' feature value. */
    bool isEnabledNestedPaging() const;
    /** Defines whether 'enable nested paging' option @a fAvailable. */
    void setEnableNestedPagingAvailable(bool fAvailable);

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
        /** Holds the 'enable virtualization' feature value. */
        bool  m_fEnableVirtualization;
        /** Holds the 'enable nested paging' feature value. */
        bool  m_fEnableNestedPaging;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;
        /** Holds the label instance. */
        QLabel      *m_pLabel;
        /** Holds the 'enable virtualization' check-box instance. */
        QCheckBox   *m_pCheckBoxEnableVirtualization;
        /** Holds the 'enable nested paging' check-box instance. */
        QCheckBox   *m_pCheckBoxEnableNestedPaging;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIAccelerationFeaturesEditor_h */
