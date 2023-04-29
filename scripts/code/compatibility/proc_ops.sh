#!/bin/bash

check_root="./.kernel_api_check"
check_api="proc_ops"
check_dir="${check_root}/${check_api}"
output_root="./comp_utils/"

echo -n "Check compatibility [${check_api}]: "

mkdir -p "${check_dir}"

pushd "${check_dir}" >/dev/null || exit 2

cat << EOF >"${check_api}.c"
#include <linux/fs.h>
#include <linux/proc_fs.h>

int main(void)
{
	// Check if proc_create() takes file_operstions or proc_ops as the last argument
	struct file_operations *fop;
	proc_create("fs_name", 0666, NULL, fop);
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

rm -rf "${check_dir}"

mkdir -p "${output_root}"

if [ ${compatible} == "y" ]; then
	echo "Linux <= 5.5"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_PROC_CREATE_H
		#define LENS_PROC_CREATE_H

		#include <linux/proc_fs.h>
		#include <linux/seq_file.h>
		#include <linux/fs.h>

		#define PROC_OPS(op_name, open_func, write_func) \\
		static struct file_operations op_name = {        \\
			.owner	 = THIS_MODULE,                      \\
			.open	 = open_func,                        \\
			.read	 = seq_read,                         \\
			.llseek	 = seq_lseek,                        \\
			.release = single_release,                   \\
			.write	 = write_func,                       \\
		};

		#endif  /* LENS_PROC_CREATE_H */
	EOF
else
	echo "Linux >= 5.6"
	cat <<- EOF > "${output_root}/${check_api}.h"
		#ifndef LENS_PROC_CREATE_H
		#define LENS_PROC_CREATE_H

		#include <linux/proc_fs.h>
		#include <linux/seq_file.h>
		#include <linux/fs.h>

		#define PROC_OPS(op_name, open_func, write_func) \\
		static struct proc_ops op_name = {               \\
			.proc_open    = open_func,                   \\
			.proc_read    = seq_read,                    \\
			.proc_lseek   = seq_lseek,                   \\
			.proc_release = single_release,              \\
			.proc_write   = write_func,                  \\
		};

		#endif  /* LENS_PROC_CREATE_H */
	EOF
fi
