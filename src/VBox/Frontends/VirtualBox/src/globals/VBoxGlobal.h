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

    /** @name Common stuff.
     * @{ */
        /** Asks #UIStarter listener to commit data. */
        void sigAskToCommitData();
    /** @} */

    /** @name Process arguments stuff.
     * @{ */
        /** Asks #UIStarter listener to open URLs. */
        void sigAskToOpenURLs();
    /** @} */

    /** @name COM stuff.
     * @{ */
        /** Asks #UIStarter listener to restart UI. */
        void sigAskToRestartUI();

        /** Notifies listeners about the VBoxSVC availability change. */
        void sigVBoxSVCAvailabilityChange();
    /** @} */

    /** @name COM: Virtual Media stuff.
     * @{ */
        /** Notifies listeners about medium with certain @a strMediumID created. */
        void sigMediumCreated(const QString &strMediumID);
        /** Notifies listeners about medium with certain @a strMediumID deleted. */
        void sigMediumDeleted(const QString &strMediumID);

        /** Notifies listeners about medium enumeration started. */
        void sigMediumEnumerationStarted();
        /** Notifies listeners about medium with certain @a strMediumID enumerated. */
        void sigMediumEnumerated(const QString &strMediumID);
        /** Notifies listeners about medium enumeration finished. */
        void sigMediumEnumerationFinished();
    /** @} */

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

    /** VM launch running options. */
    enum StartRunning
    {
        StartRunning_Default, /**< Default (depends on debug settings). */
        StartRunning_No,      /**< Start the VM paused. */
        StartRunning_Yes      /**< Start the VM running. */
    };

    /** Returns VBoxGlobal instance. */
    static VBoxGlobal *instance() { return s_pInstance; }
#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
    /** Creates VBoxGlobal instance. */
    static void create();
#else
    /** Creates VBoxGlobal instance of passed @a enmType. */
    static void create(UIType enmType);
#endif
    /** Destroys VBoxGlobal instance. */
    static void destroy();

    /** @name Common stuff.
     * @{ */
        /** Returns whether VBoxGlobal cleanup is in progress. */
        bool isCleaningUp() { return s_fCleanupInProgress; }

        /** Returns Qt runtime version string. */
        static QString qtRTVersionString();
        /** Returns Qt runtime version. */
        static uint qtRTVersion();
        /** Returns Qt compiled version string. */
        static QString qtCTVersionString();
        /** Returns Qt compiled version. */
        static uint qtCTVersion();

        /** Returns whether VBoxGlobal instance is properly initialized. */
        bool isValid() { return mValid; }

        /** Returns VBox version string. */
        QString vboxVersionString() const;
        /** Returns normalized VBox version string. */
        QString vboxVersionStringNormalized() const;
        /** Returns whether VBox version string contains BETA word. */
        bool isBeta() const;

#ifdef VBOX_WS_MAC
        /** Mac OS X: Returns #MacOSXRelease determined by <i>uname</i> call. */
        static MacOSXRelease determineOsRelease();
        /** Mac OS X: Returns #MacOSXRelease determined during VBoxGlobal prepare routine. */
        MacOSXRelease osRelease() const { return m_osRelease; }
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_X11
        /** X11: Returns whether the Window Manager we are running under is composition one. */
        bool isCompositingManagerRunning() const { return m_fCompositingManagerRunning; }
        /** X11: Returns the type of the Window Manager we are running under. */
        X11WMType typeOfWindowManager() const { return m_enmWindowManagerType; }
