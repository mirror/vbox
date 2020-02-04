/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkAttachmentEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_widgets_UINetworkAttachmentEditor_h
#define FEQT_INCLUDED_SRC_widgets_UINetworkAttachmentEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QLabel;
class QIComboBox;

/** QWidget subclass used as a network attachment editor. */
class SHARED_LIBRARY_STUFF UINetworkAttachmentEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about value type has changed. */
    void sigValueTypeChanged();
    /** Notifies listeners about value name has changed. */
    void sigValueNameChanged();

    /** Notifies listeners about value has became @a fValid. */
    void sigValidChanged(bool fValid);

public:

    /** Constructs network attachment editor passing @a pParent to the base-class.
      * @param  fWithLabels  Brings whether we should add labels ourselves. */
    UINetworkAttachmentEditor(QWidget *pParent = 0, bool fWithLabels = false);

    /** Returns focus proxy 1. */
    QWidget *focusProxy1() const;
    /** Returns focus proxy 2. */
    QWidget *focusProxy2() const;

    /** Defines value @a enmType. */
    void setValueType(KNetworkAttachmentType enmType);
    /** Returns value type. */
    KNetworkAttachmentType valueType() const;

    /** Defines value @a names for specified @a enmType. */
    void setValueNames(KNetworkAttachmentType enmType, const QStringList &names);
    /** Defines value @a strName for specified @a enmType. */
    void setValueName(KNetworkAttachmentType enmType, const QString &strName);
    /** Returns current name for specified @a enmType. */
    QString valueName(KNetworkAttachmentType enmType) const;

    /** Returns bridged adapter list. */
    static QStringList bridgedAdapters();
    /** Returns internal network list. */
    static QStringList internalNetworks();
    /** Returns host-only interface list. */
    static QStringList hostInterfaces();
    /** Returns generic driver list. */
    static QStringList genericDrivers();
    /** Returns NAT network list. */
    static QStringList natNetworks();
#ifdef VBOX_WITH_CLOUD_NET
    /** Returns cloud network list. */
    static QStringList cloudNetworks();
#endif /* VBOX_WITH_CLOUD_NET */

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles current type change. */
    void sltHandleCurrentTypeChanged();
    /** Handles current name change. */
    void sltHandleCurrentNameChanged();

private:

    /** Prepares all. */
    void prepare();
    /** Populates type combo. */
    void populateTypeCombo();
    /** Populates name combo. */
    void populateNameCombo();

    /** Retranslates name description. */
    void retranslateNameDescription();

    /** Validates editor values. */
    void revalidate();

    /** Returns UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork corresponding to passed KNetworkAttachmentType. */
    static UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork toUiNetworkAdapterEnum(KNetworkAttachmentType comEnum);

    /** Holds the empty item data id. */
    static QString s_strEmptyItemId;

    /** Holds whether descriptive labels should be created. */
    bool  m_fWithLabels;

    /** Holds the attachment type restrictions. */
    UIExtraDataMetaDefs::DetailsElementOptionTypeNetwork  m_enmRestrictedNetworkAttachmentTypes;

    /** Holds the map of possible names. */
    QMap<KNetworkAttachmentType, QStringList>  m_names;
    /** Holds the map of current names. */
    QMap<KNetworkAttachmentType, QString>      m_name;

    /** Holds the requested type. */
    KNetworkAttachmentType  m_enmType;

    /** Holds the type label instance. */
    QLabel     *m_pLabelType;
    /** Holds the type combo instance. */
    QIComboBox *m_pComboType;
    /** Holds the name label instance. */
    QLabel     *m_pLabelName;
    /** Holds the name combo instance. */
    QIComboBox *m_pComboName;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UINetworkAttachmentEditor_h */
