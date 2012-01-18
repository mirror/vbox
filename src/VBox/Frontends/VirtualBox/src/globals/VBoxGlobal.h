/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxGlobal class declaration
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxGlobal_h__
#define __VBoxGlobal_h__

#include "COMDefs.h"
#include "VBox/com/Guid.h"

#include "VBoxGlobalSettings.h"
#include "VBoxMedium.h"

/* Qt includes */
#include <QApplication>
#include <QLayout>
#include <QMenu>
#include <QStyle>
#include <QProcess>
#include <QHash>
#include <QFileIconProvider>

#ifdef Q_WS_X11
# include <sys/wait.h>
#endif

class QAction;
class QLabel;
class QToolButton;
class UIMachine;

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
#ifdef Q_WS_X11
        int status;
        if (process.pid() > 0)
            waitpid(process.pid(), &status, 0);
#endif
        return result;
    }

protected:

    Process (QWidget *aParent = 0) : QProcess (aParent) {}
};

struct StorageSlot
{
    StorageSlot() : bus (KStorageBus_Null), port (0), device (0) {}
    StorageSlot (const StorageSlot &aOther) : bus (aOther.bus), port (aOther.port), device (aOther.device) {}
    StorageSlot (KStorageBus aBus, LONG aPort, LONG aDevice) : bus (aBus), port (aPort), device (aDevice) {}
    StorageSlot& operator= (const StorageSlot &aOther) { bus = aOther.bus; port = aOther.port; device = aOther.device; return *this; }
    bool operator== (const StorageSlot &aOther) const { return bus == aOther.bus && port == aOther.port && device == aOther.device; }
    bool operator!= (const StorageSlot &aOther) const { return bus != aOther.bus || port != aOther.port || device != aOther.device; }
    bool operator< (const StorageSlot &aOther) const { return (bus < aOther.bus) ||
                                                              (bus == aOther.bus && port < aOther.port) ||
                                                              (bus == aOther.bus && port == aOther.port && device < aOther.device); }
    bool operator> (const StorageSlot &aOther) const { return (bus > aOther.bus) ||
                                                              (bus == aOther.bus && port > aOther.port) ||
                                                              (bus == aOther.bus && port == aOther.port && device > aOther.device); }
    bool isNull() { return bus == KStorageBus_Null; }
    KStorageBus bus; LONG port; LONG device;
};
Q_DECLARE_METATYPE (StorageSlot);

// VBoxGlobal class
////////////////////////////////////////////////////////////////////////////////

class UISelectorWindow;
class UIRegistrationWzd;
class VBoxUpdateDlg;

class VBoxGlobal : public QObject
{
    Q_OBJECT

public:

    typedef QHash <ulong, QString> QULongStringHash;
    typedef QHash <long, QString> QLongStringHash;

    static VBoxGlobal &instance();

    bool isValid() { return mValid; }

    static QString qtRTVersionString();
    static uint qtRTVersion();
    static QString qtCTVersionString();
    static uint qtCTVersion();

    QString vboxVersionString() const;
    QString vboxVersionStringNormalized() const;

    QString versionString() const { return mVerString; }
    bool isBeta() const;

    CVirtualBox virtualBox() const { return mVBox; }

    VBoxGlobalSettings &settings() { return gset; }
    bool setSettings (VBoxGlobalSettings &gs);

    UISelectorWindow &selectorWnd();

    QWidget *vmWindow();

    bool createVirtualMachine(const CSession &session);
    UIMachine* virtualMachine();

    /* main window handle storage */
    void setMainWindow (QWidget *aMainWindow) { mMainWindow = aMainWindow; }
    QWidget *mainWindow() const { return mMainWindow; }

    bool is3DAvailable();

#ifdef VBOX_GUI_WITH_PIDFILE
    void createPidfile();
    void deletePidfile();
#endif

    /* branding */
    bool brandingIsActive (bool aForce = false);
    QString brandingGetKey (QString aKey);

    bool processArgs();

    bool switchToMachine(CMachine &machine);
    bool launchMachine(CMachine &machine, bool fHeadless = false);