#endif /* VBOX_WS_X11 */

        /** Returns whether branding is active. */
        bool brandingIsActive (bool aForce = false);
        /** Returns value for certain branding @a strKey from custom.ini file. */
        QString brandingGetKey (QString aKey);
    /** @} */

    /** @name Process arguments stuff.
     * @{ */
        /** Returns whether passed @a strExt ends with one of allowed extension in the @a extList. */
        static bool hasAllowedExtension(const QString &strExt, const QStringList &extList);

        /** Process application args. */
        bool processArgs();

        /** Returns the URL arguments list. */
        QList<QUrl> &argUrlList() { return m_ArgUrlList; }

        /** Returns the --startvm option value (managed VM id). */
        QString managedVMUuid() const { return vmUuid; }
        /** Returns whether this is VM console process. */
        bool isVMConsoleProcess() const { return !vmUuid.isNull(); }
        /** Returns the --separate option value (whether GUI process is separate from VM process). */
        bool isSeparateProcess() const { return m_fSeparateProcess; }
        /** Returns the --no-startvm-errormsgbox option value (whether startup VM errors are disabled). */
        bool showStartVMErrors() const { return mShowStartVMErrors; }

        /** Returns the --aggressive-caching / --no-aggressive-caching option value (whether medium-enumeration is required). */
        bool agressiveCaching() const { return mAgressiveCaching; }

        /** Returns the --restore-current option value (whether we should restore current snapshot before VM started). */
        bool shouldRestoreCurrentSnapshot() const { return mRestoreCurrentSnapshot; }
        /** Defines whether we should fRestore current snapshot before VM started. */
        void setShouldRestoreCurrentSnapshot(bool fRestore) { mRestoreCurrentSnapshot = fRestore; }

        /** Returns the --fda option value (whether we have floppy image). */
        bool hasFloppyImageToMount() const { return !m_strFloppyImage.isEmpty(); }
        /** Returns the --dvd | --cdrom option value (whether we have DVD image). */
        bool hasDvdImageToMount() const { return !m_strDvdImage.isEmpty(); }
        /** Returns floppy image name. */
        QString const &getFloppyImage() const { return m_strFloppyImage; }
        /** Returns DVD image name. */
        QString const &getDvdImage() const { return m_strDvdImage; }

        /** Returns the --disable-patm option value. */
        bool isPatmDisabled() const { return mDisablePatm; }
        /** Returns the --disable-csam option value. */
        bool isCsamDisabled() const { return mDisableCsam; }
        /** Returns the --recompile-supervisor option value. */
        bool isSupervisorCodeExecedRecompiled() const { return mRecompileSupervisor; }
        /** Returns the --recompile-user option value. */
        bool isUserCodeExecedRecompiled()       const { return mRecompileUser; }
        /** Returns the --execute-all-in-iem option value. */
        bool areWeToExecuteAllInIem()           const { return mExecuteAllInIem; }
        /** Returns whether --warp-factor option value is equal to 100. */
        bool isDefaultWarpPct() const { return mWarpPct == 100; }
        /** Returns the --warp-factor option value. */
        uint32_t getWarpPct()       const { return mWarpPct; }

#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Holds whether the debugger should be accessible. */
        bool isDebuggerEnabled() const;
        /** Holds whether to show the debugger automatically with the console. */
        bool isDebuggerAutoShowEnabled() const;
        /** Holds whether to show the command line window when m_fDbgAutoShow is set. */
        bool isDebuggerAutoShowCommandLineEnabled() const;
        /** Holds whether to show the statistics window when m_fDbgAutoShow is set. */
        bool isDebuggerAutoShowStatisticsEnabled() const;

        /** VBoxDbg module handle. */
        RTLDRMOD getDebuggerModule() const { return m_hVBoxDbg; }
#endif /* VBOX_WITH_DEBUGGER_GUI */

        /** Returns whether VM should start paused. */
        bool shouldStartPaused() const;

#ifdef VBOX_GUI_WITH_PIDFILE
        /** Creates PID file. */
        void createPidfile();
        /** Deletes PID file. */
        void deletePidfile();
