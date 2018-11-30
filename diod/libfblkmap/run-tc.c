#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "fblkmap.h"

#define PAGE_SIZE 4096
char page_buf[PAGE_SIZE];
static int __err_line = 0;

#define err_return() do {			\
		if (__err_line == 0)		\
			__err_line = __LINE__;	\
		goto err_out;			\
	} while(0)

#define HERE() printf("%s:%d\n", __func__, __LINE__)

int tc_one_chunk(const char *file)
{
	int fd;
	int i;

	fd = open(file, O_CREAT | O_TRUNC | O_RDWR);
	if (fd == -1) {
		fd = -errno;
		err_return();
	}

	for (i = 0; i < 10; ++i) {
		if (pwrite(fd, page_buf, sizeof(page_buf),
			   i * sizeof(page_buf)) == -1) {
			fd = -errno;
			err_return();
		}
	}

err_out:
	return fd;
}

int tc_two_chunks(const char *file)
{
	int fd;
	int i;

	fd = open(file, O_CREAT | O_TRUNC | O_RDWR);
	if (fd == -1) {
		fd = -errno;
		err_return();
	}

	for (i = 0; i < 10; ++i) {
		if (pwrite(fd, page_buf, sizeof(page_buf),
			   i * sizeof(page_buf)) == -1) {
			fd = -errno;
			err_return();
		}
	}

	{
		int rc = pread(fd,
				page_buf,
				10 * sizeof(page_buf),
				9 * sizeof(page_buf));
		printf("[11] pread from hole: rc: %d %s\n",
		       rc, strerror(errno));
	}

	for (i = 20; i < 30; ++i) {
		if (pwrite(fd, page_buf, sizeof(page_buf),
			   i * sizeof(page_buf)) == -1) {
			fd = -errno;
			err_return();
		}
	}

	{
		int rc = pread(fd,
				page_buf,
				sizeof(page_buf),
				100 * sizeof(page_buf));
		printf("[100] pread from hole: rc: %d %s\n",
		       rc, strerror(errno));
	}

err_out:
	return fd;
}

int tc_growing_chunks(const char *file)
{
	int fd;
	int i;

	fd = open(file, O_CREAT | O_TRUNC | O_RDWR);
	if (fd == -1) {
		fd = -errno;
		err_return();
	}

	for (i = 0; i < 10; ++i) {
		if (pwrite(fd, page_buf, sizeof(page_buf),
			   i * sizeof(page_buf)) == -1) {
			fd = -errno;
			err_return();
		}
	}

	for (i = 20; i < 30; ++i) {
		if (pwrite(fd, page_buf, sizeof(page_buf),
			   i * sizeof(page_buf)) == -1) {
			fd = -errno;
			err_return();
		}
	}

err_out:
	return fd;
}

