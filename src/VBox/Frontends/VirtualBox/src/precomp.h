/* $Id$*/
/** @file
 * VBox Qt GUI - Header used if VBOX_WITH_PRECOMPILED_HEADERS is active.
 */

/*
 * Copyright (C) 2009-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_GUI


/*
 * Qt
 */
#include <QAbstractButton>
#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QAbstractScrollArea>
#include <QAbstractTableModel>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBasicTimer>
#include <QBitmap>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCleanlooksStyle>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCommonStyle>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGLContext>
#include <QGLWidget>
#include <QGlobalStatic>
#include <QGraphicsLinearLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QHelpEvent>
#include <QHostAddress>
#include <QHttp>
#include <QHttpResponseHeader>
#include <QIWithRetranslateUI.h>
#include <QIcon>
#include <QImage>
#include <QImageWriter>
#include <QIntValidator>
#include <QItemDelegate>
#include <QItemEditorFactory>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLibrary>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QLinkedList>
#include <QList>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#ifdef Q_WS_MAC
# include <QMacCocoaViewContainer>
#endif
#include <QMainWindow>
#include <QMap>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMenuItem>
#include <QMessageBox>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QMetaType>
#include <QMimeData>
#include <QMouseEvent>
#include <QMouseEventTransition>
#include <QMoveEvent>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPair>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QPixmapCache>
#include <QPlastiqueStyle>
#include <QPoint>
#include <QPointer>
#include <QPolygon>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QQueue>
#include <QRadioButton>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QRect>
#include <QRegExp>
#include <QRegExpValidator>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QSignalMapper>
#include <QSignalTransition>
#include <QSizeGrip>
#include <QSlider>
#include <QSocketNotifier>
#include <QSortFilterProxyModel>
#include <QSpacerItem>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStateMachine>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionFocusRect>
#include <QStyleOptionFrame>
#include <QStyleOptionGraphicsItem>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextLayout>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTouchEvent>
#include <QTransform>
#include <QTranslator>
#include <QTreeView>
#include <QTreeWidget>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QValidator>
#include <QVarLengthArray>
#include <QVariant>
#include <QVector>
#include <QWaitCondition>
#include <QWidget>
#include <QWindowsStyle>
#include <QWindowsVistaStyle>
#include <QWizard>
#include <QWizardPage>
#ifdef Q_WS_X11
# include <QX11Info>
#endif
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtGlobal>


/*
 * System specific headers.
 */
#ifdef Q_WS_WIN
# include <shlobj.h>
# include <Windows.h>
#endif



/*
 * IPRT
 */
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/cdefs.h>
#include <iprt/cidr.h>
#include <iprt/cpp/utils.h>
#include <iprt/critsect.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/http.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/memcache.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/sha.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/types.h>
#include <iprt/uri.h>
#include <iprt/zip.h>


/*
 * VirtualBox COM API
 */
#ifdef VBOX_WITH_XPCOM
# include <VirtualBox_XPCOM.h>
#else
# include <VirtualBox.h>
#endif

/*
 * VBox headers.
 */
#include <VBox/VBoxCocoa.h>
#include <VBox/VBoxGL2D.h>
//#include <VBox/VBoxKeyboard.h> - includes X11/X.h which causes trouble.
#include <VBox/VBoxOGLTest.h>
#include <VBox/VBoxVideo.h>
#ifdef VBOX_WITH_VIDEOHWACCEL
# include <VBox/VBoxVideo3D.h>
#endif
#include <VBox/VDEPlug.h>
#include <VBox/VMMDev.h>
#include <VBox/cdefs.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/assert.h>
#include <VBox/com/com.h>
#include <VBox/com/listeners.h>
#include <VBox/dbggui.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/sup.h>
#include <VBox/types.h>
#include <VBox/vd.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_VIDEOHWACCEL
# include <VBox/vmm/ssm.h>
#endif


/*
 * VirtualBox Qt GUI - QI* headers.
 */
#include "QIAdvancedSlider.h"
#include "QIAdvancedToolBar.h"
#include "QIArrowButtonPress.h"
#include "QIArrowButtonSwitch.h"
#include "QIArrowSplitter.h"
#include "QIDialog.h"
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#include "QIGraphicsWidget.h"
#include "QILabel.h"
#include "QILabelSeparator.h"
#include "QILineEdit.h"
#include "QIListView.h"
#include "QIMainDialog.h"
#include "QIMenu.h"
#include "QIMessageBox.h"
#include "QIProcess.h"
#include "QIRichTextLabel.h"
#include "QIRichToolButton.h"
#include "QISplitter.h"
#include "QIStatusBar.h"
#include "QIStatusBarIndicator.h"
#include "QITabWidget.h"
#include "QITableView.h"
#include "QIToolButton.h"
#include "QITreeView.h"
#include "QITreeWidget.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"


