/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxGlobal class declaration
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#ifndef __VBoxGlobal_h__
#define __VBoxGlobal_h__

#include "COMDefs.h"

#include "VBoxGlobalSettings.h"
#include "VBoxMedium.h"

/* Qt includes */
#include <QApplication>
#include <QLayout>
#include <QMenu>
#include <QStyle>
#include <QProcess>
#include <QHash>
#include <QDesktopServices>
#include <QDir>

class QAction;
class QLabel;
class QToolButton;

// VirtualBox callback events
////////////////////////////////////////////////////////////////////////////////

class VBoxMachineStateChangeEvent : public QEvent
{
public:
    VBoxMachineStateChangeEvent (const QUuid &aId, KMachineState aState)
        : QEvent ((QEvent::Type) VBoxDefs::MachineStateChangeEventType)
        , id (aId), state (aState)
        {}

    const QUuid id;
    const KMachineState state;
};

class VBoxMachineDataChangeEvent : public QEvent
{
public:
    VBoxMachineDataChangeEvent (const QUuid &aId)
        : QEvent ((QEvent::Type) VBoxDefs::MachineDataChangeEventType)
        , id (aId)
        {}

    const QUuid id;
};

class VBoxMachineRegisteredEvent : public QEvent
{
public:
    VBoxMachineRegisteredEvent (const QUuid &aId, bool aRegistered)
        : QEvent ((QEvent::Type) VBoxDefs::MachineRegisteredEventType)
        , id (aId), registered (aRegistered)
        {}

    const QUuid id;
    const bool registered;
};

class VBoxSessionStateChangeEvent : public QEvent
{
public:
    VBoxSessionStateChangeEvent (const QUuid &aId, KSessionState aState)
        : QEvent ((QEvent::Type) VBoxDefs::SessionStateChangeEventType)
        , id (aId), state (aState)
        {}

    const QUuid id;
    const KSessionState state;
};

class VBoxSnapshotEvent : public QEvent
{
public:

    enum What { Taken, Discarded, Changed };

    VBoxSnapshotEvent (const QUuid &aMachineId, const QUuid &aSnapshotId,
                       What aWhat)
        : QEvent ((QEvent::Type) VBoxDefs::SnapshotEventType)
        , what (aWhat)
        , machineId (aMachineId), snapshotId (aSnapshotId)
        {}

    const What what;

    const QUuid machineId;
    const QUuid snapshotId;
};

class VBoxCanShowRegDlgEvent : public QEvent
{
public:
    VBoxCanShowRegDlgEvent (bool aCanShow)
        : QEvent ((QEvent::Type) VBoxDefs::CanShowRegDlgEventType)
        , mCanShow (aCanShow)
        {}

    const bool mCanShow;
};

class VBoxCanShowUpdDlgEvent : public QEvent
{
public:
    VBoxCanShowUpdDlgEvent (bool aCanShow)
        : QEvent ((QEvent::Type) VBoxDefs::CanShowUpdDlgEventType)
        , mCanShow (aCanShow)
        {}

    const bool mCanShow;
};

class VBoxChangeGUILanguageEvent : public QEvent
{
public:
    VBoxChangeGUILanguageEvent (QString aLangId)
        : QEvent ((QEvent::Type) VBoxDefs::ChangeGUILanguageEventType)
        , mLangId (aLangId)
        {}

    const QString mLangId;
};

#ifdef VBOX_GUI_WITH_SYSTRAY
class VBoxMainWindowCountChangeEvent : public QEvent
{
public:
    VBoxMainWindowCountChangeEvent (int aCount)
        : QEvent ((QEvent::Type) VBoxDefs::MainWindowCountChangeEventType)
        , mCount (aCount)
        {}

    const int mCount;
};

class VBoxCanShowTrayIconEvent : public QEvent
{
public:
    VBoxCanShowTrayIconEvent (bool aCanShow)
        : QEvent ((QEvent::Type) VBoxDefs::CanShowTrayIconEventType)
        , mCanShow (aCanShow)
        {}

