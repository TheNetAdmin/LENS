#!/bin/bash

check_root="./.kernel_api_check"
check_api="timespec"
check_dir="${check_root}/${check_api}"
output_root="./comp_utils/"

echo -n "Check compatibility [${check_api}]: "

rm -rf "${check_dir}"
mkdir -p "${check_dir}"

pushd "${check_dir}" >/dev/null || exit 2

cat << EOF >"${check_api}.c"
#include <linux/time.h>

int main(void)
{
	// Check if "struct timespec" exists, otherwise it's replaced by timespec64
	struct timespec ts;
	getrawmonotonic(&ts)
	return 0;
}
EOF

cat << EOF >Makefile
obj-m += ${check_api}.o
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

# rm -rf "${check_dir}"

mkdir -p "${output_root}"

if [ ${compatible} == "y" ]; then
	echo "Linux <= 5.5"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_TIMESPEC_H
		#define LENS_TIMESPEC_H

		#include <linux/time.h>

		#define LENS_TIMESPEC struct timespec
		#define LENS_GET_RAW_TS(ts) do { getrawmonotonic(ts); } while(0)

		#endif  /* LENS_TIMESPEC_H */
	EOF
else
	echo "Linux >= 5.6"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_TIMESPEC_H
		#define LENS_TIMESPEC_H

		#include <linux/time.h>

		#define LENS_TIMESPEC struct timespec64
		#define LENS_GET_RAW_TS(ts) do { ktime_get_raw_ts64(ts); } while(0)

		#endif  /* LENS_TIMESPEC_H */
	EOF
fi
