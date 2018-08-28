/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxGlobal class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDesktopServices>
# include <QDir>
# include <QFileDialog>
# include <QGraphicsWidget>
# include <QLocale>
# include <QMenu>
# include <QMutex>
# include <QPainter>
# include <QProcess>
# include <QSpinBox>
# include <QStandardPaths>
# include <QThread>
# include <QTimer>
# include <QToolButton>
# include <QToolTip>
# include <QTranslator>
# ifdef VBOX_WS_WIN
#  include <QEventLoop>
# endif /* VBOX_WS_WIN */
# ifdef VBOX_WS_X11
#  include <QScrollBar>
#  include <QTextBrowser>
#  include <QX11Info>
# endif /* VBOX_WS_X11 */
# ifdef VBOX_GUI_WITH_PIDFILE
#  include <QTextStream>
# endif /* VBOX_GUI_WITH_PIDFILE */
# ifdef VBOX_WS_X11
#  include <QScreen>
# endif

/* GUI includes: */
# include "VBoxGlobal.h"
# include "VBoxLicenseViewer.h"
# include "UIMessageCenter.h"
# include "UIPopupCenter.h"
# include "QIMessageBox.h"
# include "QIDialogButtonBox.h"
# include "UIFDCreationDialog.h"
# include "UIIconPool.h"
# include "UIThreadPool.h"
# include "UIShortcutPool.h"
# include "UIExtraDataManager.h"
# include "QIFileDialog.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UINetworkManager.h"
#  include "UIUpdateManager.h"
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
# include "UIConverter.h"
# include "UIMediumEnumerator.h"
# include "UIMedium.h"
# include "UIModalWindowManager.h"
# include "UIIconPool.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIDesktopWidgetWatchdog.h"
# ifdef VBOX_WS_X11
#  include "UIHostComboEditor.h"
#  include "VBoxX11Helper.h"
# endif
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
#  include "UIMachineWindowFullscreen.h"
#  include "UIMachineWindowSeamless.h"
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CExtPack.h"
# include "CExtPackFile.h"
# include "CExtPackManager.h"
# include "CMachine.h"
# include "CUSBDevice.h"
# include "CUSBDeviceFilters.h"
# include "CUSBDeviceFilter.h"
# include "CBIOSSettings.h"
# include "CVRDEServer.h"
# include "CStorageController.h"
# include "CMediumAttachment.h"
# include "CAudioAdapter.h"
# include "CNetworkAdapter.h"
# include "CSerialPort.h"
# include "CUSBController.h"
# include "CHostUSBDevice.h"
# include "CHostVideoInputDevice.h"
# include "CSharedFolder.h"
# include "CConsole.h"
# include "CSnapshot.h"

/* Other VBox includes: */
# include <iprt/asm.h>
# include <iprt/env.h>
# include <iprt/getopt.h>
# include <iprt/ldr.h>
# include <iprt/param.h>
# include <iprt/path.h>
# include <iprt/system.h>
# include <iprt/stream.h>
# ifdef VBOX_WS_X11
#  include <iprt/mem.h>
# endif /* VBOX_WS_X11 */
# include <VBox/sup.h>
# include <VBox/com/Guid.h>

/* External includes: */
# ifdef VBOX_WS_WIN
#  include <iprt/win/shlobj.h>
# endif
# ifdef VBOX_WS_X11
#  include <xcb/xcb.h>
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* VirtualBox interface declarations: */
#ifndef VBOX_WITH_XPCOM
# include "VirtualBox.h"
#else /* !VBOX_WITH_XPCOM */
# include "VirtualBox_XPCOM.h"
#endif /* VBOX_WITH_XPCOM */

/* Qt includes: */
#include <QLibraryInfo>
#include <QProgressDialog>
#include <QSettings>
#include <QStyleOptionSpinBox>
#include <QSessionManager>

/* Other VBox includes: */
#include <VBox/VBoxOGL.h>
#include <VBox/vd.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>

/* External includes: */
# include <math.h>
#ifdef VBOX_WS_MAC
# include <sys/utsname.h>
#endif /* VBOX_WS_MAC */
#ifdef VBOX_WS_X11
// WORKAROUND:
// typedef CARD8 BOOL in Xmd.h conflicts with #define BOOL PRBool
// in COMDefs.h. A better fix would be to isolate X11-specific
// stuff by placing XX* helpers below to a separate source file.
# undef BOOL
# include <X11/X.h>
# include <X11/Xmd.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/extensions/Xinerama.h>
# define BOOL PRBool
#endif /* VBOX_WS_X11 */

//#define VBOX_WITH_FULL_DETAILS_REPORT /* hidden for now */

/* Namespaces: */
using namespace UIExtraDataDefs;
using namespace UIMediumDefs;


/** QTranslator subclass for VBox needs. */
class VBoxTranslator : public QTranslator
{
public:

    /** Constructs translator passing @a pParent to the base-class. */
    VBoxTranslator(QObject *pParent = 0)
        : QTranslator(pParent)
    {}

    /** Loads language file with gained @a strFileName. */
    bool loadFile(const QString &strFileName)
    {
        QFile file(strFileName);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        m_data = file.readAll();
        return load((uchar*)m_data.data(), m_data.size());
    }

private:

    /** Holds the loaded data. */
    QByteArray m_data;
};

/** Holds the static #VBoxTranslator instance. */
static VBoxTranslator *sTranslator = 0;


/** Port config cache. */
struct PortConfig
{
    const char *name;
    const ulong IRQ;
    const ulong IOBase;
};

/** Known port config COM ports. */
static const PortConfig kComKnownPorts[] =
{
    { "COM1", 4, 0x3F8 },
    { "COM2", 3, 0x2F8 },
    { "COM3", 4, 0x3E8 },
    { "COM4", 3, 0x2E8 },
    /* Must not contain an element with IRQ=0 and IOBase=0 used to cause
     * toCOMPortName() to return the "User-defined" string for these values. */
};

/** Known port config LPT ports. */
static const PortConfig kLptKnownPorts[] =
{
    { "LPT1", 7, 0x378 },
    { "LPT2", 5, 0x278 },
    { "LPT1", 2, 0x3BC },
    /* Must not contain an element with IRQ=0 and IOBase=0 used to cause
     * toLPTPortName() to return the "User-defined" string for these values. */
};


/* static */
VBoxGlobal *VBoxGlobal::s_pInstance = 0;
bool        VBoxGlobal::s_fCleaningUp = false;
QString     VBoxGlobal::s_strLoadedLanguageId = vboxBuiltInLanguageName();
QString     VBoxGlobal::s_strUserDefinedPortName = QString();

/* static */
#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
void VBoxGlobal::create()
#else
void VBoxGlobal::create(UIType enmType)
#endif
{
    /* Make sure instance is NOT created yet: */
    if (s_pInstance)
    {
        AssertMsgFailed(("VBoxGlobal instance is already created!"));
        return;
    }

#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
    /* Create instance: */
    new VBoxGlobal;
#else
    /* Create instance: */
    new VBoxGlobal(enmType);
#endif
    /* Prepare instance: */
    s_pInstance->prepare();
}

/* static */
void VBoxGlobal::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!s_pInstance)
    {
        AssertMsgFailed(("VBoxGlobal instance is already destroyed!"));
        return;
    }

    /* Cleanup instance:
     * 1. By default, automatically on QApplication::aboutToQuit() signal.
     * 2. But if QApplication was not started at all and we perform
     *    early shutdown, we should do cleanup ourselves. */
    if (s_pInstance->isValid())
        s_pInstance->cleanup();
    /* Destroy instance: */
    delete s_pInstance;
}

#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
VBoxGlobal::VBoxGlobal()
    : m_fValid(false)
#else
VBoxGlobal::VBoxGlobal(UIType enmType)
    : m_enmType(enmType)
    , m_fValid(false)
#endif
#ifdef VBOX_WS_MAC
    , m_enmMacOSVersion(MacOSXRelease_Old)
#endif
#ifdef VBOX_WS_X11
    , m_enmWindowManagerType(X11WMType_Unknown)
    , m_fCompositingManagerRunning(false)
#endif
    , m_fSeparateProcess(false)
    , m_fShowStartVMErrors(true)
#if defined(DEBUG_bird)
    , m_fAgressiveCaching(false)
#else
    , m_fAgressiveCaching(true)
#endif
    , m_fRestoreCurrentSnapshot(false)
    , m_fDisablePatm(false)
    , m_fDisableCsam(false)
    , m_fRecompileSupervisor(false)
    , m_fRecompileUser(false)
    , m_fExecuteAllInIem(false)
    , m_uWarpPct(100)
#ifdef VBOX_WITH_DEBUGGER_GUI
    , m_fDbgEnabled(0)
    , m_fDbgAutoShow(0)
    , m_fDbgAutoShowCommandLine(0)
    , m_fDbgAutoShowStatistics(0)
    , m_hVBoxDbg(NIL_RTLDRMOD)
    , m_enmLaunchRunning(LaunchRunning_Default)
#endif
    , m_fSettingsPwSet(false)
    , m_fWrappersValid(false)
    , m_fVBoxSVCAvailable(true)
    , m_i3DAvailable(-1)
    , m_pThreadPool(0)
    , m_pIconPool(0)
    , m_pMediumEnumerator(0)
{
    /* Assign instance: */
    s_pInstance = this;
}

VBoxGlobal::~VBoxGlobal()
{
    /* Unassign instance: */
    s_pInstance = 0;
}

/* static */
QString VBoxGlobal::qtRTVersionString()
{
    return QString::fromLatin1(qVersion());
}

/* static */
uint VBoxGlobal::qtRTVersion()
{
    const QString strVersionRT = VBoxGlobal::qtRTVersionString();
    return (strVersionRT.section('.', 0, 0).toInt() << 16) +
           (strVersionRT.section('.', 1, 1).toInt() << 8) +
           strVersionRT.section('.', 2, 2).toInt();
}

/* static */
uint VBoxGlobal::qtRTMajorVersion()
{
    return VBoxGlobal::qtRTVersionString().section('.', 0, 0).toInt();
}

/* static */
uint VBoxGlobal::qtRTMinorVersion()
{
    return VBoxGlobal::qtRTVersionString().section('.', 1, 1).toInt();
}

/* static */
uint VBoxGlobal::qtRTRevisionNumber()
{
    return VBoxGlobal::qtRTVersionString().section('.', 2, 2).toInt();
}

/* static */
QString VBoxGlobal::qtCTVersionString()
{
    return QString::fromLatin1(QT_VERSION_STR);
}

/* static */
uint VBoxGlobal::qtCTVersion()
{
    const QString strVersionCompiled = VBoxGlobal::qtCTVersionString();
    return (strVersionCompiled.section('.', 0, 0).toInt() << 16) +
           (strVersionCompiled.section('.', 1, 1).toInt() << 8) +
           strVersionCompiled.section('.', 2, 2).toInt();
}

QString VBoxGlobal::vboxVersionString() const
{
    return m_comVBox.GetVersion();
}

QString VBoxGlobal::vboxVersionStringNormalized() const
{
    return m_comVBox.GetVersionNormalized();
}

bool VBoxGlobal::isBeta() const
{
    return vboxVersionString().contains("BETA", Qt::CaseInsensitive);
}

#ifdef VBOX_WS_MAC
/* static */
MacOSXRelease VBoxGlobal::determineOsRelease()
{
    /* Prepare 'utsname' struct: */
    utsname info;
    if (uname(&info) != -1)
    {
        /* Compose map of known releases: */
        QMap<int, MacOSXRelease> release;
        release[10] = MacOSXRelease_SnowLeopard;
        release[11] = MacOSXRelease_Lion;
        release[12] = MacOSXRelease_MountainLion;
        release[13] = MacOSXRelease_Mavericks;
        release[14] = MacOSXRelease_Yosemite;
        release[15] = MacOSXRelease_ElCapitan;

        /* Cut the major release index of the string we have, s.a. 'man uname': */
        const int iRelease = QString(info.release).section('.', 0, 0).toInt();

        /* Return release if determined, return 'New' if version more recent than latest, return 'Old' otherwise: */
        return release.value(iRelease, iRelease > release.keys().last() ? MacOSXRelease_New : MacOSXRelease_Old);
    }
    /* Return 'Old' by default: */
    return MacOSXRelease_Old;
}
#endif /* VBOX_WS_MAC */

bool VBoxGlobal::brandingIsActive(bool fForce /* = false */)
{
    if (fForce)
        return true;

    if (m_strBrandingConfigFilePath.isEmpty())
    {
        m_strBrandingConfigFilePath = QDir(QApplication::applicationDirPath()).absolutePath();
        m_strBrandingConfigFilePath += "/custom/custom.ini";
    }

    return QFile::exists(m_strBrandingConfigFilePath);
}

QString VBoxGlobal::brandingGetKey(QString strKey)
{
    QSettings settings(m_strBrandingConfigFilePath, QSettings::IniFormat);
    return settings.value(QString("%1").arg(strKey)).toString();
}

/* static */
bool VBoxGlobal::hasAllowedExtension(const QString &strExt, const QStringList &extList)
{
    for (int i = 0; i < extList.size(); ++i)
        if (strExt.endsWith(extList.at(i), Qt::CaseInsensitive))
            return true;
    return false;
}

bool VBoxGlobal::processArgs()
{
    /* Among those arguments: */
    bool fResult = false;
    const QStringList args = qApp->arguments();

    /* We are looking for a list of file URLs passed to the executable: */
    QList<QUrl> listArgUrls;
    for (int i = 1; i < args.size(); ++i)
    {
        /* But we break out after the first parameter, cause there
         * could be parameters with arguments (e.g. --comment comment). */
        if (args.at(i).startsWith("-"))
            break;

#ifdef VBOX_WS_MAC
        const QString strArg = ::darwinResolveAlias(args.at(i));
#else
        const QString strArg = args.at(i);
#endif

        /* So if the argument file exists, we add it to URL list: */
        if (   !strArg.isEmpty()
            && QFile::exists(strArg))
            listArgUrls << QUrl::fromLocalFile(strArg);
    }

    /* If there are file URLs: */
    if (!listArgUrls.isEmpty())
    {
        /* We enumerate them and: */
        for (int i = 0; i < listArgUrls.size(); ++i)
        {
            /* Check which of them has allowed VM extensions: */
            const QString &strFile = listArgUrls.at(i).toLocalFile();
            if (VBoxGlobal::hasAllowedExtension(strFile, VBoxFileExts))
            {
                /* So that we could run existing VMs: */
                CVirtualBox comVBox = virtualBox();
                CMachine comMachine = comVBox.FindMachine(strFile);
                if (!comMachine.isNull())
                {
                    fResult = true;
                    launchMachine(comMachine);
                    /* And remove their URLs from the ULR list: */
                    listArgUrls.removeAll(strFile);
                }
            }
        }
    }

    /* And if there are *still* URLs: */
    if (!listArgUrls.isEmpty())
    {
        /* We store them: */
        m_listArgUrls = listArgUrls;
        /* And ask UIStarter to open them: */
        emit sigAskToOpenURLs();
    }

    return fResult;
}

#ifdef VBOX_WITH_DEBUGGER_GUI

bool VBoxGlobal::isDebuggerEnabled() const
{
    return isDebuggerWorker(&m_fDbgEnabled, GUI_Dbg_Enabled);
}

bool VBoxGlobal::isDebuggerAutoShowEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShow, GUI_Dbg_AutoShow);
}

bool VBoxGlobal::isDebuggerAutoShowCommandLineEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShowCommandLine, GUI_Dbg_AutoShow);
}