    const bool mCanShow;
};

class VBoxShowTrayIconEvent : public QEvent
{
public:
    VBoxShowTrayIconEvent (bool aShow)
        : QEvent ((QEvent::Type) VBoxDefs::ShowTrayIconEventType)
        , mShow (aShow)
        {}

    const bool mShow;
};

class VBoxChangeTrayIconEvent : public QEvent
{
public:
    VBoxChangeTrayIconEvent (bool aChanged)
        : QEvent ((QEvent::Type) VBoxDefs::TrayIconChangeEventType)
        , mChanged (aChanged)
        {}

    const bool mChanged;
};
#endif

class VBoxChangeDockIconUpdateEvent : public QEvent
{
public:
    VBoxChangeDockIconUpdateEvent (bool aChanged)
        : QEvent ((QEvent::Type) VBoxDefs::ChangeDockIconUpdateEventType)
        , mChanged (aChanged)
        {}

    const bool mChanged;
};

class Process : public QProcess
{
    Q_OBJECT;

public:

    static QByteArray singleShot (const QString &aProcessName,
                                  int aTimeout = 5000
                                  /* wait for data maximum 5 seconds */)
    {
        /* Why is it really needed is because of Qt4.3 bug with QProcess.
         * This bug is about QProcess sometimes (~70%) do not receive
         * notification about process was finished, so this makes
         * 'bool QProcess::waitForFinished (int)' block the GUI thread and
         * never dismissed with 'true' result even if process was really
         * started&finished. So we just waiting for some information
         * on process output and destroy the process with force. Due to
         * QProcess::~QProcess() has the same 'waitForFinished (int)' blocker
         * we have to change process state to QProcess::NotRunning. */

        QByteArray result;
        Process process;
        process.start (aProcessName);
        bool firstShotReady = process.waitForReadyRead (aTimeout);
        if (firstShotReady)
            result = process.readAllStandardOutput();
        process.setProcessState (QProcess::NotRunning);
        return result;
    }

protected:

    Process (QWidget *aParent = 0) : QProcess (aParent) {}
};

// VBoxGlobal class
////////////////////////////////////////////////////////////////////////////////

class VBoxSelectorWnd;
class VBoxConsoleWnd;
class VBoxRegistrationDlg;
class VBoxUpdateDlg;

class VBoxGlobal : public QObject
{
    Q_OBJECT

public:

    typedef QHash <ulong, QString> QULongStringHash;
    typedef QHash <long, QString> QLongStringHash;

    static VBoxGlobal &instance();

    bool isValid() { return mValid; }

    QString versionString() { return mVerString; }

    CVirtualBox virtualBox() const { return mVBox; }

    const VBoxGlobalSettings &settings() const { return gset; }
    bool setSettings (const VBoxGlobalSettings &gs);

    VBoxSelectorWnd &selectorWnd();
    VBoxConsoleWnd &consoleWnd();

    /* main window handle storage */
    void setMainWindow (QWidget *aMainWindow) { mMainWindow = aMainWindow; }
    QWidget *mainWindow() const { return mMainWindow; }

    bool isVMConsoleProcess() const { return !vmUuid.isNull(); }
#ifdef VBOX_GUI_WITH_SYSTRAY
    bool isTrayMenu() const;
    void setTrayMenu(bool aIsTrayMenu);
    void trayIconShowSelector();
    bool trayIconInstall();
#endif
    QUuid managedVMUuid() const { return vmUuid; }

    VBoxDefs::RenderMode vmRenderMode() const { return vm_render_mode; }
    const char *vmRenderModeStr() const { return vm_render_mode_str; }

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool isDebuggerEnabled() const { return mDbgEnabled; }
    bool isDebuggerAutoShowEnabled() const { return mDbgAutoShow; }
    RTLDRMOD getDebuggerModule() const { return mhVBoxDbg; }
#else
    bool isDebuggerAutoShowEnabled() const { return false; }
#endif

