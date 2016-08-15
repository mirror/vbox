/* $Id$ */
/** @file
 * VBox Qt GUI -  Helper application for starting vbox the right way when the user double clicks on a file type association.
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

#import <Cocoa/Cocoa.h>

@interface AppDelegate: NSObject
{
NSString *m_strVBoxPath;
}
@end

@implementation AppDelegate
-(id) init
{
    self = [super init];
    if (self)
    {
        /* Get the path of VBox by looking where our bundle is located. */
        m_strVBoxPath = [[[[NSBundle mainBundle] bundlePath]
                          stringByAppendingPathComponent:@"/../../../../VirtualBox.app"]
                         stringByStandardizingPath];
        /* We kill ourself after 1 seconds */
        [NSTimer scheduledTimerWithTimeInterval:1.0
            target:NSApp
            selector:@selector(terminate:)
            userInfo:nil
            repeats:NO];
    }

    return self;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    BOOL fResult = FALSE;
    NSWorkspace *pWS = [NSWorkspace sharedWorkspace];
    /* We need to check if vbox is running already. If so we sent an open
       event. If not we start a new process with the file as parameter. */
    NSArray *pApps = [pWS runningApplications];
    bool fVBoxRuns = false;
    for (NSDictionary *pDict in pApps)
    {
        if ([[pDict valueForKey:@"NSApplicationBundleIdentifier"] isEqualToString:@"org.virtualbox.app.VirtualBox"])
        {
            fVBoxRuns = true;
            break;
        }
    }
    if (fVBoxRuns)
    {
        /* Send the open event.
         * Todo: check for an method which take a list of files. */
        for (NSString *filename in filenames)
            fResult = [pWS openFile:filename withApplication:m_strVBoxPath andDeactivate:TRUE];
    }
    else
    {
        /* Fire up a new instance of VBox. We prefer LSOpenApplication over
           NSTask, cause it makes sure that VBox will become the front most
           process after starting up. */
        OSStatus err = noErr;
        Boolean fDir;
        void *asyncLaunchRefCon = NULL;
        FSRef fileRef;
        CFStringRef file = NULL;
        CFArrayRef args = NULL;
        void **list = (void**)malloc(sizeof(void*) * [filenames count]);
        for (size_t i = 0; i < [filenames count]; ++i)
            list[i] = [filenames objectAtIndex:i];
        do
        {
            NSString *strVBoxExe = [m_strVBoxPath stringByAppendingPathComponent:@"Contents/MacOS/VirtualBox"];
            if ((err = FSPathMakeRef((const UInt8*)[strVBoxExe UTF8String], &fileRef, &fDir)) != noErr)
                break;
            if ((args = CFArrayCreate(NULL, (const void**)list, [filenames count], &kCFTypeArrayCallBacks)) == NULL)
                break;
            LSApplicationParameters par = { 0, 0, &fileRef, asyncLaunchRefCon, 0, args, 0 };
            if ((err = LSOpenApplication(&par, NULL)) != noErr)
                break;
            fResult = TRUE;
        }while(0);
        if (list)
            free(list);
        if (file)
            CFRelease(file);
        if (args)
            CFRelease(args);
    }
}
@end

int main(int argc, char *argv[])
{
    /* Global auto release pool. */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    /* Create our own delegate for the application. */
    [[NSApplication sharedApplication] setDelegate: [[AppDelegate alloc] init]];
    /* Start the event loop. */
    [NSApp run];
    /* Cleanup */
    [pool release];
    return 0;
}

