#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (c) 1999-2004 Silicon Graphics, Inc.  All Rights Reserved.
#

obj-$(CONFIG_KDB)	+= kdba_bp.o x86-dis.o \
			   kdba_io.o kdba_id.o kdba_support.o

override CFLAGS := $(CFLAGS:%-pg=% )

CFLAGS_kdba_io.o += -I $(srctree)/arch/$(SRCARCH)/kdb
