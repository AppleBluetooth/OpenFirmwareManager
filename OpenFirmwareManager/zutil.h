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

#ifndef _OFM_ZUTIL_H
#define _OFM_ZUTIL_H

#include <IOKit/IOLib.h>
#include <IOKit/IOTypes.h>
#include <libkern/zlib.h>

typedef struct z_mem
{
    UInt32 allocSize;
    UInt8 data[0];
} z_mem;

extern void * zalloc(void * opaque, UInt32 items, UInt32 size);
extern void zfree(void * opaque, void * ptr);

#endif
