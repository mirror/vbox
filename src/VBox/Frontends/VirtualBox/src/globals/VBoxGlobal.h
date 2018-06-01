/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxGlobal class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxGlobal_h___
#define ___VBoxGlobal_h___

/* Qt includes: */
#include <QFileIconProvider>
#include <QReadWriteLock>

/* GUI includes: */
#include "UIDefs.h"
#include "UILibraryDefs.h"
#include "UIMediumDefs.h"
#ifdef VBOX_WS_X11
# include "VBoxX11Helper.h"
#endif

/* COM includes: */
#include "VBox/com/Guid.h"
#include "CHost.h"
#include "CVirtualBoxClient.h"
#include "CVirtualBox.h"
#include "CSession.h"
#include "CGuestOSType.h"

/* Other includes: */
#ifdef VBOX_WS_X11
# include <X11/Xdefs.h>
#endif

/* Forward declarations: */
class QMenu;
class QToolButton;
class QSessionManager;
class QSpinBox;
class CHostVideoInputDevice;
class CMachine;
class CMedium;
class CUSBDevice;
class UIMedium;
class UIMediumEnumerator;
class UIIconPoolGeneral;
class UIThreadPool;

/** QObject subclass containing common GUI functionality. */
class SHARED_LIBRARY_STUFF VBoxGlobal : public QObject
{
    Q_OBJECT

signals:

    /** Asks listener to commit data. */
    void sigAskToCommitData();

    /** Asks listener to open URLs. */
    void sigAskToOpenURLs();

    /** Asks listener to recreate UI. */
    void sigAskToRestartUI();

    /** Notifies listeners about the VBoxSVC availability change. */
    void sigVBoxSVCAvailabilityChange();

    /* Notifiers: Medium-processing stuff: */
    void sigMediumCreated(const QString &strMediumID);
    void sigMediumDeleted(const QString &strMediumID);

    /* Notifiers: Medium-enumeration stuff: */
    void sigMediumEnumerationStarted();
    void sigMediumEnumerated(const QString &strMediumID);
    void sigMediumEnumerationFinished();

public:

#ifdef VBOX_GUI_WITH_SHARED_LIBRARY
    /** UI types. */
    enum UIType
    {
        UIType_SelectorUI,
        UIType_RuntimeUI
    };
#endif

    /** VM launch modes. */
    enum LaunchMode
    {
        LaunchMode_Invalid,
        LaunchMode_Default,
        LaunchMode_Headless,
        LaunchMode_Separate
    };

    /** Whether to start the VM running. */
    enum StartRunning
    {
        StartRunning_Default,   /**< Default (depends on debug settings). */
        StartRunning_No,        /**< Start the VM paused. */
        StartRunning_Yes        /**< Start the VM running. */
    };

    /* Static API: Create/destroy stuff: */
    static VBoxGlobal *instance() { return s_pInstance; }
#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
    static void create();
#else
    static void create(UIType enmType);
#endif
    static void destroy();

    bool isCleaningUp() { return s_fCleanupInProgress; }

    static QString qtRTVersionString();
    static uint qtRTVersion();
    static QString qtCTVersionString();
    static uint qtCTVersion();

    bool isValid() { return mValid; }

    QString vboxVersionString() const;
    QString vboxVersionStringNormalized() const;
    bool isBeta() const;

    QString versionString() const { return mVerString; }

#ifdef VBOX_WS_MAC
    /** Mac OS X: Returns #MacOSXRelease determined using <i>uname</i> call. */
    static MacOSXRelease determineOsRelease();
    /** Mac OS X: Returns #MacOSXRelease determined during VBoxGlobal prepare routine. */
    MacOSXRelease osRelease() const { return m_osRelease; }
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_X11
    /** X11: Returns whether the Window Manager we are running at is composition one. */
    bool isCompositingManagerRunning() const { return m_fCompositingManagerRunning; }
    /** X11: Returns the type of the Window Manager we are running under. */
    X11WMType typeOfWindowManager() const { return m_enmWindowManagerType; }
#endif /* VBOX_WS_X11 */

    /* branding */
    bool brandingIsActive (bool aForce = false);
    QString brandingGetKey (QString aKey);

