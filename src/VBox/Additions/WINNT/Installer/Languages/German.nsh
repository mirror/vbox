; $Id$
;; @file
; NLS for German language.
;

;
; Copyright (C) 2006-2020 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

LangString VBOX_TEST ${LANG_GERMAN}                                 "Das ist eine Test-Nachricht von $(^Name)!"

LangString VBOX_NOADMIN ${LANG_GERMAN}                              "Sie benï¿½tigen Administrations-Rechte zum (De-)Installieren der $(^Name)!$\r$\nDas Setup wird nun beendet."

LangString VBOX_NOTICE_ARCH_X86 ${LANG_GERMAN}                      "Diese Applikation lï¿½uft nur auf 32-bit Windows-Systemen. Bitte installieren Sie die 64-bit Version der $(^Name)!"
LangString VBOX_NOTICE_ARCH_AMD64 ${LANG_GERMAN}                    "Diese Applikation lï¿½uft nur auf 64-bit Windows-Systemen. Bitte installieren Sie die 32-bit Version der $(^Name)!"
LangString VBOX_NT4_NO_SP6 ${LANG_GERMAN}                           "Es ist kein Service Pack 6 fï¿½r NT 4.0 installiert.$\r$\nEs wird empfohlen das Service-Pack vor dieser Installation zu installieren. Trotzdem jetzt ohne Service-Pack installieren?"

LangString VBOX_PLATFORM_UNSUPPORTED ${LANG_GERMAN}                 "Diese Plattform wird noch nicht durch diese Guest Additions unterstï¿½tzt!"

LangString VBOX_SUN_FOUND ${LANG_GERMAN}                            "Eine veraltete Version der Sun Guest Additions ist auf diesem System bereits installiert. Diese muss erst deinstalliert werden bevor aktuelle Guest Additions installiert werden kï¿½nnen.$\r$\n$\r$\nJetzt die alten Guest Additions deinstallieren?"
LangString VBOX_SUN_ABORTED ${LANG_GERMAN}                          "Die Installation der Guest Additions kann nicht fortgesetzt werden.$\r$\nBitte deinstallieren Sie erst die alten Sun Guest Additions!"

LangString VBOX_INNOTEK_FOUND ${LANG_GERMAN}                        "Eine veraltete Version der innotek Guest Additions ist auf diesem System bereits installiert. Diese muss erst deinstalliert werden bevor aktuelle Guest Additions installiert werden kï¿½nnen.$\r$\n$\r$\nJetzt die alten Guest Additions deinstallieren?"
LangString VBOX_INNOTEK_ABORTED ${LANG_GERMAN}                      "Die Installation der Guest Additions kann nicht fortgesetzt werden.$\r$\nBitte deinstallieren Sie erst die alten innotek Guest Additions!"

LangString VBOX_UNINSTALL_START ${LANG_GERMAN}                      "Auf OK klicken um mit der Deinstallation zu beginnen.$\r$\nBitte warten Sie dann wï¿½hrend die Deinstallation im Hintergrund ausgefï¿½hrt wird ..."
LangString VBOX_UNINSTALL_REBOOT ${LANG_GERMAN}                     "Es wird dringend empfohlen das System neu zu starten bevor die neuen Guest Additions installiert werden.$\r$\nBitte starten Sie die Installation nach dem Neustart erneut.$\r$\n$\r$\nJetzt neu starten?"

LangString VBOX_COMPONENT_MAIN ${LANG_GERMAN}                       "VirtualBox Guest Additions"
LangString VBOX_COMPONENT_MAIN_DESC ${LANG_GERMAN}                  "Hauptkomponenten der VirtualBox Guest Additions"

LangString VBOX_COMPONENT_AUTOLOGON ${LANG_GERMAN}                  "Unterstï¿½tzung fï¿½r automatisches Anmelden"
LangString VBOX_COMPONENT_AUTOLOGON_DESC ${LANG_GERMAN}             "Ermï¿½glicht automatisches Anmelden von Benutzern"
LangString VBOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY ${LANG_GERMAN}    "Es ist bereits eine Komponente fï¿½r das automatische Anmelden installiert.$\r$\nFalls Sie diese Komponente nun mit der von VirtualBox ersetzen, kï¿½nnte das System instabil werden.$\r$\nDennoch installieren?"

