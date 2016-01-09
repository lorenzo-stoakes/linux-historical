O_TARGET	:= kdba.o
obj-y		:= kdba_bt.o kdba_bp.o kdba_id.o kdba_io.o ppc-dis.o ppc-opc.o kdbasupport.o  start.o kdba_utils.o

# Warning: running with a minimal-toc means that kdb_setjmp will break
# due to saving the wrong r30. A solution would be to move it into setjmp.S
EXTRA_CFLAGS = -mno-minimal-toc

# override CFLAGS := $(CFLAGS) -g

override CFLAGS := $(CFLAGS) -I.

# Translate to Rules.make lists.
O_OBJS		:= $(filter-out	$(export-objs), $(obj-y))
OX_OBJS		:= $(filter	$(export-objs), $(obj-y))

include $(TOPDIR)/Rules.make
