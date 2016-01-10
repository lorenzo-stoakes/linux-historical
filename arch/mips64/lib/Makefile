#
# Makefile for MIPS-specific library files..
#

USE_STANDARD_AS_RULE := true

L_TARGET = lib.a

obj-y	+= csum_partial.o csum_partial_copy.o dump_tlb.o promlib.o rtc-std.o \
	  rtc-no.o memset.o memcpy.o strlen_user.o strncpy_user.o \
	  strnlen_user.o watch.o

#
# The ugly subst stuff is here only because I hesistate to stuff the IDE and
# floppy bits into the kernel of every machine that possibly might load a
# module ...
#
obj-$(subst m,y,$(CONFIG_BLK_DEV_FD))	+= floppy-no.o floppy-std.o
obj-$(subst m,y,$(CONFIG_IDE))	+= ide-std.o ide-no.o	# needed for ide module
obj-$(CONFIG_PC_KEYB)		+= kbd-std.o

include $(TOPDIR)/Rules.make
