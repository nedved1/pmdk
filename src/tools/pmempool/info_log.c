// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * info_log.c -- pmempool info command source file for log pool (DEPRECATED)
 */
#include <stdbool.h>
#include <stdlib.h>
#include <err.h>
#include <sys/mman.h>

#include "common.h"
#include "output.h"
#include "info.h"

/*
 * info_log_data -- print used data from log pool
 */
static int
info_log_data(struct pmem_info *pip, int v, struct pmemlog *plp)
{
	if (!outv_check(v))
		return 0;

	uint64_t size_used = plp->write_offset - plp->start_offset;

	if (size_used == 0)
		return 0;

	uint8_t *addr = pool_set_file_map(pip->pfile, plp->start_offset);
	if (addr == MAP_FAILED) {
		warn("%s", pip->file_name);
		outv_err("cannot read pmem log data\n");
		return -1;
	}

	if (pip->args.log.walk == 0) {
		outv_title(v, "PMEMLOG data");
		struct range *curp = NULL;
		PMDK_LIST_FOREACH(curp, &pip->args.ranges.head, next) {
			uint8_t *ptr = addr + curp->first;
			if (curp->last >= size_used)
				curp->last = size_used - 1;
			uint64_t count = curp->last - curp->first + 1;
			outv_hexdump(v, ptr, count, curp->first +
					plp->start_offset, 1);
			size_used -= count;
			if (!size_used)
				break;
		}
	} else {

		/*
		 * Walk through used data with fixed chunk size
		 * passed by user.
		 */
		uint64_t nchunks = size_used / pip->args.log.walk;

		outv_title(v, "PMEMLOG data [chunks: total = %lu size = %ld]",
				nchunks, pip->args.log.walk);

		struct range *curp = NULL;
		PMDK_LIST_FOREACH(curp, &pip->args.ranges.head, next) {
			uint64_t i;
			for (i = curp->first; i <= curp->last &&
					i < nchunks; i++) {
				outv(v, "Chunk %10lu:\n", i);
				outv_hexdump(v, addr + i * pip->args.log.walk,
					pip->args.log.walk,
					plp->start_offset +
					i * pip->args.log.walk,
					1);
			}
		}
	}

	return 0;
}

/*
 * info_logs_stats -- print log type pool statistics
 */
static void
info_log_stats(struct pmem_info *pip, int v, struct pmemlog *plp)
{
	uint64_t size_total = plp->end_offset - plp->start_offset;
	uint64_t size_used = plp->write_offset - plp->start_offset;
	uint64_t size_avail = size_total - size_used;

	if (size_total == 0)
		return;

	double perc_used = (double)size_used / (double)size_total * 100.0;
	double perc_avail =  100.0 - perc_used;

	outv_title(v, "PMEM LOG Statistics");
	outv_field(v, "Total", "%s",
			out_get_size_str(size_total, pip->args.human));
	outv_field(v, "Available", "%s [%s]",
			out_get_size_str(size_avail, pip->args.human),
			out_get_percentage(perc_avail));
	outv_field(v, "Used", "%s [%s]",
			out_get_size_str(size_used, pip->args.human),
			out_get_percentage(perc_used));

}

/*
 * info_log_descriptor -- print pmemlog descriptor and return 1 if
 * write offset is valid
 */
static int
info_log_descriptor(struct pmem_info *pip, int v, struct pmemlog *plp)
{
	outv_title(v, "PMEM LOG Header");

	/* dump pmemlog header without pool_hdr */
	outv_hexdump(pip->args.vhdrdump, (uint8_t *)plp + sizeof(plp->hdr),
			sizeof(*plp) - sizeof(plp->hdr),
			sizeof(plp->hdr), 1);

	log_convert2h(plp);

	int write_offset_valid = plp->write_offset >= plp->start_offset &&
				plp->write_offset <= plp->end_offset;
	outv_field(v, "Start offset", "0x%lx", plp->start_offset);
	outv_field(v, "Write offset", "0x%lx [%s]", plp->write_offset,
			write_offset_valid ? "OK":"ERROR");
	outv_field(v, "End offset", "0x%lx", plp->end_offset);

	return write_offset_valid;
}

/*
 * pmempool_info_log -- print information about log type pool
 */
int
pmempool_info_log(struct pmem_info *pip)
{
	int ret = 0;

	struct pmemlog *plp = malloc(sizeof(struct pmemlog));
	if (!plp)
		err(1, "Cannot allocate memory for pmemlog structure");

	if (pmempool_info_read(pip, plp, sizeof(struct pmemlog), 0)) {
		outv_err("cannot read pmemlog header\n");
		free(plp);
		return -1;
	}

	if (info_log_descriptor(pip, VERBOSE_DEFAULT, plp)) {
		info_log_stats(pip, pip->args.vstats, plp);
		ret = info_log_data(pip, pip->args.vdata, plp);
	}

	free(plp);

	return ret;
}
