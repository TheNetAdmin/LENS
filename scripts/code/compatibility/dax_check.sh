#!/bin/bash

## Check bdev_dax_supported() compatibility and generate check function

check_root="./.kernel_api_check"
check_api="dax_check"
check_dir="${check_root}/${check_api}"
output_root="./comp_utils/"

echo -n "Check compatibility [${check_api}]: "

mkdir -p "${check_dir}"

pushd "${check_dir}" >/dev/null || exit 2

cat << EOF >"${check_api}.c"
#include <linux/fs.h>
#include <linux/dax.h>

int main(void)
{
	struct super_block *sb;
	return bdev_dax_supported(sb, 4096);
}
EOF

cat << EOF >Makefile
obj-m += dax_check.o
all:
	\${MAKE} -C "/lib/modules/$(uname -r)/build" M="$(pwd)"
EOF

make >make.log 2>&1

## Check if obj file is generated
compatible=n
if [ -f "${check_api}.o" ]; then
	compatible=y
fi

popd >/dev/null || exit 2

rm -rf "${check_dir}"

mkdir -p "${output_root}"

if [ ${compatible} == "y" ]; then
	# Old style, introduced before kernel v4.17.6
	echo "Old"
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
else
	# New style, introduced by kernel v4.17.6
	echo "New"
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
fi