int main(int argc, char *argv[])
{
	struct fblkmap_t *fblkmap;
	int total_exts;
	char *tc_name;
	int fd, rc, idx, i;
	__u64 logical_addr;
	FILE *logout = stdout;

	/* one chunk  */
	tc_name = "tc_one_chunk.fbmdat";
	fprintf(logout, "=== %s\n", tc_name);
	rc = fd = tc_one_chunk(tc_name);
	if (rc < 0)
		err_return();
	sync();

	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 10,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);
	close(fd);

	/* growing a chunk by enforcing */
	tc_name = "tc_growing_by_force.fbmdat";
	fd = open(tc_name, O_CREAT | O_TRUNC | O_RDWR);
	if (fd == -1) {
		fd = -errno;
		err_return();
	}
	for (i = 0; i < 10; ++i) {
		fprintf(logout, "=== %s\n", tc_name);
		rc = fbm_find_block_mapping_force(fd, 0,
						  &fblkmap, &total_exts,
						  PAGE_SIZE * i,
						  PAGE_SIZE);
		if (rc < 0)
			err_return();
		fbm_print_block_map(logout, total_exts, fblkmap);
	}
	close(fd);

	/* two chunks  */
	tc_name = "tc_two_chunks.fbmdat";
	fprintf(logout, "=== %s\n", tc_name);
	rc = fd = tc_two_chunks(tc_name);
	if (rc < 0)
		err_return();
	sync();

	rc = fbm_get_block_map(fd, 0, 0, 0xFFFFFFFFFFFFFFFF,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);

	logical_addr =  1 * PAGE_SIZE;
	idx = fbm_find_block_mapping(fblkmap, total_exts, logical_addr);
	fprintf(logout, "    %llx: %d\n", logical_addr, idx);

	logical_addr = 11 * PAGE_SIZE;
	idx = fbm_find_block_mapping(fblkmap, total_exts, logical_addr);
	fprintf(logout, "    %llx: %d\n", logical_addr, idx);

	logical_addr = 21 * PAGE_SIZE;
	idx = fbm_find_block_mapping(fblkmap, total_exts, logical_addr);
	fprintf(logout, "    %llx: %d\n", logical_addr, idx);

	logical_addr = 31 * PAGE_SIZE;
	idx = fbm_find_block_mapping(fblkmap, total_exts, logical_addr);
	fprintf(logout, "    %llx: %d\n", logical_addr, idx);
	close(fd);

	/* growing chunks  */
	tc_name = "tc_growing_chunks.fbmdat";
	fprintf(logout, "=== %s\n", tc_name);
	rc = fd = tc_growing_chunks(tc_name);
	if (rc < 0)
		err_return();
	sync();

	rc = fbm_get_block_map(fd,
			       FBM_FLAG_ALIGN |
			       FBM_FLAG_SYNC  |
			       FBM_FLAG_ALLOC,
			       PAGE_SIZE * 40,
			       PAGE_SIZE * 10,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);
	close(fd);

	/* two chunks with filling a hole */
	tc_name = "tc_two_chunks_with_filling_a_hole.fbmdat";
	fprintf(logout, "=== %s\n", tc_name);
	rc = fd = tc_two_chunks(tc_name);
	if (rc < 0)
		err_return();
	sync();

	rc = fbm_get_block_map(fd,
			       FBM_FLAG_ALIGN |
			       FBM_FLAG_SYNC  |
			       FBM_FLAG_ALLOC,
			       0, PAGE_SIZE * 30,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);
	close(fd);

	/* two chunks with growing using lookup */
	tc_name = "tc_two_chunks_with_growing_using_lookup.fbmdat";
	fprintf(logout, "=== %s\n", tc_name);
	rc = fd = tc_two_chunks(tc_name);
	if (rc < 0)
		err_return();
	sync();

	fprintf(logout, " -- [0, 10] [20, 30]\n");
	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 30,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);

	fprintf(logout, " -- [0, # 10] [20, 30]\n");
	rc = fbm_find_block_mapping_force(fd, 0,
					  &fblkmap, &total_exts,
					  1 * PAGE_SIZE, 1 * PAGE_SIZE);
	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 30,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);

	fprintf(logout, " -- [0, 10] [#12, 17#] [20, 30]\n");
	rc = fbm_find_block_mapping_force(fd, 0,
					  &fblkmap, &total_exts,
					  12 * PAGE_SIZE, 5 * PAGE_SIZE);
	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 30,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);

	fprintf(logout, " -- [0, 10] [12, 17] [20, # 30]\n");
	rc = fbm_find_block_mapping_force(fd, 0,
					  &fblkmap, &total_exts,
					  25 * PAGE_SIZE, 1 * PAGE_SIZE);
	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 30,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);

	fprintf(logout, " -- [0, 10] [12, 17] [20, 30] [#30, 40#]\n");
	rc = fbm_find_block_mapping_force(fd, FBM_FLAG_SYNC,
					  &fblkmap, &total_exts,
					  30 * PAGE_SIZE, 10 * PAGE_SIZE);

	rc = fbm_get_block_map(fd, 0, 0, PAGE_SIZE * 40,
			       &fblkmap, &total_exts);
	if (rc < 0)
		err_return();
	fbm_print_block_map(logout, total_exts, fblkmap);
	close(fd);

err_out:
	printf("[%s:%d] %s\n", tc_name, __err_line, strerror(rc));
	return rc;
}
