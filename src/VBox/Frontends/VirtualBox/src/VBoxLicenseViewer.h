/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxLicenseViewer class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef __VBoxLicenseViewer__
#define __VBoxLicenseViewer__

#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QDialog>

class QTextBrowser;
class QPushButton;

/**
 *  This class is used to show a user license under linux.
 */
class VBoxLicenseViewer : public QIWithRetranslateUI<QDialog>
{
    Q_OBJECT;

public:

    VBoxLicenseViewer (const QString &aFilePath);

public slots:

    int exec();

protected:

    void retranslateUi();

private slots:

    void onScrollBarMoving (int aValue);

    void unlockButtons();

private:

    void showEvent (QShowEvent *aEvent);

    bool eventFilter (QObject *aObject, QEvent *aEvent);

    /* Private member vars */
    QString       mFilePath;
    QTextBrowser *mLicenseText;
    QPushButton  *mAgreeButton;
    QPushButton  *mDisagreeButton;
};

#endif /* __VBoxLicenseViewer__ */

