#!/bin/bash

set -e

curr_path=$(realpath $(dirname $0))
grub_file=/etc/default/grub

host_name=$(hostname)
result_path=$(realpath $curr_path/../../results/machine)
operation_id=$(date +%Y%m%d%H%M%S)-$(hostname)-grub
operation_path=$result_path/$operation_id
mkdir -p $operation_path

cp $grub_file $operation_path/grub.bef

print_help() {
    echo "$0 setup|reset [kvm]"
}

grub_apply() {
    if [ "$host_name" == AEP2 ]; then
        grub2-mkconfig -o /boot/efi/EFI/fedora/grub.cfg
    elif [ "$host_name" == "sdp" ]; then
        update-grub
    elif [ "$host_name" == "nv-4" ]; then
        update-grub
	else
		echo "Unknown host: [$host_name]"
		exit 2
    fi
}

grub_setup_aep2() {
    if [ "${1}" == "kvm" ]; then
        echo "Grub setup for KVM"
        grub_default_kernel="$(grubentries | grep '(5.11.0)' | awk '{print $1;}')"
    else
        grub_default_kernel="$(grubentries | grep 'appdirect+' | awk '{print $1;}')"
    fi

cat <<- EOF >>$grub_file
### Zixuan grub start
GRUB_DEFAULT=${grub_default_kernel}
GRUB_CMDLINE_LINUX="rd.lvm.lv=fedora/root rd.lvm.lv=fedora/swap rhgb quiet console=ttyS0,115200 modprobe.blacklist=noueavu modprobe.blacklist=qat_c62x nokaslr memmap=32G!16G log-buf-len=1G"
### Zixuan grub end
EOF
}

grub_setup_sdp() {
    grub_default_kernel=0

cat <<- EOF >>$grub_file
### Zixuan grub start
GRUB_DEFAULT=${grub_default_kernel}
GRUB_CMDLINE_LINUX="nokaslr memmap=32G!16G log-buf-len=1G mitigations=off"
### Zixuan grub end
EOF
}


grub_setup_nv_4() {
    grub_default_kernel=0

cat <<- EOF >>$grub_file
### Zixuan grub start
GRUB_DEFAULT=${grub_default_kernel}
GRUB_CMDLINE_LINUX="nokaslr memmap=32G!16G log-buf-len=1G mitigations=off"
### Zixuan grub end
EOF
}

grub_setup() {
    echo "Grub setup"

    if grep -q "Zixuan grub start" "$grub_file"; then
        echo "Config already in grub file"
        exit 1
    fi

    if [ "$host_name" == "AEP2" ]; then
        grub_setup_aep2 $*
    elif [ "$host_name" == "sdp" ]; then
        grub_setup_sdp $*
    elif [ "$host_name" == "nv-4" ]; then
        grub_setup_nv_4 $*
	else
		echo "Unknown host: [$host_name]"
		exit 2
    fi

    cp $grub_file $operation_path/grub.aft

    echo "Grub apply"
    grub_apply

    echo "Grub done"

    if [ "${1}" == "kvm" ]; then
        echo "You may want to reboot using: systemctl reboot --firmware-setup"
		echo "                          or: $(dirname "$0")/bios.sh"
    fi
}

grub_reset() {
    echo "Grub reset"
    
    # https://stackoverflow.com/questions/37680636/sed-multiline-delete-with-pattern
    sed -i '/### Zixuan grub start/{:a;N;/### Zixuan grub end/!ba};//d' $grub_file

    cp $grub_file $operation_path/grub.aft

    echo "Grub apply"
    grub_apply

    echo "Grub done"
}

case $1 in
    setup)
		shift 1
        grub_setup ${*} | tee -a $operation_path/log
    ;;
    reset)
        grub_reset ${*} | tee -a $operation_path/log
    ;;
    *)
        print_help
    ;;
esac
