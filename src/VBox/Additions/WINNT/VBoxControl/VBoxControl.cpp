/** @file
 *
 * VBoxControl - Guest Additions Utility
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */

#include <windows.h>
#include <stdio.h>

void printHelp()
{
    printf("VBoxControl   getvideoacceleration\n"
           "\n"
           "VBoxControl   setvideoacceleration <on|off>\n"
           "\n"
           "VBoxControl   listcustommodes\n"
           "\n"
           "VBoxControl   addcustommode <width> <height> <bpp>\n"
           "\n"
           "VBoxControl   removecustommode <width> <height> <bpp>\n");
}


HKEY getVideoKey(bool writable)
{
    HKEY hkeyDeviceMap = 0;
    HKEY hkeyVideo = 0;
    LONG status;

    status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", 0, KEY_READ, &hkeyDeviceMap);
    if ((status != ERROR_SUCCESS) || !hkeyDeviceMap)
    {
        printf("Error opening video device map registry key!\n");
        return 0;
    }
    char szVideoLocation[256];
    DWORD dwKeyType;
    szVideoLocation[0] = 0;
    DWORD len = sizeof(szVideoLocation);
    status = RegQueryValueExA(hkeyDeviceMap, "\\Device\\Video0", NULL, &dwKeyType, (LPBYTE)szVideoLocation, &len);
    /*
     * This value will start with a weird value: \REGISTRY\Machine
     * Make sure this is true.
     */
    if (   (status == ERROR_SUCCESS)
        && (dwKeyType == REG_SZ)
        && (_strnicmp(szVideoLocation, "\\REGISTRY\\Machine", 17) == 0))
    {
        /* open that branch */
        status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, &szVideoLocation[18], 0, KEY_READ | (writable ? KEY_WRITE : 0), &hkeyVideo);
    }
    else
    {
        printf("Error opening registry key '%s'\n", &szVideoLocation[18]);
    }
    RegCloseKey(hkeyDeviceMap);
    return hkeyVideo;
}

void handleGetVideoAcceleration(int argc, char *argv[])
{
    ULONG status;
    HKEY hkeyVideo = getVideoKey(false);

    if (hkeyVideo)
    {
        /* query the actual value */
        DWORD fAcceleration = 1;
        DWORD len = sizeof(fAcceleration);
        DWORD dwKeyType;
        status = RegQueryValueExA(hkeyVideo, "EnableVideoAccel", NULL, &dwKeyType, (LPBYTE)&fAcceleration, &len);
        if (status != ERROR_SUCCESS)
            printf("Video acceleration: default\n");
        else
            printf("Video acceleration: %s\n", fAcceleration ? "on" : "off");
        RegCloseKey(hkeyVideo);
    }
}

void handleSetVideoAcceleration(int argc, char *argv[])
{
    ULONG status;
    HKEY hkeyVideo;

    /* must have exactly one argument: the new offset */
    if (   (argc != 1)
        || (   strcmp(argv[0], "on")
            && strcmp(argv[0], "off")))
    {
        printf("Error: invalid video acceleration status!\n");
        return;
    }

    hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        int fAccel = 0;
        if (!strcmp(argv[0], "on"))
            fAccel = 1;
        /* set a new value */
        status = RegSetValueExA(hkeyVideo, "EnableVideoAccel", 0, REG_DWORD, (LPBYTE)&fAccel, sizeof(fAccel));
        if (status != ERROR_SUCCESS)
        {
            printf("Error %d writing video acceleration status!\n", status);
        }
        RegCloseKey(hkeyVideo);
    }
}

#define MAX_CUSTOM_MODES 128

/* the table of custom modes */
struct
{
    DWORD xres;
    DWORD yres;
    DWORD bpp;
} customModes[MAX_CUSTOM_MODES] = {0};

void getCustomModes(HKEY hkeyVideo)
{
    ULONG status;
    int curMode = 0;

    /* null out the table */
    memset(customModes, 0, sizeof(customModes));

    do
    {
        char valueName[20];
        DWORD xres, yres, bpp = 0;
        DWORD dwType;
        DWORD dwLen = sizeof(DWORD);

        sprintf(valueName, "CustomMode%dWidth", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&xres, &dwLen);
        if (status != ERROR_SUCCESS)
            break;
        sprintf(valueName, "CustomMode%dHeight", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&yres, &dwLen);
        if (status != ERROR_SUCCESS)
            break;
        sprintf(valueName, "CustomMode%dBPP", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&bpp, &dwLen);
        if (status != ERROR_SUCCESS)
            break;

        /* check if the mode is OK */
        if (   (xres > (1 << 16))
            && (yres > (1 << 16))
            && (   (bpp != 16)
                || (bpp != 24)
                || (bpp != 32)))
            break;

        /* add mode to table */
        customModes[curMode].xres = xres;
        customModes[curMode].yres = yres;
        customModes[curMode].bpp  = bpp;

        ++curMode;

        if (curMode >= MAX_CUSTOM_MODES)
            break;
    } while(1);
}

