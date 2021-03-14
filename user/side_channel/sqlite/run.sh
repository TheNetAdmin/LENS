#!/bin/bash

set -e
set -u

source ../common.sh
source ../bench.sh

set -x

usage() {
	echo "Usage: $0 [both|both_per_set|sql_only|probe]"
	echo "Env:   sql_op=[count|query|insert1000|insert10000|update1000|update100|range1|range_all|index_range_all]"
	echo "       sqlite_db_file=[npi_data_ranged.db|npi_data_basic.db]"
}

if [ $# -ne 1 ]; then
	usage
	exit 1
fi

op="${1}"
case ${op} in
both) ;;
both_per_set) ;;
both_per_set_debug) ;;
query) ;;
probe) ;;
*)
	echo "ERROR: op [${op}] not supported"
	usage
	exit 1
	;;
esac
shift 1

copy_new_db=y
sqlite_db_file="${sqlite_db_file:-npi_data_basic.db}"
# sql_op="${sql_op:-count}"

case ${sql_op} in
count)
	copy_new_db=n
	;;
query)
	copy_new_db=n
	;;
sort)
	copy_new_db=n
	;;
insert1000) ;;
insert10000) ;;
update100) ;;
update1000) ;;
range*)
	copy_new_db=n
	sqlite_db_file="npi_data_ranged.db"
	;;
index_range*)
	copy_new_db=n
	sqlite_db_file="npi_data_ranged_indexed.db"
	;;
*)
	echo "ERROR: sql_op [${sql_op}] not supported"
	usage
	exit 1
	;;
esac

prepare_sqlite() {
	tgt_file="/mnt/pmem/sqlite/${sqlite_db_file}"
	src_file="/home/zixuan/code/nvsec-lens/user/side_channel/data/nppes_npi/${sqlite_db_file}" # TODO: need better file path assignments
	if [ "$copy_new_db" == "y" ]; then
		echo "Copy new database file to pmem"
		mkdir -p "/mnt/pmem/sqlite/"
		rm -f ${tgt_file}
		cp "${src_file}" "${tgt_file}"
	else
		if [ ! -f "${tgt_file}" ]; then
			mkdir -p "/mnt/pmem/sqlite/"
			rm -f ${tgt_file}
			cp "${src_file}" "${tgt_file}"
		fi
	fi
}

sqlite_bench() {
	echo "SQLite SQL code file: $1"
	echo "SQLite DB file: ${sqlite_db_file}"
	sqlite3 /mnt/pmem/sqlite/${sqlite_db_file} < "sql/$1";
}

exec_sql_op() {
	echo "SQL op: ${sql_op}"
	case ${sql_op} in
	count)
		sqlite_bench npees_count1.sql
		sleep 1
		sqlite_bench npees_count2.sql
		sleep 1
		sqlite_bench npees_count3.sql
		sleep 1
		sqlite_bench npees_count4.sql
		;;
	sort)
		sqlite_bench npees_sort1.sql
		;;
	query)
		sqlite_bench npees_query1.sql
		sleep 1
		sqlite_bench npees_query2.sql
		sleep 1
		sqlite_bench npees_query3.sql
		;;
	insert1000)
		sqlite_bench npees_insert1000.sql
		sleep 1
		sqlite_bench npees_insert1000.sql.loc.sql
		;;
	insert10000)
		sqlite_bench npees_insert10000.sql
		sleep 1
		sqlite_bench npees_insert10000.sql.loc.sql
		;;
	update100)
		sqlite_bench npees_insert1000.sql
		sleep 1
		sqlite_bench npees_insert1000.sql.loc.sql
		sleep 1
		sqlite_bench npees_update100.sql
		sleep 1
		sqlite_bench npees_update1.sql.loc.sql
		;;
	update1000)
		sqlite_bench npees_insert1000.sql
		sleep 1
		sqlite_bench npees_insert1000.sql.loc.sql
		sleep 1
		sqlite_bench npees_update1000.sql
		sleep 1
		sqlite_bench npees_update1.sql.loc.sql
		;;
	range1)
		sqlite_bench npees_range_date1.sql
		;;
	range2)
		sqlite_bench npees_range_date2.sql
		;;
	range3)
		sqlite_bench npees_range_date3.sql
		;;
	range4)
		sqlite_bench npees_range_date4.sql
		;;
	range_month1)
		sqlite_bench npees_range_month1.sql
		;;
	range_month2)
		sqlite_bench npees_range_month2.sql
		;;
	range_month3)
		sqlite_bench npees_range_month3.sql
		;;
	range_month4)
		sqlite_bench npees_range_month4.sql
		;;
	range_all)
		sqlite_bench npees_range_date1.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date2.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date3.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date4.sql
		drop_system_cache
		sleep 2
		sqlite_bench npees_range_date1.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date2.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date3.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date4.sql
		drop_system_cache
		sleep 1
		;;
	index_range_all)
		sqlite_bench npees_range_date1.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date2.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date3.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date4.sql
		drop_system_cache
		sleep 2
		sqlite_bench npees_range_date1.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date2.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date3.sql
		drop_system_cache
		sleep 1
		sqlite_bench npees_range_date4.sql
		drop_system_cache
		sleep 1
		;;
	*)
		echo "ERROR: sql_op [${sql_op}] not supported"
		usage
		exit 1
		;;
	esac
}

