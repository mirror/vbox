/* $Id$*/
/** @file
 * Header used if VBOX_WITH_PRECOMPILED_HEADERS is active.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

//#include <Q3PopupMenu>
#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QAbstractScrollArea>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBasicTimer>
#include <QBitmap>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCleanlooksStyle>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
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
#include <QFrame>
#include <QGLContext>
#include <QGLWidget>
#include <QGlobalStatic>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QHelpEvent>
#include <QHostAddress>
#include <QHttp>
#include <QHttpResponseHeader>
#include <QImage>
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
#include <QLocale>
#ifdef Q_WS_MAC
# include <QMacCocoaViewContainer>
#endif
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMenuItem>
#include <QMessageBox>
#include <QMetaProperty>
#include <QMetaType>
#include <QMimeData>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QMutex>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlastiqueStyle>
#include <QPointer>
#include <QPolygon>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRect>
#include <QRegExp>
#include <QRegExpValidator>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalMapper>
#include <QSizeGrip>
#include <QSlider>
#include <QSocketNotifier>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionFocusRect>
#include <QStyleOptionFrame>
#include <QStyleOptionSlider>
#include <QStylePainter>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
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
#include <QWidget>
#include <QWindowsStyle>
#include <QWindowsVistaStyle>
#ifdef Q_WS_X11
# include <QX11Info>
#endif

#include "QIAbstractWizard.h"
#include "QIAdvancedSlider.h"
#include "QIArrowButtonPress.h"
#include "QIArrowButtonSwitch.h"
#include "QIArrowSplitter.h"
#include "QIDialog.h"
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#if 0
#include "QIHotKeyEdit.h"
#endif
#include "QILabel.h"
#include "QILabelSeparator.h"
#include "QILineEdit.h"
#include "QIListView.h"
#include "QIMainDialog.h"
#include "QIMessageBox.h"
#include "QIRichToolButton.h"
#include "QISplitter.h"
#include "QIStateIndicator.h"
#include "QIStatusBar.h"
#include "QIToolButton.h"
#include "QITreeView.h"
#include "QITreeWidget.h"
#include "QIWidgetValidator.h"
#include "QIWithRetranslateUI.h"

//expensive: #include "AbstractDockIconPreview.h"
#include "CIShared.h"
#include "COMDefs.h"
#ifdef Q_WS_MAC
# include "DarwinKeyboard.h"
# include "DockIconPreview.h"
#endif
#include "VBoxAboutDlg.h"
#include "UIApplianceEditorWidget.h"
#include "VBoxCloseVMDlg.h"
#ifdef Q_WS_MAC
# include "VBoxCocoaHelper.h"
# include "VBoxCocoaSpecialControls.h"
#endif
#include "UIDefs.h"
#include "VBoxDownloaderWgt.h"
#include "UIApplianceExportEditorWidget.h"
#include "VBoxExportApplianceWzd.h"
#include "VBoxFBOverlay.h"
#include "VBoxFBOverlayCommon.h"
#include "VBoxFilePathSelectorWidget.h"
#include "VBoxFrameBuffer.h"
#include "UIGlobalSettingsGeneral.h"
#include "UIGlobalSettingsInput.h"
#include "UIGlobalSettingsLanguage.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetails.h"
#include "UIGlobalSettingsUpdate.h"
#include "VBoxGlobal.h"
#include "VBoxGlobalSettings.h"
#include "VBoxGuestRAMSlider.h"
#ifdef Q_WS_MAC
# include "VBoxIChatTheaterWrapper.h"
#endif
#include "UIApplianceImportEditorWidget.h"
#include "VBoxImportApplianceWzd.h"
#include "VBoxLicenseViewer.h"
#include "UILineTextEdit.h"
//#include "VBoxMediaComboBox.h"
#include "VBoxMediaManagerDlg.h"
//#include "VBoxMedium.h"               /* Expensive? Or what? */
#include "VBoxMiniToolBar.h"
#include "VBoxNewHDWzd.h"
#include "VBoxNewVMWzd.h"
#include "VBoxOSTypeSelectorButton.h"
#include "UINameAndSystemEditor.h"
#include "UIMessageCenter.h"
#include "VBoxProgressDialog.h"
#include "UISelectorWindow.h"
#include "UISettingsDialog.h"
#include "UISettingsDialogSpecific.h"
#include "UISettingsPage.h"
#include "VBoxSettingsSelector.h"
#include "VBoxSnapshotDetailsDlg.h"
#include "VBoxSnapshotsWgt.h"
#include "VBoxSpecialControls.h"
#include "VBoxTakeSnapshotDlg.h"
#include "UIToolBar.h"
#include "VBoxUpdateDlg.h"
#include "VBoxUtils-darwin.h"
#include "VBoxUtils.h"
#include "VBoxVMFirstRunWzd.h"
#include "VBoxVMInformationDlg.h"
#include "UIVMListView.h"
#include "UIVMLogViewer.h"
#include "UIMachineSettingsAudio.h"
#include "UIMachineSettingsDisplay.h"
#include "UIMachineSettingsGeneral.h"
#include "UIMachineSettingsStorage.h"
#include "UIMachineSettingsNetwork.h"
#include "UIMachineSettingsParallel.h"
#include "UIMachineSettingsSF.h"
#include "UIMachineSettingsSFDetails.h"
#include "UIMachineSettingsSerial.h"
#include "UIMachineSettingsSystem.h"
#include "UIMachineSettingsUSB.h"
#include "UIMachineSettingsUSBFilterDetails.h"

#ifdef Q_WS_X11
# undef BOOL /* typedef CARD8 BOOL in Xmd.h conflicts with #define BOOL PRBool
              * in COMDefs.h. A better fix would be to isolate X11-specific
              * stuff by placing XX* helpers below to a separate source file. */
RT_C_DECLS_BEGIN                        /* rhel3 build hack */
/** @todo stuff might be missing here... */
# include <X11/X.h>
# include <X11/Xmd.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/extensions/dpms.h>
RT_C_DECLS_END                          /* rhel3 build hack */
# define BOOL PRBool
# include "VBoxX11Helper.h"
# include "XKeyboard.h"
#endif

#ifdef Q_WS_MAC
# include <ApplicationServices/ApplicationServices.h>
#endif

#if defined (Q_WS_WIN)
# include <shlobj.h>
#endif

#include <math.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/thread.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/sup.h>
#include <VBox/com/Guid.h>              /* ...a bit expensive... */
#include <VBox/VMMDev.h>                /** @todo @bugref{4084} */
#include <VBox/VBoxHDD.h>
#include <VBox/VBoxGL2D.h>
#ifdef VBOX_WITH_VIDEOHWACCEL
# include <VBox/VBoxVideo.h>
# include <VBox/vmm/ssm.h>
#endif

#ifdef Q_WS_MAC
# if MAC_LEOPARD_STYLE /* This is defined by UIDefs.h and must come after it was included */
#  include <qmacstyle_mac.h>
# endif
#endif

