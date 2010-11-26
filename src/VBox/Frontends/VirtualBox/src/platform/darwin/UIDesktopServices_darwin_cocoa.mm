/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Qt GUI - Utility Classes and Functions specific to darwin.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox includes */
#include "UIDesktopServices_darwin_p.h"

/* System includes */
#include <Carbon/Carbon.h>
#import <AppKit/NSWorkspace.h>

/* It's crazy how hard it is to create an Alias file on Mac OS X. You could try
   to write such files yourself, with all the resource fork fun (and the
   understanding what to write in) or you could kindly ask Finder for doing
   this job. We take the second approach, which is hard enough. */
bool darwinCreateMachineShortcut(NativeNSStringRef pstrSrcFile, NativeNSStringRef pstrDstPath, NativeNSStringRef pstrName, NativeNSStringRef /* pstrUuid */)
{
    /* First of all we need to figure out which process Id the Finder currently has. */
    NSWorkspace *pWS = [NSWorkspace sharedWorkspace];
    NSArray *pApps = [pWS launchedApplications];
    bool fFFound = false;
    ProcessSerialNumber psn;
    for (NSDictionary *pDict in pApps)
    {
        if ([[pDict valueForKey:@"NSApplicationBundleIdentifier"] isEqualToString:@"com.apple.finder"])
        {
            psn.highLongOfPSN = [[pDict valueForKey:@"NSApplicationProcessSerialNumberHigh"] intValue];
            psn.lowLongOfPSN  = [[pDict valueForKey:@"NSApplicationProcessSerialNumberLow"] intValue];
            fFFound = true;
            break;
        }
    }
    if (!fFFound)
        return false;

    /* Now the event fun begins. */
    OSErr err = noErr;
    AliasHandle hSrcAlias = 0;
    AliasHandle hDstAlias = 0;
    do
    {
        /* Create a descriptor which contains the target psn. */
        NSAppleEventDescriptor *finderPSNDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeProcessSerialNumber bytes:&psn length:sizeof(psn)];
        if (!finderPSNDesc)
            break;
        /* Create the Apple event descriptor which points to the Finder target already. */
        NSAppleEventDescriptor *finderEventDesc = [NSAppleEventDescriptor
                                                     appleEventWithEventClass:kAECoreSuite
                                                     eventID:kAECreateElement
                                                     targetDescriptor:finderPSNDesc
                                                     returnID:kAutoGenerateReturnID
                                                     transactionID:kAnyTransactionID];
        if (!finderEventDesc)
            break;
        /* Create and add an event type descriptor: Alias */
        NSAppleEventDescriptor *osTypeDesc = [NSAppleEventDescriptor descriptorWithTypeCode:typeAlias];
        if (!osTypeDesc)
            break;
        [finderEventDesc setParamDescriptor:osTypeDesc forKeyword:keyAEObjectClass];
        /* Now create the source Alias, which will be attached to the event. */
        err = FSNewAliasFromPath(nil, [pstrSrcFile fileSystemRepresentation], 0, &hSrcAlias, 0);
        if (err != noErr)
            break;
        char handleState;
        handleState = HGetState((Handle)hSrcAlias);
        HLock((Handle)hSrcAlias);
        NSAppleEventDescriptor *srcAliasDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeAlias bytes:*hSrcAlias length:GetAliasSize(hSrcAlias)];
        if (!srcAliasDesc)
            break;
        [finderEventDesc setParamDescriptor:srcAliasDesc forKeyword:keyASPrepositionTo];
        HSetState((Handle)hSrcAlias, handleState);
        /* Next create the target Alias and attach it to the event. */
        err = FSNewAliasFromPath(nil, [pstrDstPath fileSystemRepresentation], 0, &hDstAlias, 0);
        if (err != noErr)
            break;
        handleState = HGetState((Handle)hDstAlias);
        HLock((Handle)hDstAlias);
        NSAppleEventDescriptor *dstAliasDesc = [NSAppleEventDescriptor descriptorWithDescriptorType:typeAlias bytes:*hDstAlias length:GetAliasSize(hDstAlias)];
        if (!dstAliasDesc)
            break;
        [finderEventDesc setParamDescriptor:dstAliasDesc forKeyword:keyAEInsertHere];
        HSetState((Handle)hDstAlias, handleState);
        /* Finally a property descriptor containing the target Alias name. */
        NSAppleEventDescriptor *finderPropDesc = [NSAppleEventDescriptor recordDescriptor];
        if (!finderPropDesc)
            break;
        [finderPropDesc setDescriptor:[NSAppleEventDescriptor descriptorWithString:pstrName] forKeyword:keyAEName];
        [finderEventDesc setParamDescriptor:finderPropDesc forKeyword:keyAEPropData];
        /* Now send the event to the Finder. */
        err = AESend([finderEventDesc aeDesc], NULL, kAENoReply, kAENormalPriority, kNoTimeOut, 0, nil);
        if (err != noErr)
            break;
    } while(0);

    /* Cleanup */
    if (hSrcAlias)
        DisposeHandle((Handle)hSrcAlias);
    if (hDstAlias)
        DisposeHandle((Handle)hDstAlias);

    return err == noErr ? true : false;
}

bool darwinOpenInFileManager(NativeNSStringRef pstrFile)
{
    return [[NSWorkspace sharedWorkspace] selectFile:pstrFile inFileViewerRootedAtPath:@""];
}

