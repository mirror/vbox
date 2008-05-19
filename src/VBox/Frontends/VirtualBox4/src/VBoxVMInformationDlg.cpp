/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMInformationDlg class implementation
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

#include <VBoxVMInformationDlg.h>
#include <VBoxGlobal.h>
#include <VBoxConsoleView.h>

/* Qt includes */
#include <QTimer>

VBoxVMInformationDlg::InfoDlgMap VBoxVMInformationDlg::mSelfArray = InfoDlgMap();

void VBoxVMInformationDlg::createInformationDlg (const CSession &aSession,
                                                 VBoxConsoleView *aConsole)
{
    CMachine machine = aSession.GetMachine();
    if (mSelfArray.find (machine.GetName()) == mSelfArray.end())
    {
        /* Creating new information dialog if there is no one existing */
        mSelfArray [machine.GetName()] = new VBoxVMInformationDlg (
            aConsole, aSession, Qt::WType_TopLevel | Qt::WDestructiveClose);
    }

    VBoxVMInformationDlg *info = mSelfArray [machine.GetName()];
    info->show();
    info->raise();
    info->setWindowState (info->windowState() & ~Qt::WindowMinimized);
    info->activateWindow();
}


VBoxVMInformationDlg::VBoxVMInformationDlg (VBoxConsoleView *aConsole,
                                            const CSession &aSession,
                                            Qt::WindowFlags aFlags)
    : QIAbstractDialog (aConsole, aFlags)
    , mIsPolished (false)
    , mConsole (aConsole)
    , mSession (aSession)
    , mStatTimer (new QTimer (this))
{
    /* Apply UI decorations */
    setupUi (this);

    /* Initialize parent defaults */
    initializeDialog();

    /* Setup focus-proxy for pages */
    mPage1->setFocusProxy (mDetailsText);
    mPage2->setFocusProxy (mStatisticText);

    /* Setup mTeSummary browser.
     * Contents propagation of the parent background */
    mDetailsText->viewport()->setAutoFillBackground (false);
    mStatisticText->viewport()->setAutoFillBackground (false);

    /* Setup handlers */
    connect (mInfoStack, SIGNAL (currentChanged (int)),
             this, SLOT (onPageChanged (int)));
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (updateDetails()));
    connect (mConsole, SIGNAL (mediaChanged (VBoxDefs::DiskType)),
             this, SLOT (updateDetails()));
    connect (mConsole, SIGNAL (sharedFoldersChanged()),
             this, SLOT (updateDetails()));
    connect (mStatTimer, SIGNAL (timeout()),
             this, SLOT (processStatistics()));
    connect (mConsole, SIGNAL (resizeHintDone()),
             this, SLOT (processStatistics()));

    /* Loading language constants */
    languageChange();

    /* Details page update */
    updateDetails();

    /* Statistics page update */
    processStatistics();
    mStatTimer->start (5000);

    /* Preload dialog attributes for this vm */
    QString dlgsize =
        mSession.GetMachine().GetExtraData (VBoxDefs::GUI_InfoDlgState);
    if (dlgsize.isNull())
    {
        mWidth = 400;
        mHeight = 450;
        mMax = false;
    }
    else
    {
        QStringList list = dlgsize.split(',');
        mWidth = list [0].toInt(), mHeight = list [1].toInt();
        mMax = list [2] == "max";
    }

    /* Make statistics page the default one */
    mInfoStack->setCurrentIndex (1);
}

VBoxVMInformationDlg::~VBoxVMInformationDlg()
{
    /* Save dialog attributes for this vm */
    QString dlgsize ("%1,%2,%3");
    mSession.GetMachine().SetExtraData (VBoxDefs::GUI_InfoDlgState,
        dlgsize.arg (mWidth).arg (mHeight).arg (isMaximized() ? "max" : "normal"));

    if (!mSession.isNull() && !mSession.GetMachine().isNull())
        mSelfArray.remove (mSession.GetMachine().GetName());
}


