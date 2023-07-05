#!/bin/bash

# preparation
pushd "$(dirname "$0")"

RESULTS_DIR=results
if [[ ! -e $RESULTS_DIR ]]; then
    mkdir $RESULTS_DIR
else
    rm -r $RESULTS_DIR
    mkdir $RESULTS_DIR
fi

FREQ_GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

NMI_WATCHDOG=$(cat /proc/sys/kernel/nmi_watchdog)
echo 0 > /proc/sys/kernel/nmi_watchdog

function measure {
    insmod mwait.ko $2
    cp -r /sys/mwait_measurements $RESULTS_DIR/$3/$1
    rmmod mwait
}

# measurements
MEASUREMENT_NAME=cstates
mkdir $RESULTS_DIR/$MEASUREMENT_NAME
measure "C0" "target_cstate=0" $MEASUREMENT_NAME
measure "C1" "target_cstate=1" $MEASUREMENT_NAME
measure "C1E" "target_cstate=1 target_subcstate=1" $MEASUREMENT_NAME
measure "C3" "target_cstate=2" $MEASUREMENT_NAME
measure "C6" "target_cstate=3" $MEASUREMENT_NAME
measure "C7s" "target_cstate=4 target_subcstate=3" $MEASUREMENT_NAME
measure "C8" "target_cstate=5" $MEASUREMENT_NAME

MEASUREMENT_NAME=cores_mwait
mkdir $RESULTS_DIR/$MEASUREMENT_NAME
measure "0" "cpus_mwait=0" $MEASUREMENT_NAME
measure "1" "cpus_mwait=1" $MEASUREMENT_NAME
measure "2" "cpus_mwait=2" $MEASUREMENT_NAME
measure "3" "cpus_mwait=3" $MEASUREMENT_NAME
measure "4" "cpus_mwait=4" $MEASUREMENT_NAME
measure "5" "cpus_mwait=5" $MEASUREMENT_NAME
measure "6" "cpus_mwait=6" $MEASUREMENT_NAME
measure "7" "cpus_mwait=7" $MEASUREMENT_NAME
measure "8" "cpus_mwait=8" $MEASUREMENT_NAME

# cleanup
echo $NMI_WATCHDOG > /proc/sys/kernel/nmi_watchdog
echo $FREQ_GOVERNOR | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
popd