AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = \
	-I../libnpfs -I../liblsd -I../libdiod -I../libfblkmap -I../../host-kernel/usr/include/ -DRING_BUFFER_CONF_NO_DOUBLE_MMAP

sbin_PROGRAMS = diod

diod_LDADD = \
	$(top_builddir)/libdiod/libdiod.a \
	$(top_builddir)/libnpclient/libnpclient.a \
	$(top_builddir)/libnpfs/libnpfs.a \
	$(top_builddir)/liblsd/liblsd.a \
	$(top_builddir)/libfblkmap/libfblkmap.a \
	$(LIBWRAP) $(LIBPTHREAD) $(LIBLUA) $(LIBMUNGE) $(LIBCAP) \
	$(LIBIBVERBS) $(LIBRDMACM) $(LIBTCMALLOC) \
	$(top_builddir)/pci-ring-buffer/build_user/x86_64/pci_ring_buffer.a \
	-lscif -lrt -lpthread

diod_SOURCES = \
	diod.c \
	ops.c \
	ops.h \
	exp.c \
	exp.h \
	ioctx.c \
	ioctx.h \
	fid.c \
	fid.h \
	xattr.c \
	xattr.h \
	zcio.c \
	zcio.h

man8_MANS = \
        diod.8