/*
 * VirtualBox Qt GUI - COM headers & Wrappers (later).
 */
/* Note! X.h and Xlib.h shall not be included as they may redefine None and
         Status respectively, both are user in VBox enums.  Don't bother
         undefining them, just prevent their inclusion! */
#include "COMDefs.h"
#include "COMEnums.h"

#include "CAppliance.h"
#include "CAudioAdapter.h"
#include "CBIOSSettings.h"
#include "CCanShowWindowEvent.h"
#include "CConsole.h"
#include "CDHCPServer.h"
#include "CDisplay.h"
#include "CDisplaySourceBitmap.h"
#include "CDnDSource.h"
#include "CDnDTarget.h"
#include "CEmulatedUSB.h"
#include "CEvent.h"
#include "CEventListener.h"
#include "CEventSource.h"
#include "CExtPack.h"
#include "CExtPackFile.h"
#include "CExtPackManager.h"
#include "CExtraDataCanChangeEvent.h"
#include "CExtraDataChangedEvent.h"
#include "CFramebuffer.h"
#include "CGuest.h"
#include "CGuestDnDSource.h"
#include "CGuestDnDTarget.h"
#include "CGuestMonitorChangedEvent.h"
#include "CGuestOSType.h"
#include "CHost.h"
#include "CHostNetworkInterface.h"
#include "CHostUSBDevice.h"
#include "CHostUSBDeviceFilter.h"
#include "CHostVideoInputDevice.h"
#include "CIShared.h"
#include "CKeyboard.h"
#include "CKeyboardLedsChangedEvent.h"
#include "CMachine.h"
#include "CMachineDataChangedEvent.h"
#include "CMachineDebugger.h"
#include "CMachineRegisteredEvent.h"
#include "CMachineStateChangedEvent.h"
#include "CMedium.h"
#include "CMediumAttachment.h"
#include "CMediumChangedEvent.h"
#include "CMediumFormat.h"
#include "CMouse.h"
#include "CMouseCapabilityChangedEvent.h"
#include "CMousePointerShapeChangedEvent.h"
#include "CNATEngine.h"
#include "CNATNetwork.h"
#include "CNetworkAdapter.h"
#include "CNetworkAdapterChangedEvent.h"
#include "CParallelPort.h"
#include "CProgress.h"
#include "CRuntimeErrorEvent.h"
#include "CSerialPort.h"
#include "CSession.h"
#include "CSessionStateChangedEvent.h"
#include "CSharedFolder.h"
#include "CShowWindowEvent.h"
#include "CSnapshot.h"
#include "CSnapshotChangedEvent.h"
#include "CSnapshotDeletedEvent.h"
#include "CSnapshotTakenEvent.h"
#include "CStateChangedEvent.h"
#include "CStorageController.h"
#include "CSystemProperties.h"
#include "CUSBController.h"
#include "CUSBDevice.h"
#include "CUSBDeviceFilter.h"
#include "CUSBDeviceFilters.h"
#include "CUSBDeviceStateChangedEvent.h"
#include "CVFSExplorer.h"
#include "CVRDEServer.h"
#include "CVRDEServerInfo.h"
#include "CVirtualBox.h"
#include "CVirtualBoxErrorInfo.h"
#include "CVirtualSystemDescription.h"


/*
 * VirtualBox Qt GUI - UI headers.
 */
