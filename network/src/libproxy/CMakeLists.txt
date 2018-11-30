set(SOURCES
  proxy_u.c
  proxy.c
)

add_library(proxy STATIC ${SOURCES})

target_link_libraries (proxy link rt scif pthread numa pci_ring_buffer)
