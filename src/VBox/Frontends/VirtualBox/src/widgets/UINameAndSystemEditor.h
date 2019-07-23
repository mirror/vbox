/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class declaration.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_UINameAndSystemEditor_h
#define FEQT_INCLUDED_SRC_widgets_UINameAndSystemEditor_h
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
#include "CGuestOSType.h"

/* Forward declarations: */
class QComboBox;
class QLabel;
class QILineEdit;
class QString;
class UIFilePathSelector;

/** QWidget subclass providing complex editor for basic VM parameters. */
class SHARED_LIBRARY_STUFF UINameAndSystemEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QString name READ name WRITE setName);
    Q_PROPERTY(CGuestOSType type READ type WRITE setType);

    /** Simple struct representing CGuestOSType cache. */
    struct UIGuestOSType
    {
        QString typeId;
        QString typeDescription;
        bool is64bit;
    };

signals:

    /** Notifies listeners about VM name change. */
    void sigNameChanged(const QString &strNewName);
    void sigPathChanged(const QString &strName);

    /** Notifies listeners about VM OS type change. */
    void sigOsTypeChanged();

public:

    /** Constructs VM parameters editor passing @a pParent to the base-class.
     * @param  fChooseName  Controls whether we should propose to choose name.
     * @param  fChoosePath  Controls whether we should propose to choose path.
     * @param  fChooseType  Controls whether we should propose to choose type. */
    UINameAndSystemEditor(QWidget *pParent,
                          bool fChooseName = true,
                          bool fChoosePath = false,
                          bool fChooseType = true);

    /** Defines the VM @a strName. */
    void setName(const QString &strName);
    /** Returns the VM name. */
    QString name() const;

    /** Defines the VM @a strPath. */
    void setPath(const QString &strPath);
    /** Returns path string selected by the user. */
    QString path() const;

    /** Defines the VM OS @a strTypeId and @a strFamilyId if passed. */
    void setTypeId(QString strTypeId, QString strFamilyId = QString());
    /** Returns the VM OS type ID. */
    QString typeId() const;
    /** Returns the VM OS family ID. */
    QString familyId() const;

    /** Defines the VM OS @a enmType. */
    void setType(const CGuestOSType &enmType);
    /** Returns the VM OS type. */
    CGuestOSType type() const;

    /** Defines the name-field @a strValidator. */
    void setNameFieldValidator(const QString &strValidator);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles VM OS family @a iIndex change. */
    void sltFamilyChanged(int iIndex);
    /** Handles VM OS type @a iIndex change. */
    void sltTypeChanged(int iIndex);

private:

    /** @name Prepare cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares VM OS family combo. */
        void prepareFamilyCombo();
        /** Prepares connections. */
        void prepareConnections();
    /** @} */

    /** Holds the current family ID list. */
    QStringList  m_familyIDs;

    /** Holds the current type cache. */
    QMap<QString, QList<UIGuestOSType> >  m_types;

    /** Holds the VM OS type ID. */
    QString  m_strTypeId;
    /** Holds the VM OS family ID. */
    QString  m_strFamilyId;

    /** Holds the currently chosen OS type IDs on per-family basis. */
    QMap<QString, QString>  m_currentIds;

    /** Holds whether we should propose to choose a name. */
    bool  m_fChooseName;
    /** Holds whether we should propose to choose a path. */
    bool  m_fChoosePath;
    /** Holds whether we should propose to choose a type. */
    bool  m_fChooseType;
    /** Holds whether host supports hardware virtualization. */
    bool  m_fSupportsHWVirtEx;
    /** Holds whether host supports long mode. */
    bool  m_fSupportsLongMode;

    /** Holds the VM name label instance. */
    QLabel *m_pNameLabel;
    /** Holds the VM path label instance. */
    QLabel *m_pPathLabel;
    /** Holds the VM OS family label instance. */
    QLabel *m_pLabelFamily;
    /** Holds the VM OS type label instance. */
    QLabel *m_pLabelType;
    /** Holds the VM OS type icon instance. */
    QLabel *m_pIconType;

    /** Holds the VM name editor instance. */
    QILineEdit         *m_pNameLineEdit;
    /** Holds the VM path editor instance. */
    UIFilePathSelector *m_pPathSelector;
    /** Holds the VM OS family combo instance. */
    QComboBox          *m_pComboFamily;
    /** Holds the VM OS type combo instance. */
    QComboBox          *m_pComboType;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_UINameAndSystemEditor_h */
