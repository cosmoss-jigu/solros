set(SOURCES
  pcnlink.c
  pcnlink_uapi.c
  pcnlink_eventpoll.c
)

add_library(pcnlink STATIC ${SOURCES})

target_link_libraries (pcnlink link rt scif pthread pci_ring_buffer)
