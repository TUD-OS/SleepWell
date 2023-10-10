# Usage

Before being able to make measurements on a real machine, some setup is required:

## 1. Compile and install custom linux kernel

Since some small changes to the linux kernel code were necessary to achieve the required results, a custom kernel has to be installed first.
The kernel source code can be found in the submodule ```linuxMWAIT``` in the root of this repository.
Since there are multiple well documented ways to compile and install a custom linux kernel on a machine this README won`t give any further details at this point.

### Notes for starting on a real machine

To avoid the Intel IOMMU driver interfering with our setup (specifically the routing of the HPET interrupt) it might be necessary to use the following startup parameter:

```
intremap=off
```

## 2. Compile kernel module and measure

Our usual setup for using and developing this measurement tool was having an additional system where the development and also the evaluation of results takes place.
The system under measurement (which is the one which needs the custom kernel) only needs to compile the kernel module and insert it.
To facilitate this process, there are the 2 following scripts in the root of this repository:

### ```deploy.sh```

This script takes the IPv4 address of the system under measure as its' only parameter.
Its' purpose is to copy all the files needed on the system under measurement to it and compile the kernel module.
These files are prepared in the ```mwait_deploy``` folder.

### ```measure.sh```

This script as well takes the IPv4 address of the system to be measured as its' only parameter.
It starts the script ```mwait_deploy/measure.sh``` that was transfered during the last execution of ```deploy.sh``` on the target machine.
After this step has been successfully completed, it copies the measurement results from the measured machine to the ```output``` folder.
As a last step it calls the ```scripts/evaluateMeasurements.py``` script to immediately produce some diagrams.
These can also be found in the ```output``` folder.

# Configuring the measurement

The script ```mwait_deploy/measure.sh``` describes what happens during a measurement run.
Apart from some setup and cleanup, this also includes the specific circumstances that will be measured, e.g. which C-State will be entered or how many threads will execute mwait.
By default, all the C-States used by the cpuidle driver on a Haswell system are measured, as well as each combination of threads doing mwait/threads in a busy loop.
Additionally, the cpuidle driver C-States for a Skylake system are configured, but commented out, to enable an easy switch between the two generations.
Further measurements can be specified here by calling the measure function.
This function's parameters are the name of the specific measurement, then the parameters to be used when inserting the kernel module and finally the name of the folder to put the results in.
For more information on the available parameters of the kernel module, please execute modinfo on the compiled module.

# Development

To ease development, this project was set up to work with ```buildroot```, a software that can create bootable kernels and filesystem images from scratch.
Since this was only used with emulators (namely ```qemu```), and those usually don't fully support mwait or machine specific features such as MSRs, this was only really useful in the early stages of development.
Despite this it is still possible to use the project on an emulator following these steps:

## Setup (first use only)

```sh
cd buildroot
make BR2_EXTERNAL="$(pwd)/../mwait_module" qemu_x86_64_defconfig
```

In ```.config``` set:
* BR2_PACKAGE_MWAIT_MODULE=y
* BR2_ROOTFS_OVERLAY="../overlay"
* BR2_PACKAGE_OVERRIDE_FILE="../buildroot_override"
* BR2_PACKAGE_HOST_LINUX_HEADERS_CUSTOM_6_1=y
* BR2_ENABLE_DEBUG=y

```sh
make
```

## Compile Kernel Module

```sh
make mwait_module-dirclean && make
```

## Compile Kernel

```sh
make linux-rebuild
```

## Starting Qemu

```sh
qemu-system-x86_64 -M pc -kernel output/images/bzImage -drive file=output/images/rootfs.ext2,if=virtio,format=raw \
    -append 'root=/dev/vda console=ttyS0' -net nic,model=virtio -nographic -serial mon:stdio -net user -smp 2
```