bool VBoxGlobal::isDebuggerAutoShowStatisticsEnabled() const
{
    return isDebuggerWorker(&m_fDbgAutoShowStatistics, GUI_Dbg_AutoShow);
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

bool VBoxGlobal::shouldStartPaused() const
{
#ifdef VBOX_WITH_DEBUGGER_GUI
    return m_enmLaunchRunning == LaunchRunning_Default ? isDebuggerAutoShowEnabled() : m_enmLaunchRunning == LaunchRunning_No;
#else
    return false;
#endif
}

#ifdef VBOX_GUI_WITH_PIDFILE

void VBoxGlobal::createPidfile()
{
    if (!m_strPidFile.isEmpty())
    {
        const qint64 iPid = qApp->applicationPid();
        QFile file(m_strPidFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
             QTextStream out(&file);
             out << iPid << endl;
        }
        else
            LogRel(("Failed to create pid file %s\n", m_strPidFile.toUtf8().constData()));
    }
}

void VBoxGlobal::deletePidfile()
{
    if (   !m_strPidFile.isEmpty()
        && QFile::exists(m_strPidFile))
        QFile::remove(m_strPidFile);
}

#endif /* VBOX_GUI_WITH_PIDFILE */

/* static */
QString VBoxGlobal::languageName()
{
    /* Returns "English" if no translation is installed
     * or if the translation file is invalid. */
    return QApplication::translate("@@@", "English",
                                   "Native language name");
}

/* static */
QString VBoxGlobal::languageCountry()
{
    /* Returns "--" if no translation is installed or if the translation file
     * is invalid, or if the language is independent on the country. */
    return QApplication::translate("@@@", "--",
                                   "Native language country name "
                                   "(empty if this language is for all countries)");
}

/* static */
QString VBoxGlobal::languageNameEnglish()
{
    /* Returns "English" if no translation is installed
     * or if the translation file is invalid. */
    return QApplication::translate("@@@", "English",
                                   "Language name, in English");
}

/* static */
QString VBoxGlobal::languageCountryEnglish()
{
    /* Returns "--" if no translation is installed or if the translation file
     * is invalid, or if the language is independent on the country. */
    return QApplication::translate("@@@", "--",
                                   "Language country name, in English "
                                   "(empty if native country name is empty)");
}

/* static */
QString VBoxGlobal::languageTranslators()
{
    /* Returns "Oracle Corporation" if no translation is installed or if the translation file
     * is invalid, or if the translation is supplied by Oracle Corporation. */
    return QApplication::translate("@@@", "Oracle Corporation",
                                   "Comma-separated list of translators");
}

/* static */
QString VBoxGlobal::vboxLanguageSubDirectory()
{
    return "/nls";
}

/* static */
QString VBoxGlobal::vboxLanguageFileBase()
{
    return "VirtualBox_";
}

/* static */
QString VBoxGlobal::vboxLanguageFileExtension()
{
    return ".qm";
}

/* static */
QString VBoxGlobal::vboxLanguageIdRegExp()
{
    return "(([a-z]{2})(?:_([A-Z]{2}))?)|(C)";
}

/* static */
QString VBoxGlobal::vboxBuiltInLanguageName()
{
    return "C";
}

/* static */
QString VBoxGlobal::languageId()
{
    /* Note that it may not match with UIExtraDataManager::languageId() if the specified language cannot be loaded.
     *
     * If the built-in language is active, this method returns "C". "C" is treated as the built-in language for
     * simplicity -- the C locale is used in unix environments as a fallback when the requested locale is invalid.
     * This way we don't need to process both the "built_in" language and the "C" language (which is a valid
     * environment setting) separately. */

    return s_strLoadedLanguageId;
}

/* static */
QString VBoxGlobal::systemLanguageId()
{
    /* This does exactly the same as QLocale::system().name() but corrects its wrong behavior on Linux systems
     * (LC_NUMERIC for some strange reason takes precedence over any other locale setting in the QLocale::system()
     * implementation). This implementation first looks at LC_ALL (as defined by SUS), then looks at LC_MESSAGES
     * which is designed to define a language for program messages in case if it differs from the language for
     * other locale categories. Then it looks for LANG and finally falls back to QLocale::system().name().
     *
     * The order of precedence is well defined here:
     * http://opengroup.org/onlinepubs/007908799/xbd/envvar.html
     *
     * This method will return "C" when the requested locale is invalid or when the "C" locale is set explicitly. */

#if defined(VBOX_WS_MAC)
    /* QLocale return the right id only if the user select the format
     * of the language also. So we use our own implementation */
    return ::darwinSystemLanguage();
#elif defined(Q_OS_UNIX)
    const char *pszValue = RTEnvGet("LC_ALL");
    if (pszValue == 0)
        pszValue = RTEnvGet("LC_MESSAGES");
    if (pszValue == 0)
        pszValue = RTEnvGet("LANG");
    if (pszValue != 0)
        return QLocale(pszValue).name();
#endif
    return QLocale::system().name();
}

/* static */
void VBoxGlobal::loadLanguage(const QString &strLangId)
{
    QString strEffectiveLangId = strLangId.isEmpty()
                               ? VBoxGlobal::systemLanguageId()
                               : strLangId;
    QString strLanguageFileName;
    QString strSelectedLangId = vboxBuiltInLanguageName();

    /* If C is selected we change it temporary to en. This makes sure any extra
     * "en" translation file will be loaded. This is necessary for loading the
     * plural forms of some of our translations. */
    bool fResetToC = false;
    if (strEffectiveLangId == "C")
    {
        strEffectiveLangId = "en";
        fResetToC = true;
    }

    char szNlsPath[RTPATH_MAX];
    int rc;

    rc = RTPathAppPrivateNoArch(szNlsPath, sizeof(szNlsPath));
    AssertRC(rc);

    QString strNlsPath = QString(szNlsPath) + vboxLanguageSubDirectory();
    QDir nlsDir(strNlsPath);

    Assert(!strEffectiveLangId.isEmpty());
    if (!strEffectiveLangId.isEmpty() && strEffectiveLangId != vboxBuiltInLanguageName())
    {
        QRegExp regExp(vboxLanguageIdRegExp());
        int iPos = regExp.indexIn(strEffectiveLangId);
        /* The language ID should match the regexp completely: */
        AssertReturnVoid(iPos == 0);

        QString strStrippedLangId = regExp.cap(2);

        if (nlsDir.exists(vboxLanguageFileBase() + strEffectiveLangId + vboxLanguageFileExtension()))
        {
            strLanguageFileName = nlsDir.absoluteFilePath(vboxLanguageFileBase() +
                                                          strEffectiveLangId +
                                                          vboxLanguageFileExtension());
            strSelectedLangId = strEffectiveLangId;
        }
        else if (nlsDir.exists(vboxLanguageFileBase() + strStrippedLangId + vboxLanguageFileExtension()))
        {
            strLanguageFileName = nlsDir.absoluteFilePath(vboxLanguageFileBase() +
                                                          strStrippedLangId +
                                                          vboxLanguageFileExtension());
            strSelectedLangId = strStrippedLangId;
        }
        else
        {
            /* Never complain when the default language is requested.  In any
             * case, if no explicit language file exists, we will simply
             * fall-back to English (built-in). */
            if (!strLangId.isNull() && strEffectiveLangId != "en")
                msgCenter().cannotFindLanguage(strEffectiveLangId, strNlsPath);
            /* strSelectedLangId remains built-in here: */
            AssertReturnVoid(strSelectedLangId == vboxBuiltInLanguageName());
        }
    }

    /* Delete the old translator if there is one: */
    if (sTranslator)
    {
        /* QTranslator destructor will call qApp->removeTranslator() for
         * us. It will also delete all its child translations we attach to it
         * below, so we don't have to care about them specially. */
        delete sTranslator;
    }

    /* Load new language files: */
    sTranslator = new VBoxTranslator(qApp);
    Assert(sTranslator);
    bool fLoadOk = true;
    if (sTranslator)
    {
        if (strSelectedLangId != vboxBuiltInLanguageName())
        {
            Assert(!strLanguageFileName.isNull());
            fLoadOk = sTranslator->loadFile(strLanguageFileName);
        }
        /* We install the translator in any case: on failure, this will
         * activate an empty translator that will give us English (built-in): */
        qApp->installTranslator(sTranslator);
    }
    else
        fLoadOk = false;

    if (fLoadOk)
        s_strLoadedLanguageId = strSelectedLangId;
    else
    {
        msgCenter().cannotLoadLanguage(strLanguageFileName);
        s_strLoadedLanguageId = vboxBuiltInLanguageName();
    }

    /* Try to load the corresponding Qt translation: */
    if (languageId() != vboxBuiltInLanguageName() && languageId() != "en")
    {
#ifdef Q_OS_UNIX
        /* We use system installations of Qt on Linux systems, so first, try
         * to load the Qt translation from the system location. */
        strLanguageFileName = QLibraryInfo::location(QLibraryInfo::TranslationsPath) + "/qt_" +
                              languageId() + vboxLanguageFileExtension();
        QTranslator *pQtSysTr = new QTranslator(sTranslator);
        Assert(pQtSysTr);
        if (pQtSysTr && pQtSysTr->load(strLanguageFileName))
            qApp->installTranslator(pQtSysTr);
        /* Note that the Qt translation supplied by Oracle is always loaded
         * afterwards to make sure it will take precedence over the system
         * translation (it may contain more decent variants of translation
         * that better correspond to VirtualBox UI). We need to load both
         * because a newer version of Qt may be installed on the user computer
         * and the Oracle version may not fully support it. We don't do it on
         * Win32 because we supply a Qt library there and therefore the
         * Oracle translation is always the best one. */
#endif
        strLanguageFileName = nlsDir.absoluteFilePath(QString("qt_") +
                                                      languageId() +
                                                      vboxLanguageFileExtension());
        QTranslator *pQtTr = new QTranslator(sTranslator);
        Assert(pQtTr);
        if (pQtTr && (fLoadOk = pQtTr->load(strLanguageFileName)))
            qApp->installTranslator(pQtTr);
        /* The below message doesn't fit 100% (because it's an additional
         * language and the main one won't be reset to built-in on failure)
         * but the load failure is so rare here that it's not worth a separate
         * message (but still, having something is better than having none) */
        if (!fLoadOk && !strLangId.isNull())
            msgCenter().cannotLoadLanguage(strLanguageFileName);
    }
    if (fResetToC)
        s_strLoadedLanguageId = vboxBuiltInLanguageName();
#ifdef VBOX_WS_MAC
    /* Qt doesn't translate the items in the Application menu initially.
     * Manually trigger an update. */
    ::darwinRetranslateAppMenu();
#endif
}

/* static */
QString VBoxGlobal::yearsToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n year(s)", "", cVal);
}

/* static */
QString VBoxGlobal::monthsToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n month(s)", "", cVal);
}

/* static */
QString VBoxGlobal::daysToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n day(s)", "", cVal);
}

/* static */
QString VBoxGlobal::hoursToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n hour(s)", "", cVal);
}

/* static */
QString VBoxGlobal::minutesToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n minute(s)", "", cVal);
}

/* static */
QString VBoxGlobal::secondsToString(uint32_t cVal)
{
    return QApplication::translate("VBoxGlobal", "%n second(s)", "", cVal);
}

/* static */
QChar VBoxGlobal::decimalSep()
{
    return QLocale::system().decimalPoint();
}

/* static */
QString VBoxGlobal::sizeRegexp()
{
    /* This regexp will capture 5 groups of text:
     * - cap(1): integer number in case when no decimal point is present
     *           (if empty, it means that decimal point is present)
     * - cap(2): size suffix in case when no decimal point is present (may be empty)
     * - cap(3): integer number in case when decimal point is present (may be empty)
     * - cap(4): fraction number (hundredth) in case when decimal point is present
     * - cap(5): size suffix in case when decimal point is present (note that
     *           B cannot appear there). */

    const QString strRegexp =
        QString("^(?:(?:(\\d+)(?:\\s?(%2|%3|%4|%5|%6|%7))?)|(?:(\\d*)%1(\\d{1,2})(?:\\s?(%3|%4|%5|%6|%7))))$")
            .arg(decimalSep())
            .arg(tr("B", "size suffix Bytes"))
            .arg(tr("KB", "size suffix KBytes=1024 Bytes"))
            .arg(tr("MB", "size suffix MBytes=1024 KBytes"))
            .arg(tr("GB", "size suffix GBytes=1024 MBytes"))
            .arg(tr("TB", "size suffix TBytes=1024 GBytes"))
            .arg(tr("PB", "size suffix PBytes=1024 TBytes"));
    return strRegexp;
}

/* static */
quint64 VBoxGlobal::parseSize(const QString &strText)
{
    /* Text should be in form of B|KB|MB|GB|TB|PB. */
    QRegExp regexp(sizeRegexp());
    int iPos = regexp.indexIn(strText);
    if (iPos != -1)
    {
        QString strInteger = regexp.cap(1);
        QString strHundred;
        QString strSuff = regexp.cap(2);
        if (strInteger.isEmpty())
        {
            strInteger = regexp.cap(3);
            strHundred = regexp.cap(4);
            strSuff = regexp.cap(5);
        }

        quint64 uDenominator = 0;
        if (strSuff.isEmpty() || strSuff == tr("B", "size suffix Bytes"))
            uDenominator = 1;
        else if (strSuff == tr("KB", "size suffix KBytes=1024 Bytes"))
            uDenominator = _1K;
        else if (strSuff == tr("MB", "size suffix MBytes=1024 KBytes"))
            uDenominator = _1M;
        else if (strSuff == tr("GB", "size suffix GBytes=1024 MBytes"))
            uDenominator = _1G;
        else if (strSuff == tr("TB", "size suffix TBytes=1024 GBytes"))
            uDenominator = _1T;
        else if (strSuff == tr("PB", "size suffix PBytes=1024 TBytes"))
            uDenominator = _1P;

        quint64 iInteger = strInteger.toULongLong();
        if (uDenominator == 1)
            return iInteger;

        quint64 iHundred = strHundred.leftJustified(2, '0').toULongLong();
        iHundred = iHundred * uDenominator / 100;
        iInteger = iInteger * uDenominator + iHundred;
        return iInteger;
    }
    else
        return 0;
}

/* static */
QString VBoxGlobal::formatSize(quint64 uSize, uint cDecimal /* = 2 */,
                               FormatSize enmMode /* = FormatSize_Round */)
{
    /* Text will be in form of B|KB|MB|GB|TB|PB.
     *
     * When enmMode is FormatSize_Round, the result is rounded to the
     *              closest number containing @a aDecimal decimal digits.
     * When enmMode is FormatSize_RoundDown, the result is rounded to the
     *              largest number with @a aDecimal decimal digits that is not greater than
     *              the result. This guarantees that converting the resulting string back to
     *              the integer value in bytes will not produce a value greater that the
     *              initial size parameter.
     * When enmMode is FormatSize_RoundUp, the result is rounded to the
     *              smallest number with @a aDecimal decimal digits that is not less than the
     *              result. This guarantees that converting the resulting string back to the
     *              integer value in bytes will not produce a value less that the initial
     *              size parameter. */

    quint64 uDenominator = 0;
    int iSuffix = 0;

    if (uSize < _1K)
    {
        uDenominator = 1;
        iSuffix = 0;
    }
    else if (uSize < _1M)
    {
        uDenominator = _1K;
        iSuffix = 1;
    }
    else if (uSize < _1G)
    {
        uDenominator = _1M;
        iSuffix = 2;
    }
    else if (uSize < _1T)
    {
        uDenominator = _1G;
        iSuffix = 3;
    }
    else if (uSize < _1P)
    {
        uDenominator = _1T;
        iSuffix = 4;
    }
    else
    {
        uDenominator = _1P;
        iSuffix = 5;
    }

    quint64 uInteger = uSize / uDenominator;
    quint64 uDecimal = uSize % uDenominator;
    quint64 uMult = 1;
    for (uint i = 0; i < cDecimal; ++i)
        uMult *= 10;

    QString strNumber;
    if (uDenominator > 1)
    {
        if (uDecimal)
        {
            uDecimal *= uMult;
            /* Not greater: */
            if (enmMode == FormatSize_RoundDown)
                uDecimal = uDecimal / uDenominator;
            /* Not less: */
            else if (enmMode == FormatSize_RoundUp)
                uDecimal = (uDecimal + uDenominator - 1) / uDenominator;
            /* Nearest: */
            else
                uDecimal = (uDecimal + uDenominator / 2) / uDenominator;
        }
        /* Check for the fractional part overflow due to rounding: */
        if (uDecimal == uMult)
        {
            uDecimal = 0;
            ++uInteger;
            /* Check if we've got 1024 XB after rounding and scale down if so: */
            if (uInteger == 1024 && iSuffix + 1 < (int)SizeSuffix_Max)
            {
                uInteger /= 1024;
                ++iSuffix;
            }
        }
        strNumber = QString::number(uInteger);
        if (cDecimal)
            strNumber += QString("%1%2").arg(decimalSep())
                                        .arg(QString::number(uDecimal).rightJustified(cDecimal, '0'));
    }
    else
    {
        strNumber = QString::number(uInteger);
    }

    return QString("%1 %2").arg(strNumber).arg(gpConverter->toString(static_cast<SizeSuffix>(iSuffix)));
}

/* static */
QStringList VBoxGlobal::COMPortNames()
{
    QStringList list;
    for (size_t i = 0; i < RT_ELEMENTS(kComKnownPorts); ++i)
        list << kComKnownPorts[i].name;

    return list;
}

/* static */
QString VBoxGlobal::toCOMPortName(ulong uIRQ, ulong uIOBase)
{
    for (size_t i = 0; i < RT_ELEMENTS(kComKnownPorts); ++i)
        if (kComKnownPorts[i].IRQ == uIRQ &&
            kComKnownPorts[i].IOBase == uIOBase)
            return kComKnownPorts[i].name;

    return s_strUserDefinedPortName;
}

/* static */
bool VBoxGlobal::toCOMPortNumbers(const QString &strName, ulong &uIRQ, ulong &uIOBase)
{
    for (size_t i = 0; i < RT_ELEMENTS(kComKnownPorts); ++i)
        if (strcmp(kComKnownPorts[i].name, strName.toUtf8().data()) == 0)
        {
            uIRQ = kComKnownPorts[i].IRQ;
            uIOBase = kComKnownPorts[i].IOBase;
            return true;
        }

    return false;
}

/* static */
QStringList VBoxGlobal::LPTPortNames()
{
    QStringList list;
    for (size_t i = 0; i < RT_ELEMENTS(kLptKnownPorts); ++i)
        list << kLptKnownPorts[i].name;

    return list;
}

/* static */
QString VBoxGlobal::toLPTPortName(ulong uIRQ, ulong uIOBase)
{
    for (size_t i = 0; i < RT_ELEMENTS(kLptKnownPorts); ++i)
        if (kLptKnownPorts[i].IRQ == uIRQ &&
            kLptKnownPorts[i].IOBase == uIOBase)
            return kLptKnownPorts[i].name;

    return s_strUserDefinedPortName;
}

/* static */
bool VBoxGlobal::toLPTPortNumbers(const QString &strName, ulong &uIRQ, ulong &uIOBase)
{
    for (size_t i = 0; i < RT_ELEMENTS(kLptKnownPorts); ++i)
        if (strcmp(kLptKnownPorts[i].name, strName.toUtf8().data()) == 0)
        {
            uIRQ = kLptKnownPorts[i].IRQ;
            uIOBase = kLptKnownPorts[i].IOBase;
            return true;
        }

    return false;
}

/* static */
QString VBoxGlobal::highlight(QString strText, bool fToolTip /* = false */)
{
    /* We should reformat the input strText so that:
     * - strings in single quotes will be put inside <nobr> and marked
     *   with blue color;
     * - UUIDs be put inside <nobr> and marked
     *   with green color;
     * - replaces new line chars with </p><p> constructs to form paragraphs
     *   (note that <p\> and </p> are not appended to the beginning and to the
     *    end of the string respectively, to allow the result be appended
     *    or prepended to the existing paragraph).
     *
     *  If @a fToolTip is true, colouring is not applied, only the <nobr> tag
     *  is added. Also, new line chars are replaced with <br> instead of <p>. */

    QString strFont;
    QString uuidFont;
    QString endFont;
    if (!fToolTip)
    {
        strFont = "<font color=#0000CC>";
        uuidFont = "<font color=#008000>";
        endFont = "</font>";
    }

    /* Replace special entities, '&' -- first! */
    strText.replace('&', "&amp;");
    strText.replace('<', "&lt;");
    strText.replace('>', "&gt;");
    strText.replace('\"', "&quot;");

    /* Mark strings in single quotes with color: */
    QRegExp rx = QRegExp("((?:^|\\s)[(]?)'([^']*)'(?=[:.-!);]?(?:\\s|$))");
    rx.setMinimal(true);
    strText.replace(rx, QString("\\1%1<nobr>'\\2'</nobr>%2").arg(strFont).arg(endFont));

    /* Mark UUIDs with color: */
    strText.replace(QRegExp(
        "((?:^|\\s)[(]?)"
        "(\\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\\})"
        "(?=[:.-!);]?(?:\\s|$))"),
        QString("\\1%1<nobr>\\2</nobr>%2").arg(uuidFont).arg(endFont));

    /* Split to paragraphs at \n chars: */
    if (!fToolTip)
        strText.replace('\n', "</p><p>");
    else
        strText.replace('\n', "<br>");

    return strText;
}

/* static */
QString VBoxGlobal::emphasize(QString strText)
{
    /* We should reformat the input string @a strText so that:
     * - strings in single quotes will be put inside \<nobr\> and marked
     *   with bold style;
     * - UUIDs be put inside \<nobr\> and marked
     *   with italic style;
     * - replaces new line chars with \</p\>\<p\> constructs to form paragraphs
     *   (note that \<p\> and \</p\> are not appended to the beginning and to the
     *    end of the string respectively, to allow the result be appended
     *    or prepended to the existing paragraph). */

    QString strEmphStart("<b>");
    QString strEmphEnd("</b>");
    QString uuidEmphStart("<i>");
    QString uuidEmphEnd("</i>");

    /* Replace special entities, '&' -- first! */
    strText.replace('&', "&amp;");
    strText.replace('<', "&lt;");
    strText.replace('>', "&gt;");
    strText.replace('\"', "&quot;");

    /* Mark strings in single quotes with bold style: */
    QRegExp rx = QRegExp("((?:^|\\s)[(]?)'([^']*)'(?=[:.-!);]?(?:\\s|$))");
    rx.setMinimal(true);
    strText.replace(rx, QString("\\1%1<nobr>'\\2'</nobr>%2").arg(strEmphStart).arg(strEmphEnd));

    /* Mark UUIDs with italic style: */
    strText.replace(QRegExp(
        "((?:^|\\s)[(]?)"
        "(\\{[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\\})"
        "(?=[:.-!);]?(?:\\s|$))"),
        QString("\\1%1<nobr>\\2</nobr>%2").arg(uuidEmphStart).arg(uuidEmphEnd));

    /* Split to paragraphs at \n chars: */
    strText.replace('\n', "</p><p>");

    return strText;
}

/* static */
QString VBoxGlobal::removeAccelMark(QString strText)
{
    /* In order to support accelerators used in non-alphabet languages
     * (e.g. Japanese) that has a form of "(&<L>)" (where <L> is a latin letter),
     * this method first searches for this pattern and, if found, removes it as a
     * whole. If such a pattern is not found, then the '&' character is simply
     * removed from the string. */

    QRegExp accel("\\(&[a-zA-Z]\\)");
    int iPos = accel.indexIn(strText);
    if (iPos >= 0)
        strText.remove(iPos, accel.cap().length());
    else
    {
        iPos = strText.indexOf('&');
        if (iPos >= 0)
            strText.remove(iPos, 1);
    }

    return strText;
}

