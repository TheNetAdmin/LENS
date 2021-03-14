#include "lat.h"
#include "latency.h"
#include "misc.h"
#include <linux/delay.h>

void buffer_write_back_policy(channel_info_t *ci)
{
	struct latencyfs_worker_ctx *ctx = ci->ctx;

	char  value;
	char *addr_cur;
	char *addr_beg;
	char *addr_end;
	int   i;

	size_t sleep_ms = 2000;

	kr_info("Buffer write back policy test.\n");
	kr_info("DEBUG: ci->buf=0x%016llx\n", (uint64_t)ci->buf);

	for (i = 0; i < 3; i++) {
		addr_beg = (char *)ci->buf;
		addr_end = (char *)ci->buf + ci->region_size;

		if (!validate_address(ci->ctx->sbi, addr_beg) ||
		    !validate_address(ci->ctx->sbi, addr_end)) {
			return;
		}

		kr_info("Sleeping %lu ms\n", sleep_ms);
		msleep(sleep_ms);

		kr_info("Writing to region [0x%016llx-0x%016llx]\n",
			(uint64_t)addr_beg,
			(uint64_t)addr_end);

		for (addr_cur = addr_beg; addr_cur < addr_end;
		     addr_cur += 256) {
			repeat_256byte_ntstore(addr_cur);
		}

		kr_info("Sleeping %lu ms\n", sleep_ms);
		msleep(sleep_ms);

		addr_beg = (char *)ci->buf + ci->region_size;
		addr_end = addr_beg + 16 * 1024 * 1024; /* L2 buf size 16MiB */

		if (addr_end < addr_beg) {
			kr_info("Addr ERROR: addr_beg(0x%016llx) < addr_end(0x%016llx)",
				(uint64_t)addr_beg,
				(uint64_t)addr_end);
			BUG_ON("Invalid addr range");
		}

		if (!validate_address(ci->ctx->sbi, addr_beg) ||
		    !validate_address(ci->ctx->sbi, addr_end)) {
			return;
		}

		kr_info("Reading from another region [0x%016llx-0x%016llx] to flush L2 buffer\n",
			(uint64_t)addr_beg,
			(uint64_t)addr_end);

		for (addr_cur = addr_beg; addr_cur < addr_end;
		     addr_cur += 256) {
			value += *addr_cur;
		}

		kr_info("Value: %u\n", value);
	}
}
