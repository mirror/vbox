/* $Id$ */
/** @file
 * VBox Qt GUI - UIFDCreationDialog class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_UIFDCreationDialog_h
#define FEQT_INCLUDED_SRC_medium_UIFDCreationDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QDialog>
#include <QUuid>

/* GUI Includes */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class UIFilePathSelector;

/* A QDialog extension to get necessary setting from the user for floppy disk creation */
class SHARED_LIBRARY_STUFF UIFDCreationDialog : public QIWithRetranslateUI<QDialog>

{
    Q_OBJECT;


public:

    UIFDCreationDialog(QWidget *pParent,
                       const QString &strDefaultFolder,
                       const QString &strMachineName = QString());


    virtual void accept() /* override */;
    /* Return the mediumID */
    QUuid mediumID() const;

protected:

    void retranslateUi();

private slots:


private:
    enum FDSize
    {
        //FDSize_2_88M,
        FDSize_1_44M,
        FDSize_1_2M,
        FDSize_720K,
        FDSize_360K
    };
    void                prepare();
    QString             getDefaultFolder() const;

    UIFilePathSelector *m_pFilePathselector;
    QLabel             *m_pPathLabel;
    QLabel             *m_pSizeLabel;
    QComboBox          *m_pSizeCombo;
    QDialogButtonBox   *m_pButtonBox;
    QCheckBox          *m_pFormatCheckBox;
    QString             m_strDefaultFolder;
    QString             m_strMachineName;
    QUuid               m_uMediumID;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIFDCreationDialog_h */
