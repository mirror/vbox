/** @file
 * VBox Qt GUI - UIVMInfoDialog class declaration.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMInfoDialog_h___
#define ___UIVMInfoDialog_h___

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIVMInfoDialog.gen.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"

/* Forward declarations: */
class UIMachineWindow;
class QTimer;

/** QMainWindow based dialog providing user with VM details and statistics. */
class UIVMInfoDialog : public QIWithRetranslateUI<QMainWindow>, public Ui::UIVMInfoDialog
{
    Q_OBJECT;

public:

    /** Instance pointer map. */
    typedef QMap <QString, UIVMInfoDialog*> InfoDlgMap;

    /** VM statistics counter data map. */
    typedef QMap <QString, QString> DataMapType;
    /** VM statistics counter links map. */
    typedef QMap <QString, QStringList> LinksMapType;
    /** VM statistics counter struct. */
    struct CounterElementType { QString type; DataMapType list; };

    /** Factory function to create information-dialog for passed @a pMachineWindow. */
    static void createInformationDlg(UIMachineWindow *pMachineWindow);

protected:

    /** Information dialog constructor. */
    UIVMInfoDialog(UIMachineWindow *pMachineWindow);
    /** Information dialog destructor. */
    ~UIVMInfoDialog();

    /** Translation handler. */
    void retranslateUi();

    /** Common event-handler. */
    virtual bool event (QEvent *aEvent);
    /** Resize event-handler. */
    virtual void resizeEvent (QResizeEvent *aEvent);
    /** Show event-handler. */
    virtual void showEvent (QShowEvent *aEvent);

private slots:

    /** Slot to update general VM details. */
    void updateDetails();
    /** Slot to update runtime VM statistics. */
    void processStatistics();
    /** Slot to handle tab-widget page change. */
    void onPageChanged (int aIndex);

private:

    /** Helper to parse passed VM statistics @a aText. */
    QString parseStatistics (const QString &aText);
    /** Helper to re-acquire whole VM statistics. */
    void refreshStatistics();

    /** Helper to format common VM statistics value. */
    QString formatValue (const QString &aValueName, const QString &aValue, int aMaxSize);
    /** Helper to format VM storage-medium statistics value. */
    QString formatMedium (const QString &aCtrName, LONG aPort, LONG aDevice, const QString &aBelongsTo);
    /** Helper to format VM network-adapter statistics value. */
    QString formatAdapter (ULONG aSlot, const QString &aBelongsTo);

    /** Helper to compose user-oriented article. */
    QString composeArticle (const QString &aBelongsTo, int aSpacesCount = 0);

    /** @name General variables.
     * @{ */
    /** Dialog instance array. */
    static InfoDlgMap  mSelfArray;
    /** Widget to center dialog according. */
    QWidget           *m_pPseudoParentWidget;
    /** Whether dialog was polished. */
    bool               mIsPolished;
    /** @} */

    /** @name Geometry variables.
     * @{ */
    /** Current dialog width. */
    int                mWidth;
    /** Current dialog height. */
    int                mHeight;
    /** Whether dialog maximized. */
    bool               mMax;
    /** @} */

    /** @name VM details/statistics variables.
     * @{ */
    /** Session to acquire VM details/statistics from. */
    CSession           mSession;
    /** VM statistics update timer. */
    QTimer            *mStatTimer;
    /** VM statistics counter names. */
    DataMapType        mNamesMap;
    /** VM statistics counter values. */
    DataMapType        mValuesMap;
    /** VM statistics counter units. */
    DataMapType        mUnitsMap;
    /** VM statistics counter links. */
    LinksMapType       mLinksMap;
    /** @} */
};

#endif /* !___UIVMInfoDialog_h___ */
