/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoConfigurationPanel class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationPanel_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIDialogPanel.h"

/* Forward declarations: */
class QComboBox;
class QILabel;
class QILineEdit;
class QIToolButton;
class UIVisoCreator;

class UIVisoConfigurationPanel : public UIDialogPanel
{
    Q_OBJECT;

signals:

    void sigVisoNameChanged(const QString &strVisoName);
    void sigCustomVisoOptionsChanged(const QStringList &customVisoOptions);

public:
    UIVisoConfigurationPanel(UIVisoCreator *pCreator, QWidget *pParent = 0);
    ~UIVisoConfigurationPanel();
    virtual QString panelName() const /* override */;
    void setVisoName(const QString& strVisoName);
    void setVisoCustomOptions(const QStringList& visoCustomOptions);

protected:

    bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    void retranslateUi() /* override */;

private slots:

    void sltHandleVisoNameChanged();
    void sltHandleDeleteCurrentCustomOption();

private:

    void prepareObjects();
    void prepareConnections();
    void addCustomVisoOption();
    void emitCustomVisoOptions();

    /** Holds the parent creator reference. */
    UIVisoCreator *m_pCreator;

    QILabel      *m_pVisoNameLabel;
    QILabel      *m_pCustomOptionsLabel;
    QILineEdit   *m_pVisoNameLineEdit;
    QComboBox    *m_pCustomOptionsComboBox;
    QIToolButton *m_pDeleteButton;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationPanel_h */