/* static */
QString VBoxGlobal::insertKeyToActionText(const QString &strText, const QString &strKey)
{
#ifdef VBOX_WS_MAC
    QString strPattern("%1 (Host+%2)");
#else
    QString strPattern("%1 \tHost+%2");
#endif
    if (   strKey.isEmpty()
        || strKey.compare("None", Qt::CaseInsensitive) == 0)
        return strText;
    else
        return strPattern.arg(strText).arg(QKeySequence(strKey).toString(QKeySequence::NativeText));
}

/* static */
QString VBoxGlobal::helpFile()
{
#if defined(VBOX_WS_WIN)
    const QString strName = "VirtualBox";
    const QString strSuffix = "chm";
#elif defined(VBOX_WS_MAC)
    const QString strName = "UserManual";
    const QString strSuffix = "pdf";
#elif defined(VBOX_WS_X11)
# if defined VBOX_OSE
    const QString strName = "UserManual";
    const QString strSuffix = "pdf";
# else
    const QString strName = "VirtualBox";
    const QString strSuffix = "chm";
# endif
#endif

    /* Where are the docs located? */
    char szDocsPath[RTPATH_MAX];
    int rc = RTPathAppDocs(szDocsPath, sizeof(szDocsPath));
    AssertRC(rc);

    /* Make sure that the language is in two letter code.
     * Note: if languageId() returns an empty string lang.name() will
     * return "C" which is an valid language code. */
    QLocale lang(VBoxGlobal::languageId());

    /* Construct the path and the filename: */
    QString strManual = QString("%1/%2_%3.%4").arg(szDocsPath)
                                              .arg(strName)
                                              .arg(lang.name())
                                              .arg(strSuffix);

    /* Check if a help file with that name exists: */
    QFileInfo fi(strManual);
    if (fi.exists())
        return strManual;

    /* Fall back to the standard: */
    strManual = QString("%1/%2.%4").arg(szDocsPath)
                                   .arg(strName)
                                   .arg(strSuffix);
    return strManual;
}

/* static */
QString VBoxGlobal::documentsPath()
{
    QString strPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir dir(strPath);
    if (dir.exists())
        return QDir::cleanPath(dir.canonicalPath());
    else
    {
        dir.setPath(QDir::homePath() + "/Documents");
        if (dir.exists())
            return QDir::cleanPath(dir.canonicalPath());
        else
            return QDir::homePath();
    }
}

/* static */
QRect VBoxGlobal::normalizeGeometry(const QRect &rectangle, const QRegion &boundRegion, bool fCanResize /* = true */)
{
    /* Perform direct and flipped search of position for @a rectangle to make sure it is fully contained
     * inside @a boundRegion region by moving & resizing (if @a fCanResize is specified) @a rectangle if
     * necessary. Selects the minimum shifted result between direct and flipped variants. */

    /* Direct search for normalized rectangle: */
    QRect var1(getNormalized(rectangle, boundRegion, fCanResize));

    /* Flipped search for normalized rectangle: */
    QRect var2(flip(getNormalized(flip(rectangle).boundingRect(),
                                  flip(boundRegion), fCanResize)).boundingRect());

    /* Calculate shift from starting position for both variants: */
    double dLength1 = sqrt(pow((double)(var1.x() - rectangle.x()), (double)2) +
                           pow((double)(var1.y() - rectangle.y()), (double)2));
    double dLength2 = sqrt(pow((double)(var2.x() - rectangle.x()), (double)2) +
                           pow((double)(var2.y() - rectangle.y()), (double)2));

    /* Return minimum shifted variant: */
    return dLength1 > dLength2 ? var2 : var1;
}

/* static */
QRect VBoxGlobal::getNormalized(const QRect &rectangle, const QRegion &boundRegion, bool /* fCanResize = true */)
{
    /* Ensures that the given rectangle @a rectangle is fully contained within the region @a boundRegion
     * by moving @a rectangle if necessary. If @a rectangle is larger than @a boundRegion, top left
     * corner of @a rectangle is aligned with the top left corner of maximum available rectangle and,
     * if @a fCanResize is true, @a rectangle is shrinked to become fully visible. */

    /* Storing available horizontal sub-rectangles & vertical shifts: */
    const int iWindowVertical = rectangle.center().y();
    QList<QRect> rectanglesList;
    QList<int> shiftsList;
    foreach (QRect currentItem, boundRegion.rects())
    {
        const int iCurrentDelta = qAbs(iWindowVertical - currentItem.center().y());
        const int iShift2Top = currentItem.top() - rectangle.top();
        const int iShift2Bot = currentItem.bottom() - rectangle.bottom();

        int iTtemPosition = 0;
        foreach (QRect item, rectanglesList)
        {
            const int iDelta = qAbs(iWindowVertical - item.center().y());
            if (iDelta > iCurrentDelta)
                break;
            else
                ++iTtemPosition;
        }
        rectanglesList.insert(iTtemPosition, currentItem);

        int iShift2TopPos = 0;
        foreach (int iShift, shiftsList)
            if (qAbs(iShift) > qAbs(iShift2Top))
                break;
            else
                ++iShift2TopPos;
        shiftsList.insert(iShift2TopPos, iShift2Top);

        int iShift2BotPos = 0;
        foreach (int iShift, shiftsList)
            if (qAbs(iShift) > qAbs(iShift2Bot))
                break;
            else
                ++iShift2BotPos;
        shiftsList.insert(iShift2BotPos, iShift2Bot);
    }

    /* Trying to find the appropriate place for window: */
    QRect result;
    for (int i = -1; i < shiftsList.size(); ++i)
    {
        /* Move to appropriate vertical: */
        QRect newRectangle(rectangle);
        if (i >= 0)
            newRectangle.translate(0, shiftsList[i]);

        /* Search horizontal shift: */
        int iMaxShift = 0;
        foreach (QRect item, rectanglesList)
        {
            QRect trectangle(newRectangle.translated(item.left() - newRectangle.left(), 0));
            if (!item.intersects(trectangle))
                continue;

            if (newRectangle.left() < item.left())
            {
                const int iShift = item.left() - newRectangle.left();
                iMaxShift = qAbs(iShift) > qAbs(iMaxShift) ? iShift : iMaxShift;
            }
            else if (newRectangle.right() > item.right())
            {
                const int iShift = item.right() - newRectangle.right();
                iMaxShift = qAbs(iShift) > qAbs(iMaxShift) ? iShift : iMaxShift;
            }
        }

        /* Shift across the horizontal direction: */
        newRectangle.translate(iMaxShift, 0);

        /* Check the translated rectangle to feat the rules: */
        if (boundRegion.united(newRectangle) == boundRegion)
            result = newRectangle;

        if (!result.isNull())
            break;
    }

    if (result.isNull())
    {
        /* Resize window to feat desirable size
         * using max of available rectangles: */
        QRect maxRectangle;
        quint64 uMaxSquare = 0;
        foreach (QRect item, rectanglesList)
        {
            const quint64 uSquare = item.width() * item.height();
            if (uSquare > uMaxSquare)
            {
                uMaxSquare = uSquare;
                maxRectangle = item;
            }
        }

        result = rectangle;
        result.moveTo(maxRectangle.x(), maxRectangle.y());
        if (maxRectangle.right() < result.right())
            result.setRight(maxRectangle.right());
        if (maxRectangle.bottom() < result.bottom())
            result.setBottom(maxRectangle.bottom());
    }

    return result;
}

/* static */
QRegion VBoxGlobal::flip(const QRegion &region)
{
    QRegion result;
    QVector<QRect> rectangles(region.rects());
    foreach (QRect rectangle, rectangles)
        result += QRect(rectangle.y(), rectangle.x(),
                        rectangle.height(), rectangle.width());
    return result;
}

/* static */
void VBoxGlobal::centerWidget(QWidget *pWidget, QWidget *pRelative, bool fCanResize /* = true */)
{
    /* If necessary, pWidget's position is adjusted to make it fully visible within
     * the available desktop area. If pWidget is bigger then this area, it will also
     * be resized unless fCanResize is false or there is an inappropriate minimum
     * size limit (in which case the top left corner will be simply aligned with the top
     * left corner of the available desktop area). pWidget must be a top-level widget.
     * pRelative may be any widget, but if it's not top-level itself, its top-level
     * widget will be used for calculations. pRelative can also be NULL, in which case
     * pWidget will be centered relative to the available desktop area. */

    AssertReturnVoid(pWidget);
    AssertReturnVoid(pWidget->isTopLevel());

    QRect deskGeo, parentGeo;
    if (pRelative)
    {
        pRelative = pRelative->window();
        deskGeo = gpDesktop->availableGeometry(pRelative);
        parentGeo = pRelative->frameGeometry();
        // WORKAROUND:
        // On X11/Gnome, geo/frameGeo.x() and y() are always 0 for top level
        // widgets with parents, what a shame. Use mapToGlobal() to workaround.
        QPoint d = pRelative->mapToGlobal(QPoint(0, 0));
        d.rx() -= pRelative->geometry().x() - pRelative->x();
        d.ry() -= pRelative->geometry().y() - pRelative->y();
        parentGeo.moveTopLeft(d);
    }
    else
    {
        deskGeo = gpDesktop->availableGeometry();
        parentGeo = deskGeo;
    }

    // WORKAROUND:
    // On X11, there is no way to determine frame geometry (including WM
    // decorations) before the widget is shown for the first time. Stupidly
    // enumerate other top level widgets to find the thickest frame. The code
    // is based on the idea taken from QDialog::adjustPositionInternal().

    int iExtraW = 0;
    int iExtraH = 0;

    QWidgetList list = QApplication::topLevelWidgets();
    QListIterator<QWidget*> it(list);
    while ((iExtraW == 0 || iExtraH == 0) && it.hasNext())
    {
        int iFrameW, iFrameH;
        QWidget *pCurrent = it.next();
        if (!pCurrent->isVisible())
            continue;

        iFrameW = pCurrent->frameGeometry().width() - pCurrent->width();
        iFrameH = pCurrent->frameGeometry().height() - pCurrent->height();

        iExtraW = qMax(iExtraW, iFrameW);
        iExtraH = qMax(iExtraH, iFrameH);
    }

    /* On non-X11 platforms, the following would be enough instead of the above workaround: */
    // QRect geo = frameGeometry();
    QRect geo = QRect(0, 0, pWidget->width() + iExtraW,
                            pWidget->height() + iExtraH);

    geo.moveCenter(QPoint(parentGeo.x() + (parentGeo.width() - 1) / 2,
                          parentGeo.y() + (parentGeo.height() - 1) / 2));

    /* Ensure the widget is within the available desktop area: */
    QRect newGeo = normalizeGeometry(geo, deskGeo, fCanResize);
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // No idea why, but Qt doesn't respect if there is a unified toolbar on the
    // ::move call. So manually add the height of the toolbar before setting
    // the position.
    if (pRelative)
        newGeo.translate(0, ::darwinWindowToolBarHeight(pWidget));
#endif /* VBOX_WS_MAC */

    pWidget->move(newGeo.topLeft());

    if (   fCanResize
        && (geo.width() != newGeo.width() || geo.height() != newGeo.height()))
        pWidget->resize(newGeo.width() - iExtraW, newGeo.height() - iExtraH);
}

#ifdef VBOX_WS_X11
typedef struct {
/** User specified flags */
uint32_t flags;
/** User-specified position */
int32_t x, y;
/** User-specified size */
int32_t width, height;
/** Program-specified minimum size */
int32_t min_width, min_height;
/** Program-specified maximum size */
int32_t max_width, max_height;
/** Program-specified resize increments */
int32_t width_inc, height_inc;
/** Program-specified minimum aspect ratios */
int32_t min_aspect_num, min_aspect_den;
/** Program-specified maximum aspect ratios */
int32_t max_aspect_num, max_aspect_den;
/** Program-specified base size */
int32_t base_width, base_height;
/** Program-specified window gravity */
uint32_t win_gravity;
} xcb_size_hints_t;
#endif /* VBOX_WS_X11 */

/* static */
void VBoxGlobal::setTopLevelGeometry(QWidget *pWidget, int x, int y, int w, int h)
{
    AssertPtrReturnVoid(pWidget);
#ifdef VBOX_WS_X11
# define QWINDOWSIZE_MAX ((1<<24)-1)
    if (pWidget->isWindow() && pWidget->isVisible())
    {
        // WORKAROUND:
        // X11 window managers are not required to accept geometry changes on
        // the top-level window.  Unfortunately, current at Qt 5.6 and 5.7, Qt
        // assumes that the change will succeed, and resizes all sub-windows
        // unconditionally.  By calling ConfigureWindow directly, Qt will see
        // our change request as an externally triggered one on success and not
        // at all if it is rejected.
        const double dDPR = gpDesktop->devicePixelRatio(pWidget);
        uint16_t fMask =   XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                         | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        uint32_t values[] = { (uint32_t)(x * dDPR), (uint32_t)(y * dDPR), (uint32_t)(w * dDPR), (uint32_t)(h * dDPR) };
        xcb_configure_window(QX11Info::connection(), (xcb_window_t)pWidget->winId(),
                             fMask, values);
        xcb_size_hints_t hints;
        hints.flags =   1 /* XCB_ICCCM_SIZE_HINT_US_POSITION */
                      | 2 /* XCB_ICCCM_SIZE_HINT_US_SIZE */
                      | 512 /* XCB_ICCCM_SIZE_P_WIN_GRAVITY */;
        hints.x           = x * dDPR;
        hints.y           = y * dDPR;
        hints.width       = w * dDPR;
        hints.height      = h * dDPR;
        hints.min_width   = pWidget->minimumSize().width() * dDPR;
        hints.min_height  = pWidget->minimumSize().height() * dDPR;
        hints.max_width   = pWidget->maximumSize().width() * dDPR;
        hints.max_height  = pWidget->maximumSize().height() * dDPR;
        hints.width_inc   = pWidget->sizeIncrement().width() * dDPR;
        hints.height_inc  = pWidget->sizeIncrement().height() * dDPR;
        hints.base_width  = pWidget->baseSize().width() * dDPR;
        hints.base_height = pWidget->baseSize().height() * dDPR;
        hints.win_gravity = XCB_GRAVITY_STATIC;
        if (hints.min_width > 0 || hints.min_height > 0)
            hints.flags |= 16 /* XCB_ICCCM_SIZE_HINT_P_MIN_SIZE */;
        if (hints.max_width < QWINDOWSIZE_MAX || hints.max_height < QWINDOWSIZE_MAX)
            hints.flags |= 32 /* XCB_ICCCM_SIZE_HINT_P_MAX_SIZE */;
        if (hints.width_inc > 0 || hints.height_inc)
            hints.flags |=   64 /* XCB_ICCCM_SIZE_HINT_P_MIN_SIZE */
                           | 256 /* XCB_ICCCM_SIZE_HINT_BASE_SIZE */;
        xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE,
                            (xcb_window_t)pWidget->winId(), XCB_ATOM_WM_NORMAL_HINTS,
                            XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(hints) >> 2, &hints);
        xcb_flush(QX11Info::connection());
    }
    else
        // WORKAROUND:
        // Call the Qt method if the window is not visible as otherwise no
        // Configure event will arrive to tell Qt what geometry we want.
        pWidget->setGeometry(x, y, w, h);
# else /* !VBOX_WS_X11 */
    pWidget->setGeometry(x, y, w, h);
# endif /* !VBOX_WS_X11 */
}

/* static */
void VBoxGlobal::setTopLevelGeometry(QWidget *pWidget, const QRect &rect)
{
    VBoxGlobal::setTopLevelGeometry(pWidget, rect.x(), rect.y(), rect.width(), rect.height());
}

#if defined(VBOX_WS_X11)

static char *XXGetProperty(Display *pDpy, Window windowHandle, Atom propType, const char *pszPropName)
{
    Atom propNameAtom = XInternAtom(pDpy, pszPropName, True /* only_if_exists */);
    if (propNameAtom == None)
        return NULL;

    Atom actTypeAtom = None;
    int actFmt = 0;
    unsigned long nItems = 0;
    unsigned long nBytesAfter = 0;
    unsigned char *propVal = NULL;
    int rc = XGetWindowProperty(pDpy, windowHandle, propNameAtom,
                                0, LONG_MAX, False /* delete */,
                                propType, &actTypeAtom, &actFmt,
                                &nItems, &nBytesAfter, &propVal);
    if (rc != Success)
        return NULL;

    return reinterpret_cast<char*>(propVal);
}

static Bool XXSendClientMessage(Display *pDpy, Window windowHandle, const char *pszMsg,
                                unsigned long aData0 = 0, unsigned long aData1 = 0,
                                unsigned long aData2 = 0, unsigned long aData3 = 0,
                                unsigned long aData4 = 0)
{
    Atom msgAtom = XInternAtom(pDpy, pszMsg, True /* only_if_exists */);
    if (msgAtom == None)
        return False;

    XEvent ev;

    ev.xclient.type = ClientMessage;
    ev.xclient.serial = 0;
    ev.xclient.send_event = True;
    ev.xclient.display = pDpy;
    ev.xclient.window = windowHandle;
    ev.xclient.message_type = msgAtom;

    /* Always send as 32 bit for now: */
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = aData0;
    ev.xclient.data.l[1] = aData1;
    ev.xclient.data.l[2] = aData2;
    ev.xclient.data.l[3] = aData3;
    ev.xclient.data.l[4] = aData4;

    return XSendEvent(pDpy, DefaultRootWindow(pDpy), False,
                      SubstructureRedirectMask, &ev) != 0;
}

#endif

/* static */
bool VBoxGlobal::activateWindow(WId wId, bool fSwitchDesktop /* = true */)
{
    RT_NOREF(fSwitchDesktop);
    bool fResult = true;

#if defined(VBOX_WS_WIN)

    HWND handle = (HWND)wId;

    if (IsIconic(handle))
        fResult &= !!ShowWindow(handle, SW_RESTORE);
    else if (!IsWindowVisible(handle))
        fResult &= !!ShowWindow(handle, SW_SHOW);

    fResult &= !!SetForegroundWindow(handle);

#elif defined(VBOX_WS_X11)

    Display *pDisplay = QX11Info::display();

    if (fSwitchDesktop)
    {
        /* try to find the desktop ID using the NetWM property */
        CARD32 *pDesktop = (CARD32 *) XXGetProperty(pDisplay, wId, XA_CARDINAL,
                                                    "_NET_WM_DESKTOP");
        if (pDesktop == NULL)
            // WORKAROUND:
            // if the NetWM properly is not supported try to find
            // the desktop ID using the GNOME WM property.
            pDesktop = (CARD32 *) XXGetProperty(pDisplay, wId, XA_CARDINAL,
                                                "_WIN_WORKSPACE");

        if (pDesktop != NULL)
        {
            Bool ok = XXSendClientMessage(pDisplay, DefaultRootWindow(pDisplay),
                                          "_NET_CURRENT_DESKTOP",
                                          *pDesktop);
            if (!ok)
            {
                Log1WarningFunc(("Couldn't switch to pDesktop=%08X\n", pDesktop));
                fResult = false;
            }
            XFree(pDesktop);
        }
        else
        {
            Log1WarningFunc(("Couldn't find a pDesktop ID for wId=%08X\n", wId));
            fResult = false;
        }
    }

    Bool ok = XXSendClientMessage(pDisplay, wId, "_NET_ACTIVE_WINDOW");
    fResult &= !!ok;

    XRaiseWindow(pDisplay, wId);

#else

    NOREF(wId);
    NOREF(fSwitchDesktop);
    AssertFailed();
    fResult = false;

#endif

    if (!fResult)
        Log1WarningFunc(("Couldn't activate wId=%08X\n", wId));

    return fResult;
}

