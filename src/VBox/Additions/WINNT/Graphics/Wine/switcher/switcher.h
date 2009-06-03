/* $Id$ */

/** @file
 * VBox D3D8/9 dll switcher
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___CROPENGL_SWITCHER_H_
#define ___CROPENGL_SWITCHER_H_

typedef BOOL (APIENTRY *DrvValidateVersionProc)(DWORD version);

#define SW_FILLPROC(dispatch, hdll, name) \
    dispatch.p##name = (name##Proc) GetProcAddress(hdll, #name);

#define SW_DISPINIT(dispatch)                                   \
    {                                                           \
        if (!dispatch.initialized)                              \
        {                                                       \
            InitD3DExports(dispatch.vboxName, dispatch.msName); \
            dispatch.initialized = 1;                           \
        }                                                       \
    }

#define SW_CHECKRET(dispatch, func, failret)   \
    {                                          \
        SW_DISPINIT(dispatch)                  \
        if (!dispatch.p##func)                 \
            return failret;                    \
    }

#define SW_CHECKCALL(dispatch, func)    \
    {                                   \
        SW_DISPINIT(dispatch)           \
        if (!dispatch.p##func) return;  \
    }

extern BOOL IsVBox3DEnabled(void);
extern BOOL CheckOptions(void);
extern void FillD3DExports(HANDLE hDLL);
extern void InitD3DExports(const char *vboxName, const char *msName);

#endif /* #ifndef ___CROPENGL_SWITCHER_H_ */