    static bool hasAllowedExtension(const QString &strExt, const QStringList &extList)
    {
        for (int i = 0; i < extList.size(); ++i)
            if (strExt.endsWith(extList.at(i), Qt::CaseInsensitive))
                return true;
        return false;
    }

    bool processArgs();

    QList<QUrl> &argUrlList() { return m_ArgUrlList; }

    QString managedVMUuid() const { return vmUuid; }
    bool isVMConsoleProcess() const { return !vmUuid.isNull(); }
    /** Returns whether GUI is separate (from VM) process. */
    bool isSeparateProcess() const { return m_fSeparateProcess; }
    bool showStartVMErrors() const { return mShowStartVMErrors; }

    bool agressiveCaching() const { return mAgressiveCaching; }

    /** Returns whether we should restore current snapshot before VM started. */
    bool shouldRestoreCurrentSnapshot() const { return mRestoreCurrentSnapshot; }
    /** Defines whether we should fRestore current snapshot before VM started. */
    void setShouldRestoreCurrentSnapshot(bool fRestore) { mRestoreCurrentSnapshot = fRestore; }

    bool hasFloppyImageToMount() const    { return !m_strFloppyImage.isEmpty(); }
    bool hasDvdImageToMount() const       { return !m_strDvdImage.isEmpty(); }
    QString const &getFloppyImage() const { return m_strFloppyImage; }
    QString const &getDvdImage() const    { return m_strDvdImage; }

    bool isPatmDisabled() const { return mDisablePatm; }
    bool isCsamDisabled() const { return mDisableCsam; }
    bool isSupervisorCodeExecedRecompiled() const { return mRecompileSupervisor; }
    bool isUserCodeExecedRecompiled()       const { return mRecompileUser; }
    bool areWeToExecuteAllInIem()           const { return mExecuteAllInIem; }
    bool isDefaultWarpPct() const { return mWarpPct == 100; }
    uint32_t getWarpPct()       const { return mWarpPct; }

#ifdef VBOX_WITH_DEBUGGER_GUI
    bool isDebuggerEnabled() const;
    bool isDebuggerAutoShowEnabled() const;
    bool isDebuggerAutoShowCommandLineEnabled() const;
    bool isDebuggerAutoShowStatisticsEnabled() const;

    RTLDRMOD getDebuggerModule() const { return m_hVBoxDbg; }
#endif /* VBOX_WITH_DEBUGGER_GUI */

    bool shouldStartPaused() const
    {
#ifdef VBOX_WITH_DEBUGGER_GUI
        return m_enmStartRunning == StartRunning_Default ? isDebuggerAutoShowEnabled() : m_enmStartRunning == StartRunning_No;
#else
        return false;
#endif
    }

#ifdef VBOX_GUI_WITH_PIDFILE
    void createPidfile();
    void deletePidfile();
#endif

    QString languageName() const;
    QString languageCountry() const;
    QString languageNameEnglish() const;
    QString languageCountryEnglish() const;
    QString languageTranslators() const;

    /** Returns VBox language sub-directory. */
    static QString vboxLanguageSubDirectory();
    /** Returns VBox language file-base. */
    static QString vboxLanguageFileBase();
    /** Returns VBox language file-extension. */
    static QString vboxLanguageFileExtension();
    /** Returns VBox language ID reg-exp. */
    static QString vboxLanguageIdRegExp();
    /** Returns built in language name. */
    static QString vboxBuiltInLanguageName();

    static QString languageId();
    static QString systemLanguageId();

    static void loadLanguage (const QString &aLangId = QString::null);

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

    static QChar decimalSep();
    static QString sizeRegexp();

    static quint64 parseSize (const QString &);
    static QString formatSize (quint64 aSize, uint aDecimal = 2, FormatSize aMode = FormatSize_Round);

    /* Returns full medium-format name for the given base medium-format name: */
    static QString fullMediumFormatName(const QString &strBaseMediumFormatName);

    QStringList COMPortNames() const;
    QString toCOMPortName (ulong aIRQ, ulong aIOBase) const;
    bool toCOMPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    QStringList LPTPortNames() const;
    QString toLPTPortName (ulong aIRQ, ulong aIOBase) const;
    bool toLPTPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