    /* VBox enum to/from string/icon/color convertors */

    QList <CGuestOSType> vmGuestOSFamilyList() const;
    QList <CGuestOSType> vmGuestOSTypeList (const QString &aFamilyId) const;
    QPixmap vmGuestOSTypeIcon (const QString &aTypeId) const;
    CGuestOSType vmGuestOSType (const QString &aTypeId,
                                const QString &aFamilyId = QString::null) const;
    QString vmGuestOSTypeDescription (const QString &aTypeId) const;

    QPixmap toIcon (KMachineState s) const
    {
        QPixmap *pm = mVMStateIcons.value (s);
        AssertMsg (pm, ("Icon for VM state %d must be defined", s));
        return pm ? *pm : QPixmap();
    }

    const QColor &toColor (KMachineState s) const
    {
        static const QColor none;
        AssertMsg (mVMStateColors.value (s), ("No color for %d", s));
        return mVMStateColors.value (s) ? *mVMStateColors.value (s) : none;
    }

    QString toString (KMachineState s) const
    {
        AssertMsg (!mMachineStates.value (s).isNull(), ("No text for %d", s));
        return mMachineStates.value (s);
    }

    QString toString (KSessionState s) const
    {
        AssertMsg (!mSessionStates.value (s).isNull(), ("No text for %d", s));
        return mSessionStates.value (s);
    }

    /**
     * Returns a string representation of the given KStorageBus enum value.
     * Complementary to #toStorageBusType (const QString &) const.
     */
    QString toString (KStorageBus aBus) const
    {
        AssertMsg (!mStorageBuses.value (aBus).isNull(), ("No text for %d", aBus));
        return mStorageBuses [aBus];
    }

    /**
     * Returns a KStorageBus enum value corresponding to the given string
     * representation. Complementary to #toString (KStorageBus) const.
     */
    KStorageBus toStorageBusType (const QString &aBus) const
    {
        QULongStringHash::const_iterator it =
            qFind (mStorageBuses.begin(), mStorageBuses.end(), aBus);
        AssertMsg (it != mStorageBuses.end(), ("No value for {%s}",
                                               aBus.toLatin1().constData()));
        return KStorageBus (it.key());
    }

    QString toString (KStorageBus aBus, LONG aChannel) const;
    LONG toStorageChannel (KStorageBus aBus, const QString &aChannel) const;

    QString toString (KStorageBus aBus, LONG aChannel, LONG aDevice) const;
    LONG toStorageDevice (KStorageBus aBus, LONG aChannel, const QString &aDevice) const;

    QString toFullString (KStorageBus aBus, LONG aChannel, LONG aDevice) const;

    QString toString (KHardDiskType t) const
    {
        AssertMsg (!mDiskTypes.value (t).isNull(), ("No text for %d", t));
        return mDiskTypes.value (t);
    }

    /**
     * Similar to toString (KHardDiskType), but returns 'Differencing' for
     * normal hard disks that have a parent.
     */
    QString hardDiskTypeString (const CHardDisk &aHD) const
    {
        if (!aHD.GetParent().isNull())
        {
            Assert (aHD.GetType() == KHardDiskType_Normal);
            return mDiskTypes_Differencing;
        }
        return toString (aHD.GetType());
    }

    QString toString (KVRDPAuthType t) const
    {
        AssertMsg (!mVRDPAuthTypes.value (t).isNull(), ("No text for %d", t));
        return mVRDPAuthTypes.value (t);
    }

    QString toString (KPortMode t) const
    {
        AssertMsg (!mPortModeTypes.value (t).isNull(), ("No text for %d", t));
        return mPortModeTypes.value (t);
    }

    QString toString (KUSBDeviceFilterAction t) const
    {
        AssertMsg (!mUSBFilterActionTypes.value (t).isNull(), ("No text for %d", t));
        return mUSBFilterActionTypes.value (t);
    }