#include "UIMessageCenter.h"
#include "UINetworkReply.h"
#ifdef RT_OS_DARWIN
# include "UIAbstractDockIconPreview.h"
#endif
#include "UIActionPool.h"
#include "UIActionPoolRuntime.h"
#include "UIActionPoolSelector.h"
#include "UIAnimationFramework.h"
#include "UIApplianceEditorWidget.h"
#include "UIApplianceExportEditorWidget.h"
#include "UIApplianceImportEditorWidget.h"
#include "UIBar.h"
#include "UIBootTable.h"
#ifdef RT_OS_DARWIN
# include "UICocoaApplication.h"
# include "UICocoaDockIconPreview.h"
# include "UICocoaSpecialControls.h"
#endif
#include "UIConsoleEventHandler.h"
#include "UIConverter.h"
#include "UIConverterBackend.h"
#include "UIDefs.h"
#include "UIDesktopServices.h"
#ifdef RT_OS_DARWIN
# include "UIDesktopServices_darwin_p.h"
#endif
#include "UIDnDDrag.h"
#ifdef RT_OS_WINDOWS
# include "UIDnDDataObject_win.h"
# include "UIDnDDropSource_win.h"
# include "UIDnDEnumFormat_win.h"
#endif
#include "UIDnDHandler.h"
#include "UIDnDMIMEData.h"
#include "UIDownloader.h"
#include "UIDownloaderAdditions.h"
#include "UIDownloaderExtensionPack.h"
#include "UIDownloaderUserManual.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIFilmContainer.h"
#include "UIFrameBuffer.h"
#include "UIGChooser.h"
#include "UIGChooserHandlerKeyboard.h"
#include "UIGChooserHandlerMouse.h"
#include "UIGChooserItem.h"
#include "UIGChooserItemGroup.h"
#include "UIGChooserItemMachine.h"
#include "UIGChooserModel.h"
#include "UIGChooserView.h"
#include "UIGDetails.h"
#include "UIGDetailsElement.h"
#include "UIGDetailsElements.h"
#include "UIGDetailsGroup.h"
#include "UIGDetailsItem.h"
#include "UIGDetailsModel.h"
#include "UIGDetailsSet.h"
#include "UIGDetailsView.h"
#include "UIGMachinePreview.h"
#include "UIGlobalSettingsDisplay.h"
#include "UIGlobalSettingsExtension.h"
#include "UIGlobalSettingsGeneral.h"
#include "UIGlobalSettingsInput.h"
#include "UIGlobalSettingsLanguage.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetailsHost.h"
#include "UIGlobalSettingsNetworkDetailsNAT.h"
#include "UIGlobalSettingsPortForwardingDlg.h"
#include "UIGlobalSettingsProxy.h"
#include "UIGlobalSettingsUpdate.h"
#include "UIGraphicsButton.h"
#include "UIGraphicsRotatorButton.h"
#include "UIGraphicsTextPane.h"
#include "UIGraphicsToolBar.h"
#include "UIGraphicsZoomButton.h"
#include "UIHostComboEditor.h"
#include "UIHotKeyEditor.h"
#include "UIIconPool.h"
#include "UIImageTools.h"
#include "UIIndicatorsPool.h"
#include "UIKeyboardHandler.h"
#include "UIKeyboardHandlerFullscreen.h"
#include "UIKeyboardHandlerNormal.h"
#include "UIKeyboardHandlerScale.h"
#include "UIKeyboardHandlerSeamless.h"
#include "UILineTextEdit.h"
#include "UIMachine.h"
#include "UIMachineDefs.h"
#include "UIMachineLogic.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineLogicScale.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineSettingsAudio.h"
#include "UIMachineSettingsDisplay.h"
#include "UIMachineSettingsGeneral.h"
#include "UIMachineSettingsNetwork.h"
#include "UIMachineSettingsParallel.h"
#include "UIMachineSettingsPortForwardingDlg.h"
#include "UIMachineSettingsSF.h"
#include "UIMachineSettingsSFDetails.h"
#include "UIMachineSettingsSerial.h"
#include "UIMachineSettingsStorage.h"
#include "UIMachineSettingsSystem.h"
#include "UIMachineSettingsUSB.h"
#include "UIMachineSettingsUSBFilterDetails.h"
#include "UIMachineView.h"
#include "UIMachineViewFullscreen.h"
#include "UIMachineViewNormal.h"
#include "UIMachineViewScale.h"
#include "UIMachineViewSeamless.h"
#include "UIMachineWindow.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineWindowScale.h"
#include "UIMachineWindowSeamless.h"
#include "UIMainEventListener.h"
#include "UIMedium.h"
#include "UIMediumDefs.h"
#include "UIMediumEnumerator.h"
#include "UIMediumManager.h"
#include "UIMediumTypeChangeDialog.h"
#include "UIMenuBar.h"
#include "UIMenuBarEditorWindow.h"
#include "UIMessageCenter.h"
#include "UIMiniToolBar.h"
#include "UIModalWindowManager.h"
#include "UIMouseHandler.h"
#include "UIMultiScreenLayout.h"
#include "UINameAndSystemEditor.h"
#include "UINetworkCustomer.h"
#include "UINetworkDefs.h"
#include "UINetworkManager.h"
#include "UINetworkManagerDialog.h"
#include "UINetworkManagerIndicator.h"
#include "UINetworkReply.h"
#include "UINetworkRequest.h"
#include "UINetworkRequestWidget.h"
#include "UIPopupBox.h"
#include "UIPopupCenter.h"
#include "UIPopupPane.h"
#include "UIPopupPaneButtonPane.h"
#include "UIPopupPaneTextPane.h"
#include "UIPopupStack.h"
#include "UIPopupStackViewport.h"
#include "UIPortForwardingTable.h"
#include "UIProgressDialog.h"
#include "UISelectorWindow.h"
#include "UISession.h"
#include "UISettingsDefs.h"
#include "UISettingsDialog.h"
#include "UISettingsDialogSpecific.h"
#include "UISettingsPage.h"
#include "UIShortcutPool.h"
#include "UISlidingToolBar.h"
#include "UISpacerWidgets.h"
#include "UISpecialControls.h"
#include "UIStatusBarEditorWindow.h"
#include "UIThreadPool.h"
#include "UIToolBar.h"
#include "UIUpdateDefs.h"
#include "UIUpdateManager.h"
#include "UIVMCloseDialog.h"
#include "UIVMDesktop.h"
#include "UIVMInfoDialog.h"
#include "UIVMItem.h"
#include "UIVMLogViewer.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIWarningPane.h"
#include "UIWindowMenuManager.h"
#include "UIWizard.h"
#include "UIWizardCloneVD.h"
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVDPageBasic2.h"
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardCloneVDPageBasic4.h"
#include "UIWizardCloneVDPageExpert.h"
#include "UIWizardCloneVM.h"
#include "UIWizardCloneVMPageBasic1.h"
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVMPageBasic3.h"
#include "UIWizardCloneVMPageExpert.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppDefs.h"
#include "UIWizardExportAppPageBasic1.h"
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportAppPageBasic3.h"
#include "UIWizardExportAppPageBasic4.h"
#include "UIWizardExportAppPageExpert.h"
#include "UIWizardFirstRun.h"
#include "UIWizardFirstRunPageBasic.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppDefs.h"
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"
#include "UIWizardImportAppPageExpert.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVDPageBasic1.h"
#include "UIWizardNewVDPageBasic2.h"
#include "UIWizardNewVDPageBasic3.h"
#include "UIWizardNewVDPageExpert.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageBasic1.h"
#include "UIWizardNewVMPageBasic2.h"
#include "UIWizardNewVMPageBasic3.h"
#include "UIWizardNewVMPageExpert.h"
#include "UIWizardPage.h"


