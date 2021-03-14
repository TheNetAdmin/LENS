#!/bin/bash

set -ex

curr_path=$(realpath $(dirname $0))
grub_file=/etc/default/grub

result_path=$(realpath $curr_path/../../results/machine)
operation_id=$(date +%Y%m%d%H%M%S)-$(hostname)-optane
operation_path=$result_path/$operation_id
mkdir -p $operation_path

nvram_bus=$(ndctl list --buses | jq '.[] | select(.provider=="ACPI.NFIT" )| .dev' | tr -d '"')
echo "nvram_bus=${nvram_bus}"

print_help() {
    echo "$(basename $0) setup appdirect ni|i: non-interleaved or interleaved"
    echo "$(basename $0) ndctl [devdax|mongo]: create local Optane namespace through ndctl"
    echo "$(basename $0) status"
    echo "$(basename $0) reset"
}

optane_status() {
    ipmctl show -system -capabilities
    ipmctl show -memoryresources
    ipmctl show -preferences
    ipmctl show -firmware
    ipmctl show -region
    ipmctl show -topology
}

optane_non_interleaved() {
    echo "### Optane status before:"
    optane_status

    ipmctl delete -goal
    ipmctl create -goal MemoryMode=0 PersistentMemoryType=AppDirectNotInterleaved || true

    echo "### Optane status after:"
    optane_status
}

optane_interleaved() {
    echo "### Optane status before:"
    optane_status

    ipmctl delete -goal
    ipmctl create -goal MemoryMode=0 PersistentMemoryType=AppDirect

    echo "### Optane status after:"
    optane_status
}

ndctl_status() {
    ndctl list --namespaces
    ndctl list --regions
}

ndctl_create() {
    ndctl_check_non_interleaved

	option="${1}"

    echo "### Ndctl status before:"
    ndctl_status

	local_region=$(ndctl list --bus "${nvram_bus}" --regions | grep region | cut -d '"' -f 4 | sed 's/region//' | sort -n | head -n 1)
	echo "Creating region [${local_region}]"
	case $option in
		devdax)
			echo "Creating devdax mode"
			ndctl create-namespace --mode devdax --size 8G --align 1G --region "${local_region}" --name "dax-sender"
			ndctl create-namespace --mode devdax --size 8G --align 1G --region "${local_region}" --name "dax-receiver"
		;;
		fsdevdax)
			echo "Creating for fsdevdax"
			ndctl create-namespace --mode fsdax  --size 8G --align 1G --region "${local_region}" --name "dax-victim"
			ndctl create-namespace --mode devdax --size 8G --align 1G --region "${local_region}" --name "dax-attacker"
		;;
		*)
			echo "Creating fsdax mode"
			ndctl create-namespace --mode fsdax --region "$local_region"
		;;
	esac

	if [ $? -ne 0 ]; then
		echo "Failed creating region"
		exit 2
	fi

    echo "### Ndctl status after:"
    ndctl_status

    ndctl_check_local
}

ndctl_check_non_interleaved() {
    interleaved_regions=$(ipmctl show -region | grep AppDirect | grep -v AppDirectNotInterleaved | wc -l)
    if [ "$interleaved_regions" -gt 0 ]; then
        echo "ERROR: $interleaved_regions regions are interleaved, should use non-interleaved"
        exit 1
    fi
}

ndctl_check_local() {
    remote_regions=$(ipmctl show -region | grep -F '0.000 GiB' | awk '{print $1;}' | grep -c 0x0001)
    if [ "$remote_regions" -gt 0 ]; then
        ipmctl show -region
        echo "ERROR: There are remote regions!!!"
        exit 1
    fi
}

ndctl_check_all_removed() {
    remaining_regions=$(ipmctl show -region | grep -c '0.0 GiB')
    if [ "$remaining_regions" -gt 0 ]; then
        echo "ERROR: There are remaining regions not deleted!!!"
        ipmctl show -region
        exit 1
    fi
}

ndctl_reset() {
    echo "### Ndctl status before:"
    ndctl_status

    current_namespaces=$(ndctl list --namespaces --bus "${nvram_bus}"  | grep namespace | cut -d '"' -f 4 | grep -v 'namespace0.0')
    for ns in ${current_namespaces[@]}; do
        ndctl destroy-namespace $ns -f
    done

    echo "### Ndctl status after:"
    ndctl_status

    ndctl_check_all_removed
}

optane_setup() {
    case $1 in
    appdirect)
        case $2 in
        ni)
            optane_non_interleaved
            ;;
        i)
            optane_interleaved
            ;;
        *)
            print_help
            ;;
        esac
        ;;
    *)
        print_help
        ;;
    esac
}

optane_reset() {
    optane_interleaved
}

optane() {
    case $1 in
    setup)
        shift 1
        ndctl_reset | tee -a $operation_path/log
        optane_setup $@ | tee -a $operation_path/log
        ;;
    ndctl)
		shift 1
        ndctl_create $@ | tee -a $operation_path/log
        ;;
    reset)
        ndctl_reset | tee -a $operation_path/log
        optane_reset | tee -a $operation_path/log
        ;;
    status)
        optane_status
        ndctl_status
        ;;
    *)
        print_help
        ;;
    esac
}

optane $@
