/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxNewHDWzd class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __VBoxNewHDWzd_h__
#define __VBoxNewHDWzd_h__

#include "QIAbstractWizard.h"
#include "VBoxNewHDWzd.gen.h"
#include "COMDefs.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"

class VBoxNewHDWzd : public QIWithRetranslateUI<QIAbstractWizard>,
                     public Ui::VBoxNewHDWzd
{
    Q_OBJECT;

public:

    VBoxNewHDWzd (QWidget *aParent = 0);

    CMedium hardDisk() { return mHD; }
    void setRecommendedFileName (const QString &aName);
    void setRecommendedSize (quint64 aSize);

protected:

    void retranslateUi();

private slots:

    void accept();
    void slSizeValueChanged (int aVal);
    void leSizeTextChanged (const QString &aText);
    void tbNameSelectClicked();
    void revalidate (QIWidgetValidator *aWval);
    void enableNext (const QIWidgetValidator *aWval);
    void onPageShow();
    void showNextPage();

private:

    QString location();
    bool isDynamicStorage();
    void updateSizeToolTip (quint64 aSizeB);
    bool createHardDisk();

    QIWidgetValidator *mWValNameAndSize;
    CMedium            mHD;
    int                mSliderScale;
    quint64            mMaxVDISize;
    quint64            mCurrentSize;
};

#endif // __VBoxNewHDWzd_h__