    QString toString (KClipboardMode t) const
    {
        AssertMsg (!mClipboardTypes.value (t).isNull(), ("No text for %d", t));
        return mClipboardTypes.value (t);
    }

    KClipboardMode toClipboardModeType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mClipboardTypes.begin(), mClipboardTypes.end(), s);
        AssertMsg (it != mClipboardTypes.end(), ("No value for {%s}",
                                                 s.toLatin1().constData()));
        return KClipboardMode (it.key());
    }

    QString toString (KStorageControllerType t) const
    {
        AssertMsg (!mStorageControllerTypes.value (t).isNull(), ("No text for %d", t));
        return mStorageControllerTypes.value (t);
    }

    KStorageControllerType toIDEControllerType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mStorageControllerTypes.begin(), mStorageControllerTypes.end(), s);
        AssertMsg (it != mStorageControllerTypes.end(), ("No value for {%s}",
                                                         s.toLatin1().constData()));
        return KStorageControllerType (it.key());
    }

    KVRDPAuthType toVRDPAuthType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mVRDPAuthTypes.begin(), mVRDPAuthTypes.end(), s);
        AssertMsg (it != mVRDPAuthTypes.end(), ("No value for {%s}",
                                                s.toLatin1().constData()));
        return KVRDPAuthType (it.key());
    }

    KPortMode toPortMode (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mPortModeTypes.begin(), mPortModeTypes.end(), s);
        AssertMsg (it != mPortModeTypes.end(), ("No value for {%s}",
                                                s.toLatin1().constData()));
        return KPortMode (it.key());
    }

    KUSBDeviceFilterAction toUSBDevFilterAction (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mUSBFilterActionTypes.begin(), mUSBFilterActionTypes.end(), s);
        AssertMsg (it != mUSBFilterActionTypes.end(), ("No value for {%s}",
                                                       s.toLatin1().constData()));
        return KUSBDeviceFilterAction (it.key());
    }

    QString toString (KDeviceType t) const
    {
        AssertMsg (!mDeviceTypes.value (t).isNull(), ("No text for %d", t));
        return mDeviceTypes.value (t);
    }

    KDeviceType toDeviceType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mDeviceTypes.begin(), mDeviceTypes.end(), s);
        AssertMsg (it != mDeviceTypes.end(), ("No value for {%s}",
                                              s.toLatin1().constData()));
        return KDeviceType (it.key());
    }

    QStringList deviceTypeStrings() const;

    QString toString (KAudioDriverType t) const
    {
        AssertMsg (!mAudioDriverTypes.value (t).isNull(), ("No text for %d", t));
        return mAudioDriverTypes.value (t);
    }

    KAudioDriverType toAudioDriverType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mAudioDriverTypes.begin(), mAudioDriverTypes.end(), s);
        AssertMsg (it != mAudioDriverTypes.end(), ("No value for {%s}",
                                                   s.toLatin1().constData()));
        return KAudioDriverType (it.key());
    }

    QString toString (KAudioControllerType t) const
    {
        AssertMsg (!mAudioControllerTypes.value (t).isNull(), ("No text for %d", t));
        return mAudioControllerTypes.value (t);
    }

    KAudioControllerType toAudioControllerType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mAudioControllerTypes.begin(), mAudioControllerTypes.end(), s);
        AssertMsg (it != mAudioControllerTypes.end(), ("No value for {%s}",
                                                       s.toLatin1().constData()));
        return KAudioControllerType (it.key());
    }

    QString toString (KNetworkAdapterType t) const
    {
        AssertMsg (!mNetworkAdapterTypes.value (t).isNull(), ("No text for %d", t));
        return mNetworkAdapterTypes.value (t);
    }

    KNetworkAdapterType toNetworkAdapterType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mNetworkAdapterTypes.begin(), mNetworkAdapterTypes.end(), s);
        AssertMsg (it != mNetworkAdapterTypes.end(), ("No value for {%s}",
                                                      s.toLatin1().constData()));
        return KNetworkAdapterType (it.key());
    }

    QString toString (KNetworkAttachmentType t) const
    {
        AssertMsg (!mNetworkAttachmentTypes.value (t).isNull(), ("No text for %d", t));
        return mNetworkAttachmentTypes.value (t);
    }

    KNetworkAttachmentType toNetworkAttachmentType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mNetworkAttachmentTypes.begin(), mNetworkAttachmentTypes.end(), s);
        AssertMsg (it != mNetworkAttachmentTypes.end(), ("No value for {%s}",
                                                         s.toLatin1().constData()));
        return KNetworkAttachmentType (it.key());
    }

    QString toString (KUSBDeviceState aState) const
    {
        AssertMsg (!mUSBDeviceStates.value (aState).isNull(), ("No text for %d", aState));
        return mUSBDeviceStates.value (aState);
    }

    QStringList COMPortNames() const;
    QString toCOMPortName (ulong aIRQ, ulong aIOBase) const;
    bool toCOMPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    QStringList LPTPortNames() const;
    QString toLPTPortName (ulong aIRQ, ulong aIOBase) const;
    bool toLPTPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    QPixmap snapshotIcon (bool online) const
    {
        return online ? mOnlineSnapshotIcon : mOfflineSnapshotIcon;
    }

    QPixmap warningIcon() const { return mWarningIcon; }
    QPixmap errorIcon() const { return mErrorIcon; }

    /* details generators */

    QString details (const CHardDisk &aHD, bool aPredictDiff);

    QString details (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDeviceFilter &aFilter) const;

    QString detailsReport (const CMachine &aMachine, bool aIsNewVM,
                           bool aWithLinks);

    QString platformInfo();

    /* VirtualBox helpers */

