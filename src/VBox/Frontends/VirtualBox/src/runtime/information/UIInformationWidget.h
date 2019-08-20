/* $Id$ */
/** @file
 * VBox Qt GUI - UIInformationWidget class declaration.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIInformationWidget_h
#define FEQT_INCLUDED_SRC_runtime_information_UIInformationWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"
#include "CMachine.h"
#include "CConsole.h"

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UITextTable.h"

/* Forward declarations: */
class QTableWidget;
class QVBoxLayout;

/** An abstract base class for showing tabular data in Session Information dialog. */
class UIInformationWidget  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs information-view passing @a pParent to the base-class. */
    UIInformationWidget(QWidget *pParent, const CMachine &machine, const CConsole &console);

protected:

    virtual void retranslateUi() /* override */ = 0;
    void insertTitleRow(const QString &strTitle, const QIcon &icon, const QFontMetrics &fontMetrics);
    void insertInfoRows(const UITextTable &table, const QFontMetrics &fontMetrics,
                        QTextDocument &textDocument, int &iMaxColumn1Length);
    void insertInfoRow(const QString strText1, const QString &strText2,
                       const QFontMetrics &fontMetrics, int &iMaxColumn1Length);
    void resetTable();

    CMachine m_machine;
    CConsole m_console;
    QVBoxLayout *m_pMainLayout;
    QTableWidget *m_pTableWidget;
    const int m_iColumCount;

private:
    void prepareObjects();

    const int m_iRowLeftMargin;
    const int m_iRowTopMargin;
    const int m_iRowRightMargin;
    const int m_iRowBottomMargin;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIInformationWidget_h */