side_channel_iter=4096

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe
	fi
}


function bench_run_per_set() {
	side_channel_iter=$((2 ** 16))
	_bench_run_side_channel_kernel -i ${side_channel_iter} -o probe -p "${1}"
}

_run() {
	echo "RUNNING op=${op}"
	case ${op} in
	probe)
		prepare
		bench_func -i 512 -o probe "$@"
		cleanup
		;;
	sql_only)
		exec_sql_op
		;;
	both)
		mkdir -p "${TaskDir}"
		{
			# Prepare
			prepare
			prepare_sqlite

			# Run
			{
				sleep 1
				echo "Exec SQL op start"
				exec_sql_op
				echo "Exec SQL op end"
			} &
			pid_bench=$!

			{
				bench_run
				echo "probe end"
			} &
			pid_run=$!

			wait ${pid_bench}
			wait ${pid_run}

			cleanup
		} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "$TaskDir/stdout.both.log")
		;;
	both_per_set)
		mkdir -p "${TaskDir}"
		{
			# Prepare
			prepare
			prepare_sqlite

			# Run
			for set_idx in {0..255}; do
			{
				bench_echo "Probing at set [${set_idx}]"
				{
					bench_echo "Probe start"
					bench_run_per_set "${set_idx}"
					bench_echo "Probe end"
				} &
				pid_run=$!

				{
					sleep 0.25
					bench_echo "Exec SQL op start"
					exec_sql_op
					bench_echo "Exec SQL op end"
				} &
				pid_bench=$!

				wait ${pid_bench}
				wait ${pid_run}
			}
			done

			bench_echo "Parse start"
			parse_side_channel
			bench_echo "Parse done"

			cleanup
		} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "$TaskDir/stdout.both.log")
		;;
	both_per_set_debug)
		mkdir -p "${TaskDir}"
		{
			# Prepare
			prepare
			prepare_sqlite

			# Run
			for set_idx in {0..4}; do
			{
				bench_echo "Probing at set [${set_idx}]"
				{
					bench_echo "Probe start"
					bench_run_per_set "${set_idx}"
					bench_echo "Probe end"
				} &
				pid_run=$!

				{
					sleep 0.25
					bench_echo "Exec SQL op start"
					exec_sql_op
					bench_echo "Exec SQL op end"
				} &
				pid_bench=$!

				wait ${pid_bench}
				wait ${pid_run}
			}
			done

			bench_echo "Parse start"
			parse_side_channel
			bench_echo "Parse done"

			cleanup
		} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "$TaskDir/stdout.both.log")
		;;
	esac
}

_run "$@"
