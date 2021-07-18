/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  Copyright (c) 2021 cjiang. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  Zlib implementation based on /apple/xnu/libkern/c++/OSKext.cpp
 */

#include "zutil.h"

void * zalloc(void * opaque, UInt32 items, UInt32 size)
{
   void * result = NULL;
   z_mem * zmem = NULL;
   UInt32 allocSize =  items * size + sizeof(zmem);
   
   zmem = (z_mem *) IOMalloc(allocSize);
   
   if (zmem)
   {
       zmem->allocSize = allocSize;
       result = (void *) &(zmem->data);
   }
   
   return result;
}

void zfree(void * opaque, void * ptr)
{
   UInt32 * skipper = (UInt32 *) ptr - 1;
   z_mem * zmem = (z_mem *) skipper;
   IOFree((void *) zmem, zmem->allocSize);
}
