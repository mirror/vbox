
LangString VBOX_TEST ${LANG_ENGLISH}                     "This is a test message of $(^Name)!"

LangString VBOX_NOADMIN ${LANG_ENGLISH}                  "You need to have administrator rights to (un)install the $(^Name)!$\r$\nThis setup will abort now."

LangString VBOX_NOTICE_ARCH_X86 ${LANG_ENGLISH}          "This application only runs on 32-bit Windows systems. Please install the 64-bit version of $(^Name)!"
LangString VBOX_NOTICE_ARCH_AMD64 ${LANG_ENGLISH}        "This application only runs on 64-bit Windows systems. Please install the 32-bit version of $(^Name)!"
LangString VBOX_NT4_NO_SP6 ${LANG_ENGLISH}               "Setup has detected, that there is no Service Pack 6 for NT4 installed.$\r$\nIt is recommended that you install it first. Continue anyway?"

LangString VBOX_PLATFORM_UNSUPPORTED ${LANG_ENGLISH}     "Platform is not supported for Guest Additions yet!"

LangString VBOX_INNOTEK_FOUND ${LANG_ENGLISH}            "An old version of the Guest Additions is installed in this virtual machine. This must be uninstalled before the current Guest Additions can be installed.$\r$\n$\r$\nDo you want to uninstall the old Guest Additions now?"
LangString VBOX_INNOTEK_ABORTED ${LANG_ENGLISH}          "Setup cannot continue installing the Guest Additions.$\r$\nPlease uninstall old Guest Additions first!"
LangString VBOX_INNOTEK_REBOOT ${LANG_ENGLISH}           "It is strongly recommended that you reboot this virtual machine before installing the new version of the Guest Additions.$\r$\nPlease start the Guest Additions setup again after rebooting.$\r$\n$\r$\nRestart now?"

LangString VBOX_COMPONENT_MAIN ${LANG_ENGLISH}           			"VirtualBox Guest Additions"
LangString VBOX_COMPONENT_MAIN_DESC ${LANG_ENGLISH}      			"Main Files of VirtualBox Guest Additions"
LangString VBOX_COMPONENT_AUTOLOGON ${LANG_ENGLISH}      			"Auto-Logon Support"
LangString VBOX_COMPONENT_AUTOLOGON_DESC ${LANG_ENGLISH} 			"Enables auto-logon support for guests"
LangString VBOX_COMPONENT_AUTOLOGON_WARN_3RDPARTY ${LANG_ENGLISH} 	"There already is a component installed for providing auto-logon support.$\r$\nIf you replace this component with the one from VirtualBox, the system could become unstable.$\r$\nReplace it anyway?"
LangString VBOX_COMPONENT_D3D  ${LANG_ENGLISH}           			"Direct3D Support (Experimental)"
LangString VBOX_COMPONENT_D3D_DESC  ${LANG_ENGLISH}      			"Enables Direct3D Support for guests (Experimental)"
LangString VBOX_COMPONENT_D3D_NO_SM ${LANG_ENGLISH}      			"Windows is currently not started in safe mode.$\r$\nTherefore the D3D support cannot be installed."

LangString VBOX_WFP_WARN_REPLACE ${LANG_ENGLISH}         "The setup just has replaced some system files in order to make ${PRODUCT_NAME} work correctly.$\r$\nIn case Windows File Protection shows a warning dialog, please cancel its suggestion of restoring the original files."
LangString VBOX_REBOOT_REQUIRED ${LANG_ENGLISH}          "To apply all changes, the system must be restarted. Restart Windows now?"

LangString VBOX_EXTRACTION_COMPLETE ${LANG_ENGLISH}      "$(^Name): Files were successfully extracted to $\"$INSTDIR$\"!"

LangString VBOX_ERROR_INST_FAILED ${LANG_ENGLISH}        "An error occurred while installing!$\r$\nPlease refer to the log file under '$INSTDIR\install_ui.log' for more information."

LangString VBOX_UNINST_CONFIRM ${LANG_ENGLISH}           "Do you really want to uninstall $(^Name)?"
LangString VBOX_UNINST_SUCCESS ${LANG_ENGLISH}           "$(^Name) have been uninstalled."