    static QString highlight (const QString &aStr, bool aToolTip = false);
    static QString emphasize (const QString &aStr);
    static QString removeAccelMark (const QString &aText);
    static QString insertKeyToActionText (const QString &aText, const QString &aKey);

    static QString replaceHtmlEntities(QString strText);

    QString helpFile() const;
    static QString documentsPath();

    static QRect normalizeGeometry (const QRect &aRectangle, const QRegion &aBoundRegion,
                                    bool aCanResize = true);
    static QRect getNormalized (const QRect &aRectangle, const QRegion &aBoundRegion,
                                bool aCanResize = true);
    static QRegion flip (const QRegion &aRegion);

    static void centerWidget (QWidget *aWidget, QWidget *aRelative,
                              bool aCanResize = true);

    /** Assigns top-level @a pWidget geometry passed as QRect coordinates.
      * @note  Take into account that this request may fail on X11. */
    static void setTopLevelGeometry(QWidget *pWidget, int x, int y, int w, int h);
    /** Assigns top-level @a pWidget geometry passed as @a rect.
      * @note  Take into account that this request may fail on X11. */
    static void setTopLevelGeometry(QWidget *pWidget, const QRect &rect);

    static bool activateWindow (WId aWId, bool aSwitchDesktop = true);

#ifdef VBOX_WS_X11
    /** X11: Test whether the current window manager supports full screen mode. */
    static bool supportsFullScreenMonitorsProtocolX11();
    /** X11: Performs mapping of the passed @a pWidget to host-screen with passed @a uScreenId. */
    static bool setFullScreenMonitorX11(QWidget *pWidget, ulong uScreenId);

    /** X11: Returns a list of current _NET_WM_STATE flags for passed @a pWidget. */
    static QVector<Atom> flagsNetWmState(QWidget *pWidget);
    /** X11: Check whether _NET_WM_STATE_FULLSCREEN flag is set for passed @a pWidget. */
    static bool isFullScreenFlagSet(QWidget *pWidget);
    /** X11: Sets _NET_WM_STATE_FULLSCREEN flag for passed @a pWidget. */
    static void setFullScreenFlag(QWidget *pWidget);
    /** X11: Sets _NET_WM_STATE_SKIP_TASKBAR flag for passed @a pWidget. */
    static void setSkipTaskBarFlag(QWidget *pWidget);
    /** X11: Sets _NET_WM_STATE_SKIP_PAGER flag for passed @a pWidget. */
    static void setSkipPagerFlag(QWidget *pWidget);

    /** Assigns WM_CLASS property for passed @a pWidget. */
    static void setWMClass(QWidget *pWidget, const QString &strNameString, const QString &strClassString);
#endif /* VBOX_WS_X11 */

    /* Shame on Qt it hasn't stuff for tuning
     * widget size suitable for reflecting content of desired size.
     * For example QLineEdit, QSpinBox and similar widgets should have a methods
     * to strict the minimum width to reflect at least [n] symbols. */
    static void setMinimumWidthAccordingSymbolCount(QSpinBox *pSpinBox, int cCount);

    /** Try to acquire COM cleanup protection token for reading. */
    bool comTokenTryLockForRead() { return m_comCleanupProtectionToken.tryLockForRead(); }
    /** Unlock previously acquired COM cleanup protection token. */
    void comTokenUnlock() { return m_comCleanupProtectionToken.unlock(); }

    /** Returns the copy of VirtualBox client wrapper. */
    CVirtualBoxClient virtualBoxClient() const { return m_client; }
    /** Returns the copy of VirtualBox object wrapper. */
    CVirtualBox virtualBox() const { return m_vbox; }
    /** Returns the copy of VirtualBox host-object wrapper. */
    CHost host() const { return m_host; }
    /** Returns the symbolic VirtualBox home-folder representation. */
    QString homeFolder() const { return m_strHomeFolder; }

    /** Returns the VBoxSVC availability value. */
    bool isVBoxSVCAvailable() const { return m_fVBoxSVCAvailable; }

    QList <CGuestOSType> vmGuestOSFamilyList() const;
    QList <CGuestOSType> vmGuestOSTypeList (const QString &aFamilyId) const;

    CGuestOSType vmGuestOSType (const QString &aTypeId,
                                const QString &aFamilyId = QString::null) const;
    QString vmGuestOSTypeDescription (const QString &aTypeId) const;

