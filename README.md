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

## 2. Compile kernel module

## 3. 

# Development

To ease development, this project was set up to work with ```buildroot```, a software that can create bootable kernels and filesystem images from scratch.
Since this was only used with emulators (namely ```qemu```), and those usually don't fully support machine specific features such as MSRs and mwait, this was only really useful in the early stages of development.
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
