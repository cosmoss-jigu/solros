add_definitions(-DRTE_MACHINE_CPUFLAG_SSE)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE2)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE3)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSSE3)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE4_1)
add_definitions(-DRTE_MACHINE_CPUFLAG_SSE4_2)
add_definitions(-DRTE_MACHINE_CPUFLAG_AES)
add_definitions(-DRTE_MACHINE_CPUFLAG_PCLMULQDQ)
add_definitions(-DRTE_MACHINE_CPUFLAG_AVX)
add_definitions(-DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSSE3,RTE_CPUFLAG_SSE4_1,RTE_CPUFLAG_SSE4_2,RTE_CPUFLAG_AES,RTE_CPUFLAG_PCLMULQDQ,RTE_CPUFLAG_AVX)

set(SOURCES
  pcnsrv_mtcp.c
)

find_package(Threads)

add_executable (pcnsrv_mtcp ${SOURCES})
target_link_libraries (pcnsrv_mtcp proxy_mtcp link pci_ring_buffer ibverbs dl ${MTCP_LIBS} -Wl,--whole-archive ${DPDK_LIBS} -Wl,--no-whole-archive)

configure_file(epserver.conf epserver.conf COPYONLY)
configure_file(config/arp.conf config/arp.conf COPYONLY)
configure_file(config/route.conf config/route.conf COPYONLY)
