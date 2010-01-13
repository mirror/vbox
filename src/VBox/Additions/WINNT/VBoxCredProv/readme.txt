Note: This module is based on the official Microsoft Credential Provider examples, downloaded from
http://www.microsoft.com/downloads/details.aspx?FamilyID=1287ec56-77b4-48c4-8b58-35b7295d6c2c&displaylang=en

Brief Description:
5384 (Beta 2)
Windows Vista Beta 2 Credential Provider Samples

File Name:	Beta 2 Credential Provider Samples.zip
Version:	1.0
Date Published:	6/25/2006
Language:	English
Download Size:	874 KB


//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
//
Overview
--------
This sample implements a simple credential provider. A credential provider allows a 3rd
party to provide an alternate way of logging on. For example, a fingerprint solution vendor
would write a credential provider to interact with the user and send the appropriate
credentials to the system for authentication. Questions should be sent to
credprov@microsoft.com.

This sample implements a simplified credential provider that is based on the password
credential provider that ships with Windows Vista.  When run, the credential provider
should enumerate two tiles, one for the administrator and one for the guest account. Note
that unless your machine is non-domain joined and you have enabled the guest account, it is
likely that the guest account is disabled and you will receive an error if you try to use that
tile to log on.

Please also see Windows Vista Credential Provider Samples Overview.doc, included in this package

How to run this sample
--------------------------------
Once you have built the project, copy samplecredentialprovider.dll to the System32 directory
on a Vista machine and run Register.reg from an elevated command prompt. The credential
should appear the next time a logon is invoked (such as when logging off or rebooting the machine).

What this sample demonstrates
-----------------------------
This sample demonstrates simple password based log on and unlock behavior.  It also shows how to construct
a simple user tile and handle the user interaction with that tile.

What this sample does not demonstrate
-------------------------------------
- other credential provider scenarios, like participating in credui or handling change password.
- every possible field that you can put in your user tile
- any network access prior to local authentication (the Pre-Logon Access Provider (PLAP) behavior)
- an event based credential provider (like a smartcard or fingerprint based system)

Parts of the sample
-------------------
common.h - sets up what a tile looks like
CSampleCredential.h/CSampleCredential.cpp - implements ICredentialProviderCredential, which describes one tile
CSampleProvider.h/CSampleProvider.cpp - implements ICredentialProvider, which is the main interface used by LogonUI
										to talk to a credential provider.
Dll.h/Dll.cpp - standard dll setup for a dll that implements COM objects
helpers.h/helpers.cpp - useful functionality to deal with serializing credentials, UNICODE_STRING's, etc
