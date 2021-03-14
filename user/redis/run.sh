#!/bin/bash

usage() {
	echo "USAGE: $0 [server|server-strace|client]"
}

if [ $# -ne 1 ]; then
	usage
	exit 1
fi

cmd=$1

: "${REDIS_PATH:="/root/Zixuan/nvsec/redis/redis/src/"}"
: "${REDIS_SERVER:="${REDIS_PATH}/redis-server"}"
: "${REDIS_CLIENT:="${REDIS_PATH}/redis-cli"}"
: "${REDIS_CONFIG:="./redis.conf"}"

if [ "${cmd}" == "server" ]; then
	${REDIS_SERVER} ${REDIS_CONFIG}
elif [ "${cmd}" == "server-strace" ]; then
	strace -f -F -e trace=fdatasync,fsync \
	${REDIS_SERVER} ${REDIS_CONFIG}
elif [ "${cmd}" == "client" ]; then
	${REDIS_CLIENT}
else
	usage
	exit 1
fi