#endif
    /** @} */

    /** @name Localization stuff.
     * @{ */
        /** Native language name of the currently installed translation. */
        QString languageName() const;
        /** Native language country name of the currently installed translation. */
        QString languageCountry() const;
        /** Language name of the currently installed translation, in English. */
        QString languageNameEnglish() const;
        /** Language country name of the currently installed translation, in English. */
        QString languageCountryEnglish() const;
        /** Comma-separated list of authors of the currently installed translation. */
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

        /** Returns the loaded (active) language ID. */
        static QString languageId();
        /** Returns the system language ID. */
        static QString systemLanguageId();

        /** Loads the language by language ID.
          * @param  strLangId  Brings the language ID in in form of xx_YY.
          *                    QString() means the system default language. */
        static void loadLanguage (const QString &aLangId = QString::null);

        /** Returns tr("%n year(s)"). */
        static QString yearsToString(uint32_t cVal);
        /** Returns tr("%n month(s)"). */
        static QString monthsToString(uint32_t cVal);
        /** Returns tr("%n day(s)"). */
        static QString daysToString(uint32_t cVal);
        /** Returns tr("%n hour(s)"). */
        static QString hoursToString(uint32_t cVal);
        /** Returns tr("%n minute(s)"). */
        static QString minutesToString(uint32_t cVal);
        /** Returns tr("%n second(s)"). */
        static QString secondsToString(uint32_t cVal);

        /** Returns the decimal separator for the current locale. */
        static QChar decimalSep();
        /** Returns the regexp string that defines the format of the human-readable size representation. */
        static QString sizeRegexp();
        /** Parses the given size strText and returns the size value in bytes. */
        static quint64 parseSize (const QString &);
        /** Formats the given @a uSize value in bytes to a human readable string.
          * @param  uSize     Brings the size value in bytes.
          * @param  enmMode   Brings the conversion mode.
          * @param  cDecimal  Brings the number of decimal digits in result. */
        static QString formatSize (quint64 aSize, uint aDecimal = 2, FormatSize aMode = FormatSize_Round);

        /** Returns full medium-format name for the given @a strBaseMediumFormatName. */
        static QString fullMediumFormatName(const QString &strBaseMediumFormatName);

        /** Returns the list of the standard COM port names (i.e. "COMx"). */
        QStringList COMPortNames() const;
        /** Returns the name of the standard COM port corresponding to the given parameters,
          * or "User-defined" (which is also returned when both @a uIRQ and @a uIOBase are 0). */
        QString toCOMPortName (ulong aIRQ, ulong aIOBase) const;
        /** Returns port parameters corresponding to the given standard COM name.
          * Returns @c true on success, or @c false if the given port name is not one of the standard names (i.e. "COMx"). */
        bool toCOMPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;
        /** Returns the list of the standard LPT port names (i.e. "LPTx"). */
        QStringList LPTPortNames() const;
        /** Returns the name of the standard LPT port corresponding to the given parameters,
          * or "User-defined" (which is also returned when both @a uIRQ and @a uIOBase are 0). */
        QString toLPTPortName (ulong aIRQ, ulong aIOBase) const;
        /** Returns port parameters corresponding to the given standard LPT name.
          * Returns @c true on success, or @c false if the given port name is not one of the standard names (i.e. "LPTx"). */
        bool toLPTPortNumbers (const QString &aName, ulong &aIRQ, ulong &aIOBase) const;

        /** Reformats the input @a strText to highlight it. */
        static QString highlight (const QString &aStr, bool aToolTip = false);
        /** Reformats the input @a strText to emphasize it. */
        static QString emphasize (const QString &aStr);
        /** Removes the first occurrence of the accelerator mark (the ampersand symbol) from the given @a strText. */
        static QString removeAccelMark (const QString &aText);
        /** Inserts a passed @a strKey into action @a strText. */
        static QString insertKeyToActionText (const QString &aText, const QString &aKey);
    /** @} */

    /** @name File-system stuff.
     * @{ */
        /** Returns full help file name. */
        QString helpFile() const;

        /** Returns documents path. */
        static QString documentsPath();
    /** @} */

    /** @name Window/widget geometry stuff.
     * @{ */
        /** Search position for @a rectangle to make sure it is fully contained @a boundRegion. */
        static QRect normalizeGeometry (const QRect &aRectangle, const QRegion &aBoundRegion,
                                        bool aCanResize = true);
        /** Ensures that the given rectangle @a rectangle is fully contained within the region @a boundRegion. */
        static QRect getNormalized (const QRect &aRectangle, const QRegion &aBoundRegion,
                                    bool aCanResize = true);
        /** Returns the flipped (transposed) @a region. */
        static QRegion flip (const QRegion &aRegion);

        /** Aligns the center of @a pWidget with the center of @a pRelative. */
        static void centerWidget (QWidget *aWidget, QWidget *aRelative,
                                  bool aCanResize = true);

        /** Assigns top-level @a pWidget geometry passed as QRect coordinates.
          * @note  Take into account that this request may fail on X11. */
        static void setTopLevelGeometry(QWidget *pWidget, int x, int y, int w, int h);
        /** Assigns top-level @a pWidget geometry passed as @a rect.
          * @note  Take into account that this request may fail on X11. */
        static void setTopLevelGeometry(QWidget *pWidget, const QRect &rect);

        /** Activates the specified window with given @a wId. Can @a fSwitchDesktop if requested. */
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

        /** Assigns minimum @a pSpinBox to correspond to @a cCount digits. */
        static void setMinimumWidthAccordingSymbolCount(QSpinBox *pSpinBox, int cCount);
    /** @} */

    /** @name COM stuff.
     * @{ */
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
    /** @} */

    /** @name COM: Guest OS Type.
     * @{ */
        /** Returns the list of few guest OS types, queried from
          * IVirtualBox corresponding to every family id. */
        QList <CGuestOSType> vmGuestOSFamilyList() const;
        /** Returns the list of all guest OS types, queried from
          * IVirtualBox corresponding to passed family id. */
        QList <CGuestOSType> vmGuestOSTypeList (const QString &aFamilyId) const;

        /** Returns the guest OS type object corresponding to the given type id of list
          * containing OS types related to OS family determined by family id attribute.
          * If the index is invalid a null object is returned. */
        CGuestOSType vmGuestOSType (const QString &aTypeId,
                                    const QString &aFamilyId = QString::null) const;
        /** Returns the description corresponding to the given guest OS type id. */
        QString vmGuestOSTypeDescription (const QString &aTypeId) const;

        /** Returns whether guest type with passed @a strOSTypeId is one of DOS types. */
        static bool isDOSType (const QString &aOSTypeId);
    /** @} */

    /** @name COM: Virtual Machine stuff.
     * @{ */
        /** Switches to certain @a comMachine. */
        bool switchToMachine(CMachine &machine);
        /** Launches certain @a comMachine in specified @a enmLaunchMode. */
        bool launchMachine(CMachine &machine, LaunchMode enmLaunchMode = LaunchMode_Default);

        /** Opens session of certain @a enmLockType for VM with certain @a strId. */
        CSession openSession(const QString &aId, KLockType aLockType = KLockType_Write);
        /** Opens session of KLockType_Shared type for VM with certain @a strId. */
        CSession openExistingSession(const QString &aId) { return openSession(aId, KLockType_Shared); }
    /** @} */

    /** @name COM: Virtual Media stuff.
     * @{ */
        /** Returns medium formats which are currently supported by VirtualBox for the given @a enmDeviceType. */
        static QList <QPair <QString, QString> > MediumBackends(KDeviceType enmDeviceType);
        /** Returns which hard disk formats are currently supported by VirtualBox. */
        static QList <QPair <QString, QString> > HDDBackends();
        /** Returns which optical disk formats are currently supported by VirtualBox. */
        static QList <QPair <QString, QString> > DVDBackends();
        /** Returns which floppy disk formats are currently supported by VirtualBox. */
        static QList <QPair <QString, QString> > FloppyBackends();

        /** Starts medium enumeration. */
        void startMediumEnumeration();
        /** Returns whether medium enumeration is in progress. */
        bool isMediumEnumerationInProgress() const;
        /** Returns enumerated medium with certain @a strMediumID. */
        UIMedium medium(const QString &strMediumID) const;
        /** Returns enumerated medium IDs. */
        QList<QString> mediumIDs() const;
        /** Creates medium on the basis of passed @a guiMedium description. */
        void createMedium(const UIMedium &medium);
        /** Deletes medium with certain @a strMediumID. */
        void deleteMedium(const QString &strMediumID);

        /** Opens external medium by passed @a strMediumLocation.
          * @param  enmMediumType      Brings the medium type.
          * @param  pParent            Brings the dialog parent.
          * @param  strMediumLocation  Brings the file path to load medium from.
          * @param  pParent            Brings the dialog parent. */
        QString openMedium(UIMediumType mediumType, QString strMediumLocation, QWidget *pParent = 0);

        /** Opens external medium using file-open dialog.
          * @param  enmMediumType     Brings the medium type.
          * @param  pParent           Brings the dialog parent.
          * @param  strDefaultFolder  Brings the folder to browse for medium.
          * @param  fUseLastFolder    Brings whether we should propose to use last used folder. */
        QString openMediumWithFileOpenDialog(UIMediumType mediumType, QWidget *pParent = 0,
                                             const QString &strDefaultFolder = QString(), bool fUseLastFolder = true);

        /** Creates a VISO using the file-open dialog.
          * @param  pParent    Brings the dialog parent.
          * @param  strFolder  Brings the folder to browse for VISO file contents. */
        QString createVisoMediumWithFileOpenDialog(QWidget *pParent, const QString &strMachineFolder);

        /** Prepares storage menu according passed parameters.
          * @param  menu               Brings the #QMenu to be prepared.
          * @param  pListener          Brings the listener #QObject, this @a menu being prepared for.
          * @param  pszSlotName        Brings the name of the SLOT in the @a pListener above, this menu will be handled with.
          * @param  machine            Brings the #CMachine object, this @a menu being prepared for.
          * @param  strControllerName  Brings the name of the #CStorageController in the @a machine above.
          * @param  storageSlot        Brings the #StorageSlot of the storage controller with @a strControllerName above. */
        void prepareStorageMenu(QMenu &menu,
                                QObject *pListener, const char *pszSlotName,
                                const CMachine &machine, const QString &strControllerName, const StorageSlot &storageSlot);
        /** Updates @a comConstMachine storage with data described by @a target. */
        void updateMachineStorage(const CMachine &constMachine, const UIMediumTarget &target);

        /** Generates details for passed @a comMedium.
          * @param  fPredictDiff  Brings whether medium will be marked differencing on attaching.
          * @param  fUseHtml      Brings whether HTML subsets should be used in the generated output. */
        QString details(const CMedium &medium, bool fPredictDiff, bool fUseHtml = true);
    /** @} */

    /** @name COM: USB stuff.
     * @{ */
