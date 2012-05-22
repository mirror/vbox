/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINameAndSystemEditor class declaration
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UINameAndSystemEditor_h__
#define __UINameAndSystemEditor_h__

/* Global includes: */
#include <QWidget>

/* Local includes: */
#include "QIWithRetranslateUI.h"
#include "VBoxGlobal.h"

/* Forward declarations: */
class QLabel;
class QLineEdit;
class QComboBox;

/* QWidget reimplementation providing editor for basic VM parameters: */
class UINameAndSystemEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QString name READ name WRITE setName);
    Q_PROPERTY(CGuestOSType type READ type WRITE setType);

signals:

    /* Notifies listeners about VM name change: */
    void sigNameChanged(const QString &strNewName);

    /* Notifies listeners about VM operating system type change: */
    void sigOsTypeChanged();

public:

    /* Constructor: */
    UINameAndSystemEditor(QWidget *pParent);

    /* Name stuff: */
    QLineEdit* nameEditor() const;
    void setName(const QString &strName);
    QString name() const;

    /* Operating system type stuff: */
    void setType(const CGuestOSType &type);
    CGuestOSType type() const;

protected:

    /* Translation stuff: */
    void retranslateUi();

private slots:

    /* Handles OS family change: */
    void sltFamilyChanged(int iIndex);

    /* Handles OS type change: */
    void sltTypeChanged(int iIndex);

private:

    /* Widgets: */
    QLabel *m_pNameLabel;
    QLabel *m_pFamilyLabel;
    QLabel *m_pTypeLabel;
    QLabel *m_pTypeIcon;
    QLineEdit *m_pNameEditor;
    QComboBox *m_pFamilyCombo;
    QComboBox *m_pTypeCombo;

    /* Variables: */
    CGuestOSType m_type;
    QMap<QString, QString> m_currentIds;
    bool m_fSupportsHWVirtEx;
    bool m_fSupportsLongMode;
};

#endif /* __UINameAndSystemEditor_h__ */

