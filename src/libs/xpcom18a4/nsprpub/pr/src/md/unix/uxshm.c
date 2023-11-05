/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** uxshm.c -- Unix Implementations NSPR Named Shared Memory
**
**
** lth. Jul-1999.
**
*/
#include <string.h>
#include <prshm.h>
#include <prerr.h>
#include <prmem.h>
#include "primpl.h"       
#include <fcntl.h>

extern PRLogModuleInfo *_pr_shm_lm;


#define NSPR_IPC_SHM_KEY 'b'
/*
** Implementation for System V
*/
#if defined PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>

#define _MD_OPEN_SHARED_MEMORY _MD_OpenSharedMemory
#define _MD_ATTACH_SHARED_MEMORY _MD_AttachSharedMemory
#define _MD_DETACH_SHARED_MEMORY _MD_DetachSharedMemory
#define _MD_CLOSE_SHARED_MEMORY _MD_CloseSharedMemory
#define _MD_DELETE_SHARED_MEMORY  _MD_DeleteSharedMemory

extern PRSharedMemory * _MD_OpenSharedMemory( 
    const char *name,
    PRSize      size,
    PRIntn      flags,
    PRIntn      mode
)
{
    PRStatus rc = PR_SUCCESS;
    key_t   key;
    PRSharedMemory *shm;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return( NULL );
    }

    shm = PR_NEWZAP( PRSharedMemory );
    if ( NULL == shm ) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New PRSharedMemory out of memory")); 
        return( NULL );
    }

    shm->ipcname = (char*)PR_MALLOC( strlen( ipcname ) + 1 );
    if ( NULL == shm->ipcname )
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New shm->ipcname out of memory")); 
        return( NULL );
    }

    /* copy args to struct */
    strcpy( shm->ipcname, ipcname );
    shm->size = size; 
    shm->mode = mode; 
    shm->flags = flags;
    shm->ident = _PR_SHM_IDENT;

    /* create the file first */
    if ( flags & PR_SHM_CREATE )  {
        int osfd = open( shm->ipcname, (O_RDWR | O_CREAT), shm->mode );
        if ( -1 == osfd ) {
            _PR_MD_MAP_OPEN_ERROR( errno );
            PR_FREEIF( shm->ipcname );
            PR_DELETE( shm );
            return( NULL );
        } 
        if ( close(osfd) == -1 ) {
            _PR_MD_MAP_CLOSE_ERROR( errno );
            PR_FREEIF( shm->ipcname );
            PR_DELETE( shm );
            return( NULL );
        }
    }

    /* hash the shm.name to an ID */
    key = ftok( shm->ipcname, NSPR_IPC_SHM_KEY );
    if ( -1 == key )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): ftok() failed on name: %s", shm->ipcname));
        PR_FREEIF( shm->ipcname );
        PR_DELETE( shm );
        return( NULL );
    }

    /* get the shared memory */
    if ( flags & PR_SHM_CREATE )  {
        shm->id = shmget( key, shm->size, ( shm->mode | IPC_CREAT|IPC_EXCL));
        if ( shm->id >= 0 ) {
            return( shm );
        }
        if ((errno == EEXIST) && (flags & PR_SHM_EXCL)) {
            PR_SetError( PR_FILE_EXISTS_ERROR, errno );
            PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
                ("_MD_OpenSharedMemory(): shmget() exclusive failed, errno: %d", errno));
            PR_FREEIF(shm->ipcname);
            PR_DELETE(shm);
            return(NULL);
        }
    } 

    shm->id = shmget( key, shm->size, shm->mode );
    if ( -1 == shm->id ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): shmget() failed, errno: %d", errno));
        PR_FREEIF(shm->ipcname);
        PR_DELETE(shm);
        return(NULL);
    }

    return( shm );
} /* end _MD_OpenSharedMemory() */

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    void        *addr;
    PRUint32    aFlags = shm->mode;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    aFlags |= (flags & PR_SHM_READONLY )? SHM_RDONLY : 0;

    addr = shmat( shm->id, NULL, aFlags );
    if ( (void*)-1 == addr )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): shmat() failed on name: %s, OsError: %d", 
                shm->ipcname, PR_GetOSError() ));
        addr = NULL;
    }

    return addr;
}    

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PRStatus rc = PR_SUCCESS;
    PRIntn   urc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = shmdt( addr );
    if ( -1 == urc )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DetachSharedMemory(): shmdt() failed on name: %s", shm->ipcname ));
    }

    return rc;
}    

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    PR_FREEIF(shm->ipcname);
    PR_DELETE(shm);

    return PR_SUCCESS;
}    

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    PRStatus rc = PR_SUCCESS;
    key_t   key;
    int     id;
    PRIntn  urc;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return(PR_FAILURE);
    }

    /* create the file first */ 
    {
        int osfd = open( ipcname, (O_RDWR | O_CREAT), 0666 );
        if ( -1 == osfd ) {
            _PR_MD_MAP_OPEN_ERROR( errno );
            return( PR_FAILURE );
        } 
        if ( close(osfd) == -1 ) {
            _PR_MD_MAP_CLOSE_ERROR( errno );
            return( PR_FAILURE );
        }
    }

    /* hash the shm.name to an ID */
    key = ftok( ipcname, NSPR_IPC_SHM_KEY );
    if ( -1 == key )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): ftok() failed on name: %s", ipcname));
    }

    id = shmget( key, 0, 0 );
    if ( -1 == id ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shmget() failed, errno: %d", errno));
        return(PR_FAILURE);
    }

    urc = shmctl( id, IPC_RMID, NULL );
    if ( -1 == urc )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shmctl() failed on name: %s", ipcname ));
        return(PR_FAILURE);
    }

    urc = unlink( ipcname );
    if ( -1 == urc ) {
        _PR_MD_MAP_UNLINK_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): unlink() failed: %s", ipcname ));
        return(PR_FAILURE);
    }

    return rc;
}  /* end _MD_DeleteSharedMemory() */

