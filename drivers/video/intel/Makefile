# $DHD: linux/drivers/video/intel/Makefile,v 1.1.2.2 2003/02/05 00:07:45 dawes Exp $
# $TG$

O_TARGET := intelfb.o

obj-$(CONFIG_FB_INTEL)	+= intelfbdrv.o intelfbhw.o

include $(TOPDIR)/Rules.make

intelfbdrv.o: builtinmodes.c

builtinmodes.c: modes.c
	rm -f builtinmodes.c
	ln -s modes.c builtinmodes.c

clean:
	rm -f core *.o *.a *.s builtinmodes.c