#if defined(Q_WS_X11) && !defined(VBOX_OSE)
    double findLicenseFile (const QStringList &aFilesList, QRegExp aPattern, QString &aLicenseFile) const;
    bool showVirtualBoxLicense();
#endif

    bool checkForAutoConvertedSettings (bool aAfterRefresh = false);

    void checkForAutoConvertedSettingsAfterRefresh()
    { checkForAutoConvertedSettings (true); }

    CSession openSession (const QUuid &aId, bool aExisting = false);

    /** Shortcut to openSession (aId, true). */
    CSession openExistingSession (const QUuid &aId) { return openSession (aId, true); }

    bool startMachine (const QUuid &id);

    void startEnumeratingMedia();

    /**
     * Returns a list of all currently registered media. This list is used to
     * globally track the accessiblity state of all media on a dedicated thread.
     *
     * Note that the media list is initially empty (i.e. before the enumeration
     * process is started for the first time using #startEnumeratingMedia()).
     * See #startEnumeratingMedia() for more information about how meida are
     * sorted in the returned list.
     */
    const VBoxMediaList &currentMediaList() const { return mMediaList; }

    /** Returns true if the media enumeration is in progress. */
    bool isMediaEnumerationStarted() const { return mMediaEnumThread != NULL; }

    void addMedium (const VBoxMedium &);
    void updateMedium (const VBoxMedium &);
    void removeMedium (VBoxDefs::MediaType, const QUuid &);

    bool findMedium (const CMedium &, VBoxMedium &) const;

    /** Compact version of #findMediumTo(). Asserts if not found. */
    VBoxMedium getMedium (const CMedium &aObj) const
    {
        VBoxMedium medium;
        if (!findMedium (aObj, medium))
            AssertFailed();
        return medium;
    }

    /* Returns the number of current running Fe/Qt4 main windows. */
    int mainWindowCount();

    /* various helpers */

    QString languageName() const;
    QString languageCountry() const;
    QString languageNameEnglish() const;
    QString languageCountryEnglish() const;
    QString languageTranslators() const;

    void retranslateUi();

    /** @internal made public for internal purposes */
    void cleanup();

    /* public static stuff */

    static bool isDOSType (const QString &aOSTypeId);

    static void adoptLabelPixmap (QLabel *);

    static QString languageId();
    static void loadLanguage (const QString &aLangId = QString::null);
    QString helpFile() const;

    static QIcon iconSet (const char *aNormal,
                          const char *aDisabled = NULL,
                          const char *aActive = NULL);
    static QIcon iconSetOnOff (const char *aNormal, const char *aNormalOff,
                               const char *aDisabled = NULL,
                               const char *aDisabledOff = NULL,
                               const char *aActive = NULL,
                               const char *aActiveOff = NULL);
    static QIcon iconSetFull (const QSize &aNormalSize, const QSize &aSmallSize,
                              const char *aNormal, const char *aSmallNormal,
                              const char *aDisabled = NULL,
                              const char *aSmallDisabled = NULL,
                              const char *aActive = NULL,
                              const char *aSmallActive = NULL);

    static QIcon standardIcon (QStyle::StandardPixmap aStandard, QWidget *aWidget = NULL);

    static void setTextLabel (QToolButton *aToolButton, const QString &aTextLabel);

    static QRect normalizeGeometry (const QRect &aRectangle, const QRegion &aBoundRegion,
                                    bool aCanResize = true);
    static QRect getNormalized (const QRect &aRectangle, const QRegion &aBoundRegion,
                                bool aCanResize = true);
    static QRegion flip (const QRegion &aRegion);

    static void centerWidget (QWidget *aWidget, QWidget *aRelative,
                              bool aCanResize = true);

    static QChar decimalSep();
    static QString sizeRegexp();

    static quint64 parseSize (const QString &);
    static QString formatSize (quint64 aSize, uint aDecimal = 2,
                               VBoxDefs::FormatSize aMode = VBoxDefs::FormatSize_Round);

    static quint64 requiredVideoMemory (CMachine *aMachine = 0);

    static QString locationForHTML (const QString &aFileName);

    static QString highlight (const QString &aStr, bool aToolTip = false);

    static QString emphasize (const QString &aStr);

    static QString systemLanguageId();

    static QString getExistingDirectory (const QString &aDir, QWidget *aParent,
                                         const QString &aCaption = QString::null,
                                         bool aDirOnly = TRUE,
                                         bool resolveSymlinks = TRUE);

    static QString getSaveFileName (const QString &aStartWith, const QString &aFilters, QWidget *aParent,
                                    const QString &aCaption, QString *aSelectedFilter = NULL,
                                    bool aResolveSymLinks = true);

    static QString getOpenFileName (const QString &aStartWith, const QString &aFilters, QWidget *aParent,
                                    const QString &aCaption, QString *aSelectedFilter = NULL,
                                    bool aResolveSymLinks = true);

    static QStringList getOpenFileNames (const QString &aStartWith, const QString &aFilters, QWidget *aParent,
                                         const QString &aCaption, QString *aSelectedFilter = NULL,
                                         bool aResolveSymLinks = true,
                                         bool aSingleFile = false);

    static QString getFirstExistingDir (const QString &);

    static bool activateWindow (WId aWId, bool aSwitchDesktop = true);

    static QString removeAccelMark (const QString &aText);

    static QString insertKeyToActionText (const QString &aText, const QString &aKey);
    static QString extractKeyFromActionText (const QString &aText);

    static QPixmap joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2);

    static QWidget *findWidget (QWidget *aParent, const char *aName,
                                const char *aClassName = NULL,
                                bool aRecursive = false);

    static QList <QPair <QString, QString> > HDDBackends();

    /* Qt 4.2.0 support function */
    static inline void setLayoutMargin (QLayout *aLayout, int aMargin)
    {
#if QT_VERSION < 0x040300
        /* Deprecated since > 4.2 */
        aLayout->setMargin (aMargin);
#else
        /* New since > 4.2 */
        aLayout->setContentsMargins (aMargin, aMargin, aMargin, aMargin);
#endif
    }

    static inline QString documentsPath()
    {
        QString path;
#if QT_VERSION < 0x040400
        path = QDir::homePath();
#else
        path = QDesktopServices::storageLocation (QDesktopServices::DocumentsLocation);
#endif
        return QDir::cleanPath (path);
    }