    bool isVMConsoleProcess() const { return !vmUuid.isNull(); }
    bool showStartVMErrors() const { return mShowStartVMErrors; }
#ifdef VBOX_GUI_WITH_SYSTRAY
    bool isTrayMenu() const;
    void setTrayMenu(bool aIsTrayMenu);
    void trayIconShowSelector();
    bool trayIconInstall();
#endif
    QString managedVMUuid() const { return vmUuid; }
    QList<QUrl> &argUrlList() { return m_ArgUrlList; }

    VBoxDefs::RenderMode vmRenderMode() const { return vm_render_mode; }
    const char *vmRenderModeStr() const { return vm_render_mode_str; }
    bool isKWinManaged() const { return mIsKWinManaged; }

    const QRect availableGeometry(int iScreen = 0) const;

    bool isPatmDisabled() const { return mDisablePatm; }
    bool isCsamDisabled() const { return mDisableCsam; }
    bool isSupervisorCodeExecedRecompiled() const { return mRecompileSupervisor; }
    bool isUserCodeExecedRecompiled()       const { return mRecompileUser; }

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool isDebuggerEnabled(CMachine &aMachine);
    bool isDebuggerAutoShowEnabled(CMachine &aMachine);
    bool isDebuggerAutoShowCommandLineEnabled(CMachine &aMachine);
    bool isDebuggerAutoShowStatisticsEnabled(CMachine &aMachine);
    RTLDRMOD getDebuggerModule() const { return mhVBoxDbg; }

    bool isStartPausedEnabled() const { return mStartPaused; }
#else
    bool isDebuggerAutoShowEnabled(CMachine & /*aMachine*/) const { return false; }
    bool isDebuggerAutoShowCommandLineEnabled(CMachine & /*aMachine*/) const { return false; }
    bool isDebuggerAutoShowStatisticsEnabled(CMachine & /*aMachine*/) const { return false; }

    bool isStartPausedEnabled() const { return false; }
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

    static inline QString yearsToString (uint32_t cVal)
    {
        return tr("%n year(s)", "", cVal);
    }

    static inline QString monthsToString (uint32_t cVal)
    {
        return tr("%n month(s)", "", cVal);
    }

    static inline QString daysToString (uint32_t cVal)
    {
        return tr("%n day(s)", "", cVal);
    }

    static inline QString hoursToString (uint32_t cVal)
    {
        return tr("%n hour(s)", "", cVal);
    }

    static inline QString minutesToString (uint32_t cVal)
    {
        return tr("%n minute(s)", "", cVal);
    }

    static inline QString secondsToString (uint32_t cVal)
    {
        return tr("%n second(s)", "", cVal);
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

    KStorageBus toStorageBusType (KStorageControllerType aControllerType) const
    {
        KStorageBus sb = KStorageBus_Null;
        switch (aControllerType)
        {
            case KStorageControllerType_Null: sb = KStorageBus_Null; break;
            case KStorageControllerType_PIIX3:
            case KStorageControllerType_PIIX4:
            case KStorageControllerType_ICH6: sb = KStorageBus_IDE; break;
            case KStorageControllerType_IntelAhci: sb = KStorageBus_SATA; break;
            case KStorageControllerType_LsiLogic:
            case KStorageControllerType_BusLogic: sb = KStorageBus_SCSI; break;
            case KStorageControllerType_I82078: sb = KStorageBus_Floppy; break;
            default:
              AssertMsgFailed (("toStorageBusType: %d not handled\n", aControllerType)); break;
        }
        return sb;
    }

    QString toString (KStorageBus aBus, LONG aChannel) const;
    LONG toStorageChannel (KStorageBus aBus, const QString &aChannel) const;

    QString toString (KStorageBus aBus, LONG aChannel, LONG aDevice) const;
    LONG toStorageDevice (KStorageBus aBus, LONG aChannel, const QString &aDevice) const;

    QString toString (StorageSlot aSlot) const;
    StorageSlot toStorageSlot (const QString &aSlot) const;

    QString toString (KMediumType t) const
    {
        AssertMsg (!mDiskTypes.value (t).isNull(), ("No text for %d", t));
        return mDiskTypes.value (t);
    }
    QString differencingMediumTypeName() const { return mDiskTypes_Differencing; }

    QString toString(KMediumVariant mediumVariant) const;

    /**
     * Similar to toString (KMediumType), but returns 'Differencing' for
     * normal hard disks that have a parent.
     */
    QString mediumTypeString (const CMedium &aHD) const
    {
        if (!aHD.GetParent().isNull())
        {
            Assert (aHD.GetType() == KMediumType_Normal);
            return mDiskTypes_Differencing;
        }
        return toString (aHD.GetType());
    }

    QString toString (KAuthType t) const
    {
        AssertMsg (!mAuthTypes.value (t).isNull(), ("No text for %d", t));
        return mAuthTypes.value (t);
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

    KStorageControllerType toControllerType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mStorageControllerTypes.begin(), mStorageControllerTypes.end(), s);
        AssertMsg (it != mStorageControllerTypes.end(), ("No value for {%s}",
                                                         s.toLatin1().constData()));
        return KStorageControllerType (it.key());
    }