#ifdef RT_OS_LINUX
        /** Verifies that USB drivers are properly configured on Linux. */
        static void checkForWrongUSBMounted();
#endif /* RT_OS_LINUX */

        /** Generates details for passed USB @a comDevice. */
        QString details (const CUSBDevice &aDevice) const;
        /** Generates tool-tip for passed USB @a comDevice. */
        QString toolTip (const CUSBDevice &aDevice) const;
        /** Generates tool-tip for passed USB @a comFilter. */
        QString toolTip (const CUSBDeviceFilter &aFilter) const;
        /** Generates tool-tip for passed USB @a comWebcam. */
        QString toolTip(const CHostVideoInputDevice &webcam) const;
    /** @} */

    /** @name COM: Extension Pack stuff.
     * @{ */
        /** Initiates the extension pack installation process.
          * @param  strFilePath      Brings the extension pack file path.
          * @param  strDigest        Brings the extension pack file digest.
          * @param  pParent          Brings the parent dialog reference.
          * @param  pstrExtPackName  Brings the extension pack name. */
        static void doExtPackInstallation(QString const &strFilePath,
                                          QString const &strDigest,
                                          QWidget *pParent,
                                          QString *pstrExtPackName);

        /** @todo remove */
        bool isEPInstallationRequested() const { return m_fEPInstallationRequested; }
        /** @todo remove */
        void setEPInstallationRequested(bool fRequested) { m_fEPInstallationRequested = fRequested; }
    /** @} */

    /** @name Display stuff.
     * @{ */
        /** Inner worker for lazily querying for 3D support. */
        bool is3DAvailableWorker() const;
        /** Returns whether 3D is available, runs worker above if necessary. */
        bool is3DAvailable() const;

