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
for STATE in /sys/devices/system/cpu/cpu0/cpuidle/state*;
do
    NAME=$(< "$STATE"/name);
    [[ "$NAME" == 'POLL' ]] && continue;
    DESC=$(< "$STATE"/desc);
    MWAIT_HINT=${DESC#MWAIT };
    measure $NAME "mwait_hint=$MWAIT_HINT" $MEASUREMENT_NAME
done

MEASUREMENT_NAME=cores_mwait
mkdir $RESULTS_DIR/$MEASUREMENT_NAME
for ((i=0; i<=$(getconf _NPROCESSORS_ONLN); i++));
do
    measure $i "cpus_mwait=$i" $MEASUREMENT_NAME
done

# cleanup
echo $NMI_WATCHDOG > /proc/sys/kernel/nmi_watchdog
echo $FREQ_GOVERNOR | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
popd