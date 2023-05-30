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
    cp /sys/mwait_measurements/measurement_results results/$1.csv
    rmmod mwait
}

# measurements
measure "C0" "target_cstate=0"
measure "C1" "target_cstate=1"
measure "C2" "target_cstate=2"
measure "C3" "target_cstate=3"
measure "C4" "target_cstate=4"
measure "C5" "target_cstate=5"
measure "C6" "target_cstate=6"
measure "C7" "target_cstate=7"
measure "C8" "target_cstate=8"
measure "C9" "target_cstate=9"
measure "C10" "target_cstate=10"

measure "0" "cpus_mwait=0"
measure "1" "cpus_mwait=1"
measure "2" "cpus_mwait=2"
measure "3" "cpus_mwait=3"
measure "4" "cpus_mwait=4"
measure "5" "cpus_mwait=5"
measure "6" "cpus_mwait=6"
measure "7" "cpus_mwait=7"
measure "8" "cpus_mwait=8"

# cleanup
echo $NMI_WATCHDOG > /proc/sys/kernel/nmi_watchdog
echo $FREQ_GOVERNOR | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
popd