LangString VBOX_COMPONENT_D3D  ${LANG_GERMAN}                       "Direct3D-Unterstï¿½tzung (Experimentell)"
LangString VBOX_COMPONENT_D3D_DESC  ${LANG_GERMAN}                  "Ermï¿½glicht Direct3D-Unterstï¿½tzung fï¿½r Gï¿½ste (Experimentell)"
LangString VBOX_COMPONENT_D3D_NO_SM ${LANG_GERMAN}                  "Windows befindet sich aktuell nicht im abgesicherten Modus.$\r$\nDaher kann die D3D-Unterstï¿½tzung nicht installiert werden."
LangString VBOX_COMPONENT_D3D_NOT_SUPPORTED ${LANG_GERMAN}          "Direct3D Gast-Unterstï¿½tzung nicht verfï¿½gbar unter Windows $g_strWinVersion!"
;LangString VBOX_COMPONENT_D3D_HINT_VRAM ${LANG_GERMAN}              "Bitte beachten Sie, dass die virtuelle Maschine fï¿½r die Benutzung von 3D-Beschleunigung einen Grafikspeicher von mindestens 128 MB fï¿½r einen Monitor benï¿½tigt und fï¿½r den Multi-Monitor-Betrieb bis zu 256 MB empfohlen wird.$\r$\n$\r$\nSie kï¿½nnen den Grafikspeicher in den VM-Einstellungen in der Kategorie $\"Anzeige$\" ï¿½ndern."
LangString VBOX_COMPONENT_D3D_INVALID ${LANG_GERMAN}                "Das Setup hat eine ungï¿½ltige/beschï¿½digte DirectX-Installation festgestellt.$\r$\n$\r$\nUm die Direct3D-Unterstï¿½tzung installieren zu kï¿½nnen wird empfohlen, zuerst das VirtualBox Benutzerhandbuch zu konsultieren.$\r$\n$\r$\nMit der Installation jetzt trotzdem fortfahren?"
LangString VBOX_COMPONENT_D3D_INVALID_MANUAL ${LANG_GERMAN}         "Soll nun das VirtualBox-Handbuch angezeigt werden um nach einer Lï¿½sung zu suchen?"

LangString VBOX_COMPONENT_STARTMENU ${LANG_GERMAN}                  "Startmenï¿½-Eintrï¿½ge"
LangString VBOX_COMPONENT_STARTMENU_DESC ${LANG_GERMAN}             "Erstellt Eintrï¿½ge im Startmenï¿½"

LangString VBOX_WFP_WARN_REPLACE ${LANG_GERMAN}                     "Das Setup hat gerade Systemdateien ersetzt um die ${PRODUCT_NAME} korrekt installieren zu kï¿½nnen.$\r$\nFalls nun ein Warn-Dialog des Windows-Dateischutzes erscheint, diesen bitte abbrechen und die Dateien nicht wiederherstellen lassen!"
LangString VBOX_REBOOT_REQUIRED ${LANG_GERMAN}                      "Um alle ï¿½nderungen durchfï¿½hren zu kï¿½nnen, muss das System neu gestartet werden. Jetzt neu starten?"

LangString VBOX_EXTRACTION_COMPLETE ${LANG_GERMAN}                  "$(^Name): Die Dateien wurden erfolgreich nach $\"$INSTDIR$\" entpackt!"

LangString VBOX_ERROR_INST_FAILED ${LANG_GERMAN}                    "Es trat ein Fehler wï¿½hrend der Installation auf!$\r$\nBitte werfen Sie einen Blick in die Log-Datei unter '$INSTDIR\install_ui.log' fï¿½r mehr Informationen."
LangString VBOX_ERROR_OPEN_LINK ${LANG_GERMAN}                      "Link konnte nicht im Standard-Browser geï¿½ffnet werden."

LangString VBOX_UNINST_CONFIRM ${LANG_GERMAN}                       "Wollen Sie wirklich die $(^Name) deinstallieren?"
LangString VBOX_UNINST_SUCCESS ${LANG_GERMAN}                       "$(^Name) wurden erfolgreich deinstalliert."
LangString VBOX_UNINST_INVALID_D3D ${LANG_GERMAN}                   "Unvollstï¿½ndige oder ungï¿½ltige Installation der Direct3D-Unterstï¿½tzung erkannt; Deinstallation wird ï¿½bersprungen."
LangString VBOX_UNINST_UNABLE_TO_RESTORE_D3D ${LANG_GERMAN}         "Konnte Direct3D-Originaldateien nicht wiederherstellen. Bitte DirectX neu installieren."
