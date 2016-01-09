#ifndef _SPEEDCRC_H_
#define _SPEEDCRC_H_

/******************************************************************************
 *  speedcrc.h  --  CRC library for use with speedtouch.
 *
 *  Copyright (C) 2000, Johan Verrept
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ******************************************************************************/

unsigned long spdcrc32 (char *mem, int len, unsigned initial);
#define crc32_be(crc, mem, len) spdcrc32(mem, len, crc)

#endif				/* _SPEEDCRC_H_ */