void VBoxVMInformationDlg::languageChange()
{
    AssertReturnVoid (!mSession.isNull()); 
 	CMachine machine = mSession.GetMachine(); 
 	AssertReturnVoid (!machine.isNull()); 

    /* Setup a dialog caption */
    setWindowTitle (tr ("%1 - Session Information").arg (machine.GetName()));

    /* Setup a tabwidget page names */
    mInfoStack->setTabText (0, tr ("&Details"));
    mInfoStack->setTabText (1, tr ("&Runtime"));

    /* Clear counter names initially */
    mNamesMap.clear();
    mUnitsMap.clear(); 
    mLinksMap.clear(); 

    /* IDE HD statistics: */ 
    for (int i = 0; i < 2; ++ i) 
        for (int j = 0; j < 2; ++ j) 
        { 
            /* Names */ 
            mNamesMap [QString ("/Devices/ATA%1/Unit%2/*DMA") 
                .arg (i).arg (j)] = tr ("DMA Transfers"); 
            mNamesMap [QString ("/Devices/ATA%1/Unit%2/*PIO") 
                .arg (i).arg (j)] = tr ("PIO Transfers"); 
            mNamesMap [QString ("/Devices/ATA%1/Unit%2/ReadBytes") 
                .arg (i).arg (j)] = tr ("Data Read"); 
            mNamesMap [QString ("/Devices/ATA%1/Unit%2/WrittenBytes") 
                .arg (i).arg (j)] = tr ("Data Written"); 

            /* Units */ 
            mUnitsMap [QString ("/Devices/ATA%1/Unit%2/*DMA") 
                .arg (i).arg (j)] = "[B]"; 
            mUnitsMap [QString ("/Devices/ATA%1/Unit%2/*PIO") 
                .arg (i).arg (j)] = "[B]"; 
            mUnitsMap [QString ("/Devices/ATA%1/Unit%2/ReadBytes") 
                .arg (i).arg (j)] = "B"; 
            mUnitsMap [QString ("/Devices/ATA%1/Unit%2/WrittenBytes") 
                .arg (i).arg (j)] = "B"; 

            /* Belongs to */ 
            mLinksMap [QString ("IDE%1%2").arg (i).arg (j)] = QStringList() 
                << QString ("/Devices/ATA%1/Unit%2/*DMA").arg (i).arg (j) 
                << QString ("/Devices/ATA%1/Unit%2/*PIO").arg (i).arg (j) 
                << QString ("/Devices/ATA%1/Unit%2/ReadBytes").arg (i).arg (j) 
                << QString ("/Devices/ATA%1/Unit%2/WrittenBytes").arg (i).arg (j); 
        } 

    /* SATA HD statistics: */ 
    for (int i = 0; i < 30; ++ i) 
    { 
        /* Names */ 
        mNamesMap [QString ("/Devices/SATA/Port%1/DMA").arg (i)] 
            = tr ("DMA Transfers"); 
        mNamesMap [QString ("/Devices/SATA/Port%1/ReadBytes").arg (i)] 
            = tr ("Data Read"); 
        mNamesMap [QString ("/Devices/SATA/Port%1/WrittenBytes").arg (i)] 
            = tr ("Data Written"); 

        /* Units */ 
        mUnitsMap [QString ("/Devices/SATA/Port%1/DMA").arg (i)] = "[B]"; 
        mUnitsMap [QString ("/Devices/SATA/Port%1/ReadBytes").arg (i)] = "B"; 
        mUnitsMap [QString ("/Devices/SATA/Port%1/WrittenBytes").arg (i)] = "B"; 

        /* Belongs to */ 
        mLinksMap [QString ("SATA%1").arg (i)] = QStringList() 
            << QString ("/Devices/SATA/Port%1/DMA").arg (i) 
            << QString ("/Devices/SATA/Port%1/ReadBytes").arg (i) 
            << QString ("/Devices/SATA/Port%1/WrittenBytes").arg (i); 
    } 

    /* Network Adapters statistics: */ 
    ulong count = vboxGlobal().virtualBox() 
        .GetSystemProperties().GetNetworkAdapterCount(); 
    for (ulong i = 0; i < count; ++ i)
    { 
        CNetworkAdapter na = machine.GetNetworkAdapter (i); 
        KNetworkAdapterType ty = na.GetAdapterType(); 
        const char *name; 

        switch (ty) 
        { 
            case KNetworkAdapterType_I82540EM:
            case KNetworkAdapterType_I82543GC:
                name = "E1k"; 
                break; 
            default: 
                name = "PCNet"; 
                break; 
        } 

        /* Names */ 
        mNamesMap [QString ("/Devices/%1%2/TransmitBytes") 
            .arg (name).arg (i)] = tr ("Data Transmitted"); 
        mNamesMap [QString ("/Devices/%1%2/ReceiveBytes") 
            .arg (name).arg (i)] = tr ("Data Received"); 

        /* Units */ 
        mUnitsMap [QString ("/Devices/%1%2/TransmitBytes") 
            .arg (name).arg (i)] = "B"; 
        mUnitsMap [QString ("/Devices/%1%2/ReceiveBytes") 
            .arg (name).arg (i)] = "B"; 

        /* Belongs to */ 
        mLinksMap [QString ("NA%1").arg (i)] = QStringList() 
            << QString ("/Devices/%1%2/TransmitBytes").arg (name).arg (i) 
            << QString ("/Devices/%1%2/ReceiveBytes").arg (name).arg (i);
    }

    /* Statistics page update. */
    refreshStatistics();
}

