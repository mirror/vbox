/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * MODULE NOTES:
 *
 * The Deque is a very small, very efficient container object
 * than can hold elements of type void*, offering the following features:
 *    Its interface supports pushing and popping of elements.
 *    It can iterate (via an interator class) its elements.
 *    When full, it can efficiently resize dynamically.
 *
 *
 * NOTE: The only bit of trickery here is that this deque is
 * built upon a ring-buffer. Like all ring buffers, the first
 * element may not be at index[0]. The mOrigin member determines
 * where the first child is. This point is quietly hidden from
 * customers of this class.
 *
 */

#ifndef _NSDEQUE
#define _NSDEQUE

#include "nscore.h"

/******************************************************
 * Here comes the nsDeque class itself...
 ******************************************************/

/**
 * The deque (double-ended queue) class is a common container type,
 * whose behavior mimics a line in your favorite checkout stand.
 * Classic CS describes the common behavior of a queue as FIFO.
 * A deque allows insertion and removal at both ends of
 * the container.
 *
 * The deque stores pointers to items.
 */
class NS_COM nsDeque {
  public:
   nsDeque();
  ~nsDeque();

  /**
   * Returns the number of elements currently stored in
   * this deque.
   *
   * @return  number of elements currently in the deque
   */
  inline PRInt32 GetSize() const {return mSize;}

  /**
   * Appends new member at the end of the deque.
   *
   * @param   item to store in deque
   * @return  *this
   */
  nsDeque& Push(void* aItem);

  /**
   * Inserts new member at the front of the deque.
   *
   * @param   item to store in deque
   * @return  *this
   */
  nsDeque& PushFront(void* aItem);

  /**
   * Remove and return the last item in the container.
   *
   * @return  the item that was the last item in container
   */
  void* Pop();

  /**
   * Remove and return the first item in the container.
   *
   * @return  the item that was first item in container
   */
  void* PopFront();

  /**
   * Retrieve the bottom item without removing it.
   *
   * @return  the first item in container
   */

  void* Peek();
  /**
   * Return topmost item without removing it.
   *
   * @return  the first item in container
   */
  void* PeekFront();

  /**
   * Retrieve the i'th member from the deque without removing it.
   *
   * @param   index of desired item
   * @return  i'th element in list
   */
  void* ObjectAt(int aIndex) const;

  /**
   * Remove all items from container without destroying them.
   *
   * @return  *this
   */
  nsDeque& Empty();

  /**
   * Remove and delete all items from container.
   * Deletes are handled by the deallocator nsDequeFunctor
   * which is specified at deque construction.
   *
   * @return  *this
   */
  nsDeque& Erase();

protected:
  PRInt32         mSize;
  PRInt32         mCapacity;
  PRInt32         mOrigin;
  void*           mBuffer[8];
  void**          mData;

private:

  /**
   * Copy constructor (PRIVATE)
   *
   * @param another deque
   */
  nsDeque(const nsDeque& other);

  /**
   * Deque assignment operator (PRIVATE)
   *
   * @param   another deque
   * @return  *this
   */
  nsDeque& operator=(const nsDeque& anOther);

  PRInt32 GrowCapacity();
};

#endif
