set(SOURCES
  ring_buffer_scif.c
  ring_buffer_porting.c
)

set_source_files_properties(ring_buffer_scif.c PROPERTIES LANGUAGE C)
set_source_files_properties(ring_buffer_porting.c PROPERTIES LANGUAGE C)

find_package(Threads)
add_library(pci_ring_buffer STATIC ${SOURCES})

target_link_libraries(pci_ring_buffer rt scif)