/*
 * VirtualBox Qt GUI - VBox* headers.
 */
#include "VBoxAboutDlg.h"
//#include "VBoxCocoaHelper.h"
//#include "VBoxCocoaSpecialControls.h"
#include "VBoxFBOverlay.h"
#include "VBoxFBOverlayCommon.h"
#include "VBoxFilePathSelectorWidget.h"
#include "VBoxGlobal.h"
#include "VBoxGlobalSettings.h"
#include "VBoxGuestRAMSlider.h"
//#include "VBoxIChatTheaterWrapper.h"
#include "VBoxLicenseViewer.h"
#include "VBoxMediaComboBox.h"
#include "VBoxOSTypeSelectorButton.h"
#include "VBoxSettingsSelector.h"
#include "VBoxSnapshotDetailsDlg.h"
#include "VBoxSnapshotsWgt.h"
#include "VBoxTakeSnapshotDlg.h"
#ifdef RT_OS_DARWIN
# include "VBoxUtils-darwin.h"
#endif
#ifdef RT_OS_WINDOWS
# include "VBoxUtils-win.h"
#endif
/* Note! X.h shall not be included as it defines KeyPress and KeyRelease that
         are used as enum constants in VBoxUtils.h.  Don't bother undefining
         the redefinitions, just prevent the inclusion of the header! */
#include "VBoxUtils.h"
#include "VBoxVersion.h"
//#include "VBoxX11Helper.h"
//#include "WinKeyboard.h"
//#include "XKeyboard.h"


/*
 * Final tweaks.
 */
#ifdef Q_WS_MAC
# if MAC_LEOPARD_STYLE /* This is defined by UIDefs.h and must come after it was included */
#  include <qmacstyle_mac.h>
# endif
#endif

