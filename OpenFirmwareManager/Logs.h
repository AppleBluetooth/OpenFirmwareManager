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
 */

#ifndef _OFM_LOGS_H
#define _OFM_LOGS_H

#define OpenLog kprintf

#define AlwaysLog(name, format, ...) do { OpenLog("[" OS_STRINGIFY(PRODUCT_NAME) "][" name "] -- " format, ## __VA_ARGS__); } while (0)
#define DebugLog(name, format, ...) do { } while (0)
#ifdef DEBUG
#undef DebugLog
#define DebugLog AlwaysLog
#endif

#endif
