# Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version
# 2 of the License, or (at your option) any later version.

# File: drivers/net/WAN/atiXX50/Makefile
#
# Makefile for the Aurora ESSC based cards
# Specifically the 2520, 4020, 4520, 8520
#

all: ASLX.o 8253xcfg 8253xmac eprom9050 8253xspeed 8253xpeer eprom9050

O_TARGET := ASLX.o

obj-y := 8253xini.o 8253xnet.o 8253xsyn.o crc32.o 8253xdbg.o 8253xplx.o 8253xtty.o 8253xchr.o 8253xint.o amcc5920.o 8253xmcs.o 8253xutl.o
obj-m := ASLX.o

#EXTRA_CFLAGS += -I. -DFREEININTERRUPT 
EXTRA_CFLAGS += -I.

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s 8253xcfg 8253xmac eprom9050 *~

8253xcfg: 8253xcfg.c
	$(CC)  -o 8253xcfg $(EXTRA_CFLAGS) -U__KERNEL__ 8253xcfg.c

8253xmac: 8253xmac.c
	$(CC)  -o 8253xmac $(EXTRA_CFLAGS) -U__KERNEL__ 8253xmac.c

8253xspeed: 8253xspeed.c
	$(CC)  -o 8253xspeed $(EXTRA_CFLAGS) -U__KERNEL__ 8253xspeed.c

8253xpeer: 8253xpeer.c
	$(CC)  -o 8253xpeer $(EXTRA_CFLAGS) -U__KERNEL__ 8253xpeer.c

eprom9050: eprom9050.c
	$(CC)  -o eprom9050 $(EXTRA_CFLAGS) -U__KERNEL__ eprom9050.c


