# Prior Remarks

This guide's goal is to enable you to utilize our measurement framework on your machines.
That being said, it is still in development and has so far not been tested on a lot of different systems.
Experience has shown that some tinkering is likely necessary when trying new machines.
At the end of this guide, under [Troubleshooting](#Troubleshooting), some problems we already encountered as well as their solutions are listed.
Please try those if you have trouble using the framework!


# General Setup

Using our measurement framework in its' current form requires 3 steps to be taken, namely:

1. Building and installing our custom linux kernel
2. Deploying and compiling our kernel module
3. Measuring

These steps will be described in more detail further into this document.

The way we used the framework so far is to have 2 machines, one that is the one to be measured (aka ```measurebox```),
and another one that is in charge of controlling the measuring process and evaluating the results (aka ```controllbox```).
Technically you can do all that on one machine, but this approach has advantages when measuring multiple machines and for development.
Since this guide as well as our utilities are geared towards splitting these tasks between two systems, we recommend you do to.
If you want to use our scripts, the systems need to be able to reach each other over IPv4.


# Setup Steps

## 1. Installing our custom linux kernel

Since some small changes to the linux kernel code were necessary to achieve the required results,
a custom kernel has to be installed on the ```measurebox``` for it to work with our kernel module.
For this, you need the custom kernel source code in the ```linuxMWAIT``` submodule, that is based on linux version 6.2.9.
Please download it, compile the kernel as appropriate for your machine and install it.

On debian this process could look something like this:
```console
root@measurebox:~# git clone https://github.com/obibabobi/linuxMWAIT.git
root@measurebox:~# cd linuxMWAIT
root@measurebox:~/linuxMWAIT# cp /boot/config-$(uname --kernel-release) .config
root@measurebox:~/linuxMWAIT# make olddefconfig
root@measurebox:~/linuxMWAIT# make
root@measurebox:~/linuxMWAIT# make modules_install
root@measurebox:~/linuxMWAIT# make install
```
Then reboot into our newly installed kernel.


## 2. Deploy and compile the kernel module

In this step, the files necessary for compiling the kernel module and executing a measurement run need to be transferred from the ```controllbox``` to the ```measurebox```.
These files can be found in the ```mwait_deploy``` folder.
Afterwards, the kernel module needs to be compiled on the ```measurebox``` in preparation for its' usage.

The ```deploy.sh``` script was created to facilitate this process and can be used like this:
```console
user@controllbox:~/MWAITmeasurements# ./deploy.sh <IPv4 of the measurebox>
```
It copies the ```mwait_deploy``` folder to the ```measurebox``` and starts the compilation.


## 3. Measure

The last step is to use the compiled kernel module to take measurements.
This is mainly achieved by starting the ```mwait_deploy/measure.sh``` script on the measurebox.

For this step again a script (```measure.sh```) exists, which can be used as follows:
```console
user@controllbox:~/MWAITmeasurements# ./measure.sh <IPv4 of the measurebox>
```
It starts the copy of ```mwait_deploy/measure.sh``` on the ```measurebox``` that was transferred during the deployment step.
Afterwards it copies the results from the ```measurebox``` to the ```output``` folder on the ```controllbox```.
Lastly, it calls the script ```scripts/evaluateMeasurements.py``` to generate some simple visualisations into the ```output``` folder.


# Further Notes

## Configuring the measurements

Which specific circumstances should be measured during a measurement run can be configured in the ```mwait_deploy/measure.sh``` script.
Depending on which (Sub-)C-States / how many hardware threads are available on you machine, you may need to adjust it.

New measurements are added by calling the ```measure``` function.
This function's parameters are the name of the specific measurement, then the parameters to be used when inserting the kernel module and finally the name of the folder to put the results in.
For information on the available parameters of the kernel module, please execute ```modinfo``` on the compiled module.


# Troubleshooting

## Kernel module does not terminate

The most likely cause for this behavior is that the NMIs that should terminate the measurements never reach the kernel module.


A possible reason for this is that something is interfering with the delivery of the NMIs we configured the HPET to generate.
A likely suspect in that case is the Intel IOMMU driver.
Please try the following kernel startup parameter:
```
intremap=off
```


## Kernel NULL pointer dereference

If, on insertion of the kernel module, it is immediately killed because of a "Kernel NULL pointer dereference", the kernel may have disabled the HPET on startup.
Please try force-enabling the HPET using the following kernel startup parameter:
```
hpet=force
```
