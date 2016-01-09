#
# Makefile for the SiS framebuffer device driver
#

O_TARGET := sisfb.o
obj-y    := sis_main.o init.o init301.o

# All of the (potential) objects that export symbols.

export-objs    :=  sis_main.o

# Each configuration option enables a list of files.

obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o