/* static */
void VBoxGlobal::setCursor(QWidget *pWidget, const QCursor &cursor)
{
    if (!pWidget)
        return;

#ifdef VBOX_WS_X11
    /* As reported in https://www.virtualbox.org/ticket/16348,
     * in X11 QWidget::setCursor(..) call uses RENDER
     * extension. Qt (before 5.11) fails to handle the case where the mentioned extension
     * is missing. Please see https://codereview.qt-project.org/#/c/225665/ for Qt patch: */
    if ((VBoxGlobal::qtRTMajorVersion() < 5) ||
        (VBoxGlobal::qtRTMajorVersion() == 5 && VBoxGlobal::qtRTMinorVersion() < 11))
    {
        if (X11CheckExtension("RENDER"))
            pWidget->setCursor(cursor);
    }
    else
    {
        pWidget->setCursor(cursor);
    }
#else
    pWidget->setCursor(cursor);
#endif
}

/* static */
void VBoxGlobal::setCursor(QGraphicsWidget *pWidget, const QCursor &cursor)
{
    if (!pWidget)
        return;

#ifdef VBOX_WS_X11
    /* As reported in https://www.virtualbox.org/ticket/16348,
     * in X11 QGraphicsWidget::setCursor(..) call uses RENDER
     * extension. Qt (before 5.11) fails to handle the case where the mentioned extension
     * is missing. Please see https://codereview.qt-project.org/#/c/225665/ for Qt patch: */
    if ((VBoxGlobal::qtRTMajorVersion() < 5) ||
        (VBoxGlobal::qtRTMajorVersion() == 5 && VBoxGlobal::qtRTMinorVersion() < 11))
    {
        if (X11CheckExtension("RENDER"))
            pWidget->setCursor(cursor);
    }
    else
    {
        pWidget->setCursor(cursor);
    }
#else
    pWidget->setCursor(cursor);
#endif
}

/* static */
void VBoxGlobal::unsetCursor(QWidget *pWidget)
{
    if (!pWidget)
        return;

#ifdef VBOX_WS_X11
    /* As reported in https://www.virtualbox.org/ticket/16348,
     * in X11 QWidget::unsetCursor(..) call uses RENDER
     * extension. Qt (before 5.11) fails to handle the case where the mentioned extension
     * is missing. Please see https://codereview.qt-project.org/#/c/225665/ for Qt patch: */
    if ((VBoxGlobal::qtRTMajorVersion() < 5) ||
        (VBoxGlobal::qtRTMajorVersion() == 5 && VBoxGlobal::qtRTMinorVersion() < 11))
    {
        if (X11CheckExtension("RENDER"))
            pWidget->unsetCursor();
    }
    else
    {
        pWidget->unsetCursor();
    }
#else
    pWidget->unsetCursor();
#endif
}

/* static */
void VBoxGlobal::unsetCursor(QGraphicsWidget *pWidget)
{
    if (!pWidget)
        return;

#ifdef VBOX_WS_X11
    /* As reported in https://www.virtualbox.org/ticket/16348,
     * in X11 QGraphicsWidget::unsetCursor(..) call uses RENDER
     * extension. Qt (before 5.11) fails to handle the case where the mentioned extension
     * is missing. Please see https://codereview.qt-project.org/#/c/225665/ for Qt patch: */
    if ((VBoxGlobal::qtRTMajorVersion() < 5) ||
        (VBoxGlobal::qtRTMajorVersion() == 5 && VBoxGlobal::qtRTMinorVersion() < 11))
    {
        if (X11CheckExtension("RENDER"))
            pWidget->unsetCursor();
    }
    else
    {
        pWidget->unsetCursor();
    }
#else
    pWidget->unsetCursor();
#endif
}


#if defined(VBOX_WS_X11)

/* static */
bool VBoxGlobal::supportsFullScreenMonitorsProtocolX11()
{
    /* This method tests whether the current X11 window manager supports full-screen mode as we need it.
     * Unfortunately the EWMH specification was not fully clear about whether we can expect to find
     * all of these atoms on the _NET_SUPPORTED root window property, so we have to test with all
     * interesting window managers. If this fails for a user when you think it should succeed
     * they should try executing:
     * xprop -root | egrep -w '_NET_WM_FULLSCREEN_MONITORS|_NET_WM_STATE|_NET_WM_STATE_FULLSCREEN'
     * in an X11 terminal window.
     * All three strings should be found under a property called "_NET_SUPPORTED(ATOM)". */

    /* Using a global to get at the display does not feel right, but that is how it is done elsewhere in the code. */
    Display *pDisplay = QX11Info::display();
    Atom atomSupported            = XInternAtom(pDisplay, "_NET_SUPPORTED",
                                                True /* only_if_exists */);
    Atom atomWMFullScreenMonitors = XInternAtom(pDisplay,
                                                "_NET_WM_FULLSCREEN_MONITORS",
                                                True /* only_if_exists */);
    Atom atomWMState              = XInternAtom(pDisplay,
                                                "_NET_WM_STATE",
                                                True /* only_if_exists */);
    Atom atomWMStateFullScreen    = XInternAtom(pDisplay,
                                                "_NET_WM_STATE_FULLSCREEN",
                                                True /* only_if_exists */);
    bool fFoundFullScreenMonitors = false;
    bool fFoundState              = false;
    bool fFoundStateFullScreen    = false;
    Atom atomType;
    int cFormat;
    unsigned long cItems;
    unsigned long cbLeft;
    Atom *pAtomHints;
    int rc;
    unsigned i;

    if (   atomSupported == None || atomWMFullScreenMonitors == None
        || atomWMState == None || atomWMStateFullScreen == None)
        return false;
    /* Get atom value: */
    rc = XGetWindowProperty(pDisplay, DefaultRootWindow(pDisplay),
                            atomSupported, 0, 0x7fffffff /*LONG_MAX*/,
                            False /* delete */, XA_ATOM, &atomType,
                            &cFormat, &cItems, &cbLeft,
                            (unsigned char **)&pAtomHints);
    if (rc != Success)
        return false;
    if (pAtomHints == NULL)
        return false;
    if (atomType == XA_ATOM && cFormat == 32 && cbLeft == 0)
        for (i = 0; i < cItems; ++i)
        {
            if (pAtomHints[i] == atomWMFullScreenMonitors)
                fFoundFullScreenMonitors = true;
            if (pAtomHints[i] == atomWMState)
                fFoundState = true;
            if (pAtomHints[i] == atomWMStateFullScreen)
                fFoundStateFullScreen = true;
        }
    XFree(pAtomHints);
    return fFoundFullScreenMonitors && fFoundState && fFoundStateFullScreen;
}

/* static */
bool VBoxGlobal::setFullScreenMonitorX11(QWidget *pWidget, ulong uScreenId)
{
    return XXSendClientMessage(QX11Info::display(),
                               pWidget->window()->winId(),
                               "_NET_WM_FULLSCREEN_MONITORS",
                               uScreenId, uScreenId, uScreenId, uScreenId,
                               1 /* Source indication (1 = normal application) */);
}

/* static */
QVector<Atom> VBoxGlobal::flagsNetWmState(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState;
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);

    /* Get the size of the property data: */
    Atom actual_type;
    int iActualFormat;
    ulong uPropertyLength;
    ulong uBytesLeft;
    uchar *pPropertyData = 0;
    if (XGetWindowProperty(pDisplay, pWidget->window()->winId(),
                           net_wm_state, 0, 0, False, XA_ATOM, &actual_type, &iActualFormat,
                           &uPropertyLength, &uBytesLeft, &pPropertyData) == Success &&
        actual_type == XA_ATOM && iActualFormat == 32)
    {
        resultNetWmState.resize(uBytesLeft / 4);
        XFree((char*)pPropertyData);
        pPropertyData = 0;

        /* Fetch all data: */
        if (XGetWindowProperty(pDisplay, pWidget->window()->winId(),
                               net_wm_state, 0, resultNetWmState.size(), False, XA_ATOM, &actual_type, &iActualFormat,
                               &uPropertyLength, &uBytesLeft, &pPropertyData) != Success)
            resultNetWmState.clear();
        else if (uPropertyLength != (ulong)resultNetWmState.size())
            resultNetWmState.resize(uPropertyLength);

        /* Put it into resultNetWmState: */
        if (!resultNetWmState.isEmpty())
            memcpy(resultNetWmState.data(), pPropertyData, resultNetWmState.size() * sizeof(Atom));
        if (pPropertyData)
            XFree((char*)pPropertyData);
    }

    /* Return result: */
    return resultNetWmState;
}

/* static */
bool VBoxGlobal::isFullScreenFlagSet(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    Atom net_wm_state_fullscreen = XInternAtom(pDisplay, "_NET_WM_STATE_FULLSCREEN", True /* only if exists */);

    /* Check if flagsNetWmState(pWidget) contains full-screen flag: */
    return flagsNetWmState(pWidget).contains(net_wm_state_fullscreen);
}

/* static */
void VBoxGlobal::setFullScreenFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_fullscreen = XInternAtom(pDisplay, "_NET_WM_STATE_FULLSCREEN", True /* only if exists */);

    /* Append resultNetWmState with fullscreen flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_fullscreen))
    {
        resultNetWmState.append(net_wm_state_fullscreen);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}

/* static */
void VBoxGlobal::setSkipTaskBarFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_skip_taskbar = XInternAtom(pDisplay, "_NET_WM_STATE_SKIP_TASKBAR", True /* only if exists */);

    /* Append resultNetWmState with skip-taskbar flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_skip_taskbar))
    {
        resultNetWmState.append(net_wm_state_skip_taskbar);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}

/* static */
void VBoxGlobal::setSkipPagerFlag(QWidget *pWidget)
{
    /* Get display: */
    Display *pDisplay = QX11Info::display();

    /* Prepare atoms: */
    QVector<Atom> resultNetWmState = flagsNetWmState(pWidget);
    Atom net_wm_state = XInternAtom(pDisplay, "_NET_WM_STATE", True /* only if exists */);
    Atom net_wm_state_skip_pager = XInternAtom(pDisplay, "_NET_WM_STATE_SKIP_PAGER", True /* only if exists */);

    /* Append resultNetWmState with skip-pager flag if necessary: */
    if (!resultNetWmState.contains(net_wm_state_skip_pager))
    {
        resultNetWmState.append(net_wm_state_skip_pager);
        /* Apply property to widget again: */
        XChangeProperty(pDisplay, pWidget->window()->winId(),
                        net_wm_state, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)resultNetWmState.data(), resultNetWmState.size());
    }
}

/* static */
void VBoxGlobal::setWMClass(QWidget *pWidget, const QString &strNameString, const QString &strClassString)
{
    /* Make sure all arguments set: */
    AssertReturnVoid(pWidget && !strNameString.isNull() && !strClassString.isNull());

    /* Define QByteArray objects to make sure data is alive within the scope: */
    QByteArray nameByteArray;
    /* Check the existence of RESOURCE_NAME env. variable and override name string if necessary: */
    const char resourceName[] = "RESOURCE_NAME";
    if (qEnvironmentVariableIsSet(resourceName))
        nameByteArray = qgetenv(resourceName);
    else
        nameByteArray = strNameString.toLatin1();
    QByteArray classByteArray = strClassString.toLatin1();

    AssertReturnVoid(nameByteArray.data() && classByteArray.data());

    XClassHint windowClass;
    windowClass.res_name = nameByteArray.data();
    windowClass.res_class = classByteArray.data();
    /* Set WM_CLASS of the window to passed name and class strings: */
    XSetClassHint(QX11Info::display(), pWidget->window()->winId(), &windowClass);
}

#endif /* VBOX_WS_X11 */

/* static */
void VBoxGlobal::setMinimumWidthAccordingSymbolCount(QSpinBox *pSpinBox, int cCount)
{
    /* Shame on Qt it hasn't stuff for tuning
     * widget size suitable for reflecting content of desired size.
     * For example QLineEdit, QSpinBox and similar widgets should have a methods
     * to strict the minimum width to reflect at least [n] symbols. */

    /* Load options: */
    QStyleOptionSpinBox option;
    option.initFrom(pSpinBox);

    /* Acquire edit-field rectangle: */
    QRect rect = pSpinBox->style()->subControlRect(QStyle::CC_SpinBox,
                                                   &option,
                                                   QStyle::SC_SpinBoxEditField,
                                                   pSpinBox);

    /* Calculate minimum-width magic: */
    const int iSpinBoxWidth = pSpinBox->width();
    const int iSpinBoxEditFieldWidth = rect.width();
    const int iSpinBoxDelta = qMax(0, iSpinBoxWidth - iSpinBoxEditFieldWidth);
    QFontMetrics metrics(pSpinBox->font(), pSpinBox);
    const QString strDummy(cCount, '0');
    const int iTextWidth = metrics.width(strDummy);

    /* Tune spin-box minimum-width: */
    pSpinBox->setMinimumWidth(iTextWidth + iSpinBoxDelta);
}

QString VBoxGlobal::vmGuestOSFamilyDescription(const QString &strFamilyId) const
{
    AssertMsg(m_guestOSFamilyDescriptions.contains(strFamilyId),
              ("Family ID incorrect: '%s'.", strFamilyId.toLatin1().constData()));
    return m_guestOSFamilyDescriptions.value(strFamilyId);
}

QList<CGuestOSType> VBoxGlobal::vmGuestOSTypeList(const QString &strFamilyId) const
{
    AssertMsg(m_guestOSFamilyIDs.contains(strFamilyId),
              ("Family ID incorrect: '%s'.", strFamilyId.toLatin1().constData()));
    return m_guestOSFamilyIDs.contains(strFamilyId) ?
           m_guestOSTypes[m_guestOSFamilyIDs.indexOf(strFamilyId)] : QList<CGuestOSType>();
}

CGuestOSType VBoxGlobal::vmGuestOSType(const QString &strTypeId,
                                       const QString &strFamilyId /* = QString() */) const
{
    QList<CGuestOSType> list;
    if (m_guestOSFamilyIDs.contains(strFamilyId))
    {
        list = m_guestOSTypes.at(m_guestOSFamilyIDs.indexOf(strFamilyId));
    }
    else
    {
        for (int i = 0; i < m_guestOSFamilyIDs.size(); ++i)
            list += m_guestOSTypes.at(i);
    }
    for (int j = 0; j < list.size(); ++j)
        if (!list.at(j).GetId().compare(strTypeId))
            return list.at(j);
    return CGuestOSType();
}

QString VBoxGlobal::vmGuestOSTypeDescription(const QString &strTypeId) const
{
    for (int i = 0; i < m_guestOSFamilyIDs.size(); ++i)
    {
        QList<CGuestOSType> list(m_guestOSTypes[i]);
        for (int j = 0; j < list.size(); ++j)
            if (!list.at(j).GetId().compare(strTypeId))
                return list.at(j).GetDescription();
    }
    return QString();
}

/* static */
bool VBoxGlobal::isDOSType(const QString &strOSTypeId)
{
    if (   strOSTypeId.left(3) == "dos"
        || strOSTypeId.left(3) == "win"
        || strOSTypeId.left(3) == "os2")
        return true;

    return false;
}

/* static */
bool VBoxGlobal::switchToMachine(CMachine &comMachine)
{
#ifdef VBOX_WS_MAC
    ULONG64 id = comMachine.ShowConsoleWindow();
#else
    WId id = (WId)comMachine.ShowConsoleWindow();
#endif

    AssertWrapperOk(comMachine);
    if (!comMachine.isOk())
        return false;

    // WORKAROUND:
    // winId = 0 it means the console window has already done everything
    // necessary to implement the "show window" semantics.
    if (id == 0)
        return true;

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)

    return activateWindow(id, true);

#elif defined(VBOX_WS_MAC)

    // WORKAROUND:
    // This is just for the case were the other process cannot steal
    // the focus from us. It will send us a PSN so we can try.
    ProcessSerialNumber psn;
    psn.highLongOfPSN = id >> 32;
    psn.lowLongOfPSN = (UInt32)id;
# ifdef __clang__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    OSErr rc = ::SetFrontProcess(&psn);
#  pragma GCC diagnostic pop
# else
    OSErr rc = ::SetFrontProcess(&psn);
# endif
    if (!rc)
        Log(("GUI: %#RX64 couldn't do SetFrontProcess on itself, the selector (we) had to do it...\n", id));
    else
        Log(("GUI: Failed to bring %#RX64 to front. rc=%#x\n", id, rc));
    return !rc;

#else

    return false;

#endif
}

bool VBoxGlobal::launchMachine(CMachine &comMachine, LaunchMode enmLaunchMode /* = LaunchMode_Default */)
{
    /* Switch to machine window(s) if possible: */
    if (   comMachine.GetSessionState() == KSessionState_Locked /* precondition for CanShowConsoleWindow() */
        && comMachine.CanShowConsoleWindow())
    {
        /* For the Selector UI: */
        if (!isVMConsoleProcess())
        {
            /* Just switch to existing VM window: */
            return switchToMachine(comMachine);
        }
        /* For the Runtime UI: */
        else
        {
            /* Only separate UI process can reach that place,
             * switch to existing VM window and exit. */
            switchToMachine(comMachine);
            return false;
        }
    }

    if (enmLaunchMode != LaunchMode_Separate)
    {
        /* Make sure machine-state is one of required: */
        KMachineState enmState = comMachine.GetState(); NOREF(enmState);
        AssertMsg(   enmState == KMachineState_PoweredOff
                  || enmState == KMachineState_Saved
                  || enmState == KMachineState_Teleported
                  || enmState == KMachineState_Aborted
                  , ("Machine must be PoweredOff/Saved/Teleported/Aborted (%d)", enmState));
    }

    /* Create empty session instance: */
    CSession comSession;
    comSession.createInstance(CLSID_Session);
    if (comSession.isNull())
    {
        msgCenter().cannotOpenSession(comSession);
        return false;
    }

    /* Configure environment: */
    QString strEnv;
#ifdef Q_OS_WIN
    /* Allow started VM process to be foreground window: */
    AllowSetForegroundWindow(ASFW_ANY);
#endif
#ifdef VBOX_WS_X11
    /* Make sure VM process will start on the same display as the VM selector: */
    const char *pDisplay = RTEnvGet("DISPLAY");
    if (pDisplay)
        strEnv.append(QString("DISPLAY=%1\n").arg(pDisplay));
    const char *pXauth = RTEnvGet("XAUTHORITY");
    if (pXauth)
        strEnv.append(QString("XAUTHORITY=%1\n").arg(pXauth));
#endif
    QString strType;
    switch (enmLaunchMode)
    {
        case LaunchMode_Default:  strType = ""; break;
        case LaunchMode_Separate: strType = isSeparateProcess() ? "headless" : "separate"; break;
        case LaunchMode_Headless: strType = "headless"; break;
        default: AssertFailedReturn(false);
    }

    /* Prepare "VM spawning" progress: */
    CProgress comProgress = comMachine.LaunchVMProcess(comSession, strType, strEnv);
    if (!comMachine.isOk())
    {
        /* If the VM is started separately and the VM process is already running, then it is OK. */
        if (enmLaunchMode == LaunchMode_Separate)
        {
            KMachineState enmState = comMachine.GetState();
            if (   enmState >= KMachineState_FirstOnline
                && enmState <= KMachineState_LastOnline)
            {
                /* Already running. */
                return true;
            }
        }

        msgCenter().cannotOpenSession(comMachine);
        return false;
    }

    /* Postpone showing "VM spawning" progress.
     * Hope 1 minute will be enough to spawn any running VM silently,
     * otherwise we better show the progress...
     * If starting separately, then show the progress now. */
    int iSpawningDuration = enmLaunchMode == LaunchMode_Separate ? 0 : 60000;
    msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(),
                                        ":/progress_start_90px.png", 0, iSpawningDuration);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        msgCenter().cannotOpenSession(comProgress, comMachine.GetName());

    /* Unlock machine, close session: */
    comSession.UnlockMachine();

    /* True finally: */
    return true;
}

