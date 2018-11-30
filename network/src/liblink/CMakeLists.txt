set(SOURCES
  link.c
  netmsg.c
)

add_library(link STATIC ${SOURCES})

target_link_libraries (link rt scif pthread pci_ring_buffer)