    KAuthType toAuthType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mAuthTypes.begin(), mAuthTypes.end(), s);
        AssertMsg (it != mAuthTypes.end(), ("No value for {%s}",
                                                s.toLatin1().constData()));
        return KAuthType (it.key());
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

    QString toString (KNetworkAdapterPromiscModePolicy t) const
    {
        AssertMsg (!mNetworkAdapterPromiscModePolicyTypes.value (t).isNull(), ("No text for %d", t));
        return mNetworkAdapterPromiscModePolicyTypes.value (t);
    }

    KNetworkAdapterPromiscModePolicy toNetworkAdapterPromiscModePolicyType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mNetworkAdapterPromiscModePolicyTypes.begin(), mNetworkAdapterPromiscModePolicyTypes.end(), s);
        AssertMsg (it != mNetworkAdapterPromiscModePolicyTypes.end(), ("No value for {%s}",
                                                                       s.toLatin1().constData()));
        return KNetworkAdapterPromiscModePolicy (it.key());
    }

    QString toString (KNATProtocol t) const
    {
        AssertMsg (!mNATProtocolTypes.value (t).isNull(), ("No text for %d", t));
        return mNATProtocolTypes.value (t);
    }

    KNATProtocol toNATProtocolType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mNATProtocolTypes.begin(), mNATProtocolTypes.end(), s);
        AssertMsg (it != mNATProtocolTypes.end(), ("No value for {%s}",
                                                   s.toLatin1().constData()));
        return KNATProtocol (it.key());
    }

    QString toString (KUSBDeviceState aState) const
    {
        AssertMsg (!mUSBDeviceStates.value (aState).isNull(), ("No text for %d", aState));
        return mUSBDeviceStates.value (aState);
    }

    QString toString (KChipsetType t) const
    {
        AssertMsg (!mChipsetTypes.value (t).isNull(), ("No text for %d", t));
        return mChipsetTypes.value (t);
    }

    KChipsetType toChipsetType (const QString &s) const
    {
        QULongStringHash::const_iterator it =
            qFind (mChipsetTypes.begin(), mChipsetTypes.end(), s);
        AssertMsg (it != mChipsetTypes.end(), ("No value for {%s}",
                                               s.toLatin1().constData()));
        return KChipsetType (it.key());
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

    static bool hasAllowedExtension(const QString &strExt, const QStringList &extList) { for(int i = 0; i < extList.size(); ++i) if (strExt.endsWith(extList.at(i), Qt::CaseInsensitive)) return true; return false;}
    QIcon icon(QFileIconProvider::IconType type) { return m_globalIconProvider.icon(type); }
    QIcon icon(const QFileInfo &info) { return m_globalIconProvider.icon(info); }

    QPixmap warningIcon() const { return mWarningIcon; }
    QPixmap errorIcon() const { return mErrorIcon; }

    /* details generators */

    QString details (const CMedium &aHD, bool aPredictDiff);

    QString details (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDeviceFilter &aFilter) const;

    QString detailsReport (const CMachine &aMachine, bool aWithLinks);

    QString platformInfo();

    /* VirtualBox helpers */

#if defined(Q_WS_X11) && !defined(VBOX_OSE)
    double findLicenseFile (const QStringList &aFilesList, QRegExp aPattern, QString &aLicenseFile) const;
    bool showVirtualBoxLicense();
#endif

    CSession openSession(const QString &aId, bool aExisting = false);

    /** Shortcut to openSession (aId, true). */
    CSession openExistingSession(const QString &aId) { return openSession (aId, true); }

    bool startMachine(const QString &strId);

    void startEnumeratingMedia();

    void reloadProxySettings();

    /**
     * Returns a list of all currently registered media. This list is used to
     * globally track the accessibility state of all media on a dedicated thread.
     *
     * Note that the media list is initially empty (i.e. before the enumeration
     * process is started for the first time using #startEnumeratingMedia()).
     * See #startEnumeratingMedia() for more information about how meida are
     * sorted in the returned list.
     */
    const VBoxMediaList &currentMediaList() const { return mMediaList; }

    /** Returns true if the media enumeration is in progress. */
    bool isMediaEnumerationStarted() const { return mMediaEnumThread != NULL; }

    VBoxDefs::MediumType mediumTypeToLocal(KDeviceType globalType);
    KDeviceType mediumTypeToGlobal(VBoxDefs::MediumType localType);

    void addMedium (const VBoxMedium &);
    void updateMedium (const VBoxMedium &);
    void removeMedium (VBoxDefs::MediumType, const QString &);

    bool findMedium (const CMedium &, VBoxMedium &) const;
    VBoxMedium findMedium (const QString &aMediumId) const;

    /** Compact version of #findMediumTo(). Asserts if not found. */
    VBoxMedium getMedium (const CMedium &aObj) const
    {
        VBoxMedium medium;
        if (!findMedium (aObj, medium))
            AssertFailed();
        return medium;
    }

    QString openMediumWithFileOpenDialog(VBoxDefs::MediumType mediumType, QWidget *pParent = 0,
                                         const QString &strDefaultFolder = QString(), bool fUseLastFolder = true);
    QString openMedium(VBoxDefs::MediumType mediumType, QString strMediumLocation, QWidget *pParent = 0);

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

    static QString languageId();
    static void loadLanguage (const QString &aLangId = QString::null);
    QString helpFile() const;

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

    static QString toHumanReadableList(const QStringList &list);

    static quint64 parseSize (const QString &);
    static QString formatSize (quint64 aSize, uint aDecimal = 2,
                               VBoxDefs::FormatSize aMode = VBoxDefs::FormatSize_Round);

    static quint64 requiredVideoMemory(const QString &strGuestOSTypeId, int cMonitors = 1);

    static QString locationForHTML (const QString &aFileName);

    static QString highlight (const QString &aStr, bool aToolTip = false);

    static QString replaceHtmlEntities(QString strText);
    static QString emphasize (const QString &aStr);

    static QString systemLanguageId();

    static bool activateWindow (WId aWId, bool aSwitchDesktop = true);

    static QString removeAccelMark (const QString &aText);

    static QString insertKeyToActionText (const QString &aText, const QString &aKey);
    static QString extractKeyFromActionText (const QString &aText);

    static QPixmap joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2);

    static QWidget *findWidget (QWidget *aParent, const char *aName,
                                const char *aClassName = NULL,
                                bool aRecursive = false);

    static QList <QPair <QString, QString> > MediumBackends(KDeviceType enmDeviceType);
    static QList <QPair <QString, QString> > HDDBackends();
    static QList <QPair <QString, QString> > DVDBackends();
    static QList <QPair <QString, QString> > FloppyBackends();

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

    static QString documentsPath();