CSession VBoxGlobal::openSession(const QString &strId, KLockType lockType /* = KLockType_Shared */)
{
    /* Prepare session: */
    CSession comSession;

    /* Simulate try-catch block: */
    bool fSuccess = false;
    do
    {
        /* Create empty session instance: */
        comSession.createInstance(CLSID_Session);
        if (comSession.isNull())
        {
            msgCenter().cannotOpenSession(comSession);
            break;
        }

        /* Search for the corresponding machine: */
        CMachine comMachine = m_comVBox.FindMachine(strId);
        if (comMachine.isNull())
        {
            msgCenter().cannotFindMachineById(m_comVBox, strId);
            break;
        }

        if (lockType == KLockType_VM)
            comSession.SetName("GUI/Qt");

        /* Lock found machine to session: */
        comMachine.LockMachine(comSession, lockType);
        if (!comMachine.isOk())
        {
            msgCenter().cannotOpenSession(comMachine);
            break;
        }

        /* Pass the language ID as the property to the guest: */
        if (comSession.GetType() == KSessionType_Shared)
        {
            CMachine comStartedMachine = comSession.GetMachine();
            /* Make sure that the language is in two letter code.
             * Note: if languageId() returns an empty string lang.name() will
             * return "C" which is an valid language code. */
            QLocale lang(VBoxGlobal::languageId());
            comStartedMachine.SetGuestPropertyValue("/VirtualBox/HostInfo/GUI/LanguageID", lang.name());
        }

        /* Success finally: */
        fSuccess = true;
    }
    while (0);
    /* Cleanup try-catch block: */
    if (!fSuccess)
        comSession.detach();

    /* Return session: */
    return comSession;
}

void VBoxGlobal::startMediumEnumeration()
{
    /* Make sure VBoxGlobal is already valid: */
    AssertReturnVoid(m_fValid);

    /* Make sure medium-enumerator is already created: */
    if (!m_pMediumEnumerator)
        return;

    /* Make sure enumeration is not already started: */
    if (isMediumEnumerationInProgress())
        return;

    /* Ignore the request during VBoxGlobal cleanup: */
    if (s_fCleaningUp)
        return;

    /* If asked to restore snapshot, don't do this till *after* we're done
     * restoring or the code with have a heart attack. */
    if (shouldRestoreCurrentSnapshot())
        return;

    if (m_meCleanupProtectionToken.tryLockForRead())
    {
        /* Redirect request to medium-enumerator: */
        if (m_pMediumEnumerator)
            m_pMediumEnumerator->enumerateMedia();
        m_meCleanupProtectionToken.unlock();
    }
}

void VBoxGlobal::refreshMedia()
{
    /* Make sure VBoxGlobal is already valid: */
    AssertReturnVoid(m_fValid);

    /* Make sure medium-enumerator is already created: */
    if (!m_pMediumEnumerator)
        return;

    /* Make sure enumeration is not already started: */
    if (isMediumEnumerationInProgress())
        return;

    /* Ignore the request during VBoxGlobal cleanup: */
    if (s_fCleaningUp)
        return;

    /* If asked to restore snapshot, don't do this till *after* we're done
     * restoring or the code with have a heart attack. */
    if (shouldRestoreCurrentSnapshot())
        return;

    /* We assume it's safe to call it without locking,
     * since we are performing blocking operation here. */
    m_pMediumEnumerator->refreshMedia();
}

bool VBoxGlobal::isMediumEnumerationInProgress() const
{
    /* Redirect request to medium-enumerator: */
    return    m_pMediumEnumerator
           && m_pMediumEnumerator->isMediumEnumerationInProgress();
}

UIMedium VBoxGlobal::medium(const QString &strMediumID) const
{
    if (m_meCleanupProtectionToken.tryLockForRead())
    {
        /* Redirect call to medium-enumerator: */
        UIMedium guiMedium;
        if (m_pMediumEnumerator)
            guiMedium = m_pMediumEnumerator->medium(strMediumID);
        m_meCleanupProtectionToken.unlock();
        return guiMedium;
    }
    return UIMedium();
}

QList<QString> VBoxGlobal::mediumIDs() const
{
    if (m_meCleanupProtectionToken.tryLockForRead())
    {
        /* Redirect call to medium-enumerator: */
        QList<QString> listOfMedia;
        if (m_pMediumEnumerator)
            listOfMedia = m_pMediumEnumerator->mediumIDs();
        m_meCleanupProtectionToken.unlock();
        return listOfMedia;
    }
    return QList<QString>();
}

void VBoxGlobal::createMedium(const UIMedium &guiMedium)
{
    if (m_meCleanupProtectionToken.tryLockForRead())
    {
        /* Create medium in medium-enumerator: */
        if (m_pMediumEnumerator)
            m_pMediumEnumerator->createMedium(guiMedium);
        m_meCleanupProtectionToken.unlock();
    }
}

void VBoxGlobal::deleteMedium(const QString &strMediumID)
{
    if (m_meCleanupProtectionToken.tryLockForRead())
    {
        /* Delete medium from medium-enumerator: */
        if (m_pMediumEnumerator)
            m_pMediumEnumerator->deleteMedium(strMediumID);
        m_meCleanupProtectionToken.unlock();
    }
}

QString VBoxGlobal::openMedium(UIMediumType enmMediumType, QString strMediumLocation, QWidget *pParent /* = 0 */)
{
    /* Convert to native separators: */
    strMediumLocation = QDir::toNativeSeparators(strMediumLocation);

    /* Initialize variables: */
    CVirtualBox comVBox = virtualBox();

    updateRecentlyUsedMediumListAndFolder(enmMediumType, strMediumLocation);

    /* Open corresponding medium: */
    CMedium comMedium = comVBox.OpenMedium(strMediumLocation, mediumTypeToGlobal(enmMediumType), KAccessMode_ReadWrite, false);

    if (comVBox.isOk())
    {
        /* Prepare vbox medium wrapper: */
        UIMedium guiMedium = medium(comMedium.GetId());

        /* First of all we should test if that medium already opened: */
        if (guiMedium.isNull())
        {
            /* And create new otherwise: */
            guiMedium = UIMedium(comMedium, enmMediumType, KMediumState_Created);
            createMedium(guiMedium);
        }

        /* Return guiMedium id: */
        return guiMedium.id();
    }
    else
        msgCenter().cannotOpenMedium(comVBox, enmMediumType, strMediumLocation, pParent);

    return QString();
}

QString VBoxGlobal::openMediumWithFileOpenDialog(UIMediumType enmMediumType, QWidget *pParent,
                                                 const QString &strDefaultFolder /* = QString() */,
                                                 bool fUseLastFolder /* = false */)
{
    /* Initialize variables: */
    QList<QPair <QString, QString> > filters;
    QStringList backends;
    QStringList prefixes;
    QString strFilter;
    QString strTitle;
    QString allType;
    QString strLastFolder;
    switch (enmMediumType)
    {
        case UIMediumType_HardDisk:
        {
            filters = HDDBackends(virtualBox());
            strTitle = tr("Please choose a virtual hard disk file");
            allType = tr("All virtual hard disk files (%1)");
            strLastFolder = gEDataManager->recentFolderForHardDrives();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForOpticalDisks();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForFloppyDisks();
            break;
        }
        case UIMediumType_DVD:
        {
            filters = DVDBackends(virtualBox());
            strTitle = tr("Please choose a virtual optical disk file");
            allType = tr("All virtual optical disk files (%1)");
            strLastFolder = gEDataManager->recentFolderForOpticalDisks();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForFloppyDisks();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForHardDrives();
            break;
        }
        case UIMediumType_Floppy:
        {
            filters = FloppyBackends(virtualBox());
            strTitle = tr("Please choose a virtual floppy disk file");
            allType = tr("All virtual floppy disk files (%1)");
            strLastFolder = gEDataManager->recentFolderForFloppyDisks();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForOpticalDisks();
            if (strLastFolder.isEmpty())
                strLastFolder = gEDataManager->recentFolderForHardDrives();
            break;
        }
        default:
            break;
    }
    QString strHomeFolder = fUseLastFolder && !strLastFolder.isEmpty() ? strLastFolder :
                            strDefaultFolder.isEmpty() ? homeFolder() : strDefaultFolder;

    /* Prepare filters and backends: */
    for (int i = 0; i < filters.count(); ++i)
    {
        /* Get iterated filter: */
        QPair<QString, QString> item = filters.at(i);
        /* Create one backend filter string: */
        backends << QString("%1 (%2)").arg(item.first).arg(item.second);
        /* Save the suffix's for the "All" entry: */
        prefixes << item.second;
    }
    if (!prefixes.isEmpty())
        backends.insert(0, allType.arg(prefixes.join(" ").trimmed()));
    backends << tr("All files (*)");
    strFilter = backends.join(";;").trimmed();

    /* Create open file dialog: */
    QStringList files = QIFileDialog::getOpenFileNames(strHomeFolder, strFilter, pParent, strTitle, 0, true, true);

    /* If dialog has some result: */
    if (!files.empty() && !files[0].isEmpty())
        return openMedium(enmMediumType, files[0], pParent);

    return QString();
}