/*
** Implementation for Posix
*/
#elif defined PR_HAVE_POSIX_NAMED_SHARED_MEMORY
#include <sys/mman.h>

#define _MD_OPEN_SHARED_MEMORY _MD_OpenSharedMemory
#define _MD_ATTACH_SHARED_MEMORY _MD_AttachSharedMemory
#define _MD_DETACH_SHARED_MEMORY _MD_DetachSharedMemory
#define _MD_CLOSE_SHARED_MEMORY _MD_CloseSharedMemory
#define _MD_DELETE_SHARED_MEMORY  _MD_DeleteSharedMemory

struct _MDSharedMemory {
    int     handle;
};

extern PRSharedMemory * _MD_OpenSharedMemory( 
    const char *name,
    PRSize      size,
    PRIntn      flags,
    PRIntn      mode
)
{
    PRStatus    rc = PR_SUCCESS;
    PRInt32     end;
    PRSharedMemory *shm;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return( NULL );
    }

    shm = PR_NEWZAP( PRSharedMemory );
    if ( NULL == shm ) 
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New PRSharedMemory out of memory")); 
        return( NULL );
    }

    shm->ipcname = PR_MALLOC( strlen( ipcname ) + 1 );
    if ( NULL == shm->ipcname )
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0 );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, ( "PR_OpenSharedMemory: New shm->ipcname out of memory")); 
        return( NULL );
    }

    /* copy args to struct */
    strcpy( shm->ipcname, ipcname );
    shm->size = size; 
    shm->mode = mode;
    shm->flags = flags;
    shm->ident = _PR_SHM_IDENT;

    /*
    ** Create the shared memory
    */
    if ( flags & PR_SHM_CREATE )  {
        int oflag = (O_CREAT | O_RDWR);
        
        if ( flags & PR_SHM_EXCL )
            oflag |= O_EXCL;
        shm->id = shm_open( shm->ipcname, oflag, shm->mode );
    } else {
        shm->id = shm_open( shm->ipcname, O_RDWR, shm->mode );
    }

    if ( -1 == shm->id )  {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): shm_open failed: %s, OSError: %d",
                shm->ipcname, PR_GetOSError())); 
        PR_DELETE( shm->ipcname );
        PR_DELETE( shm );
        return(NULL);
    }

    end = ftruncate( shm->id, shm->size );
    if ( -1 == end ) {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG(_pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): ftruncate failed, OSError: %d",
                PR_GetOSError()));
        PR_DELETE( shm->ipcname );
        PR_DELETE( shm );
        return(NULL);
    }

    return(shm);
} /* end _MD_OpenSharedMemory() */

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags )
{
    void        *addr;
    PRIntn      prot = (PROT_READ | PROT_WRITE);

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    if ( PR_SHM_READONLY == flags)
        prot ^= PROT_WRITE;

    addr = mmap( (void*)0, shm->size, prot, MAP_SHARED, shm->id, 0 );
    if ((void*)-1 == addr )
    {
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): mmap failed: %s, errno: %d",
                shm->ipcname, PR_GetOSError()));
        addr = NULL;
    } else {
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_AttachSharedMemory(): name: %s, attached at: %p", shm->ipcname, addr));
    }
    
    return addr;
}    

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr )
{
    PRStatus    rc = PR_SUCCESS;
    PRIntn      urc;

    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = munmap( addr, shm->size );
    if ( -1 == urc )
    {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DetachSharedMemory(): munmap failed: %s, errno: %d", 
                shm->ipcname, PR_GetOSError()));
    }
    return rc;
}    

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm )
{
    int urc;
    
    PR_ASSERT( shm->ident == _PR_SHM_IDENT );

    urc = close( shm->id );
    if ( -1 == urc ) {
        _PR_MD_MAP_CLOSE_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_CloseSharedMemory(): close() failed, error: %d", PR_GetOSError()));
        return(PR_FAILURE);
    }
    PR_DELETE( shm->ipcname );
    PR_DELETE( shm );
    return PR_SUCCESS;
}    

extern PRStatus _MD_DeleteSharedMemory( const char *name )
{
    PRStatus    rc = PR_SUCCESS;
    PRUintn     urc;
    char        ipcname[PR_IPC_NAME_SIZE];

    rc = _PR_MakeNativeIPCName( name, ipcname, PR_IPC_NAME_SIZE, _PRIPCShm );
    if ( PR_FAILURE == rc )
    {
        PR_SetError( PR_UNKNOWN_ERROR , errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_OpenSharedMemory(): _PR_MakeNativeIPCName() failed: %s", name ));
        return rc;
    }

    urc = shm_unlink( ipcname );
    if ( -1 == urc ) {
        rc = PR_FAILURE;
        _PR_MD_MAP_DEFAULT_ERROR( errno );
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): shm_unlink failed: %s, errno: %d", 
                ipcname, PR_GetOSError()));
    } else {
        PR_LOG( _pr_shm_lm, PR_LOG_DEBUG, 
            ("_MD_DeleteSharedMemory(): %s, success", ipcname));
    }

    return rc;
} /* end _MD_DeleteSharedMemory() */
#endif