#ifdef VBOX_WITH_VIDEOHWACCEL
    static bool isAcceleration2DVideoAvailable();

    /** additional video memory required for the best 2D support performance
     *  total amount of VRAM required is thus calculated as requiredVideoMemory + required2DOffscreenVideoMemory  */
    static quint64 required2DOffscreenVideoMemory();
#endif

#ifdef VBOX_WITH_CRHGSMI
    static bool isWddmCompatibleOsType(const QString &strGuestOSTypeId);
    static quint64 required3DWddmOffscreenVideoMemory(const QString &strGuestOSTypeId, int cMonitors = 1);
#endif /* VBOX_WITH_CRHGSMI */

#ifdef Q_WS_MAC
    bool isSheetWindowsAllowed(QWidget *pParent) const;
#endif /* Q_WS_MAC */

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
    void mediumRemoved (VBoxDefs::MediumType, const QString &);

#ifdef VBOX_GUI_WITH_SYSTRAY
    void sigTrayIconShow(bool fEnabled);
#endif

public slots:

    bool openURL (const QString &aURL);

    void showRegistrationDialog (bool aForce = true);
    void sltGUILanguageChange(QString strLang);
    void sltProcessGlobalSettingChange();

protected:

    bool event (QEvent *e);
    bool eventFilter (QObject *, QEvent *);

