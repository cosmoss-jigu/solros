LEVEL = .
include $(LEVEL)/Makefile.inc

all: host-modules phi-modules testcases apps

##
# host modules
host-modules: host-kernel host-mpss nvme-p2p diod network

host-kernel:
	(cd ${HOST_KNL_SRC}; make -j${NJOB} ${ARGS} && make modules -j${NJOB} ${ARGS}  && sudo make modules_install && sudo make install && make headers_install)
	(cd ${HOST_KNL_SRC}/tools; make -j${NJOB} perf; sudo ${TOOLS_ROOT}/install-perf.sh)

host-mpss:
	(cd ${SRC_ROOT}/mpss-modules; make -j${NJOB} ${ARGS} && sudo make install)

nvme-p2p: host-mpss
	(cd ${SRC_ROOT}/nvme-p2p && make -j${NJOB} && sudo make install)

${SRC_ROOT}/diod/Makefile: host-headers
	sudo apt-get install liblua5.1-dev libmunge-dev libwrap0-dev libcap-dev libattr1-dev automake
	(cd ${SRC_ROOT}/diod && ./autogen.sh && ./configure CPPFLAGS="-I/usr/include/lua5.1" --with-lua-suffix=5.1 --enable-prbtrans)

diod: ${SRC_ROOT}/diod/Makefile
	rm -rf ${SRC_ROOT}/pci-ring-buffer/build_user/x86_64 # XXX: ugly!!!
	(cd ${SRC_ROOT}/diod && make -j${NJOB} ${ARGS})

host-headers:
	make -C ${HOST_KNL_SRC} headers_install

${SRC_ROOT}/build/Debug-x86_64/Makefile:
	(cd ${SRC_ROOT}/network/src; make cmake BUILD_MODES=Debug)

${SRC_ROOT}/build/Release-x86_64/Makefile:
	(cd ${SRC_ROOT}/network/src; make cmake BUILD_MODES=Release)

network: ${SRC_ROOT}/build/Debug-x86_64/Makefile ${SRC_ROOT}/build/Release-x86_64/Makefile
	 (cd ${SRC_ROOT}/network/src; make ${ARGS})

##
# phi modules
phi-modules: phi-kernel phi-install network

${PHI_KNL_BLD_ROOT}/.config: ${PHI_KNL_CONF}
	mkdir -p ${PHI_KNL_BLD_ROOT}
	cp -f $< $@

phi-kernel: build-phi-kernel
	make phi-install

build-phi-kernel: ${PHI_KNL_BLD_ROOT}/.config
	mkdir -p ${PHI_KNL_BLD_ROOT}
	(cd ./phi-kernel && . ${MPSS_SETUP} && KBUILD_OUTPUT=${PHI_KNL_BLD_ROOT} make ARCH=k1om CROSS_COMPILE=k1om-mpss-linux- O=${PHI_KNL_BLD_ROOT} -j${NJOB} ${ARGS})
	sudo cp ${PHI_OKNL} ${PHI_OKNL}.old
	sudo cp ${PHI_OSYM} ${PHI_OSYM}.old
	sudo cp ${PHI_NKNL} ${PHI_OKNL}
	sudo cp ${PHI_NSYM} ${PHI_OSYM}

phi-install:
	sudo ${SRC_ROOT}/scripts/postinst-phiknl.py ${PHI_KNL_BLD_ROOT} ${PHI_CDIR} ${PHI_VSN}

##
# testcases
testcases: micro snifftest

micro:
	(cd ${SRC_ROOT}/micro && make -j${NJOB} ${ARGS})

snifftest:
	(cd ${SRC_ROOT}/snifftest && make -j${NJOB} ${ARGS})

##
# applications
apps: fio

fio: host-fio mic-fio

host-fio:
	(cd ${SRC_ROOT}/fio && make clean && make distclean && \
	 mkdir -p ${SRC_ROOT}/fio/bin_host)
	(cd ${SRC_ROOT}/fio && \
	 ${SRC_ROOT}/fio/configure --prefix=${SRC_ROOT}/fio/bin_host && \
	 make -j${NJOB} ${ARGS} && make install)

mic-fio:
	(cd ${SRC_ROOT}/fio && make clean && make distclean && \
	 mkdir -p ${SRC_ROOT}/fio/bin_mic)
	(cd ${SRC_ROOT}/fio && . ${MPSS_SETUP} && \
	 ${SRC_ROOT}/fio/configure --prefix=${SRC_ROOT}/fio/bin_mic \
	   --disable-numa --disable-optimizations \
	   --extra-cflags='-m64 --sysroot=${MPSS_SYSROOT}' \
	   --cc=${PHI_GCC} && \
	 make -j${NJOB} ${ARGS} && make install)

##
# other targets
clean:
	@- rm -rf ${HOST_KNL_BLD_ROOT}
	@- rm -rf ${PHI_KNL_BLD_ROOT}
	@- rm -rf ${SRC_ROOT}/phi-kernel/include/config
	@- rm -f ${PHI_NKNL} ${PHI_NSYM}
	@- rm -f ${SRC_ROOT}/mpss-modules/Module.symvers
	@- (cd ${SRC_ROOT}/nvme-p2p; make clean)
	@- (cd ${SRC_ROOT}/micro; make clean)
	@- (cd ${SRC_ROOT}/snifftest; make clean)
	@- (cd ${SRC_ROOT}/diod; make distclean)
	@- (cd ${SRC_ROOT}/network/src; make distclean)
	@- (cd ${SRC_ROOT}/fio; make distclean; \
	 rm -rf ${SRC_ROOT}/fio/bin_host; \
	 rm -rf ${SRC_ROOT}/fio/bin_mic)

help:
	@ cat README | less

.PHONY: all host-modules phi-modules testcases apps host-kernel host-mpss phi-kernel phi-install nvme-p2p diod micro snifftest clean help