signals:

    /**
     * Emitted at the beginning of the enumeration process started by
     * #startEnumeratingMedia().
     */
    void mediumEnumStarted();

    /**
     * Emitted when a new medium item from the list has updated its
     * accessibility state.
     */
    void mediumEnumerated (const VBoxMedium &aMedum);

    /**
     * Emitted at the end of the enumeration process started by
     * #startEnumeratingMedia(). The @a aList argument is passed for
     * convenience, it is exactly the same as returned by #currentMediaList().
     */
    void mediumEnumFinished (const VBoxMediaList &aList);

    /** Emitted when a new media is added using #addMedia(). */
    void mediumAdded (const VBoxMedium &);

    /** Emitted when the media is updated using #updateMedia(). */
    void mediumUpdated (const VBoxMedium &);

    /** Emitted when the media is removed using #removeMedia(). */
    void mediumRemoved (VBoxDefs::MediaType, const QUuid &);

    /* signals emitted when the VirtualBox callback is called by the server
     * (note that currently these signals are emitted only when the application
     * is the in the VM selector mode) */

    void machineStateChanged (const VBoxMachineStateChangeEvent &e);
    void machineDataChanged (const VBoxMachineDataChangeEvent &e);
    void machineRegistered (const VBoxMachineRegisteredEvent &e);
    void sessionStateChanged (const VBoxSessionStateChangeEvent &e);
    void snapshotChanged (const VBoxSnapshotEvent &e);