#ifdef VBOX_WITH_CRHGSMI
        /** Returns whether guest OS type with passed @a strGuestOSTypeId is WDDM compatible. */
        static bool isWddmCompatibleOsType(const QString &strGuestOSTypeId);
#endif /* VBOX_WITH_CRHGSMI */
        /** Returns the required video memory in bytes for the current desktop
          * resolution at maximum possible screen depth in bpp. */
        static quint64 requiredVideoMemory(const QString &strGuestOSTypeId, int cMonitors = 1);
    /** @} */

    /** @name Thread stuff.
     * @{ */
        /** Returns the thread-pool instance. */
        UIThreadPool* threadPool() const { return m_pThreadPool; }
    /** @} */

    /** @name Icon/Pixmap stuff.
     * @{ */
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

        /** Returns default icon of certain @a enmType. */
        QIcon icon(QFileIconProvider::IconType type) { return m_globalIconProvider.icon(type); }
        /** Returns file icon fetched from passed file @a info. */
        QIcon icon(const QFileInfo &info) { return m_globalIconProvider.icon(info); }

        /** Returns cached default warning pixmap. */
        QPixmap warningIcon() const { return mWarningIcon; }
        /** Returns cached default error pixmap. */
        QPixmap errorIcon() const { return mErrorIcon; }

        /** Joins two pixmaps horizontally with 2px space between them and returns the result. */
        static QPixmap joinPixmaps (const QPixmap &aPM1, const QPixmap &aPM2);
    /** @} */

