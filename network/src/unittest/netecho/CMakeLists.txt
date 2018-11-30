set(SOURCES
  netecho.c
)

find_package(Threads)

add_executable (netecho ${SOURCES})

# IF(NOT $ENV{TARGET_ARCH} MATCHES "k1om")
#  target_link_libraries (netecho pcnlink)
# ENDIF()
