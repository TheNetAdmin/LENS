#!/bin/bash

## Check bdev_dax_supported() compatibility and generate check function

check_root="./.kernel_api_check"
check_api="dax_check"
check_dir="${check_root}/${check_api}"
output_root="./comp_utils/"

echo -n "Check compatibility [${check_api}]: "

rm -rf "${check_dir}"
mkdir -p "${check_dir}"

function check_prev_4_17_6() {
	cat <<- EOF > "${check_api}.c"
	#include <linux/fs.h>
	#include <linux/dax.h>

	int main(void)
	{
		struct super_block *sb;
		return bdev_dax_supported(sb, 4096);
	}
	EOF

	cat <<- EOF >Makefile
	obj-m += ${check_api}.o
	all:
		\${MAKE} -C "/lib/modules/$(uname -r)/build" M="$(pwd)"
	EOF

	make >make.log 2>&1

	if [ -f "${check_api}.o" ]; then
		return 0
	else
		return 1
	fi
}

function generate_prev_4_17_6() {
	mkdir -p "${output_root}"
	echo "Linux < 4.17.6"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_DAX_CHECK_H
		#define LENS_DAX_CHECK_H
		#include <linux/fs.h>
		#include <linux/dax.h>

		static inline int check_dax(struct super_block *sb, int blocksize)
		{
			return bdev_dax_supported(sb, blocksize);
		}

		#endif  /* LENS_DAX_CHECK_H */
	EOF
}

function prev_4_17_6() {
	if check_prev_4_17_6; then
		generate_prev_4_17_6
		return 0
	else
		return 1
	fi
}

function check_post_4_17_6() {
	cat <<- EOF > "${check_api}.c"
	#include <linux/fs.h>
	#include <linux/dax.h>

	int main(void)
	{
		struct super_block *sb;
		return bdev_dax_supported(sb->s_bdev, 4096);
	}
	EOF

	cat <<- EOF >Makefile
	obj-m += ${check_api}.o
	all:
		\${MAKE} -C "/lib/modules/$(uname -r)/build" M="$(pwd)"
	EOF

	make >make.log 2>&1

	if [ -f "${check_api}.o" ]; then
		return 0
	else
		return 1
	fi
}

function generate_post_4_17_6() {
	mkdir -p "${output_root}"
	echo "Linux >= 4.17.6"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_DAX_CHECK_H
		#define LENS_DAX_CHECK_H
		#include <linux/fs.h>
		#include <linux/dax.h>

		static inline int check_dax(struct super_block *sb, int blocksize)
		{
			return !bdev_dax_supported(sb->s_bdev, blocksize);
		}

		#endif  /* LENS_DAX_CHECK_H */
	EOF
}

function post_4_17_6() {
	if check_post_4_17_6; then
		generate_post_4_17_6
		return 0
	else
		return 1
	fi
}

pushd "${check_dir}" >/dev/null || exit 2

prev_4_17_6 || \
post_4_17_6 || \
(echo "Not compatible" && exit 2)

popd >/dev/null || exit 2


# bdev_dax_supported is depricated starting from v5.15
# https://github.com/torvalds/linux/commit/bdd3c50d83bf7f6acc869b48d02670d19030ae03