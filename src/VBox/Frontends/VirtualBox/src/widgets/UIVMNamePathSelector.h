/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMNamePathSelector class declaration.
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

#ifndef ___UIVMNamePathSelector_h___
#define ___UIVMNamePathSelector_h___

/* Qt includes: */
#include <QLineEdit>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QILabel;
class QILineEdit;
class QIToolButton;

class SHARED_LIBRARY_STUFF UIVMNamePathSelector : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigNameChanged(const QString &strName);

public:

    UIVMNamePathSelector(QWidget *pParent = 0);

public slots:

    QString path() const;
    void    setPath(const QString &path);

    QString name() const;
    void    setName(const QString &name);

protected:

    void retranslateUi() /* override */;

private slots:

    void sltOpenPathSelector();

private:

    void         prepareWidgets();
    QString      defaultMachineFolder() const;

    QHBoxLayout *m_pMainLayout;
    QILineEdit  *m_pPath;
    QILineEdit  *m_pName;
    QILabel     *m_pSeparator;
    QIToolButton *m_pFileDialogButton;

};

#endif /* !___UIVMNamePathSelector_h___ */