    static bool isDOSType (const QString &aOSTypeId);

    bool switchToMachine(CMachine &machine);
    bool launchMachine(CMachine &machine, LaunchMode enmLaunchMode = LaunchMode_Default);

    CSession openSession(const QString &aId, KLockType aLockType = KLockType_Write);
    /** Shortcut to openSession (aId, true). */
    CSession openExistingSession(const QString &aId) { return openSession(aId, KLockType_Shared); }

    static QList <QPair <QString, QString> > MediumBackends(KDeviceType enmDeviceType);
    static QList <QPair <QString, QString> > HDDBackends();
    static QList <QPair <QString, QString> > DVDBackends();
    static QList <QPair <QString, QString> > FloppyBackends();

    /* API: Medium-enumeration stuff: */
    void startMediumEnumeration();
    bool isMediumEnumerationInProgress() const;
    UIMedium medium(const QString &strMediumID) const;
    QList<QString> mediumIDs() const;
    void createMedium(const UIMedium &medium);
    void deleteMedium(const QString &strMediumID);

    /* API: Medium-processing stuff: */
    QString openMedium(UIMediumType mediumType, QString strMediumLocation, QWidget *pParent = 0);

    QString openMediumWithFileOpenDialog(UIMediumType mediumType, QWidget *pParent = 0,
                                         const QString &strDefaultFolder = QString(), bool fUseLastFolder = true);

    QString createVisoMediumWithFileOpenDialog(QWidget *pParent, const QString &strMachineFolder);

    /** Prepares storage menu according passed parameters.
      * @param menu              QMenu being prepared.
      * @param pListener         Listener QObject, this menu being prepared for.
      * @param pszSlotName       SLOT in the @a pListener above, this menu will be handled with.
      * @param machine           CMachine object, this menu being prepared for.
      * @param strControllerName The name of the CStorageController in the @a machine above.
      * @param storageSlot       The StorageSlot of the CStorageController called @a strControllerName above. */
    void prepareStorageMenu(QMenu &menu,
                            QObject *pListener, const char *pszSlotName,
                            const CMachine &machine, const QString &strControllerName, const StorageSlot &storageSlot);
    /** Updates @a constMachine storage with data described by @a target. */
    void updateMachineStorage(const CMachine &constMachine, const UIMediumTarget &target);

    QString details(const CMedium &medium, bool fPredictDiff, bool fUseHtml = true);

#ifdef RT_OS_LINUX
    static void checkForWrongUSBMounted();
#endif /* RT_OS_LINUX */

    QString details (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDevice &aDevice) const;
    QString toolTip (const CUSBDeviceFilter &aFilter) const;
    QString toolTip(const CHostVideoInputDevice &webcam) const;

    /** Initiates the extension pack installation process.
      * @param  strFilePath      Brings the extension pack file path.
      * @param  strDigest        Brings the extension pack file digest.
      * @param  pParent          Brings the parent dialog reference.
      * @param  pstrExtPackName  Brings the extension pack name. */
    static void doExtPackInstallation(QString const &strFilePath,
                                      QString const &strDigest,
                                      QWidget *pParent,
                                      QString *pstrExtPackName);

    /** Returns whether the Extension Pack installation was requested at startup. */
    bool isEPInstallationRequested() const { return m_fEPInstallationRequested; }
    /** Defines whether the Extension Pack installation was @a fRequested at startup. */
    void setEPInstallationRequested(bool fRequested) { m_fEPInstallationRequested = fRequested; }

    bool is3DAvailableWorker() const;
    bool is3DAvailable() const { if (m3DAvailable < 0) return is3DAvailableWorker(); return m3DAvailable != 0; }

#ifdef VBOX_WITH_CRHGSMI
    static bool isWddmCompatibleOsType(const QString &strGuestOSTypeId);
#endif /* VBOX_WITH_CRHGSMI */

    static quint64 requiredVideoMemory(const QString &strGuestOSTypeId, int cMonitors = 1);

    /** Returns the thread-pool instance. */
    UIThreadPool* threadPool() const { return m_pThreadPool; }

