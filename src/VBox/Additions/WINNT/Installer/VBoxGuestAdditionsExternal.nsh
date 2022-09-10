; $Id$
;; @file
; VBoxGuestAdditionExternal.nsh - Utility function for invoking external applications.
;

;
; Copyright (C) 2013-2022 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; SPDX-License-Identifier: GPL-3.0-only
;

;;
; Macro for executing external applications.
;
; Uses the nsExec plugin in different styles, depending on whether this
; installer runs in silent mode or not. If the external program reports an exit
; code other than 0 the installer will be aborted.
;
; @param   Command line (full qualified and quoted).
; @param   If set to "false" the installer aborts if the external program reports
;          an exit code other than 0, "true" just prints a warning and continues
;          execution.
;
!macro _cmdExecute cmdline optional
  ; Save $0 & $1
  Push $0
  Push $1

  ;
  ; Execute the command, putting the exit code in $0 when done.
  ;
  ${LogVerbose} "Executing: ${cmdline}"
  ${If} ${Silent}
    nsExec::ExecToStack "${cmdline}"
    Pop $0 ; Return value (exit code)
    Pop $1 ; Stdout/stderr output (up to ${NSIS_MAX_STRLEN})
    ${LogVerbose} "$1"
  ${Else}
    nsExec::ExecToLog "${cmdline}"
    Pop $0 ; Return value (exit code)
  ${EndIf}

  ${LogVerbose} "Execution returned exit code: $0"

  ;
  ; Check if it failed and take action according to the 2nd argument.
  ;
  ${If} $0 <> 0
    ${If} ${optional} == "false"
      ${LogVerbose} "Error excuting $\"${cmdline}$\" (exit code: $0) -- aborting installation"
      Abort "Error excuting $\"${cmdline}$\" (exit code: $0) -- aborting installation"
    ${Else}
      ${LogVerbose} "Warning: Executing $\"${cmdline}$\" returned with exit code $0"
    ${EndIf}
  ${EndIf}

  ; Restore $0 and $1.
  Pop $1
  Pop $0
!macroend
!define CmdExecute "!insertmacro _cmdExecute"
