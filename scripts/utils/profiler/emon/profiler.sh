
SEPDRV_DIR='/opt/intel/emon/sepdk/src/'
EMonEventFile=${EMonEventFile:-emon/events.txt}

print_profiler() {
    echo \#\#\# Profiler: EMon
    echo \#\#\#     - Bin:           $(which emon)
    echo \#\#\#     - EMonEventFile: $EMonEventFile
    echo \#\#\# Profiler Events:
    cat $EMonEventFile
    echo
    if [ $(hostname) == "sdp" ]; then
        echo "Host [$(hostname)] emon is not supported yet, try fixing emon"
        exit 1
    fi
}

load_profiler() {
    if [ ! -f $EMonEventFile ]; then
        echo Error: EMonEventFile not found: [$EMonEventFile]
        exit 1
    fi
    source /opt/intel/emon/sep_vars.sh
    (cd $SEPDRV_DIR && ./insmod-sep)
}

unload_profiler() {
    (cd $SEPDRV_DIR && ./rmmod-sep)
}

start_profiler() {
    if [ -z $TaskDir ]; then
        echo "[$0](start_profiler): Invalid output path"
        exit 1
    fi
    emon -i $EMonEventFile -f $TaskDir/emon.dat & disown
    sleep 1
}

stop_profiler() {
    sleep 1
    emon -stop
}