QString VBoxGlobal::createVisoMediumWithFileOpenDialog(QWidget *pParent, const QString &strFolder)
{
    AssertReturn(!strFolder.isEmpty(), QString());

    /* Figure out where to start browsing for content. */
    QString strDirectory = gEDataManager->recentFolderForVISOContent();
    if (strDirectory.isEmpty())
        strDirectory = QDir::homePath();
    if (strDirectory.isEmpty())
        strDirectory = homeFolder();

    /* Execute the open file dialog, getting a list of files & dirs back. */
    /** @todo make it possible to select directories... */
    QStringList files = QIFileDialog::getOpenFileNames(strDirectory, tr("All files (*)"), pParent,
                                                       /// @todo tr("Please select files and directories to be on the VISO"),
                                                       tr("Please select files to be on the VISO"),
                                                       0, true /* resolve symlinks? */, false /* single file? */);

    /* Return if no result. */
    if (files.empty() || files[0].isEmpty())
        return QString();

    /* Remember folder for the next time. */
    gEDataManager->setRecentFolderForVISOContent(QFileInfo(files[0]).absolutePath());

    /* Produce the VISO. */
    char szVisoPath[RTPATH_MAX];
    int vrc = RTPathJoin(szVisoPath, sizeof(szVisoPath), strFolder.toUtf8().constData(), "ad-hoc.viso");
    if (RT_SUCCESS(vrc))
    {
        PRTSTREAM pStrmViso;
        vrc = RTStrmOpen(szVisoPath, "w", &pStrmViso);
        if (RT_SUCCESS(vrc))
        {
            RTUUID Uuid;
            vrc = RTUuidCreate(&Uuid);
            if (RT_SUCCESS(vrc))
            {
                RTStrmPrintf(pStrmViso, "--iprt-iso-maker-file-marker-bourne-sh %RTuuid\n", &Uuid);

                for (int iFile = 0; iFile < files.size(); iFile++)
                {
                    QByteArray const utf8Name = files[iFile].toUtf8();
                    const char *apszArgv[2] = { utf8Name.constData(), NULL };
                    char *pszQuoted;
                    vrc = RTGetOptArgvToString(&pszQuoted, apszArgv, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
                    if (RT_SUCCESS(vrc))
                    {
                        RTStrmPrintf(pStrmViso, "%s\n", pszQuoted);
                        RTStrFree(pszQuoted);
                    }
                    else
                        break;
                }

                RTStrmFlush(pStrmViso);
                if (RT_SUCCESS(vrc))
                    vrc = RTStrmError(pStrmViso);
            }

            RTStrmClose(pStrmViso);
        }
    }

    /* Done. */
    if (RT_SUCCESS(vrc))
        return openMedium(UIMediumType_DVD, QString(szVisoPath), pParent);

    /** @todo error message. */
    return QString();
}

QString VBoxGlobal::showCreateFloppyDiskDialog(QWidget *pParent, const QString &strMachineName, const QString &strMachineFolder)
{
    UIFDCreationDialog *pDialog = new UIFDCreationDialog(pParent, strMachineName, strMachineFolder);
    if (pDialog->exec())
    {
        return pDialog->mediumID();
    }
    delete pDialog;
    return QString();
}

void VBoxGlobal::prepareStorageMenu(QMenu &menu,
                                    QObject *pListener, const char *pszSlotName,
                                    const CMachine &comMachine, const QString &strControllerName, const StorageSlot &storageSlot)
{
    /* Current attachment attributes: */
    const CMediumAttachment comCurrentAttachment = comMachine.GetMediumAttachment(strControllerName,
                                                                                  storageSlot.port,
                                                                                  storageSlot.device);
    const CMedium comCurrentMedium = comCurrentAttachment.GetMedium();
    const QString strCurrentID = comCurrentMedium.isNull() ? QString() : comCurrentMedium.GetId();
    const QString strCurrentLocation = comCurrentMedium.isNull() ? QString() : comCurrentMedium.GetLocation();

    /* Other medium-attachments of same machine: */
    const CMediumAttachmentVector comAttachments = comMachine.GetMediumAttachments();

    /* Determine device & medium types: */
    const UIMediumType enmMediumType = mediumTypeToLocal(comCurrentAttachment.GetType());
    AssertMsgReturnVoid(enmMediumType != UIMediumType_Invalid, ("Incorrect storage medium type!\n"));

    /* Prepare open-existing-medium action: */
    QAction *pActionOpenExistingMedium = menu.addAction(UIIconPool::iconSet(":/select_file_16px.png"),
                                                        QString(), pListener, pszSlotName);
    pActionOpenExistingMedium->setData(QVariant::fromValue(UIMediumTarget(strControllerName, comCurrentAttachment.GetPort(),
                                                                          comCurrentAttachment.GetDevice(), enmMediumType)));
    pActionOpenExistingMedium->setText(QApplication::translate("UIMachineSettingsStorage", "Choose disk image...",
                                                               "This is used for hard disks, optical media and floppies"));

    /* Prepare create floppy disk action: */
    if (enmMediumType == UIMediumType_Floppy)
    {
        QAction *pActionCreateFloppy = menu.addAction(UIIconPool::iconSet(":/fd_add_16px.png"),
                                                      QString(), pListener, pszSlotName);
        pActionCreateFloppy->setData(QVariant::fromValue(UIMediumTarget(strControllerName, comCurrentAttachment.GetPort(),
                                                                        comCurrentAttachment.GetDevice(), enmMediumType,
                                                                        UIMediumTarget::UIMediumTargetType_CreateFloppyDisk)));
        pActionCreateFloppy->setText(QApplication::translate("UIMachineSettingsStorage", "Create a new floppy disk...",
                                                             "This is used to create a new floppy disk"));
    }
    /* Prepare ad-hoc-viso action for DVD-ROMs: */
    if (enmMediumType == UIMediumType_DVD)
    {
        QAction *pActionAdHocViso = menu.addAction(UIIconPool::iconSet(":/select_file_16px.png"),
                                                   QString(), pListener, pszSlotName);
        pActionAdHocViso->setData(QVariant::fromValue(UIMediumTarget(strControllerName, comCurrentAttachment.GetPort(),
                                                                     comCurrentAttachment.GetDevice(), enmMediumType,
                                                                     UIMediumTarget::UIMediumTargetType_CreateAdHocVISO)));
        pActionAdHocViso->setText(QApplication::translate("UIMachineSettingsStorage", "Create ad hoc VISO...",
                                                          "This is used for optical media"));
    }

    /* Insert separator: */
    menu.addSeparator();

    /* Get existing-host-drive vector: */
    CMediumVector comMedia;
    switch (enmMediumType)
    {
        case UIMediumType_DVD:    comMedia = host().GetDVDDrives(); break;
        case UIMediumType_Floppy: comMedia = host().GetFloppyDrives(); break;
        default: break;
    }
    /* Prepare choose-existing-host-drive actions: */
    foreach (const CMedium &comMedium, comMedia)
    {
        /* Make sure host-drive usage is unique: */
        bool fIsHostDriveUsed = false;
        foreach (const CMediumAttachment &comOtherAttachment, comAttachments)
        {
            if (comOtherAttachment != comCurrentAttachment)
            {
                const CMedium &comOtherMedium = comOtherAttachment.GetMedium();
                if (!comOtherMedium.isNull() && comOtherMedium.GetId() == comMedium.GetId())
                {
                    fIsHostDriveUsed = true;
                    break;
                }
            }
        }
        /* If host-drives usage is unique: */
        if (!fIsHostDriveUsed)
        {
            QAction *pActionChooseHostDrive = menu.addAction(UIMedium(comMedium, enmMediumType).name(), pListener, pszSlotName);
            pActionChooseHostDrive->setCheckable(true);
            pActionChooseHostDrive->setChecked(!comCurrentMedium.isNull() && comMedium.GetId() == strCurrentID);
            pActionChooseHostDrive->setData(QVariant::fromValue(UIMediumTarget(strControllerName,
                                                                               comCurrentAttachment.GetPort(),
                                                                               comCurrentAttachment.GetDevice(),
                                                                               enmMediumType,
                                                                               UIMediumTarget::UIMediumTargetType_WithID,
                                                                               comMedium.GetId())));
        }
    }

    /* Get recent-medium list: */
    QStringList recentMediumList;
    QStringList recentMediumListUsed;
    switch (enmMediumType)
    {
        case UIMediumType_HardDisk: recentMediumList = gEDataManager->recentListOfHardDrives(); break;
        case UIMediumType_DVD:      recentMediumList = gEDataManager->recentListOfOpticalDisks(); break;
        case UIMediumType_Floppy:   recentMediumList = gEDataManager->recentListOfFloppyDisks(); break;
        default: break;
    }
    /* Prepare choose-recent-medium actions: */
    foreach (const QString &strRecentMediumLocationBase, recentMediumList)
    {
        /* Confirm medium uniqueness: */
        if (recentMediumListUsed.contains(strRecentMediumLocationBase))
            continue;
        /* Mark medium as known: */
        recentMediumListUsed << strRecentMediumLocationBase;
        /* Convert separators to native: */
        const QString strRecentMediumLocation = QDir::toNativeSeparators(strRecentMediumLocationBase);
        /* Confirm medium presence: */
        if (!QFile::exists(strRecentMediumLocation))
            continue;
        /* Make sure recent-medium usage is unique: */
        bool fIsRecentMediumUsed = false;
        foreach (const CMediumAttachment &otherAttachment, comAttachments)
        {
            if (otherAttachment != comCurrentAttachment)
            {
                const CMedium &comOtherMedium = otherAttachment.GetMedium();
                if (!comOtherMedium.isNull() && comOtherMedium.GetLocation() == strRecentMediumLocation)
                {
                    fIsRecentMediumUsed = true;
                    break;
                }
            }
        }
        /* If recent-medium usage is unique: */
        if (!fIsRecentMediumUsed)
        {
            QAction *pActionChooseRecentMedium = menu.addAction(QFileInfo(strRecentMediumLocation).fileName(),
                                                                pListener, pszSlotName);
            pActionChooseRecentMedium->setCheckable(true);
            pActionChooseRecentMedium->setChecked(!comCurrentMedium.isNull() && strRecentMediumLocation == strCurrentLocation);
            pActionChooseRecentMedium->setData(QVariant::fromValue(UIMediumTarget(strControllerName,
                                                                                  comCurrentAttachment.GetPort(),
                                                                                  comCurrentAttachment.GetDevice(),
                                                                                  enmMediumType,
                                                                                  UIMediumTarget::UIMediumTargetType_WithLocation,
                                                                                  strRecentMediumLocation)));
            pActionChooseRecentMedium->setToolTip(strRecentMediumLocation);
        }
    }

    /* Last action for optical/floppy attachments only: */
    if (enmMediumType == UIMediumType_DVD || enmMediumType == UIMediumType_Floppy)
    {
        /* Insert separator: */
        menu.addSeparator();

        /* Prepare unmount-current-medium action: */
        QAction *pActionUnmountMedium = menu.addAction(QString(), pListener, pszSlotName);
        pActionUnmountMedium->setEnabled(!comCurrentMedium.isNull());
        pActionUnmountMedium->setData(QVariant::fromValue(UIMediumTarget(strControllerName, comCurrentAttachment.GetPort(),
                                                                         comCurrentAttachment.GetDevice())));
        pActionUnmountMedium->setText(QApplication::translate("UIMachineSettingsStorage", "Remove disk from virtual drive"));
        if (enmMediumType == UIMediumType_DVD)
            pActionUnmountMedium->setIcon(UIIconPool::iconSet(":/cd_unmount_16px.png", ":/cd_unmount_disabled_16px.png"));
        else if (enmMediumType == UIMediumType_Floppy)
            pActionUnmountMedium->setIcon(UIIconPool::iconSet(":/fd_unmount_16px.png", ":/fd_unmount_disabled_16px.png"));
    }
}

void VBoxGlobal::updateMachineStorage(const CMachine &comConstMachine, const UIMediumTarget &target)
{
    /* Mount (by default): */
    bool fMount = true;
    /* Null medium (by default): */
    CMedium comMedium;
    /* With null ID (by default): */
    QString strActualID;

    /* Current mount-target attributes: */
    const CStorageController comCurrentController = comConstMachine.GetStorageControllerByName(target.name);
    const KStorageBus enmCurrentStorageBus = comCurrentController.GetBus();
    const CMediumAttachment comCurrentAttachment = comConstMachine.GetMediumAttachment(target.name, target.port, target.device);
    const CMedium comCurrentMedium = comCurrentAttachment.GetMedium();
    const QString strCurrentID = comCurrentMedium.isNull() ? QString() : comCurrentMedium.GetId();
    const QString strCurrentLocation = comCurrentMedium.isNull() ? QString() : comCurrentMedium.GetLocation();

    /* Which additional info do we have? */
    switch (target.type)
    {
        /* Do we have an exact ID or do we let the user open a medium? */
        case UIMediumTarget::UIMediumTargetType_WithID:
        case UIMediumTarget::UIMediumTargetType_CreateAdHocVISO:
        case UIMediumTarget::UIMediumTargetType_CreateFloppyDisk:
        {
            /* New mount-target attributes: */
            QString strNewID;

            /* Invoke file-open dialog to choose medium ID: */
            if (target.mediumType != UIMediumType_Invalid && target.data.isNull())
            {
                /* Keyboard can be captured by machine-view.
                 * So we should clear machine-view focus to let file-open dialog get it.
                 * That way the keyboard will be released too.. */
                QWidget *pLastFocusedWidget = 0;
                if (QApplication::focusWidget())
                {
                    pLastFocusedWidget = QApplication::focusWidget();
                    pLastFocusedWidget->clearFocus();
                }
                /* Call for file-open dialog: */
                const QString strMachineFolder(QFileInfo(comConstMachine.GetSettingsFilePath()).absolutePath());
                QString strMediumID;
                if (target.type == UIMediumTarget::UIMediumTargetType_WithID)
                    strMediumID = openMediumWithFileOpenDialog(target.mediumType, windowManager().mainWindowShown(), strMachineFolder);
                else if(target.type == UIMediumTarget::UIMediumTargetType_CreateAdHocVISO)
                    strMediumID = createVisoMediumWithFileOpenDialog(windowManager().mainWindowShown(), strMachineFolder);
                else if(target.type == UIMediumTarget::UIMediumTargetType_CreateFloppyDisk)
                    strMediumID = showCreateFloppyDiskDialog(windowManager().mainWindowShown(), comConstMachine.GetName(), strMachineFolder);

                /* Return focus back: */
                if (pLastFocusedWidget)
                    pLastFocusedWidget->setFocus();
                /* Accept new medium ID: */
                if (!strMediumID.isNull())
                    strNewID = strMediumID;
                /* Else just exit: */
                else return;
            }
            /* Use medium ID which was passed: */
            else if (!target.data.isNull() && target.data != strCurrentID)
                strNewID = target.data;

            /* Should we mount or unmount? */
            fMount = !strNewID.isEmpty();

            /* Prepare target medium: */
            const UIMedium guiMedium = medium(strNewID);
            comMedium = guiMedium.medium();
            strActualID = fMount ? strNewID : strCurrentID;
            break;
        }
        /* Do we have a resent location? */
        case UIMediumTarget::UIMediumTargetType_WithLocation:
        {
            /* Open medium by location and get new medium ID if any: */
            const QString strNewID = openMedium(target.mediumType, target.data);
            /* Else just exit: */
            if (strNewID.isEmpty())
                return;

            /* Should we mount or unmount? */
            fMount = strNewID != strCurrentID;

            /* Prepare target medium: */
            const UIMedium guiMedium = fMount ? medium(strNewID) : UIMedium();
            comMedium = fMount ? guiMedium.medium() : CMedium();
            strActualID = fMount ? strNewID : strCurrentID;
            break;
        }
    }

    /* Do not unmount hard-drives: */
    if (target.mediumType == UIMediumType_HardDisk && !fMount)
        return;

    /* Get editable machine: */
    CSession comSession;
    CMachine comMachine = comConstMachine;
    KSessionState enmSessionState = comMachine.GetSessionState();
    /* Session state unlocked? */
    if (enmSessionState == KSessionState_Unlocked)
    {
        /* Open own 'write' session: */
        comSession = openSession(comMachine.GetId());
        AssertReturnVoid(!comSession.isNull());
        comMachine = comSession.GetMachine();
    }
    /* Is it Selector UI call? */
    else if (!isVMConsoleProcess())
    {
        /* Open existing 'shared' session: */
        comSession = openExistingSession(comMachine.GetId());
        AssertReturnVoid(!comSession.isNull());
        comMachine = comSession.GetMachine();
    }
    /* Else this is Runtime UI call
     * which has session locked for itself. */

    /* Remount medium to the predefined port/device: */
    bool fWasMounted = false;
    /* Hard drive case: */
    if (target.mediumType == UIMediumType_HardDisk)
    {
        /* Detaching: */
        comMachine.DetachDevice(target.name, target.port, target.device);
        fWasMounted = comMachine.isOk();
        if (!fWasMounted)
            msgCenter().cannotDetachDevice(comMachine, UIMediumType_HardDisk, strCurrentLocation,
                                           StorageSlot(enmCurrentStorageBus, target.port, target.device));
        else
        {
            /* Attaching: */
            comMachine.AttachDevice(target.name, target.port, target.device, KDeviceType_HardDisk, comMedium);
            fWasMounted = comMachine.isOk();
            if (!fWasMounted)
                msgCenter().cannotAttachDevice(comMachine, UIMediumType_HardDisk, strCurrentLocation,
                                               StorageSlot(enmCurrentStorageBus, target.port, target.device));
        }
    }
    /* Optical/floppy drive case: */
    else
    {
        /* Remounting: */
        comMachine.MountMedium(target.name, target.port, target.device, comMedium, false /* force? */);
        fWasMounted = comMachine.isOk();
        if (!fWasMounted)
        {
            /* Ask for force remounting: */
            if (msgCenter().cannotRemountMedium(comMachine, medium(strActualID),
                                                fMount, true /* retry? */))
            {
                /* Force remounting: */
                comMachine.MountMedium(target.name, target.port, target.device, comMedium, true /* force? */);
                fWasMounted = comMachine.isOk();
                if (!fWasMounted)
                    msgCenter().cannotRemountMedium(comMachine, medium(strActualID),
                                                    fMount, false /* retry? */);
            }
        }
        /* If mounting was successful: */
        if (fWasMounted)
        {
            /* Disable First RUN Wizard: */
            if (gEDataManager->machineFirstTimeStarted(comMachine.GetId()))
                gEDataManager->setMachineFirstTimeStarted(false, comMachine.GetId());
        }
    }

    /* Save settings: */
    if (fWasMounted)
    {
        comMachine.SaveSettings();
        if (!comMachine.isOk())
            msgCenter().cannotSaveMachineSettings(comMachine, windowManager().mainWindowShown());
    }

    /* Close session to editable comMachine if necessary: */
    if (!comSession.isNull())
        comSession.UnlockMachine();
}

QString VBoxGlobal::details(const CMedium &comMedium, bool fPredictDiff, bool fUseHtml /* = true */)
{
    /* Search for corresponding UI medium: */
    const QString strMediumID = comMedium.isNull() ? UIMedium::nullID() : comMedium.GetId();
    UIMedium guiMedium = medium(strMediumID);
    if (!comMedium.isNull() && guiMedium.isNull())
    {
        /* UI medium may be new and not among our media, request enumeration: */
        startMediumEnumeration();

        /* Search for corresponding UI medium again: */
        guiMedium = medium(strMediumID);
        if (guiMedium.isNull())
        {
            /* Medium might be deleted already, return null string: */
            return QString();
        }
    }

    /* Return UI medium details: */
    return fUseHtml ? guiMedium.detailsHTML(true /* no diffs? */, fPredictDiff) :
                      guiMedium.details(true /* no diffs? */, fPredictDiff);
}

void VBoxGlobal::updateRecentlyUsedMediumListAndFolder(UIMediumType enmMediumType, QString strMediumLocation)
{
       /* Remember the path of the last chosen medium: */
    switch (enmMediumType)
    {
        case UIMediumType_HardDisk: gEDataManager->setRecentFolderForHardDrives(QFileInfo(strMediumLocation).absolutePath()); break;
        case UIMediumType_DVD:      gEDataManager->setRecentFolderForOpticalDisks(QFileInfo(strMediumLocation).absolutePath()); break;
        case UIMediumType_Floppy:   gEDataManager->setRecentFolderForFloppyDisks(QFileInfo(strMediumLocation).absolutePath()); break;
        default: break;
    }

    /* Update recently used list: */
    QStringList recentMediumList;
    switch (enmMediumType)
    {
        case UIMediumType_HardDisk: recentMediumList = gEDataManager->recentListOfHardDrives(); break;
        case UIMediumType_DVD:      recentMediumList = gEDataManager->recentListOfOpticalDisks(); break;
        case UIMediumType_Floppy:   recentMediumList = gEDataManager->recentListOfFloppyDisks(); break;
        default: break;
    }
    if (recentMediumList.contains(strMediumLocation))
        recentMediumList.removeAll(strMediumLocation);
    recentMediumList.prepend(strMediumLocation);
    while(recentMediumList.size() > 5)
        recentMediumList.removeLast();
    switch (enmMediumType)
    {
        case UIMediumType_HardDisk: gEDataManager->setRecentListOfHardDrives(recentMediumList); break;
        case UIMediumType_DVD:      gEDataManager->setRecentListOfOpticalDisks(recentMediumList); break;
        case UIMediumType_Floppy:   gEDataManager->setRecentListOfFloppyDisks(recentMediumList); break;
        default: break;
    }
}

#ifdef RT_OS_LINUX
/* static */
void VBoxGlobal::checkForWrongUSBMounted()
{
    /* Make sure '/proc/mounts' exists and can be opened: */
    QFile file("/proc/mounts");
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    /* Fetch contents: */
    QStringList contents;
    for (;;)
    {
        QByteArray line = file.readLine();
        if (line.isEmpty())
            break;
        contents << line;
    }
    /* Grep contents for usbfs presence: */
    QStringList grep1(contents.filter("/sys/bus/usb/drivers"));
    QStringList grep2(grep1.filter("usbfs"));
    if (grep2.isEmpty())
        return;

    /* Show corresponding warning: */
    msgCenter().warnAboutWrongUSBMounted();
}
#endif /* RT_OS_LINUX */

/* static */
QString VBoxGlobal::details(const CUSBDevice &comDevice)
{
    QString strDetails;
    if (comDevice.isNull())
        strDetails = tr("Unknown device", "USB device details");
    else
    {
        QVector<QString> devInfoVector = comDevice.GetDeviceInfo();
        QString strManufacturer;
        QString strProduct;

        if (devInfoVector.size() >= 1)
            strManufacturer = devInfoVector[0].trimmed();
        if (devInfoVector.size() >= 2)
            strProduct = devInfoVector[1].trimmed();

        if (strManufacturer.isEmpty() && strProduct.isEmpty())
        {
            strDetails =
                tr("Unknown device %1:%2", "USB device details")
                   .arg(QString().sprintf("%04hX", comDevice.GetVendorId()))
                   .arg(QString().sprintf("%04hX", comDevice.GetProductId()));
        }
        else
        {
            if (strProduct.toUpper().startsWith(strManufacturer.toUpper()))
                strDetails = strProduct;
            else
                strDetails = strManufacturer + " " + strProduct;
        }
        ushort iRev = comDevice.GetRevision();
        if (iRev != 0)
            strDetails += QString().sprintf(" [%04hX]", iRev);
    }

    return strDetails.trimmed();
}

/* static */
QString VBoxGlobal::toolTip(const CUSBDevice &comDevice)
{
    QString strTip =
        tr("<nobr>Vendor ID: %1</nobr><br>"
           "<nobr>Product ID: %2</nobr><br>"
           "<nobr>Revision: %3</nobr>", "USB device tooltip")
           .arg(QString().sprintf("%04hX", comDevice.GetVendorId()))
           .arg(QString().sprintf("%04hX", comDevice.GetProductId()))
           .arg(QString().sprintf("%04hX", comDevice.GetRevision()));

    const QString strSerial = comDevice.GetSerialNumber();
    if (!strSerial.isEmpty())
        strTip += QString(tr("<br><nobr>Serial No. %1</nobr>", "USB device tooltip"))
                             .arg(strSerial);

    /* Add the state field if it's a host USB device: */
    CHostUSBDevice hostDev(comDevice);
    if (!hostDev.isNull())
    {
        strTip += QString(tr("<br><nobr>State: %1</nobr>", "USB device tooltip"))
                             .arg(gpConverter->toString(hostDev.GetState()));
    }

    return strTip;
}

/* static */
QString VBoxGlobal::toolTip(const CUSBDeviceFilter &comFilter)
{
    QString strTip;

    const QString strVendorId = comFilter.GetVendorId();
    if (!strVendorId.isEmpty())
        strTip += tr("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip")
                     .arg(strVendorId);

    const QString strProductId = comFilter.GetProductId();
    if (!strProductId.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product ID: %2</nobr>", "USB filter tooltip")
                                                     .arg(strProductId);

    const QString strRevision = comFilter.GetRevision();
    if (!strRevision.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Revision: %3</nobr>", "USB filter tooltip")
                                                     .arg(strRevision);

    const QString strProduct = comFilter.GetProduct();
    if (!strProduct.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product: %4</nobr>", "USB filter tooltip")
                                                     .arg(strProduct);

    const QString strManufacturer = comFilter.GetManufacturer();
    if (!strManufacturer.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip")
                                                     .arg(strManufacturer);

    const QString strSerial = comFilter.GetSerialNumber();
    if (!strSerial.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Serial No.: %1</nobr>", "USB filter tooltip")
                                                     .arg(strSerial);

    const QString strPort = comFilter.GetPort();
    if (!strPort.isEmpty())
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>Port: %1</nobr>", "USB filter tooltip")
                                                     .arg(strPort);

    /* Add the state field if it's a host USB device: */
    CHostUSBDevice hostDev(comFilter);
    if (!hostDev.isNull())
    {
        strTip += strTip.isEmpty() ? "":"<br/>" + tr("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                     .arg(gpConverter->toString(hostDev.GetState()));
    }

    return strTip;
}

/* static */
QString VBoxGlobal::toolTip(const CHostVideoInputDevice &comWebcam)
{
    QStringList records;

    const QString strName = comWebcam.GetName();
    if (!strName.isEmpty())
        records << strName;

    const QString strPath = comWebcam.GetPath();
    if (!strPath.isEmpty())
        records << strPath;

    return records.join("<br>");
}

/* static */
void VBoxGlobal::doExtPackInstallation(QString const &strFilePath, QString const &strDigest,
                                       QWidget *pParent, QString *pstrExtPackName) const
{
    /* Open the extpack tarball via IExtPackManager: */
    CExtPackManager comManager = virtualBox().GetExtensionPackManager();
    CExtPackFile comExtPackFile;
    if (strDigest.isEmpty())
        comExtPackFile = comManager.OpenExtPackFile(strFilePath);
    else
    {
        QString strFileAndHash = QString("%1::SHA-256=%2").arg(strFilePath).arg(strDigest);
        comExtPackFile = comManager.OpenExtPackFile(strFileAndHash);
    }
    if (!comManager.isOk())
    {
        msgCenter().cannotOpenExtPack(strFilePath, comManager, pParent);
        return;
    }

    if (!comExtPackFile.GetUsable())
    {
        msgCenter().warnAboutBadExtPackFile(strFilePath, comExtPackFile, pParent);
        return;
    }

    const QString strPackName = comExtPackFile.GetName();
    const QString strPackDescription = comExtPackFile.GetDescription();
    const QString strPackVersion = QString("%1r%2%3").arg(comExtPackFile.GetVersion()).arg(comExtPackFile.GetRevision()).arg(comExtPackFile.GetEdition());

    /* Check if there is a version of the extension pack already
     * installed on the system and let the user decide what to do about it. */
    CExtPack comExtPackCur = comManager.Find(strPackName);
    bool fReplaceIt = comExtPackCur.isOk();
    if (fReplaceIt)
    {
        QString strPackVersionCur = QString("%1r%2%3").arg(comExtPackCur.GetVersion()).arg(comExtPackCur.GetRevision()).arg(comExtPackCur.GetEdition());
        if (!msgCenter().confirmReplaceExtensionPack(strPackName, strPackVersion, strPackVersionCur, strPackDescription, pParent))
            return;
    }
    /* If it's a new package just ask for general confirmation. */
    else
    {
        if (!msgCenter().confirmInstallExtensionPack(strPackName, strPackVersion, strPackDescription, pParent))
            return;
    }

    /* Display the license dialog if required by the extension pack. */
    if (comExtPackFile.GetShowLicense())
    {
        QString strLicense = comExtPackFile.GetLicense();
        VBoxLicenseViewer licenseViewer(pParent);
        if (licenseViewer.showLicenseFromString(strLicense) != QDialog::Accepted)
            return;
    }

    /* Install the selected package.
     * Set the package name return value before doing
     * this as the caller should do a refresh even on failure. */
    QString strDisplayInfo;
#ifdef VBOX_WS_WIN
    if (pParent)
        strDisplayInfo.sprintf("hwnd=%#llx", (uint64_t)(uintptr_t)pParent->winId());
#endif
    /* Prepare installation progress: */
    CProgress comProgress = comExtPackFile.Install(fReplaceIt, strDisplayInfo);
    if (comExtPackFile.isOk())
    {
        /* Show installation progress: */
        /** @todo move this tr into UIUpdateManager context */
        msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIGlobalSettingsExtension",
                                                                              "Extensions"),
                                            ":/progress_install_guest_additions_90px.png", pParent);
        if (!comProgress.GetCanceled())
        {
            if (comProgress.isOk() && comProgress.GetResultCode() == 0)
                msgCenter().warnAboutExtPackInstalled(strPackName, pParent);
            else
                msgCenter().cannotInstallExtPack(comProgress, strFilePath, pParent);
        }
    }
    else
        msgCenter().cannotInstallExtPack(comExtPackFile, strFilePath, pParent);

    if (pstrExtPackName)
        *pstrExtPackName = strPackName;
}

bool VBoxGlobal::is3DAvailableWorker() const
{
    /* Rational is that when starting a text mode guest (like DOS) that does not
     * have 3D enabled, don't wast the developer's or user's time on launching the
     * test application when starting the VM or editing it's settings.
     *
     * Keep in mind that if we ever end up checking this concurrently on multiple threads,
     * use a RTONCE construct to serialize the efforts. */

#ifdef VBOX_WITH_CROGL
    bool fSupported = VBoxOglIs3DAccelerationSupported();
#else
    bool fSupported = false;
#endif
    m_i3DAvailable = fSupported;
    return fSupported;
}

bool VBoxGlobal::is3DAvailable() const
{
    if (m_i3DAvailable < 0)
        return is3DAvailableWorker();
    return m_i3DAvailable != 0;
}

#ifdef VBOX_WITH_CRHGSMI
/* static */
bool VBoxGlobal::isWddmCompatibleOsType(const QString &strGuestOSTypeId)
{
    return    strGuestOSTypeId.startsWith("WindowsVista")
           || strGuestOSTypeId.startsWith("Windows7")
           || strGuestOSTypeId.startsWith("Windows8")
           || strGuestOSTypeId.startsWith("Windows81")
           || strGuestOSTypeId.startsWith("Windows10")
           || strGuestOSTypeId.startsWith("Windows2008")
           || strGuestOSTypeId.startsWith("Windows2012");
}
#endif /* VBOX_WITH_CRHGSMI */

/* static */
quint64 VBoxGlobal::requiredVideoMemory(const QString &strGuestOSTypeId, int cMonitors /* = 1 */)
{
    /* We create a list of the size of all available host monitors. This list
     * is sorted by value and by starting with the biggest one, we calculate
     * the memory requirements for every guest screen. This is of course not
     * correct, but as we can't predict on which host screens the user will
     * open the guest windows, this is the best assumption we can do, cause it
     * is the worst case. */
    const int cHostScreens = gpDesktop->screenCount();
    QVector<int> screenSize(qMax(cMonitors, cHostScreens), 0);
    for (int i = 0; i < cHostScreens; ++i)
    {
        QRect r = gpDesktop->screenGeometry(i);
        screenSize[i] = r.width() * r.height();
    }
    /* Now sort the vector: */
    qSort(screenSize.begin(), screenSize.end(), qGreater<int>());
    /* For the case that there are more guest screens configured then host
     * screens available, replace all zeros with the greatest value in the
     * vector. */
    for (int i = 0; i < screenSize.size(); ++i)
        if (screenSize.at(i) == 0)
            screenSize.replace(i, screenSize.at(0));

    quint64 uNeedBits = 0;
    for (int i = 0; i < cMonitors; ++i)
    {
        /* Calculate summary required memory amount in bits: */
        uNeedBits += (screenSize.at(i) * /* with x height */
                     32 + /* we will take the maximum possible bpp for now */
                     8 * _1M) + /* current cache per screen - may be changed in future */
                     8 * 4096; /* adapter info */
    }
    /* Translate value into megabytes with rounding to highest side: */
    quint64 uNeedMBytes = uNeedBits % (8 * _1M)
                        ? uNeedBits / (8 * _1M) + 1
                        : uNeedBits / (8 * _1M) /* convert to megabytes */;

    if (strGuestOSTypeId.startsWith("Windows"))
    {
        /* Windows guests need offscreen VRAM too for graphics acceleration features: */
#ifdef VBOX_WITH_CRHGSMI
        if (isWddmCompatibleOsType(strGuestOSTypeId))
        {
            /* WDDM mode, there are two surfaces for each screen: shadow & primary: */
            uNeedMBytes *= 3;
        }
        else
#endif /* VBOX_WITH_CRHGSMI */
        {
            uNeedMBytes *= 2;
        }
    }

    return uNeedMBytes * _1M;
}

QIcon VBoxGlobal::vmUserIcon(const CMachine &comMachine) const
{
    /* Prepare fallback icon: */
    static QIcon nullIcon;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullIcon);

    /* Redirect to general icon-pool: */
    return m_pIconPool->userMachineIcon(comMachine);
}

QPixmap VBoxGlobal::vmUserPixmap(const CMachine &comMachine, const QSize &size) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullPixmap);

    /* Redirect to general icon-pool: */
    return m_pIconPool->userMachinePixmap(comMachine, size);
}

QPixmap VBoxGlobal::vmUserPixmapDefault(const CMachine &comMachine, QSize *pLogicalSize /* = 0 */) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullPixmap);

    /* Redirect to general icon-pool: */
    return m_pIconPool->userMachinePixmapDefault(comMachine, pLogicalSize);
}