public slots:

    /** @name Process arguments stuff.
     * @{ */
        /** Opens the specified URL using OS/Desktop capabilities. */
        bool openURL (const QString &aURL);
    /** @} */

    /** @name Localization stuff.
     * @{ */
        /** Handles language change to new @a strLanguage. */
        void sltGUILanguageChange(QString strLang);
    /** @} */

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    bool eventFilter (QObject *, QEvent *);

    /** Handles translation event. */
    void retranslateUi();

protected slots:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** @name Common stuff.
     * @{ */
        /** Handles @a manager request for emergency session shutdown. */
        void sltHandleCommitDataRequest(QSessionManager &manager);
    /** @} */

    /** @name COM stuff.
     * @{ */
        /** Handles the VBoxSVC availability change. */
        void sltHandleVBoxSVCAvailabilityChange(bool fAvailable);
    /** @} */

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

    /** @name Common stuff.
     * @{ */
#ifdef VBOX_WS_WIN
        /** Wraps WinAPI ShutdownBlockReasonCreate function. */
        static BOOL ShutdownBlockReasonCreateAPI(HWND hWnd, LPCWSTR pwszReason);
#endif
    /** @} */

    /** @name Process arguments stuff.
     * @{ */
#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Initializes a debugger config variable.
          * @param  piDbgCfgVar       Brings the debugger config variable to init.
          * @param  pszEnvVar         Brings the environment variable name relating to this variable.
          * @param  pszExtraDataName  Brings the extra data name relating to this variable.
          * @param  fDefault          Brings the default value. */
        void initDebuggerVar(int *piDbgCfgVar, const char *pszEnvVar, const char *pszExtraDataName, bool fDefault = false);
        /** Set a debugger config variable according according to start up argument.
          * @param  piDbgCfgVar  Brings the debugger config variable to set.
          * @param  fState       Brings the value from the command line. */
        void setDebuggerVar(int *piDbgCfgVar, bool fState);
        /** Checks the state of a debugger config variable, updating it with the machine settings on the first invocation.
          * @param  piDbgCfgVar       Brings the debugger config variable to consult.
          * @param  pszExtraDataName  Brings the extra data name relating to this variable. */
        bool isDebuggerWorker(int *piDbgCfgVar, const char *pszExtraDataName) const;
#endif
    /** @} */

    /** @name COM stuff.
     * @{ */
        /** Re-initializes COM wrappers and containers. */
        void comWrappersReinit();
    /** @} */

    /** Holds the singleton VBoxGlobal instance. */
    static VBoxGlobal *s_pInstance;

    /** @name Common stuff.
     * @{ */
        /** Holds whether VBoxGlobal cleanup is in progress. */
        static bool        s_fCleanupInProgress;

        /** Holds the currently loaded language ID. */
        static QString     s_strLoadedLanguageId;

#ifdef VBOX_GUI_WITH_SHARED_LIBRARY
        /** Holds the UI type. */
        UIType m_enmType;
#endif

        /** Holds whether VBoxGlobal instance is properly initialized. */
        bool mValid;

#ifdef VBOX_WS_MAC
        /** Mac OS X: Holds the #MacOSXRelease determined using <i>uname</i> call. */
        MacOSXRelease m_osRelease;
#endif /* VBOX_WS_MAC */

#ifdef VBOX_WS_X11
        /** X11: Holds the #X11WMType of the Window Manager we are running under. */
        X11WMType m_enmWindowManagerType;
        /** X11: Holds whether the Window Manager we are running at is composition one. */
        bool m_fCompositingManagerRunning;
#endif /* VBOX_WS_X11 */

        /** Holds the VBox branding config file path. */
        QString mBrandingConfig;
    /** @} */

    /** @name Process arguments stuff.
     * @{ */
        /** Holds the URL arguments list. */
        QList<QUrl> m_ArgUrlList;

        /** Holds the --startvm option value (managed VM id). */
        QString vmUuid;
        /** Holds the --separate option value (whether GUI process is separate from VM process). */
        bool m_fSeparateProcess;
        /** Holds the --no-startvm-errormsgbox option value (whether startup VM errors are disabled). */
        bool mShowStartVMErrors;

        /** Holds the --aggressive-caching / --no-aggressive-caching option value (whether medium-enumeration is required). */
        bool mAgressiveCaching;

        /** Holds the --restore-current option value. */
        bool mRestoreCurrentSnapshot;

        /** Holds the --fda option value (floppy image). */
        QString m_strFloppyImage;
        /** Holds the --dvd | --cdrom option value (DVD image). */
        QString m_strDvdImage;

        /** Holds the --disable-patm option value. */
        bool mDisablePatm;
        /** Holds the --disable-csam option value. */
        bool mDisableCsam;
        /** Holds the --recompile-supervisor option value. */
        bool mRecompileSupervisor;
        /** Holds the --recompile-user option value. */
        bool mRecompileUser;
        /** Holds the --execute-all-in-iem option value. */
        bool mExecuteAllInIem;
        /** Holds the --warp-factor option value. */
        uint32_t mWarpPct;

#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Holds whether the debugger should be accessible. */
        mutable int m_fDbgEnabled;
        /** Holds whether to show the debugger automatically with the console. */
        mutable int m_fDbgAutoShow;
        /** Holds whether to show the command line window when m_fDbgAutoShow is set. */
        mutable int m_fDbgAutoShowCommandLine;
        /** Holds whether to show the statistics window when m_fDbgAutoShow is set. */
        mutable int m_fDbgAutoShowStatistics;
        /** VBoxDbg module handle. */
        RTLDRMOD m_hVBoxDbg;

        /** Holds whether --start-running, --start-paused or nothing was given. */
        enum StartRunning m_enmStartRunning;
#endif

        /** Holds the --settingspw option value. */
        char mSettingsPw[256];
        /** Holds the --settingspwfile option value. */
        bool mSettingsPwSet;

#ifdef VBOX_GUI_WITH_PIDFILE
        /** Holds the --pidfile option value (application PID file path). */
        QString m_strPidfile;
#endif

        /** @todo remove */
        QString mUserDefinedPortName;
    /** @} */

    /** @name COM stuff.
     * @{ */
        /** Holds the COM cleanup protection token. */
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
    /** @} */

    /** @name Display stuff.
     * @{ */
        /** Holds whether 3D is available. */
        int m3DAvailable;
    /** @} */

    /** @name Thread stuff.
     * @{ */
        /** Holds the thread-pool instance. */
        UIThreadPool *m_pThreadPool;
    /** @} */

    /** @name Icon/Pixmap stuff.
     * @{ */
        /** Holds the general icon-pool instance. */
        UIIconPoolGeneral *m_pIconPool;

        /** Holds the global file icon provider instance. */
        QFileIconProvider m_globalIconProvider;

        /** Holds the warning pixmap. */
        QPixmap mWarningIcon;
        /** Holds the error pixmap. */
        QPixmap mErrorIcon;
    /** @} */

    /** @name Media related stuff.
     * @{ */
        /** Holds the medium enumerator cleanup protection token. */
        mutable QReadWriteLock m_mediumEnumeratorDtorRwLock;

        /** Holds the medium enumerator. */
        UIMediumEnumerator *m_pMediumEnumerator;
    /** @} */

    /** @todo remove */
    bool m_fEPInstallationRequested;

#if defined(VBOX_WS_WIN) && defined(VBOX_GUI_WITH_SHARED_LIBRARY)
    /** @name ATL stuff.
     * @{ */
        /** Holds the ATL module instance (for use with VBoxGlobal shared library only).
          * @note  Required internally by ATL (constructor records instance in global variable). */
        ATL::CComModule _Module;
    /** @} */
#endif

    /** Allows for shortcut access. */
    friend VBoxGlobal &vboxGlobal();
};

/** Singleton VBoxGlobal 'official' name. */
inline VBoxGlobal &vboxGlobal() { return *VBoxGlobal::instance(); }

#endif /* !___VBoxGlobal_h___ */