#ifdef VBOX_GUI_WITH_SYSTRAY
    void mainWindowCountChanged (const VBoxMainWindowCountChangeEvent &e);
    void trayIconCanShow (const VBoxCanShowTrayIconEvent &e);
    void trayIconShow (const VBoxShowTrayIconEvent &e);
    void trayIconChanged (const VBoxChangeTrayIconEvent &e);
#endif
    void dockIconUpdateChanged (const VBoxChangeDockIconUpdateEvent &e);

    void canShowRegDlg (bool aCanShow);
    void canShowUpdDlg (bool aCanShow);

public slots:

    bool openURL (const QString &aURL);

    void showRegistrationDialog (bool aForce = true);
    void showUpdateDialog (bool aForce = true);
    void perDayNewVersionNotifier();

protected:

    bool event (QEvent *e);
    bool eventFilter (QObject *, QEvent *);

private:

    VBoxGlobal();
    ~VBoxGlobal();

    void init();

    bool mValid;

    CVirtualBox mVBox;

    VBoxGlobalSettings gset;

    VBoxSelectorWnd *mSelectorWnd;
    VBoxConsoleWnd *mConsoleWnd;
    QWidget* mMainWindow;

#ifdef VBOX_WITH_REGISTRATION
    VBoxRegistrationDlg *mRegDlg;
#endif
    VBoxUpdateDlg *mUpdDlg;

    QUuid vmUuid;

#ifdef VBOX_GUI_WITH_SYSTRAY
    bool mIsTrayMenu : 1; /*< Tray icon active/desired? */
    bool mIncreasedWindowCounter : 1;
#endif

    QThread *mMediaEnumThread;
    VBoxMediaList mMediaList;

    VBoxDefs::RenderMode vm_render_mode;
    const char * vm_render_mode_str;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Whether the debugger should be accessible or not.
     * Use --dbg, the env.var. VBOX_GUI_DBG_ENABLED, --debug or the env.var.
     * VBOX_GUI_DBG_AUTO_SHOW to enable. */
    bool mDbgEnabled;
    /** Whether to show the debugger automatically with the console.
     * Use --debug or the env.var. VBOX_GUI_DBG_AUTO_SHOW to enable. */
    bool mDbgAutoShow;
    /** VBoxDbg module handle. */
    RTLDRMOD mhVBoxDbg;
