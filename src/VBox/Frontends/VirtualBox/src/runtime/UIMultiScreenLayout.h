/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMultiScreenLayout class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIMultiScreenLayout_h__
#define __UIMultiScreenLayout_h__

/* Global includes */
#include <QObject>

class UIMachineLogic;

class QMenu;
class QAction;
template <class Key, class T> class QMap;

class UIMultiScreenLayout : public QObject
{
    Q_OBJECT;

public:
    UIMultiScreenLayout(UIMachineLogic *pMachineLogic);
    ~UIMultiScreenLayout();

    void initialize(QMenu *pMenu);
    void update();
    int hostScreenCount() const;
    int guestScreenCount() const;
    int hostScreenForGuestScreen(int screenId) const;
    quint64 memoryRequirements() const;

signals:
    void screenLayoutChanged();

private slots:

    void sltScreenLayoutChanged(QAction *pAction);

private:

    quint64 memoryRequirements(const QMap<int, int> *pScreenLayout) const;

    /* Private member vars */
    UIMachineLogic *m_pMachineLogic;

    int m_cGuestScreens;
    int m_cHostScreens;

    QMap<int, int> *m_pScreenMap;
};

#endif /* __UIMultiScreenLayout_h__ */