    /** Returns icon defined for a passed @a comMachine. */
    QIcon vmUserIcon(const CMachine &comMachine) const;
    /** Returns pixmap of a passed @a size defined for a passed @a comMachine. */
    QPixmap vmUserPixmap(const CMachine &comMachine, const QSize &size) const;
    /** Returns pixmap defined for a passed @a comMachine.
      * In case if non-null @a pLogicalSize pointer provided, it will be updated properly. */
    QPixmap vmUserPixmapDefault(const CMachine &comMachine, QSize *pLogicalSize = 0) const;

    /** Returns pixmap corresponding to passed @a strOSTypeID. */
    QIcon vmGuestOSTypeIcon(const QString &strOSTypeID) const;
    /** Returns pixmap corresponding to passed @a strOSTypeID and @a size. */
    QPixmap vmGuestOSTypePixmap(const QString &strOSTypeID, const QSize &size) const;
    /** Returns pixmap corresponding to passed @a strOSTypeID.
      * In case if non-null @a pLogicalSize pointer provided, it will be updated properly. */
    QPixmap vmGuestOSTypePixmapDefault(const QString &strOSTypeID, QSize *pLogicalSize = 0) const;

    QIcon icon(QFileIconProvider::IconType type) { return m_globalIconProvider.icon(type); }
    QIcon icon(const QFileInfo &info) { return m_globalIconProvider.icon(info); }

    QPixmap warningIcon() const { return mWarningIcon; }
    QPixmap errorIcon() const { return mErrorIcon; }

    static QPixmap joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2);

    static void setTextLabel (QToolButton *aToolButton, const QString &aTextLabel);

    static QString locationForHTML (const QString &aFileName);

    static QWidget *findWidget (QWidget *aParent, const char *aName,
                                const char *aClassName = NULL,
                                bool aRecursive = false);

public slots:

    bool openURL (const QString &aURL);

    void sltGUILanguageChange(QString strLang);

protected:

    bool eventFilter (QObject *, QEvent *);

    void retranslateUi();

protected slots:

    /* Handlers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /** Handles @a manager request for emergency session shutdown. */
    void sltHandleCommitDataRequest(QSessionManager &manager);

    /** Handles the VBoxSVC availability change. */
    void sltHandleVBoxSVCAvailabilityChange(bool fAvailable);

private:

#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
    /** Construcs global VirtualBox object. */
    VBoxGlobal();
#else
    /** Construcs global VirtualBox object of passed @a enmType. */
    VBoxGlobal(UIType enmType);
#endif

    /** Destrucs global VirtualBox object. */
    virtual ~VBoxGlobal() /* override */;

#ifdef VBOX_WS_WIN
    /** Wraps WinAPI ShutdownBlockReasonCreate function. */
    static BOOL ShutdownBlockReasonCreateAPI(HWND hWnd, LPCWSTR pwszReason);
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI
    void initDebuggerVar(int *piDbgCfgVar, const char *pszEnvVar, const char *pszExtraDataName, bool fDefault = false);
    void setDebuggerVar(int *piDbgCfgVar, bool fState);
    bool isDebuggerWorker(int *piDbgCfgVar, const char *pszExtraDataName) const;
#endif

    /** Re-initializes COM wrappers and containers. */
    void comWrappersReinit();

    /** Holds the singleton VBoxGlobal instance. */
    static VBoxGlobal *s_pInstance;

    /** Holds the currently loaded language ID. */
    static QString     s_strLoadedLanguageId;

    /** Holds whether VBoxGlobal cleanup is in progress. */
    static bool        s_fCleanupInProgress;

#ifdef VBOX_GUI_WITH_SHARED_LIBRARY
    /** Holds the UI type. */
    UIType m_enmType;
#endif

    bool mValid;

#ifdef VBOX_WS_MAC
    /** Mac OS X: Holds the #MacOSXRelease determined using <i>uname</i> call. */
    MacOSXRelease m_osRelease;
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_X11
    /** X11: Holds whether the Window Manager we are running at is composition one. */
    bool m_fCompositingManagerRunning;
    /** X11: Holds the type of the Window Manager we are running under. */
    X11WMType m_enmWindowManagerType;