void writeCustomModes(HKEY hkeyVideo)
{
    ULONG status;
    int tableIndex = 0;
    int modeIndex = 0;

    /* first remove all values */
    for (int i = 0; i < MAX_CUSTOM_MODES; i++)
    {
        char valueName[20];
        sprintf(valueName, "CustomMode%dWidth", i);
        RegDeleteValueA(hkeyVideo, valueName);
        sprintf(valueName, "CustomMode%dHeight", i);
        RegDeleteValueA(hkeyVideo, valueName);
        sprintf(valueName, "CustomMode%dBPP", i);
        RegDeleteValueA(hkeyVideo, valueName);
    }

    do
    {
        if (tableIndex >= MAX_CUSTOM_MODES)
            break;

        /* is the table entry present? */
        if (   (!customModes[tableIndex].xres)
            || (!customModes[tableIndex].yres)
            || (!customModes[tableIndex].bpp))
        {
            tableIndex++;
            continue;
        }

        printf("writing mode %d (%dx%dx%d)\n", modeIndex, customModes[tableIndex].xres, customModes[tableIndex].yres, customModes[tableIndex].bpp);
        char valueName[20];
        sprintf(valueName, "CustomMode%dWidth", modeIndex);
        status = RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].xres,
                                sizeof(customModes[tableIndex].xres));
        sprintf(valueName, "CustomMode%dHeight", modeIndex);
        RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].yres,
                       sizeof(customModes[tableIndex].yres));
        sprintf(valueName, "CustomMode%dBPP", modeIndex);
        RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].bpp,
                       sizeof(customModes[tableIndex].bpp));

        modeIndex++;
        tableIndex++;

    } while(1);

}

void handleListCustomModes(int argc, char *argv[])
{
    if (argc != 0)
    {
        printf("Error: too many parameters!");
        return;
    }

    HKEY hkeyVideo = getVideoKey(false);

    if (hkeyVideo)
    {
        getCustomModes(hkeyVideo);
        for (int i = 0; i < (sizeof(customModes) / sizeof(customModes[0])); i++)
        {
            if (   !customModes[i].xres
                || !customModes[i].yres
                || !customModes[i].bpp)
                continue;

            printf("Mode: %d x %d x %d\n",
                   customModes[i].xres, customModes[i].yres, customModes[i].bpp);
        }
        RegCloseKey(hkeyVideo);
    }
}

void handleAddCustomMode(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Error: not enough parameters!\n");
        return;
    }

    DWORD xres = atoi(argv[0]);
    DWORD yres = atoi(argv[1]);
    DWORD bpp  = atoi(argv[2]);

    /** @todo better check including xres mod 8 = 0! */
    if (   (xres > (1 << 16))
        && (yres > (1 << 16))
        && (   (bpp != 16)
            || (bpp != 24)
            || (bpp != 32)))
    {
        printf("Error: invalid mode specified!\n");
        return;
    }

    HKEY hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        getCustomModes(hkeyVideo);
        for (int i = 0; i < MAX_CUSTOM_MODES; i++)
        {
            /* item free? */
            if (!customModes[i].xres)
            {
                customModes[i].xres = xres;
                customModes[i].yres = yres;
                customModes[i].bpp  = bpp;
                break;
            }
        }
        writeCustomModes(hkeyVideo);
        RegCloseKey(hkeyVideo);
    }
}

void handleRemoveCustomMode(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Error: not enough parameters!\n");
        return;
    }

    DWORD xres = atoi(argv[0]);
    DWORD yres = atoi(argv[1]);
    DWORD bpp  = atoi(argv[2]);

    HKEY hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        getCustomModes(hkeyVideo);
        for (int i = 0; i < MAX_CUSTOM_MODES; i++)
        {
            /* correct item? */
            if (   (customModes[i].xres == xres)
                && (customModes[i].yres == yres)
                && (customModes[i].bpp  == bpp))
            {
printf("found mode at index %d\n", i);
                memset(&customModes[i], 0, sizeof(customModes[i]));
                break;
            }
        }
        writeCustomModes(hkeyVideo);
        RegCloseKey(hkeyVideo);
    }
}


/**
 * Main function
 */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    /* determine which command */
    if (strcmp(argv[1], "getvideoacceleration") == 0)
    {
        handleGetVideoAcceleration(argc - 2, &argv[2]);
    }
    else if (strcmp(argv[1], "setvideoacceleration") == 0)
    {
        handleSetVideoAcceleration(argc - 2, &argv[2]);
    }
    else if (strcmp(argv[1], "listcustommodes") == 0)
    {
        handleListCustomModes(argc - 2, &argv[2]);
    }
    else if (strcmp(argv[1], "addcustommode") == 0)
    {
        handleAddCustomMode(argc - 2, &argv[2]);
    }
    else if (strcmp(argv[1], "removecustommode") == 0)
    {
        handleRemoveCustomMode(argc - 2, &argv[2]);
    }
    else
    {
        printHelp();
        return 1;
    }

    return 0;
}