bool VBoxVMInformationDlg::event (QEvent *aEvent)
{
    bool result = QIAbstractDialog::event (aEvent);
    switch (aEvent->type())
    {
        case QEvent::WindowStateChange:
        {
            if (mIsPolished)
                mMax = isMaximized();
            else if (mMax == isMaximized())
                mIsPolished = true;
            break;
        }
        default:
            break;
    }
    return result;
}

void VBoxVMInformationDlg::resizeEvent (QResizeEvent *aEvent)
{
    QIAbstractDialog::resizeEvent (aEvent);

    /* Store dialog size for this vm */
    if (mIsPolished && !isMaximized())
    {
        mWidth = width();
        mHeight = height();
    }
}

void VBoxVMInformationDlg::showEvent (QShowEvent *aEvent)
{
    QIAbstractDialog::showEvent (aEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation */

    if (mIsPolished)
        return;

    /* Load window size and state */
    resize (mWidth, mHeight);
    if (mMax)
        QTimer::singleShot (0, this, SLOT (showMaximized()));
    else
        mIsPolished = true;

    VBoxGlobal::centerWidget (this, parentWidget());
}


void VBoxVMInformationDlg::updateDetails()
{
    /* Details page update */
    mDetailsText->setText (
        vboxGlobal().detailsReport (mSession.GetMachine(), false /* isNewVM */,
                                    false /* withLinks */, false /* refresh */));
}

void VBoxVMInformationDlg::processStatistics()
{
    CMachineDebugger dbg = mSession.GetConsole().GetDebugger();
    QString info;

    /* Process selected statistics: */
    for (DataMapType::const_iterator it = mNamesMap.begin();
         it != mNamesMap.end(); ++ it)
    {
        dbg.GetStats (it.key(), true, info);
        mValuesMap [it.key()] = parseStatistics (info);
    }

    /* Statistics page update */
    refreshStatistics();
}

void VBoxVMInformationDlg::onPageChanged (int aIndex)
{
    /* Focusing the browser on shown page */
    mInfoStack->widget (aIndex)->setFocus();
}


QString VBoxVMInformationDlg::parseStatistics (const QString &aText)
{
    /* Filters the statistic counters body */
    QRegExp query ("^.+<Statistics>\n(.+)\n</Statistics>.*$");
    if (query.indexIn (aText) == -1)
        return QString::null;

    QStringList wholeList = query.cap (1).split ("\n");

    ULONG64 summa = 0;
    for (QStringList::Iterator lineIt = wholeList.begin();
         lineIt != wholeList.end(); ++ lineIt)
    {
        QString text = *lineIt;
        text.remove (1, 1);
        text.remove (text.length() - 2, 2);

        /* Parse incoming counter and fill the counter-element values. */
        CounterElementType counter;
        counter.type = text.section (" ", 0, 0);
        text = text.section (" ", 1);
        QStringList list = text.split ("\" ");
        for (QStringList::Iterator it = list.begin(); it != list.end(); ++ it)
        {
            QString pair = *it;
            QRegExp regExp ("^(.+)=\"([^\"]*)\"?$");
            regExp.indexIn (pair);
            counter.list.insert (regExp.cap (1), regExp.cap (2));
        }

        /* Fill the output with the necessary counter's value.
         * Currently we are using "c" field of simple counter only. */
        QString result = counter.list.contains ("c") ? counter.list ["c"] : "0";
        summa += result.toULongLong();
    }

    return QString::number (summa);
}

void VBoxVMInformationDlg::refreshStatistics()
{
    if (mSession.isNull())
        return;

    QString table = "<table border=0 cellspacing=2 cellpadding=0>%1</table>";
    QString hdrRow = "<tr><td align=left><img src='%1'></td><td colspan=3><b>%2</b></td></tr>";
    QString bdyRow = "<tr><td></td><td><nobr>%1</nobr></td><td colspan=2><nobr>%2</nobr></td></tr>";
    QString paragraph = "<tr><td colspan=4></td></tr>";
    QString result;

    CMachine m = mSession.GetMachine();

    /* Screen & VT-X Runtime Parameters */
    {
        CConsole console = mSession.GetConsole();
        ULONG bpp = console.GetDisplay().GetBitsPerPixel();
        QString resolution = QString ("%1x%2")
            .arg (console.GetDisplay().GetWidth())
            .arg (console.GetDisplay().GetHeight());
        if (bpp)
            resolution += QString ("x%1").arg (bpp);
        QString virt = console.GetDebugger().GetHWVirtExEnabled() ?
            VBoxGlobal::tr ("Enabled", "details report (VT-x/AMD-V)") : 
            VBoxGlobal::tr ("Disabled", "details report (VT-x/AMD-V)"); 
        QString addInfo = console.GetGuest().GetAdditionsVersion(); 
        uint addVersion = addInfo.toUInt(); 
        QString addVerisonStr = !addInfo.isNull() ? 
            tr ("Version %1.%2", "guest additions") 
                .arg (RT_HIWORD (addVersion)).arg (RT_LOWORD (addVersion)) : 
            tr ("Not Detected", "guest additions"); 
        QString osType = console.GetGuest().GetOSTypeId(); 
        if (osType.isNull()) 
            osType = tr ("Not Detected", "guest os type");

        result += hdrRow.arg (":/state_running_16px.png").arg (tr ("Runtime Attributes"));
        result += bdyRow.arg (tr ("Screen Resolution")).arg (resolution) +
                  bdyRow.arg (VBoxGlobal::tr ("VT-x/AMD-V", "details report")).arg (virt); 
        result += bdyRow.arg (tr ("Guest Additions")).arg (addVerisonStr);
        result += bdyRow.arg (tr ("Guest OS Type")).arg (osType);
        result += paragraph;
    }

    /* Hard Disk Statistics */
    { 
        QString hdStat; 

        result += hdrRow.arg (":/hd_16px.png").arg (tr ("Hard Disk Statistics")); 

        /* IDE Hard Disk (Primary Master) */ 
        if (!m.GetHardDisk (KStorageBus_IDE, 0, 0).isNull()) 
        { 
            hdStat += formatHardDisk (KStorageBus_IDE, 0, 0, "IDE00"); 
            hdStat += paragraph; 
        } 

        /* IDE Hard Disk (Primary Slave) */ 
        if (!m.GetHardDisk (KStorageBus_IDE, 0, 1).isNull()) 
        { 
            hdStat += formatHardDisk (KStorageBus_IDE, 0, 1, "IDE01"); 
            hdStat += paragraph; 
        } 

        /* IDE Hard Disk (Secondary Slave) */ 
        if (!m.GetHardDisk (KStorageBus_IDE, 1, 1).isNull()) 
        { 
            hdStat += formatHardDisk (KStorageBus_IDE, 1, 1, "IDE11"); 
            hdStat += paragraph; 
        } 

        /* SATA Hard Disks */ 
        for (int i = 0; i < 30; ++ i) 
        { 
            if (!m.GetHardDisk (KStorageBus_SATA, i, 0).isNull()) 
            { 
                hdStat += formatHardDisk (KStorageBus_SATA, i, 0, 
                                          QString ("SATA%1").arg (i)); 
                hdStat += paragraph; 
            } 
        } 

        /* If there are no Hard Disks */ 
        if (hdStat.isNull()) 
        { 
            hdStat = composeArticle (tr ("No Hard Disks"));
            hdStat += paragraph; 
        } 

        result += hdStat; 

        /* CD/DVD-ROM (Secondary Master) */ 
        result += hdrRow.arg (":/cd_16px.png").arg (tr ("CD/DVD-ROM Statistics")); 
        result += formatHardDisk (KStorageBus_IDE, 1, 0, "IDE10"); 
        result += paragraph; 
    } 

    /* Network Adapters Statistics */
    { 
        QString naStat;

        result += hdrRow.arg (":/nw_16px.png") 
            .arg (tr ("Network Adapter Statistics")); 

        /* Network Adapters list */ 
        ulong count = vboxGlobal().virtualBox() 
            .GetSystemProperties().GetNetworkAdapterCount(); 
        for (ulong slot = 0; slot < count; ++ slot) 
        { 
            if (m.GetNetworkAdapter (slot).GetEnabled()) 
            { 
                naStat += formatAdapter (slot, QString ("NA%1").arg (slot)); 
                naStat += paragraph; 
            } 
        } 

        /* If there are no Network Adapters */ 
        if (naStat.isNull()) 
        { 
            naStat = composeArticle (tr ("No Network Adapters"));
            naStat += paragraph; 
        } 

        result += naStat;
    }
    /* Show full composed page & save/restore scroll-bar position */
    int vv = mStatisticText->verticalScrollBar()->value();
    mStatisticText->setText (table.arg (result));
    mStatisticText->verticalScrollBar()->setValue (vv);
}

QString VBoxVMInformationDlg::formatHardDisk (KStorageBus aBus, 
                                              LONG aChannel, 
                                              LONG aDevice, 
                                              const QString &aBelongsTo)
{
    if (mSession.isNull())
        return QString::null;

    CHardDisk hd = mSession.GetMachine().GetHardDisk (aBus, aChannel, aDevice);
    QString header = "<tr><td></td><td colspan=3><nobr><u>%1</u></nobr></td></tr>";
    QString name = vboxGlobal().toFullString (aBus, aChannel, aDevice); 
    QString result = hd.isNull() ? QString::null : header.arg (name); 
    result += composeArticle (aBelongsTo);
    return result;
}

QString VBoxVMInformationDlg::formatAdapter (ULONG aSlot, 
                                             const QString &aBelongsTo)
{
    if (mSession.isNull())
        return QString::null;

    QString header = "<tr><td></td><td colspan=3><nobr><u>%1</u></nobr></td></tr>";
    QString name = VBoxGlobal::tr ("Adapter %1", "details report (network)").arg (aSlot); 
    QString result = header.arg (name); 
    result += composeArticle (aBelongsTo);
    return result;
}

QString VBoxVMInformationDlg::composeArticle (const QString &aBelongsTo) 
{ 
    QString body = "<tr><td></td><td><nobr>%1</nobr></td><td align=right>" 
                   "<nobr>%2%3</nobr></td><td></td></tr>";
    QString result;

    if (mLinksMap.contains (aBelongsTo)) 
    { 
        QStringList keyList = mLinksMap [aBelongsTo]; 
        for (QStringList::Iterator it = keyList.begin(); it != keyList.end(); ++ it) 
        { 
            QString line (body); 
            QString key = *it; 
            if (mNamesMap.contains (key) && 
                mValuesMap.contains (key) && 
                mUnitsMap.contains (key)) 
            { 
                line = line.arg (mNamesMap [key]) 
                           .arg (QString ("%L1") 
                           .arg (mValuesMap [key].toULongLong())); 
                line = mUnitsMap [key].contains (QRegExp ("\\[\\S+\\]")) ? 
                    line.arg (QString ("<img src=:/tpixel.png width=%1 height=1>") 
                              .arg (QApplication::fontMetrics().width ( 
                              QString (" %1").arg (mUnitsMap [key] 
                              .mid (1, mUnitsMap [key].length() - 2))))) : 
                    line.arg (QString (" %1").arg (mUnitsMap [key])); 
                result += line; 
            } 
        } 
    } 
    else 
        result = body.arg (aBelongsTo).arg (QString::null).arg (QString::null);

    return result;
}


/* Old qt3 code for two columns support */
#if 0
void VBoxVMInformationDlg::refreshStatistics()
{
    QString table = "<p><table border=0 cellspacing=0 cellpadding=0 width=100%>%1</table></p>";
    QString hdrRow = "<tr><td align=left><img src='%1'></td><td colspan=4><b>%2</b></td></tr>";
    QString subRow = "<tr><td></td><td colspan=2><nobr><u>%1</u></nobr></td>"
                                  "<td colspan=2><nobr><u>%2</u></nobr></td></tr>";
    QString bdyRow = "<tr><td></td><td><nobr>%1</nobr></td><td width=50%><nobr>%2</nobr></td>"
                                  "<td><nobr>%3</nobr></td><td width=50%><nobr>%4</nobr></td></tr>";
    QString paragraph = "<tr><td colspan=5></td></tr>";
    QString interline = "<tr><td colspan=5><font size=1>&nbsp;</font></td></tr>";
    QString result;

    /* Screen & VT-X Runtime Parameters */
    if (!mSession.isNull())
    {
        CConsole console = mSession.GetConsole();
        ULONG bpp = console.GetDisplay().GetBitsPerPixel();
        QString resolution = QString ("%1x%2")
            .arg (console.GetDisplay().GetWidth())
            .arg (console.GetDisplay().GetHeight());
        if (bpp)
            resolution += QString ("x%1").arg (bpp);
        QString virt = console.GetDebugger().GetHWVirtExEnabled() ?
            tr ("Enabled") : tr ("Disabled");

        result += hdrRow.arg (":/state_running_16px.png").arg (tr ("Runtime Attributes"));
        result += bdyRow.arg (tr ("Screen Resolution")) .arg (resolution)
                        .arg (tr ("Hardware Virtualization")).arg (virt);
        result += paragraph;
    }

    /* Hard Disk Statistics. */
    result += hdrRow.arg (":/hd_16px.png").arg (tr ("Hard Disks Statistics"));

    result += subRow.arg (tr ("Primary Master")).arg (tr ("Primary Slave"));
    result += composeArticle (QString::null, 0, 1, 4, 5);
    result += composeArticle ("B", 2, 3, 6, 7);
    result += interline;

    result += subRow.arg (tr ("Secondary Master")).arg (tr ("Secondary Slave"));
    result += composeArticle (QString::null, 8, 9, 12, 13);
    result += composeArticle ("B", 10, 11, 14, 15);
    result += paragraph;

    /* Network Adapters Statistics. Counters are currently missed. */
    result += hdrRow.arg (":/nw_16px.png").arg (tr ("Network Adapter Statistics"));
    result += subRow.arg (tr ("Adapter 1")).arg (tr ("Adapter 2"));
    result += composeArticle ("B", 16, 17, 18, 19);

    /* Show full composed page. */
    mStatisticText->setText (table.arg (result));
}


QString VBoxVMInformationDlg::composeArticle (const QString &aUnits,
                                              int aStart1, int aFinish1,
                                              int aStart2, int aFinish2)
{
    QString bdyRow = "<tr><td></td><td><nobr>%1</nobr></td><td width=50%><nobr>%2</nobr></td>"
                                  "<td><nobr>%3</nobr></td><td width=50%><nobr>%4</nobr></td></tr>";

    QString result;

    int id1 = aStart1, id2 = aStart2;
    while (id1 <= aFinish1 || id2 <= aFinish2)
    {
        QString line = bdyRow;
        /* Processing first column */
        if (id1 > aFinish1)
        {
            line = line.arg (QString::null).arg (QString::null)
                       .arg (QString::null).arg (QString::null);
        }
        else if (mValuesMap.contains (mNamesMap.keys() [id1]))
        {
            line = line.arg (mNamesMap.values() [id1]);
            ULONG64 value = mValuesMap.values() [id1].toULongLong();
            line = aUnits.isNull() ?
                line.arg (QString ("%L1").arg (value)) :
                line.arg (QString ("%L1 %2").arg (value).arg (aUnits));
        }
        /* Processing second column */
        if (id2 > aFinish2)
        {
            line = line.arg (QString::null).arg (QString::null)
                       .arg (QString::null).arg (QString::null);
        }
        else if (mValuesMap.contains (mNamesMap.keys() [id2]))
        {
            line = line.arg (mNamesMap.values() [id2]);
            ULONG64 value = mValuesMap.values() [id2].toULongLong();
            line = aUnits.isNull() ?
                line.arg (QString ("%L1").arg (value)) :
                line.arg (QString ("%L1 %2").arg (value).arg (aUnits));
        }
        result += line;
        ++ id1; ++ id2;
    }

    return result;
}
#endif