QIcon VBoxGlobal::vmGuestOSTypeIcon(const QString &strOSTypeID) const
{
    /* Prepare fallback icon: */
    static QIcon nullIcon;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullIcon);

    /* Redirect to general icon-pool: */
    return m_pIconPool->guestOSTypeIcon(strOSTypeID);
}

QPixmap VBoxGlobal::vmGuestOSTypePixmap(const QString &strOSTypeID, const QSize &size) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullPixmap);

    /* Redirect to general icon-pool: */
    return m_pIconPool->guestOSTypePixmap(strOSTypeID, size);
}

QPixmap VBoxGlobal::vmGuestOSTypePixmapDefault(const QString &strOSTypeID, QSize *pLogicalSize /* = 0 */) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* Make sure general icon-pool initialized: */
    AssertReturn(m_pIconPool, nullPixmap);

    /* Redirect to general icon-pool: */
    return m_pIconPool->guestOSTypePixmapDefault(strOSTypeID, pLogicalSize);
}

/* static */
QPixmap VBoxGlobal::joinPixmaps(const QPixmap &pixmap1, const QPixmap &pixmap2)
{
    if (pixmap1.isNull())
        return pixmap2;
    if (pixmap2.isNull())
        return pixmap1;

    QPixmap result(pixmap1.width() + pixmap2.width() + 2,
                   qMax(pixmap1.height(), pixmap2.height()));
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.drawPixmap(0, 0, pixmap1);
    painter.drawPixmap(pixmap1.width() + 2, result.height() - pixmap2.height(), pixmap2);
    painter.end();

    return result;
}

bool VBoxGlobal::openURL(const QString &strUrl) const
{
    /** Service event. */
    class ServiceEvent : public QEvent
    {
    public:

        /** Constructs service event on th basis of passed @a fResult. */
        ServiceEvent(bool fResult)
            : QEvent(QEvent::User)
            , m_fResult(fResult)
        {}

        /** Returns the result which event brings. */
        bool result() const { return m_fResult; }

    private:

        /** Holds the result which event brings. */
        bool m_fResult;
    };

    /** Service client object. */
    class ServiceClient : public QEventLoop
    {
    public:

        /** Constructs service client on the basis of passed @a fResult. */
        ServiceClient()
            : m_fResult(false)
        {}

        /** Returns the result which event brings. */
        bool result() const { return m_fResult; }

    private:

        /** Handles any Qt @a pEvent. */
        bool event(QEvent *pEvent)
        {
            /* Handle service event: */
            if (pEvent->type() == QEvent::User)
            {
                ServiceEvent *pServiceEvent = static_cast<ServiceEvent*>(pEvent);
                m_fResult = pServiceEvent->result();
                pServiceEvent->accept();
                quit();
                return true;
            }
            return false;
        }

        bool m_fResult;
    };

    /** Service server object. */
    class ServiceServer : public QThread
    {
    public:

        /** Constructs service server on the basis of passed @a client and @a strUrl. */
        ServiceServer(ServiceClient &client, const QString &strUrl)
            : m_client(client), m_strUrl(strUrl) {}

    private:

        /** Executes thread task. */
        void run()
        {
            QApplication::postEvent(&m_client, new ServiceEvent(QDesktopServices::openUrl(m_strUrl)));
        }

        /** Holds the client reference. */
        ServiceClient &m_client;
        /** Holds the URL to be processed. */
        const QString &m_strUrl;
    };

    /* Create client & server: */
    ServiceClient client;
    ServiceServer server(client, strUrl);
    server.start();
    client.exec();
    server.wait();

    /* Acquire client result: */
    bool fResult = client.result();

    if (!fResult)
        msgCenter().cannotOpenURL(strUrl);

    return fResult;
}

void VBoxGlobal::sltGUILanguageChange(QString strLanguage)
{
    /* Make sure medium-enumeration is not in progress! */
    AssertReturnVoid(!isMediumEnumerationInProgress());
    /* Load passed language: */
    loadLanguage(strLanguage);
}

bool VBoxGlobal::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /** @todo Just use the QIWithRetranslateUI3 template wrapper. */

    if (   pEvent->type() == QEvent::LanguageChange
        && pObject->isWidgetType()
        && static_cast<QWidget*>(pObject)->isTopLevel())
    {
        /* Catch the language change event before any other widget gets it in
         * order to invalidate cached string resources (like the details view
         * templates) that may be used by other widgets. */
        QWidgetList list = QApplication::topLevelWidgets();
        if (list.first() == pObject)
        {
            /* Call this only once per every language change (see
             * QApplication::installTranslator() for details): */
            retranslateUi();
        }
    }

    /* Call to base-class: */
    return QObject::eventFilter(pObject, pEvent);
}

void VBoxGlobal::retranslateUi()
{
    s_strUserDefinedPortName = tr("User-defined", "serial port");

    m_pixWarning = UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_MessageBoxWarning).pixmap(16, 16);
    Assert(!m_pixWarning.isNull());

    m_pixError = UIIconPool::defaultIcon(UIIconPool::UIDefaultIconType_MessageBoxCritical).pixmap(16, 16);
    Assert(!m_pixError.isNull());

    /* Re-enumerate uimedium since they contain some translations too: */
    if (m_fValid)
        refreshMedia();

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // As X11 do not have functionality for providing human readable key names,
    // we keep a table of them, which must be updated when the language is changed.
    UINativeHotKey::retranslateKeyNames();
#endif
}

