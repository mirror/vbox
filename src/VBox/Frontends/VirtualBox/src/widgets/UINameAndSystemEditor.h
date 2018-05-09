/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class declaration.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UINameAndSystemEditor_h___
#define ___UINameAndSystemEditor_h___

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
class QLineEdit;
class QString;
class UIFilePathSelector;
class UIVMNamePathSelector;

/** QWidget subclass providing complex editor for basic VM parameters. */
class SHARED_LIBRARY_STUFF UINameAndSystemEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QString name READ name WRITE setName);
    Q_PROPERTY(CGuestOSType type READ type WRITE setType);

signals:

    /** Notifies listeners about VM name change. */
    void sigNameChanged(const QString &strNewName);

    /** Notifies listeners about VM OS type change. */
    void sigOsTypeChanged();

public:

    /** Constructs VM parameters editor passing @a pParent to the base-class.
      * @param  fChooseFullPath  Brings whether we should propose to choose location. */
    UINameAndSystemEditor(QWidget *pParent, bool fChooseLocation = false);

    /** Returns the VM name. */
    QString name() const;
    /** Returns path string selected by the user. */
    QString path() const;

    /** Defines the VM @a strName. */
    void setName(const QString &strName);

    /** Returns the VM OS type. */
    CGuestOSType type() const;
    /** Defines the VM OS @a enmType. */
    void setType(const CGuestOSType &enmType);

    void setNameFieldValidator(const QString &strValidatorString);

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

    /** Holds the VM OS type. */
    CGuestOSType            m_enmType;
    /** Holds the currently chosen OS type IDs on per-family basis. */
    QMap<QString, QString>  m_currentIds;
    /** Holds whether we should propose to choose a full path. */
    bool                    m_fChooseLocation;
    /** Holds whether host supports hardware virtualization. */
    bool                    m_fSupportsHWVirtEx;
    /** Holds whether host supports long mode. */
    bool                    m_fSupportsLongMode;

    /** Holds the VM OS family label instance. */
    QLabel                 *m_pLabelFamily;
    /** Holds the VM OS type label instance. */
    QLabel                 *m_pLabelType;
    /** Holds the VM OS type icon instance. */
    QLabel                 *m_pIconType;

    QLabel                 *m_pNamePathLabel;
    UIVMNamePathSelector   *m_pNamePathSelector;
    /** Holds the VM OS family combo instance. */
    QComboBox              *m_pComboFamily;
    /** Holds the VM OS type combo instance. */
    QComboBox              *m_pComboType;
};

#endif /* !___UINameAndSystemEditor_h___ */
