#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <numa.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/limits.h>
#include <pcnporting.h>
#include "proxy_i.h"

/*----------------------------------------------------------------------------*/
int get_num_cpus()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
pid_t get_tid()
{
	return syscall(__NR_gettid);
}
/*----------------------------------------------------------------------------*/
int proxy_core_affinitize(int cpu)
{
	cpu_set_t cpus;
	struct bitmask *bmask;
	FILE *fp;
	char sysfname[PATH_MAX];
	int phy_id;
	size_t n;
	int ret;
	int unused;

	n = get_num_cpus();

	if (cpu < 0 || cpu >= (int) n) {
		errno = -EINVAL;
		return -1;
	}

	CPU_ZERO(&cpus);
	CPU_SET((unsigned)cpu, &cpus);

	ret = sched_setaffinity(get_tid(), sizeof(cpus), &cpus);

	if (numa_max_node() == 0)
		return ret;
	bmask = numa_bitmask_alloc(n);
	pcn_assert(bmask, "numa_bitmask_alloc");

	/* read physical id of the core from sys information */
	snprintf(sysfname, sizeof(sysfname) - 1,
		"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",
		cpu);
	fp = fopen(sysfname, "r");
	if (!fp) {
		perror(sysfname);
		errno = EFAULT;
		return -1;
	}
	unused = fscanf(fp, "%d", &phy_id);

	numa_bitmask_setbit(bmask, phy_id);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	fclose(fp);

	return ret;
}