#endif

#if defined (Q_WS_WIN32)
    DWORD dwHTMLHelpCookie;
#endif

    CVirtualBoxCallback callback;

    QString mVerString;

    QList <QString> mFamilyIDs;
    QList <QList <CGuestOSType> > mTypes;
    QHash <QString, QPixmap *> mOsTypeIcons;

    QHash <ulong, QPixmap *> mVMStateIcons;
    QHash <ulong, QColor *> mVMStateColors;

    QPixmap mOfflineSnapshotIcon, mOnlineSnapshotIcon;

    QULongStringHash mMachineStates;
    QULongStringHash mSessionStates;
    QULongStringHash mDeviceTypes;

    QULongStringHash mStorageBuses;
    QLongStringHash mStorageBusChannels;
    QLongStringHash mStorageBusDevices;

    QULongStringHash mDiskTypes;
    QString mDiskTypes_Differencing;

    QULongStringHash mVRDPAuthTypes;
    QULongStringHash mPortModeTypes;
    QULongStringHash mUSBFilterActionTypes;
    QULongStringHash mAudioDriverTypes;
    QULongStringHash mAudioControllerTypes;
    QULongStringHash mNetworkAdapterTypes;
    QULongStringHash mNetworkAttachmentTypes;
    QULongStringHash mClipboardTypes;
    QULongStringHash mStorageControllerTypes;
    QULongStringHash mUSBDeviceStates;

    QString mUserDefinedPortName;

    QPixmap mWarningIcon, mErrorIcon;

    mutable bool mDetailReportTemplatesReady;

    friend VBoxGlobal &vboxGlobal();
    friend class VBoxCallback;
};

inline VBoxGlobal &vboxGlobal() { return VBoxGlobal::instance(); }

// Helper classes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Generic asyncronous event.
 *
 *  This abstract class is intended to provide a conveinent way to execute
 *  code on the main GUI thread asynchronously to the calling party. This is
 *  done by putting necessary actions to the #handle() function in a subclass
 *  and then posting an instance of the subclass using #post(). The instance
 *  must be allocated on the heap using the <tt>new</tt> operation and will be
 *  automatically deleted after processing. Note that if you don't call #post()
 *  on the created instance, you have to delete it yourself.
 */
class VBoxAsyncEvent : public QEvent
{
public:

    VBoxAsyncEvent() : QEvent ((QEvent::Type) VBoxDefs::AsyncEventType) {}

    /**
     *  Worker function. Gets executed on the GUI thread when the posted event
     *  is processed by the main event loop.
     */
    virtual void handle() = 0;

    /**
     *  Posts this event to the main event loop.
     *  The caller loses ownership of this object after this method returns
     *  and must not delete the object.
     */
    void post()
    {
        QApplication::postEvent (&vboxGlobal(), this);
    }
};

/**
 *  USB Popup Menu class.
 *  This class provides the list of USB devices attached to the host.
 */
class VBoxUSBMenu : public QMenu
{
    Q_OBJECT

public:

    VBoxUSBMenu (QWidget *);

    const CUSBDevice& getUSB (QAction *aAction);

    void setConsole (const CConsole &);

private slots:

    void processAboutToShow();

private:
    bool event(QEvent *aEvent);

    QMap <QAction *, CUSBDevice> mUSBDevicesMap;
    CConsole mConsole;
};

/**
 *  Enable/Disable Menu class.
 *  This class provides enable/disable menu items.
 */
class VBoxSwitchMenu : public QMenu
{
    Q_OBJECT

public:

    VBoxSwitchMenu (QWidget *, QAction *, bool aInverted = false);

    void setToolTip (const QString &);

private slots:

    void processAboutToShow();

private:

    QAction *mAction;
    bool     mInverted;
};

#endif /* __VBoxGlobal_h__ */
