/* $Id: VBoxVMInfoAdditions.h 33539 2008-07-21 12:21:29Z bird $ */
/** @file
 *  VBoxUtil - Some tool functions.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___VBOXUTILS_H
#define ___VBOXUTILS_H

BOOL vboxGetFileVersion (LPCWSTR a_pszFileName,
                         DWORD* a_pdwMajor, DWORD* a_pdwMinor, DWORD* a_pdwBuildNumber, DWORD* a_pdwRevisionNumber);

BOOL vboxGetFileString (LPCWSTR a_pszFileName, LPWSTR a_pszBlock, LPWSTR a_pszString, PUINT a_puiSize);

BOOL vboxGetFileVersionString (LPCWSTR a_pszPath, LPCWSTR a_pszFileName, char* a_pszVersion, UINT a_uiSize);

#endif /* !___VBOXUTILS_H */