void VBoxGlobal::prepare()
{
    /* Make sure QApplication cleanup us on exit: */
    connect(qApp, &QGuiApplication::aboutToQuit, this, &VBoxGlobal::cleanup);
    /* Make sure we handle host OS session shutdown as well: */
    connect(qApp, &QGuiApplication::commitDataRequest,
            this, &VBoxGlobal::sltHandleCommitDataRequest);

#ifdef VBOX_WS_MAC
    /* Determine OS release early: */
    m_enmMacOSVersion = determineOsRelease();
#endif /* VBOX_WS_MAC */

    /* Prepare converter: */
    UIConverter::prepare();

    /* Create desktop-widget watchdog: */
    UIDesktopWidgetWatchdog::create();

    /* Create message-center: */
    UIMessageCenter::create();
    /* Create popup-center: */
    UIPopupCenter::create();

    /* Prepare general icon-pool: */
    m_pIconPool = new UIIconPoolGeneral;

    /* Load translation based on the current locale: */
    loadLanguage();

    HRESULT rc = COMBase::InitializeCOM(true);
    if (FAILED(rc))
    {
#ifdef VBOX_WITH_XPCOM
        if (rc == NS_ERROR_FILE_ACCESS_DENIED)
        {
            char szHome[RTPATH_MAX] = "";
            com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
            msgCenter().cannotInitUserHome(QString(szHome));
        }
        else
#endif
            msgCenter().cannotInitCOM(rc);
        return;
    }

#ifdef VBOX_WITH_SDS
    // setup Client COM Security to enable impersonation required by VBOX_SDS
    HRESULT hrGUICoInitializeSecurity = CoInitializeSecurity(NULL,
                                                             -1,
                                                             NULL,
                                                             NULL,
                                                             RPC_C_AUTHN_LEVEL_DEFAULT,
                                                             RPC_C_IMP_LEVEL_IMPERSONATE,
                                                             NULL,
                                                             EOAC_NONE,
                                                             NULL);
    NOREF(hrGUICoInitializeSecurity);
    Assert(RPC_E_TOO_LATE != hrGUICoInitializeSecurity);
    Assert(hrGUICoInitializeSecurity == S_OK);
#endif

    /* Make sure VirtualBoxClient instance created: */
    m_comVBoxClient.createInstance(CLSID_VirtualBoxClient);
    if (!m_comVBoxClient.isOk())
    {
        msgCenter().cannotCreateVirtualBoxClient(m_comVBoxClient);
        return;
    }
    /* Make sure VirtualBox instance acquired: */
    m_comVBox = m_comVBoxClient.GetVirtualBox();
    if (!m_comVBoxClient.isOk())
    {
        msgCenter().cannotAcquireVirtualBox(m_comVBoxClient);
        return;
    }
    /* Init wrappers: */
    comWrappersReinit();

    /* Watch for the VBoxSVC availability changes: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigVBoxSVCAvailabilityChange,
            this, &VBoxGlobal::sltHandleVBoxSVCAvailabilityChange);

    /* Prepare thread-pool instance: */
    m_pThreadPool = new UIThreadPool(3 /* worker count */, 5000 /* worker timeout */);

    /* Load translation based on the user settings: */
    QString sLanguageId = gEDataManager->languageId();
    if (!sLanguageId.isNull())
        loadLanguage(sLanguageId);

    retranslateUi();

    connect(gEDataManager, &UIExtraDataManager::sigLanguageChange,
            this, &VBoxGlobal::sltGUILanguageChange);

    qApp->installEventFilter(this);

    /* process command line */

    UIVisualStateType visualStateType = UIVisualStateType_Invalid;

#ifdef VBOX_WS_X11
    /* Check whether we have compositing manager running: */
    m_fCompositingManagerRunning = X11IsCompositingManagerRunning();

    /* Acquire current Window Manager type: */
    m_enmWindowManagerType = X11WindowManagerType();
#endif /* VBOX_WS_X11 */

#ifdef VBOX_WITH_DEBUGGER_GUI
# ifdef VBOX_WITH_DEBUGGER_GUI_MENU
    initDebuggerVar(&m_fDbgEnabled, "VBOX_GUI_DBG_ENABLED", GUI_Dbg_Enabled, true);
# else
    initDebuggerVar(&m_fDbgEnabled, "VBOX_GUI_DBG_ENABLED", GUI_Dbg_Enabled, false);
# endif
    initDebuggerVar(&m_fDbgAutoShow, "VBOX_GUI_DBG_AUTO_SHOW", GUI_Dbg_AutoShow, false);
    m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = m_fDbgAutoShow;
#endif

    /*
     * Parse the command line options.
     *
     * This is a little sloppy but we're trying to tighten it up.  Unfortuately,
     * both on X11 and darwin (IIRC) there might be additional arguments aimed
     * for client libraries with GUI processes.  So, using RTGetOpt or similar
     * is a bit hard since we have to cope with unknown options.
     */
    m_fShowStartVMErrors = true;
    bool startVM = false;
    bool fSeparateProcess = false;
    QString vmNameOrUuid;

    const QStringList &arguments = QCoreApplication::arguments();
    const int argc = arguments.size();
    int i = 1;
    while (i < argc)
    {
        const QByteArray &argBytes = arguments.at(i).toUtf8();
        const char *arg = argBytes.constData();
        enum { OptType_Unknown, OptType_VMRunner, OptType_VMSelector, OptType_MaybeBoth } enmOptType = OptType_Unknown;
        /* NOTE: the check here must match the corresponding check for the
         * options to start a VM in main.cpp and hardenedmain.cpp exactly,
         * otherwise there will be weird error messages. */
        if (   !::strcmp(arg, "--startvm")
            || !::strcmp(arg, "-startvm"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
            {
                vmNameOrUuid = arguments.at(i);
                startVM = true;
            }
        }
        else if (!::strcmp(arg, "-separate") || !::strcmp(arg, "--separate"))
        {
            enmOptType = OptType_VMRunner;
            fSeparateProcess = true;
        }
#ifdef VBOX_GUI_WITH_PIDFILE
        else if (!::strcmp(arg, "-pidfile") || !::strcmp(arg, "--pidfile"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
                m_strPidFile = arguments.at(i);
        }
#endif /* VBOX_GUI_WITH_PIDFILE */
        /* Visual state type options: */
        else if (!::strcmp(arg, "-normal") || !::strcmp(arg, "--normal"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Normal;
        }
        else if (!::strcmp(arg, "-fullscreen") || !::strcmp(arg, "--fullscreen"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Fullscreen;
        }
        else if (!::strcmp(arg, "-seamless") || !::strcmp(arg, "--seamless"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Seamless;
        }
        else if (!::strcmp(arg, "-scale") || !::strcmp(arg, "--scale"))
        {
            enmOptType = OptType_MaybeBoth;
            visualStateType = UIVisualStateType_Scale;
        }
        /* Passwords: */
        else if (!::strcmp(arg, "--settingspw"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
            {
                RTStrCopy(m_astrSettingsPw, sizeof(m_astrSettingsPw), arguments.at(i).toLocal8Bit().constData());
                m_fSettingsPwSet = true;
            }
        }
        else if (!::strcmp(arg, "--settingspwfile"))
        {
            enmOptType = OptType_MaybeBoth;
            if (++i < argc)
            {
                const QByteArray &argFileBytes = arguments.at(i).toLocal8Bit();
                const char *pszFile = argFileBytes.constData();
                bool fStdIn = !::strcmp(pszFile, "stdin");
                int vrc = VINF_SUCCESS;
                PRTSTREAM pStrm;
                if (!fStdIn)
                    vrc = RTStrmOpen(pszFile, "r", &pStrm);
                else
                    pStrm = g_pStdIn;
                if (RT_SUCCESS(vrc))
                {
                    size_t cbFile;
                    vrc = RTStrmReadEx(pStrm, m_astrSettingsPw, sizeof(m_astrSettingsPw) - 1, &cbFile);
                    if (RT_SUCCESS(vrc))
                    {
                        if (cbFile >= sizeof(m_astrSettingsPw) - 1)
                            cbFile = sizeof(m_astrSettingsPw) - 1;
                        unsigned i;
                        for (i = 0; i < cbFile && !RT_C_IS_CNTRL(m_astrSettingsPw[i]); i++)
                            ;
                        m_astrSettingsPw[i] = '\0';
                        m_fSettingsPwSet = true;
                    }
                    if (!fStdIn)
                        RTStrmClose(pStrm);
                }
            }
        }
        /* Misc options: */
        else if (!::strcmp(arg, "-comment") || !::strcmp(arg, "--comment"))
        {
            enmOptType = OptType_MaybeBoth;
            ++i;
        }
        else if (!::strcmp(arg, "--no-startvm-errormsgbox"))
        {
            enmOptType = OptType_VMRunner;
            m_fShowStartVMErrors = false;
        }
        else if (!::strcmp(arg, "--aggressive-caching"))
        {
            enmOptType = OptType_MaybeBoth;
            m_fAgressiveCaching = true;
        }
        else if (!::strcmp(arg, "--no-aggressive-caching"))
        {
            enmOptType = OptType_MaybeBoth;
            m_fAgressiveCaching = false;
        }
        else if (!::strcmp(arg, "--restore-current"))
        {
            enmOptType = OptType_VMRunner;
            m_fRestoreCurrentSnapshot = true;
        }
        /* Ad hoc VM reconfig options: */
        else if (!::strcmp(arg, "--fda"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_strFloppyImage = arguments.at(i);
        }
        else if (!::strcmp(arg, "--dvd") || !::strcmp(arg, "--cdrom"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_strDvdImage = arguments.at(i);
        }
        /* VMM Options: */
        else if (!::strcmp(arg, "--disable-patm"))
        {
            enmOptType = OptType_VMRunner;
            m_fDisablePatm = true;
        }
        else if (!::strcmp(arg, "--disable-csam"))
        {
            enmOptType = OptType_VMRunner;
            m_fDisableCsam = true;
        }
        else if (!::strcmp(arg, "--recompile-supervisor"))
        {
            enmOptType = OptType_VMRunner;
            m_fRecompileSupervisor = true;
        }
        else if (!::strcmp(arg, "--recompile-user"))
        {
            enmOptType = OptType_VMRunner;
            m_fRecompileUser = true;
        }
        else if (!::strcmp(arg, "--recompile-all"))
        {
            enmOptType = OptType_VMRunner;
            m_fDisablePatm = m_fDisableCsam = m_fRecompileSupervisor = m_fRecompileUser = true;
        }
        else if (!::strcmp(arg, "--execute-all-in-iem"))
        {
            enmOptType = OptType_VMRunner;
            m_fDisablePatm = m_fDisableCsam = m_fExecuteAllInIem = true;
        }
        else if (!::strcmp(arg, "--warp-pct"))
        {
            enmOptType = OptType_VMRunner;
            if (++i < argc)
                m_uWarpPct = RTStrToUInt32(arguments.at(i).toLocal8Bit().constData());
        }
#ifdef VBOX_WITH_DEBUGGER_GUI
        /* Debugger/Debugging options: */
        else if (!::strcmp(arg, "-dbg") || !::strcmp(arg, "--dbg"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
        }
        else if (!::strcmp( arg, "-debug") || !::strcmp(arg, "--debug"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, true);
            setDebuggerVar(&m_fDbgAutoShowStatistics, true);
        }
        else if (!::strcmp(arg, "--debug-command-line"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, true);
        }
        else if (!::strcmp(arg, "--debug-statistics"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, true);
            setDebuggerVar(&m_fDbgAutoShow, true);
            setDebuggerVar(&m_fDbgAutoShowStatistics, true);
        }
        else if (!::strcmp(arg, "-no-debug") || !::strcmp(arg, "--no-debug"))
        {
            enmOptType = OptType_VMRunner;
            setDebuggerVar(&m_fDbgEnabled, false);
            setDebuggerVar(&m_fDbgAutoShow, false);
            setDebuggerVar(&m_fDbgAutoShowCommandLine, false);
            setDebuggerVar(&m_fDbgAutoShowStatistics, false);
        }
        /* Not quite debug options, but they're only useful with the debugger bits. */
        else if (!::strcmp(arg, "--start-paused"))
        {
            enmOptType = OptType_VMRunner;
            m_enmLaunchRunning = LaunchRunning_No;
        }
        else if (!::strcmp(arg, "--start-running"))
        {
            enmOptType = OptType_VMRunner;
            m_enmLaunchRunning = LaunchRunning_Yes;
        }
#endif
#ifdef RT_OS_WINDOWS /** @todo add more here, please... */
        else
            msgCenter().warnAboutUnknownOptionType(arguments.at(i));
#endif
#ifdef VBOX_GUI_WITH_SHARED_LIBRARY
        if (enmOptType == OptType_VMRunner && m_enmType != UIType_RuntimeUI)
            msgCenter().warnAboutUnrelatedOptionType(arg);
#endif

        i++;
    }

#ifndef VBOX_GUI_WITH_SHARED_LIBRARY
    if (startVM)
#else
    if (m_enmType == UIType_RuntimeUI && startVM)
#endif
    {
        /* m_fSeparateProcess makes sense only if a VM is started. */
        m_fSeparateProcess = fSeparateProcess;

        /* Search for corresponding VM: */
        QUuid uuid = QUuid(vmNameOrUuid);
        const CMachine machine = m_comVBox.FindMachine(vmNameOrUuid);
        if (!uuid.isNull())
        {
            if (machine.isNull() && showStartVMErrors())
                return msgCenter().cannotFindMachineById(m_comVBox, vmNameOrUuid);
        }
        else
        {
            if (machine.isNull() && showStartVMErrors())
                return msgCenter().cannotFindMachineByName(m_comVBox, vmNameOrUuid);
        }
        m_strManagedVMId = machine.GetId();
    }

    /* After initializing *m_strManagedVMId* we already know if that is VM process or not: */
    if (!isVMConsoleProcess())
    {
        /* We should create separate logging file for VM selector: */
        char szLogFile[RTPATH_MAX];
        const char *pszLogFile = NULL;
        com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        RTPathAppend(szLogFile, sizeof(szLogFile), "selectorwindow.log");
        pszLogFile = szLogFile;
        /* Create release logger, to file: */
        com::VBoxLogRelCreate("GUI VM Selector Window",
                              pszLogFile,
                              RTLOGFLAGS_PREFIX_TIME_PROG,
                              "all",
                              "VBOX_GUI_SELECTORWINDOW_RELEASE_LOG",
                              RTLOGDEST_FILE | RTLOGDEST_F_NO_DENY,
                              UINT32_MAX,
                              10,
                              60 * 60,
                              _1M,
                              NULL /*pErrInfo*/);

        LogRel(("Qt version: %s\n", qtRTVersionString().toUtf8().constData()));
    }

    if (m_fSettingsPwSet)
        m_comVBox.SetSettingsSecret(m_astrSettingsPw);

    if (visualStateType != UIVisualStateType_Invalid && !m_strManagedVMId.isEmpty())
        gEDataManager->setRequestedVisualState(visualStateType, m_strManagedVMId);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* setup the debugger gui. */
    if (RTEnvExist("VBOX_GUI_NO_DEBUGGER"))
        m_fDbgEnabled = m_fDbgAutoShow =  m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = false;
    if (m_fDbgEnabled)
    {
        RTERRINFOSTATIC ErrInfo;
        RTErrInfoInitStatic(&ErrInfo);
        int vrc = SUPR3HardenedLdrLoadAppPriv("VBoxDbg", &m_hVBoxDbg, RTLDRLOAD_FLAGS_LOCAL, &ErrInfo.Core);
        if (RT_FAILURE(vrc))
        {
            m_hVBoxDbg = NIL_RTLDRMOD;
            m_fDbgAutoShow = m_fDbgAutoShowCommandLine = m_fDbgAutoShowStatistics = false;
            LogRel(("Failed to load VBoxDbg, rc=%Rrc - %s\n", vrc, ErrInfo.Core.pszMsg));
        }
    }
#endif

    m_fValid = true;

    /* Create medium-enumerator but don't do any immediate caching: */
    m_pMediumEnumerator = new UIMediumEnumerator;
    {
        /* Prepare medium-enumerator: */
        connect(m_pMediumEnumerator, &UIMediumEnumerator::sigMediumCreated,
                this, &VBoxGlobal::sigMediumCreated);
        connect(m_pMediumEnumerator, &UIMediumEnumerator::sigMediumDeleted,
                this, &VBoxGlobal::sigMediumDeleted);
        connect(m_pMediumEnumerator, &UIMediumEnumerator::sigMediumEnumerationStarted,
                this, &VBoxGlobal::sigMediumEnumerationStarted);
        connect(m_pMediumEnumerator, &UIMediumEnumerator::sigMediumEnumerated,
                this, &VBoxGlobal::sigMediumEnumerated);
        connect(m_pMediumEnumerator, &UIMediumEnumerator::sigMediumEnumerationFinished,
                this, &VBoxGlobal::sigMediumEnumerationFinished);
    }

    /* Create shortcut pool: */
    UIShortcutPool::create();

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Create network manager: */
    UINetworkManager::create();

    /* Schedule update manager: */
    UIUpdateManager::schedule();
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

#ifdef RT_OS_LINUX
    /* Make sure no wrong USB mounted: */
    checkForWrongUSBMounted();
#endif /* RT_OS_LINUX */
}

void VBoxGlobal::cleanup()
{
    /// @todo Shouldn't that be protected with a mutex or something?
    /* Remember that the cleanup is in progress preventing any unwanted
     * stuff which could be called from the other threads: */
    s_fCleaningUp = true;

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* Shutdown update manager: */
    UIUpdateManager::shutdown();

    /* Destroy network manager: */
    UINetworkManager::destroy();
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /* Destroy shortcut pool: */
    UIShortcutPool::destroy();

#ifdef VBOX_GUI_WITH_PIDFILE
    deletePidfile();
#endif /* VBOX_GUI_WITH_PIDFILE */

    /* Starting medium-enumerator cleanup: */
    m_meCleanupProtectionToken.lockForWrite();
    {
        /* Destroy medium-enumerator: */
        delete m_pMediumEnumerator;
        m_pMediumEnumerator = 0;
    }
    /* Finishing medium-enumerator cleanup: */
    m_meCleanupProtectionToken.unlock();

    /* Destroy the global (VirtualBox) Main event handler
     * which is used in both Selector and Runtime UI. */
    UIVirtualBoxEventHandler::destroy();

    /* Destroy the extra-data manager finally after everything
     * above which could use it already destroyed: */
    UIExtraDataManager::destroy();

    /* Cleanup converter: */
    UIConverter::cleanup();

    /* Cleanup thread-pool: */
    delete m_pThreadPool;
    m_pThreadPool = 0;
    /* Cleanup general icon-pool: */
    delete m_pIconPool;
    m_pIconPool = 0;

    /* Ensure CGuestOSType objects are no longer used: */
    m_guestOSFamilyIDs.clear();
    m_guestOSTypes.clear();

    /* Starting COM cleanup: */
    m_comCleanupProtectionToken.lockForWrite();
    {
        /* First, make sure we don't use COM any more: */
        m_comHost.detach();
        m_comVBox.detach();
        m_comVBoxClient.detach();

        /* There may be UIMedium(s)EnumeratedEvent instances still in the message
         * queue which reference COM objects. Remove them to release those objects
         * before uninitializing the COM subsystem. */
        QApplication::removePostedEvents(this);

        /* Finally cleanup COM itself: */
        COMBase::CleanupCOM();
    }
    /* Finishing COM cleanup: */
    m_comCleanupProtectionToken.unlock();

    /* Destroy popup-center: */
    UIPopupCenter::destroy();
    /* Destroy message-center: */
    UIMessageCenter::destroy();

    /* Destroy desktop-widget watchdog: */
    UIDesktopWidgetWatchdog::destroy();

    m_fValid = false;
}

void VBoxGlobal::sltHandleCommitDataRequest(QSessionManager &manager)
{
    LogRel(("GUI: VBoxGlobal::sltHandleCommitDataRequest: Emergency shutdown initiated\n"));

    /* Ask listener to commit data: */
    emit sigAskToCommitData();

    /* Ask session manager to postpone shutdown until we done: */
    manager.cancel();

#ifdef VBOX_WS_WIN
    // WORKAROUND:
    // In theory that's Qt5 who should allow us to provide postponing reason as well,
    // but that functionality seems missed in Windows platform plugin, so we are making that ourselves.
    // That also implies that since we had postponed a shutdown process, host will send us WM_QUIT to
    // allow to properly do an application cleanup first. That signal will cause QApplication to quit().
    ShutdownBlockReasonCreateAPI((HWND)windowManager().mainWindowShown()->winId(), L"Shutdown in progress...");
#endif
}

void VBoxGlobal::sltHandleVBoxSVCAvailabilityChange(bool fAvailable)
{
    /* Make sure the VBoxSVC availability changed: */
    if (m_fVBoxSVCAvailable == fAvailable)
        return;

    /* Cache the new VBoxSVC availability value: */
    m_fVBoxSVCAvailable = fAvailable;

    /* If VBoxSVC is not available: */
    if (!m_fVBoxSVCAvailable)
    {
        /* Mark wrappers invalid: */
        m_fWrappersValid = false;
        /* Re-fetch corresponding CVirtualBox to restart VBoxSVC: */
        m_comVBox = m_comVBoxClient.GetVirtualBox();
        if (!m_comVBoxClient.isOk())
        {
            // The proper behavior would be to show the message and to exit the app, e.g.:
            // msgCenter().cannotAcquireVirtualBox(m_comVBoxClient);
            // return QApplication::quit();
            // But CVirtualBox is still NULL in current Main implementation,
            // and this call do not restart anything, so we are waiting
            // for subsequent event about VBoxSVC is available again.
        }
    }
    /* If VBoxSVC is available: */
    else
    {
        if (!m_fWrappersValid)
        {
            /* Re-fetch corresponding CVirtualBox: */
            m_comVBox = m_comVBoxClient.GetVirtualBox();
            if (!m_comVBoxClient.isOk())
            {
                msgCenter().cannotAcquireVirtualBox(m_comVBoxClient);
                return QApplication::quit();
            }
            /* Re-init wrappers: */
            comWrappersReinit();

            /* If that is Selector UI: */
            if (!isVMConsoleProcess())
            {
                /* Recreate Main event listeners: */
                UIVirtualBoxEventHandler::destroy();
                UIExtraDataManager::destroy();
                UIExtraDataManager::instance();
                UIVirtualBoxEventHandler::instance();
                /* Ask UIStarter to restart UI: */
                emit sigAskToRestartUI();
            }
        }
    }

    /* Notify listeners about the VBoxSVC availability change: */
    emit sigVBoxSVCAvailabilityChange();
}

#ifdef VBOX_WS_WIN
/* static */
BOOL VBoxGlobal::ShutdownBlockReasonCreateAPI(HWND hWnd, LPCWSTR pwszReason)
{
    BOOL fResult = FALSE;
    typedef BOOL(WINAPI *PFNSHUTDOWNBLOCKREASONCREATE)(HWND hWnd, LPCWSTR pwszReason);

    PFNSHUTDOWNBLOCKREASONCREATE pfn = (PFNSHUTDOWNBLOCKREASONCREATE)GetProcAddress(
        GetModuleHandle(L"User32.dll"), "ShutdownBlockReasonCreate");
    _ASSERTE(pfn);
    if (pfn)
        fResult = pfn(hWnd, pwszReason);
    return fResult;
}
#endif

#ifdef VBOX_WITH_DEBUGGER_GUI

# define VBOXGLOBAL_DBG_CFG_VAR_FALSE       (0)
# define VBOXGLOBAL_DBG_CFG_VAR_TRUE        (1)
# define VBOXGLOBAL_DBG_CFG_VAR_MASK        (1)
# define VBOXGLOBAL_DBG_CFG_VAR_CMD_LINE    RT_BIT(3)
# define VBOXGLOBAL_DBG_CFG_VAR_DONE        RT_BIT(4)

void VBoxGlobal::initDebuggerVar(int *piDbgCfgVar, const char *pszEnvVar, const char *pszExtraDataName, bool fDefault)
{
    QString strEnvValue;
    char    szEnvValue[256];
    int rc = RTEnvGetEx(RTENV_DEFAULT, pszEnvVar, szEnvValue, sizeof(szEnvValue), NULL);
    if (RT_SUCCESS(rc))
    {
        strEnvValue = QString::fromUtf8(&szEnvValue[0]).toLower().trimmed();
        if (strEnvValue.isEmpty())
            strEnvValue = "yes";
    }
    else if (rc != VERR_ENV_VAR_NOT_FOUND)
        strEnvValue = "veto";

    QString strExtraValue = m_comVBox.GetExtraData(pszExtraDataName).toLower().trimmed();
    if (strExtraValue.isEmpty())
        strExtraValue = QString();

    if ( strEnvValue.contains("veto") || strExtraValue.contains("veto"))
        *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_DONE | VBOXGLOBAL_DBG_CFG_VAR_FALSE;
    else if (strEnvValue.isNull() && strExtraValue.isNull())
        *piDbgCfgVar = fDefault ? VBOXGLOBAL_DBG_CFG_VAR_TRUE : VBOXGLOBAL_DBG_CFG_VAR_FALSE;
    else
    {
        QString *pStr = !strEnvValue.isEmpty() ? &strEnvValue : &strExtraValue;
        if (   pStr->startsWith("y")  // yes
            || pStr->startsWith("e")  // enabled
            || pStr->startsWith("t")  // true
            || pStr->startsWith("on")
            || pStr->toLongLong() != 0)
            *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_TRUE;
        else if (   pStr->startsWith("n")  // o
                 || pStr->startsWith("d")  // disable
                 || pStr->startsWith("f")  // false
                 || pStr->startsWith("off")
                 || pStr->contains("veto") /* paranoia */
                 || pStr->toLongLong() == 0)
            *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_FALSE;
        else
        {
            LogFunc(("Ignoring unknown value '%s' for '%s'\n", pStr->toUtf8().constData(), pStr == &strEnvValue ? pszEnvVar : pszExtraDataName));
            *piDbgCfgVar = fDefault ? VBOXGLOBAL_DBG_CFG_VAR_TRUE : VBOXGLOBAL_DBG_CFG_VAR_FALSE;
        }
    }
}

void VBoxGlobal::setDebuggerVar(int *piDbgCfgVar, bool fState)
{
    if (!(*piDbgCfgVar & VBOXGLOBAL_DBG_CFG_VAR_DONE))
        *piDbgCfgVar = (fState ? VBOXGLOBAL_DBG_CFG_VAR_TRUE : VBOXGLOBAL_DBG_CFG_VAR_FALSE)
                     | VBOXGLOBAL_DBG_CFG_VAR_CMD_LINE;
}

bool VBoxGlobal::isDebuggerWorker(int *piDbgCfgVar, const char *pszExtraDataName) const
{
    if (!(*piDbgCfgVar & VBOXGLOBAL_DBG_CFG_VAR_DONE))
    {
        const QString str = gEDataManager->debugFlagValue(pszExtraDataName);
        if (str.contains("veto"))
            *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_DONE | VBOXGLOBAL_DBG_CFG_VAR_FALSE;
        else if (str.isEmpty() || (*piDbgCfgVar & VBOXGLOBAL_DBG_CFG_VAR_CMD_LINE))
            *piDbgCfgVar |= VBOXGLOBAL_DBG_CFG_VAR_DONE;
        else if (   str.startsWith("y")  // yes
                 || str.startsWith("e")  // enabled
                 || str.startsWith("t")  // true
                 || str.startsWith("on")
                 || str.toLongLong() != 0)
            *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_DONE | VBOXGLOBAL_DBG_CFG_VAR_TRUE;
        else if (   str.startsWith("n")  // no
                 || str.startsWith("d")  // disable
                 || str.startsWith("f")  // false
                 || str.toLongLong() == 0)
            *piDbgCfgVar = VBOXGLOBAL_DBG_CFG_VAR_DONE | VBOXGLOBAL_DBG_CFG_VAR_FALSE;
        else
            *piDbgCfgVar |= VBOXGLOBAL_DBG_CFG_VAR_DONE;
    }

    return (*piDbgCfgVar & VBOXGLOBAL_DBG_CFG_VAR_MASK) == VBOXGLOBAL_DBG_CFG_VAR_TRUE;
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

void VBoxGlobal::comWrappersReinit()
{
    /* Re-fetch corresponding objects/values: */
    m_comHost = virtualBox().GetHost();
    m_strHomeFolder = virtualBox().GetHomeFolder();

    /* Re-initialize guest OS Type list: */
    m_guestOSFamilyIDs.clear();
    m_guestOSTypes.clear();
    const CGuestOSTypeVector guestOSTypes = m_comVBox.GetGuestOSTypes();
    const int cGuestOSTypeCount = guestOSTypes.size();
    AssertMsg(cGuestOSTypeCount > 0, ("Number of OS types must not be zero"));
    if (cGuestOSTypeCount > 0)
    {
        /* Here we ASSUME the 'Other' types are always the first,
         * so we remember them and will append them to the list when finished.
         * We do a two pass, first adding the specific types, then the two 'Other' types. */
        for (int j = 0; j < 2; ++j)
        {
            int cMax = j == 0 ? cGuestOSTypeCount : RT_MIN(2, cGuestOSTypeCount);
            for (int i = j == 0 ? 2 : 0; i < cMax; ++i)
            {
                const CGuestOSType os = guestOSTypes.at(i);
                const QString strFamilyID = os.GetFamilyId();
                const QString strFamilyDescription = os.GetFamilyDescription();
                if (!m_guestOSFamilyIDs.contains(strFamilyID))
                {
                    m_guestOSFamilyIDs << strFamilyID;
                    m_guestOSFamilyDescriptions[strFamilyID] = strFamilyDescription;
                    m_guestOSTypes << QList<CGuestOSType>();
                }
                m_guestOSTypes[m_guestOSFamilyIDs.indexOf(strFamilyID)].append(os);
            }
        }
    }

    /* Mark wrappers valid: */
    m_fWrappersValid = true;
}
