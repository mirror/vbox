/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNameDomainEditor class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIHostNameDomainEditor_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIHostNameDomainEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QWidget>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QILineEdit;
class UIPasswordLineEdit;

class UIHostNameDomainEditor : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigHostnameDomainChanged(const QString &strHostNameDomain);

public:

    UIHostNameDomainEditor(QWidget *pParent = 0);

    QString hostname() const;
    void setHostname(const QString &strHostname);

    QString domain() const;
    void setDomain(const QString &strDomain);

    QString hostnameDomain() const;

    bool isComplete() const;

protected:

    void retranslateUi();

private slots:

    void sltHostnameChanged();
    void sltDomainChanged();

private:

    void prepare();
    void addLineEdit(int &iRow, QLabel *&pLabel, QILineEdit *&pLineEdit, QGridLayout *pLayout);

    QILineEdit  *m_pHostnameLineEdit;
    QILineEdit  *m_pDomainLineEdit;

    QLabel *m_pHostnameLabel;
    QLabel *m_pDomainLabel;

};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIHostNameDomainEditor_h */
