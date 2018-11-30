set(SOURCES
  pcnsrv.c
)

find_package(Threads)

add_executable (pcnsrv ${SOURCES})
target_link_libraries (pcnsrv proxy link pci_ring_buffer)