#endif /* VBOX_WS_X11 */

    QString mVerString;
    QString mBrandingConfig;

    QList<QUrl> m_ArgUrlList;

    QString vmUuid;
    /** Holds whether GUI is separate (from VM) process. */
    bool m_fSeparateProcess;
    /** Whether to show error message boxes for VM start errors. */
    bool mShowStartVMErrors;

    /** The --aggressive-caching / --no-aggressive-caching option. */
    bool mAgressiveCaching;

    /** The --restore-current option. */
    bool mRestoreCurrentSnapshot;

    /** @name Ad-hoc VM reconfiguration.
     * @{ */
        /** Floppy image. */
        QString m_strFloppyImage;
        /** DVD image. */
        QString m_strDvdImage;
    /** @} */

    /** @name VMM options
     * @{ */
        /** The --disable-patm option. */
        bool mDisablePatm;
        /** The --disable-csam option. */
        bool mDisableCsam;
        /** The --recompile-supervisor option. */
        bool mRecompileSupervisor;
        /** The --recompile-user option. */
        bool mRecompileUser;
        /** The --execute-all-in-iem option. */
        bool mExecuteAllInIem;
        /** The --warp-factor option value. */
        uint32_t mWarpPct;
    /** @} */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Whether the debugger should be accessible or not. */
    mutable int m_fDbgEnabled;
    /** Whether to show the debugger automatically with the console. */
    mutable int m_fDbgAutoShow;
    /** Whether to show the command line window when m_fDbgAutoShow is set. */
    mutable int m_fDbgAutoShowCommandLine;
    /** Whether to show the statistics window when m_fDbgAutoShow is set. */
    mutable int m_fDbgAutoShowStatistics;
    /** VBoxDbg module handle. */
    RTLDRMOD m_hVBoxDbg;

    /** Whether --start-running, --start-paused or nothing was given. */
    enum StartRunning m_enmStartRunning;
#endif

    char mSettingsPw[256];
    bool mSettingsPwSet;

#ifdef VBOX_GUI_WITH_PIDFILE
    QString m_strPidfile;
#endif

    QString mUserDefinedPortName;

    /** COM cleanup protection token. */
    QReadWriteLock m_comCleanupProtectionToken;

    /** Holds the instance of VirtualBox client wrapper. */
    CVirtualBoxClient m_client;
    /** Holds the copy of VirtualBox object wrapper. */
    CVirtualBox m_vbox;
    /** Holds the copy of VirtualBox host-object wrapper. */
    CHost m_host;
    /** Holds the symbolic VirtualBox home-folder representation. */
    QString m_strHomeFolder;

    /** Holds whether acquired COM wrappers are currently valid. */
    bool m_fWrappersValid;
    /** Holds whether VBoxSVC is currently available. */
    bool m_fVBoxSVCAvailable;

    /** Holds the guest OS family IDs. */
    QList<QString> m_guestOSFamilyIDs;
    /** Holds the guest OS types for each family ID. */
    QList<QList<CGuestOSType> > m_guestOSTypes;

    int m3DAvailable;

    /** Holds the thread-pool instance. */
    UIThreadPool *m_pThreadPool;

    /** General icon-pool. */
    UIIconPoolGeneral *m_pIconPool;

    QFileIconProvider m_globalIconProvider;

    QPixmap mWarningIcon, mErrorIcon;

    /* Variable: Medium-enumeration stuff: */
    mutable QReadWriteLock m_mediumEnumeratorDtorRwLock;
    UIMediumEnumerator *m_pMediumEnumerator;

    /** Holds whether the Extension Pack installation was requested at startup. */
    bool m_fEPInstallationRequested;

#if defined(VBOX_WS_WIN) && defined(VBOX_GUI_WITH_SHARED_LIBRARY)
    /** Holds the ATL module instance (for use with VBoxGlobal shared library only).
      * @note  Required internally by ATL (constructor records instance in global variable). */
    ATL::CComModule _Module;
#endif

#if defined (VBOX_WS_WIN)
    DWORD dwHTMLHelpCookie;
#endif

    /** Allows for shortcut access. */
    friend VBoxGlobal &vboxGlobal();
};

/** Singleton VBoxGlobal 'official' name. */
inline VBoxGlobal &vboxGlobal() { return *VBoxGlobal::instance(); }

#endif /* !___VBoxGlobal_h___ */
