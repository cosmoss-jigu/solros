PROJ_ROOT := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))/..
BUILD_DIR := ${PROJ_ROOT}/build
SRC_DIR := ${PROJ_ROOT}/src
SCRIPTS_DIR := ${PROJ_ROOT}/../scripts
MAKE :=make --no-print-directory

# compiler settings
CC := icc
ifeq ("${CC}","icc")
  CC_SETUP := source ${SCRIPTS_DIR}/icc-setup.sh
  export CC := icc
  export CXX := icpc
else
  CC_SETUP := :
  export CC :=
  export CXX :=
endif

# target settings
BUILD_MODES = Debug Release
TARGET_ARCHS = x86_64 k1om

# misc.
NLCPU := ${shell nproc}

# mtcp & dpdk
MTCP_PATH := ${PROJ_ROOT}/mtcp/mtcp
DPDK_PATH := ${PROJ_ROOT}/dpdk
MTCP_LIBS := ${MTCP_PATH}/lib/libmtcp.a
DPDK_LIBS := ${DPDK_PATH}/lib/librte_distributor.a ${DPDK_PATH}/lib/librte_kni.a ${DPDK_PATH}/lib/librte_pipeline.a  ${DPDK_PATH}/lib/librte_table.a ${DPDK_PATH}/lib/librte_port.a ${DPDK_PATH}/lib/librte_timer.a ${DPDK_PATH}/lib/librte_hash.a ${DPDK_PATH}/lib/librte_lpm.a ${DPDK_PATH}/lib/librte_power.a ${DPDK_PATH}/lib/librte_acl.a ${DPDK_PATH}/lib/librte_meter.a ${DPDK_PATH}/lib/librte_sched.a ${DPDK_PATH}/lib/librte_kvargs.a ${DPDK_PATH}/lib/librte_mbuf.a ${DPDK_PATH}/lib/librte_ip_frag.a ${DPDK_PATH}/lib/libethdev.a ${DPDK_PATH}/lib/librte_mempool.a ${DPDK_PATH}/lib/librte_ring.a ${DPDK_PATH}/lib/librte_eal.a ${DPDK_PATH}/lib/librte_cmdline.a ${DPDK_PATH}/lib/librte_cfgfile.a ${DPDK_PATH}/lib/librte_pmd_bond.a ${DPDK_PATH}/lib/librte_pmd_vmxnet3_uio.a ${DPDK_PATH}/lib/librte_pmd_mlx4.a ${DPDK_PATH}/lib/librte_pmd_mlx5.a ${DPDK_PATH}/lib/librte_pmd_ring.a


# make targets
all: make

cmake:
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			mkdir -p ${BUILD_DIR}/$$MODE-$$ARCH; \
			cd ${BUILD_DIR}/$$MODE-$$ARCH; \
			${CC_SETUP}; \
			TARGET_ARCH=$$ARCH cmake ${SRC_DIR} \
				-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
				-DCMAKE_BUILD_TYPE=$$MODE; \
		); done; \
	); done
	exit

make:
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			echo "-----------------"; \
			echo $$ARCH "-" $$MODE; \
			mkdir -p ${BUILD_DIR}/$$MODE-$$ARCH; \
			cd ${BUILD_DIR}/$$MODE-$$ARCH; \
			${CC_SETUP}; \
			$(MAKE) -j${NLCPU}; \
		); done; \
	); done

dpdk:
	(${CC_SETUP} ; \
	 cd ${PROJ_ROOT}/../mtcp/dpdk-2.2.0 && \
	 make install T=x86_64-native-linuxapp-gcc -j${NLCPU} && \
	 ln -fs ${PROJ_ROOT}/../mtcp/dpdk-2.2.0/x86_64-native-linuxapp-gcc/lib \
	        ${PROJ_ROOT}/../mtcp/dpdk/lib ; \
	 ln -fs ${PROJ_ROOT}/../mtcp/dpdk-2.2.0/x86_64-native-linuxapp-gcc/include \
	        ${PROJ_ROOT}/../mtcp/dpdk/include)

mtcp: dpdk
	(${CC_SETUP} ; \
	 cd ${PROJ_ROOT}/../mtcp && \
	 ./configure --with-dpdk-lib=${PROJ_ROOT}/../mtcp/dpdk/ && \
	 cd ${PROJ_ROOT}/../mtcp/mtcp/src && \
	 make -j${NLCPU})

clean:
	@for ARCH in $(TARGET_ARCHS); do ( : ; \
		for MODE in $(BUILD_MODES); do ( : ; \
			if [ -e ${BUILD_DIR}/$$MODE-$$ARCH/Makefile ]; \
			then \
				cd ${BUILD_DIR}/$$MODE-$$ARCH; \
				$(MAKE) clean; \
			fi; \
		); done; \
	); done
	(cd ${PROJ_ROOT}/../mtcp/dpdk-2.2.0; make clean) ;\
	(cd ${PROJ_ROOT}/../mtcp;            make clean)

tags:
	etags `find . -type f -name '*.cc' -or -name '*.cpp'  -or -name '*.c' -or -name '*.h'`

cscope:
	find ${SRC_DIR} -name '*.cc'  \
	            -or -name '*.cpp' \
	            -or -name '*.c'   \
	            -or -name '*.h' > cscope.files
	cscope -b -q -R

distclean:
	(cd ${PROJ_ROOT}/../mtcp/dpdk-2.2.0; make clean) ;\
	(cd ${PROJ_ROOT}/../mtcp;            make distclean) ;\
	rm -rf ${BUILD_DIR}/* ;\
	rm -f ${DPDK_LIBS} ;\
	rm -f ${MTCP_LIBS} ;\
	rm -f cscope.out tags

help:
	@cat README.md
