/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostnameDomainNameEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIHostnameDomainNameEditor_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIHostnameDomainNameEditor_h
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

class UIHostnameDomainNameEditor : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigHostnameDomainNameChanged(const QString &strHostNameDomain);

public:

    UIHostnameDomainNameEditor(QWidget *pParent = 0);

    QString hostname() const;
    void setHostname(const QString &strHostname);

    QString domainName() const;
    void setDomainName(const QString &strDomain);

    QString hostnameDomainName() const;

    bool isComplete() const;
    void mark();

    int firstColumnWidth() const;
    void setFirstColumnWidth(int iWidth);


protected:

    void retranslateUi();

private slots:

    void sltHostnameChanged();
    void sltDomainChanged();

private:

    void prepare();
    void addLineEdit(int &iRow, QLabel *&pLabel, QILineEdit *&pLineEdit, QGridLayout *pLayout);

    QILineEdit  *m_pHostnameLineEdit;
    QILineEdit  *m_pDomainNameLineEdit;

    QLabel *m_pHostnameLabel;
    QLabel *m_pDomainNameLabel;
    QGridLayout *m_pMainLayout;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIHostnameDomainNameEditor_h */
