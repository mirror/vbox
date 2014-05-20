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

    /** VM statistics counter data map. */
    typedef QMap <QString, QString> DataMapType;
    /** VM statistics counter links map. */
    typedef QMap <QString, QStringList> LinksMapType;
    /** VM statistics counter struct. */
    struct CounterElementType { QString type; DataMapType list; };

    /** Shows (and creates if necessary)
      * information-dialog for passed @a pMachineWindow. */
    static void invoke(UIMachineWindow *pMachineWindow);

protected:

    /** Information dialog constructor. */
    UIVMInfoDialog(UIMachineWindow *pMachineWindow);
    /** Information dialog destructor. */
    ~UIVMInfoDialog();

    /** Translation handler. */
    void retranslateUi();

    /** Common event-handler. */
    virtual bool event(QEvent *pEvent);
    /** Resize event-handler. */
    virtual void resizeEvent(QResizeEvent *pEvent);
    /** Show event-handler. */
    virtual void showEvent(QShowEvent *pEvent);

private slots:

    /** Slot to destroy dialog immediately. */
    void suicide() { delete this; }
    /** Slot to update general VM details. */
    void sltUpdateDetails();
    /** Slot to update runtime VM statistics. */
    void sltProcessStatistics();
    /** Slot to handle tab-widget page change. */
    void sltHandlePageChanged(int iIndex);

private:

    /** General prepare helper. */
    void prepare();
    /** Dialog prepare helper. */
    void prepareThis();
    /** Load settings helper. */
    void loadSettings();

    /** Save settings helper. */
    void saveSettings();
    // /** Dialog cleanup helper. */
    // void cleanupThis() {}
    /** General cleanup helper. */
    void cleanup();

    /** Helper to parse passed VM statistics @a aText. */
    QString parseStatistics(const QString &strText);
    /** Helper to re-acquire whole VM statistics. */
    void refreshStatistics();

    /** Helper to format common VM statistics value. */
    QString formatValue(const QString &strValueName, const QString &strValue, int iMaxSize);
    /** Helper to format VM storage-statistics value. */
    QString formatStorageElement(const QString &strCtrName, LONG iPort, LONG iDevice, const QString &strBelongsTo);
    /** Helper to format VM network-statistics value. */
    QString formatNetworkElement(ULONG uSlot, const QString &strBelongsTo);

    /** Helper to compose user-oriented article. */
    QString composeArticle(const QString &strBelongsTo, int iSpacesCount = 0);

    /** @name General variables.
     * @{ */
    /** Dialog instance pointer. */
    static UIVMInfoDialog *m_spInstance;
    /** Machine-window to center dialog according. */
    UIMachineWindow       *m_pMachineWindow;
    /** Whether dialog was polished. */
    bool                   m_fIsPolished;
    /** @} */

    /** @name Geometry variables.
     * @{ */
    /** Current dialog width. */
    int                m_iWidth;
    /** Current dialog height. */
    int                m_iHeight;
    /** Whether dialog maximized. */
    bool               m_fMax;
    /** @} */

    /** @name VM details/statistics variables.
     * @{ */
    /** Session to acquire VM details/statistics from. */
    CSession           m_session;
    /** VM statistics update timer. */
    QTimer            *m_pTimer;
    /** VM statistics counter names. */
    DataMapType        m_names;
    /** VM statistics counter values. */
    DataMapType        m_values;
    /** VM statistics counter units. */
    DataMapType        m_units;
    /** VM statistics counter links. */
    LinksMapType       m_links;
    /** @} */
};

#endif /* !___UIVMInfoDialog_h___ */
