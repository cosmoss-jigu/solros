###
# project setting
project (pcie-cloud-network)
cmake_minimum_required (VERSION 2.8)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

###
# compiler options
# - set compiler-dependent options
IF(NOT $ENV{CC} MATCHES "icc")
  set(VEC_OPT "-ftree-vectorize -ftree-vectorizer-verbose=0")
ENDIF()

# - set target-dependent options
IF($ENV{TARGET_ARCH} MATCHES "k1om")
  set(TARGET_OPT "-mmic -DTARGET_ARCH_K1OM")
ENDIF()

# - set general options
set(COMMON_FLAGS  "-W -Wall -march=native -mtune=native -fno-rtti ${TARGET_OPT} ${VEC_OPT}")
set(DEBUG_FLAGS   "-g -O0 -fno-omit-frame-pointer" )
set(RELEASE_FLAGS "-g -O3 -fno-omit-frame-pointer" ) # XXX: -fno-omit-frame-pointer -g

set(CMAKE_C_FLAGS "${COMMON_FLAGS} ${DEBUG_FLAGS}" )
set(CMAKE_C_FLAGS_RELEASE "${COMMON_FLAGS} ${RELEASE_FLAGS}" )

set(CMAKE_CXX_FLAGS "-std=c++11 ${COMMON_FLAGS} ${DEBUG_FLAGS}" )
set(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 ${COMMON_FLAGS} ${RELEASE_FLAGS}" )

# - set debug options
IF(CMAKE_BUILD_TYPE MATCHES Debug)
  # enables printing debug-information
  add_definitions(-DPCIE_CLOUD_NETWORK_DEBUG)
  add_definitions(-DRING_BUFFER_DEBUG)
  add_definitions(-DRING_BUFFER_TRACE_EVENT)
ENDIF()

# - enable shared listening mode by default
add_definitions(-DPCIE_CLOUD_NETWORK_CONF_SHARED_LISTEN)

# - do not use double mmap for compatability with kernel
add_definitions(-DRING_BUFFER_CONF_NO_DOUBLE_MMAP)

###
# - external libs
set(PRB_PATH  ${CMAKE_CURRENT_SOURCE_DIR}/util/pci-ring-buffer)
set(MTCP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/util/mtcp/mtcp)
set(DPDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/util/dpdk)

set(MTCP_LIBS ${MTCP_PATH}/lib/libmtcp.a)
set(DPDK_LIBS ${DPDK_PATH}/lib/librte_distributor.a ${DPDK_PATH}/lib/librte_kni.a ${DPDK_PATH}/lib/librte_pipeline.a  ${DPDK_PATH}/lib/librte_table.a ${DPDK_PATH}/lib/librte_port.a ${DPDK_PATH}/lib/librte_timer.a ${DPDK_PATH}/lib/librte_hash.a ${DPDK_PATH}/lib/librte_lpm.a ${DPDK_PATH}/lib/librte_power.a ${DPDK_PATH}/lib/librte_acl.a ${DPDK_PATH}/lib/librte_meter.a ${DPDK_PATH}/lib/librte_sched.a ${DPDK_PATH}/lib/librte_kvargs.a ${DPDK_PATH}/lib/librte_mbuf.a ${DPDK_PATH}/lib/librte_ip_frag.a ${DPDK_PATH}/lib/libethdev.a ${DPDK_PATH}/lib/librte_mempool.a ${DPDK_PATH}/lib/librte_ring.a ${DPDK_PATH}/lib/librte_eal.a ${DPDK_PATH}/lib/librte_cmdline.a ${DPDK_PATH}/lib/librte_cfgfile.a ${DPDK_PATH}/lib/librte_pmd_bond.a ${DPDK_PATH}/lib/librte_pmd_vmxnet3_uio.a ${DPDK_PATH}/lib/librte_pmd_mlx4.a ${DPDK_PATH}/lib/librte_pmd_mlx5.a ${DPDK_PATH}/lib/librte_pmd_ring.a)

###
# - where to find cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")

set(PCIE_CLOUD_NETWORK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include ${PRB_PATH}/include ${MTCP_PATH}/src/include ${DPDK_PATH}/include)

include_directories(${PCIE_CLOUD_NETWORK_INCLUDE_DIR})

IF(NOT $ENV{TARGET_ARCH} MATCHES "k1om")
  add_subdirectory(pcnsrv)
  # add_subdirectory(pcnsrv_mtcp)
  add_subdirectory(libpcnlink)
  add_subdirectory(libproxy)
  # add_subdirectory(libproxy_mtcp)
  add_subdirectory(liblink)
  add_subdirectory(util)
ENDIF()

add_subdirectory(unittest)
add_subdirectory(lmbench)
