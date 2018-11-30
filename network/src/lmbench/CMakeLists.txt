add_definitions(-DHAVE_int64_t=1)
add_definitions(-DHAVE_pmap_clnt_h)
add_definitions(-DHAVE_socklen_t)
add_definitions(-DHAVE_DRAND48)
add_definitions(-DHAVE_SCHED_SETAFFINITY=1)

ADD_LIBRARY(lmbench lib_tcp.c lib_udp.c lib_unix.c lib_timing.c lib_mem.c lib_stats.c lib_debug.c
	getopt.c lib_sched.c)

set(SOURCES
  tcp_benchmark.c
)

set(BWSOURCES
  bw_tcp.c
)

find_package(Threads)

add_executable (tcp_benchmark ${SOURCES})
add_executable (bw_tcp ${BWSOURCES})

target_link_libraries (tcp_benchmark lmbench m)
target_link_libraries (bw_tcp lmbench m)
