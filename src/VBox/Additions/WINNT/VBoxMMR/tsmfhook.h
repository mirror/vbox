/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#pragma once

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the TSMFHOOK_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// TSMFHOOK_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef TSMFHOOK_EXPORTS
#define TSMFHOOK_API __declspec(dllexport)
#else
#define TSMFHOOK_API __declspec(dllimport)
#endif

extern "C" TSMFHOOK_API LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam);

void Shutdown();

// {56E95534-F42E-4C79-8EED-B121B4823163}
static const GUID ProviderId = 
{ 0x56e95534, 0xf42e, 0x4c79, { 0x8e, 0xed, 0xb1, 0x21, 0xb4, 0x82, 0x31, 0x63 } };

// {06797744-5A74-4782-B2AB-B86D9F6C7B4A}
static const GUID ChannelOpenCategoryId = 
{ 0x6797744, 0x5a74, 0x4782, { 0xb2, 0xab, 0xb8, 0x6d, 0x9f, 0x6c, 0x7b, 0x4a } };

// {BF94ED39-9585-4822-B69E-DF19549A664C}
static const GUID ChannelWriteCategoryId = 
{ 0xbf94ed39, 0x9585, 0x4822, { 0xb6, 0x9e, 0xdf, 0x19, 0x54, 0x9a, 0x66, 0x4c } };

// {95375270-AE5F-423E-A4EB-5AE7FC649CF6}
static const GUID ChannelReadCategoryId = 
{ 0x95375270, 0xae5f, 0x423e, { 0xa4, 0xeb, 0x5a, 0xe7, 0xfc, 0x64, 0x9c, 0xf6 } };

// {01F2A23A-4144-45E6-9933-4668915A1758}
static const GUID ChannelCloseCategoryId = 
{ 0x1f2a23a, 0x4144, 0x45e6, { 0x99, 0x33, 0x46, 0x68, 0x91, 0x5a, 0x17, 0x58 } };

#pragma pack(push, 1)

struct TraceEventDataChannelOpen
{
    HANDLE             Channel;
};

struct TraceEventDataChannelReadEnd
{
    DWORD              ErrorCode;
    CHAR               Data[64];
};

struct TraceEventDataChannelWriteStart : TraceEventDataChannelOpen
{
    CHAR               Data[64];
};

typedef TraceEventDataChannelOpen TraceEventDataChannelClose;
typedef TraceEventDataChannelOpen TraceEventDataChannelReadStart;

struct TraceEventData
{
    EVENT_TRACE_HEADER Header;
    union
    {
        TraceEventDataChannelOpen       Open;
        TraceEventDataChannelClose      Close;
        TraceEventDataChannelReadStart  ReadStart;
        TraceEventDataChannelReadEnd    ReadEnd;
        TraceEventDataChannelWriteStart WriteStart;
    };
};


#pragma pack(pop)