private:

    VBoxGlobal();
    ~VBoxGlobal();

    void init();
#ifdef VBOX_WITH_DEBUGGER_GUI
    void initDebuggerVar(int *piDbgCfgVar, const char *pszEnvVar, const char *pszExtraDataName, bool fDefault = false);
    void setDebuggerVar(int *piDbgCfgVar, bool fState);
    bool isDebuggerWorker(int *piDbgCfgVar, CMachine &rMachine, const char *pszExtraDataName);
#endif

    bool mValid;

    CVirtualBox mVBox;

    VBoxGlobalSettings gset;

    UISelectorWindow *mSelectorWnd;
    UIMachine *m_pVirtualMachine;
    QWidget* mMainWindow;

#ifdef VBOX_WITH_REGISTRATION
    UIRegistrationWzd *mRegDlg;
#endif

    QString vmUuid;
    QList<QUrl> m_ArgUrlList;

#ifdef VBOX_GUI_WITH_SYSTRAY
    bool mIsTrayMenu : 1; /*< Tray icon active/desired? */
    bool mIncreasedWindowCounter : 1;
#endif

    /** Whether to show error message boxes for VM start errors. */
    bool mShowStartVMErrors;

    QThread *mMediaEnumThread;
    VBoxMediaList mMediaList;

    VBoxDefs::RenderMode vm_render_mode;
    const char * vm_render_mode_str;
    bool mIsKWinManaged;

    /** The --disable-patm option. */
    bool mDisablePatm;
    /** The --disable-csam option. */
    bool mDisableCsam;
    /** The --recompile-supervisor option. */
    bool mRecompileSupervisor;
    /** The --recompile-user option. */
    bool mRecompileUser;

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Whether the debugger should be accessible or not.
     * Use --dbg, the env.var. VBOX_GUI_DBG_ENABLED, --debug or the env.var.
     * VBOX_GUI_DBG_AUTO_SHOW to enable. */
    int mDbgEnabled;
    /** Whether to show the debugger automatically with the console.
     * Use --debug or the env.var. VBOX_GUI_DBG_AUTO_SHOW to enable. */
    int mDbgAutoShow;
    /** Whether to show the command line window when mDbgAutoShow is set. */
    int mDbgAutoShowCommandLine;
    /** Whether to show the statistics window when mDbgAutoShow is set. */
    int mDbgAutoShowStatistics;
    /** VBoxDbg module handle. */
    RTLDRMOD mhVBoxDbg;

    /** Whether to start the VM in paused state or not. */
    bool mStartPaused;
#endif

#if defined (Q_WS_WIN32)
    DWORD dwHTMLHelpCookie;
#endif

    QString mVerString;
    QString mBrandingConfig;
    
    int m3DAvailable;

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
    QULongStringHash mSlotTemplates;

    QULongStringHash mDiskTypes;
    QString mDiskTypes_Differencing;

    QULongStringHash mAuthTypes;
    QULongStringHash mPortModeTypes;
    QULongStringHash mUSBFilterActionTypes;
    QULongStringHash mAudioDriverTypes;
    QULongStringHash mAudioControllerTypes;
    QULongStringHash mNetworkAdapterTypes;
    QULongStringHash mNetworkAttachmentTypes;
    QULongStringHash mNetworkAdapterPromiscModePolicyTypes;
    QULongStringHash mNATProtocolTypes;
    QULongStringHash mClipboardTypes;
    QULongStringHash mStorageControllerTypes;
    QULongStringHash mUSBDeviceStates;
    QULongStringHash mChipsetTypes;

    QString mUserDefinedPortName;

    QPixmap mWarningIcon, mErrorIcon;

    QFileIconProvider m_globalIconProvider;

#ifdef VBOX_GUI_WITH_PIDFILE
    QString m_strPidfile;
#endif

    friend VBoxGlobal &vboxGlobal();
};

inline VBoxGlobal &vboxGlobal() { return VBoxGlobal::instance(); }

// Helper classes
////////////////////////////////////////////////////////////////////////////////

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

