/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver            <shaver@mozilla.org>
 *   Christopher Blizzard   <blizzard@mozilla.org>
 *   Jason Eager            <jce2@po.cwru.edu>
 *   Stuart Parmenter       <pavlov@netscape.com>
 *   Brendan Eich           <brendan@mozilla.org>
 *   Pete Collins           <petejc@mozdev.org>
 *   Paul Ashford           <arougthopher@lizardland.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * Implementation of nsIFile for L4 systems.
 *
 * Notes to all those who have something to do with this file again: I have told almost
 * all functions to return NS_ERROR_FAILURE.  A good starting point if you need to
 * implement any of it would be to search/replace those with standard assertions.
 */

#include "nsDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsLocalFile.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "prproces.h"
#include "nsISimpleEnumerator.h"
#include "nsITimelineService.h"

#include "nsNativeCharsetUtils.h"

#if 0
/* directory enumerator */
class NS_COM
nsDirEnumeratorL4 : public nsISimpleEnumerator
{
    public:
    nsDirEnumeratorL4();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    NS_IMETHOD Init(nsLocalFile *parent, PRBool ignored);

    private:
    ~nsDirEnumeratorL4();

    protected:
    NS_IMETHOD GetNextEntry();

};

nsDirEnumeratorL4::nsDirEnumeratorL4() :
                         mDir(nsnull), 
                         mEntry(nsnull)
{
    NS_ASSERTION(0, "nsDirEnumeratorL4 created!");
}

nsDirEnumeratorL4::~nsDirEnumeratorL4()
{
}

NS_IMPL_ISUPPORTS1(nsDirEnumeratorL4, nsISimpleEnumerator)

NS_IMETHODIMP
nsDirEnumeratorL4::Init(nsLocalFile *parent, PRBool resolveSymlinks /*ignored*/)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirEnumeratorL4::HasMoreElements(PRBool *result)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirEnumeratorL4::GetNext(nsISupports **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDirEnumeratorL4::GetNextEntry()
{
    return NS_ERROR_FAILURE;
}
#endif /* 0 */

nsLocalFile::nsLocalFile()
{
    NS_ASSERTION(0, "nsLocalFile created!");
}

nsLocalFile::nsLocalFile(const nsLocalFile& other)
{
    NS_ASSERTION(0, "nsLocalFile created!");
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLocalFile,
                              nsIFile,
                              nsILocalFile)

nsresult
nsLocalFile::nsLocalFileConstructor(nsISupports *outer, 
                                    const nsIID &aIID,
                                    void **aInstancePtr)
{
    NS_ASSERTION(0, "nsLocalFile::nsLocalFileConstructor called!");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Clone(nsIFile **file)
{
    // Just copy-construct ourselves
    *file = new nsLocalFile(*this);
    if (!*file)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*file);
    
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithNativePath(const nsACString &filePath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Create(PRUint32 type, PRUint32 permissions)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString &fragment)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString &fragment)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_ERROR_FAILURE;
}

void
nsLocalFile::LocateNativeLeafName(nsACString::const_iterator &begin, 
                                  nsACString::const_iterator &end)
{
}

NS_IMETHODIMP
nsLocalFile::GetNativeLeafName(nsACString &aLeafName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetNativeLeafName(const nsACString &aLeafName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetNativePath(nsACString &_retval)
{
    return NS_ERROR_FAILURE;
}

nsresult
nsLocalFile::GetNativeTargetPathName(nsIFile *newParent, 
                                     const nsACString &newName,
                                     nsACString &_retval)
{
    return NS_ERROR_FAILURE;
}

nsresult
nsLocalFile::CopyDirectoryTo(nsIFile *newParent)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::CopyToNative(nsIFile *newParent, const nsACString &newName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParent, const nsACString &newName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile *newParent, const nsACString &newName)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Remove(PRBool recursive)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModTime)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModTime)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModTimeOfLink)
{
    return NS_ERROR_FAILURE;
}

/*
 * utime(2) may or may not dereference symlinks, joy.
 */
NS_IMETHODIMP
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModTimeOfLink)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile **aParent)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Exists(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsReadable(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsExecutable(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsDirectory(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsFile(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{
    return NS_ERROR_FAILURE;
}

/* attribute PRBool followLinks; */
NS_IMETHODIMP
nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator **entries)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Reveal()
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Launch()
{
    return NS_ERROR_FAILURE;
}

nsresult
NS_NewNativeLocalFile(const nsACString &path, PRBool followSymlinks, nsILocalFile **result)
{
    return NS_ERROR_FAILURE;
}

// Unicode interface Wrapper
nsresult  
nsLocalFile::InitWithPath(const nsAString &filePath)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::Append(const nsAString &node)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::AppendRelativePath(const nsAString &node)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::GetPath(nsAString &_retval)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    return NS_ERROR_FAILURE;
}
nsresult  
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    return NS_ERROR_FAILURE;
}
nsresult
nsLocalFile::GetTarget(nsAString &_retval)
{   
    return NS_ERROR_FAILURE;
}
nsresult 
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// global init/shutdown
//-----------------------------------------------------------------------------

void
nsLocalFile::GlobalInit()
{
}

void
nsLocalFile::GlobalShutdown()
{
}
